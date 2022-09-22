/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 *
 * Subclass of MidiInput for Windows.
 * 
 * Interrupt handling for the MIDI input device was a lot more
 * complicated to do reliably than the MIDI output device.
 * For all practical purposes the application callback has to be
 * called outside of the interrupt handler.  All the interrupt
 * handler does is capture the received events in an input queue
 * and then notifies a "monitor" thread with a SetEvent call.
 * The monitor thread is launched automatically by each MidiIn 
 * object, it waits to be notified by the interrupt handler, 
 * and the calls the application callback.  The application callback
 * is expected to process the events in the input queue and remove them.
 * Because both the monitor thread and the interupt handler can be
 * accessing the event queue at the same time, we have to maintain
 * critical sections.
 *
 * Sysex handling is a lot more complicated because we have to
 * be prepared for anything.  It could be simplified a lot if
 * we could assume a maximum size, but its best not to.
 *
 * Because of the latency between the time we receive an event
 * and the time the application callback ends up running, we normally
 * have a Timer object that we can use to immediately timestamp
 * the event with the current clock.
 *
 * If we want the event echoed, we can be given a MidiOut device
 * that we will immediately send to, to avoid echo latency.
 *
 */

#include <stdio.h>
#include <memory.h>
#include <windows.h>
#include <mmsystem.h>
#include <process.h>		// for _beginthread

#include "Util.h"
#include "Vbuf.h"
#include "Trace.h"
#include "Thread.h"

#include "MidiPort.h"
#include "MidiInput.h"
#include "WinMidiEnv.h"
#include "WinSysexBuffer.h"

//////////////////////////////////////////////////////////////////////
//
// WinMidiThread
//
//////////////////////////////////////////////////////////////////////

/**
 * Holds state for the MIDI input monitor thread.
 */
class WinMidiThread {

  public:

	WinMidiThread(WinMidiInput *in);
	~WinMidiThread();

	// Called to signal the thread that something happened
	void signal();

	// called to shut stop the thread
	bool stop();

	// public only so startThread can call it
	void run();

	// flag to keep the thread from processing events
	void setAllowProcessing(bool b);

  private:
	
	void handleEvent();

	// the input device we monitor
	WinMidiInput *mIn;

	// the event we wait on
	HANDLE mEvent;

	// handle to the thread
	long mThread;
	
	// flag to make us stop
	bool mStop;

	// flag indicating that the thread is running
	bool mRunning;
	
	// flag indicating that we should prevent processing 
	bool mAllowProcessing;
};

/**
 * The C function that is the entry point for the thread.
 */
void startThread(void *arg)
{
	WinMidiThread *thread = (WinMidiThread *)arg;
	thread->run();
}

/**
 * Launch a new thread to monitor activity on a particular MidiIn device.
 */
PUBLIC WinMidiThread::WinMidiThread(WinMidiInput *in) 
{
	mStop = false;
	mIn = in;
	mAllowProcessing = true;

	// no security attributes, manual, initialy off, no name
	mEvent = CreateEvent(NULL, true, false, NULL);
	if (mEvent == NULL) {
		DWORD e = GetLastError();
		trace("WinMidiThread: CreateEvent error %d\n", (int)e);
	}

	// launch the thread, no initial stack size
	mThread = _beginthread(startThread, 0, (void *)this);
}

PUBLIC WinMidiThread::~WinMidiThread()
{
}

/**
 * Twiddle the allow processing flag.  This is used to temporarily
 * suspend input processing during a few special sysex operations.
 */
PUBLIC void WinMidiThread::setAllowProcessing(bool b) 
{
	mAllowProcessing = b;
}

PRIVATE void WinMidiThread::run()
{
	mRunning = true;
    trace("WinMidiThread for %s running...\n",  
          mIn->getPort()->getName());

	while (!mStop) {

		DWORD rc = WaitForSingleObject(mEvent, 1000);
		ResetEvent(mEvent);

		if (rc == WAIT_TIMEOUT) {
			// timeout expired, loop again
		}
		else if (rc == WAIT_ABANDONED) {
			// should only see this with mutexes
			trace("WinMidiThread: WAIT_ABANDONED\n");
		}
		else if (rc != WAIT_OBJECT_0) {
			// see -1 when shutting down
			trace("WinMidiThread: Unknown wait code %d\n", (int)rc);
			mStop = true;
		}
		else {
			// the event was signaled by the interrupt handler
			// turn right around and call processEventsReceived if we're allowed
			if (!mStop && mAllowProcessing && mIn != NULL)
			  mIn->processEventsReceived();
		}
	}

	trace("WinMidiThread for %s stopped.\n", mIn->getPort()->getName());
	mRunning = false;
}

