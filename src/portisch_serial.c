#include "portisch.h"
#include "portisch_protocols.h"
#include "portisch_serial.h"
#include "uart.h"

void uart_put_command(uint8_t command)
{
	uart_putc(RF_CODE_START);
	uart_putc(command);
	uart_putc(RF_CODE_STOP);
}

void uart_put_RF_Data_Standard(uint8_t command)
{
	uint8_t index = 0;

	uart_putc(RF_CODE_START);
	uart_putc(command);

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
	index = 0;

	// FIXME: used to say 24/8 but in any case would be better to avoid magic numbers
	while(index < 3)
	{
		uart_putc(RF_DATA[index]);
		index++;
	}
    
	uart_putc(RF_CODE_STOP);
}


void uart_put_RF_Data_Advanced(uint8_t command, uint8_t protocol_index)
{
	uint8_t index = 0;
	uint8_t b = 0;
	uint8_t bits = 0;

	uart_putc(RF_CODE_START);
	uart_putc(command);

	bits = PROTOCOL_DATA[protocol_index].bit_count;

	while(index < bits)
	{
		index += 8;
		b++;
	}

	uart_putc(b+1);

	// send index off this protocol
	uart_putc(protocol_index);

	// copy data to UART buffer
	index = 0;
	while(index < b)
	{
		uart_putc(RF_DATA[index]);
		index++;
	}
    
	uart_putc(RF_CODE_STOP);

}



// for bucket sniffing
void uart_put_RF_buckets(uint8_t command)
{
	uint8_t index = 0;

	uart_putc(RF_CODE_START);
	uart_putc(command);
    
	// put bucket count + sync bucket
	uart_putc(bucket_count + 1);

	// FIXME: should not need this as long as buffer is large enough...
	// start and wait for transmit
	//UART0_initTxPolling();
	//uart_wait_until_TX_finished();

	// send up to 7 buckets
	while (index < bucket_count)
	{
		uart_putc((buckets[index] >> 8) & 0x7F);
		uart_putc(buckets[index] & 0xFF);
		index++;
	}

	// send sync bucket
	uart_putc((bucket_sync >> 8) & 0x7F);
	uart_putc(bucket_sync & 0xFF);

	// start and wait for transmit
	//UART0_initTxPolling();
	//uart_wait_until_TX_finished();

	index = 0;
    
	while(index < actual_byte)
	{
		uart_putc(RF_DATA[index]);
		index++;

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