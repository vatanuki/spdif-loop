/*
 * File:   ssd1306.h
 * Author: vadzim
 *
 * Created on October 3, 2016, 6:15 PM
 */

#ifndef SSD1306_H
#define SSD1306_H

#ifdef __cplusplus
extern "C" {
#endif

#include "font.h"
#include "ubuntuMono_8pt.h"
#include "ubuntuMono_16pt.h"
#include "ubuntuMono_24pt.h"

#define     SWAP(a, b)                  {int16_t t = a; a = b; b = t;}

#define     BLACK                       0
#define     WHITE                       1
#define     INVERSE                     2

#define     SSD1306_LCDWIDTH            128
#define     SSD1306_LCDHEIGHT           32

// Commands
#define SSD1306_SETCONTRAST             0x81
#define SSD1306_DISPLAYALLON_RESUME     0xA4
#define SSD1306_DISPLAYALLON            0xA5
#define SSD1306_NORMALDISPLAY           0xA6
#define SSD1306_INVERTDISPLAY           0xA7
#define SSD1306_DISPLAYOFF              0xAE
#define SSD1306_DISPLAYON               0xAF
#define SSD1306_SETDISPLAYOFFSET        0xD3
#define SSD1306_SETCOMPINS              0xDA
#define SSD1306_SETVCOMDETECT           0xDB
#define SSD1306_SETDISPLAYCLOCKDIV      0xD5
#define SSD1306_SETPRECHARGE            0xD9
#define SSD1306_SETMULTIPLEX            0xA8
#define SSD1306_SETLOWCOLUMN            0x00
#define SSD1306_SETHIGHCOLUMN           0x10
#define SSD1306_SETSTARTLINE            0x40
#define SSD1306_MEMORYMODE              0x20
#define SSD1306_SETCOLUMNADDR           0x21
#define SSD1306_SETPAGEADDR             0x22
#define SSD1306_SCROLLOFF               0x2E
#define SSD1306_COMSCANINC              0xC0
#define SSD1306_COMSCANDEC              0xC8
#define SSD1306_SEGREMAP                0xA0
#define SSD1306_CHARGEPUMP              0x8D
#define SSD1306_EXTERNALVCC             0x1
#define SSD1306_SWITCHCAPVCC            0x2

// Initialisation/Config Prototypes
void ssd1306Init(int, uint8_t);
void ssd1306SetFont(const FONT_INFO*);
void ssd1306Command(uint8_t);
void ssd1306ClearScreen(void);
void ssd1306Refresh(void);
void ssd1306DrawPixel(int16_t, int16_t, uint8_t);
void ssd1306DrawLine(int16_t, int16_t, int16_t, int16_t, uint8_t);
void ssd1306FillRect(uint8_t, uint8_t, uint8_t, uint8_t, uint8_t);
void ssd1306DrawRect(uint8_t, uint8_t, uint8_t, uint8_t, uint8_t);
int16_t ssd1306DrawChar(int16_t, int16_t, uint8_t, uint8_t, uint8_t);
void ssd1306DrawString(int16_t, int16_t, char*, uint8_t, uint8_t);

#ifdef __cplusplus
}
#endif

#endif /* SSD1306_H */
