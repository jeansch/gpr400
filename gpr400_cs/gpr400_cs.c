/*
 * Gemplus GPR400 / GemPC400 reader client driver.
 * Copyright (c) 2009 Jean Schurger <jean@schurger.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation;  either version 2  of  the License, or
 *
 * This program is distributed  in  the  hope  that it  will be useful,
 * but  WITHOUT ANY  WARRANTY;  without  even  the  implied warranty of
 *
 * You should have received  a copy  of  the GNU General Public License
 * along with  this  program;  if  not,  write  to  the  Free  Software
 * Foundation, Inc.,   675  Mass  Ave,   Cambridge,   MA  02139,   USA.
 *
 * HISTORY:
 *	Import the official jaiger's driver : gpr400_cs-0.9.7
 *	
 * CHANGES:
 *	- Change to new 2.6 pccard managment
 *      - Fixes by Jose Antonio Sánchez Vázquez <jasvazquez@gmail.com>
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

#include <linux/miscdevice.h>

#include <linux/slab.h>
#include <linux/string.h>

#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/ioport.h>

#include <linux/fs.h>
#include <linux/interrupt.h>

#include <asm/uaccess.h>
#include <linux/ioport.h>

#include <asm/io.h>
#include <asm/system.h>

#include <pcmcia/cs_types.h>
#include <pcmcia/cs.h>
#include <pcmcia/cistpl.h>
#include <pcmcia/cisreg.h>
#include <pcmcia/ciscode.h>
#include <pcmcia/ds.h>

#include "gpr400.h"

MODULE_AUTHOR("Jean Schurger <jean@schurger.org>");
MODULE_DESCRIPTION("Gemplus GPR400 driver");
MODULE_LICENSE("GPL");

#define GPR400_RST    0x01
#define GPR400_START  0x02
#define GPR400_IRQACK 0x04
#define GPR400_CLSE   0x10  
#define GPR400_OPN    0x20  
#define GPR400_APDU   0x30  
#define GPR400_POWER  0x40
#define GPR400_SLCT   0x50  
#define GPR400_STS    0xA0  
#define GPR400_RECV   0x02  
#define GPR400_CONT   0x04  
#define GPR_BUFSZ     256

#ifdef PCMCIA_DEBUG
#define GPR_DEBUG(n, args...) if (pc_debug>(n)) printk(KERN_DEBUG args)
#else
#define GPR_DEBUG(n, args...)
#endif

static dev_info_t dev_info = "gpr400_cs";
static char *version = "GPR400 Driver 0.9.9";

#define CM_MAX_DEV              4
static struct pcmcia_device *global_link = NULL;

/* The gpr400 private structure */
typedef struct gpr400_priv {
  wait_queue_head_t wq;
  u_long    io_base;  /* Base of I/O port range */
  u_int   io_mem;
  u_short   *am_base;
  u_char    driver;   /* flag: -1 if driver is loaded */
  int   irq;    /* irq assigned to this card  */
  struct timer_list release_timer;
} gpr400_priv;

/* gpr400_reset() reset the reader. */
static void gpr400_reset(struct gpr400_priv *dev) {
  GPR_DEBUG(0, "gpr400_reset\n");
  outb(GPR400_RST, dev->io_base);
  udelay(10);				/* wait 5 us at least	*/
  outb(0, dev->io_base);
  mdelay(20);				/* wait 20 ms		*/
}

