/*
 * File:   spdif-loop.h
 * Author: vatanuki.kun
 *
 * Created on October 23, 2019, 9:28 PM
 */

#ifndef SPDIF_LOOP_H
#define SPDIF_LOOP_H

#include <libavdevice/avdevice.h>
#include <libswresample/swresample.h>

#define SPDIF_SYNCWORD				0x72F81F4E
#define SPDIF_IN_CODEC_PROBESIZE	4096

#define ALSA_IO_BUFFER_SIZE			512
#define PCM_DETECT_DELAY_SIZE		512*1024

#define FRONT_LEFT			0
#define FRONT_RIGHT			1
#define FRONT_CENTER		2
#define LOW_FREQUENCY		3
#define BACK_LEFT			4
#define BACK_RIGHT			5
#define NUM_NAMED_CHANNELS	6

//buttons
#define PIN_74HC164_AB		7 //blue, hw pin #7, board pin #7
#define PIN_74HC164_CK		0 //green, hw pin #11, board pin #8
#define PIN_74HC164_CLR		2 //yellow, hw pin #13, board pin #9
#define PIN_BTN_OUT			3 //orange, hw pin #15, board pin #10

#define BTN_OK			0x01
#define BTN_MENU		0x02
#define BTN_POWER		0x04
#define BTN_LEFT		0x08
#define BTN_RIGHT		0x10
#define BTN_DOWN		0x20
#define BTN_UP			0x40

#define BTN_POLL_DELAY		10
#define BTN_READ_DELAY		500
#define BTN_HOLD_DELAY		1000
#define BTN_REPEAT_DELAY	5

//menu
#define MENU_NONE		0
#define MENU_INFO		1
#define MENU_VOLUME		2
#define MENU_SETTINGS	3
#define MENU_POWEROFF	4
#define MENU_LAST		5

#define MENU_DISPLAY_TIMEOUT 30000

typedef struct looper_data_s {
	AVPacket in_pkt;
	int in_pkt_offset;
	int in_pcm_mode;

	AVInputFormat *alsa_fmt;
	AVInputFormat *spdif_fmt;

	AVFormatContext *in_alsa_ctx;
	AVFormatContext *in_spdif_ctx;
	AVCodecContext *in_codec_ctx;

	SwrContext *swr_ctx;

	char *out_dev_name;
	AVFormatContext *out_ctx;
	AVStream *out_stream;

	int64_t in_ch_layout;
	int in_sample_rate;
	enum AVSampleFormat in_sample_fmt;
	char in_codec_name[4];

	int verbose;
	int upmix;

	int i2c_fd;
} looper_data_t;

void *control_thread(void* av);

#endif
