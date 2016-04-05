/* mbed TextLCD Library,4-bit 16x2 LCD for KS0066U
 * Copyright (c) 2015 Rune Langoy
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#ifndef LCD_H_
#define LCD_H_

/**  A TextLCD interface for driving 4-bit 16x2 KS0066U LCD
 *
 * Simple example:
 * @code
 * #include "mbed.h"
 * #include "TextLCD.h"
 *
 * TextLCD lcd(D11,D10,D9,D5,D4,D3,D2);
 * int main()
 * {
 *    lcd.gotoxy(1,1);
 *    lcd.printf("Hello");
 *
 *    lcd.gotoxy(1,2);
 *    lcd.printf("       World");
 *
 *    while(1) {
 *        wait_ms(300);
 *    }
 * }
 * @endcode
 */
class TextLCD : public Stream
{
public:


    /** Create a TextLCD interface and initiated 16x2 char mode
      *
      * @param rs    Instruction/data control line
      * @param rw    Read/Write (is forced to '1')
      * @param e     Enable line (clock)
      * @param d4-d7 Data lines for using as a 4-bit interface
      * @param name  I/O stream name (Optional)
      * Stream example:
      * @code
      * #include "mbed.h"
      * #include "TextLCD.h"
      *
      * TextLCD lcd(D11,D10,D9,D5,D4,D3,D2,"lcdOut");
      * int main()
      * {
      *     freopen("/lcdOut", "w", stdout);
      *     printf("Hello World");
      *     while(1) {
      *         wait_ms(300);
      *     }
      * }
      * @endcode
      */
    TextLCD(PinName rs,PinName rw, PinName e, PinName d4, PinName d5,
            PinName d6, PinName d7,const char* name=NULL) ;

    /**  Writes a Command to the LCD-module
     *
     * @param cmd command to be sendt to the LCD-Controller
     */
    void lcdComand(unsigned char cmd);


    /**  Writes charecters to the LCD display
     *
     * @param data char to be sendt to the LCD-Controller
     */
    void lcdData(unsigned char data);

    /** moves text cursor to a screen column and row
     *
     * @param column  The horizontal position from the left, indexed from 0
     * @param row     The vertical position from the top, indexed from 0
     */
    void gotoxy(int , int );
#if DOXYGEN_ONLY
    /** Write a character to the LCD
     *
     * @param c The character to write to the display
     */
    int putc(int c);

    /** Write a formatted string to the LCD
     *
     * @param format A printf-style format string, followed by the
     *               variables to use in formatting the string.
     */
    int printf(const char* format, ...);
#endif
protected:
    /**  Writes the low nible of data to the LCD-module
      *
      * @param data  Writes the low-nible to the LCD data pins D4 to D7
      */
    void writeLcdBitD4toD7(char data);

    /** Causes the LCD-module to read the data on the data input pins
    *  EN = 1 for L-to-H /
    *  EN = 0 for H-to-L    
    */
    void pulseEn();

    /**  Enable 4 bit mode  From KS0066U Documentation
    */   
    void init_4BitMode2LinesDisplayOn();
    // Stream implementation functions
    virtual int _putc(int value);
    virtual int _getc();

    DigitalOut LCD_RS,LCD_RW,LCD_EN;
    BusOut LCD_D4to7;
};

#endif /* LCD_H_ */