/* tlv_send() send tlv based commands */
#define GPR400_CONT	0x04			/* Chained block	*/ 
static int tlv_send(struct gpr400_priv *dev, u_char t, int l, u_char *v) {
  int s;					/* length sent		*/
  int i;					/* loop var		*/
  u_char va;				/* reader status	*/
  
#ifdef PCMCIA_DEBUG
  GPR_DEBUG("tlv_send(dev, t=0x%02x, l=0x%02x, v=0x", t, l);
  for ( i= 0; i < l; ++i)
    GPR_DEBUG(0, "%02x", v[i]);
  GPR_DEBUG("\n");
#endif
  
  /* Send chunks. */
  t |= GPR400_CONT;
  for (s = 0; l - s > 28; s += 28) {
    
    /* Copy data to reader */
    outb(t, dev->io_base + 2);
    outb(28, dev->io_base + 3);
    
    for (i = 0; i < 28; ++i)
      outb(*v++, dev->io_base + 4 + i);
    
    /* Start processing and wait for it to finish */
    outb(GPR400_START, dev->io_base);
    interruptible_sleep_on(&dev->wq);
    
    /* Get status byte */
    va = inb(dev->io_base + 4);
    if (va != 0x00) {
      GPR_DEBUG(0, "gpr400_cs: %s\n",
		cardReaderError(va) );
      return -EIO;
    }
  }
  
  /* Send final chunk */
  t &= ~GPR400_CONT;
  outb(t, dev->io_base + 2);
  outb(l - s, dev->io_base + 3);
  for (i = 0; i < l - s; ++i)
    outb(*v++, dev->io_base + 4 + i);
  s += i;
  
  /* Start processing and wait for it to finish */
  outb(GPR400_START, dev->io_base);
  interruptible_sleep_on(&dev->wq);
  
  /*
   * The final status may mark an error return from the card
   * (i.e. sw1/sw2 != 0x90/00) which must be handled by higher
   * layers. This is not a reader error, so it is ignored here.
   */
  va = inb(dev->io_base + 4);
  if (va != 0x00 && va != 0xe7) {
    GPR_DEBUG(0, "gpr400_cs: %s\n", cardReaderError(va) );
    return -EIO;
  }
  return s;
}

/* tlv_recv() receive a tlv string from the smart card reader. */
static int tlv_recv(struct gpr400_priv *dev, u_char *t, int *l, u_char *v) {
  u_char ta;		/* tag of chunk				*/
  int la;			/* length of chunk			*/
  u_char s;		/* reader status			*/
  int i;			/* loop var				*/
  int j;			/* loop var				*/
  
  ta = *t = inb(dev->io_base + 2);
  la = *l = inb(dev->io_base + 3) & 0xFF;
  s = inb(dev->io_base + 4);
  
  GPR_DEBUG(0, "tlv_recv(dev,0x%02x,0x%02x,?): %s\n",
	    *t, *l, cardReaderError(s));
  
  /* Command not successful is *not* a reader error */
  if (s == 0xe7)
    s = 0x00;
  
  if (s != 0x00) {
    GPR_DEBUG(0, "gpr400_cs: %s", cardReaderError(s));
    return s;
  }
  
  /* Reader status byte doesn't count for length returned */
  *l -= 1;
  
  /* Clear continuation flag, if any */
  *t &= ~GPR400_CONT;	
  for (i = 1; ta & GPR400_CONT; i = 0) {
    for (j = 0; j < la - i; ++j)
      *v++ = inb(dev->io_base + 4 + i + j);
    
    /* Get next chunk */
    outb(GPR400_START, dev->io_base);
    interruptible_sleep_on(&dev->wq);
    
    ta = inb(dev->io_base + 2);
    la = inb(dev->io_base + 3) & 0xFF;
    *l += la;
  }
  
  /* Get last chunk of data */
  for (j = 0; j < la - i; ++j)
    *v++ = inb(dev->io_base + 4 + i + j);
  
  GPR_DEBUG(0, "tlv_recv(%d) ->", *l);
  for (i = 0; i < *l; ++i)
    GPR_DEBUG(0, " %02x", vsave[i]);
  GPR_DEBUG(0, "\n");
  
  return s;
}

