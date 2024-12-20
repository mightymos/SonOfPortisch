/*
 * uart.c
 *
 *  Created on: 27.11.2017
 *      Author:
 */

#include "delay.h"
#include "hal.h"
#include "portisch.h"
#include "portisch_protocols.h"
#include "uart.h"

#include <stdbool.h>
#include <stdint.h>
#include <EFM8BB1.h>

//-----------------------------------------------------------------------------
// Global Variables
//-----------------------------------------------------------------------------
__xdata uint8_t volatile UART_RX_Buffer[UART_RX_BUFFER_SIZE];
__xdata uint8_t volatile UART_TX_Buffer[UART_TX_BUFFER_SIZE];

__xdata static volatile uint8_t UART_RX_Buffer_Position = 0;
__xdata static volatile uint8_t UART_TX_Buffer_Position = 0;
__xdata static volatile uint8_t UART_Buffer_Read_Position = 0;
__xdata static volatile uint8_t UART_Buffer_Write_Position = 0;
__xdata static volatile uint8_t UART_Buffer_Write_Len = 0;

// FIXME: add comment as to why set true
bool static volatile gTXFinished = true;


//-----------------------------------------------------------------------------
// UART ISR Callbacks
//-----------------------------------------------------------------------------
//void UART0_receiveCompleteCb(void)
//{
//}

//void UART0_transmitCompleteCb(void)
//{
//}


void UART0_ISR(void) __interrupt (4)
{
    // UART0 TX Interrupt
    #define UART0_TX_IF TI__BMASK
    // UART0 RX Interrupt
    #define UART0_RX_IF RI__BMASK

	// buffer and clear flags immediately so we don't miss an interrupt while processing
	uint8_t flags = SCON0 & (UART0_RX_IF | UART0_TX_IF);
	SCON0 &= ~flags;

	// receiving byte
	if ((flags & RI__SET))
	{
        // store received data in buffer
    	UART_RX_Buffer[UART_RX_Buffer_Position] = UART0_read();
        UART_RX_Buffer_Position++;

        // set to beginning of buffer if end is reached
        if (UART_RX_Buffer_Position == UART_RX_BUFFER_SIZE)
        {
	    	UART_RX_Buffer_Position = 0;
        }
	}

	// transmit byte
	if ((flags & TI__SET))
	{
		if (UART_Buffer_Write_Len > 0)
		{
			UART0_write(UART_TX_Buffer[UART_Buffer_Write_Position]);
			UART_Buffer_Write_Position++;
			UART_Buffer_Write_Len--;
            
            gTXFinished = false;
		} else {
			gTXFinished = true;
        }

		if (UART_Buffer_Write_Position == UART_TX_BUFFER_SIZE)
        {
			UART_Buffer_Write_Position = 0;
        }
	}
}

void UART0_init(UART0_RxEnable_t rxen, UART0_Width_t width, UART0_Multiproc_t mce)
{
    SCON0 &= ~(SMODE__BMASK | MCE__BMASK | REN__BMASK);
    SCON0 = mce | rxen | width;
}

void UART0_initStdio(void)
{
    SCON0 |= REN__RECEIVE_ENABLED | TI__SET;
}

void UART0_initTxPolling(void)
{
    TI = 1;
}

void UART0_write(uint8_t value)
{
	SBUF0 = value;
}

uint8_t UART0_read(void)
{
    return SBUF0;
}


// polled version
//int putchar (int c)
//{
//    // assumes UART is initialized
//    while (!TI);
//    TI = 0;
//    SBUF0 = c;
//    return c;
//}

// interrupt version with buffer
int putchar (int c)
{
    // basically a wrapper
    uart_putc(c);
    
    return c;
}


bool is_uart_tx_finished(void)
{
    return gTXFinished;
}

bool is_uart_tx_buffer_empty(void)
{
    bool isBufferEmpty = true;
    
    if (UART_Buffer_Write_Len > 0)
    {
        isBufferEmpty = false;
    }
    
    return isBufferEmpty;
}


/*************************************************************************
Function: uart_getc()
Purpose:  return byte from ringbuffer
Returns:  lower byte:  received byte from ringbuffer
          higher byte: last receive error
**************************************************************************/
unsigned int uart_getc(void)
{
	unsigned int rxdata;

    if (UART_Buffer_Read_Position == UART_RX_Buffer_Position)
    {
        // no data available
        return UART_NO_DATA;
    }

    // get data from receive buffer
    rxdata = UART_RX_Buffer[UART_Buffer_Read_Position];
    UART_Buffer_Read_Position++;

    if (UART_Buffer_Read_Position == UART_RX_BUFFER_SIZE)
    {
    	UART_Buffer_Read_Position = 0;
    }

    // FIXME: can not see where lastRxError is ever set?
    //rxdata |= (lastRxError << 8);    
    //lastRxError = 0;
    
    return rxdata;
}

/*************************************************************************
Function: uart_putc()
Purpose:  write byte to ringbuffer for transmitting via UART
Input:    byte to be transmitted
Returns:  none
**************************************************************************/
void uart_putc(uint8_t txdata)
{
	if (UART_TX_Buffer_Position == UART_TX_BUFFER_SIZE)
    {
		UART_TX_Buffer_Position = 0;
    }

	UART_TX_Buffer[UART_TX_Buffer_Position] = txdata;

	UART_TX_Buffer_Position++;
	UART_Buffer_Write_Len++;

	// FIXME: we have to decide if this is just useful for development, or for normal operation
	// have we written more bytes than the transmit buffer can hold (without keeping up with reading out)?
	if (UART_Buffer_Write_Len > UART_TX_BUFFER_SIZE)
	{
		// if overwritten buffer, go into infinite loop so watchdog resets processor
		while (1)
		{
			led_on();
			delay_ms(250);
			led_off();
			delay_ms(250);
		}
	}
}