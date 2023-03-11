//
// Created by marce on 01/03/2023.
//

#ifndef TESTBOARDCPLUSPLUS_RX_H
#define TESTBOARDCPLUSPLUS_RX_H

#include "commons.h"

enum rxProvider_t {
    RX_PROVIDER_NONE = 0,
    RX_PROVIDER_SERIAL,
    RX_PROVIDER_SPI,
};

enum serialRxType_t {
    SERIALRX_NONE = 0,
    SERIALRX_CRSF = 1,
    SERIALRX_GHST = 2
};

typedef enum {
    RSSI_SOURCE_NONE = 0,
    RSSI_SOURCE_ADC,
    RSSI_SOURCE_RX_CHANNEL,
    RSSI_SOURCE_RX_PROTOCOL,
    RSSI_SOURCE_MSP,
    RSSI_SOURCE_FRAME_ERRORS,
    RSSI_SOURCE_RX_PROTOCOL_CRSF,
} rssiSource_e;

typedef enum {
    RX_FRAME_PENDING = 0,
    RX_FRAME_COMPLETE = (1 << 0),
    RX_FRAME_FAILSAFE = (1 << 1),
    RX_FRAME_PROCESSING_REQUIRED = (1 << 2),
    RX_FRAME_DROPPED = (1 << 3)
} rxFrameState_e;

#define RSSI_MAX_VALUE 1023

/**
 * @brief Interface for all receivers
 */
class Rx {
public:
    virtual void Init() = 0;

    // used by receiver driver to return channel data
    virtual float RcReadRaw(uint8_t chan) = 0;

    // used to determine the status of the received frame
    virtual uint8_t RcFrameStatus() = 0;

    // used to process a receiving frame
    virtual bool ProcessFrame() = 0;

    // used to retrieve the timestamp in microseconds for the last channel data frame
    virtual timeUs_t FrameTimeUs() = 0;

    // Receive ISR callback, called back from serial port
    virtual void ReceiveDataISR(uint16_t c, void* data) {}; // We can choose to implement this or not

    virtual ~Rx() = default;

protected:

    void SetRssi(uint16_t rssi, rssiSource_e source);


protected:      // Or private, will see...
    uint8_t m_ChannelCount;
    uint16_t* m_RxChannelData;
    void* m_FrameData;
    timeUs_t m_LastRcFrameTimeUs;

    rssiSource_e m_RssiSource;
    uint16_t m_Rssi;
    uint16_t m_RssiRaw;

};


#endif //TESTBOARDCPLUSPLUS_RX_H
