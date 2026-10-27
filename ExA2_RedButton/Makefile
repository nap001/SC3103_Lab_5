# Makefile - Exercise A2: Push Button -> Toggle Red LED (ON/OFF)
# Compile on Raspberry Pi:   make
# Run:                        sudo ./gpio_a2

CC      = gcc
CFLAGS  = -Wall -O2
TARGET  = gpio_a2
SRC     = gpio_a2.c

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)
