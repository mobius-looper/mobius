/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Multi track sequencer/recorder.
 * 
 * Interrupt handlers for the MIDI and Timer devices.
 * There are also some of the SeqRecording methods in here related
 * to the capture of events as they come in through the interrupts.
 *
 */

#include <stdio.h>
#include <stdlib.h>

#include "util.h"
#include "midi.h"
#include "mmdev.h"
#include "smf.h"

#include "seq.h"
#include "track.h"
#include "recording.h"

/****************************************************************************
 *                                                                          *
 *   							TIMER HANDLER                               *
 *                                                                          *
 ****************************************************************************/

/****************************************************************************
 * seq_timer_callback
 *
 * Arguments:
 *	   t: timer object
 *	args: args set when timer was initialized
 *
 * Returns: next clock of interest
 *
 * Description: 
 *
 * Timer callback set in the Timer device we allocate to manage the
 * high resolution timer device.  THIS IS AN INTERRUPT HANDLER!
 *
 * We adjust our internal state, and then call other callbacks
 * that may be specified for the Sequencer.
 *
 * This used to always be called via the message loop, but now that we're
 * in an interrupt, be careful.
 * 
 * Handling of the record loop is sort of kludgey, we get the time
 * for it in getNextSweepTime, but then we process it out here.  Should
 * be part of the sweep?
 * 
 ****************************************************************************/

INTERFACE void seq_timer_callback(Timer *t, void *args)
{
	Sequencer *s = (Sequencer *)args;

	s->timerCallback();
}

PRIVATE void Sequencer::timerCallback(void)
{
	int now;

	// pay attention to some control flags
	// if we're not running, shouldn't even be here
	if (!running || pending_stop)
	  return;

	// stop the timer if we want to debug the track sweep
	if (debug_track_sweep)
	  timer->setInterruptEnable(0);

	// need to get the actual running clock, is this accurate? 
	now = timer->getClock();

	// Check for editing loops, if one has been set and we've hit the end, 
	// stop and loop back to the start.
	// This is a rather expensive way to loop...

	if (recording && loop_end_enable && now >= loop_end) {

		// stop, but don't call command callback
		stopInternal(0);

		// call edit loop callback
		if (mCallbackloop != NULL)
		  (*mCallbackloop)(this, NULL, recording->getNewEvents());

		if (mEventMask & SeqEventLoop)
		  addEvent(SeqEventLoop, loop_end, 0, 0);

		// This is a rather expensive way to loop, think about saving
		// the loopback state.

		setClock(loop_start);
		startInternal(0);
	}
	else {

		// see if we've hit a beat boundary
		if (now >= next_beat_clock) {

			int beatclock = next_beat_clock;

			// advance the metronome
			// for now, metronome always goes to port 0
			metronome->advance(_outputs[_defaultOutput]);
			next_beat_clock += timer->getResolution();

			// If there is a beat spy, call it and let it stop the clock 
			// if it returns a non-zero value.  Do this before kicking
			// the metronome ? 

			if (mCallbackbeat != NULL) {
				if ((*mCallbackbeat)(this)) {
					stop();
					goto done;
				}
			}

			if (mEventMask & SeqEventBeat)
			  addEvent(SeqEventBeat, beatclock, 0, 0);
		}

		// see if the tracks have something to say
		if (now >= next_sweep_clock) {

			sweeping = 1;
			next_sweep_clock = sweepTracks(now);
			sweeping = 0;

			if (debug_track_sweep)
			  timer->setInterruptEnable(1);
		}
 
		// Still going, see if there is a specific end clock set or if
		// a deferred stop was requested.

		if (pending_stop || (now >= end_clock && end_enable))
		  stop();
		else {
			// We're continuing on to the next event, calculate the wait time
			// and re-arm the timer.  Note that since we always deal with
			// absolute times rather than relative time delays, we don't have
			// to worry about the time spent in this function when scheduling
			// the next event time.  If we've already gone past the time of
			// the next event, then we'll get signalled right away by the
			// timer.

			int nextclock = next_sweep_clock;
			if (next_beat_clock < nextclock)
			  nextclock = next_beat_clock;

			timer->setNextSignalClock(nextclock);
		}
	}

	// turn the timer back on 
  done:
	if (debug_track_sweep)
	  timer->setInterruptEnable(1);
}

