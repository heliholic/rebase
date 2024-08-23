/*
 * This file is part of Rotorflight.
 *
 * Rotorflight is free software. You can redistribute this software
 * and/or modify this software under the terms of the GNU General
 * Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later
 * version.
 *
 * Rotorflight is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this software.
 *
 * If not, see <http://www.gnu.org/licenses/>.
 */

/// The trig approximations track libm closely enough for control use,
/// each within the range its own doc comment promises.
///
/// Reference values are libm's float overloads (sinf(), atan2f(), ...):
/// correctly rounded to within +/-1 ULP, which is precise enough to measure
/// an approximation whose own error is orders of magnitude larger.

#include <cmath>
#include <math.h>

extern "C" {
    #include "common/maths.h"
}

#include "unittest_macros.h"
#include "gtest/gtest.h"

#ifndef USE_STANDARD_MATH

namespace {

float maxAbsError(float got, float want, float prevMax)
{
    const float err = fabsf(got - want);
    return (err > prevMax) ? err : prevMax;
}

} // namespace

/*
 * sin_fast / cos_fast / tan_fast / sincos_fast  —  |x| <= pi/4
 */

TEST(TrigUnittest, FastSinCosTanTrackLibmWithinQuarterTurn)
{
    float sinErr = 0.0f, cosErr = 0.0f, tanErr = 0.0f;
    for (float a = -M_PI_4f; a <= M_PI_4f; a += 1e-4f) {
        sinErr = maxAbsError(sin_fast(a), sinf(a), sinErr);
        cosErr = maxAbsError(cos_fast(a), cosf(a), cosErr);
        tanErr = maxAbsError(tan_fast(a), tanf(a), tanErr);
    }
    EXPECT_LT(sinErr, 1e-6f);
    EXPECT_LT(cosErr, 1e-6f);
    EXPECT_LT(tanErr, 2e-6f);
}

TEST(TrigUnittest, SincosFastMatchesSinFastCosFast)
{
    for (float a = -M_PI_4f; a <= M_PI_4f; a += 1e-3f) {
        const sincosf_t sc = sincos_fast(a);
        EXPECT_EQ(sc.sin, sin_fast(a));
        EXPECT_EQ(sc.cos, cos_fast(a));
    }
}

TEST(TrigUnittest, SincosFastTracksLibmWithinQuarterTurn)
{
    float sinErr = 0.0f, cosErr = 0.0f;
    for (float a = -M_PI_4f; a <= M_PI_4f; a += 1e-4f) {
        const sincosf_t sc = sincos_fast(a);
        sinErr = maxAbsError(sc.sin, sinf(a), sinErr);
        cosErr = maxAbsError(sc.cos, cosf(a), cosErr);
    }
    EXPECT_LT(sinErr, 1e-6f);
    EXPECT_LT(cosErr, 1e-6f);
}

TEST(TrigUnittest, FastSinCosKeyValues)
{
    EXPECT_FLOAT_EQ(sin_fast(0.0f), 0.0f);
    EXPECT_FLOAT_EQ(cos_fast(0.0f), 1.0f);
    EXPECT_FLOAT_EQ(tan_fast(0.0f), 0.0f);

    EXPECT_NEAR(sin_fast(1e-5f), 1e-5f, 1e-12f);
    EXPECT_NEAR(cos_fast(1e-5f), 1.0f, 1e-9f);

    EXPECT_NEAR(sin_fast(M_PI_4f), 0.70710678f, 1e-6f);
    EXPECT_NEAR(cos_fast(M_PI_4f), 0.70710678f, 1e-6f);
    EXPECT_NEAR(tan_fast(M_PI_4f), 1.0f, 2e-6f);
}

TEST(TrigUnittest, FastSinOddCosEven)
{
    for (float a = 1e-4f; a <= M_PI_4f; a += 1e-3f) {
        EXPECT_EQ(sin_fast(-a), -sin_fast(a));
        EXPECT_EQ(cos_fast(-a), cos_fast(a));
        EXPECT_EQ(tan_fast(-a), -tan_fast(a));
    }
}