/**
 * Called when the associated MidiIn device is deleted.
 * Set the stop flag and send an event to the thread which 
 * should terminte in a short time.
 */
PUBLIC bool WinMidiThread::stop() 
{
	bool stopped = false;

	trace("WinMidiThread stop request.\n");

	mStop = true;

	// hmm, what if we're currently processing?
	SetEvent(mEvent);

	// poll at 1/10 second intervals till we're done
	for (int i = 0 ; i < 10 ; i++) {
		if (mRunning)
		  Sleep(100);
	}

	stopped = !mRunning;
	if (!stopped)
	  trace("ERROR: Couldn't stop WinMidiThread\n");

	return stopped;
}

/**
 * Wake the thread up.
 */
PUBLIC void WinMidiThread::signal()
{
	// hmm, what if we're currently processing?
	SetEvent(mEvent);
}

//////////////////////////////////////////////////////////////////////
//
// WinMidiInput
//
//////////////////////////////////////////////////////////////////////

/**
 * Creates and initializes a new midi input device object.
 * The object is initially in a disconnected, use setPort()
 * and connect() to connect to an output device.
 * 
 * This is normally only called by MidiEnv::getInput so it can
 * maintain a global list of allocated input devices.
 */
PUBLIC WinMidiInput::WinMidiInput(WinMidiEnv* env, MidiPort* port) :
	MidiInput(env, port)
{

	mMonitorThread		= NULL;
	mNativePort			= 0;

	mInCallback         = false;
	mEchoSysex			= false;
	mSysexBuffers		= NULL;
	mSysexReceived		= NULL;
	mLastSysexReceived	= NULL;
	mSysexProcessed		= NULL;
	mLastSysexProcessed	= NULL;
	mSysexActive		= NULL;
	mLastSysexActive	= NULL;

	mSysexEchoSize		= 0;
	mIgnoreSysex		= true;

	// initialize a few sysex buffers
	// need three, one for the echo'd request, one for the response,
	// and one to have activated
	allocSysexBuffer();
	allocSysexBuffer();
	allocSysexBuffer();

	// allocate a monitor thread
	// should we have one of these for each input object,
	// or just one shared by everyone?
    // UPDATE: defer this until the device is actually open   
    // so we don't bother with a thread if there is an 
    // error opening the device
    mMonitorThread = new WinMidiThread(this);
}

/**
 * Destructor for the MIDI input object.
 * Detaches from the port if we're still attached.
 */
PUBLIC WinMidiInput::~WinMidiInput(void)
{
	disconnect();
    stopMonitorThread();
	
	WinSysexBuffer *next = NULL;
	for (WinSysexBuffer *b = mSysexBuffers ; b != NULL ; b = next) {
		// remember to use the link field for the master list
		next = b->getLink();
		delete b;
	}

}

void WinMidiInput::stopMonitorThread(void)
{
	if (mMonitorThread != NULL) {
		bool stopped = mMonitorThread->stop();
		if (stopped) {
            delete mMonitorThread;
            mMonitorThread = NULL;
        }
		else {
			trace("ERROR: WinMidiInput: Unable to stop monitor thread!\n");
		}
	}
}

//////////////////////////////////////////////////////////////////////
//
// MidiInput Overloads
//
//////////////////////////////////////////////////////////////////////

/**
 * Forward reference to our interrupt handler callback.
 */
extern void WINAPI midi_in_callback(HMIDIIN dev, UINT msg, DWORD instance,
									DWORD param1, DWORD param2);

/**
 * Attempts to open the Windows port for a MidiPort.
 * Returns a non-zero error code on failure.
 * If there is no currently designated input port, the request is ignored
 * and no error code is returned.
 * 
 * NOTE WELL:
 *
 * During initialization of Java, multiple threads can try to 
 * connect at the same time.  This should be handled at a higher level, 
 * but try to catch it here using our critical section.
 */
