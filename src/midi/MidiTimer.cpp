/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Base class for the platform-specific high resolution timer.
 *
 */

#include <stdio.h>

#include "Port.h"
#include "Trace.h"
#include "Thread.h"

#include "MidiListener.h"
#include "MidiOutput.h"
#include "MidiTimer.h"

/****************************************************************************
 *                                                                          *
 *   						  INTERRUPT HANDLER                             *
 *                                                                          *
 ****************************************************************************/

/**
 * Set if tempo change are deferred until the next MIDI clock rather
 * than applied immediately.
 * 
 * An experiment to process resizes immediately rather than waiting
 * for the next MIDI clock.  This doesn't appear to help drift and dealign,
 * it seems to make it worse.  I can't explain it, need to explore this.
 */
bool MidiTimerDeferredTempoChange = true;

/**
 * Maximum number of iterations we will attempt to correct a tick.
 */
#define MAX_ITERATIONS 100

/**
 * This method must be called every millisecond by the platform-specific timer.
 * On Windows we are in an interrupt handler and have to be careful what
 * API functions we call.  On Mac we're in a high priority thread and appear
 * to have more flexibility.
 * 
 * This of course needs to be as fast as humanly possible, though
 * if we finish up within 1ms it should be ok.
 * 
 * NOTE WELL: Despite the documentation, calling timeKillEvent inside
 * the timer interrupt handler doesn't appear to work, at least not under
 * the debugger.  The mInterruptEnable flag was added to temporarily suspend
 * timer interrupts, without actually killing the timer.
 * 
 * The user callback is expected to call setNextSignalClock or some other
 * timer control method to re-arm the timer.
 *
 */
