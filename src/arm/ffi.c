/* -----------------------------------------------------------------------
   ffi.c - Copyright (c) 2011 Timothy Wall
           Copyright (c) 2011 Plausible Labs Cooperative, Inc.
           Copyright (c) 2011 Anthony Green
	   Copyright (c) 2011 Free Software Foundation
           Copyright (c) 1998, 2008, 2011  Red Hat, Inc.

   ARM Foreign Function Interface

   Permission is hereby granted, free of charge, to any person obtaining
   a copy of this software and associated documentation files (the
   ``Software''), to deal in the Software without restriction, including
   without limitation the rights to use, copy, modify, merge, publish,
   distribute, sublicense, and/or sell copies of the Software, and to
   permit persons to whom the Software is furnished to do so, subject to
   the following conditions:

   The above copyright notice and this permission notice shall be included
   in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED ``AS IS'', WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
   NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
   HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
   WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.
   ----------------------------------------------------------------------- */

#include <ffi.h>
#include <ffi_common.h>

#include <stdlib.h>

/* Forward declares. */
static int vfp_type_p (const ffi_type *);
static void layout_vfp_args (ffi_cif *);

int ffi_prep_args_SYSV (char *stack, extended_cif *ecif, float *vfp_space);
int ffi_prep_args_VFP (char *stack, extended_cif *ecif, float *vfp_space);

static void *
ffi_align (ffi_type *ty, void *p)
{
  /* Align if necessary */
  size_t alignment;
#ifdef _WIN32_WCE
  alignment = 4;
#else
  alignment = ty->alignment;
  if (alignment < 4)
    alignment = 4;
#endif
  return (void *) ALIGN (p, alignment);
}

static size_t
ffi_put_arg (ffi_type *ty, void *src, void *dst)
{
  size_t z = ty->size;

  switch (ty->type)
    {
    case FFI_TYPE_SINT8:
      *(UINT32 *)dst = *(SINT8 *)src;
      break;
    case FFI_TYPE_UINT8:
      *(UINT32 *)dst = *(UINT8 *)src;
      break;
    case FFI_TYPE_SINT16:
      *(UINT32 *)dst = *(SINT16 *)src;
      break;
    case FFI_TYPE_UINT16:
      *(UINT32 *)dst = *(UINT16 *)src;
      break;

    case FFI_TYPE_INT:
    case FFI_TYPE_SINT32:
    case FFI_TYPE_UINT32:
    case FFI_TYPE_POINTER:
    case FFI_TYPE_FLOAT:
      *(UINT32 *)dst = *(UINT32 *)src;
      break;

    case FFI_TYPE_SINT64:
    case FFI_TYPE_UINT64:
    case FFI_TYPE_DOUBLE:
      *(UINT64 *)dst = *(UINT64 *)src;
      break;

    case FFI_TYPE_STRUCT:
    default:
      memcpy (dst, src, z);
      break;
    }

  return ALIGN (z, 4);
}

/* ffi_prep_args is called by the assembly routine once stack space
   has been allocated for the function's arguments

   The vfp_space parameter is the load area for VFP regs, the return
   value is cif->vfp_used (word bitset of VFP regs used for passing
   arguments). These are only used for the VFP hard-float ABI.
*/
int
ffi_prep_args_SYSV (char *stack, extended_cif *ecif, float *vfp_space)
{
  register unsigned int i;
  register void **p_argv;
  register char *argp;
  register ffi_type **p_arg;
  argp = stack;

  if (ecif->cif->flags == FFI_TYPE_STRUCT)
    {
      *(void **) argp = ecif->rvalue;
      argp += 4;
    }

  p_argv = ecif->avalue;

  for (i = ecif->cif->nargs, p_arg = ecif->cif->arg_types;
       (i != 0); i--, p_arg++, p_argv++)
    {
      argp = ffi_align (*p_arg, argp);
      argp += ffi_put_arg (*p_arg, *p_argv, argp);
    }

  return 0;
}

