/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Subclass of MidiInput for Mac.
 * 
 */

#include <stdio.h>

#include "Util.h"
#include "Trace.h"
#include "Thread.h"
#include "MacUtil.h"

#include "MidiInput.h"
#include "MacMidiEnv.h"

//////////////////////////////////////////////////////////////////////
//
// MacMidiInput
//
//////////////////////////////////////////////////////////////////////

PUBLIC MacMidiInput::MacMidiInput(MacMidiEnv* env, MidiPort* port) :
	MidiInput(env, port)
{
	mSource = 0;
	mInputPort = 0;
}

PUBLIC MacMidiInput::~MacMidiInput()
{
	disconnect();
}

PRIVATE MIDIClientRef MacMidiInput::getClient()
{
	return ((MacMidiEnv*)mEnv)->getClient();
}

/**
 * Read proc needed for connect.
 */
extern void MacInputReadProc(const MIDIPacketList* packets, void* arg1, void* arg2);

/**
 * Attempts to open the native port for a MidiPort.
 * Returns a non-zero error code on failure.
 * If there is no currently designated input port, the request is ignored
 * and no error code is returned.
 */
PUBLIC int MacMidiInput::connect(void)
{
	OSStatus status;
	int error = 0;

	if (mSource == NULL && mPort != NULL) {

		// downcast so we can get to the special methods
		MacMidiPort* port = (MacMidiPort*)mPort;

		// these we can reuse
		if (mInputPort == NULL) {
			// each port has a name, not sure why or if it has to be unique
			CFStringRef name = MakeCFStringRef("MacMidiInput:port");

			status = MIDIInputPortCreate(getClient(),
										 name, 
										 MacInputReadProc,
										 this,
										 &mInputPort);
			CheckStatus(status, "MIDIInputPortCreate");
		}

		if (mInputPort == NULL) 
		  error = 1;	// ERR_NO_INPUT_PORT
		else {
			// third arg is "connRefCon" which will be passed
			// to the MIDIReadProc as a way to identify the source
			status = MIDIPortConnectSource(mInputPort, 
										   port->getEndpoint(),
										   port);
			if (CheckStatus(status, "MIDIPortConnectSource"))
			  mSource = port->getEndpoint();
			else
			  error = 2;
		}
	}

	return error;
}

/**
 * Closes the input port, though the object remains allocated and
 * can be reconnected later.
 */
PUBLIC void MacMidiInput::disconnect()
{
	if (mSource != NULL) {
		OSStatus status = MIDIPortDisconnectSource(mInputPort, mSource);
		CheckStatus(status, "MIDIPortDisconnectSource");
		mSource = NULL;
	}
}

PUBLIC bool MacMidiInput::isConnected()
{
	return (mSource != NULL);
}

/**
 * Since we're already running in a thread managed by MIDIServer
 * we don't have to signal our own monitor thread as we don on Windows.
 */
PUBLIC void MacMidiInput::notifyEventsReceived()
{
	if (mListener != NULL) {
		mListener->midiInputEvent(this);
	}
	else {
		// ignore everything
		ignoreSysex();
		ignoreEvents();
	}
}

PUBLIC void MacMidiInput::ignoreSysex()
{
}

//////////////////////////////////////////////////////////////////////
//
// Interrupt Handler
//
//////////////////////////////////////////////////////////////////////

/**
 * The MIDIReadProc give to each input port.
 * First arg is the refCon passed to MIDIInputPortCreate.
 * Second arg is the refCon passed to MIDIPortConnectSource.
 */
void MacInputReadProc(const MIDIPacketList* packets, void* arg1, void* arg2)
{
	MacMidiInput* input = (MacMidiInput*)arg1;
	MacMidiPort* port = (MacMidiPort*)arg2;

	input->processPackets(packets, port);
}

PRIVATE void MacMidiInput::processPackets(const MIDIPacketList* packets, 
										  MacMidiPort* port)
{
	// Jesus-fucking-Christ who came up with MIDIPacketList?!
	if (packets != NULL) {

		MidiEvent* events = NULL;
		MidiEvent* lastEvent = NULL;

		const MIDIPacket *packet = &packets->packet[0];
		for (int i = 0; i < packets->numPackets; i++) {
			MidiEvent* event = NULL;

			// This is a "host clock time" as returned by 
			// mach_absolute_time or UpTime.  Ultimately it is a UInt64.
			// %ld doesn't seem to print it properly?
			MIDITimeStamp time = packet->timeStamp;
			/*
			  if (time != 0) {
			  printf("MIDIPacket timestamp %ld\n", time);
			  fflush(stdout);
			  }
			*/

			int length = packet->length;
			const Byte* data = packet->data;
			int psn = 0;
			
			// not uncommon to get more than one message in a packet
			// if you're twisting more than one knob at the same time
			// !! in theory realtime (>= 0xF8) can be interleaved
			// within other multi-byte messages, not handling that

			while (psn < length) {
				
				int statusChannel = data[psn++];
				int status = statusChannel & 0xF0;
				int dataBytes = 0;

				if (status < 0x80) {
					// we're either in the middle of a sysex or running
					// status is being used
					Trace(1, "Unexpected data byte, ignoring MIDI packet!\n");
					dataBytes = -1;
				}
				else if (status == MS_PROGRAM || status == MS_TOUCH) {
					dataBytes = 1;
				}
				else if (status != 0xF0) {
					// noteon, noteoff, polypressure, control, bend
					dataBytes = 2;
				}
				else {
					// sysex, clocks, various
					dataBytes = 0;
					switch (statusChannel) {
						case MS_SYSEX:
							Trace(1, "Ignoring sysex!\n");
							dataBytes = -1;
							break;
						case MS_QTRFRAME:
							dataBytes = 1;
							break;
						case MS_SONGPOSITION:
							dataBytes = 2;
							break;
						case MS_SONGSELECT:
							dataBytes = 1;
							break;
					}
				}

				if (dataBytes == 0) {
					processShortMessage(statusChannel);
				}
				else if (dataBytes == 1) {
					if (psn < length) {
						int byte1 = data[psn++];
						int msg = statusChannel | (byte1 << 8);
						processShortMessage(msg);
					}
					else {
						Trace(1, "Incomplete MIDI message %ld\n", 
							  (long)statusChannel);
					}
				}
				else if (dataBytes == 2) {
					if (psn < (length - 1)) {
						int byte1 = data[psn++];
						int byte2 = data[psn++];
						int msg = statusChannel | (byte1 << 8) | (byte2 << 16);
						processShortMessage(msg);
					}
					else {
						Trace(1, "Incomplete MIDI message %ld\n", 
							  (long)statusChannel);
						// ignore the rest of the buffer
						psn = length;
					}
				}
				else {
					// malformed message, stop now
					psn = length;
				}
			}

			packet = MIDIPacketNext(packet);
		}
	}
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
