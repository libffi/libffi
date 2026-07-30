/* -----------------------------------------------------------------------
   ffi64.c - Copyright (c) 2000, 2007 Software AG
             Copyright (c) 2008 Red Hat, Inc
             Copyright IBM Corp. 2016
 
   S390 Foreign Function Interface
 
   Permission is hereby granted, free of charge, to any person obtaining
   a copy of this software and associated documentation files (the
   ``Software''), to deal in the Software without restriction, including
   without limitation the rights to use, copy, modify, merge, publish,
   distribute, sublicense, and/or sell copies of the Software, and to
   permit persons to whom the Software is furnished to do so, subject to
   the following conditions:
 
   The above copyright notice and this permission notice shall be included
   in all copies or substantial portions of the Software.
 
   THE SOFTWARE IS PROVIDED ``AS IS'', WITHOUT WARRANTY OF ANY KIND, EXPRESS
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
   IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY CLAIM, DAMAGES OR
   OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
   ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
   OTHER DEALINGS IN THE SOFTWARE.
   ----------------------------------------------------------------------- */
/*====================================================================*/
/*                          Includes                                  */
/*                          --------                                  */
/*====================================================================*/

#define TRAMP_DEBUG
#define FFI_CLOSURE_DEBUG

#include <ffi.h>
#include <ffi_common.h>
 
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
 
/*====================== End of Includes =============================*/
 
/*====================================================================*/
/*                           Defines                                  */
/*                           -------                                  */
/*====================================================================*/

/* Maximum number of GPRs available for argument passing.  */ 
#define MAX_GPRARGS 3

/* Maximum number of FPRs available for argument passing.  */ 
#define MAX_FPRARGS 4

/* Round to multiple of 32.  */
#define ROUND_SIZE(size) (((size) + 31) & ~32)

/* If these values change, xplink.S RTABLE must be updated to match!  */
#define FFI390_RET_VOID		      0
#define FFI390_RET_STRUCT	      1
#define FFI390_RET_FLOAT	      2
#define FFI390_RET_DOUBLE	      3
#define FFI390_RET_LDBLE        4
#define FFI390_RET_INT32	      5
#define FFI390_RET_INT64        6
/* struct containing two floats or complexf returned in FPR0 + FPR2 (float halves) */
#define FFI390_RET_STRUCT_FF    7
/* struct containing two doubles or complexd returned in FPR0 + FPR2 */
#define FFI390_RET_STRUCT_DD    8
/* struct containing two long doubles or complexld returned in FPR0/2 + FPR4/6 */
#define FFI390_RET_STRUCT_LDLD  9
/* integer-based complex (e.g. _Complex int): real in GPR3, imag in GPR2 */
#define FFI390_RET_COMPLEX_INT  10

/*===================== End of Defines ===============================*/
 
/*====================================================================*/
/*                          Prototypes                                */
/*                          ----------                                */
/*====================================================================*/
 
/*Making it extern to call this from sysvz.S*/
#pragma map(ffi_prep_args, "PREPARGS")
void ffi_prep_args (unsigned char *, extended_cif *);
#pragma map(ffi_closure_helper_SYSV, "CLSRHLP")
void ffi_closure_helper_SYSV (ffi_closure *, void *, struct ffi_reg_data *);
#pragma map(ffi_determine_return_type, "DETRET")
int ffi_determine_return_type(ffi_closure *);
#pragma map(ffi_struct_float_pair_type, "STFPTYP")
unsigned int ffi_struct_float_pair_type (ffi_type **);
#pragma map(ffi_dummy, "FFIDUMMY")
void ffi_dummy (void);

// void ffi_closure_helper_SYSV (ffi_closure *, unsigned long *, 
//                          unsigned long long *, unsigned long *);

/*====================== End of Prototypes ===========================*/
 
/*====================================================================*/
/*                          Externals                                 */
/*                          ---------                                 */
/*====================================================================*/
 

#pragma map(ffi_call_SYSV, "FFISYS")
extern void ffi_call_SYSV(void (*fn)(void), extended_cif *,
                          unsigned, unsigned *, unsigned, unsigned,
                          unsigned, void *);

#pragma map(ffi_closure_SYSV, "FFISYS2")
extern void ffi_closure_SYSV(void);

/*====================== End of Externals ============================*/
 
/*====================================================================*/
/*                                                                    */
/* Name     - ffi_check_struct_type.                                  */
/*                                                                    */
/* Function - Determine if a structure can be passed within a         */
/*            general purpose or floating point register.             */
/*                                                                    */
/*====================================================================*/
 
static int
ffi_check_struct_type (ffi_type *arg)
{
  size_t size = arg->size;

  /* If the struct has just one element, look at that element
     to find out whether to consider the struct as floating point.  */
  while (arg->type == FFI_TYPE_STRUCT 
         && arg->elements[0] && !arg->elements[1])
    arg = arg->elements[0];

  /* Structs of size 1, 2, 4, and 8 are passed in registers,
     just like the corresponding int/float types.  */
  switch (size)
  {
    case 1:
    case 2:
      return FFI_TYPE_UINT32; 
    case 4:
      if (arg->type == FFI_TYPE_FLOAT)
               return FFI_TYPE_FLOAT;
      else
               return FFI_TYPE_UINT32; 
    case 8:
      if (arg->type == FFI_TYPE_DOUBLE)
               return FFI_TYPE_DOUBLE;
      else
               return FFI_TYPE_UINT64; 
    default:
      break;
  }

  /* Other structs are passed via a pointer to the data.  */
  return FFI_TYPE_POINTER;
}

/*======================== End of Routine ============================*/

/*====================================================================*/
/*                                                                    */
/* Name     - ffi_prep_args.                                          */
/*                                                                    */
/* Function - Prepare parameters for call to function.                */
/*                                                                    */
/* ffi_prep_args is called by the assembly routine once stack space   */
/* has been allocated for the function's arguments.                   */
/*                                                                    */
/*====================================================================*/
 
void
ffi_prep_args (unsigned char *stack, extended_cif *ecif)
{
  /* The stack space will be filled with these areas:
     Note: XPLINK Stack is downward growing (ie. towards low addresses)

     ------------------------------------ <- Low Addresses
       Guard Area (1 MB)
     ------------------------------------ 
       Stack Frame for Called functions
     ------------------------------------ <- Stack Ptr (r4)
       Backchain                            |+2048
     ------------------------------------   |
       Environment                          |
     ------------------------------------   |
       Entry Point                          |  Savearea 
     ------------------------------------   |  96 bytes
       Return Address                       |
     ------------------------------------   |
       R8 - R15                             |
     ------------------------------------ <-
       Reserved (16 bytes)                   +2144
     ------------------------------------ 
       Debug Area (8 bytes)                 +2160
     ------------------------------------ 
       Arg area prefix (8 bytes)            +2168
     ------------------------------------ 
       Argument area: Parm1 ... ParmN       +2176
     ------------------------------------ 
       Local (automatic storage)
      Saved FPRs   Saved  ARs   Saved VRs 
     ------------------------------------ <- High Addresses
  */
  int i;
  ffi_type **type_ptr;
  void **p_argv = ecif->avalue;
  unsigned char* arg_ptr = stack;
 
#ifdef FFI_DEBUG
  printf("prep_args: stack=%x, extended_cif=%x\n",stack,ecif);
#endif
  /* If we returning a structure larger than 12bytes,
     we set the first parameter register
     to the address of where we are returning this structure.  
   */  
  if (ecif->cif->flags == FFI_TYPE_STRUCT &&
      (ecif->cif->rtype->size > 12)){
     arg_ptr += (unsigned long) ecif->rvalue;

  }
   /*Now for the arguments.  */
 
  for (type_ptr = ecif->cif->arg_types, i = ecif->cif->nargs;
       i > 0;
       i--, type_ptr++, p_argv++)
    {
      void *arg = *p_argv;
      int type = (*type_ptr)->type;
      int size = (*type_ptr)->size;

     /*  Check how a structure type is passed.   */
      // if (type == FFI_TYPE_STRUCT) {
      //                           memcpy(arg_ptr, (char*)p_argv, (*type_ptr)->size);
      //                           arg_ptr += (*type_ptr)->size;
      //                           continue;
      // }

     /*  Now handle all primitive int/pointer/float data types.  */
      switch (type) 
        {

          case FFI_TYPE_STRUCT:
          case FFI_TYPE_COMPLEX:
            memcpy(arg_ptr, *p_argv, size);
            break;

#if FFI_TYPE_LONGDOUBLE != FFI_TYPE_DOUBLE 
          case FFI_TYPE_LONGDOUBLE: 
            *(long double *) arg_ptr = * (long double *) (*p_argv);
            break;
#endif

          case FFI_TYPE_DOUBLE:
            *(double *) arg_ptr = * (double *) (*p_argv);
            break;
        
          case FFI_TYPE_FLOAT:
            *(float *) arg_ptr = * (float *) (*p_argv);
            break;

          case FFI_TYPE_POINTER:
            *(void **) arg_ptr = *(void**) (* p_argv); 
            break;
 
          case FFI_TYPE_SINT64:
            *(signed long long *) arg_ptr = * (signed long long *) (* p_argv);
            break;

          case FFI_TYPE_UINT64:
            *(unsigned long long *) arg_ptr = * (unsigned long long *) (* p_argv);
            break;
 
          case FFI_TYPE_UINT32:
            *(unsigned int *) arg_ptr = * (unsigned int *) (*p_argv);
            break;
 
          case FFI_TYPE_SINT32:
          case FFI_TYPE_INT:
            *(signed int *) arg_ptr = * (signed int *) (*p_argv);
            break;
 
          case FFI_TYPE_UINT16:
            *(unsigned short *) arg_ptr = * (unsigned short *) (* p_argv);
            // arg_ptr += 2;
            break;
 
          case FFI_TYPE_SINT16:
            *(signed short *) arg_ptr = * (signed short *) (* p_argv);
            // arg_ptr += 2;
            break;

          case FFI_TYPE_UINT8:
            *(unsigned char *) arg_ptr = * (unsigned char *) (* p_argv);
	          // arg_ptr += 3;
            break;
 
          case FFI_TYPE_SINT8:
            *(signed char *) arg_ptr = * (signed char*) (* p_argv);
	          // arg_ptr += 3;
            break;
 
          default:
            FFI_ASSERT (0);
            break;
        }
      arg_ptr += size;

    }

}

