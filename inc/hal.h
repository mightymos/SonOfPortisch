/*
 * hal.h
 *
 *  Created on: 27.11.2017
 *      Author:
 */

#ifndef HAL_H_
#define HAL_H_

#include <stdbool.h>
#include <stdint.h>
#include <EFM8BB1.h>

// used in timer functions
#define SYSCLK	24500000

//
#define FIRMWARE_VERSION		0x03

// macros
#define ARRAY_LENGTH(array) (sizeof((array))/sizeof((array)[0]))


// used to help with defining stored protocol constants
#define PROTOCOL_BUCKETS(X) X ## _buckets
#define PROTOCOL_START(X) X ## _start
#define PROTOCOL_BIT0(X) X ## _bit0
#define PROTOCOL_BIT1(X) X ## _bit1
#define PROTOCOL_END(X) X ## _end

// set bit to high or low to indicate pin logic level
#define HIGH(x) ((x) | 0x08)
#define LOW(x) ((x) & 0x07)


// hardware aliases

// FIXME: handle pins that vary on different hardware automatically
#define TDATA  P0_0

//(uncomment only one LED line)
// Sonoff Bridge (i.e., black case)
//#define LED    P1_0

// EFM8BB1LCK development board
#define LED    P1_4
#define RDATA  P1_3
#define BUZZER P1_6

// mainly helpful on the development board
#define DEBUG_PIN0 P1_5
#define DEBUG_PIN1 P1_6

// function prototypes
void enter_DefaultMode_from_RESET(void);



// a simple hardware abstraction layer
inline void buzzer_on(void)
{
    BUZZER = 1;
}

inline void buzzer_off(void)
{
    BUZZER = 0;
}

inline bool rdata_level(void)
{
    return RDATA;
}

// setter prototypes
inline void led_on(void)
{
	// FIXME: handle inverted output circuit hookup
	// sonoff bridge
    //LED = 1;
	// EFM8BB1LCK board
	LED = 0;
}

inline void led_off(void)
{
	// sonoff bridge
    //LED = 0;
	// EFM8BB1LCK board
	LED = 1;
}

inline void led_toggle(void)
{
    LED = !LED;
}


inline void tdata_on(void)
{
    TDATA = 1;
}


inline void tdata_off(void)
{
    TDATA = 0;
}

inline void debug_pin0_on(void)
{
	DEBUG_PIN0 = 1;
}

inline void debug_pin0_off(void)
{
	DEBUG_PIN0 = 0;
}

inline void debug_pin0_toggle(void)
{
	DEBUG_PIN0 = !DEBUG_PIN0;
}

inline void debug_pin1_on(void)
{
	DEBUG_PIN1 = 1;
}

inline void debug_pin1_off(void)
{
	DEBUG_PIN1 = 0;
}

inline void debug_pin1_toggle(void)
{
	DEBUG_PIN1 = !DEBUG_PIN1;
}


inline void enable_global_interrupts(void)
{
    EA = 1;
}

inline void disable_global_interrupts(void)
{
    EA = 0;
}

inline void enable_capture_interrupt(void)
{
    PCA0CPM0 |= ECCF__ENABLED;
    EIE1     |= EPCA0__ENABLED;
}

inline void disable_capture_interrupt(void)
{
    PCA0CPM0 &= ~ECCF__ENABLED;
    EIE1     &= ~EPCA0__ENABLED;
}

#endif // HAL_H_
