/*
  lnBMP: Gpio driver for SWD
  This code is derived from the blackmagic one but has been modified
  to aim at simplicity at the expense of performances (does not matter much though)
  (The compiler may mitigate that by inlining)

Original license header

 * This file is part of the Black Magic Debug project.
 *
 * Copyright (C) 2011  Black Sphere Technologies Ltd.
 * Written by Gareth McMullin <gareth@blacksphere.co.nz>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.


 This file implements the SW-DP interface.

 */
#include "lnArduino.h"
#include "lnBMP_pinout.h"
extern "C"
{
  #include "general.h"
  #include "timing.h"
  #include "adiv5.h"
}

uint32_t swd_delay_cnt=1;

#include "lnBMP_swdio.h"

static uint32_t SwdRead(int ticks) ;
static bool SwdRead_parity(uint32_t *ret, int ticks);
static void SwdWrite(uint32_t MS, int ticks);
static void SwdWrite_parity(uint32_t MS, int ticks);

static void swdioSetAsOutput(bool output);

SwdPin pSWDIO(TSWDIO_PIN);
SwdWaitPin pSWCLK(TSWDCK_PIN); // automatically add delay after toggle
SwdPin pReset(TRESET_PIN);
const lnPin pinDirection=PB5;
/**

*/
void bmp_gpio_init()
{
  pSWDIO.on();
  pSWDIO.output();
  pSWCLK.clockOn();
  pSWCLK.output();
  pReset.input();
  lnPinMode(pinDirection,lnOUTPUT);
  lnDigitalWrite(pinDirection,1);
}

/**
*/
static uint32_t SwdRead(int len)
{
	uint32_t index = 1;
	uint32_t ret = 0;
  int bit;

  swdioSetAsOutput(false);
	while (len--)
  {
		bit = pSWDIO.read();
    pSWCLK.clockOn();
    if(bit) ret|=index;
		index <<= 1;
    pSWCLK.clockOff();
	}
	return ret;
}
/**
*/
static bool SwdRead_parity(uint32_t *ret, int len)
{
	uint32_t res = 0;
  res= SwdRead( len);
	int currentParity = __builtin_popcount(res) & 1;
	int parityBit = pSWDIO.read();
	pSWCLK.clockOn();
	pSWCLK.clockOff();
	*ret = res;
	swdioSetAsOutput(true);
	return 1&(currentParity^parityBit); // should be equal
}
/**

*/
static void SwdWrite(uint32_t MS, int ticks)
{
  int cnt;
	swdioSetAsOutput(true);
  pSWDIO.set(MS & 1);
	while (ticks--)
  {
      pSWCLK.clockOn();
			MS >>= 1;
      pSWDIO.set(MS & 1);
      pSWCLK.clockOff();
	}
}
/**
*/
static void SwdWrite_parity(uint32_t MS, int ticks)
{
	int parity = __builtin_popcount(MS) & 1;
  SwdWrite(MS,ticks);
	pSWDIO.set(parity);
  pSWCLK.clockOn();
	pSWCLK.clockOff();
}


/**
    properly invert SWDIO direction if needed
*/
static bool oldDrive=false;
void swdioSetAsOutput(bool output)
{
  if(output==oldDrive) return;
	oldDrive = output;

  switch((int)output)
  {
      case false: // in
       {
           lnDigitalWrite(pinDirection,0);
           pSWDIO.input();
           pSWCLK.clockOn();
           pSWCLK.clockOff();
           break;
       }
       break;
      case true: // out
      {
          lnDigitalWrite(pinDirection,1);
          pSWCLK.clockOn();
          pSWCLK.clockOff();
          pSWDIO.output();
          break;
      }
      default:
        break;
  }
}

/**
*/
extern "C" void platform_srst_set_val(bool assert)
{
  if(assert) // force reset to low
  {
    pReset.off();
    swait();
    pReset.output();
  }
  else // release reset
  {
    pReset.input();
  }
}
/**
*/
extern "C" bool platform_srst_get_val(void)
{
  pReset.input();
  swait();
  return pReset.read();
}

/**

*/
extern "C" int swdptap_init(ADIv5_DP_t *dp)
{
	dp->seq_in         = SwdRead;
	dp->seq_in_parity  = SwdRead_parity;
	dp->seq_out        = SwdWrite;
	dp->seq_out_parity = SwdWrite_parity;
	return 0;
}

// EOF
