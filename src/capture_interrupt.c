#include "capture_interrupt.h"

#include <EFM8BB1.h>

void clear_interrupt_flags_pca(void)
{
    // clear all interrupt flags of PCA0
	PCA0CN0 &= ~(CF__BMASK | CCF0__BMASK | CCF1__BMASK | CCF2__BMASK);
}

void clear_pca_counter(void)
{
	uint8_t flags = PCA0MD;
    
	// FIXME: replace or move function to be abstracted from hardware
	// clear counter
	PCA0MD &= 0xBF;
	PCA0H = 0x00;
	PCA0L = 0x00;
	
    PCA0MD = flags;
}

uint16_t get_capture_value(void)
{
    return PCA0CP0 * 10;
}

void SetTimer0Overflow(uint8_t T0_Overflow)
{
	// FIXME: add comment
	// timer 0 high byte - overflow
	 // shift was 0x00 anyway...
	//TH0 = (T0_Overflow << TH0_TH0__SHIFT);
	TH0 = T0_Overflow;
}