/*
 * $Id: gpr400_ifd.c,v 1.4 2001/08/22 02:21:43 jaiger Exp $
 *
 * Author:    Joe Phillips <joe.phillips@innovationsw.com>
 * Purpose:   This provides GPR400 specific low-level calls.
 *            based on the ifd-dev source code and code 
 *            released by Wolf Geldmacher.
 *            See http://www.linuxnet.com for more information.
 * License:   See file COPYING.LIB
 x
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

#include <wintypes.h>
#include <pcscdefines.h>
#include <ifdhandler.h>

#include "gpr400.h"


#define T_0 0
#define T_1 1
#define APDU_SIZE 4


/* local data */
static int gpr_fd= -1;
static struct gpr400_atr gpr_atr;
static DEVICE_CAPABILITIES gpr_capabilities = {
    "GEMPLUS", /* Vendor_Name */
    "GPR400",  /* IFD_Type */
    0,         /* IFD_Version */
    "Unknown", /* IFD_Serial */
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    1,
    0,
    0,
    0 
};
	


/**
 * open the given channel
 * @param Lun
 * @param Channel
 * @return IFD_SUCCESS or IFD_COMMUNICATION_ERROR
 **/
RESPONSECODE IFDHCreateChannel ( DWORD Lun, DWORD Channel ) {

  /* This function is required to open a communications channel to the 
     port listed by Channel.  For example, the first serial reader on COM1 would
     link to /dev/pcsc/1 which would be a sym link to /dev/ttyS0 on some machines
     This is used to help with intermachine independance.
     
     Once the channel is opened the reader must be in a state in which it is possible
     to query IFDHICCPresence() for card status.
 
     returns:

     IFD_SUCCESS
     IFD_COMMUNICATION_ERROR
  */
#ifdef PCSC_DEBUG
	printf("IFDHCreateChannel:\n");
#endif
    if(gpr_fd < 0){
        gpr_fd= open("/dev/gpr400", O_RDWR);
        if(gpr_fd < 0){
            return IFD_COMMUNICATION_ERROR;
        }
    }
    return IFD_SUCCESS;

}/* IFDHCreateChannel */

/**
 * close the GPR400 device
 * @param Lun the logical unit number of the device to close
 * @return IFD_SUCCESS or IFD_COMMUNICATION_ERROR
 **/
RESPONSECODE IFDHCloseChannel ( DWORD Lun ) {

#ifdef PCSC_DEBUG
	printf("IFDHCloseChannel:\n");
#endif
  
    if(gpr_fd >= 0){
        close(gpr_fd);
        gpr_fd= -1;
    } 
    return IFD_SUCCESS;
}/* IFDHCloseChannel */

/**
 * This function should get the slot/card capabilities for a particular
 * slot/card specified by Lun.  Again, if you have only 1 card slot and don't mind
 * loading a new driver for each reader then ignore Lun.
 *
 * Tag - the tag for the information requested
 *     example: TAG_IFD_ATR - return the Atr and it's size (required).
 *     these tags are defined in ifdhandler.h
 *
 * Length - the length of the returned data
 * Value  - the value of the data
 *
 * returns:
 *
 * IFD_SUCCESS
 * IFD_ERROR_TAG
 *
 **/
RESPONSECODE IFDHGetCapabilities ( DWORD Lun, DWORD Tag, 
				   PDWORD Length, PUCHAR Value ) 
{
#ifdef PCSC_DEBUG
	printf("IFDHGetCapabilities:\n");
#endif
    if((gpr_fd < 0) || (*Length < gpr_atr.len)){
        return IFD_COMMUNICATION_ERROR;
    }

    switch(Tag){
    case TAG_IFD_ATR:
        *Length= gpr_atr.len;
        memcpy(Value, gpr_atr.data, gpr_atr.len);
        break;
    case 0x0100: /* get vendor name */
        *Length= strlen(gpr_capabilities.Vendor_Name);
        strncpy(gpr_capabilities.Vendor_Name, Value, *Length);
        break;
    case 0x0300: /* ICC Presence */
        *Length= sizeof(UCHAR);
        Value[0]= IFDHGetICCPresence(Lun);
        break;
    default:
        return IFD_ERROR_TAG;
    }

    return IFD_SUCCESS;

}/* IFDHGetCapabilities */

