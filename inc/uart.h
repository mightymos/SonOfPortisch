/*
 * uart.h
 *
 *  Created on: 28.11.2017
 *      Author:
 */

#ifndef INC_UART_H_
#define INC_UART_H_

#include "RF_Config.h"

//-----------------------------------------------------------------------------
// Global Constants
//-----------------------------------------------------------------------------
#define RF_CODE_START		0xAA
#define RF_CODE_STOP		0x55

#define UART_RX_BUFFER_SIZE	64
#define UART_TX_BUFFER_SIZE	32

/*
** high byte error return code of uart_getc()
*/
#define UART_FRAME_ERROR      0x1000              /* Framing Error by UART       */
#define UART_OVERRUN_ERROR    0x0800              /* Overrun condition by UART   */
#define UART_PARITY_ERROR     0x0400              /* Parity Error by UART        */
#define UART_BUFFER_OVERFLOW  0x0200              /* receive ringbuffer overflow */
#define UART_NO_DATA          0x0100              /* no receive data available   */
//-----------------------------------------------------------------------------
// Global Enums
//-----------------------------------------------------------------------------
typedef enum
{
	IDLE,
	SYNC_INIT,
	SYNC_FINISH,
	RECEIVE_LEN,
	RECEIVING,
	TRANSMIT,
	COMMAND
} uart_state_t;

typedef enum
{
	NONE = 0x00,
	RF_CODE_ACK = 0xA0,
	RF_CODE_LEARN = 0xA1,
	RF_CODE_LEARN_KO = 0xA2,
	RF_CODE_LEARN_OK = 0xA3,
	RF_CODE_RFIN = 0xA4,
	RF_CODE_RFOUT = 0xA5,
	RF_CODE_SNIFFING_ON = 0xA6,
	RF_CODE_SNIFFING_OFF = 0xA7,
	RF_CODE_RFOUT_NEW = 0xA8,
	RF_CODE_LEARN_NEW = 0xA9,
	RF_CODE_LEARN_KO_NEW = 0xAA,
	RF_CODE_LEARN_OK_NEW = 0xAB,
#if INCLUDE_BUCKET_SNIFFING == 1
	RF_CODE_RFOUT_BUCKET = 0xB0,
	RF_CODE_SNIFFING_ON_BUCKET = 0xB1,
#endif
	RF_DO_BEEP = 0xC0,
	RF_ALTERNATIVE_FIRMWARE = 0xFF
} uart_command_t;


//-----------------------------------------------------------------------------
// Global Variables
//-----------------------------------------------------------------------------
extern __xdata uart_state_t uart_state;
extern __xdata uart_command_t uart_command;

/// UART transfer width enums.
typedef enum
{
  UART0_WIDTH_8 = SCON0_SMODE__8_BIT, //!< UART in 8-bit mode.
  UART0_WIDTH_9 = SCON0_SMODE__9_BIT, //!< UART in 9-bit mode.
} UART0_Width_t;

/// UART Multiprocessor support enums.
typedef enum
{
  UART0_MULTIPROC_DISABLE = SCON0_MCE__MULTI_DISABLED, //!< UART Multiprocessor communication Disabled.
  UART0_MULTIPROC_ENABLE  = SCON0_MCE__MULTI_ENABLED,  //!< UART Multiprocessor communication Enabled.
} UART0_Multiproc_t;

/// UART RX support enums
typedef enum
{
  UART0_RX_ENABLE  = SCON0_REN__RECEIVE_ENABLED,   //!< UART Receive Enabled.
  UART0_RX_DISABLE = SCON0_REN__RECEIVE_DISABLED,  //!< UART Receive Disabled.
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
void UART0_initTxPolling(void);
void UART0_write(uint8_t value);
uint8_t UART0_read(void);


extern unsigned int uart_getc(void);
extern void uart_put_command(uint8_t command);
extern void uart_put_RF_Data_Advanced(uint8_t Command, uint8_t protocol_index);
extern void uart_put_RF_Data_Standard(uint8_t Command);
#if INCLUDE_BUCKET_SNIFFING == 1
extern void uart_put_RF_buckets(uint8_t Command);
#endif

#endif // INC_UART_H_
