PROG=	spdif-loop

#CFLAGS+=	-Wall -std=c99 -g -I/usr/include/ffmpeg
CFLAGS+=	-O0 -Wall -std=c99 -g -I/home/dev/local/ffmpeg
#LDFLAGS+=	-lavcodec -lavformat -lavdevice -lavutil -lswresample -lm
LDFLAGS+=	-Wl,-rpath,/home/dev/local/ffmpeg/libavcodec -lavcodec \
			-Wl,-rpath,/home/dev/local/ffmpeg/libavformat -lavformat \
			-Wl,-rpath,/home/dev/local/ffmpeg/libavdevice -lavdevice \
			-Wl,-rpath,/home/dev/local/ffmpeg/libavutil -lavutil \
			-Wl,-rpath,/home/dev/local/ffmpeg/libswresample -lswresample \
			-lm

all: ${PROG}

clean:
	-rm -f ${PROG}
