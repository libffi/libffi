/**
  Copyright 1993 Bill Triggs <Bill.Triggs@inrialpes.fr>
  Copyright 1995-2017 Bruno Haible <bruno@clisp.org>

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
**/

/* { dg-do run } */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ffi.h>
#include "alignof.h"

#include "testcases.c"

/* libffi testsuite local changes -------------------------------- */
/* Redefine exit(1) as a test failure */
#define exit(V) (void)((V) ? (abort(), 1) : exit(0))
int count = 0;
char rbuf1[2048];
char rbuf2[2048];
#define FPRINTF(STREAM,FMT,ARGS...)			\
  { fprintf(STREAM, FMT, ##ARGS) ;			\
    switch (count++)					\
      {							\
        case 0:						\
        case 1:						\
	  sprintf(&rbuf1[strlen(rbuf1)], FMT, ##ARGS);	\
	  break ;					\
        case 2:						\
	  sprintf(rbuf2, FMT, ##ARGS);	                \
	  break;					\
        case 3:						\
	  sprintf(&rbuf2[strlen(rbuf2)], FMT, ##ARGS);	\
	  if (strcmp (rbuf1, rbuf2)) abort();		\
	  break;					\
      }							\
  }
/* --------------------------------------------------------------- */

/* Definitions that ought to be part of libffi. */
static ffi_type ffi_type_char;
#define ffi_type_slonglong ffi_type_sint64
#define ffi_type_ulonglong ffi_type_uint64

/* libffi does not support arrays inside structs. */
#define SKIP_EXTRA_STRUCTS

#define FFI_PREP_CIF(cif,argtypes,rettype) \
  if (ffi_prep_cif(&(cif),FFI_DEFAULT_ABI,sizeof(argtypes)/sizeof(argtypes[0]),&rettype,argtypes) != FFI_OK) abort()
#define FFI_PREP_CIF_NOARGS(cif,rettype) \
  if (ffi_prep_cif(&(cif),FFI_DEFAULT_ABI,0,&rettype,NULL) != FFI_OK) abort()
#define FFI_CALL(cif,fn,args,retaddr) \
  ffi_call(&(cif),(void(*)(void))(fn),retaddr,args)

long clear_traces_i (long a, long b, long c, long d, long e, long f, long g, long h,
                     long i, long j, long k, long l, long m, long n, long o, long p)
{ return 0; }
float clear_traces_f (float a, float b, float c, float d, float e, float f, float g,
                      float h, float i, float j, float k, float l, float m, float n,
                      float o, float p)
{ return 0.0; }
double clear_traces_d (double a, double b, double c, double d, double e, double f, double g,
                       double h, double i, double j, double k, double l, double m, double n,
                       double o, double p)
{ return 0.0; }
J clear_traces_J (void)
{ J j; j.l1 = j.l2 = 0; return j; }
void clear_traces (void)
{ clear_traces_i(0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0);
  clear_traces_f(0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0);
  clear_traces_d(0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0);
  clear_traces_J();
}

void
  void_tests (void)
{
#ifdef TEST01  
  v_v();
  clear_traces();
  {
    ffi_cif cif;
    FFI_PREP_CIF_NOARGS(cif,ffi_type_void);
    {
      FFI_CALL(cif,v_v,NULL,NULL);
    }
  }
#endif  
  return;
}
void
  int_tests (void)
{
  int ir;
#ifdef TEST02
  ir = i_v();
  FPRINTF(out,"->%d\n",ir);
  fflush(out);
  ir = 0; clear_traces();
  {
    ffi_cif cif;
    FFI_PREP_CIF_NOARGS(cif,ffi_type_sint);
    {
      FFI_CALL(cif,i_v,NULL,&ir);
    }
  }
  FPRINTF(out,"->%d\n",ir);
  fflush(out);
#endif

#ifdef TEST03
  ir = i_i(i1);
  FPRINTF(out,"->%d\n",ir);
  fflush(out);
  ir = 0; clear_traces();
  {
    ffi_type* argtypes[] = { &ffi_type_sint };
    ffi_cif cif;
    FFI_PREP_CIF(cif,argtypes,ffi_type_sint);
    {
      /*const*/ void* args[] = { &i1 };
      FFI_CALL(cif,i_i,args,&ir);
    }
  }
  FPRINTF(out,"->%d\n",ir);
  fflush(out);
#endif

#ifdef TEST04
  ir = i_i2(i1,i2);
  FPRINTF(out,"->%d\n",ir);
  fflush(out);
  ir = 0; clear_traces();
  {
    ffi_type* argtypes[] = { &ffi_type_sint, &ffi_type_sint };
    ffi_cif cif;
    FFI_PREP_CIF(cif,argtypes,ffi_type_sint);
    {
      /*const*/ void* args[] = { &i1, &i2 };
      FFI_CALL(cif,i_i2,args,&ir);
    }
  }
  FPRINTF(out,"->%d\n",ir);
  fflush(out);
#endif

#ifdef TEST05
  ir = i_i4(i1,i2,i3,i4);
  FPRINTF(out,"->%d\n",ir);
  fflush(out);
  ir = 0; clear_traces();
  {
    ffi_type* argtypes[] = { &ffi_type_sint, &ffi_type_sint, &ffi_type_sint, &ffi_type_sint };
    ffi_cif cif;
    FFI_PREP_CIF(cif,argtypes,ffi_type_sint);
    {
      /*const*/ void* args[] = { &i1, &i2, &i3, &i4 };
      FFI_CALL(cif,i_i4,args,&ir);
    }
  }
  FPRINTF(out,"->%d\n",ir);
  fflush(out);
#endif

#ifdef TEST06
  ir = i_i8(i1,i2,i3,i4,i5,i6,i7,i8);
  FPRINTF(out,"->%d\n",ir);
  fflush(out);
  ir = 0; clear_traces();
  {
    ffi_type* argtypes[] = { &ffi_type_sint, &ffi_type_sint, &ffi_type_sint, &ffi_type_sint, &ffi_type_sint, &ffi_type_sint, &ffi_type_sint, &ffi_type_sint };
    ffi_cif cif;
    FFI_PREP_CIF(cif,argtypes,ffi_type_sint);
    {
      /*const*/ void* args[] = { &i1, &i2, &i3, &i4, &i5, &i6, &i7, &i8 };
      FFI_CALL(cif,i_i8,args,&ir);
    }
  }
  FPRINTF(out,"->%d\n",ir);
  fflush(out);
#endif

#ifdef TEST07
  ir = i_i16(i1,i2,i3,i4,i5,i6,i7,i8,i9,i10,i11,i12,i13,i14,i15,i16);
  FPRINTF(out,"->%d\n",ir);
  fflush(out);
  ir = 0; clear_traces();
  {
    ffi_type* argtypes[] = { &ffi_type_sint, &ffi_type_sint, &ffi_type_sint, &ffi_type_sint, &ffi_type_sint, &ffi_type_sint, &ffi_type_sint, &ffi_type_sint, &ffi_type_sint, &ffi_type_sint, &ffi_type_sint, &ffi_type_sint, &ffi_type_sint, &ffi_type_sint, &ffi_type_sint, &ffi_type_sint };
    ffi_cif cif;
    FFI_PREP_CIF(cif,argtypes,ffi_type_sint);
    {
      /*const*/ void* args[] = { &i1, &i2, &i3, &i4, &i5, &i6, &i7, &i8, &i9, &i10, &i11, &i12, &i13, &i14, &i15, &i16 };
      FFI_CALL(cif,i_i16,args,&ir);
    }
  }
  FPRINTF(out,"->%d\n",ir);
  fflush(out);
#endif

  return;
}
void
  float_tests (void)
{
  float fr;

#ifdef TEST08
  fr = f_f(f1);
  FPRINTF(out,"->%g\n",fr);
  fflush(out);
  fr = 0.0; clear_traces();
  {
    ffi_type* argtypes[] = { &ffi_type_float };
    ffi_cif cif;
    FFI_PREP_CIF(cif,argtypes,ffi_type_float);
    {
      /*const*/ void* args[] = { &f1 };
      FFI_CALL(cif,f_f,args,&fr);
    }
  }
  FPRINTF(out,"->%g\n",fr);
  fflush(out);
#endif

#ifdef TEST09
  fr = f_f2(f1,f2);
  FPRINTF(out,"->%g\n",fr);
  fflush(out);
  fr = 0.0; clear_traces();
  {
    ffi_type* argtypes[] = { &ffi_type_float, &ffi_type_float };
    ffi_cif cif;
    FFI_PREP_CIF(cif,argtypes,ffi_type_float);
    {
      /*const*/ void* args[] = { &f1, &f2 };
      FFI_CALL(cif,f_f2,args,&fr);
    }
  }
  FPRINTF(out,"->%g\n",fr);
  fflush(out);
#endif

#ifdef TEST10
  fr = f_f4(f1,f2,f3,f4);
  FPRINTF(out,"->%g\n",fr);
  fflush(out);
  fr = 0.0; clear_traces();
  {
    ffi_type* argtypes[] = { &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float };
    ffi_cif cif;
    FFI_PREP_CIF(cif,argtypes,ffi_type_float);
    {
      /*const*/ void* args[] = { &f1, &f2, &f3, &f4 };
      FFI_CALL(cif,f_f4,args,&fr);
    }
  }
  FPRINTF(out,"->%g\n",fr);
  fflush(out);
#endif

#ifdef TEST11
  fr = f_f8(f1,f2,f3,f4,f5,f6,f7,f8);
  FPRINTF(out,"->%g\n",fr);
  fflush(out);
  fr = 0.0; clear_traces();
  {
    ffi_type* argtypes[] = { &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float };
    ffi_cif cif;
    FFI_PREP_CIF(cif,argtypes,ffi_type_float);
    {
      /*const*/ void* args[] = { &f1, &f2, &f3, &f4, &f5, &f6, &f7, &f8 };
      FFI_CALL(cif,f_f8,args,&fr);
    }
  }
  FPRINTF(out,"->%g\n",fr);
  fflush(out);
#endif

#ifdef TEST12
  fr = f_f16(f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,f13,f14,f15,f16);
  FPRINTF(out,"->%g\n",fr);
  fflush(out);
  fr = 0.0; clear_traces();
  {
    ffi_type* argtypes[] = { &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float };
    ffi_cif cif;
    FFI_PREP_CIF(cif,argtypes,ffi_type_float);
    {
      /*const*/ void* args[] = { &f1, &f2, &f3, &f4, &f5, &f6, &f7, &f8, &f9, &f10, &f11, &f12, &f13, &f14, &f15, &f16 };
      FFI_CALL(cif,f_f16,args,&fr);
    }
  }
  FPRINTF(out,"->%g\n",fr);
  fflush(out);
#endif

#ifdef TEST13
  fr = f_f24(f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,f13,f14,f15,f16,f17,f18,f19,f20,f21,f22,f23,f24);
  FPRINTF(out,"->%g\n",fr);
  fflush(out);
  fr = 0.0; clear_traces();
  {
    ffi_type* argtypes[] = { &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float };
    ffi_cif cif;
    FFI_PREP_CIF(cif,argtypes,ffi_type_float);
    {
      /*const*/ void* args[] = { &f1, &f2, &f3, &f4, &f5, &f6, &f7, &f8, &f9, &f10, &f11, &f12, &f13, &f14, &f15, &f16, &f17, &f18, &f19, &f20, &f21, &f22, &f23, &f24 };
      FFI_CALL(cif,f_f24,args,&fr);
    }
  }
  FPRINTF(out,"->%g\n",fr);
  fflush(out);
#endif
}
void
  double_tests (void)
{
  double dr;

#ifdef TEST14
  
  FPRINTF(out,"->%g\n",dr);
  fflush(out);
  dr = 0.0; clear_traces();
  {
    ffi_type* argtypes[] = { &ffi_type_double };
    ffi_cif cif;
    FFI_PREP_CIF(cif,argtypes,ffi_type_double);
    {
      /*const*/ void* args[] = { &d1 };
      FFI_CALL(cif,d_d,args,&dr);
    }
  }
  FPRINTF(out,"->%g\n",dr);
  fflush(out);
#endif

#ifdef TEST15
  dr = d_d2(d1,d2);
  FPRINTF(out,"->%g\n",dr);
  fflush(out);
  dr = 0.0; clear_traces();
  {
    ffi_type* argtypes[] = { &ffi_type_double, &ffi_type_double };
    ffi_cif cif;
    FFI_PREP_CIF(cif,argtypes,ffi_type_double);
    {
      /*const*/ void* args[] = { &d1, &d2 };
      FFI_CALL(cif,d_d2,args,&dr);
    }
  }
  FPRINTF(out,"->%g\n",dr);
  fflush(out);
#endif

#ifdef TEST16
  dr = d_d4(d1,d2,d3,d4);
  FPRINTF(out,"->%g\n",dr);
  fflush(out);
  dr = 0.0; clear_traces();
  {
    ffi_type* argtypes[] = { &ffi_type_double, &ffi_type_double, &ffi_type_double, &ffi_type_double };
    ffi_cif cif;
    FFI_PREP_CIF(cif,argtypes,ffi_type_double);
    {
      /*const*/ void* args[] = { &d1, &d2, &d3, &d4 };
      FFI_CALL(cif,d_d4,args,&dr);
    }
  }
  FPRINTF(out,"->%g\n",dr);
  fflush(out);
#endif

#ifdef TEST17
  dr = d_d8(d1,d2,d3,d4,d5,d6,d7,d8);
  FPRINTF(out,"->%g\n",dr);
  fflush(out);
  dr = 0.0; clear_traces();
  {
    ffi_type* argtypes[] = { &ffi_type_double, &ffi_type_double, &ffi_type_double, &ffi_type_double, &ffi_type_double, &ffi_type_double, &ffi_type_double, &ffi_type_double };
    ffi_cif cif;
    FFI_PREP_CIF(cif,argtypes,ffi_type_double);
    {
      /*const*/ void* args[] = { &d1, &d2, &d3, &d4, &d5, &d6, &d7, &d8 };
      FFI_CALL(cif,d_d8,args,&dr);
    }
  }
  FPRINTF(out,"->%g\n",dr);
  fflush(out);
#endif

#ifdef TEST18
  dr = d_d16(d1,d2,d3,d4,d5,d6,d7,d8,d9,d10,d11,d12,d13,d14,d15,d16);
  FPRINTF(out,"->%g\n",dr);
  fflush(out);
  dr = 0.0; clear_traces();
  {
    ffi_type* argtypes[] = { &ffi_type_double, &ffi_type_double, &ffi_type_double, &ffi_type_double, &ffi_type_double, &ffi_type_double, &ffi_type_double, &ffi_type_double, &ffi_type_double, &ffi_type_double, &ffi_type_double, &ffi_type_double, &ffi_type_double, &ffi_type_double, &ffi_type_double, &ffi_type_double };
    ffi_cif cif;
    FFI_PREP_CIF(cif,argtypes,ffi_type_double);
    {
      /*const*/ void* args[] = { &d1, &d2, &d3, &d4, &d5, &d6, &d7, &d8, &d9, &d10, &d11, &d12, &d13, &d14, &d15, &d16 };
      FFI_CALL(cif,d_d16,args,&dr);
    }
  }
  FPRINTF(out,"->%g\n",dr);
  fflush(out);
#endif  
  return;
}
void
  pointer_tests (void)
{
  void* vpr;

#ifdef TEST19
  FPRINTF(out,"->0x%p\n",vpr);
  fflush(out);
  vpr = 0; clear_traces();
  {
    ffi_type* argtypes[] = { &ffi_type_pointer, &ffi_type_pointer, &ffi_type_pointer, &ffi_type_pointer };
    ffi_cif cif;
    FFI_PREP_CIF(cif,argtypes,ffi_type_pointer);
    {
      void* puc1 = &uc1;
      void* pd2 = &d2;
      void* pstr3 = str3;
      void* pI4 = &I4;
      /*const*/ void* args[] = { &puc1, &pd2, &pstr3, &pI4 };
      FFI_CALL(cif,vp_vpdpcpsp,args,&vpr);
    }
  }
  FPRINTF(out,"->0x%p\n",vpr);
  fflush(out);
#endif  
  return;
}
void
  mixed_number_tests (void)
{
  uchar ucr;
  ushort usr;
  float fr;
  double dr;
  long long llr;

  /* Unsigned types.
   */
#ifdef TEST20
  ucr = uc_ucsil(uc1, us2, ui3, ul4);
  FPRINTF(out,"->%u\n",ucr);
  fflush(out);
  ucr = 0; clear_traces();
  {
    ffi_type* argtypes[] = { &ffi_type_uchar, &ffi_type_ushort, &ffi_type_uint, &ffi_type_ulong };
    ffi_cif cif;
    FFI_PREP_CIF(cif,argtypes,ffi_type_uchar);
    {
      ffi_arg rint;
      /*const*/ void* args[] = { &uc1, &us2, &ui3, &ul4 };
      FFI_CALL(cif,uc_ucsil,args,&rint);
      ucr = (uchar) rint;
    }
  }
  FPRINTF(out,"->%u\n",ucr);
  fflush(out);
#endif

#ifdef TEST21
  /* Mixed int & float types.
   */
  dr = d_iidd(i1,i2,d3,d4);
  FPRINTF(out,"->%g\n",dr);
  fflush(out);
  dr = 0.0; clear_traces();
  {
    ffi_type* argtypes[] = { &ffi_type_sint, &ffi_type_sint, &ffi_type_double, &ffi_type_double };
    ffi_cif cif;
    FFI_PREP_CIF(cif,argtypes,ffi_type_double);
    {
      /*const*/ void* args[] = { &i1, &i2, &d3, &d4 };
      FFI_CALL(cif,d_iidd,args,&dr);
    }
  }
  FPRINTF(out,"->%g\n",dr);
  fflush(out);
#endif

#ifdef TEST22
  dr = d_iiidi(i1,i2,i3,d4,i5);
  FPRINTF(out,"->%g\n",dr);
  fflush(out);
  dr = 0.0; clear_traces();
  {
    ffi_type* argtypes[] = { &ffi_type_sint, &ffi_type_sint, &ffi_type_sint, &ffi_type_double, &ffi_type_sint };
    ffi_cif cif;
    FFI_PREP_CIF(cif,argtypes,ffi_type_double);
    {
      /*const*/ void* args[] = { &i1, &i2, &i3, &d4, &i5 };
      FFI_CALL(cif,d_iiidi,args,&dr);
    }
  }
  FPRINTF(out,"->%g\n",dr);
  fflush(out);
#endif

#ifdef TEST23
  dr = d_idid(i1,d2,i3,d4);
  FPRINTF(out,"->%g\n",dr);
  fflush(out);
  dr = 0.0; clear_traces();
  {
    ffi_type* argtypes[] = { &ffi_type_sint, &ffi_type_double, &ffi_type_sint, &ffi_type_double };
    ffi_cif cif;
    FFI_PREP_CIF(cif,argtypes,ffi_type_double);
    {
      /*const*/ void* args[] = { &i1, &d2, &i3, &d4 };
      FFI_CALL(cif,d_idid,args,&dr);
    }
  }
  FPRINTF(out,"->%g\n",dr);
  fflush(out);
#endif

#ifdef TEST24
  dr = d_fdi(f1,d2,i3);
  FPRINTF(out,"->%g\n",dr);
  fflush(out);
  dr = 0.0; clear_traces();
  {
    ffi_type* argtypes[] = { &ffi_type_float, &ffi_type_double, &ffi_type_sint };
    ffi_cif cif;
    FFI_PREP_CIF(cif,argtypes,ffi_type_double);
    {
      /*const*/ void* args[] = { &f1, &d2, &i3 };
      FFI_CALL(cif,d_fdi,args,&dr);
    }
  }
  FPRINTF(out,"->%g\n",dr);
  fflush(out);
#endif

#ifdef TEST25
  usr = us_cdcd(c1,d2,c3,d4);
  FPRINTF(out,"->%u\n",usr);
  fflush(out);
  usr = 0; clear_traces();
  {
    ffi_type* argtypes[] = { &ffi_type_char, &ffi_type_double, &ffi_type_char, &ffi_type_double };
    ffi_cif cif;
    FFI_PREP_CIF(cif,argtypes,ffi_type_ushort);
    {
      ffi_arg rint;
      /*const*/ void* args[] = { &c1, &d2, &c3, &d4 };
      FFI_CALL(cif,us_cdcd,args,&rint);
      usr = (ushort) rint;
    }
  }
  FPRINTF(out,"->%u\n",usr);
  fflush(out);
#endif

#ifdef TEST26
  /* Long long types.
   */
  llr = ll_iiilli(i1,i2,i3,ll1,i13);
  FPRINTF(out,"->0x%lx%08lx\n",(long)(llr>>32),(long)(llr&0xffffffff));
  fflush(out);
  llr = 0; clear_traces();
  {
    ffi_type* argtypes[] = { &ffi_type_sint, &ffi_type_sint, &ffi_type_sint, &ffi_type_slonglong, &ffi_type_sint };
    ffi_cif cif;
    FFI_PREP_CIF(cif,argtypes,ffi_type_slonglong);
    {
      /*const*/ void* args[] = { &i1, &i2, &i3, &ll1, &i13 };
      FFI_CALL(cif,ll_iiilli,args,&llr);
    }
  }
  FPRINTF(out,"->0x%lx%08lx\n",(long)(llr>>32),(long)(llr&0xffffffff));
  fflush(out);
#endif

#ifdef TEST27
  llr = ll_flli(f13,ll1,i13);
  FPRINTF(out,"->0x%lx%08lx\n",(long)(llr>>32),(long)(llr&0xffffffff));
  fflush(out);
  llr = 0; clear_traces();
  {
    ffi_type* argtypes[] = { &ffi_type_float, &ffi_type_slonglong, &ffi_type_sint };
    ffi_cif cif;
    FFI_PREP_CIF(cif,argtypes,ffi_type_slonglong);
    {
      /*const*/ void* args[] = { &f13, &ll1, &i13 };
      FFI_CALL(cif,ll_flli,args,&llr);
    }
  }
  FPRINTF(out,"->0x%lx%08lx\n",(long)(llr>>32),(long)(llr&0xffffffff));
  fflush(out);
#endif

#ifdef TEST28
  fr = f_fi(f1,i9);
  FPRINTF(out,"->%g\n",fr);
  fflush(out);
  fr = 0.0; clear_traces();
  {
    ffi_type* argtypes[] = { &ffi_type_float, &ffi_type_sint };
    ffi_cif cif;
    FFI_PREP_CIF(cif,argtypes,ffi_type_float);
    {
      /*const*/ void* args[] = { &f1, &i9 };
      FFI_CALL(cif,f_fi,args,&fr);
    }
  }
  FPRINTF(out,"->%g\n",fr);
  fflush(out);
#endif

#ifdef TEST29
  fr = f_f2i(f1,f2,i9);
  FPRINTF(out,"->%g\n",fr);
  fflush(out);
  fr = 0.0; clear_traces();
  {
    ffi_type* argtypes[] = { &ffi_type_float, &ffi_type_float, &ffi_type_sint };
    ffi_cif cif;
    FFI_PREP_CIF(cif,argtypes,ffi_type_float);
    {
      /*const*/ void* args[] = { &f1, &f2, &i9 };
      FFI_CALL(cif,f_f2i,args,&fr);
    }
  }
  FPRINTF(out,"->%g\n",fr);
  fflush(out);
#endif

#ifdef TEST30
  fr = f_f3i(f1,f2,f3,i9);
  FPRINTF(out,"->%g\n",fr);
  fflush(out);
  fr = 0.0; clear_traces();
  {
    ffi_type* argtypes[] = { &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_sint };
    ffi_cif cif;
    FFI_PREP_CIF(cif,argtypes,ffi_type_float);
    {
      /*const*/ void* args[] = { &f1, &f2, &f3, &i9 };
      FFI_CALL(cif,f_f3i,args,&fr);
    }
  }
  FPRINTF(out,"->%g\n",fr);
  fflush(out);
#endif

#ifdef TEST31
  fr = f_f4i(f1,f2,f3,f4,i9);
  FPRINTF(out,"->%g\n",fr);
  fflush(out);
  fr = 0.0; clear_traces();
  {
    ffi_type* argtypes[] = { &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_sint };
    ffi_cif cif;
    FFI_PREP_CIF(cif,argtypes,ffi_type_float);
    {
      /*const*/ void* args[] = { &f1, &f2, &f3, &f4, &i9 };
      FFI_CALL(cif,f_f4i,args,&fr);
    }
  }
  FPRINTF(out,"->%g\n",fr);
  fflush(out);
#endif

#ifdef TEST32
  fr = f_f7i(f1,f2,f3,f4,f5,f6,f7,i9);
  FPRINTF(out,"->%g\n",fr);
  fflush(out);
  fr = 0.0; clear_traces();
  {
    ffi_type* argtypes[] = { &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_sint };
    ffi_cif cif;
    FFI_PREP_CIF(cif,argtypes,ffi_type_float);
    {
      /*const*/ void* args[] = { &f1, &f2, &f3, &f4, &f5, &f6, &f7, &i9 };
      FFI_CALL(cif,f_f7i,args,&fr);
    }
  }
  FPRINTF(out,"->%g\n",fr);
  fflush(out);
#endif

#ifdef TEST33
  fr = f_f8i(f1,f2,f3,f4,f5,f6,f7,f8,i9);
  FPRINTF(out,"->%g\n",fr);
  fflush(out);
  fr = 0.0; clear_traces();
  {
    ffi_type* argtypes[] = { &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_sint };
    ffi_cif cif;
    FFI_PREP_CIF(cif,argtypes,ffi_type_float);
    {
      /*const*/ void* args[] = { &f1, &f2, &f3, &f4, &f5, &f6, &f7, &f8, &i9 };
      FFI_CALL(cif,f_f8i,args,&fr);
    }
  }
  FPRINTF(out,"->%g\n",fr);
  fflush(out);
#endif

#ifdef TEST34
  fr = f_f12i(f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,i9);
  FPRINTF(out,"->%g\n",fr);
  fflush(out);
  fr = 0.0; clear_traces();
  {
    ffi_type* argtypes[] = { &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_sint };
    ffi_cif cif;
    FFI_PREP_CIF(cif,argtypes,ffi_type_float);
    {
      /*const*/ void* args[] = { &f1, &f2, &f3, &f4, &f5, &f6, &f7, &f8, &f9, &f10, &f11, &f12, &i9 };
      FFI_CALL(cif,f_f12i,args,&fr);
    }
  }
  FPRINTF(out,"->%g\n",fr);
  fflush(out);
#endif

#ifdef TEST35
  fr = f_f13i(f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,f13,i9);
  FPRINTF(out,"->%g\n",fr);
  fflush(out);
  fr = 0.0; clear_traces();
  {
    ffi_type* argtypes[] = { &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_sint };
    ffi_cif cif;
    FFI_PREP_CIF(cif,argtypes,ffi_type_float);
    {
      /*const*/ void* args[] = { &f1, &f2, &f3, &f4, &f5, &f6, &f7, &f8, &f9, &f10, &f11, &f12, &f13, &i9 };
      FFI_CALL(cif,f_f13i,args,&fr);
    }
  }
  FPRINTF(out,"->%g\n",fr);
  fflush(out);
#endif

#ifdef TEST36
  dr = d_di(d1,i9);
  FPRINTF(out,"->%g\n",dr);
  fflush(out);
  dr = 0.0; clear_traces();
  {
    ffi_type* argtypes[] = { &ffi_type_double, &ffi_type_sint };
    ffi_cif cif;
    FFI_PREP_CIF(cif,argtypes,ffi_type_double);
    {
      /*const*/ void* args[] = { &d1, &i9 };
      FFI_CALL(cif,d_di,args,&dr);
    }
  }
  FPRINTF(out,"->%g\n",dr);
  fflush(out);
#endif

#ifdef TEST37
  dr = d_d2i(d1,d2,i9);
  FPRINTF(out,"->%g\n",dr);
  fflush(out);
  dr = 0.0; clear_traces();
  {
    ffi_type* argtypes[] = { &ffi_type_double, &ffi_type_double, &ffi_type_sint };
    ffi_cif cif;
    FFI_PREP_CIF(cif,argtypes,ffi_type_double);
    {
      /*const*/ void* args[] = { &d1, &d2, &i9 };
      FFI_CALL(cif,d_d2i,args,&dr);
    }
  }
  FPRINTF(out,"->%g\n",dr);
  fflush(out);
#endif

#ifdef TEST38
  dr = d_d3i(d1,d2,d3,i9);
  FPRINTF(out,"->%g\n",dr);
  fflush(out);
  dr = 0.0; clear_traces();
  {
    ffi_type* argtypes[] = { &ffi_type_double, &ffi_type_double, &ffi_type_double, &ffi_type_sint };
    ffi_cif cif;
    FFI_PREP_CIF(cif,argtypes,ffi_type_double);
    {
      /*const*/ void* args[] = { &d1, &d2, &d3, &i9 };
      FFI_CALL(cif,d_d3i,args,&dr);
    }
  }
  FPRINTF(out,"->%g\n",dr);
  fflush(out);
#endif

#ifdef TEST39
  dr = d_d4i(d1,d2,d3,d4,i9);
  FPRINTF(out,"->%g\n",dr);
  fflush(out);
  dr = 0.0; clear_traces();
  {
    ffi_type* argtypes[] = { &ffi_type_double, &ffi_type_double, &ffi_type_double, &ffi_type_double, &ffi_type_sint };
    ffi_cif cif;
    FFI_PREP_CIF(cif,argtypes,ffi_type_double);
    {
      /*const*/ void* args[] = { &d1, &d2, &d3, &d4, &i9 };
      FFI_CALL(cif,d_d4i,args,&dr);
    }
  }
  FPRINTF(out,"->%g\n",dr);
  fflush(out);
#endif

#ifdef TEST40
  dr = d_d7i(d1,d2,d3,d4,d5,d6,d7,i9);
  FPRINTF(out,"->%g\n",dr);
  fflush(out);
  dr = 0.0; clear_traces();
  {
    ffi_type* argtypes[] = { &ffi_type_double, &ffi_type_double, &ffi_type_double, &ffi_type_double, &ffi_type_double, &ffi_type_double, &ffi_type_double, &ffi_type_sint };
    ffi_cif cif;
    FFI_PREP_CIF(cif,argtypes,ffi_type_double);
    {
      /*const*/ void* args[] = { &d1, &d2, &d3, &d4, &d5, &d6, &d7, &i9 };
      FFI_CALL(cif,d_d7i,args,&dr);
    }
  }
  FPRINTF(out,"->%g\n",dr);
  fflush(out);
#endif

#ifdef TEST41
  dr = d_d8i(d1,d2,d3,d4,d5,d6,d7,d8,i9);
  FPRINTF(out,"->%g\n",dr);
  fflush(out);
  dr = 0.0; clear_traces();
  {
    ffi_type* argtypes[] = { &ffi_type_double, &ffi_type_double, &ffi_type_double, &ffi_type_double, &ffi_type_double, &ffi_type_double, &ffi_type_double, &ffi_type_double, &ffi_type_sint };
    ffi_cif cif;
    FFI_PREP_CIF(cif,argtypes,ffi_type_double);
    {
      /*const*/ void* args[] = { &d1, &d2, &d3, &d4, &d5, &d6, &d7, &d8, &i9 };
      FFI_CALL(cif,d_d8i,args,&dr);
    }
  }
  FPRINTF(out,"->%g\n",dr);
  fflush(out);
#endif

#ifdef TEST42
  dr = d_d12i(d1,d2,d3,d4,d5,d6,d7,d8,d9,d10,d11,d12,i9);
  FPRINTF(out,"->%g\n",dr);
  fflush(out);
  dr = 0.0; clear_traces();
  {
    ffi_type* argtypes[] = { &ffi_type_double, &ffi_type_double, &ffi_type_double, &ffi_type_double, &ffi_type_double, &ffi_type_double, &ffi_type_double, &ffi_type_double, &ffi_type_double, &ffi_type_double, &ffi_type_double, &ffi_type_double, &ffi_type_sint };
    ffi_cif cif;
    FFI_PREP_CIF(cif,argtypes,ffi_type_double);
    {
      /*const*/ void* args[] = { &d1, &d2, &d3, &d4, &d5, &d6, &d7, &d8, &d9, &d10, &d11, &d12, &i9 };
      FFI_CALL(cif,d_d12i,args,&dr);
    }
  }
  FPRINTF(out,"->%g\n",dr);
  fflush(out);
#endif

#ifdef TEST43
  dr = d_d13i(d1,d2,d3,d4,d5,d6,d7,d8,d9,d10,d11,d12,d13,i9);
  FPRINTF(out,"->%g\n",dr);
  fflush(out);
  dr = 0.0; clear_traces();
  {
    ffi_type* argtypes[] = { &ffi_type_double, &ffi_type_double, &ffi_type_double, &ffi_type_double, &ffi_type_double, &ffi_type_double, &ffi_type_double, &ffi_type_double, &ffi_type_double, &ffi_type_double, &ffi_type_double, &ffi_type_double, &ffi_type_double, &ffi_type_sint };
    ffi_cif cif;
    FFI_PREP_CIF(cif,argtypes,ffi_type_double);
    {
      /*const*/ void* args[] = { &d1, &d2, &d3, &d4, &d5, &d6, &d7, &d8, &d9, &d10, &d11, &d12, &d13, &i9 };
      FFI_CALL(cif,d_d13i,args,&dr);
    }
  }
  FPRINTF(out,"->%g\n",dr);
  fflush(out);
#endif  
  return;
}
void
  small_structure_return_tests (void)
{
#ifdef TEST44
  {
    Size1 r = S1_v();
    FPRINTF(out,"->{%c}\n",r.x1);
    fflush(out);
    memset(&r,0,sizeof(r)); clear_traces();
    {
      ffi_type* ffi_type_Size1_elements[] = { &ffi_type_char, NULL };
      ffi_type ffi_type_Size1;
      ffi_type_Size1.type = FFI_TYPE_STRUCT;
      ffi_type_Size1.size = sizeof(Size1);
      ffi_type_Size1.alignment = alignof_slot(Size1);
      ffi_type_Size1.elements = ffi_type_Size1_elements;
      ffi_cif cif;
      FFI_PREP_CIF_NOARGS(cif,ffi_type_Size1);
      {
        FFI_CALL(cif,S1_v,NULL,&r);
      }
    }
    FPRINTF(out,"->{%c}\n",r.x1);
    fflush(out);
  }
#endif
#ifdef TEST45
  {
    Size2 r = S2_v();
    FPRINTF(out,"->{%c%c}\n",r.x1,r.x2);
    fflush(out);
    memset(&r,0,sizeof(r)); clear_traces();
    {
      ffi_type* ffi_type_Size2_elements[] = { &ffi_type_char, &ffi_type_char, NULL };
      ffi_type ffi_type_Size2;
      ffi_type_Size2.type = FFI_TYPE_STRUCT;
      ffi_type_Size2.size = sizeof(Size2);
      ffi_type_Size2.alignment = alignof_slot(Size2);
      ffi_type_Size2.elements = ffi_type_Size2_elements;
      ffi_cif cif;
      FFI_PREP_CIF_NOARGS(cif,ffi_type_Size2);
      {
        FFI_CALL(cif,S2_v,NULL,&r);
      }
    }
    FPRINTF(out,"->{%c%c}\n",r.x1,r.x2);
    fflush(out);
  }
#endif
#ifdef TEST46
  {
    Size3 r = S3_v();
    FPRINTF(out,"->{%c%c%c}\n",r.x1,r.x2,r.x3);
    fflush(out);
    memset(&r,0,sizeof(r)); clear_traces();
    {
      ffi_type* ffi_type_Size3_elements[] = { &ffi_type_char, &ffi_type_char, &ffi_type_char, NULL };
      ffi_type ffi_type_Size3;
      ffi_type_Size3.type = FFI_TYPE_STRUCT;
      ffi_type_Size3.size = sizeof(Size3);
      ffi_type_Size3.alignment = alignof_slot(Size3);
      ffi_type_Size3.elements = ffi_type_Size3_elements;
      ffi_cif cif;
      FFI_PREP_CIF_NOARGS(cif,ffi_type_Size3);
      {
        FFI_CALL(cif,S3_v,NULL,&r);
      }
    }
    FPRINTF(out,"->{%c%c%c}\n",r.x1,r.x2,r.x3);
    fflush(out);
  }
#endif
#ifdef TEST47
  {
    Size4 r = S4_v();
    FPRINTF(out,"->{%c%c%c%c}\n",r.x1,r.x2,r.x3,r.x4);
    fflush(out);
    memset(&r,0,sizeof(r)); clear_traces();
    {
      ffi_type* ffi_type_Size4_elements[] = { &ffi_type_char, &ffi_type_char, &ffi_type_char, &ffi_type_char, NULL };
      ffi_type ffi_type_Size4;
      ffi_type_Size4.type = FFI_TYPE_STRUCT;
      ffi_type_Size4.size = sizeof(Size4);
      ffi_type_Size4.alignment = alignof_slot(Size4);
      ffi_type_Size4.elements = ffi_type_Size4_elements;
      ffi_cif cif;
      FFI_PREP_CIF_NOARGS(cif,ffi_type_Size4);
      {
        FFI_CALL(cif,S4_v,NULL,&r);
      }
    }
    FPRINTF(out,"->{%c%c%c%c}\n",r.x1,r.x2,r.x3,r.x4);
    fflush(out);
  }
#endif
#ifdef TEST48
  {
    Size7 r = S7_v();
    FPRINTF(out,"->{%c%c%c%c%c%c%c}\n",r.x1,r.x2,r.x3,r.x4,r.x5,r.x6,r.x7);
    fflush(out);
    memset(&r,0,sizeof(r)); clear_traces();
    {
      ffi_type* ffi_type_Size7_elements[] = { &ffi_type_char, &ffi_type_char, &ffi_type_char, &ffi_type_char, &ffi_type_char, &ffi_type_char, &ffi_type_char, NULL };
      ffi_type ffi_type_Size7;
      ffi_type_Size7.type = FFI_TYPE_STRUCT;
      ffi_type_Size7.size = sizeof(Size7);
      ffi_type_Size7.alignment = alignof_slot(Size7);
      ffi_type_Size7.elements = ffi_type_Size7_elements;
      ffi_cif cif;
      FFI_PREP_CIF_NOARGS(cif,ffi_type_Size7);
      {
        FFI_CALL(cif,S7_v,NULL,&r);
      }
    }
    FPRINTF(out,"->{%c%c%c%c%c%c%c}\n",r.x1,r.x2,r.x3,r.x4,r.x5,r.x6,r.x7);
    fflush(out);
  }
#endif
#ifdef TEST49
  {
    Size8 r = S8_v();
    FPRINTF(out,"->{%c%c%c%c%c%c%c%c}\n",r.x1,r.x2,r.x3,r.x4,r.x5,r.x6,r.x7,r.x8);
    fflush(out);
    memset(&r,0,sizeof(r)); clear_traces();
    {
      ffi_type* ffi_type_Size8_elements[] = { &ffi_type_char, &ffi_type_char, &ffi_type_char, &ffi_type_char, &ffi_type_char, &ffi_type_char, &ffi_type_char, &ffi_type_char, NULL };
      ffi_type ffi_type_Size8;
      ffi_type_Size8.type = FFI_TYPE_STRUCT;
      ffi_type_Size8.size = sizeof(Size8);
      ffi_type_Size8.alignment = alignof_slot(Size8);
      ffi_type_Size8.elements = ffi_type_Size8_elements;
      ffi_cif cif;
      FFI_PREP_CIF_NOARGS(cif,ffi_type_Size8);
      {
        FFI_CALL(cif,S8_v,NULL,&r);
      }
    }
    FPRINTF(out,"->{%c%c%c%c%c%c%c%c}\n",r.x1,r.x2,r.x3,r.x4,r.x5,r.x6,r.x7,r.x8);
    fflush(out);
  }
#endif
#ifdef TEST50
  {
    Size12 r = S12_v();
    FPRINTF(out,"->{%c%c%c%c%c%c%c%c%c%c%c%c}\n",r.x1,r.x2,r.x3,r.x4,r.x5,r.x6,r.x7,r.x8,r.x9,r.x10,r.x11,r.x12);
    fflush(out);
    memset(&r,0,sizeof(r)); clear_traces();
    {
      ffi_type* ffi_type_Size12_elements[] = { &ffi_type_char, &ffi_type_char, &ffi_type_char, &ffi_type_char, &ffi_type_char, &ffi_type_char, &ffi_type_char, &ffi_type_char, &ffi_type_char, &ffi_type_char, &ffi_type_char, &ffi_type_char, NULL };
      ffi_type ffi_type_Size12;
      ffi_type_Size12.type = FFI_TYPE_STRUCT;
      ffi_type_Size12.size = sizeof(Size12);
      ffi_type_Size12.alignment = alignof_slot(Size12);
      ffi_type_Size12.elements = ffi_type_Size12_elements;
      ffi_cif cif;
      FFI_PREP_CIF_NOARGS(cif,ffi_type_Size12);
      {
        FFI_CALL(cif,S12_v,NULL,&r);
      }
    }
    FPRINTF(out,"->{%c%c%c%c%c%c%c%c%c%c%c%c}\n",r.x1,r.x2,r.x3,r.x4,r.x5,r.x6,r.x7,r.x8,r.x9,r.x10,r.x11,r.x12);
    fflush(out);
  }
#endif
#ifdef TEST51  
  {
    Size15 r = S15_v();
    FPRINTF(out,"->{%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c}\n",r.x1,r.x2,r.x3,r.x4,r.x5,r.x6,r.x7,r.x8,r.x9,r.x10,r.x11,r.x12,r.x13,r.x14,r.x15);
    fflush(out);
    memset(&r,0,sizeof(r)); clear_traces();
    {
      ffi_type* ffi_type_Size15_elements[] = { &ffi_type_char, &ffi_type_char, &ffi_type_char, &ffi_type_char, &ffi_type_char, &ffi_type_char, &ffi_type_char, &ffi_type_char, &ffi_type_char, &ffi_type_char, &ffi_type_char, &ffi_type_char, &ffi_type_char, &ffi_type_char, &ffi_type_char, NULL };
      ffi_type ffi_type_Size15;
      ffi_type_Size15.type = FFI_TYPE_STRUCT;
      ffi_type_Size15.size = sizeof(Size15);
      ffi_type_Size15.alignment = alignof_slot(Size15);
      ffi_type_Size15.elements = ffi_type_Size15_elements;
      ffi_cif cif;
      FFI_PREP_CIF_NOARGS(cif,ffi_type_Size15);
      {
        FFI_CALL(cif,S15_v,NULL,&r);
      }
    }
    FPRINTF(out,"->{%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c}\n",r.x1,r.x2,r.x3,r.x4,r.x5,r.x6,r.x7,r.x8,r.x9,r.x10,r.x11,r.x12,r.x13,r.x14,r.x15);
    fflush(out);
  }
#endif
#ifdef TEST52  
  {
    Size16 r = S16_v();
    FPRINTF(out,"->{%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c}\n",r.x1,r.x2,r.x3,r.x4,r.x5,r.x6,r.x7,r.x8,r.x9,r.x10,r.x11,r.x12,r.x13,r.x14,r.x15,r.x16);
    fflush(out);
    memset(&r,0,sizeof(r)); clear_traces();
    {
      ffi_type* ffi_type_Size16_elements[] = { &ffi_type_char, &ffi_type_char, &ffi_type_char, &ffi_type_char, &ffi_type_char, &ffi_type_char, &ffi_type_char, &ffi_type_char, &ffi_type_char, &ffi_type_char, &ffi_type_char, &ffi_type_char, &ffi_type_char, &ffi_type_char, &ffi_type_char, &ffi_type_char, NULL };
      ffi_type ffi_type_Size16;
      ffi_type_Size16.type = FFI_TYPE_STRUCT;
      ffi_type_Size16.size = sizeof(Size16);
      ffi_type_Size16.alignment = alignof_slot(Size16);
      ffi_type_Size16.elements = ffi_type_Size16_elements;
      ffi_cif cif;
      FFI_PREP_CIF_NOARGS(cif,ffi_type_Size16);
      {
        FFI_CALL(cif,S16_v,NULL,&r);
      }
    }
    FPRINTF(out,"->{%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c}\n",r.x1,r.x2,r.x3,r.x4,r.x5,r.x6,r.x7,r.x8,r.x9,r.x10,r.x11,r.x12,r.x13,r.x14,r.x15,r.x16);
    fflush(out);
  }
#endif
}
void
  structure_tests (void)
{
  Int Ir;
  Char Cr;
  Float Fr;
  Double Dr;
  J Jr;
  T Tr;
  X Xr;

#ifdef TEST53  
  Ir = I_III(I1,I2,I3);
  FPRINTF(out,"->{%d}\n",Ir.x);
  fflush(out);
  Ir.x = 0; clear_traces();
  {
    ffi_type* ffi_type_Int_elements[] = { &ffi_type_sint, NULL };
    ffi_type ffi_type_Int;
    ffi_type_Int.type = FFI_TYPE_STRUCT;
    ffi_type_Int.size = sizeof(Int);
    ffi_type_Int.alignment = alignof_slot(Int);
    ffi_type_Int.elements = ffi_type_Int_elements;
    ffi_type* argtypes[] = { &ffi_type_Int, &ffi_type_Int, &ffi_type_Int };
    ffi_cif cif;
    FFI_PREP_CIF(cif,argtypes,ffi_type_Int);
    {
      /*const*/ void* args[] = { &I1, &I2, &I3 };
      FFI_CALL(cif,I_III,args,&Ir);
    }
  }
  FPRINTF(out,"->{%d}\n",Ir.x);
  fflush(out);
#endif
#ifdef TEST54
  Cr = C_CdC(C1,d2,C3);
  FPRINTF(out,"->{'%c'}\n",Cr.x);
  fflush(out);
  Cr.x = '\0'; clear_traces();
  {
    ffi_type* ffi_type_Char_elements[] = { &ffi_type_char, NULL };
    ffi_type ffi_type_Char;
    ffi_type_Char.type = FFI_TYPE_STRUCT;
    ffi_type_Char.size = sizeof(Char);
    ffi_type_Char.alignment = alignof_slot(Char);
    ffi_type_Char.elements = ffi_type_Char_elements;
    ffi_type* argtypes[] = { &ffi_type_Char, &ffi_type_double, &ffi_type_Char };
    ffi_cif cif;
    FFI_PREP_CIF(cif,argtypes,ffi_type_Char);
    {
      /*const*/ void* args[] = { &C1, &d2, &C3 };
      FFI_CALL(cif,C_CdC,args,&Cr);
    }
  }
  FPRINTF(out,"->{'%c'}\n",Cr.x);
  fflush(out);
#endif
#ifdef TEST55
  Fr = F_Ffd(F1,f2,d3);
  FPRINTF(out,"->{%g}\n",Fr.x);
  fflush(out);
  Fr.x = 0.0; clear_traces();
  {
    ffi_type* ffi_type_Float_elements[] = { &ffi_type_float, NULL };
    ffi_type ffi_type_Float;
    ffi_type_Float.type = FFI_TYPE_STRUCT;
    ffi_type_Float.size = sizeof(Float);
    ffi_type_Float.alignment = alignof_slot(Float);
    ffi_type_Float.elements = ffi_type_Float_elements;
    ffi_type* argtypes[] = { &ffi_type_Float, &ffi_type_float, &ffi_type_double };
    ffi_cif cif;
    FFI_PREP_CIF(cif,argtypes,ffi_type_Float);
    {
      /*const*/ void* args[] = { &F1, &f2, &d3 };
      FFI_CALL(cif,F_Ffd,args,&Fr);
    }
  }
  FPRINTF(out,"->{%g}\n",Fr.x);
  fflush(out);
#endif
#ifdef TEST56  
  Dr = D_fDd(f1,D2,d3);
  FPRINTF(out,"->{%g}\n",Dr.x);
  fflush(out);
  Dr.x = 0.0; clear_traces();
  {
    ffi_type* ffi_type_Double_elements[] = { &ffi_type_double, NULL };
    ffi_type ffi_type_Double;
    ffi_type_Double.type = FFI_TYPE_STRUCT;
    ffi_type_Double.size = sizeof(Double);
    ffi_type_Double.alignment = alignof_slot(Double);
    ffi_type_Double.elements = ffi_type_Double_elements;
    ffi_type* argtypes[] = { &ffi_type_float, &ffi_type_Double, &ffi_type_double };
    ffi_cif cif;
    FFI_PREP_CIF(cif,argtypes,ffi_type_Double);
    {
      /*const*/ void* args[] = { &f1, &D2, &d3 };
      FFI_CALL(cif,D_fDd,args,&Dr);
    }
  }
  FPRINTF(out,"->{%g}\n",Dr.x);
  fflush(out);
#endif
#ifdef TEST57  
  Dr = D_Dfd(D1,f2,d3);
  FPRINTF(out,"->{%g}\n",Dr.x);
  fflush(out);
  Dr.x = 0.0; clear_traces();
  {
    ffi_type* ffi_type_Double_elements[] = { &ffi_type_double, NULL };
    ffi_type ffi_type_Double;
    ffi_type_Double.type = FFI_TYPE_STRUCT;
    ffi_type_Double.size = sizeof(Double);
    ffi_type_Double.alignment = alignof_slot(Double);
    ffi_type_Double.elements = ffi_type_Double_elements;
    ffi_type* argtypes[] = { &ffi_type_Double, &ffi_type_float, &ffi_type_double };
    ffi_cif cif;
    FFI_PREP_CIF(cif,argtypes,ffi_type_Double);
    {
      /*const*/ void* args[] = { &D1, &f2, &d3 };
      FFI_CALL(cif,D_Dfd,args,&Dr);
    }
  }
  FPRINTF(out,"->{%g}\n",Dr.x);
  fflush(out);
#endif
#ifdef TEST58  
  Jr = J_JiJ(J1,i2,J2);
  FPRINTF(out,"->{%ld,%ld}\n",Jr.l1,Jr.l2);
  fflush(out);
  Jr.l1 = Jr.l2 = 0; clear_traces();
  {
    ffi_type* ffi_type_J_elements[] = { &ffi_type_slong, &ffi_type_slong, NULL };
    ffi_type ffi_type_J;
    ffi_type_J.type = FFI_TYPE_STRUCT;
    ffi_type_J.size = sizeof(J);
    ffi_type_J.alignment = alignof_slot(J);
    ffi_type_J.elements = ffi_type_J_elements;
    ffi_type* argtypes[] = { &ffi_type_J, &ffi_type_sint, &ffi_type_J };
    ffi_cif cif;
    FFI_PREP_CIF(cif,argtypes,ffi_type_J);
    {
      /*const*/ void* args[] = { &J1, &i2, &J2 };
      FFI_CALL(cif,J_JiJ,args,&Jr);
    }
  }
  FPRINTF(out,"->{%ld,%ld}\n",Jr.l1,Jr.l2);
  fflush(out);
#endif
#ifndef SKIP_EXTRA_STRUCTS
#ifdef TEST59
  Tr = T_TcT(T1,' ',T2);
  FPRINTF(out,"->{\"%c%c%c\"}\n",Tr.c[0],Tr.c[1],Tr.c[2]);
  fflush(out);
  Tr.c[0] = Tr.c[1] = Tr.c[2] = 0; clear_traces();
  {
    ffi_type* ffi_type_T_elements[] = { ??, NULL };
    ffi_type ffi_type_T;
    ffi_type_T.type = FFI_TYPE_STRUCT;
    ffi_type_T.size = sizeof(T);
    ffi_type_T.alignment = alignof_slot(T);
    ffi_type_T.elements = ffi_type_T_elements;
    ffi_type* argtypes[] = { &ffi_type_T, &ffi_type_char, &ffi_type_T };
    ffi_cif cif;
    FFI_PREP_CIF(cif,argtypes,ffi_type_T);
    {
      char space = ' ';
      /*const*/ void* args[] = { &T1, &space, &T2 };
      FFI_CALL(cif,T_TcT,args,&Tr);
    }
  }
  FPRINTF(out,"->{\"%c%c%c\"}\n",Tr.c[0],Tr.c[1],Tr.c[2]);
  fflush(out);
#endif
#ifdef TEST60
  Xr = X_BcdB(B1,c2,d3,B2);
  FPRINTF(out,"->{\"%s\",'%c'}\n",Xr.c,Xr.c1);
  fflush(out);
  Xr.c[0]=Xr.c1='\0'; clear_traces();
  {
    ffi_type* ffi_type_X_elements[] = { ??, NULL };
    ffi_type ffi_type_X;
    ffi_type_X.type = FFI_TYPE_STRUCT;
    ffi_type_X.size = sizeof(X);
    ffi_type_X.alignment = alignof_slot(X);
    ffi_type_X.elements = ffi_type_X_elements;
    ffi_type* argtypes[] = { &ffi_type_X, &ffi_type_char, &ffi_type_double, &ffi_type_X };
    ffi_cif cif;
    FFI_PREP_CIF(cif,argtypes,ffi_type_X);
    {
      /*const*/ void* args[] = { &B1, &c2, &d3, &B2 };
      FFI_CALL(cif,X_BcdB,args,&Xr);
    }
  }
  FPRINTF(out,"->{\"%s\",'%c'}\n",Xr.c,Xr.c1);
  fflush(out);
#endif
#endif

  return;
}

void
  gpargs_boundary_tests (void)
{
  ffi_type* ffi_type_K_elements[] = { &ffi_type_slong, &ffi_type_slong, &ffi_type_slong, &ffi_type_slong, NULL };
  ffi_type ffi_type_K;
  ffi_type* ffi_type_L_elements[] = { &ffi_type_slong, &ffi_type_slong, &ffi_type_slong, &ffi_type_slong, &ffi_type_slong, &ffi_type_slong, NULL };
  ffi_type ffi_type_L;
  long lr;
  long long llr;
  float fr;
  double dr;

  ffi_type_K.type = FFI_TYPE_STRUCT;
  ffi_type_K.size = sizeof(K);
  ffi_type_K.alignment = alignof_slot(K);
  ffi_type_K.elements = ffi_type_K_elements;

  ffi_type_L.type = FFI_TYPE_STRUCT;
  ffi_type_L.size = sizeof(L);
  ffi_type_L.alignment = alignof_slot(L);
  ffi_type_L.elements = ffi_type_L_elements;

#ifdef TEST61  
  lr = l_l0K(K1,l9);
  FPRINTF(out,"->%ld\n",lr);
  fflush(out);
  lr = 0; clear_traces();
  {
    ffi_type* argtypes[] = { &ffi_type_K, &ffi_type_slong };
    ffi_cif cif;
    FFI_PREP_CIF(cif,argtypes,ffi_type_slong);
    {
      /*const*/ void* args[] = { &K1, &l9 };
      FFI_CALL(cif,l_l0K,args,&lr);
    }
  }
  FPRINTF(out,"->%ld\n",lr);
  fflush(out);
#endif
#ifdef TEST62  
  lr = l_l1K(l1,K1,l9);
  FPRINTF(out,"->%ld\n",lr);
  fflush(out);
  lr = 0; clear_traces();
  {
    ffi_type* argtypes[] = { &ffi_type_slong, &ffi_type_K, &ffi_type_slong };
    ffi_cif cif;
    FFI_PREP_CIF(cif,argtypes,ffi_type_slong);
    {
      /*const*/ void* args[] = { &l1, &K1, &l9 };
      FFI_CALL(cif,l_l1K,args,&lr);
    }
  }
  FPRINTF(out,"->%ld\n",lr);
  fflush(out);
#endif
#ifdef TEST63  
  lr = l_l2K(l1,l2,K1,l9);
  FPRINTF(out,"->%ld\n",lr);
  fflush(out);
  lr = 0; clear_traces();
  {
    ffi_type* argtypes[] = { &ffi_type_slong, &ffi_type_slong, &ffi_type_K, &ffi_type_slong };
    ffi_cif cif;
    FFI_PREP_CIF(cif,argtypes,ffi_type_slong);
    {
      /*const*/ void* args[] = { &l1, &l2, &K1, &l9 };
      FFI_CALL(cif,l_l2K,args,&lr);
    }
  }
  FPRINTF(out,"->%ld\n",lr);
  fflush(out);
#endif
#ifdef TEST64  
  lr = l_l3K(l1,l2,l3,K1,l9);
  FPRINTF(out,"->%ld\n",lr);
  fflush(out);
  lr = 0; clear_traces();
  {
    ffi_type* argtypes[] = { &ffi_type_slong, &ffi_type_slong, &ffi_type_slong, &ffi_type_K, &ffi_type_slong };
    ffi_cif cif;
    FFI_PREP_CIF(cif,argtypes,ffi_type_slong);
    {
      /*const*/ void* args[] = { &l1, &l2, &l3, &K1, &l9 };
      FFI_CALL(cif,l_l3K,args,&lr);
    }
  }
  FPRINTF(out,"->%ld\n",lr);
  fflush(out);
#endif
#ifdef TEST65
  lr = l_l4K(l1,l2,l3,l4,K1,l9);
  FPRINTF(out,"->%ld\n",lr);
  fflush(out);
  lr = 0; clear_traces();
  {
    ffi_type* argtypes[] = { &ffi_type_slong, &ffi_type_slong, &ffi_type_slong, &ffi_type_slong, &ffi_type_K, &ffi_type_slong };
    ffi_cif cif;
    FFI_PREP_CIF(cif,argtypes,ffi_type_slong);
    {
      /*const*/ void* args[] = { &l1, &l2, &l3, &l4, &K1, &l9 };
      FFI_CALL(cif,l_l4K,args,&lr);
    }
  }
  FPRINTF(out,"->%ld\n",lr);
  fflush(out);
#endif
#ifdef TEST66
  lr = l_l5K(l1,l2,l3,l4,l5,K1,l9);
  FPRINTF(out,"->%ld\n",lr);
  fflush(out);
  lr = 0; clear_traces();
  {
    ffi_type* argtypes[] = { &ffi_type_slong, &ffi_type_slong, &ffi_type_slong, &ffi_type_slong, &ffi_type_slong, &ffi_type_K, &ffi_type_slong };
    ffi_cif cif;
    FFI_PREP_CIF(cif,argtypes,ffi_type_slong);
    {
      /*const*/ void* args[] = { &l1, &l2, &l3, &l4, &l5, &K1, &l9 };
      FFI_CALL(cif,l_l5K,args,&lr);
    }
  }
  FPRINTF(out,"->%ld\n",lr);
  fflush(out);
#endif
#ifdef TEST67  
  lr = l_l6K(l1,l2,l3,l4,l5,l6,K1,l9);
  FPRINTF(out,"->%ld\n",lr);
  fflush(out);
  lr = 0; clear_traces();
  {
    ffi_type* argtypes[] = { &ffi_type_slong, &ffi_type_slong, &ffi_type_slong, &ffi_type_slong, &ffi_type_slong, &ffi_type_slong, &ffi_type_K, &ffi_type_slong };
    ffi_cif cif;
    FFI_PREP_CIF(cif,argtypes,ffi_type_slong);
    {
      /*const*/ void* args[] = { &l1, &l2, &l3, &l4, &l5, &l6, &K1, &l9 };
      FFI_CALL(cif,l_l6K,args,&lr);
    }
  }
  FPRINTF(out,"->%ld\n",lr);
  fflush(out);
#endif
#ifdef TEST68  
  fr = f_f17l3L(f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,f13,f14,f15,f16,f17,l6,l7,l8,L1);
  FPRINTF(out,"->%g\n",fr);
  fflush(out);
  fr = 0.0; clear_traces();
  {
    ffi_type* argtypes[] = { &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_float, &ffi_type_slong, &ffi_type_slong, &ffi_type_slong, &ffi_type_L };
    ffi_cif cif;
    FFI_PREP_CIF(cif,argtypes,ffi_type_float);
    {
      /*const*/ void* args[] = { &f1, &f2, &f3, &f4, &f5, &f6, &f7, &f8, &f9, &f10, &f11, &f12, &f13, &f14, &f15, &f16, &f17, &l6, &l7, &l8, &L1 };
      FFI_CALL(cif,f_f17l3L,args,&fr);
    }
  }
  FPRINTF(out,"->%g\n",fr);
  fflush(out);
#endif
#ifdef TEST69  
  dr = d_d17l3L(d1,d2,d3,d4,d5,d6,d7,d8,d9,d10,d11,d12,d13,d14,d15,d16,d17,l6,l7,l8,L1);
  FPRINTF(out,"->%g\n",dr);
  fflush(out);
  dr = 0.0; clear_traces();
  {
    ffi_type* argtypes[] = { &ffi_type_double, &ffi_type_double, &ffi_type_double, &ffi_type_double, &ffi_type_double, &ffi_type_double, &ffi_type_double, &ffi_type_double, &ffi_type_double, &ffi_type_double, &ffi_type_double, &ffi_type_double, &ffi_type_double, &ffi_type_double, &ffi_type_double, &ffi_type_double, &ffi_type_double, &ffi_type_slong, &ffi_type_slong, &ffi_type_slong, &ffi_type_L };
    ffi_cif cif;
    FFI_PREP_CIF(cif,argtypes,ffi_type_double);
    {
      /*const*/ void* args[] = { &d1, &d2, &d3, &d4, &d5, &d6, &d7, &d8, &d9, &d10, &d11, &d12, &d13, &d14, &d15, &d16, &d17, &l6, &l7, &l8, &L1 };
      FFI_CALL(cif,d_d17l3L,args,&dr);
    }
  }
  FPRINTF(out,"->%g\n",dr);
  fflush(out);
#endif
#ifdef TEST70  
  llr = ll_l2ll(l1,l2,ll1,l9);
  FPRINTF(out,"->0x%lx%08lx\n",(long)(llr>>32),(long)(llr&0xffffffff));
  fflush(out);
  llr = 0; clear_traces();
  {
    ffi_type* argtypes[] = { &ffi_type_slong, &ffi_type_slong, &ffi_type_slonglong, &ffi_type_slong };
    ffi_cif cif;
    FFI_PREP_CIF(cif,argtypes,ffi_type_slonglong);
    {
      /*const*/ void* args[] = { &l1, &l2, &ll1, &l9 };
      FFI_CALL(cif,ll_l2ll,args,&llr);
    }
  }
  FPRINTF(out,"->0x%lx%08lx\n",(long)(llr>>32),(long)(llr&0xffffffff));
  fflush(out);
#endif
#ifdef TEST71
  llr = ll_l3ll(l1,l2,l3,ll1,l9);
  FPRINTF(out,"->0x%lx%08lx\n",(long)(llr>>32),(long)(llr&0xffffffff));
  fflush(out);
  llr = 0; clear_traces();
  {
    ffi_type* argtypes[] = { &ffi_type_slong, &ffi_type_slong, &ffi_type_slong, &ffi_type_slonglong, &ffi_type_slong };
    ffi_cif cif;
    FFI_PREP_CIF(cif,argtypes,ffi_type_slonglong);
    {
      /*const*/ void* args[] = { &l1, &l2, &l3, &ll1, &l9 };
      FFI_CALL(cif,ll_l3ll,args,&llr);
    }
  }
  FPRINTF(out,"->0x%lx%08lx\n",(long)(llr>>32),(long)(llr&0xffffffff));
  fflush(out);
#endif
#ifdef TEST72  
  llr = ll_l4ll(l1,l2,l3,l4,ll1,l9);
  FPRINTF(out,"->0x%lx%08lx\n",(long)(llr>>32),(long)(llr&0xffffffff));
  fflush(out);
  llr = 0; clear_traces();
  {
    ffi_type* argtypes[] = { &ffi_type_slong, &ffi_type_slong, &ffi_type_slong, &ffi_type_slong, &ffi_type_slonglong, &ffi_type_slong };
    ffi_cif cif;
    FFI_PREP_CIF(cif,argtypes,ffi_type_slonglong);
    {
      /*const*/ void* args[] = { &l1, &l2, &l3, &l4, &ll1, &l9 };
      FFI_CALL(cif,ll_l4ll,args,&llr);
    }
  }
  FPRINTF(out,"->0x%lx%08lx\n",(long)(llr>>32),(long)(llr&0xffffffff));
  fflush(out);
#endif
#ifdef TEST73  
  llr = ll_l5ll(l1,l2,l3,l4,l5,ll1,l9);
  FPRINTF(out,"->0x%lx%08lx\n",(long)(llr>>32),(long)(llr&0xffffffff));
  fflush(out);
  llr = 0; clear_traces();
  {
    ffi_type* argtypes[] = { &ffi_type_slong, &ffi_type_slong, &ffi_type_slong, &ffi_type_slong, &ffi_type_slong, &ffi_type_slonglong, &ffi_type_slong };
    ffi_cif cif;
    FFI_PREP_CIF(cif,argtypes,ffi_type_slonglong);
    {
      /*const*/ void* args[] = { &l1, &l2, &l3, &l4, &l5, &ll1, &l9 };
      FFI_CALL(cif,ll_l5ll,args,&llr);
    }
  }
  FPRINTF(out,"->0x%lx%08lx\n",(long)(llr>>32),(long)(llr&0xffffffff));
  fflush(out);
#endif
#ifdef TEST74  
  llr = ll_l6ll(l1,l2,l3,l4,l5,l6,ll1,l9);
  FPRINTF(out,"->0x%lx%08lx\n",(long)(llr>>32),(long)(llr&0xffffffff));
  fflush(out);
  llr = 0; clear_traces();
  {
    ffi_type* argtypes[] = { &ffi_type_slong, &ffi_type_slong, &ffi_type_slong, &ffi_type_slong, &ffi_type_slong, &ffi_type_slong, &ffi_type_slonglong, &ffi_type_slong };
    ffi_cif cif;
    FFI_PREP_CIF(cif,argtypes,ffi_type_slonglong);
    {
      /*const*/ void* args[] = { &l1, &l2, &l3, &l4, &l5, &l6, &ll1, &l9 };
      FFI_CALL(cif,ll_l6ll,args,&llr);
    }
  }
  FPRINTF(out,"->0x%lx%08lx\n",(long)(llr>>32),(long)(llr&0xffffffff));
  fflush(out);
#endif
#ifdef TEST75  
  llr = ll_l7ll(l1,l2,l3,l4,l5,l6,l7,ll1,l9);
  FPRINTF(out,"->0x%lx%08lx\n",(long)(llr>>32),(long)(llr&0xffffffff));
  fflush(out);
  llr = 0; clear_traces();
  {
    ffi_type* argtypes[] = { &ffi_type_slong, &ffi_type_slong, &ffi_type_slong, &ffi_type_slong, &ffi_type_slong, &ffi_type_slong, &ffi_type_slong, &ffi_type_slonglong, &ffi_type_slong };
    ffi_cif cif;
    FFI_PREP_CIF(cif,argtypes,ffi_type_slonglong);
    {
      /*const*/ void* args[] = { &l1, &l2, &l3, &l4, &l5, &l6, &l7, &ll1, &l9 };
      FFI_CALL(cif,ll_l7ll,args,&llr);
    }
  }
  FPRINTF(out,"->0x%lx%08lx\n",(long)(llr>>32),(long)(llr&0xffffffff));
  fflush(out);
#endif
#ifdef TEST76  
  dr = d_l2d(l1,l2,d2,l9);
  FPRINTF(out,"->%g\n",dr);
  fflush(out);
  dr = 0.0; clear_traces();
  {
    ffi_type* argtypes[] = { &ffi_type_slong, &ffi_type_slong, &ffi_type_double, &ffi_type_slong };
    ffi_cif cif;
    FFI_PREP_CIF(cif,argtypes,ffi_type_double);
    {
      /*const*/ void* args[] = { &l1, &l2, &d2, &l9 };
      FFI_CALL(cif,d_l2d,args,&dr);
    }
  }
  FPRINTF(out,"->%g\n",dr);
  fflush(out);
#endif
#ifdef TEST77  
  dr = d_l3d(l1,l2,l3,d2,l9);
  FPRINTF(out,"->%g\n",dr);
  fflush(out);
  dr = 0.0; clear_traces();
  {
    ffi_type* argtypes[] = { &ffi_type_slong, &ffi_type_slong, &ffi_type_slong, &ffi_type_double, &ffi_type_slong };
    ffi_cif cif;
    FFI_PREP_CIF(cif,argtypes,ffi_type_double);
    {
      /*const*/ void* args[] = { &l1, &l2, &l3, &d2, &l9 };
      FFI_CALL(cif,d_l3d,args,&dr);
    }
  }
  FPRINTF(out,"->%g\n",dr);
  fflush(out);
#endif
#ifdef TEST78  
  dr = d_l4d(l1,l2,l3,l4,d2,l9);
  FPRINTF(out,"->%g\n",dr);
  fflush(out);
  dr = 0.0; clear_traces();
  {
    ffi_type* argtypes[] = { &ffi_type_slong, &ffi_type_slong, &ffi_type_slong, &ffi_type_slong, &ffi_type_double, &ffi_type_slong };
    ffi_cif cif;
    FFI_PREP_CIF(cif,argtypes,ffi_type_double);
    {
      /*const*/ void* args[] = { &l1, &l2, &l3, &l4, &d2, &l9 };
      FFI_CALL(cif,d_l4d,args,&dr);
    }
  }
  FPRINTF(out,"->%g\n",dr);
  fflush(out);
#endif
#ifdef TEST79  
  dr = d_l5d(l1,l2,l3,l4,l5,d2,l9);
  FPRINTF(out,"->%g\n",dr);
  fflush(out);
  dr = 0.0; clear_traces();
  {
    ffi_type* argtypes[] = { &ffi_type_slong, &ffi_type_slong, &ffi_type_slong, &ffi_type_slong, &ffi_type_slong, &ffi_type_double, &ffi_type_slong };
    ffi_cif cif;
    FFI_PREP_CIF(cif,argtypes,ffi_type_double);
    {
      /*const*/ void* args[] = { &l1, &l2, &l3, &l4, &l5, &d2, &l9 };
      FFI_CALL(cif,d_l5d,args,&dr);
    }
  }
  FPRINTF(out,"->%g\n",dr);
  fflush(out);
#endif
#ifdef TEST80  
  dr = d_l6d(l1,l2,l3,l4,l5,l6,d2,l9);
  FPRINTF(out,"->%g\n",dr);
  fflush(out);
  dr = 0.0; clear_traces();
  {
    ffi_type* argtypes[] = { &ffi_type_slong, &ffi_type_slong, &ffi_type_slong, &ffi_type_slong, &ffi_type_slong, &ffi_type_slong, &ffi_type_double, &ffi_type_slong };
    ffi_cif cif;
    FFI_PREP_CIF(cif,argtypes,ffi_type_double);
    {
      /*const*/ void* args[] = { &l1, &l2, &l3, &l4, &l5, &l6, &d2, &l9 };
      FFI_CALL(cif,d_l6d,args,&dr);
    }
  }
  FPRINTF(out,"->%g\n",dr);
  fflush(out);
#endif
#ifdef TEST81  
  dr = d_l7d(l1,l2,l3,l4,l5,l6,l7,d2,l9);
  FPRINTF(out,"->%g\n",dr);
  fflush(out);
  dr = 0.0; clear_traces();
  {
    ffi_type* argtypes[] = { &ffi_type_slong, &ffi_type_slong, &ffi_type_slong, &ffi_type_slong, &ffi_type_slong, &ffi_type_slong, &ffi_type_slong, &ffi_type_double, &ffi_type_slong };
    ffi_cif cif;
    FFI_PREP_CIF(cif,argtypes,ffi_type_double);
    {
      /*const*/ void* args[] = { &l1, &l2, &l3, &l4, &l5, &l6, &l7, &d2, &l9 };
      FFI_CALL(cif,d_l7d,args,&dr);
    }
  }
  FPRINTF(out,"->%g\n",dr);
  fflush(out);
#endif
  return;
}

int
  main (void)
{
  ffi_type_char = (char)(-1) < 0 ? ffi_type_schar : ffi_type_uchar;
  out = stdout;

  void_tests();
  int_tests();
  float_tests();
  double_tests();
  pointer_tests();
  mixed_number_tests();
  small_structure_return_tests();
  structure_tests();
  gpargs_boundary_tests();

  exit(0);
}
