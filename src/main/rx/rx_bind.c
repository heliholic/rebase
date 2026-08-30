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

#include "platform.h"

#if defined(USE_RX_BIND)

#include "rx/srxl2.h"
#include "rx/crsf.h"

#include "rx_bind.h"

static bool doRxBind(bool doBind)
{
#if !defined(USE_SERIALRX_SRXL2) && !defined(USE_SERIALRX_CRSF)
    UNUSED(doBind);
#endif

    switch (rxRuntimeState.rxProvider) {
    default:
        return false;
    case RX_PROVIDER_SERIAL:
        switch (rxRuntimeState.serialrxProvider) {
        default:
            return false;
#if defined(USE_SERIALRX_CRSF)
        case SERIALRX_CRSF:
            if (doBind) {
                crsfRxBind();
            }

            break;
#endif
#if defined(USE_SERIALRX_SRXL2)
        case SERIALRX_SRXL2:
            if (doBind) {
                srxl2Bind();
            }

            break;
#endif
        }

        break;
    }

    return true;
}

bool startRxBind(void)
{
    return doRxBind(true);
}

bool getRxBindSupported(void)
{
    return doRxBind(false);
}
#endif
