/*
 * File:   control.c
 * Author: vatanuki.kun
 *
 * Created on October 23, 2019, 9:27 PM
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <wiringPi.h>

#include "ssd1306.h"
#include "spdif-loop.h"

static void init_btns(void){
	wiringPiSetup();

	pinMode(PIN_74HC164_AB, OUTPUT);
	pinMode(PIN_74HC164_CK, OUTPUT);
	pinMode(PIN_74HC164_CLR, OUTPUT);

	pinMode(PIN_BTN_OUT, INPUT);
	pullUpDnControl(PIN_BTN_OUT, PUD_UP);

	//reset 164
	digitalWrite(PIN_74HC164_CLR, LOW);
	digitalWrite(PIN_74HC164_AB, LOW);
	digitalWrite(PIN_74HC164_CK, LOW);
	delay(1);
	digitalWrite(PIN_74HC164_CLR, HIGH);
	delay(1);
}

static int read_btns(void){
	int btns = 0;

	digitalWrite(PIN_74HC164_AB, LOW);
	for(int i = 0; i < 7; i++){
		digitalWrite(PIN_74HC164_CK, HIGH);
		delay(1);
		digitalWrite(PIN_74HC164_CK, LOW);

		btns<<= 1;
		btns|= digitalRead(PIN_BTN_OUT) == LOW ? 1 : 0;

		if(!i)
			digitalWrite(PIN_74HC164_AB, HIGH);
	}

	return btns;
}

static int wait_btns(int delay_ms){
	int btns;

	while(read_btns())
		delay(delay_ms);

	while(!(btns = read_btns()))
		delay(delay_ms);

	while(read_btns())
		delay(delay_ms);

	return btns;
}

void *control_thread(void* av){
	looper_data_t *ld = (looper_data_t *)av;

	if(ld->i2c_fd >= 0){
		ssd1306Init(ld->i2c_fd, SSD1306_SWITCHCAPVCC);
		ssd1306ClearScreen();
		ssd1306SetFont(&ubuntuMono_24ptFontInfo);
		ssd1306DrawString(0, 4, "OHAYO", 1, WHITE);
		ssd1306Refresh();

		ssd1306ClearScreen();
		ssd1306Refresh();
	}

	init_btns();

	while(1){
		switch(wait_btns(10)){
			case BTN_OK:
				printf("OK\n");
				break;
			case BTN_MENU:
				printf("MENU\n");
				break;
			case BTN_POWER:
				printf("POWER\n");
				break;
			case BTN_LEFT:
				printf("LEFT\n");
				break;
			case BTN_RIGHT:
				printf("RIGHT\n");
				break;
			case BTN_DOWN:
				printf("DOWN\n");
				break;
			case BTN_UP:
				printf("UP\n");
				break;
		}
	}
}