int
ffi_prep_args_VFP (char *stack, extended_cif * ecif, float *vfp_space)
{
  register unsigned int i, vi = 0;
  register void **p_argv;
  register char *argp, *regp, *eo_regp;
  register ffi_type **p_arg;
  char stack_used = 0;
  char done_with_regs = 0;

  /* Make sure we are using FFI_VFP.  */
  FFI_ASSERT (ecif->cif->abi == FFI_VFP);

  /* The first 4 words on the stack are used for values
     passed in core registers.  */
  regp = stack;
  eo_regp = argp = regp + 16;

  /* If the function returns an FFI_TYPE_STRUCT in memory,
     that address is passed in r0 to the function.  */
  if (ecif->cif->flags == FFI_TYPE_STRUCT)
    {
      *(void **) regp = ecif->rvalue;
      regp += 4;
    }

  p_argv = ecif->avalue;

  for (i = ecif->cif->nargs, p_arg = ecif->cif->arg_types;
       (i != 0); i--, p_arg++, p_argv++)
    {
      int is_vfp_type = vfp_type_p (*p_arg);

      /* Allocated in VFP registers. */
      if (vi < ecif->cif->vfp_nargs && is_vfp_type)
	{
	  char *vfp_slot = (char *) (vfp_space + ecif->cif->vfp_args[vi++]);
	  ffi_put_arg (*p_arg, *p_argv, vfp_slot);
	  continue;
	}
      /* Try allocating in core registers. */
      else if (!done_with_regs && !is_vfp_type)
	{
	  char *tregp = ffi_align (*p_arg, regp);
	  size_t size = (*p_arg)->size;
	  size = (size < 4) ? 4 : size;	// pad
	  /* Check if there is space left in the aligned register
	     area to place the argument.  */
	  if (tregp + size <= eo_regp)
	    {
	      regp = tregp + ffi_put_arg (*p_arg, *p_argv, tregp);
	      done_with_regs = (regp == argp);
	      // ensure we did not write into the stack area
	      FFI_ASSERT (regp <= argp);
	      continue;
	    }
	  /* In case there are no arguments in the stack area yet,
	     the argument is passed in the remaining core registers
	     and on the stack.  */
	  else if (!stack_used)
	    {
	      stack_used = 1;
	      done_with_regs = 1;
	      argp = tregp + ffi_put_arg (*p_arg, *p_argv, tregp);
	      FFI_ASSERT (eo_regp < argp);
	      continue;
	    }
	}
      /* Base case, arguments are passed on the stack */
      stack_used = 1;
      argp = ffi_align (*p_arg, argp);
      argp += ffi_put_arg (*p_arg, *p_argv, argp);
    }
  /* Indicate the VFP registers used. */
  return ecif->cif->vfp_used;
}

/* Perform machine dependent cif processing */
ffi_status
ffi_prep_cif_machdep (ffi_cif * cif)
{
  /* Round the stack up to a multiple of 8 bytes.  This isn't needed
     everywhere, but it is on some platforms, and it doesn't harm anything
     when it isn't needed.  */
  cif->bytes = (cif->bytes + 7) & ~7;

  /* Set the return type flag */
  switch (cif->rtype->type)
    {
    case FFI_TYPE_VOID:
    case FFI_TYPE_FLOAT:
    case FFI_TYPE_DOUBLE:
      cif->flags = (unsigned) cif->rtype->type;
      break;

    case FFI_TYPE_SINT64:
    case FFI_TYPE_UINT64:
      cif->flags = (unsigned) FFI_TYPE_SINT64;
      break;

    case FFI_TYPE_STRUCT:
      if (cif->abi == FFI_VFP)
	{
	  int h = vfp_type_p (cif->rtype);
	  if (h)
	    {
	      int ele_count = h >> 8;
	      int type_code = h & 0xff;
	      if (ele_count > 1)
		{
		  if (type_code == FFI_TYPE_FLOAT)
		    type_code = FFI_TYPE_STRUCT_VFP_FLOAT;
		  else
		    type_code = FFI_TYPE_STRUCT_VFP_DOUBLE;
		}
	      cif->flags = type_code;
	      break;
	    }
	}
      if (cif->rtype->size <= 4)
	{
	  /* A Composite Type not larger than 4 bytes is returned in r0.  */
	  cif->flags = (unsigned) FFI_TYPE_INT;
	}
      else
	{
	  /* A Composite Type larger than 4 bytes, or whose size cannot
	     be determined statically ... is stored in memory at an
	     address passed [in r0].  */
	  cif->flags = (unsigned) FFI_TYPE_STRUCT;
	}
      break;

    default:
      cif->flags = FFI_TYPE_INT;
      break;
    }

  /* Map out the register placements of VFP register args.  The VFP
     hard-float calling conventions are slightly more sophisticated
     than the base calling conventions, so we do it here instead of
     in ffi_prep_args(). */
  if (cif->abi == FFI_VFP)
    layout_vfp_args (cif);

  return FFI_OK;
}

