/*
 * RF_Handling.c
 *
 *  Created on: 27.11.2017
 *      Author:
 */
#include <stdint.h>
#include <EFM8BB1.h>

#include <string.h>
//#include <stdlib.h>

#include "delay.h"
#include "globals.h"
#include "RF_Handling.h"
#include "RF_Protocols.h"
#include "pca_0.h"

// DEBUG
#include "serial.h"
#include "uart.h"

//  global available to rf_handling.c and main.c
__xdata rf_state_t rf_state;

// FIXME: add comment
__xdata uint8_t RF_DATA[RF_DATA_BUFFERSIZE];

// RF_DATA_STATUS
// Bit 7:	1 Data received, 0 nothing received
// Bit 6-0:	Protocol identifier
__xdata uint8_t RF_DATA_STATUS = 0;
__xdata rf_sniffing_mode_t sniffing_mode = STANDARD;


// PT226x variables
__xdata uint16_t SYNC_LOW = 0x00;
__xdata uint16_t BIT_HIGH = 0x00;
__xdata uint16_t BIT_LOW  = 0x00;

// FIXME: add comment
bool actual_byte_high_nibble = false;

// FIXME: add comment
__xdata uint8_t actual_byte = 0;

// status of each protocol
__xdata PROTOCOL_STATUS status[NUM_OF_PROTOCOLS];


__xdata uint8_t old_crc = 0;
__xdata uint8_t crc = 0;

// up to 8 timing buckets for RF_CODE_SNIFFING_ON_BUCKET
// -1 because of the bucket_sync
__xdata uint16_t buckets[7];

#if INCLUDE_BUCKET_SNIFFING == 1
__xdata uint16_t bucket_sync;
__xdata uint8_t bucket_count = 0;
__xdata uint8_t bucket_count_sync_1;
__xdata uint8_t bucket_count_sync_2;
#endif


#define BUFFER_BUCKETS_SIZE 4
__xdata uint16_t buffer_buckets[BUFFER_BUCKETS_SIZE] = {0};

// use separate read and write "pointers" into circular buffer
__xdata uint8_t buffer_buckets_read = 0;
__xdata uint8_t buffer_buckets_write = 0;

//-----------------------------------------------------------------------------
// Callbacks
//-----------------------------------------------------------------------------
void PCA0_overflowCb(void) { }
void PCA0_intermediateOverflowCb(void) { }

uint8_t Compute_CRC8_Simple_OneByte(uint8_t byteVal)
{
    const uint8_t generator = 0x1D;
    uint8_t i;

	// init crc directly with input byte instead of 0, avoid useless 8 bitshifts until input byte is in crc register
    uint8_t crc = byteVal;

    for (i = 0; i < 8; i++)
    {
        if ((crc & 0x80) != 0)
        { 
			// most significant bit set, shift crc register and perform XOR operation, taking not-saved 9th set bit into account
            crc = (uint8_t)((crc << 1) ^ generator);
        }
        else
        { 
			// most significant bit not set, go to next bit
            crc <<= 1;
        }
    }

    return crc;
}

uint16_t compute_delta(uint16_t bucket)
{
	//return ((bucket >> 2) + (bucket >> 4));

	// 25% delta of bucket for advanced decoding
	return (bucket >> 2);
}

bool CheckRFBucket(uint16_t duration, uint16_t bucket, uint16_t delta)
{
	return (((bucket - delta) < duration) && (duration < (bucket + delta)));
}

// duration would typically be the measured pulse width
// bucket would typically be the protocol defined width to be compared
// returns a match (within tolerance) as true, otherwise false
bool CheckRFSyncBucket(uint16_t duration, uint16_t bucket)
{
	uint16_t delta = compute_delta(bucket);

	delta = delta > TOLERANCE_MAX ? TOLERANCE_MAX : delta;
	delta = delta < TOLERANCE_MIN ? TOLERANCE_MIN : delta;

	return CheckRFBucket(duration, bucket, delta);
}

