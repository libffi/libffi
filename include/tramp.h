/* -----------------------------------------------------------------------
   ffi_tramp.h - Copyright (C) 2021  Microsoft, Inc.

   Static trampoline definitions.
   ----------------------------------------------------------------------- */

#ifndef FFI_TRAMP_H
#define FFI_TRAMP_H

#ifdef __cplusplus
extern "C" {
#endif

int ffi_tramp_is_supported(void);
void *ffi_tramp_alloc (int flags);
void ffi_tramp_set_parms (void *tramp, void *data, void *code);
void *ffi_tramp_get_addr (void *tramp);
void ffi_tramp_free (void *tramp);

#ifdef __cplusplus
}
#endif

#endif /* FFI_TRAMP_H */