/* Perform machine dependent cif processing for variadic calls */
ffi_status
ffi_prep_cif_machdep_var (ffi_cif * cif,
			  unsigned int nfixedargs, unsigned int ntotalargs)
{
  /* VFP variadic calls actually use the SYSV ABI */
  if (cif->abi == FFI_VFP)
    cif->abi = FFI_SYSV;

  return ffi_prep_cif_machdep (cif);
}

/* Prototypes for assembly functions, in sysv.S.  */
extern void ffi_call_SYSV (void (*fn) (void), extended_cif *, unsigned,
			   unsigned, unsigned *);
extern void ffi_call_VFP (void (*fn) (void), extended_cif *, unsigned,
			  unsigned, unsigned *);

void
ffi_call (ffi_cif * cif, void (*fn) (void), void *rvalue, void **avalue)
{
  extended_cif ecif;

  int small_struct = (cif->flags == FFI_TYPE_INT
		      && cif->rtype->type == FFI_TYPE_STRUCT);
  int vfp_struct = (cif->flags == FFI_TYPE_STRUCT_VFP_FLOAT
		    || cif->flags == FFI_TYPE_STRUCT_VFP_DOUBLE);

  unsigned int temp;

  ecif.cif = cif;
  ecif.avalue = avalue;

  /* If the return value is a struct and we don't have a return
     value address then we need to make one.  */

  if ((rvalue == NULL) && (cif->flags == FFI_TYPE_STRUCT))
    {
      ecif.rvalue = alloca (cif->rtype->size);
    }
  else if (small_struct)
    ecif.rvalue = &temp;
  else if (vfp_struct)
    {
      /* Largest case is double x 4. */
      ecif.rvalue = alloca (32);
    }
  else
    ecif.rvalue = rvalue;

  switch (cif->abi)
    {
    case FFI_SYSV:
      ffi_call_SYSV (fn, &ecif, cif->bytes, cif->flags, ecif.rvalue);
      break;

    case FFI_VFP:
#ifdef __ARM_EABI__
      ffi_call_VFP (fn, &ecif, cif->bytes, cif->flags, ecif.rvalue);
      break;
#endif

    default:
      FFI_ASSERT (0);
      break;
    }
  if (small_struct)
    {
      FFI_ASSERT (rvalue != NULL);
      memcpy (rvalue, &temp, cif->rtype->size);
    }
  else if (vfp_struct)
    {
      FFI_ASSERT (rvalue != NULL);
      memcpy (rvalue, ecif.rvalue, cif->rtype->size);
    }
}

/** private members **/

static void ffi_prep_incoming_args_SYSV (char *stack, void **ret,
					 void **args, ffi_cif *cif,
					 float *vfp_stack);

static void ffi_prep_incoming_args_VFP (char *stack, void **ret,
					void **args, ffi_cif *cif,
					float *vfp_stack);

