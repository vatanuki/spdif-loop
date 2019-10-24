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

void *control_thread(void* av){
	clock_t c;
	int btns, btns_last = 0, hold;
	looper_data_t *ld = (looper_data_t *)av;

	if(ld->i2c_fd >= 0 && 0){
		ssd1306Init(ld->i2c_fd, SSD1306_SWITCHCAPVCC);
		ssd1306ClearScreen();
		ssd1306SetFont(&ubuntuMono_24ptFontInfo);
		ssd1306DrawString(0, 4, "OHAYO", 1, WHITE);
		ssd1306Refresh();

//		ssd1306ClearScreen();
//		ssd1306Refresh();
	}

	init_btns();

	while(1){
		c = clock();

		do{
			if((btns = read_btns()))
				break;
			btns_last = 0;
			delay(BTN_POLL_DELAY);
		}while(clock() - c < BTN_READ_DELAY * 1000);

		if(btns != btns_last){
			do{
				if(!read_btns()){
					btns_last = 0;
					break;
				}
				delay(BTN_POLL_DELAY);
			}while(clock() - c < BTN_HOLD_DELAY * 1000);

		}else if(btns)
			delay(BTN_REPEAT_DELAY);

		hold = btns && (btns == btns_last || btns == read_btns());

		switch(btns){
			case BTN_OK:
				if(ld->verbose)
					av_log(NULL, AV_LOG_INFO, "button: OK\n");
				break;
			case BTN_MENU:
				if(ld->verbose)
					av_log(NULL, AV_LOG_INFO, "button: MENU\n");
				break;
			case BTN_POWER:
				if(ld->verbose)
					av_log(NULL, AV_LOG_INFO, "button: POWER\n");
/*
#include <unistd.h>
#include <linux/reboot.h>
reboot(LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2, LINUX_REBOOT_CMD_POWER_OFF, 0);
*/
				break;
			case BTN_LEFT:
				if(ld->verbose)
					av_log(NULL, AV_LOG_INFO, "button: LEFT\n");
				break;
			case BTN_RIGHT:
				if(ld->verbose)
					av_log(NULL, AV_LOG_INFO, "button: RIGHT\n");
				break;
			case BTN_DOWN:
				if(ld->verbose)
					av_log(NULL, AV_LOG_INFO, "button: DOWN\n");
				break;
			case BTN_UP:
				if(ld->verbose)
					av_log(NULL, AV_LOG_INFO, "button: UP\n");
				break;
		}

		btns_last = btns;
	}
}
