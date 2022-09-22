/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * A class managing coordination between a MIDI output device
 * and the millisecond timer to provide a higher level "transport"
 * abstraction for generating MIDI realtime events.
 *
 * Designed for use with the Synchronizer, it could be used elsewhere
 * except that we have a dependency on Event.
 *
 * When Synchronizer is constructed it will create one MidiTransport.
 * The MidiTransport is given the MidiInterface that was given
 * to Synchronizer by Mobius.  MidiTransport will register itself
 * as the MidiClockListener for the MidiInterface.  Thereafter it
 * will receive notification each time the timer thread encapsulated
 * within MidiInterface sends an MS_CLOCK, MS_START, MS_STOP, 
 * or MS_CONTINUE event.
 *
 * Inside we manage a MidiQueue object and forward MidiEvents to it.
 * MidiQueue handles the semantics of the event stream including whether
 * we are started or stopped, the song position,  and when we've
 * received enough clocks to make a MIDI "beat".  
 *
 * During the audio interrupt Synchronizer will call MidiTransport.getEvents
 * to convert the raw MIDI events received into a list of Event objects
 * to be processed.  Event objects will have one of these SyncType values:
 *
 *     SYNC_TYPE_START
 *     SYNC_TYPE_STOP
 *     SYNC_TYPE_CONTINUE
 *     SYNC_TYPE_PULSE
 *
 * When SyncType is SYNC_TYPE_CONTINUE the Event will also contain
 * a ContinueClock.
 *
 * When SyncType is SYNC_TYPE_PULSE, the Event will also contain
 * a SyncUnit value:
 *
 *     SYNC_UNIT_MIDI_CLOCK
 *     SYNC_UNIT_MIDI_BEAT
 *
 * The SyncTracker for the MIDI clock generator will watch clock pulses.
 * The distinction between CLOCK and BEAT is only important when 
 * quantizing the start of a recording and when rounding it off.
 * 
 */

#include <stdlib.h>
#include <memory.h>

#include "Trace.h"
#include "Thread.h"

#include "MidiByte.h"
#include "MidiEvent.h"
#include "MidiInterface.h"
#include "AudioInterface.h"

#include "Event.h"
#include "MidiQueue.h"

#include "MidiTransport.h"

/****************************************************************************
 *                                                                          *
 *                               MIDI TRANSPORT                             *
 *                                                                          *
 ****************************************************************************/

PUBLIC MidiTransport::MidiTransport(MidiInterface* midi, int sampleRate)
{
    mMidi = midi;
    mSampleRate = sampleRate;

	mTempo = 0.0f;
	mSending = false;
	mStarts = 0;
	mIgnoreClocks = false;
	mInterruptMsec = 0;

    // mQueue initializes itself, but assign a trace name
	mQueue.setName("internal");

	// ask to be notified of midi clock events being sent
	mMidi->setClockListener(this);

    // Experimental flag to change how we add START/STOP/CONTINUE
    // events to the queue.  This was !UseInternalTransport in older
    // releases, it has been on for a long time.
    mImmediateTransportQueue = false;
}

PUBLIC MidiTransport::~MidiTransport()
{
	if (mMidi != NULL)
	  mMidi->setClockListener(NULL);
}

/****************************************************************************
 *                                                                          *
 *                            MIDI CLOCK LISTENER                           *
 *                                                                          *
 ****************************************************************************/

/** 
 * MidiClockListener interface method, called indirectly by the MidiTimer
 * burried under MidiInterface.  We are considered to be in an "interrupt" 
 * here, so be very careful about what you do.
 *
 * There are four callbacks for clock, start, stop, and continue events.
 * Note that MidiTimer will give us a clock event immediately after
 * sending start or continue which is what the MIDI spec requires.  
 * MidiQueue knows to merge those when converting them to Events.
 */