TEST(TrigUnittest, FastTanMatchesSinOverCos)
{
    for (float a = -M_PI_4f; a <= M_PI_4f; a += 1e-3f) {
        EXPECT_NEAR(tan_fast(a), sin_fast(a) / cos_fast(a), 1e-6f);
    }
}

TEST(TrigUnittest, FastPythagoreanIdentity)
{
    for (float a = -M_PI_4f; a <= M_PI_4f; a += 1e-3f) {
        const float s = sin_fast(a);
        const float c = cos_fast(a);
        EXPECT_NEAR(s * s + c * c, 1.0f, 2e-6f);
    }
}

/*
 * sin_approx / cos_approx / sincos_approx / sincosf_approx  —  any x
 */

TEST(TrigUnittest, ApproxSinCosTrackLibmOverManyTurns)
{
    float sinErr = 0.0f, cosErr = 0.0f;
    for (float a = -10.0f * M_PIf; a < 10.0f * M_PIf; a += M_PIf / 300.0f) {
        sinErr = maxAbsError(sin_approx(a), sinf(a), sinErr);
        cosErr = maxAbsError(cos_approx(a), cosf(a), cosErr);
    }
    EXPECT_LT(sinErr, 4e-6f);
    EXPECT_LT(cosErr, 4e-6f);
}

TEST(TrigUnittest, SincosApproxTracksLibmOverManyTurns)
{
    float sinErr = 0.0f, cosErr = 0.0f;
    for (float a = -10.0f * M_PIf; a < 10.0f * M_PIf; a += M_PIf / 300.0f) {
        const sincosf_t sc = sincos_approx(a);
        sinErr = maxAbsError(sc.sin, sinf(a), sinErr);
        cosErr = maxAbsError(sc.cos, cosf(a), cosErr);
    }
    EXPECT_LT(sinErr, 4e-6f);
    EXPECT_LT(cosErr, 4e-6f);
}

TEST(TrigUnittest, SincosApproxMatchesSinApproxCosApprox)
{
    for (float a = -10.0f * M_PIf; a < 10.0f * M_PIf; a += M_PIf / 300.0f) {
        const sincosf_t sc = sincos_approx(a);
        EXPECT_EQ(sc.sin, sin_approx(a));
        EXPECT_EQ(sc.cos, cos_approx(a));
    }
}

TEST(TrigUnittest, SincosfApproxMatchesSincosApprox)
{
    for (float a = -10.0f * M_PIf; a < 10.0f * M_PIf; a += M_PIf / 50.0f) {
        float s, c;
        sincosf_approx(a, &s, &c);
        const sincosf_t sc = sincos_approx(a);
        EXPECT_EQ(s, sc.sin);
        EXPECT_EQ(c, sc.cos);
    }
}

TEST(TrigUnittest, ApproxSinCosKeyValues)
{
    EXPECT_NEAR(sin_approx(0.0f), 0.0f, 4e-6f);
    EXPECT_NEAR(cos_approx(0.0f), 1.0f, 4e-6f);

    EXPECT_NEAR(sin_approx(M_PI2f), 1.0f, 4e-6f);
    EXPECT_NEAR(cos_approx(M_PI2f), 0.0f, 4e-6f);

    EXPECT_NEAR(sin_approx(M_PIf), 0.0f, 4e-6f);
    EXPECT_NEAR(cos_approx(M_PIf), -1.0f, 4e-6f);

    EXPECT_NEAR(sin_approx(-M_PI2f), -1.0f, 4e-6f);
    EXPECT_NEAR(cos_approx(-M_PI2f), 0.0f, 4e-6f);

    EXPECT_NEAR(sin_approx(M_2PIf), 0.0f, 4e-6f);
    EXPECT_NEAR(cos_approx(M_2PIf), 1.0f, 4e-6f);
}

