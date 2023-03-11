//
// Created by marce on 01/03/2023.
//
#include "Rx/Ghst/ghst.h"
#include "Drivers/system.h"
#include "Utilities/time.h"
#include "Utilities/math.h"
#include "Utilities/crc.h"

/* DEFINES */
#define GHST_PORT_OPTIONS               (SERIAL_STOPBITS_1 | SERIAL_PARITY_NO | SERIAL_BIDIR | SERIAL_BIDIR_PP)
#define GHST_PORT_MODE                  MODE_RXTX   // bidirectional on single pin

#define GHST_MAX_FRAME_TIME_US          500         // 14 bytes @ 420k = ~450us
#define GHST_TIME_BETWEEN_FRAMES_US     2000        // fastest frame rate = 500Hz, or 2000us

#define GHST_RSSI_DBM_MIN (-117)            // Long Range mode value
#define GHST_RSSI_DBM_MAX (-60)             // Typical RSSI with typical power levels, typical antennas, and a few feet/meters between Tx and Rx

// define the time window after the end of the last received packet where telemetry packets may be sent
// NOTE: This allows the Rx to double-up on Rx packets to transmit data other than servo data, but
// only if sent < 1ms after the servo data packet.
#define GHST_RX_TO_TELEMETRY_MIN_US     1000
#define GHST_RX_TO_TELEMETRY_MAX_US     2000

#define GHST_PAYLOAD_OFFSET offsetof(ghstFrameDef_t, type)

#define GHST_RC_CTR_VAL_12BIT_PRIMARY 2048
#define GHST_RC_CTR_VAL_12BIT_AUX     (128 << 2)

/* END DEFINES */

static uint8_t ghstFrameCRC(const ghstFrame_t *const pGhstFrame);

// TODO: All the telemetry stuff ...

void Ghst::Init() {
    // TODO: Open serial ports and implement the actual program flow
    for (int iChan = 0; iChan < m_GhstChannelData.size(); ++iChan) {
        if (iChan < 4 ) {
            m_GhstChannelData[iChan] = GHST_RC_CTR_VAL_12BIT_PRIMARY;
        } else {
            m_GhstChannelData[iChan] = GHST_RC_CTR_VAL_12BIT_AUX;
        }
    }



}

float Ghst::RcReadRaw(uint8_t chan) {
    // Scaling 12bit channels (8bit channels in brackets)
    //      OpenTx          RC   PWM (BF)
    // min  -1024        0(  0)     988us
    // ctr      0     2048(128)    1500us
    // max   1024     4096(256)    2012us
    //

    // Scaling legacy (nearly 10bit)
    // derived from original SBus scaling, with slight correction for offset
    // now symmetrical around OpenTx 0 value
    // scaling is:
    //      OpenTx         RC   PWM (BF)
    // min  -1024     172( 22)     988us
    // ctr      0     992(124)    1500us
    // max   1024    1811(226)    2012us
    //

    float pwm = m_GhstChannelData[chan];

    if (chan < 4) {
        pwm = 0.25f * pwm;
    }

    return pwm + 988;
}

uint8_t Ghst::RcFrameStatus() {
    static int16_t crcErrorCount = 0;
    uint8_t status = RX_FRAME_PENDING;
    auto& [ghstIncomingFrame, ghstValidatedFrame] = m_GhstFrameBufferPtrs;

    if (m_GhstFrameAvailable) {
        m_GhstFrameAvailable = false;

        const uint8_t crc = ghstFrameCRC(m_GhstFrameBufferPtrs.ghstValidatedFrame);
        const int fullFrameLength = ghstValidatedFrame->frame.len + GHST_FRAME_LENGTH_ADDRESS + GHST_FRAME_LENGTH_FRAMELENGTH;
        if (crc == ghstValidatedFrame->bytes[fullFrameLength - 1]) {
            m_GhstValidatedFrameAvailable = true;
            // TODO: Think about how to handle this
            // rxRuntimeState->lastRcFrameTimeUs = ghstRxFrameEndAtUs;
            status = RX_FRAME_COMPLETE | RX_FRAME_PROCESSING_REQUIRED;      // request callback through ghstProcessFrame to do the decoding work
        } else {
            //DEBUG_SET(DEBUG_GHST, DEBUG_GHST_CRC_ERRORS, ++crcErrorCount);
            status = RX_FRAME_DROPPED;                            // frame was invalid
        }
    } else {
#ifdef USE_TELEMETRY_GHST
        if (checkGhstTelemetryState() && shouldSendTelemetryFrame()) {
            status = RX_FRAME_PROCESSING_REQUIRED;
        }
#endif
    }

    return status;
}

