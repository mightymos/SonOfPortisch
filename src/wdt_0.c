/**************************************************************************//**
 * Copyright (c) 2015 by Silicon Laboratories Inc. All rights reserved.
 *
 * http://developer.silabs.com/legal/version/v11/Silicon_Labs_Software_License_Agreement.txt
 *****************************************************************************/

#include "wdt_0.h"
#include "assert.h"

#include <stdbool.h>

void WDT0_start(void)
{
    WDTCN = 0xA5;
}

void WDT0_stop(void)
{
    bool flag = EA;

    EA = 0;
    WDTCN = 0xDE;
    WDTCN = 0xAD;

    EA = flag;
}

void WDT0_feed(void)
{
	// examples
	//(1/80000)*4^(0+3) =  0.0008 secs
	//(1/80000)*4^(7+3) = 13.1072 secs
	// default is divide by 8, so 10 kHz
    WDTCN = 0xA5;
}

void WDT0_init(uint8_t interval, WDT0_Timebase_t  timebase, WDT0_IdleState_t idleState)
{
    timebase = timebase;
    idleState = idleState;
    WDTCN = interval;
}