PUBLIC void MidiTimer::interrupt()
{
	// hack to disable interrupts during debugging
	if (!mEnabled) return;

	mMillis++;

	// Don't allow reentries, this shouldn't happen on a reasonably 
	// fast machine
	if (!mEntered)
	  mEntered = true;
	else {
		// hey, shouldn't we advance time here!!
		mReentries++;
		Trace(1, "MidiTimer: interrupt reentry!\n");
		return;
	} 

	// If the pending start flag is sset, send StartSong followed by a clock.
	// Spec says we're supposed to wait 1ms between the two events, 
	// but modern devices don't seem to have a problem with these.  
    bool restarted = false;
	if (mPendingStart) {
		mPendingStart = false;
		if (mMidiSync) {
			sendStart();
			sendClock();
		}
		if (mMidiClockListener != NULL) {
			mMidiClockListener->midiStartEvent();
			mMidiClockListener->midiClockEvent();
		}
		restarted = true;
	}

	if (mPendingStop) {
		mPendingStop = false;
		if (mMidiSync)
		  sendStop();
		if (mMidiClockListener != NULL)
		  mMidiClockListener->midiStopEvent();
	}

	// Like sending StartSong, we're technically supposed to wait
	// 1ms between the two events.  If we send a song position, we probably
	// actually do need to wait a few ms for the transport to catch up!!
    // if we have to do that then we will need to pre-schedule the
    // song position, then send the clock when we're ready to align
	if (mPendingContinue) {
		mPendingContinue = false;
		if (mPendingSongPosition) {
			mPendingSongPosition = false;
			if (mMidiSync)
			  sendSongPosition(mSongPosition);
		}
		if (mMidiSync) {
			sendContinue();
			sendClock();
		}
		if (mMidiClockListener != NULL) {
			mMidiClockListener->midiContinueEvent();
			mMidiClockListener->midiClockEvent();
		}
		restarted = true;
	}

    // If we're restarting the tick counter could send a clock
    // now since we're logically at clock zero.  It isn't that important
    // since we only do this to start a clock stream so slave devices
    // can track tempo before we send START, they'll still get a clock
    // out of nowhere and then start tracking the distance between them.
    if (mRestartTicks) {
        mRestartTicks = false;
        // send an initial clock?
        restarted = true;
    }

	// Certain operations like enabling clocks for the first time or
	// sending StartSong require that we reset the MIDI clock accumulator.
	// In theory mRestartTicks could be set again while we're thinking but
	// I don't think that can happen in practice...
    // TODO: Shouldn't we be resetting the "user" clock too? mTick and mClock

	if (restarted) {
		mMidiTick = 0.0;
		mMidiClocks = 0;
		// since we're logically at a clock boundary adjust the tempo too
		// necessary to do this now since we sometimes set tempo before 
		// starting
		setPendingTempo();
	}

	// advance midi clock, ignore if not set up yet
    // if we just sent START or CONTINUE do not advance yet
	if (!restarted && mMidiMillisPerClock > 0.0) {
		mMidiTick += 1.0f;
		if (mMidiTick >= mMidiMillisPerClock) {
			// We're at or beyond the time to send a midi clock pulse
			if (mSendingClocks) {
				if (mMidiSync)
				  sendClock();
				if (mMidiClockListener != NULL)
				  mMidiClockListener->midiClockEvent();
				mMidiClocks++;
			}

			mMidiTick -= mMidiMillisPerClock;

			// Keep decrementing in case we're more than one clock width over
			// the threshold.  Now that we make tempo changes (which
			// can shorten the clock width) at even clock boundaries, should
			// only see this if something is delaying the interrupt handler.
			// The effect of this is that we will drop clocks rather than 
			// sending out a burst of them only 1ms apart.

			if (mMidiTick >= mMidiMillisPerClock) {
				Trace(1, "ERROR: MidiTimer: Unexpected clock width change!\n");
				// occasionally see this hang on startup, make sure 
				// to constrain the loop
				float startMidiTick = mMidiTick;
				int iteration = 0;
				while (mMidiTick >= mMidiMillisPerClock && 
					   iteration < MAX_ITERATIONS) {
					mMidiTick -= mMidiMillisPerClock;
					iteration++;	
				}
				if (mMidiTick >= mMidiMillisPerClock) {
					Trace(1, "ERROR: Unable to correct mMidiTick, starting value %ld, decrement %ld (x1000)\n",
						  (long)(startMidiTick * 1000),
						  (long)(mMidiMillisPerClock * 1000));

					mMidiTick = 0;
				}
			}

			// process pending tempo change
			// ?? should we do this before calling the clock listener?
			setPendingTempo();
		}
	}

	// advance the user clock
	// !! this counter isn't going to sync as nicely as the MIDI clock
	// counter, may be changing mMillisPerClock not at a boundary,
	// should be ok?
	if (mMillisPerClock > 0.0) {
		mTick += 1.0f;
		if (mTick >= mMillisPerClock) {
			mClock++;
			mBeatCounter++;
			if (mBeatCounter >= mClocksPerBeat) {
				mBeat++;
				mBeatCounter = 0;
			}
			mTick -= mMillisPerClock;

			// Note that since we don't defer tempo changes to user clock 
			// boundaries, we have the potential to be more than one clock
			// width over the boundary.  
			if (mTick >= mMillisPerClock) {
				// since we're only using this with Mobius be silent
				//Trace(1, "WARNING: MidiTimer: Compensating for clock tick overflow!\n");
				// like the loop above, be careful not to let it run too long
				float startTick = mTick;
				int iteration = 0;
				while (mTick >= mMillisPerClock && iteration < MAX_ITERATIONS) {
					mTick -= mMillisPerClock;
					iteration++;
				}
				if (mTick >= mMillisPerClock) {
					Trace(1, "ERROR: Unable to correct mTick, starting value %ld, decrement %ld (x1000)\n",
						  (long)(startTick * 1000),
						  (long)(mMillisPerClock * 1000));

					mTick = 0;
				}
			}
		}
	}

	// Call the application callback if we've reached the signal clock.
    // Save the current time so we can account for delays during signal
    // propogation when scheduling the next signal.
	// Note that the callback is under the same restrictions we are,
	// it should either post a message, or record clock stats somewhere that's
	// being polled.

	if (mSignalClock > 0 && mClock >= mSignalClock) {

		// If the current time is beyond the desired signal time, 
		// we have experienced an uncorrectable delay.  This can
		// happen often during debugging with breakpoints but when 
		// running normally it will introduce timing errors.
		if (mClock > mSignalClock)
		  mOverflows++;

		// must be re-armed by the callback at some point
		mSignalClock = 0;

		if (mCallback != NULL) {
			try {
				(*mCallback)(this, mCallbackArgs);
			}
			catch (...) {
				// just in case trace is hosed
				printf("Exception in timer callback!\n");
				fflush(stdout);
				Trace(1, "Exception in timer callback!\n");
			}
		}
	}

	mEntered = false;
}