/**
 * This function should set the slot/card capabilities for a particular
 * slot/card specified by Lun.  Again, if you have only 1 card slot and don't mind
 * loading a new driver for each reader then ignore Lun.
 *
 * Tag - the tag for the information needing set
 *
 * Length - the length of the returned data
 * Value  - the value of the data
 *
 * returns:
 *
 * IFD_SUCCESS
 * IFD_ERROR_TAG
 * IFD_ERROR_SET_FAILURE
 * IFD_ERROR_VALUE_READ_ONLY
 *
 **/
RESPONSECODE IFDHSetCapabilities ( DWORD Lun, DWORD Tag, 
			       DWORD Length, PUCHAR Value ) 
{

    return IFD_ERROR_TAG;


//    struct gpr400_tlv tlv;
//
//    if(gpr_fd < 0){
//        return IFD_COMMUNICATION_ERROR;
//    }
//
//    memcpy(tlv.value, Value, Length);
//    tlv.tag= Tag;
//    tlv.length= Length;
//
//    if(ioctl(gpr_fd, GPR400_TLV, &tlv) < 0){
//        /* FIXME - may want to check errno */
//        return IFD_ERROR_SET_FAILURE;
//    }
//    return IFD_SUCCESS;

  
}/* IFDHSetCapabilities */

/**
 * This function should set the PTS of a particular card/slot using
     the three PTS parameters sent

     Protocol  - 0 .... 14  T=0 .... T=14
     Flags     - Logical OR of possible values:
     IFD_NEGOTIATE_PTS1 IFD_NEGOTIATE_PTS2 IFD_NEGOTIATE_PTS3
     to determine which PTS values to negotiate.
     PTS1,PTS2,PTS3 - PTS Values.

     returns:

     IFD_SUCCESS
     IFD_ERROR_PTS_FAILURE
     IFD_COMMUNICATION_ERROR
     IFD_PROTOCOL_NOT_SUPPORTED
 *
 **/
RESPONSECODE IFDHSetProtocolParameters ( DWORD Lun, DWORD Protocol, 
				   UCHAR Flags, UCHAR PTS1,
				   UCHAR PTS2, UCHAR PTS3) 
{
    return IFD_NOT_SUPPORTED;

}/* IFDHSetProtocolParameters */


/**
 * This function controls the power and reset signals of the smartcard reader
 * at the particular reader/slot specified by Lun.
 * 
 * Action - Action to be taken on the card.
 * 
 * IFD_POWER_UP - Power and reset the card if not done so 
 * (store the ATR and return it and it's length).
 *  
 * IFD_POWER_DOWN - Power down the card if not done already 
 * (Atr/AtrLength should
 * be zero'd)
 *
 * IFD_RESET - Perform a quick reset on the card.  If the card is not powered
 * power up the card.  (Store and return the Atr/Length)
 *
 * Atr - Answer to Reset of the card.  The driver is responsible for caching
 * this value in case IFDHGetCapabilities is called requesting the ATR and it's
 * length.  This should not exceed MAX_ATR_SIZE.
 * 
 * AtrLength - Length of the Atr.  This should not exceed MAX_ATR_SIZE.
 * 
 * Notes:
 * 
 * Memory cards without an ATR should return IFD_SUCCESS on reset
 * but the Atr should be zero'd and the length should be zero
 * 
 * Reset errors should return zero for the AtrLength and return 
 * IFD_ERROR_POWER_ACTION.
 * 
 * returns:
 * 
 * IFD_SUCCESS
 * IFD_ERROR_POWER_ACTION
 * IFD_COMMUNICATION_ERROR
 * IFD_NOT_SUPPORTED
 *
 **/
