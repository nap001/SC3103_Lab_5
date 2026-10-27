# Makefile - Exercise B2: GPIO LKM Device Driver
# Must be compiled on the Raspberry Pi (kernel headers required).
#
# Usage:
#   make                    <- builds gpio_lkm.ko
#   sudo insmod gpio_lkm.ko
#   dmesg | tail            <- verify init messages + IRQ number assigned
#   cat /proc/interrupts | grep rpi_gpio_handler
#   ls /sys/class/gpio/     <- verify GPIO4 and GPIO11 exported
#   [Press button on Pibrella -> Green LED toggles]
#   dmesg | tail            <- verify interrupt log messages
#   sudo rmmod gpio_lkm
#   dmesg | tail            <- verify "Goodbye from the GPIO LKM!"
#   make clean

obj-m += gpio_lkm.o

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

.PHONY: all clean

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
