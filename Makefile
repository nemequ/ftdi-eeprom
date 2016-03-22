CFLAGS:=-g -O2 -std=c99 -Wall -Wextra

ftdi-eeprom: ftdi-eeprom.c
	$(CC) $(CFLAGS) -o $@ $^ `pkg-config --libs --cflags libftdi1`

clean:
	rm -rf ftdi-eeprom

all: ftdi-eeprom
