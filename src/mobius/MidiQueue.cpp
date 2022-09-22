/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Classes used to accumulate MIDI realtime events and 
 * maintain transport status (started, stopped, beat, etc.)
 *
 * This was designed for use by the Mobius synchronizer but parts
 * of it are relatively general.  Could consider moving some of
 * it to the midi library.
 *
 * In between audio interrupts, MIDI events that are received
 * are placed in a ring buffer in the MidiQueue.
 *
 * During the audio interrupt Synchronizer will call MidiQueue.getEvents
 * to determine which queued events should be processed in this
 * interrupt and what they're buffer offsets will be.
 *
 * Event objects are returned  with one of these SyncType values:
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
 */

#include <stdlib.h>
#include <memory.h>
#include <stdarg.h>

#include "Port.h"
#include "Trace.h"
#include "Thread.h"

#include "MidiByte.h"
#include "MidiEvent.h"
#include "MidiQueue.h"

// we build an Event, would be nice to refactor so we don't have
// a Mobius dependency
#include "Event.h"

/****************************************************************************
 *                                                                          *
 *                                 CONSTANTS                                *
 *                                                                          *
 ****************************************************************************/

/**
 * This is the maximum number of milliseconds that can appear between
 * MS_CLOCK events before we consider that the clock stream has stopped.
 * Used in the determination of the MidiState.receivingClocks field, 
 * which is in turn exposed as the syncInReceiving script variable.
 *
 * Some BPM/clock ratios to consider:
 *
 * 60 bpm = 24 clocks/second
 * 15 bpm = 7 clocks/second
 * 7.5 bpm =  1.5 clocks/second
 * 1.875 bpm = .75 clocks/second
 *
 * If the clock rate drops below 10 bpm we should be able to consider
 * that "not receiving", for the purpose of the syncInReceiving variable.
 * 7.5 bpm is 666.66 milliseconds.
 *
 * Get thee behind me Satan!
 */
#define MAXIMUM_CLOCK_DISTANCE 666

/****************************************************************************
 *                                                                          *
 *   							  MIDI STATE                                *
 *                                                                          *
 ****************************************************************************/

PUBLIC MidiState::MidiState()
{
	name = "*unnamed*";

	lastClockMillisecond = 0;
	receivingClocks = false;

	songPosition = -1;
	songClock = 0;
	beatClock = 0;
	beat = 0;

	waitingStatus = 0;
	started = false;
}

PUBLIC MidiState::~MidiState()
{
}

/**
 * Called periodically to let the MidiState check the time since
 * the last clock event.  If the distance becomes too great,
 * we turn off the receivingClocks flag.
 *
 * This isn't actually used for anything besides the syncInReceiving
 * script variable used in the unit tests.
 */
void MidiState::tick(long currentClock)
{
	if (receivingClocks) {
		long delta = currentClock - lastClockMillisecond;
		if (delta > MAXIMUM_CLOCK_DISTANCE) {
			Trace(2, "MidiState %s stopped receiving clocks\n", name);
			receivingClocks = false;
		}
	}
}

/**
 * Consume one MIDI event and advance state.
 * Note that we don't maintin a running song position, the songPosition
 * field is just used to hold the last MS_SONGPOS value to use
 * if we continue.
 */
