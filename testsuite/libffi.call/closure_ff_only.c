/* Area:	ffi_call, ffi_closure
   Purpose:	Check float struct pass/return via closures:
		struct{float;float}, struct{double;double},
		struct{long double;long double}, struct{double;long double},
		and nested single-element struct variants.
		These are corner cases on z/OS due to its float struct
		calling convention.
   Limitations:	none.
   PR:		none.
   Originator:	Austin S. Wells <austin.wells@ibm.com>  */
/* { dg-do run } */
#include "ffitest.h"
#include <stdio.h>

/* --- flat float structs --- */

/* struct { float; float } */
typedef struct { float a; float b; } FF;
static ffi_type *ff_elems[3];
static ffi_type ff_type = { 0, 0, FFI_TYPE_STRUCT, ff_elems };

static void closure_FF(ffi_cif *cif, void *resp, void **args, void *ud)
{
  (void)cif; (void)ud;
  FF s = *(FF *)args[0];
  printf("closure_FF: s.a=%g s.b=%g\n", (double)s.a, (double)s.b);
  fflush(stdout);
  s.a += 1.0f; s.b += 2.0f;
  *(FF *)resp = s;
}

/* struct { double; double } */
typedef struct { double a; double b; } DD;
static ffi_type *dd_elems[3];
static ffi_type dd_type = { 0, 0, FFI_TYPE_STRUCT, dd_elems };

static void closure_DD(ffi_cif *cif, void *resp, void **args, void *ud)
{
  (void)cif; (void)ud;
  DD s = *(DD *)args[0];
  printf("closure_DD: s.a=%g s.b=%g\n", s.a, s.b);
  fflush(stdout);
  s.a += 1.0; s.b += 2.0;
  *(DD *)resp = s;
}

#if FFI_TYPE_LONGDOUBLE != FFI_TYPE_DOUBLE
/* struct { long double; long double } */
typedef struct { long double a; long double b; } LDLD;
static ffi_type *ldld_elems[3];
static ffi_type ldld_type = { 0, 0, FFI_TYPE_STRUCT, ldld_elems };

static void closure_LDLD(ffi_cif *cif, void *resp, void **args, void *ud)
{
  (void)cif; (void)ud;
  LDLD s = *(LDLD *)args[0];
  printf("closure_LDLD: s.a=%Lg s.b=%Lg\n", s.a, s.b);
  fflush(stdout);
  s.a += 1.0L; s.b += 2.0L;
  *(LDLD *)resp = s;
}

/* struct { double; long double } - mixed, not a float struct;
   passed as a plain struct (corner case on z/OS) */
typedef struct { double a; long double b; } DLD;
static ffi_type *dld_elems[3];
static ffi_type dld_type = { 0, 0, FFI_TYPE_STRUCT, dld_elems };

static void closure_DLD(ffi_cif *cif, void *resp, void **args, void *ud)
{
  (void)cif; (void)ud;
  DLD s = *(DLD *)args[0];
  printf("closure_DLD: s.a=%g s.b=%Lg\n", s.a, s.b);
  fflush(stdout);
  s.a += 1.0; s.b += 2.0L;
  *(DLD *)resp = s;
}
#endif /* FFI_TYPE_LONGDOUBLE != FFI_TYPE_DOUBLE */

/* --- nested single-element struct variants ---
   struct { struct{float}; float } - the inner wrapper must be unwrapped to
   detect the float struct pattern (corner case on z/OS) */

typedef struct { float x; } SF;        /* struct wrapping one float  */
typedef struct { SF   a; float b; } SFF; /* struct { struct{float}; float } */
typedef struct { SF   a; SF    b; } SFSF; /* struct { struct{float}; struct{float} } */

static ffi_type *sf_elems[2];
static ffi_type sf_type = { 0, 0, FFI_TYPE_STRUCT, sf_elems };

static ffi_type *sff_elems[3];
static ffi_type sff_type = { 0, 0, FFI_TYPE_STRUCT, sff_elems };

static ffi_type *sfsf_elems[3];
static ffi_type sfsf_type = { 0, 0, FFI_TYPE_STRUCT, sfsf_elems };

static void closure_SFF(ffi_cif *cif, void *resp, void **args, void *ud)
{
  (void)cif; (void)ud;
  SFF s = *(SFF *)args[0];
  printf("closure_SFF: s.a.x=%g s.b=%g\n", (double)s.a.x, (double)s.b);
  fflush(stdout);
  s.a.x += 1.0f; s.b += 2.0f;
  *(SFF *)resp = s;
}

static void closure_SFSF(ffi_cif *cif, void *resp, void **args, void *ud)
{
  (void)cif; (void)ud;
  SFSF s = *(SFSF *)args[0];
  printf("closure_SFSF: s.a.x=%g s.b.x=%g\n", (double)s.a.x, (double)s.b.x);
  fflush(stdout);
  s.a.x += 1.0f; s.b.x += 2.0f;
  *(SFSF *)resp = s;
}

