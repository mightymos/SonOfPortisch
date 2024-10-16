/*
 * uart.h
 *
 *  Created on: 28.11.2017
 *      Author:
 */

#ifndef INC_UART_H_
#define INC_UART_H_

#include <stdint.h>
#include <EFM8BB1.h>

#include "RF_Config.h"

//-----------------------------------------------------------------------------
// Global Constants
//-----------------------------------------------------------------------------

#define UART_RX_BUFFER_SIZE	32
#define UART_TX_BUFFER_SIZE	96

/*
** high byte error return code of uart_getc()
*/
#define UART_FRAME_ERROR      0x1000              /* Framing Error by UART       */
#define UART_OVERRUN_ERROR    0x0800              /* Overrun condition by UART   */
#define UART_PARITY_ERROR     0x0400              /* Parity Error by UART        */
#define UART_BUFFER_OVERFLOW  0x0200              /* receive ringbuffer overflow */
#define UART_NO_DATA          0x0100              /* no receive data available   */


/// UART transfer width enums.
typedef enum
{
  UART0_WIDTH_8 = SMODE__8_BIT, //!< UART in 8-bit mode.
  UART0_WIDTH_9 = SMODE__9_BIT, //!< UART in 9-bit mode.
} UART0_Width_t;

/// UART Multiprocessor support enums.
typedef enum
{
  UART0_MULTIPROC_DISABLE = MCE__MULTI_DISABLED, //!< UART Multiprocessor communication Disabled.
  UART0_MULTIPROC_ENABLE  = MCE__MULTI_ENABLED,  //!< UART Multiprocessor communication Enabled.
} UART0_Multiproc_t;

/// UART RX support enums
typedef enum
{
  UART0_RX_ENABLE  = REN__RECEIVE_ENABLED,   //!< UART Receive Enabled.
  UART0_RX_DISABLE = REN__RECEIVE_DISABLED,  //!< UART Receive Disabled.
} UART0_RxEnable_t;

/***************************************************************************//**
 * @brief
 * Initialize the UART
 *
 * @param rxen:
 * Receive enable status.
 * @param width:
 * Data word width.
 * @param mce:
 * Multiprocessor mode status.
 *
 ******************************************************************************/
void UART0_init(UART0_RxEnable_t rxen, UART0_Width_t width, UART0_Multiproc_t mce);
void UART0_initStdio(void);

void UART0_initTxPolling(void);

void    UART0_write(uint8_t value);
uint8_t UART0_read(void);


bool is_uart_tx_finished(void);
bool is_uart_tx_buffer_empty(void);


extern unsigned int uart_getc(void);
extern void         uart_putc(uint8_t txdata);

void putstring(const char *s);
void puthex(unsigned char v);
void puthex2(const unsigned char x);


#endif // INC_UART_H_