/*======================== End of Routine ============================*/
 
/*====================================================================*/
/*                                                                    */
/* Name     - unwrap_single_element_struct.                           */
/*                                                                    */
/* Function - Unwrap nested single-element struct wrappers to reach   */
/*            the leaf type.                                          */
/*                                                                    */
/*====================================================================*/

static ffi_type *
unwrap_single_element_struct(ffi_type *t)
{
  while (t->type == FFI_TYPE_STRUCT
         && t->elements[0] != NULL
         && t->elements[1] == NULL)
    t = t->elements[0];
  return t;
}

/*======================== End of Routine ============================*/

/*====================================================================*/
/*                                                                    */
/* Name     - ffi_struct_float_pair_type.                             */
/*                                                                    */
/* Function - Determine whether a struct matches the z/OS XPLINK HFA */
/*            (homogeneous float aggregate) rule.                     */
/*            Returns FFI_TYPE_FLOAT, FFI_TYPE_DOUBLE,                */
/*            FFI_TYPE_LONGDOUBLE, or 0.                              */
/*                                                                    */
/*====================================================================*/

unsigned int
ffi_struct_float_pair_type (ffi_type **argp)
{
  ffi_type *arg = *argp;

  if (arg->type != FFI_TYPE_STRUCT)
    return 0;

  /* Must have exactly two direct elements (elements[2] == NULL). */
  if (arg->elements[0] == NULL || arg->elements[1] == NULL
      || arg->elements[2] != NULL)
    return 0;

  /* Unwrap single-element struct wrappers to get leaf types */
  ffi_type *leaf0 = unwrap_single_element_struct(arg->elements[0]);
  ffi_type *leaf1 = unwrap_single_element_struct(arg->elements[1]);

  unsigned int t0 = leaf0->type;
  unsigned int t1 = leaf1->type;

  if (t0 != t1)
    return 0;

  if (t0 == FFI_TYPE_FLOAT || t0 == FFI_TYPE_DOUBLE
      || t0 == FFI_TYPE_LONGDOUBLE)
    return t0;

  return 0;
}

/*======================== End of Routine ============================*/

/*====================================================================*/
/*                                                                    */
/* Name     - ffi_dummy.                                              */
/*                                                                    */
/* Function - Intentional no-op; used from xplink.S STRCTDISP to     */
/*            probe that CELQCALL works correctly in that context.    */
/*                                                                    */
/*====================================================================*/

void
ffi_dummy (void)
{
  printf("abc\n");
}

/*======================== End of Routine ============================*/

/*====================================================================*/
/*                                                                    */
/* Name     - ffi_prep_cif_machdep.                                   */
/*                                                                    */
/* Function - Perform machine dependent CIF processing.               */
/*                                                                    */
/*====================================================================*/
 
ffi_status
ffi_prep_cif_machdep(ffi_cif *cif)
{

  // preemptively set nfixedargs to nargs, so that the default is correct for non-variadic functions
  // we will correct this in ffi_prep_cif_machdep_var if this is a variadic function
  cif->nfixedargs = cif->nargs;

  ffi_type **ptr;
  int i;

  /* 64-bit XPLINK handling below */

  /* Determine return value handling. 

     Integral values <=8bytes are widened and put in GPR3
    
     Integral values >8bytes and <=16bytes are widened and put in
     GPR2 (left most 64-bits) and GPR3 (right most 64-bits)
    
     Floating point values, including complex type, are returned in 
     FPR0, FPR2, FPR4, FPR6 (as many registers as required)
     
     Aggregates not return in floats are handled as follows: 
     
     Aggregates size of <=8 are returned GPR1 (left adjusted)
     Aggregates size between 9 bytes- 16 bytes are returned in GPR1 and 
     GPR2 (left adjusted)
     Aggregates size between 17 bytes - 24 bytes are returned in GPR1, GPR2,
     and GPR3 (left adjusted)
     
     Anything greater in size and any other type is returned in a buffer,
     the buffer is passed in as hidden first argument (ie. in GPR1).
     */

  switch (cif->rtype->type)
    {
      /* Void is easy.  */
      case FFI_TYPE_VOID:
        cif->flags = FFI390_RET_VOID;
        break;

      /* Structures are returned in GPR or buffer depending on size.
         A struct whose only scalar leaves are exactly two values of the
         same float/double/longdouble type is returned in FPR pairs.
         ffi_struct_float_pair_type recurses through any nesting depth.  */
      case FFI_TYPE_STRUCT:
        {
          unsigned int fp = ffi_struct_float_pair_type (&cif->rtype);
          if (fp == FFI_TYPE_FLOAT)
            cif->flags = FFI390_RET_STRUCT_FF;
          else if (fp == FFI_TYPE_DOUBLE)
            cif->flags = FFI390_RET_STRUCT_DD;
          else if (fp == FFI_TYPE_LONGDOUBLE)
            cif->flags = FFI390_RET_STRUCT_LDLD;
          else
            cif->flags = FFI390_RET_STRUCT;
        }
        break;

      /* Scalar float/double/long-double */
      case FFI_TYPE_FLOAT:
        cif->flags = FFI390_RET_FLOAT;
        break;

      case FFI_TYPE_DOUBLE:
        cif->flags = FFI390_RET_DOUBLE;
        break;

#if FFI_TYPE_LONGDOUBLE != FFI_TYPE_DOUBLE
      case FFI_TYPE_LONGDOUBLE:
        cif->flags = FFI390_RET_LDBLE;
        break;
#endif

      /* Complex types: returned in FPR pairs for float/double/longdouble elements.
         Integer-based complex (e.g. _Complex int): real in GPR3, imag in GPR2.  */
      case FFI_TYPE_COMPLEX:
        {
          unsigned int fp = (cif->rtype->elements && cif->rtype->elements[0])
                            ? cif->rtype->elements[0]->type : 0;
          if (fp == FFI_TYPE_FLOAT)
            cif->flags = FFI390_RET_STRUCT_FF;
          else if (fp == FFI_TYPE_DOUBLE)
            cif->flags = FFI390_RET_STRUCT_DD;
          else if (fp == FFI_TYPE_LONGDOUBLE)
            cif->flags = FFI390_RET_STRUCT_LDLD;
          else
            cif->flags = FFI390_RET_COMPLEX_INT; /* GPR3=real, GPR2=imag */
        }
        break;

      /* Integer values are returned in GPR3.  */
      case FFI_TYPE_UINT64:
      case FFI_TYPE_SINT64:
      case FFI_TYPE_POINTER:
      case FFI_TYPE_INT:
      case FFI_TYPE_UINT32:
      case FFI_TYPE_SINT32:
      case FFI_TYPE_UINT16:
      case FFI_TYPE_SINT16:
      case FFI_TYPE_UINT8:
      case FFI_TYPE_SINT8:
        cif->flags = FFI390_RET_INT64;
        break;
      default:
        FFI_ASSERT (0);
        break;
    }

  /* Compute the required argument area size.
   *
   * Every argument occupies a slot whose size is:
   *   - 8 bytes if the argument's type size is <= 8 (fits in one register slot,
   *     right-adjusted big-endian within the slot)
   *   - rounded up to the next multiple of 8 if the argument's type size > 8
   *     (e.g. long double = 16, a 24-byte struct = 24)
   *
   * A hidden return pointer for structs > 24 bytes also occupies one 8-byte slot.
   * The total must be a multiple of 32 and at least 32 bytes (ABI minimum of
   * 4 doublewords).
   */
  unsigned int bytes = 0;

  /* Hidden return pointer for large struct returns. */
  if (cif->rtype->type == FFI_TYPE_STRUCT && cif->rtype->size > 24)
    bytes += 8;

  for (ptr = cif->arg_types, i = cif->nargs; i > 0; i--, ptr++)
    {
      size_t size = (*ptr)->size;
      if (size <= 8)
        bytes += 8;
      else
        bytes += (size + 7) & ~7;  /* round up to multiple of 8 */
    }

  if (bytes < 32)
    bytes = 32;
  cif->bytes = (bytes + 31) & ~31;  /* round up to multiple of 32 */

#ifdef FFI_DEBUG
  printf("prep_cif_machdep_cif_bytes: %u\n", cif->bytes);
#endif
  return FFI_OK;
}
 