/****************************************************************************
 *                                                                          *
 *   						  MIDI INPUT HANDLER                            *
 *                                                                          *
 ****************************************************************************/

/****************************************************************************
 * seq_midi_in_callback
 *
 * Arguments:
 *	  in: input object
 *	args: the Sequencer object
 *
 * Returns: none
 *
 * Description: 
 * 
 * Called whenever the MidiIn device receives something on the input
 * port.  THIS IS AN INTERRUPT HANDLER!
 * 
 * As notes come in, they are stored on a temporary list in the
 * SeqRecording object for later merger into the target sequence.
 *
 ****************************************************************************/

INTERFACE void seq_midi_in_callback(MidiIn *in, void *args)
{
	Sequencer *s = (Sequencer *)args;
	s->midiInCallback(in);
}

PRIVATE void Sequencer::midiInCallback(MidiIn *input)
{
	MidiEvent	*newevents, *e, *nexte;
	SeqTrack 	*track;
	int 		now, clock;

	// Formerly called ignoreSysex here, but that screwed up
	// the new sysex request interface.
	// Let it linger.
	// !! This will suck if sysex happens to come in during recording
	// or when we're not actively involved in a transfer.

	if (recording == NULL) {

		// Ignore it if we're not set up for recording, in this
		// case the input device should also be disabled so we 
		// don't get here...
		// UPDATE: Now that we always go through the sequencer from
		// the Java apps, we might have the input enabled and want
		// to process certain interesting events

		// call the generic packet spy if defined
		if (mCallbackevent == NULL && mListener == NULL)
		  input->ignoreEvents();
		else {
			newevents = input->getEvents();
			if (newevents != NULL) {
				if (mCallbackevent != NULL)
				  (*mCallbackevent)(this, newevents);
				else {
					mListener->MIDIEvent(newevents);
					newevents->free();
				}
			}
		}
	}
	else if (!recording->isRecording()) {

		// There is an installed recording but we're not actively recording
		// anything, notify the callback.

		newevents = input->getEvents();

		if (newevents != NULL) {

			// call the generic event spy if defined
			if (mCallbackevent != NULL)
			  (*mCallbackevent)(this, newevents);
		  
			if (mEventMask & SeqEventRecordNoteOn) {
				// add ON or OFF events for every note in the list
			}

			// the events are now ours, do we need a way to indicate that
			// these become owned by the mCallbackevent ? 

			newevents->free();
		}
	}
	else {
		// Here, we're actively recording, get the current time and normalize
		// it relative to loops in this recording.

		newevents  = input->getEvents();
		now		= timer->getClock();
		track 	= recording->getTrack();
		clock 	= now - track->getLoopAdjust();

		// remember the fact that we saw some events
		recording->setNewEvents(1);

		// hack, shouldn't be necessary any more, but prevents 
		// runaway interrupts
		{
			int i;
			for (i = 0, e = newevents ; e != NULL && i < 100 ; 
				 i++, e = e->getNext());
			if (i >= 100) {
				module->getEnv()->error("input list loop !\n");
				newevents = NULL;
			}
		}

		for (e = newevents, nexte = NULL ; e != NULL ; e = nexte) {
			nexte = e->getNext();
			e->setNext(NULL);

			// adjust the clock
			e->setClock(clock);

			switch (e->getStatus()) {

				case MS_NOTEON:
					if (e->getVelocity())
					  recording->recordNoteOn(e, now);
					else
					  recording->recordNoteOff(e, now);
					break;

				case MS_NOTEOFF:
					recording->recordNoteOff(e, now);
					break;

				case MS_PITCHBEND:
					recording->recordPitch(e, now);
					break;

				case MS_TOUCH:
					recording->recordTouch(e, now);
					break;

				case MS_CONTROL:
					recording->recordControl(e, now);
					break;

				default:
					// MS_PROG, others
					// nothing special, just add it to the list
					recording->recordMisc(e, now);
					break;
			}
		}
	}
}


/****************************************************************************
 *                                                                          *
 *   						  RECORDING METHODS                             *
 *                                                                          *
 ****************************************************************************/

