/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 *
 * Windows implemtnation of MidiOutput.
 * 
 */

#include <stdio.h>
#include <memory.h>
#include <windows.h>
#include <mmsystem.h>

#include "Util.h"
#include "Trace.h"

#include "MidiPort.h"
#include "MidiOutput.h"
#include "WinMidiEnv.h"
#include "WinSysexBuffer.h"

//////////////////////////////////////////////////////////////////////
//
// WinMidiOutput
//
//////////////////////////////////////////////////////////////////////

PUBLIC WinMidiOutput::WinMidiOutput(WinMidiEnv* env, MidiPort* port) :
	MidiOutput(env, port)
{
    mNativePort         = 0;
    // mOutHeader is initialized when used
    mSysexTimeouts      = 0;
    mSysexPrepared		= false;
    mSendingSysex 		= false;
    mSysexBuffer 		= NULL;
    mSysexBufferLength 	= 0;
    mSysexLastLength   	= 0;
    mSysexRequestLength = 0;
    mSysexReceived 		= 0;
    mSysexDone 			= 0;
}

PUBLIC WinMidiOutput::~WinMidiOutput(void)
{
	disconnect();
}

/**
 * Overload this so we can print additional warnings.
 */
PUBLIC void WinMidiOutput::printWarnings() 
{
	MidiOutput::printWarnings();

	if (mSysexTimeouts != 0)
	  trace("%d WinMidiOutputput sysex timeouts!\n", mSysexTimeouts);
}

//////////////////////////////////////////////////////////////////////
//
// MidiOutput Overloads
//
//////////////////////////////////////////////////////////////////////

/**
 * Interrupt handler function passed when connecting.
 */
extern void WINAPI MidiOutInterrupt(HMIDIOUT dev, UINT msg, DWORD instance,
									DWORD param1, DWORD param2);

/**
 * Called after one of the Windows functions returned an error status.
 * We ask the OS for the error text.  Should save this and let
 * the application ask for it and display it in an appropriate way!
 */
PRIVATE void WinMidiOutput::setError(int rc)
{
	char msg[128];

	midiOutGetErrorText(rc, msg, sizeof(msg));

    trace("ERROR: WinMidiOutput: %s\n", msg);

	// could do code specific processing ? 

	switch (rc) {
		// midiOutShortMsg errors
		case MIDIERR_BADOPENMODE:
			break;
		case MIDIERR_NOTREADY:
			break;
		case MMSYSERR_INVALHANDLE:
			break;
	}
}

/**
 * Attempts to put the midi port in a connected state.
 * If the connection fails, an error code is returned.
 * If there is no currently designated output port, the request is ignored
 * and no error code is returned.
 *
 * In an ancient version of this, we had a critical section around the
 * connection.  I don't remember why that was, multiple threads really
 * shouldn't be connecting at the same time.  Though they could 
 * be SENDING at the same time.  Think...
 */
PUBLIC int WinMidiOutput::connect(void)
{
	int error = 0;

	if (mNativePort == 0 && mPort != NULL) {

		trace("WinMidiOutput::connect output %s\n", mPort->getName());	

		// open the port, this may fail if someone else has it
		int rc = midiOutOpen(&mNativePort,
                             mPort->getId(),
                             (DWORD)MidiOutInterrupt,
                             (DWORD)this,
                             CALLBACK_FUNCTION);

		if (rc != MMSYSERR_NOERROR) {
			trace("WinMidiOutput::connect midiOutOpen rc=%d\n", rc);
			setError(rc);
			error = 1;
			// make sure
			mNativePort = 0;
		}
	}

	return error;
}

/**
 * Disconnects from the physical midi output port.
 * 
 * Some old comments indicate that resetting & closing the output device
 * may take a long time, around 2 seconds.  This doesn't seem to happen
 * any more, it may have been related to the former practice of unpreparing
 * the output buffer from within the interrupt handler.
 */
PUBLIC void WinMidiOutput::disconnect(void)
{
	if (mNativePort) {

		trace("WinMidiOutput::disconnect output %s\n", mPort->getName());

		// Among other things, this sends note off's and centers controllers.
		midiOutReset(mNativePort);

		trace("WinMidiOutput::disconnect midiOutClose\n");
		midiOutClose(mNativePort);

		mNativePort = 0;
	}
}

PUBLIC bool WinMidiOutput::isConnected(void)
{
	return (mNativePort != 0);
}

//////////////////////////////////////////////////////////////////////
//
// Interrupt Handler
//
//////////////////////////////////////////////////////////////////////

