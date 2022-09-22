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
 * On Windows we cannot do much MIDI event processing directly 
 * in the interrupt handler, instead we immediately convert the received
 * MIDI bytes into a MidiEvent, leave it on a list, and notify a monitor thread
 * to process the list.  Because of the possible delay between the time we 
 * receive an event and the time the application callback ends up running we
 * normally have a MidiTimer object that we use to immediately timestamp 
 * the event.
 *
 * We may also be configured with one or more MidiOutput streams that can 
 * be used for MIDI through routing with minimal latency.
 * 
 */

#include <stdio.h>

#include "Util.h"
#include "Trace.h"
#include "Thread.h"

#include "MidiEnv.h"
#include "MidiEvent.h"
#include "MidiMap.h"
#include "MidiTimer.h"
#include "MidiOutput.h"
#include "MidiInput.h"

//////////////////////////////////////////////////////////////////////
//
// MidiInput
//
//////////////////////////////////////////////////////////////////////

/**
 * Create and initializes a new base midi input stream.
 * The object is initially in a disconnected, use setPort()
 * and connect() to connect to a port.
 *
 */
PUBLIC MidiInput::MidiInput(MidiEnv* env, MidiPort* port)
{
	mEnv				= env;
	mPort				= port;
	mNext 				= NULL;
	mTimer				= NULL;
	mTempo				= new TempoMonitor();
	mEnabled			= false;

	mCsect = new CriticalSection;

	// mFilters is self initializing
	mInputMap     		= NULL;
	mEchoDevice 		= NULL;
    mEchoMap            = NULL;

	mEvents 			= NULL;
	mLastEvent			= NULL;

	mListener			= NULL;
	mInInterruptHandler = 0;

	mShortErrors 		= 0;
	mLongErrors			= 0;
	mWeirdErrors 		= 0;
	mEventOverflows		= 0;
	mInterruptOverruns	= 0;
	mLongOverflows		= 0;
}

/**
 * Destructor for the MIDI input stream.
 * Disconnects from the port if we're still attached.
 * Release various events we may have accumulated.
 */
PUBLIC MidiInput::~MidiInput(void)
{
	// get a warning if an abstract virtual is called
	// from a destructor, make sure the subclass does...
	//disconnect();

	if (mEvents != NULL)
	  mEvents->free();

	printWarnings();

	delete mCsect;
	delete mInputMap;
	delete mEchoMap;
	delete mTempo;
}

/**
 * Get the current port.
 */
PUBLIC MidiPort* MidiInput::getPort()
{
	return mPort;
}

/**
 * Set the port.
 * If we're already connected to a port the connection is closed.
 */
PUBLIC void MidiInput::setPort(MidiPort* port)
{
	disconnect();

	mPort = port;

	// should we auto connect?
}

/**
 * Set a timer object we can use in the interrupt handler to 
 * timestamp events as quickly as possible.
 */
PUBLIC void MidiInput::setTimer(MidiTimer *t) 
{
	mTimer = t;
}

/**
 * Set a MIDI output device so we can echo received events
 * directly in the interrupt handler.
 * TODO: Need to have a list of more than one.
 */
PUBLIC void MidiInput::setEchoDevice(MidiOutput* out)
{
	mEchoDevice = out;
}

PUBLIC void MidiInput::removeEchoDevice(MidiOutput* out)
{
    if (mEchoDevice == out)
      mEchoDevice = NULL;
}

PUBLIC void MidiInput::setInputMap(MidiMap* map) 
{
	delete mInputMap;
	mInputMap = map;
}

PUBLIC void MidiInput::setInputMap(MidiMapDefinition* def) 
{
	setInputMap(new MidiMap(def));
}

PUBLIC void MidiInput::setEchoMap(MidiMap* map)
{
	delete mEchoMap;
	mEchoMap = map;
}

PUBLIC void MidiInput::setEchoMap(MidiMapDefinition* def) 
{
	setEchoMap(new MidiMap(def));
}

/**
 * Print any disturbing statistics we accumulated while running.
 */
PUBLIC void MidiInput::printWarnings(void)
{
	// various statistics
	if (mWeirdErrors)
	  printf("%d weird errors in MIDI input!\n", mWeirdErrors);

	if (mShortErrors)
	  printf("%d short errors in MIDI input!\n", mShortErrors);

	if (mLongErrors)
	  printf("%d long errors in MIDI input!\n", mLongErrors);

	if (mEventOverflows)
	  printf("%d event overflows in MIDI input!\n", mEventOverflows);
	
	if (mInterruptOverruns)
	  printf("%d interuppt overruns in MIDI input!\n", 
				   mInterruptOverruns);
	if (mLongOverflows)
	  printf("%d sysex overflows in MIDI input!\n", mLongOverflows);
}