void ffi_closure_SYSV (ffi_closure *);

void ffi_closure_VFP (ffi_closure *);

/* This function is jumped to by the trampoline */

unsigned int FFI_HIDDEN
ffi_closure_inner (ffi_closure *closure,
		   void **respp, void *args, void *vfp_args)
{
  // our various things...
  ffi_cif *cif;
  void **arg_area;

  cif = closure->cif;
  arg_area = (void **) alloca (cif->nargs * sizeof (void *));

  /* this call will initialize ARG_AREA, such that each
   * element in that array points to the corresponding
   * value on the stack; and if the function returns
   * a structure, it will re-set RESP to point to the
   * structure return address.  */
  if (cif->abi == FFI_VFP)
    ffi_prep_incoming_args_VFP (args, respp, arg_area, cif, vfp_args);
  else
    ffi_prep_incoming_args_SYSV (args, respp, arg_area, cif, vfp_args);

  (closure->fun) (cif, *respp, arg_area, closure->user_data);

  return cif->flags;
}

/*@-exportheader@*/
static void
ffi_prep_incoming_args_SYSV (char *stack, void **rvalue,
			     void **avalue, ffi_cif *cif,
			     /* Used only under VFP hard-float ABI. */
			     float *vfp_stack)
/*@=exportheader@*/
{
  register unsigned int i;
  register void **p_argv;
  register char *argp;
  register ffi_type **p_arg;

  argp = stack;

  if (cif->flags == FFI_TYPE_STRUCT)
    {
      *rvalue = *(void **) argp;
      argp += 4;
    }

  p_argv = avalue;

  for (i = cif->nargs, p_arg = cif->arg_types; (i != 0); i--, p_arg++)
    {
      size_t z;

      argp = ffi_align (*p_arg, argp);

      z = (*p_arg)->size;

      /* because we're little endian, this is what it turns into.   */

      *p_argv = (void *) argp;

      p_argv++;
      argp += z;
    }

  return;
}

/*@-exportheader@*/
static void
ffi_prep_incoming_args_VFP (char *stack, void **rvalue,
			    void **avalue, ffi_cif * cif,
			    /* Used only under VFP hard-float ABI. */
			    float *vfp_stack)
/*@=exportheader@*/
{
  register unsigned int i, vi = 0;
  register void **p_argv;
  register char *argp, *regp, *eo_regp;
  register ffi_type **p_arg;
  char done_with_regs = 0;
  char stack_used = 0;

  FFI_ASSERT (cif->abi == FFI_VFP);
  regp = stack;
  eo_regp = argp = regp + 16;

  if (cif->flags == FFI_TYPE_STRUCT)
    {
      *rvalue = *(void **) regp;
      regp += 4;
    }

  p_argv = avalue;

  for (i = cif->nargs, p_arg = cif->arg_types; (i != 0); i--, p_arg++)
    {
      int is_vfp_type = vfp_type_p (*p_arg);
      size_t z;

      if (vi < cif->vfp_nargs && is_vfp_type)
	{
	  *p_argv++ = (void *) (vfp_stack + cif->vfp_args[vi++]);
	  continue;
	}
      else if (!done_with_regs && !is_vfp_type)
	{
	  char *tregp = ffi_align (*p_arg, regp);

	  z = (*p_arg)->size;
	  z = (z < 4) ? 4 : z;	// pad

	  /* if the arguments either fits into the registers or uses registers
	   * and stack, while we haven't read other things from the stack */
	  if (tregp + z <= eo_regp || !stack_used)
	    {
	      /* because we're little endian, this is what it turns into. */
	      *p_argv = (void *) tregp;

	      p_argv++;
	      regp = tregp + z;
	      // if we read past the last core register, make sure we have not read
	      // from the stack before and continue reading after regp
	      if (regp > eo_regp)
		{
		  if (stack_used)
		    {
		      abort ();	// we should never read past the end of the register
		      // are if the stack is already in use
		    }
		  argp = regp;
		}
	      if (regp >= eo_regp)
		{
		  done_with_regs = 1;
		  stack_used = 1;
		}
	      continue;
	    }
	}
      stack_used = 1;

      argp = ffi_align (*p_arg, argp);

      z = (*p_arg)->size;

      /* because we're little endian, this is what it turns into.   */

      *p_argv = (void *) argp;

      p_argv++;
      argp += z;
    }

  return;
}