PUBLIC int WinMidiInput::connect(void)
{
	int error = 0;

	enterCriticalSection();

	if (!mNativePort && mPort != NULL) {

		trace("WinMidiInput::connect %s\n", mPort->getName());

		// Open the input device.
		// Fourth arg can be some data passed in as the "instance" argument
		// to the callback function.
		// This may fail if someone else has opened nit.

		int rc = midiInOpen(&mNativePort,
							mPort->getId(),
							(DWORD)midi_in_callback,
							(DWORD)this,
							CALLBACK_FUNCTION | MIDI_IO_STATUS);

		if (rc != MMSYSERR_NOERROR) {
			trace("ERROR: WinMidiInput::connect: Error %d opening input port %s\n", 
                  rc, mPort->getName());
			setError(rc);
			disconnect();
			error = 1;
		}
        else {
            trace("WinMidiInput::connect opened %s on port %d\n",
                  mPort->getName(), mNativePort);
            
            // Launch the thread as soon as we connect sucessfully
            if (mMonitorThread == NULL)
              mMonitorThread = new WinMidiThread(this);

            if (mSysexBuffers == NULL) {
                // shouldn't be here, we pre-allocate them
                // in the constructor
                trace("ERROR: WinMidiInput::connect No sysex buffers at connect!\n");
            }
            else {
                // A buffer must first be "prepared" then "added"
                for (WinSysexBuffer *b = mSysexBuffers ; 
                     b != NULL && !error ; b = b->getLink())
                  error = b->prepare(mNativePort);

                if (error) {
                    disconnect();
                }
                else if (mSysexActive == NULL) {
                    // shouldn't get here
                    trace("ERROR: WinMidiInput::connect:"
                          " No active sysex buffers after connect!\n");
                }
                else {
                    // now "add" the buffers to the device
                    for (WinSysexBuffer *b = mSysexActive ; b != NULL ; 
                         b = b->getNext()) {
                        b->add();
                    }
                }

                trace("WinMidiInput::connect finished\n");
            }
        }
	}

	leaveCriticalSection();

	// originally enable was a seperate operation but this isn't
	// exposed now
	enable();

	return error;
}

/**
 * Closes the input port, though the object remains allocated and
 * can be reconnected later.
 * 
 * Note that one of the calls we make here generates spurious interrupts
 * with empty data.  This can be a problem if the interrupt code
 * isn't prepared to deal with it.  Tell the monitor thread
 * to ignore anything that comes in while we do this.
 */
PUBLIC void WinMidiInput::disconnect()
{
	// this disables interrupts
	disable();

    // What about listener, timer, echoDevice, and echoMap?
    // I guess we could leave those behind for the next connect, but
    // it feels better to reset them.  
    setListener(NULL);
    setTimer(NULL);
    setEchoDevice(NULL);
    setEchoMap((MidiMap*)NULL);

	if (mNativePort) {

        // wait a little to make sure we're out of the last interrupt
        // not sure this is necessary but paranoia is a good thing here
        SleepMillis(1);

		trace("WinMidiInput::disconnect %s\n", mPort->getName());

		// Don't need to call both midiInStop and MidiInReset, 
		// reset is better because it removes the added buffers
		// Suspend the monitor thread while we reset to prevent
		// the buffers from being readded!

        mMonitorThread->setAllowProcessing(false);
		trace("WinMidiInput::disconnect midiInReset\n");
		midiInReset(mNativePort);
		Sleep(100);			// 1/10 to let the dust settle

		if (mSysexActive != NULL) {
			trace("WARN: WinMidiInput::disconnect Active sysex after reset!\n");

			// buffers are no longer "added"
			WinSysexBuffer *b = NULL;
			enterCriticalSection();
			for (b = mSysexActive ; b != NULL ; b = b->getNext())
			  b->setAdded(false);
			leaveCriticalSection();
		}

		// Unprepare the sysex buffers, docs indicate that we have
		// to wait for the device to finish, won't midiInReset do that?
		// Hmm, we may have outstanding receive buffers that haven't been
		// returned yet, so have to be able to re-prepare them during connect.

		for (WinSysexBuffer *b = mSysexBuffers ; b != NULL ; b = b->getLink())
		  b->unprepare();

		// this hangs for a few seconds with the MIDI Yoke driver with
		// some prepared sysex buffers.  Not sure what we can do about it.
		trace("WinMidiInput::disconnect midiInClose\n");
		midiInClose(mNativePort);
		mNativePort = 0;

        // the thread lives only while the port is open
        stopMonitorThread();
		
		// hmm, we'll now have zero length buffers on the received list
		// that we want back on the active list when we connect next time
		// Since there are no non-zero ports, it should be ok to 
		// process inputs.  These will be added back, but
		// since the port is zero, they would not be prepared.
		processEventsReceived();
	}
}

/**
 * Called after one of the SDK functions returns an error code.
 * TODO: This doesn't actually get saved anywhere, we just print it.
 */
PRIVATE void WinMidiInput::setError(int rc)
{
	char msg[128];

	midiInGetErrorText(rc, msg, sizeof(msg));

	trace("ERROR: WinMidiInput: %s!\n", msg);
}

PUBLIC bool WinMidiInput::isConnected()
{
	return (mNativePort != 0);
}

/**
 * Enables input interrupts.
 * Just connecting isn't enough to get the interrupt flowing, you
 * also have to enable.
 * 
 * Should we just enable by default during connection?
 */