/**
 * MidiOutInterrupt
 *
 * Arguments:
 *	     dev: output device handle
 *	     msg: message 
 *	instance: the fourth argument to midiOutOpen, a WinMidiOutput object pointer
 *	  param1: pointer to MIDIHDR if MOM_DONE
 *	  param2: not used
 *
 * Returns: none
 *
 * Description: 
 * 
 * This is the interrupt callback function registered with midiOutOpen.
 *
 * We're in an interrupt here, so we have to be careful what we call.
 * Waite book says the following are ok:
 *
 * 		PostMessage, timeGetSystemTime, timeGetTime, timeSetEvent,
 *		timeKillEvent, midiOutShortMsg, midiOutLongMsg, OutputDebugStr
 *
 * Not much to do, if we're spinning waiting for a long data output
 * completion event (MOM_DONE), stop the spin so we can continue.
 */
void WINAPI MidiOutInterrupt(HMIDIOUT dev, UINT msg, DWORD instance,
                             DWORD param1, DWORD param2)
{
	WinMidiOutput *m = (WinMidiOutput *)instance;

	switch (msg) {

		case MOM_OPEN:
			// midiOutOpen called, ignore
			break;

		case MOM_CLOSE:
			// midiOutClose called, ignore
			break;

		case MOM_DONE:
			// midiOutLongMsg completed
			m->finishedLongData();
			break;

		default:
			trace("WinMidiOutput: MOM_???\n");
			break;
	}
}

/**
 * Called from the midi_out_interrupt if we receive a MOM_DONE event
 * indicating that the output of a long message (sysex) has completed.
 *
 * There is some confusion on whether it is ok to unprepare the buffer
 * here, since it isn't on the list of approved functions.  I had lots 
 * of problems preparing buffers in the input handler, so its best not to try.
 */
PUBLIC void WinMidiOutput::finishedLongData(void)
{
	// clear this flag
	mSendingSysex = false;

	// check an odd error
	if (!(mOutHeader.dwFlags & MHDR_DONE)) {
		mWeirdErrors++;
		// we're in an interrupt handler, can only use debug functions
		trace("WinMidiOutput::finishedLongData" 
			  " Got MOM_DONE, but header flag isn't set!\n");
	}
}

//////////////////////////////////////////////////////////////////////
//
// Short Messages
//
//////////////////////////////////////////////////////////////////////

/**
 * Send an encoded one, two, or three byte message to the output device.
 */
PUBLIC void WinMidiOutput::send(int msg) 
{
	if (mNativePort) {
		// arg is actually a DWORD
		int rc = midiOutShortMsg(mNativePort, msg);
		if (rc != MMSYSERR_NOERROR)
		  setError(rc);
	}
}

//////////////////////////////////////////////////////////////////////
//
// Sysex
//
//////////////////////////////////////////////////////////////////////

/**
 * Sends a buffer containing a sysex message.
 * 
 * The multimedia device interface is designed to let you queue the sysex
 * buffer and have it sent in the background while control returns to the
 * application.  We also provide an option here to synchronously send
 * the buffer by spinning until it has finished.
 * 
 * It is usually best if the application is designed to let
 * the sysex transfer happen in the background, polling the WinMidiOutput
 * object for status and displaying some kind of visual status indication.
 *
 * We only support one outgoing block at a time, so multithreaded
 * apps need to be careful not to send too much.
 */
PUBLIC int WinMidiOutput::sendSysex(const unsigned char *buffer, 
									int length)
{
	return sendSysex(buffer, length, true);
}

PUBLIC int WinMidiOutput::sendSysexNoWait(const unsigned char *buffer, 
										  int length)
{
	return sendSysex(buffer, length, false);
}