/*======================== End of Routine ============================*/

/*====================================================================*/
/*                                                                    */
/* Name     - ffi_prep_cif_machdep_var.                               */
/*                                                                    */
/* Function - Perform machine dependent CIF processing for variadic   */
/*            functions.                                              */
/*                                                                    */
/*====================================================================*/

ffi_status
ffi_prep_cif_machdep_var(ffi_cif *cif,
                         unsigned int nfixedargs,
                         unsigned int ntotalargs)
{
  ffi_status status = ffi_prep_cif_machdep(cif);
  cif->nfixedargs = nfixedargs;
  return status;
}

/*======================== End of Routine ============================*/
 
/*====================================================================*/
/*                                                                    */
/* Name     - ffi_call.                                               */
/*                                                                    */
/* Function - Call the FFI routine.                                   */
/*                                                                    */
/*====================================================================*/
 
void
ffi_call(ffi_cif *cif,
         void (*fn)(void),
         void *rvalue,
         void **avalue)
{
  int ret_type = cif->flags;
  extended_cif ecif;
 
  ecif.cif    = cif;
  ecif.avalue = avalue;
  ecif.rvalue = rvalue;

#ifdef FFI_DEBUG
  printf("ffi_call: cif=%p{%d}, fn=%p, rvalue=%p, aavalue=%p\n", cif, cif->abi, fn, rvalue, avalue);
#endif 

  /* If we don't have a return value, we need to fake one.  */
  if (rvalue == NULL)
    {
      /* For struct returns that use in-register/in-memory return (not a
         hidden-pointer return), the assembly still writes the result through
         ecif.rvalue.  Provide scratch space so we don't store to NULL.
         For hidden-pointer returns (flags == FFI_TYPE_STRUCT with size > 24)
         the hidden pointer was already passed as arg, so just discard.  */
      if (cif->flags == FFI390_RET_STRUCT && cif->rtype->size <= 24)
        ecif.rvalue = alloca (cif->rtype->size);
      else
        ret_type = FFI_TYPE_VOID;
    }

  switch (cif->abi)
    {
      case FFI_XPLINK:
        {
          unsigned lstor_size = 24; // initialize with enough space for 3 GPRs
          for (unsigned i = 0; i < cif->nargs; i++)
            lstor_size += cif->arg_types[i]->size;
          
          ffi_call_SYSV(fn, &ecif, cif->flags, ecif.rvalue, cif->bytes,
                        cif->nargs, cif->nfixedargs,
                        alloca(lstor_size));

#ifdef FFI_DEBUG
          printf("called_ffi_call_sysv nargs=%d\n",cif->nargs);
#endif
        }
        break;
      default:
        FFI_ASSERT (0);
        break;
    }
}
 
/*======================== End of Routine ============================*/

/*====================================================================*/
/*                                                                    */
/* Name     - ffi_determine_return_type.                              */
/*                                                                    */
/* Function - Map closure->cif->flags to a closure-side return-       */
/*            dispatch index for closure_xplink.S RTABLE.             */
/*                                                                    */
/*====================================================================*/

int ffi_determine_return_type(ffi_closure *closure)
{
  fprintf(stderr, "DETRET entry: closure=%p flags=%u\n",
          (void*)closure, closure->cif->flags);
  fflush(stderr);
  /* Use the flags already computed by ffi_prep_cif_machdep, they are the
     canonical return classification and keep call-side and closure-side
     dispatch tables in sync without re-classifying.  */
  switch (closure->cif->flags)
    {
      case FFI390_RET_VOID:
        return 1;   /* no meaningful return value; GPR3 is harmless */

      case FFI390_RET_INT32:
      case FFI390_RET_INT64:
        fprintf(stderr, "DETRET: INT return path, returning 1\n"); fflush(stderr);
        return 1;   /* GPR3 */

      case FFI390_RET_FLOAT:
        return 5;   /* FPR0 */

      case FFI390_RET_DOUBLE:
        return 5;   /* FPR0 */

      case FFI390_RET_LDBLE:
        return 6;   /* FPR0+FPR2 (long-double is 128-bit, two DFP regs) */

      case FFI390_RET_STRUCT_FF:
        return 7;   /* FPR0 (float) + FPR2 (float), 4-byte strides */

      case FFI390_RET_STRUCT_DD:
        return 6;   /* FPR0 + FPR2, 8-byte strides */

      case FFI390_RET_STRUCT_LDLD:
        return 8;   /* FPR0+FPR2+FPR4+FPR6 */

      case FFI390_RET_STRUCT:
        {
          size_t sz = closure->cif->rtype->size;
          if (sz <= 8)  return 2;
          if (sz <= 16) return 3;
          if (sz <= 24) return 4;
          return 0;   /* hidden pointer was passed; callee wrote result */
        }

      case FFI390_RET_COMPLEX_INT:
        return 9;   /* GPR3=real, GPR2=imag */

      default:
        return 0;
    }
}


/*======================== End of Routine ============================*/

/*====================================================================*/
/*                                                                    */
/* Name     - ffi_closure_helper_SYSV.                                */
/*                                                                    */
/* Function - Call a FFI closure target function.                     */
/*                                                                    */
/*====================================================================*/
 
 void