PRIVATE void MidiTimer::setPendingTempo()
{
	if (mPendingTempo != 0.0) {
		setTempoInternal(mPendingTempo);
		mPendingTempo = 0.0f;

		// If we've been accumulating a remainder, let it carry
		// into the new tempo.  This feels right since it is a 
		// compensation for clocks that have already been sent?
		//mMidiTick = 0.0f;
	}
}

PRIVATE void MidiTimer::setTempoInternal(float tempo)
{
	mBeatsPerMinute = tempo;
	mMillisPerClock = TIMER_MSEC_PER_CLOCK(mBeatsPerMinute, mClocksPerBeat);
	mMidiMillisPerClock	= TIMER_MSEC_PER_CLOCK(mBeatsPerMinute, 24);
}

/****************************************************************************
 *                                                                          *
 *                                CONSTRUCTOR                               *
 *                                                                          *
 ****************************************************************************/

PUBLIC MidiTimer::MidiTimer(MidiEnv* env) {

	mEnv = env;

    // Configuration

    mBeatsPerMinute		= 0.0;
    mNewBeatsPerMinute	= 0.0;
    mClocksPerBeat      = 0;
    mBeatsPerMeasure	= 4;
	mMillisPerClock     = 0.0;
	mMidiMillisPerClock = 0.0;
    mMidiSync           = false;

    mCallback           = NULL;
    mCallbackArgs       = NULL;
    mSignalClock        = 0;
	mMidiClockListener	= NULL;

    // Runtime state

	mMillis				= 0;
    mClock              = 0;
    mBeat               = 0;
    mMeasure            = 0;
    mSongPosition       = 0;
    mBeatCounter        = 0;
    mTick               = 0.0;
    mMidiTick           = 0.0;
	mMidiClocks 		= 0;
	
	mMidiStarted		= false;
	mSendingClocks		= false;
	mPendingStart		= false;
	mPendingStop		= false;
	mPendingContinue	= false;
	mPendingSongPosition = false;
	mRestartTicks		= false;
	mPendingTempo		= 0.0f;

    // Interupt stats

    mEnabled            = true;
    mEntered            = false;
    mReentries          = 0;
    mOverflows          = 0;

    clearRegisters();
	resetMidiOutputs();

    // these calls will calculate mMillisPerClock and mMidiMillisPerClock
    setTempo(TIMER_DEFAULT_TEMPO);
    setResolution(TIMER_DEFAULT_CPB);
}

/**
 * Destructs a timer object.
 * Subclass should ensure that any platform resources are freed.
 */
PUBLIC MidiTimer::~MidiTimer(void)
{
}

/**
 * Print interesting timer metrics.  These indiciate that something is wrong.  
 */
PUBLIC void MidiTimer::printWarnings()
{
	// various statistics
	if (mReentries)
	  printf("%d MidiTimer reentries!\n", mReentries);

	if (mOverflows)
	  printf("%d MidiTimer overflows!\n", mOverflows);
}

/**
 * Set the timer callback function.
 * This will be called by the timer interrupt handler as each
 * "signal time" is reached.
 */
PUBLIC 	void MidiTimer::setCallback(TIMER_CALLBACK cb, void *args) {

    mCallback = cb;
    mCallbackArgs = args;
}

