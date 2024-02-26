/*
 * uart.c
 *
 *  Created on: 27.11.2017
 *      Author:
 */

#include <SI_EFM8BB1_Register_Enums.h>


#include "efm8_config.h"

#ifndef EFM8PDL_UART0_USE_POLLED
 #define EFM8PDL_UART0_USE_POLLED 1
#endif

#include "Globals.h"
#include "RF_Handling.h"
#include "RF_Protocols.h"
#include "uart.h"


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
__xdata static volatile uint8_t lastRxError = 0;

bool static volatile TX_Finished = true;

__xdata uart_state_t uart_state = IDLE;
__xdata uart_command_t uart_command = NONE;

//-----------------------------------------------------------------------------
// UART ISR Callbacks
//-----------------------------------------------------------------------------
//void UART0_receiveCompleteCb(void)
//{
//}

//void UART0_transmitCompleteCb(void)
//{
//}


void UART0_ISR(void) __interrupt (UART0_IRQn)
{
    // UART0 TX Interrupt
    #define UART0_TX_IF SCON0_TI__BMASK
    // UART0 RX Interrupt
    #define UART0_RX_IF SCON0_RI__BMASK

	// buffer and clear flags immediately so we don't miss an interrupt while processing
	uint8_t flags = SCON0 & (UART0_RX_IF | UART0_TX_IF);
	SCON0 &= ~flags;

	// receiving byte
	if ((flags & SCON0_RI__SET))
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
	if ((flags & SCON0_TI__SET))
	{
		if (UART_Buffer_Write_Len > 0)
		{
			UART0_write(UART_TX_Buffer[UART_Buffer_Write_Position]);
			UART_Buffer_Write_Position++;
			UART_Buffer_Write_Len--;
            
            TX_Finished = false;
		} else {
			TX_Finished = true;
        }

		if (UART_Buffer_Write_Position == UART_TX_BUFFER_SIZE)
        {
			UART_Buffer_Write_Position = 0;
        }
	}
}

void UART0_init(UART0_RxEnable_t rxen, UART0_Width_t width, UART0_Multiproc_t mce)
{
    SCON0 &= ~(SCON0_SMODE__BMASK | SCON0_MCE__BMASK | SCON0_REN__BMASK);
    SCON0 = mce | rxen | width;
}

void UART0_initStdio(void)
{
    SCON0 |= SCON0_REN__RECEIVE_ENABLED | SCON0_TI__SET;
}

void UART0_initTxPolling(void)
{
    SCON0_TI = 1;
}

void UART0_write(uint8_t value)
{
	SBUF0 = value;
}

uint8_t UART0_read(void)
{
    return SBUF0;
}

#if EFM8PDL_UART0_USE_POLLED

    int putchar (int c)
    {
        // assumes UART is initialized
        while (!SCON0_TI);
        SCON0_TI = 0;
        SBUF0 = c;
        return c;
    }

#else

    int putchar (int c)
    {
        // basically a wrapper
        uart_putc(c);
        
        return c;
    }

#endif


bool is_uart_tx_finished(void)
{
    return TX_Finished;
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
    rxdata |= (lastRxError << 8);
    
    lastRxError = 0;
    
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
}

void uart_put_command(uint8_t command)
{
	uart_putc(RF_CODE_START);
	uart_putc(command);
	uart_putc(RF_CODE_STOP);
}

#if 1
void uart_put_RF_Data_Advanced(uint8_t Command, uint8_t protocol_index)
{
	uint8_t i = 0;
	uint8_t b = 0;
	uint8_t bits = 0;

	uart_putc(RF_CODE_START);
	uart_putc(Command);

	bits = PROTOCOL_DATA[protocol_index].bit_count;

	while(i < bits)
	{
		i += 8;
		b++;
	}

	uart_putc(b+1);

	// send index off this protocol
	uart_putc(protocol_index);

	// copy data to UART buffer
	i = 0;
	while(i < b)
	{
		uart_putc(RF_DATA[i]);
		i++;
	}
    
	uart_putc(RF_CODE_STOP);

}

#endif

void uart_put_RF_Data_Standard(uint8_t Command)
{
	uint8_t i = 0;
	uint8_t b = 0;

	uart_putc(RF_CODE_START);
	uart_putc(Command);

	// sync low time
	uart_putc((SYNC_LOW >> 8) & 0xFF);
	uart_putc(SYNC_LOW & 0xFF);
	// bit 0 high time
	uart_putc((BIT_LOW >> 8) & 0xFF);
	uart_putc(BIT_LOW & 0xFF);
	// bit 1 high time
	uart_putc((BIT_HIGH >> 8) & 0xFF);
	uart_putc(BIT_HIGH & 0xFF);

	// copy data to UART buffer
	i = 0;
	while(i < (24 / 8))
	{
		uart_putc(RF_DATA[i]);
		i++;
	}
    
	uart_putc(RF_CODE_STOP);
}

#if INCLUDE_BUCKET_SNIFFING == 1
void uart_put_RF_buckets(uint8_t Command)
{
	uint8_t i = 0;

	uart_putc(RF_CODE_START);
	uart_putc(Command);
    
	// put bucket count + sync bucket
	uart_putc(bucket_count + 1);

	// start and wait for transmit
	//UART0_initTxPolling();
	//uart_wait_until_TX_finished();

	// send up to 7 buckets
	while (i < bucket_count)
	{
		uart_putc((buckets[i] >> 8) & 0x7F);
		uart_putc(buckets[i] & 0xFF);
		i++;
	}

	// send sync bucket
	uart_putc((bucket_sync >> 8) & 0x7F);
	uart_putc(bucket_sync & 0xFF);

	// start and wait for transmit
	//UART0_initTxPolling();
	//uart_wait_until_TX_finished();

	i = 0;
    
	while(i < actual_byte)
	{
		uart_putc(RF_DATA[i]);
		i++;

		// be safe to have no buffer overflow
		//if ((i % UART_TX_BUFFER_SIZE) == 0)
		//{
		//	// start and wait for transmit
		//	UART0_initTxPolling();
		//	uart_wait_until_TX_finished();
		//}
	}

	uart_putc(RF_CODE_STOP);

}
#endif
