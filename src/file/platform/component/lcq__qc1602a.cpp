#include "mbed.h"
#include "TextLCD.h"

#define SET_EN() (LCD_EN=1)
#define SET_RS() (LCD_RS=1)
#define SET_RW() (LCD_RW=1)

#define CLEAR_EN() (LCD_EN=0)
#define CLEAR_RS() (LCD_RS=0)
#define CLEAR_RW() (LCD_RW=0)


TextLCD::TextLCD(PinName rs,PinName rw, PinName e, PinName d4, PinName d5,
                 PinName d6, PinName d7,  const char* name) : Stream(name), LCD_RS(rs),LCD_RW(rw), LCD_EN(e),
    LCD_D4to7(d4,d5,d6,d7)
{
    //CLEAR_RW();
    //wait_ms(50);  // Wait for disp to turn on

    CLEAR_EN(); //EN =0
    CLEAR_RS();

    //init_4BitMode2LinesDisplayOn();    //enabler 4 bit mode
    wait_ms(2);	//wait_ms(1);

    lcdComand(0x22);
    wait_us(100);

    lcdComand(0x28);//(0b0010 1000);   // 4- bit mode   LCD 2 line 16x2 Matrix
    wait_us(100);

    lcdComand(0x0C);	//lcdComand(0x01); //(0b00000001);
    wait_us(100);
    lcdComand(0x06);//(0b00000110);
    wait_us(100);
    lcdComand(0x01);	//lcdComand(0xC);//(0b0000 1100);
    wait_us(100);
}

// Causes the LCD-module to read the data on the data input pins
void TextLCD::pulseEn()
{
    SET_EN();		// EN = 1 for L-to-H /
    //wait_us(200);
    CLEAR_EN();		// EN = 0 for H-to-L
}

//Writes the low lible of data to the LCD-module data pins D4 to D7
void TextLCD::writeLcdBitD4toD7(char data)
{
    LCD_D4to7=data;
}

//Enable 4 bit mode  From KS0066U Documentation
void TextLCD::init_4BitMode2LinesDisplayOn()
{

    //Start by selecting configuration mode
    CLEAR_RS();
    CLEAR_RW();

    wait(.015);        // Wait 150ms to ensure powered up

    // send "Display Settings" 3 times (Only top nibble of 0x30 as we've got 4-bit bus)
    writeLcdBitD4toD7(0x3);
    pulseEn();
    wait_ms(2);
    writeLcdBitD4toD7(0x3);
    pulseEn();
    wait_ms(2);
    writeLcdBitD4toD7(0x3);
    pulseEn();
    wait_ms(2);

    //Enable 4 bit mode  From KS0066U Documentation
    writeLcdBitD4toD7(0x2);// (0b0010)  4- bit start / 4-bit mode
    wait_us(100);
    writeLcdBitD4toD7(0x2);
    pulseEn();
    wait_us(100);
    writeLcdBitD4toD7(0x2);////(0b0010);   // Select 4- bit start  (Already on the out port no need to write once more )
    pulseEn();	//LCD exec function
    wait_us(50);

    writeLcdBitD4toD7(0xC);//(0b1100);   // 2 Lines + Disp On
    pulseEn();	//LCD exec function

    lcdComand(0x01);  // Clear Display
    wait_ms(3);
    lcdComand(0x28);  // Function set 001 BW N F - -    
    lcdComand(0x06);  // Cursor Direction and Display Shift : 0000 01 CD S (CD 0-left, 1-right S(hift) 0-no, 1-yes    
    lcdComand(0x0C);  // Dispon + Hide cursor
    wait_us(100);
}


//Writes the byte comand to the LCD-module using 4 bits mode
void TextLCD::lcdComand(unsigned char cmd)
{
    CLEAR_RS();		// RS = 0 for command
    CLEAR_RW();		// RW = 0 for write

	writeLcdBitD4toD7(cmd>>4); //Write the first high cmd nibble

    pulseEn();  //EN Hi-Lo

    writeLcdBitD4toD7(cmd); //Write the second low cmd nibble

    pulseEn();  //EN H to Lo

    wait_us(100); //wait
}

void TextLCD::lcdData(unsigned char data)
{
    SET_RS();       // RS = 1 for data
    CLEAR_RW();     // RW = 0 for write
    
    writeLcdBitD4toD7(data>>4); //Write the first high data nibble
    
    pulseEn();      // EN H to Lo
    wait_us(100); //wait
    
	writeLcdBitD4toD7(data);
    
    pulseEn();  //EN H to Lo
    wait_us(100); //wait
}

//Moves cursor to the X,Y position
void TextLCD::gotoxy(int x, int y)
{
    unsigned char firstCharAdr[]= {0x80,0xc0,0x94,0xD4};
    lcdComand(firstCharAdr[y-1]+ x -1);

    wait_us(1000);
}

int TextLCD::_putc(int value)
{
    lcdData(value);
    return value;
}

int TextLCD::_getc()
{
    return -1;
}