/****************************************************************************
 *                                                                          *
 *   							TEMPO MONITOR                               *
 *                                                                          *
 ****************************************************************************/

PUBLIC TempoMonitor::TempoMonitor()
{
	reset();
    // note that this is an integer 10x the actual float tempo
	mSmoothTempo = 0;
}

/**
 * Reset the tracker but leave teh last tempo in place until
 * we can start calculating a new one.
 */
PUBLIC void TempoMonitor::reset()
{
	mLastTime = 0;
	mPulse = 0.0f;
	mJitter = 0;
	initSamples();
}

PRIVATE void TempoMonitor::initSamples()
{
	mSample = 0;
	mTotal = 0;
	mDivisor = 0;
	mJitter = 0;
	for (int i = 0 ; i < MIDI_TEMPO_SAMPLES ; i++)
	  mSamples[i] = 0;
}

PUBLIC TempoMonitor::~TempoMonitor()
{
}

PUBLIC float TempoMonitor::getPulseWidth()
{
	return mPulse;
}

PUBLIC float TempoMonitor::getTempo()
{
	// 2500 / mPulse works too, but this is more obvious
	float msecPerBeat = mPulse * 24.0f;
	return (60000.0f / msecPerBeat);
}

PUBLIC int TempoMonitor::getSmoothTempo()
{
	return mSmoothTempo;
}

/**
 * If we are syncing to a device that does not send clocks when the
 * transport is stopped (I'm looking at you Ableton Live) when 
 * the transport stats again, TempoMonitor::clock will be called with
 * an abnormally long delta since the last clock.  We want to ignore this
 * delta so it doesn't throw the tempo smoother way out of line.  

 * At 60 BPM there is one beat per second or 24 MIDI clocks per second.
 * Each MIDI clock should ideally be 41.666r milliseconds apart.  This
 * will round to an average of 41 msec per clock.
 *
 * 30 PM would be 82 msec per clock.  15 BPM = 164 mpc,  5 BPM = 492 mpc.
 *
 * If we get a clock delta above 500 it is almost certainly because
 * the clocks have been paused and rseumed.  If we actually needed to 
 * support tempos under 5 BPM we could make this configurable, but it's
 * unlikely
 *
 * Ableton can also send out clocks very close together when it starts
 * which results in an extremely large tempo that then influences
 * the average for a few seconds. 120 BPM is 40.5 msec per clock, 240
 * is 20.25 msec, 480 is 10 msec.  Let's assume that anything under 5 msec
 * is noise and ignored.
 * 
 */
#define MAX_CLOCK_DELTA 500
#define MIN_CLOCK_DELTA 5