/**
 * Sets an object that wants to be notified whenever the MIDI
 * clock ticks.
 */
PUBLIC void MidiTimer::setMidiClockListener(MidiClockListener* l)
{
	mMidiClockListener = l;
}

/**
 * Sets the beats per measure for the clock.
 * This doesn't result in any tempo or time change but it does
 * affect the maintenance of the logical "measure" counter.
 *
 * Should clear the measure counter?
 */
PUBLIC void MidiTimer::setBeatsPerMeasure(int beats)
{
	mBeatsPerMeasure = beats;
}

/**
 * Called by the application, usually in its callback, to set the
 * next time at which the callback is to be called.
 */
PUBLIC void MidiTimer::setNextSignalClock(int c)
{
	mSignalClock = c;
}

/**
 * Sets the resolution of the user clock.
 * The unit of measure is "clocks per beat", the default is 96.
 * Resolutions are normally multiples of 24 (the MIDI clock tick), 
 * though that doesn't have to be the case.
 *
 * NOTE: It is assumed that clock resolution never changes so we don't
 * have to worry about changing exactly on a clock boundary which is an
 * issue when synchronizing with non-MIDI timelines (e.g. digital audio).
 */
PUBLIC void MidiTimer::setResolution(int cpb)
{
	// Should stop if running? If not, then we should proably
    // reset some of the related clock state.

	mClocksPerBeat = cpb;

	// recalculate msec clock values
	mMillisPerClock = TIMER_MSEC_PER_CLOCK(mBeatsPerMinute, mClocksPerBeat);
}

/**
 * Sets the tempo of the user and MIDI clocks.
 * The internal clock always runs at 1ms ticks, but we maintain both
 * an internal "user" clock and the "midi" clock relative to these
 * 1ms ticks.  The overall tempo is specified here, in beats per minute.
 * 
 * The clock may continue to run as this is adjusted.
 * 
 * NOTE: It is very important that we not change the tempo until
 * the next clock if we're currently sending clocks.  This is for 
 * synchronization with non-MIDI timelines like digital audio that need 
 * to know what a "clock" means in real time.  It is much simpler if we
 * do not have to divide a clock into several pieces.  This is accomplished
 * by setting mNewBeatsPerMinute and letting the interrupt handler
 * change the tempo at the right time.
 */
PUBLIC void MidiTimer::setTempo(float tempo)
{
	if (mMidiStarted && MidiTimerDeferredTempoChange)
	  mPendingTempo = tempo;
	else {
		mPendingTempo = 0.0f;
		setTempoInternal(tempo);
	}
}

/**
 * Return the tempo.  If we've set a pending tempo return that so 
 * applications can tell if a tempo change was registered, if not yet
 * processed.
 */
PUBLIC float MidiTimer::getTempo()
{
	return ((mPendingTempo != 0.0) ? mPendingTempo : mBeatsPerMinute);
}

/**
 * Diagnostic functions to access internal clock state.
 */
PUBLIC int MidiTimer::getMidiClocks() 
{
	return mMidiClocks;
}

PUBLIC float MidiTimer::getMidiMillisPerClock()
{
	return mMidiMillisPerClock;
}

/**
 * Used in some special cases like debugging to disable interrupt handling.
 * This allows us to sit in the debugger for many interrupts without
 * having the clock leap ahead when we continue.
 */
PUBLIC void MidiTimer::setInterruptEnabled(bool b)
{
    mEnabled = b;
}

/****************************************************************************
 *                                                                          *
 *   							  MIDI SYNC                                 *
 *                                                                          *
 ****************************************************************************/
/*
 * These methods are used when the timer is generating MIDI realtime
 * messages but we are not "sequencing" and maintaining an application clock.  
 * Mobius uses it this way.
 *
 */

/**
 * Enable sending MIDI realtime events.
 * This is expected to be an unusual operation, you either leave it
 * on or off.
 */
PUBLIC void MidiTimer::setMidiSync(bool b)
{
	if (mMidiSync != b) {
		mMidiSync = b;
		if (mMidiSync) {
			// when turning it on after a pause, make sure the ticks are reset
			mRestartTicks = true;
		}
	}
}