/****************************************************************************
 * SeqRecording::popRecordNote
 *
 * Arguments:
 *	channel: event channel
 *	    key: event key
 *
 * Returns: note we popped (if any)
 *
 * Description: 
 * 
 * Looks for a note on the recording's "on" list and if found, 
 * removes it.  Normally called as MIDI note off events come in but 
 * also called ot resolve hung notes.
 * 
 * NOTE:
 * Formerly both the channel & the key had to match, now the loop
 * ignores the channel.  This isn't really a problem since the current
 * hardware setup will only result in MIDI input from one channel at a time.
 * Still, I think this should be allowed here.  Perhaps it had something
 * to do with the mapper?   Unfortunately, the ignoring of channels is
 * several functions deep at this point to it does no good to
 * enable it here.
 * 
 ****************************************************************************/

PRIVATE MidiEvent *SeqRecording::popRecordNote(int channel, int key)
{
	MidiEvent *e, *prev, *next;
  
	prev = NULL;
	next = NULL;
	e    = on;

	while ((e != NULL) && (e->getKey() != key)) {
		prev = e;
		e = e->getStack();
	}  

	if (e != NULL) {
		next = e->getStack();
		e->setStack(NULL);
	}

	if (prev != NULL)
	  prev->setStack(next);
	else
	  on = next;

	// If mute was on and the "on" list is now empty, unmute the track.
	// What the hell does this do ? 
	// something similar in flushHangingNotes

	if (mute && on == NULL && !sequencer->getPunchInEnable()) {
		rectrack->setMute(0);
		mute = 0;
	}

	return e;
}

/****************************************************************************
 * SeqRecording::checkHang
 *
 * Arguments:
 *	key: note key
 *	now: absolute clock
 *
 * Returns: none
 *
 * Description: 
 * 
 * Called by recordNoteOn when a note event comes in.  
 * We check to see if there is already an "on" event queued
 * on this key, if there is we somehow missed a note "off" event for this
 * key since no MIDI device can send the same note twice.  
 *
 * Think about this, it is certainly possible for a device to receive
 * the same note several times.  Presumably some new non-keyboard oriented
 * device could also send notes multiple times assuming they stack up and
 * send the corresponding number of note off events.
 *
 * For the current hardware, this does however represent a hang.
 * Since this may be indicative of a problem, display a little message
 * when this happens.
 * 
 * HMM, not sure I like this, things like seq-303 can easilly be set up
 * to send the note more than once, this can also be usefull to get 
 * flangy effects.  I'm leaving this out...
 * 
 ****************************************************************************/

PRIVATE void SeqRecording::checkHang(int key, int now)
{
	MidiEvent *e, *next;
	int stop = 0;

	for (e = on ; e != NULL && !stop ; e = next) {
		next = e->getStack();

		if (e->getKey() == key) {

			MidiEvent *pop;

			getEnv()->error("Hanging record note\n");
			pop = popRecordNote(0, key);
			if (pop != NULL)
			  pop->setDuration(now - (int)pop->getData());

			// assuming we call check_hang when we're supposed to, we can
			// never have more than one note queued up for this key and so
			// we can stop the loop now.
			stop = 1;
		}
	}
}

/****************************************************************************
 * SeqRecording::recordNoteOn
 *
 * Arguments:
 *	    e: event we just got
 *	  now: the absolute clock
 *	clock: event clock adjusted for loops
 *
 * Returns: none
 *
 * Description: 
 * 
 * Called by midiInCallback when it finds a MS_NOTEON event with a non-zero
 * velocity.  We add it to the recorder's event list, and also temporarily
 * push it on the "on" list so that we can match it up later when the
 * corresponding MS_NOTEOFF event comes in.
 * 
 ****************************************************************************/

PRIVATE void SeqRecording::recordNoteOn(MidiEvent *e, int now)
{

	// add to the recording event list
	if (events == NULL)
	  events = e;
	else
	  last_event->setNext(e);
	last_event = e;

	// if we're recording drums, don't bother to queue the event
	if (drum_mode) {
		if (mCallbackrecord != NULL)
		  (*mCallbackrecord)(sequencer, e);

		if (mEventMask & SeqEventRecordNoteOn) {
			// add this event, but need to start start/end to the same time
		}

	}
	else {
		//  these aren't drum events, queue then on the "on" list

		// see commentary above for why we don't do this any more
		// checkHang(e->getKey(), now);

		// save absolute time for on events, and stack it
		e->setData((char *)now);
		e->setStack(on);
		on = e;

		// mute the track if we're not in "merge" mode
		// not sure what the hell this does...

		if (!mute && !sequencer->getRecordMerge()) {
			rectrack->setMute(1);
			mute = 1;
		}
	}
}

