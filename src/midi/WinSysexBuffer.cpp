/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 *
 * A class used by MidiIn to maintain state related
 * to SYSEX reception.
 *
 */

#include <stdio.h>
#include <memory.h>
#include <windows.h>
#include <mmsystem.h>

#include "Util.h"
#include "Trace.h"

#include "WinMidiEnv.h"
#include "WinSysexBuffer.h"

PUBLIC WinSysexBuffer::WinSysexBuffer() 
{
	mIn						= NULL;
	mLink					= NULL;
	mNext 					= NULL;
	mLength 				= 0;
	mPort					= 0;
	mFinished 				= false;
	mProcessed				= false;
	mAdded					= false;
	mAccessible				= false;
	mError					= false;

	mHeader.lpData			= (char *)mBuffer;
	mHeader.dwBufferLength 	= MIDI_SYSEX_MAX;
	mHeader.dwBytesRecorded	= 0;
	mHeader.dwUser 			= 0;
	mHeader.dwFlags 		= 0;

	init();
}

PUBLIC WinSysexBuffer::~WinSysexBuffer() 
{
	// should have been done by now
	if (mPort)
	  trace("ERROR: ~WinSysexBuffer: still prepared!\n");
}

PUBLIC void WinSysexBuffer::init()
{
	mFinished 	= false;
	mAccessible = false;
	mProcessed 	= false;
	mError 		= false;

	// the number of bytes received, maintained manually since
	// dwBytesRecorded isn't always updated by the driver while
	// bytes come in
	mLength = 0;

	// This is important, after the buffer has been used
	// once, this doesn't seem to be reset to zero.
	mHeader.dwBytesRecorded = 0;

	// set the bytes to an illegal value
	for (int i = 0 ; i < MIDI_SYSEX_MAX ; i++)
	  mBuffer[i] = 0xFF;
}

PUBLIC void WinSysexBuffer::setFinished()
{
	mFinished = true;
	
	// this should also now be accurate
	mLength = mHeader.dwBytesRecorded;
}

PUBLIC void WinSysexBuffer::process() 
{
	// should we bother with this if the error flag is on?
	if (mError)
	  mProcessed = true;

	else if (!mProcessed) {
		// KLUDGE: The M1 program buffer came in padded out to 
		// a 4byte boundary, since the receiver is checking for a particular
		// size, trim the padding.

		int adjust = 0;
		while (mBuffer[mLength-1] != 0xF7 && mLength > 0) {
			mLength--;
			adjust++;
		}
		
		if (adjust > 0)
		  trace("WinSysexBuffer::process"
				" Trimmed %d pad bytes from sysex buffer.\n", adjust);

		// KLUDGE: we seem to consistently lose the initial F0 byte, not sure
		// if that's normal or a bug.  So we can deal with nice normalized
		// sysex blocks, add it here.
		// If this happens all the time, we should just keep an extra
		// byte handy at the front.

		if (mLength > 0 && mBuffer[0] != 0xF0) {

			trace("WinSysexBuffer::process"
				  " Adding initial F0 byte.\n");

			for (int i = mLength ; i > 0 ; i--)
			  mBuffer[i] = mBuffer[i-1];

			mBuffer[0] = 0xF0;
			mLength++;
		}

		mProcessed = true;
	}
}

PUBLIC int WinSysexBuffer::getBytesReceived() 
{
	if (!mFinished) {

		int bytes = mHeader.dwBytesRecorded;

		if (bytes != 0) {
			// seems to be working, so don't clutter up the output stream
			// trace("WinSysexBuffer::getSysexBytesReceived %d\n", bytes);

			// hmm, if we had been calculating mLength, and for some
			// reason the device driver decided to post a count here, it
			// may get stuck again.  We could just always calculate, 
			// or try to calculate only if dwBytesRecorded stays 
			// at the same value twice in a row.

			mLength = bytes;
		}
		else {
			// hack, since this field doesn't seem to be updated propertly, try
			// to determine it by looking at the bytes in the buffer.
			// We keep a running total in mLength so we don't have
			// to scan the buffer from the front every time.
	
			int maxlen = mHeader.dwBufferLength;
			int actual = 0;
			for (int i = mLength ; i < maxlen ; i++) {
				if (mBuffer[i] == 0xFF) {
					actual = i;
					break;
				}
			}

			mLength = actual;
		}
	}

	return mLength;
}

PUBLIC int WinSysexBuffer::prepare(HMIDIIN port)
{
	int error = 0;

	if (mPort != 0) {
		// already prepared!
		trace("ERROR: WinSysexBuffer::prepare already prepared!\n");
	}
	else {
		trace("WinSysexBuffer::prepare midiInPrepareHeader\n");
		int rc = midiInPrepareHeader(port, &mHeader, sizeof(MIDIHDR));
		if (rc == MMSYSERR_NOERROR)
		  mPort = port;
		else {
			mIn->setError(rc);
			error = 1;
		}
	}

	return error;
}

PUBLIC void WinSysexBuffer::unprepare()
{
	if (mPort) {

		trace("WinSysexBuffer::unprepare midiUnprepareHeader\n");
		int rc = midiInUnprepareHeader(mPort, &mHeader, sizeof(MIDIHDR));
		if (rc != MMSYSERR_NOERROR) {
			if (mIn != NULL)
			  mIn->setError(rc);
			else
			  trace("ERROR: WinSysexBuffer with no input device!\n");
		}
		mPort = 0;
		mAdded = false;
	}
}

PUBLIC void WinSysexBuffer::add()
{
	if (mPort && !mAdded) {
		trace("WinSysexBuffer::add midiInAddBuffer\n");
		midiInAddBuffer(mPort, &mHeader, sizeof(MIDIHDR));
		mAdded = true;
	}
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
