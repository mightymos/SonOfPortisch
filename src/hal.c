/*
 * Globals.c
 *
 *  Created on: 07.12.2017
 *      Author:
 */


#include "hal.h"

#include <stdint.h>
#include <EFM8BB1.h>

// globals to track timer expiration
__xdata uint16_t Timer_2_Timeout  = 0x0000;
__xdata uint16_t Timer_2_Interval = 0x0000;
__xdata uint16_t Timer_3_Timeout  = 0x0000;
__xdata uint16_t Timer_3_Interval = 0x0000;


//==============================================================================
// enter_DefaultMode_from_RESET
//==============================================================================
void enter_DefaultMode_from_RESET(void)
{

	// FIXME: add comment to explain pin functions
	P0MDOUT = B0__PUSH_PULL | B1__OPEN_DRAIN | B2__OPEN_DRAIN | B3__OPEN_DRAIN | B4__PUSH_PULL | B5__OPEN_DRAIN | B6__OPEN_DRAIN | B7__OPEN_DRAIN;

	// add explanation
	P0SKIP = B0__SKIPPED | B1__SKIPPED | B2__SKIPPED | B3__SKIPPED | B4__NOT_SKIPPED | B5__NOT_SKIPPED | B6__SKIPPED | B7__SKIPPED;



	// FIXME: correctly handle LED on sonoff different from LED on EFM8BB1LCK board
	P1MDOUT = B0__PUSH_PULL | B1__OPEN_DRAIN| B2__OPEN_DRAIN | B3__OPEN_DRAIN| B4__PUSH_PULL | B5__PUSH_PULL| B6__PUSH_PULL | B7__PUSH_PULL;

	// $[P1SKIP - Port 1 Skip]
	/***********************************************************************
	 - P1.0 pin is skipped by the crossbar
	 - P1.1 pin is skipped by the crossbar
	 - P1.2 pin is skipped by the crossbar
	 - P1.3 pin is not skipped by the crossbar
	 - P1.4 pin is skipped by the crossbar
	 - P1.5 pin is skipped by the crossbar
	 - P1.6 pin is skipped by the crossbar
	 ***********************************************************************/
	P1SKIP = B0__SKIPPED | B1__SKIPPED | B2__SKIPPED| B3__NOT_SKIPPED | B4__SKIPPED | B5__SKIPPED| B6__SKIPPED | B7__SKIPPED;


	// $[XBR2 - Port I/O Crossbar 2]
	/***********************************************************************
	 - Weak Pullups enabled 
	 - Crossbar enabled
	 ***********************************************************************/
	XBR2 = WEAKPUD__PULL_UPS_ENABLED | XBARE__ENABLED;


	// $[XBR0 - Port I/O Crossbar 0]
	/***********************************************************************
	 - UART TX, RX routed to Port pins P0.4 and P0.5
	 - SPI I/O unavailable at Port pins
	 - SMBus 0 I/O unavailable at Port pins
	 - CP0 unavailable at Port pin
	 - Asynchronous CP0 unavailable at Port pin
	 - CP1 unavailable at Port pin
	 - Asynchronous CP1 unavailable at Port pin
	 - SYSCLK unavailable at Port pin
	 ***********************************************************************/
	XBR0 = URT0E__ENABLED | SPI0E__DISABLED | SMB0E__DISABLED | CP0E__DISABLED | CP0AE__DISABLED | CP1E__DISABLED | CP1AE__DISABLED | SYSCKE__DISABLED;

	// $[XBR1 - Port I/O Crossbar 1]
	/***********************************************************************
	 - CEX0 routed to Port pin
	 - ECI unavailable at Port pin
	 - T0 unavailable at Port pin
	 - T1 unavailable at Port pin
	 - T2 unavailable at Port pin
	 ***********************************************************************/
	XBR1 = PCA0ME__CEX0 | ECIE__DISABLED | T0E__DISABLED | T1E__DISABLED | T2E__DISABLED;


    //*****************************************
	// - Clock derived from the Internal High-Frequency Oscillator
	// - SYSCLK is equal to selected clock source divided by 1
	//*****************************************
	CLKSEL = CLKSL__HFOSC | CLKDIV__SYSCLK_DIV_1;


	// FIXME: what is this timer used for?
    // FIXME: what are the shifts used for?
	//TH0 = (0xA0 << TH0_TH0__SHIFT);
    //TH0 = 0xA0;
	// FIXME: we later load this with 0x0B;
	// 1/((24500000)/(256-0x0B)) = 0.00001
	// which seems to correspond with 10 microsec counts
    TH0 = 0x0B;

	// FIXME:
    // sec. 19.3.1 Baud Rate Generation uart0 baud rate is set by timer 1 in 8-bit auto-reload mode
    //TH1 = (0xCB << TH1_TH1__SHIFT);
    TH1 = 0xCB;

	// FIXME: we removed timer3 resource to save on code space



	//**********************************************************************
	// - System clock divided by 12
	// - Counter/Timer 0 uses the system clock
	// - Timer 2 high byte uses the clock defined by T2XCLK in TMR2CN0
	// - Timer 2 low byte uses the system clock
	// - Timer 3 high byte uses the clock defined by T3XCLK in TMR3CN0
	// - Timer 3 low byte uses the system clock
	// - Timer 1 uses the clock defined by the prescale field, SCA
	//***********************************************************************/
	CKCON0 = SCA__SYSCLK_DIV_12 | T0M__SYSCLK | T2MH__EXTERNAL_CLOCK | T2ML__SYSCLK | T3MH__EXTERNAL_CLOCK | T3ML__SYSCLK | T1M__PRESCALE;


	/***********************************************************************
	 - Mode 2, 8-bit Counter/Timer with Auto-Reload
	 - Mode 2, 8-bit Counter/Timer with Auto-Reload
	 - Timer Mode
	 - Timer 0 enabled when TR0 = 1 irrespective of INT0 logic level
	 - Timer Mode
	 - Timer 1 enabled when TR1 = 1 irrespective of INT1 logic level
	 ***********************************************************************/
    // pg. 194  Setting the TR0 bit enables the timer when either GATE0 in the TMOD register is logic 0
	TMOD = T0M__MODE2 | T1M__MODE2 | CT0__TIMER | GATE0__DISABLED | CT1__TIMER | GATE1__DISABLED;


	/***********************************************************************
	 - Start Timer 0 running
	 - Start Timer 1 running
	 ***********************************************************************/
	TCON |= TR0__RUN | TR1__RUN;

    
	// Stop Timer
	TMR2CN0 &= ~(TR2__BMASK);


	// 
	PCA0CN0 = CR__STOP;


	// $[PCA0MD - PCA Mode]
	/***********************************************************************
	 - PCA continues to function normally while the system controller is in
	 Idle Mode
	 - Enable a PCA Counter/Timer Overflow interrupt request when CF is set
	 - Timer 0 overflow
	 ***********************************************************************/
	PCA0MD = CIDL__NORMAL | ECF__OVF_INT_ENABLED | CPS__T0_OVERFLOW;

	/***********************************************************************
	 - PCA Counter/Timer Low Byte = 0xFF
	 ***********************************************************************/
	//PCA0L = (0xFF << PCA0L_PCA0L__SHIFT);
    PCA0L = 0xFF;

	/***********************************************************************
	 - Invert polarity
	 - Use default polarity
	 - Use default polarity
	 ***********************************************************************/
	 // FIXME: invert polarity seems to apply to output mode only
	 // so why was this inverted?
	 // standard decoding appears to continue working with default (non-inverting)
	//PCA0POL = CEX0POL__INVERT | CEX1POL__DEFAULT | CEX2POL__DEFAULT;


	// FIXME: comment
	PCA0PWM &= ~ARSEL__BMASK;


	// $[PCA0CPM0 - PCA Channel 0 Capture/Compare Mode]
	/***********************************************************************
	 - Enable negative edge capture
	 - Disable CCF0 interrupts
	 - Disable match function
	 - 8 to 11-bit PWM selected
	 - Enable positive edge capture
	 - Disable comparator function
	 - Disable PWM function
	 - Disable toggle function
	 ***********************************************************************/
	PCA0CPM0 = CAPN__ENABLED | ECCF__DISABLED | MAT__DISABLED | PWM16__8_BIT | CAPP__ENABLED | ECOM__DISABLED | PWM__DISABLED | TOG__DISABLED;


	// $[EIE1 - Extended Interrupt Enable 1]
	/***********************************************************************
	 - Disable ADC0 Conversion Complete interrupt
	 - Disable ADC0 Window Comparison interrupt
	 - Disable CP0 interrupts
	 - Disable CP1 interrupts
	 - Disable all Port Match interrupts
	 - Enable interrupt requests generated by PCA0
	 - Disable all SMB0 interrupts
	 - Enable interrupt requests generated by the TF3L or TF3H flags
	 ***********************************************************************/
	EIE1 = EADC0__DISABLED | EWADC0__DISABLED | ECP0__DISABLED | ECP1__DISABLED | EMAT__DISABLED | EPCA0__ENABLED | ESMB0__DISABLED | ET3__ENABLED;

	/***********************************************************************
	 - Enable each interrupt according to its individual mask setting
	 - Disable external interrupt 0
	 - Disable external interrupt 1
	 - Disable all SPI0 interrupts
	 - Disable all Timer 0 interrupt
	 - Disable all Timer 1 interrupt
	 - Enable interrupt requests generated by the TF2L or TF2H flags
	 - Enable UART0 interrupt
	 ***********************************************************************/
	IE = EA__ENABLED | EX0__DISABLED | EX1__DISABLED | ESPI0__DISABLED | ET0__DISABLED | ET1__DISABLED | ET2__ENABLED | ES0__ENABLED;
}