// i is the considered protocol
// high_low is the measured pin value
// duration is the measured pulse width
// pulses is the available buckets (candidate pulse widths)
// bit 0 is
// bit 1 is
// bit_count is
bool DecodeBucket(uint8_t i, bool high_low, uint16_t duration, uint16_t *pulses, uint8_t* bit0, uint8_t bit0_size, uint8_t* bit1, uint8_t bit1_size, uint8_t bit_count)
{
	uint8_t last_bit = 0;

	uint8_t index;
	uint8_t count;
	bool result;

	// DEBUG:
	//uart_putc('^');
	//puthex2(high_low);
	//puthex2((duration >> 8) & 0xff);
	//puthex2(duration & 0xff);

	// do init before first data bit received
	index = (uint8_t)status[i].bit_count;

	// clear
	if (index == 0)
	{
		status[i].actual_bit_of_byte = 8;
		memset(RF_DATA, 0, sizeof(RF_DATA));
		crc = 0x00;
	}

	// start decoding of the bits in sync of the buckets

	// check for possible logic bit0
	index = (uint8_t)((status[i].status >> 8) & 0x0F);

	// this essentially compares the measured duration to the expected duration
	// we examine only the lower three bits because the logic level is stored in the upper most bit
	result = CheckRFSyncBucket(duration, pulses[bit0[index] & 0x07]);
	if (result)
	{

		// DEBUG:
		//set_debug_pin1(high_low);

		// decode only if high or low does match
		result = (bool)((bit0[index] & 0x08) >> 3);
		if (result == high_low)
		{
			// check if this is an initial data pulse and extract time
			if (index == 0)
			{
				// store the measured timing for later output to uart
				BIT_LOW = duration;
			}

			// increment bit0 status
			status[i].status = ((index + 1) << 8) | (status[i].status & 0xF0FF);
		}
	}
	// bucket does not match bit, reset status
	else
	{
		// clear bit0 status
		status[i].status &= 0xF0FF;
	}

	// check for possible logic bit1
	index = (uint8_t)((status[i].status >> 4) & 0x0F);
	result = CheckRFSyncBucket(duration, pulses[bit1[index] & 0x07]);
	if (result)
	{
		// decode only if high/low does match
		result = (bool)((bit1[index] & 0x08) >> 3);
		if (result == high_low)
		{
			if (index == 0)
			{
				BIT_HIGH = duration;
			}

			// increment bit1 status
			status[i].status = ((index + 1) << 4) | (status[i].status & 0xFF0F);

			// DEBUG:
			//set_debug_pin1(high_low);
		}
	}
	// bucket does not match bit, reset status
	else
	{
		// clear bit1 status
		status[i].status &= 0xFF0F;
	}

	index = (uint8_t)((status[i].status >> 8) & 0x0F);
	count = (uint8_t)((status[i].status >> 4) & 0x0F);

	// check if any bucket got decoded, if not restart
	if ((index == 0) && (count == 0))
	{
		led_off();

		// clear status
		status[i].status = 0;
		status[i].bit_count = 0;
		status[i].actual_bit_of_byte = 0;
		
		// indicates failure to decode anything
		return false;
	}

	// on the last bit do not check the last bucket
	// because it may not be correct due to a repeat delay
	index = (uint8_t)status[i].bit_count;
	if (index == bit_count - 1)
	{
		last_bit = 1;
	}

	// check if bit 0 is finished
	index = (uint8_t)((status[i].status >> 8) & 0x0F);
	count = (uint8_t)((status[i].status >> 4) & 0x0F);
	if (index == bit0_size - last_bit)
	{
		led_on();

		status[i].status &= 0xF00F;
		status[i].bit_count += 1;
		status[i].actual_bit_of_byte -= 1;
	}
	else if (count == bit1_size - last_bit)
	{
		// check if bit 1 is finished
		led_on();

		status[i].status &= 0xF00F;
		status[i].bit_count += 1;
		status[i].actual_bit_of_byte -= 1;
		
		// 
		index = status[i].bit_count;

		// shifting by three essentially divides by eight
		// so bit index is converted to a byte index
		// and we set the bit there since we confirmed a logic one measured
		RF_DATA[(index - 1) >> 3] |= (1 << (uint8_t)((status[i]).actual_bit_of_byte));

		// DEBUG
		//uart_putc(index);
		//uart_putc(RF_DATA[(index - 1) >> 3]);
	}

	// 8 bits are done, compute crc of data
	// FIXME: why compute crc?
	index = (uint8_t)(status[i].actual_bit_of_byte);
	if (index == 0)
	{
		index = (uint8_t)status[i].bit_count;

		crc = Compute_CRC8_Simple_OneByte(crc ^ RF_DATA[(index - 1) >> 3]);

		status[i].actual_bit_of_byte = 8;
	}


	// check if all bits got collected
	index = (uint8_t)status[i].bit_count;
	if (index >= bit_count)
	{
		// check if timeout timer for crc is finished
		if (IsTimer2Finished())
		{
			old_crc = 0;
		}

		// check new crc on last received data for debounce
		if (crc != old_crc)
		{
			// new data, restart crc timeout
			StopTimer2();
			InitTimer2_ms(1, 800);
			old_crc = crc;

			// FIXME: it can be confusing to bury things like this in functions
			// disable interrupt for RF receiving while uart transfer
			//PCA0CPM0 &= ~ECCF__ENABLED;

			// set status
			RF_DATA_STATUS = i;
			RF_DATA_STATUS |= RF_DATA_RECEIVED_MASK;
		}

		led_off();

		// clear status
		status[i].status = 0;
		status[i].bit_count = 0;
		status[i].actual_bit_of_byte = 0;
		
		// FIXME: this tells the calling logic to stop checking because we successfully decoded
		return true;
	}

	// FIXME: this tells the calling logic to keep decoding (not necessarily failure)?
	return false;
}

