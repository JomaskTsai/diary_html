
#include  "wiringPiI2C.h"
#include  "wiringPi.h"
#include  <stdio.h>
#include  <math.h>
#include  <unistd.h>
#include  <time.h>

#define CMD_SEC   (0x00)
#define CMD_MIN   (0x01)
#define CMD_HOUR  (0x02)
#define CMD_DAY   (0x03)
#define CMD_DATE  (0x04)
#define CMD_MON   (0x05)
#define CMD_YEAR  (0x06)
#define CMD_SEC_A1 (0x07)
#define CMD_MIN_A1 (0x08)
#define CMD_HOUR_A1 (0x09)
#define CMD_DAY_A1 (0x0A)
#define CMD_MIN_A2 (0x0B)
#define CMD_HOUR_A2 (0x0C)
#define CMD_DAY_A2 (0x0D)
#define CMD_CONTROL (0x0E)
#define CMD_STATUS (0x0F)  	

#define BIT_CHG(num,value,shift) (num^=(-value^num)&(1<<shift))
#define BIT_CHECK(num,shift) ((num>>shift)&1)

enum {
  MODE_NULL=0,
  MODE_IDEL,
  MODE_READ_1S,
  MODE_SQW_1HZ,
  MODE_SQW_32kHZ,
  MODE_INT_1HZ,
  MODE_OTHER,
};

int fd_int;

const int BCDlist[8] = {1,2,4,8,10,20,40,80};

void RTC_write_time(int fd,struct tm *tm_rtc)
{
  int reg_s=0;
  int reg_m=0;
  int reg_h=0;
  
  reg_s = RTC_dec2BCD(tm_rtc->tm_sec,6);
  wiringPiI2CWriteReg8(fd, CMD_SEC, reg_s);
  reg_m = RTC_dec2BCD(tm_rtc->tm_min,6);
  wiringPiI2CWriteReg8(fd, CMD_MIN, reg_m);
  reg_h = RTC_dec2BCD(tm_rtc->tm_hour,4);
  wiringPiI2CWriteReg8(fd, CMD_HOUR, reg_h);
}

void RTC_write_date(int fd,struct tm *tm_rtc)
{
  int reg_y=0;
  int reg_m=0;
  int reg_md=0;
  int reg_wd=0;

  reg_y = RTC_dec2BCD(tm_rtc->tm_year,7);
  wiringPiI2CWriteReg8(fd, CMD_YEAR, reg_y);
  reg_m = RTC_dec2BCD(tm_rtc->tm_mon,4);
  wiringPiI2CWriteReg8(fd, CMD_MON, reg_m);
  reg_md = RTC_dec2BCD(tm_rtc->tm_mday,5);
  wiringPiI2CWriteReg8(fd, CMD_DATE, reg_md);
  reg_wd = RTC_dec2BCD(tm_rtc->tm_wday,2);
  wiringPiI2CWriteReg8(fd, CMD_DAY, reg_wd);
}

void RTC_read_time(int fd, struct tm *tm_info)
{
  wiringPiI2CWrite(fd, CMD_HOUR);
  tm_info->tm_hour =  wiringPiI2CRead(fd);
  wiringPiI2CWrite(fd, CMD_MIN);
  tm_info->tm_min =  wiringPiI2CRead(fd);	
  wiringPiI2CWrite(fd, CMD_SEC);
  tm_info->tm_sec =  wiringPiI2CRead(fd);
}

void RTC_read_date(int fd, struct tm *tm_info)
{
  wiringPiI2CWrite(fd, CMD_YEAR);
  tm_info->tm_year =  wiringPiI2CRead(fd);
  wiringPiI2CWrite(fd, CMD_MON);
  tm_info->tm_mon =  wiringPiI2CRead(fd);
  wiringPiI2CWrite(fd, CMD_DATE);
  tm_info->tm_mday =  wiringPiI2CRead(fd);
  wiringPiI2CWrite(fd, CMD_DAY);
  tm_info->tm_wday =  wiringPiI2CRead(fd);
}

int RTC_dec2BCD(int dec,int maxbit)
{
  int i;
  int value = dec;
  int reg = 0;
  int bit = 0;
  
  for (i=maxbit;i>=0;i--)
  {
      bit = value/BCDlist[i];
      value = value%BCDlist[i];	
      BIT_CHG(reg,bit,i);	
  }
  return reg;
}

int RTC_BCD2dec(int BCD,int maxbit)
{
  int i;
  int dec = 0;		
  for (i=maxbit;i>=0;i--)
  {
      dec += BIT_CHECK(BCD,i)*BCDlist[i];
  }
  return dec;
}

void IT7C4337_write_control(int fd, int reg)
{
  wiringPiI2CWriteReg8(fd, CMD_CONTROL, reg);  
}

void IT7C4337_write_status(int fd, int reg)
{
  wiringPiI2CWriteReg8(fd, CMD_STATUS, reg);
}

void IT7C4337_clean_A1(int fd)
{
  wiringPiI2CWriteReg8(fd, CMD_SEC_A1, 0);
  wiringPiI2CWriteReg8(fd, CMD_MIN_A1, 0);
  wiringPiI2CWriteReg8(fd, CMD_HOUR_A1, 0);
  wiringPiI2CWriteReg8(fd, CMD_DAY_A1, 0);  
}

void IT7C4337_clean_A2(int fd)
{
  wiringPiI2CWriteReg8(fd, CMD_MIN_A2, 0);
  wiringPiI2CWriteReg8(fd, CMD_HOUR_A2, 0);
  wiringPiI2CWriteReg8(fd, CMD_DAY_A2, 0);
}

