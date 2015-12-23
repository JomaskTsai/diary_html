/*
 * spiflash.c:
 *	Extend wiringPi with the EN25T80 SPI EEPROM
 **********************************************************************
 */

#include <stdio.h>
#include <stdint.h>

//delay
#include <time.h>

#include "spi_generic.h"

#define MHz	(1000000)

#define TESTSIGNAL

//determind the Speed 
#define	DEF_SPEED	((1)*MHz)

#define CMD_WE	0x06	//write enable
#define CMD_WD	0x04	//write disable
#define CMD_RS	0x05	//read status
#define CMD_WS	0x01	//write status
#define CMD_RD	0x03	//read data
#define CMD_FR	0x0B	//fast read
#define CMD_PP	0x02	//page programe
#define CMD_SE	0x20	//sector erase
#define CMD_BE	0xD8	//block erase
#define CMD_CE	0xC7	//chip erase




#define SPI_CHNNEL	(0)

void delay (unsigned int howLong);

int fd;
int i=0;
int ad=0;


/*
 * writeByte:
 *	Write a byte to a register on the EN25T80 on the SPI bus.
 *********************************************************************************
 */

static void writeByte (uint8_t spiPort,  uint8_t *addr, uint8_t data)
{
  uint8_t spiData [6] ;   
  
  spiData [0] = CMD_PP ;
  spiData [1] = addr[0] ;
  spiData [2] = addr[1]  ;
  spiData [3] = addr[2]  ;
  spiData [4] = data  ;

  readwriteSpiData (spiPort, spiData, 5) ;
}

static void writeData (uint8_t spiPort,  uint8_t *addr, uint8_t *data, uint8_t len)
{
  uint8_t spiData [len+4] ;   
  
  if (len > 256)
    return;
    
  spiData [0] = CMD_PP ;
  spiData [1] = addr[0] ;
  spiData [2] = addr[1]  ;
  spiData [3] = addr[2]  ;
  for (i=0;i<len;i++)
    spiData [i+4] = data[i];

  readwriteSpiData (spiPort, spiData, (len+4)) ;
}

/*
 * readByte:
 *	Read a byte from a register on the EN25T80 on the SPI bus.
 *********************************************************************************
 */

static uint8_t readByte (uint8_t spiPort, uint8_t *addr)
{
  uint8_t spiData [6] ;

  spiData [0] = CMD_RD ;
  spiData [1] = addr[0] ;
  spiData [2] = addr[1] ;
  spiData [3] = addr[2] ;

  readwriteSpiData (spiPort, spiData, 5) ;

  return spiData[4] ;
}

static void readData (uint8_t spiPort, uint8_t *addr, uint8_t *data, uint8_t len)
{
  uint8_t spiData [len+4] ;  

  spiData [0] = CMD_RD ;
  spiData [1] = addr[0] ;
  spiData [2] = addr[1] ;
  spiData [3] = addr[2] ;

  readwriteSpiData (spiPort, spiData,  (len+4)) ;

  for (i=0;i<len;i++)
    data[i]=spiData [i+4];
}

/*
 * en25t80Setup:
 *	Create a new instance of an EN25T80 SPI GPIO interface. 
 *********************************************************************************
 */

int en25t80Setup (const int spiPort)
{
  int    x ;

  if ((x = setupSpi (spiPort, DEF_SPEED)) < 0)
    return x ;

  return 0 ;
}

//
static int IsDeviceID(const int spiPort)
{
  uint8_t spiData[5] ;
  spiData [0] = 0xAB ;
  
  readwriteSpiData (spiPort, spiData, 5) ;
  printf("Device ID is 0x%x.\n",spiData[4]); 
  return (spiData[4] == 0x13);
}

//
static int IsIdentification(const int spiPort)
{
  uint8_t spiData[4] ;
  spiData [0] = 0x9F ;
  readwriteSpiData (spiPort, spiData, 4) ;
  return ((spiData[1] == 0x1C)&&(spiData[2] == 0x51)&&(spiData[3] == 0x14));
}

//spi flash check
static int check_spiflash(const int spiPort)
{
  printf("SPI Flash check...\n");
  
  if (!IsDeviceID(SPI_CHNNEL))
  {
    printf("Device ID is wrong.\n");
    return -1;
  }  
  
  printf("Device is OK!\n");

  if (!IsIdentification(SPI_CHNNEL))
  {
    printf("Identification is wrong.\n");
    return -1;
  }  
  printf("Identification is OK.\n");  
  
  return 0;
}

//initial
static int init_spiflash(const int spiPort)
{
  //spi flash setup
  fd = en25t80Setup(spiPort);

  if (fd != 0) {
    fprintf (stderr, "SPI setup failed 0x02%X\n",fd);
    return -1;
  }
  #ifndef TESTSIGNAL  
  if (check_spiflash(spiPort) < 0)
    return -1;
  else
    return fd;
  #endif
}