PRIVATE int WinMidiOutput::sendSysex(const unsigned char *buffer, 
									 int length,
									 bool waitFinished)
{ 
	int error = 0;

	trace("WinMidiOutput::sendSysex\n");

    // don't allow overlapping sends, could have an option to block
    // here but the app really should know better
	if (mSendingSysex) {
		trace("WinMidiOutput::sendSysex Sysex already in progress");
		return -1;
	}

	if (mNativePort && buffer != NULL && length > 0) {

		// Prepare the buffer, we use a "header" maintained in our
		// object, which means we can't send a sysex until the previous
		// one has completed.  It would be more flexible to allocate
        // these on the fly and keep a list of pending transfers.

		mOutHeader.lpData			= (char *)buffer;
		mOutHeader.dwBufferLength	= length;
		mOutHeader.dwBytesRecorded	= 0;
		mOutHeader.dwFlags			= 0;
		mOutHeader.dwOffset			= 0;

		trace("WinMidiOutput::sendSysex midiOutPrepareHeader\n");
		int rc = midiOutPrepareHeader(mNativePort, &mOutHeader, sizeof(MIDIHDR));
		if (rc != MMSYSERR_NOERROR) {
			setError(rc);
			error = 1;
		}
		else {
			mSysexPrepared = true;

			// Since we may not have a reliable way to determine the
			// actual number of bytes sent, remember the amount queued
			// so we at least have something accurate to return from
			// getSysexBytesSent when we're done.
			mSysexLastLength = length;

			// send the buffer
			// unfortunately, the device driver gets to determine
			// whether this is a synchronous or asynchronous call,
			// so we may suspend here
			mSendingSysex = true;

			trace("WinMidiOutput::sendSysex midioutLongMsg %d\n", length);
			int rc = midiOutLongMsg(mNativePort, &mOutHeader, sizeof(MIDIHDR));
			trace("WinMidiOutput::sendSysex midioutLongMsg done\n");
			if (rc != MMSYSERR_NOERROR) {
				setError(rc);
				error = 1;
			}
			else {
				// If the device driver handled midiOutLongMsg synchronously,
				// then we won't do any further waiting here.

				int status = midiOutUnprepareHeader(mNativePort, &mOutHeader,
													sizeof(MIDIHDR));

				if (status != MIDIERR_STILLPLAYING) {
					mSysexPrepared = false;
					mSendingSysex = false;
				}
				else if (waitFinished) {
                    // we can also poll on the MHDR_DONE bit of 
                    // the dwFlags field  of the MIDIHDR structure, this bit
                    // will be set when the  driver is finished.

					trace("WinMidiOutput::sendSysex Waiting for sysex...\n");
					// don't wait more than 2 seconds
					int maxWait = 2000;
					int cycleWait = 100;
					int totalWait = 0;
					bool timeout = false;

					while (MIDIERR_STILLPLAYING ==
						   midiOutUnprepareHeader(mNativePort, &mOutHeader,
												  sizeof(MIDIHDR))) {

						// delay a bit, time in milliseconds
						Sleep(cycleWait);
						totalWait += cycleWait;
						if (totalWait >= maxWait) {
							error = -2;
							trace("WinMidiOutput::sendSysex"
								  " Timeout waiting for Sysex output.\n");
							timeout = true;
							break;
						}
					}

					if (!timeout) {
						mSysexPrepared = false;
						// this should already be off, but be sure
						mSendingSysex = false;
						trace("WinMidiOutput::sendSysex Waited %d milliseconds"
							  " for sysex to be sent.\n",
							  totalWait);
					}
					else {
						trace("WinMidiOutput::sendSysex Timeout after %d"
							  " milliseconds waiting for sysex to be sent.\n",
							  totalWait);
						endSysex();
					}
				}
			}
		}
	}

	return error;
}

/**
 * Return true if the last sysex block sent with sendSysex has 
 * been fully sent.
 */
PUBLIC bool WinMidiOutput::isSysexFinished()
{
	bool finished = false;

	// if we haven't prepared a buffer, then we must be done
	if (!mSysexPrepared)
		finished = true;
	else {

		// At least three ways to do this, if MOM_DONE is being received
		// properly, then we can just track the mSendingSysex field,
        // otherwise we can poll the MHDR_DONE bit.  We can
        // also try calling midiOutUnprepareHeader and testing
        // for MIDIERR_STILLPLAYING

		finished = !mSendingSysex;

		// this is the truth, but make sure we're in sync
		bool altFinished = mOutHeader.dwFlags & MHDR_DONE;
		if (finished != altFinished)
			trace("WinMidiOutput::isSysexFinished inconsistent completion state");
	}

	return finished;
}


/**
 * Return the number of sysex bytes that have been sent by the
 * last call to sendSysex.  If the last call was synchronous,
 * we'll have saved the length, otherwise look in the header.
 */
PUBLIC int WinMidiOutput::getSysexBytesSent() 
{
	int bytes = 0;

	trace("WinMidiOutput::getSysexBytesSent %d %d\n", 
		  (int)mOutHeader.dwBytesRecorded,
		  (int)mOutHeader.dwOffset);

	// Since we may not have reliable byte counts from the driver,
	// always return the total size of the last block if we're finished.
	if (isSysexFinished())
	  bytes = mSysexLastLength;
	else {
		// not sure if this will actually work
		bytes = mOutHeader.dwBytesRecorded;
	}

	return bytes;
}

