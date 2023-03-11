//
// Created by marce on 07/03/2023.
//

#include "Rx/Rx.h"


void Rx::SetRssi(uint16_t rssiValue, rssiSource_e source)
{
    if (source != m_RssiSource) {
        return;
    }

    // Filter RSSI value
    if (source == RSSI_SOURCE_FRAME_ERRORS) {
        //@TODO: Implement the filter stuff
        //m_RssiRaw = pt1FilterApply(&frameErrFilter, rssiValue);
    } else {
        m_RssiRaw = rssiValue;
    }
}