void HandleRFBucket(uint16_t duration, bool high_low)
{
	// FIXME: in original this is uninitialized, but does it matter?
	uint8_t i = 0;
	
	uint8_t index;
	uint8_t count;

	uint16_t pulsewidth;

	// DEBUG:
	//volatile uint16_t debugbucket;
	//volatile uint8_t startsize;

	bool result;
	
	// if noise got received stop all running decodings
	if (duration < MIN_BUCKET_LENGTH)
	{
		// this will be optimized out if only one protocol exists
		for (i = 0; i < NUM_OF_PROTOCOLS; i++)
		{
			//START_CLEAR(status[i]);
			status[i].status = 0;
			status[i].bit_count = 0;
			status[i].actual_bit_of_byte = 0;
		}

		led_off();

		return;
	}

	// DEBUG:
	//set_debug_pin1(high_low);

	// handle the buckets by standard or advanced decoding
	switch(sniffing_mode)
	{
		case STANDARD:
			
			// 0 or 1 means we are looking for a sync pulse width, 2 means we are looking at data bits
			index = (uint8_t)(status[0].status >> 12) & 0x0F;

			// check if protocol is now starting (i.e., zero means it has not previously started)
			if (index == 0)
			{
				// if PT226x standard sniffing, calculate the pulse time by the longer sync bucket
				// this will enable receive PT226x in a range of PT226x_SYNC_MIN <-> 32767µs
				if (duration > PT226x_SYNC_MIN && !high_low) // && (duration < PT226x_SYNC_MAX))
				{
					// DEBUG:
					//debug_pin1_off();


					// increment start twice because of the skipped first high bucket
					//START_INC(status[0]);
					count = (uint8_t)(status[0].status >> 12) & 0x0F;
					status[0].status = ((count + 1) << 12) | (status[0].status & 0x0FFF);

					//START_INC(status[0]);
					count = (uint8_t)(status[0].status >> 12) & 0x0F;
					status[0].status = ((count + 1) << 12) | (status[0].status & 0x0FFF);

					// stores measured sync time, for later sending over uart
					SYNC_LOW = duration;

                    // standard time assumes all other pulse widths are divisible by a common factor
					buckets[0] = duration / 31;
					buckets[1] = buckets[0] * 3;
					buckets[2] = duration;
				}
			}
			// if sync is finished check if bit0 or bit1 is starting
			else if (index == 2)
			{

				// we place all arguments on one line so that debugger does not get confused regarding line numbers
				DecodeBucket(0, high_low, duration, buckets, PROTOCOL_DATA[0].bit0.dat, PROTOCOL_DATA[0].bit0.size, PROTOCOL_DATA[0].bit1.dat, PROTOCOL_DATA[0].bit1.size, PROTOCOL_DATA[0].bit_count);
			}
			break;

		case ADVANCED:

			// this will be optimized out if only one protocol exists
			for (i = 0; i < NUM_OF_PROTOCOLS; i++)
			{
				// track which pulse width we should be on for sync starting from zero
				// in other words, protocol started
				index = (uint8_t)(status[i].status >> 12) & 0x0F;

				// check if sync is finished
				if (index < PROTOCOL_DATA[i].start.size)
				{
					// bit 3 of the byte stores the expected logic level
					result = (bool)((PROTOCOL_DATA[i].start.dat[index] & 0x08) >> 3);

					// check if sync bucket high or low matches
					if (result != high_low)
					{
						continue;
					}


					// get index into bucket array of bucket associated with currently considered start pulse width
					count = PROTOCOL_DATA[i].start.dat[index] & 0x07;
					// get the actual start pulse width now
					pulsewidth = PROTOCOL_DATA[i].buckets.dat[count];

					// DEBUG:
					//uart_putc(':');
					//puthex2((pulsewidth >> 8) & 0xff);
					//puthex2(pulsewidth & 0xff);

					result = CheckRFSyncBucket(duration, pulsewidth);

					if (result)
					{
						// increment the index into the start pulse width by one
						status[i].status = ((index + 1) << 12) | (status[i].status & 0x0FFF);

						// DEBUG:
						set_debug_pin1(high_low);

						// DEBUG:
						//uart_putc(':');
						//puthex2(count);
						//puthex2((duration >> 8) & 0xff);
						//puthex2(duration & 0xff);
						//puthex2((status[i].status >> 8) & 0xff);
						//puthex2(status[i].status & 0xff);
						
						continue;
					}
					else
					{
						// clear status
						status[i].status = 0;
						status[i].bit_count = 0;
						status[i].actual_bit_of_byte = 0;

						// DEBUG:
						debug_pin1_off();

						continue;
					}
				}
				// if sync is finished check if bit0 or bit1 is starting
				else if (index == PROTOCOL_DATA[i].start.size)
				{
					// DEBUG: because debugger watch does not work with pointers
					//uart_putc(RF_CODE_START);
					uart_putc(':');
					puthex2(index);
					puthex2((duration >> 8) & 0xff);
					puthex2(duration & 0xff);
					//uart_putc(RF_CODE_STOP);e

					// decode bucket is probably working because it works during standard mode
					result = DecodeBucket(i, high_low, duration, PROTOCOL_DATA[i].buckets.dat, PROTOCOL_DATA[i].bit0.dat, PROTOCOL_DATA[i].bit0.size, PROTOCOL_DATA[i].bit1.dat, PROTOCOL_DATA[i].bit1.size, PROTOCOL_DATA[i].bit_count);

					// DEBUG: 
					//uart_putc('\r');
					//uart_putc('\n');

					// FIXME: add comment
					if (result)
					{
						return;
					} else {
						// DEBUG:
						uart_putc('?');
					}
				}
			}
			break;
	}	// switch(sniffing_mode)
}