/**
 * Unprepares a sysex buffer that had been previously prepared.
 * I used to do this automatically within finishedLongData, but that's 
 * an interrupt handler, and isn't supposed to call these functions.
 * This seemed to cause problems, but I'm no longer sure.
 * Now, the application will have to call this after it has scheduled 
 * a wakeup call, or is otherwise out of the interrupt callback.
 */
PUBLIC void WinMidiOutput::endSysex()
{
	// if we're still sending, then we need to cancel it,
	// probably by resetting the device?
	if (mSendingSysex)
	  trace("WinMidiOutput::endSysex Still sending!\n");

	if (mSysexPrepared) {

		trace("WinMidiOutput::endSysex midiOutUnprepareHeader\n");

		int rc = midiOutUnprepareHeader(mNativePort, &mOutHeader, sizeof(MIDIHDR));
		if (rc != MMSYSERR_NOERROR)
		  setError(rc);

		mSysexPrepared = false;
    }
}

/****************************************************************************
 *                                                                          *
 *   						  SYSEX SEND/RECEIVE                            *
 *                                                                          *
 ****************************************************************************/
/*
 * This is a combination sysex send/receive function that
 * will handle the ugly details of the sysex callback events.
 * This is really just a hack to make short message transfer from
 * simple tests applications easier.  For large sysex transfers you 
 * should be using the async interface and providing some sort
 * of realtime status.
 */

/**
 * MidiInListener interface.
 * We override the previous MidiIn listener.
 */
PUBLIC void WinMidiOutput::midiInputEvent(MidiInput *in)
{
	// ignore non-sysex events in case we got any
	in->ignoreEvents();

	sysexCallback((WinMidiInput*)in);
}

/**
 * WinMidiOutput::sysexCallback
 *
 * Arguments:
 *	in: input device
 *
 * Returns: none
 *
 * Description: 
 * 
 * Called via sysex_request_callback when we get a sysex event.
 * Check the size of the received buffer, and ignore it if we just see
 * the echo'd request.  This will commonly happen devices that
 * echo MIDI input to MIDI output (like the Nanosynth)
 * but shouldn't for most devices.  I don't like this, should be storing
 * the actual request bytes for comparison here...
 */
PRIVATE void WinMidiOutput::sysexCallback(WinMidiInput *in)
{
	trace("WinMidiOutput::sysexCallback\n");

	if (mSysexBuffer == NULL) {
		// just ignore whatever it is and terminate
		in->ignoreSysex();
		mSysexDone = 1;
		mSysexReceived = 0;
	}
	else {
		WinSysexBuffer *sysex = in->getSysex();
		if (sysex != NULL) {

			int count = sysex->getLength();
			if (count <= 0) {
				trace("WinMidiOutput::sysexCallback Error on sysex %d\n", count);
				mSysexDone = 1;
			}
			else if (count <= mSysexRequestLength) {
				// this is probably an echo'd dump request, 
				// ignore and wait for the  next one
				trace("WinMidiOutput::sysexCallback Error on sysex response,"
					  " Looks like a request echo\n");
			}
			else {
				// it looks like the real thing
				memmove(mSysexBuffer, sysex->getBuffer(), count);
				mSysexReceived = count;
				mSysexDone = 1;
			}
			in->freeSysex(sysex);
		}
	}
}

/**
 * WinMidiOutput::sysexRequest
 *
 * Arguments:
 *	buffer: request buffer
 *	length: length of request buffer
 *	    in: input device
 *	 reply: output buffer
 *	replen: length of output buffer
 *
 * Returns: length of reply or -1 on error
 *
 * Description: 
 * 
 * Performs a synchronous sysex request.
 * This is kludged for console apps by spinning in a hard loop, which
 * isn't very nice, but sleeping for 1/10 second seems to take too long
 * and we lose events.  Might need to combine this with a timer device
 * so we can have a timeout?
 * 
 * If the supplied buffer is NULL, then we wait for the next sysex input
 * of any kind and remove it.  This is handy for the fucking nanosynth
 * that echo's everything it receives.
 */
