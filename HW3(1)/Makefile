CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra -std=c11
LDFLAGS ?=
LDLIBS ?= -ldl

COMMON_OBJ = common.o

all: sender receiver calibrate

sender: sender.o $(COMMON_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

receiver: receiver.o $(COMMON_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

calibrate: calibrate.o $(COMMON_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

clean:
	rm -f *.o sender receiver calibrate

.PHONY: all clean
