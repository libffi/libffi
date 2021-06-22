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

#define EM_JS_MACROS(ret, name, args, body...) EM_JS(ret, name, args, body...)

#define DEREF_U16(addr, offset) HEAPU16[(addr >> 1) + offset]

#define DEREF_U32(addr, offset) HEAPU32[(addr >> 2) + offset]
#define DEREF_I32(addr, offset) HEAP32[(addr >> 2) + offset]
#define DEREF_F32(addr, offset) HEAPF32[(addr >> 2) + offset]

#define DEREF_F64(addr, offset) HEAPF64[(addr >> 3) + offset]

#define FFI_TYPE__SIZE(addr)   DEREF_U32(addr, 0)
#define FFI_TYPE__ALIGN(addr)  DEREF_U16(addr + 4, 0)
#define FFI_TYPE__TYPE(addr)  DEREF_U16(addr + 6, 0)
#define FFI_TYPE__ELEMENTS(addr)  DEREF_U32(addr + 8, 0)

#define PADDING(size, align) align - ((size + align - 1) % align) - 1

#if WASM_BIGINT
#define SIG(sig)
#else
#define SIG(sig) sig
#endif

EM_JS_MACROS(void, ffi_call, (ffi_cif *cif, ffi_fp fn, void *rvalue, void **avalue), {
  function ffi_struct_size_and_alignment(arg_type){
    let stored_size = FFI_TYPE_SIZE(arg_type);
    if(stored_size){
      let stored_align = FFI_TYPE_ALIGN(arg_type);
      return [stored_size, stored_align];
    }
    let elements = FFI_TYPE__ELEMENTS(arg_type);
    let idx = 0;
    let size = 0;
    let align = 1;
    for(let idx = 0; DEREF_U32(elements, idx) !== 0; idx++){
      let item_size;
      let item_align = 0;
      let element = DEREF_U32(elements, idx);
      switch(FFI_TYPE__TYPE(element)) {
        case FFI_TYPE_STRUCT:
          let [item_size, item_align] = ffi_struct_size_and_alignment(element);
          break;
        case FFI_TYPE_UINT8:
        case FFI_TYPE_SINT8:
          item_size = 1;
          break;
        case FFI_TYPE_UINT16:
        case FFI_TYPE_SINT16:
          item_size = 2;
          break;
        case FFI_TYPE_INT:
        case FFI_TYPE_UINT32:
        case FFI_TYPE_SINT32:
        case FFI_TYPE_POINTER:
        case FFI_TYPE_FLOAT:
          item_size = 4;
          break;
        case FFI_TYPE_DOUBLE:
        case FFI_TYPE_UINT64:
        case FFI_TYPE_SINT64:
          item_size = 8;
          break;
        case FFI_TYPE_LONGDOUBLE:
          item_size = 16;
        case FFI_TYPE_COMPLEX:
          throw new Error('complex ret marshalling nyi');
        default:
          throw new Error('Unexpected rtype ' + rtype);
      }
      item_align ||= item_size;
      FFI_TYPE_SIZE(arg_type) = item_size;
      FFI_TYPE_ALIGN(arg_type) = item_align;
      size += item_size + PADDING(size, item_align);
      align = item_align > align ? item_align : align;
    }
    return [size, align];
  }

  const cif_abi = DEREF_U32(cif, 0);
  const cif_nargs = DEREF_U32(cif, 1);
  const cif_arg_types = DEREF_U32(cif, 2);
  const cif_rtype = DEREF_U32(cif, 3);

  const args = [];
  const rtype = FFI_TYPE__TYPE(cif_rtype);

#if WASM_BIGINT
  switch(rtype){
    case FFI_TYPE_COMPLEX:
      throw new Error('complex ret marshalling nyi');
  }
  if (rtype < 0 || rtype > 14) {
    throw new Error('Unexpected rtype ' + rtype);
  }
#else
  let sig;
  switch(rtype) {
    case FFI_TYPE_VOID:
      sig = 'v';
      break;
    case FFI_TYPE_INT:
    case FFI_TYPE_UINT8:
    case FFI_TYPE_SINT8:
    case FFI_TYPE_UINT16:
    case FFI_TYPE_SINT16:
    case FFI_TYPE_UINT32:
    case FFI_TYPE_SINT32:
    case FFI_TYPE_POINTER:
    case FFI_TYPE_STRUCT:
      sig = 'i';
      break;
    case FFI_TYPE_FLOAT:
      sig = 'f';
      break;
    case FFI_TYPE_DOUBLE:
    case FFI_TYPE_LONGDOUBLE:
      sig = 'd';
      break;
    case FFI_TYPE_UINT64:
    case FFI_TYPE_SINT64:
      sig = 'j';
      break;
    case FFI_TYPE_COMPLEX:
      throw new Error('complex ret marshalling nyi');
    default:
      throw new Error('Unexpected rtype ' + rtype);
  }
#endif

  for (let i = 0; i < cif_nargs; i++) {
    let arg_ptr = DEREF_U32(avalue, i);

    let arg_type = DEREF_U32(cif_arg_types, i);
    let typ = FFI_TYPE__TYPE(arg_type);

    let structs_size = 0;
    let struct_args = [];

    switch(typ){
      case FFI_TYPE_INT:
      case FFI_TYPE_SINT32:
        args.push(DEREF_I32(arg_ptr, 0));
        SIG(sig += 'i');
        break;
      case FFI_TYPE_FLOAT:
        args.push(DEREF_F32(arg_ptr, 0));
        SIG(sig += 'f');
        break;
      case FFI_TYPE_DOUBLE:
        args.push(DEREF_F64(arg_ptr, 0));
        SIG(sig += 'd'); 
        break;
      case FFI_TYPE_LONGDOUBLE:
        let HEAPU64 = new BigInt64Array(HEAP8.buffer);
        args.push(HEAPU64[arg_ptr >> 3]);
        args.push(HEAPU64[(arg_ptr >> 3) + 1]);
        break;
      case FFI_TYPE_UINT8:
        args.push(HEAPU8[arg_ptr]);
        SIG(sig += 'i');
        break;
      case FFI_TYPE_SINT8:
        args.push(HEAP8[arg_ptr]);
        SIG(sig += 'i');
        break;
      case FFI_TYPE_UINT16:
        args.push(DEREF_U16(arg_ptr, 0));
        SIG(sig += 'i');
        break;
      case FFI_TYPE_SINT16:
        args.push(DEREF_U16(arg_ptr, 0));
        SIG(sig += 'i');
        break;
      case FFI_TYPE_UINT32:
      case FFI_TYPE_POINTER:
        args.push(DEREF_U32(arg_ptr, 0));
        SIG(sig += 'i');
        break;
      case FFI_TYPE_UINT64:
      case FFI_TYPE_SINT64:
#if WASM_BIGINT
        args.push(BigInt(DEREF_U32(arg_ptr, 0)) | (BigInt(DEREF_U32(arg_ptr, 1)) << 32n));
#else
        // LEGALIZE_JS_FFI mode splits i64 (j) into two i32 args
        // for compatibility with JavaScript's f64-based numbers.
        args.push(DEREF_U32(arg_ptr, 0));
        args.push(DEREF_U32(arg_ptr, 1));
        sig += 'j';
#endif
        break;
      case FFI_TYPE_STRUCT:
        let [item_size, item_align] = Module.ffi_struct_size_and_alignment(arg_type);
        structs_size += PADDING(structs_size, item_align);
        struct_args.push(args.length);
        // put the offset into the struct memory now, we'll have to update this
        // when we allocate the memory
        args.push(structs_size); 
        structs_size += item_size;
        break;
      case FFI_TYPE_COMPLEX:
        throw new Error('complex marshalling nyi');
      default:
        throw new Error('Unexpected type ' + typ);
    }
  }

  if(structs_size > 0){
    let structs_addr = _malloc(structs_size);
    for(let idx of struct_args){
      // Update the offsets to actual pointers
      args[idx] += structs_addr;
      let arg_type = DEREF_U32(cif_arg_types, idx);
      let src_ptr = DEREF_U32(avalue, i);
      let dest_ptr = args[idx];
      let size = FFI_TYPE_SIZE(arg_type);
      HEAP8.subarray(dest_ptr, dest_ptr + size).set(HEAP8.slice(src_ptr, src_ptr + size));
    }
  }

#if WASM_BIGINT
  var result = wasmTable.get(fn).apply(null, args);
#else
  var result = dynCall(sig, fn, args);
#endif

  switch(rtype){
    case FFI_TYPE_VOID:
      break;
    case FFI_TYPE_INT:
    case FFI_TYPE_UINT32: 
    case FFI_TYPE_SINT32: 
    case FFI_TYPE_POINTER:
      DEREF_I32(rvalue, 0) = result;
      break;
    case FFI_TYPE_FLOAT:
      DEREF_F32(rvalue, 0) = result;
      break;
    case FFI_TYPE_DOUBLE: 
    case FFI_TYPE_LONGDOUBLE:
      DEREF_F64(rvalue, 0) = result;
      break;
    case FFI_TYPE_UINT8:
    case FFI_TYPE_SINT8:
      HEAP8[rvalue] = result;
      break;
    case FFI_TYPE_UINT16: 
    case FFI_TYPE_SINT16:
      DEREF_I16(rvalue, 0) = result;
      break;
    case FFI_TYPE_UINT64: 
    case FFI_TYPE_SINT64:
#if WASM_BIGINT
      DEREF_I32(rvalue, 0) = Number(result & 0xffffffffn) | 0;
      DEREF_I32(rvalue, 1) = Number(result >> 32n) | 0;
#else
      // Warning: returns a truncated 32-bit integer directly.
      // High bits are in $tempRet0
      DEREF_I32(rvalue, 0) = result;
      DEREF_I32(rvalue, 1) = Module.getTempRet0();
#endif 
      break;
    case  FFI_TYPE_STRUCT:
      throw new Error('struct ret marshalling nyi');
    case FFI_TYPE_COMPLEX:
      throw new Error('complex ret marshalling nyi');
    default:
      throw new Error('Unexpected rtype ' + rtype);
  }
});


// void *ffi_closure_alloc (size_t size, void **code){

// }

// void ffi_closure_free(void *writable){

// }

// EM_JS_MACROS(void *, ffi_prep_closure_create_trampoline, (void (*fun)(ffi_cif*,void*,void**,void*)), {

// })


// ffi_status
// ffi_prep_closure_loc (ffi_closure* closure,
//         ffi_cif* cif,
//         void (*fun)(ffi_cif*,void*,void**,void*),
//         void *user_data,
//         void *codeloc)
// {
//   void (*closure_func)(ffi_closure*) = NULL;


//   closure->cif  = cif;
//   closure->user_data = user_data;
//   closure->fun  = fun;

//   return FFI_OK;
// }
