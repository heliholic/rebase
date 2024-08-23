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

#include <math.h>

#include "platform.h"
#include "types.h"
#include "maths.h"


// Cubic polynomial blending function
static float cubicBlend(const float t)
{
    return t * t * (3.0f - 2.0f * t);
}

// Smooth step-up transition function from 0 to 1
float smoothStepUpTransition(const float x, const float center, const float width)
{
    const float half_width = width * 0.5f;
    const float left_limit = center - half_width;
    const float right_limit = center + half_width;

    if (x < left_limit) {
        return 0.0f;
    } else if (x > right_limit) {
        return 1.0f;
    } else {
        const float t = (x - left_limit) / width; // Normalize x within the range
        return cubicBlend(t);
    }
}

void scaleRangefInit(scaleRangef_t *scale, float srcFrom, float srcTo, float destFrom, float destTo)
{
    float range_src = srcTo - srcFrom;
    float range_dest = destTo - destFrom;
    scale->scale = range_dest / range_src;
    scale->offset = destFrom - (srcFrom * scale->scale);
}
