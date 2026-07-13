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

#include <ffi.h>
#include <ffi_common.h>
 
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
 
/*====================== End of Includes =============================*/
 
/*====================================================================*/
/*                           Defines                                  */
/*                           -------                                  */
/*====================================================================*/

/* Maximum number of GPRs available for argument passing.  */ 
#define MAX_GPRARGS 3

/* Maximum number of FPRs available for argument passing.  */ 
#define MAX_FPRARGS 4

/* Round to multiple of 16.  */
#define ROUND_SIZE(size) (((size) + 15) & ~15)

/* If these values change, xplink.s must be adapted!  */
#define FFI390_RET_VOID		0
#define FFI390_RET_STRUCT	1
#define FFI390_RET_FLOAT	2
#define FFI390_RET_DOUBLE	3
#define FFI390_RET_LDBLE  4
#define FFI390_RET_INT32	5
#define FFI390_RET_INT64  6

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

// void ffi_closure_helper_SYSV (ffi_closure *, unsigned long *, 
//                          unsigned long long *, unsigned long *);

/*====================== End of Prototypes ===========================*/
 
/*====================================================================*/
/*                          Externals                                 */
/*                          ---------                                 */
/*====================================================================*/
 

#pragma map(ffi_call_SYSV, "FFISYS")
extern void ffi_call_SYSV(void (*fn)(void), extended_cif *, 
                          unsigned, unsigned *, unsigned, unsigned);

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
     Note: XPLINK Stack is downward growing

     ------------------------------------ <- Low Addresses
       Guard Page (4KB)
     ------------------------------------ 
       Stack Frame for Called functions
     ------------------------------------ <- Stack Ptr (r4)
       Backchain                            |+2048
     ------------------------------------   |
       Environment                          |
     ------------------------------------   |
       Entry Point                          |  Savearea 
     ------------------------------------   |  48 bytes
       Return Address                       |
     ------------------------------------   |
       R8 - R15                             |
     ------------------------------------ <-
       Reserved (8 bytes)                   +2096
     ------------------------------------ 
       Debug Area (4 bytes)                 +2104
     ------------------------------------ 
       Arg area prefix (4 bytes)            +2108
     ------------------------------------ 
       Argument area: Parm1 ... ParmN       +2112
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
          case FFI_TYPE_STRUCT_FF:
          case FFI_TYPE_STRUCT_DD:
	        case FFI_TYPE_STRUCT_LDLD:
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
 
/**
 * Helper functions to know if the given struct needs to be treated for complex
 * type for float or double.
 */
static unsigned short
get_ffi_element_type_in_struct(ffi_type *arg_type)
{
  while ((FFI_TYPE_STRUCT == arg_type->type)
        && (NULL != arg_type->elements[0])
        && (NULL == arg_type->elements[1])
  ) {
    arg_type = arg_type->elements[0];
  }
  return arg_type->type;
}