ffi_closure_helper_SYSV (ffi_closure *closure, void *retbuf, struct ffi_reg_data *reg)
{
  fprintf(stderr, "CLSRHLP: closure=%p retbuf=%p reg=%p\n",
          (void*)closure, (void*)retbuf, (void*)reg);
  fflush(stderr);

  /* ---- CLOSURE POINTER SANITY ----------------------------------------- */
  int ret_size;
  ffi_cif *cif = closure->cif;
  fprintf(stderr, "CLSRHLP: cif=%p fun=%p user_data=%p\n",
          (void*)cif, (void*)closure->fun, closure->user_data);
  fprintf(stderr, "CLSRHLP: cif->flags=%u nargs=%d nfixedargs=%u bytes=%u\n",
          cif->flags, cif->nargs, cif->nfixedargs, cif->bytes);
  fprintf(stderr, "CLSRHLP: rtype=%p rtype->type=%u rtype->size=%zu\n",
          (void*)cif->rtype,
          cif->rtype ? (unsigned)cif->rtype->type : 99u,
          cif->rtype ? cif->rtype->size : 0);
  for (int _i = 0; _i < 16; _i++)
    fprintf(stderr, "CLSRHLP: gpr[%2d]=%p\n", _i, (void*)reg->gpr[_i]);
  { uint64_t _r;
    for (int _i = 0; _i < 7; _i++) {
      memcpy(&_r, &reg->fpr[_i], 8);
      fprintf(stderr, "CLSRHLP: fpr[%d]=0x%016llx\n", _i, (unsigned long long)_r);
    }
  }
  fflush(stderr);

  unsigned short struct_subtype = FFI_TYPE_VOID;
  if (cif->rtype)
    struct_subtype = cif->rtype->type;

  int num_args = cif->nargs;
  if (cif->rtype == NULL || cif->rtype->type == FFI_TYPE_VOID)
    ret_size = 8;
  else if (cif->flags == FFI390_RET_STRUCT_LDLD)
    ret_size = 32;
  else
    ret_size = cif->rtype->size;

  // allocate a pointer per arg
  void **args = (void**)alloca(sizeof(void*) * num_args);
  void *ret = (void*)alloca(ret_size);
  char *savearea = ((char*)reg->gpr[3]) + 2176;

#ifdef FFI_CLOSURE_DEBUG
  /* Dump incoming register state so we can verify what the trampoline
     captured.  Gate behind FFI_CLOSURE_DEBUG to avoid noise in normal
     builds.  Enable with -DFFI_CLOSURE_DEBUG at compile time.        */
  fprintf(stderr, "CLSRHLP entry: closure=%p retbuf=%p reg=%p\n",
          (void*)closure, retbuf, (void*)reg);
  fprintf(stderr, "  sizeof(ffi_cif)=%zu sizeof(ffi_closure)=%zu\n",
          sizeof(ffi_cif), sizeof(ffi_closure));
  fprintf(stderr, "  cif=%p  cif raw bytes[0..31]:", (void*)cif);
  { unsigned char *_cp = (unsigned char*)cif;
    for (int _b = 0; _b < 32; _b++) fprintf(stderr, " %02x", _cp[_b]);
    fprintf(stderr, "\n"); }
  fprintf(stderr, "  cif->abi=%u nargs=%d arg_types=%p rtype=%p bytes=%u flags=%u nfixedargs=%u\n",
          (unsigned)cif->abi, cif->nargs, (void*)cif->arg_types,
          (void*)cif->rtype, cif->bytes, cif->flags, cif->nfixedargs);
  if (cif->arg_types && cif->nargs > 0)
    fprintf(stderr, "  cif->arg_types[0]=%p\n", (void*)cif->arg_types[0]);
  fprintf(stderr, "  cif->flags=%u nargs=%d rtype->type=%u ret_size=%d\n",
          cif->flags, num_args,
          cif->rtype ? (unsigned)cif->rtype->type : 99u, ret_size);
  /* GPRs 0-3 (the ones that carry incoming args / stack ptr) */
  for (int _d = 0; _d < 4; _d++)
    fprintf(stderr, "  gpr[%d] = %p\n", _d, (void*)reg->gpr[_d]);
  /* FPRs 0..6 as stored by STD in the trampoline; show value and raw high 32 bits */
  for (int _d = 0; _d < 7; _d++) {
    uint64_t _raw; memcpy(&_raw, &reg->fpr[_d], 8);
    fprintf(stderr, "  fpr[%d] = %.17g  (raw hi32=0x%08x lo32=0x%08x)\n",
            _d, reg->fpr[_d],
            (unsigned)(_raw >> 32), (unsigned)(_raw & 0xffffffff));
  }
  fprintf(stderr, "  savearea=%p\n", (void*)savearea);
  /* First 32 bytes of the savearea (first 4 arg slots) */
  { unsigned char *_p = (unsigned char*)savearea;
    fprintf(stderr, "  savearea[0..31]:");
    for (int _b = 0; _b < 32; _b++) fprintf(stderr, " %02x", _p[_b]);
    fprintf(stderr, "\n"); }
  fflush(stderr);
#endif /* FFI_CLOSURE_DEBUG */
  
  // incase we need to pass a complex type, messy but functional
  float *cmplx_arg;
  double *cmplx_arg_double;

  long double *arg_longdouble;

  // This possible causes a compiler bug?
  ffi_type **atype = cif->arg_types;

  // number of remaining registers
  int n_gprs = 3;
  int n_fprs = 4;
  // offset into the xplink save area
  int n_arg = 0;
  // fixed-arg countdown for variadic functions: when this hits 0 FPRs are exhausted
  // for subsequent args (matching xplink.S VARFNC logic)
  int n_fixed_remaining = (int)cif->nfixedargs;

  if (ret_size > 24 && cif->flags != FFI390_RET_STRUCT_LDLD) {
    // if we have a return pointer
    // passed into r1
    n_gprs--;
    n_arg += 8;
    ret = reg->gpr[0];
  }

  for (int i = 0; i < cif->nargs; i++) {
#ifdef FFI_CLOSURE_DEBUG
    fprintf(stderr, "  arg[%d]: atype=%p atype->type=%u size=%zu\n",
            i, (void*)atype[i],
            atype[i] ? (unsigned)atype[i]->type : 99u,
            atype[i] ? atype[i]->size : 0);
    fflush(stderr);
#endif
    switch (atype[i]->type) {
      case FFI_TYPE_UINT8:
      case FFI_TYPE_SINT8:
        if (n_gprs == 3) {
          args[i] = (void*)(((long)(&reg->gpr[0])) + 7);
          n_gprs--;
          n_arg += 8;
        }
        else if (n_gprs == 2) { 
          args[i] = (void*)(((long)(&reg->gpr[1])) + 7);
          n_gprs--;
          n_arg += 8;
        }
        else if (n_gprs == 1) { 
          args[i] = (void*)(((long)(&reg->gpr[2])) + 7);
          n_gprs--;
          n_arg += 8;
        } 
        else { 
          args[i] = (savearea + n_arg + 7);
          n_arg += 8;
        }
        break;

      case FFI_TYPE_UINT16:
      case FFI_TYPE_SINT16:
        if (n_gprs == 3) {
          args[i] = (void*)(((long)(&reg->gpr[0])) + 6);
          n_gprs--;
          n_arg += 8;
        }
        else if (n_gprs == 2) { 
          args[i] = (void*)(((long)(&reg->gpr[1])) + 6);
          n_gprs--;
          n_arg += 8;
        }
        else if (n_gprs == 1) { 
          args[i] = (void*)(((long)(&reg->gpr[2])) + 6);
          n_gprs--;
          n_arg += 8;
        } 
        else { 
          args[i] = (savearea + n_arg + 6);
          n_arg += 8;
        }
        break;

      case FFI_TYPE_UINT32:
      case FFI_TYPE_SINT32:
      case FFI_TYPE_INT:
        if (n_gprs == 3) {
          args[i] = (void*)(((long)(&reg->gpr[0])) + 4); 
          n_gprs--;
          n_arg += 8;
        }
        else if (n_gprs == 2) { 
          args[i] = (void*)(((long)(&reg->gpr[1])) + 4); 
          n_gprs--;
          n_arg += 8;
        }
        else if (n_gprs == 1) { 
          args[i] = (void*)(((long)(&reg->gpr[2])) + 4); 
          n_gprs--;
          n_arg += 8;
        } 
        else { 
          args[i] = (savearea + n_arg + 4);
          n_arg += 8;
        }
        break;
      case FFI_TYPE_UINT64:
      case FFI_TYPE_SINT64:
      case FFI_TYPE_POINTER:
        if (n_gprs == 3) {
          args[i] = &reg->gpr[0];
          n_gprs--;
          n_arg += 8;
        }
        else if (n_gprs == 2) { 
          args[i] = &reg->gpr[1]; 
          n_gprs--;
          n_arg += 8;
        }
        else if (n_gprs == 1) { 
          args[i] = &reg->gpr[2];
          n_gprs--;
          n_arg += 8;
        } 
        else { 
          args[i] = (savearea + n_arg);
          n_arg += 8;
        }
        break;
      
      case FFI_TYPE_FLOAT:
      case FFI_TYPE_DOUBLE:
        if (n_fixed_remaining == 0) {
          /* variadic: floats/doubles passed in GPR/memory, not FPRs */
          if (n_gprs > 0) {
            args[i] = &reg->gpr[3 - n_gprs];
            n_gprs--;
          } else {
            args[i] = (savearea + n_arg);
          }
          n_arg += 8;
        }
        else if (n_fprs == 4) {
          args[i] = &reg->fpr[0];
          n_gprs--;
          n_fprs--;
          n_arg += 8;
        }
        else if (n_fprs == 3) {
          args[i] = &reg->fpr[2];
          n_gprs--;
          n_fprs--;
          n_arg += 8;
        }
        else if (n_fprs == 2) {
          args[i] = &reg->fpr[4];
          n_gprs--;
          n_fprs--;
          n_arg += 8;
        }
        else if (n_fprs == 1) {
          args[i] = &reg->fpr[6];
          n_gprs--;
          n_fprs--;
          n_arg += 8;
        }
        else {
          args[i] = (savearea + n_arg);
          n_arg += 8;
        }
        break;
      case FFI_TYPE_LONGDOUBLE:
        if (n_fixed_remaining == 0) {
          /* variadic: long double passed as 16-byte opaque value in GPR/memory */
          args[i] = (savearea + n_arg);
          n_arg += 16;
        }
        else if (n_fprs == 4) {
          arg_longdouble = alloca(sizeof(long double));
          ((double *) arg_longdouble)[0] = reg->fpr[0];
          ((double *) arg_longdouble)[1] = reg->fpr[2];
          args[i] = arg_longdouble;
          n_gprs -= 2;
          n_fprs -= 2;
          n_arg += 16;
        }
        else if (n_fprs == 3 || n_fprs == 2) {
          arg_longdouble = alloca(sizeof(long double));
          ((double *) arg_longdouble)[0] = reg->fpr[4];
          ((double *) arg_longdouble)[1] = reg->fpr[6];
          args[i] = arg_longdouble;
          n_gprs -= 2;
          n_fprs -= 2;
          n_arg += 16;
        }
        else {
          args[i] = (savearea + n_arg);
          n_arg += 16;
        }
        break;
      case FFI_TYPE_STRUCT:
      case FFI_TYPE_COMPLEX:
 {
      /* For _Complex T the element type is in elements[0]; for a struct
         use the recursive leaf checker.  Either way fp == FFI_TYPE_FLOAT/
         DOUBLE/LONGDOUBLE means FPR-pair passing, 0 means GPR/memory.  */
      unsigned int fp;
        if (atype[i]->type == FFI_TYPE_COMPLEX)
          fp = (atype[i]->elements && atype[i]->elements[0])
               ? atype[i]->elements[0]->type : 0;
        else
          fp = ffi_struct_float_pair_type(&atype[i]);
#ifdef FFI_CLOSURE_DEBUG
        fprintf(stderr,
          "  STRUCT dispatch arg[%d]: atype->type=%u fp=%u n_fprs=%d n_gprs=%d\n",
          i, (unsigned)atype[i]->type,
          fp, n_fprs, n_gprs);
        fflush(stderr);
#endif
#ifdef FFI_CLOSURE_DEBUG
        fprintf(stderr, "  fp-branch: fp=%u (0=none 2=FLOAT 3=DBL 4=LD)\n", fp);
        fflush(stderr);
#endif
        if (fp == 0) {
#ifdef FFI_CLOSURE_DEBUG
        fprintf(stderr, "  --> fp==0 (GPR/mem) size=%zu\n", atype[i]->size);
        fflush(stderr);
#endif
        // TODO fill this in
        if (atype[i]->size <= 8) {
          // handle like an int basically
          if (n_gprs == 3) {
#ifdef FFI_CLOSURE_DEBUG
            fprintf(stderr, "    fp0: size<=8 n_gprs==3 -> &gpr[0]=%p\n", (void*)&reg->gpr[0]); fflush(stderr);
#endif
            args[i] = &reg->gpr[0];
            n_gprs--;
            n_arg += 8;
          }
          else if (n_gprs == 2) { 
            args[i] = &reg->gpr[1]; 
            n_gprs--;
            n_arg += 8;
          }
          else if (n_gprs == 1) { 
            args[i] = &reg->gpr[2];
            n_gprs--;
            n_arg += 8;
          } 
          else { 
            // TODO, some weird stuff happens here I think...
            // can padding happen?
            args[i] = (savearea + n_arg);
            n_arg += atype[i]->size;
          }
        }
        else if (atype[i]->size <= 16) {
          if (n_gprs == 3) {
            args[i] = &reg->gpr[0];
            n_gprs -= 2;
            n_arg += 16;
          }
          else if (n_gprs == 2) { 
            args[i] = &reg->gpr[1]; 
            n_gprs -= 2;
            n_arg += 16;
          }
          else if (n_gprs == 1) { 
            // so we have overlap with closure->reg+memory
            // think we can just save to save area and use that
            *(long*)(savearea + n_arg) = (long)reg->gpr[2];
            args[i] = (savearea + n_arg);
            n_gprs--;
            n_arg += 16;
          } 
          else { 
            args[i] = (savearea + n_arg);
            n_arg += atype[i]->size;
          }
        }
        else if (atype[i]->size <= 24) {
          if (n_gprs == 3) {
            args[i] = &reg->gpr[0];
            n_gprs -= 3;
            n_arg += 24;
          }
          else if (n_gprs == 2) {
            *(long*)(savearea + n_arg) = (long)reg->gpr[1];
            *(long*)(savearea + n_arg + 8) = (long)reg->gpr[2];
            args[i] = (savearea + n_arg);
            n_gprs -= 2;
            n_arg += 24;
          }
          else if (n_gprs == 1) {
            *(long*)(savearea + n_arg) = (long)reg->gpr[2];
            args[i] = (savearea + n_arg);
            n_gprs--;
            n_arg += 24;
          }
          else {
            args[i] = (savearea + n_arg);
            n_arg += atype[i]->size;
          }
        }
        else {
          // really big
          if (n_gprs == 3) {
            *(long*)(savearea + n_arg) = (long)reg->gpr[0];
            *(long*)(savearea + n_arg + 8) = (long)reg->gpr[1];
            *(long*)(savearea + n_arg + 16) = (long)reg->gpr[2];
            args[i] = (savearea + n_arg);
            n_gprs -= 3;
            n_arg += atype[i]->size;
          }
          else if (n_gprs == 2) {
            *(long*)(savearea + n_arg) = (long)reg->gpr[1];
            *(long*)(savearea + n_arg + 8) = (long)reg->gpr[2];
            args[i] = (savearea + n_arg);
            n_gprs -= 2;
            n_arg += atype[i]->size;
          }
          else if (n_gprs == 1) {
            *(long*)(savearea + n_arg) = (long)reg->gpr[2];
            args[i] = (savearea + n_arg);
            n_gprs--;
            n_arg += atype[i]->size;
          }
          else {
            args[i] = (savearea + n_arg);
            // TODO deal with padding
            n_arg += atype[i]->size;
          }
        }
    }
        else if(fp == FFI_TYPE_DOUBLE){
#ifdef FFI_CLOSURE_DEBUG
        fprintf(stderr, "  --> fp==DOUBLE n_fprs=%d n_fixed_remaining=%d\n", n_fprs, n_fixed_remaining); fflush(stderr);
#endif
        if (n_fixed_remaining == 0) {
          /* variadic: _Complex double passed as 16-byte opaque value in GPR/memory */
          args[i] = (savearea + n_arg);
          n_arg += 16;
        }
        else if (n_fprs == 4) {
          args[i] = &reg->fpr[0];
          reg->fpr[1] = reg->fpr[2];
          n_gprs--;
          n_fprs -= 2;
          n_arg += 16;
        }
        else if (n_fprs == 3) {
          args[i] = &reg->fpr[2];
          reg->fpr[3] = reg->fpr[4];
          n_gprs--;
          n_fprs -= 2;
          n_arg += 16;
        }
        else if (n_fprs == 2) {
          args[i] = &reg->fpr[4];
          reg->fpr[5] = reg->fpr[6];
          n_gprs--;
          n_fprs -= 2;
          n_arg += 16;
        }
        else if (n_fprs == 1) {
          cmplx_arg_double = alloca(2*sizeof(double));
          args[i] = cmplx_arg_double;
          cmplx_arg_double[0] = reg->fpr[6];
          cmplx_arg_double[1] = *(double*)(savearea + n_arg + 8);
          n_gprs--;
          n_fprs -= 2;
          n_arg += 16;
        }
        else {
          args[i] = (savearea + n_arg);
          n_arg += 16;
        }
        }
        else if(fp == FFI_TYPE_FLOAT){
#ifdef FFI_CLOSURE_DEBUG
            fprintf(stderr, "  --> fp==FLOAT n_fprs=%d n_fixed_remaining=%d\n", n_fprs, n_fixed_remaining); fflush(stderr);
#endif
            cmplx_arg = alloca(2*sizeof(float));
#ifdef FFI_CLOSURE_DEBUG
            fprintf(stderr, "    alloca -> cmplx_arg=%p\n", (void*)cmplx_arg); fflush(stderr);
#endif
            if (n_fixed_remaining == 0) {
#ifdef FFI_CLOSURE_DEBUG
            fprintf(stderr, "    FLOAT: variadic path, args[i]=savearea+%d\n", n_arg); fflush(stderr);
#endif
            /* variadic: _Complex float passed as 8-byte opaque value in GPR/memory */
            args[i] = (savearea + n_arg);
            n_arg += 8;
            }
            else if (n_fprs == 4) {
#ifdef FFI_CLOSURE_DEBUG
            fprintf(stderr, "    FLOAT: n_fprs==4 path\n"); fflush(stderr);
#endif
            args[i] = cmplx_arg;
            cmplx_arg[0] = *(float*)&reg->fpr[0];
            cmplx_arg[1] = *(float*)&reg->fpr[2];
#ifdef FFI_CLOSURE_DEBUG
            { uint64_t _r0, _r2; memcpy(&_r0,&reg->fpr[0],8); memcpy(&_r2,&reg->fpr[2],8);
              fprintf(stderr, "    wrote: [0]=0x%08x [1]=0x%08x  fpr0_raw=0x%016llx fpr2_raw=0x%016llx\n",
                *(unsigned*)&cmplx_arg[0], *(unsigned*)&cmplx_arg[1],
                (unsigned long long)_r0, (unsigned long long)_r2); fflush(stderr); }
#endif
            n_gprs--;
            n_fprs -= 2;
            n_arg += 8;
            }
            else if ((n_fprs == 3)) {
#ifdef FFI_CLOSURE_DEBUG
            fprintf(stderr, "    FLOAT: n_fprs==3 path\n"); fflush(stderr);
#endif
            args[i] = cmplx_arg;
            cmplx_arg[0] = *(float*)&reg->fpr[2];
            cmplx_arg[1] = *(float*)&reg->fpr[4];
            n_gprs--;
            n_fprs -= 2;
            n_arg += 8;
            }
            else if ((n_fprs == 2)) {
#ifdef FFI_CLOSURE_DEBUG
            fprintf(stderr, "    FLOAT: n_fprs==2 path\n"); fflush(stderr);
#endif
            args[i] = cmplx_arg;
            cmplx_arg[0] = *(float*)&reg->fpr[4];
            cmplx_arg[1] = *(float*)&reg->fpr[6];
            n_gprs--;
            n_fprs -= 2;
            n_arg += 8;
            }
            else if (n_fprs == 1) {
#ifdef FFI_CLOSURE_DEBUG
            fprintf(stderr, "    FLOAT: n_fprs==1 path\n"); fflush(stderr);
#endif
            args[i] = cmplx_arg;
            cmplx_arg[0] = *(float*)&reg->fpr[6];
            cmplx_arg[1] = *(float*)(savearea + n_arg + 4);
            n_gprs--;
            n_fprs -= 2;
            n_arg += 8;
            }
            else {
#ifdef FFI_CLOSURE_DEBUG
            fprintf(stderr, "    FLOAT: n_fprs==0 (mem) path\n"); fflush(stderr);
#endif
            args[i] = (savearea + n_arg);
            cmplx_arg = (savearea + n_arg);
            n_arg += 8;
            }
        }
        else if(fp == FFI_TYPE_LONGDOUBLE){
            if (n_fixed_remaining == 0) {
            /* variadic: _Complex long double passed as 32-byte opaque value in memory */
            args[i] = (savearea + n_arg);
            n_arg += 32;
            }
            else if (n_fprs == 4) {
                arg_longdouble = alloca(2*sizeof(long double));
                ((double *) arg_longdouble)[0] = reg->fpr[0];
                ((double *) arg_longdouble)[1] = reg->fpr[2];
                ((double *) arg_longdouble)[2] = reg->fpr[4];
                ((double *) arg_longdouble)[3] = reg->fpr[6];
                args[i] = arg_longdouble;
                n_gprs = 0;
                n_fprs = 0;
                n_arg += 32;
            }
     else if(n_fprs == 2){
                arg_longdouble = alloca(2*sizeof(long double));
                ((double *) arg_longdouble)[0] = reg->fpr[4];
                ((double *) arg_longdouble)[1] = reg->fpr[6];
                ((double *) arg_longdouble)[2] = *(double *)(savearea + n_arg + 16);
                ((double *) arg_longdouble)[3] = *(double *)(savearea + n_arg + 24);
                args[i] = arg_longdouble;
                n_gprs = 0;
                n_fprs = 0;
                n_arg += 32;
     }
            else {
                args[i] = (savearea + n_arg);
                n_gprs = 0;
                n_fprs = 0;
                n_arg += 32;
            }
        }
        break; 
	}
    }

    /* Variadic fence: mirror xplink.S CONT/VARFNC logic.
       Decrement the fixed-arg countdown; when it reaches 0, exhaust
       n_fprs so all subsequent fp-typed args use the GPR/memory path. */
    if (n_fixed_remaining > 0) {
      n_fixed_remaining--;
      if (n_fixed_remaining == 0)
        n_fprs = 0;
    }
  }

  closure->fun(cif, ret, args, closure->user_data);

  /* Write ret to retbuf unless the callee wrote the result directly via a
     hidden pointer (flags == FFI390_RET_STRUCT with size > 24).  For that
     case ret_size was still set correctly above but the actual data is
     already in the caller's buffer, so no copy is needed.  */
  int needs_copy = (cif->rtype != NULL)
                   && (cif->rtype->type != FFI_TYPE_VOID)
                   && !(cif->flags == FFI390_RET_STRUCT
                        && cif->rtype->size > 24);
  if (needs_copy)
    {
      switch (cif->rtype->type)
        {
          case FFI_TYPE_UINT8:
            *(unsigned long*)(retbuf) = *(unsigned long*)ret;
            break;
          case FFI_TYPE_SINT8:
            *(signed long*)(retbuf) = *(unsigned long*)ret;
            break;
          case FFI_TYPE_UINT16:
            *(unsigned long*)(retbuf) = *(unsigned long*)ret;
            break;
          case FFI_TYPE_SINT16:
            *(signed long*)(retbuf) = *(signed long*)ret;
            break;
          case FFI_TYPE_UINT32:
            *(unsigned long*)(retbuf) = *(unsigned long*)ret;
            break;
          case FFI_TYPE_INT:
          case FFI_TYPE_SINT32:
            *(signed long*)(retbuf) = *(signed long*)ret;
            break;
          case FFI_TYPE_UINT64:
          case FFI_TYPE_SINT64:
          case FFI_TYPE_POINTER:
            *(void**)(retbuf) = *(void**)ret;
            break;
          case FFI_TYPE_FLOAT:
            *(float*)(retbuf) = *(float*)ret;
            break;
          case FFI_TYPE_DOUBLE:
            *(double*)(retbuf) = *(double*)ret;
            break;
          default:
            /* Covers STRUCT, COMPLEX, LONGDOUBLE, STRUCT_FF/DD/LDLD subtypes */
            for (int i = 0; i < ret_size; i++)
              ((char*)retbuf)[i] = ((char*)ret)[i];
            break;
        }
    }

  fprintf(stderr, "holy shit we made it\n"); fflush(stderr);
  return;
}
/*======================== End of Routine ============================*/