TEST(TrigUnittest, ApproxSinOddCosEven)
{
    for (float a = M_PIf / 300.0f; a < 10.0f * M_PIf; a += M_PIf / 50.0f) {
        EXPECT_NEAR(sin_approx(-a), -sin_approx(a), 1e-6f);
        EXPECT_NEAR(cos_approx(-a), cos_approx(a), 1e-6f);
    }
}

TEST(TrigUnittest, ApproxPythagoreanIdentity)
{
    for (float a = -10.0f * M_PIf; a < 10.0f * M_PIf; a += M_PIf / 50.0f) {
        const sincosf_t sc = sincos_approx(a);
        EXPECT_NEAR(sc.sin * sc.sin + sc.cos * sc.cos, 1.0f, 2e-5f);
    }
}

TEST(TrigUnittest, ApproxQuadrantShiftIdentities)
{
    for (float a = -4.0f * M_PIf; a < 4.0f * M_PIf; a += M_PIf / 50.0f) {
        EXPECT_NEAR(sin_approx(a + M_PI2f), cos_approx(a), 1e-5f);
        EXPECT_NEAR(cos_approx(a + M_PI2f), -sin_approx(a), 1e-5f);
        EXPECT_NEAR(sin_approx(a + M_PIf), -sin_approx(a), 1e-5f);
        EXPECT_NEAR(cos_approx(a + M_PIf), -cos_approx(a), 1e-5f);
    }
}

/*
 * tan_approx / tan_approx2
 */

TEST(TrigUnittest, ApproxTanTracksLibmAcrossTheFoldedRange)
{
    float tan1Err = 0.0f, tan2Err = 0.0f;
    for (float a = -1.4f; a <= 1.4f; a += M_PIf / 800.0f) {
        const float ref = tanf(a);
        if (!std::isfinite(ref) || fabsf(ref) > 1e3f) {
            continue;
        }
        tan1Err = maxAbsError(tan_approx(a), ref, tan1Err);
        tan2Err = maxAbsError(tan_approx2(a), ref, tan2Err);
    }
    EXPECT_LT(tan1Err, 3e-5f);
    EXPECT_LT(tan2Err, 3e-5f);
}

TEST(TrigUnittest, ApproxTanCoreRange)
{
    const float tol = 1.0f / (float)(1u << 20);
    float maxErr = 0.0f;
    for (float a = -0.78f; a <= 0.78f; a += 1e-4f) {
        maxErr = maxAbsError(tan_approx(a), tanf(a), maxErr);
    }
    EXPECT_LT(maxErr, tol);
}

TEST(TrigUnittest, ApproxTanOdd)
{
    for (float a = 0.02f; a <= 1.4f; a += 0.05f) {
        const float ref = tanf(a);
        if (!std::isfinite(ref) || fabsf(ref) > 1e3f) {
            continue;
        }
        EXPECT_NEAR(tan_approx(-a), -tan_approx(a), 1e-5f);
        EXPECT_NEAR(tan_approx2(-a), -tan_approx2(a), 1e-5f);
    }
}

TEST(TrigUnittest, ApproxTanZero)
{
    EXPECT_NEAR(tan_approx(0.0f), 0.0f, 1e-6f);
    EXPECT_NEAR(tan_approx2(0.0f), 0.0f, 1e-6f);
}

/*
 * asin_approx / acos_approx
 */

TEST(TrigUnittest, AsinAcosApproxTrackLibmAndSumToHalfPi)
{
    float asinErr = 0.0f, acosErr = 0.0f;
    for (float x = -1.0f; x <= 1.0f; x += 1e-3f) {
        asinErr = maxAbsError(asin_approx(x), asinf(x), asinErr);
        acosErr = maxAbsError(acos_approx(x), acosf(x), acosErr);
        EXPECT_LT(fabsf((asin_approx(x) + acos_approx(x)) - M_PI2f), 1e-6f);
    }
    EXPECT_LT(asinErr, 2e-4f);
    EXPECT_LT(acosErr, 2e-4f);
}

