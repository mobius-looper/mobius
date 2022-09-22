/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * SeqRecording recording state for the sequencer.
 * 
 * A Sequencer object will allocate one internal SeqRecording object
 * to maintain state during recording.  Currently, there can only be
 * one active recording state, but we may want to think about having several
 * allocated for each channel?  
 * 
 * This object is not visible to the end user.
 */

#include <stdio.h>

#include "port.h"
#include "mmdev.h"
#include "smf.h"

#include "seq.h"
#include "recording.h"

/****************************************************************************
 *                                                                          *
 *   						  SEQUENCER METHODS                             *
 *                                                                          *
 ****************************************************************************/
/*
 * Primary interface for controlling the recording operations of the
 * sequencer.
 * 
 */

/****************************************************************************
 * ~SeqRecording
 *
 * Arguments:
 *
 * Returns: 
 *
 * Description: 
 * 
 * Destructor for a recording object.
 * 
 ****************************************************************************/

PUBLIC SeqRecording::~SeqRecording(void)
{

	// assume most of this stuff is gone?
	runtimeInit(1);
	
}


/****************************************************************************
 * Sequencer::startRecording
 *
 * Arguments:
 *	    tr: track to record
 *	direct: non-zero for "direct" recording
 *
 * Returns: none
 *
 * Description: 
 * 
 * Designates a track for recording.
 * 
 * If the "direct" flag is set, recording is made directly into the sequence
 * without buffering.  If the direct flag is not set, the recording is 
 * buffered and must be sent to the destination sequence through an explict
 * call to acceptRecording.
 * 
 * Currently only one sequence can be recorded at a time, which is rather
 * limiting.  
 * 
 ****************************************************************************/

INTERFACE void Sequencer::startRecording(SeqTrack *tr, int direct)
{
	MidiSequence *copy;
	SeqTrack *buffer;

	// can't do this while running
	stop();

	// throw away anything currently being recorded
	stopRecording();

	// Automatically set up echo for this sequence, should have
	// more control?  

	// Set up midi input, to echo through to this sequence's channel 
	// if echo is on.  Need more control?
	// Took this out, we'll handle it in the Sequencer's callback, might
	// make sense to do it at this lower level though.  
	// See Sequencer::echoEvents in seqint.cxx
	// We only support recording through port 0

	_inputs[_defaultInput]->connect();
	_inputs[_defaultInput]->enable();
#if 0
	input->setEchoDevice(output);
	input->setEchoChannel(seq->getChannel());
#endif

	// build a new recording state
	recording = new SeqRecording;
	if (recording != NULL) {

		// remember where we came from, and other things
		recording->setSequencer(this);
		recording->setCallbackRecord(callback_record);
		recording->setEventMask(mEventMask);

		if (direct) {
			// direct recording, install it
			recording->setTrack(tr);
		}
		else {
			// make a working buffer & track, and disable the source track
			MidiSequence *seq = tr->getSequence();
			copy = seq->copy();
			if (copy != NULL) {

				addSequence(copy);
				buffer = findTrack(copy);
				if (buffer != NULL) {

					recording->setTrack(buffer);
					recording->setDestTrack(tr);

					// disable the track that this sequence is installed in
					tr->setDisabled(1);
				}
			}
		}
	}
}

/****************************************************************************
 * Sequencer::stopRecording
 *
 * Arguments:
 *	none: 
 *
 * Returns: none
 *
 * Description: 
 * 
 * Throws away the current recording state if any.  
 ****************************************************************************/

INTERFACE void Sequencer::stopRecording(void)
{
	SeqTrack *tr, *buffer;
	MidiSequence *seq;

	stop();
	if (recording != NULL) {
		
		// stop echoing input
		// this isn't really necessary now that we don't set it up in
		// startRecording, but in case we ever do, reset it here
		_inputs[_defaultInput]->setEchoDevice(NULL);

		// empty out the rum-time event lists
		recording->runtimeInit(1);

		// if this is set, then we've been buffering
		tr = recording->getDestTrack();
		if (tr != NULL) {

			// discard the buffered sequence/track
			// this is a bit roundabout, since the track removal
			// operation starts with the sequence
			buffer = recording->getTrack();
			seq = buffer->getSequence();
			removeSequence(seq);
			delete seq;

			// enable the track controlling the sequence we were recording
			tr->setDisabled(0);
		}

		// toss recording state
		delete recording;
		recording = NULL;
	}
}