#if FFI_TYPE_LONGDOUBLE != FFI_TYPE_DOUBLE
typedef struct { long double x; } SLD;      /* struct wrapping one long double */
typedef struct { SLD a; long double b; } SLDLD; /* struct { struct{ldbl}; ldbl } */

static ffi_type *sld_elems[2];
static ffi_type sld_type = { 0, 0, FFI_TYPE_STRUCT, sld_elems };

static ffi_type *sldld_elems[3];
static ffi_type sldld_type = { 0, 0, FFI_TYPE_STRUCT, sldld_elems };

static void closure_SLDLD(ffi_cif *cif, void *resp, void **args, void *ud)
{
  (void)cif; (void)ud;
  SLDLD s = *(SLDLD *)args[0];
  printf("closure_SLDLD: s.a.x=%Lg s.b=%Lg\n", s.a.x, s.b);
  fflush(stdout);
  s.a.x += 1.0L; s.b += 2.0L;
  *(SLDLD *)resp = s;
}
#endif /* FFI_TYPE_LONGDOUBLE != FFI_TYPE_DOUBLE */

int main(void)
{
  /* --- FF: struct { float; float } --- */
  ff_elems[0] = &ffi_type_float;
  ff_elems[1] = &ffi_type_float;
  ff_elems[2] = NULL;
  {
    ffi_cif cif;
    ffi_type *args[1] = { &ff_type };
    CHECK(ffi_prep_cif(&cif, ABI_NUM, 1, &ff_type, args) == FFI_OK);
    printf("FF cif.flags=%u\n", cif.flags); fflush(stdout);
    void *code;
    ffi_closure *cl = ffi_closure_alloc(sizeof(ffi_closure), &code);
    CHECK(cl != NULL);
    CHECK(ffi_prep_closure_loc(cl, &cif, closure_FF, NULL, code) == FFI_OK);
    FF in = { 10.0f, 20.0f };
    FF out = ((FF(*)(FF))code)(in);
    printf("FF out.a=%g out.b=%g  (expect 11 22)\n",
           (double)out.a, (double)out.b); fflush(stdout);
    CHECK_FLOAT_EQ(out.a, 11.0f);
    CHECK_FLOAT_EQ(out.b, 22.0f);
    ffi_closure_free(cl);
  }

  /* --- DD: struct { double; double } --- */
  dd_elems[0] = &ffi_type_double;
  dd_elems[1] = &ffi_type_double;
  dd_elems[2] = NULL;
  {
    ffi_cif cif;
    ffi_type *args[1] = { &dd_type };
    CHECK(ffi_prep_cif(&cif, ABI_NUM, 1, &dd_type, args) == FFI_OK);
    printf("DD cif.flags=%u\n", cif.flags); fflush(stdout);
    void *code;
    ffi_closure *cl = ffi_closure_alloc(sizeof(ffi_closure), &code);
    CHECK(cl != NULL);
    CHECK(ffi_prep_closure_loc(cl, &cif, closure_DD, NULL, code) == FFI_OK);
    DD in = { 10.0, 20.0 };
    DD out = ((DD(*)(DD))code)(in);
    printf("DD out.a=%g out.b=%g  (expect 11 22)\n", out.a, out.b);
    fflush(stdout);
    CHECK_DOUBLE_EQ(out.a, 11.0);
    CHECK_DOUBLE_EQ(out.b, 22.0);
    ffi_closure_free(cl);
  }

#if FFI_TYPE_LONGDOUBLE != FFI_TYPE_DOUBLE
  /* --- LDLD: struct { long double; long double } --- */
  ldld_elems[0] = &ffi_type_longdouble;
  ldld_elems[1] = &ffi_type_longdouble;
  ldld_elems[2] = NULL;
  {
    ffi_cif cif;
    ffi_type *args[1] = { &ldld_type };
    CHECK(ffi_prep_cif(&cif, ABI_NUM, 1, &ldld_type, args) == FFI_OK);
    printf("LDLD cif.flags=%u\n", cif.flags); fflush(stdout);
    void *code;
    ffi_closure *cl = ffi_closure_alloc(sizeof(ffi_closure), &code);
    CHECK(cl != NULL);
    CHECK(ffi_prep_closure_loc(cl, &cif, closure_LDLD, NULL, code) == FFI_OK);
    LDLD in = { 10.0L, 20.0L };
    LDLD out = ((LDLD(*)(LDLD))code)(in);
    printf("LDLD out.a=%Lg out.b=%Lg  (expect 11 22)\n", out.a, out.b);
    fflush(stdout);
    CHECK_DOUBLE_EQ((double)out.a, 11.0);
    CHECK_DOUBLE_EQ((double)out.b, 22.0);
    ffi_closure_free(cl);
  }

  /* --- DLD: struct { double; long double } - mixed, plain struct path --- */
  dld_elems[0] = &ffi_type_double;
  dld_elems[1] = &ffi_type_longdouble;
  dld_elems[2] = NULL;
  {
    ffi_cif cif;
    ffi_type *args[1] = { &dld_type };
    CHECK(ffi_prep_cif(&cif, ABI_NUM, 1, &dld_type, args) == FFI_OK);
    printf("DLD cif.flags=%u\n", cif.flags); fflush(stdout);
    void *code;
    ffi_closure *cl = ffi_closure_alloc(sizeof(ffi_closure), &code);
    CHECK(cl != NULL);
    CHECK(ffi_prep_closure_loc(cl, &cif, closure_DLD, NULL, code) == FFI_OK);
    DLD in = { 10.0, 20.0L };
    DLD out = ((DLD(*)(DLD))code)(in);
    printf("DLD out.a=%g out.b=%Lg  (expect 11 22)\n", out.a, out.b);
    fflush(stdout);
    CHECK_DOUBLE_EQ(out.a, 11.0);
    CHECK_DOUBLE_EQ((double)out.b, 22.0);
    ffi_closure_free(cl);
  }
#endif /* FFI_TYPE_LONGDOUBLE != FFI_TYPE_DOUBLE */

  /* --- SFF: struct { struct{float}; float } - nested unwrapping --- */
  sf_elems[0] = &ffi_type_float;
  sf_elems[1] = NULL;
  sff_elems[0] = &sf_type;
  sff_elems[1] = &ffi_type_float;
  sff_elems[2] = NULL;
  {
    ffi_cif cif;
    ffi_type *args[1] = { &sff_type };
    CHECK(ffi_prep_cif(&cif, ABI_NUM, 1, &sff_type, args) == FFI_OK);
    printf("SFF cif.flags=%u\n", cif.flags); fflush(stdout);
    void *code;
    ffi_closure *cl = ffi_closure_alloc(sizeof(ffi_closure), &code);
    CHECK(cl != NULL);
    CHECK(ffi_prep_closure_loc(cl, &cif, closure_SFF, NULL, code) == FFI_OK);
    SFF in = { {10.0f}, 20.0f };
    SFF out = ((SFF(*)(SFF))code)(in);
    printf("SFF out.a.x=%g out.b=%g  (expect 11 22)\n",
           (double)out.a.x, (double)out.b); fflush(stdout);
    CHECK_FLOAT_EQ(out.a.x, 11.0f);
    CHECK_FLOAT_EQ(out.b,   22.0f);
    ffi_closure_free(cl);
  }

  /* --- SFSF: struct { struct{float}; struct{float} } - nested pair --- */
  sfsf_elems[0] = &sf_type;
  sfsf_elems[1] = &sf_type;
  sfsf_elems[2] = NULL;
  {
    ffi_cif cif;
    ffi_type *args[1] = { &sfsf_type };
    CHECK(ffi_prep_cif(&cif, ABI_NUM, 1, &sfsf_type, args) == FFI_OK);
    printf("SFSF cif.flags=%u\n", cif.flags); fflush(stdout);
    void *code;
    ffi_closure *cl = ffi_closure_alloc(sizeof(ffi_closure), &code);
    CHECK(cl != NULL);
    CHECK(ffi_prep_closure_loc(cl, &cif, closure_SFSF, NULL, code) == FFI_OK);
    SFSF in = { {10.0f}, {20.0f} };
    SFSF out = ((SFSF(*)(SFSF))code)(in);
    printf("SFSF out.a.x=%g out.b.x=%g  (expect 11 22)\n",
           (double)out.a.x, (double)out.b.x); fflush(stdout);
    CHECK_FLOAT_EQ(out.a.x, 11.0f);
    CHECK_FLOAT_EQ(out.b.x, 22.0f);
    ffi_closure_free(cl);
  }

#if FFI_TYPE_LONGDOUBLE != FFI_TYPE_DOUBLE
  /* --- SLDLD: struct { struct{long double}; long double } - nested unwrapping --- */
  sld_elems[0] = &ffi_type_longdouble;
  sld_elems[1] = NULL;
  sldld_elems[0] = &sld_type;
  sldld_elems[1] = &ffi_type_longdouble;
  sldld_elems[2] = NULL;
  {
    ffi_cif cif;
    ffi_type *args[1] = { &sldld_type };
    CHECK(ffi_prep_cif(&cif, ABI_NUM, 1, &sldld_type, args) == FFI_OK);
    printf("SLDLD cif.flags=%u\n", cif.flags); fflush(stdout);
    void *code;
    ffi_closure *cl = ffi_closure_alloc(sizeof(ffi_closure), &code);
    CHECK(cl != NULL);
    CHECK(ffi_prep_closure_loc(cl, &cif, closure_SLDLD, NULL, code) == FFI_OK);
    SLDLD in = { {10.0L}, 20.0L };
    SLDLD out = ((SLDLD(*)(SLDLD))code)(in);
    printf("SLDLD out.a.x=%Lg out.b=%Lg  (expect 11 22)\n",
           out.a.x, out.b); fflush(stdout);
    CHECK_DOUBLE_EQ((double)out.a.x, 11.0);
    CHECK_DOUBLE_EQ((double)out.b,   22.0);
    ffi_closure_free(cl);
  }
#endif /* FFI_TYPE_LONGDOUBLE != FFI_TYPE_DOUBLE */

  printf("PASS\n");
  return 0;
}