void SetTimer2Reload(uint16_t reload)
{
	/***********************************************************************
	 - Timer 2 Reload High Byte
	 ***********************************************************************/
	TMR2RLH = ((reload >> 8) & 0xFF);
	/***********************************************************************
	 - Timer 2 Reload Low Byte = 0x86
	 ***********************************************************************/
	TMR2RLL = (reload & 0xFF);
}

#if 0
void SetTimer3Reload(uint16_t reload)
{
	/***********************************************************************
	 - Timer 3 Reload High Byte
	 ***********************************************************************/
	TMR3RLH = ((reload >> 8) & 0xFF);
	/***********************************************************************
	 - Timer 3 Reload Low Byte = 0x86
	 ***********************************************************************/
	TMR3RLL = (reload & 0xFF);
}
#endif

/*
 * Init Timer 2 with microseconds interval, maximum is 65535탎.
 */
void InitTimer2_us(uint16_t interval, uint16_t timeout)
{
	SetTimer2Reload((uint16_t)(0x10000 - ((uint32_t)SYSCLK / (1000000 / (uint32_t)interval))));

	// remove 65탎 because of startup delay
	Timer_2_Timeout = timeout - 65;
	Timer_2_Interval = interval;

	// start timer
	TMR2CN0 |= TR2__RUN;
}

