/* Content-addressed cache of precompiled call plans.

   ffi_call and the closure paths call ffi_plan_get(cif) to obtain a plan that
   captures a signature's argument placement, so they can skip re-classifying it
   on every call.  Each signature is identified by a 64-bit fingerprint folded
   from its abi, nargs, return type and the *contents* of arg_types (the element
   ffi_type pointers) -- so a freed arg_types array whose address is later reused
   for a different signature does not alias a stale plan.  A per-thread
   direct-mapped cache holds one plan per slot; a miss builds the plan once and
   frees the slot's previous plan, and a thread's plans are freed when it exits.
   Plans are opaque (each backend casts the void* to its own type) and are
   self-contained single allocations, released with plain free().

   The cache is internal: no public symbols and no ABI change.  Exactly one
   translation unit per architecture must also #include "plan-cache-impl.h" to
   instantiate the storage and provide ffi_build_plan_arch. */

#ifndef FFI_PLAN_CACHE_H
#define FFI_PLAN_CACHE_H

#include <ffi.h>
#include <stdint.h>

#if defined(_MSC_VER)
# define FFI_TLS __declspec(thread)
#else
# define FFI_TLS __thread
#endif

#define FFI_PLAN_CACHE_BITS 10			/* 1024 slots / thread */
#define FFI_PLAN_CACHE_SIZE (1u << FFI_PLAN_CACHE_BITS)
#define FFI_PLAN_NONE ((void *) 1)		/* sentinel: not plan-able */

struct ffi_plan_slot
{
  uint64_t   fp;	/* content fingerprint (0 = empty slot)             */
  unsigned   abi;
  unsigned   nargs;
  ffi_type  *rtype;
  void      *plan;	/* NULL=empty, FFI_PLAN_NONE=no fast path, else plan */
};

extern FFI_TLS struct ffi_plan_slot ffi_plan_cache[FFI_PLAN_CACHE_SIZE];

/* Cold miss handler + per-arch builder, defined in the impl/arch TUs. */
void *ffi_plan_miss (ffi_cif *cif, struct ffi_plan_slot *s, uint64_t fp);
void *ffi_build_plan_arch (ffi_cif *cif);

/* 64-bit FNV-1a-style fingerprint over the signature, including the element
   type pointers -- so two signatures that differ only in argument types (even
   if they happen to reuse the same arg_types array address) get distinct keys. */
static inline uint64_t
ffi_plan_fp (ffi_cif *cif)
{
  uint64_t h = 0xCBF29CE484222325ULL;
  unsigned i, n = cif->nargs;
  h = (h ^ (uint64_t) (uintptr_t) cif->rtype) * 0x100000001B3ULL;
  h = (h ^ cif->abi)                          * 0x100000001B3ULL;
  h = (h ^ n)                                 * 0x100000001B3ULL;
  for (i = 0; i < n; i++)
    h = (h ^ (uint64_t) (uintptr_t) cif->arg_types[i]) * 0x100000001B3ULL;
  return h | 1;				/* never 0 (0 marks an empty slot) */
}

/* Hot path: the plan for CIF (built and cached on first use), or NULL if the
   signature isn't plan-able.  Computing the fingerprint is O(nargs) of cheap
   pointer reads -- far less than re-classifying the call. */
static inline void *
ffi_plan_get (ffi_cif *cif)
{
  uint64_t fp = ffi_plan_fp (cif);
  struct ffi_plan_slot *s =
    &ffi_plan_cache[(unsigned) fp & (FFI_PLAN_CACHE_SIZE - 1)];
  if (s->fp == fp && s->abi == cif->abi && s->nargs == cif->nargs
      && s->rtype == cif->rtype)
    return s->plan == FFI_PLAN_NONE ? NULL : s->plan;
  return ffi_plan_miss (cif, s, fp);
}

#endif /* FFI_PLAN_CACHE_H */