static int gpr400_open(struct inode *inode, struct file *file) {
  struct pcmcia_device *link;
  struct gpr400_priv *dev;
  
  link = global_link;
  if (link == NULL || !pcmcia_dev_present(link)) {
    GPR_DEBUG(0, "gpr400_open (0x%p) error : nodev (NULL or not present)\n", link);
    return -ENODEV;
  }
  if (link->open) {
    GPR_DEBUG(0, "gpr400_open error : Busy\n");
    return -EBUSY;
  }
  dev = link->priv;
  file->private_data = link; 
  /* Initialize the wait token */
  init_waitqueue_head(&dev->wq);
  /* Reset the reader for good measure (clean slate) */
  gpr400_reset(dev);
  ++link->open;
  return 0;
}

static int gpr400_close(struct inode *inode, struct file *file) {
  struct pcmcia_device *link;
  struct gpr400_priv *dev;
  
  link = global_link;
  
  if (link == NULL || !pcmcia_dev_present(link)) {
    GPR_DEBUG(0, "gpr400_open (0x%p) error : nodev (NULL or not present)\n", link);
    return 0;
  }
  dev = link->priv;
  
  /* Close session with card */
  tlv_send(dev, GPR400_CLSE, 0x00, NULL);
  
  /* Make device available again, regardless of errors during close. */
  --link->open;
  
  return 0;
} 