#if 0
/*
 * Init Timer 3 with microseconds interval, maximum is 65535탎.
 */
void InitTimer3_us(uint16_t interval, uint16_t timeout)
{
	SetTimerReload((uint16_t)(0x10000 - ((uint32_t)SYSCLK / (1000000 / (uint32_t)interval))));

	// remove 65탎 because of startup delay
	Timer_3_Timeout = timeout - 65;
	Timer_3_Interval = interval;

	// start timer
	TMR3CN0 |= TR3__RUN;
}
#endif

/*
 * Init Timer 2 with milliseconds interval, maximum is ~2.5ms.
 */
void InitTimer2_ms(uint16_t interval, uint16_t timeout)
{
	SetTimer2Reload((uint16_t)(0x10000 - ((uint32_t)SYSCLK / (1000 / (uint32_t)interval))));

	Timer_2_Timeout = timeout;
	Timer_2_Interval = interval;

	// start timer
	TMR2CN0 |= TR2__RUN;
}

#if 0
/*
 * Init Timer 3 with milliseconds interval, maximum is ~2.5ms.
 */
void InitTimer3_ms(uint16_t interval, uint16_t timeout)
{
	SetTimerReload((uint16_t)(0x10000 - ((uint32_t)SYSCLK / (1000 / (uint32_t)interval))));

	Timer_3_Timeout = timeout;
	Timer_3_Interval = interval;

	// start timer
	TMR3CN0 |= TR3__RUN;
}
#endif

