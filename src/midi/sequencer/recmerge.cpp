/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * SeqRecord methods related to merging during recording.
 * There are currently three styles of merger supported:
 *
 * 		- normal
 * 		- absolute punch
 * 		- dynamic punch		
 *
 */

#include <stdio.h>

#include "port.h"
#include "mmdev.h"
#include "smf.h"

#include "seq.h"
#include "recording.h"
#include "track.h"

/****************************************************************************
 *                                                                          *
 *   							COMMAND EVENTS                              *
 *                                                                          *
 ****************************************************************************/
/*
 * The SeqRecording maintains a list of "command" events.  These are inserted
 * at various points during recording and processed when the recording
 * stops or when a loop is taken.
 *
 * Currently, there is only one type of command event, MS_CMD_ERASE.
 *
 * The commands are inserted into the recording through an explicit
 * call to one of the functions provided rather than from an incomming
 * MidiEvent.
 *
 * The main thing this provides is a way to "punch" in over a section and
 * erase notes.  
 *
 */

/****************************************************************************
 * SeqRecording::addCommand
 *
 * Arguments:
 *	e: event to add
 *
 * Returns: none
 *
 * Description: 
 * 
 * Adds a command event to the list in the recording object.
 ****************************************************************************/

PRIVATE void SeqRecording::addCommand(MidiEvent *e)
{
	if (commands == NULL)
	  commands = e;
	else
	  last_command->setNext(e);

	last_command = e;
	new_events_flag = 1;
}

/****************************************************************************
 * Sequencer::startRecordErase
 *
 * Arguments:
 *	none: 
 *
 * Returns: none
 *
 * Description: 
 * 
 * Begin the definition of an MS_CMD_ERASE command event on the track
 * currently being recorded.  If the recorder is not installed or
 * if there is no track being recorded in the recorder, the function
 * is ignored.
 * 
 * This function call should be followed by a call to stopRecordErase.
 * If it isn't, then the duration will be left zero, and the command
 * will be ignored when the recording is compiled.
 * 
 ****************************************************************************/

INTERFACE void Sequencer::startRecordErase(void)
{
	// ignore if we're not recording
	if (running && recording != NULL)
	  recording->startRecordErase();
}

PUBLIC void SeqRecording::startRecordErase(void)
{
	MidiEvent 	*event;
	int			clock;
	
	// build an erase event
	clock = sequencer->getClock() - rectrack->getLoopAdjust();
	event = sequencer->getMidiModule()->newEvent(0, 0, MS_CMD_ERASE, 0);
	event->setClock(clock);

	// add the event to the commands list
	addCommand(event);

	// mute the recording sequence
	if (!rectrack->isMute())
	  rectrack->setMute(1);
}

/****************************************************************************
 * Sequencer::stopRecordErase
 *
 * Arguments:
 *	none: 
 *
 * Returns: none
 *
 * Description: 
 * 
 * Stops the definition of an MS_CMD_ERASE event on the track being
 * recorded.  Must have been an call to startRecordErase prior to this.
 ****************************************************************************/

INTERFACE void Sequencer::stopRecordErase(void)
{
	if (running && recording != NULL)
	  recording->stopRecordErase();
}

PUBLIC void SeqRecording::stopRecordErase(void)
{
	MidiEvent *e;

	e = last_command;
	if (e != NULL) {
		e->setDuration(sequencer->getClock() - e->getClock());
		if (rectrack->isMute() && 
			(!sequencer->getPunchInEnable() || !recording))
		  rectrack->setMute(0);
	}
}

/****************************************************************************
 * SeqRecording::processCommands
 *
 * Arguments:
 *
 * Returns: none
 *
 * Description: 
 * 
 * Work function for the various merger methods.
 * We map over the events in the target sequence, applying the commands
 * in the command list.  
 * 
 * Currently the only operation is to erase ranges of events in the event
 * list as defined by MS_CMD_ERASE events in the command list.
 * The loopclock if present is used to "wrap" the end time of the
 * erasure back around to be relative to the loopstart time.  This
 * may result in the erasure of events that start BEFORE the start
 * time of the erasure.
 ****************************************************************************/