static int gpr400_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg) {
  struct pcmcia_device *link;
  gpr400_priv *dev;
  u_char ta;      /* reader answer tag    */
  int la;       /* reader answer length   */
  int ret = 0;      /* return value     */
  unsigned int size;      /* size for data transfers  */
  struct gpr400_status status;  /* reader status structure  */
  struct gpr400_cmd gpr400_cmd; /* reader command structure */
  struct gpr400_atr atr;    /* reader ATR structure   */
  struct gpr400_tlv tlv;    /* TLV command buffer   */
  unsigned char rcv_status; /* status of TLV command  */
  u_char buf[GPR_BUFSZ];
  
  link = file->private_data;
  dev = link->priv;
  
  GPR_DEBUG(0, "gpr400_ioctl cmd : %x\n", cmd);
  
  size = (cmd & IOCSIZE_MASK) >> IOCSIZE_SHIFT;
  /* if (cmd & IOC_IN)
     if ((ret = access_ok(VERIFY_READ, (char *)arg, size)))
     return ret;
     if (cmd & IOC_OUT)
     if ((ret = access_ok(VERIFY_WRITE, (char *)arg, size)))
     return ret; */
  
  switch (cmd) {
  case GPR400_RESET:		/* reset the card reader	*/
    gpr400_reset(dev);
    pcmcia_request_configuration(link, &link->conf);
    /* Send select command */
    if ((ret = tlv_send(dev, GPR400_SLCT, 0x01, "\x02")) < 0)
      return ret;
    break;
    
  case GPR400_PWROFF:		/* poweroff the card reader	*/
  case GPR400_STNDBY:		/* put card reader in standby	*/
    if ((ret = tlv_send(dev, GPR400_POWER, 0x01, "\x00")) >= 0)
      ret = 0;
    break;
    
  case GPR400_STATUS:		/* fetch stati of card reader	*/
    /*
     * The reader status returned is actually the status of
     * the first partial command!
     */
    tlv_send(dev, GPR400_STS, 0x01, "\x00");
    tlv_recv(dev, &ta, &la, buf);
    status.status = ret;
    status.os_version = buf[0];
    status.flash_mem = buf[1];
    status.manufacturer = buf[6];
    
    tlv_send(dev, GPR400_STS, 0x01, "\x01");
    tlv_recv(dev, &ta, &la, buf);
    status.rom_sum = buf[0];
    status.ram_sum = buf[1];
    status.flash_sum = ((la > 2) ? buf[2] : 0);
    
    tlv_send(dev, GPR400_STS, 0x01, "\x02");
    tlv_recv(dev, &ta, &la, buf);
    status.reg1 = buf[0];
    status.reg2 = buf[1];
    status.info = buf[2];
    status.card_inserted = ((buf[0] & 0x80) ? 1 : 0);
    
    copy_to_user((struct gpr400_status *)arg, &status,
		 sizeof(struct gpr400_status));
    break;
    
  case GPR400_RAM:		/* Get image of reader ram	*/
    copy_to_user((struct gpr400_ram *)arg, dev->am_base,
		 sizeof(struct gpr400_ram));
    break;
    
  case GPR400_CMD:		/* Perform a command		*/
    copy_from_user(&gpr400_cmd, (struct gpr400_cmd *)arg,
		   sizeof(struct gpr400_cmd));
    /*
     * Send command to reader.
     *	Note: Assumes that there is no padding in
     *	struct gpr400_cmd!
     */
    size = 6;
    if (gpr400_cmd.dir == 0x00)
      size += gpr400_cmd.len;
    
    tlv_send(dev, GPR400_APDU, size, (u_char *)&gpr400_cmd);
    
    /*
     * Get status or status and command data from card
     */
    gpr400_cmd.status = tlv_recv(dev, &ta, &la, buf);
    gpr400_cmd.sw1 = gpr400_cmd.sw2 = 0;
    if (la >= 2) {
      memcpy(gpr400_cmd.data, buf, la - 2);
      gpr400_cmd.sw1 = buf[la - 2];
      gpr400_cmd.sw2 = buf[la - 1];
    }
    
    copy_to_user((struct gpr400_cmd *)arg, &gpr400_cmd,
		 sizeof(struct gpr400_cmd));
    break;
    
  case GPR400_OPEN:		/* open session command		*/
    /*
     * Open session with card:
     *	Powers up the smart card in accordance with ISO7816-3.
     *	Response contains Reader status and ATR. Note that the
     *	ATR is TS + a maximum of 32 bytes.
     */
    tlv_send(dev, GPR400_OPN, 0x00, "");
    la = 0;
    memset(&atr, 0, sizeof(struct gpr400_atr));
    atr.status = tlv_recv(dev, &ta, &la, buf);
    atr.len = la;
    if (ret == 0x00)
      memcpy(atr.data, buf, la);
    copy_to_user((struct gpr400_atr *)arg, &atr,
		 sizeof(struct gpr400_atr));
    break;
    
  case GPR400_CLOSE:		/* close session command	*/
    if ((ret = tlv_send(dev, GPR400_CLSE, 0x00, NULL)) < 0)
      return ret;
    break;
    
  case GPR400_SELECT:		/* select reader protocol	*/
    /*
     * Send select card command:
     *	bit 4:		=0: clock speed 3.68 MHz
     *			=1: clock speed 7.36 MHz
     *	bit 1,0:	=00: downloaded driver 0
     *			=10: ISO7816-3 driver
     * all other values are RFU.
     */
    if ((ret = tlv_send(dev, GPR400_SLCT, 0x01, "\x02")) < 0)
      return ret;
    break;
    
  case GPR400_TLV:     /* Do TLV Exchange with GPR400 */
    copy_from_user(&tlv, (struct gpr400_tlv *)arg,
		   sizeof(struct gpr400_tlv));
    
    tlv_send(dev, tlv.tag, tlv.length, (u_char *)&tlv.value);
    
    rcv_status = tlv_recv(dev, &ta, &la, buf);
    
    tlv.tag = ta;
    if (rcv_status == 0)
      tlv.length = la + 1;
    else
      tlv.length = la;
    
    tlv.value[0] = rcv_status;
    memcpy(&(tlv.value[1]), buf, la);
    
    copy_to_user((struct gpr400_tlv *)arg, &tlv, 
		 sizeof(struct gpr400_tlv));
    break;
    
  default:
    ret = -EINVAL;
    break;
  }
  
  if ( ret != 0 )
    ret = -EINVAL;
  return ret;
}


static struct file_operations gpr400_chr_fops = {
 open:    gpr400_open,
 ioctl:   gpr400_ioctl,
 release: gpr400_close,
 owner:   THIS_MODULE,
};

static struct miscdevice gpr400_dev = {
  .minor      = 0,
  .name       = "gpr400",
  .fops       = &gpr400_chr_fops,
};


