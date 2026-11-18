/* Area:	ffi_call, ffi_closure
   Purpose:	Check that structs whose only scalar leaves are exactly two
		values of the same float type are passed/returned in FPR pairs
		regardless of nesting depth.  Corner case on z/OS due to its
		float struct calling convention.
   Limitations:	none.
   PR:		none.
   Originator:	Austin S. Wells <austin.wells@ibm.com>  */

/* { dg-do run } */
#include "ffitest.h"
#include <stdio.h>
#include <string.h>

/* ---- C types ------------------------------------------------------------ */

/* flat: {float, float} */
typedef struct { float a; float b; } FF;

/* wrapper on first element: {struct{float}, float} */
typedef struct { float v; } Wf;
typedef struct { Wf   a; float b; } WfF;

/* wrapper on second element: {float, struct{float}} */
typedef struct { float a; Wf    b; } FWf;

/* wrappers on both: {struct{float}, struct{float}} */
typedef struct { Wf a; Wf b; } WfWf;

/* outer single-element struct wrapping a {float,float} pair: struct { FF } */
typedef struct { FF inner; } WFF;

/* double-wrapped: struct { struct { FF } } */
typedef struct { WFF inner; } WWFF;

/* same four shapes for double */
typedef struct { double a; double b; } DD;
typedef struct { double v; } Wd;
typedef struct { Wd     a; double b; } WdD;
typedef struct { double a; Wd     b; } DWd;
typedef struct { Wd a; Wd b; } WdWd;

/* outer single-element struct wrapping a {double,double} pair: struct { DD } */
typedef struct { DD inner; } WDD;

/* double-wrapped: struct { struct { DD } } */
typedef struct { WDD inner; } WWDD;

/* ---- C functions under test --------------------------------------------- */

static FF    ABI_ATTR fn_FF   (FF    s) { s.a += 1.0f; s.b += 2.0f; return s; }
static WfF   ABI_ATTR fn_WfF  (WfF   s) { s.a.v += 1.0f; s.b += 2.0f; return s; }
static FWf   ABI_ATTR fn_FWf  (FWf   s) { s.a += 1.0f; s.b.v += 2.0f; return s; }
static WfWf  ABI_ATTR fn_WfWf (WfWf  s) { s.a.v += 1.0f; s.b.v += 2.0f; return s; }

static DD    ABI_ATTR fn_DD   (DD    s) { s.a += 1.0; s.b += 2.0; return s; }
static WdD   ABI_ATTR fn_WdD  (WdD   s) { s.a.v += 1.0; s.b += 2.0; return s; }
static DWd   ABI_ATTR fn_DWd  (DWd   s) { s.a += 1.0; s.b.v += 2.0; return s; }
static WdWd  ABI_ATTR fn_WdWd (WdWd  s) { s.a.v += 1.0; s.b.v += 2.0; return s; }

static WFF   ABI_ATTR fn_WFF  (WFF   s) { s.inner.a += 1.0f; s.inner.b += 2.0f; return s; }
static WWFF  ABI_ATTR fn_WWFF (WWFF  s) { s.inner.inner.a += 1.0f; s.inner.inner.b += 2.0f; return s; }
static WDD   ABI_ATTR fn_WDD  (WDD   s) { s.inner.a += 1.0; s.inner.b += 2.0; return s; }
static WWDD  ABI_ATTR fn_WWDD (WWDD  s) { s.inner.inner.a += 1.0; s.inner.inner.b += 2.0; return s; }

/* ---- ffi_type descriptors ----------------------------------------------- */

static ffi_type *wf_elems[2];
static ffi_type wf_type = { 0, 0, FFI_TYPE_STRUCT, wf_elems };

static ffi_type *ff_elems[3];
static ffi_type ff_type = { 0, 0, FFI_TYPE_STRUCT, ff_elems };

static ffi_type *wff_elems[3];
static ffi_type wff_type = { 0, 0, FFI_TYPE_STRUCT, wff_elems };

static ffi_type *fwf_elems[3];
static ffi_type fwf_type = { 0, 0, FFI_TYPE_STRUCT, fwf_elems };

