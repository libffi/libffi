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

ffi_status FFI_HIDDEN ffi_prep_cif_machdep(ffi_cif *cif) { return FFI_OK; }

#define EM_JS_MACROS(ret, name, args, body...) EM_JS(ret, name, args, body)

#define DEREF_U16(addr, offset) HEAPU16[(addr >> 1) + offset]
#define DEREF_I16(addr, offset) HEAPU16[(addr >> 1) + offset]

#define DEREF_U32(addr, offset) HEAPU32[(addr >> 2) + offset]
#define DEREF_I32(addr, offset) HEAP32[(addr >> 2) + offset]
#define DEREF_F32(addr, offset) HEAPF32[(addr >> 2) + offset]

#define DEREF_U64(addr, offset) HEAPU64[(addr >> 3) + offset]
#define DEREF_F64(addr, offset) HEAPF64[(addr >> 3) + offset]

#define CIF__ABI(addr) DEREF_U32(addr, 0)
#define CIF__NARGS(addr) DEREF_U32(addr, 1)
#define CIF__ARGTYPES(addr) DEREF_U32(addr, 2)
#define CIF__RTYPE(addr) DEREF_U32(addr, 3)

#define FFI_TYPE__SIZE(addr) DEREF_U32(addr, 0)
#define FFI_TYPE__ALIGN(addr) DEREF_U16(addr + 4, 0)
#define FFI_TYPE__TYPEID(addr) DEREF_U16(addr + 6, 0)
#define FFI_TYPE__ELEMENTS(addr) DEREF_U32(addr + 8, 0)

#define PADDING(size, align) align - ((size + align - 1) % align) - 1

#if WASM_BIGINT
#define SIG(sig)
#else
#define SIG(sig) sig
#endif

