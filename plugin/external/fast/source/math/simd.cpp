#include "math/simd.h"

#define NVGT_PLUGIN_INCLUDE

#include "nvgt_plugin.h"

#include <sleef.h>

#if defined(__SSE4_1__) || defined(__SSE2__)
#  include <immintrin.h>
#endif

static double hypot3(double x, double y, double z) {
  return Sleef_fabs(
      Sleef_sqrt_u05(Sleef_pow_u10(x, 2.0) + Sleef_pow_u10(y, 2.0) + Sleef_pow_u10(z, 2.0)));
}

static inline double blend(double a, double b, double x, double y) {
#if defined(__SSE4_1__)
  return _mm_cvtsd_f64(
      _mm_blendv_pd(_mm_set_sd(y), _mm_set_sd(x), _mm_cmplt_sd(_mm_set_sd(a), _mm_set_sd(b))));
#elif defined(__SSE2__)
  __m128d cc = _mm_cmplt_sd(_mm_set_sd(a), _mm_set_sd(b));
  return _mm_cvtsd_f64(_mm_or_pd(_mm_and_pd(cc, _mm_set_sd(x)), _mm_andnot_pd(cc, _mm_set_sd(y))));
#else
  return a < b ? x : y;
#endif
}

// Fast arctan(x) algorithm
// Error in the order of ~2.22E-16
double fastatan(double x) {
  const double c3 = -3.3333333333331788272957396657147910445928573608398E-1;
  const double c5 = 1.9999999999746972956238266760919941589236259460449E-1;
  const double c7 = -1.4285714270985122587021010076568927615880966186523E-1;
  const double c9 = 1.1111110670649392007103273272150545381009578704834E-1;
  const double c11 = -9.0909011195370925673131523581105284392833709716797e-2;
  const double c13 = 7.6922118180920429075797528639668598771095275878906e-2;
  const double c15 = -6.6658528038443493057840782967105042189359664916992e-2;
  const double c17 = 5.8772701139760089028563072588440263643860816955566e-2;
  const double c19 = -5.2390921524556287314222657869322574697434902191162e-2;
  const double c21 = 4.6735478230248365949517364015264320187270641326904e-2;
  const double c23 = -4.0917561483705074121264289033206296153366565704346e-2;
  const double c25 = 3.4052860223616393531287371843063738197088241577148e-2;
  const double c27 = -2.5807287359851206060001871378517535049468278884888e-2;
  const double c29 = 1.6958625295544118433133107259891403373330831527710e-2;
  const double c31 = -9.1701096131817233514382792236574459820985794067383e-3;
  const double c33 = 3.8481862661788874928336934289063719916157424449921e-3;
  const double c35 = -1.1612018409582773505878128261770143581088632345199e-3;
  const double c37 = 2.2237585554124372289389044432539321860531345009804e-4;
  const double c39 = -2.0191399088793571194207221441985211640712805092335e-5;
  const double z = Sleef_fabs(x);                            // z=|x|
  const double a = Sleef_fmin(z, 1.0) / Sleef_fmax(z, 1.0);  // a = 1<z ? 1/z : z
  const double s = a * a;                                    // a^2
  double p
      = a
        + a * s
              * (c3
                 + s
                       * (c5
                          + s
                                * (c7
                                   + s
                                         * (c9
                                            + s
                                                  * (c11
                                                     + s
                                                           * (c13
                                                              + s
                                                                    * (c15
                                                                       + s
                                                                             * (c17
                                                                                + s
                                                                                      * (c19
                                                                                         + s
                                                                                               * (c21
                                                                                                  + s
                                                                                                        * (c23
                                                                                                           + s
                                                                                                                 * (c25
                                                                                                                    + s
                                                                                                                          * (c27
                                                                                                                             + s
                                                                                                                                   * (c29
                                                                                                                                      + s
                                                                                                                                            * (c31
                                                                                                                                               + s
                                                                                                                                                     * (c33
                                                                                                                                                        + s
                                                                                                                                                              * (c35
                                                                                                                                                                 + s
                                                                                                                                                                       * (c37
                                                                                                                                                                          + s * c39))))))))))))))))));
  p = blend(1.0, z, 1.57079632679489661923132 - p, p);  // r = 1<z ? pi/2 - p : p
  return Sleef_copysign(p, x);
}