RESPONSECODE IFDHPowerICC ( DWORD Lun, DWORD Action, 
			    PUCHAR Atr, PDWORD AtrLength ) 
{
    struct gpr400_atr atr;

    if(gpr_fd < 0){
        return IFD_COMMUNICATION_ERROR;
    }

    /* zero the ATR buffers */ 
    memset(Atr, '\0', *AtrLength);
    memset(&atr, '\0', sizeof(struct gpr400_atr));

    switch(Action){
    case IFD_POWER_UP:
#ifdef PCSC_DEBUG
	printf("IFDHPowerICC: power up\n");
#endif
        /* power-up and reset the card */
        if(ioctl(gpr_fd, GPR400_OPEN, &atr) < 0){
            return IFD_COMMUNICATION_ERROR;
        }
        /* set ATR values */
        if(atr.len > MAX_ATR_SIZE){
            /* error, too much data to copy */
            return IFD_ERROR_POWER_ACTION;
        }
        *AtrLength= atr.len;
        memcpy(Atr, atr.data, atr.len);
        memcpy(&gpr_atr, &atr, sizeof(struct gpr400_atr));
#ifdef ATR_DEBUG
        /* print the ATR */
	printf("IFDHPowerICC: ATR ");
        { 
        int i;
        for(i=0;i<gpr_atr.len;i++){
	    printf(" %x",Atr[i]);
        }
        }
	printf("\n");
#endif

        return IFD_SUCCESS;

    case IFD_POWER_DOWN:
#ifdef PCSC_DEBUG
	printf("IFDHPowerICC: power down\n");
#endif
        /* power-down the card */
        if(ioctl(gpr_fd, GPR400_CLOSE, 0) < 0){
            return IFD_COMMUNICATION_ERROR;
        }
        return IFD_SUCCESS;

    case IFD_RESET:
#ifdef PCSC_DEBUG
	printf("IFDHPowerICC: reset\n");
#endif
        /* reset the card */
        if(ioctl(gpr_fd, GPR400_OPEN, &atr) < 0){
            return IFD_COMMUNICATION_ERROR;
        }
        /* set ATR values */
        if(atr.len > MAX_ATR_SIZE){
            /* error, too much data to copy */
            return IFD_ERROR_POWER_ACTION;
        }
        *AtrLength= atr.len;
        memcpy(Atr, atr.data, atr.len);
        memcpy(&gpr_atr, &atr, sizeof(struct gpr400_atr));

#ifdef ATR_DEBUG
        /* print the ATR */
	printf("IFDHPowerICC: ATR ");
        { 
        int i;
        for(i=0;i<gpr_atr.len;i++){
	    printf(" %x",Atr[i]);
        }
        }
	printf("\n");
#endif

        return IFD_SUCCESS;

    default:
#ifdef PCSC_DEBUG
	printf("IFDHPowerICC: unknown (not supported)\n");
#endif
        return IFD_NOT_SUPPORTED;
    }

}/* IFDHPowerICC */

/**
 * This function performs an APDU exchange with the card/slot specified by
     Lun.  The driver is responsible for performing any protocol specific exchanges
     such as T=0/1 ... differences.  Calling this function will abstract all protocol
     differences.

     SendPci
     Protocol - 0, 1, .... 14
     Length   - Not used.

     TxBuffer - Transmit APDU example (0x00 0xA4 0x00 0x00 0x02 0x3F 0x00)
     TxLength - Length of this buffer.
     RxBuffer - Receive APDU example (0x61 0x14)
     RxLength - Length of the received APDU.  This function will be passed
     the size of the buffer of RxBuffer and this function is responsible for
     setting this to the length of the received APDU.  This should be ZERO
     on all errors.  The resource manager will take responsibility of zeroing
     out any temporary APDU buffers for security reasons.
  
     RecvPci
     Protocol - 0, 1, .... 14
     Length   - Not used.

     Notes:
     The driver is responsible for knowing what type of card it has.  If the current
     slot/card contains a memory card then this command should ignore the Protocol
     and use the MCT style commands for support for these style cards and transmit 
     them appropriately.  If your reader does not support memory cards or you don't
     want to then ignore this.

     RxLength should be set to zero on error.

     returns:
     
     IFD_SUCCESS
     IFD_COMMUNICATION_ERROR
     IFD_RESPONSE_TIMEOUT
     IFD_ICC_NOT_PRESENT
     IFD_PROTOCOL_NOT_SUPPORTED
 *
 **/