EM_JS_MACROS(
void,
ffi_call, (ffi_cif * cif, ffi_fp fn, void *rvalue, void **avalue),
{
  "use strict";
  function ffi_struct_size_and_alignment(arg_type) {
    const stored_size = FFI_TYPE__SIZE(arg_type);
    if (stored_size) {
      const stored_align = FFI_TYPE__ALIGN(arg_type);
      return [ stored_size, stored_align ];
    }
    const elements = FFI_TYPE__ELEMENTS(arg_type);
    let size = 0;
    let align = 1;
    for (let idx = 0; DEREF_U32(elements, idx) !== 0; idx++) {
      let item_size;
      let item_align;
      const element = DEREF_U32(elements, idx);
      switch (FFI_TYPE__TYPEID(element)) {
      case FFI_TYPE_STRUCT:
        [ item_size, item_align ] = ffi_struct_size_and_alignment(element);
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
        break;
      case FFI_TYPE_COMPLEX:
        throw new Error('complex ret marshalling nyi');
      default:
        throw new Error('Unexpected rtype ' + rtype);
      }
      item_align = item_align || item_size;
      FFI_TYPE__SIZE(arg_type) = item_size;
      FFI_TYPE__ALIGN(arg_type) = item_align;
      size += item_size + PADDING(size, item_align);
      align = item_align > align ? item_align : align;
    }
    return [ size, align ];
  }

  // Unbox structs of size 0 and 1
  function unbox_small_structs(typ) {
    let typ_id = FFI_TYPE__TYPEID(typ);
    while (typ_id === FFI_TYPE_STRUCT) {
      let elements = FFI_TYPE__ELEMENTS(typ);
      let first_element = DEREF_U32(elements, 0);
      if (first_element === 0) {
        typ_id = FFI_TYPE_VOID;
        break;
      } else if (DEREF_U32(elements, 1) === 0) {
        typ = first_element;
        typ_id = FFI_TYPE__TYPEID(first_element);
      } else {
        break;
      }
    }
    return [ typ, typ_id ];
  }

  const abi = CIF__ABI(cif);
  const nargs = CIF__NARGS(cif);
  const arg_types = CIF__ARGTYPES(cif);
  const [rtype, rtype_id] = unbox_small_structs(CIF__RTYPE(cif));

  const args = [];
  let ret_by_arg = false;

#if WASM_BIGINT
  if (rtype_id === FFI_TYPE_COMPLEX) {
    throw new Error('complex ret marshalling nyi');
  }
  if (rtype_id < 0 || rtype_id > FFI_TYPE_LAST) {
    throw new Error('Unexpected rtype ' + rtype_id);
  }
  if (rtype_id === FFI_TYPE_LONGDOUBLE || rtype_id === FFI_TYPE_STRUCT) {
    args.push(rvalue);
    ret_by_arg = true;
  }
#else
  let sig;
  switch(rtype_id) {
  case FFI_TYPE_VOID:
    sig = 'v';
    break;
  case FFI_TYPE_STRUCT:
  case FFI_TYPE_LONGDOUBLE:
    sig = 'vi';
    args.push(rvalue);
    ret_by_arg = true;
    break;
  case FFI_TYPE_INT:
  case FFI_TYPE_UINT8:
  case FFI_TYPE_SINT8:
  case FFI_TYPE_UINT16:
  case FFI_TYPE_SINT16:
  case FFI_TYPE_UINT32:
  case FFI_TYPE_SINT32:
  case FFI_TYPE_POINTER:
    sig = 'i';
    break;
  case FFI_TYPE_FLOAT:
    sig = 'f';
    break;
  case FFI_TYPE_DOUBLE:
    sig = 'd';
    break;
  case FFI_TYPE_UINT64:
  case FFI_TYPE_SINT64:
    sig = 'j';
    break;
  case FFI_TYPE_COMPLEX:
    throw new Error('complex ret marshalling nyi');
  default:
    throw new Error('Unexpected rtype ' + rtype_id);
  }
#endif

  const orig_stack_ptr = stackSave();
  let structs_addr = orig_stack_ptr;
  for (let i = 0; i < nargs; i++) {
    const arg_ptr = DEREF_U32(avalue, i);
    const [arg_type, arg_type_id] =
        unbox_small_structs(DEREF_U32(arg_types, i));

    switch (arg_type_id) {
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
      // emscripten doesn't define HEAPU64 by default
      const HEAPU64 = new BigInt64Array(HEAP8.buffer);
      args.push(DEREF_U64(arg_ptr, 0));
      args.push(DEREF_U64(arg_ptr, 1));
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
      args.push(BigInt(DEREF_U32(arg_ptr, 0)) |
                (BigInt(DEREF_U32(arg_ptr, 1)) << 32n));
#else
      // LEGALIZE_JS_FFI mode splits i64 (j) into two i32 args
      // for compatibility with JavaScript's f64-based numbers.
      args.push(DEREF_U32(arg_ptr, 0));
      args.push(DEREF_U32(arg_ptr, 1));
      sig += 'j';
#endif
      break;
    case FFI_TYPE_STRUCT:
      const [item_size, item_align] =
          ffi_struct_size_and_alignment(arg_type);
      structs_addr -= item_size;
      structs_addr &= (~(item_align - 1));
      args.push(structs_addr);
      const src_ptr = DEREF_U32(avalue, i);
      HEAP8.subarray(structs_addr, structs_addr + item_size)
          .set(HEAP8.subarray(src_ptr, src_ptr + item_size));
      break;
    case FFI_TYPE_COMPLEX:
      throw new Error('complex marshalling nyi');
    default:
      throw new Error('Unexpected type ' + arg_type_id);
    }
  }

  stackRestore(structs_addr);

#if WASM_BIGINT
  const result = wasmTable.get(fn).apply(null, args);
#else
  const result = dynCall(sig, fn, args);
#endif

  stackRestore(orig_stack_ptr);

  if (ret_by_arg) {
    return;
  }

  switch (rtype_id) {
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
  case FFI_TYPE_COMPLEX:
    throw new Error('complex ret marshalling nyi');
  default:
    throw new Error('Unexpected rtype ' + rtype_id);
  }
});