PUBLIC void MidiState::advance(MidiSyncEvent* e)
{
	int status = e->status;

	switch (status) {

		case MS_START: {
			// arm start event for the next clock
			waitingStatus = status;
			started = false;
			// this is also considered a "clock" for the purpose
			// of detecting start/stops in the stream
			lastClockMillisecond = e->clock;
		}
		break;

		case MS_STOP: {
			waitingStatus = 0;
			songPosition = -1;
			started = false;
		}
		break;
		
		case MS_CONTINUE: {
			// arm continue for the next clock
			waitingStatus = status;
			started = false;
			// this is also considered a "clock" for the purpose
			// of detecting start/stops in the stream
			lastClockMillisecond = e->clock;
		}
		break;

		case MS_SONGPOSITION: {
			// this isn't a running song position, we just remember the
			// last message for a later continue
			// ignore if we're already in a started state?
			songPosition = e->songpos;
		}
		break;

		case MS_CLOCK: {

			// Check for resurection of the clock stream for the
			// syncInReceiving variable.  If the clocks stop, this
			// will be detected in the tick() method below.

			long currentClock = e->clock;
			long delta = currentClock - lastClockMillisecond;
			lastClockMillisecond = currentClock;
			if (!receivingClocks && delta <  MAXIMUM_CLOCK_DISTANCE) {
				Trace(2, "MidiState %s started receiving clocks\n", name);
				receivingClocks = true;
			}

			// these can come in when the sequencer isn't running,
			// but continue counting so the loop can still run
			if (!started && waitingStatus == MS_CONTINUE) {
				// use songPosition if it was set, otherwise keep
				// going from where are, would it be better to 
				// assume starting from zero??
				if (songPosition >= 0)
				  songClock = songPosition * 6;
				songPosition = -1;
				beatClock = songClock % 24;
				beat = songClock / 24;
				started = true;
			}
			else if (!started && waitingStatus == MS_START) {
				songPosition = -1;
				songClock = 0;
				beatClock = 0;
				beat = 0;
				started = true;
			}
			else {
				// this only persists for the first clock, then it
				// must be cleared
				waitingStatus = 0;

				songClock++;
				beatClock++;
				if (beatClock >= 24) {
					beat++;
					beatClock = 0;
				}
			}
		}
		break;
	}
}

/****************************************************************************
 *                                                                          *
 *   								QUEUE                                   *
 *                                                                          *
 ****************************************************************************/

PUBLIC MidiQueue::MidiQueue()
{
    // MidiState self initializes
	mOverflows = 0;
	mHead = 0;
	mTail = 0;
	memset(&mEvents, 0, sizeof(mEvents));
}

PUBLIC MidiQueue::~MidiQueue()
{
}

PUBLIC MidiState* MidiQueue::getMidiState()
{
	return &mState;
}

/**
 * Set a name to disambiguate the MidiQueue and internal MidiState
 * when generating trace messages.  The name must be a string constant.
 */
void MidiQueue::setName(const char* name)
{
	mState.name = name;
}

/**
 * Add an event from the MIDI thread.
 * If we overflow, we'll start dropping events.
 */
PRIVATE void MidiQueue::add(MidiEvent* e)
{
	int status = e->getStatus();
	int next = mHead + 1;
	if (next >= MAX_SYNC_EVENTS)
	  next = 0;

	mEvents[mHead].status = status;
	mEvents[mHead].clock = e->getClock();
	if (status == MS_SONGPOSITION)
	  mEvents[mHead].songpos = e->getSongPosition();
	else
	  mEvents[mHead].songpos = 0;
	
	if (next != mTail)
	  mHead = next;
	else {
		// overflow, should only happen if the audio interrupt is stuck
		// don't emit anything since in this case, we're likely
		// to generate a LOT of messages
		mOverflows++;
	}
}

/**
 * Simplified interface to add a single clock.
 */
PUBLIC void MidiQueue::add(int status, long clock)
{
	int next = mHead + 1;
	if (next >= MAX_SYNC_EVENTS)
	  next = 0;

	mEvents[mHead].status = status;
	mEvents[mHead].clock = clock;
	mEvents[mHead].songpos = 0;
	
	if (next != mTail)
	  mHead = next;
	else {
		// overflow, should only happen if the audio interrupt is stuck
		// don't emit anything since in this case, we're likely
		// to generate a LOT of messages
		mOverflows++;
	}
}

/**
 * Called by Synchronizer at the beginning of a new audio interrupt.
 * Pass the current millisecond counter along to the MidiState so 
 * it can detect sudden clock stopages.
 */
