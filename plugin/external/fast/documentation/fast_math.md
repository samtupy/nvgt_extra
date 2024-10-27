# Fast Math

## 1 Introduction

This document provides a quick overview of the Fast SIMD-accelerated
elementary functions in the Fast plugin. This is a quick overview
because the majority of functions are trivial to translate.

## 2 Switching to fast math

The `fast` namespace has all of the built-in elementary functions one
would expect in a math library. Here is a short table describing the
equivalents compared to those in the global namespace. Note that not all
of these functions may be implemented in the global namespace, and
therefore are "hypothetical" implementations for illustrative purposes.
The signatures are the same on both sides, so the translation process is
as simple as prepending `fast::` before any elementary function call.
There are some functions where no equivalent is provided; this is
because, at present, no accelerated version is available, or, in some
cases, no equivalent is needed. It very well may be the case that, at
the time of writing, the fast namespace has more math functions than
those provided by NVGT itself! For now, however, we'll assume that all
of the functions are available regardless. If a function is implemented
but is missing from this table, feel free to contribute or let me know;
same for if this table says a function exists when it doesn't!

| Global namespace function | Fast math equivalent | Description |
|----|----|----|
| abs | fast::abs | absolute value of a floating point value |
| fmod | fast::fmod | remainder of the floating point division operation |
| remainder | fast::remainder | signed remainder of the division operation |
| remquo | Unavailable | signed remainder as well as the three last bits of the division operation |
| fma | fast::fma | fused multiply-add operation |
| max | fast::max | larger of two floating-point values |
| min | fast::min | smaller of two floating point values |
| fdim | fast::fdim | positive difference of two floating point values |
| exp | fast::exp | returns e raised to the given power |
| exp2 | fast::exp2 | returns 2 raised to the given power |
| expm1 | fast::expm1 | returns e raised to the given power, minus one |
| log | fast::log | computes natural (base e) logarithm |
| log10 | fast::log10 | computes common (base 10) logarithm |
| log2 | fast::log2 | base 2 logarithm of the given number |
| log1p | fast::log1p | natural logarithm (to base e) of 1 plus the given number |
| pow | fast::pow | raises a number to the given power |
| sqrt | fast::sqrt | computes square root |
| cbrt | fast::cbrt | computes cube root |
| hypot | fast::hypot | computes square root of the sum of the squares of two or three given numbers |
| sin | fast::sin | computes sine |
| cos | fast::cos | computes cosine |
| tan | fast::tan | computes tangent |
| asin | fast::asin | computes arc sine |
| acos | fast::acos | computes arc cosine |
| atan | fast::atan | computes arc tangent |
| atan2 | fast::atan2 | arc tangent, using signs to determine quadrants |
| sinh | fast::sinh | computes hyperbolic sine |
| cosh | fast::cosh | computes hyperbolic cosine |
| tanh | fast::tanh | computes hyperbolic tangent |
| asinh | fast::asinh | computes the inverse hyperbolic sine |
| acosh | fast::acosh | computes the inverse hyperbolic cosine |
| atanh | fast::atanh | computes the inverse hyperbolic tangent |
| erf | fast::erf | error function |
| erfc | fast::erfc | complementary error function |
| tgamma | fast::tgamma | gamma function |
| lgamma | fast::lgamma | natural logarithm of the gamma function |
| ceil | fast::ceil | nearest integer not less than the given value |
| floor | fast::floor | nearest integer not greater than the given value |
| trunc | fast::trunc | nearest integer not greater in magnitude than the given value |
| round | fast::round | nearest integer, rounding away from zero in halfway cases |
| nearbyint | unavailable | nearest integer using current rounding mode |
| rint | fast::rint | nearest integer using current rounding mode (built-in would cause exception, this one does not) |
| frexp | fast::frfrexp, fast::expfrexp | decomposes a number into significand and base-2 exponent |
| ldexp | fast::ldexp | multiplies a number by 2 raised to an integral power |
| modf | unavailable | decomposes a number into integer and fractional parts |
| ilogb | fast::ilogb | extracts exponent of the number |
| logb | unavailable | extracts exponent of the number |
| nextafter | fast::nextafter | next representable floating-point value towards the given value |
| copysign | fast::copysign | copies the sign of a floating point value |

## 3 Approximation and blending functions

Note: all functions here are in namespace `fast`.

## 3.1 blend

### Synopsis

``` angelscript
#pragma plugin fast
double blend(double a, double b, double x, double y);
```

### Description

Conditionally returns `x` or `y` depending on whether `a` is less than
`b`. On x86 processors that support SSE 4.2, this function compiles down
to the `BLENDVPD` (Variable Blend Packed Double Precision Floating-Point
Values) instruction.

### Returrns

If `a < b`, returns `x`. Otherwise, returns `y`.

## 3.2 sin_approx/cos_approx/atan_approx/atan2_approx

### Synopsis

``` angelscript
#pragma plugin fast
double sin_approx(double x);
double cos_approx(double x);
double atan_approx(double x);
double atan2_approx(double y, double x);
```

### Description

These functions compute:

- The sine of x, as if by `sin(x)`;
- The cosine of x, as if by `cos(x)`;
- The arc tangent of x, as if by `tan(x)`; or
- The arc tangent of `y/x` using the signs of arguments to determine the
  correct quadrant, as if by `atan2(y, x)`;

except that these functions do not validate the input for:

- Not-a-Number (NaN) values;
- Subnormal numbers; or
- Infinities.

The behavior is undefined if `x` (or, in the case of `atan2_approx`, `y`
and/or `x`) are NaN values, subnormal numbers, or positive or negative
infinity values. These functions shall not raise a domain or pole error
when invalid input is provided.

### Returns

If the input is not invalid, returns either the sine, cosine, or arc
tangent of the provided input. If the input is invalid, the result is
undefined.
