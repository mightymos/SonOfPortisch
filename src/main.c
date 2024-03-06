//=========================================================
// Jonathan Armstrong - Port to SDCC compiler
// Silicon Labs IDE 5.50.00 seems to the last IDE that makes compiling with SDCC possible
//=========================================================

//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------
// SFR declarations
// FIXME: eventually it seems like we would like to use definitions shipped with compiler
#include <SI_EFM8BB1_Register_Enums.h> 
//#include <EFM8BB1.h>

#include "efm8_config.h"


// for printf_tiny()
//#include <stdio.h>

#include "delay.h"
#include "globals.h"
#include "InitDevice.h"
#include "RF_Handling.h"
#include "RF_Protocols.h"
#include "serial.h"
#include "uart.h"

#include "pca_0.h"
//#include "uart_0.h"
//#include "wdt_0.h"

uint8_t uartPacket[10];

// some operations set flag to true so that uart receiving is temporarily ignored
volatile static bool ignoreUARTFlag = false;

// sdcc manual section 3.8.1 general information
// requires interrupt definition to appear or be included in main
void UART0_ISR(void) __interrupt (UART0_IRQn);

void PCA0_ISR(void)   __interrupt (PCA0_IRQn);
void TIMER2_ISR(void) __interrupt (TIMER2_IRQn);
//void TIMER3_ISR(void) __interrupt (TIMER3_IRQn);

//-----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// This function is called immediately after reset, before the initialization
// code (which runs before main() ). This is a
// useful place to disable the watchdog timer, which is enable by default
// and may trigger before main() in some instances.
//-----------------------------------------------------------------------------
unsigned char __sdcc_external_startup(void)
{
    // pg. 218, sec. 20.3 disable watchdog timer
    IE_EA = 0;
    WDTCN = 0xDE;
    WDTCN = 0xAD;
    
    // re-enable interrupts
    //IE_EA = 1
    
    return 0;
}

inline void ignore_uart(const bool ignore)
{
    ignoreUARTFlag = ignore;
}

inline bool is_uart_ignored(void)
{
    return ignoreUARTFlag;
}



#if 0

void serial_loopback(void)
{
	volatile unsigned int rxdata = UART_NO_DATA;

	// check if serial transmit buffer is empty
	if(!is_uart_tx_buffer_empty())
	{
	    if (is_uart_tx_finished())
	    {
	        // if not empty, set TI, which triggers interrupt to actually transmit
	        UART0_initTxPolling();
	    }
	}

	// check if something got received by UART
	// only read data from uart if idle
	if (!is_uart_ignored())
    {
		rxdata = uart_getc();
	} else {
		rxdata = UART_NO_DATA;
    }

	if (rxdata == UART_NO_DATA)
	{
		led_on();
	} else {
		uart_putc(rxdata & 0xff);
	}
    
}

#endif


void finish_command(uint8_t command)
{
	// send uart command
	uart_put_command(command);

	// enable UART again
	//ignore_uart(false);

	// FIXME: this is confusing because last_sniffing_command seems to be associated with uart state
	// restart sniffing in its previous mode
	//PCA0_DoSniffing();
	//rf_state = RF_IDLE;
}

