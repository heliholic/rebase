/*
 * This file is part of Cleanflight and Betaflight.
 *
 * Cleanflight and Betaflight are free software. You can redistribute
 * this software and/or modify this software under the terms of the
 * GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * Cleanflight and Betaflight are distributed in the hope that they
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software.
 *
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdbool.h>

#include "platform.h"

#ifdef USE_USB_CDC_HID

#include "common/maths.h"
#include "fc/rc_controls.h"
#include "rx/rx.h"
#include "pg/usb.h"
#include "sensors/battery.h"
#include "usb_cdc_hid.h"

#define USB_CDC_HID_NUM_AXES 8
#define USB_CDC_HID_NUM_BUTTONS 8

#define USB_CDC_HID_RANGE_MIN -127
#define USB_CDC_HID_RANGE_MAX 127

// In the windows joystick driver, the axes are defined as shown in the second column.

// A source is either a control function or a plain receiver channel. The
// channel numbers below are the ones the report used under the old fixed
// channel order, so the report is unchanged for a default control map.
typedef struct {
    bool    isControl;
    uint8_t index;
} hidSource_t;

#define HID_CONTROL(function)  { true,  (function) }
#define HID_CHANNEL(channel)   { false, (channel) }

static const hidSource_t hidChannelMapping[] = {
    HID_CONTROL(RC_ROLL),     // X
    HID_CONTROL(RC_PITCH),    // Y
    HID_CHANNEL(7),           // Z
    HID_CONTROL(RC_YAW),      // X Rotation
    HID_CHANNEL(5),           // Z Rotation
    HID_CONTROL(RC_THROTTLE), // Y Rotation
    HID_CHANNEL(8),           // Slider
    HID_CHANNEL(6),           // Dial
    HID_CHANNEL(9),           // Button 1
    HID_CHANNEL(10),          // Button 2
    HID_CHANNEL(11),          // Button 3
    HID_CHANNEL(12),          // Button 4
    HID_CHANNEL(13),          // Button 5
    HID_CHANNEL(14),          // Button 6
    HID_CHANNEL(15),          // Button 7
    HID_CHANNEL(16)           // Button 8
};

static float hidSourceValue(const hidSource_t *source)
{
    return source->isControl ? rcControls[source->index] : rcInput[source->index];
}

void sendRcDataToHid(void)
{
    int8_t report[9];
     // Axes
    for (unsigned i = 0; i < USB_CDC_HID_NUM_AXES; i++) {
        const hidSource_t *source = &hidChannelMapping[i];
        report[i] = scaleRange(constrain(hidSourceValue(source), PWM_RANGE_MIN, PWM_RANGE_MAX), PWM_RANGE_MIN, PWM_RANGE_MAX, USB_CDC_HID_RANGE_MIN, USB_CDC_HID_RANGE_MAX);
        if (source->isControl && source->index == RC_PITCH) {
            // PITCH is inverted in Windows
            report[i] = -report[i];
        }
    }

    // Buttons
    // Each bit in one byte represents one button so we have 8 buttons in one-byte-data
    report[8] = 0;
    for (unsigned i = 0; i < USB_CDC_HID_NUM_BUTTONS; i++) {
        const hidSource_t *source = &hidChannelMapping[i + USB_CDC_HID_NUM_AXES];
        if (scaleRange(constrain(hidSourceValue(source), PWM_RANGE_MIN, PWM_RANGE_MAX), PWM_RANGE_MIN, PWM_RANGE_MAX, USB_CDC_HID_RANGE_MIN, USB_CDC_HID_RANGE_MAX) > 0) {
            report[8] |= (1 << i);
        }
    }

    sendReport((uint8_t*)report, sizeof(report));
}

bool cdcDeviceIsMayBeActive(void)
{
    return usbDevConfig()->type == COMPOSITE && usbIsConnected() && (getBatteryState() == BATTERY_NOT_PRESENT || batteryConfig()->voltageMeterSource == VOLTAGE_METER_NONE);
}
#endif