/**
 * Begin sending MIDI clocks at the current tempo if we
 * aren't already.
 */
PUBLIC void MidiTimer::midiStartClocks()
{
	if (!mSendingClocks && start()) {
		mSendingClocks = true;
		// when starting up after a pause, be sure the tick
		// counters are initialized
		mRestartTicks = true;
	}
}

/**
 * Stop sending MIDI clocks.
 * 
 * NOTE: The interrupt handler may be processing one at this moment
 * that will still leak out.  This is relatively harmless as long as the
 * application can deal with one spurious clock. To fix this, we would have
 * to notify the app with a special event like MS_STOP_CLOCKS that it
 * can wait for.
 */
PUBLIC void MidiTimer::midiStopClocks()
{
	mSendingClocks = false;
}

PUBLIC bool MidiTimer::isSendingClocks()
{
	return mSendingClocks;
}

/**
 * Send a MIDI StartSong event followed closely by a clock 
 * to make if official.  Spec says we're supposed to wait 1ms 
 * between the StartSong and the Clock but modern devices don't
 * seem to have an issue with this.  Callers are not expecting a delay,
 * so if one is necessary will need to push it to the interrupt handler.
 *
 * Note that this will send StartSong even if we've already started, 
 * so application must use isStarted if that is important.
 *
 * The interrupt handler may be sending clocks at this very moment, so
 * rather than send the MS_START event out here, we set a flag and let
 * the interrupt handler do it at the next millisecond.
 */
PUBLIC void MidiTimer::midiStart()
{
	if (start()) {

		// start event will be sent in the next interrupt
		mPendingStart = true;

		// enable the emission of clocks if we haven't already
		midiStartClocks();

		// even though we don't technically start until the
		// next timer interrupt, to the outside world we've started
		mMidiStarted = true;
	}
}

PUBLIC bool MidiTimer::isMidiStarted()
{
	return mMidiStarted;
}

/**
 * Send MIDI StopSong and optionally stop sending MIDI clocks.
 *
 * This will send StopSong even if we don't think we're started.
 * Useful if the external device can be started manually.
 */
PUBLIC void MidiTimer::midiStop(bool stopClocks)
{
	if (stopClocks)
	  mSendingClocks = false;

	// let the event be sent by the interrupt handler to make sure we don't
	// get any spurious clocks after it
	mPendingStop = true;

	mMidiStarted = false;
}

PUBLIC void MidiTimer::midiStop()
{
	midiStop(true);
}

/**
 * Send MIDI Continue, with or without SongPosition.
 * If the songPosition flag is on, we'll send a song position event
 * before the Continue.  Note that some (only older?) devices need 
 * a little time after a song position to orient themselves before they
 * can receive a Continue message.  If we need to do that, will
 * have to push it into the interrupt handler.
 *
 * This is ignored if we are not currently stopped.
 *
 */
PUBLIC void MidiTimer::midiContinue(bool songPosition)
{
	if (!mMidiStarted) {
		
		// note that we set mPendingSongPosition first since it
		// is gated by mPendingContinue in the interrupt handler
		mPendingSongPosition = songPosition;
		mPendingContinue = true;

		// enable the emission of clocks if we haven't already
		midiStartClocks();

		mMidiStarted = true;
	}
}

PUBLIC void MidiTimer::midiContinue()
{
	midiContinue(false);
}

/****************************************************************************
 *                                                                          *
 *   						  TRANSPORT CONTROL                             *
 *                                                                          *
 ****************************************************************************/
/*
 * This set of methods is used by applications that want to maintain
 * the extra clock state necessary for a MIDI sequencer.
 */

/**
 * Sets the current time.  The timer will be stopped if it is running.
 * TODO: Could keep the timer alive if we thought hard enough.
 *
 * This can be called by the application to set the clock prior
 * to starting the timer.  Note that the clock will be rounded
 * to a MIDI song position boundary.  This is necessary to stay
 * in sync with external MIDI devices.
 */
