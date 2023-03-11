//
// Created by marce on 01/03/2023.
//

#pragma once

#include "Rx/Rx.h"
#include "ghst_protocol.h"

#include <array>

#define GHST_MAX_NUM_CHANNELS           16

// TODO: Move to pointers in order to optimize swapping buffers
struct ghstFrameBuffer_s {
    ghstFrame_t* ghstIncomingFrame;          // incoming frame, raw, not CRC checked, destination address not checked
    ghstFrame_t* ghstValidatedFrame;         // validated frame, CRC is ok, destination address is ok, ready for decode
};

class Ghst : public Rx {
public:
    Ghst();
    ~Ghst() override = default;

    void Init() override;

    float RcReadRaw(uint8_t chan) override;
    uint8_t RcFrameStatus() override;
    bool ProcessFrame() override;
    timeUs_t FrameTimeUs() override;

    void ReceiveDataISR(uint16_t c, void* data) override;

private:
    void SwapFrameBuffers();

private:
    std::array<uint16_t , GHST_MAX_NUM_CHANNELS> m_GhstChannelData;             // Actual rc channel data

    std::array<ghstFrame_t, 2> m_GhstFrameBuffers;                              // Storage for incoming and validated ghost frame buffers
    ghstFrameBuffer_s m_GhstFrameBufferPtrs;                                    // Struct with pointers to the incoming and validated frame buffers

    ghstRfProtocol_e m_GhstRfProtocol;                                          // RF protocol used by the RX

    bool m_GhstFrameAvailable;                                                  // true if a new frame is available
    bool m_GhstValidatedFrameAvailable;                                         // true if a new validated frame is available
    timeUs_t m_GhstRxFrameStartAtUs;                                            // time when the current frame was received
    timeUs_t m_GhstRxFrameEndAtUs;                                              // time when the current frame was received and saved

};


