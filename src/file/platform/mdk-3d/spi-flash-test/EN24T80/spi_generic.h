/*
 * spi.h:
 *	Simplified SPI access routines
 *	Copyright (c) 2015 Ben Tsai
 */

#ifdef __cplusplus
extern "C" {
#endif

int getSpiFd  (int channel) ;
int readwriteSpiData (int channel, unsigned char *data, int len) ;
int setupSpi  (int channel, int speed) ;

#ifdef __cplusplus
}
#endif

