# Makefile for SC3103 Practical Exercise #5
# Builds both Loadable Kernel Modules and the User-Space Application

obj-m += hello_lkm.o
obj-m += gpio_lkm.o

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

.PHONY: all modules user clean

all: modules user

modules:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

user: ../ExA_UserSpace/gpio.c
	gcc -Wall -O2 -o ../ExA_UserSpace/gpio ../ExA_UserSpace/gpio.c

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f ../ExA_UserSpace/gpio
