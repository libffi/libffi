/* Area:	closure, ffi_prep_closure_loc
   Purpose:	Check i386 THISCALL/FASTCALL closures pop the stack correctly.
   Limitations:	i386 + GNU inline asm only; a no-op elsewhere.
   PR:		none.
   Originator:	i386 closure stack-pop accounting regression.

   THISCALL and FASTCALL are callee-clean: the closure must remove its
   stack-resident arguments on return (ret $n).  When a 64-bit integer or
   a struct argument is placed on the stack, the closure return path used
   to compute the pop as cif->bytes - narg_reg*4 with narg_reg force-bumped
   to 2, discounting register slots that were never used and under-popping
   the stack.  A caller that relies on callee cleanup is then left with the
   argument bytes where its return address should be.

   This test invokes the generated closure through a minimal callee-clean
   call site and checks that ESP is balanced across the call (delta 0).
   Without the fix the delta is 8 (FASTCALL uint64) or 4 (THISCALL).  */

/* { dg-do run } */
#include "ffitest.h"

#if defined(__i386__) && defined(__GNUC__) && !defined(__APPLE__)

static uint64_t received;
static int ran;

static void
cb (ffi_cif *cif, void *resp, void **args, void *userdata)
{
  (void) cif; (void) resp; (void) userdata;
  received = *(uint64_t *) args[cif->nargs - 1];
  ran++;
}

/* Push an 8-byte stack argument, load ECX (the thiscall "this" register,
   ignored by the fastcall callee), call the closure, and return how many
   bytes ESP moved by (0 == the callee popped exactly what was pushed).
   ESP is forced back to its saved value before returning so a wrong pop
   cannot corrupt our frame.

   Compiled at -O0 so all operands are frame-pointer relative and stay
   valid across the manual pushes; marking eax/ecx/edx clobbered forces
   'before'/'after' into callee-saved registers that survive the call.  */
#if defined(__clang__)
__attribute__((optnone))
#else
__attribute__((optimize("O0")))
#endif
static int
esp_delta (void *code, uint64_t stackarg, unsigned ecxv)
{
  unsigned before, after;
  unsigned lo = (unsigned) stackarg;
  unsigned hi = (unsigned) (stackarg >> 32);
  __asm__ volatile (
      "movl %[ecxv], %%ecx\n\t"
      "movl %%esp, %[before]\n\t"
      "pushl %[hi]\n\t"
      "pushl %[lo]\n\t"
      "calll *%[code]\n\t"
      "movl %%esp, %[after]\n\t"
      "movl %[before], %%esp\n\t"
      : [before] "=&r" (before), [after] "=&r" (after)
      : [lo] "m" (lo), [hi] "m" (hi), [code] "m" (code), [ecxv] "m" (ecxv)
      : "memory", "cc", "eax", "ecx", "edx");
  return (int) (before - after);
}

static int
check_abi (ffi_abi abi, unsigned nargs, ffi_type **atypes, unsigned ecx)
{
  ffi_cif cif;
  ffi_closure *closure;
  void *code;
  int delta;

  closure = ffi_closure_alloc (sizeof (ffi_closure), &code);
  CHECK (closure != NULL);
  CHECK (ffi_prep_cif (&cif, abi, nargs, &ffi_type_void, atypes) == FFI_OK);
  CHECK (ffi_prep_closure_loc (closure, &cif, cb, NULL, code) == FFI_OK);

  ran = 0;
  received = 0;
  delta = esp_delta (code, 0x1122334455667788ULL, ecx);

  CHECK (ran == 1);
  CHECK (received == 0x1122334455667788ULL);
  ffi_closure_free (closure);
  return delta;
}

int
main (void)
{
  ffi_type *fastcall_args[1] = { &ffi_type_uint64 };
  ffi_type *thiscall_args[2] = { &ffi_type_pointer, &ffi_type_uint64 };
  int d;

  /* FASTCALL void cb(uint64_t): the uint64 is stack-resident; pop must be 8. */
  d = check_abi (FFI_FASTCALL, 1, fastcall_args, 0);
  printf ("FASTCALL uint64 esp delta: %d\n", d);
  CHECK (d == 0);

  /* THISCALL void cb(void*, uint64_t): 'this' in ECX, uint64 on the stack;
     pop must be 8 (not 4).  */
  d = check_abi (FFI_THISCALL, 2, thiscall_args, 0xdeadbeef);
  printf ("THISCALL this+uint64 esp delta: %d\n", d);
  CHECK (d == 0);

  exit (0);
}

#else

int
main (void)
{
  /* Not an i386 GNU target: nothing to check here. */
  exit (0);
}

#endif