PUBLIC void MidiTransport::midiClockEvent()
{
    // After posting transport events like MS_START or MS_CONTINUE 
    // we normally want to ignore the few remaining clock pulses that 
    // might be sent before the transport event is sent
	if (mIgnoreClocks) return;

	long now = mMidi->getMilliseconds();
	mQueue.add(MS_CLOCK, now);
}

/**
 * Called indirectly by MidiTimer when it sends an MS_START event.
 * Note that MidiTimer will immediately call midiClockEvent since
 * it must send a clock after every START or CONTINUE
 */
PUBLIC void MidiTransport::midiStartEvent()
{
    if (mImmediateTransportQueue) {
        // we queued the events in start() don't need to do them here
        // since it has worked it's way through the machinery 
        // we can stop ignoring clocks
        mIgnoreClocks = false;
    }
    else {
		long now = mMidi->getMilliseconds();
		mQueue.add(MS_START, now);
    }
}

/**
 * Called indirectly by MidiTimer when it sends an MS_CONTINUE event.
 */
PUBLIC void MidiTransport::midiContinueEvent()
{
    if (mImmediateTransportQueue) {
        // we queued the events in midiContinue() don't need to do them here
        // since it has worked it's way through the machinery 
        // we can stop ignoring clocks
        mIgnoreClocks = false;
    }
    else {
		long now = mMidi->getMilliseconds();
		mQueue.add(MS_CONTINUE, now);
	}
}

/**
 * Called indirectly by MidiTimer when it sends an MS_STOP event.
 */
PUBLIC void MidiTransport::midiStopEvent()
{
    if (mImmediateTransportQueue) {
        // we queued the event in stop() don't need to do it here
    }
    else {
		long now = mMidi->getMilliseconds();
		mQueue.add(MS_STOP, now);
	}
}

/****************************************************************************
 *                                                                          *
 *                             TRANSPORT COMMANDS                           *
 *                                                                          *
 ****************************************************************************/

/**
 * Changes the the output tempo.
 */
PUBLIC void MidiTransport::setTempo(TraceContext* context, float tempo)
{
	if (tempo < 0) {
		Trace(context, 1, "MidiTransport: Invalid negative tempo!\n");
	}
	else if (tempo == 0.0f) {
		// should we ignore this?
		Trace(context, 1, "MidiTransport: Tempo changed from %ld (x100) to zero, sync disabled\n",
			  (long)(mTempo * 100));
		mTempo = 0;
	}
	else {
		float clocksPerSecond = (tempo / 60.0f) * 24.0f;
		float framesPerClock = mSampleRate / clocksPerSecond;
		float millisPerClock = 1000.0f / clocksPerSecond;
		Trace(context, 2, 
			  "MidiTransport: tempo changed from %ld to %ld (x100) millis/clock %ld frames/clock %ld\n",
			  (long)(mTempo * 100),
			  (long)(tempo * 100),
			  (long)millisPerClock,
			  (long)framesPerClock);

		mTempo = tempo;
		mMidi->setOutputTempo(mTempo);
	}
}

/**
 * Begin sending clocks to the MIDI output device.
 * Call this only for the master track after the tempo has been calculated.
 * This should only called when SyncMode=OutUserStart.
 */
PUBLIC void MidiTransport::startClocks(TraceContext* c)
{
	if (!mSending && mTempo > 0) {

		Trace(c, 2, "MidiTransport: Starting MIDI clocks, tempo (x100) %ld\n",
			  (long)(mTempo * 100));

		mMidi->startClocks(mTempo);
		mSending = true;
	}
}

/**
 * Send a MIDI Start message and start clocks.
 * 
 * It is mandatory that the MidiInterface implementation reset
 * the internal millisecond counters that determine when the next
 * clock will be sent, so we get a full pulse width after the start event.
 */
