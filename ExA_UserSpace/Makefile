# Makefile - SC3103 Practical Exercise #5 (Exercise A: User Space)
# Compile on Raspberry Pi:   make
# Run:                        sudo ./gpio

CC      = gcc
CFLAGS  = -Wall -O2
TARGET  = gpio
SRC     = gpio.c

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)
