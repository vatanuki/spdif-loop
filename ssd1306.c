/*
 * File:   newmain.c
 * Author: vadzim
 * Modify: vatanuki.kun
 *
 * Created on November 23, 2016, 9:04 AM
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "ssd1306.h"

static int bus;
static const FONT_INFO *_font;
static uint8_t buffer[1 + SSD1306_LCDWIDTH * SSD1306_LCDHEIGHT / 8];

void ssd1306Init(int i2c_fd, uint8_t vccstate){
	bus = i2c_fd;

	// Initialisation sequence
	ssd1306Command(SSD1306_DISPLAYOFF);
	// 1. set mux ratio
	ssd1306Command(SSD1306_SETMULTIPLEX);
	ssd1306Command(SSD1306_LCDHEIGHT == 32 ? 0x1F : 0x3F);
	// 2. set display offset
	ssd1306Command(SSD1306_SETDISPLAYOFFSET);
	ssd1306Command(0x0);
	// 3. set display start line
	ssd1306Command(SSD1306_SCROLLOFF);
	ssd1306Command(SSD1306_MEMORYMODE);
	ssd1306Command(0x00); // 0x0 act like ks0108 (Horizontal Addressing Mode)
	ssd1306Command(SSD1306_SETCOLUMNADDR); // 0x21 Setup column start and end address
	ssd1306Command(0x00);
	ssd1306Command(0x7f);
	ssd1306Command(SSD1306_SETPAGEADDR); // 0x21 Setup column start and end address
	ssd1306Command(0x00);
	ssd1306Command(SSD1306_LCDHEIGHT == 32 ? 0x03 : 0x07);
	ssd1306Command(SSD1306_SETSTARTLINE | 0x0);
	// 4. set Segment re-map A0h/A1h
	ssd1306Command(SSD1306_SEGREMAP | 0x1);
	// 5. Set COM Output Scan Direction C0h/C8h
	ssd1306Command(SSD1306_COMSCANDEC);
	// 6. Set COM Pins hardware configuration DAh, 12
	ssd1306Command(SSD1306_SETCOMPINS);
	ssd1306Command(SSD1306_LCDHEIGHT == 32 ? 0x02 : 0x12);
	// 7. Set Contrast Control 81h, 7Fh
	ssd1306Command(SSD1306_SETCONTRAST);
	ssd1306Command(SSD1306_EXTERNALVCC == vccstate ? 0x9F : 0xff);
	// 8. Disable Entire Display On A4h
	ssd1306Command(SSD1306_DISPLAYALLON_RESUME);
	// 9. Set Normal Display A6h
	ssd1306Command(SSD1306_NORMALDISPLAY);
	// 10. Set Osc Frequency D5h, 80h
	ssd1306Command(SSD1306_SETDISPLAYCLOCKDIV);
	ssd1306Command(0x80);
	// 11. Enable charge pump regulator 8Dh, 14h
	ssd1306Command(SSD1306_CHARGEPUMP);
	ssd1306Command(SSD1306_EXTERNALVCC == vccstate ? 0x10 : 0x14);
	// 12. Display On AFh
	ssd1306Command(SSD1306_DISPLAYON);
}

void ssd1306SetFont(const FONT_INFO *f){
	_font = f;
}

void ssd1306Command(uint8_t comm){
	uint8_t pre[] = {0, comm};
	write(bus, pre, sizeof(pre));
}

void ssd1306ClearScreen(void){
	memset(buffer, 0, sizeof(buffer));
}

void ssd1306Refresh(void){
	buffer[0] = 0x40;
	write(bus, buffer, sizeof(buffer));
}

void ssd1306DrawPixel(int16_t x, int16_t y, uint8_t color){
	if(x >= SSD1306_LCDWIDTH || x < 0 || y >= SSD1306_LCDHEIGHT || y < 0)
		return;

	switch(color){
		case WHITE:   buffer[1 + x + (y/8)*SSD1306_LCDWIDTH]|=  (1 << (y&7)); break;
		case BLACK:   buffer[1 + x + (y/8)*SSD1306_LCDWIDTH]&= ~(1 << (y&7)); break;
		case INVERSE: buffer[1 + x + (y/8)*SSD1306_LCDWIDTH]^=  (1 << (y&7)); break;
	}
}

void ssd1306DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color){
	int16_t steep = abs(y1 - y0) > abs(x1 - x0);

	if(steep){
		SWAP(x0, y0);
		SWAP(x1, y1);
	}

	if(x0 > x1){
		SWAP(x0, x1);
		SWAP(y0, y1);
	}

	int16_t dx = x1 - x0, dy = abs(y1 - y0), err = dx / 2, ystep = y0 < y1 ? 1 : -1;

	for(; x0 <= x1; x0++){
		if(steep)
			ssd1306DrawPixel(y0, x0, color);
		else
			ssd1306DrawPixel(x0, y0, color);
		err-= dy;
		if(err < 0){
			y0+= ystep;
			err+= dx;
		}
	}
}

void ssd1306FillRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color){
	uint8_t x0 = x, x1 = x + w, y1 = y + h;

	for(; y < y1; y++)
		for(x = x0; x < x1; x++)
			ssd1306DrawPixel(x, y, color);
}

void ssd1306DrawRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color){
	uint8_t x1 = x + w - 1, y1 = y + h - 1;

	if(h > 2 || w > 2){
		ssd1306DrawLine(x,    y, x1,    y, color);
		ssd1306DrawLine(x,   y1, x1,   y1, color);
		ssd1306DrawLine(x,  y+1,  x, y1-1, color);
		ssd1306DrawLine(x1, y+1, x1, y1-1, color);
	}else{
		ssd1306DrawLine(x,  y, x1,  y, color);
		ssd1306DrawLine(x, y1, x1, y1, color);
	}
}

int16_t ssd1306DrawChar(int16_t x, int16_t y, uint8_t c, uint8_t size, uint8_t color){
	int16_t i, j, k, _x;
	uint16_t line;

	if(c < _font->startChar || c > _font->endChar)
		return 0;

	c = c - _font->startChar;
	line = _font->charInfo[c].offset;

	for(i=0; i < _font->charInfo[c].heightBits; i++){
		k = (_font->charInfo[c].widthBits-1)/8 + 1; //number of bytes in a row
		_x = 0;
		do{
			int16_t l = _font->data[line];
			for(j = 0; j < 8; j++){
				if(l & 0x80){
					if(size == 1)
						ssd1306DrawPixel(x+_x, y+i, color);
					else
						ssd1306DrawRect(x+(_x*size), y+(i*size), size, size, color);
				}
				l<<= 1;
				_x++;
			}
			k--;
			line++;
		}while(k > 0);
	}

	return _font->charInfo[c].widthBits;
}

void ssd1306DrawString(int16_t x, int16_t y, char *text, uint8_t size, uint8_t color){
	static uint16_t l, pos;
	pos =  0;
	for (l = 0; l < strlen(text); l++)
	{
		pos = pos + ssd1306DrawChar(x + (pos * size + 1), y, text[l], size, color);
	}
}