// places measured pulse width durations in an array
void buffer_in(uint16_t bucket)
{
	// check if writing next byte into circular buffer will catch up to read position, and if so bail out
	if ((buffer_buckets_write + 1 == buffer_buckets_read) || (buffer_buckets_read == 0 && buffer_buckets_write + 1 == BUFFER_BUCKETS_SIZE))
	{
		return;
	}

	buffer_buckets[buffer_buckets_write] = bucket;

	buffer_buckets_write++;


	if (buffer_buckets_write >= BUFFER_BUCKETS_SIZE)
	{
		buffer_buckets_write = 0;
	}
}

// read out measured pulse width durations that have been stored in an array
bool buffer_out(uint16_t* bucket)
{
	// this saves flags and especially state of interrupt (i.e. enabled)
	// FIXME: why not just explicitly enable it when needed?
	//uint8_t backup_PCA0CPM0 = PCA0CPM0;

	// check if buffer is empty
	if (buffer_buckets_write == buffer_buckets_read)
	{
		// therefore, indicate nothing was read from buffer (because nothing is available)
		return false;
	}

	// disable interrupt for RF receiving while reading buffer
	//PCA0CPM0 &= ~ECCF__ENABLED;

	// if not empty, go ahead and read a measured duration from buffer
	*bucket = buffer_buckets[buffer_buckets_read];

	// increment "read" pointer
	buffer_buckets_read++;

	// check for wraparound of "read" pointer
	if (buffer_buckets_read >= BUFFER_BUCKETS_SIZE)
	{
		buffer_buckets_read = 0;
	}

	// restore register
	//PCA0CPM0 = backup_PCA0CPM0;

	// indicate we read from buffer
	return true;
}