PUBLIC void MidiTimer::setClock(int clock)
{
	transStop();

	// Calculate song position, note that if we're being called from 
	// the top() method, the song position will already be correct.

	int midiClocks = mClocksPerBeat / 24;
    mSongPosition = (clock / (midiClocks + 1)) / 6;
	int actual = (mSongPosition * 6) * (midiClocks + 1);

	// set the time rounded to the song position
    // we might want to make this optional if we're not doing
    // external sync?
	mClock = actual;

    // update derived clock state
    updateClock();
}

/**
 * Update various internal state to reflect a change in the clock.
 * Do NOT round mClock here.
 */
PRIVATE void MidiTimer::updateClock()
{
    // track song position, this usually has been done by now
    int midiClocks = mClocksPerBeat / 24;
    mSongPosition = (mClock / (midiClocks + 1)) / 6;

	// convert absolute clock setting into the corresponding millisecond tick

	// number of "real" milliseconds to get to this clock
	float mclock = mMillisPerClock * (float)mClock;

	// leave the fractional part as the "tick remainder"
	long base = (long)mclock;
    mTick = mclock - (float)base;

	// same calculation for midi clocks
	mclock = mMidiMillisPerClock * (float)mClock;
	base = (long)mclock;
	mMidiTick = mclock - (float)base;

	// position the beat counter
    mBeat = mClock / mClocksPerBeat;
    mBeatCounter = mClock % mClocksPerBeat;
   
	// Ordinarily don't bother keeping the millisecond counter in sync,
	// though we could.  This is used for applications that just
	// want a millisecond counter for timestamping (EDP)
	if (mClock == 0)
	  mMillis = 0;
}

/**
 * Alternative to setClock(), sets the time using MIDI song position.
 */
PUBLIC void MidiTimer::setSongPosition(int psn)
{
	int midiClocks = mClocksPerBeat / 24;
	int clock = (psn * 6) * (midiClocks + 1);

	// should be no rounding
	setClock(clock);
}

/**
 * Start the timer if not already running.
 * The current clock will be rounded to a song position boundary, if you
 * don't want that use the cont() method.
 * 
 * An initial delay in user clocks may be specifed.
 * Is this really necessary?
 */
PUBLIC bool MidiTimer::transStart(int initialDelay)
{
	// if interrupt handler is already running, should stop it so
	// we don't conflict?
	if (isRunning()) {
		transStop();
		// !! now what, it may still be doing something, if we have
		// to do this better to defer the whole thing to the interrupt
		// handler
		SleepMillis(10);
	}

	// round clock to a song position boundary, it will often
	// already be there but may not if stop() was just called
	// without an intervening setClock
	setClock(mClock);

	// calculate the first signal clock
	mSignalClock = 0;
	if (initialDelay > 0)
	  mSignalClock = mClock + initialDelay;

	// send various MIDI sync messages
	// if we're at time zero, send START rather than song position
	if (mClock == 0)
	  midiStart();
	else
	  midiContinue(true);

	return start();
}

/**
 * Stops the timer.
 *
 * Note that we may have received some millisecond events that have
 * caused mTick and mMidiTick to advance, but which were not 
 * enough to cause the clocks to increment.  If we continue from this
 * point, we have to behave as if we're starting from the exact user
 * clock that we stopped on, which may imply that we roll back slightly
 * from our internal millisecond time.  This is accomplished by calling
 * updateClock here to rest the tick counters but keep the current clock.
 */
PUBLIC void MidiTimer::transStop(void)
{
    if (isRunning()) {

		midiStop();

		// we have historically stopped the whole damn interrupt
		// handler, really necessary?
		stop();

        // round millisecond tick counters back down, this
        // will also capture the ending song position
		// !! hey, the interrupt may still be running and fuck with these
		// either need to pause or push this into the interrupt handler
		SleepMillis(10);
        updateClock();
    }
}

