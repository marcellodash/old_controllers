/* Name: main.c
 * Project: Multiple NES/SNES to USB converter
 * Author: Raphael Assenat <raph@raphnet.net>
 * Copyright: (C) 2007-2009 Raphael Assenat <raph@raphnet.net>
 * License: GPLv2
 * Tabsize: 4
 * Comments: Based on HID-Test by Christian Starkjohann
 */
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/wdt.h>
#include <util/delay.h>
#include <string.h>

#include "usbdrv.h"
#include "oddebug.h"
#include "gamepad.h"

#include "fournsnes.h"

#include "leds.h"
#include "devdesc.h"

static uchar *rt_usbHidReportDescriptor=NULL;
static uchar rt_usbHidReportDescriptorSize=0;
static uchar *rt_usbDeviceDescriptor=NULL;
static uchar rt_usbDeviceDescriptorSize=0;

/* The maximum number of independent reports that are supported. */
#define MAX_REPORTS 8	

PROGMEM int usbDescriptorStringSerialNumber[]  = {
	USB_STRING_DESCRIPTOR_HEADER(4),
	'1','0','0','0'
};

int usbDescriptorStringDevice[] = {
	USB_STRING_DESCRIPTOR_HEADER(DEVICE_STRING_LENGTH),
	DEFAULT_PROD_STRING
};

char usbDescriptorConfiguration[] = { 0 }; // dummy

uchar my_usbDescriptorConfiguration[] = {    /* USB configuration descriptor */
    9,          /* sizeof(usbDescriptorConfiguration): length of descriptor in bytes */
    USBDESCR_CONFIG,    /* descriptor type */
    18 + 7 * USB_CFG_HAVE_INTRIN_ENDPOINT + 9, 0,
                /* total length of data returned (including inlined descriptors) */
    1,          /* number of interfaces in this configuration */
    1,          /* index of this configuration */
    0,          /* configuration name string index */
#if USB_CFG_IS_SELF_POWERED
    USBATTR_SELFPOWER,  /* attributes */
#else
    USBATTR_BUSPOWER,   /* attributes */
#endif
    USB_CFG_MAX_BUS_POWER/2,            /* max USB current in 2mA units */
/* interface descriptor follows inline: */
    9,          /* sizeof(usbDescrInterface): length of descriptor in bytes */
    USBDESCR_INTERFACE, /* descriptor type */
    0,          /* index of this interface */
    0,          /* alternate setting for this interface */
    USB_CFG_HAVE_INTRIN_ENDPOINT,   /* endpoints excl 0: number of endpoint descriptors to follow */
    USB_CFG_INTERFACE_CLASS,
    USB_CFG_INTERFACE_SUBCLASS,
    USB_CFG_INTERFACE_PROTOCOL,
    0,          /* string index for interface */
//#if (USB_CFG_DESCR_PROPS_HID & 0xff)    /* HID descriptor */
    9,          /* sizeof(usbDescrHID): length of descriptor in bytes */
    USBDESCR_HID,   /* descriptor type: HID */
    0x01, 0x01, /* BCD representation of HID version */
    0x00,       /* target country code */
    0x01,       /* number of HID Report (or other HID class) Descriptor infos to follow */
    0x22,       /* descriptor type: report */
    USB_CFG_HID_REPORT_DESCRIPTOR_LENGTH, 0,  /* total length of report descriptor */
//#endif
#if USB_CFG_HAVE_INTRIN_ENDPOINT    /* endpoint descriptor for endpoint 1 */
    7,          /* sizeof(usbDescrEndpoint) */
    USBDESCR_ENDPOINT,  /* descriptor type = endpoint */
    0x81,       /* IN endpoint number 1 */
    0x03,       /* attrib: Interrupt endpoint */
    8, 0,       /* maximum packet size */
    USB_CFG_INTR_POLL_INTERVAL, /* in ms */
#endif
};


static Gamepad *curGamepad;


/* ----------------------- hardware I/O abstraction ------------------------ */

static void hardwareInit(void)
{
	uchar	i, j;

	// init port C as input with pullup
	DDRC = 0x00;
	PORTC = 0xff;
	
	/* 1101 1000 bin: activate pull-ups except on USB lines 
	 *
	 * USB signals are on bit 0 and 2. 
	 *
	 * Bit 1 is connected with bit 0 (rev.C pcb error), so the pullup
	 * is not enabled.
	 * */
	PORTD = 0xf8;   

	/* Usb pin are init as outputs */
	DDRD = 0x01 | 0x04;    

	
	j = 0;
	while(--j){     /* USB Reset by device only required on Watchdog Reset */
		i = 0;
		while(--i); /* delay >10ms for USB reset */
	}
	DDRD = 0x00;    /* 0000 0000 bin: remove USB reset condition */
			/* configure timer 0 for a rate of 12M/(1024 * 256) = 45.78 Hz (~22ms) */
	TCCR0 = 5;      /* timer 0 prescaler: 1024 */

	TCCR2 = (1<<WGM21)|(1<<CS22)|(1<<CS21)|(1<<CS20);
	OCR2 = 196; // for 60 hz

}

static uchar    reportBuffer[12];    /* buffer for HID reports */



/* ------------------------------------------------------------------------- */
/* ----------------------------- USB interface ----------------------------- */
/* ------------------------------------------------------------------------- */

static uchar    idleRates[MAX_REPORTS];           /* in 4 ms units */

