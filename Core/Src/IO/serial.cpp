//
// Created by marce on 10/03/2023.
//

#include "IO/serial.h"
#include "stm32f4xx_hal_uart.h"

// TODO: Refactor
Serial::Serial() : m_Handle({
.Instance = USART1,
    .Init = {
        .BaudRate = 115200,
        .WordLength = UART_WORDLENGTH_8B,
        .StopBits = UART_STOPBITS_1,
        .Parity = UART_PARITY_NONE,
        .Mode = UART_MODE_TX_RX,
        .HwFlowCtl = UART_HWCONTROL_NONE,
        .OverSampling = UART_OVERSAMPLING_16,
    }
})
{
    HAL_UART_Init(&m_Handle);
}