// Fast arctan(x) algorithm
// Error in the order of ~4.44E-16
double fastatan2(double y, double x) {
  const double c3 = -3.3333333333331788272957396657147910445928573608398E-1;
  const double c5 = 1.9999999999746972956238266760919941589236259460449E-1;
  const double c7 = -1.4285714270985122587021010076568927615880966186523E-1;
  const double c9 = 1.1111110670649392007103273272150545381009578704834E-1;
  const double c11 = -9.0909011195370925673131523581105284392833709716797e-2;
  const double c13 = 7.6922118180920429075797528639668598771095275878906e-2;
  const double c15 = -6.6658528038443493057840782967105042189359664916992e-2;
  const double c17 = 5.8772701139760089028563072588440263643860816955566e-2;
  const double c19 = -5.2390921524556287314222657869322574697434902191162e-2;
  const double c21 = 4.6735478230248365949517364015264320187270641326904e-2;
  const double c23 = -4.0917561483705074121264289033206296153366565704346e-2;
  const double c25 = 3.4052860223616393531287371843063738197088241577148e-2;
  const double c27 = -2.5807287359851206060001871378517535049468278884888e-2;
  const double c29 = 1.6958625295544118433133107259891403373330831527710e-2;
  const double c31 = -9.1701096131817233514382792236574459820985794067383e-3;
  const double c33 = 3.8481862661788874928336934289063719916157424449921e-3;
  const double c35 = -1.1612018409582773505878128261770143581088632345199e-3;
  const double c37 = 2.2237585554124372289389044432539321860531345009804e-4;
  const double c39 = -2.0191399088793571194207221441985211640712805092335e-5;
  const double absx = Sleef_fabs(x);  // |x|
  const double absy = Sleef_fabs(y);  // |y|
  const double a
      = Sleef_fmin(absx, absy) / Sleef_fmax(absx, absy);  // a = |x|<|y| ? |x|/|y| : |y|/|x|
  const double s = a * a;
  double r;
  r = a
      + a * s
            * (c3
               + s
                     * (c5
                        + s
                              * (c7
                                 + s
                                       * (c9
                                          + s
                                                * (c11
                                                   + s
                                                         * (c13
                                                            + s
                                                                  * (c15
                                                                     + s
                                                                           * (c17
                                                                              + s
                                                                                    * (c19
                                                                                       + s
                                                                                             * (c21
                                                                                                + s
                                                                                                      * (c23
                                                                                                         + s
                                                                                                               * (c25
                                                                                                                  + s
                                                                                                                        * (c27
                                                                                                                           + s
                                                                                                                                 * (c29
                                                                                                                                    + s
                                                                                                                                          * (c31
                                                                                                                                             + s
                                                                                                                                                   * (c33
                                                                                                                                                      + s
                                                                                                                                                            * (c35
                                                                                                                                                               + s
                                                                                                                                                                     * (c37
                                                                                                                                                                        + s * c39))))))))))))))))));
  r = blend(absy, absx, r, 1.57079632679489661923132 - r);
  r = blend(0.0, x, r, 3.1415926535897932384626433832795028841971693993751 - r);
  r = blend(0.0, y, r, -r);
  return r;
}

double fastsin(double x) {
  const double A = -7.28638965935448382375e-18;
  const double B = 2.79164354009975374566e-15;
  const double C = -7.64479307785677023759e-13;
  const double D = 1.60588695928966278105e-10;
  const double E = -2.50521003012188316353e-08;
  const double F = 2.75573189892671884365e-06;
  const double G = -1.98412698371840334929e-04;
  const double H = 8.33333333329438515047e-03;
  const double I = -1.66666666666649732329e-01;
  const double J = 9.99999999999997848557e-01;
  const double x2 = x * x;
  return (((((((((A * x2 + B) * x2 + C) * x2 + D) * x2 + E) * x2 + F) * x2 + G) * x2 + H) * x2 + I)
              * x2
          + J)
         * x;
}

double fastcos(double x) {
  const double c1 = 3.68396216222400477886e-19;
  const double c2 = -1.55289318377801496607e-16;
  const double c3 = 4.77840439714556611532e-14;
  const double c4 = -1.14706678499029860238e-11;
  const double c5 = 2.08767534780769871595e-09;
  const double c6 = -2.75573191273279748439e-07;
  const double c7 = 2.48015873000796780048e-05;
  const double c8 = -1.38888888888779804960e-03;
  const double c9 = 4.16666666666665603386e-02;
  const double c10 = -5.00000000000000154115e-01;
  const double c11 = 1.00000000000000001607e+00;
  const double x2 = x * x;
  return (((((((((c1 * x2 + c2) * x2 + c3) * x2 + c4) * x2 + c5) * x2 + c6) * x2 + c7) * x2 + c8)
               * x2
           + c9)
              * x2
          + c10)
             * x2
         + c11;
}

