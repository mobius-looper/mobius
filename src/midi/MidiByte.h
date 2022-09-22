/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * MOBIUS PUBLIC INTERFACE
 *
 * Platform independent MIDI message constants.
 * 
 */

#ifndef MIDIBYTE_H
#define MIDIBYTE_H

/* MS_
 *
 * MIDI status bytes.
 */

// sometimes used as the "wildcard" type
#define MS_NULL			0

// used to match any "controller" type which includes pitch, touch etc
#define MS_ANYCONTROL	1

// used to identify "name" events
#define MS_NAME			2

#define MS_NOTEOFF    		0x80
#define MS_NOTEON     		0x90
#define MS_POLYPRESSURE  	0xA0
#define MS_CONTROL    		0xB0
#define MS_PROGRAM    		0xC0
#define MS_TOUCH      		0xD0
#define MS_BEND		  		0xE0

#define MIDI_IS_CONTROLLER_STATUS(type) \
type == MS_POLYPRESSURE || type == MS_CONTROL || type == MS_TOUCH || \
type == MS_BEND

#define MS_SYSEX      		0xF0
#define MS_QTRFRAME   		0xF1
#define MS_SONGPOSITION		0xF2
#define MS_SONGSELECT 		0xF3
#define MS_TUNEREQ    		0xF6
#define MS_EOX	      		0xF7

#define MS_CLOCK      0xF8
#define MS_START      0xFA
#define MS_CONTINUE   0xFB
#define MS_STOP       0xFC
#define MS_SENSE      0xFE
#define MS_RESET      0xFF

/* MC_
 *
 * MIDI Controller numbers.
 *
 * Normal continuous controllers will range from 0 to 32, and 64 to 95.  
 * Others are in the highest undefined range just below the channel 
 * mode messages.
 *
 * These (or an unassigned number) will be stored in the "key" field
 * of the event.
 *
 * This is kind of a kludge since pitch and aftertouch aren't really
 * continuous controllers but it makes it easier to process the
 * controller event list.
 *
 */

/* continuous 14 bit - MSB: 0-1f, LSB: 20-3f */

#define MC_MODWHEEL  0x01
#define MC_BREATH    0x02
#define MC_FOOT      0x04
#define MC_PORTATIME 0x05
#define MC_DATAENTRY 0x06
#define MC_VOLUME    0x07
#define MC_BALANCE   0x08
#define MC_PAN	     0x0a
#define MC_EXPRESSION 0x0b
#define MC_GENERAL1  0x10
#define MC_GENERAL2  0x11
#define MC_GENERAL3  0x12
#define MC_GENERAL4  0x13

/* continuous 7 bit (switches: 0-3f=off, 40-7f=on) */

#define MC_SUSTAIN   0x40
#define MC_PORTA     0x41
#define MC_SUSTENUTO 0x42
#define MC_SOFTPEDAL 0x43
#define MC_HOLD2     0x45
#define MC_GENERAL5  0x50
#define MC_GENERAL6  0x51
#define MC_GENERAL7  0x52
#define MC_GENERAL8  0x53
#define MC_EXTDEPTH	0x5b
#define MC_TREMOLODEPTH 0x5c
#define MC_CHORUSDEPTH	0x5d
#define MC_CELESTEDEPTH 0x5e
#define MC_PHASERDEPTH	0x5f

/* parameters */

#define MC_DATAINCR  0x60
#define MC_DATADECR  0x61
#define MC_NRPNL     0x62
#define MC_NRPNH     0x63
#define MC_RPNL      0x64
#define MC_RPNH      0x65

#define MC_MAX	     0x78	/* max controller value */

/* special modes */

#define MC_PITCH     118
#define MC_TOUCH     119
#define MC_NAME      121	// what is this???
#define MC_LOOP

/**
 * Examine a MIDI status byte and determine if there will
 * be one or two data bytes for this status byte.
 * Assumptions about the currently defined MIDI v1 status bytes so the
 * test is simple.
 */
#define IS_TWO_BYTE_EVENT(byte) ((byte & 0x60) != 0x40)

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
