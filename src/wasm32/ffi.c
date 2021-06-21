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

#define EM_JS_WRAP(ret, name, args, body) EM_JS(ret, name, args, body)

EM_JS_WRAP(void, ffi_call, (ffi_cif *cif, ffi_fp fn, void *rvalue, void **avalue), {
  var cif_abi = HEAPU32[cif >> 2];
  var cif_nargs = HEAPU32[(cif + 4) >> 2];
  var cif_arg_types = HEAPU32[(cif + 8) >> 2];
  var cif_rtype = HEAPU32[(cif + 12) >> 2];

  var args = [];
  var rtype = HEAPU16[(cif_rtype + 6 /* rtype->type*/ ) >> 1];

#if WASM_BIGINT
  if (rtype === FFI_TYPE_STRUCT) {
    throw new Error('struct ret marshalling nyi');
  } else if (rtype === FFI_TYPE_COMPLEX) {
    throw new Error('complex ret marshalling nyi');
  } else if (rtype < 0 || rtype > 14) {
    throw new Error('Unexpected rtype ' + rtype);
  }
#else
  var sig;
  if (rtype === FFI_TYPE_VOID) {
    sig = 'v';
  } else if (rtype === FFI_TYPE_INT ||
             rtype === FFI_TYPE_UINT8 ||
             rtype === FFI_TYPE_SINT8 ||
             rtype === FFI_TYPE_UINT16 ||
             rtype === FFI_TYPE_SINT16 ||
             rtype === FFI_TYPE_UINT32 ||
             rtype === FFI_TYPE_SINT32 ||
             rtype === FFI_TYPE_POINTER) {
    sig = 'i';
  } else if (rtype === FFI_TYPE_FLOAT) {
    sig = 'f';
  } else if (rtype === FFI_TYPE_DOUBLE ||
             rtype === FFI_TYPE_LONGDOUBLE) {
    sig = 'd';
  } else if (rtype === FFI_TYPE_UINT64 ||
             rtype === FFI_TYPE_SINT64) {
    // Warning: returns a truncated 32-bit integer directly.
    // High bits are in $tempRet0
    sig = 'j';
  } else if (rtype === FFI_TYPE_STRUCT) {
    throw new Error('struct ret marshalling nyi');
  } else if (rtype === FFI_TYPE_COMPLEX) {
    throw new Error('complex ret marshalling nyi');
  } else {
    throw new Error('Unexpected rtype ' + rtype);
  }
#endif

  for (var i = 0; i < cif_nargs; i++) {
    var ptr = HEAPU32[(avalue >> 2) + i];

    var arg_type = HEAPU32[(cif_arg_types >> 2) + i];
    var typ = HEAPU16[(arg_type + 6) >> 1];

    if (typ === /* FFI_TYPE_INT*/ 1 || typ === FFI_TYPE_SINT32) {
      args.push(HEAP32[ptr >> 2]);
#if !WASM_BIGINT
      sig += 'i';
#endif
    } else if (typ === FFI_TYPE_FLOAT) {
      args.push(HEAPF32[ptr >> 2]);
#if !WASM_BIGINT
      sig += 'f';
#endif
    } else if (typ === FFI_TYPE_DOUBLE) {
      args.push(HEAPF64[ptr >> 3]);
#if !WASM_BIGINT
      sig += 'd'; 
#endif
    } else if (typ === FFI_TYPE_LONGDOUBLE){
      let HEAPU64 = new BigInt64Array(HEAP8.buffer);
      args.push(HEAPU64[ptr >> 3]);
      args.push(HEAPU64[(ptr >> 3) + 1]);
    } else if (typ === FFI_TYPE_UINT8) {
      args.push(HEAPU8[ptr]);
#if !WASM_BIGINT
      sig += 'i';
#endif
    } else if (typ === FFI_TYPE_SINT8) {
      args.push(HEAP8[ptr]);
#if !WASM_BIGINT
      sig += 'i';
#endif
    } else if (typ === FFI_TYPE_UINT16) {
      args.push(HEAPU16[ptr >> 1]);
#if !WASM_BIGINT
      sig += 'i';
#endif
    } else if (typ === FFI_TYPE_SINT16) {
      args.push(HEAP16[ptr >> 1]);
#if !WASM_BIGINT
      sig += 'i';
#endif
    } else if (typ === FFI_TYPE_UINT32 || typ === FFI_TYPE_POINTER) {
      args.push(HEAPU32[ptr >> 2]);
#if !WASM_BIGINT
      sig += 'i';
#endif
    } else if (typ === FFI_TYPE_UINT64 || typ === FFI_TYPE_SINT64) {
#if WASM_BIGINT
      args.push(BigInt(HEAPU32[ptr >> 2]) | (BigInt(HEAPU32[(ptr + 4) >> 2]) << BigInt(32)));
#else
      // LEGALIZE_JS_FFI mode splits i64 (j) into two i32 args
      // for compatibility with JavaScript's f64-based numbers.
      args.push(HEAPU32[ptr >> 2]);
      args.push(HEAPU32[(ptr + 4) >> 2]);
      sig += 'j';
#endif
    } else if (typ === FFI_TYPE_STRUCT) {
      throw new Error('struct marshalling nyi');
    } else if (typ === FFI_TYPE_COMPLEX) {
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

  if (rtype === FFI_TYPE_VOID) {
    // void
  } else if (rtype === FFI_TYPE_INT || rtype === FFI_TYPE_UINT32 || rtype === FFI_TYPE_SINT32 || rtype === FFI_TYPE_POINTER) {
    HEAP32[rvalue >> 2] = result;
  } else if (rtype === FFI_TYPE_FLOAT) {
    HEAPF32[rvalue >> 2] = result;
  } else if (rtype === FFI_TYPE_DOUBLE || rtype === FFI_TYPE_LONGDOUBLE) {
    HEAPF64[rvalue >> 3] = result;
  } else if (rtype === FFI_TYPE_UINT8 || rtype === FFI_TYPE_SINT8) {
    HEAP8[rvalue] = result;
  } else if (rtype === FFI_TYPE_UINT16 || rtype === FFI_TYPE_SINT16) {
    HEAP16[rvalue >> 1] = result;
  } else if (rtype === FFI_TYPE_UINT64 || rtype === FFI_TYPE_SINT64) {
#if WASM_BIGINT
    HEAP32[rvalue >> 2] = Number(result & BigInt(0xffffffff)) | 0;
    HEAP32[(rvalue + 4) >> 2] = Number(result >> BigInt(32)) | 0;
#else
    // Warning: returns a truncated 32-bit integer directly.
    // High bits are in $tempRet0
    HEAP32[rvalue >> 2] = result;
    HEAP32[(rvalue + 4) >> 2] = Module.getTempRet0();
#endif
  } else if (rtype === FFI_TYPE_STRUCT) {
    throw new Error('struct ret marshalling nyi');
  } else if (rtype === FFI_TYPE_COMPLEX) {
    throw new Error('complex ret marshalling nyi');
  } else {
    throw new Error('Unexpected rtype ' + rtype);
  }
});
