# Makefile - Exercise A1: Green & Yellow LED Blink
# Compile on Raspberry Pi:   make
# Run:                        sudo ./gpio_a1

CC      = gcc
CFLAGS  = -Wall -O2
TARGET  = gpio_a1
SRC     = gpio_a1.c

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)