#define CLOSURE__wrapper(addr) DEREF_U32(addr, 0)
#define CLOSURE__cif(addr) DEREF_U32(addr, 1)
#define CLOSURE__fun(addr) DEREF_U32(addr, 2)
#define CLOSURE__user_data(addr) DEREF_U32(addr, 3)

EM_JS_MACROS(void *, ffi_closure_alloc_helper, (size_t size, void **code), {
  let closure = _malloc(size);
  let func_ptr = getEmptyTableSlot();
  DEREF_U32(code, 0) = func_ptr;
  CLOSURE__wrapper(closure) = func_ptr;
  return closure;
})

void * __attribute__ ((visibility ("default")))
ffi_closure_alloc(size_t size, void **code) {
  return ffi_closure_alloc_helper(size, code);
}

EM_JS_MACROS(void, ffi_closure_free_helper, (void *closure), {
  let func_ptr = CLOSURE__wrapper(closure);
  removeFunction(func_ptr);
  _free(closure);
})

void __attribute__ ((visibility ("default")))
ffi_closure_free(void *closure) {
  return ffi_closure_free_helper(closure);
}

EM_JS_MACROS(
ffi_status,
ffi_prep_closure_loc_helper,
(ffi_closure * closure, ffi_cif *cif, void *fun, void *user_data, void *codeloc),
{
  "use strict";
  function unbox_small_structs(typ) {
    let typ_id = FFI_TYPE__TYPEID(typ);
    while (typ_id === FFI_TYPE_STRUCT) {
      let elements = FFI_TYPE__ELEMENTS(typ);
      let first_element = DEREF_U32(elements, 0);
      if (first_element === 0) {
        typ_id = FFI_TYPE_VOID;
        break;
      } else if (DEREF_U32(elements, 1) === 0) {
        typ = first_element;
        typ_id = FFI_TYPE__TYPEID(first_element);
      } else {
        break;
      }
    }
    return [ typ, typ_id ];
  }
  const abi = CIF__ABI(cif);
  const nargs = CIF__NARGS(cif);
  const arg_types = CIF__ARGTYPES(cif);
  const [rtype, rtype_id] = unbox_small_structs(CIF__RTYPE(cif));
  let sig;
  let ret_by_arg = false;
  switch (rtype_id) {
  case FFI_TYPE_VOID:
    sig = 'v';
    break;
  case FFI_TYPE_STRUCT:
  case FFI_TYPE_LONGDOUBLE:
    sig = 'vi';
    ret_by_arg = true;
    break;
  case FFI_TYPE_INT:
  case FFI_TYPE_UINT8:
  case FFI_TYPE_SINT8:
  case FFI_TYPE_UINT16:
  case FFI_TYPE_SINT16:
  case FFI_TYPE_UINT32:
  case FFI_TYPE_SINT32:
  case FFI_TYPE_POINTER:
    sig = 'i';
    break;
  case FFI_TYPE_FLOAT:
    sig = 'f';
    break;
  case FFI_TYPE_DOUBLE:
    sig = 'd';
    break;
  case FFI_TYPE_UINT64:
  case FFI_TYPE_SINT64:
    sig = 'j';
    break;
  case FFI_TYPE_COMPLEX:
    throw new Error('complex ret marshalling nyi');
  default:
    throw new Error('Unexpected rtype ' + rtype_id);
  }
  let struct_flags = [];
  for (let i = 0; i < nargs; i++) {
    const[arg_type, arg_type_id] =
        unbox_small_structs(DEREF_U32(arg_types, i));
    struct_flags.push(arg_type_id === FFI_TYPE_STRUCT);
    switch (arg_type_id) {
    case FFI_TYPE_INT:
    case FFI_TYPE_UINT8:
    case FFI_TYPE_SINT8:
    case FFI_TYPE_UINT16:
    case FFI_TYPE_SINT16:
    case FFI_TYPE_UINT32:
    case FFI_TYPE_SINT32:
    case FFI_TYPE_POINTER:
    case FFI_TYPE_STRUCT:
      sig += 'i';
      break;
    case FFI_TYPE_FLOAT:
      sig += 'f';
      break;
    case FFI_TYPE_DOUBLE:
      sig += 'd';
      break;
    case FFI_TYPE_LONGDOUBLE:
      throw new Error('longdouble marshalling nyi');
      break;
    case FFI_TYPE_UINT64:
    case FFI_TYPE_SINT64:
      sig += 'j';
      break;
    case FFI_TYPE_COMPLEX:
      throw new Error('complex marshalling nyi');
    default:
      throw new Error('Unexpected argtype ' + arg_type_id);
    }
  }
  function trampoline(...args) {
    let size = 0;
    const orig_stack_ptr = stackSave();
    let cur_ptr = orig_stack_ptr;
    let ret_ptr;
    if (ret_by_arg) {
      ret_ptr = args[0];
    } else {
      cur_ptr -= 4;
      cur_ptr &= (~(4 - 1));
      ret_ptr = cur_ptr;
    }
    cur_ptr -= 4 * (sig.length - 1 - ret_by_arg);
    const args_ptr = cur_ptr;
    const HEAPU64 = new BigInt64Array(HEAP8.buffer);
    for (let sig_idx = 1 + ret_by_arg; sig_idx < sig.length; sig_idx++) {
      const jsargs_idx = sig_idx - 1;
      const cargs_idx = jsargs_idx - ret_by_arg;
      const x = sig[sig_idx];
      const cur_arg = args[jsargs_idx];
      if (struct_flags[cargs_idx]) {
        cur_ptr -= 4;
        DEREF_U32(args_ptr, cargs_idx) = cur_arg;
        continue;
      }
      switch (x) {
      case "i":
        cur_ptr -= 4;
        DEREF_U32(args_ptr, cargs_idx) = cur_ptr;
        DEREF_U32(cur_ptr, 0) = cur_arg;
        break;
      case "j":
        cur_ptr &= ~(8 - 1);
        cur_ptr -= 8;
        DEREF_U32(args_ptr, cargs_idx) = cur_ptr;
        DEREF_I32(cur_ptr, 0) = Number(cur_arg & 0xffffffffn) | 0;
        DEREF_I32(cur_ptr, 1) = Number(cur_arg >> 32n) | 0;
        break;
      case "d":
        cur_ptr &= ~(8 - 1);
        cur_ptr -= 8;
        DEREF_U32(args_ptr, cargs_idx) = cur_ptr;
        DEREF_F64(cur_ptr, 0) = cur_arg;
        break;
      case "f":
        cur_ptr -= 4;
        DEREF_U32(args_ptr, cargs_idx) = cur_ptr;
        DEREF_F32(cur_ptr, 0) = cur_arg;
        break;
      }
    }
    stackRestore(cur_ptr);
    wasmTable.get(CLOSURE__fun(closure))(
        CLOSURE__cif(closure), ret_ptr, args_ptr,
        CLOSURE__user_data(closure));
    stackRestore(orig_stack_ptr);
    if (!ret_by_arg) {
      return DEREF_U32(ret_ptr, 0);
    }
  }
  let wasm_trampoline = convertJsFunctionToWasm(trampoline, sig);
  wasmTable.set(codeloc, wasm_trampoline);
  CLOSURE__cif(closure) = cif;
  CLOSURE__fun(closure) = fun;
  CLOSURE__user_data(closure) = user_data;
  return 0 /* FFI_OK */;
})

// EM_JS does not correctly handle function pointer arguments, so we need a
// helper
ffi_status ffi_prep_closure_loc(ffi_closure *closure, ffi_cif *cif,
                                void (*fun)(ffi_cif *, void *, void **, void *),
                                void *user_data, void *codeloc) {
  return ffi_prep_closure_loc_helper(closure, cif, (void *)fun, user_data,
                                     codeloc);
}