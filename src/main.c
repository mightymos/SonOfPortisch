//=========================================================
// Jonathan Armstrong - Port to SDCC compiler
// Silicon Labs IDE 5.50.00 seems to the last IDE that makes compiling with SDCC possible
//=========================================================

//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------
// SFR declarations
#include <stdint.h>
#include <EFM8BB1.h>

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

// uart state machine
__xdata uart_state_t uart_state = IDLE;
__xdata uart_command_t uart_command = NONE;
__xdata uart_command_t last_sniffing_command;
__xdata uint8_t uartPacket[10];

// used for count down radio transmissions

__xdata uint8_t tr_repeats = 0;

// sdcc manual section 3.8.1 general information
// requires interrupt definition to appear or be included in main
void UART0_ISR(void) __interrupt (4);
void TIMER2_ISR(void) __interrupt (5);
void PCA0_ISR(void)   __interrupt (11);
//void TIMER3_ISR(void) __interrupt (14);

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
    EA = 0;
    WDTCN = 0xDE;
    WDTCN = 0xAD;
    
    // re-enable interrupts
    //IE_EA = 1
    
    return 0;
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



void uart_state_machine(const unsigned int rxdata)
{
	//
	static __xdata uint8_t packetLength = 0;

	// FIXME: add comment
	static __xdata uint8_t position = 0;

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
					PCA0_StopSniffing();
					rf_state = RF_IDLE;
					uart_state = RECEIVING;
					tr_repeats = RF_TRANSMIT_REPEATS;
					position = 0;
					packetLength = 9;
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
					sniffing_mode = ADVANCED;
					PCA0_DoSniffing();
					last_sniffing_command = RF_CODE_SNIFFING_ON;
					rf_state = RF_IDLE;
					break;
				case RF_CODE_SNIFFING_OFF:
					// set desired RF protocol PT2260
					sniffing_mode = STANDARD;
					// re-enable default RF_CODE_RFIN sniffing
					PCA0_DoSniffing();
					uart_command          = RF_CODE_RFIN;
					last_sniffing_command = RF_CODE_RFIN;
					rf_state = RF_IDLE;
					break;
				case RF_CODE_RFOUT_NEW:
					//tr_repeats = RF_TRANSMIT_REPEATS;
					// no break
				case RF_CODE_RFOUT_BUCKET:
					//uart_state = RECEIVE_LEN;
					break;
				case RF_CODE_SNIFFING_ON_BUCKET:
					PCA0_DoSniffing();
					last_sniffing_command = RF_CODE_SNIFFING_ON_BUCKET;
					rf_state = RF_IDLE;
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

			// FIXME: should look for buffer overflow as Portisch did
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
						break;
					case RF_CODE_RFOUT_BUCKET:
						tr_repeats = uartPacket[1] + 1;
						break;
				}
			}
			break;
	}
}

bool radio_state_machine(void)
{
	bool completed = false;

	// DEBUG:
	uint16_t buckets_dummy[3] = {350, 1050, 10850};
	uint8_t  packet_dummy[3]  = {0xDE, 0xAD, 0xBE};

	// FIXME: need to troubleshoot sending data received by web interface
	//uint16_t buckets[3];

	// helps allow sendbuckets call to be more readable
	uint8_t start_size;
	uint8_t bit0_size;
	uint8_t bit1_size;
	uint8_t end_size;
	uint8_t bitcount;

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
			// byte 6..8:	24bit Data
			//buckets[0] = *(uint16_t *)&uartPacket[2];
			//buckets[1] = *(uint16_t *)&uartPacket[4];
			//buckets[2] = *(uint16_t *)&uartPacket[0];
			
			// help make function call more readable
			start_size = PROTOCOL_DATA[0].start.size;
			bit0_size  = PROTOCOL_DATA[0].bit0.size;
			bit1_size  = PROTOCOL_DATA[0].bit1.size;
			end_size   = PROTOCOL_DATA[0].end.size;
			bitcount   = PROTOCOL_DATA[0].bit_count;

			//puthex2((buckets[0] >> 8) & 0xff);
			//puthex2((buckets[1] >> 8) & 0xff);
			//puthex2((buckets[2] >> 8) & 0xff);
			SendBuckets(buckets_dummy, PROTOCOL_DATA[0].start.dat, start_size, PROTOCOL_DATA[0].bit0.dat, bit0_size, PROTOCOL_DATA[0].bit1.dat, bit1_size,PROTOCOL_DATA[0].end.dat, end_size, bitcount, packet_dummy);
			
			// ping pong between idle and finished state until we reach zero repeat index
			rf_state = RF_FINISHED;
			
			break;

		// wait until data got transfered
		case RF_FINISHED:
			if (tr_repeats == 0)
			{
				// disable RF transmit
				tdata_off();

				// send uart command
				uart_put_command(RF_CODE_ACK);

				completed = true;
			} else {
				rf_state = RF_IDLE;
			}
			break;
	}

	return completed;
}