/* How to make a trampoline.  */

extern unsigned int ffi_arm_trampoline[3];

#if FFI_EXEC_TRAMPOLINE_TABLE

#include <mach/mach.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

extern void *ffi_closure_trampoline_table_page;

typedef struct ffi_trampoline_table ffi_trampoline_table;
typedef struct ffi_trampoline_table_entry ffi_trampoline_table_entry;

struct ffi_trampoline_table
{
  /* contiguous writable and executable pages */
  vm_address_t config_page;
  vm_address_t trampoline_page;

  /* free list tracking */
  uint16_t free_count;
  ffi_trampoline_table_entry *free_list;
  ffi_trampoline_table_entry *free_list_pool;

  ffi_trampoline_table *prev;
  ffi_trampoline_table *next;
};

struct ffi_trampoline_table_entry
{
  void *(*trampoline) ();
  ffi_trampoline_table_entry *next;
};

/* Override the standard architecture trampoline size */
// XXX TODO - Fix
#undef FFI_TRAMPOLINE_SIZE
#define FFI_TRAMPOLINE_SIZE 12

/* The trampoline configuration is placed at 4080 bytes prior to the trampoline's entry point */
#define FFI_TRAMPOLINE_CODELOC_CONFIG(codeloc) ((void **) (((uint8_t *) codeloc) - 4080));

/* The first 16 bytes of the config page are unused, as they are unaddressable from the trampoline page. */
#define FFI_TRAMPOLINE_CONFIG_PAGE_OFFSET 16

/* Total number of trampolines that fit in one trampoline table */
#define FFI_TRAMPOLINE_COUNT ((PAGE_SIZE - FFI_TRAMPOLINE_CONFIG_PAGE_OFFSET) / FFI_TRAMPOLINE_SIZE)

static pthread_mutex_t ffi_trampoline_lock = PTHREAD_MUTEX_INITIALIZER;
static ffi_trampoline_table *ffi_trampoline_tables = NULL;

static ffi_trampoline_table *
ffi_trampoline_table_alloc ()
{
  ffi_trampoline_table *table = NULL;

  /* Loop until we can allocate two contiguous pages */
  while (table == NULL)
    {
      vm_address_t config_page = 0x0;
      kern_return_t kt;

      /* Try to allocate two pages */
      kt =
	vm_allocate (mach_task_self (), &config_page, PAGE_SIZE * 2,
		     VM_FLAGS_ANYWHERE);
      if (kt != KERN_SUCCESS)
	{
	  fprintf (stderr, "vm_allocate() failure: %d at %s:%d\n", kt,
		   __FILE__, __LINE__);
	  break;
	}

      /* Now drop the second half of the allocation to make room for the trampoline table */
      vm_address_t trampoline_page = config_page + PAGE_SIZE;
      kt = vm_deallocate (mach_task_self (), trampoline_page, PAGE_SIZE);
      if (kt != KERN_SUCCESS)
	{
	  fprintf (stderr, "vm_deallocate() failure: %d at %s:%d\n", kt,
		   __FILE__, __LINE__);
	  break;
	}

      /* Remap the trampoline table to directly follow the config page */
      vm_prot_t cur_prot;
      vm_prot_t max_prot;

      kt =
	vm_remap (mach_task_self (), &trampoline_page, PAGE_SIZE, 0x0, FALSE,
		  mach_task_self (),
		  (vm_address_t) & ffi_closure_trampoline_table_page, FALSE,
		  &cur_prot, &max_prot, VM_INHERIT_SHARE);

      /* If we lost access to the destination trampoline page, drop our config allocation mapping and retry */
      if (kt != KERN_SUCCESS)
	{
	  /* Log unexpected failures */
	  if (kt != KERN_NO_SPACE)
	    {
	      fprintf (stderr, "vm_remap() failure: %d at %s:%d\n", kt,
		       __FILE__, __LINE__);
	    }

	  vm_deallocate (mach_task_self (), config_page, PAGE_SIZE);
	  continue;
	}

      /* We have valid trampoline and config pages */
      table = calloc (1, sizeof (ffi_trampoline_table));
      table->free_count = FFI_TRAMPOLINE_COUNT;
      table->config_page = config_page;
      table->trampoline_page = trampoline_page;

      /* Create and initialize the free list */
      table->free_list_pool =
	calloc (FFI_TRAMPOLINE_COUNT, sizeof (ffi_trampoline_table_entry));

      uint16_t i;
      for (i = 0; i < table->free_count; i++)
	{
	  ffi_trampoline_table_entry *entry = &table->free_list_pool[i];
	  entry->trampoline =
	    (void *) (table->trampoline_page + (i * FFI_TRAMPOLINE_SIZE));

	  if (i < table->free_count - 1)
	    entry->next = &table->free_list_pool[i + 1];
	}

      table->free_list = table->free_list_pool;
    }

  return table;
}

