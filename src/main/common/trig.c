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

#include <math.h>
#include <stdint.h>

#include "platform.h"

#include "maths.h"
#include "trig.h"

/*
 * Fast trig approximations.
 *
 * sin_fast()/cos_fast()/tan_fast() do no range reduction: valid only for
 * |x| <= pi/4, where a degree-9/8 Taylor polynomial already tracks libm to
 * better than 1e-7. sin_approx()/cos_approx()/tan_approx() fold any x into
 * that range with a quadrant reduction first.
 */

#ifndef USE_STANDARD_MATH

// Degree-9 odd Taylor poly; valid for |x| <= pi/4.
static inline float sin_fast_poly(float x)
{
    const float c1 =  1.0f;                         //  1
    const float c3 = -0.16666666666666667f;         // -1/3!
    const float c5 =  0.0083333333333333332f;       //  1/5!
    const float c7 = -0.00019841269841269841f;      // -1/7!
    const float c9 =  0.0000027557319223985893f;    //  1/9!
    const float x2 = x * x;
    return x * (c1 + x2 * (c3 + x2 * (c5 + x2 * (c7 + x2 * c9))));
}

// Degree-8 even Taylor poly; valid for |x| <= pi/4.
static inline float cos_fast_poly(float x)
{
    const float c0 =  1.0f;                         //  1
    const float c2 = -0.5f;                         // -1/2
    const float c4 =  0.041666666666666664f;        //  1/24
    const float c6 = -0.001388888888888889f;        // -1/720
    const float c8 =  0.00002480158730158730f;      //  1/40320
    const float x2 = x * x;
    return (c0 + x2 * (c2 + x2 * (c4 + x2 * (c6 + x2 * c8))));
}

FAST_CODE float sin_fast(float x)
{
    return sin_fast_poly(x);
}

FAST_CODE float cos_fast(float x)
{
    return cos_fast_poly(x);
}

FAST_CODE sincosf_t sincos_fast(float x)
{
    sincosf_t sc;
    sc.sin = sin_fast_poly(x);
    sc.cos = cos_fast_poly(x);
    return sc;
}

FAST_CODE float tan_fast(float x)
{
    return sin_fast_poly(x) / cos_fast_poly(x);
}

// Minimum degree-5 odd polynomial, valid over one quarter-turn.
static inline float sin_poly(float r)
{
    const float c1 =  1.570788468983057f;
    const float c3 = -0.645711990181946f;
    const float c5 =  0.077667393626301f;
    const float r2 = r * r;
    return r * (c1 + r2 * (c3 + r2 * c5));
}

// Minimum degree-6 even polynomial, valid over one quarter-turn.
static inline float cos_poly(float r)
{
    const float c2 = -1.233697953970536f;
    const float c4 =  0.253606361920527f;
    const float c6 = -0.020426250304794f;
    const float r2 = r * r;
    return 1.0f + r2 * (c2 + r2 * (c4 + r2 * c6));
}

FAST_CODE float sin_approx(float rad)
{
    float x = rad * M_2_PIf;
    float q = roundf(x);
    float r = x - q;
    int32_t i = (int32_t)q;
    float y = (i & 1) ? cos_poly(r) : sin_poly(r);
    return (i & 2) ? -y : y;
}

FAST_CODE float cos_approx(float rad)
{
    float x = rad * M_2_PIf;
    float q = roundf(x);
    float r = x - q;
    int32_t i = (int32_t)q;
    float y = (i & 1) ? -sin_poly(r) : cos_poly(r);
    return (i & 2) ? -y : y;
}

FAST_CODE sincosf_t sincos_approx(float rad)
{
    float x = rad * M_2_PIf;
    float q = roundf(x);
    float r = x - q;
    int32_t i = (int32_t)q;
    float s = sin_poly(r);
    float c = cos_poly(r);
    float sinv = (i & 1) ?  c : s;
    float cosv = (i & 1) ? -s : c;
    sincosf_t sc;
    if (i & 2) {
        sc.sin = -sinv;
        sc.cos = -cosv;
    } else {
        sc.sin = sinv;
        sc.cos = cosv;
    }
    return sc;
}

FAST_CODE void sincosf_approx(float x, float *restrict out_s, float *restrict out_c)
{
    sincosf_t sc = sincos_approx(x);
    *out_s = sc.sin;
    *out_c = sc.cos;
}

FAST_CODE float tan_approx(float rad)
{
    float x = rad * M_2_PIf;
    float q = roundf(x);
    float r = x - q;
    int32_t i = (int32_t)q;
    float s = sin_poly(r);
    float c = cos_poly(r);
    return (i & 1) ? -c / s : s / c;
}

// Alternative full-range tan(): a single rational polynomial, folded the same
// way as sin_approx()/cos_approx() but with no division by a near-zero
// cosine near the folded range's edge.
FAST_CODE float tan_approx2(float rad)
{
    const float p0 = 1.29192793f;
    const float p1 = 1.27515733f;
    const float p2 = 1.26977468f;
    const float p3 = 1.33686483f;
    const float p4 = 0.791981876f;
    const float p5 = 2.81889224f;

    float x = rad * M_2_PIf;
    float q = roundf(x);
    float r = x - q;
    int32_t i = (int32_t)q;

    const float w = r * r;
    const float p = (((((p5 * w + p4) * w + p3) * w + p2) * w + p1) * w + p0);
    const float y = r * (M_PI2f + w * p);

    return (i & 1) ? (-1.0f / y) : y;
}

FAST_CODE float asin_approx(float x)
{
    return M_PI2f - acos_approx(x);
}

FAST_CODE float acos_approx(float x)
{
    const float a0 =  1.5707288f;
    const float a1 = -0.2121144f;
    const float a2 =  0.0742610f;
    const float a3 = -0.0187293f;

    const float z = fabsf(x);
    const float y = sqrtf(fmaxf(0.0f, 1.0f - z)) * (a0 + z * (a1 + z * (a2 + z * a3)));

    return (x < 0) ? M_PIf - y : y;
}

FAST_CODE float atan2_approx(float y, float x)
{
    const float n1 = 3.14551665884836e-7f;
    const float n2 = 0.99997356613987f;
    const float n3 = 0.14744007058297684f;
    const float n4 = 0.3099814292351353f;
    const float n5 = 0.05030176425872175f;
    const float d1 = 0.1471039133652469f;
    const float d2 = 0.6444640676891548f;

    float absX = fabsf(x);
    float absY = fabsf(y);

    float z = fmaxf(absX, absY);
    float r = (z != 0.0f) ? fminf(absX, absY) / z : 0.0f;

    float s = -((((n5 * r - n4) * r - n3) * r - n2) * r - n1) /
                ((d2 * r + d1) * r + 1.0f);

    if (absY > absX)
        s = M_PI2f - s;
    if (x < 0)
        s = M_PIf - s;
    if (y < 0)
        s = -s;

    return s;
}

#endif /* USE_STANDARD_MATH */
