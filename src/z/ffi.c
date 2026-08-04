/* -----------------------------------------------------------------------
   ffi.c   - Copyright (c) 2000, 2007 Software AG
             Copyright (c) 2008 Red Hat, Inc
             Copyright (c) IBM Corp. 2026
 
   z/OS Foreign Function Interface
 
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

#include <ffi.h>
#include <ffi_common.h>

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

/*====================== End of Includes =============================*/

/*====================================================================*/
/*                           Defines                                  */
/*                           -------                                  */
/*====================================================================*/

/* Maximum number of GPRs available for argument passing.  */
#define MAX_GPRARGS 3

/* Maximum number of FPRs available for argument passing.  */
#define MAX_FPRARGS 4

/* Maximum bytes passable in GPR slots (3 regs x 8 bytes).
   Also the struct size threshold above which a hidden return pointer is used,
   and the base size of the lstor buffer in ffi_call.  */
#define MAX_GPRARG_BYTES (MAX_GPRARGS * 8)

/* Offset from the XPLINK stack pointer (r4) to the argument area.  */
#define XPLINK_ARGAREA_OFFSET 2176

/* Size of one GPR slot in bytes.  */
#define GPR_SIZE 8

/* If these values change, xplink.S RTABLE must be updated to match!  */
#define FFI390_RET_VOID         0
#define FFI390_RET_STRUCT       1
#define FFI390_RET_FLOAT        2
#define FFI390_RET_DOUBLE       3
#define FFI390_RET_LDBLE        4
#define FFI390_RET_INT32        5
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

#pragma map(ffi_prep_args, "PREPARGS")
void ffi_prep_args (unsigned char *, extended_cif *);
#pragma map(ffi_closure_helper_XPLINK, "CLSRHLP")
void ffi_closure_helper_XPLINK (ffi_closure *, void *, struct ffi_reg_data *);
#pragma map(ffi_determine_return_type, "DETRET")
int ffi_determine_return_type(ffi_closure *);
#pragma map(ffi_struct_float_pair_type, "STFPTYP")
unsigned int ffi_struct_float_pair_type (ffi_type **);

/*====================== End of Prototypes ===========================*/

/*====================================================================*/
/*                          Externals                                 */
/*                          ---------                                 */
/*====================================================================*/

/* located in xplink.S */
#pragma map(ffi_call_XPLINK, "FFISYS")
extern void ffi_call_XPLINK(void (*fn)(void), extended_cif *,
                          unsigned, unsigned *, unsigned, unsigned,
                          unsigned, void *);