void *
ffi_closure_alloc (size_t size, void **code)
{
  /* Create the closure */
  ffi_closure *closure = malloc (size);
  if (closure == NULL)
    return NULL;

  pthread_mutex_lock (&ffi_trampoline_lock);

  /* Check for an active trampoline table with available entries. */
  ffi_trampoline_table *table = ffi_trampoline_tables;
  if (table == NULL || table->free_list == NULL)
    {
      table = ffi_trampoline_table_alloc ();
      if (table == NULL)
	{
	  free (closure);
	  return NULL;
	}

      /* Insert the new table at the top of the list */
      table->next = ffi_trampoline_tables;
      if (table->next != NULL)
	table->next->prev = table;

      ffi_trampoline_tables = table;
    }

  /* Claim the free entry */
  ffi_trampoline_table_entry *entry = ffi_trampoline_tables->free_list;
  ffi_trampoline_tables->free_list = entry->next;
  ffi_trampoline_tables->free_count--;
  entry->next = NULL;

  pthread_mutex_unlock (&ffi_trampoline_lock);

  /* Initialize the return values */
  *code = entry->trampoline;
  closure->trampoline_table = table;
  closure->trampoline_table_entry = entry;

  return closure;
}

void
ffi_closure_free (void *ptr)
{
  ffi_closure *closure = ptr;

  pthread_mutex_lock (&ffi_trampoline_lock);

  /* Fetch the table and entry references */
  ffi_trampoline_table *table = closure->trampoline_table;
  ffi_trampoline_table_entry *entry = closure->trampoline_table_entry;

  /* Return the entry to the free list */
  entry->next = table->free_list;
  table->free_list = entry;
  table->free_count++;

  /* If all trampolines within this table are free, and at least one other table exists, deallocate
   * the table */
  if (table->free_count == FFI_TRAMPOLINE_COUNT
      && ffi_trampoline_tables != table)
    {
      /* Remove from the list */
      if (table->prev != NULL)
	table->prev->next = table->next;

      if (table->next != NULL)
	table->next->prev = table->prev;

      /* Deallocate pages */
      kern_return_t kt;
      kt = vm_deallocate (mach_task_self (), table->config_page, PAGE_SIZE);
      if (kt != KERN_SUCCESS)
	fprintf (stderr, "vm_deallocate() failure: %d at %s:%d\n", kt,
		 __FILE__, __LINE__);

      kt =
	vm_deallocate (mach_task_self (), table->trampoline_page, PAGE_SIZE);
      if (kt != KERN_SUCCESS)
	fprintf (stderr, "vm_deallocate() failure: %d at %s:%d\n", kt,
		 __FILE__, __LINE__);

      /* Deallocate free list */
      free (table->free_list_pool);
      free (table);
    }
  else if (ffi_trampoline_tables != table)
    {
      /* Otherwise, bump this table to the top of the list */
      table->prev = NULL;
      table->next = ffi_trampoline_tables;
      if (ffi_trampoline_tables != NULL)
	ffi_trampoline_tables->prev = table;

      ffi_trampoline_tables = table;
    }

  pthread_mutex_unlock (&ffi_trampoline_lock);

  /* Free the closure */
  free (closure);
}