RESPONSECODE IFDHTransmitToICC ( DWORD Lun, SCARD_IO_HEADER SendPci, 
				 PUCHAR TxBuffer, DWORD TxLength, 
				 PUCHAR RxBuffer, PDWORD RxLength, 
				 PSCARD_IO_HEADER RecvPci ) 
{
    struct gpr400_cmd cmd;
    int i;
    UCHAR Le= 0;
    /*
	struct gpr400_cmd {
        unsigned char dir;
        unsigned char cla;
        unsigned char ins;
        unsigned char p1;
        unsigned char p2;
        unsigned char len;
        unsigned char data[256];
        unsigned char status;
        unsigned char sw1;
        unsigned char sw2;
	};
    */

    *RxLength= 0;

    if(gpr_fd < 0) {
        return IFD_COMMUNICATION_ERROR;
    }

    /* zero the command structure */
    memset(&cmd,'\0',sizeof(struct gpr400_cmd));

    /* 
     * NOTE: the following switch statement assumes that
     *       T=0 and T=1 differences are handled in the hardware.
     *       There is no indication that this is the case.
     *       There is no indication that this is NOT the case.
     *       If T=1 operation fails, then this should be fixed.
     */

    switch(SendPci.Protocol){
    case T_1: /* T=1 protocol */
        /* assume T=1 is handled like T=0 */
    case T_0: /* T=0 protocol */

        /* is this an input or output command? */
        if(TxLength < APDU_SIZE){
            /* not enough data to make a full APDU */
            return IFD_COMMUNICATION_ERROR;
        }else
        if( (TxLength == APDU_SIZE+1) && (TxBuffer[APDU_SIZE] != 0) ){
            /* expect response data (READ) */

#ifdef PCSC_DEBUG
    printf("IFDHTransmitToICC: read command\n");
#endif

            cmd.dir= 1; /* READ */
            cmd.cla= TxBuffer[0];
            cmd.ins= TxBuffer[1];
            cmd.p1=  TxBuffer[2];
            cmd.p2=  TxBuffer[3];
            cmd.len= TxBuffer[4];

            if(TxLength == (TxBuffer[APDU_SIZE]+APDU_SIZE+1)){
                Le= 0;
            }else{
                Le= cmd.len;
            }

#ifdef PCSC_DEBUG
    printf("IFDHTransmitToICC: ");
    {
        int i;
        for(i=0;i<TxLength;i++){
            printf(" %x",TxBuffer[i]);
        }
    }
    printf("\n");
#endif

            if(ioctl(gpr_fd, GPR400_CMD, &cmd) < 0){
                /* error check */
                return IFD_COMMUNICATION_ERROR;
            }

#ifdef PCSC_DEBUG
    printf("IFDHTransmitToICC: status=%2x\n",cmd.status);
#endif

            /* AFAIK, the only 'good' return code is 0x9000 */
            if((cmd.sw1 != 0x90) || (cmd.sw2 != 0x00)){
                /* ISO 7816-4 error condition */
                Le= 0; /* will only return sw1 and sw2 */
            }

            /* copy the response APDU to the Rx buffers */
            *RxLength= Le+2;
            for(i=0;i<Le;i++){
                RxBuffer[i]= cmd.data[i];
            }
            RxBuffer[i]= cmd.sw1; i++;
            RxBuffer[i]= cmd.sw2;

        }else
        if( TxLength >= APDU_SIZE ){
            /* WRITE */

            /* Input command */
#ifdef PCSC_DEBUG
    printf("IFDHTransmitToICC: write command\n");
#endif

            cmd.dir= 0; /* WRITE */
            cmd.cla= TxBuffer[0];
            cmd.ins= TxBuffer[1];
            cmd.p1=  TxBuffer[2];
            cmd.p2=  TxBuffer[3];
            cmd.len= TxBuffer[APDU_SIZE];

            if(TxLength == (TxBuffer[APDU_SIZE]+APDU_SIZE+1)){
                Le= 0;
            }else{
                Le= cmd.len;
            }

#ifdef PCSC_DEBUG
    printf("IFDHTransmitToICC: ");
    {
        int i;
        for(i=0;i<TxLength;i++){
            printf(" %x",TxBuffer[i]);
        }
    }
    printf("\n");
#endif

            for(i=0;i<cmd.len;i++){
                cmd.data[i]= TxBuffer[i+5];
            }

            if(ioctl(gpr_fd, GPR400_CMD, &cmd) < 0){
                /* error check */
                return IFD_COMMUNICATION_ERROR;
            }

            /* AFAIK, the only 'good' return code is 0x9000 */
            if((cmd.sw1 != 0x90) || (cmd.sw2 != 0x00)){
                /* ISO 7816-4 error condition */
                Le= 0; /* will only return sw1 and sw2 */
            }

            /* copy the response APDU to the Rx buffers */
            *RxLength= Le+2;
            for(i=0;i<Le;i++){
                RxBuffer[i]= cmd.data[i];
            }
            RxBuffer[i]= cmd.sw1; i++;
            RxBuffer[i]= cmd.sw2;

#ifdef PCSC_DEBUG
    printf("IFDHTransmitToICC: status=%2x\n",cmd.status);
#endif

            RecvPci->Protocol= SendPci.Protocol;
            RecvPci->Length= 0;

        }

        break;

    default:
        return IFD_PROTOCOL_NOT_SUPPORTED;
    }
  
#ifdef PCSC_DEBUG
    printf("IFDHTransmitToICC: end\n");
#endif

    return IFD_SUCCESS;
}/* IFDHTransmitToICC */

