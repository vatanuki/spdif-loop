PROG = spdif-loop
OBJS = spdif-loop.o

CC = gcc
CFLAGS = -g -Wall -Wno-unused-function $(DEFS)
LDFLAGS = -lavformat -lavdevice -lavcodec -lswresample -lavutil \
	 -lasound -lpthread -lm

HEADERS =

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