void uart_state_machine(const unsigned int rxdata)
{
	// debug: echo sent character
	//uart_putc(rxdata & 0xff);

	// FIXME: add comment
	//idleResetCount = 0;

	__xdata rf_state_t rf_state;

    __xdata uint8_t tr_repeats = 0;

	// need to specify volatile so optimizer does not remove
	// given that it is set by external data input to uart
	volatile __xdata uint8_t packetLength = 0;
	// FIXME: add comment
	__xdata uint8_t position = 0;

	// state machine for UART
	switch(uart_state)
	{
		// check if start sequence got received
		case IDLE:
			if ((rxdata & 0xFF) == RF_CODE_START)
            {
				uart_state = SYNC_INIT;
            }
			break;

		// sync byte got received, read command
		case SYNC_INIT:
			uart_command = rxdata & 0xFF;
			uart_state = SYNC_FINISH;

			// check if some data needs to be received
			switch(uart_command)
			{
				case RF_CODE_LEARN:
					break;
				case RF_CODE_RFOUT:
					// stop sniffing while handling received data
					//PCA0_StopSniffing();
					//rf_state = RF_IDLE;
					//uart_state = RECEIVING;
					//tr_repeats = RF_TRANSMIT_REPEATS;
					//position = 0;
					//packetLength = 9;
					break;
				case RF_DO_BEEP:
					// stop sniffing while handling received data
					//PCA0_StopSniffing();
					//rf_state = RF_IDLE;
					uart_state = RECEIVING;
					position = 0;
					packetLength = 2;
					break;
				case RF_ALTERNATIVE_FIRMWARE:
					break;
				case RF_CODE_SNIFFING_ON:
					//sniffing_mode = ADVANCED;
					//PCA0_DoSniffing();
					//last_sniffing_command = RF_CODE_SNIFFING_ON;
					//rf_state = RF_IDLE;
					break;
				case RF_CODE_SNIFFING_OFF:
					// set desired RF protocol PT2260
					//sniffing_mode = STANDARD;
					// re-enable default RF_CODE_RFIN sniffing
					//PCA0_DoSniffing();
					//last_sniffing_command = RF_CODE_RFIN;
					//rf_state = RF_IDLE;
					break;
				case RF_CODE_RFOUT_NEW:
					//tr_repeats = RF_TRANSMIT_REPEATS;
					// no break
				case RF_CODE_RFOUT_BUCKET:
					//uart_state = RECEIVE_LEN;
					break;
				case RF_CODE_SNIFFING_ON_BUCKET:
					//last_sniffing_command = PCA0_DoSniffing();
					//rf_state = RF_IDLE;
					break;
				case RF_CODE_LEARN_NEW:
					break;
				case RF_CODE_ACK:
					// re-enable default RF_CODE_RFIN sniffing
					//last_sniffing_command = PCA0_DoSniffing();
					uart_state = IDLE;
					rf_state = RF_IDLE;
					break;

				// unknown command
				default:
					uart_command = NONE;
					uart_state = IDLE;
					break;
			}
			break;

		// Receiving UART data length
		case RECEIVE_LEN:
			position = 0;
			packetLength = rxdata & 0xFF;
			if (packetLength > 0)
			{
				// stop sniffing while handling received data
				PCA0_StopSniffing();
				rf_state = RF_IDLE;
				uart_state = RECEIVING;
			} else {
				uart_state = SYNC_FINISH;
			}
			break;

		// Receiving UART data
		case RECEIVING:
			uartPacket[position] = rxdata & 0xFF;
			position++;

			if (position >= packetLength)
			{
				uart_state = SYNC_FINISH;
			}
			break;

		// wait and check for UART_SYNC_END
		case SYNC_FINISH:
			if ((rxdata & 0xFF) == RF_CODE_STOP)
			{
				uart_state = IDLE;
				//ignore_uart(true);

				// check if ACK should be sent
				switch(uart_command)
				{
					case RF_CODE_LEARN:
					case RF_CODE_SNIFFING_ON:
					case RF_CODE_SNIFFING_OFF:
					case RF_CODE_RFIN:
					case RF_CODE_SNIFFING_ON_BUCKET:
						// send acknowledge
						uart_put_command(RF_CODE_ACK);
					case RF_CODE_ACK:
						// enable UART again
						//ignore_uart(false);
						break;
					case RF_CODE_RFOUT_BUCKET:
						tr_repeats = uartPacket[1] + 1;
						break;
				}
			}
			break;
	}
}