PUBLIC void MidiTransport::start(TraceContext* c)
{
	Trace(c, 2, "MidiTransport: Sending MIDI Start, tempo (x100) %ld\n",
		  (long)(mTempo * 100));

	// This will send MS_START followed by MS_CLOCK, and enable clocks.
	// Since clocks are automatically enabled be sure to set the tempo
	// in case it changed while we were stopped.
	mMidi->setOutputTempo(mTempo);
	mMidi->midiStart();

	mSending = true;
	mStarts++;

	if (mImmediateTransportQueue) {
        // don't wait for timer callbacks, queue them now so we can
        // see them within this interrupt
        // UPDATE: actually this is broken now since we calculate the
        // event list at the beginning of the interrupt we won't get these
        // events until the next interrupt.

		long now =  mMidi->getMilliseconds();
		mQueue.add(MS_START, now);
		mQueue.add(MS_CLOCK, now);

        // ignore residual clocks until the event makes it's way through
        // this probably won't work either since we may have already
        // consumed one when we converted the event list
        // Why was this thought to be important?  
        // !! Small race condition here where the timer callback can
        // add another clock.  I'm really not liking this...
        // !! this is why the old queue had a critial section
        mIgnoreClocks = true;

        // OLD NOTES
		// NB: These need to be processed in the current interrupt
		// or else we'll have an immediate 256 frame dealignment that
		// may cause a drift adjust on the first playback.  
		// Do this by bumping the iterator head to include the events
		// we just inserted.
        // 
		// Tracks processed before the new master track won't see this.
		// I think this is ok since the only time mQueue should be used
        // is for the track symc master and the others can't be slaving?
		//mQueue.interruptStart(mInterruptMsec);
	}
}

/**
 * Send a MIDI stop event and optionally stop clocks.
 * Call this only for the out sync master track.
 */
PUBLIC void MidiTransport::stop(TraceContext* c, bool sendStop, 
                                bool stopClocks)
{
    if (sendStop) {
        if (stopClocks)
          Trace(c, 2, "MidiTransport: Sending MIDI Stop and stopping clocks\n");
        else
          Trace(c, 2, "MidiTransport: Sending MIDI Stop\n");

        // the event is actually sent on the next timer interupt,
        // but it will *not* call the clock listener
        mMidi->midiStop(stopClocks);

        // this resets after a stop event
        mStarts = 0;

        if (mImmediateTransportQueue) {
            // post a STOP to get the queue in the right state
            mQueue.add(MS_STOP, mMidi->getMilliseconds());
        }

    }
    else if (stopClocks) {
        Trace(c, 2, "MidiTransport: Stopping MIDI clocks\n");
        mMidi->stopClocks();
    }

	if (stopClocks)
      mSending = false;
}

/**
 * A stop variant that traces and turns of clock ignore.
 * This little pattern was used in a few places in Synchronizer, 
 * not sure if it is necessary but preserve it.
 */
PUBLIC void MidiTransport::fullStop(TraceContext* c, const char* msg)
{
    if (mSending) {
        Trace(c, 2, msg);
        stop(c, true, true);
    }

    mIgnoreClocks = false;
}

PUBLIC void MidiTransport::midiContinue(TraceContext* c)
{
	Trace(c, 2, "MidiTransport: Sending MIDI Continue\n");
		
	// whis will send MS_CONTINUE followed by MS_CLOCK and restart clocks
	// Since clocks are automatically enabled be sure to set the tempo
	// in case it changed while we were stopped.
	mMidi->setOutputTempo(mTempo);
	mMidi->midiContinue();

	mSending = true;
	// hmm, treat this like a start for now
	mStarts++;
    
    if (mImmediateTransportQueue) {
        // add events immediately to the queue
		// have the same issues as start() where the master track
		// may need to process the mQueue events in the same interrupt or
		// else there will be a dealign

		long now =  mMidi->getMilliseconds();
		mQueue.add(MS_CONTINUE, now);
		mQueue.add(MS_CLOCK, now);

        // ignore residual clocks until MS_CONTINUE works it's way
        // through the timer
        mIgnoreClocks = true;

        // !! this doesn't work any more, figure out how to add
        // Events to the list
		//mQueue.interruptStart(mInterruptMsec);
	}
}