void PCA0_channel0EventCb(void)
{
    //
	uint16_t current_capture_value = PCA0CP0 * 10;
	uint16_t logic_mask;
	bool pin;

	uint8_t flags = PCA0MD;

	// clear counter
	PCA0MD &= 0xBF;
	PCA0H = 0x00;
	PCA0L = 0x00;
	PCA0MD = flags;


	// FIXME: additional comments; if bucket is not noise add it to buffer
	if (current_capture_value < 0x8000)
	{
		// FIXME: changed from original logic - why was rdata inverted?
		pin = rdata_level();
		logic_mask = (uint16_t)(pin) << 15;
		buffer_in(current_capture_value | logic_mask);

		//DEBUG:
		set_debug_pin0(pin);
	}
	else
	{
		// received noise, so clear all received buckets
		buffer_buckets_read = 0;
		buffer_buckets_write = 0;
	}
}

void PCA0_channel1EventCb(void) { }

void PCA0_channel2EventCb(void) { }

void SetTimer0Overflow(uint8_t T0_Overflow)
{
	// FIXME: add comment
	// timer 0 high byte - overflow
	 // shift was 0x00 anyway...
	//TH0 = (T0_Overflow << TH0_TH0__SHIFT);
	TH0 = T0_Overflow;
}

void PCA0_DoSniffing(void)
{
	// FIXME:
	//uint8_t ret = 0;

    // FIXME: possible to remove to save code size?
	memset(status, 0, sizeof(PROTOCOL_STATUS) * NUM_OF_PROTOCOLS);

	// restore timer to 100000Hz, 10µs interval
	SetTimer0Overflow(0x0B);

	// enable interrupt for RF receiving
	PCA0CPM0 |= ECCF__ENABLED;

	// start PCA
	PCA0_run();

	// FIXME: trying to remove use of Timer3 resource to save code size
	//InitTimer3_ms(1, 10);
	// wait until timer has finished
	//WaitTimer3Finished();
	efm8_delay_ms(10);

	// FIXME: add comment
	RF_DATA_STATUS = 0;


	//return ret;
}

void PCA0_StopSniffing(void)
{
	// stop PCA
	PCA0_halt();

	// clear all interrupt flags of PCA0
	PCA0CN0 &= ~(CF__BMASK | CCF0__BMASK | CCF1__BMASK | CCF2__BMASK);

	// disable interrupt for RF receiving
	PCA0CPM0 &= ~ECCF__ENABLED;

	// be sure the timeout timer is stopped
	StopTimer2();
}

bool SendSingleBucket(bool high_low, uint16_t bucket_time)
{
	// switch to high_low
	//LED = high_low;
	//T_DATA = high_low;
    if (high_low)
    {
        led_on();
        tdata_on();
    } else {
        led_off();
        tdata_off();
    }

	
	// FIXME: remove need for Timer3 resource
    //InitTimer3_us(10, bucket_time);
	// wait until timer has finished
	//WaitTimer3Finished();
	efm8_delay_us(bucket_time);

	return !high_low;
}

//-----------------------------------------------------------------------------
// Send generic signal based on n time bucket pairs (high/low timing)
//-----------------------------------------------------------------------------
#if INCLUDE_BUCKET_SNIFFING == 1