#else

#define FFI_INIT_TRAMPOLINE(TRAMP,FUN,CTX)				\
({ unsigned char *__tramp = (unsigned char*)(TRAMP);			\
   unsigned int  __fun = (unsigned int)(FUN);				\
   unsigned int  __ctx = (unsigned int)(CTX);				\
   unsigned char *insns = (unsigned char *)(CTX);                       \
   memcpy (__tramp, ffi_arm_trampoline, sizeof ffi_arm_trampoline);     \
   *(unsigned int*) &__tramp[12] = __ctx;				\
   *(unsigned int*) &__tramp[16] = __fun;				\
   __clear_cache((&__tramp[0]), (&__tramp[19])); /* Clear data mapping.  */ \
   __clear_cache(insns, insns + 3 * sizeof (unsigned int));             \
                                                 /* Clear instruction   \
                                                    mapping.  */        \
 })

#endif

/* the cif must already be prep'ed */

ffi_status
ffi_prep_closure_loc (ffi_closure * closure,
		      ffi_cif * cif,
		      void (*fun) (ffi_cif *, void *, void **, void *),
		      void *user_data, void *codeloc)
{
  void (*closure_func) (ffi_closure *) = NULL;

  if (cif->abi == FFI_SYSV)
    closure_func = &ffi_closure_SYSV;
#ifdef __ARM_EABI__
  else if (cif->abi == FFI_VFP)
    closure_func = &ffi_closure_VFP;
#endif
  else
    return FFI_BAD_ABI;

#if FFI_EXEC_TRAMPOLINE_TABLE
  void **config = FFI_TRAMPOLINE_CODELOC_CONFIG (codeloc);
  config[0] = closure;
  config[1] = closure_func;
#else
  FFI_INIT_TRAMPOLINE (&closure->tramp[0], closure_func, codeloc);
#endif

  closure->cif = cif;
  closure->user_data = user_data;
  closure->fun = fun;

  return FFI_OK;
}

/* Below are routines for VFP hard-float support. */

/* A subroutine of vfp_type_p.  Given a structure type, return the type code
   of the first non-structure element.  Recurse for structure elements.
   Return -1 if the structure is in fact empty, i.e. no nested elements.  */

static int
is_hfa0 (const ffi_type *ty)
{
  ffi_type **elements = ty->elements;
  int i, ret = -1;

  if (elements != NULL)
    for (i = 0; elements[i]; ++i)
      {
        ret = elements[i]->type;
        if (ret == FFI_TYPE_STRUCT)
          {
            ret = is_hfa0 (elements[i]);
            if (ret < 0)
              continue;
          }
        break;
      }

  return ret;
}

/* A subroutine of vfp_type_p.  Given a structure type, return true if all
   of the non-structure elements are the same as CANDIDATE.  */

static int
is_hfa1 (const ffi_type *ty, int candidate)
{
  ffi_type **elements = ty->elements;
  int i;

  if (elements != NULL)
    for (i = 0; elements[i]; ++i)
      {
        int t = elements[i]->type;
        if (t == FFI_TYPE_STRUCT)
          {
            if (!is_hfa1 (elements[i], candidate))
              return 0;
          }
        else if (t != candidate)
          return 0;
      }

  return 1;
}