/* located in closure_xplink.S */
#pragma map(ffi_closure_XPLINK, "FFISYS2")
extern void ffi_closure_XPLINK(void);

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
  unsigned char *arg_ptr = stack;

  /* If we returning a structure larger than MAX_GPRARG_BYTES,
     we set the first parameter register
     to the address of where we are returning this structure.  */
  if (ecif->cif->flags == FFI_TYPE_STRUCT &&
      (ecif->cif->rtype->size > MAX_GPRARG_BYTES))
    arg_ptr += (unsigned long) ecif->rvalue;

  /* Now for the arguments.  */
  for (type_ptr = ecif->cif->arg_types, i = ecif->cif->nargs;
       i > 0;
       i--, type_ptr++, p_argv++)
    {
      void *arg = *p_argv;
      int type = (*type_ptr)->type;
      int size = (*type_ptr)->size;

      /* Now handle all primitive int/pointer/float data types.  */
      switch (type)
        {
          case FFI_TYPE_STRUCT:
          case FFI_TYPE_COMPLEX:
            memcpy(arg_ptr, *p_argv, size);
            break;

#if FFI_TYPE_LONGDOUBLE != FFI_TYPE_DOUBLE
          case FFI_TYPE_LONGDOUBLE:
            *(long double *) arg_ptr = *(long double *) (*p_argv);
            break;
#endif

          case FFI_TYPE_DOUBLE:
            *(double *) arg_ptr = *(double *) (*p_argv);
            break;

          case FFI_TYPE_FLOAT:
            *(float *) arg_ptr = *(float *) (*p_argv);
            break;

          case FFI_TYPE_POINTER:
            *(void **) arg_ptr = *(void **) (*p_argv);
            break;

          case FFI_TYPE_SINT64:
            *(signed long long *) arg_ptr = *(signed long long *) (*p_argv);
            break;

          case FFI_TYPE_UINT64:
            *(unsigned long long *) arg_ptr = *(unsigned long long *) (*p_argv);
            break;

          case FFI_TYPE_UINT32:
            *(unsigned int *) arg_ptr = *(unsigned int *) (*p_argv);
            break;

          case FFI_TYPE_SINT32:
          case FFI_TYPE_INT:
            *(signed int *) arg_ptr = *(signed int *) (*p_argv);
            break;

          case FFI_TYPE_UINT16:
            *(unsigned short *) arg_ptr = *(unsigned short *) (*p_argv);
            break;

          case FFI_TYPE_SINT16:
            *(signed short *) arg_ptr = *(signed short *) (*p_argv);
            break;

          case FFI_TYPE_UINT8:
            *(unsigned char *) arg_ptr = *(unsigned char *) (*p_argv);
            break;

          case FFI_TYPE_SINT8:
            *(signed char *) arg_ptr = *(signed char *) (*p_argv);
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
/* Name     - ffi_prep_cif_machdep.                                   */
/*                                                                    */
/* Function - Perform machine dependent CIF processing.               */
/*                                                                    */
/*====================================================================*/

ffi_status
ffi_prep_cif_machdep(ffi_cif *cif)
{
  /* Set nfixedargs to nargs; ffi_prep_cif_machdep_var corrects it for
     variadic functions.  */
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
  if (cif->rtype->type == FFI_TYPE_STRUCT && cif->rtype->size > MAX_GPRARG_BYTES)
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

  /* If we don't have a return value, we need to fake one.  */
  if (rvalue == NULL)
    {
      /* For struct returns that use in-register/in-memory return (not a
         hidden-pointer return), the assembly still writes the result through
         ecif.rvalue.  Provide scratch space so we don't store to NULL.
         For hidden-pointer returns (flags == FFI_TYPE_STRUCT with size > 24)
         the hidden pointer was already passed as arg, so just discard.  */
      if (cif->flags == FFI390_RET_STRUCT && cif->rtype->size <= MAX_GPRARG_BYTES)
        ecif.rvalue = alloca (cif->rtype->size);
      else
        ret_type = FFI_TYPE_VOID;
    }

  switch (cif->abi)
    {
      case FFI_XPLINK:
        {
          /* initial space for the GPR argument slots.  */
          unsigned int lstor_size = MAX_GPRARG_BYTES;
          for (unsigned int i = 0; i < cif->nargs; i++)
            lstor_size += cif->arg_types[i]->size;
          
          ffi_call_XPLINK(fn, &ecif, cif->flags, ecif.rvalue, cif->bytes,
                        cif->nargs, cif->nfixedargs,
                        alloca(lstor_size));
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

int
ffi_determine_return_type(ffi_closure *closure)
{
  /* Use the flags already computed by ffi_prep_cif_machdep, they are the
     canonical return classification and keep call-side and closure-side
     dispatch tables in sync without re-classifying.  */
  switch (closure->cif->flags)
    {
      case FFI390_RET_VOID:
      case FFI390_RET_INT32:
      case FFI390_RET_INT64:
        return 1;   /* GPR3 */

      case FFI390_RET_FLOAT:
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
          if (sz <= 1 * GPR_SIZE) return 2;
          if (sz <= 2 * GPR_SIZE) return 3;
          if (sz <= 3 * GPR_SIZE) return 4;
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
/* Name     - ffi_closure_helper_XPLINK.                                */
/*                                                                    */
/* Function - Call a FFI closure target function.                     */
/*                                                                    */
/*====================================================================*/

void
ffi_closure_helper_XPLINK (ffi_closure *closure, void *retbuf,
                          struct ffi_reg_data *reg)
{
  int ret_size;
  ffi_cif *cif = closure->cif;

  if (cif->rtype == NULL || cif->rtype->type == FFI_TYPE_VOID)
    ret_size = 8;
  else if (cif->flags == FFI390_RET_STRUCT_LDLD)
    ret_size = 32;
  else
    ret_size = cif->rtype->size;

  void **args = (void **) alloca(sizeof(void *) * cif->nargs);
  void *ret = (void *) alloca(ret_size);
  char *savearea = ((char *) reg->gpr[3]) + XPLINK_ARGAREA_OFFSET;

  /* Scratch buffers for _Complex float/double/longdouble assembly.  */
  float *cmplx_arg;
  double *cmplx_arg_double;
  long double *arg_longdouble;

  ffi_type **atype = cif->arg_types;

  /* Remaining incoming register slots.  */
  int n_gprs = MAX_GPRARGS;
  int n_fprs = MAX_FPRARGS;
  /* Byte offset into the XPLINK save area.  */
  int n_arg = 0;
  /* Fixed-arg countdown: when it hits 0, exhaust n_fprs so subsequent
     fp-typed args use the GPR/memory path (mirrors xplink.S VARFNC).  */
  int n_fixed_remaining = (int)cif->nfixedargs;

  if (ret_size > MAX_GPRARG_BYTES && cif->flags != FFI390_RET_STRUCT_LDLD)
    {
      n_gprs--;
      n_arg += 8;
      ret = reg->gpr[0];
    }

  for (int i = 0; i < cif->nargs; i++)
    {
      switch (atype[i]->type)
        {
          case FFI_TYPE_UINT8:
          case FFI_TYPE_SINT8:
            if (n_gprs == 3)
              {
                args[i] = (void *)(((long)(&reg->gpr[0])) + 7);
                n_gprs--;
                n_arg += 8;
              }
            else if (n_gprs == 2)
              {
                args[i] = (void *)(((long)(&reg->gpr[1])) + 7);
                n_gprs--;
                n_arg += 8;
              }
            else if (n_gprs == 1)
              {
                args[i] = (void *)(((long)(&reg->gpr[2])) + 7);
                n_gprs--;
                n_arg += 8;
              }
            else
              {
                args[i] = (savearea + n_arg + 7);
                n_arg += 8;
              }
            break;

          case FFI_TYPE_UINT16:
          case FFI_TYPE_SINT16:
            if (n_gprs == 3)
              {
                args[i] = (void *)(((long)(&reg->gpr[0])) + 6);
                n_gprs--;
                n_arg += 8;
              }
            else if (n_gprs == 2)
              {
                args[i] = (void *)(((long)(&reg->gpr[1])) + 6);
                n_gprs--;
                n_arg += 8;
              }
            else if (n_gprs == 1)
              {
                args[i] = (void *)(((long)(&reg->gpr[2])) + 6);
                n_gprs--;
                n_arg += 8;
              }
            else
              {
                args[i] = (savearea + n_arg + 6);
                n_arg += 8;
              }
            break;

          case FFI_TYPE_UINT32:
          case FFI_TYPE_SINT32:
          case FFI_TYPE_INT:
            if (n_gprs == 3)
              {
                args[i] = (void *)(((long)(&reg->gpr[0])) + 4);
                n_gprs--;
                n_arg += 8;
              }
            else if (n_gprs == 2)
              {
                args[i] = (void *)(((long)(&reg->gpr[1])) + 4);
                n_gprs--;
                n_arg += 8;
              }
            else if (n_gprs == 1)
              {
                args[i] = (void *)(((long)(&reg->gpr[2])) + 4);
                n_gprs--;
                n_arg += 8;
              }
            else
              {
                args[i] = (savearea + n_arg + 4);
                n_arg += 8;
              }
            break;

          case FFI_TYPE_UINT64:
          case FFI_TYPE_SINT64:
          case FFI_TYPE_POINTER:
            if (n_gprs == 3)
              {
                args[i] = &reg->gpr[0];
                n_gprs--;
                n_arg += 8;
              }
            else if (n_gprs == 2)
              {
                args[i] = &reg->gpr[1];
                n_gprs--;
                n_arg += 8;
              }
            else if (n_gprs == 1)
              {
                args[i] = &reg->gpr[2];
                n_gprs--;
                n_arg += 8;
              }
            else
              {
                args[i] = (savearea + n_arg);
                n_arg += 8;
              }
            break;

          case FFI_TYPE_FLOAT:
            if (n_fixed_remaining == 0)
              {
                /* variadic: float passed in GPR/memory, not FPR */
                if (n_gprs > 0)
                  {
                    args[i] = (char *)&reg->gpr[3 - n_gprs] + 4;
                    n_gprs--;
                  }
                else
                  args[i] = (savearea + n_arg + 4);
                n_arg += 8;
              }
            else if (n_fprs == 4)
              {
                args[i] = &reg->fpr[0];
                n_gprs--;
                n_fprs--;
                n_arg += 8;
              }
            else if (n_fprs == 3)
              {
                args[i] = &reg->fpr[2];
                n_gprs--;
                n_fprs--;
                n_arg += 8;
              }
            else if (n_fprs == 2)
              {
                args[i] = &reg->fpr[4];
                n_gprs--;
                n_fprs--;
                n_arg += 8;
              }
            else if (n_fprs == 1)
              {
                args[i] = &reg->fpr[6];
                n_gprs--;
                n_fprs--;
                n_arg += 8;
              }
            else
              {
                args[i] = (savearea + n_arg);
                n_arg += 8;
              }
            break;

          case FFI_TYPE_DOUBLE:
            if (n_fixed_remaining == 0)
              {
                /* variadic: double passed in GPR/memory, not FPR */
                if (n_gprs > 0)
                  {
                    args[i] = &reg->gpr[3 - n_gprs];
                    n_gprs--;
                  }
                else
                  args[i] = (savearea + n_arg);
                n_arg += 8;
              }
            else if (n_fprs == 4)
              {
                args[i] = &reg->fpr[0];
                n_gprs--;
                n_fprs--;
                n_arg += 8;
              }
            else if (n_fprs == 3)
              {
                args[i] = &reg->fpr[2];
                n_gprs--;
                n_fprs--;
                n_arg += 8;
              }
            else if (n_fprs == 2)
              {
                args[i] = &reg->fpr[4];
                n_gprs--;
                n_fprs--;
                n_arg += 8;
              }
            else if (n_fprs == 1)
              {
                args[i] = &reg->fpr[6];
                n_gprs--;
                n_fprs--;
                n_arg += 8;
              }
            else
              {
                args[i] = (savearea + n_arg);
                n_arg += 8;
              }
            break;

          case FFI_TYPE_LONGDOUBLE:
            if (n_fixed_remaining == 0)
              {
                /* variadic: long double as 16-byte opaque in GPR/memory */
                args[i] = (savearea + n_arg);
                n_arg += 16;
              }
            else if (n_fprs == 4)
              {
                arg_longdouble = alloca(sizeof(long double));
                ((double *) arg_longdouble)[0] = reg->fpr[0];
                ((double *) arg_longdouble)[1] = reg->fpr[2];
                args[i] = arg_longdouble;
                n_gprs -= 2;
                n_fprs -= 2;
                n_arg += 16;
              }
            else if (n_fprs == 3 || n_fprs == 2)
              {
                arg_longdouble = alloca(sizeof(long double));
                ((double *) arg_longdouble)[0] = reg->fpr[4];
                ((double *) arg_longdouble)[1] = reg->fpr[6];
                args[i] = arg_longdouble;
                n_gprs -= 2;
                n_fprs -= 2;
                n_arg += 16;
              }
            else
              {
                args[i] = (savearea + n_arg);
                n_arg += 16;
              }
            break;

          case FFI_TYPE_STRUCT:
          case FFI_TYPE_COMPLEX:
            {
              /* For _Complex T the element type is in elements[0]; for a
                 struct use the recursive leaf checker.  fp == FFI_TYPE_FLOAT/
                 DOUBLE/LONGDOUBLE means FPR-pair passing, 0 means GPR/memory. */
              unsigned int fp;
              if (atype[i]->type == FFI_TYPE_COMPLEX)
                fp = (atype[i]->elements && atype[i]->elements[0])
                     ? atype[i]->elements[0]->type : 0;
              else
                fp = ffi_struct_float_pair_type(&atype[i]);

              if (fp == 0)
                {
                  if (atype[i]->size <= 8)
                    {
                      if (n_gprs == 3)
                        {
                          args[i] = &reg->gpr[0];
                          n_gprs--;
                          n_arg += 8;
                        }
                      else if (n_gprs == 2)
                        {
                          args[i] = &reg->gpr[1];
                          n_gprs--;
                          n_arg += 8;
                        }
                      else if (n_gprs == 1)
                        {
                          args[i] = &reg->gpr[2];
                          n_gprs--;
                          n_arg += 8;
                        }
                      else
                        {
                          args[i] = (savearea + n_arg);
                          n_arg += atype[i]->size;
                        }
                    }
                  else if (atype[i]->size <= 16)
                    {
                      if (n_gprs == 3)
                        {
                          args[i] = &reg->gpr[0];
                          n_gprs -= 2;
                          n_arg += 16;
                        }
                      else if (n_gprs == 2)
                        {
                          args[i] = &reg->gpr[1];
                          n_gprs -= 2;
                          n_arg += 16;
                        }
                      else if (n_gprs == 1)
                        {
                          *(long *)(savearea + n_arg) = (long)reg->gpr[2];
                          args[i] = (savearea + n_arg);
                          n_gprs--;
                          n_arg += 16;
                        }
                      else
                        {
                          args[i] = (savearea + n_arg);
                          n_arg += atype[i]->size;
                        }
                    }
                  else if (atype[i]->size <= 24)
                    {
                      if (n_gprs == 3)
                        {
                          args[i] = &reg->gpr[0];
                          n_gprs -= 3;
                          n_arg += 24;
                        }
                      else if (n_gprs == 2)
                        {
                          *(long *)(savearea + n_arg) = (long)reg->gpr[1];
                          *(long *)(savearea + n_arg + 8) = (long)reg->gpr[2];
                          args[i] = (savearea + n_arg);
                          n_gprs -= 2;
                          n_arg += 24;
                        }
                      else if (n_gprs == 1)
                        {
                          *(long *)(savearea + n_arg) = (long)reg->gpr[2];
                          args[i] = (savearea + n_arg);
                          n_gprs--;
                          n_arg += 24;
                        }
                      else
                        {
                          args[i] = (savearea + n_arg);
                          n_arg += atype[i]->size;
                        }
                    }
                  else
                    {
                      if (n_gprs == 3)
                        {
                          *(long *)(savearea + n_arg) = (long)reg->gpr[0];
                          *(long *)(savearea + n_arg + 8) = (long)reg->gpr[1];
                          *(long *)(savearea + n_arg + 16) = (long)reg->gpr[2];
                          args[i] = (savearea + n_arg);
                          n_gprs -= 3;
                          n_arg += atype[i]->size;
                        }
                      else if (n_gprs == 2)
                        {
                          *(long *)(savearea + n_arg) = (long)reg->gpr[1];
                          *(long *)(savearea + n_arg + 8) = (long)reg->gpr[2];
                          args[i] = (savearea + n_arg);
                          n_gprs -= 2;
                          n_arg += atype[i]->size;
                        }
                      else if (n_gprs == 1)
                        {
                          *(long *)(savearea + n_arg) = (long)reg->gpr[2];
                          args[i] = (savearea + n_arg);
                          n_gprs--;
                          n_arg += atype[i]->size;
                        }
                      else
                        {
                          args[i] = (savearea + n_arg);
                          n_arg += atype[i]->size;
                        }
                    }
                }
              else if (fp == FFI_TYPE_DOUBLE)
                {
                  if (n_fixed_remaining == 0)
                    {
                      /* variadic: _Complex double as 16-byte opaque in GPR/memory */
                      args[i] = (savearea + n_arg);
                      n_arg += 16;
                    }
                  else if (n_fprs == 4)
                    {
                      args[i] = &reg->fpr[0];
                      reg->fpr[1] = reg->fpr[2];
                      n_gprs--;
                      n_fprs -= 2;
                      n_arg += 16;
                    }
                  else if (n_fprs == 3)
                    {
                      args[i] = &reg->fpr[2];
                      reg->fpr[3] = reg->fpr[4];
                      n_gprs--;
                      n_fprs -= 2;
                      n_arg += 16;
                    }
                  else if (n_fprs == 2)
                    {
                      args[i] = &reg->fpr[4];
                      reg->fpr[5] = reg->fpr[6];
                      n_gprs--;
                      n_fprs -= 2;
                      n_arg += 16;
                    }
                  else if (n_fprs == 1)
                    {
                      cmplx_arg_double = alloca(2 * sizeof(double));
                      args[i] = cmplx_arg_double;
                      cmplx_arg_double[0] = reg->fpr[6];
                      cmplx_arg_double[1] = *(double *)(savearea + n_arg + 8);
                      n_gprs--;
                      n_fprs -= 2;
                      n_arg += 16;
                    }
                  else
                    {
                      args[i] = (savearea + n_arg);
                      n_arg += 16;
                    }
                }
              else if (fp == FFI_TYPE_FLOAT)
                {
                  cmplx_arg = alloca(2 * sizeof(float));
                  if (n_fixed_remaining == 0)
                    {
                      /* variadic: _Complex float as 8-byte opaque in GPR/memory */
                      args[i] = (savearea + n_arg);
                      n_arg += 8;
                    }
                  else if (n_fprs == 4)
                    {
                      args[i] = cmplx_arg;
                      cmplx_arg[0] = *(float *)&reg->fpr[0];
                      cmplx_arg[1] = *(float *)&reg->fpr[2];
                      n_gprs--;
                      n_fprs -= 2;
                      n_arg += 8;
                    }
                  else if (n_fprs == 3)
                    {
                      args[i] = cmplx_arg;
                      cmplx_arg[0] = *(float *)&reg->fpr[2];
                      cmplx_arg[1] = *(float *)&reg->fpr[4];
                      n_gprs--;
                      n_fprs -= 2;
                      n_arg += 8;
                    }
                  else if (n_fprs == 2)
                    {
                      args[i] = cmplx_arg;
                      cmplx_arg[0] = *(float *)&reg->fpr[4];
                      cmplx_arg[1] = *(float *)&reg->fpr[6];
                      n_gprs--;
                      n_fprs -= 2;
                      n_arg += 8;
                    }
                  else if (n_fprs == 1)
                    {
                      args[i] = cmplx_arg;
                      cmplx_arg[0] = *(float *)&reg->fpr[6];
                      cmplx_arg[1] = *(float *)(savearea + n_arg + 4);
                      n_gprs--;
                      n_fprs -= 2;
                      n_arg += 8;
                    }
                  else
                    {
                      args[i] = (savearea + n_arg);
                      cmplx_arg = (savearea + n_arg);
                      n_arg += 8;
                    }
                }
              else if (fp == FFI_TYPE_LONGDOUBLE)
                {
                  if (n_fixed_remaining == 0)
                    {
                      /* variadic: _Complex long double as 32-byte opaque in memory */
                      args[i] = (savearea + n_arg);
                      n_arg += 32;
                    }
                  else if (n_fprs == 4)
                    {
                      arg_longdouble = alloca(2 * sizeof(long double));
                      ((double *) arg_longdouble)[0] = reg->fpr[0];
                      ((double *) arg_longdouble)[1] = reg->fpr[2];
                      ((double *) arg_longdouble)[2] = reg->fpr[4];
                      ((double *) arg_longdouble)[3] = reg->fpr[6];
                      args[i] = arg_longdouble;
                      n_gprs = 0;
                      n_fprs = 0;
                      n_arg += 32;
                    }
                  else if (n_fprs == 2)
                    {
                      arg_longdouble = alloca(2 * sizeof(long double));
                      ((double *) arg_longdouble)[0] = reg->fpr[4];
                      ((double *) arg_longdouble)[1] = reg->fpr[6];
                      ((double *) arg_longdouble)[2] = *(double *)(savearea + n_arg + 16);
                      ((double *) arg_longdouble)[3] = *(double *)(savearea + n_arg + 24);
                      args[i] = arg_longdouble;
                      n_gprs = 0;
                      n_fprs = 0;
                      n_arg += 32;
                    }
                  else
                    {
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
          if (n_fixed_remaining > 0)
            {
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
                        && cif->rtype->size > MAX_GPRARG_BYTES);
  if (needs_copy)
    {
      switch (cif->rtype->type)
        {
          case FFI_TYPE_UINT8:
            *(unsigned long *)(retbuf) = *(unsigned long *)ret;
            break;
          case FFI_TYPE_SINT8:
            *(signed long *)(retbuf) = *(unsigned long *)ret;
            break;
          case FFI_TYPE_UINT16:
            *(unsigned long *)(retbuf) = *(unsigned long *)ret;
            break;
          case FFI_TYPE_SINT16:
            *(signed long *)(retbuf) = *(signed long *)ret;
            break;
          case FFI_TYPE_UINT32:
            *(unsigned long *)(retbuf) = *(unsigned long *)ret;
            break;
          case FFI_TYPE_INT:
          case FFI_TYPE_SINT32:
            *(signed long *)(retbuf) = *(signed long *)ret;
            break;
          case FFI_TYPE_UINT64:
          case FFI_TYPE_SINT64:
          case FFI_TYPE_POINTER:
            *(void **)(retbuf) = *(void **)ret;
            break;
          case FFI_TYPE_FLOAT:
            *(float *)(retbuf) = *(float *)ret;
            break;
          case FFI_TYPE_DOUBLE:
            *(double *)(retbuf) = *(double *)ret;
            break;
          default:
            /* Covers STRUCT, COMPLEX, LONGDOUBLE, STRUCT_FF/DD/LDLD subtypes */
            for (int i = 0; i < ret_size; i++)
              ((char *)retbuf)[i] = ((char *)ret)[i];
            break;
        }
    }

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
 

ffi_status
ffi_prep_closure_loc (ffi_closure *closure,
                      ffi_cif *cif,
                      void (*fun) (ffi_cif *, void *, void **, void *),
                      void *user_data,
                      void *codeloc)
{
  if (cif->abi != FFI_XPLINK)
    return FFI_BAD_ABI;

  *(long *)&closure->tramp[0] = 0x0000000000000000;
  *(long *)&closure->tramp[8] = (long)(&closure->tramp[16]);

  /* Trampoline layout: no DSA pushed, no frame on the stack.
 * r1/r2/r3/r4 are left completely untouched so CELQPRLG saves the
 * real caller arguments into 2176(,r4).
 *
 * r0 carries the closure ptr into FFISYS2 (r0 is not an argument
 * register and is not saved by CELQPRLG into the arg area).
 *
 * tramp[8..15]    = entry point = &tramp[16]
 * tramp[120..127] = pointer to ffi_closure_XPLINK function descriptor
 *
 * Instruction sequence (all offsets relative to tramp[0]):
 *   [16]  basr  r5, 0           r5 = tramp+18  (PC anchor)
 *   [18]  lgr   r0, r5          r0 = tramp+18
 *   [22]  ahi   r0, -18         r0 = tramp+0   = closure ptr
 *   [26]  lg    r5, 102(,r5)    r5 = value at tramp[120]  (18+102=120 ✓)
 *   [32]  lmg   r5, r6, 0(,r5) r5 = env, r6 = FFISYS2 entry point
 *   [38]  bcr   15, r6          branch — r0=closure, r1/r2/r3/r4 untouched
 *
 * In FFISYS2: closure ptr is in r0 on entry.
   */

  /* [16]  basr %r5,0              0D 50         r5 = tramp+18 */
  *(short *)&closure->tramp[16] = 0x0d50;

  /* [18]  lgr %r0,%r5             B9 04 00 05   r0 = tramp+18
   * LGR: opcode B904, R1=0, R2=5.  */
  *(short *)&closure->tramp[18] = 0xb904;
  *(short *)&closure->tramp[20] = 0x0005;

  /* [22]  ahi %r0,-18             A7 0A FF EE   r0 = tramp+0 = closure ptr
   * AHI: opcode A7, mask 0A (reg 0, sub-opcode A), immediate -18 = 0xFFEE. */
  *(short *)&closure->tramp[22] = 0xa70a;
  *(short *)&closure->tramp[24] = 0xffee;

  /* [26]  lg %r5,102(,%r5)        E3 50 50 66 00 04
   * r5=tramp+18; tramp+18+102 = tramp+120 = &ffi_closure_XPLINK descriptor ptr.
   * 102 = 0x066.  */
  *(short *)&closure->tramp[26] = 0xe350;
  *(short *)&closure->tramp[28] = 0x5066;
  *(short *)&closure->tramp[30] = 0x0004;

  /* [32]  lmg %r5,%r6,0(,%r5)    EB 56 50 00 00 04
   * Load env->r5, entry->r6 from the ffi_closure_XPLINK function descriptor. */
  *(short *)&closure->tramp[32] = 0xeb56;
  *(short *)&closure->tramp[34] = 0x5000;
  *(short *)&closure->tramp[36] = 0x0004;

  /* [38]  bcr 15,%r6              07 F6
   * Branch to FFISYS2.  r0=closure ptr, r1/r2/r3/r4/r7 all untouched. */
  *(short *)&closure->tramp[38] = 0x07f6;

  *(long *)&closure->tramp[120] = (long)&ffi_closure_XPLINK;

  closure->cif = cif;
  closure->user_data = user_data;
  closure->fun = fun;

  return FFI_OK;
}

/*======================== End of Routine ============================*/