void WaitTimer2Finished(void)
{
	// wait until timer has finished
	while((TMR2CN0 & TR2__BMASK) == TR2__RUN);
}

#if 0
void WaitTimer3Finished(void)
{
	// wait until timer has finished
	while((TMR3CN0 & TR3__BMASK) == TR3__RUN);
}
#endif

#if 1
void StopTimer2(void)
{
	// stop timer
	TMR2CN0 &= ~TR2__RUN;
	// Clear Timer 2 high overflow flag
	TMR2CN0 &= ~TF2H__SET;
}
#endif

#if 0
void StopTimer3(void)
{
	// stop timer
	TMR3CN0 &= ~TR3__RUN;
	// Clear Timer 3 high overflow flag
	TMR3CN0 &= ~TF3H__SET;
}
#endif

bool IsTimer2Finished(void)
{
	return ((TMR2CN0 & TR2__BMASK) != TR2__RUN);
}

#if 0
bool IsTimer3Finished(void)
{
	return ((TMR3CN0 & TR3__BMASK) != TR3__RUN);
}
#endif

//-----------------------------------------------------------------------------
// TIMER2_ISR
//-----------------------------------------------------------------------------
//
// TIMER2 ISR Content goes here. Remember to clear flag bits:
// TMR2CN0::TF2H (Timer # High Byte Overflow Flag)
// TMR2CN0::TF2L (Timer # Low Byte Overflow Flag)
//
//-----------------------------------------------------------------------------
void TIMER2_ISR(void) __interrupt (5)
{
	// Clear Timer 2 high overflow flag
	TMR2CN0 &= ~TF2H__SET;

	// check if pulse time is over
	if(Timer_2_Timeout == 0)
	{
		// stop timer
		TMR2CN0 &= ~TR2__RUN;
	}

	if (Timer_2_Timeout < Timer_2_Interval)
		Timer_2_Timeout = 0;
	else
		Timer_2_Timeout -= Timer_2_Interval;
}

#if 0
//-----------------------------------------------------------------------------
// TIMER3_ISR
//-----------------------------------------------------------------------------
//
// TIMER3 ISR Content goes here. Remember to clear flag bits:
// TMR3CN0::TF3H (Timer # High Byte Overflow Flag)
// TMR3CN0::TF3L (Timer # Low Byte Overflow Flag)
//
//-----------------------------------------------------------------------------
void TIMER3_ISR(void) __interrupt (TIMER3_IRQn)
{
	// Clear Timer 3 high overflow flag
	TMR3CN0 &= ~TF3H__SET;

	// check if pulse time is over
	if(Timer_3_Timeout == 0)
	{
		// stop timer
		TMR3CN0 &= ~TR3__RUN;
	}

	if (Timer_3_Timeout < Timer_3_Interval)
		Timer_3_Timeout = 0;
	else
		Timer_3_Timeout -= Timer_3_Interval;
}
#endif
