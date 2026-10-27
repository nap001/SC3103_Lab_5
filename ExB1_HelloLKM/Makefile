# Makefile - Exercise B1: Hello LKM
# Must be compiled on the Raspberry Pi (or Ubuntu with kernel headers installed).
#
# Usage:
#   make                    <- builds hello_lkm.ko
#   sudo insmod hello_lkm.ko
#   dmesg | tail            <- verify "Hello from kernel world"
#   lsmod | grep hello
#   modinfo hello_lkm.ko
#   sudo rmmod hello_lkm
#   dmesg | tail            <- verify "Goodbye from kernel world"
#   make clean

obj-m += hello_lkm.o

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

.PHONY: all clean

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