PRIVATE void WinMidiInput::enable(void)
{
	if (mNativePort == 0) {
		trace("WARN: WinMidiInput::enable device is not open!\n");
	}
	else if (!mEnabled) {
		trace("WinMidiInput::enable midiInStart for %s\n", mPort->getName());
		midiInStart(mNativePort);
		mEnabled = true;
	}
}

/**
 * Disables input interrupts.
 */
PRIVATE void WinMidiInput::disable(void)
{
	if (mNativePort && mEnabled) {
		trace("WinMidiInput::disable midiInStop for %s\n", mPort->getName());
		midiInStop(mNativePort);
		mEnabled = false;
		mTempo->reset();
	}
}

/**
 * Called by the main event processor MidiInput::processData
 * when it has added someting to the event list and needs
 * to notify the application.  On Windows we maintain a 
 * monitor thread which will signaled and eventually call
 * MidiInput::processReceivedEvents.
 */
PUBLIC void WinMidiInput::notifyEventsReceived()
{
	if (mMonitorThread != NULL)
	  mMonitorThread->signal();
}

//////////////////////////////////////////////////////////////////////
//
// Interrupt Handler
//
//////////////////////////////////////////////////////////////////////

/**
 * MidiInProc
 *
 * Arguments:
 *	     dev: input device handle
 *	     msg: message to respond to
 *	instance: the fourth argument to midiInOpen
 *	  param1: MIDI event as a 4 byte integer (for MIM_DATA)
 *	  param2: millisecond count	(for MIM_DATA)
 *
 * Returns: none
 * 
 * This is the registered MidiInProc interrupt callback function.
 * You are only allowed to call the following functions:
 * 
 * 		EnterCriticalSection, LeaveCriticalsection
 * 		PostMessage, PostThreadMessage,	SetEvent, 
 * 	    timeGetSystemTime, timeGetTime, timeSetEvent, timeKillEvent, 
 * 		midiOutShortMsg, midiOutLongMsg, OutputDebugString
 * 
 * We dispatch on the message and call one of the MidiInput handler methods.
 * 
 * Note that we can get here via the WinMidiInput::disconnect method, since
 * it seems that one of midiInStop, MidiInReset, and midiInClose will
 * generate bogus events with empty data.  We could detect this here and
 * igore the request, but the sub-handlers should be able to deal with it.
 *
 */
void WINAPI midi_in_callback(HMIDIIN dev, UINT msg, DWORD instance,
							 DWORD param1, DWORD param2)
{
	WinMidiInput *in = (WinMidiInput *)instance;

	switch (msg) {

		case MIM_OPEN:
			// midiInOpen() called, ignore
			trace("MidiIn: MIM_OPEN\n");
			break;

		case MIM_CLOSE:
			// midiInClose called, ignore
			trace("MidiIn: MIM_CLOSE\n");
			break;

		case MIM_DATA:
			// short message received
			in->processShortMessage((int)param1);
			break;

		case MIM_ERROR:
			// faulty short message received
			in->incShortErrors();
			trace("MidiIn: MIM_ERROR\n");
			break;

		case MIM_LONGDATA:
			// long message received
			trace("MidiIn: MIM_LONGDATA\n");
			in->processLongData(param1, param2, false);
			break;

		case MIM_LONGERROR:
			// faulty long message received
			trace("MidiIn: MIM_LONGERROR\n");
			in->incLongErrors();
			in->processLongData(param1, param2, true);
			break;
			
		case MIM_MOREDATA:
			// we're not processing fast enough, we could accumulate
			// the data and try to append it to the buffer, but for now
			// just make note of the fact that it happened and try 
			// to become faster.
			// You have to specify the MIDI_IO_STATUS flag in midiInOpen
			// to get these events.
			trace("MidiIn: MIM_MOREDATA\n");
			in->incLongOverflows();
			break;

		default:
			trace("MidiIn: MIM_???\n");
 			break;
	}

}

//////////////////////////////////////////////////////////////////////
//
// Sysex Interrupt
//
//////////////////////////////////////////////////////////////////////

