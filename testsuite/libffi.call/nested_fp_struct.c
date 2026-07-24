/* Area:      ffi_call, ffi_closure
   Purpose:   Check that any struct whose only scalar leaves are exactly two
              values of the same float type is passed/returned in FPR pairs,
              regardless of nesting depth.  Exercises the recursive
              count_float_leaves() logic in ffi.c and the STRCTDISP TYPCLASS
              call in xplink.S.
   Limitations: none.
   PR:         none.
   Originator: IBM */

/* { dg-do run } */
#include "ffitest.h"
#include <stdio.h>
#include <string.h>

/* forward decl so we can call it from main for tracing */
#pragma map(ffi_struct_float_pair_type, "STFPTYP")
unsigned int ffi_struct_float_pair_type (ffi_type **);

/* ---- C types ------------------------------------------------------------ */

/* flat: {float, float} — the baseline */
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

/* ffi_type for Wf: struct { float } */
static ffi_type *wf_elems[2];
static ffi_type wf_type = { 0, 0, FFI_TYPE_STRUCT, wf_elems };

/* ffi_type for FF: struct { float, float } */
static ffi_type *ff_elems[3];
static ffi_type ff_type = { 0, 0, FFI_TYPE_STRUCT, ff_elems };

/* ffi_type for WfF: struct { Wf, float } */
static ffi_type *wff_elems[3];
static ffi_type wff_type = { 0, 0, FFI_TYPE_STRUCT, wff_elems };

/* ffi_type for FWf: struct { float, Wf } */
static ffi_type *fwf_elems[3];
static ffi_type fwf_type = { 0, 0, FFI_TYPE_STRUCT, fwf_elems };

/* ffi_type for WfWf: struct { Wf, Wf } */
static ffi_type *wfwf_elems[3];
static ffi_type wfwf_type = { 0, 0, FFI_TYPE_STRUCT, wfwf_elems };

/* ffi_type for Wd: struct { double } */
static ffi_type *wd_elems[2];
static ffi_type wd_type = { 0, 0, FFI_TYPE_STRUCT, wd_elems };

/* ffi_type for DD: struct { double, double } */
static ffi_type *dd_elems[3];
static ffi_type dd_type = { 0, 0, FFI_TYPE_STRUCT, dd_elems };

/* ffi_type for WdD: struct { Wd, double } */
static ffi_type *wdd_elems[3];
static ffi_type wdd_type = { 0, 0, FFI_TYPE_STRUCT, wdd_elems };

/* ffi_type for DWd: struct { double, Wd } */
static ffi_type *dwd_elems[3];
static ffi_type dwd_type = { 0, 0, FFI_TYPE_STRUCT, dwd_elems };

/* ffi_type for WdWd: struct { Wd, Wd } */
static ffi_type *wdwd_elems[3];
static ffi_type wdwd_type = { 0, 0, FFI_TYPE_STRUCT, wdwd_elems };

/* ffi_type for WFF: struct { FF } */
static ffi_type *wff2_elems[2];
static ffi_type wff2_type = { 0, 0, FFI_TYPE_STRUCT, wff2_elems };

/* ffi_type for WWFF: struct { WFF } */
static ffi_type *wwff_elems[2];
static ffi_type wwff_type = { 0, 0, FFI_TYPE_STRUCT, wwff_elems };

/* ffi_type for WDD: struct { DD } */
static ffi_type *wdd2_elems[2];
static ffi_type wdd2_type = { 0, 0, FFI_TYPE_STRUCT, wdd2_elems };

/* ffi_type for WWDD: struct { WDD } */
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
  (void)ud;
  FF s = *(FF *)args[0];
  printf("TRACE closure_FF: args[0]=%p s.a=%g s.b=%g cif.flags=%u\n",
         args[0], (double)s.a, (double)s.b, cif->flags);
  { unsigned char *p = (unsigned char*)args[0];
    printf("TRACE closure_FF arg bytes:");
    for (int i = 0; i < 8; i++) printf(" %02x", p[i]);
    printf("\n"); }
  fflush(stdout);
  s.a += 1.0f; s.b += 2.0f;
  *(FF *)resp = s;
  printf("TRACE closure_FF: resp.a=%g resp.b=%g\n",
         (double)((FF*)resp)->a, (double)((FF*)resp)->b);
  fflush(stdout);
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

