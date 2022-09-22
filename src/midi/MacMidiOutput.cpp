/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Subclass of MidiOutput for Mac.
 * 
 */

#include <stdio.h>

#include "Util.h"
#include "Trace.h"
#include "Thread.h"
#include "MacUtil.h"

#include "MidiOutput.h"
#include "MacMidiEnv.h"

//////////////////////////////////////////////////////////////////////
//
// MacMidiOutput
//
//////////////////////////////////////////////////////////////////////

PUBLIC MacMidiOutput::MacMidiOutput(MacMidiEnv* env, MidiPort* port) :
	MidiOutput(env, port)
{
	mOutputPort = NULL;
	mDestination = NULL;
}

PUBLIC MacMidiOutput::~MacMidiOutput()
{
	disconnect();
}

PRIVATE MIDIClientRef MacMidiOutput::getClient()
{
	return ((MacMidiEnv*)mEnv)->getClient();
}

/**
 * Attempts to open the native port for a MidiPort.
 * Returns a non-zero error code on failure.
 * If there is no currently designated input port, the request is ignored
 * and no error code is returned.
 */
PUBLIC int MacMidiOutput::connect(void)
{
	OSStatus status;
	int error = 0;

	if (mDestination == NULL && mPort != NULL) {
		MacMidiPort* port = (MacMidiPort*)mPort;

		// these can apparently be reused
		if (mOutputPort == NULL) {
			// each port has a name, not sure why or if it has to be unique
			CFStringRef name = MakeCFStringRef("MacMidiOutput:port");

			status = MIDIOutputPortCreate(getClient(),
										  name,
										  &mOutputPort);
			CheckStatus(status, "MIDIOutputPortCreate");
		}

		if (mOutputPort == NULL)
		  error = 1;
		else {
			// you don't appear to need to connect the destination
			// with the port, the association is done for every MIDISend call
			mDestination = port->getEndpoint();
		}
	}

	return error;
}

/**
 * Closes the input port, though the object remains allocated and
 * can be reconnected later.
 */
PUBLIC void MacMidiOutput::disconnect()
{
	if (mDestination != NULL) {
		OSStatus status = MIDIPortDisconnectSource(mOutputPort, mDestination);
		CheckStatus(status, "MIDIPortDisconnectSource");
		mDestination = NULL;
	}
}

PUBLIC bool MacMidiOutput::isConnected()
{
	return (mDestination != NULL);
}

/**
 * Send a short message.
 */
PUBLIC void MacMidiOutput::send(int msg)
{
	if (mOutputPort != NULL && mDestination != NULL) {
		
		// this is the style you see in most examples
		/*
		MIDIPacketList pktlist;
		MIDIPacket* packet;
		packet = MIDIPacketListInit(&pktlist);
		packet = MIDIPacketListAdd( &pktlist,
									sizeof(MIDIPacketList),
									packet,
									0, // time stamp
									nbytes, 
									bytes);
		*/

		MIDIPacketList packets;
		MIDIPacket* packet = &(packets.packet[0]);
		Byte* data = &(packet->data[0]);
		packets.numPackets = 1;
		packet->timeStamp = 0;

		// sigh, have to undo some of the work MidiOutput::send does
		int status = msg & 0xFF;

		data[0] = status;
		int bytes = 1;

		if (status < 0xF0) {
			data[1] = (msg >> 8) & 0xFF;
			if (!IS_TWO_BYTE_EVENT(status))
			  bytes = 2;
			else {
				data[2] = msg >> 16;
				bytes = 3;
			}
		}
		else if (status == MS_SONGPOSITION) {
			data[1] = (msg >> 8) & 0xFF;
			data[2] = msg >> 16;
			bytes = 3;
		}
		else if (status == MS_SONGSELECT) {
			data[1] = (msg >> 8) & 0xFF;
			bytes = 2;
		}
		else if (status == 0xF0) {
			// ignoring sysex
			bytes = 0;
		}

		if (bytes > 0) {
			packet->length = bytes;
			OSStatus status = MIDISend(mOutputPort, mDestination, &packets);
			CheckStatus(status, "MIDISend");
		}

	}
}

PUBLIC int MacMidiOutput::sendSysex(const unsigned char *buffer, int length)
{
	return 0;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
