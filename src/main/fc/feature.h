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

#include "pg/feature.h"

#ifndef DEFAULT_RX_FEATURE

#if ENABLE_RX_UDP
#define DEFAULT_RX_FEATURE FEATURE_RX_UDP
#elif defined(USE_SERIALRX)
#define DEFAULT_RX_FEATURE FEATURE_RX_SERIAL
#elif defined(USE_RX_MSP)
#define DEFAULT_RX_FEATURE FEATURE_RX_MSP
#endif

#endif // DEFAULT_RX_FEATURE

#ifndef DEFAULT_RX_FEATURE
#define DEFAULT_RX_FEATURE 0
#endif

// Mask of features that have code compiled in with current config.
//  Other restrictions on available features may apply.
extern const uint32_t featuresSupportedByBuild;

// Feature names for known features
extern const char * const featureNames[FEATURE_BIT_COUNT];

void featureInit(void);
bool featureIsEnabled(const uint32_t mask);
bool featureIsConfigured(const uint32_t mask);
void featureEnableImmediate(const uint32_t mask);
void featureDisableImmediate(const uint32_t mask);
void featureConfigSet(const uint32_t mask);
void featureConfigClear(const uint32_t mask);
void featureConfigReplace(const uint32_t mask);