/**
 * Resumes the timer from its stopped state, without changing the
 * current time and the signal clock.
 *
 * For the transport controls we always send song position.
 */
PUBLIC void MidiTimer::transContinue(void)
{
	if (!isRunning())
	  midiContinue(true);
}

/****************************************************************************
 *                                                                          *
 *   						   CLOCK REGISTERS                              *
 *                                                                          *
 ****************************************************************************/

/**
 * Sets the clock in one of the registers.
 */
PUBLIC void MidiTimer::setRegister(int reg, int clk)
{
	if (reg >= 0 && reg < TIMER_MAX_REGISTERS)
	  mRegisters[reg] = clk;
}

/**
 * Sets a register to the current time.
 */
PUBLIC void MidiTimer::captureRegister(int reg)
{
	if (reg >= 0 && reg < TIMER_MAX_REGISTERS)
	  mRegisters[reg] = mClock;
}

/**
 * Restores the current clock from one of the clock registers.
 * Note that due to song position rounding, the actual time set may
 * be different than the time in the register, which is what was returned
 * eariler by captureRegister.
 */
PUBLIC void MidiTimer::restoreRegister(int reg)
{
	if (reg >= 0 && reg < TIMER_MAX_REGISTERS)
	  setClock(mRegisters[reg]);
}

/**
 * Clears all of the clock registers.
 */
PUBLIC void MidiTimer::clearRegisters(void)
{
	for (int i = 0 ; i < TIMER_MAX_REGISTERS ; i++) 
	  mRegisters[i] = 0;
}

/****************************************************************************
 *                                                                          *
 *   							 MIDI OUTPUTS                               *
 *                                                                          *
 ****************************************************************************/

/**
 * Resets the MIDI output devices.
 */
PUBLIC void MidiTimer::resetMidiOutputs()
{
	mMidiOutputCount = 0;
	for (int i = 0 ; i < TIMER_MAX_OUTPUTS ; i++)
	  mMidiOutputs[i] = NULL;
}

/**
 * Adds a MIDI output device to be sent realtime events if MIDI
 * sync is enabled.
 */
PUBLIC void MidiTimer::addMidiOutput(MidiOutput *dev)
{
    bool found = false;
    for (int i = 0 ; i < mMidiOutputCount ; i++) {
        if (mMidiOutputs[i] == dev) {
            found = true;
            break;
        }
    }

    if (!found && mMidiOutputCount < TIMER_MAX_OUTPUTS)
	  mMidiOutputs[mMidiOutputCount++] = dev;
}

/**
 * Remove a MIDI output device.
 */
PUBLIC void MidiTimer::removeMidiOutput(MidiOutput* dev)
{
    for (int i = 0 ; i < mMidiOutputCount ; i++) {
        if (mMidiOutputs[i] == dev) {
            // slide everything down
            for (int j = i + 1 ; j < mMidiOutputCount ; j++) 
              mMidiOutputs[j - 1] = mMidiOutputs[j];

            mMidiOutputs[mMidiOutputCount - 1] = NULL;
            mMidiOutputCount--;
            break;
        }
    }
}


PRIVATE void MidiTimer::sendClock()
{
	for (int i = 0 ; i < mMidiOutputCount ; i++) {
		mMidiOutputs[i]->sendClock();
	}
}

PRIVATE void MidiTimer::sendStart()
{
	for (int i = 0 ; i < mMidiOutputCount ; i++) {
		mMidiOutputs[i]->sendStart();
	}
}

PRIVATE void MidiTimer::sendStop()
{
	for (int i = 0 ; i < mMidiOutputCount ; i++) {
		mMidiOutputs[i]->sendStop();
	}
}

PRIVATE void MidiTimer::sendContinue()
{
	for (int i = 0 ; i < mMidiOutputCount ; i++)
	  mMidiOutputs[i]->sendContinue();
}

PRIVATE void MidiTimer::sendSongPosition(int psn)
{
	for (int i = 0 ; i < mMidiOutputCount ; i++)
	  mMidiOutputs[i]->sendSongPosition(psn);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
