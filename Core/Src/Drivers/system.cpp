//
// Created by marce on 01/03/2023.
//

#include "Drivers/system.h"

extern "C" {
    #include "core_cm4.h"
    #include "stm32f4xx_hal.h"
}

#define NVIC_PRIORITY_GROUPING NVIC_PRIORITYGROUP_2
#define NVIC_PRIO_MAX                      NVIC_BUILD_PRIORITY(0, 1)
#define NVIC_BUILD_PRIORITY(base,sub) (((((base)<<(4-(7-(NVIC_PRIORITY_GROUPING))))|((sub)&(0x0f>>(7-(NVIC_PRIORITY_GROUPING)))))<<4)&0xf0)


// cycles per microsecond
static uint32_t usTicks = 0;
// current uptime for 1kHz systick timer. will rollover after 49 days. hopefully we won't care.
static uint32_t sysTickUptime = 0;
static uint32_t sysTickValStamp = 0;

// SysTick
static volatile int sysTickPending = 0;

void SysTick_Handler(void)
{
    // ATOMIC_BLOCK(NVIC_PRIO_MAX) {
    {   // This should be atomic
        sysTickUptime++;
        sysTickValStamp = SysTick->VAL;
        sysTickPending = 0;
        (void)(SysTick->CTRL);
    }
    // }
#ifdef USE_HAL_DRIVER
    // used by the HAL for some timekeeping and timeouts, should always be 1ms
    HAL_IncTick();
#endif
}

uint32_t microsISR() {
    uint32_t ms, pending, cycle_cnt;

    // ATOMIC_BLOCK(NVIC_PRIO_MAX) {
    {   // This should be atomic
        cycle_cnt = SysTick->VAL;

        if (SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) {
            // Update pending.
            // Record it for multiple calls within the same rollover period
            // (Will be cleared when serviced).
            // Note that multiple rollovers are not considered.

            sysTickPending = 1;

            // Read VAL again to ensure the value is read after the rollover.

            cycle_cnt = SysTick->VAL;
    }

    //    }

        ms = sysTickUptime;
        pending = sysTickPending;
    }

    return ((ms + pending) * 1000) + (usTicks * 1000 - cycle_cnt) / usTicks;
}