void SendRFBuckets(uint16_t* buckets, uint8_t* rfdata, uint8_t data_len)
{
	// start transmit of the buckets with a high bucket
	bool high_low = true;
	bool high_low_mark = false;
	uint8_t i;

	// check first two buckets if high/low marking is included
	high_low_mark = (rfdata[0] & 0x88) > 0;

	// transmit data
	for (i = 0; i < data_len; i++)
	{
		high_low = SendSingleBucket(high_low_mark ? (bool)(rfdata[i] >> 7) : high_low, buckets[(rfdata[i] >> 4) & 0x07]);
		high_low = SendSingleBucket(high_low_mark ? (bool)((rfdata[i] >> 3) & 0x01) : high_low, buckets[rfdata[i] & 0x07]);
	}

	led_off();
}

#endif

void SendBuckets(uint16_t *pulses, uint8_t* start, uint8_t start_size, uint8_t* bit0, uint8_t bit0_size, uint8_t* bit1, uint8_t bit1_size, uint8_t* end, uint8_t end_size, uint8_t bit_count, uint8_t* rfdata)
{
	uint8_t i, a;
	uint8_t actual_byte = 0;
	uint8_t actual_bit = 0x80;

	uint8_t index;
	bool result;

	// transmit sync bucket(s)
	for (i = 0; i < start_size; i++)
	{
		index = start[i] & 0x07;
		result = (bool)((start[i] & 0x08) >> 3);
		SendSingleBucket(result, pulses[index]);
	}

	// transmit bit bucket(s)
	for (i = 0; i < bit_count; i++)
	{
		// send bit 0
		if ((rfdata[actual_byte] & actual_bit) == 0)
		{
			for (a = 0; a < bit0_size; a++)
			{
				index = bit0[a] & 0x07;
				result = (bool)((bit0[a] & 0x08) >> 3);
				SendSingleBucket(result, pulses[index]);
			}
		}
		else
		{	// send bit 1
			for (a = 0; a < bit1_size; a++)
			{
				index = bit1[a] & 0x07;
				result = (bool)((bit1[a] & 0x08) >> 3);
				SendSingleBucket(result, pulses[index]);
			}
		}

		actual_bit >>= 1;

		if (actual_bit == 0)
		{
			actual_byte++;
			actual_bit = 0x80;
		}
	}

	// transmit end bucket(s)
	for (i = 0; i < end_size; i++)
	{
		index = end[i] & 0x07;
		result = (bool)((end[i] & 0x08) >> 3);
		SendSingleBucket(result, pulses[index]);
	}

	led_off();

}

void SendBucketsByIndex(uint8_t index, uint8_t* rfdata)
{
	SendBuckets(PROTOCOL_DATA[index].buckets.dat, PROTOCOL_DATA[index].start.dat, PROTOCOL_DATA[index].start.size, PROTOCOL_DATA[index].bit0.dat, PROTOCOL_DATA[index].bit0.size, PROTOCOL_DATA[index].bit1.dat, PROTOCOL_DATA[index].bit1.size, PROTOCOL_DATA[index].end.dat, PROTOCOL_DATA[index].end.size, PROTOCOL_DATA[index].bit_count, rfdata);
}

#if INCLUDE_BUCKET_SNIFFING == 1

bool probablyFooter(uint16_t duration)
{
	return duration >= MIN_FOOTER_LENGTH;
}

bool matchesFooter(uint16_t duration, bool high_low)
{
	bool result;

	if (!((bucket_sync & 0x8000) >> 15) && high_low)
	{
		return false;
	}

	result = CheckRFSyncBucket(duration, bucket_sync & 0x7FFF);

	return result;
}

bool findBucket(uint16_t duration, uint8_t *index)
{
	uint8_t i;
	uint16_t delta;

	for (i = 0; i < bucket_count; i++)
	{
		// calculate delta by the current duration and check if the new duration fits into
		delta = ((duration >> 2) + (duration >> 3));
		delta = delta > buckets[i] ? buckets[i] : delta;

		if (CheckRFBucket(duration, buckets[i], delta))
		{
			*index = i;
			return true;
		}
	}

	return false;
}