/****************************************************************************
 * Sequencer::isRecordingSequence
 *
 * Arguments:
 *
 * Returns: non-zero if the sequence is being recorded
 *
 * Description: 
 * 
 * Tests to see if the given sequence is also the one installed as
 * the record sequence.  It may or may not be buffered.
 ****************************************************************************/

INTERFACE int Sequencer::isRecordingSequence(MidiSequence *seq)
{
	int status;

	status = 
		seq != NULL &&
		recording != NULL && 

		// is it installed for direct recording
		((recording->getTrack() != NULL &&
		  recording->getTrack()->getSequence() == seq) ||

		 // is it installed for buffered recording
		 (recording->getDestTrack() != NULL &&
		  recording->getDestTrack()->getSequence() == seq));

	return status;
}

/****************************************************************************
 * Sequencer::acceptRecording
 *
 * Arguments:
 *
 * Returns: none
 *
 * Description: 
 * 
 * Called immediately after a buffered recording is stopped to copy the
 * new events into the destination sequence.  If the currently installed
 * record sequence was installed as a "direct" record, then this
 * function will have no effect.
 * 
 ****************************************************************************/

INTERFACE void Sequencer::acceptRecording(void)
{
	MidiSequence *src, *dest;

	if (recording != NULL) {

		src  = recording->getSequence();
		dest = recording->getDestSequence();

		if (src != NULL && dest != NULL) {

			// it was buffered, overwrite the destination
			// don't especially like the names here
			dest->clear();
			dest->clone(src);

			// should we now clear the src ? 

			// enable the track that holds the sequence we were recording
			// hmm, since we're still set up for recording, should
			// we do this?
#if 0
			tr = recording->getDestTrack();
			if (tr != NULL)
			  tr->enable();
#endif

		}
	}
}

/****************************************************************************
 * Sequencer::revertRecording
 *
 * Arguments:
 *	none: 
 *
 * Returns: none
 *
 * Description: 
 * 
 * Called after a buffered recording to throw away the last recording
 * and restore the recording buffer from the source.
 * If the sequence was installed as a "direct" record, this function
 * will have no effect.
 * 
 ****************************************************************************/

INTERFACE void Sequencer::revertRecording(void)
{
	MidiSequence *src, *dest;

	if (recording != NULL) {

		// must we stop here?
		stop();

		src = recording->getSequence();
		dest = recording->getDestSequence();

		if (src != NULL && dest != NULL) {

			// toss the buffered recording
			src->clear();

			// replace with the original sequence
			src->clone(dest);
		}
	}
}

/****************************************************************************
 * Sequencer::clearRecording
 *
 * Arguments:
 *	none: 
 *
 * Returns: none
 *
 * Description: 
 * 
 * Called to throw away whatever is in the recording buffer, but leave
 * things set up for further recording.
 * If this is a "direct" record, it will clear out the original track too.
 ****************************************************************************/

INTERFACE void Sequencer::clearRecording(void)
{
	MidiSequence *seq;

	if (recording != NULL) {

		// must we stop here ?
		stop();

		seq = recording->getSequence();
		if (seq != NULL) {
			seq->clear();
			// do we need to enable the track?
		}
	}
}

/****************************************************************************
 * Sequencer::setLoopStart
 *
 * Arguments:
 *
 * Returns: none
 *
 * Description: 
 * 
 * Set the beginning of the record loop, normally this is zero.
 * Note that this just sets the parameters in the Sequencer, it 
 * does NOT set loop events the sequence itself, which is the way
 * you normally setup play loops.
 * 
 * When set, this will cause the recorder to loop continuously
 * between the start and end times.  On each loop, the loop_callback function
 * will be called.  Normally this will do something with the new events
 * that have been added during the last iteration such as calling
 * one of the merge methods
 * 
 * To disable the edit loop, set both the start and end values to zero.
 *
 * Note that the specified loop DOES NOT REMAIN in the resulting
 * sequence.  Furthermore, any "persistent" loops stored in the sequence
 * are disabled while recording or punch-in is in progress.
 *
 ****************************************************************************/

INTERFACE void Sequencer::setLoopStart(int c)
{
	stop();
	loop_start = c;
}

INTERFACE void Sequencer::setLoopEnd(int c)
{
	stop();
	loop_end = c;
}

/****************************************************************************
 *                                                                          *
 *   					   RECORDING OBJECT METHODS                         *
 *                                                                          *
 ****************************************************************************/