/* cardReaderError() decode card reader error message */
/* Register device in kernel */
static int allocate_gpr400_resource(struct pcmcia_driver *pgpr400_cs_driver) {
  int ret;

  ret = misc_register (&gpr400_dev);
  if (ret < 0) {
    GPR_DEBUG(0, "gpr400_cs: could not create device\n");
    pcmcia_unregister_driver(pgpr400_cs_driver);
  }
  GPR_DEBUG(0, "gpr400_cs: minor assigned = %d\n", gpr400_dev.minor);
	
  return ret;
}


#ifdef CONFIG_PM
static int gpr400_suspend(struct pcmcia_device *link)
{
  GPR_DEBUG(0, "gpr400 Suspend\n");
  return 0;
}

static int gpr400_resume(struct pcmcia_device *link)
{
  GPR_DEBUG(0, "gpr400 Resuming...\n");
  gpr400_reset((gpr400_priv *)&link->priv);
  /* pcmcia_request_configuration(link, &link->conf); */
  GPR_DEBUG(0, "gpr400 Device resumed.\n");
  return 0;
}
#endif        /* CONFIG_PM */


static irqreturn_t gpr400_interrupt(int irq, void *dev_id) {
  u_char ack;
  gpr400_priv *dev;

  GPR_DEBUG(0, "gpr400_interrupt()\n");

  dev = dev_id;
	
  /* Acknowledge interrupt. */
  ack = inb(dev->io_base);
  ack &= ~GPR400_IRQACK;
  outb(ack, dev->io_base);

  /* Wakeup upper half. */
  wake_up_interruptible(&dev->wq);
  return IRQ_HANDLED;
} /* gpr400_interrupt }}} */


/*
 * After a card is removed, gpr400_release() will unregister the
 * device, and release the PCMCIA configuration.  If the device is
 * still open, this will be postponed until it is closed.
 */
static void gpr400_release(struct pcmcia_device *link) {
  struct gpr400_priv *dev;
  dev = link->priv;
  pcmcia_disable_device(link);
  free_irq(link->irq.AssignedIRQ, dev);
}

#define CS_CHECK(fn, ret)						\
  do { last_fn = (fn); if ((last_ret = (ret)) != 0) goto cs_failed; } while (0)

#define CFG_CHECK(fn, ret)			\
  if(ret != 0) goto next_entry

