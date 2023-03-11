//
// Created by marce on 10/03/2023.
//

#include "Drivers/STM32/io/usart.h"

#pragma once

extern "C" {
    #include "stm32f4xx_hal.h"
}

#include "commons.h"

// @TODO: Complete refactoring of peripheral drivers

class Serial {
public:
    Serial();

private:
    UART_HandleTypeDef m_Handle;
};