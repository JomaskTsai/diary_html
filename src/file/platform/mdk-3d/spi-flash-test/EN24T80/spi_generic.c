/*
 * spi.c:
 *	Simplified SPI access routines
 *	Copyright (c) 2015 Ben Tsai
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>


#include <stdint.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>

#include "spi_generic.h"


// The SPI bus parameters
//	Variables as they need to be passed as pointers later on

const static char       *spiDev0  = "/dev/spidev32766.0" ;
const static char       *spiDev1  = "/dev/spidev32766.1" ;

const static uint8_t     spiMode  = 0;
const static uint8_t     spiBPW   =  8 ;
const static uint16_t    spiDelay = 0 ;

static uint32_t    spiSpeeds [2] ;
static int         spiFds [2] ;


/*
 * getSpiFd:
 *	Return the file-descriptor for the given channel
 *********************************************************************************
 */

int getSpiFd (int channel)
{
  return spiFds [channel & 1] ;
}


/*
 * readwriteSpiData:
 *	Write and Read a block of data over the SPI bus.
 *	Note the data ia being read into the transmit buffer, so will
 *	overwrite it!
 *	This is also a full-duplex operation.
 *********************************************************************************
 */

int readwriteSpiData (int channel, unsigned char *data, int len)
{
  struct spi_ioc_transfer spi ;

  channel &= 1 ;

  spi.tx_buf        = (unsigned long)data ;
  spi.rx_buf        = (unsigned long)data ;
  spi.len           = len ;
  spi.delay_usecs   = spiDelay ;
  spi.speed_hz      = spiSpeeds [channel] ;
  spi.bits_per_word = spiBPW ;
  spi.cs_change     = 0;
  
  return ioctl (spiFds [channel], SPI_IOC_MESSAGE(1), &spi) ;
}


/*
 * setupSpi:
 *	Open the SPI device, and set it up, etc.
 *********************************************************************************
 */

int setupSpi (int channel, int speed)
{
  int fd ;

  channel &= 1 ;

  if ((fd = open (channel == 0 ? spiDev0 : spiDev1, O_RDWR)) < 0){
    fprintf (stderr, "Unable to open SPI device: %s\n",strerror (errno));
    return -1;
  }
  
  spiSpeeds [channel] = speed ;
  spiFds    [channel] = fd ;

  // Set SPI parameters.
  //	Why are we reading it afterwriting it? I've no idea, but for now I'm blindly
  //	copying example code I've seen online...

  if (ioctl (fd, SPI_IOC_WR_MODE, &spiMode)         < 0){
    fprintf (stderr, "SPI Mode Change failure: %s\n",strerror (errno));
    return -1;
  }

  if (ioctl (fd, SPI_IOC_WR_BITS_PER_WORD, &spiBPW) < 0){
    fprintf (stderr, "SPI BPW Change failure: %s\n",strerror (errno));
    return -2;
  }
   
  if (ioctl (fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed)   < 0){
    fprintf (stderr, "SPI Speed Change failure: %s\n",strerror (errno));
    return -3;
  }  

  return fd ;
}


