/*
 * File:   control.c
 * Author: vatanuki.kun
 *
 * Created on October 23, 2019, 9:27 PM
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <wiringPi.h>
#include <alsa/asoundlib.h>
#include <unistd.h>
#include <linux/reboot.h>

#include "ssd1306.h"
#include "spdif-loop.h"

static const char *item_channel_name[] = {"MASTER", "FRONT", "CENTER", "LFE", "REAR"};
static const uint8_t item_channel_range[][2] = {{0,5}, {0,1}, {2,2}, {3,3}, {4,5}};

static uint32_t clock_elapsed_msec(struct timespec *start){
	struct timespec finish;
	clock_gettime(CLOCK_MONOTONIC, &finish);
	return (finish.tv_sec - start->tv_sec) * 1000 + (finish.tv_nsec - start->tv_nsec) / 1000000;
}

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

static snd_mixer_t *mixer_init(looper_data_t *ld, snd_mixer_elem_t **elem){
	int err;
	snd_mixer_t *mhandle;

	if((err = snd_mixer_open(&mhandle, 0)) < 0){
		av_log(NULL, AV_LOG_ERROR, "snd_mixer_open | %s\n", snd_strerror(err));
		return NULL;
	}

	if((err = snd_mixer_attach(mhandle, ld->out_dev_name)) < 0){
		av_log(NULL, AV_LOG_ERROR, "snd_mixer_attach %s | %s\n", ld->out_dev_name, snd_strerror(err));
		snd_mixer_close(mhandle);
		return NULL;
	}

	if((err = snd_mixer_selem_register(mhandle, NULL, NULL)) < 0){
		av_log(NULL, AV_LOG_ERROR, "snd_mixer_selem_register | %s\n", snd_strerror(err));
		snd_mixer_close(mhandle);
		return NULL;
	}

	if((err = snd_mixer_load(mhandle)) < 0){
		av_log(NULL, AV_LOG_ERROR, "snd_mixer_load: %s | %s\n", ld->out_dev_name, snd_strerror(err));
		snd_mixer_close(mhandle);
		return NULL;
	}

	for(*elem = snd_mixer_first_elem(mhandle); *elem; *elem = snd_mixer_elem_next(*elem)){
		if(!snd_mixer_selem_is_active(*elem) || !snd_mixer_selem_has_playback_volume(*elem))
			continue;

		av_log(NULL, AV_LOG_INFO, "mixer '%s' volume\n", snd_mixer_selem_get_name(*elem));

		if(snd_mixer_selem_has_playback_channel(*elem, 5))
			break;
	}

	if(!*elem){
		av_log(NULL, AV_LOG_ERROR, "mixer cannot find 5.1 volume control\n");
		snd_mixer_close(mhandle);
		return NULL;
	}

	return mhandle;
}

static void mixer_close(snd_mixer_t **mhandle){
	if(*mhandle){
		snd_mixer_close(*mhandle);
		*mhandle = NULL;
	}
}

void *control_thread(void* av){
	struct timespec start;
	snd_mixer_selem_channel_id_t ch;
	int i, btns, btns_last = 0, hold, menu = MENU_NONE, item, display = 0;
	looper_data_t *ld = (looper_data_t *)av;
	snd_mixer_t *mhandle = NULL;
	snd_mixer_elem_t *elem;
	long v, vol[6], pmin, pmax;
	char str[22];

	if(ld->i2c_fd >= 0){
		ssd1306Init(ld->i2c_fd, SSD1306_SWITCHCAPVCC);
		ssd1306ClearScreen();
		ssd1306SetFont(&ubuntuMono_24ptFontInfo);
		ssd1306DrawString(0, 4, "OHAYO!!!", 1, WHITE);
		ssd1306Refresh();
	}

	init_btns();

	while(1){
		clock_gettime(CLOCK_MONOTONIC, &start);

		do{
			if((btns = read_btns()))
				break;
			btns_last = 0;
			delay(BTN_POLL_DELAY);
		}while(clock_elapsed_msec(&start) < BTN_READ_DELAY);

		if(btns != btns_last){
			do{
				if(!read_btns()){
					btns_last = 0;
					break;
				}
				delay(BTN_POLL_DELAY);
			}while(clock_elapsed_msec(&start) < BTN_HOLD_DELAY);

		}else if(btns)
			delay(BTN_REPEAT_DELAY);

		hold = btns && (btns == btns_last || btns == read_btns());
		btns_last = btns;

		if(btns){
			if(display > MENU_DISPLAY_TIMEOUT){
				if(ld->i2c_fd >= 0)
					ssd1306Command(SSD1306_DISPLAYON);
				av_log(NULL, AV_LOG_INFO, "OLED: SSD1306_DISPLAYON\n");
			}

			ssd1306ClearScreen();

			switch(btns){
				case BTN_MENU:
					if(display > MENU_DISPLAY_TIMEOUT)
						break;
					item = 0;
					if(++menu >= MENU_LAST - 1)
						menu = MENU_NONE + 1;
					break;
				case BTN_POWER:
					if(hold)
						menu = MENU_POWEROFF;
					else
						menu = MENU_NONE;
					break;
			}

			switch(menu){
				case MENU_INFO:
					ssd1306SetFont(&ubuntuMono_24ptFontInfo);
					snprintf(str, sizeof(str), "5.1 DTS");
					ssd1306DrawString(0, 4, str, 1, WHITE);
					break;

				case MENU_VOLUME:
					ssd1306SetFont(&ubuntuMono_16ptFontInfo);
					if(!mhandle){
						if(!(mhandle = mixer_init(ld, &elem))){
							ssd1306DrawString(0, 8, "MIXER ERROR", 1, WHITE);
							break;
						}else
							snd_mixer_selem_get_playback_volume_range(elem, &pmin, &pmax);
					}

					switch(btns){
						case BTN_DOWN:
							if(!item)
								item = sizeof(item_channel_name)/sizeof(char*);
							item--;
							break;
						case BTN_UP:
							if(++item >= sizeof(item_channel_name)/sizeof(char*))
								item = 0;
							break;
					}

					for(v = 0, i = 0, ch = item_channel_range[item][0]; ch <= item_channel_range[item][1]; ch++, i++){
						snd_mixer_selem_get_playback_volume(elem, ch, &vol[ch]);
						if(btns == BTN_LEFT && vol[ch] > pmin) vol[ch]--;
						if(btns == BTN_RIGHT && vol[ch] < pmax) vol[ch]++;
						v+= vol[ch];
						snd_mixer_selem_set_playback_volume(elem, ch, vol[ch]);
					}

					snprintf(str, sizeof(str), "%s: %ld", item_channel_name[item], v ? v / i : 0);
					ssd1306DrawString(0, 0, str, 1, WHITE);
					ssd1306DrawRect(0, 18, 128, 14, WHITE);
					if(v)
						ssd1306FillRect(3, 21, v / i * 122 / pmax, 8, WHITE);
					break;

				case MENU_SETTINGS:
					ssd1306SetFont(&ubuntuMono_24ptFontInfo);

					switch(btns){
						case BTN_OK:
							ld->upmix = v;
							snprintf(str, sizeof(str), " SAVED");
							break;
						case BTN_LEFT:
						case BTN_RIGHT:
						case BTN_DOWN:
						case BTN_UP:
							v = v ? 0 : 1;
							snprintf(str, sizeof(str), "UPMIX %s", v ? "Y" : "N");
							break;
						default:
							v = ld->upmix;
							snprintf(str, sizeof(str), "SETTINGS");
							break;
					}

					ssd1306DrawString(0, 4, str, 1, WHITE);

					break;

				case MENU_POWEROFF:
					ssd1306SetFont(&ubuntuMono_16ptFontInfo);
					if(btns == BTN_OK){
						snprintf(str, sizeof(str), "SHUTTING");
						ssd1306DrawString(0, 0, str, 1, WHITE);
						snprintf(str, sizeof(str), "  DOWN");
						ssd1306DrawString(0, 16, str, 1, WHITE);
						reboot(LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2, LINUX_REBOOT_CMD_POWER_OFF, 0);
					}else{
						snprintf(str, sizeof(str), "POWER OFF?");
						ssd1306DrawString(0, 8, str, 1, WHITE);
					}
					break;
			}

			if(ld->i2c_fd >= 0)
				ssd1306Refresh();

			display = 0;
		}

		if(display < MENU_DISPLAY_TIMEOUT){
			display+= clock_elapsed_msec(&start);
			if(display >= MENU_DISPLAY_TIMEOUT){
				if(ld->i2c_fd >= 0)
					ssd1306Command(SSD1306_DISPLAYOFF);
				av_log(NULL, AV_LOG_INFO, "OLED: SSD1306_DISPLAYOFF\n");
				mixer_close(&mhandle);
				menu = MENU_VOLUME;
				item = 0;
			}
		}
	}
}
