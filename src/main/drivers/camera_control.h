/*
 * This file is part of Betaflight.
 *
 * Betaflight is free software. You can redistribute this software
 * and/or modify this software under the terms of the GNU General
 * Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later
 * version.
 *
 * Betaflight is distributed in the hope that it will be useful,
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

#include "io_types.h"
#include "drivers/time.h"

#include "pg/camera_control.h"

typedef struct cameraControlRuntime_s{
    bool enabled;
    IO_t io;
    uint32_t period;
    uint8_t inverted;
    timeMs_t endTimeMillis;
} cameraControlRuntime_t;

void cameraControlInit(void);
void cameraControlProcess(timeUs_t currentTimeUs);
void cameraControlKeyPress(cameraControlKey_e key, timeMs_t holdDurationMs);
