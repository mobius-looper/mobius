/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Base implementation of a MIDI input stream.
 * Will be subclassed on each platform.
 *
 */

#include <stdio.h>

#include "util.h"
#include "Trace.h"

#include "MidiEnv.h"
#include "MidiEvent.h"
#include "MidiOutput.h"

//////////////////////////////////////////////////////////////////////
//
// MidiOutput
//
//////////////////////////////////////////////////////////////////////

/**
 * Initialize a disconnected output stream.
 */
PUBLIC MidiOutput::MidiOutput(MidiEnv* env, MidiPort* port)
{
    mEnv                = env;
	mPort				= port;
    mNext               = NULL;

    mWeirdErrors        = 0;
	mSysexTimeouts		= 0;
}

/**
 * Destructs a midi output device.
 */
PUBLIC MidiOutput::~MidiOutput(void)
{
	// get warning if we call an abstract virtual from a destructor,
	// make the subclasses do it
	//disconnect();
	printWarnings();
}

PUBLIC void MidiOutput::printWarnings() 
{
	if (mWeirdErrors != 0)
	  printf("%d weird MidiOutput errors!\n", mWeirdErrors);

	if (mSysexTimeouts != 0)
	  printf("%d MidiOutput sysex timeouts!\n", mSysexTimeouts);
}

PUBLIC MidiPort* MidiOutput::getPort()
{
    return mPort;
}

PUBLIC void MidiOutput::setPort(MidiPort* port)
{
	disconnect();
	mPort = port;
}

//////////////////////////////////////////////////////////////////////
//
// Short Messages
//
//////////////////////////////////////////////////////////////////////

/**
 * Send an event fully described by a MidiEvent object.
 * The channel may be overridden.
 */
PUBLIC void MidiOutput::send(MidiEvent *e, int channel)
{
	if (mPort) {
		int msg = 0;
		int status = e->getStatus();

		if (status < 0xF0) {
			if (channel < 0)
			  msg = status | e->getChannel();		// do not override
			else
			  msg = status | channel;				// override channel
			
			msg |= e->getKey() << 8;
			if (IS_TWO_BYTE_EVENT(status))
                msg |= e->getVelocity() << 16;
		}
		else if (status == MS_SONGPOSITION) {
		  msg = status | 
			  (e->getKey() << 8) |
			  (e->getVelocity() << 16);
		}

		else if (status == MS_SONGSELECT)
		  msg = status | (e->getKey() << 8);

		else if (status != 0xF0)
            msg = status;

		// ignore if out of range or sysex
		if (msg)
		  send(msg);
	}
}

/**
 * Sends a program change event.
 */
PUBLIC void MidiOutput::sendProgram(int channel, int program)
{
    int msg = (MS_PROGRAM | (channel & 0x0F)) | program << 8;
    send(msg);
}

/**
 * Sends a control change event.
 */
PUBLIC void MidiOutput::sendControl(int channel, int type, int value)
{
    int msg = (MS_CONTROL | (channel & 0x0F)) | (type << 8) | (value << 16);
    send(msg);
}

/**
 * Sends a note on event.
 */
PUBLIC void MidiOutput::sendNoteOn(int channel, int key, int velocity)
{
    int msg = (MS_NOTEON | (channel & 0x0F)) | (key << 8) | (velocity << 16);
    send(msg);
}

/**
 * Sends a note off event.
 * Not supporting release velocity here? 
 */
PUBLIC void MidiOutput::sendNoteOff(int channel, int key)
{
    int msg = (MS_NOTEOFF | (channel & 0x0F)) | (key << 8);
    send(msg);
}

/**
 * Send a start event.
 */
PUBLIC void MidiOutput::sendStart(void)
{
    send(MS_START);
}

/**
 * Sends a stop event.
 */
PUBLIC void MidiOutput::sendStop(void)
{
	send(MS_STOP);
}

/**
 * Send a continue event.
 */
PUBLIC void MidiOutput::sendContinue(void)
{
	send(MS_CONTINUE);
}

/**
 * Sends a clock event.
 */
PUBLIC void MidiOutput::sendClock(void)
{
	send(MS_CLOCK);
}

/**
 * Sends a song position.
 */
PUBLIC void MidiOutput::sendSongPosition(int psn)
{
    int msg = MS_SONGPOSITION | ((psn & 0x7F) << 8) | 
		(((psn >> 7) & 0x7F) << 16);
    send(msg);
}

/**
 * Sends a song select event.
 */
PUBLIC void MidiOutput::sendSongSelect(int song)
{
    int msg = MS_SONGSELECT | ((song & 0x7F) << 8);
    send(msg);
}

/**
 * Sends a local on/off message.
 * This is actually a special form of control change.
 */
PUBLIC void MidiOutput::sendLocal(int channel, int onoff)
{
    int msg = (MS_CONTROL | (channel & 0x0F)) | (122 << 8);
    if (onoff)
      msg |= 127 << 16;
    send(msg);
}

/**
 * Sends all notes off on a particular channel.
 */
PUBLIC void MidiOutput::sendAllNotesOff(int channel)
{
    int msg = (MS_CONTROL | (channel & 0x0F)) | (123 << 8);
    send(msg);
}

/**
 * Sends all notes off on all channels, and individual note off events
 * for each note.
 */
PUBLIC void MidiOutput::panic(void)
{
	if (mPort) {
		for (int i = 0 ; i < 16 ; i++) {
			sendAllNotesOff(i);
			for (int j = 0 ; j < 127 ; j++) 
			  sendNoteOff(i, j);
		}
	}
}

//////////////////////////////////////////////////////////////////////
//
// Sysex Messages
//
//////////////////////////////////////////////////////////////////////

// still need to work out the generic interface for these...

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