/*====================================================================*/
/*                                                                    */
/* Name     - ffi_prep_closure_loc.                                   */
/*                                                                    */
/* Function - Prepare a FFI closure.                                  */
/*                                                                    */
/*====================================================================*/
 

void debug_stub(ffi_closure *closure, char *sp, double *fpr) {
  return;
  ffi_cif *cif = closure->cif;

  fprintf(stderr, "=== debug_stub ===\n");

  fprintf(stderr, "sp = %p\n", sp);
  fprintf(stderr, "fpr = %p\n", fpr);

  sp += 2176;

  fprintf(stderr, "sp [2048..2200]:");
  for (int i = 0; i <= 16*8; i++) {
      if (i % 8 == 0) {
        fprintf(stderr, "\n\t");
      }
      fprintf(stderr, " %02x", sp[i]);
  }
  fprintf(stderr, "\n");


  unsigned char *p = (unsigned char*)closure->cif;
  fprintf(stderr, "cif raw [20..31]:");
  for (int i = 20; i <= 31; i++)
    fprintf(stderr, " %02x", p[i]);
  fprintf(stderr, "\n");

  /* ---- ffi_closure fields ----------------------------------------- */
  fprintf(stderr, "ffi_closure @ %p\n", (void *)closure);
  fprintf(stderr, "  .cif       = %p\n", (void *)closure->cif);
  fprintf(stderr, "  .fun       = %p\n", (void *)(uintptr_t)closure->fun);
  fprintf(stderr, "  .user_data = %p\n", closure->user_data);

  /* First 32 bytes of the trampoline in hex */
  fprintf(stderr, "  .tramp[0..31] =");
  for (int i = 0; i < 32; i++)
    fprintf(stderr, " %02x", (unsigned char)closure->tramp[i]);
  fprintf(stderr, "\n");

  /* ---- ffi_cif fields --------------------------------------------- */
  fprintf(stderr, "ffi_cif @ %p\n", (void *)cif);
  if (cif == NULL) {
    fprintf(stderr, "  (null)\n");
    fflush(stderr);
    return;
  }
  fprintf(stderr, "  .abi        = %u\n",  (unsigned)cif->abi);
  fprintf(stderr, "  .nargs      = %u\n",  cif->nargs);
  fprintf(stderr, "  .nfixedargs = %u\n",  cif->nfixedargs);
  fprintf(stderr, "  .bytes      = %u\n",  cif->bytes);
  fprintf(stderr, "  .flags      = %u\n",  cif->flags);
  fprintf(stderr, "  .rtype      = %p",    (void *)cif->rtype);
  if (cif->rtype)
    fprintf(stderr, "  (type=%u size=%zu align=%u)",
            (unsigned)cif->rtype->type,
            cif->rtype->size,
            (unsigned)cif->rtype->alignment);
  fprintf(stderr, "\n");
  fprintf(stderr, "  .arg_types  = %p\n",  (void *)cif->arg_types);
  for (unsigned i = 0; i < cif->nargs && cif->arg_types; i++) {
    ffi_type *at = cif->arg_types[i];
    if (at)
      fprintf(stderr, "  .arg_types[%u] = %p  type=%u size=%zu align=%u\n",
              i, (void *)at,
              (unsigned)at->type, at->size, (unsigned)at->alignment);
    else
      fprintf(stderr, "  .arg_types[%u] = (null)\n", i);
  }

  fprintf(stderr, "=== debug_stub done ===\n");
  fflush(stderr);
  exit(0);
}


