CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra -std=c11
LDFLAGS ?=
LDLIBS ?= -ldl -lrt

COMMON_OBJ = channel_common.o

all: sender receiver threshold

sender: sender.o $(COMMON_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

receiver: receiver.o $(COMMON_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

threshold: threshold.o $(COMMON_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

sender.o: sender.c channel_common.h
	$(CC) $(CFLAGS) -c -o $@ sender.c

receiver.o: receiver.c channel_common.h
	$(CC) $(CFLAGS) -c -o $@ receiver.c

threshold.o: threshold.c channel_common.h
	$(CC) $(CFLAGS) -c -o $@ threshold.c

channel_common.o: channel_common.c channel_common.h
	$(CC) $(CFLAGS) -c -o $@ channel_common.c

clean:
	rm -f *.o sender receiver threshold

demo:
	@echo "Terminal 1: ./receiver --threshold 170 --bit-us 3000 --probes 7 --max-frames 3"
	@echo "Terminal 2: ./sender --threshold 170 --bit-us 3000 --repeat 3 --message \"HELLO HW3\""

.PHONY: all clean demo
