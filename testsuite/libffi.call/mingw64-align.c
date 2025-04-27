/* Test code adapted from Emil Taylor Bye's bug report:
   https://github.com/libffi/libffi/issues/897 */

/* { dg-do run } */

#include "ffitest.h"
#include "ffi_common.h"

typedef struct u64struct {
	uint64_t a;
	uint64_t b;
} u64struct_t;

typedef u64struct_t (*callback_t) (u64struct_t);

void ABI_ATTR callback(ffi_cif *cif, void *ret, void *args[], void *dontcare __UNUSED__) {
	ffi_type *argty = *(cif->arg_types);

	// This fails as the argument is not properly aligned
	CHECK(((size_t) *args) % (argty->alignment) == 0);

	*((u64struct_t *) ret) = *((u64struct_t *) *args);
}

int main() {
	ffi_cif cif;
	ffi_type *args[1];
	ffi_closure *closure;
	void *code_ptr;
	ffi_type struct_type;
	ffi_type *struct_type_elements[3];

	struct_type.size = struct_type.alignment = 0;
	struct_type.type = FFI_TYPE_STRUCT;
	struct_type.elements = struct_type_elements;
	struct_type_elements[0] = &ffi_type_uint64;
	struct_type_elements[1] = &ffi_type_uint64;
	struct_type_elements[2] = NULL;

	args[0] = &struct_type;

	closure = ffi_closure_alloc(sizeof(ffi_closure), &code_ptr);

	if (closure) {
		if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 1, &struct_type, args) == FFI_OK) {
			if (ffi_prep_closure_loc(closure, &cif, callback, NULL, code_ptr) == FFI_OK) {
				u64struct_t in_arg = { a:0x123456789ABCDEF0, b:0x123456789ABCDEF0 };
				((callback_t)code_ptr)(in_arg);
			}
		}
	}
}