bool Ghst::ProcessFrame() {
    // Assume that the only way we get here is if ghstFrameStatus returned RX_FRAME_PROCESSING_REQUIRED, which indicates that the CRC
    // is correct, and the message was actually for us.
    static int16_t unknownFrameCount = 0;

#ifdef USE_TELEMETRY_GHST
    // do we have a telemetry buffer to send?
    if (checkGhstTelemetryState() && shouldSendTelemetryFrame()) {
        ghstTransmittingTelemetry = true;
        ghstRxSendTelemetryData();
    }
#endif

    if (m_GhstValidatedFrameAvailable) {
        m_GhstValidatedFrameAvailable = false;

        const uint8_t ghstFrameType = m_GhstFrameBufferPtrs.ghstValidatedFrame->frame.type;
        const bool scalingLegacy = ghstFrameType >= GHST_UL_RC_CHANS_HS4_FIRST && ghstFrameType <= GHST_UL_RC_CHANS_HS4_LAST;
        const bool scaling12bit = ghstFrameType >= GHST_UL_RC_CHANS_HS4_12_FIRST && ghstFrameType <= GHST_UL_RC_CHANS_HS4_12_LAST;

        if ( scaling12bit || scalingLegacy ) {

            int startIdx = 0;

            switch (ghstFrameType) {
                case GHST_UL_RC_CHANS_HS4_RSSI:
                case GHST_UL_RC_CHANS_HS4_12_RSSI: {
                    const ghstPayloadPulsesRssi_t* const rssiFrame = (ghstPayloadPulsesRssi_t*)&m_GhstFrameBufferPtrs.ghstValidatedFrame->frame.payload;

                    //DEBUG_SET(DEBUG_GHST, DEBUG_GHST_RX_RSSI, -rssiFrame->rssi);
                    //DEBUG_SET(DEBUG_GHST, DEBUG_GHST_RX_LQ, rssiFrame->lq);

                    m_GhstRfProtocol = (ghstRfProtocol_e)rssiFrame->rfProtocol;

#ifdef USE_TELEMETRY_GHST
                    // Enable telemetry just for modes that support it
                    setGhstTelemetryState(ghstRfProtocol == GHST_RF_PROTOCOL_NORMAL
                                       || ghstRfProtocol == GHST_RF_PROTOCOL_RACE
                                       || ghstRfProtocol == GHST_RF_PROTOCOL_LONGRANGE
                                       || ghstRfProtocol == GHST_RF_PROTOCOL_RACE250);
#endif

                    if (m_RssiSource == RSSI_SOURCE_RX_PROTOCOL) {
                        // rssi sent sign-inverted
                        uint16_t rssiPercentScaled = Math::ScaleRange(-rssiFrame->rssi, GHST_RSSI_DBM_MIN, GHST_RSSI_DBM_MAX, 0, RSSI_MAX_VALUE);
                        rssiPercentScaled = Math::Min(rssiPercentScaled, RSSI_MAX_VALUE);
                        SetRssi(rssiPercentScaled, RSSI_SOURCE_RX_PROTOCOL);
                    }

#ifdef USE_RX_RSSI_DBM
                    setRssiDbm(-rssiFrame->rssi, RSSI_SOURCE_RX_PROTOCOL);
#endif

#ifdef USE_RX_LINK_QUALITY_INFO
                    if (linkQualitySource == LQ_SOURCE_RX_PROTOCOL_GHST) {
                        setLinkQualityDirect(rssiFrame->lq);
                    }
#endif
                    break;
                }

                case GHST_UL_RC_CHANS_HS4_5TO8:
                case GHST_UL_RC_CHANS_HS4_12_5TO8:
                    startIdx = 4;
                    break;

                case GHST_UL_RC_CHANS_HS4_9TO12:
                case GHST_UL_RC_CHANS_HS4_12_9TO12:
                    startIdx = 8;
                    break;

                case GHST_UL_RC_CHANS_HS4_13TO16:
                case GHST_UL_RC_CHANS_HS4_12_13TO16:
                    startIdx = 12;
                    break;

                default:
                    // DEBUG_SET(DEBUG_GHST, DEBUG_GHST_UNKNOWN_FRAMES, ++unknownFrameCount);
                    break;
            }

            // We need to wait for the first RSSI frame to know ghstRfProtocol
            if (m_GhstRfProtocol != GHST_RF_PROTOCOL_UNDEFINED) {
                const ghstPayloadPulses_t* const rcChannels = (ghstPayloadPulses_t*)&m_GhstFrameBufferPtrs.ghstValidatedFrame->frame.payload;

                // all uplink frames contain CH1..4 data (12 bit)
                m_GhstChannelData[0] = rcChannels->ch1to4.ch1;
                m_GhstChannelData[1] = rcChannels->ch1to4.ch2;
                m_GhstChannelData[2] = rcChannels->ch1to4.ch3;
                m_GhstChannelData[3] = rcChannels->ch1to4.ch4;

                if (startIdx > 0) {
                    // remainder of uplink frame contains 4 more channels (8 bit), sent in a round-robin fashion
                    m_GhstChannelData[startIdx++] = rcChannels->cha << 2;
                    m_GhstChannelData[startIdx++] = rcChannels->chb << 2;
                    m_GhstChannelData[startIdx++] = rcChannels->chc << 2;
                    m_GhstChannelData[startIdx++] = rcChannels->chd << 2;
                }

                // Recalculate old scaling to the new one
                if (scalingLegacy) {
                    for (int i = 0; i < 4; i++) {
                        m_GhstChannelData[i] = ((5 * m_GhstChannelData[i]) >> 2) - 430;  // Primary
                        if (startIdx > 4) {
                            --startIdx;
                            m_GhstChannelData[startIdx] = 5 * (m_GhstChannelData[startIdx] >> 2) - 108; // Aux
                        }
                    }
                }

            }
        } else {
            switch(ghstFrameType) {

#if defined(USE_TELEMETRY_GHST) && defined(USE_MSP_OVER_TELEMETRY)
                case GHST_UL_MSP_REQ:
            case GHST_UL_MSP_WRITE: {
                static uint8_t mspFrameCounter = 0;
                DEBUG_SET(DEBUG_GHST_MSP, 0, ++mspFrameCounter);
                if (handleMspFrame(ghstValidatedFrame->frame.payload, ghstValidatedFrame->frame.len - GHST_FRAME_LENGTH_CRC - GHST_FRAME_LENGTH_TYPE, NULL)) {
                    ghstScheduleMspResponse();
                }
                break;
            }
#endif
                default:
                    // DEBUG_SET(DEBUG_GHST, DEBUG_GHST_UNKNOWN_FRAMES, ++unknownFrameCount);
                    break;
            }
        }
    }

    return true;
}