#define RUN_CALL(cif, fn, rtype, arg_type, arg_val, expected_a, expected_b, fa, fb) \
  do {                                                                   \
    ffi_type *_args[1] = { &(arg_type) };                               \
    void *_vals[1];                                                      \
    arg_val _in;                                                         \
    rtype   _out;                                                        \
    _in.fa = 10.0; _in.fb = 20.0;                                       \
    _vals[0] = &_in;                                                     \
    CHECK(ffi_prep_cif(&(cif), ABI_NUM, 1, &(arg_type), _args) == FFI_OK); \
    ffi_call(&(cif), FFI_FN(fn), &_out, _vals);                         \
    CHECK_FLOAT_EQ((float)(_out.fa), (float)(expected_a));              \
    CHECK_FLOAT_EQ((float)(_out.fb), (float)(expected_b));              \
  } while (0)

/* args[1] must be caller-owned and live as long as the CIF is in use,
   because ffi_prep_cif stores the pointer directly without copying. */
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

#define RUN_CLOSURE(cl_fn, rtype, arg_type, arg_val, expected_a, expected_b, fa, fb) \
  do {                                                                   \
    ffi_cif _cif;                                                        \
    void *_code;                                                         \
    ffi_type *_args[1] = { &(arg_type) };                               \
    arg_val _in;                                                         \
    rtype   _out;                                                        \
    _in.fa = 10.0; _in.fb = 20.0;                                       \
    ffi_closure *_cl = make_closure(&_cif, &(arg_type), _args,         \
                                    cl_fn, &_code);                     \
    typedef rtype (*fn_t)(arg_val);                                     \
    _out = ((fn_t)_code)(_in);                                          \
    CHECK_FLOAT_EQ((float)(_out.fa), (float)(expected_a));              \
    CHECK_FLOAT_EQ((float)(_out.fb), (float)(expected_b));              \
    ffi_closure_free(_cl);                                               \
  } while (0)

/* Trace helper: print ffi_struct_float_pair_type result and cif flags for a
   named type, flush immediately so output survives a subsequent SIGABRT.  */
#define TRACE_TYPE(label, ffi_type_ptr)                                         \
  do {                                                                          \
    ffi_type *_tp = (ffi_type_ptr);                                             \
    unsigned int _fp = ffi_struct_float_pair_type(&_tp);                        \
    printf("TRACE %-12s stfptyp=%u (%s)\n",                                     \
           (label), _fp,                                                        \
           _fp == 2 ? "FLOAT" : _fp == 3 ? "DOUBLE" :                          \
           _fp == 4 ? "LDBL"  : "0=none");                                      \
    fflush(stdout);                                                             \
  } while (0)

#define TRACE_CIF(label, cif_ptr)                                               \
  do {                                                                          \
    static const char *_rnames[] = {                                            \
      "VOID","STRUCT","FLOAT","DOUBLE","LDBL",                                  \
      "INT32","INT64","STRUCT_FF","STRUCT_DD","STRUCT_LDLD","COMPLEX_INT"        \
    };                                                                          \
    unsigned _f = (cif_ptr)->flags;                                             \
    printf("TRACE %-12s cif.flags=%u (%s)\n",                                   \
           (label), _f,                                                         \
           _f < 11 ? _rnames[_f] : "?");                                        \
    fflush(stdout);                                                             \
  } while (0)

#define TRACE_FF_RESULT(label, a, b, ea, eb)                                    \
  do {                                                                          \
    printf("TRACE %-12s got a=%g b=%g  expect a=%g b=%g  %s\n",                \
           (label), (double)(a), (double)(b), (double)(ea), (double)(eb),       \
           ((float)(a)==(float)(ea) && (float)(b)==(float)(eb)) ? "OK" : "FAIL"); \
    fflush(stdout);                                                             \
  } while (0)

/* ---- main --------------------------------------------------------------- */