/**
 * Used in one place by Synchronizer::restartSyncOut to:
 * "The unit tests want to verify that we at least tried
 * to send a start event.  If we suppressed one because we're
 * already there, still increment the start count."
 *
 */
PUBLIC void MidiTransport::incStarts()
{
    mStarts++;
}

/****************************************************************************
 *                                                                          *
 *                                   STATUS                                 *
 *                                                                          *
 ****************************************************************************/

/**
 * For variable syncOutTempo.
 */
PUBLIC float MidiTransport::getTempo()
{
	return mTempo;
}

/**
 * For variable syncOutRawBeat.
 * 
 * The current raw beat count maintained by the internal clock.
 * This will be zero if the internal clock is not running.
 */
PUBLIC int MidiTransport::getRawBeat()
{
	MidiState* s = mQueue.getMidiState();
	return s->beat;
}

/**
 * For variable syncOutBeat.
 * The current beat count maintained by the internal clock relative 
 * to the bar.
 *
 * beatsPerBar will be taken from the recordBeats or subcycles
 * parameters of the track preset.
 */
PUBLIC int MidiTransport::getBeat(int beatsPerBar)
{
	MidiState* s = mQueue.getMidiState();
	int beat = s->beat;

	if (beatsPerBar > 0)
	  beat = beat % beatsPerBar;

	return beat;
}

/**
 * For variable syncOutBar.
 * The current bar count maintained by the internal clock.
 * This is calculated from the raw beat count, modified by the
 * effective beatsPerBar.
 * 
 * beatsPerBar will be taken from the recordBeats or subcycles
 * parameters of the track preset.
 */
PUBLIC int MidiTransport::getBar(int beatsPerBar)
{
	MidiState* s = mQueue.getMidiState();
	int beat = s->beat;

	if (beatsPerBar > 0)
	  beat = beat / beatsPerBar;

	return beat;
}

/**
 * For variable syncOutSending.
 * Return true if we're sending clocks.
 */
PUBLIC bool MidiTransport::isSending()
{
	return mSending;
}

/**
 * For variable syncOutStarted.
 * Return true if we've sent the MIDI Start event and are sending clocks.
 */
PUBLIC bool MidiTransport::isStarted()
{
	return (mStarts > 0);
}

/**
 * For variable syncOutStarts.
 * Return the number of Start messages sent since the last stop.
 * Used by unit tests to verify that we're sending start messages.
 */
PUBLIC int MidiTransport::getStarts()
{
	return mStarts;
}

/**
 * For Synchronizer::getMidiSongClock, not exposed as a variable.
 * Used only for trace messages.
 * Be sure to return the ITERATOR clock, not the global one that hasn't
 * been incremented yet.
 */
PUBLIC int MidiTransport::getSongClock()
{
	MidiState* s = mQueue.getMidiState();
	return s->songClock;
}

/****************************************************************************
 *                                                                          *
 *                              AUDIO INTERRUPT                             *
 *                                                                          *
 ****************************************************************************/

/**
 * Called at the beginning of each audio interrupt to prepare the MidiQueue.
 */
PUBLIC void MidiTransport::interruptStart(long millisecond)
{
    // remember for a few adjusments
    mInterruptMsec = millisecond;
    mQueue.interruptStart(millisecond);
}

/**
 * Convert events from the internal MIDI queue.
 * The queue is updated as we send MIDI events to the output port.
 * Generates start/stop/continue/clockPulse/barPulse events
 */
PUBLIC Event* MidiTransport::getEvents(EventPool* pool, long interruptFrames)
{
    return mQueue.getEvents(pool, interruptFrames);
}

PUBLIC bool MidiTransport::hasEvents()
{
    return mQueue.hasEvents();
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
