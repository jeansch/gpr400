/*
 * $Id: gpr400.h,v 1.1 2002/05/25 03:11:14 jaiger Exp $
 *
 * NAME:
 *	gpr400.h -- Definitions for the GEMPLUS GPR400 PCMCIA smartcard reader
 *
 * DESCRIPTION:
 *	Interface definition to the GPR400 PCMCIA driver
 *
 * COPYRIGHT:
 *	Copyright 1997 Wolf Geldmacher, Comunicon AG, CH-9052 Niederteufen.
 *	All rights reserved. See file COPYING.
 *
 * CONTRIBUTORS:
 *	 2000.12.01	Alain Macaire, Gemplus Research
 *			Support for TLV commands
 */
#ifndef gpr400_h
#define gpr400_h

#include <linux/ioctl.h>

/*
 * Structure used to fetch reader status information
 */
struct gpr400_status {
	unsigned char status;		/* reader status		*/
	unsigned char os_version;	/* reader os version		*/
	unsigned char flash_mem;	/* flash memory present		*/
	unsigned char manufacturer;	/* Card manufacturer byte	*/
	unsigned char rom_sum;		/* ROM checksum			*/
	unsigned char ram_sum;		/* RAM checksum			*/
	unsigned char flash_sum;	/* Flash checksum		*/
	unsigned char reg1;		/* Smartcard register 1		*/
	unsigned char reg2;		/* Smartcard register 2		*/
	unsigned char info;		/* Clock & Control register	*/
	unsigned char card_inserted;	/* boolean for card inserted	*/
};

struct gpr400_cmd {
	unsigned char dir;		/* 00 to card, 01 from card	*/
	unsigned char cla;		/* CLA byte			*/
	unsigned char ins;		/* INS byte			*/
	unsigned char p1;		/* P1 byte			*/
	unsigned char p2;		/* P2 byte			*/
	unsigned char len;		/* LEN byte			*/
	unsigned char data[256];	/* data buffer			*/
	unsigned char status;		/* reader status		*/
	unsigned char sw1;		/* SW1 status byte		*/
	unsigned char sw2;		/* SW2 status byte		*/
};

struct gpr400_atr {
	unsigned char status;		/* status of card reader	*/
	unsigned char len;		/* length of ATR		*/
	unsigned char data[62];		/* buffer for ATR		*/
};

struct gpr400_ram {
	unsigned char ram[2016];	/* RAM area			*/
};

struct gpr400_tlv {			/* GPR400 TLV command		*/
	unsigned char tag;
	unsigned short length;
	unsigned char value[300];
};

/*
 * ioctl's for the GPR400 smart card reader
 */
#define GPR400_RESET	_IO('g', 0x01)	/* reset the reader		*/
#define GPR400_PWROFF	_IO('g', 0x02)	/* power down the reader	*/
#define GPR400_STNDBY	_IO('g', 0x03)	/* reader to standby mode	*/
#define GPR400_OPEN	_IOR('g', 0x04, struct gpr400_atr)
#define GPR400_CLOSE	_IO('g', 0x05)	/* close session		*/
#define GPR400_SELECT	_IO('g', 0x06)	/* select card protocol		*/
#define GPR400_STATUS	_IOR('g', 0x07, struct gpr400_status)
#define GPR400_RAM	_IOR('g', 0x08, struct gpr400_ram)
#define GPR400_CMD	_IOWR('g', 0x09, struct gpr400_cmd)
#define	GPR400_TLV	_IOWR('g', 0x0a, struct gpr400_tlv)

#endif