TEST(TrigUnittest, AsinAcosApproxKeyValues)
{
    const float tol = 2.5e-4f;
    const float xs[] = {
        -1.0f, -0.8660254f, -0.70710678f, -0.5f, -0.258819f, 0.0f,
        0.258819f, 0.5f, 0.70710678f, 0.8660254f, 1.0f
    };
    for (float x : xs) {
        EXPECT_NEAR(asin_approx(x), asinf(x), tol);
        EXPECT_NEAR(acos_approx(x), acosf(x), tol);
    }

    EXPECT_NEAR(asin_approx(0.0f), 0.0f, tol);
    EXPECT_NEAR(acos_approx(0.0f), M_PI2f, tol);
    EXPECT_NEAR(asin_approx(1.0f), M_PI2f, tol);
    EXPECT_NEAR(acos_approx(1.0f), 0.0f, tol);
    EXPECT_NEAR(asin_approx(-1.0f), -M_PI2f, tol);
    EXPECT_NEAR(acos_approx(-1.0f), M_PIf, tol);
}

TEST(TrigUnittest, AsinAcosApproxSymmetry)
{
    const float tol = 2.5e-4f;
    for (float x = 1.0f / 64.0f; x <= 1.0f; x += 1.0f / 64.0f) {
        EXPECT_NEAR(asin_approx(-x), -asin_approx(x), tol);
        EXPECT_NEAR(acos_approx(-x), M_PIf - acos_approx(x), tol);
    }
}

TEST(TrigUnittest, AsinAcosApproxMonotonic)
{
    float prevAsin = asin_approx(-1.0f);
    float prevAcos = acos_approx(-1.0f);
    for (float x = -1.0f + 1.0f / 128.0f; x <= 1.0f; x += 1.0f / 128.0f) {
        const float a = asin_approx(x);
        const float c = acos_approx(x);
        EXPECT_GE(a, prevAsin);
        EXPECT_LE(c, prevAcos);
        prevAsin = a;
        prevAcos = c;
    }
}

/*
 * atan2_approx
 */

TEST(TrigUnittest, Atan2ApproxTracksLibm)
{
    float err = 0.0f;
    for (float x = -1.0f; x <= 1.0f; x += 0.02f) {
        for (float y = -1.0f; y <= 1.0f; y += 0.02f) {
            if (x == 0.0f && y == 0.0f) {
                continue;
            }
            err = maxAbsError(atan2_approx(y, x), atan2f(y, x), err);
        }
    }
    EXPECT_LT(err, 2e-6f);
}

TEST(TrigUnittest, Atan2ApproxAxes)
{
    const float tol = 2e-6f;
    EXPECT_NEAR(atan2_approx(0.0f, 1.0f), 0.0f, tol);
    EXPECT_NEAR(atan2_approx(1.0f, 0.0f), M_PI2f, tol);
    EXPECT_NEAR(atan2_approx(0.0f, -1.0f), M_PIf, tol);
    EXPECT_NEAR(atan2_approx(-1.0f, 0.0f), -M_PI2f, tol);

    EXPECT_NEAR(atan2_approx(1.0f, 1.0f), M_PI_4f, 1e-5f);
    EXPECT_NEAR(atan2_approx(1.0f, -1.0f), 3.0f * M_PI_4f, 1e-5f);
    EXPECT_NEAR(atan2_approx(-1.0f, -1.0f), -3.0f * M_PI_4f, 1e-5f);
    EXPECT_NEAR(atan2_approx(-1.0f, 1.0f), -M_PI_4f, 1e-5f);
}

TEST(TrigUnittest, Atan2ApproxOrigin)
{
    EXPECT_NEAR(atan2_approx(0.0f, 0.0f), 0.0f, 1e-6f);
}

TEST(TrigUnittest, Atan2ApproxOddInY)
{
    for (float x = -1.0f; x <= 1.0f; x += 0.1f) {
        for (float y = 0.1f; y <= 1.0f; y += 0.1f) {
            EXPECT_NEAR(atan2_approx(-y, x), -atan2_approx(y, x), 1e-5f);
        }
    }
}

#endif
