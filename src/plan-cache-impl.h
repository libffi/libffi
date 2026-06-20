/* Mechanism A: plan-cache storage.  #include in EXACTLY ONE translation unit
   per architecture (the one that also defines ffi_build_plan_arch).  See
   plan-cache.h. */

#ifndef FFI_PLAN_CACHE_IMPL_H
#define FFI_PLAN_CACHE_IMPL_H

#include <stdlib.h>
#include "plan-cache.h"

__thread struct ffi_plan_slot ffi_plan_cache[FFI_PLAN_CACHE_SIZE];

/* Direct-mapped miss: free the evicted plan, build this signature's plan once,
   record the key.  A non-plan-able signature caches FFI_PLAN_NONE so it isn't
   rebuilt every call.  Plans are self-contained -> plain free(). */
void *
ffi_plan_miss (ffi_cif *cif, struct ffi_plan_slot *s)
{
  void *p;

  if (s->plan != NULL && s->plan != FFI_PLAN_NONE)
    free (s->plan);

  p = ffi_build_plan_arch (cif);
  s->abi    = cif->abi;
  s->nargs  = cif->nargs;
  s->rtype  = cif->rtype;
  s->atypes = cif->arg_types;
  s->plan   = (p == NULL) ? FFI_PLAN_NONE : p;
  return p;
}

#endif /* FFI_PLAN_CACHE_IMPL_H */
