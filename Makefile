PROG = spdif-loop
OBJS = spdif-loop.o control.o ubuntuMono_8pt.o ubuntuMono_16pt.o ubuntuMono_24pt.o ssd1306.o
HEADERS = spdif-loop.h font.h ubuntuMono_8pt.h ubuntuMono_16pt.h ubuntuMono_24pt.h ssd1306.h
FFMPEG = ../ffmpeg

CC = gcc
CFLAGS = -g -Wall -Wno-unused-function $(DEFS)
LDFLAGS = -lwiringPi -lasound -lpthread -lm -lavformat -lavdevice -lavcodec -lswresample -lavutil

ifneq ($(wildcard $(FFMPEG)),)
CFLAGS+= -I$(FFMPEG)
LDFLAGS+= -L$(FFMPEG)/libavformat -L$(FFMPEG)/libavdevice \
	-L$(FFMPEG)/libavcodec -L$(FFMPEG)/libswresample -L$(FFMPEG)/libavutil
endif

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)
	@echo ==================== DONE ====================

install: all
	install -m 755 $(PROG) /usr/sbin/$(PROG)

clean:
	$(RM) $(OBJS)

distclean: clean
	$(RM) $(PROG)

%.o: %.c
	$(CC) $(CFLAGS) -c $*.c -o $*.o

$(OBJS): $(HEADERS)