PUBLIC void TempoMonitor::clock(long msec)
{
	if (mLastTime == 0) {
		// first one, wait for another
        Trace(2, "MidiInput::clock start at msec %ld\n", msec);
	}
	else if (msec < mLastTime) {
		// not supposed to go back in time, reset but leave last tempo
        Trace(2, "MidiInput::clock rewinding at msec %ld\n", msec);
		initSamples();
	}
	else {
		long delta = msec - mLastTime;

        if (delta > MAX_CLOCK_DELTA) {
            Trace(2, "MidiInput::clock ignoring startup delta %ld\n", delta);
            initSamples();
        }
        else if (delta < MIN_CLOCK_DELTA) {
            Trace(2, "MidiInput::clock ignoring noise delta %ld\n", delta);
            initSamples();
        }
        else {
            mTotal -= mSamples[mSample];
            mTotal += delta;
            mSamples[mSample] = delta;

            mSample++;
            if (mSample >= MIDI_TEMPO_SAMPLES)
              mSample = 0;

            if (mDivisor < MIDI_TEMPO_SAMPLES)
              mDivisor++;

            /*
              long total = 0;
              for (int i = 0 ; i < MIDI_TEMPO_SAMPLES ; i++)
              total += mSamples[i];
              if (total != mTotal)
              Trace(2, "Totals don't match %d %d\n", mTotal, total);
            */

            // maintain the average pulse width
            mPulse = (float)mTotal / (float)mDivisor;

            bool clockTrace = false;

            if (clockTrace)
              Trace(2, "MidiInput::clock msec delta %ld total %ld divisor %ld width %ld (x1000)\n", 
                    (long)delta, (long)mTotal, (long)mDivisor, 
                    (long)(mPulse * 1000));

            // I played around with smoothing the pulse width but we have to 
            // be careful as this number needs at least 2 digits of precision
            // and probably 4.  Averaging seems to smooth it well enough.
            // And the tempo smoothing below keeps the display from jittering.

            // calculate temo
            float msecPerBeat = mPulse * 24.0f;
            float newTempo = (60000.0f / msecPerBeat);

            if (clockTrace)
              Trace(2, "MidiInput::clock tempo (x100) %ld\n", 
                    (long)(newTempo * 1000));
            
            // Tempo jitters around by about .4 plus or minus the center
            // Try to maintain a relatively stable number for display
            // purposes.  
		
            // If we notice a jump larger than this, just go there
            // immediately rather than changing gradually.
            int tempoJumpThreshold = 10;

            // The number of times we need to see a jitter in one direction
            // to consider it a "trend" that triggers a tempo change in that
            // direction.  Started with 4 which works okay but it still
            // bounces quite a bit, at 120 BPM from Ableton get frequent
            // bounce between 120 and 119.9.
            // One full beat shold be enough, this would be a good thing
            // to expose as a tunable parameter
            int jitterTrendThreshold = 24;

            // remember that this is an integer 10x the actual float tempo
            int smoothTempo = mSmoothTempo;
            int itempo = (int)(newTempo * 10);
            int diff = itempo - mSmoothTempo;
            int absdiff = (diff > 0) ? diff : -diff;

            if (absdiff > tempoJumpThreshold) {
                smoothTempo = itempo;
                // reset jitter?
                mJitter = 0;
            }
            else if (diff > 0) {
                mJitter++;
                if (mJitter > jitterTrendThreshold) {
                    smoothTempo++; 
                }
            }
            else if (diff < 0) {
                mJitter--;
                if (mJitter < -jitterTrendThreshold) {
                    smoothTempo--;
                }
            }
            else {
                // stability moves it closer to the center?
                if (mJitter > 0)
                  mJitter--;
                else if (mJitter < 0)
                  mJitter++;
            }

            /*
              Trace(2, "MidiIn: tempo %ld (x100) diff %ld jitter %ld smooth %ld\n",
			  (long)(newTempo * 100),
			  (long)diff,
			  (long)mJitter,
			  (long)smoothTempo);
            */

            if (smoothTempo != mSmoothTempo) {
                Trace(2, "MIDI In: *** Tempo changing from %ld to %ld (x10)\n", 
                      (long)mSmoothTempo,
                      (long)smoothTempo);
                mSmoothTempo = smoothTempo;
                mJitter = 0;
            }
        }
    }
	mLastTime = msec;
}

/****************************************************************************
 *                                                                          *
 *   						MAPPING AND FILTERING                           *
 *                                                                          *
 ****************************************************************************/

PUBLIC MidiFilter::MidiFilter(void) 
{
	init();
}

PUBLIC MidiFilter::~MidiFilter(void) 
{
}

PUBLIC void MidiFilter::init(void) 
{
	polyPressure = 0;
	control 	= 0;
	program 	= 0;
	touch 		= 0;
	bend 		= 0;
	common		= 0;
	sysex 		= 0;
	realtime 	= 0;
}

/****************************************************************************
 *                                                                          *
 *   						   INPUT INTERRUPT                              *
 *                                                                          *
 ****************************************************************************/

/**
 * Called by the MIDI interrupt a "short" data event is received.
 * Converts an encoded MIDI event into a MidiEvent object and 
 * adds it to the event list.
 * 
 * The "msg" argument as the MIDI event packed into a 4 byte integer.
 *
 * If a MidiOut echo device has been registered, immediately
 * sent the event back out.
 *
 * If a callback function has been specified, create a MidiEvent
 * and add it to the input queue.  We then notify the subclass.
 * On Windows this will signal the monitor thread which will wake up
 * and call the listener.  On Mac we're already in a handler thread
 * so we can call the listener immediately.
 *
 * Some common filtering and mapping functions are supported
 * in the interrupt handler so we don't have to get the
 * application callback involved to simple things like echo to
 * a different channel.
 *
 */