// this is used for the sniffing on bucket mode (i.e., 0xB1)
void Bucket_Received(uint16_t duration, bool high_low)
{
	// rf_state is a global variable

	// FIXME: add comment
	uint8_t bucket_index;

	// if pulse is too short reset status
	if (duration < MIN_BUCKET_LENGTH)
	{
		rf_state = RF_IDLE;
		return;
	}

	switch (rf_state)
	{
		// check if we maybe receive a sync
		case RF_IDLE:
			led_off();

			if (probablyFooter(duration))
			{
				bucket_sync = duration | ((uint16_t)high_low << 15);
				bucket_count_sync_1 = 0;
				rf_state = RF_BUCKET_REPEAT;
			}
			break;

		// check if the same bucket gets received
		case RF_BUCKET_REPEAT:
			if (matchesFooter(duration, high_low))
			{
				// check if a minimum of buckets were between two sync pulses
				if (bucket_count_sync_1 > 4)
				{
					led_on();
					bucket_count = 0;
					actual_byte = 0;
					actual_byte_high_nibble = false;
					bucket_count_sync_2 = 0;
					crc = 0x00;
					RF_DATA[0] = 0;
					rf_state = RF_BUCKET_IN_SYNC;
				}
				else
				{
					rf_state = RF_IDLE;
				}
			}
			// check if duration is longer than sync bucket restart
			else if (duration > (bucket_sync & 0x7FFF))
			{
				// this bucket looks like the sync bucket
				bucket_sync = duration | ((uint16_t)high_low << 15);
				bucket_count_sync_1 = 0;
			}
			else
			{
				bucket_count_sync_1++;
			}

			// no more buckets are possible, reset
			if (bucket_count_sync_1 >= RF_DATA_BUFFERSIZE << 1)
			{
				rf_state = RF_IDLE;
			}

			break;

		// same sync bucket got received, filter buckets
		case RF_BUCKET_IN_SYNC:
			bucket_count_sync_2++;

			// check if all buckets got received
			if (bucket_count_sync_2 <= bucket_count_sync_1)
			{
				// check if bucket was already received
				if (!findBucket(duration, &bucket_index))
				{
					// new bucket received, add to array
					buckets[bucket_count] = duration;
					bucket_index = bucket_count;
					bucket_count++;

					// check if maximum of array got reached
					if (bucket_count > ARRAY_LENGTH(buckets))
					{
						// restart sync
						rf_state = RF_IDLE;
					}
				}

				// fill rf data with the current bucket number
				if (actual_byte_high_nibble)
				{
					RF_DATA[actual_byte] = (bucket_index << 4) | ((uint8_t)high_low << 7);
				}
				else
				{
					RF_DATA[actual_byte] |= (bucket_index | ((uint8_t)high_low << 3));

					crc = Compute_CRC8_Simple_OneByte(crc ^ RF_DATA[actual_byte]);

					actual_byte++;

					// check if maximum of array got reached
					if (actual_byte > RF_DATA_BUFFERSIZE)
					{
						// restart sync
						rf_state = RF_IDLE;
					}
				}

				actual_byte_high_nibble = !actual_byte_high_nibble;
			}
			// next bucket after data have to be a sync bucket
			else if (matchesFooter(duration, high_low))
			{
				// check if timeout timer for crc is finished
				if (IsTimer2Finished())
					old_crc = 0;

				// check new crc on last received data for debounce
				if (crc != old_crc)
				{
					// new data, restart crc timeout
					StopTimer2();
					InitTimer2_ms(1, 800);
					old_crc = crc;

					// disable interrupt for RF receiving while uart transfer
					//FIXME: want to move outside of buried function
					// I do not think this is as important anymore because uart and rf no longer share a memory buffer
					//PCA0CPM0 &= ~ECCF__ENABLED;

					// add sync bucket number to data
					RF_DATA[0] |= ((bucket_count << 4) | ((bucket_sync & 0x8000) >> 8));

					// clear high/low flag
					bucket_sync &= 0x7FFF;

					RF_DATA_STATUS |= RF_DATA_RECEIVED_MASK;
				}

				led_off();

				rf_state = RF_IDLE;
			}
			// next bucket after receiving all data buckets was not a sync bucket, restart
			else
			{
				// restart sync
				rf_state = RF_IDLE;
			}
			break;
	}
}

#endif