int IT7C4337_Initial(void)
{
  int fd = wiringPiI2CSetup(0x68);

  if (fd < 1) {
    fprintf (stderr, "I2C device open failed\n");
    return -1;
  }
  
  IT7C4337_clean_A1(fd);
  IT7C4337_clean_A2(fd);
  IT7C4337_write_control(fd,0x04);
  IT7C4337_write_status(fd,0x80); 

  return fd;
}

void IT7C4337_set_time(int fd)
{
  time_t t;
  time(&t);
  struct tm *tm_info = localtime(&t);
  RTC_write_time(fd,tm_info);
  RTC_write_date(fd,tm_info);
}

void IT7C4337_read_time(int fd, struct tm *tm_info)
{
  RTC_read_date(fd,tm_info);
  RTC_read_time(fd,tm_info);
}

void IT7C4337_isr(void)
{
  printf("ISR in...\n");
  IT7C4337_write_status(fd_int,0x80);  
}

void IT7C4337_INT1Hz_process(int fd)
{
  wiringPiSetup();
  IT7C4337_write_control(fd,0x05);
  wiringPiI2CWriteReg8(fd, CMD_SEC_A1, 0x80);
  wiringPiI2CWriteReg8(fd, CMD_MIN_A1, 0x80);
  wiringPiI2CWriteReg8(fd, CMD_HOUR_A1, 0x80);
  wiringPiI2CWriteReg8(fd, CMD_DAY_A1, 0x80);
  fd_int=fd;
  wiringPiISR(0,INT_EDGE_FALLING,&IT7C4337_isr);
  while(1){};
}

void IT7C4337_RD1Hz_process(int fd)
{
  struct tm tm_info;
  
  while(1)
  {
    //if (timeout_passed(time1))
    {
      IT7C4337_read_time(fd, &tm_info);
      printf("year/mon/date = %d/%d/%d, today is %d\n",1900+RTC_BCD2dec(tm_info.tm_year,7),RTC_BCD2dec(tm_info.tm_mon+1,4),RTC_BCD2dec(tm_info.tm_mday,5),RTC_BCD2dec(tm_info.tm_wday,2));
      printf("hour/min/sec = %d/%d/%d\n",RTC_BCD2dec(tm_info.tm_hour,4),RTC_BCD2dec(tm_info.tm_min,6),RTC_BCD2dec(tm_info.tm_sec,6));
      sleep(1);
    }
  }
}


int main (int argc, char *argv[])
{
  int fd;
  int i;		
  int cmd_opt = 0;
  int mode = 0;
  char name[5];
 
  struct tm tm_info;

  fd = IT7C4337_Initial();

  while ((cmd_opt = getopt(argc,argv,"sStTm:M:")) != -1){

    switch(cmd_opt){
      case 's':
      case 'S':
        mode = MODE_OTHER;
        IT7C4337_set_time(fd);
        printf("Set Time...OK!\n");
        break;
      case 't':
      case 'T':
        mode = MODE_OTHER;
        printf("Now Time...\n");
        IT7C4337_read_time(fd, &tm_info);
        printf("year/mon/date = %d/%d/%d, today is %d\n",1900+RTC_BCD2dec(tm_info.tm_year,7),RTC_BCD2dec(tm_info.tm_mon+1,4),RTC_BCD2dec(tm_info.tm_mday,5),RTC_BCD2dec(tm_info.tm_wday,2));
        printf("hour/min/sec = %d/%d/%d\n",RTC_BCD2dec(tm_info.tm_hour,4),RTC_BCD2dec(tm_info.tm_min,6),RTC_BCD2dec(tm_info.tm_sec,6));
        break;
      case 'm':
      case 'M':
        if (!strcmp(optarg,"mode1")||!strcmp(optarg,"MODE1")) mode = MODE_IDEL;//MODE_IDLE;
        else if (!strcmp(optarg,"mode2")||!strcmp(optarg,"MODE2")) mode = MODE_READ_1S;//MODE_RD;
        else if (!strcmp(optarg,"mode3")||!strcmp(optarg,"MODE3")) mode = MODE_SQW_1HZ;//MODE_SQ_1HZ;
        else if (!strcmp(optarg,"mode4")||!strcmp(optarg,"MODE4")) mode = MODE_SQW_32kHZ;//MODE_SQ_32KHZ;
        else if (!strcmp(optarg,"mode5")||!strcmp(optarg,"MODE5")) mode = MODE_INT_1HZ;//MODE_INT_1HZ;
        
        if (mode == MODE_NULL) printf("%s is wrong mode! \n",optarg);     
           
        break;
    } 

  }

  if (mode==MODE_NULL) {
    printf ("Not supported option! \n");
    return 0;
  }
   
  switch (mode) {
    case MODE_IDEL:
      printf ("%s is IDLE Mode \n",optarg);      
      break;
    case MODE_READ_1S:
      printf ("This Mode %s will read RTC date time every 1s!\n",optarg);
      IT7C4337_RD1Hz_process(fd);
      break;
    case MODE_SQW_1HZ:
      printf ("%s is SQW/INTB 1Hz Mode!\n",optarg);
      printf ("SQW/INTB pine will output square-wave with 1Hz!\n");
      IT7C4337_write_control(fd,0x00);      
      break;
    case MODE_SQW_32kHZ:
      printf ("%s is SQW/INTB 32kHz Mode!\n",optarg);
      printf ("SQW/INTB pine will output square-wave with 32kHz!\n");  
      IT7C4337_write_control(fd,0x18);
      break;
    case MODE_INT_1HZ:
      printf ("%s is INTA 1Hz Mode!\n",optarg);
      printf ("INTA is falling edge INT!\n");
      IT7C4337_INT1Hz_process(fd);
      break;
  }
 
  return 0;
}