PRIVATE void MidiInput::processShortMessage(int msg)
{
	// don't allow reentrancies, should be fast enough
	if (mInInterruptHandler) {
		mInterruptOverruns++;
		trace("MidiInput::processData input overrun!\n");
	}
	else {
		mInInterruptHandler = 1;

		// if we've been given a timer, get the time now to avoid 
		// processing drift
		// !! formerly used mTimer->getClock which returns the MIDI
		// clock, but I want this to be millis for greater accuracy,
		// not sure if the old sequencer can deal with this

		int clock = 0; 
		if (mTimer != NULL) {
			//clock = mTimer->getClock();
			clock = mTimer->getMilliseconds();
		}

		MidiEvent *event = NULL;

		int status  = msg & 0xFF;
		int byte1   = (msg >> 8) & 0xFF;
		int byte2   = (msg >> 16) & 0xFF;

		if (status >= 0xF0) {
			// a non-channel event, always filter out active sense garbage
			// also filter realtime if requested
			// do "commons" come in here??

			if (status != MS_SENSE && !mFilters.realtime) {

                if (mTimer != NULL) {
                    if (status == MS_CLOCK)
                      mTempo->clock(mTimer->getMilliseconds());
                }

				if (mEchoDevice != NULL)
					mEchoDevice->send(msg);

				if (mListener != NULL)
				  event = mEnv->newMidiEvent(status, 0, byte1, byte2);
			}
		}
		else {
			// its a channel event that may be mapped
			int channel = status & 0x0F;
			status  = status & 0xF0;

			bool filtered = 
				(status == MS_POLYPRESSURE && mFilters.polyPressure) ||
				(status == MS_CONTROL && mFilters.control) ||
				(status == MS_PROGRAM && mFilters.program) ||
				(status == MS_TOUCH && mFilters.touch) ||
				(status == MS_BEND && mFilters.bend);
		
			if (!filtered) {
				// do data mapping if we have an installed map

                if (mInputMap != NULL)
                  mInputMap->map(&channel, &status, &byte1, &byte2);

                if (mEchoDevice != NULL) {
                    int echannel = channel;
                    int estatus = status;
                    int ebyte1 = byte1;
                    int ebyte2 = byte2;

                    if (mEchoMap != NULL)
                      mEchoMap->map(&echannel, &estatus, &ebyte1, &ebyte2);

					int echomsg = estatus | echannel | 
						(ebyte1 << 8) | (ebyte2 << 16);

					// trace("MidiIn: Echoing %lx as %lx\n", p1, echomsg);
					mEchoDevice->send(echomsg);
				}

				// create an event if there is further processing to be done
				if (mListener != NULL) {

					event = mEnv->newMidiEvent(status, channel, byte1, byte2);

					// formerly used the "drum" flag of the map to 
					// set event duration to 1, is that still
					// desireable?
				}
			}
		}

		// process the event object if we created one,
		// should only have done this if callback was non-null

		if (event != NULL) {

			// don't trace clocks or active sense
#if 0			
			int status = event->getStatus();
			if (status != MS_CLOCK && status != MS_SENSE)
			  trace("MidiIn: Event %d %d\n", 
					event->getStatus(), event->getKey());
#endif

			if (mListener == NULL) {
				// shouldn't be here, but just in case we need to
				// create events for some special processing, free them here
				event->free();
			}
			else {
				// captured the clock earlier
				event->setClock(clock);

				// Add it to the list
				enterCriticalSection();
				if (mLastEvent != NULL)
				  mLastEvent->setNext(event);
				else
				  mEvents = event;
				mLastEvent = event;
				leaveCriticalSection();

				// notify the monitor thread
				//if (mMonitorThread)
				//mMonitorThread->signal();
				notifyEventsReceived();
			}
		}

		mInInterruptHandler = 0;
	}
}

//////////////////////////////////////////////////////////////////////
//
// Listener Control Functions
//
// These are called by the MidiInputListener after it has been notified
// of activity from the MIDI stream.
//
//////////////////////////////////////////////////////////////////////

/**
 * Critical section transition methods.  These must be used
 * whenever you need to access one of the event lists since
 * both the interrupt handler and the monitor thread may be accessing
 * them at the same time.
 */
PRIVATE void MidiInput::enterCriticalSection(void)
{
	if (mCsect)
	  mCsect->enter();
}

PRIVATE void MidiInput::leaveCriticalSection(void)
{
	if (mCsect)
	  mCsect->leave();
}

/**
 * Return the tempo if we have a timer.
 */
PUBLIC float MidiInput::getPulseWidth()
{
	return mTempo->getPulseWidth();
}

PUBLIC float MidiInput::getTempo()
{
	return mTempo->getTempo();
}

PUBLIC int MidiInput::getSmoothTempo()
{
	return mTempo->getSmoothTempo();
}

/**
 * Returns the list of events that have accumulated since the
 * interrupt handler was first invoked.  If the callback does something
 * really expensive, we can potentially keep adding things to the event
 * list until they finally call this function.
 */
PUBLIC MidiEvent *MidiInput::getEvents(void) 
{
	MidiEvent *events = mEvents;

	enterCriticalSection();
	mEvents = NULL;
	mLastEvent = NULL;
	leaveCriticalSection();

	return events;
}

/**
 * Called internally if the listener decides to ignore the accumulated events.
 * ?? Can also be called by the listener?
 */
PUBLIC void MidiInput::ignoreEvents(void)
{
	MidiEvent *events = getEvents();
	if (events != NULL)
	  events->free();
}

/**
 * Called by the listener to ignore any sysex buffers that have come in.
 * A stub right now till we can figure out how to generalize sysex.
 */
PUBLIC void MidiInput::ignoreSysex()
{
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