static ffi_type *wfwf_elems[3];
static ffi_type wfwf_type = { 0, 0, FFI_TYPE_STRUCT, wfwf_elems };

static ffi_type *wd_elems[2];
static ffi_type wd_type = { 0, 0, FFI_TYPE_STRUCT, wd_elems };

static ffi_type *dd_elems[3];
static ffi_type dd_type = { 0, 0, FFI_TYPE_STRUCT, dd_elems };

static ffi_type *wdd_elems[3];
static ffi_type wdd_type = { 0, 0, FFI_TYPE_STRUCT, wdd_elems };

static ffi_type *dwd_elems[3];
static ffi_type dwd_type = { 0, 0, FFI_TYPE_STRUCT, dwd_elems };

static ffi_type *wdwd_elems[3];
static ffi_type wdwd_type = { 0, 0, FFI_TYPE_STRUCT, wdwd_elems };

static ffi_type *wff2_elems[2];
static ffi_type wff2_type = { 0, 0, FFI_TYPE_STRUCT, wff2_elems };

static ffi_type *wwff_elems[2];
static ffi_type wwff_type = { 0, 0, FFI_TYPE_STRUCT, wwff_elems };

static ffi_type *wdd2_elems[2];
static ffi_type wdd2_type = { 0, 0, FFI_TYPE_STRUCT, wdd2_elems };

static ffi_type *wwdd_elems[2];
static ffi_type wwdd_type = { 0, 0, FFI_TYPE_STRUCT, wwdd_elems };

static void init_types(void)
{
  wf_elems[0] = &ffi_type_float;  wf_elems[1] = NULL;

  ff_elems[0] = &ffi_type_float;  ff_elems[1] = &ffi_type_float;  ff_elems[2] = NULL;
  wff_elems[0] = &wf_type;        wff_elems[1] = &ffi_type_float; wff_elems[2] = NULL;
  fwf_elems[0] = &ffi_type_float; fwf_elems[1] = &wf_type;        fwf_elems[2] = NULL;
  wfwf_elems[0] = &wf_type;       wfwf_elems[1] = &wf_type;       wfwf_elems[2] = NULL;

  wd_elems[0] = &ffi_type_double; wd_elems[1] = NULL;

  dd_elems[0] = &ffi_type_double;  dd_elems[1] = &ffi_type_double;  dd_elems[2] = NULL;
  wdd_elems[0] = &wd_type;         wdd_elems[1] = &ffi_type_double; wdd_elems[2] = NULL;
  dwd_elems[0] = &ffi_type_double; dwd_elems[1] = &wd_type;         dwd_elems[2] = NULL;
  wdwd_elems[0] = &wd_type;        wdwd_elems[1] = &wd_type;        wdwd_elems[2] = NULL;

  wff2_elems[0] = &ff_type;   wff2_elems[1] = NULL;
  wwff_elems[0] = &wff2_type; wwff_elems[1] = NULL;

  wdd2_elems[0] = &dd_type;   wdd2_elems[1] = NULL;
  wwdd_elems[0] = &wdd2_type; wwdd_elems[1] = NULL;
}

/* ---- closure handlers --------------------------------------------------- */

static void closure_FF(ffi_cif *cif, void *resp, void **args, void *ud)
{
  (void)cif; (void)ud;
  FF s = *(FF *)args[0];
  s.a += 1.0f; s.b += 2.0f;
  *(FF *)resp = s;
}

static void closure_WfF(ffi_cif *cif, void *resp, void **args, void *ud)
{
  (void)cif; (void)ud;
  WfF s = *(WfF *)args[0];
  s.a.v += 1.0f; s.b += 2.0f;
  *(WfF *)resp = s;
}

static void closure_FWf(ffi_cif *cif, void *resp, void **args, void *ud)
{
  (void)cif; (void)ud;
  FWf s = *(FWf *)args[0];
  s.a += 1.0f; s.b.v += 2.0f;
  *(FWf *)resp = s;
}

