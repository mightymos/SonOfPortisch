/**************************************************************************//**
 * Copyright (c) 2015 by Silicon Laboratories Inc. All rights reserved.
 *
 * http://developer.silabs.com/legal/version/v11/Silicon_Labs_Software_License_Agreement.txt
 *****************************************************************************/

#include "pca_0.h"

void PCA0_run(void)
{
    PCA0CN0 |= CR__RUN;
}

void PCA0_halt(void)
{
    PCA0CN0 &= ~CR__RUN;
}



void PCA0_ISR(void) __interrupt (PCA0_VECTOR)
{
  // save and clear flags
  uint8_t flags = PCA0CN0 & (CF__BMASK | CCF0__BMASK | CCF1__BMASK | CCF2__BMASK);

  PCA0CN0 &= ~flags;

  if( (PCA0PWM & COVF__BMASK) && (PCA0PWM & ECOV__BMASK))
  {
    PCA0_intermediateOverflowCb();
  }

  PCA0PWM &= ~COVF__BMASK;

  if((flags & CF__BMASK) && (PCA0MD & ECF__BMASK))
  {
    PCA0_overflowCb();
  }

  if((flags & CCF0__BMASK) && (PCA0CPM0 & ECCF__BMASK))
  {
  	// apparently our radio input
    PCA0_channel0EventCb();
  }

  if((flags & CCF1__BMASK) && (PCA0CPM1 & ECCF__BMASK))
  {
    PCA0_channel1EventCb();
  }

  if((flags & CCF2__BMASK) && (PCA0CPM2 & ECCF__BMASK))
  {
    PCA0_channel2EventCb();
  }
}