PUBLIC void MidiQueue::interruptStart(long millisecond)
{
	mState.tick(millisecond);
}

/****************************************************************************
 *                                                                          *
 *                              EVENT CONVERSION                            *
 *                                                                          *
 ****************************************************************************/

/**
 * Convert the queue of MidiSyncEvents into a list of Events.
 * 
 * We may have several related events in the queue, such
 * as MS_START & MS_CLOCK that need to be processed together,
 * so keep going until we have an interesting combined event.
 *
 * !! In earlier releases we maintained a seperate MidiState object
 * that was reset for each track during an interrupt and advanced
 * incrementally so you could look at MidiState as the events were processed.
 * Now, the MidiState will be fully advanced up front during event conversion,
 * in theory this means that if the buffer contained both a START and
 * a STOP we would end up stopped even though for a brief period
 * a script might be expecting us to be started.  I really hope
 * this isn't important, if so we'll have to annotate the Events.
 *
 * !! We should try to offset these into the buffer based on when they
 * were received, but I think we need a finer resolution clock.  See
 * See Synchronizer::adjustEventFrame for an earlier attempt at this.
 * If we do adjust these we can only return ones that fit within
 * the given interruptFrames.  As it stands now, we process all of them
 * at the beginning of the buffer.
 */
PUBLIC Event* MidiQueue::getEvents(EventPool* pool, long interruptFrames)
{
    Event* events = NULL;
    Event* lastEvent = NULL;

    // in theory we should try to offset these into the buffer
    // based on when they were received...need a finer resolution clock
    long offset = 0;

	while (mTail != mHead) {

		MidiSyncEvent* e = &(mEvents[mTail]);
		mTail++;
		if (mTail >= MAX_SYNC_EVENTS)
		  mTail = 0;

		// advance the state tracker
		mState.advance(e);

        Event* newEvent = NULL;
		int status = e->status;

		if (status == MS_STOP) {
            newEvent = pool->newEvent();
            newEvent->fields.sync.eventType = SYNC_EVENT_STOP;
		}
		else if (status == MS_CLOCK) {
            newEvent = pool->newEvent();

			if (mState.waitingStatus == MS_CONTINUE) {
                newEvent->fields.sync.eventType = SYNC_EVENT_CONTINUE;
                newEvent->fields.sync.continuePulse = mState.songClock;
                // If we're exactly on a beat boundary, set the continue
                // pulse type so we can treat this as a beat pulse later
                if (mState.beatClock == 0)
                  newEvent->fields.sync.pulseType = SYNC_PULSE_BEAT;
			}
			else if (mState.waitingStatus == MS_START) {
                // by definition this is also a beat/bar boundary,  
                // Synchronizer will convert this to a bar pulse
                newEvent->fields.sync.eventType = SYNC_EVENT_START;
                newEvent->fields.sync.pulseType = SYNC_PULSE_BEAT;
			}
			else {
				// hmm, would like to detect UNIT_BAR here but currently
                // that can be different for each track, should we just
                // require one beatsPerBar in the Setup?
                newEvent->fields.sync.eventType = SYNC_EVENT_PULSE;
                if (mState.beatClock != 0)
                  newEvent->fields.sync.pulseType = SYNC_PULSE_CLOCK;
                else {
                    newEvent->fields.sync.pulseType = SYNC_PULSE_BEAT;
                    newEvent->fields.sync.beat = mState.beat;
                }
			}
		}

        if (newEvent != NULL) {
            newEvent->type = SyncEvent;
			// squirell this away for trace debugging
            newEvent->fields.sync.millisecond = e->clock;

            // TODO: set event->frame to buffer offset!1
            
            if (lastEvent == NULL)
              events = newEvent;
            else
              lastEvent->setNext(newEvent);

            lastEvent = newEvent;
        }
    }

	return events;
}

PUBLIC bool MidiQueue::hasEvents()
{
    return (mHead != mTail);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
