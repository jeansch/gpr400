ifneq ($(KERNELRELEASE),)
obj-m	:= gpr400_cs.o

else
KVER	:= $(shell uname -r)
KDIR	:= /lib/modules/$(KVER)/build
PWD	:= $(shell pwd)

default:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules 

install: default
	mkdir -p /lib/modules/$(KVER)/kernel/drivers/misc/pcmcia
	cp gpr400_cs.ko /lib/modules/$(KVER)/kernel/drivers/misc/pcmcia
	/sbin/depmod -ae

clean: 
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) clean

.PHONY: default install clean
endif
