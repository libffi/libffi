/* -----------------------------------------------------------------------
   ffi.c - Copyright (c) 2018  Brion Vibber

   wasm32/emscripten Foreign Function Interface

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
#include <stdint.h>
#include <stdlib.h>

#include <emscripten/emscripten.h>

ffi_status FFI_HIDDEN
ffi_prep_cif_machdep(ffi_cif *cif)
{
  return FFI_OK;
}

EM_JS(void, ffi_call, (ffi_cif *cif, ffi_fp fn, void *rvalue, void **avalue), {
  var cif_abi = HEAPU32[cif >> 2];
  var cif_nargs = HEAPU32[(cif + 4) >> 2];
  var cif_arg_types = HEAPU32[(cif + 8) >> 2];
  var cif_rtype = HEAPU32[(cif + 12) >> 2];

  var args = [];
  var rtype = HEAPU16[(cif_rtype + 6 /* rtype->type*/ ) >> 1];

#if WASM_BIGINT
  if (rtype === /* FFI_TYPE_STRUCT */ 13) {
    throw new Error('struct ret marshalling nyi');
  } else if (rtype === /* FFI_TYPE_COMPLEX */ 15) {
    throw new Error('complex ret marshalling nyi');
  } else if (rtype < 0 || rtype > 14) {
    throw new Error('Unexpected rtype ' + rtype);
  }
#else
  var sig;
  if (rtype === /* FFI_TYPE_VOID */ 0) {
    sig = 'v';
  } else if (rtype === /* FFI_TYPE_INT */ 1 ||
             rtype === /* FFI_TYPE_UINT8 */ 5 ||
             rtype === /* FFI_TYPE_SINT8 */ 6 ||
             rtype === /* FFI_TYPE_UINT16 */ 7 ||
             rtype === /* FFI_TYPE_SINT16 */ 8 ||
             rtype === /* FFI_TYPE_UINT32 */ 9 ||
             rtype === /* FFI_TYPE_SINT32 */ 10 ||
             rtype === /* FFI_TYPE_POINTER */ 14) {
    sig = 'i';
  } else if (rtype === /* FFI_TYPE_FLOAT */ 2) {
    sig = 'f';
  } else if (rtype === /* FFI_TYPE_DOUBLE */ 3 ||
             rtype === /* FFI_TYPE_LONGDOUBLE */ 4) {
    sig = 'd';
  } else if (rtype === /* FFI_TYPE_UINT64 */ 11 ||
             rtype === /* FFI_TYPE_SINT64 */ 12) {
    // Warning: returns a truncated 32-bit integer directly.
    // High bits are in $tempRet0
    sig = 'j';
  } else if (rtype === /* FFI_TYPE_STRUCT */ 13) {
    throw new Error('struct ret marshalling nyi');
  } else if (rtype === /* FFI_TYPE_COMPLEX */ 15) {
    throw new Error('complex ret marshalling nyi');
  } else {
    throw new Error('Unexpected rtype ' + rtype);
  }
#endif

  for (var i = 0; i < cif_nargs; i++) {
    var ptr = HEAPU32[(avalue >> 2) + i];

    var arg_type = HEAPU32[(cif_arg_types >> 2) + i];
    var typ = HEAPU16[(arg_type + 6) >> 1];

    if (typ === /* FFI_TYPE_INT*/ 1 || typ === /* FFI_TYPE_SINT32 */ 10) {
      args.push(HEAP32[ptr >> 2]);
#if !WASM_BIGINT
      sig += 'i';
#endif
    } else if (typ === /* FFI_TYPE_FLOAT */ 2) {
      args.push(HEAPF32[ptr >> 2]);
#if !WASM_BIGINT
      sig += 'f';
#endif
    } else if (typ === /* FFI_TYPE_DOUBLE */ 3 || typ === /* FFI_TYPE_LONGDOUBLE */ 4) {
      args.push(HEAPF64[ptr >> 3]);
#if !WASM_BIGINT
      sig += 'd'; 
#endif
    } else if (typ === /* FFI_TYPE_UINT8*/ 5) {
      args.push(HEAPU8[ptr]);
#if !WASM_BIGINT
      sig += 'i';
#endif
    } else if (typ === /* FFI_TYPE_SINT8 */ 6) {
      args.push(HEAP8[ptr]);
#if !WASM_BIGINT
      sig += 'i';
#endif
    } else if (typ === /* FFI_TYPE_UINT16 */ 7) {
      args.push(HEAPU16[ptr >> 1]);
#if !WASM_BIGINT
      sig += 'i';
#endif
    } else if (typ === /* FFI_TYPE_SINT16 */ 8) {
      args.push(HEAP16[ptr >> 1]);
#if !WASM_BIGINT
      sig += 'i';
#endif
    } else if (typ === /* FFI_TYPE_UINT32 */ 9 || typ === /* FFI_TYPE_POINTER */ 14) {
      args.push(HEAPU32[ptr >> 2]);
#if !WASM_BIGINT
      sig += 'i';
#endif
    } else if (typ === /* FFI_TYPE_UINT64 */ 11 || typ === /* FFI_TYPE_SINT64 */ 12) {
#if WASM_BIGINT
      args.push(BigInt(HEAPU32[ptr >> 2]) | (BigInt(HEAPU32[(ptr + 4) >> 2]) << BigInt(32)));
#else
      // LEGALIZE_JS_FFI mode splits i64 (j) into two i32 args
      // for compatibility with JavaScript's f64-based numbers.
      args.push(HEAPU32[ptr >> 2]);
      args.push(HEAPU32[(ptr + 4) >> 2]);
      sig += 'j';
#endif
    } else if (typ === /* FFI_TYPE_STRUCT */ 13) {
      throw new Error('struct marshalling nyi');
    } else if (typ === /* FFI_TYPE_COMPLEX */ 15) {
      throw new Error('complex marshalling nyi');
    } else {
      throw new Error('Unexpected type ' + typ);
    }
  }

#if WASM_BIGINT
  var result = wasmTable.get(fn).apply(null, args);
#else
  var result = dynCall(sig, fn, args);
#endif

  if (rtype === 0) {
    // void
  } else if (rtype === 1 || rtype === 9 || rtype === 10 || rtype === 14) {
    HEAP32[rvalue >> 2] = result;
  } else if (rtype === 2) {
    HEAPF32[rvalue >> 2] = result;
  } else if (rtype === 3 || rtype === 4) {
    HEAPF64[rvalue >> 3] = result;
  } else if (rtype === 5 || rtype === 6) {
    HEAP8[rvalue] = result;
  } else if (rtype === 7 || rtype === 8) {
    HEAP16[rvalue >> 1] = result;
  } else if (rtype === 11 || rtype === 12) {
#if WASM_BIGINT
    HEAP32[rvalue >> 2] = Number(result & BigInt(0xffffffff)) | 0;
    HEAP32[(rvalue + 4) >> 2] = Number(result >> BigInt(32)) | 0;
#else
    // Warning: returns a truncated 32-bit integer directly.
    // High bits are in $tempRet0
    HEAP32[rvalue >> 2] = result;
    HEAP32[(rvalue + 4) >> 2] = Module.getTempRet0();
#endif
  } else if (rtype === 13) {
    throw new Error('struct ret marshalling nyi');
  } else if (rtype === 15) {
    throw new Error('complex ret marshalling nyi');
  } else {
    throw new Error('Unexpected rtype ' + rtype);
  }
});