/****************************************************************************
 * SeqRecording::runtimeInit
 *
 * Arguments:
 *	flush: non-zero if we're really stopping
 *
 * Returns: none
 *
 * Description: 
 * 
 * Called when recording stops or a loop is taken, and some operation
 * has been performed on the accumulated event list.
 *
 * If the "flush" argument is non-zero, fields related to the dangling
 * "on" events are initialized too.  This is relavent only when loops 
 * are used during recording.  When loops are active, we don't flush
 * the "on" events even though we've merged all the others.
 * 
 * Hmm, shouldn't the "events" and "on" list be NULL'd after they've
 * been processed elsewhere?
 ****************************************************************************/

PRIVATE void SeqRecording::runtimeInit(int flush)
{
	// remove any dangling events, should really be gone by now
	if (events != NULL)
	  events->free();

	events		= NULL;
	last_event	= NULL;

	if (flush) {
		recording = 0;
		flushHangingNotes();
		flushCommands(1);
	}
}

/****************************************************************************
 * SeqRecording::flushHangingNotes
 *
 * Arguments:
 *	none: 
 *
 * Returns: none
 *
 * Description: 
 * 
 * Called from several places to force off any note events that
 * are queued in the "on" list.  Since we have to give the dangling
 * notes a termination time, we call timer->getClock() to get the
 * current time.  Note that this may not be correct, it means that
 * this should only be called if the track is being actively recorded
 * and the clock is either stopped or very close to the actual end point.
 * 
 ****************************************************************************/

PRIVATE void SeqRecording::flushHangingNotes(void)
{
	MidiEvent *ev, *next;
	int now;

	now = sequencer->getClock();

	for (ev = on ; ev != NULL ; ev = next) {
		next = ev->getStack();
		ev->setDuration(now - (int)ev->getData());
	}
	on = NULL;

	// What the hell does this do? 
	// See notes in popRecordNote.

	if (mute && !sequencer->getPunchInEnable()) {
		rectrack->setMute(0);
		mute = 0;
	}
}

/****************************************************************************
 * SeqRecording::start
 *
 * Arguments:
 *	clock: clock we're starting on
 *
 * Returns: none
 *
 * Description: 
 * 
 * Called by Sequencer::startInternal to prepare the recording object.
 ****************************************************************************/

PUBLIC void SeqRecording::start(int clock)
{
	SeqMetronome *metro;

	// ignore if we have no sequence or if we're temporarily disabled
	if (rectrack != NULL) {

		// should have been done by now?
		runtimeInit(1);

		// make sure these are off
		mute = 0;
		new_events_flag = 0;

		// once used a global drum map to determine if we were recording
		// drums and set the "drums" field, not sure how that would
		// get set now
		drum_mode = 0;

		// Set the recording track channel from the destination channel
		// if one was defined.  This allows us to specify the recording channel
		// by updating the channel in the destination track rather than having
		// another method for this.

		if (desttrack != NULL)
		  rectrack->setChannel(desttrack->getChannel());

		// unmute the recording track
		rectrack->setMute(0);

		if (!sequencer->getPunchInEnable()) {
			// don't wait, we're on
			recording = 1;
		}
		else if (sequencer->getPunchIn() >= clock) {
			// start only if we're within the punch in zone
			// this looks funny
			recording = 1;
			mute = 1;
			rectrack->setMute(1);
		}
		
		// send a metronome noise
		metro = sequencer->getMetronome();
		if (metro != NULL)
		  metro->sendRecord(sequencer->getOutput(0));
	}
}

/****************************************************************************
 * SeqRecording::stop
 *
 * Arguments:
 *
 * Returns: non-zero if any events entered
 *
 * Description: 
 * 
 * Called by Sequencer::stopInternal when the sequencer is stopped, and
 * we had a recording in progress.
 * Also called by SeqRecording::sweep when a loop is taken.
 * 
 * Merge the events collected during the recording with the destination
 * sequence.  
 *
 ****************************************************************************/

PUBLIC int SeqRecording::stop(void)
{
	int neu;

	recording = 0;

	neu = new_events_flag;
	if (neu) {

		// merge in different ways, depending on option flag settings
		if (sequencer->getPunchInEnable())
		  mergePunch();

		else if (sequencer->getRecordMerge())
		  mergeNormal(1);	// non-zero indicates flush hanging notes
		
		else
		  mergeDynaPunch(1);

		new_events_flag = 0;
	}

	runtimeInit(1);

	return neu;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