timeUs_t Ghst::FrameTimeUs() {
    return 0;
}

Ghst::Ghst()
    :   Rx(),
        m_GhstFrameBufferPtrs({&m_GhstFrameBuffers[0], &m_GhstFrameBuffers[1]}),
        m_GhstRfProtocol(GHST_RF_PROTOCOL_UNDEFINED),
        m_GhstValidatedFrameAvailable(false),
        m_GhstFrameAvailable(false),
        m_GhstRxFrameStartAtUs(0),
        m_GhstRxFrameEndAtUs(0)
{

}

void Ghst::ReceiveDataISR(uint16_t c, void *data) {
    static uint8_t ghstFrameIdx = 0;
    const timeUs_t currentTimeUs = microsISR();

    if (cmpTimeUs(currentTimeUs, m_GhstRxFrameStartAtUs) > GHST_MAX_FRAME_TIME_US) {
        // Character received after the max. frame time, assume that this is a new frame
        ghstFrameIdx = 0;
    }

    if (ghstFrameIdx == 0) {
        // timestamp the start of the frame, to allow us to detect frame sync issues
        m_GhstRxFrameStartAtUs = currentTimeUs;
    }

    // assume frame is 5 bytes long until we have received the frame length
    // full frame length includes the length of the address and framelength fields
    const int fullFrameLength = ghstFrameIdx < 3 ? 5 : m_GhstFrameBufferPtrs.ghstIncomingFrame->frame.len + GHST_FRAME_LENGTH_ADDRESS + GHST_FRAME_LENGTH_FRAMELENGTH;

    if (ghstFrameIdx < fullFrameLength && ghstFrameIdx < sizeof(ghstFrame_t)) {
        m_GhstFrameBufferPtrs.ghstIncomingFrame->bytes[ghstFrameIdx++] = (uint8_t)c;
        if (ghstFrameIdx >= fullFrameLength) {
            ghstFrameIdx = 0;

            // NOTE: this data is not yet CRC checked, nor do we know whether we are the correct recipient, this is
            // handled in ghstFrameStatus

            // Not CRC checked but we are interested just in frame for us
            // eg. telemetry frames are read back here also, skip them
            if (m_GhstFrameBufferPtrs.ghstIncomingFrame->frame.addr == GHST_ADDR_FC) {

                SwapFrameBuffers();
                m_GhstFrameAvailable = true;

                // remember what time the incoming (Rx) packet ended, so that we can ensure a quite bus before sending telemetry
                m_GhstRxFrameEndAtUs = microsISR();
            }
        }
    }




}

void Ghst::SwapFrameBuffers() {
    ghstFrame_t* const tmp =  m_GhstFrameBufferPtrs.ghstIncomingFrame;
    m_GhstFrameBufferPtrs.ghstIncomingFrame = m_GhstFrameBufferPtrs.ghstValidatedFrame;
    m_GhstFrameBufferPtrs.ghstValidatedFrame = tmp;
}


static uint8_t ghstFrameCRC(const ghstFrame_t *const pGhstFrame)
{
    // CRC includes type and payload
    uint8_t crc = crc::crc8_dvb_s2(0, pGhstFrame->frame.type);
    for (int i = 0; i < pGhstFrame->frame.len - GHST_FRAME_LENGTH_TYPE - GHST_FRAME_LENGTH_CRC; ++i) {
        crc = crc::crc8_dvb_s2(crc, pGhstFrame->frame.payload[i]);
    }
    return crc;
}