/**
 * Called by midi_in_interrupt when a MM_MIM_LONGDATA message is received.
 * Here, we've just finished capturing a sysex stream into one
 * of the prepared input buffers.  Move this to a list for further
 * processing.
 *
 *	p1: pointer to the prepared MIDIHDR with data
 *	p2: milliseond count
 * 
 * There are mixed signals about whether we can re-arm the input buffer
 * within the interrupt handler.  midiInAddBuffer isn't on the approved
 * list of functions to be called in here, but some code examples
 * on the web call it.  When I tried to re-arm in here, it almost always 
 * ended up hanging when closing the input device.  Now we signal the monitor
 * thread and let it add the buffer, its safer.
 * 
 * NOTE: We can get here with hdr->dwBytesRecorded == 0, when midiInReset
 * is called prior to shutting down the input device.  If you call
 * midiInAddBuffer after that, it appears to hang the system, apparently
 * this isn't expected.
 *
 * NOTE: If we're echoing to an output device, we really 
 * should echo the long data immediately, to preserve the ordering
 * of sysex buffers with other events.  Since we're in an
 * interrupt handler, that means we can't prepare an output buffer
 * here, so we would have to have had one already prepared, and the
 * prepared buffer would need to be at least the same size.  
 * Not sure how transfers greater than the registered block size are
 * handled.
 *
 * For now, we'll just add it to the received list and make the processing
 * thread wake up and do the echo outside of the interrupt.  This
 * should be ok for most things, but it can't ensure accurate interleaving.
 * 
 */
PRIVATE void WinMidiInput::processLongData(DWORD p1, DWORD p2, bool error)
{
	MIDIHDR 	*hdr;
	DWORD		len;

	if (mSysexActive == NULL) {
		// no prepared buffer, shouldn't be here
		trace("WinMidiInput::processLongData with no buffer!\n");
		mWeirdErrors++;
		return;
	}

	// allow reentancies?
	if (mInInterruptHandler)
	  trace("WinMidiInput::processLongData reentered!\n");
	mInInterruptHandler++;

	// Locate the active WinSysexBuffer object that owns the data buffer
	// and remove it from the list.
	// Usually this is the first one on the list, but some devices don't
	// return them in FIFO order.  MIDI Yoke appears to use LIFO.

	enterCriticalSection();
	WinSysexBuffer *prev = NULL;
	WinSysexBuffer *buffer = mSysexActive;
	hdr = buffer->getHeader();
	if (hdr != (MIDIHDR *)p1) {
		prev = buffer;
		for (buffer = buffer->getNext() ; buffer != NULL ; 
			 buffer = buffer->getNext()) {
			hdr = buffer->getHeader();
			if (hdr == (MIDIHDR*)p1)
			  break;
			else
			  prev = buffer;
		}
	}
	if (buffer != NULL) {
		WinSysexBuffer *next = buffer->getNext();
		if (prev == NULL) {
			mSysexActive = next;
			if (mSysexActive == NULL)
			  mLastSysexActive = NULL;
		}
		else {
			prev->setNext(next);
			if (next == NULL)
			  mLastSysexActive = prev;
		}
	}
	leaveCriticalSection();
		
	if (buffer == NULL) {
		// p1 didn't match any registered input buffers, shouldn't happen
		trace("WinMidiInput::processLongData unexpected MIDIHDR!\n");
		mWeirdErrors++;
	}
	else {
		if (!(hdr->dwFlags & MHDR_DONE)) {
			// We got a message, but the device says it isn't done receiving
			// the message.  This might happen if the buffer isn't big enough
			// for the entire message ? 
			trace("WinMidiInput::processLongData MIDIHDR not done!\n");
			mWeirdErrors++;
			error = true;
		}

		// Note that buffer length can be 0 after MidiInReset is called.
		// Go ahead and transfer to the ready list and let
		// processInput deal with it.

		len = hdr->dwBytesRecorded;
		trace("WinMidiInput::processLongData: recieved %d bytes.\n", len);

		buffer->setNext(NULL);
		buffer->setError(error);
		buffer->setAdded(false);
		buffer->setFinished();

		enterCriticalSection();
		if (mLastSysexReceived == NULL)
		  mSysexReceived = buffer;
		else
		  mLastSysexReceived->setNext(buffer);
		mLastSysexReceived = buffer;
		leaveCriticalSection();
		
		// always notify the monitor thread so it can arm another buffer
		// need to think about what happens in the error cases above,
		// if we hit any of them, another buffer won't be added!!

		if (mMonitorThread != NULL)
		  mMonitorThread->signal();
	}

	mInInterruptHandler--;
}

//////////////////////////////////////////////////////////////////////
//
// Input Event Processing
//
//////////////////////////////////////////////////////////////////////

/**
 * Called by the monitor thread after the interrupt handler signals it
 * that an event is ready.  We are now outside the interrupt handler
 * and can safely call the application callback.
 *
 * If mEchoSysex is on, we'll also echo the sysex buffers
 * to the output device.  See commentary in processLongData about
 * why ideally this shouldn't be done here.
 *
 * We have to be careful with the mSysexReceived list here, since
 * there can be interrupts happening that add things to this list
 * while we're processing.  Since the thread may think we've already
 * been signaled, there could be a delay in the delivery of sysex
 * buffers if we don't loop here until all received sysex buffers are
 * processed?
 * 
 */