ffi_status
ffi_prep_closure_loc (ffi_closure *closure,
                      ffi_cif *cif,
                      void (*fun) (ffi_cif *, void *, void **, void *),
                      void *user_data,
                      void *codeloc)
{

  if (cif->abi != FFI_XPLINK)
    return FFI_BAD_ABI;

  /*
  * 0.) remember z/OS is weird, the first 16 bytes of the tramp
  *     are the "function descriptor", so the tramp code starts
  *     8 bytes ahead of where it seems like it shoulds
  * 1.) "branch" to self - basr with r0 sets the first register (r5)
  *     to the value of the PC (in this case the trampoline)
  * 2.) using this addr, save the GPRs and FPRs to the closure
  * 3.) branch to our helper function to figure out how to deal
  *     with the calling parameters
  */

  /* Trampoline V2
  *  0.) figure out where we are by doing the fake branch
         to load the PC into %r5
     1.) extract "cif->bytes", so we can allocate the DSA+save information
     2.) call FFISYS2 (closure_xplink.S),
         function signature FFISYS2(closure, register save info)
     3.) on return, restore registers (including stack pointer)
     4.) return to caller, return value itself is done in FFISYS2
  */

  /* Trampoline V2 assembly-ish
  *  0x000000000000000       (8 bytes "env pointer")
  *  &(closure->tramp[8])    (function entry point)
  *  basr %r5,0              (r5 = pc = &closure->tramp [16])
  *  stmg %r6,%r15,1848(%r4) (save r6-r7 to save area 2048-dsasize+16)
  *  aghi %r4,-216           (update stack pointer r4 = r4 - dsasize
                              dsasize=save area (12 registers * 8 bytes = 96 bytes)
                              + 16 bytes (reserved) + 8 (bytes debug area)
                              + 8 bytes (reserved) + argument area (stored in cif->bytes)
                              )
  *
  */

  #if 0
  *(long  *)&closure->tramp [0]  = 0x000000000000000;
  *(long  *)&closure->tramp [8]  = (long)(&(closure->tramp[16]));

  // put the pc in r5 so we can find the closure
  *(short *)&closure->tramp [16] = 0x0d50;   /* basr %r5,0 */

  *(short *)&closure->tramp [18] = 0xe300;  /* lg %r0,112(,%r5) */
  *(short *)&closure->tramp [20] = 0x5070;
  *(short *)&closure->tramp [22] = 0x0004;

  *(short *)&closure->tramp [24] = 0xe300;  /* llgf %r0,24(,%r0) */
  *(short *)&closure->tramp [26] = 0x0018;
  *(short *)&closure->tramp [28] = 0x0016;

  *(short *)&closure->tramp [30] = 0xa70b;  /* aghi  %r0,216 */
  *(short *)&closure->tramp [32] = 0x00d8;

  // *(short *)&closure->tramp [24] = 0xa74b;  /* aghi  %r0,216 */
  // *(short *)&closure->tramp [26] = 0xffd8;

  *(short *)&closure->tramp [34] = 0xb909;  /* sgr   %r4,%r0 */
  *(short *)&closure->tramp [36] = 0x0040;

  // save preserved registers
  *(short  *)&closure->tramp [38]  = 0xeb6f;   /* stmg %r6,%r15,2064(%r4) */
  *(short  *)&closure->tramp [40]  = 0x4810;
  *(short  *)&closure->tramp [42]  = 0x0024;

  // save arguments
  *(short  *)&closure->tramp [44]  = 0xeb13;   /* stmg %r1,%r3,2176(%r4) */
  *(short  *)&closure->tramp [46]  = 0x4880;
  *(short  *)&closure->tramp [48]  = 0x0024;

  *(short *)&closure->tramp [50] = 0xe360;   /* lg  %r6,104(,%r5) */
  *(short *)&closure->tramp [52] = 0x5068;
  *(short *)&closure->tramp [54] = 0x0004;

  *(short *)&closure->tramp [56] = 0xe360;  /* lg %r5,0(,%r6) */
  *(short *)&closure->tramp [58] = 0x6000;
  *(short *)&closure->tramp [60] = 0x0004;

  *(short *)&closure->tramp [62] = 0xeb56;  /* lmg %r5,%r6,0(,%r6) */
  *(short *)&closure->tramp [64] = 0x6000;
  *(short *)&closure->tramp [66] = 0x0004;

  *(short *)&closure->tramp[68]  = 0x0d76;               /* basr  %fr7,%r6 */

  // branch to debug stub for testing
  *(long  *)&closure->tramp [120] = ((long)&debug_stub);
#endif

*(long  *)&closure->tramp[0]   = 0x0000000000000000;
*(long  *)&closure->tramp[8]   = (long)(&closure->tramp[16]);

/* Trampoline frame layout (TRAMP_ALLOC = 512)
 * ============================================
 * Stack adjust and register save happen first (per XPLINK prolog rules).
 * r4_old is recomputed after the fact as r4_new + TRAMP_ALLOC.
 *
 * XPLINK save area of the new frame (r4_new+2048):
 *   r4_new+2048  backchain = r4_old
 *   r4_new+2056  r5
 *   r4_new+2064  r6
 *   r4_new+2072  r7  ← caller's return address; lmg restores this
 *   r4_new+2080  r8 .. r4_new+2144  r15
 *
 * We must NOT save into r4_old's local storage (r4_old+0..r4_old+2047):
 * the caller uses it, and FFISYS2's CELQPRLG writes there too, so any
 * value we place there will be overwritten before we can read it back.
 *
 * Instruction sequence:
 *   [16] aghi r4, -512            r4 = r4_new   (stack adjust first)
 *   [20] stmg r5,r15, 2056(r4)    save r5..r15 into r4_new save area
 *   [26] stmg r1,r3, 2688(r4)     save r1/r2/r3 BEFORE r2 is clobbered
 *   [32] basr r5, 0               r5 = tramp+34  (PC anchor)
 *   [34] lay  r2, 512(r4)         r2 = r4_old  (r2 already saved above)
 *   [40] stg  r2, 2048(r4)        backchain at r4_new+2048
 *   [46] lay  r1, -34(r5)         r1 = closure  (tramp+34-34 = tramp+0)
 *   [52] lay  r3, 2688(r4)        r3 = arg area (FFISYS2 ignores it)
 *   [58] lg   r6, 86(r5)          r6 = tramp[120]  (tramp+34+86=tramp+120)
 *   [64] lmg  r5,r6, 0(r6)        load env+entry from descriptor
 *   [70] basr r7, r6              call FFISYS2; r7 = tramp+72
 *   [72] nopr                     CELQEPLG "BCR 15,r7" lands here
 *   [74] lmg  r4,r15, 2048(r4)    restore r4..r15 from r4_new+2048
 *   [80] nopr nopr                padding
 *   [84] br   r7                  return; r7 = caller's return address
 *   [86] nopr                     padding
 */

/* [16] aghi %r4,-512               A74B FE00   r4 = r4_new  (stack first) */
*(short *)&closure->tramp[16]  = 0xa74b;
*(short *)&closure->tramp[18]  = 0xfe00;

/* [20] stmg %r5,%r15,2056(%r4)     EB5F 4808 0024
 * Save r5..r15 immediately after stack adjust.
 * r7 (caller return address) lands at r4_new+2072; restored by lmg. */
*(short *)&closure->tramp[20]  = 0xeb5f;
*(short *)&closure->tramp[22]  = 0x4808;
*(short *)&closure->tramp[24]  = 0x0024;

/* [26] stmg %r1,%r3,2688(%r4)      EB13 4B80 0024
 * Save incoming arg registers BEFORE r2 is clobbered by lay below.
 * 2176+512=2688=0xB80.                                             */
*(short *)&closure->tramp[26]  = 0xeb13;
*(short *)&closure->tramp[28]  = 0x4a80;
*(short *)&closure->tramp[30]  = 0x0024;

/* [32] basr %r5,0                  0D50                r5 = tramp+34 */
*(short *)&closure->tramp[32]  = 0x0d50;

/* [34] lay %r2,512(,%r4)           E3 20 42 00 00 71   r2 = r4_old
 * r2 is now free to clobber (already saved by stmg above).        */
*(short *)&closure->tramp[34]  = 0xe320;
*(short *)&closure->tramp[36]  = 0x4200;
*(short *)&closure->tramp[38]  = 0x0071;

/* [40] stg %r2,2048(%r4)           E3 20 48 00 00 24   backchain */
*(short *)&closure->tramp[40]  = 0xe320;
*(short *)&closure->tramp[42]  = 0x4800;
*(short *)&closure->tramp[44]  = 0x0024;

/* [46] lay %r1,-34(,%r5)           E3 10 5F DE FF 71
 * r5=tramp+34; tramp+34-34=tramp+0 = closure address.
 * -34 = 0xFFDE as 20-bit signed: DL=0xFDE, DH=0xFF.               */
*(short *)&closure->tramp[46]  = 0xe310;
*(short *)&closure->tramp[48]  = 0x5fde;
*(short *)&closure->tramp[50]  = 0xff71;

/* [52] lay %r3,2688(,%r4)          E3 30 4B 80 00 71
 * r3 = arg area base; FFISYS2 ignores it (does LR 3,13 immediately). */
*(short *)&closure->tramp[52]  = 0xe330;
*(short *)&closure->tramp[54]  = 0x4b80;
*(short *)&closure->tramp[56]  = 0x0071;

/* [58] lg %r6,86(,%r5)             E3 60 50 56 00 04
 * r5=tramp+34; 34+86=120 = tramp[120] = &ffi_closure_SYSV.
 * 86 = 0x056.                                                      */
*(short *)&closure->tramp[58]  = 0xe360;
*(short *)&closure->tramp[60]  = 0x5056;
*(short *)&closure->tramp[62]  = 0x0004;

/* [64] lmg %r5,%r6,0(,%r6)         EB56 6000 0004
 * Load env→r5, entry→r6 from ffi_closure_SYSV function descriptor. */
*(short *)&closure->tramp[64]  = 0xeb56;
*(short *)&closure->tramp[66]  = 0x6000;
*(short *)&closure->tramp[68]  = 0x0004;

/* [70] basr %r7,%r6                0D76
 * Call FFISYS2; r7 = tramp+72 (the return landing address).       */
*(short *)&closure->tramp[70]  = 0x0d76;

/* [72] nopr                        0707
 * CELQEPLG does "BCR 15,r7" → tramp+72; falls through to [74].   */
*(short *)&closure->tramp[72]  = 0x0707;

/* [74] lmg %r4,%r15,2048(%r4)      EB4F 4800 0004
 * r4=r4_new here. Restores r4=r4_old, r7=caller_r7 from r4_new+2048.
 * Opcode byte = 0x04 (LMG = load multiple 64-bit).
 * 0x24 would be STMG (store) — wrong direction, causes infinite loop. */
*(short *)&closure->tramp[74]  = 0xeb4f;
*(short *)&closure->tramp[76]  = 0x4800;
*(short *)&closure->tramp[78]  = 0x0004;

/* [80] nopr nopr                   padding */
*(short *)&closure->tramp[80]  = 0x0707;
*(short *)&closure->tramp[82]  = 0x0707;

/* [84] br %r7                      07F7
 * r7 restored by lmg = caller's return address (set by caller's BASR). */
*(short *)&closure->tramp[84]  = 0x07f7;

/* [86] nopr                        padding */
*(short *)&closure->tramp[86]  = 0x0707;

*(long  *)&closure->tramp[120] = (long)&ffi_closure_SYSV;
//*(long  *)&closure->tramp[120] = (long)&debug_stub;

/* descriptor probe */
{
  long *desc = (long *)&ffi_closure_SYSV;
  fprintf(stderr, "ffi_closure_SYSV    @ %p: env=%016lx entry=%016lx\n",
          (void*)desc, desc[0], desc[1]);
  long *dbg = (long *)&debug_stub;
  fprintf(stderr, "debug_stub          @ %p: env=%016lx entry=%016lx\n",
          (void*)dbg, dbg[0], dbg[1]);
  long *tramp120 = *(long **)&closure->tramp[120];
  fprintf(stderr, "tramp[120] -> %p: [0]=%016lx [1]=%016lx\n",
          (void*)tramp120, tramp120[0], tramp120[1]);
  fflush(stderr);
}

fprintf(stderr, "tramp base = %p\n", (void*)closure->tramp);
fprintf(stderr, "tramp[8]   = %p\n", &closure->tramp[8]);
fprintf(stderr, "tramp[70]  = %p  (basr r7,r6)\n", &closure->tramp[70]);
fprintf(stderr, "tramp[72]  = %p  (nopr; CELQEPLG returns here)\n", &closure->tramp[72]);
fprintf(stderr, "tramp[74]  = %p  (return landing after nopr)\n", &closure->tramp[74]);
fprintf(stderr, "tramp[120] = %p  (&ffi_closure_SYSV = %p)\n",
        *(void**)&closure->tramp[120], (void*)&ffi_closure_SYSV);
fprintf(stderr, "tramp bytes [16..88]:");
for (int i = 16; i <= 88; i++)
    fprintf(stderr, " %02x", (unsigned char)closure->tramp[i]);
fprintf(stderr, "\n");
fflush(stderr);

//   *(long  *)&closure->tramp [0]  = 0x000000000000000;
//   *(long  *)&closure->tramp [8]  = (long)(&(closure->tramp[16]));

//   // put the pc in r5 so we can find the closure
//   *(short *)&closure->tramp [16] = 0x0d50;   /* basr %r5,0 */

// #ifdef NEW_TAMP

//   // load closure->cif->bytes (we have &closure->tramp[16] in %r5)
//   // so just (%r5 - 16) = closure address
//   //         &closure->cif = closure address + 64
//   //         closure->cif + 24 = closure->cif->bytes

//   // step 1. r6 = closure->cif
//   *(short *)&closure->tramp [] = 0xe360;  /* lg %r6,48(,%r5) */
//   *(short *)&closure->tramp [] = 0x5030;
//   *(short *)&closure->tramp [] = 0x0004;

//   // step 2. r6 = cif->bytes
//   *(short *)&closure->tramp [] = 0xe360;  /* l %r6,24(,%r6) */
//   *(short *)&closure->tramp [] = 0x6018;

//   // adjust stack pointer to include arg area
//   *(short *)&closure->tramp [] = 0xb909;   /* sgr %r4,r6 */
//   *(short *)&closure->tramp [] = 0x0046;

//   // allocate trampoline's local storage space
//   *(short *)&closure->tramp [] = 0xa740;   /* aghi %r4,-216 */
//   *(short *)&closure->tramp [] = 0xff28;

//   // save preserved registers to callers savearea (plus backchain and fake "env")
//   // the offset here looks weird, I think technically we ought to save this first
//   // then adjust the stack pointer, but we have the chicken egg scenario of having
//   // to do the work to determine the stack size, but we're still using
//   // the same location ie. 2048-dsasize+16, we're just subtracting dsasize from r4 first
//   *(short  *)&closure->tramp []  = 0xeb6f;   /* stmg %r6,%r15,2064(%r4) */
//   *(short  *)&closure->tramp []  = 0x4810;
//   *(short  *)&closure->tramp []  = 0x0024;

//   // now that we've saved our registers, we can actually do real stuff
//   // save the dsa size in a register that will be preserved for easy adjustment of the stack
//   // on the return side, since we don't actually save r4 or know dsasize without it
//   *(short  *)&closure->tramp []  = 0xeb6f;   /* lgr %r9,%r6,2064(%r4) */
//   *(short  *)&closure->tramp []  = 0x4810;
//   *(short  *)&closure->tramp []  = 0x0024;

//   // reload the entry point into r6 (maybe?)

//   // save the argument registers in the trampolines arg area
//   *(short  *)&closure->tramp []  = 0xeb13;   /* stmg %r1,%r3,2176(%r4) */
//   *(short  *)&closure->tramp []  = 0x4880;
//   *(short  *)&closure->tramp []  = 0x0024;


//   /* la %r5,(,%r4) */

//   // store the FPRs
//   //
//   // branch to the helper

//   // restore the preserved registers
//   *(short *)&closure->tramp [] = 0xeb7f;  /* lmg %r7,%r15,2048+24(,%r4) */
//   *(short *)&closure->tramp [] = 0x4818;
//   *(short *)&closure->tramp [] = 0x0004;

//   // deallocate the stack
//   *(short *)&closure->tramp [] = 0xa740;   /* aghi %r4,216 */
//   *(short *)&closure->tramp [] = 0x00d8;

//   // return
//   *(short *)&closure->tramp [] = 0x47f0;   /* b 2(,7) */
//   *(short *)&closure->tramp [] = 0x7002;
// #endif

//   // offset into closure register save area
//   *(short *)&closure->tramp [18] = 0xa75b;   /* aghi %r5,70 */
//   *(short *)&closure->tramp [20] = 0x0046;

//   // save the registers into the closure
//   *(short *)&closure->tramp [22] = 0xeb1f;   /* stmg %r1,%r15,0(%r5) */
//   *(short *)&closure->tramp [24] = 0x5000;
//   *(short *)&closure->tramp [26] = 0x0024;

//   // put the addr of the closure into r1
//   *(short *)&closure->tramp [28] = 0xe310;   /* lay %r1,-86(,%r5) */
//   *(short *)&closure->tramp [30] = 0x5fa8;
//   *(short *)&closure->tramp [32] = 0xff71;

//   // adjust r5 to point to the help address
//   *(short *)&closure->tramp [34] = 0xa75b;   /* aghi %r5,-34*/
//   *(short *)&closure->tramp [36] = 0xffde;

//   // // load the pointer to the helper address
//   *(short *)&closure->tramp [38] = 0xe350;  /* lg %r5,0(,%r5) */
//   *(short *)&closure->tramp [40] = 0x5000;
//   *(short *)&closure->tramp [42] = 0x0004;

//   *(short *)&closure->tramp [44] = 0xeb56;  /* lmg %r5,%r6,0(,%r5) */
//   *(short *)&closure->tramp [46] = 0x5000;
//   *(short *)&closure->tramp [48] = 0x0004;

//   // load the actual address of the help
//   // *(short *)&closure->tramp [44] = 0xe350;   /* lg %r5,8(,%r5) */
//   // *(short *)&closure->tramp [46] = 0x5008;
//   // *(short *)&closure->tramp [48] = 0x0004;

//   // branch to the helper
//   *(short *)&closure->tramp [50] = 0x0d66;   /* basr %r6,%r6 */

//   // pointer to the helper address
//   *(long  *)&closure->tramp [54] = ((long)&ffi_closure_SYSV);

  closure->cif = cif;
  closure->user_data = user_data;
  closure->fun = fun;

  return FFI_OK;
}

/*======================== End of Routine ============================*/