int main(void)
{
  init_types();

  /* ---- dump all type classifications up front so they appear before any
     potential SIGABRT ---------------------------------------------------- */

  printf("=== ffi_struct_float_pair_type classifications ===\n"); fflush(stdout);
  TRACE_TYPE("ff_type",    &ff_type);
  TRACE_TYPE("wff_type",   &wff_type);
  TRACE_TYPE("fwf_type",   &fwf_type);
  TRACE_TYPE("wfwf_type",  &wfwf_type);
  TRACE_TYPE("wff2_type",  &wff2_type);   /* struct { FF }       — expect FLOAT */
  TRACE_TYPE("wwff_type",  &wwff_type);   /* struct { struct{FF} } — expect FLOAT */
  TRACE_TYPE("dd_type",    &dd_type);
  TRACE_TYPE("wdd_type",   &wdd_type);
  TRACE_TYPE("dwd_type",   &dwd_type);
  TRACE_TYPE("wdwd_type",  &wdwd_type);
  TRACE_TYPE("wdd2_type",  &wdd2_type);   /* struct { DD }       — expect DOUBLE */
  TRACE_TYPE("wwdd_type",  &wwdd_type);   /* struct { struct{DD} } — expect DOUBLE */
  printf("=== end classifications ===\n"); fflush(stdout);

  /* ---- ffi_call: float variants ---------------------------------------- */

  ffi_cif cif;

  /* {float, float} */
  printf("\n--- ffi_call FF ---\n"); fflush(stdout);
  { FF in = {10.0f, 20.0f}, out;
    ffi_type *args[1] = { &ff_type };
    void *vals[1] = { &in };
    CHECK(ffi_prep_cif(&cif, ABI_NUM, 1, &ff_type, args) == FFI_OK);
    TRACE_CIF("ff_type", &cif);
    ffi_call(&cif, FFI_FN(fn_FF), &out, vals);
    TRACE_FF_RESULT("FF", out.a, out.b, 11.0f, 22.0f);
    CHECK_FLOAT_EQ(out.a, 11.0f);
    CHECK_FLOAT_EQ(out.b, 22.0f); }

  /* {struct{float}, float} */
  printf("\n--- ffi_call WfF ---\n"); fflush(stdout);
  { WfF in = {{10.0f}, 20.0f}, out;
    ffi_type *args[1] = { &wff_type };
    void *vals[1] = { &in };
    CHECK(ffi_prep_cif(&cif, ABI_NUM, 1, &wff_type, args) == FFI_OK);
    TRACE_CIF("wff_type", &cif);
    ffi_call(&cif, FFI_FN(fn_WfF), &out, vals);
    TRACE_FF_RESULT("WfF", out.a.v, out.b, 11.0f, 22.0f);
    CHECK_FLOAT_EQ(out.a.v, 11.0f);
    CHECK_FLOAT_EQ(out.b,   22.0f); }

  /* {float, struct{float}} */
  printf("\n--- ffi_call FWf ---\n"); fflush(stdout);
  { FWf in = {10.0f, {20.0f}}, out;
    ffi_type *args[1] = { &fwf_type };
    void *vals[1] = { &in };
    CHECK(ffi_prep_cif(&cif, ABI_NUM, 1, &fwf_type, args) == FFI_OK);
    TRACE_CIF("fwf_type", &cif);
    ffi_call(&cif, FFI_FN(fn_FWf), &out, vals);
    TRACE_FF_RESULT("FWf", out.a, out.b.v, 11.0f, 22.0f);
    CHECK_FLOAT_EQ(out.a,   11.0f);
    CHECK_FLOAT_EQ(out.b.v, 22.0f); }

  /* {struct{float}, struct{float}} */
  printf("\n--- ffi_call WfWf ---\n"); fflush(stdout);
  { WfWf in = {{10.0f}, {20.0f}}, out;
    ffi_type *args[1] = { &wfwf_type };
    void *vals[1] = { &in };
    CHECK(ffi_prep_cif(&cif, ABI_NUM, 1, &wfwf_type, args) == FFI_OK);
    TRACE_CIF("wfwf_type", &cif);
    ffi_call(&cif, FFI_FN(fn_WfWf), &out, vals);
    TRACE_FF_RESULT("WfWf", out.a.v, out.b.v, 11.0f, 22.0f);
    CHECK_FLOAT_EQ(out.a.v, 11.0f);
    CHECK_FLOAT_EQ(out.b.v, 22.0f); }

  /* ---- ffi_call: deeply nested float variants -------------------------- */

  /* struct { FF } */
  printf("\n--- ffi_call WFF (struct{FF}) ---\n"); fflush(stdout);
  { WFF in, out;
    in.inner.a = 10.0f; in.inner.b = 20.0f;
    ffi_type *args[1] = { &wff2_type };
    void *vals[1] = { &in };
    memset(&out, 0, sizeof(out));
    CHECK(ffi_prep_cif(&cif, ABI_NUM, 1, &wff2_type, args) == FFI_OK);
    TRACE_CIF("wff2_type", &cif);
    printf("TRACE WFF in  bytes:"); fflush(stdout);
    { unsigned char *p = (unsigned char*)&in;
      for (int i=0; i<(int)sizeof(in); i++) printf(" %02x", p[i]);
      printf("\n"); fflush(stdout); }
    ffi_call(&cif, FFI_FN(fn_WFF), &out, vals);
    printf("TRACE WFF out bytes:"); fflush(stdout);
    { unsigned char *p = (unsigned char*)&out;
      for (int i=0; i<(int)sizeof(out); i++) printf(" %02x", p[i]);
      printf("\n"); fflush(stdout); }
    TRACE_FF_RESULT("WFF", out.inner.a, out.inner.b, 11.0f, 22.0f);
    /* Also verify direct C call works */
    { WFF direct_out = fn_WFF(in);
      printf("TRACE WFF direct: a=%g b=%g (expect 11 22)\n",
             (double)direct_out.inner.a, (double)direct_out.inner.b);
      fflush(stdout); }
    CHECK_FLOAT_EQ(out.inner.a, 11.0f);
    CHECK_FLOAT_EQ(out.inner.b, 22.0f); }

  /* struct { struct { FF } } */
  printf("\n--- ffi_call WWFF (struct{struct{FF}}) ---\n"); fflush(stdout);
  { WWFF in, out;
    in.inner.inner.a = 10.0f; in.inner.inner.b = 20.0f;
    ffi_type *args[1] = { &wwff_type };
    void *vals[1] = { &in };
    CHECK(ffi_prep_cif(&cif, ABI_NUM, 1, &wwff_type, args) == FFI_OK);
    TRACE_CIF("wwff_type", &cif);
    ffi_call(&cif, FFI_FN(fn_WWFF), &out, vals);
    TRACE_FF_RESULT("WWFF", out.inner.inner.a, out.inner.inner.b, 11.0f, 22.0f);
    CHECK_FLOAT_EQ(out.inner.inner.a, 11.0f);
    CHECK_FLOAT_EQ(out.inner.inner.b, 22.0f); }

  /* ---- ffi_call: double variants --------------------------------------- */

  /* {double, double} */
  printf("\n--- ffi_call DD ---\n"); fflush(stdout);
  { DD in = {10.0, 20.0}, out;
    ffi_type *args[1] = { &dd_type };
    void *vals[1] = { &in };
    CHECK(ffi_prep_cif(&cif, ABI_NUM, 1, &dd_type, args) == FFI_OK);
    TRACE_CIF("dd_type", &cif);
    ffi_call(&cif, FFI_FN(fn_DD), &out, vals);
    TRACE_FF_RESULT("DD", out.a, out.b, 11.0, 22.0);
    CHECK_DOUBLE_EQ(out.a, 11.0);
    CHECK_DOUBLE_EQ(out.b, 22.0); }

  /* {struct{double}, double} */
  printf("\n--- ffi_call WdD ---\n"); fflush(stdout);
  { WdD in = {{10.0}, 20.0}, out;
    ffi_type *args[1] = { &wdd_type };
    void *vals[1] = { &in };
    CHECK(ffi_prep_cif(&cif, ABI_NUM, 1, &wdd_type, args) == FFI_OK);
    TRACE_CIF("wdd_type", &cif);
    ffi_call(&cif, FFI_FN(fn_WdD), &out, vals);
    TRACE_FF_RESULT("WdD", out.a.v, out.b, 11.0, 22.0);
    CHECK_DOUBLE_EQ(out.a.v, 11.0);
    CHECK_DOUBLE_EQ(out.b,   22.0); }

  /* {double, struct{double}} */
  printf("\n--- ffi_call DWd ---\n"); fflush(stdout);
  { DWd in = {10.0, {20.0}}, out;
    ffi_type *args[1] = { &dwd_type };
    void *vals[1] = { &in };
    CHECK(ffi_prep_cif(&cif, ABI_NUM, 1, &dwd_type, args) == FFI_OK);
    TRACE_CIF("dwd_type", &cif);
    ffi_call(&cif, FFI_FN(fn_DWd), &out, vals);
    TRACE_FF_RESULT("DWd", out.a, out.b.v, 11.0, 22.0);
    CHECK_DOUBLE_EQ(out.a,   11.0);
    CHECK_DOUBLE_EQ(out.b.v, 22.0); }

  /* {struct{double}, struct{double}} */
  printf("\n--- ffi_call WdWd ---\n"); fflush(stdout);
  { WdWd in = {{10.0}, {20.0}}, out;
    ffi_type *args[1] = { &wdwd_type };
    void *vals[1] = { &in };
    CHECK(ffi_prep_cif(&cif, ABI_NUM, 1, &wdwd_type, args) == FFI_OK);
    TRACE_CIF("wdwd_type", &cif);
    ffi_call(&cif, FFI_FN(fn_WdWd), &out, vals);
    TRACE_FF_RESULT("WdWd", out.a.v, out.b.v, 11.0, 22.0);
    CHECK_DOUBLE_EQ(out.a.v, 11.0);
    CHECK_DOUBLE_EQ(out.b.v, 22.0); }

  /* ---- ffi_call: deeply nested double variants ------------------------- */

  /* struct { DD } */
  printf("\n--- ffi_call WDD (struct{DD}) ---\n"); fflush(stdout);
  { WDD in, out;
    in.inner.a = 10.0; in.inner.b = 20.0;
    ffi_type *args[1] = { &wdd2_type };
    void *vals[1] = { &in };
    CHECK(ffi_prep_cif(&cif, ABI_NUM, 1, &wdd2_type, args) == FFI_OK);
    TRACE_CIF("wdd2_type", &cif);
    ffi_call(&cif, FFI_FN(fn_WDD), &out, vals);
    TRACE_FF_RESULT("WDD", out.inner.a, out.inner.b, 11.0, 22.0);
    CHECK_DOUBLE_EQ(out.inner.a, 11.0);
    CHECK_DOUBLE_EQ(out.inner.b, 22.0); }

  /* struct { struct { DD } } */
  printf("\n--- ffi_call WWDD (struct{struct{DD}}) ---\n"); fflush(stdout);
  { WWDD in, out;
    in.inner.inner.a = 10.0; in.inner.inner.b = 20.0;
    ffi_type *args[1] = { &wwdd_type };
    void *vals[1] = { &in };
    CHECK(ffi_prep_cif(&cif, ABI_NUM, 1, &wwdd_type, args) == FFI_OK);
    TRACE_CIF("wwdd_type", &cif);
    ffi_call(&cif, FFI_FN(fn_WWDD), &out, vals);
    TRACE_FF_RESULT("WWDD", out.inner.inner.a, out.inner.inner.b, 11.0, 22.0);
    CHECK_DOUBLE_EQ(out.inner.inner.a, 11.0);
    CHECK_DOUBLE_EQ(out.inner.inner.b, 22.0); }

  /* ---- closures: float variants ---------------------------------------- */

  printf("\n--- closure FF ---\n"); fflush(stdout);
  { ffi_cif cl_cif; void *code; ffi_type *cl_args[1] = { &ff_type };
    FF in = {10.0f, 20.0f}, out;
    memset(&out, 0, sizeof(out));
    ffi_closure *cl = make_closure(&cl_cif, &ff_type, cl_args, closure_FF, &code);
    TRACE_CIF("cl ff_type", &cl_cif);
    printf("TRACE cl FF &in=%p &out=%p\n", (void*)&in, (void*)&out); fflush(stdout);
    out = ((FF(*)(FF))code)(in);
    printf("TRACE cl FF out bytes:"); fflush(stdout);
    { unsigned char *p = (unsigned char*)&out;
      for (int i = 0; i < (int)sizeof(out); i++) printf(" %02x", p[i]);
      printf("\n"); fflush(stdout); }
    TRACE_FF_RESULT("cl FF", out.a, out.b, 11.0f, 22.0f);
    CHECK_FLOAT_EQ(out.a, 11.0f); CHECK_FLOAT_EQ(out.b, 22.0f);
    ffi_closure_free(cl); }

  printf("\n--- closure WfF ---\n"); fflush(stdout);
  { ffi_cif cl_cif; void *code; ffi_type *cl_args[1] = { &wff_type };
    WfF in = {{10.0f}, 20.0f}, out;
    ffi_closure *cl = make_closure(&cl_cif, &wff_type, cl_args, closure_WfF, &code);
    TRACE_CIF("cl wff_type", &cl_cif);
    out = ((WfF(*)(WfF))code)(in);
    TRACE_FF_RESULT("cl WfF", out.a.v, out.b, 11.0f, 22.0f);
    CHECK_FLOAT_EQ(out.a.v, 11.0f); CHECK_FLOAT_EQ(out.b, 22.0f);
    ffi_closure_free(cl); }

  printf("\n--- closure FWf ---\n"); fflush(stdout);
  { ffi_cif cl_cif; void *code; ffi_type *cl_args[1] = { &fwf_type };
    FWf in = {10.0f, {20.0f}}, out;
    ffi_closure *cl = make_closure(&cl_cif, &fwf_type, cl_args, closure_FWf, &code);
    TRACE_CIF("cl fwf_type", &cl_cif);
    out = ((FWf(*)(FWf))code)(in);
    TRACE_FF_RESULT("cl FWf", out.a, out.b.v, 11.0f, 22.0f);
    CHECK_FLOAT_EQ(out.a, 11.0f); CHECK_FLOAT_EQ(out.b.v, 22.0f);
    ffi_closure_free(cl); }

  printf("\n--- closure WfWf ---\n"); fflush(stdout);
  { ffi_cif cl_cif; void *code; ffi_type *cl_args[1] = { &wfwf_type };
    WfWf in = {{10.0f}, {20.0f}}, out;
    ffi_closure *cl = make_closure(&cl_cif, &wfwf_type, cl_args, closure_WfWf, &code);
    TRACE_CIF("cl wfwf_type", &cl_cif);
    out = ((WfWf(*)(WfWf))code)(in);
    TRACE_FF_RESULT("cl WfWf", out.a.v, out.b.v, 11.0f, 22.0f);
    CHECK_FLOAT_EQ(out.a.v, 11.0f); CHECK_FLOAT_EQ(out.b.v, 22.0f);
    ffi_closure_free(cl); }

  /* struct { FF } */
  printf("\n--- closure WFF (struct{FF}) ---\n"); fflush(stdout);
  { ffi_cif cl_cif; void *code; ffi_type *cl_args[1] = { &wff2_type };
    WFF in, out; in.inner.a = 10.0f; in.inner.b = 20.0f;
    ffi_closure *cl = make_closure(&cl_cif, &wff2_type, cl_args, closure_WFF, &code);
    TRACE_CIF("cl wff2_type", &cl_cif);
    out = ((WFF(*)(WFF))code)(in);
    TRACE_FF_RESULT("cl WFF", out.inner.a, out.inner.b, 11.0f, 22.0f);
    CHECK_FLOAT_EQ(out.inner.a, 11.0f); CHECK_FLOAT_EQ(out.inner.b, 22.0f);
    ffi_closure_free(cl); }

  /* struct { struct { FF } } */
  printf("\n--- closure WWFF (struct{struct{FF}}) ---\n"); fflush(stdout);
  { ffi_cif cl_cif; void *code; ffi_type *cl_args[1] = { &wwff_type };
    WWFF in, out; in.inner.inner.a = 10.0f; in.inner.inner.b = 20.0f;
    ffi_closure *cl = make_closure(&cl_cif, &wwff_type, cl_args, closure_WWFF, &code);
    TRACE_CIF("cl wwff_type", &cl_cif);
    out = ((WWFF(*)(WWFF))code)(in);
    TRACE_FF_RESULT("cl WWFF", out.inner.inner.a, out.inner.inner.b, 11.0f, 22.0f);
    CHECK_FLOAT_EQ(out.inner.inner.a, 11.0f); CHECK_FLOAT_EQ(out.inner.inner.b, 22.0f);
    ffi_closure_free(cl); }

  /* ---- closures: double variants --------------------------------------- */

  printf("\n--- closure DD ---\n"); fflush(stdout);
  { ffi_cif cl_cif; void *code; ffi_type *cl_args[1] = { &dd_type };
    DD in = {10.0, 20.0}, out;
    ffi_closure *cl = make_closure(&cl_cif, &dd_type, cl_args, closure_DD, &code);
    TRACE_CIF("cl dd_type", &cl_cif);
    out = ((DD(*)(DD))code)(in);
    TRACE_FF_RESULT("cl DD", out.a, out.b, 11.0, 22.0);
    CHECK_DOUBLE_EQ(out.a, 11.0); CHECK_DOUBLE_EQ(out.b, 22.0);
    ffi_closure_free(cl); }

  printf("\n--- closure WdD ---\n"); fflush(stdout);
  { ffi_cif cl_cif; void *code; ffi_type *cl_args[1] = { &wdd_type };
    WdD in = {{10.0}, 20.0}, out;
    ffi_closure *cl = make_closure(&cl_cif, &wdd_type, cl_args, closure_WdD, &code);
    TRACE_CIF("cl wdd_type", &cl_cif);
    out = ((WdD(*)(WdD))code)(in);
    TRACE_FF_RESULT("cl WdD", out.a.v, out.b, 11.0, 22.0);
    CHECK_DOUBLE_EQ(out.a.v, 11.0); CHECK_DOUBLE_EQ(out.b, 22.0);
    ffi_closure_free(cl); }

  printf("\n--- closure DWd ---\n"); fflush(stdout);
  { ffi_cif cl_cif; void *code; ffi_type *cl_args[1] = { &dwd_type };
    DWd in = {10.0, {20.0}}, out;
    ffi_closure *cl = make_closure(&cl_cif, &dwd_type, cl_args, closure_DWd, &code);
    TRACE_CIF("cl dwd_type", &cl_cif);
    out = ((DWd(*)(DWd))code)(in);
    TRACE_FF_RESULT("cl DWd", out.a, out.b.v, 11.0, 22.0);
    CHECK_DOUBLE_EQ(out.a, 11.0); CHECK_DOUBLE_EQ(out.b.v, 22.0);
    ffi_closure_free(cl); }

  printf("\n--- closure WdWd ---\n"); fflush(stdout);
  { ffi_cif cl_cif; void *code; ffi_type *cl_args[1] = { &wdwd_type };
    WdWd in = {{10.0}, {20.0}}, out;
    ffi_closure *cl = make_closure(&cl_cif, &wdwd_type, cl_args, closure_WdWd, &code);
    TRACE_CIF("cl wdwd_type", &cl_cif);
    out = ((WdWd(*)(WdWd))code)(in);
    TRACE_FF_RESULT("cl WdWd", out.a.v, out.b.v, 11.0, 22.0);
    CHECK_DOUBLE_EQ(out.a.v, 11.0); CHECK_DOUBLE_EQ(out.b.v, 22.0);
    ffi_closure_free(cl); }

  /* struct { DD } */
  printf("\n--- closure WDD (struct{DD}) ---\n"); fflush(stdout);
  { ffi_cif cl_cif; void *code; ffi_type *cl_args[1] = { &wdd2_type };
    WDD in, out; in.inner.a = 10.0; in.inner.b = 20.0;
    ffi_closure *cl = make_closure(&cl_cif, &wdd2_type, cl_args, closure_WDD, &code);
    TRACE_CIF("cl wdd2_type", &cl_cif);
    out = ((WDD(*)(WDD))code)(in);
    TRACE_FF_RESULT("cl WDD", out.inner.a, out.inner.b, 11.0, 22.0);
    CHECK_DOUBLE_EQ(out.inner.a, 11.0); CHECK_DOUBLE_EQ(out.inner.b, 22.0);
    ffi_closure_free(cl); }

  /* struct { struct { DD } } */
  printf("\n--- closure WWDD (struct{struct{DD}}) ---\n"); fflush(stdout);
  { ffi_cif cl_cif; void *code; ffi_type *cl_args[1] = { &wwdd_type };
    WWDD in, out; in.inner.inner.a = 10.0; in.inner.inner.b = 20.0;
    ffi_closure *cl = make_closure(&cl_cif, &wwdd_type, cl_args, closure_WWDD, &code);
    TRACE_CIF("cl wwdd_type", &cl_cif);
    out = ((WWDD(*)(WWDD))code)(in);
    TRACE_FF_RESULT("cl WWDD", out.inner.inner.a, out.inner.inner.b, 11.0, 22.0);
    CHECK_DOUBLE_EQ(out.inner.inner.a, 11.0); CHECK_DOUBLE_EQ(out.inner.inner.b, 22.0);
    ffi_closure_free(cl); }

  printf("nested_fp_struct: all checks passed\n");
  exit(0);
}
