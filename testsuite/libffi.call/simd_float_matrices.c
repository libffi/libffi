/* Area:    fficall.
   Purpose: Check multiple values passing from different type.
        Also, exceed the limit of gpr and fpr registers on PowerPC
        Darwin.
   Limitations: none.
   PR:      none.
   Originator:  <teodor.dermendzhiev@progress.com> 20180228  */

/* { dg-do run } */
#include "ffitest.h"
#include <stdio.h>
#include <simd/simd.h>
#include <stdlib.h>

float floatsContainer[16] = {
    1.2345, 2.3456, 3.4567, 4.5678, 5.6789, 6.7891, 7.8912, 8.9123,
    9.2345, 10.3456, 11.4567, 12.5678, 13.6789, 14.7891, 15.8912, 16.9123
};

typedef struct TNSNestedAnonymousStruct {
    struct {
        int x1;
        int x2;
    } y1;
    
    struct {
        int x3;
        int x4;
    } y2;
} TNSNestedAnonymousStruct;

matrix_float2x2 getMatrixFloat2x2() {
    matrix_float2x2 result;
    for (int i = 0; i < 2; i++) {
        static int u = 0;
        for (int y = 0; y < 2; y++) {
            result.columns[i][y] = floatsContainer[u];
            u++;
        }
    }
    
    return result;
}

matrix_float2x3 getMatrixFloat2x3() {
    matrix_float2x3 result;
    
    for (int i = 0; i < 2; i++) {
        static int u = 0;
        for (int y = 0; y < 3; y++) {
            result.columns[i][y] = floatsContainer[u];
            u++;
        }
    }
    return result;
}

matrix_float2x4 getMatrixFloat2x4() {
    matrix_float2x4 result;
    
    for (int i = 0; i < 2; i++) {
        static int u = 0;
        for (int y = 0; y < 4; y++) {
            result.columns[i][y] = floatsContainer[u];
            u++;
        }
    }
    return result;
}

matrix_float3x2 getMatrixFloat3x2() {
    matrix_float3x2 result;
    for (int i = 0; i < 3; i++) {
        static int u = 0;
        for (int y = 0; y < 2; y++) {
            result.columns[i][y] = floatsContainer[u];
            u++;
        }
    }
    return result;
}

matrix_float3x3 getMatrixFloat3x3() {
    matrix_float3x3 result;
    for (int i = 0; i < 3; i++) {
        static int u = 0;
        for (int y = 0; y < 3; y++) {
            result.columns[i][y] = floatsContainer[u];
            u++;
        }
    }
    return result;
}

matrix_float3x4 getMatrixFloat3x4() {
    matrix_float3x4 result;
    for (int i = 0; i < 3; i++) {
        static int u = 0;
        for (int y = 0; y < 4; y++) {
            result.columns[i][y] = floatsContainer[u];
            u++;
        }
    }
    return result;
}

matrix_float4x2 getMatrixFloat4x2() {
    matrix_float4x2 result;
    for (int i = 0; i < 4; i++) {
        static int u = 0;
        for (int y = 0; y < 2; y++) {
            result.columns[i][y] = floatsContainer[u];
            u++;
        }
    }
    return result;
}

matrix_float4x3 getMatrixFloat4x3() {
    matrix_float4x3 result;
    for (int i = 0; i < 4; i++) {
        static int u = 0;
        for (int y = 0; y < 3; y++) {
            result.columns[i][y] = floatsContainer[u];
            u++;
        }
    }
    return result;
}

matrix_float4x4 getMatrixFloat4x4() {
    matrix_float4x4 result;
    for (int i = 0; i < 4; i++) {
        static int u = 0;
        for (int y = 0; y < 4; y++) {
            result.columns[i][y] = floatsContainer[u];
            u++;
        }
    }
    return result;
}

int matrixDimensionsArray[9][2] = {{2,2}, {2,3}, {2,4}, {3,2}, {3,3}, {3,4}, {4,2}, {4,3}, {4,4}};

