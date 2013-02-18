# GPR400 IFD Handler Makefile
# 
# Author: Joe Phillips <joe.phillips@innovationsw.com>
# Based on the IFD Devkit Makefile by David Corcoran and MUSCLE
#
# Modified my Jean Schurger <jean@schurger.org>
#
# $Id: Makefile,v 1.2 2001/08/09 05:14:42 jaiger Exp $
#

CC       = cc
CFLAGS   = -Wall -g -fPIC -fno-stack-protector
LD       = ld
LEX      = flex
OBJ     := $(filter-out test.o, $(patsubst %.c,%.o,$(wildcard *.c)))
INCLUDE  = -I.  -I/usr/include/PCSC

MAKEXE   = make
LIBNAME  = libgpr400_ifd.so
PREFIX   = /usr/pcsc

DEFS     = -DPCSC_DEBUG=1 #-DATR_DEBUG=1

all: unix

clean:
	rm -f *.o $(LIBNAME) core

unix: $(OBJ)
	$(LD) -shared $(OBJ) -o $(LIBNAME) 

$(patsubst %.c,%.o,$(wildcard *.c)) : %.o : %.c
	$(CC) $(CFLAGS) -c $< $(INCLUDE) $(DEFS)

install:
	mkdir -p  $(DESTDIR)/usr/lib/pcsc/drivers/gpr400/
	cp libgpr400_ifd.so $(DESTDIR)/usr/lib/pcsc/drivers/gpr400/