PUBLIC void WinMidiInput::processEventsReceived()
{
	if (mInCallback) {
		// hmm, we haven't gotten out of the last call yet
		// after the introduction of the monitor thread,
		// this really shouldn't happen
		trace("WinMidiInput::processEventsReceived reentered!\n");
		mEventOverflows++;
	}
	else {
		mInCallback = true;

		// process sysex buffers, move them from the mSysexReceived
		// list to the 
		if (mSysexReceived != NULL) {
			// loop until there are no more received buffers to process
			int loop = 1;
			while (mSysexReceived != NULL) {

				trace("WinMidiInput::processEventsReceived loop %d\n", loop);

				WinSysexBuffer *buffers = NULL;

				// capture the buffers to process
				enterCriticalSection();
				buffers = mSysexReceived;
				mSysexReceived = NULL;
				mLastSysexReceived = NULL;
				leaveCriticalSection();

				if (buffers != NULL)
				  processSysexBuffers(buffers);

				loop++;
			}
		}

		// notify the callback
		if (mSysexProcessed == NULL && mEvents == NULL)
		  trace("WinMidiInput::processEventsReceived false alarm\n");

		else if (mListener != NULL) {
			mListener->midiInputEvent(this);
		}
		else {
			// ignore everything
			ignoreSysex();
			ignoreEvents();
		}

		mInCallback = false;
	}
}

/**
 * Called by processEventsReceived to process a list of received sysex buffers.
 * We'll leave the ones that need attention on the mSysexProcessed
 * list, and return those that don't to the active list.
 */
PRIVATE void WinMidiInput::processSysexBuffers(WinSysexBuffer *buffers) 
{
	// Hack, if we're expecting a sysex echo of a certain size,
	// remove it from the "input stream"
	// NOTE: There is a small window where we might not be able
	// to catch this, if a normal event comes in and launches
	// processing and the callback gets to the sysex before we do.
	// This is unlikely.
	// !! It may not be, since they can be polling.
	// Might need to have yet another list?
	
	// trace("WinMidiInput::processBuffers Sysex echo size is %d\n", mSysexEchoSize);
	if (mSysexEchoSize > 0) {
		if (buffers != NULL) {
			buffers->process();

			// Allow it to be less ?
			// If there was an error on the receive, its almost always
			// the echo if we were expecting one.

			if (buffers->getLength() == mSysexEchoSize ||
				buffers->isError()) {

				// Set it to error status if it wasn't already so the
				// loop below will remove it.
				buffers->setError(true);
			}
		}
	}

	// Filter out any zero length buffers that can come in after
	// midiInReset, and any buffers marked with errors.

	WinSysexBuffer* ignore = NULL;
	WinSysexBuffer* next = NULL;
	WinSysexBuffer* prev = NULL;
	WinSysexBuffer* b;

	for (b = buffers ; b != NULL ; b = next) {
		next = b->getNext();
		if (b->isError() || b->getLength() == 0) {
			if (b->isError())
			  trace("WinMidiInput::processBuffers"
					" Ignoring invalid receive buffer.\n");	
			else
			  trace("WinMidiInput::processBuffers"
					" Ignoring zero length receive buffer.\n");	

			if (prev == NULL)
			  buffers = next;
			else
			  prev->setNext(next);

			b->setNext(ignore);
			ignore = b;
		}
		else
		  prev = b;
	}

	// add em back
	next = NULL;
	for (b = ignore ; b != NULL ; b = next) {
		next = b->getNext();
		addSysex(b);
	}

	if (buffers == NULL)
	  trace("WinMidiInput::processBuffers No buffers left after filtering.\n");
	else {
		// echo sysex to the output device if enabled
		// NOTE WELL: This can take a LONG time, many seconds,  if the
		// sysex buffer is large and the device driver handles it
		// synchronously.  If we're on the receiving end of MIDI Yoke,
		// it may break up large buffers into smaller ones, and send
		// them to both to us quickly.  We're often still waiting
		// to send the first one when the second one comes in.
		echoSysex(buffers);

		// automatically release sysex buffers if we don't care
		if (mIgnoreSysex)
		  ignoreSysex(buffers);

		else {
			// Anything that remains is now considered accessible by the
			// application.  The accessible flag has been off till
			// now so we have a chance to remove clutter from the sysex
			// list before getSysex returns anything.

			int count = 0;
			for (b = buffers ; b != NULL ; b = b->getNext()) {
				b->setAccessible(true);
				count++;
			}

			// add them to the end of the processed list
			enterCriticalSection();
			WinSysexBuffer *last = NULL;
			for (last = mSysexProcessed ; 
				 last != NULL && last->getNext() != NULL ; 
				 last = last->getNext());
			if (last != NULL)
			  last->setNext(buffers);
			else
			  mSysexProcessed = buffers;
			leaveCriticalSection();

		}
	}

	// make sure we're left with an active buffer
	if (mSysexActive == NULL) {
		trace("WinMidiInput::processBuffers no more active buffers!\n");

		// always do this??
		// should we try to stay one ahead?
		allocSysexBuffer();
	}
}