int main(void) {
    for (int i = 0; i < 9; i++) {
        int dimensions[2] = {matrixDimensionsArray[i][0], matrixDimensionsArray[i][1]};
        ffi_cif cif;
        int el_size = 4;
        unsigned short alignment = dimensions[1] == 3 ? 4 : dimensions[1];
        int bufferSize = el_size * dimensions[0] * alignment;
        void* buffer = malloc(bufferSize);
        
        ffi_type ffiType;
        ffiType.size = bufferSize;
        ffiType.alignment = alignment;
        ffiType.type = FFI_TYPE_STRUCT;
        
        ffi_type ffiTypeCol;
        ffiTypeCol.size = bufferSize;
        ffiTypeCol.alignment = alignment;
        ffiTypeCol.type = FFI_TYPE_STRUCT;
        
        ffi_type ffiTypeVector;
        ffiTypeVector.size = bufferSize/dimensions[0];
        ffiTypeVector.alignment = alignment;
        ffiTypeVector.type = FFI_TYPE_EXT_VECTOR;
        
        ffi_type ffiTypeEl;
        ffiTypeEl.size = 4;
        ffiTypeEl.alignment = alignment;
        ffiTypeEl.type = FFI_TYPE_FLOAT;
        
        ffi_type* colElements[2];
        ffi_type** colElementsP = colElements;
        
        
        ffi_type* matrixElements[dimensions[0] + 1];
        ffi_type** matrixElementsP = matrixElements;
        
        ffi_type* vectorElements[dimensions[1] + 1];
        ffi_type** vectorElementsP = vectorElements;
        
        ffiType.elements = colElementsP;
        ffiTypeCol.elements = matrixElementsP;
        ffiTypeVector.elements = vectorElementsP;
        
        for (int i = 0; i < dimensions[0]; i++) {
            ffiTypeCol.elements[i] = &ffiTypeVector;
        }
        for (int i = 0;i < dimensions[1]; i++) {
            ffiTypeVector.elements[i] = &ffiTypeEl;
        }
        ffiType.elements[0] = &ffiTypeCol;
        ffiType.elements[1] = NULL;
        ffiTypeCol.elements[dimensions[0]] = NULL;
        ffiTypeVector.elements[dimensions[1]] = NULL;
        
        if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 0,
                         &ffiType, 0) == FFI_OK)
        {
            switch (i) {
                case 0: {
                    ffi_call(&cif, FFI_FN(getMatrixFloat2x2), buffer, 0);
                    matrix_float2x2 *m = (matrix_float2x2*)buffer;
                    int u = 0;
                    for (int i = 0; i < dimensions[0]; i++) {
                        for (int y = 0; y < dimensions[1]; y++) {
                            CHECK(m->columns[i][y] == floatsContainer[u]);
                            u++;
                        }
                    }
                    break;
                }
                case 1: {
                    ffi_call(&cif, FFI_FN(getMatrixFloat2x3), buffer, 0);
                    matrix_float2x3 *m = (matrix_float2x3*)buffer;
                    for (int i = 0; i < dimensions[0]; i++) {
                        static int u = 0;
                        for (int y = 0; y < dimensions[1]; y++) {
                            CHECK(m->columns[i][y] == floatsContainer[u]);
                            u++;
                        }
                    }
                    break;
                }
                case 2: {
                    ffi_call(&cif, FFI_FN(getMatrixFloat2x4), buffer, 0);
                    matrix_float2x4 *m = (matrix_float2x4*)buffer;
                    for (int i = 0; i < dimensions[0]; i++) {
                        static int u = 0;
                        for (int y = 0; y < dimensions[1]; y++) {
                            CHECK(m->columns[i][y] == floatsContainer[u]);
                            u++;
                        }
                    }
                    break;
                }
                case 3: {
                    ffi_call(&cif, FFI_FN(getMatrixFloat3x2), buffer, 0);
                    matrix_float3x2 *m = (matrix_float3x2*)buffer;
                    for (int i = 0; i < dimensions[0]; i++) {
                        static int u = 0;
                        for (int y = 0; y < dimensions[1]; y++) {
                            CHECK(m->columns[i][y] == floatsContainer[u]);
                            u++;
                        }
                    }
                    break;
                }
                case 4: {
                    ffi_call(&cif, FFI_FN(getMatrixFloat3x3), buffer, 0);
                    matrix_float3x3 *m = (matrix_float3x3*)buffer;
                    for (int i = 0; i < dimensions[0]; i++) {
                        static int u = 0;
                        for (int y = 0; y < dimensions[1]; y++) {
                            CHECK(m->columns[i][y] == floatsContainer[u]);
                            u++;
                        }
                    }
                    break;
                }
                case 5: {
                    ffi_call(&cif, FFI_FN(getMatrixFloat3x4), buffer, 0);
                    matrix_float3x4 *m = (matrix_float3x4*)buffer;
                    for (int i = 0; i < dimensions[0]; i++) {
                        static int u = 0;
                        for (int y = 0; y < dimensions[1]; y++) {
                            CHECK(m->columns[i][y] == floatsContainer[u]);
                            u++;
                        }
                    }
                    break;
                }
                case 6: {
                    ffi_call(&cif, FFI_FN(getMatrixFloat4x2), buffer, 0);
                    matrix_float4x2 *m = (matrix_float4x2*)buffer;
                    for (int i = 0; i < dimensions[0]; i++) {
                        static int u = 0;
                        for (int y = 0; y < dimensions[1]; y++) {
                            CHECK(m->columns[i][y] == floatsContainer[u]);
                            u++;
                        }
                    }
                    break;
                }
                case 7: {
                    ffi_call(&cif, FFI_FN(getMatrixFloat4x3), buffer, 0);
                    matrix_float4x3 *m = (matrix_float4x3*)buffer;
                    for (int i = 0; i < dimensions[0]; i++) {
                        static int u = 0;
                        for (int y = 0; y < dimensions[1]; y++) {
                            CHECK(m->columns[i][y] == floatsContainer[u]);
                            u++;
                        }
                    }
                    break;
                }
                case 8: {
                    ffi_call(&cif, FFI_FN(getMatrixFloat4x4), buffer, 0);
                    matrix_float4x4 *m = (matrix_float4x4*)buffer;
                    for (int i = 0; i < dimensions[0]; i++) {
                        static int u = 0;
                        for (int y = 0; y < dimensions[1]; y++) {
                            CHECK(m->columns[i][y] == floatsContainer[u]);
                            u++;
                        }
                    }
                    break;
                }
                default:
                    break;
            }
        }
    }
    exit(0);
    printf("End matrix float tests\n");
}