void register_simd_elementary_functions(asIScriptEngine *engine) {
  engine->SetDefaultNamespace("fast");
  // Trigonometric functions
  engine->RegisterGlobalFunction("double sin(double x)", asFUNCTION(Sleef_sin_u10), asCALL_CDECL);
  engine->RegisterGlobalFunction("double cos(double x)", asFUNCTION(Sleef_cos_u10), asCALL_CDECL);
  engine->RegisterGlobalFunction("double sinpi(double a)", asFUNCTION(Sleef_sinpi_u05),
                                 asCALL_CDECL);
  engine->RegisterGlobalFunction("double cospi(double x)", asFUNCTION(Sleef_cospi_u05),
                                 asCALL_CDECL);
  engine->RegisterGlobalFunction("double tan(double x)", asFUNCTION(Sleef_tan_u10), asCALL_CDECL);
  // Power, exponential, and logarithmic functions
  engine->RegisterGlobalFunction("double pow(double x, double y)", asFUNCTION(Sleef_pow_u10),
                                 asCALL_CDECL);
  engine->RegisterGlobalFunction("double log(double x)", asFUNCTION(Sleef_log_u10), asCALL_CDECL);
  engine->RegisterGlobalFunction("double log10(double x)", asFUNCTION(Sleef_log10_u10),
                                 asCALL_CDECL);
  engine->RegisterGlobalFunction("double log2(double x)", asFUNCTION(Sleef_log2_u10), asCALL_CDECL);
  engine->RegisterGlobalFunction("double log1p(double x)", asFUNCTION(Sleef_log1p_u10),
                                 asCALL_CDECL);
  engine->RegisterGlobalFunction("double exp(double x)", asFUNCTION(Sleef_exp_u10), asCALL_CDECL);
  engine->RegisterGlobalFunction("double exp2(double x)", asFUNCTION(Sleef_exp2_u10), asCALL_CDECL);
  engine->RegisterGlobalFunction("double exp10(double x)", asFUNCTION(Sleef_exp10_u10),
                                 asCALL_CDECL);
  engine->RegisterGlobalFunction("double expm1(double x)", asFUNCTION(Sleef_expm1_u10),
                                 asCALL_CDECL);
  engine->RegisterGlobalFunction("double sqrt(double x)", asFUNCTION(Sleef_sqrt_u05), asCALL_CDECL);
  engine->RegisterGlobalFunction("double cbrt(double x)", asFUNCTION(Sleef_cbrt_u10), asCALL_CDECL);
  engine->RegisterGlobalFunction("double hypot(double x, double y)", asFUNCTION(Sleef_hypot_u05),
                                 asCALL_CDECL);
  engine->RegisterGlobalFunction("double hypot(double x, double y, double z)", asFUNCTION(hypot3),
                                 asCALL_CDECL);
  // Inverse trigonometric functions
  engine->RegisterGlobalFunction("double asin(double x)", asFUNCTION(Sleef_asin_u10), asCALL_CDECL);
  engine->RegisterGlobalFunction("double acos(double x)", asFUNCTION(Sleef_acos_u10), asCALL_CDECL);
  engine->RegisterGlobalFunction("double atan(double x)", asFUNCTION(Sleef_atan_u10), asCALL_CDECL);
  engine->RegisterGlobalFunction("double atan2(double y, double x)", asFUNCTION(Sleef_atan2_u10),
                                 asCALL_CDECL);
  // Hyperbolic functions and inverse hyperbolic functions
  engine->RegisterGlobalFunction("double sinh(double x)", asFUNCTION(Sleef_sinh_u10), asCALL_CDECL);
  engine->RegisterGlobalFunction("double cosh(double x)", asFUNCTION(Sleef_cosh_u10), asCALL_CDECL);
  engine->RegisterGlobalFunction("double tanh(double x)", asFUNCTION(Sleef_tanh_u10), asCALL_CDECL);
  engine->RegisterGlobalFunction("double asinh(double x)", asFUNCTION(Sleef_asinh_u10),
                                 asCALL_CDECL);
  engine->RegisterGlobalFunction("double acosh(double x)", asFUNCTION(Sleef_acosh_u10),
                                 asCALL_CDECL);
  engine->RegisterGlobalFunction("double atanh(double x)", asFUNCTION(Sleef_atanh_u10),
                                 asCALL_CDECL);
  // Error and gamma functions
  engine->RegisterGlobalFunction("double erf(double x)", asFUNCTION(Sleef_erf_u10), asCALL_CDECL);
  engine->RegisterGlobalFunction("double erfc(double x)", asFUNCTION(Sleef_erfc_u15), asCALL_CDECL);
  engine->RegisterGlobalFunction("double tgamma(double x)", asFUNCTION(Sleef_tgamma_u10),
                                 asCALL_CDECL);
  engine->RegisterGlobalFunction("double lgamma(double x)", asFUNCTION(Sleef_lgamma_u10),
                                 asCALL_CDECL);
  // Nearest integer functions
  engine->RegisterGlobalFunction("double trunc(double x)", asFUNCTION(Sleef_trunc), asCALL_CDECL);
  engine->RegisterGlobalFunction("double floor(double x)", asFUNCTION(Sleef_floor), asCALL_CDECL);
  engine->RegisterGlobalFunction("double ceil(double x)", asFUNCTION(Sleef_ceil), asCALL_CDECL);
  engine->RegisterGlobalFunction("double round(double x)", asFUNCTION(Sleef_round), asCALL_CDECL);
  engine->RegisterGlobalFunction("double rint(double x)", asFUNCTION(Sleef_rint), asCALL_CDECL);
  // Other functions
  engine->RegisterGlobalFunction("double fma(double x, double y, double z)", asFUNCTION(Sleef_fma),
                                 asCALL_CDECL);
  engine->RegisterGlobalFunction("double fmod(double x, double y)", asFUNCTION(Sleef_fmod),
                                 asCALL_CDECL);
  engine->RegisterGlobalFunction("double remainder(double x, double y)",
                                 asFUNCTION(Sleef_remainder), asCALL_CDECL);
  engine->RegisterGlobalFunction("double ldexp(double n, int x)", asFUNCTION(Sleef_ldexp),
                                 asCALL_CDECL);
  engine->RegisterGlobalFunction("double frfrexp(double x)", asFUNCTION(Sleef_frfrexp),
                                 asCALL_CDECL);
  engine->RegisterGlobalFunction("int expfrexp(double x)", asFUNCTION(Sleef_expfrexp),
                                 asCALL_CDECL);
  engine->RegisterGlobalFunction("int ilogb(double x, int n)", asFUNCTION(Sleef_ilogb),
                                 asCALL_CDECL);
  engine->RegisterGlobalFunction("double abs(double x)", asFUNCTION(Sleef_fabs), asCALL_CDECL);
  engine->RegisterGlobalFunction("double max(double x, double y)", asFUNCTION(Sleef_fmax),
                                 asCALL_CDECL);
  engine->RegisterGlobalFunction("double min(double x, double y)", asFUNCTION(Sleef_fmin),
                                 asCALL_CDECL);
  engine->RegisterGlobalFunction("double fdim(double x, double y)", asFUNCTION(Sleef_fdim),
                                 asCALL_CDECL);
  engine->RegisterGlobalFunction("double copysign(double x, double y)", asFUNCTION(Sleef_copysign),
                                 asCALL_CDECL);
  engine->RegisterGlobalFunction("double nextafter(double x, double y)",
                                 asFUNCTION(Sleef_nextafter), asCALL_CDECL);
  // Fast sin/cos/atan/atan2 approximations, and blend
  engine->RegisterGlobalFunction("double blend(double a, double b, double x, double y)",
                                 asFUNCTION(blend), asCALL_CDECL);
  engine->RegisterGlobalFunction("double sin_approx(double x)", asFUNCTION(fastsin), asCALL_CDECL);
  engine->RegisterGlobalFunction("double cos_approx(double x)", asFUNCTION(fastcos), asCALL_CDECL);
  engine->RegisterGlobalFunction("double atan_approx(double x)", asFUNCTION(fastatan),
                                 asCALL_CDECL);
  engine->RegisterGlobalFunction("double atan2_approx(double y, double x)", asFUNCTION(fastatan2),
                                 asCALL_CDECL);
  engine->SetDefaultNamespace("");
}