//
static int readRegisterStatus(const int spiPort)
{
  uint8_t spiData[2] ;
  spiData[0] = CMD_RS ;
  readwriteSpiData (spiPort, spiData, 2) ;
  printf("Read Status 1: 0x%02X\n", spiData[1]); 
  return spiData[1];
}

//
static void writeRegisterStatus(const int spiPort, int data)
{
  uint8_t spiData[2] ;
  spiData[0] = CMD_WS ;
  spiData[1] = data ;
  readwriteSpiData (spiPort, spiData, 2) ;
  printf("write status : 0x%02X\n", data); 
}

//
static void writeEnable(const int spiPort)
{
  uint8_t spiData[1] ;
  spiData[0] = CMD_WE ;
  readwriteSpiData (spiPort, spiData, 1) ;
  printf("write Enable! \n"); 
}

//
static void writeDisable(const int spiPort)
{
  uint8_t spiData[1] ;
  spiData[0] = CMD_WD ;
  readwriteSpiData (spiPort, spiData, 1) ;
  printf("write Disable! \n"); 
}

//
static void ereaseChip(const int spiPort)
{
  uint8_t spiData[2] ;
  spiData[0] = CMD_CE ;
  readwriteSpiData (spiPort, spiData, 1) ;  
  while(1){
    spiData[0] = CMD_RS ;
    readwriteSpiData (spiPort, spiData, 2) ;
    if (!(spiData[1]&0x01))
      break;
  } 
  printf("Chip Erease finish! \n"); 
}

//
static void ereaseBlock(const int spiPort, uint8_t *addr)
{
  //linux os ?....?.?...??int .....?...uint8_t
  uint8_t spiData[4] ;
  spiData[0] = CMD_BE ;
  spiData[1] = addr[0];
  spiData[2] = addr[1];
  spiData[3] = addr[2];
  readwriteSpiData (spiPort, spiData, 4) ;  
  while(1){
    spiData[0] = CMD_RS ;
    readwriteSpiData (spiPort, spiData, 2) ;
    if (!(spiData[1]&0x01))
      break;
  } 
  printf("Block Erease finish! \n"); 
}

//
static void ereaseSection(const int spiPort, uint8_t *addr)
{
  uint8_t spiData[4] ;
  spiData[0] = CMD_SE ;
  spiData[1] = addr[0];
  spiData[2] = addr[1];
  spiData[3] = addr[2];
  readwriteSpiData (spiPort, spiData, 4) ;  
  while(1){
    spiData[0] = CMD_RS ;
    readwriteSpiData (spiPort, spiData, 2) ;
    if (!(spiData[1]&0x01))
      break;
  } 
  printf("Section Erease finish! \n"); 
}

//main loop
int main (void)
{
  int i;
  
  if (init_spiflash(SPI_CHNNEL) < 0 ) {
    printf("SPI Flash initial fail!\n");
    return -1;
  }

  #ifndef TESTSIGNAL
  writeEnable(SPI_CHNNEL);
  writeRegisterStatus(SPI_CHNNEL,0x02) ;
  
  delay(1000);
  
  uint8_t status = 0;

  status = readRegisterStatus(SPI_CHNNEL);  
  printf("status = 0x%02x\n", status);


  writeDisable(SPI_CHNNEL);
  readRegisterStatus(SPI_CHNNEL);  
  writeEnable(SPI_CHNNEL);
  readRegisterStatus(SPI_CHNNEL);  
  #endif
   
  
  uint8_t addr[3];
  addr[0]=0x01;
  addr[1]=0x00;
  addr[2]=0x00;
  
  #ifndef TESTSIGNAL
  ereaseChip(SPI_CHNNEL);
  
  uint8_t data[4];
  data[0] = 0xAA;
  data[1] = 0xBB;
  data[2] = 0xCC;
  data[3] = 0xDD;

  writeEnable(SPI_CHNNEL);
  writeData(SPI_CHNNEL,addr,data,4);
  #endif
  
  for (i=0; i<4; i++) {
    /*
    writeByte(SPI_CHNNEL,addr,i);
    printf("0x%02X%02X%02X 0x%02X\n", addr[0], addr[1], addr[2], i);
    */
    addr[2]=i;
    printf("0x%02X%02X%02X 0x%02x\n", addr[0], addr[1], addr[2], readByte(SPI_CHNNEL,addr));

    delay(100);
  }
  
  printf("\n");

  return 0;
}

/*
 * delay:
 *      Wait for some number of milliseconds
 *********************************************************************************
 */

void delay (unsigned int howLong)
{
  struct timespec sleeper, dummy ;

  sleeper.tv_sec  = (time_t)(howLong / 1000) ;
  sleeper.tv_nsec = (long)(howLong % 1000) * 1000000 ;

  nanosleep (&sleeper, &dummy) ;
}

