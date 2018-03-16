typedef double simd_double2 __attribute__ ((vector_size (16)));
typedef double simd_double3 __attribute__ ((vector_size (24)));
typedef double simd_double4 __attribute__ ((vector_size (32)));

typedef float simd_float2 __attribute__ ((vector_size (8)));
typedef float simd_float3 __attribute__ ((vector_size (12)));
typedef float simd_float4 __attribute__ ((vector_size (16)));

typedef struct { simd_float2 columns[2]; } simd_float2x2;
typedef struct { simd_float2 columns[3]; } simd_float3x2;
typedef struct { simd_float2 columns[4]; } simd_float4x2;
typedef struct { simd_float3 columns[2]; } simd_float2x3;
typedef struct { simd_float3 columns[3]; } simd_float3x3;
typedef struct { simd_float3 columns[4]; } simd_float4x3;
typedef struct { simd_float4 columns[2]; } simd_float2x4;
typedef struct { simd_float4 columns[3]; } simd_float3x4;
typedef struct { simd_float4 columns[4]; } simd_float4x4;

typedef struct { simd_double2 columns[2]; } simd_double2x2;
typedef struct { simd_double2 columns[3]; } simd_double3x2;
typedef struct { simd_double2 columns[4]; } simd_double4x2;
typedef struct { simd_double3 columns[2]; } simd_double2x3;
typedef struct { simd_double3 columns[3]; } simd_double3x3;
typedef struct { simd_double3 columns[4]; } simd_double4x3;
typedef struct { simd_double4 columns[2]; } simd_double2x4;
typedef struct { simd_double4 columns[3]; } simd_double3x4;
typedef struct { simd_double4 columns[4]; } simd_double4x4;