uchar	usbFunctionDescriptor(struct usbRequest *rq)
{
	if ((rq->bmRequestType & USBRQ_TYPE_MASK) != USBRQ_TYPE_STANDARD)
		return 0;

	if (rq->bRequest == USBRQ_GET_DESCRIPTOR)
	{
		// USB spec 9.4.3, high byte is descriptor type
		switch (rq->wValue.bytes[1])
		{
			case USBDESCR_DEVICE:
				usbMsgPtr = rt_usbDeviceDescriptor;		
				return rt_usbDeviceDescriptorSize;
			case USBDESCR_HID_REPORT:
				usbMsgPtr = rt_usbHidReportDescriptor;
				return rt_usbHidReportDescriptorSize;
			case USBDESCR_CONFIG:
				usbMsgPtr = my_usbDescriptorConfiguration;
				return sizeof(my_usbDescriptorConfiguration);
		}
	}

	return 0;
}


static uchar reportPos=0;

uchar	usbFunctionSetup(uchar data[8])
{
	usbRequest_t    *rq = (void *)data;
	int i;

	usbMsgPtr = reportBuffer;

	/* class request type */
	if((rq->bmRequestType & USBRQ_TYPE_MASK) == USBRQ_TYPE_CLASS){    
		switch (rq->bRequest)
		{
			case USBRQ_HID_GET_REPORT:
				/* wValue: ReportType (highbyte), ReportID (lowbyte) */
				reportPos=0;
				return curGamepad->buildReport(reportBuffer, rq->wValue.bytes[0]);
			
			case USBRQ_HID_GET_IDLE:
				if (rq->wValue.bytes[0] > 0 && rq->wValue.bytes[0] <= MAX_REPORTS) {
					usbMsgPtr = idleRates + (rq->wValue.bytes[0] - 1);
					return 1;
				}
				break;			
			
			case USBRQ_HID_SET_IDLE:
				if (rq->wValue.bytes[0]==0) {
					for (i=0; i<MAX_REPORTS; i++)
						idleRates[i] = rq->wValue.bytes[1];	
				}
				else {
					if (rq->wValue.bytes[0] > 0 && rq->wValue.bytes[0] <= MAX_REPORTS) {
						idleRates[rq->wValue.bytes[0]-1] = rq->wValue.bytes[1];
					}
				}
				break;
		}
	} else {
		/* no vendor specific requests implemented */
	}
	return 0;
}

/* ------------------------------------------------------------------------- */

int main(void)
{
	char must_report = 0;	/* bitfield */
	uchar   idleCounters[MAX_REPORTS];
	int i;
	unsigned char run_mode;

	memset(idleCounters, 0, MAX_REPORTS);


	DDRB |= 0x01;
	DDRB &= ~0x06;
	PORTB |= 0x06; /* enable pull up on DB1 and DB2 */
	PORTB &= ~0x01; /* Set DB0 to low */

	_delay_ms(10); /* let pins settle */
	run_mode = (PINB & 0x06)>>1;

	switch(run_mode)
	{
			// Close JP1 to disable live auto-detect
		case 2:
			disableLiveAutodetect();
			break;

		case 1:
		case 3:
		case 4:
			break;
	}

	curGamepad = fournsnesGetGamepad();

	// configure report descriptor according to
	// the current gamepad
	rt_usbHidReportDescriptor = curGamepad->reportDescriptor;
	rt_usbHidReportDescriptorSize = curGamepad->reportDescriptorSize;

	if (curGamepad->deviceDescriptor != 0)
	{
		rt_usbDeviceDescriptor = (void*)curGamepad->deviceDescriptor;
		rt_usbDeviceDescriptorSize = curGamepad->deviceDescriptorSize;
	}
	else
	{
		// use descriptor from devdesc.c
		//
		rt_usbDeviceDescriptor = (void*)usbDescrDevice;
		rt_usbDeviceDescriptorSize = getUsbDescrDevice_size();
	}

	// patch the config descriptor with the HID report descriptor size
	my_usbDescriptorConfiguration[25] = rt_usbHidReportDescriptorSize;

	//wdt_enable(WDTO_2S);
	hardwareInit();
	curGamepad->init();
	odDebugInit();
	usbInit();

	curGamepad->update();

	sei();
	DBG1(0x00, 0, 0);

	
	for(;;){	/* main event loop */
		wdt_reset();

		// this must be called at each 50 ms or less
		usbPoll();

		/* Read the controllers at 60hz */
		if (TIFR & (1<<OCF2))
		{
			TIFR = 1<<OCF2;
				
			curGamepad->update();

			/* Check what will have to be reported */
			for (i=0; i<curGamepad->num_reports; i++) {
				if (curGamepad->changed(i+1)) {
					must_report |= (1<<i);
				}
			}
		}

		/* Try to report at the granularity requested by
		 * the host. */
		if (TIFR & (1<<TOV0)) {   /* 22 ms timer */
			TIFR = 1<<TOV0;

			for (i=0; i<curGamepad->num_reports; i++)
			{
				// 0 means 
				if(idleRates[i] != 0){
					if (idleCounters[i] > 4) {
						idleCounters[i] -= 5;   /* 22 ms in units of 4 ms */
					} else {
						// reset the counter and schedule a report for this
						idleCounters[i] = idleRates[i];
						must_report |= (1<<i);
					}
				}
			}
		}

	
			
		if(must_report)
		{
			for (i = 0; i < curGamepad->num_reports; i++)
			{
				if ((must_report & (1<<i)) == 0) 
					continue;

				if (usbInterruptIsReady())
				{ 	
					char len;

					len = curGamepad->buildReport(reportBuffer, i+1);
					usbSetInterrupt(reportBuffer, len);

					while (!usbInterruptIsReady())
					{
						usbPoll();
						wdt_reset();
					}
				}
			}

			must_report = 0;
		}
	}
	return 0;
}

/* ------------------------------------------------------------------------- */
