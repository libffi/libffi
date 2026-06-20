/* Storage for the plan cache (see plan-cache.h).  #include this in EXACTLY ONE
   translation unit per architecture -- the one that also defines the backend's
   ffi_build_plan_arch. */

#ifndef FFI_PLAN_CACHE_IMPL_H
#define FFI_PLAN_CACHE_IMPL_H

#include <stdlib.h>
#include <pthread.h>
#include "plan-cache.h"

FFI_TLS struct ffi_plan_slot ffi_plan_cache[FFI_PLAN_CACHE_SIZE];

/* Free a thread's cached plans when it exits.  pthread is referenced through
   weak symbols so this adds no hard dependency: in a single-threaded program
   that never links pthread the registration is simply skipped (the cache is
   already bounded to FFI_PLAN_CACHE_SIZE plans per thread). */
#pragma weak pthread_key_create
#pragma weak pthread_once
#pragma weak pthread_setspecific

static pthread_key_t ffi_plan_key;
static pthread_once_t ffi_plan_once = PTHREAD_ONCE_INIT;

static void
ffi_plan_thread_cleanup (void *ignored)
{
  unsigned i;
  (void) ignored;
  for (i = 0; i < FFI_PLAN_CACHE_SIZE; i++)
    if (ffi_plan_cache[i].plan != NULL && ffi_plan_cache[i].plan != FFI_PLAN_NONE)
      {
	free (ffi_plan_cache[i].plan);
	ffi_plan_cache[i].plan = NULL;
      }
}

static void
ffi_plan_key_init (void)
{
  pthread_key_create (&ffi_plan_key, ffi_plan_thread_cleanup);
}

static void
ffi_plan_thread_register (void)
{
  if (pthread_once && pthread_setspecific && pthread_key_create)
    {
      pthread_once (&ffi_plan_once, ffi_plan_key_init);
      /* A non-NULL value makes the destructor run for this thread at exit. */
      pthread_setspecific (ffi_plan_key, (void *) 1);
    }
}

/* Direct-mapped miss: free the evicted plan, build this signature's plan once,
   record the key.  A non-plan-able signature caches FFI_PLAN_NONE so it isn't
   rebuilt every call.  Plans are self-contained -> plain free(). */
void *
ffi_plan_miss (ffi_cif *cif, struct ffi_plan_slot *s, uint64_t fp)
{
  void *p;

  if (s->plan != NULL && s->plan != FFI_PLAN_NONE)
    free (s->plan);
  else if (s->plan == NULL)
    ffi_plan_thread_register ();	/* first use of an empty slot */

  p = ffi_build_plan_arch (cif);
  s->fp    = fp;
  s->abi   = cif->abi;
  s->nargs = cif->nargs;
  s->rtype = cif->rtype;
  s->plan  = (p == NULL) ? FFI_PLAN_NONE : p;
  return p;
}

#endif /* FFI_PLAN_CACHE_IMPL_H */