static unsigned short
ffi_check_struct_for_complex(ffi_type *arg_type)
{
  if ((FFI_TYPE_STRUCT == arg_type->type) && (NULL != arg_type->elements[0]))
  {
    unsigned short firstArgType = get_ffi_element_type_in_struct(arg_type->elements[0]);
    if (FFI_TYPE_FLOAT == firstArgType)
    {
      if ((NULL != arg_type->elements[1])
          && (NULL == arg_type->elements[2])
          && (FFI_TYPE_FLOAT == get_ffi_element_type_in_struct(arg_type->elements[1]))
      ) {
        return FFI_TYPE_STRUCT_FF;
      }
    }
    else if (FFI_TYPE_DOUBLE == firstArgType)
    {
      if ((NULL != arg_type->elements[1])
          && (NULL == arg_type->elements[2])
          && (FFI_TYPE_DOUBLE == get_ffi_element_type_in_struct(arg_type->elements[1]))
      ) {
        return FFI_TYPE_STRUCT_DD;
      }
    }
    else if (FFI_TYPE_LONGDOUBLE == firstArgType)
    {
      if ((NULL != arg_type->elements[1])
          && (NULL == arg_type->elements[2])
          && (FFI_TYPE_LONGDOUBLE == get_ffi_element_type_in_struct(arg_type->elements[1]))
      ) {
        return FFI_TYPE_STRUCT_LDLD;
      }
    }
  }
  return arg_type->type;
}

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
  size_t struct_size = 0;
  int n_gpr = 0;
  int n_fpr = 0;
  int n_ov = 0;

  ffi_type **ptr;
  int i;

  /* 64-bit XPLINK handling below */

  /* TODO: This comment is describing 31-bit behaviour */
  /* Determine return value handling.  
     Integral values <=4bytes are widened and put in GPR3
     Integral values >4bytes and <=8bytes are widened and put in
     GPR2 (left most 32-bits) and GPR3 (right most 32-bits)
     Floating point values, including complex type, are returned in 
     FPR0, FPR2, FPR4, FPR6 (as many registers as required)
     Aggregates size of <=4 are returned GPR1 (left adjusted)
     Aggregates size between 5bytes-8bytes are returned in GPR1 and 
     GPR2 (left adjusted)
     Aggregates size between 9bytes-12bytes are returned in GPR1, GPR2,
     and GPR3 (left adjusted)
     Anything greater in size and anyother type is returned in a buffer,
     the buffer is passed in as hidden first argument.
     */

  switch (cif->rtype->type)
    {
      /* Void is easy.  */
      case FFI_TYPE_VOID:
        cif->flags = FFI390_RET_VOID;
        break;

      /* Structures are returned in GPR or buffer depending on size.  */
      /* TODO CLEANUP */
      case FFI_TYPE_STRUCT:
        struct_size = cif->rtype->size;
        if (struct_size <= 24)
          cif->flags = FFI390_RET_STRUCT;
        else
          /* dummy point arg, might need to fix for float only struct returns */
          n_gpr++;
          //n_ov++; 
          //n_ov = struct_size;
        break; 

      /* Floating point and complex values are returned in fpr0, 2, 4, 6 */
      case FFI_TYPE_FLOAT:
      case FFI_TYPE_COMPLEX:
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
      /* Integer values are returned in gpr 3 (and gpr 2
         for 64-bit values on 31-bit machines).  */
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

  //cif->rtype->type = ffi_check_struct_for_complex(cif->rtype);
  /* Now for the arguments.  */
 
  for (ptr = cif->arg_types, i = cif->nargs;
       i > 0;
       i--, ptr++)
    {
      int type = (*ptr)->type;

      /* Check how a structure type is passed.  */
      if (type == FFI_TYPE_STRUCT) 
        {
          
          type = ffi_check_struct_type (*ptr);
		      //(*ptr)->type = ffi_check_struct_for_complex(*ptr);
          /* If we pass the struct via pointer, we must reserve space
             to copy its data for proper call-by-value semantics.  */
          if (type == FFI_TYPE_POINTER)
            struct_size += ROUND_SIZE ((*ptr)->size);
        
        }

      /* Now handle all primitive int/float data types.  */
      switch (type) 
      {
               /* The first MAX_FPRARGS floating point arguments
             go in FPRs, the rest overflow to the stack.  */ 
        
        case FFI_TYPE_LONGDOUBLE:
          if (n_fpr < MAX_FPRARGS)
            n_fpr+=2;
          else
            n_ov += sizeof(long double) / sizeof(long);
          break; 
        
        case FFI_TYPE_COMPLEX:
               case FFI_TYPE_DOUBLE:
               case FFI_TYPE_FLOAT:
          if (n_fpr < MAX_FPRARGS)
            n_fpr++;
          else
            n_ov += sizeof (double) / sizeof (long);
          break; 
          
          /* On 31-bit machines, 64-bit integers are passed in GPR pairs,
             if one is still available, or else on the stack.  If only one
             register is free, skip the register (it won't be used for any 
             subsequent argument either).  */ 
        
        case FFI_TYPE_UINT64:
               case FFI_TYPE_SINT64:
          if (n_gpr == MAX_GPRARGS-1)
            n_gpr = MAX_GPRARGS;
          if (n_gpr < MAX_GPRARGS)
            n_gpr += 2;
          else
            n_ov += 2;
          break; 
          
          /* Everything else is passed in GPRs (until MAX_GPRARGS
             have been used) or overflows to the stack.  */ 
        
        default: 
          if (n_gpr < MAX_GPRARGS)
            n_gpr++;
          else
            n_ov++;
          break;
      }
    }

  /* Total stack space as required for:
     -empty slots for arguments passed in registers
     - overflow arguments,
     - and temporary structure copies.  */

  /* TODO
    |- for a number of reasons this code is incorrect
    |- n_ov includes arg struct, should probably just be the 1 point dummy arg 
  */
  cif->bytes = ROUND_SIZE ((n_ov * sizeof (long)) + (n_fpr * sizeof (long long)) + (n_gpr * sizeof (long)) ) + struct_size;
/*  printf("prep_cif_machdep_cif_bytes: %d n_gpr=%d n_ov=%d n_fpr=%d\n",cif->bytes,n_gpr,n_ov,n_fpr); */
 
  return FFI_OK;
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
  /*    if (ret_type == FFI_TYPE_STRUCT)
        ecif.rvalue = alloca (cif->rtype->size);
      else
   */   
        ret_type = FFI_TYPE_VOID;
    } 

  switch (cif->abi)
    {
      case FFI_XPLINK:

        // do this in call, doing it outside the call contaminates the 
        // cif which is a huge pain for parts of ctypes relying on "standard"
        // type information, we can do it here self contained, then undo it.
        if (cif->rtype->type == FFI_TYPE_STRUCT) {
          cif->rtype->type = ffi_check_struct_for_complex(cif->rtype);
        }
	if (cif->rtype->type == FFI_TYPE_STRUCT_LDLD) {
		cif->flags = FFI390_RET_STRUCT;
	}

        for (int i = 0; i < cif->nargs; i++) {
          if (cif->arg_types[i]->type == FFI_TYPE_STRUCT) {
            cif->arg_types[i]->type = ffi_check_struct_for_complex(cif->arg_types[i]);
          }
        }

        ffi_call_SYSV(fn, &ecif, cif->flags, ecif.rvalue, cif->bytes, cif->nargs);

        // undo the aforementioned type sourcery.
        if ((cif->rtype->type == FFI_TYPE_STRUCT_DD) || (cif->rtype->type == FFI_TYPE_STRUCT_FF) || (cif->rtype->type == FFI_TYPE_STRUCT_LDLD)) {
          cif->rtype->type = FFI_TYPE_STRUCT;
        }

        for (int i = 0; i < cif->nargs; i++) {
          if (cif->arg_types[i]->type == FFI_TYPE_STRUCT_DD) {
            cif->arg_types[i]->type = FFI_TYPE_STRUCT;
          }
          else if (cif->arg_types[i]->type == FFI_TYPE_STRUCT_FF) {
            cif->arg_types[i]->type = FFI_TYPE_STRUCT;
          }
          else if (cif->arg_types[i]->type == FFI_TYPE_STRUCT_LDLD) {
            cif->arg_types[i]->type = FFI_TYPE_STRUCT;
          }
        }


#ifdef FFI_DEBUG
        printf("called_ffi_call_sysv nargs=%d\n",cif->nargs);
#endif 
       break;	
      default:
        FFI_ASSERT (0);
        break;
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

/*  
* this routine determines the type of return to be used
* return = 0 -> space allocated by caller, pointer passed in to r1
* return = 1 -> gpr3 contains the return (integral type returns)
* return = 2 -> gpr1 contains the return
* return = 3 -> gpr1+2 contains the return
* return = 4 -> gpr1+2+3 contains the return
* return = 5 -> fpr0 contains the return
* return = 6 -> fpr0+2 contains the return
* return = 7 -> fpr0+2+4 contains the return
* return = 8 -> fpr0+2+4+6 contains the return
*/
int ffi_determine_return_type(ffi_closure *closure) {




  ffi_type *type = closure->cif->rtype;
  if (type == NULL) {
    return 1; // do nothing
  }

  switch(type->type) {
    
    case FFI_TYPE_UINT8:
    case FFI_TYPE_SINT8:
    case FFI_TYPE_UINT16:
    case FFI_TYPE_SINT16:
    case FFI_TYPE_UINT32:
    case FFI_TYPE_SINT32:
    case FFI_TYPE_INT:
    case FFI_TYPE_UINT64:
    case FFI_TYPE_SINT64:
    case FFI_TYPE_POINTER:
      return 1; // grp3
      break;

    case FFI_TYPE_FLOAT:
    case FFI_TYPE_DOUBLE:
      return 5; // fpr0
      break;

    case FFI_TYPE_COMPLEX:
      if (type->size == 8) {
        return 7;
      }
      else if (type->size == 16) {
        return 6;
      }
      else if (type->size == 32) {
        return 8;
      }
      else {
        return 0; // this should never happen but if it does
        // Maybe assert?
      }
      break;

    case FFI_TYPE_STRUCT:
      unsigned short struct_subtype = ffi_check_struct_for_complex(type);
      if (struct_subtype == FFI_TYPE_STRUCT_DD) {
        return 6; // fpr0+2
      }
      else if (struct_subtype == FFI_TYPE_STRUCT_FF) {
        return 7; // fpr0+2 (floats)
      }
      else if (struct_subtype == FFI_TYPE_STRUCT_LDLD) {
        return 8; // fpr0+2+4+6
      }
      else if (type->size <= 8) {
        return 2; // grp1
      }
      else if (type->size <= 16) {
        return 3; // grp1+2
      }
      else if (type->size <= 24) {
        return 4; // gpr1+2+3
      }
      else {
        return 0; // pointer was passed in
      }
      break;
    case FFI_TYPE_VOID:
      return 1;

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

  int ret_size;
  ffi_cif *cif = closure->cif;

  unsigned short struct_subtype;
  if(cif->rtype) {
    struct_subtype = ffi_check_struct_for_complex(cif->rtype);
  }

  int num_args = cif->nargs;
  if (cif->rtype)
    ret_size = cif->rtype->size;
  else if (cif->rtype->type == FFI_TYPE_VOID) {
    ret_size = 24;
  }
  else if(struct_subtype == FFI_TYPE_STRUCT_LDLD) {
    ret_size = 32;
  }
  else {
    // allocate memory regardless
    ret_size = 24;
  }


  // allocate a pointer per arg
  void **args = (void**)alloca(sizeof(void*) * num_args);
  void *ret = (void*)alloca(ret_size);
  char *savearea = ((char*)closure->reg.gpr[3]) + 2176;
  
  // incase we need to pass a complex type, messy but functional
  float *cmplx_arg;
  double *cmplx_arg_double;

  double *arg_longdouble;

  ffi_type **atype = cif->arg_types;

  // number of remaining registers
  int n_gprs = 3;
  int n_fprs = 4;
  // offset into the xplink save area
  int n_arg = 0; 

  if (ret_size > 24 && struct_subtype != FFI_TYPE_STRUCT_LDLD) {
    // if we have a return pointer
    // passed into r1
    n_gprs--;
    n_arg += 8;
    ret = closure->reg.gpr[0];
  }

  for (int i = 0; i < cif->nargs; i++) {
    switch (atype[i]->type) {
      case FFI_TYPE_UINT8:
      case FFI_TYPE_SINT8:
        if (n_gprs == 3) {
          args[i] = (void*)(((long)(&closure->reg.gpr[0])) + 7);
          n_gprs--;
          n_arg += 8;
        }
        else if (n_gprs == 2) { 
          args[i] = (void*)(((long)(&closure->reg.gpr[1])) + 7);
          n_gprs--;
          n_arg += 8;
        }
        else if (n_gprs == 1) { 
          args[i] = (void*)(((long)(&closure->reg.gpr[2])) + 7);
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
          args[i] = (void*)(((long)(&closure->reg.gpr[0])) + 6);
          n_gprs--;
          n_arg += 8;
        }
        else if (n_gprs == 2) { 
          args[i] = (void*)(((long)(&closure->reg.gpr[1])) + 6);
          n_gprs--;
          n_arg += 8;
        }
        else if (n_gprs == 1) { 
          args[i] = (void*)(((long)(&closure->reg.gpr[2])) + 6);
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
          args[i] = (void*)(((long)(&closure->reg.gpr[0])) + 4); 
          n_gprs--;
          n_arg += 8;
        }
        else if (n_gprs == 2) { 
          args[i] = (void*)(((long)(&closure->reg.gpr[1])) + 4); 
          n_gprs--;
          n_arg += 8;
        }
        else if (n_gprs == 1) { 
          args[i] = (void*)(((long)(&closure->reg.gpr[2])) + 4); 
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
          args[i] = &closure->reg.gpr[0];
          n_gprs--;
          n_arg += 8;
        }
        else if (n_gprs == 2) { 
          args[i] = &closure->reg.gpr[1]; 
          n_gprs--;
          n_arg += 8;
        }
        else if (n_gprs == 1) { 
          args[i] = &closure->reg.gpr[2];
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
        if (n_fprs == 4) {
          args[i] = &closure->reg.fpr[0];
          n_gprs--;
          n_fprs--;
          n_arg += 8;
        }
        else if (n_fprs == 3) {
          args[i] = &closure->reg.fpr[2];
          n_gprs--;
          n_fprs--;
          n_arg += 8;
        }
        else if (n_fprs == 2) {
          args[i] = &closure->reg.fpr[4];
          n_gprs--;
          n_fprs--;
          n_arg += 8;
        }        
        else if (n_fprs == 1) {
          args[i] = &closure->reg.fpr[6];
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
        if (n_fprs == 4) {
          arg_longdouble = alloca(sizeof(long double));
          arg_longdouble[0] = closure->reg.fpr[0];
          arg_longdouble[1] = closure->reg.fpr[2];
          args[i] = arg_longdouble;
          n_gprs-=2;
          n_fprs-=2;
          n_arg += 16;
        } 
        else if ( (n_fprs == 3) || (n_fprs == 2) ) { // TODO FIX
          arg_longdouble = alloca(sizeof(long double));
          arg_longdouble[0] = closure->reg.fpr[4];
          arg_longdouble[1] = closure->reg.fpr[6];
          args[i] = arg_longdouble;
          n_gprs-=2;
          n_fprs-=2;
          n_arg += 16;
        }
        else if (n_fprs == 1) { // TODO FIX
          args[i] = (savearea + n_arg);
          n_gprs-=2;
          n_fprs-=2;
          n_arg += 16;
        }
        else {
          args[i] = (savearea + n_arg);
          n_arg += 16;
        }
        break; 
      case FFI_TYPE_STRUCT:
      case FFI_TYPE_STRUCT_DD:
      case FFI_TYPE_STRUCT_FF:
      case FFI_TYPE_STRUCT_LDLD:	
      unsigned short struct_subtype = ffi_check_struct_for_complex(atype[i]);
        if (struct_subtype == FFI_TYPE_STRUCT) {
        // TODO fill this in
        if (atype[i]->size <= 8) {
          // handle like an int basically
          if (n_gprs == 3) {
            args[i] = &closure->reg.gpr[0];
            n_gprs--;
            n_arg += 8;
          }
          else if (n_gprs == 2) { 
            args[i] = &closure->reg.gpr[1]; 
            n_gprs--;
            n_arg += 8;
          }
          else if (n_gprs == 1) { 
            args[i] = &closure->reg.gpr[2];
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
            args[i] = &closure->reg.gpr[0];
            n_gprs -= 2;
            n_arg += 16;
          }
          else if (n_gprs == 2) { 
            args[i] = &closure->reg.gpr[1]; 
            n_gprs -= 2;
            n_arg += 16;
          }
          else if (n_gprs == 1) { 
            // so we have overlap with closure->reg+memory
            // think we can just save to save area and use that
            *(long*)(savearea + n_arg) = (long)closure->reg.gpr[2];
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
            args[i] = &closure->reg.gpr[0];
            n_gprs -= 3;
            n_arg += 24;
          }
          else if (n_gprs == 2) {
            *(long*)(savearea + n_arg) = (long)closure->reg.gpr[1];
            *(long*)(savearea + n_arg + 8) = (long)closure->reg.gpr[2];
            args[i] = (savearea + n_arg);
            n_gprs -= 2;
            n_arg += 24;
          }
          else if (n_gprs == 1) {
            *(long*)(savearea + n_arg) = (long)closure->reg.gpr[2];
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
            *(long*)(savearea + n_arg) = (long)closure->reg.gpr[0];
            *(long*)(savearea + n_arg + 8) = (long)closure->reg.gpr[1];
            *(long*)(savearea + n_arg + 16) = (long)closure->reg.gpr[2];
            args[i] = (savearea + n_arg);
            n_gprs -= 3;
            n_arg += atype[i]->size;
          }
          else if (n_gprs == 2) {
            *(long*)(savearea + n_arg) = (long)closure->reg.gpr[1];
            *(long*)(savearea + n_arg + 8) = (long)closure->reg.gpr[2];
            args[i] = (savearea + n_arg);
            n_gprs -= 2;
            n_arg += atype[i]->size;
          }
          else if (n_gprs == 1) {
            *(long*)(savearea + n_arg) = (long)closure->reg.gpr[2];
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
        else if(struct_subtype == FFI_TYPE_STRUCT_DD){
        if (n_fprs == 4) {
          args[i] = &closure->reg.fpr[0];
          closure->reg.fpr[1] = closure->reg.fpr[2];
          n_gprs--;
          n_fprs -= 2;
          n_arg += 16;
        }
        else if (n_fprs == 3) {
          args[i] = &closure->reg.fpr[2];
          closure->reg.fpr[3] = closure->reg.fpr[4];
          n_gprs--;
          n_fprs -= 2;
          n_arg += 16;
        }
        else if (n_fprs == 2) {
          args[i] = &closure->reg.fpr[4];
          closure->reg.fpr[5] = closure->reg.fpr[6];
          n_gprs--;
          n_fprs -= 2;
          n_arg += 16;
        }        
        else if (n_fprs == 1) {
          cmplx_arg_double = alloca(2*sizeof(double));
          args[i] = cmplx_arg_double;
          cmplx_arg_double[0] = closure->reg.fpr[6];
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
        else if(struct_subtype == FFI_TYPE_STRUCT_FF){
            cmplx_arg = alloca(2*sizeof(float));
            if (n_fprs == 4) {          
            args[i] = cmplx_arg;
            cmplx_arg[0] = *(float*)&closure->reg.fpr[0];
            cmplx_arg[1] = *(float*)&closure->reg.fpr[2];
            n_gprs--;
            n_fprs -= 2;
            n_arg += 8;
            }
            else if ((n_fprs == 3)) {
            args[i] = cmplx_arg;
            cmplx_arg[0] = *(float*)&closure->reg.fpr[2];
            cmplx_arg[1] = *(float*)&closure->reg.fpr[4];
            n_gprs--;
            n_fprs -= 2;
            n_arg += 8;
            }
            else if ((n_fprs == 2)) { 
            args[i] = cmplx_arg;
            cmplx_arg[0] = *(float*)&closure->reg.fpr[4];
            cmplx_arg[1] = *(float*)&closure->reg.fpr[6];
            n_gprs--;
            n_fprs -= 2;
            n_arg += 8;
            }        
            else if (n_fprs == 1) {
            args[i] = cmplx_arg;
            cmplx_arg[0] = *(float*)&closure->reg.fpr[6];
            cmplx_arg[1] = *(float*)(savearea + n_arg + 4);
            n_gprs--;
            n_fprs -= 2;
            n_arg += 8;
            }
            else {
            args[i] = (savearea + n_arg);
            cmplx_arg = (savearea + n_arg);
            n_arg += 8;
            }
        }
        else if(struct_subtype == FFI_TYPE_STRUCT_LDLD){
            if (n_fprs == 4) {
                arg_longdouble = alloca(2*sizeof(long double));
                arg_longdouble[0] = closure->reg.fpr[0];
                arg_longdouble[1] = closure->reg.fpr[2];
                arg_longdouble[2] = closure->reg.fpr[4];
                arg_longdouble[3] = closure->reg.fpr[6];
                args[i] = arg_longdouble;
                n_gprs = 0;
                n_fprs = 0;
                n_arg += 32;
            }
	    else if(n_fprs == 2){
                arg_longdouble = alloca(2*sizeof(long double));
                arg_longdouble[0] = closure->reg.fpr[4];
                arg_longdouble[1] = closure->reg.fpr[6];
                arg_longdouble[2] = *(double *)(savearea + n_arg + 16);
                arg_longdouble[3] = *(double *)(savearea + n_arg + 24);
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

  closure->fun(cif, ret, args, closure->user_data);

  if (cif->rtype == NULL) {
  	*(void**)(retbuf) = *(void**)ret;
  }
  else if (cif->rtype->type == FFI_TYPE_VOID) {
	*(void**)(retbuf) = *(void**)ret;
  }


  if (((cif->rtype != NULL) || (cif->rtype->type != FFI_TYPE_VOID)) && (ret_size <= 24 || struct_subtype == FFI_TYPE_STRUCT_LDLD)) {
    switch(cif->rtype->type) {
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
        for (int i = 0; i < ret_size; i++) {
          ((char*)retbuf)[i] = ((char*)ret)[i];
        }
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

  *(long  *)&closure->tramp [0]  = 0x000000000000000;
  *(long  *)&closure->tramp [8]  = (long)(&(closure->tramp[16]));

  // put the pc in r5 so we can find the closure
  *(short *)&closure->tramp [16] = 0x0d50;   /* basr %r5,0 */
    
  // offset into closure register save area 
  *(short *)&closure->tramp [18] = 0xa75b;   /* aghi %r5,70 */
  *(short *)&closure->tramp [20] = 0x0046;

  // save the registers into the closure
  *(short *)&closure->tramp [22] = 0xeb1f;   /* stmg %r1,%r15,0(%r5) */
  *(short *)&closure->tramp [24] = 0x5000;
  *(short *)&closure->tramp [26] = 0x0024;

  // put the addr of the closure into r1 
  *(short *)&closure->tramp [28] = 0xe310;   /* lay %r1,-86(,%r5) */ 
  *(short *)&closure->tramp [30] = 0x5fa8;
  *(short *)&closure->tramp [32] = 0xff71;

  // adjust r5 to point to the help address 
  *(short *)&closure->tramp [34] = 0xa75b;   /* aghi %r5,-34*/
  *(short *)&closure->tramp [36] = 0xffde;

  // // load the pointer to the helper address
  *(short *)&closure->tramp [38] = 0xe350;  /* lg %r5,0(,%r5) */
  *(short *)&closure->tramp [40] = 0x5000;   
  *(short *)&closure->tramp [42] = 0x0004;

  *(short *)&closure->tramp [44] = 0xeb56;  /* lmg %r5,%r6,0(,%r5) */
  *(short *)&closure->tramp [46] = 0x5000;
  *(short *)&closure->tramp [48] = 0x0004;

  // load the actual address of the help 
  // *(short *)&closure->tramp [44] = 0xe350;   /* lg %r5,8(,%r5) */
  // *(short *)&closure->tramp [46] = 0x5008;   
  // *(short *)&closure->tramp [48] = 0x0004;

  // branch to the helper
  *(short *)&closure->tramp [50] = 0x0d66;   /* basr %r5,%r5 */

  // pointer to the helper address
  *(long  *)&closure->tramp [54] = ((long)&ffi_closure_SYSV);

  closure->cif = cif;
  closure->user_data = user_data;
  closure->fun = fun;

  return FFI_OK;
}

/*======================== End of Routine ============================*/
 
