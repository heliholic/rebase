/*
 * This file is part of Rotorflight.
 *
 * Rotorflight is free software. You can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Rotorflight is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <math.h>
#include <stdlib.h>

#include "types.h"
#include "median.h"
#include "stddev.h"
#include "trig.h"

/*
 * Floating point constants
 */

#define M_PIf           3.14159265358979323846f
#define M_PI2f          1.57079632679489661923f
#define M_PI_2f         1.57079632679489661923f
#define M_PI_4f         0.78539816339744830962f
#define M_PI_8f         0.39269908169872415481f
#define M_2PIf          6.28318530717958647693f
#define M_1_2PIf        0.15915494309189533577f
#define M_2_PIf         0.63661977236758134308f

#define M_RADf          0.01745329251994329577f
#define RAD             M_RADf

#define M_EULERf        2.71828182845904523536f


/*
 * Fast exp/log/pow approximations
 */

#ifndef USE_STANDARD_MATH

float exp_approx(float val);
float log_approx(float val);
float pow_approx(float a, float b);

#else /* USE_STANDARD_MATH */

#define exp_approx(x)       expf(x)
#define log_approx(x)       logf(x)
#define pow_approx(a, b)    powf((a),(b))

#endif /* USE_STANDARD_MATH */


/*
 * Unit conversion macros
 */

#define DEGREES_TO_DECIDEGREES(angle)       ((angle) * 10)
#define DECIDEGREES_TO_DEGREES(angle)       ((angle) / 10)
#define DECIDEGREES_TO_RADIANS(angle)       ((angle) / 10 * M_RADf)
#define DEGREES_TO_RADIANS(angle)           ((angle) * M_RADf)
#define RADIANS_TO_DEGREES(angle)           ((angle) / M_RADf)

#define CM_S_TO_KM_H(cmps)                  ((cmps) * 9 / 250)
#define CM_S_TO_MPH(cmps)                   ((cmps) * 125 / 5588)

#define HZ_TO_INTERVAL(x)                   (1.0f / (x))
#define HZ_TO_INTERVAL_US(x)                (1000000 / (x))


#define SCALE_FACTOR(in_start, in_end, out_start, out_end) \
    ((float)((out_end) - (out_start)) / (float)((in_end) - (in_start)))

#define SCALE_OFFSET(in_start, in_end, out_start, out_end) \
    ((float)(out_start) - (SCALE_FACTOR((in_start), (in_end), (out_start), (out_end)) * (float)(in_start)))

#define DEFINE_SCALE_FN(name, in_start, in_end, out_start, out_end)              \
    static inline float name(float input) {                                      \
        return (input * (SCALE_FACTOR(in_start, in_end, out_start, out_end)))    \
                      + (SCALE_OFFSET(in_start, in_end, out_start, out_end));    \
    }


/*
 * Basic math macros
 */

#ifndef sq
#define sq(x) POWER2(x)
#endif

#ifndef power2
#define power2(x)   POWER2(x)
#endif

#ifndef power3
#define power3(x)   POWER3(x)
#endif

#ifndef power5
#define power5(x)   POWER5(x)
#endif

#define POWER2(x) \
  __extension__ ({ \
    __typeof__ (x) _x = (x); \
    _x * _x; })

#define POWER3(x) \
  __extension__ ({ \
    __typeof__ (x) _x = (x); \
    _x * _x * _x; })

#define POWER4(x) \
  __extension__ ({ \
    __typeof__ (x) _x = (x); \
    _x * _x * _x * _x; })

#define POWER5(x) \
  __extension__ ({ \
    __typeof__ (x) _x = (x); \
    _x * _x * _x * _x * _x; })

#define POWER6(x) \
  __extension__ ({ \
    __typeof__ (x) _x = (x); \
    _x * _x * _x * _x * _x * _x; })

