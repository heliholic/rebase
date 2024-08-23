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

#pragma once

#include <math.h>

/*
 * Three trig tiers, by what the caller already knows about its argument:
 *
 *   sin_fast() / cos_fast() / tan_fast() / sincos_fast()
 *     No range reduction. Valid only for |x| <= pi/4; cheapest.
 *   sin_approx() / cos_approx() / sincos_approx() / tan_approx()
 *     Quadrant reduction then the same polynomials. Valid for any x.
 *   tan_approx2()
 *     Alternative full-range tan() as a single rational polynomial, no
 *     division by a near-zero cosine.
 *
 * sincosf_approx() is the pointer-style wrapper used by existing call sites.
 */

typedef struct {
    float sin;
    float cos;
} sincosf_t;

static inline sincosf_t sinfcosf(float x)
{
    sincosf_t sc;
    sc.sin = sinf(x);
    sc.cos = cosf(x);
    return sc;
}

#ifndef USE_STANDARD_MATH

float sin_fast(float x);
float cos_fast(float x);
float tan_fast(float x);
sincosf_t sincos_fast(float x);

float sin_approx(float x);
float cos_approx(float x);
float tan_approx(float x);
float tan_approx2(float x);
sincosf_t sincos_approx(float x);
void sincosf_approx(float x, float *out_s, float *out_c);

float asin_approx(float x);
float acos_approx(float x);
float atan2_approx(float y, float x);

#else /* USE_STANDARD_MATH */

#define sin_fast(x)         sinf(x)
#define cos_fast(x)         cosf(x)
#define tan_fast(x)         tanf(x)
#define sincos_fast(x)      sinfcosf(x)

#define sin_approx(x)       sinf(x)
#define cos_approx(x)       cosf(x)
#define tan_approx(x)       tanf(x)
#define tan_approx2(x)      tanf(x)
#define sincos_approx(x)    sinfcosf(x)

#define asin_approx(x)      asinf(x)
#define acos_approx(x)      acosf(x)
#define atan2_approx(y,x)   atan2f(y,x)

static inline void sincosf_approx(float x, float *out_s, float *out_c)
{
    *out_s = sinf(x);
    *out_c = cosf(x);
}

#endif /* USE_STANDARD_MATH */