/**
 * Called to echo any sysex buffers we've received to the echo device
 * when enabled.
 */
PRIVATE void WinMidiInput::echoSysex(WinSysexBuffer *buffers)
{
	if (mEchoDevice != NULL && mEchoSysex) {

		for (WinSysexBuffer *buffer = buffers ; buffer != NULL ; 
			 buffer = buffer->getNext()) {

			// only hit the ones that aren't yet marked accessible
			// just in case the app is slow, and we end up here
			// with already processed buffers
			// now that we've split the received and processed list,
			// this shouldn't be necessary

			if (!buffer->isAccessible()) {

				trace("WinMidiInput::echoSysex echoing sysex buffer with %d bytes\n",
					  buffer->getLength());

				// this should wait till the driver says its done
				mEchoDevice->sendSysex(buffer->getBuffer(),
									   buffer->getLength());
			}
		}
	}
}

/**
 * Called to arm a sysex receive buffer.
 */
PRIVATE void WinMidiInput::addSysex(WinSysexBuffer *b)
{
	b->init();

	enterCriticalSection();
	if (mLastSysexActive != NULL) {
		mLastSysexActive->setNext(b);
	}
	else {
		mSysexActive = b;
	}
	mLastSysexActive = b;
	leaveCriticalSection();

	// this adds it to the device if it already has a HMIDIIN port
	b->add();
}

/**
 * Allocate a new sysex buffer and arm it.
 */
PRIVATE void WinMidiInput::allocSysexBuffer() 
{
	WinSysexBuffer *b = new WinSysexBuffer();
	b->setInputDevice(this);

	enterCriticalSection();
	b->setLink(mSysexBuffers);
	mSysexBuffers = b;
	leaveCriticalSection();

	addSysex(b);
}

//////////////////////////////////////////////////////////////////////
//
// Listener Event Access Functions
//
// Need to work this into the MidiInput inteface but not sure how
// to model sysex's generically yet.
//
//////////////////////////////////////////////////////////////////////

/**
 * Called by the application callback to retrieve any sysex
 * buffers that have been received.
 * 
 * The list is expected to be returned with the freeSysex method.
 */
PUBLIC WinSysexBuffer* WinMidiInput::getSysex()
{
	WinSysexBuffer *buffers = NULL;
	WinSysexBuffer *b;

	enterCriticalSection();
	buffers = mSysexProcessed;
	mSysexProcessed = NULL;
	leaveCriticalSection();

	// post process the buffers before returning them to the app
	for (b = buffers ; b != NULL ; b = b->getNext()) {
		b->process();
		trace("WinMidiInput::getSysex returning %lx with %d bytes.\n", 
			  b, b->getLength());
	}

	return buffers;
}

/**
 * Called by the application callback to return the first sysex
 * buffer in the input queue.  This might be used if you're expecting
 * small messages that fit in one buffer and don't want to bother with 
 * other messages that may have come in after.
 */
PUBLIC WinSysexBuffer* WinMidiInput::getOneSysex()
{
	WinSysexBuffer *buffer = NULL;

	enterCriticalSection();
	if (mSysexProcessed != NULL) {
		buffer = mSysexProcessed;
		mSysexProcessed = buffer->getNext();
	}
	leaveCriticalSection();

	// post process the buffers before returning them to the app
	if (buffer != NULL) {
		buffer->process();
		trace("WinMidiInput::getOneSysex returning %lx with %d bytes.\n", 
			  buffer, buffer->getLength());
	}

	return buffer;
}

/**
 * Called by the applciation callback to return a list of sysex buffers
 * previously obtained from getSysex() or getOneSysex().
 */
PUBLIC void WinMidiInput::freeSysex(WinSysexBuffer *buffers) 
{
	trace("WinMidiInput::freeSysex\n");
	if (buffers != NULL) {
		WinSysexBuffer *next = NULL;
		for (WinSysexBuffer *b = buffers ; b != NULL ; b = next) {
			next = b->getNext();
			addSysex(b);	
		}
	}
}

/**
 * Called by the application callback to forget about any sysex buffers
 * we may have accumulated.
 */
PUBLIC void WinMidiInput::ignoreSysex(void)
{
	WinSysexBuffer *buffers = getSysex();
	ignoreSysex(buffers);
}