PUBLIC int WinMidiOutput::sysexRequest(const unsigned char *request,
									   int reqlen,
									   WinMidiInput *in,
									   unsigned char *reply, 
									   int replen)
{
	MidiInputListener* saveListener;
	bool saveIgnore;
	//bool saveEnable;
	int error, size;

	trace("WinMidiOutput::sysexRequest\n");

	size = -1;

	// replace the callback in the input device temporarily with ours
	saveListener = in->getListener();
	saveIgnore = in->isIgnoreSysex();
    // assume always enbaled now
	//saveEnable = in->isEnabled();

	in->setListener(this);
	
	// initialize some state
	mSysexBuffer 		= reply;
	mSysexBufferLength  = replen;
	mSysexRequestLength = reqlen;
	mSysexReceived 		= 0;
	mSysexDone 			= 0;

	// send the request
	in->setIgnoreSysex(false);
	//in->enable();

	error = sendSysex(request, reqlen, true);
	if (!error) {

		// wait, at most 30 seconds
		int maxWait = 1000 * 15;
		int cycleWait = 100;
		int totalWait = 0;

		while (!mSysexDone) {
			
			Sleep(cycleWait);
			totalWait += cycleWait;
			if (totalWait >= maxWait) {
				trace("WinMidiOutput::sysexRequest"
					  " Timeout waiting for Sysex request to finish.\n");
				mSysexTimeouts++;
				in->cancelSysex();
				break;
			}
		}

		// return the size
		size = mSysexReceived;

		trace("WinMidiOutput::sysexRequest"
			  " Waited %d milliseconds for sysex to finish.\n", totalWait);
		trace("WinMidiOutput::sysexRequest Received %d bytes\n", size);
	}

	// restore the original settings
	//if (!saveEnable)
    //in->disable();
	in->setListener(saveListener);
	in->setIgnoreSysex(saveIgnore);

	return size;
}

/****************************************************************************
 *                                                                          *
 *                                 UNIT TEST                                *
 *                                                                          *
 ****************************************************************************/

void WINAPI TestMidiOutInterrupt(HMIDIOUT dev, UINT msg, DWORD instance,
                                 DWORD param1, DWORD param2)
{
	WinMidiOutput *m = (WinMidiOutput *)instance;

	switch (msg) {

		case MOM_OPEN:
			// midiOutOpen called, ignore
			break;

		case MOM_CLOSE:
			// midiOutClose called, ignore
			break;

		case MOM_DONE:
			// midiOutLongMsg completed
            trace("TestMidiOutInterrupt MOM_DONE\n");
			break;

		default:
			trace("TestMidiOutInterrupt: MOM_???\n");
			break;
	}
}

/**
 * Test what happens when you open and close the same device twice.
 * This doesn't use the MidiEnv interface, it goes directly against the APIs
 *
 */
void WinMidiOutput::testOpen()
{
    // at the time of this writing these were the ReMOTE ports
    int portnum = 3;

    printf("Opening output port %d\n", portnum);

    printf("Opening first time...\n");
	HMIDIOUT nativePort = 0;
    int rc = midiOutOpen(&nativePort, portnum,
                         (DWORD)TestMidiOutInterrupt,
                         (DWORD)NULL,
                         CALLBACK_FUNCTION);

    if (rc != MMSYSERR_NOERROR)
      printf("WinMidiOutput::testOpen midiOutOpen 1 rc=%d\n", rc);

    printf("First nativePort %d\n", (int)nativePort);

    // and again
    printf("Opening second time...\n");
	HMIDIOUT nativePort2 = 0;
    rc = midiOutOpen(&nativePort2, portnum,
                     (DWORD)TestMidiOutInterrupt,
                     (DWORD)NULL,
                     CALLBACK_FUNCTION);

    if (rc != MMSYSERR_NOERROR)
      printf("WinMidiOutput::testOpen midiOutOpen 2 rc=%d\n", rc);

    printf("Second nativePort 2 %d\n", (int)nativePort2);
    
    printf("Closing first one...\n");

    // and close them
    // Among other things, this sends note off's and centers controllers.
    MMRESULT res = midiOutReset(nativePort);
    if (res != MMSYSERR_NOERROR)
      printf("WinMidiOutput::testOpen midiOutReset 1 res=%d\n", res);

    res = midiOutClose(nativePort);
    if (res != MMSYSERR_NOERROR)
      printf("WinMidiOutput::testOpen midiOutClose 1 res=%d\n", res);


    if (nativePort2 != 0) {
        printf("Closing second one...\n");

        res = midiOutReset(nativePort2);
        if (res != MMSYSERR_NOERROR)
          printf("WinMidiOutput::testOpen midiOutReset 2 res=%d\n", res);

        res = midiOutClose(nativePort2);
        if (res != MMSYSERR_NOERROR)
          printf("WinMidiOutput::testOpen midiOutClose 2 res=%d\n", res);


    }

}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