/****************************************************************************
 * SeqRecording::recordNoteOff
 *
 * Arguments:
 *	e: note off event
 *
 * Returns: none
 *
 * Description: 
 * 
 * Deal with a note off event.
 * We don't actually save this, instead we match this up with a previously
 * encountered note ON event, which will have been pushed on the "on"
 * list in the recorder. 
 ****************************************************************************/

PRIVATE void SeqRecording::recordNoteOff(MidiEvent *e, int now)
{
	MidiEvent *pop;

	if (!drum_mode) {
		pop = popRecordNote(0, e->getKey());
		if (pop != NULL) {

			// adjust the absolute "on" time
			pop->setDuration(now - (int)(pop->getData()));

			if (mCallbackrecord != NULL)
			  (*mCallbackrecord)(sequencer, pop);

			if (mEventMask & SeqEventRecordNoteOn) {
				// add the record event, 
				// both on & off, or just on with duration?
			}

		}
		else {
			// couldn't find queued note for NOTEOFF event, odd	
			// can happen if we hit the panic button on the MSB+
			// getEnv()->debug("Unmatched NOTEOFF %d\n", e->getKey());
		}
	}

	// the actual off event is not used
	e->free();
}

/****************************************************************************
 * SeqRecording::recordPitch
 *
 * Arguments:
 *	  e: event
 *	now: current clock
 *
 * Returns: none
 *
 * Description: 
 * 
 * Called by midiInputCallback to deal with an incomming pitch bend event.
 * Check to see if the last controller event on this clock was also
 * a pitchbend, and if so, overwrite the last one with the new
 * value on the same clock.  This can only happen if controller events
 * are commin in very fast.  Hmm, is this really a problem?  
 ****************************************************************************/

PRIVATE void SeqRecording::recordPitch(MidiEvent *e, int now)
{

	// adjust the event fields, what the hell is this?
	e->setDuration(((e->getVelocity() << 7) | e->getKey()));

	// don't bother with the multiple event on clock weeding, can do that
	// later if it becomes a problem

	// add to the recording event list
	if (events == NULL)
	  events = e;
	else
	  last_event->setNext(e);
	last_event = e;

}                                      

/****************************************************************************
 * SeqRecording::recordTouch
 *
 * Arguments:
 *	e: event
 *
 * Returns: none
 *
 * Description: 
 * 
 * Called by midiInCallback to process an aftertouch event.
 * Like pitch events, we used to try to be smart about coalescing 
 * touch events on the same clock.
 ****************************************************************************/

PRIVATE void SeqRecording::recordTouch(MidiEvent *e, int now)
{

	// what the hell is this for?
	e->setDuration(e->getKey());

	// add to the recording event list
	if (events == NULL)
	  events = e;
	else
	  last_event->setNext(e);
	last_event = e;

}                                      

/****************************************************************************
 * SeqRecording::recordControl
 *
 * Arguments:
 *	e: event
 *
 * Returns: none
 *
 * Description: 
 * 
 * Called by midiInCallback to process a continuous controller event.
 * Like pitch events, we used to try to be smart about coalescing 
 * touch events on the same clock.
 * 
 ****************************************************************************/

PRIVATE void SeqRecording::recordControl(MidiEvent *e, int now)
{
	// UNUSED_PARAM(now);

	// what does this do?
	e->setDuration(e->getVelocity());

	// add to the recording event list
	if (events == NULL)
	  events = e;
	else
	  last_event->setNext(e);
	last_event = e;

}                                      

/****************************************************************************
 * SeqRecording::recordMisc
 *
 * Arguments:
 *	e: event
 *
 * Returns: none
 *
 * Description: 
 * 
 * Called by midiInCallback to process a program change or other
 * misc event.  We just append it to the list.
 * 
 ****************************************************************************/

PRIVATE void SeqRecording::recordMisc(MidiEvent *e, int now)
{
	// UNUSED_PARAM(now);

	if (events == NULL)
	  events = e;
	else
	  last_event->setNext(e);
	last_event = e;
}                                      

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