/* Determine if TY is an homogenous floating point aggregate (HFA).
   That is, a structure consisting of 1 to 4 members of all the same type,
   where that type is a floating point scalar.

   Returns non-zero iff TY is an HFA.  The result is an encoded value where
   bits 0-7 contain the type code, and bits 8-10 contain the element count.  */

static int
vfp_type_p (const ffi_type *ty)
{
  ffi_type **elements;
  int candidate, i;
  size_t size, ele_count;

  /* Quickest tests first.  */
  switch (ty->type)
    {
    default:
      return 0;
    case FFI_TYPE_FLOAT:
    case FFI_TYPE_DOUBLE:
      return 0x100 + ty->type;
    case FFI_TYPE_STRUCT:
      break;
    }

  /* No HFA types are smaller than 4 bytes, or larger than 32 bytes.  */
  size = ty->size;
  if (size < 4 || size > 32)
    return 0;

  /* Find the type of the first non-structure member.  */
  elements = ty->elements;
  candidate = elements[0]->type;
  if (candidate == FFI_TYPE_STRUCT)
    {
      for (i = 0; ; ++i)
        {
          candidate = is_hfa0 (elements[i]);
          if (candidate >= 0)
            break;
        }
    }

  /* If the first member is not a floating point type, it's not an HFA.
     Also quickly re-check the size of the structure.  */
  switch (candidate)
    {
    case FFI_TYPE_FLOAT:
      ele_count = size / sizeof(float);
      if (size != ele_count * sizeof(float))
        return 0;
      break;
    case FFI_TYPE_DOUBLE:
      ele_count = size / sizeof(double);
      if (size != ele_count * sizeof(double))
        return 0;
      break;
    default:
      return 0;
    }
  if (ele_count > 4)
    return 0;

  /* Finally, make sure that all scalar elements are the same type.  */
  for (i = 0; elements[i]; ++i)
    {
      if (elements[i]->type == FFI_TYPE_STRUCT)
        {
          if (!is_hfa1 (elements[i], candidate))
            return 0;
        }
      else if (elements[i]->type != candidate)
        return 0;
    }

  /* All tests succeeded.  Encode the result.  */
  return (ele_count << 8) | candidate;
}

static int
place_vfp_arg (ffi_cif *cif, int h)
{
  unsigned short reg = cif->vfp_reg_free;
  int align = 1, nregs = h >> 8;

  if ((h & 0xff) == FFI_TYPE_DOUBLE)
    align = 2, nregs *= 2;

  /* Align register number. */
  if ((reg & 1) && align == 2)
    reg++;

  while (reg + nregs <= 16)
    {
      int s, new_used = 0;
      for (s = reg; s < reg + nregs; s++)
	{
	  new_used |= (1 << s);
	  if (cif->vfp_used & (1 << s))
	    {
	      reg += align;
	      goto next_reg;
	    }
	}
      /* Found regs to allocate. */
      cif->vfp_used |= new_used;
      cif->vfp_args[cif->vfp_nargs++] = reg;

      /* Update vfp_reg_free. */
      if (cif->vfp_used & (1 << cif->vfp_reg_free))
	{
	  reg += nregs;
	  while (cif->vfp_used & (1 << reg))
	    reg += 1;
	  cif->vfp_reg_free = reg;
	}
      return 0;
    next_reg:;
    }
  // done, mark all regs as used
  cif->vfp_reg_free = 16;
  cif->vfp_used = 0xFFFF;
  return 1;
}

static void
layout_vfp_args (ffi_cif * cif)
{
  int i;
  /* Init VFP fields */
  cif->vfp_used = 0;
  cif->vfp_nargs = 0;
  cif->vfp_reg_free = 0;
  memset (cif->vfp_args, -1, 16);	/* Init to -1. */

  for (i = 0; i < cif->nargs; i++)
    {
      int h = vfp_type_p (cif->arg_types[i]);
      if (h && place_vfp_arg (cif, h) == 1)
	break;
    }
}