#define MIN(a,b) \
  __extension__ ({ \
    __typeof__ (a) _a = (a); \
    __typeof__ (b) _b = (b); \
    _a < _b ? _a : _b; })

#define MAX(a,b) \
  __extension__ ({ \
    __typeof__ (a) _a = (a); \
    __typeof__ (b) _b = (b); \
    _a > _b ? _a : _b; })

#define ABS(x) \
  __extension__ ({ \
    __typeof__ (x) _x = (x); \
    _x > 0 ? _x : -_x; })

#define SIGN(x) \
  __extension__ ({ __typeof__ (x) _x = (x); \
  (_x > 0) - (_x < 0); })

/*
 * Basic math operations
 */

static inline int constrain(int value, int low, int high)
{
    if (value < low)
        return low;
    else if (value > high)
        return high;
    else
        return value;
}

static inline float constrainf(float value, float low, float high)
{
    if (value < low)
        return low;
    else if (value > high)
        return high;
    else
        return value;
}

static inline int limit(int value, int limit)
{
    if (value < -limit)
        return -limit;
    else if (value > limit)
        return limit;
    else
        return value;
}

static inline float limitf(float value, float limit)
{
    if (value < -limit)
        return -limit;
    else if (value > limit)
        return limit;
    else
        return value;
}

static inline int scaleRange(int src, int srcFrom, int srcTo, int dstFrom, int dstTo)
{
    const int srcRange = srcTo - srcFrom;
    const int dstRange = dstTo - dstFrom;
    return ((src - srcFrom) * dstRange) / srcRange + dstFrom;
}

static inline float scaleRangef(float src, float srcFrom, float srcTo, float dstFrom, float dstTo)
{
    const float srcRange = srcTo - srcFrom;
    const float dstRange = dstTo - dstFrom;
    return ((src - srcFrom) * dstRange) / srcRange + dstFrom;
}

typedef struct scaleRangef_s {
    float offset;
    float scale;
} scaleRangef_t;

void scaleRangefInit(scaleRangef_t *scale, float srcFrom, float srcTo, float destFrom, float destTo);
float scaleRangefApply(scaleRangef_t *scale, float x);


static inline float slewLimit(float current, float target, float rate)
{
    if (rate > 0) {
        if (target > current + rate)
            return current + rate;
        if (target < current - rate)
            return current - rate;
    }
    return target;
}

static inline float slewUpLimit(float current, float target, float rate)
{
    if (rate > 0) {
        if (target > current + rate)
            return current + rate;
    }
    return target;
}

static inline float slewDownLimit(float current, float target, float rate)
{
    if (rate > 0) {
        if (target < current - rate)
            return current - rate;
    }
    return target;
}

static inline float slewUpDownLimit(float current, float target, float uprate, float downrate)
{
    if (uprate > 0 && target > current + uprate) {
        return current + uprate;
    }
    if (downrate > 0 && target < current - downrate) {
        return current - downrate;
    }
    return target;
}

static inline int32_t applyDeadband(const int32_t value, const int32_t deadband)
{
    if (value > deadband)
        return value - deadband;
    else if (value < -deadband)
        return value + deadband;
    return 0;
}

static inline float fapplyDeadband(const float value, const float deadband)
{
    if (value > deadband)
        return value - deadband;
    else if (value < -deadband)
        return value + deadband;
    return 0;
}

static inline float transition(const float src, const float srcMin, const float srcMax,
                               const float dstMin, const float dstMax)
{
    if (src > srcMax)
        return dstMax;
    else if (src < srcMin)
        return dstMin;

    return scaleRangef(src, srcMin, srcMax, dstMin, dstMax);
}


static inline float degreesToRadians(int16_t degrees)
{
    return DEGREES_TO_RADIANS(degrees);
}

static inline int16_t radiansToDegrees(float radians)
{
    return RADIANS_TO_DEGREES(radians);
}

float smoothStepUpTransition(const float x, const float center, const float width);
