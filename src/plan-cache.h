/* Mechanism A: transparent content-addressed plan cache.

   No opt-in, no API: ffi_call/closures call ffi_plan_get(cif) directly.  The
   key is (abi, nargs, rtype, arg_types) -- O(1), since arg_types is a single
   pointer and libffi already requires it stable from prep to call.  A per-thread
   direct-mapped cache holds one plan per slot; a miss builds the plan once and
   evicts the slot's previous plan.  Plans are opaque void* blobs (each arch
   casts to its own type) and self-contained single allocations (plain free()).

   Exactly one TU per architecture must also #include "plan-cache-impl.h". */

#ifndef FFI_PLAN_CACHE_H
#define FFI_PLAN_CACHE_H

#include <ffi.h>
#include <stdint.h>

#define FFI_PLAN_CACHE_BITS 10			/* 1024 slots / thread */
#define FFI_PLAN_CACHE_SIZE (1u << FFI_PLAN_CACHE_BITS)
#define FFI_PLAN_NONE ((void *) 1)		/* sentinel: not plan-able */

struct ffi_plan_slot
{
  unsigned   abi;
  unsigned   nargs;
  ffi_type  *rtype;
  ffi_type **atypes;
  void      *plan;	/* NULL=empty, FFI_PLAN_NONE=no fast path, else plan */
};

extern __thread struct ffi_plan_slot ffi_plan_cache[FFI_PLAN_CACHE_SIZE];

/* Cold miss handler + per-arch builder, defined in the impl/arch TUs. */
void *ffi_plan_miss (ffi_cif *cif, struct ffi_plan_slot *s);
void *ffi_build_plan_arch (ffi_cif *cif);

static inline unsigned
ffi_plan_hash (ffi_cif *cif)
{
  uintptr_t h = (uintptr_t) cif->arg_types;
  h ^= (uintptr_t) cif->rtype + 0x9E3779B97F4A7C15ULL;
  h ^= ((uintptr_t) cif->nargs << 3) ^ ((uintptr_t) cif->abi << 1);
  h *= 0xFF51AFD7ED558CCDULL;
  return (unsigned) (h >> 47);
}

/* Hot path: the plan for CIF (built and cached on first use), or NULL if the
   signature isn't plan-able.  Key compare is 4 words; no per-arg walk. */
static inline void *
ffi_plan_get (ffi_cif *cif)
{
  struct ffi_plan_slot *s =
    &ffi_plan_cache[ffi_plan_hash (cif) & (FFI_PLAN_CACHE_SIZE - 1)];
  if (s->atypes == cif->arg_types && s->rtype == cif->rtype
      && s->nargs == cif->nargs && s->abi == cif->abi)
    return s->plan == FFI_PLAN_NONE ? NULL : s->plan;
  return ffi_plan_miss (cif, s);
}

#endif /* FFI_PLAN_CACHE_H */