PRIVATE void SeqRecording::processCommands(void)
{
	MidiEvent *evlist,  *e, *prev, *next, *cmd;
	int end, loop_end, loop_start;

	loop_start = sequencer->getLoopStart();
	loop_end = sequencer->getLoopEnd();

	// get the events in the target sequence
	evlist = rectrack->getSequence()->stealEvents();

	e   = evlist;
	prev = NULL;

	// loop over each command in the command list
	for (cmd = commands ; cmd != NULL ; cmd = cmd->getNext()) {

		// currently, the only recognized command is MS_CMD_ERASE
		if (cmd->getKey() == MS_CMD_ERASE && cmd->getDuration() != 0) {

			// calculate the end point of the erasure
			end = cmd->getClock() + cmd->getDuration() - 1;

			// loop over the events in the list removing the ones that 
			// are within the erasure range.

			for ( ; e != NULL && e->getClock() < cmd->getClock() ; 
				  e = e->getNext())
			  prev = e;
      
			for (next = NULL ; e != NULL && e->getClock() < end ; e = next) {
				next = e->getNext();
				e->setNext(NULL);
				e->free();
			}

			if (prev == NULL)
			  evlist = e;
			else
			  prev->setNext(e);
			prev = e;

			// If the end of the erasure wrapped around to the beginning
			// of a loop, go back to the start of the region and waste
			// the notes up until the adjusted end point.

			if (end > loop_end) {
				end -= (loop_end - loop_start);
				prev = NULL;
				for (e = evlist ; e->getClock() < loop_start ; 
					 e = e->getNext())
				  prev = e;

				for (next = NULL ; e != NULL && e->getClock() < end ; 
					 e = next) {
					next = e->getNext();
					e->setNext(NULL);
					e->free();
				}
				if (prev == NULL)
				  evlist = e;
				else
				  prev->setNext(e);
			}
		}
	}

	// give the modified event list back to the sequence
	rectrack->getSequence()->setEvents(evlist);
}

/****************************************************************************
 * SeqRecording::flushCommands
 *
 * Arguments:
 *	flushall: non-zero to flush all events
 *
 * Returns: none
 *
 * Description: 
 * 
 * Used to free the list of command events in a recording.
 * The last event in the list is only freed if the "flushall" argument
 * is non-zero or if the duration of the last event is zero indicating
 * that no "off" time was set for the command.
 * 
 * The "flushall" command would be set when we're stopping recording 
 * completely, but would be off if we've just taken a loop.
 * 
 ****************************************************************************/

PRIVATE void SeqRecording::flushCommands(int flushall)
{
	MidiEvent *ev, *next;

	// free everything but the last event
	next = NULL;
	for (ev = commands ; ev != NULL && ev->getNext() != NULL ; ev = next) {
		next = ev->getNext();
		ev->setNext(NULL);
		ev->free();
	}

	// free the last event if requsted or malformed
	if (ev != NULL && (flushall || ev->getDuration() != 0)) {
		ev->free();
		ev = NULL;
	}

	commands = ev;
	last_command = ev;
}

/****************************************************************************
 *                                                                          *
 *   							SIMPLE MERGER                               *
 *                                                                          *
 ****************************************************************************/

/****************************************************************************
 * SeqRecording::mergeEvents
 *
 * Arguments:
 *	max_end: maximum end clock
 *
 * Returns: none
 *
 * Description: 
 * 
 * Part of the recording "complication" process that happens when
 * recording stops, or a loop is taken.
 * 
 * Here we combine the event list in the target sequence, with the 
 * temporary event list that we maintained in the SeqRecording object
 * while recording.  This could be moved over to the MidiModule library?
 *
 * If max_end is non-zero it indicates a hard upper bound on the merger.
 * If a new event lies beyond the max_end, it is ignored.  If a new event
 * duration extends beyond max, it is clipped.  This is used to implement
 * punch in/out with a fixed punch out clock.
 * 
 * 
 ****************************************************************************/

