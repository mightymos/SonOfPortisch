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


// some operations set flag to true so that uart receiving is temporarily ignored
static bool ignoreUARTFlag = false;

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

    void serial_test(void)
    {
        efm8_delay_ms(500);
        
        led_toggle();
        
        //printf_tiny("loop\r\n");
		uart_putc(0xAA);
    }

#endif

void finish_command(uint8_t command)
{
	// send uart command
	uart_put_command(command);

	// enable UART again
	ignore_uart(false);

	// restart sniffing in its previous mode
	PCA0_DoSniffing(last_sniffing_command);
}

//-----------------------------------------------------------------------------
// main() Routine
// ----------------------------------------------------------------------------
void main (void)
{
    __xdata rf_sniffing_mode_t last_sniffing_mode = STANDARD;
    __xdata uint8_t tr_repeats = 0;
    __xdata uint16_t bucket = 0;
    
    //__xdata uint16_t index = 0;

	// FIXME: add comment
    //__xdata uint16_t idleResetCount = 0;

	// changed by external hardware, so must specify volatile type
	volatile unsigned int rxdata = UART_NO_DATA;

	// FIXME: add comment
	uint8_t len = 0;
	// FIXME: add comment
	uint8_t position = 0;

	// for buzzer (milliseconds)
	//const uint16_t startupDelay = 100;
	// longer for LED
	const uint16_t startupDelay = 3000;


	// call hardware initialization routine
	enter_DefaultMode_from_RESET();

	// enter default state
	led_on();
	buzzer_off();
	tdata_off();



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
#if (SNIFFING_ON)
	// set desired sniffing type to PT2260
	sniffing_mode = STANDARD;
	PCA0_DoSniffing(RF_CODE_RFIN);
	last_sniffing_command = RF_CODE_RFIN;
#else
	PCA0_StopSniffing();
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


#if 1
		// check if something got received by UART
		// only read data from uart if idle
		if (!is_uart_ignored())
        {
            //disable_global_interrupts();
            
			rxdata = uart_getc();
            
            //enable_global_interrupts();
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
			led_on();
#if 0
			// FIXME: the magic numbers make this difficult to understand
			if (uart_state == IDLE)
				idleResetCount = 0;
			else
			{
				if (++idleResetCount > 10000)
                {
					//buzzer_on();
					led_on();
                }
			
				if (idleResetCount > 30000)
				{
					idleResetCount = 0;
					uart_state = IDLE;
					uart_command = NONE;
					//buzzer_off();
					led_off();
				}
			}
#endif
		}
		else
		{
			// debug: echo sent character
			uart_putc(rxdata & 0xff);

			// FIXME: add comment
			//idleResetCount = 0;

			// state machine for UART
			switch(uart_state)
			{
				// check if UART_SYNC_INIT got received
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
							//buzzer_on();
							//FIXME: seems to make more sense to just delay()
							//InitTimer3_ms(1, 50);
							//buzzer_on();
							// wait until timer has finished
							//WaitTimer3Finished();
							//efm8_delay_ms(50);
							//buzzer_off();

							// set desired RF protocol PT2260
							sniffing_mode = STANDARD;
							last_sniffing_command = PCA0_DoSniffing(RF_CODE_LEARN);

							// FIXME: need another way to time out since Timer3 resource removed
							// start timeout timer
							//InitTimer3_ms(1, 30000);
							break;
						case RF_CODE_RFOUT:
							// stop sniffing while handling received data
							PCA0_StopSniffing();
							uart_state = RECEIVING;
							tr_repeats = RF_TRANSMIT_REPEATS;
							position = 0;
							len = 9;
							break;
						case RF_DO_BEEP:
							// stop sniffing while handling received data
							PCA0_StopSniffing();
							uart_state = RECEIVING;
							position = 0;
							len = 2;
							break;
						case RF_ALTERNATIVE_FIRMWARE:
							break;
						case RF_CODE_SNIFFING_ON:
							sniffing_mode = ADVANCED;
							PCA0_DoSniffing(RF_CODE_SNIFFING_ON);
							last_sniffing_command = RF_CODE_SNIFFING_ON;
							break;
						case RF_CODE_SNIFFING_OFF:
							// set desired RF protocol PT2260
							sniffing_mode = STANDARD;
							// re-enable default RF_CODE_RFIN sniffing
							PCA0_DoSniffing(RF_CODE_RFIN);
							last_sniffing_command = RF_CODE_RFIN;
							break;
						case RF_CODE_RFOUT_NEW:
							tr_repeats = RF_TRANSMIT_REPEATS;
#if INCLUDE_BUCKET_SNIFFING == 0
							uart_state = RECEIVE_LEN;
							break;
#else
							/* no break */
						case RF_CODE_RFOUT_BUCKET:
							uart_state = RECEIVE_LEN;
							break;
						case RF_CODE_SNIFFING_ON_BUCKET:
							last_sniffing_command = PCA0_DoSniffing(RF_CODE_SNIFFING_ON_BUCKET);
							break;
#endif
						case RF_CODE_LEARN_NEW:
							//buzzer_on();
							//InitTimer3_ms(1, 50);
							//buzzer_on();
							// wait until timer has finished
							//WaitTimer3Finished();
							//efm8_delay_ms(50);
							//buzzer_off();

							// enable sniffing for all known protocols
							last_sniffing_mode = sniffing_mode;
							sniffing_mode = ADVANCED;
							last_sniffing_command = PCA0_DoSniffing(RF_CODE_LEARN_NEW);

							// start timeout timer
							//InitTimer3_ms(1, 30000);
							break;
						case RF_CODE_ACK:
							// re-enable default RF_CODE_RFIN sniffing
							last_sniffing_command = PCA0_DoSniffing(last_sniffing_command);
							uart_state = IDLE;
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
					len = rxdata & 0xFF;
					if (len > 0)
					{
						// stop sniffing while handling received data
						PCA0_StopSniffing();
						uart_state = RECEIVING;
					}
					else
						uart_state = SYNC_FINISH;
					break;

				// Receiving UART data
				case RECEIVING:
					RF_DATA[position] = rxdata & 0xFF;
					position++;

					if (position == len)
					{
						uart_state = SYNC_FINISH;
					}
					else if (position >= RF_DATA_BUFFERSIZE)
					{
						len = RF_DATA_BUFFERSIZE;
						uart_state = SYNC_FINISH;
					}
					break;

				// wait and check for UART_SYNC_END
				case SYNC_FINISH:
					if ((rxdata & 0xFF) == RF_CODE_STOP)
					{
						uart_state = IDLE;
						ignore_uart(true);

						// check if ACK should be sent
						switch(uart_command)
						{
							case RF_CODE_LEARN:
							case RF_CODE_SNIFFING_ON:
							case RF_CODE_SNIFFING_OFF:
							case RF_CODE_RFIN:
#if INCLUDE_BUCKET_SNIFFING == 1
							case RF_CODE_SNIFFING_ON_BUCKET:
#endif
								// send acknowledge
								uart_put_command(RF_CODE_ACK);
							case RF_CODE_ACK:
								// enable UART again
								ignore_uart(false);
								break;
#if INCLUDE_BUCKET_SNIFFING == 1
							case RF_CODE_RFOUT_BUCKET:
								tr_repeats = RF_DATA[1] + 1;
								break;
#endif
						}
					}
					break;
			}
		}

		/*------------------------------------------
		 * check command byte
		 ------------------------------------------*/
		switch(uart_command)
		{
			// do original learning, new RF code learning
			case RF_CODE_LEARN:
			case RF_CODE_LEARN_NEW:

				// check if a RF signal got decoded
				if ((RF_DATA_STATUS & RF_DATA_RECEIVED_MASK) != 0)
				{
					//buzzer_on();
					//InitTimer3_ms(1, 200);
					//buzzer_on();
					// wait until timer has finished
					//WaitTimer3Finished();
					//efm8_delay_ms(200);
					//buzzer_off();

					switch(uart_command)
					{
						case RF_CODE_LEARN:
							PCA0_DoSniffing(last_sniffing_command);
							uart_put_RF_Data_Standard(RF_CODE_LEARN_OK);
							break;

						case RF_CODE_LEARN_NEW:
							sniffing_mode = last_sniffing_mode;
							PCA0_DoSniffing(last_sniffing_command);
							uart_put_RF_Data_Advanced(RF_CODE_LEARN_OK_NEW, RF_DATA_STATUS & 0x7F);
							break;
					}

					// clear RF status
					RF_DATA_STATUS = 0;

					// enable interrupt for RF receiving
					PCA0CPM0 |= PCA0CPM0_ECCF__ENABLED;

					// enable UART again
					ignore_uart(false);
				}
				// FIXME: need another timer resource
				// check for learning timeout
				//else if (IsTimer3Finished())
				else if (0)
				{
					//buzzer_on();
					//InitTimer3_ms(1, 1000);
					//buzzer_on();
					// wait until timer has finished
					//WaitTimer3Finished();
					//efm8_delay_ms(100);
					//buzzer_off();

					// send not-acknowledge
					switch(uart_command)
					{
						case RF_CODE_LEARN:
							finish_command(RF_CODE_LEARN_KO);
							break;

						case RF_CODE_LEARN_NEW:
							finish_command(RF_CODE_LEARN_KO_NEW);
							break;
					}
				}
				else
				{
					// handle new received buckets
					if (buffer_out(&bucket))
                    {
						HandleRFBucket(bucket & 0x7FFF, (bool)((bucket & 0x8000) >> 15));
                    }
				}
				break;

			// do original sniffing
			case RF_CODE_RFIN:
			case RF_CODE_SNIFFING_ON:

				// check if a RF signal got decoded
				if ((RF_DATA_STATUS & RF_DATA_RECEIVED_MASK) != 0)
				{
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
					// handle new received buckets
					if (buffer_out(&bucket))
                    {
						HandleRFBucket(bucket & 0x7FFF, (bool)((bucket & 0x8000) >> 15));
                    }
				}
				break;

			// do original transfer
			case RF_CODE_RFOUT:
				// only do the job if all data got received by UART
				if (uart_state != IDLE)
					break;

				// do transmit of the data
				switch(rf_state)
				{
					// init and start RF transmit
					case RF_IDLE:
						tr_repeats--;
						PCA0_StopSniffing();

						// byte 0..1:	Tsyn
						// byte 2..3:	Tlow
						// byte 4..5:	Thigh
						// byte 6..7:	24bit Data

						buckets[0] = *(uint16_t *)&RF_DATA[2];
						buckets[1] = *(uint16_t *)&RF_DATA[4];
						buckets[2] = *(uint16_t *)&RF_DATA[0];

						SendBuckets(
								buckets,
								PROTOCOL_DATA[0].start.dat, PROTOCOL_DATA[0].start.size,
								PROTOCOL_DATA[0].bit0.dat, PROTOCOL_DATA[0].bit0.size,
								PROTOCOL_DATA[0].bit1.dat, PROTOCOL_DATA[0].bit1.size,
								PROTOCOL_DATA[0].end.dat, PROTOCOL_DATA[0].end.size,
								PROTOCOL_DATA[0].bit_count,
								RF_DATA + 6
								);
                                
						break;

					// wait until data got transfered
					case RF_FINISHED:
						if (tr_repeats == 0)
						{
							// disable RF transmit
							tdata_off();

							finish_command(RF_CODE_ACK);
						}
						else
							rf_state = RF_IDLE;
						break;
				}
				break;

			// do a beep
			case RF_DO_BEEP:
				// only do the job if all data got received by UART
				if (uart_state != IDLE)
					break;

				//InitTimer3_ms(1, *(uint16_t *)&RF_DATA[0]);
				//buzzer_on();
				// wait until timer has finished
				//WaitTimer3Finished();
				//efm8_delay_ms(*(uint16_t *)&RF_DATA[0]);
				//buzzer_off();

				// send acknowledge
				finish_command(RF_CODE_ACK);
				break;

			// host was requesting the firmware version
			case RF_ALTERNATIVE_FIRMWARE:

				// send firmware version
				finish_command(FIRMWARE_VERSION);
				break;

			// transmit data on RF
			case RF_CODE_RFOUT_NEW:
				// only do the job if all data got received by UART
				if (uart_state != IDLE)
					break;

				// do transmit of the data
				switch(rf_state)
				{
					// init and start RF transmit
					case RF_IDLE:
						tr_repeats--;
						PCA0_StopSniffing();

						// byte 0:		PROTOCOL_DATA index
						// byte 1..:	Data

						SendBucketsByIndex(RF_DATA[0], RF_DATA + 1);
						break;

					// wait until data got transfered
					case RF_FINISHED:
						if (tr_repeats == 0)
						{
							// disable RF transmit
							tdata_off();

							// send acknowledge
							finish_command(RF_CODE_ACK);
						}
						else
						{
							rf_state = RF_IDLE;
						}
						break;
				}
				break;

#if INCLUDE_BUCKET_SNIFFING == 1
			case RF_CODE_RFOUT_BUCKET:
			{
				// only do the job if all data got received by UART
				if (uart_state != IDLE)
					break;

				// do transmit of the data
				switch(rf_state)
				{
					// init and start RF transmit
					case RF_IDLE:
						tr_repeats--;
						PCA0_StopSniffing();

						// byte 0:				number of buckets: k
						// byte 1:				number of repeats: r
						// byte 2*(1..k):		bucket time high
						// byte 2*(1..k)+1:		bucket time low
						// byte 2*k+2..N:		RF buckets to send
						SendRFBuckets((uint16_t *)(RF_DATA + 2), RF_DATA + (RF_DATA[0] << 1) + 2, len - 2 - (RF_DATA[0] << 1));
						break;

					// wait until data got transfered
					case RF_FINISHED:
						if (tr_repeats == 0)
						{
							// disable RF transmit
							tdata_off();

							// send acknowledge
							finish_command(RF_CODE_ACK);
						}
						else
						{
							rf_state = RF_IDLE;
						}
						break;
				}
				break;
			}
		case RF_CODE_SNIFFING_ON_BUCKET:

			// check if a RF signal got decoded
			if ((RF_DATA_STATUS & RF_DATA_RECEIVED_MASK) != 0)
			{
				uart_put_RF_buckets(RF_CODE_SNIFFING_ON_BUCKET);

				// clear RF status
				RF_DATA_STATUS = 0;

				// enable interrupt for RF receiving
				PCA0CPM0 |= PCA0CPM0_ECCF__ENABLED;
			}
			else
			{
				// do bucket sniffing handling
				if (buffer_out(&bucket))
                {
					Bucket_Received(bucket & 0x7FFF, (bool)((bucket & 0x8000) >> 15));
                }
			}

			break;
#endif
		} //switch(uart_command)
	} //while (1)
}