static void closure_WfWf(ffi_cif *cif, void *resp, void **args, void *ud)
{
  (void)cif; (void)ud;
  WfWf s = *(WfWf *)args[0];
  s.a.v += 1.0f; s.b.v += 2.0f;
  *(WfWf *)resp = s;
}

static void closure_DD(ffi_cif *cif, void *resp, void **args, void *ud)
{
  (void)cif; (void)ud;
  DD s = *(DD *)args[0];
  s.a += 1.0; s.b += 2.0;
  *(DD *)resp = s;
}

static void closure_WdD(ffi_cif *cif, void *resp, void **args, void *ud)
{
  (void)cif; (void)ud;
  WdD s = *(WdD *)args[0];
  s.a.v += 1.0; s.b += 2.0;
  *(WdD *)resp = s;
}

static void closure_DWd(ffi_cif *cif, void *resp, void **args, void *ud)
{
  (void)cif; (void)ud;
  DWd s = *(DWd *)args[0];
  s.a += 1.0; s.b.v += 2.0;
  *(DWd *)resp = s;
}

static void closure_WdWd(ffi_cif *cif, void *resp, void **args, void *ud)
{
  (void)cif; (void)ud;
  WdWd s = *(WdWd *)args[0];
  s.a.v += 1.0; s.b.v += 2.0;
  *(WdWd *)resp = s;
}

static void closure_WFF(ffi_cif *cif, void *resp, void **args, void *ud)
{
  (void)cif; (void)ud;
  WFF s = *(WFF *)args[0];
  s.inner.a += 1.0f; s.inner.b += 2.0f;
  *(WFF *)resp = s;
}

static void closure_WWFF(ffi_cif *cif, void *resp, void **args, void *ud)
{
  (void)cif; (void)ud;
  WWFF s = *(WWFF *)args[0];
  s.inner.inner.a += 1.0f; s.inner.inner.b += 2.0f;
  *(WWFF *)resp = s;
}

static void closure_WDD(ffi_cif *cif, void *resp, void **args, void *ud)
{
  (void)cif; (void)ud;
  WDD s = *(WDD *)args[0];
  s.inner.a += 1.0; s.inner.b += 2.0;
  *(WDD *)resp = s;
}

static void closure_WWDD(ffi_cif *cif, void *resp, void **args, void *ud)
{
  (void)cif; (void)ud;
  WWDD s = *(WWDD *)args[0];
  s.inner.inner.a += 1.0; s.inner.inner.b += 2.0;
  *(WWDD *)resp = s;
}

/* ---- helpers ------------------------------------------------------------ */

static ffi_closure *make_closure(ffi_cif *cif, ffi_type *rtype,
                                  ffi_type **args,
                                  void (*handler)(ffi_cif*,void*,void**,void*),
                                  void **code)
{
  CHECK(ffi_prep_cif(cif, ABI_NUM, 1, rtype, args) == FFI_OK);
  ffi_closure *cl = ffi_closure_alloc(sizeof(ffi_closure), code);
  CHECK(cl != NULL);
  CHECK(ffi_prep_closure_loc(cl, cif, handler, NULL, *code) == FFI_OK);
  return cl;
}

/* ---- main --------------------------------------------------------------- */

