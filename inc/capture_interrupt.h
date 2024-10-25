#ifndef CAPTURE_INTERRUPT_H
#define CAPTURE_INTERRUPT_H

#include <stdint.h>

void clear_interrupt_flags_pca(void);
void clear_pca_counter(void);
uint16_t get_capture_value(void);
void SetTimer0Overflow(uint8_t T0_Overflow);


#endif // CAPTURE_INTERRUPT_H