/**
 * This function performs a data exchange with the reader (not the card)
     specified by Lun.  Here XXXX will only be used.
     It is responsible for abstracting functionality such as PIN pads,
     biometrics, LCD panels, etc.  You should follow the MCT, CTBCS 
     specifications for a list of accepted commands to implement.

     TxBuffer - Transmit data
     TxLength - Length of this buffer.
     RxBuffer - Receive data
     RxLength - Length of the received data.  This function will be passed
     the length of the buffer RxBuffer and it must set this to the length
     of the received data.

     Notes:
     RxLength should be zero on error.
 *
 **/

RESPONSECODE IFDHControl(DWORD Lun, DWORD plop, PUCHAR TxBuffer, DWORD TxLength, PUCHAR RxBuffer,
     DWORD plop2, LPDWORD RxLength) 
{
#ifdef PCSC_DEBUG
	printf("IFDHControl:\n");
#endif
    return IFD_NOT_SUPPORTED;

}

/**
 * check if there is a smartcard in the reader
 * @param Lun
 * @return IFD_ICC_PRESENT, IFD_ICC_NOT_PRESENT, IFD_COMMUNICATION_ERROR
 **/
RESPONSECODE IFDHICCPresence( DWORD Lun ) {
    struct gpr400_status status;

    if(gpr_fd < 0){
        return IFD_COMMUNICATION_ERROR;
    }
    if(ioctl(gpr_fd, GPR400_STATUS, &status) < 0){
        return IFD_COMMUNICATION_ERROR;
    }
    if(status.card_inserted == 0){
        return IFD_ICC_NOT_PRESENT;
    }else{
        return IFD_ICC_PRESENT;
    }
}/* IFDHICCPresence */

/* gpr400_ifd.c */