static int gpr400_config(struct pcmcia_device *link) {
  gpr400_priv *dev = link->priv;
  tuple_t tuple;
  cisparse_t parse;
  int last_fn, last_ret;
  u_char buf[64];
  win_req_t req;
  memreq_t map;

  GPR_DEBUG(0, "gpr400_config(0x%p)\n", link);

  /*
   * This reads the card's CONFIG tuple to find its configuration
   * registers.
   */
  tuple.DesiredTuple = CISTPL_CONFIG;
  tuple.Attributes = 0;
  tuple.TupleData = buf;
  tuple.TupleDataMax = sizeof(buf);
  tuple.TupleOffset = 0;
  CS_CHECK(GetFirstTuple, pcmcia_get_first_tuple(link, &tuple));
  CS_CHECK(GetTupleData, pcmcia_get_tuple_data(link, &tuple));
  CS_CHECK(ParseTuple, pcmcia_parse_tuple(&tuple, &parse));
  link->conf.ConfigBase = parse.config.base;
  link->conf.Present = parse.config.rmask[0];
	
  /* Get and parse CFTABLE_ENTRY */
  tuple.DesiredTuple = CISTPL_CFTABLE_ENTRY;
  tuple.Attributes = 0;
  CS_CHECK(GetFirstTuple, pcmcia_get_first_tuple(link, &tuple));
  while (last_ret == CS_SUCCESS) {
    cistpl_cftable_entry_t *cfg = &(parse.cftable_entry);

    CFG_CHECK(GetTupleData, pcmcia_get_tuple_data(link, &tuple));
    CFG_CHECK(ParseTuple, pcmcia_parse_tuple(&tuple, &parse));

    if ( cfg->index == 0 || cfg->io.nwin == 0 ) goto next_entry;

    link->conf.ConfigIndex = cfg->index;

    /* Use power settings for Vcc and Vpp if present */
    /*  Note that the CIS values need to be rescaled */
    if (cfg->vcc.present & (1<<CISTPL_POWER_VNOM)) {
      if (link->conf.Vpp != cfg->vcc.param[CISTPL_POWER_VNOM]/10000)
	goto next_entry;
    }

    if (cfg->vpp1.present & (1<<CISTPL_POWER_VNOM))
      link->conf.Vpp = cfg->vpp1.param[CISTPL_POWER_VNOM]/10000;
				

    /* Do we need to allocate an interrupt? */
    if ( cfg->irq.IRQInfo1 )
      link->conf.Attributes |= CONF_ENABLE_IRQ;

    /* IO window settings */
    link->io.NumPorts1 = link->io.NumPorts2 = 0;
    if ( cfg->io.nwin > 0 ) { 
      cistpl_io_t *io = &cfg->io;
      link->io.Attributes1 = IO_DATA_PATH_WIDTH_AUTO;
      if (!(io->flags & CISTPL_IO_8BIT))
	link->io.Attributes1 = IO_DATA_PATH_WIDTH_16;
      if (!(io->flags & CISTPL_IO_16BIT))
	link->io.Attributes1 = IO_DATA_PATH_WIDTH_8;

      link->io.IOAddrLines = io->flags & CISTPL_IO_LINES_MASK;
      link->io.BasePort1 = io->win[0].base;
      link->io.NumPorts1 = io->win[0].len;
      if (io->nwin > 1) {
	link->io.Attributes2 = link->io.Attributes1;
	link->io.BasePort2 = io->win[1].base;
	link->io.NumPorts2 = io->win[1].len;
      }

      GPR_DEBUG(1, "gpr400_cs: RequestIO: 0x%04x-0x%04x\n",
		io->win[0].base,
		(io->win[0].base + io->win[0].len) - 1);

      /* This reserves IO space but doesn't actually enable */
      CFG_CHECK(RequestIO, pcmcia_request_io(link, &link->io));
    }

    /* If we got this far, we're cool! */
    break;
		
  next_entry:
    CS_CHECK(GetNextTuple, pcmcia_get_next_tuple(link, &tuple));
  }

  /*
   * Allocate an interrupt line.  Note that this does not assign a
   * handler to the interrupt, unless the 'Handler' member of the
   * irq structure is initialized.
   */
  GPR_DEBUG(0, "gpr400_cs: pcmcia_requestIRQ\n");
   
  CS_CHECK(RequestIRQ, pcmcia_request_irq(link, &link->irq));
  dev->irq = link->irq.AssignedIRQ;

  /*
   * This actually configures the PCMCIA socket -- setting up
   * the I/O windows and the interrupt mapping, and putting the
   * card and host interface into "Memory and IO" mode.
   */
  GPR_DEBUG(0, "gpr400_cs: RequestConfiguration\n");
  CS_CHECK(RequestConfiguration, pcmcia_request_configuration(link, &link->conf));

  GPR_DEBUG(0, "gpr400_cs: RequestWindow\n");
  req.Attributes = WIN_DATA_WIDTH_8 | WIN_MEMORY_TYPE_AM | WIN_ENABLE;
  req.Base = 0; req.Size = 0; req.AccessSpeed = 55;
  CS_CHECK(RequestWindow, pcmcia_request_window(&link, &req, &link->win));
  map.Page = 0; map.CardOffset = 0;
  CS_CHECK(MapMemPage, pcmcia_map_mem_page(link->win, &map));

  dev->am_base = ioremap(req.Base, req.Size);

  dev->io_base = link->io.BasePort1;
  dev->io_mem = req.Base;

  request_irq(dev->irq, gpr400_interrupt, IRQF_SHARED, dev_info, dev);

  /* Finally, report what we've done */
  GPR_DEBUG(0, "gpr400_config : index 0x%02x: Vcc %d.%d",
	    link->conf.ConfigIndex,
	    conf.Vcc/10, conf.Vcc%10);
  if (link->conf.Vpp)
    GPR_DEBUG(0, ", Vpp %d.%d", link->conf.Vpp/10, link->conf.Vpp%10);
  if (link->io.NumPorts1)
    GPR_DEBUG(0, ", io 0x%04x-0x%04x", link->io.BasePort1,
	      link->io.BasePort1+link->io.NumPorts1-1);
  if (link->io.NumPorts2)
    GPR_DEBUG(0, " & 0x%04x-0x%04x", link->io.BasePort2,
	      link->io.BasePort2+link->io.NumPorts2-1);
  if (link->conf.Attributes & CONF_ENABLE_IRQ)
    GPR_DEBUG(0, ", irq %d\n", link->irq.AssignedIRQ);
  if (link->win)
    GPR_DEBUG(0, "gpr400_config : io_mem: %06lx-%06lx",
	      req.Base, req.Base + req.Size-1);
  GPR_DEBUG("\n");

  return 0;

 cs_failed:
  cs_error(link, last_fn, last_ret);
  gpr400_release(link);

  return 0;
}/* gpr400_config }}} */