int main(void)
{
  init_types();

  ffi_cif cif;

  /* ---- ffi_call: float variants ---------------------------------------- */

  { FF in = {10.0f, 20.0f}, out;
    ffi_type *args[1] = { &ff_type };
    void *vals[1] = { &in };
    CHECK(ffi_prep_cif(&cif, ABI_NUM, 1, &ff_type, args) == FFI_OK);
    ffi_call(&cif, FFI_FN(fn_FF), &out, vals);
    CHECK_FLOAT_EQ(out.a, 11.0f);
    CHECK_FLOAT_EQ(out.b, 22.0f); }

  { WfF in = {{10.0f}, 20.0f}, out;
    ffi_type *args[1] = { &wff_type };
    void *vals[1] = { &in };
    CHECK(ffi_prep_cif(&cif, ABI_NUM, 1, &wff_type, args) == FFI_OK);
    ffi_call(&cif, FFI_FN(fn_WfF), &out, vals);
    CHECK_FLOAT_EQ(out.a.v, 11.0f);
    CHECK_FLOAT_EQ(out.b,   22.0f); }

  { FWf in = {10.0f, {20.0f}}, out;
    ffi_type *args[1] = { &fwf_type };
    void *vals[1] = { &in };
    CHECK(ffi_prep_cif(&cif, ABI_NUM, 1, &fwf_type, args) == FFI_OK);
    ffi_call(&cif, FFI_FN(fn_FWf), &out, vals);
    CHECK_FLOAT_EQ(out.a,   11.0f);
    CHECK_FLOAT_EQ(out.b.v, 22.0f); }

  { WfWf in = {{10.0f}, {20.0f}}, out;
    ffi_type *args[1] = { &wfwf_type };
    void *vals[1] = { &in };
    CHECK(ffi_prep_cif(&cif, ABI_NUM, 1, &wfwf_type, args) == FFI_OK);
    ffi_call(&cif, FFI_FN(fn_WfWf), &out, vals);
    CHECK_FLOAT_EQ(out.a.v, 11.0f);
    CHECK_FLOAT_EQ(out.b.v, 22.0f); }

  /* struct { FF } */
  { WFF in, out;
    in.inner.a = 10.0f; in.inner.b = 20.0f;
    ffi_type *args[1] = { &wff2_type };
    void *vals[1] = { &in };
    memset(&out, 0, sizeof(out));
    CHECK(ffi_prep_cif(&cif, ABI_NUM, 1, &wff2_type, args) == FFI_OK);
    ffi_call(&cif, FFI_FN(fn_WFF), &out, vals);
    CHECK_FLOAT_EQ(out.inner.a, 11.0f);
    CHECK_FLOAT_EQ(out.inner.b, 22.0f); }

  /* struct { struct { FF } } */
  { WWFF in, out;
    in.inner.inner.a = 10.0f; in.inner.inner.b = 20.0f;
    ffi_type *args[1] = { &wwff_type };
    void *vals[1] = { &in };
    CHECK(ffi_prep_cif(&cif, ABI_NUM, 1, &wwff_type, args) == FFI_OK);
    ffi_call(&cif, FFI_FN(fn_WWFF), &out, vals);
    CHECK_FLOAT_EQ(out.inner.inner.a, 11.0f);
    CHECK_FLOAT_EQ(out.inner.inner.b, 22.0f); }

  /* ---- ffi_call: double variants --------------------------------------- */

  { DD in = {10.0, 20.0}, out;
    ffi_type *args[1] = { &dd_type };
    void *vals[1] = { &in };
    CHECK(ffi_prep_cif(&cif, ABI_NUM, 1, &dd_type, args) == FFI_OK);
    ffi_call(&cif, FFI_FN(fn_DD), &out, vals);
    CHECK_DOUBLE_EQ(out.a, 11.0);
    CHECK_DOUBLE_EQ(out.b, 22.0); }

  { WdD in = {{10.0}, 20.0}, out;
    ffi_type *args[1] = { &wdd_type };
    void *vals[1] = { &in };
    CHECK(ffi_prep_cif(&cif, ABI_NUM, 1, &wdd_type, args) == FFI_OK);
    ffi_call(&cif, FFI_FN(fn_WdD), &out, vals);
    CHECK_DOUBLE_EQ(out.a.v, 11.0);
    CHECK_DOUBLE_EQ(out.b,   22.0); }

  { DWd in = {10.0, {20.0}}, out;
    ffi_type *args[1] = { &dwd_type };
    void *vals[1] = { &in };
    CHECK(ffi_prep_cif(&cif, ABI_NUM, 1, &dwd_type, args) == FFI_OK);
    ffi_call(&cif, FFI_FN(fn_DWd), &out, vals);
    CHECK_DOUBLE_EQ(out.a,   11.0);
    CHECK_DOUBLE_EQ(out.b.v, 22.0); }

  { WdWd in = {{10.0}, {20.0}}, out;
    ffi_type *args[1] = { &wdwd_type };
    void *vals[1] = { &in };
    CHECK(ffi_prep_cif(&cif, ABI_NUM, 1, &wdwd_type, args) == FFI_OK);
    ffi_call(&cif, FFI_FN(fn_WdWd), &out, vals);
    CHECK_DOUBLE_EQ(out.a.v, 11.0);
    CHECK_DOUBLE_EQ(out.b.v, 22.0); }

  /* struct { DD } */
  { WDD in, out;
    in.inner.a = 10.0; in.inner.b = 20.0;
    ffi_type *args[1] = { &wdd2_type };
    void *vals[1] = { &in };
    CHECK(ffi_prep_cif(&cif, ABI_NUM, 1, &wdd2_type, args) == FFI_OK);
    ffi_call(&cif, FFI_FN(fn_WDD), &out, vals);
    CHECK_DOUBLE_EQ(out.inner.a, 11.0);
    CHECK_DOUBLE_EQ(out.inner.b, 22.0); }

  /* struct { struct { DD } } */
  { WWDD in, out;
    in.inner.inner.a = 10.0; in.inner.inner.b = 20.0;
    ffi_type *args[1] = { &wwdd_type };
    void *vals[1] = { &in };
    CHECK(ffi_prep_cif(&cif, ABI_NUM, 1, &wwdd_type, args) == FFI_OK);
    ffi_call(&cif, FFI_FN(fn_WWDD), &out, vals);
    CHECK_DOUBLE_EQ(out.inner.inner.a, 11.0);
    CHECK_DOUBLE_EQ(out.inner.inner.b, 22.0); }

  /* ---- closures: float variants ---------------------------------------- */

  { ffi_cif cl_cif; void *code; ffi_type *cl_args[1] = { &ff_type };
    FF in = {10.0f, 20.0f}, out;
    ffi_closure *cl = make_closure(&cl_cif, &ff_type, cl_args, closure_FF, &code);
    out = ((FF(*)(FF))code)(in);
    CHECK_FLOAT_EQ(out.a, 11.0f); CHECK_FLOAT_EQ(out.b, 22.0f);
    ffi_closure_free(cl); }

  { ffi_cif cl_cif; void *code; ffi_type *cl_args[1] = { &wff_type };
    WfF in = {{10.0f}, 20.0f}, out;
    ffi_closure *cl = make_closure(&cl_cif, &wff_type, cl_args, closure_WfF, &code);
    out = ((WfF(*)(WfF))code)(in);
    CHECK_FLOAT_EQ(out.a.v, 11.0f); CHECK_FLOAT_EQ(out.b, 22.0f);
    ffi_closure_free(cl); }

  { ffi_cif cl_cif; void *code; ffi_type *cl_args[1] = { &fwf_type };
    FWf in = {10.0f, {20.0f}}, out;
    ffi_closure *cl = make_closure(&cl_cif, &fwf_type, cl_args, closure_FWf, &code);
    out = ((FWf(*)(FWf))code)(in);
    CHECK_FLOAT_EQ(out.a, 11.0f); CHECK_FLOAT_EQ(out.b.v, 22.0f);
    ffi_closure_free(cl); }

  { ffi_cif cl_cif; void *code; ffi_type *cl_args[1] = { &wfwf_type };
    WfWf in = {{10.0f}, {20.0f}}, out;
    ffi_closure *cl = make_closure(&cl_cif, &wfwf_type, cl_args, closure_WfWf, &code);
    out = ((WfWf(*)(WfWf))code)(in);
    CHECK_FLOAT_EQ(out.a.v, 11.0f); CHECK_FLOAT_EQ(out.b.v, 22.0f);
    ffi_closure_free(cl); }

  /* struct { FF } */
  { ffi_cif cl_cif; void *code; ffi_type *cl_args[1] = { &wff2_type };
    WFF in, out; in.inner.a = 10.0f; in.inner.b = 20.0f;
    ffi_closure *cl = make_closure(&cl_cif, &wff2_type, cl_args, closure_WFF, &code);
    out = ((WFF(*)(WFF))code)(in);
    CHECK_FLOAT_EQ(out.inner.a, 11.0f); CHECK_FLOAT_EQ(out.inner.b, 22.0f);
    ffi_closure_free(cl); }

  /* struct { struct { FF } } */
  { ffi_cif cl_cif; void *code; ffi_type *cl_args[1] = { &wwff_type };
    WWFF in, out; in.inner.inner.a = 10.0f; in.inner.inner.b = 20.0f;
    ffi_closure *cl = make_closure(&cl_cif, &wwff_type, cl_args, closure_WWFF, &code);
    out = ((WWFF(*)(WWFF))code)(in);
    CHECK_FLOAT_EQ(out.inner.inner.a, 11.0f); CHECK_FLOAT_EQ(out.inner.inner.b, 22.0f);
    ffi_closure_free(cl); }

  /* ---- closures: double variants --------------------------------------- */

  { ffi_cif cl_cif; void *code; ffi_type *cl_args[1] = { &dd_type };
    DD in = {10.0, 20.0}, out;
    ffi_closure *cl = make_closure(&cl_cif, &dd_type, cl_args, closure_DD, &code);
    out = ((DD(*)(DD))code)(in);
    CHECK_DOUBLE_EQ(out.a, 11.0); CHECK_DOUBLE_EQ(out.b, 22.0);
    ffi_closure_free(cl); }

  { ffi_cif cl_cif; void *code; ffi_type *cl_args[1] = { &wdd_type };
    WdD in = {{10.0}, 20.0}, out;
    ffi_closure *cl = make_closure(&cl_cif, &wdd_type, cl_args, closure_WdD, &code);
    out = ((WdD(*)(WdD))code)(in);
    CHECK_DOUBLE_EQ(out.a.v, 11.0); CHECK_DOUBLE_EQ(out.b, 22.0);
    ffi_closure_free(cl); }

  { ffi_cif cl_cif; void *code; ffi_type *cl_args[1] = { &dwd_type };
    DWd in = {10.0, {20.0}}, out;
    ffi_closure *cl = make_closure(&cl_cif, &dwd_type, cl_args, closure_DWd, &code);
    out = ((DWd(*)(DWd))code)(in);
    CHECK_DOUBLE_EQ(out.a, 11.0); CHECK_DOUBLE_EQ(out.b.v, 22.0);
    ffi_closure_free(cl); }

  { ffi_cif cl_cif; void *code; ffi_type *cl_args[1] = { &wdwd_type };
    WdWd in = {{10.0}, {20.0}}, out;
    ffi_closure *cl = make_closure(&cl_cif, &wdwd_type, cl_args, closure_WdWd, &code);
    out = ((WdWd(*)(WdWd))code)(in);
    CHECK_DOUBLE_EQ(out.a.v, 11.0); CHECK_DOUBLE_EQ(out.b.v, 22.0);
    ffi_closure_free(cl); }

  /* struct { DD } */
  { ffi_cif cl_cif; void *code; ffi_type *cl_args[1] = { &wdd2_type };
    WDD in, out; in.inner.a = 10.0; in.inner.b = 20.0;
    ffi_closure *cl = make_closure(&cl_cif, &wdd2_type, cl_args, closure_WDD, &code);
    out = ((WDD(*)(WDD))code)(in);
    CHECK_DOUBLE_EQ(out.inner.a, 11.0); CHECK_DOUBLE_EQ(out.inner.b, 22.0);
    ffi_closure_free(cl); }

  /* struct { struct { DD } } */
  { ffi_cif cl_cif; void *code; ffi_type *cl_args[1] = { &wwdd_type };
    WWDD in, out; in.inner.inner.a = 10.0; in.inner.inner.b = 20.0;
    ffi_closure *cl = make_closure(&cl_cif, &wwdd_type, cl_args, closure_WWDD, &code);
    out = ((WWDD(*)(WWDD))code)(in);
    CHECK_DOUBLE_EQ(out.inner.inner.a, 11.0); CHECK_DOUBLE_EQ(out.inner.inner.b, 22.0);
    ffi_closure_free(cl); }

  printf("PASS\n");
  return 0;
}