//-----------------------------------------------------------------------------
// main() Routine
// ----------------------------------------------------------------------------
void main (void)
{
	// for buzzer (milliseconds)
	//const uint16_t startupDelay = 100;
	// longer for LED
	const uint16_t startupDelay = 3000;

	// changed by external hardware, so must specify volatile type so not optimized out
	volatile unsigned int rxdata = UART_NO_DATA;

	// FIXME: add comment
    uint16_t bucket = 0;
    

	// FIXME: add comment
    __xdata uint16_t idleResetCount = 0;

	// prefer bool type in internel ram to take advantage of bit addressable locations
	bool result;


	// call hardware initialization routine
	enter_DefaultMode_from_RESET();

	// set default pin states
	led_on();
	buzzer_off();
	tdata_off();


	// DEBUG:
	//debug_pin0_off();



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
	//sniffing_mode = ADVANCED;
	PCA0_DoSniffing();
	rf_state = RF_IDLE;

	// FIXME: add comment
	last_sniffing_command = RF_CODE_RFIN;
	uart_command          = RF_CODE_RFIN;
	//last_sniffing_command = RF_CODE_SNIFFING_ON;
	//uart_command          = RF_CODE_SNIFFING_ON;
#else
	PCA0_StopSniffing();
	rf_state = RF_IDLE;
#endif

#if 1
    // startup buzzer (can be annoying during development)
	// use LED instead (for development)
	//buzzer_on();
	led_on();
	efm8_delay_ms(startupDelay);

	//buzzer_off();
	led_off();
#endif

    
	// enable global interrupts
	enable_global_interrupts();
    

    
    // startup
    //requires code and memory space, which is in short supply
    //but good to check that polled uart is working
    //printf_tiny("startup...\r\n");
    //uart_put_command(RF_CODE_ACK);

	// FIXME: this is essentially our watchdog timer initialization
	//        but it needs to be done with the abstraction layer
	// instead of the default divide by 8, we do divide by 1 so that low frequency oscillator is at 80 kHz
	// sec. 20.4.1 WDTCN watchdog timer control
	// e.g., (1/80000)*4^(5+3) = 0.8192
	// so that watchdog timer will reset if not fed every 819 millisecs
	//LFO0CN |= OSCLD__DIVIDE_BY_1;

	// we disabled watchdog at startup in case external ram needs to be cleared etc.
	// now we re-enable
	//WDT0_start();
	
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
		if (true)
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

#if 1
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
					switch(uart_command)
					{
						case RF_CODE_RFIN:
							// we no longer share a buffer between radio and uart, however need to avoid writing to radio buffer while reading it
							PCA0CPM0 &= ~ECCF__ENABLED;
							//we read RF_DATA[] so do not want decoding writing to it while trying to read it
							uart_put_RF_Data_Standard(RF_CODE_RFIN);
							PCA0CPM0 |= ECCF__ENABLED;
							break;

						case RF_CODE_SNIFFING_ON:
							uart_put_RF_Data_Advanced(RF_CODE_SNIFFING_ON, RF_DATA_STATUS & 0x7F);
							break;
					}

					// clear RF status
					RF_DATA_STATUS = 0;

					// enable interrupt for RF receiving
					PCA0CPM0 |= ECCF__ENABLED;
				}
				else
				{
					// disable interrupt for radio receiving while reading buffer
					PCA0CPM0 &= ~ECCF__ENABLED;
					result = buffer_out(&bucket);
					// FIXME: reenable (should store previous and just restore that?)
					PCA0CPM0 |= ECCF__ENABLED;

					// handle new received buckets
					if (result)
                    {
						HandleRFBucket(bucket & 0x7FFF, (bool)((bucket & 0x8000) >> 15));
                    }
				}
				break;
			case RF_CODE_RFOUT:

				// only do the job if all data got received by UART
				if (uart_state != IDLE)
					break;

				// if statement allows repeat transmissions
				if (radio_state_machine())
				{
					// FIXME: need to examine this logic
					// restart sniffing in its previous mode
					PCA0_DoSniffing();

					// change back to previous command (i.e., not rfout)
					uart_command = last_sniffing_command;
				}
				break;
			case RF_CODE_SNIFFING_ON_BUCKET:

				// check if a RF signal got decoded
				if ((RF_DATA_STATUS & RF_DATA_RECEIVED_MASK) != 0)
				{
					//
					PCA0CPM0 &= ~ECCF__ENABLED;
					
					//
					uart_put_RF_buckets(RF_CODE_SNIFFING_ON_BUCKET);

					// clear RF status
					RF_DATA_STATUS = 0;

					// enable interrupt for RF receiving
					PCA0CPM0 |= ECCF__ENABLED;
				}
				else
				{
					// do bucket sniffing handling
					result = buffer_out(&bucket);
					if (result)
					{
						Bucket_Received(bucket & 0x7FFF, (bool)((bucket & 0x8000) >> 15));
					}
				}

			break;

			// do a beep
			case RF_DO_BEEP:
				// only do the job if all data got received by UART
				if (uart_state != IDLE)
					break;

				// this is blocking unfortunately
				buzzer_on();
				efm8_delay_ms(*(uint16_t *)&uartPacket[1]);
				buzzer_off();

				// send acknowledge
				// send uart command
				uart_put_command(RF_CODE_ACK);
				uart_command = last_sniffing_command;
				break;

			// host was requesting the firmware version
			case RF_ALTERNATIVE_FIRMWARE:

				// send firmware version
				uart_put_command(FIRMWARE_VERSION);
				uart_command = last_sniffing_command;
				break;

			default:
				// FIXME: not sure if this makes sense
				uart_command = NONE;
			break;
		} //switch(uart_command)
	} //while (1)
}