PRIVATE void SeqRecording::mergeEvents(int max_end)
{
	MidiEvent 	*oldlist, *newlist, *ev1, *ev2, *prev1, *next2;
	int 		durend;

	// get the two event lists, and NULL out the pointers while
	// we do surgery
	oldlist = rectrack->getSequence()->stealEvents();
	newlist = events;
	events  = NULL;

	ev1   = oldlist;
	prev1 = NULL;

	// loop over the newly reorded events
	// hmm, we're not doing Sequence::insert here but trying to maintain
	// our own insert pointer for speed.  This might suck if there
	// are ordering issues among the various event types?

	for (ev2 = events, next2 = NULL ; ev2 != NULL ; ev2 = next2) {
		next2 = ev2->getNext();

		if (max_end && ev2->getClock() > max_end) {
			// the entire note is out of range, ignore it
			ev2->setNext(NULL);
			ev2->free();
		}
		else {
			// clip the new note if beyond max end point
			if (max_end && ev2->getStatus() == MS_NOTEON) {    
				durend = ev2->getClock() + ev2->getDuration() - 1;
				if (durend > max_end)
				  ev2->setDuration(max_end - ev2->getClock() + 1);
			}

			// add the new event to the target list
			for ( ; ev1 != NULL && ev1->getClock() < ev2->getClock() ; 
				  ev1 = ev1->getNext())
			  prev1 = ev1;

			if (prev1 == NULL)
			  oldlist = ev2;
			else
			  prev1->setNext(ev2);

			prev1 = ev2;
			ev2->setNext(ev1);

			// if we have no max_end to check, stop the loop now
			if (ev1 != NULL && !max_end)
			  next2 = NULL;
		}
	}

	// store the adjusted event list
	rectrack->getSequence()->setEvents(oldlist);

	// no longer have any events here
	events = NULL;
}

/****************************************************************************
 * SeqRecording::mergeNormal
 *
 * Arguments:
 *	flush: non-zero to flush after merge
 *
 * Returns: none
 *
 * Description: 
 * 
 * Combine the events queued on the various internal event lists with
 * those in the target sequence.
 * 
 * This is done when the recording is stopped or when a recording loop point
 * is hit.
 * 
 * If the flush flag is on, call flushHangingNotes to set the 
 * final durations for any note events that are still dangling.  The 
 * flush flag is set only when the recording stops, if we're looping, 
 * we let the notes dangle.
 * 
 ****************************************************************************/

PUBLIC void SeqRecording::mergeNormal(int flush)
{
	// stop any dangling notes if we're not going to loop
	if (flush)
	  flushHangingNotes();

	// process any commands queued during the recording
	if (commands != NULL) {
		processCommands();
		flushCommands(flush);
	}

	// merge the event list, no max clock
	mergeEvents(0);

	// initalize the fields in the recording structure
	runtimeInit(flush);
}

/****************************************************************************
 *                                                                          *
 *   						 DYNAMIC PUNCH MERGER                           *
 *                                                                          *
 ****************************************************************************/

/****************************************************************************
 * SeqRecording::mergeEventsDyna
 *
 * Arguments:
 *	max_end: max clock
 *
 * Returns: none
 *
 * Description: 
 * 
 * Helper for the mergeDynaPunch method.
 * Similar to mergeEvents except that any events in the target sequence
 * that are "covered" by the events in the new list are removed as the
 * merger is performed.
 * 
 * This really only makes sense for NOTE events since they are the
 * only ones with meaningful durations?  
 *
 * Could go in the MidiModule library, except for the screwy "max_end" clock
 * value that must be passed in to account for events dangling during record
 * looping.  Think about this.
 * 
 ****************************************************************************/

PRIVATE void SeqRecording::mergeEventsDyna(int max_clock)
{
	MidiEvent *oldevents, *newevents, *ev1, *ev2, *prev1, *next1, *next2;
	int end;


	// get the target event list
	oldevents = rectrack->getSequence()->stealEvents();
	newevents = events;
	events = NULL;

	ev1   = oldevents;
	prev1 = NULL;

	// for each new event
	ev2 = newevents;
	for (next2 = NULL ; ev2 != NULL ; ev2 = next2) {
		next2 = ev2->getNext();

		// locate the punch position within the destination event list
		for ( ; ev1 != NULL && ev1->getClock() < ev2->getClock() ; 
			  ev1 = ev1->getNext())
		  prev1 = ev1;

		// insert the new event
		if (prev1 == NULL)
		  oldevents = ev2;
		else
		  prev1->setNext(ev2);
		prev1 = ev2;
		ev2->setNext(ev1);

		// Remove any (note) events in the destination list that would have
		// their start times covered by the inserted event.
		if (ev2->getStatus() == MS_NOTEON) {

			if (ev2->getDuration())
			  end = ev2->getClock() + ev2->getDuration() - 1;
			else
			  end = max_clock;

			for (next1 = NULL ; ev1 != NULL && ev1->getClock() < end ; 
				 ev1 = next1) {
				next1 = ev1->getNext();
				ev2->setNext(next1);
				ev1->setNext(NULL);
				ev1->free();
			}
		}
	}

	// store the adjusted event list
	rectrack->getSequence()->setEvents(oldevents);
	// no longer have an event list here
	events = NULL;
}