/**
 * Internal method to ignore a list of sysex buffers.
 */
PRIVATE void WinMidiInput::ignoreSysex(WinSysexBuffer *buffers)
{
	if (buffers != NULL) {
		for (WinSysexBuffer *b = buffers ; b != NULL ; b = b->getNext()) {
			trace("WinMidiInput::ignoreSysex %d bytes\n", b->getLength());

			// send the contents to the debug output stream if
			// it isn't too big
			if (b->getLength() < 32) {
				unsigned char *buffer = b->getBuffer();
				Vbuf *vb = new Vbuf();
				for (int i = 0 ; i < b->getLength() ; i++) {
					char buf[32];
					sprintf(buf, "%lx", buffer[i]);
					vb->add(buf);
					vb->add(" ");
				}
				trace(vb->getString());
				trace("\n");
				delete vb;
			}
		}

		freeSysex(buffers);
	}
}

/**
 * Set to true to ignore any sysex messages that come in.
 * The monitor thread will not be notified and the application callback
 * will not be called.
 */
PUBLIC void WinMidiInput::setIgnoreSysex(bool ignore) {

	mIgnoreSysex = ignore;
}

PUBLIC bool WinMidiInput::isIgnoreSysex()
{
    return mIgnoreSysex;
}

/**
 * Set the size of an expected sysex message.  If we receive
 * one that is exactly this size we assume that we are receiving
 * a sysex request message that had been previously been sent to a device
 * and the device is echoing it back to us.  Some primitive devices
 * like the Nanosynth have no way to disable midi-thru between
 * their input and output ports.
 *
 * When this is detected we silently ignore the message.
 */
PUBLIC void WinMidiInput::setSysexEchoSize(int size) {

	mSysexEchoSize = size;
}

/**
 * Returns the number of sysex bytes we have received that haven't
 * been processed by the application callback yet.
 * 
 * This can be used to periodically check (such as using a clock event) 
 * to see if we're recieving anything at all, and if too much time goes by
 * without getting anything, assume somethign is wrong and cancel the
 * operation.
 */
PUBLIC int WinMidiInput::getSysexBytesReceived(void)
{
	int bytes = 0;

	// If the application isn't popping buffers regularly, this
	// could be expensive.

	enterCriticalSection();
	for (WinSysexBuffer *b = mSysexProcessed ; b != NULL ; b = b->getNext())
	  bytes += b->getBytesReceived();
	leaveCriticalSection();
	
	trace("WinMidiInput::getSysexBytesReceived %d\n", bytes);

	return bytes;
}

/**
 * Returns the number of sysex bytes in blocks that are prepared
 * and actively filling, or ready to be filled.  Shouldn't be
 * that expensive unless we allow a large number of prepared buffers.
 */
PUBLIC int WinMidiInput::getSysexBytesReceiving(void)
{
	int bytes = 0;

	enterCriticalSection();
	for (WinSysexBuffer *b = mSysexActive ; b != NULL ; b = b->getNext())
	  bytes += b->getBytesReceived();
	leaveCriticalSection();

	trace("WinMidiInput::getSysexBytesReceiving %d\n", bytes);

	return bytes;
}

/**
 * Can be called by applications that have grown tired of waiting for
 * a sysex message to be recieved.  Normally they will have been 
 * calling getSysexBytesReceived to monitor progress, and time out after 
 * a while of no activity.
 *
 * NOTE: This is an expensive and disruptive thing to call since we
 * have to reset the device.  Don't call this unless you believe
 * there really is something to cancel.  Call getBytesReceiving first.
 */
PUBLIC void WinMidiInput::cancelSysex(void)
{
	// reset will cause zero length MIM_LONGDATA messages, suspend
	// the input processing thread till the dust settles

    if (mNativePort) {

        mMonitorThread->setAllowProcessing(false);
        trace("WinMidiInput::cancelSysex midiInReset\n");
        midiInReset(mNativePort);
			
        // let things get done 1/10 sec
        Sleep(100);

        // We expect all the sysex buffers to have been removed,
        // if any have been left behind, they are no longer "added"

        if (mSysexActive != NULL) {
            trace("WinMidiInput::cancelSysex Active sysex after reset!\n");

            enterCriticalSection();
            for (WinSysexBuffer *b = mSysexActive ; b != NULL ; 
                 b = b->getNext()) {
                // have to set the added flag to false first
                b->setAdded(false);
                b->add();
            }
            leaveCriticalSection();
        }

        // now siulate a processing event to get the receive buffers
        // back on the active list
        mMonitorThread->setAllowProcessing(true);
        processEventsReceived();

        // this is no longer relevant
        mSysexEchoSize = 0;
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