/* ------  init stuff ----------- */

static int __devinit gpr400_probe(struct pcmcia_device *link)
{
  gpr400_priv *dev = NULL;

  GPR_DEBUG(0, "gpr400_probe()\n");
  dev = kmalloc(sizeof(*dev), GFP_KERNEL);
  if (dev == NULL)
    return -1;	
  memset(dev, 0, sizeof(*dev));
  link->priv = dev;

  global_link = link;

  /* Initialize the release timer */
  init_timer(&dev->release_timer);
  dev->release_timer.function = (void (*)(unsigned long))&gpr400_release;
  dev->release_timer.data = (unsigned long)link;

  /* Interrupt setup */
  link->irq.Attributes = IRQ_TYPE_EXCLUSIVE;
  link->irq.Instance = dev;
  /* Config request setup */
  link->conf.Attributes = CONF_ENABLE_IRQ;
  link->conf.IntType = INT_MEMORY_AND_IO;

  init_waitqueue_head(&dev->wq);

  return gpr400_config(link);
}


static void __devexit gpr400_remove(struct pcmcia_device *link)
{
  GPR_DEBUG(0, "gpr400_detach(0x%p)\n", link);

  global_link = NULL;
  gpr400_release(link);
  kfree(link->priv);
}

static struct pcmcia_device_id gpr400_cs_ids[] = {
  PCMCIA_DEVICE_MANF_CARD(0x0157, 0x3004),
  PCMCIA_DEVICE_NULL,
};
MODULE_DEVICE_TABLE(pcmcia, gpr400_cs_ids);



static struct pcmcia_driver gpr400_cs_driver = {
  .owner    = THIS_MODULE,
  .drv    = {
    .name = "gpr400_cs",
  },
  .probe    = gpr400_probe,
  .remove   = gpr400_remove,
  .id_table       = gpr400_cs_ids,
#ifdef CONFIG_PM
  .suspend  = gpr400_suspend,
  .resume   = gpr400_resume,
#endif        /* CONFIG_PM */
};


static int __init init_gpr400_cs_module(void) {
  printk("%s\n", version);
  GPR_DEBUG(0, "gpr400_cs: loading\n");
  pcmcia_register_driver(&gpr400_cs_driver);
  return ( allocate_gpr400_resource(&gpr400_cs_driver) );
}

static void __exit exit_gpr400_cs_module(void) {
  GPR_DEBUG(0, "gpr400_cs: unloading\n");
  pcmcia_unregister_driver(&gpr400_cs_driver);
  misc_deregister (&gpr400_dev);
}

module_init(init_gpr400_cs_module);
module_exit(exit_gpr400_cs_module);