/****************************************************************************
 * SeqRecording::mergeDynaPunch
 *
 * Arguments:
 *	flush: non-zero to flush after merge
 *
 * Returns: none
 *
 * Description: 
 * 
 * One of the primary for performing a merger operation after recording
 * or loop during record.
 *
 * Here we punch in the events recorded during the last session/loop ,
 * the extent of the punch region is determined by the note events
 * entered, hence the term "dyna punch".
 * 
 * Each note event entered will automatically punch over any events
 * underneath but gaps in the new note list will allow the current events
 * to be retained.  
 * 
 * The "flush" arg will be one if we're stopping, or zero if we're looping.
 * 
 ****************************************************************************/

PUBLIC void SeqRecording::mergeDynaPunch(int flush)
{
	int max_end;

	// flush any dangling notes
	if (flush)
	  flushHangingNotes();

	// process the command list
	if (commands != NULL) {
		processCommands();
		flushCommands(flush);
	}

	// merge the new notes
	max_end = sequencer->getLoopEnd() - 1;
	mergeEventsDyna(max_end);

	// initialize the list variables
	runtimeInit(flush);
}

/****************************************************************************
 *                                                                          *
 *   						ABSOLUTE PUNCH MERGER                           *
 *                                                                          *
 ****************************************************************************/

/****************************************************************************
 * SeqRecording::erasePunchRegion
 *
 * Arguments:
 *
 * Returns: none
 *
 * Description: 
 * 
 * Helper for mergePunch.
 * Here we erase all of the events in the target sequence that fall 
 * within the punch region.
 * 
 * If the "cut" flag is on, whenever an event is encountered whose duration
 * causes it to hang into the erase range, the duration is modified so that 
 * the note cuts off when the erase range starts.  This gives a closer
 * simulation of a tape "punch in" which will cut off any notes being held.
 * 
 ****************************************************************************/

PRIVATE void SeqRecording::erasePunchRegion(void)
{
	MidiEvent *evlist, *ev, *next, *prev;
	int start, end, cut, durend;

	start = sequencer->getPunchIn();
	end = sequencer->getPunchOut();
	cut = sequencer->getRecordCut();

	// get the target sequence event list
	evlist = rectrack->getSequence()->stealEvents();

	// find the erase point, cut off hanging notes early if requested
	prev = NULL;
	for (ev = evlist ; ev != NULL && ev->getClock() < start ; 
		 ev = ev->getNext()) {

		prev = ev;
		if (cut && ev->getStatus() == MS_NOTEON) {
			durend = ev->getClock() + ev->getDuration() - 1;
			if (durend > start)
			  ev->setDuration(start - ev->getClock() + 1);
		}
	}

	// remove the notes in the erasure range
	for (next = NULL ; ev != NULL && ev->getClock() < end ; ev = next) {
		next = ev->getNext();
		ev->setNext(NULL);
		ev->free();
	}

	// update the destination list
	if (prev == NULL)
	  evlist = ev;
	else
	  prev->setNext(ev);

	rectrack->getSequence()->setEvents(evlist);
}

/****************************************************************************
 * SeqRecording::mergePunch
 *
 * Arguments:
 *
 * Returns: none
 *
 * Description: 
 * 
 * One of the primary merger operations, called when the recorder stops
 * or when a loop point is hit.
 * 
 * Here we perform a traditional "absolute" punch between two 
 * fixed times.
 * 
 * This never had a "flush" arg, which I guess means that it can't be 
 * an issue even if we're looping?  Probably because the punch region
 * must always fall within the loop region?
 * 
 ****************************************************************************/

PUBLIC void SeqRecording::mergePunch(void)
{
	flushHangingNotes();

	if (commands != NULL) {
		processCommands();
		flushCommands(1);
	}

	// remove the events in the target sequence
	erasePunchRegion();

	// merge the new events
	mergeEvents(sequencer->getPunchOut());

	runtimeInit(1);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