//-----------------------------------------------------------------------------
// main() Routine
// ----------------------------------------------------------------------------
void main (void)
{
	// for buzzer (milliseconds)
	//const uint16_t startupDelay = 100;
	// longer for LED
	//__code uint16_t startupDelay = 3000;

	// changed by external hardware, so must specify volatile type so not optimized out
	__xdata volatile unsigned int rxdata = UART_NO_DATA;


	__xdata rf_state_t rf_state = RF_IDLE;
    __xdata rf_sniffing_mode_t last_sniffing_mode = STANDARD;
    __xdata uint16_t bucket = 0;
    
    //__xdata uint16_t index = 0;

	// FIXME: add comment
    __xdata uint16_t idleResetCount = 0;

	__xdata bool result;


	// call hardware initialization routine
	enter_DefaultMode_from_RESET();

	// set default pin states
	led_on();
	buzzer_off();
	tdata_off();


	// DEBUG:
	// enable interrupt for RF receiving
	//PCA0CPM0 |= PCA0CPM0_ECCF__ENABLED;
	// start PCA
	//PCA0_run();
	
	debug_pin0_off();



// baud rate is 19200, 8 data bits, 1 stop bit, no parity
#if defined(EFM8PDL_UART0_USE_POLLED)
	#if EFM8PDL_UART0_USE_POLLED
    	// basically sets TI flag so putchar() does not get stuck in an infinite loop
    	UART0_initStdio();
	#else
		// enable uart
		UART0_init(UART0_RX_ENABLE, UART0_WIDTH_8, UART0_MULTIPROC_DISABLE);
	#endif
#else
	#error Please define EFM8PDL_UART0_USE_POLLED as 0 or 1.
#endif
    


	// start sniffing if enabled by default
#if (SNIFFING_ON_AT_STARTUP)
	// set desired sniffing type to PT2260
	sniffing_mode = STANDARD;
	PCA0_DoSniffing();
	rf_state = RF_IDLE;

	// FIXME: hack since we remove last_sniffing_command
	//last_sniffing_command = RF_CODE_RFIN;
	uart_command = RF_CODE_RFIN;
#else
	PCA0_StopSniffing();
	rf_state = RF_IDLE;
#endif

#if 0
    // startup buzzer (can be annoying during development)
	// use LED instead (for development)
	//buzzer_on();
	led_on();
	efm8_delay_ms(startupDelay);

	//buzzer_off();
	led_off();
#endif


#if 0
    // test uart transmitting in polled mode
    while (true)
    {
        serial_test();
    }
#endif

    
	// enable global interrupts
	enable_global_interrupts();
    

    
    // startup
    //requires code and memory space, which is in short supply
    //but good to check that polled uart is working
    //printf_tiny("startup...\r\n");
    //uart_put_command(RF_CODE_ACK);

	while (true)
	{
		// reset Watch Dog Timer
		//WDT0_feed();
#if 0
		// DEBUG: infinite loop to echo back serial characters
		while (true)
		{
			serial_loopback();
		}
#endif

#if 1
		// check if something got received by UART
		// only read data from uart if idle
		if (!is_uart_ignored())
        {
			rxdata = uart_getc();
		} else {
			rxdata = UART_NO_DATA;
        }
        
#endif
        
        

#if 1
        // check if serial transmit buffer is empty
        if(!is_uart_tx_buffer_empty())
        {
            if (is_uart_tx_finished())
            {
                // if not empty, set TI, which triggers interrupt to actually transmit
                UART0_initTxPolling();
            }
        }
        
#endif



		if (rxdata == UART_NO_DATA)
		{

#if 0
			// FIXME: the magic numbers make this difficult to understand
			// but seems to reset uart if it sits in non-idle state
			// for too long without receiving any more data
			if (uart_state == IDLE)
				idleResetCount = 0;
			else
			{
				idleResetCount += 1;
			
				if (idleResetCount > 30000)
				{
					idleResetCount = 0;
					uart_state = IDLE;
					uart_command = NONE;
				}
			}
#endif
		}
		else
		{
			uart_state_machine(rxdata);
		}

		/*------------------------------------------
		 * check command byte
		 ------------------------------------------*/
		switch(uart_command)
		{
			case NONE:
				break;
			// do original sniffing
			case RF_CODE_RFIN:
			case RF_CODE_SNIFFING_ON:

				// check if a RF signal got decoded
				if ((RF_DATA_STATUS & RF_DATA_RECEIVED_MASK) != 0)
				{

					// DEBUG:
					if (rdata_level())
					{
						debug_pin1_on();
					} else {
						debug_pin1_off();
					}

					switch(uart_command)
					{
						case RF_CODE_RFIN:
							uart_put_RF_Data_Standard(RF_CODE_RFIN);
							break;

						case RF_CODE_SNIFFING_ON:
							uart_put_RF_Data_Advanced(RF_CODE_SNIFFING_ON, RF_DATA_STATUS & 0x7F);
							break;
					}

					// clear RF status
					RF_DATA_STATUS = 0;

					// enable interrupt for RF receiving
					PCA0CPM0 |= PCA0CPM0_ECCF__ENABLED;
				}
				else
				{
					// disable interrupt for RF receiving while reading buffer
					PCA0CPM0 &= ~PCA0CPM0_ECCF__ENABLED;
					result = buffer_out(&bucket);
					// FIXME: reenable (should store previous and just restore that?)
					PCA0CPM0 |= PCA0CPM0_ECCF__ENABLED;
					// handle new received buckets
					if (result)
                    {
						HandleRFBucket(bucket & 0x7FFF, (bool)((bucket & 0x8000) >> 15));
                    }
				}
				break;

			// do a beep
			case RF_DO_BEEP:
				// only do the job if all data got received by UART
				if (uart_state != IDLE)
					break;

				// this is blocking unfortunately
				//buzzer_on();
				//efm8_delay_ms(*(uint16_t *)&RF_DATA[0]);
				//buzzer_off();

				// send acknowledge
				finish_command(RF_CODE_ACK);
				uart_put_rf_human_readable();
				uart_command = NONE;
				break;

			// host was requesting the firmware version
			case RF_ALTERNATIVE_FIRMWARE:

				// send firmware version
				finish_command(FIRMWARE_VERSION);
				uart_command = NONE;
				break;

			default:
				// FIXME: not sure if this makes sense
				uart_command = NONE;
			break;
		} //switch(uart_command)
	} //while (1)
}
