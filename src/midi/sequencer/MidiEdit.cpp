/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * This code is currently unused and has not been kept up to date
 * with the evolution of the data model.  It has implementations of
 * various sequence mutations.  Keep it around in case we want to 
 * expose these.
 * 
 * Sequence editing commands.  
 * Most of this was used by GEE, but simple operations like copy are
 * also implemented in terms of this.
 * 
 * Editing operations are specified by defining a MidiEdit object, 
 * and then applying it to a MidiSequence.  Among other things, the 
 * template can specifiy the boundaries of the edit, the type of 
 * operation to perform and the events to be affected.
 *
 */

#include <stdio.h>
#include "Port.h"
#include "Midi.h"

/****************************************************************************
 *                                                                          *
 *   							LIST SEARCHING                              *
 *                                                                          *
 ****************************************************************************/

/**
 * Finds the first event in the list that is exactly on the given clock.
 */
PUBLIC MidiEvent *MidiEvent::findClock(int clock, int status)
{
	MidiEvent *e, *found;

	found = NULL;
	for (e = this ; e != NULL ; e = e->mNext) {

		if (e->mClock > clock)
		  break;						// can stop now

		else if (status == 0 || status == e->mStatus ||
				 (status == MS_ANYCONTROL &&
				  MIDI_IS_CONTROLLER_STATUS(e->mStatus))) {

			if (e->mClock == clock) {
				found = e;
				break;
			}
		}
	}

	return found;
}

/**
 * Returns the event that is either on or after the given clock.
 */
PUBLIC MidiEvent *MidiEvent::findFirst(int clock, int status)
{
	MidiEvent *e, *found;

	found = NULL;
	for (e = this ; e != NULL ; e = e->mNext) {

		if (e->mClock >= clock && 
			(status == 0 || status == e->mStatus ||
			 (status == MS_ANYCONTROL &&
			  MIDI_IS_CONTROLLER_STATUS(e->mStatus)))) {

			found = e;
			break;
		}
	}

	return found;
}

/**
 * Looks for a MS_NOTE event that is "covered" by another. 
 */
PUBLIC MidiEvent *MidiEvent::findCovered(int clock, int key)
{
	MidiEvent *e, *found;

	found = NULL;
	for (e = this ; e != NULL ; e = e->mNext) {

		if (e->mStatus = MS_NOTEON && e->mKey == key) {
			int c1 = clock + mDuration;
			int c2 = e->mClock + e->mDuration;

			if ((e->mClock >= clock && c1 >= e->mClock) ||
				(e->mClock < clock && c2 >= clock)) {
			    found = e;
				break;
			}
		}
	}

	return found;
}

/**
 * Looks for a particular clock/key note event, and optionally removes
 * it from the list
 * 
 * Arguments:
 *	 clock: clock to look for
 *	   key: key to look for
 *	remove: non-zero to remove the event
 *
 */
PUBLIC MidiEvent *MidiEvent::findNote(MidiEvent **newlist, 
										 int clock, int key, int remove)
{
	MidiEvent *list, *e, *prev, *found;

	found	= NULL;
	list 	= this;
	prev 	= NULL;

	for (e = list ; e != NULL ; e = e->mNext) {

		if (e->mClock > clock)
		  break;
		
		else if (e->mStatus == MS_NOTEON && 
				 e->mClock == clock && 
				 e->mKey == key) {
		  found = e; 
		  break;
		}
		else
		  prev = e;
	}

	if (found != NULL && remove) {
		if (prev != NULL)
          prev->mNext = found->mNext;
        else
          list = found->mNext;
    }

	if (newlist != NULL)
	  *newlist = list;

	return found;
}

/**
 * Find the last event in the list with a particular status.
 * If stauts is zero, the last event in the list is returned.
 */
PUBLIC MidiEvent *MidiEvent::last(int status)
{
	MidiEvent *e, *last;

	last = NULL;
	for (e = this ; e != NULL ; e = e->mNext) {

		if (status == 0 || status == e->mStatus)
		  last = e;
	}

	return last;
}

/**
 * Looks for an event "covered" by the clock/key value, and if found removes
 * it from the list.  This is an adaptation of findCovered.
 */
PUBLIC MidiEvent *MidiEvent::capture(MidiEvent **newlist,
										int clock, int key)
{
	MidiEvent *list, *e, *prev, *found;

	found 	= NULL;
	list	= this;
	prev	= NULL;

	for (e = list ; e != NULL ; e = e->mNext) {

		if (e->mStatus = MS_NOTEON && e->mKey == key) {
			int c1 = clock + mDuration;
			int c2 = e->mClock + e->mDuration;

			if ((e->mClock >= clock && c1 >= e->mClock) ||
				(e->mClock < clock && c2 >= clock)) {
			    found = e;
				break;
			}
			else 
			  prev = e;
		}
		else 
		  prev = e;
	}
	
	if (found != NULL) {
		if (prev != NULL)
		  prev->mNext = found->mNext;
		else
		  list = found->mNext;

		found->mNext = NULL;
	}

	if (newlist != NULL)
	  *newlist = list;

	return found;
}

/****************************************************************************
 *                                                                          *
 *   						  CONTROLLER EVENTS                             *
 *                                                                          *
 ****************************************************************************/

/**
 * MidiEvent::findController
 *
 * Arguments:
 *	    newlist: returned adjusted list
 *	      clock: clock to look for
 *	     status: controller satus
 *	     number: controller number
 *	exact_clock: non-zero to match exact clock
 *	     remove: non-zero to remove
 *
 * Returns: event we found
 *
 * Description: 
 * 
 * A rather specialized method for finding controller events.
 * If status is MS_PITCH or MS_TOUCH, the number is ignored and the
 * first matching event with this status is returned.
 *
 * If status is one of the random controller types, both the status 
 * must match and the number must be the MC_ controller type we're
 * interested in.
 *
 * If the exact flag is on, the clock must match exactly, otherwise
 * the first matching controller on or after the given clock is returned.
 *
 * If the remove flag is on, the matching controller is removed from
 * the event list.
 *
 */
PUBLIC MidiEvent *MidiEvent::findController(MidiEvent **newlist,
											   int clock, int status, 
											   int number,
											   int exact_clock, int remove)
{
	MidiEvent *found, *list, *e, *prev;

	found	= NULL;
	list	= this;
	prev	= NULL;

	for (e = list ; e != NULL ; e = e->mNext) {

		if (e->mClock == clock || (!exact_clock && e->mClock > clock)) {
			if (status == MS_PITCH || status == MS_TOUCH) {
				if (e->mStatus == status)
				  found = e;
			}
			else if ((status == e->mStatus ||
					  (status == MS_ANYCONTROL && 
					   MIDI_IS_CONTROLLER_STATUS(e->mStatus))) &&
					 e->mKey == number)
			  found = e;
		}

		if (found) {
			if (remove) {
				if (prev)
				  prev->mNext = e->mNext;
				else
				  list = e->mNext;
			}
			break;
		}
		prev = e;
		if (exact_clock && e->mClock > clock)
		  break;
	}

	if (newlist != NULL)
	  *newlist = list;

	return found;
}

/****************************************************************************
 *                                                                          *
 *   						  EVENT LIST MAPPING                            *
 *                                                                          *
 ****************************************************************************/

/**
 * MidiEvent::map
 *
 * Arguments:
 *	 start: start clock
 *	   end: end clock
 *	status: status to look for
 *	mapper: function to apply
 *	  args: args to function
 *
 * Returns: none
 *
 * Description: 
 * 
 * Iterates over an event list, calling a function for each node.
 * The range of the map is specified by the start & end times.
 * Set "end" to SEQ_CLOCK_INFINITE to cover all notes from the
 * start to the end.
 *
 * The events can be filtered by specifiying the "status" argument.
 * If status is 0, all events in the list will be mapped, otherwise
 * only those with matching statuses will be mapped.
 * 
 */
PUBLIC void MidiEvent::map(int start, int end, int status, 
							  MIDI_EVENT_MAPPER function,
							  void *args)
{
	MidiEvent *e;

	// find the start point
	for (e = this ; e != NULL && e->mClock < start ; e = e->mNext);

	// map till the end point
	for ( ; e != NULL && e->mClock <= end ; e = e->mNext) {
		if (status == 0 || e->mStatus == status ||
			(status == MS_ANYCONTROL && 
			 MIDI_IS_CONTROLLER_STATUS(e->mStatus)))
		  (void)(*function)(e, args);
	}
}

/**
 * MidiEvent::mapRemove
 *
 * Arguments:
 *	   start: start clock
 *	     end: end clock
 *	  status: status to look for (zero for any)
 *	function: mapping function
 *	    args: arguments to mapping function
 *
 * Returns: new list
 *
 * Description: 
 * 
 * Maps over the events in a list calling a function.
 * Unlike MidiEvent::map, the mapping function can return a status
 * code.  If the status code is non-zero the event on which the
 * mapping function was called is removed from the sequence.
 * 
 * The new list is returned.
 * 
 * Hmm.  Since we don't have a MidiApplication in here, we can't
 * return the event to the pool, we just delete it.  Might want to 
 * be smarter.
 * 
 */
PUBLIC MidiEvent *MidiEvent::mapRemove(int start, int end, int status,
                                       MIDI_EVENT_MAPPER function,
                                       void *args)
{
	MidiEvent *list, *event, *next, *prev;
	int remove;

	list  = this;
	prev  = NULL;
	event = list;

	while (event != NULL && event->mClock < start) {
		prev  = event;
		event = event->mNext;
	}

	while (event != NULL && event->mClock < end) {
		next = event->mNext;

		if (status && event->mStatus != status)
		  prev = event;
		else {
			remove = (*function)(event, args);
			if (!remove)
			  prev = event;
			else {
				if (prev != NULL)
				  prev->mNext = event->mNext;
				else
				  list = event->mNext;
				event->mNext = NULL;
				delete event;
			}
		}
		event = next;
	}

	return list;
}

/****************************************************************************
 *                                                                          *
 *                           OLD HEADER FILE STUFF                          *
 *                                                                          *
 ****************************************************************************/

/**
 * MidiEventMapper
 *
 * Arguments:
 *	   e: event
 *	args: user arguments
 *
 * Returns: non-zero to remove event
 *
 * Description: 
 * 
 * Signature of the function used in various places when "mapping" over
 * an event list.  In some cases (but not all) returning non-zero will cause
 * the event to be removed.  See individual function comments.
 * Should modernize this and define an abstract interface.
 */

typedef int (*MidiEventMapper)(class MidiEvent *e, void *args);


/**
 * A set of bit flags that indicate whether or not the sequence
 * is ignoring a particular type of event.
 * I forget where this is used, does it apply to the sequence as it
 * is being built, or only when it is being played? 
 * If so, move it to the recorder module.
 */
#define MIDI_FILTER_NOTES  1
#define MIDI_FILTER_PROGRAMS 2
#define MIDI_FILTER_CONTROLLERS 4

/**
 * Argument for the MidiSequence::durate method.
 * Specifes how to adjust the duration of notes. 
 */
typedef enum {

    DurateRatio,
    DurateAbsolute

} MidiDurateMode;

/**
 * Argument for the MidiSequence::velocitize method.
 * Specifies how to adjust the velocities of events.
 */
typedef enum {

    VelocityAbsolute,
    VelocityCompress,
    VelocityRamp,
    VelocityIncrement

} MidiVelocityMode;

/****************************************************************************
 *                                                                          *
 *   						   SEQUENCE EDITS                               *
 *                                                                          *
 ****************************************************************************/

/****************************************************************************
 * MIDI_EDIT
 *
 * Arguments:
 *
 * Returns: 
 *
 * Description: 
 * 
 * Command constants for the MidiEdit template.
 * Each command type can have a number of parameters and be constrained
 * by other settings in the template.  Used as an argument to 
 * a MmDeviceModule factory method, so we define it up here.
 ****************************************************************************/

typedef enum {

	MIDI_EDIT_NONE,
	MIDI_EDIT_MAP,
	MIDI_EDIT_COPY,
	MIDI_EDIT_ERASE,
	MIDI_EDIT_CUT,
	MIDI_EDIT_PASTE,
	MIDI_EDIT_PASTE_COPY
 
} MIDI_EDIT;

/****************************************************************************
 * MIDI_EDIT_MAPPER
 *
 * Arguments:
 *
 * Returns: adjusted event
 *
 * Description: 
 * 
 * Signature of function that must be used for MIDI_EDIT_MAP operations.
 * Mostly used internally for the various sequence "region" operations.
 * Called for each event in the region
 * 
 ****************************************************************************/

typedef MidiEvent *(*MIDI_EDIT_MAPPER)(MidiEdit *edit, MidiEvent *event, 
									   void *args);

/****************************************************************************
 * MidiEdit
 *
 * Description: 
 * 
 * Object used to specify complex editing operations on sequences.
 ****************************************************************************/

class MidiEdit {

  public:
	
	//
	// constructors
	//

	static MidiEdit *create(MidiEnv *mod, MIDI_EDIT command);
	static MidiEdit *create(MidiSequence *seq, MIDI_EDIT command);

	void setCommand(MIDI_EDIT e) { 
		command = e;
	}

	void setStart(int s) {
		start = s;
	}

	void setEnd(int e) {
		end = e;
	}

	void setTop(int t) {
		top = t;
	}

	void setBottom(int b) {
		bottom = b;
	}

	void setNotes(int n) {
		notes = n;
	}

	void setControllers(int c) {
		controllers = c;
	}
	
	void setPrograms(int p) {
		programs = p;
	}

	void setOthers(int o) {
		others = o;
	}

	void setShift(int s) {
		shift = s;
	}
	
	void setTranspose(int t) {
		transpose = t;
	}

	void setCompression(int c) {
		compression = c;
	}

	void setStartOffset(int o) {
		start_offset = o;
	}
	
	void setPush(int p) {
		push = p;
	}

	void setMapper(MIDI_EDIT_MAPPER m) {
		mapper = m;
	}

	void setArgs(void *a) {
		args = a;
	}

	//
	// execution
	//

	MidiSequence *edit(MidiSequence *src);
	void merge(MidiSequence *source, MidiSequence *dest);

	//
	// accessors
	//

	MIDI_EDIT getCommand(void) {
		return command;
	}

	int getStart(void) {
		return start;
	}

	int getEnd(void) {
		return end;
	}

	int getTop(void) {
		return top;
	}

	int getBottom(void) {
		return bottom;
	}

	int getNotes(void) {
		return notes;
	}

	int getControllers(void) {
		return controllers;
	}
	
	int getPrograms(void) {
		return programs;
	}

	int getOthers(void) {
		return others;
	}

	int getShift(void) {
		return shift;
	}
	
	int getTranspose(void) {
		return transpose;
	}

	int getCompression(void) {
		return compression;
	}

	int getStartOffset(void) {
		return start_offset;
	}

	int getPush(void) {
		return push;
	}

	MIDI_EDIT_MAPPER getMapper(void) {
		return mapper;
	}

	void *getArgs(void) {
		return args;
	}

  private:

	MidiEnv *module;

	MIDI_EDIT command;	// defines the fundamental operation

	//
	// region, defines a rectangular region constrained by time 
	// and event value
	//

	int start;			// start clock (0 for first)
	int end;			// end clock (MIDI_CLOCK_MAX for all)
	int top;			// top key/value (-1 for all)
	int bottom;			// bottom key/value (-1 for all)

	// 
	// event filters, enable processing for specific types
	// these are ignored now, should delete them
	//

	int	notes;			// set if interested in notes
	int	controllers;	// set if interested in controllers
	int programs;		// set if interested in programs
	int others;			// set if interested in others


	// 
	// edit parameters
	//
	
	int shift;			// added to clock in range
	int transpose;		// added to each key in range
	int compression;	// added to each velocity in range


	//
	// paste parameters
	//

	int start_offset;	// star time adjust for merge
	int push;			// time adjust for subsequent events


	// 
	// user specified mapping function
	//
	
	MIDI_EDIT_MAPPER mapper;
	void *args;

	MidiEdit(void) {
		
		module			= NULL;
		command			= MIDI_EDIT_NONE;
		start 			= 0;
		end 			= 0;
		top 			= 0;
		bottom 			= 0;
		notes 			= 0;
		controllers 	= 0;
		programs 		= 0;
		others 			= 0;
		shift 			= 0;
		transpose		= 0;
		compression 	= 0;
		start_offset	= 0;
		push 			= 0;
		mapper 			= NULL;
		args 			= NULL;
	}

	void init(MIDI_EDIT command);
	void insertEvent(MidiEvent **head, MidiEvent **tail, MidiEvent *neu);
	MidiEvent *processEdit(MidiEvent **root);
	void processMerge(MidiEvent **source, MidiEvent **dest);
	MidiEvent *sortCommands(MidiEvent *events);


};

/****************************************************************************
 *                                                                          *
 *                            OLD SEQUENCE METHODS                          *
 *                                                                          *
 ****************************************************************************/

/****************************************************************************
 * MidiSequence::firstNote
 *
 * Arguments:
 *	clock: optional clock
 *
 * Returns: first note event
 *
 * Description: 
 * 
 * Finds the first note event int the sequence.
 * The clock is optional.
 ****************************************************************************/

PUBLIC MidiEvent *MidiSequence::firstNote(int clock)
{
	return mEvents->findFirst(clock, MS_NOTEON);
}

/****************************************************************************
 * MidiSequence::captureNote
 *
 * Arguments:
 * clock: clock of interest
 * key: key of interest
 *
 * Returns: captured note
 *
 * Description: 
 * 
 * Find a note that is under or "covered" by the given clock/key and if found
 * remove it from the sequence.
 * 
 ****************************************************************************/

PUBLIC MidiEvent *MidiSequence::captureNote(int clock, int key)
{
	MidiEvent *e;

	e = mEvents->capture(&mEvents, clock, key);
	return e;
}

/****************************************************************************
 * MidiSequence::mapNotes
 *
 * Arguments:
 *	   start: start clock
 *	     end: end clock
 *	function: mapping function
 *	    args: mapping function args
 *
 * Returns: none
 *
 * Description: 
 * 
 * Maps a function over the events in the sequence.
 ****************************************************************************/

PUBLIC void MidiSequence::mapNotes(int start, int end, 
									  MIDI_EVENT_MAPPER function,
									  void *args,
									  int remove)
{
	if (remove)
	  mEvents = mEvents->mapRemove(start, end, MS_NOTEON, function, args);
	else
	  mEvents->map(start, end, MS_NOTEON, function, args);
}

/****************************************************************************
 * MidiSequence::firstController
 *
 * Arguments:
 *
 * Returns: first controller event
 *
 * Description: 
 * 
 * Returns the first controller event in the sequence on or after the
 * given clock.  The controller event can be of any type.
 ****************************************************************************/

PUBLIC MidiEvent *MidiSequence::firstController(int clock)
{
	return mEvents->findFirst(clock, MS_ANYCONTROL);
}

/****************************************************************************
 * MidiSequence::findController
 *
 * Arguments:
 *	 clock: clock of interest
 *	status: specific status (may be MS_ANYCONTROL)
 *	number: controller number
 *
 * Returns: controller event
 *
 * Description: 
 * 
 * Finds the first controller of a specific type on or after the
 * given clock.
 *
 * If status is MS_PITCH or MS_TOUCH, the number is ignored and the
 * first matching event with this status is returned.
 *
 * If status is one of the random controller types, both the status 
 * must match and the number must be the MC_ controller type we're
 * interested in.
 *
 ****************************************************************************/

PUBLIC MidiEvent *MidiSequence::findController(int clock, int status,
												  int number)
{
	return mEvents->findController(NULL, clock, status, number, 
								  1, // exact match
								  0 // don't remove
								  );
}

/****************************************************************************
 * MidiSequence::captureController
 *
 * Arguments:
 *	 clock: 
 *	status: 
 *	number: 
 *
 * Returns: event
 *
 * Description: 
 * 
 * Locate a controller using the same search method as findController.
 * If found, remove it from the sequence.
 ****************************************************************************/

PUBLIC MidiEvent *MidiSequence::captureController(int clock, 
													 int status, 
													 int number)
{
	MidiEvent *e;

	e = mEvents->findController(&mEvents, clock, status, number, 
							   1, // exact match
							   1 // remove
							   );
	return e;
}

/****************************************************************************
 * MidiSequence::mapControllers
 *
 * Arguments:
 *	   start: start clock
 *	     end: end clock
 *	function: mapping function
 *	    args: mapping function args
 *
 * Returns: none
 *
 * Description: 
 * 
 * Maps a function over all of the controllers in the sequence.
 ****************************************************************************/

PUBLIC void MidiSequence::mapControllers(int start, int end, 
											MIDI_EVENT_MAPPER function,
											void *args,
											int remove)
{
	if (remove)
	  mEvents = mEvents->mapRemove(start, end, MS_ANYCONTROL, function, args);
	else
	  mEvents->map(start, end, MS_ANYCONTROL, function, args);
}

/****************************************************************************
 * MidiSequence::firstProgram
 *
 * Arguments:
 *
 * Returns: first program event
 *
 * Description: 
 * 
 * Finds the first program event on or after the given clock.
 ****************************************************************************/

PUBLIC MidiEvent *MidiSequence::firstProgram(int clock)
{
	return mEvents->findFirst(clock, MS_PROG);
}

/****************************************************************************
 * MidiSequence::findProgram
 *
 * Arguments:
 *	clock: 
 *
 * Returns: event
 *
 * Description: 
 * 
 * Finds a program event (if any) on an exact clock.
 ****************************************************************************/

PUBLIC MidiEvent *MidiSequence::findProgram(int clock)
{
	return mEvents->findClock(clock, MS_PROG);
}

/****************************************************************************
 * MidiSequence::prunePrograms
 *
 * Arguments:
 *
 * Returns: none
 *
 * Description: 
 * 
 * Examines program changes in a sequence and removes any that
 * are redundant.  Normally used with sequence combination utilities
 * like MSL that can result in duplicate program changes.
 ****************************************************************************/

PUBLIC void MidiSequence::prunePrograms(void)
{
	MidiEvent *p, *next, *prev;
	int programs[16];
	int i, chan;

	for (i = 0 ; i < 16 ; i++) 
	  programs[i] = -1;

	next = NULL;
	prev = NULL;

	for (p = mEvents ; p != NULL ; p = next) {
		next = p->getNext();
		if (p->getStatus() != MS_PROGRAM)
		  prev = p;
		else {
			chan = p->getStatus() & 0x0F;
			if (programs[chan] != p->getKey()) {
				programs[chan] = p->getKey();
				prev = p;
			}
			else {
				if (prev == NULL)
				  mEvents = next;
				else
				  prev->setNext(next);

				p->setNext(NULL);
				p->free();
			}
		}
	}
}

/****************************************************************************
 * MidiSequence::findLoop
 *
 * Arguments:
 *	start: start clock
 *	  end: end clock
 *	count: loop count
 *
 * Returns: loop event
 *
 * Description: 
 * 
 * Find a specific loop event in a sequence.
 ****************************************************************************/

PUBLIC MidiEvent *MidiSequence::findLoop(int start, int end, int count)
{
	MidiEvent *e, *found;

	found = NULL;
	for (e = mEvents ; e != NULL && found == NULL ; e = e->getNext()) {

		if (e->getStatus() == MS_CMD_LOOP && 
			e->getClock() == start && 
			e->getExtra() == count &&
			(e->getClock() + e->getDuration() == end))
		  found = e;
	}

	return found;
}

/****************************************************************************
 * MidiSequence::dropLoops
 *
 * Arguments:
 *
 * Returns: none
 *
 * Description: 
 * 
 * Remove all of the loop events from a sequence.
 ****************************************************************************/

PUBLIC void MidiSequence::dropLoops(void)
{
	MidiEvent *e, *prev, *next;

	for (e = mEvents, prev = NULL, next = NULL ; e != NULL ; e = next) {
		next = e->getNext();
		if (e->getStatus() != MS_CMD_LOOP)
		  prev = e;
		else {
			if (prev == NULL)
			  mEvents = next;
			else
			  prev->setNext(next);

			e->setNext(NULL);
			e->free();
		}
	}
}

/****************************************************************************
 * MidiSequence::addLoop
 *
 * Arguments:
 *	start: loop start clock
 *	  end: loop end clock
 *	count: loop count
 *
 * Returns: none
 *
 * Description: 
 * 
 * Establishes a loop event for a sequence.
 * There can be any number of these though in practice we have only one.
 * See event_insert for some of the rules regarding loop event
 * ordering.
 * 
 ****************************************************************************/

PUBLIC void MidiSequence::addLoop(int start, int end, int count)
{
	MidiEvent *e;

	if (start < end) {
		e = newEvent();
		if (e != NULL) {
			e->setStatus(MS_CMD_LOOP);
			e->setClock(start);
			e->setDuration(end - start);
			e->setExtra(count);
			mEvents = mEvents->insert(e);
		}
	}
}

/****************************************************************************
 * MidiSequence::setLoop
 *
 * Arguments:
 *	start: loop start
 *	  end: loop end
 *	count: loop count
 *
 * Returns: none
 *
 * Description: 
 * 
 * Establishes a loop event for a sequence.
 * Any existing loops are first deleted.
 * This is just a simplified interface for seq_add_loop since we most
 * often want only one loop.
 ****************************************************************************/

PUBLIC void MidiSequence::setLoop(int start, int end, int count)
{
	dropLoops();
	addLoop(start, end, count);
}

/****************************************************************************
 *                                                                          *
 *   					 EDITING TEMPLATE DEFINITION                        *
 *                                                                          *
 ****************************************************************************/

/****************************************************************************
 * MidiEdit::init
 *
 * Arguments: command
 *
 * Returns:  none
 *
 * Description: 
 * 
 * Initializes a MidiEdit object for a particular command.
 * Formerly public, but not called through one of our create methods.
 *
 * First call "new MidiEdit" to get an object, then call "init" with
 * the basic command.
 * 
 * Default is for a basic copy operation.  This is normally used
 * to initialize the template for a later modification before
 * calling MidiSequence::edit or ont of the other editing functions.
 * 
 ****************************************************************************/

PRIVATE void MidiEdit::init(MIDI_EDIT command)
{
	command = command;
	
	// no boundaries here
	start  		= 0;
	end    		= MIDI_MAX_CLOCK;
	top    		= -1;
	bottom 		= -1;
	
	// apply to all events
	notes		= 1;
	controllers	= 1;
	programs	= 1;
	others		= 1;
	
	// no alterations
	
	shift			= 0;
	transpose		= 0;
	compression     = 0;
	start_offset	= 0;
	push			= 0;
	
	// no mapping function
	
	mapper	 	= NULL;
	args 		= NULL;
}

/****************************************************************************
 * MidiEdit::create
 *
 * Arguments:
 *	device module (or sequence)
 *
 * Returns: midi edit object
 *
 * Description: 
 * 
 * Public constructors for a midi edit object
 * These need to get to a MmDeviceModule in order to allocate new objects,
 * so you can either pass one in, or pass a sequence allocated from one.
 * Also can be created via MmDeviceModule::newEdit.
 * 
 ****************************************************************************/

INTERFACE MidiEdit *MidiEdit::create(MidiModule *mod, MIDI_EDIT command)
{
	MidiEdit *e;

	e = new MidiEdit;
	if (e != NULL) {
		e->module = mod;
		e->init(command);
	}

	return e;
}

INTERFACE MidiEdit *MidiEdit::create(MidiSequence *seq, MIDI_EDIT command)
{
	MidiEdit *e;

	e = new MidiEdit;
	if (e != NULL) {
		e->module = seq->getModule();
		e->init(command);
	}

	return e;
}

/****************************************************************************
 *                                                                          *
 *   							 EDIT ENGINE                                *
 *                                                                          *
 ****************************************************************************/

/****************************************************************************
 * MidiEdit::insertEvent
 *
 * Arguments:
 *	head: pointer to head of event list
 *	tail: pointer to tail of event list
 *	 neu: event to add
 *
 * Returns: none
 *
 * Description: 
 * 
 * Work function for processEdit.
 * Insert an event into a list given the head and the tail.  
 * This is optimized for the usual case where the events are inserted
 * in order and the new event clock will be equal to or greater than
 * the last event.  If this isn't the case, MidiEvent::insert is used
 * to sort the event within the list.
 *
 * See discussion of CMD_LOOP event rules in event.cxx
 * insertEvent can only be called while processing event lists that
 * were already ordered so we don't have to do anything special here.
 * 
 ****************************************************************************/

PRIVATE void MidiEdit::insertEvent(MidiEvent **head, MidiEvent **tail, 
									MidiEvent *neu)
{
	neu->setNext(NULL);
	if (*tail == NULL) {
		*head = neu;
		*tail = neu;
	}
	else if ((*tail)->getClock() <= neu->getClock()) {
		(*tail)->setNext(neu);
		*tail = neu;
	}
	else
	  // we must insert the neu event from the beginning
	  *head = (*head)->insert(neu);
}

/****************************************************************************
 * MidiEdit::processEdit
 *
 * Arguments:
 *	edit: foo
 *	root: pointer to root of event list
 *
 * Returns: edited event list
 *
 * Description: 
 * 
 * Applies an editing template to a list of events.
 * This is an internal function and is normally called for event lists
 * containing the same type of event.  May not have any dependencies on 
 * this but no guarantees.
 * 
 * UPDATE: Originally written to be called for seperate lists of different
 * types of event.  Now that we have only one event list, will there
 * be problems?
 * 
 ****************************************************************************/

PRIVATE MidiEvent *MidiEdit::processEdit(MidiEvent **root)
{
	MidiEvent 	*new_first, *new_last, *neu;
	MidiEvent	*e, *prev, *next;
	int			length, newclock, in_range;
	
	// initialize the side effect sequence variables
	new_first = new_last = neu = NULL;
	
	// initialize loop iteration variables
	prev = NULL;  
	
	// Find the first event in the range
	for (e = *root ; e != NULL && e->getClock() < start ; e = e->getNext())
	  prev = e;
	
	// Process events in the range
	for (next = NULL ; e != NULL && e->getClock() < end ; e = next) {
		next = e->getNext();
		neu = NULL;
		
		// if this is a NOTEON, it must fall within our top & bottom range
		in_range = (e->getStatus() != MS_NOTEON) ||
			((top == -1 || e->getKey() <= top) &&
			 (bottom == -1 || e->getKey() >= bottom));
		
		if (!in_range) {
			// ignore this event, advance the iteration
			prev = e;
		}
		else {
			// erase or cut ? remove from input list and add to new list
			if (command == MIDI_EDIT_ERASE || command == MIDI_EDIT_CUT)
			  neu = e;
			
			// copy ?, make a copy and add to new list
			else if (command == MIDI_EDIT_COPY)
			  neu = e->copy();
			
			else if (mapper != NULL) {
				// Process the current event, if a new event is returned, it 
				// is added to the side effect list. 
				neu = (*(mapper))(this, e, (void *)args);
			}
			
			// If a "new" event was set, and it is the same as the source
			// event, remove it from the source list.  Add the new event
			// to the side effect list.  Make sure the list is sorted.

			if (neu != NULL) {
				// if from source list, remove
				if (neu == e) {
					if (prev)
					  prev->setNext(next);
					else
					  *root = next;
				}
				
				// insert onto side effect list
				insertEvent(&new_first, &new_last, neu);
			}
			else {
				// no "new" set as a side effect, use the current event for
				// any further modifications
				neu = e;
			}

			// make necessary adjustments to the event
			
			if (neu->getStatus() >= MS_NOTEON) {

				// adjust the clock of the current event
				neu->setClock(neu->getClock() + shift);

				// adjust the transpostion
				neu->setKey(neu->getKey() + transpose);

				// adjust the velocity
				neu->setVelocity(neu->getVelocity() + compression);
			}
			else {
				// Command event, adjust the start time, 
				// the end time is relative.
				neu->setClock(neu->getClock() + shift);
			}
		}
	}
	
	// Here, we've finished with the events in the selected range, 
	// for some operations (i.e. cut), we must now go through the events
	// remaining in the sequence and make adjustments.
	
	if (command == MIDI_EDIT_CUT) {
		// calculate the width of the region in clocks (for cut operations)
		if (end == MIDI_MAX_CLOCK)
		  length = 0;
		else
		  length = end - start;
		
		for ( ; e != NULL ; e = e->getNext()) {
			newclock = e->getClock() - length;
			if (newclock < 0)
			  newclock = 0;
			e->setClock(newclock);
		}
	}
	
	// return the side effect list if any
	return new_first;
}

/****************************************************************************
 * MidiEdit::edit
 *
 * Arguments:
 *	seq: source sequence
 *
 * Returns: new side effect sequence
 *
 * Description: 
 * 
 * Primary sequence editing function.  Other command specific
 * functions just call this one with appropriately initialized templates.
 *
 * Calls processEdit for the event list, returns a new sequence 
 * containing the side effect event lists.
 * 
 ****************************************************************************/

INTERFACE MidiSequence *MidiEdit::edit(MidiSequence *seq)
{
	MidiSequence 	*neu;
	MidiEvent 		*events, *edits;
	
	neu     = NULL;

	// capture the event list from the source sequence
	events	= seq->stealEvents();

	// apply the edits to the list, it may split
	edits 	= processEdit(&events);
	
	// put the edited event list back in the sequence
	seq->setEvents(events);
	
	// if we have a side-effect list, make a new sequence
	if (edits != NULL) {

		if (module != NULL)
		  neu = module->newSequence();
		else
		  neu = new MidiSequence;

		if (neu == NULL)
		  edits->free();
		else {
			// hmm, didn't expect to have "others" copied here, should
			// be filtering them?
			neu->setEvents(edits);

			// what about other stuff?
			neu->setChannel(seq->getChannel());
		}
	}
	
	return neu;
}

/****************************************************************************
 *                                                                          *
 *   						  COMBINATION ENGINE                            *
 *                                                                          *
 ****************************************************************************/

/****************************************************************************
 * MidiEdit::processMerge
 *
 * Arguments:
 *	source: source event list
 *	  dest: destination event list
 *
 * Returns: void
 *
 * Description: 
 * 
 * Work function for MidiEdit::merge.
 *
 * Combines the source event list with the destination event list
 * using parameters contained in the MidiEdit object.
 * If the source list is NULL, we process the destination list for
 * side effects but no merger is performed.
 * 
 ****************************************************************************/

PRIVATE void MidiEdit::processMerge(MidiEvent **source, MidiEvent **dest)
{
	MidiEvent 	*dest_event, *dest_prev; 
	MidiEvent 	*events, *next, *prev, *neu, *e;
	int			start;
	
	// Calculate the starting position of the merger.
	events = (source != NULL) ? *source : NULL;
	if (events == NULL) 
	  start = start_offset;
	else
	  start = events->getClock() + start_offset;
	
	// locate the destination list event where the insertion is to begin
	for (dest_event = *dest, dest_prev = NULL ; 
		 dest_event != NULL && dest_event->getClock() < start ; 
		 dest_event = dest_event->getNext())
	  dest_prev = dest_event;  
	
	// if this is an insertion, adjust start time of subsequent events
	if (push) {
		for (e = dest_event ; e != NULL ; e = e->getNext())
		  e->setClock(e->getClock() + push);
	}
	
	// loop through the source list and combine it with the dest list
	for (e = events, prev = NULL, next = NULL ; e != NULL ; e = next) {
		next = e->getNext();
		
		// Copy or remove the next event in the source list.
		// Actually, since we never set "prev", we'll always be removing
		// from the head of the list. 

		if (command == MIDI_EDIT_PASTE_COPY)
		  neu = e->copy();
		else {
			if (prev != NULL)
			  prev->setNext(next);
			else {
				if (source != NULL)
				  *source = next;
			}
			neu = e;
		}
		
		// adjust the event being inserted
		neu->setClock(neu->getClock() + start_offset);
		neu->setKey(neu->getKey() + transpose);
		
		// Sanity check for illegal offsets, must have increasing event list.
		if (dest_prev != NULL && neu->getClock() < dest_prev->getClock())
		  neu->setClock(dest_prev->getClock());
		
		// move up to the proper position in the destination list
		for ( ; dest_event != NULL && 
				  dest_event->getClock() < neu->getClock() ; 
			  dest_event = dest_event->getNext())
		  dest_prev = dest_event;
		
		// insert the event in the destination list
		neu->setNext(dest_event);
		if (dest_prev == NULL)
		  *dest = neu;
		else
		  dest_prev->setNext(neu);
		dest_prev = neu;
	}
}

/****************************************************************************
 * MidiEdit::sortCommands
 *
 * Arguments:
 *	list: event list pointer
 *
 * Returns: none
 *
 * Description: 
 * 
 * Work function for MidiEdit::merge and possibly others.
 * Because of the additional rules for MIDI_CMD_LOOP events it is
 * more difficult to make sure that they are properly sorted after
 * editing operations since most things deal with start times only.
 * 
 * Rather than muddy up the various editing loops, allow the command
 * list to be sorted properly after some other list operation has
 * been performed.
 *
 * When these were seperated out into seperate lists, it was an easy
 * insertion sort, now we have to destructively modify a larger list.
 * This is a brute force implementation which may be slow for long lists.
 * 
 * MidiEvent::insert has the rules for MIDI_CMD_LOOP insertion.
 * 
 ****************************************************************************/

PRIVATE MidiEvent *MidiEdit::sortCommands(MidiEvent *events)
{
	MidiEvent *e, *prev, *next, *commands;

	// extract all the command events
	commands = NULL;
	for (e = events, prev = NULL ; e != NULL ; e = next) {
		next = e->getNext();
		if (e->getStatus() != MS_CMD_LOOP)
		  prev = e;
		else {
			// remove it
			if (prev == NULL)
			  events = next;
			else
			  prev->setNext(next);

			// add it to the command list
			e->setNext(commands);
			commands = e;
		}
	}

	// now insert the commands one at a time back into the original list
	while (commands != NULL) {
		e = commands;
		commands = e->getNext();
		e->setNext(NULL);
		events = events->insert(e);
	}

	return events;
}

/****************************************************************************
 * MidiEdit::merge
 *
 * Arguments:
 *	source: source sequence
 *	  dest: destination sequence
 *
 * Returns: none
 *
 * Description: 
 * 
 * Merge two sequences into one using the parameters specfied
 * in the MidiEdit object.  See processMerge above for more.
 * 
 * Formerly used boolean options to allow selective merge of different
 * event types.  Now that we keep everything on one list, this is harder,
 * and it was of questionable merit anyway, so its no longer supported.
 * 
 ****************************************************************************/

INTERFACE void MidiEdit::merge(MidiSequence *source, MidiSequence *dest)
{
	MidiEvent *sevents, *devents;
	
	if (dest != NULL) {
		
		// get the event lists
		sevents = source->stealEvents();
		devents = dest->stealEvents();

		processMerge(&sevents, &devents);

		// special post processing for command events
		devents = sortCommands(devents);

		// put them back
		source->setEvents(sevents);
		dest->setEvents(devents);
	}
}

/****************************************************************************
 *                                                                          *
 *   						SEQUENCE OPERERATIONS                           *
 *                                                                          *
 ****************************************************************************/
/*
 * These are operations on the MidiSequence object that are implemented
 * using the MidiEdit object
 *
 */

/****************************************************************************
 * MidiSequence::cut
 *
 * Arguments:
 *	start: start time
 *	  end: end time
 *
 * Returns: new sequence
 *
 * Description: 
 * 
 * Remove all the events between two times and return a sequence containing
 * the removed events.
 * 
 ****************************************************************************/

INTERFACE MidiSequence *MidiSequence::cut(int start, int end)
{
	MidiEdit *edit;
	MidiSequence *result;


	edit = MidiEdit::create(this, MIDI_EDIT_CUT);
	edit->setStart(start);
	edit->setEnd(end);
	
	result = edit->edit(this);
	delete edit;

	return result;
}

/****************************************************************************
 * MidiSequence::paste
 *
 * Arguments:
 *	  dest: destination sequence
 *	offset: paste offset clock
 *
 * Returns: none
 *
 * Description: 
 * 
 * The most common paste operation, overlay without copy.
 * Events from "this" sequence are pasted into the dest sequence.
 * 
 ****************************************************************************/

INTERFACE void MidiSequence::paste(MidiSequence *dest, int offset)
{
	MidiEdit *e;

	e = MidiEdit::create(this, MIDI_EDIT_PASTE);
	e->setStartOffset(offset);

	e->merge(this, dest);
	delete e;
}

/****************************************************************************
 * MidiSequence::clone
 *
 * Arguments:
 *	src: source sequence
 *
 * Returns: none
 *
 * Description: 
 * 
 * Copies one sequence into another, similar to clone but in addition
 * to the events it also copies the various sequence state flags. 
 * !! Why is there a difference ? 
 *
 ****************************************************************************/

INTERFACE void MidiSequence::clone(MidiSequence *dest)
{
	MidiEdit *e;

	e = MidiEdit::create(this, MIDI_EDIT_PASTE_COPY);
	e->merge(this, dest);
	delete e;

	// leave the next & track fields alone !
	dest->setLength(this->getLength());
	dest->setChannel(this->getChannel());

	// what were these?
	// dest->setFilters(this->getFilters());
	// dest->setFlags(this->getFlags());
	

	// replace the properties
	// prop_free(dest->properties);
	// dest->properties = prop_copy(source->properties);

}

/****************************************************************************
 * MidiSequence::copy
 *
 * Arguments:
 *	src: source sequence
 *
 * Returns: a copy of the sequence
 *
 * Description: 
 * 
 * Makes a copy of a sequence.  Implemented using the MidiEdit object,
 * though we could simplify this if necessary.
 ****************************************************************************/

INTERFACE MidiSequence *MidiSequence::copy(void)
{
	MidiSequence *neu;
	
	if (module != NULL)
	  neu = module->newSequence();
	else
	  neu = new MidiSequence;

	if (neu != NULL)
	  this->clone(neu);

	return neu;
}

/****************************************************************************
 *                                                                          *
 *   						  REGION OPERATIONS                             *
 *                                                                          *
 ****************************************************************************/
/* 
 * This is a set of operations that uses the MidiEdit to specify the
 * boundaries of the operation but the operation itself is performed
 * by a mapping function with state.  Consider ways to move some
 * of this into the MidiEdit structure itself.  Not that big a deal.
 *
 * These were all developed for GEE, may be a bit too specific
 * for more general use?
 * 
 * Its a bit odd because the MidiEdit is used only to capture the region
 * boundaries, we hard set the command and other paremeters in here.  I don't 
 * really like this, consider doing something else when we port GEE.
 * 
 */

/****************************************************************************
 *                                                                          *
 *   							  TRANSPOSE                                 *
 *                                                                          *
 ****************************************************************************/

/****************************************************************************
 * MidiSequence::transpose
 *
 * Arguments:
 *	 shift: amount to transpose
 *	region: optional region
 *
 * Returns: none
 *
 * Description: 
 * 
 * Transposes events in a sequence, the region is specified through a
 * MidiEvent object, but we set the other parameters here.
 * 
 ****************************************************************************/

INTERFACE void MidiSequence::transpose(int shift, MidiEdit *region)
{
	MidiEdit *e;

	// operation defined on the region, so we create one if not passed
	e = region;
	if (e == NULL)
	  e = MidiEdit::create(this, MIDI_EDIT_MAP);

	// override settings
	e->setCommand(MIDI_EDIT_MAP);
	e->setTranspose(shift);
	
	// do it
	e->edit(this);
	if (e != region)
	  delete e;
}

/****************************************************************************
 *                                                                          *
 *   							   QUANTIZE                                 *
 *                                                                          *
 ****************************************************************************/

/****************************************************************************
 * processRegion
 *
 * Arguments:
 * 	  region: edit object 
 *	  source: source sequence
 *	function: mapping function
 *	    args: args to mapping function
 *
 * Returns: none
 *
 * Description: 
 * 
 * Work function for most of the region-oriented operations.
 * Initialize a region (if one does not already exist) and map over
 * the events.  The modified events will be removed from 
 * the source sequence and placed in a temporary new sequence.
 * When the operation is complete, paste the temporary sequence
 * back into the source sequence.
 * 
 ****************************************************************************/

PRIVATE void processRegion(MidiEdit *region, 
						   MidiSequence *source, 
						   MIDI_EDIT_MAPPER function, 
						   void *args)
{
	MidiEdit *e;
	MidiSequence *neu;
	
	// initialize an editing template for the region
	e = region;
	if (e == NULL)
	  e = MidiEdit::create(source, MIDI_EDIT_MAP);

	e->setCommand(MIDI_EDIT_MAP);
	e->setMapper(function);
	e->setArgs(args);
	
	// perform the operation
	neu = e->edit(source);

	// delete this if we created it
	if (e != region)
	  delete e;
	
	// merge the modified events back into the source sequence
	neu->paste(source, 0);
	delete neu;
}

/****************************************************************************
 * QUANT_STATE
 *
 * Description: 
 * 
 * State structure used for region quantization.
 * q is the number of clocks in the target "grains"
 ****************************************************************************/

typedef struct quant_state {
	
	int	q;
	int	duration;
	int time;
	
} QUANT_STATE;

/****************************************************************************
 * quantizeEvent
 *
 * Arguments:
 *	  reg: edit/region object
 *	 note: event we're on
 *	state: quantize state
 *
 * Returns: modified event
 *
 * Description: 
 * 
 * A "mapping" function used with MidiEdit to walk over all the events
 * and quantize them.
 * 
 ****************************************************************************/

PRIVATE MidiEvent *quantizeEvent(MidiEdit *reg, MidiEvent *e,
								 void *args)
{
	QUANT_STATE *state = (QUANT_STATE *)args;
	int halfq, dur;
	
	halfq = state->q / 2;
	
	if (e->getClock() > (state->time + halfq))
	  state->time += state->q; 
	else {
		e->setClock(state->time);
		if (state->duration) {
			dur = (e->getDuration() / state->q) * state->q;
			if (dur == 0)
			  dur = state->q;
			e->setDuration(dur);
		}
	}
	return e;
}

/****************************************************************************
 * MidiSequence::quantize
 *
 * Arguments:
 *	  clocks: quant clocks
 *	duration: quant duration
 *	  region: optional region
 *
 * Returns: none
 *
 * Description: 
 * 
 * Quantize a sequence.  Region specified in a MidiEdit object, which
 * we'll create if it isn't passed in.   Other edit fields are overridden
 * as appropriate.
 * 
 ****************************************************************************/

INTERFACE void MidiSequence::quantize(int clocks, int duration,
									  MidiEdit *region)
{
	QUANT_STATE state;
	
	// set up our quantization state
	state.time     = 0;
	state.q        = clocks;
	state.duration = duration;

	processRegion(region, this, quantizeEvent, &state);
}

/****************************************************************************
 *                                                                          *
 *   								 FLIP                                   *
 *                                                                          *
 ****************************************************************************/


/****************************************************************************
 * FLIP_STATE
 *
 * Description: 
 * 
 * State we carry around during a flip operation.
 ****************************************************************************/

typedef struct flip_state {
	
	int flipy;
	
} FLIP_STATE;

/****************************************************************************
 * flipEvent
 *
 * Arguments:
 *	 reg: region/edit
 *	   e: event
 *	stat: flip state
 *
 * Returns: none
 *
 * Description: 
 * 
 * MidiEdit mapper function used to perform a flip operation.
 ****************************************************************************/

PRIVATE MidiEvent *flipEvent(MidiEdit *reg, MidiEvent *event, void *args)
{
	FLIP_STATE *state = (FLIP_STATE *)args;
	int time;
	
	if (state->flipy) {
		if (event->getStatus() == MS_NOTEON)
		  event->setKey((reg->getTop() - event->getKey()) + reg->getBottom());
	}
	else {
		time = event->getClock() - reg->getStart();
		if (event->getStatus() == MS_NOTEON)
		  time += event->getDuration();
		else
		  time += 1;

		if (event->getStatus() != MS_PROG)
		  event->setClock(reg->getEnd() - time);
	}

	return event;
}

/****************************************************************************
 * MidiSequence::flip
 *
 * Arguments:
 *	 flipy: flag to flip Y dimension
 *  region: optional region
 *
 * Returns: none
 *
 * Description: 
 * 
 * Flip the notes in a sequence upside down.
 * 
 ****************************************************************************/

INTERFACE void MidiSequence::flip(int flipy, MidiEdit *region)
{
	FLIP_STATE state;
	
	state.flipy = flipy;
	
	processRegion(region, this, flipEvent, &state);
}

/****************************************************************************
 *                                                                          *
 *   								DURATE                                  *
 *                                                                          *
 ****************************************************************************/

/****************************************************************************
 * DURATE_STATE
 *
 * Arguments:
 *
 * Returns: 
 *
 * Description: 
 * 
 * State we keep around during a "durate" operation.
 ****************************************************************************/

typedef struct durate_state {
	
	MIDI_DURATE	operation;
	int			duration;
	float 		ratio;
	
} DURATE_STATE;

/****************************************************************************
 * durateEvent
 *
 * Arguments:
 *	  reg: edit/region
 *	event: event we're on
 *	state: durate state
 *
 * Returns: none
 *
 * Description: 
 * 
 * MidiEdit map function for the "durate" operation.
 ****************************************************************************/

PRIVATE MidiEvent *durateEvent(MidiEdit *reg, MidiEvent *event, void *args)
{
	DURATE_STATE *state = (DURATE_STATE *)args;

	if (event->getStatus() == MS_NOTEON) {
		if (state->operation == MIDI_DURATE_ABSOLUTE)
		  event->setDuration(state->duration);
		else {
			event->setDuration((int)(event->getDuration() * state->ratio));
			if (event->getDuration() == 0)
			  event->setDuration(1);
		}
	}
	return event;
}

/****************************************************************************
 * MidiSequence::durate
 *
 * Arguments:
 *	   cmd: style of durate
 *	clocks: duration clocks
 *	 ratio: durate ratio
 *	region: optional region
 *
 * Returns: none
 *
 * Description: 
 * 
 * Performs a "durate" operation on the sequence.  
 * This adjusts the durations of all note events in a variety of ways.
 * 
 * Need to figure out what the parameters do...
 ****************************************************************************/

INTERFACE void MidiSequence::durate(MIDI_DURATE cmd, 
									int clocks, float ratio,
									MidiEdit *region)
{
	DURATE_STATE state;
	
	state.operation = cmd;
	state.duration  = clocks;
	state.ratio     = ratio;
	
	processRegion(region, this, durateEvent, (void *)&state);
}

/****************************************************************************
 *                                                                          *
 *   								 FIT                                    *
 *                                                                          *
 ****************************************************************************/

/****************************************************************************
 * FIT_STATE
 *
 * Description: 
 * 
 * State we drag around during a "fit" operation.
 * MIGHT NEED TO USE DOUBLE PRECISION FLOATS HERE ?
 ****************************************************************************/

typedef struct fit_state {
	
	float ratio;
	
} FIT_STATE;

/****************************************************************************
 * fitEvent
 *
 * Arguments:
 *	  reg: region/edit ojbect
 *	    e: event we're on
 *	state: fit state
 *
 * Returns: none
 *
 * Description: 
 * 
 * Mapping function for the "fit" operation.
 ****************************************************************************/

PRIVATE MidiEvent *fitEvent(MidiEdit *reg, MidiEvent *event, void *args)
{
	FIT_STATE *state = (FIT_STATE *)args;
	float start, clock, duration;
	
	start    = (float)reg->getStart();
	clock    = (float)event->getClock();
	duration = (float)event->getDuration();
	
	// have to be careful with controller & name events
	event->setClock((int)(((clock - start) * state->ratio) + start));
	
	if (event->getStatus() == MS_NOTEON) {
		event->setDuration((int)(duration * state->ratio));
		if (event->getDuration() == 0)
		  event->setDuration(1);
	}
	return event;
}

/****************************************************************************
 * MidiSequence::fit
 *
 * Arguments:
 *	 ratio: fit ratio
 *	region: optional region
 *
 * Returns: none
 *
 * Description: 
 * 
 * Modify the duration of events to fit within a certain ratio.
 * I think this means that .5 divides everything in half, 2 multiplies
 * it by 2, etc.
 * 
 ****************************************************************************/

INTERFACE void MidiSequence::fit(float ratio, MidiEdit *region)
{
	FIT_STATE state;
	
	state.ratio = ratio;
	
	processRegion(region, this, fitEvent, (void *)&state);
}

/****************************************************************************
 *                                                                          *
 *   							  VELOCITIZE                                *
 *                                                                          *
 ****************************************************************************/

/****************************************************************************
 * VELO_STATE
 *
 * Arguments:
 *
 * Returns: 
 *
 * Description: 
 * 
 * State we drag around during a "velocitize" operation.
 ****************************************************************************/

typedef struct velo_state {
	
	MIDI_VELO 	operation;
	int   		velocity;
	int   		low;
	int 		startclock;
	float	 	increment;
	
} VELO_STATE;

/****************************************************************************
 * setEventVelocity
 *
 * Arguments:
 *	  e: event
 *	vel: velocity?
 *
 * Returns: none
 *
 * Description: 
 * 
 * Shorthand function to check for proper events and set the velocity.
 * Should we just do this in MidiEvent ? 
 * 
 ****************************************************************************/

PRIVATE void setEventVelocity(MidiEvent *event, int vel)
{
	if (event->getStatus() == MS_NOTEON) {
		if (vel > 127)
		  vel = 127;
		else if (vel < 1) 
		  vel = 1;

		event->setVelocity(vel);
	}
}

/****************************************************************************
 * veloEvent
 *
 * Arguments:
 *	  reg: region/edit
 *	event: event we're on
 *	state: velocitize state
 *
 * Returns: none
 *
 * Description: 
 * 
 * MidiEdit mapping function for the velocitize operation.
 ****************************************************************************/

PRIVATE MidiEvent *veloEvent(MidiEdit *reg, MidiEvent *event, void *args)
{
	VELO_STATE 	*state = (VELO_STATE *)args;
	float 		delta;
	int			inc, vel;
	
	if (event->getStatus() == MS_NOTEON) {
		if (state->operation == VELO_ABSOLUTE) {
			setEventVelocity(event, state->velocity);
		}
		else if (state->operation == VELO_COMPRESS) {

			if (event->getVelocity() > state->velocity)
			  setEventVelocity(event, state->velocity);

			else if (event->getVelocity() < state->low)
			  setEventVelocity(event, state->low);
		}
		else if (state->operation == VELO_RAMP) {
			delta = (float)(event->getClock() - state->startclock);
			inc   = (int)(delta * state->increment);
			vel   = state->velocity + inc;
			setEventVelocity(event, vel);
		}
		else if (state->operation == VELO_INCREMENT) {
			setEventVelocity(event, state->velocity);
			state->velocity += (int)state->increment;
		}
	}
	return event;
}

/****************************************************************************
 * getCoveredClocks
 *
 * Arguments:
 *	     seq: sequence to ponder
 *	     reg: optional region
 *	startvar: returned start clock
 *	deltavar: returned delta clock
 *
 * Returns: none
 *
 * Description: 
 * 
 * Work function for MidiSequence::velocitize
 * Find the total number of clocks used by the notes within the region.
 * This look only at notes, is this enough??
 * 
 ****************************************************************************/

PRIVATE void getCoveredClocks(int *startvar, int *deltavar, 
							  MidiSequence *seq, MidiEdit *reg)
{
	MidiEvent 	*e;
	int			start, end;
	
	start = reg->getStart();
	end   = reg->getEnd();
	e     = seq->firstNote(0);
	
	while ((e != NULL) && (e->getClock() < start)) 
	  e = e->nextEvent();
	
	if (e != NULL) {
		start = e->getClock();
		while ((e != NULL) && (e->getClock() <= reg->getEnd())) {
			end = e->getClock() - 1;
			e   = e->nextEvent();
		}
	}

	// return
	*startvar = start;
	if (end > start)
	  *deltavar = end - start;
	else
	  *deltavar = 0;
}

/****************************************************************************
 * getRegionNoteCount
 *
 * Arguments:
 *	seq: sequence
 *	reg: region
 *
 * Returns: number of notes
 *
 * Description: 
 * 
 * Work function for MidiSequence::velocitize.
 * Count the number of note events within a region.
 * 
 ****************************************************************************/

PRIVATE int getRegionNoteCount(MidiSequence *seq, MidiEdit *reg)
{
	MidiEvent 	*e;
	int		   	count;
	
	count = 0;
	e = seq->firstNote(0);
	
	while ((e != NULL) && (e->getClock() < reg->getStart())) 
	  e = e->nextEvent();
	
	while ((e != NULL) && (e->getClock() <= reg->getEnd())) {
		count++;
		e = e->nextEvent();
	}
	
	return count;
}

/****************************************************************************
 * MidiSequence::velocitize
 *
 * Arguments:
 *	 cmd: style of operation
 *	 vel: velocity to set
 *	vel2: secondary velocity
 *	 reg: optional region
 *
 * Returns: none
 *
 * Description: 
 * 
 * Adjust the velocities of the notes in a region.
 * 
 * Need to find move about how the various paratmers work!
 ****************************************************************************/

INTERFACE void MidiSequence::velocitize(MIDI_VELO cmd, int vel, int vel2,
										MidiEdit *reg)

{
	VELO_STATE 	state;
	int 		start, end, high, low, d;
	float 		delta1, delta2, count;
	
	state.operation = cmd;
	state.velocity  = vel;
	
	if (cmd != VELO_ABSOLUTE) {
		start = vel;
		end   = vel2;
		if (start > end) {
			high = start;
			low  = end;
		}
		else {
			high = end;
			low  = start;
		}
		delta1 = (float)(end - start);
		state.velocity  = start;
		state.increment = (float)0.0;
		if (cmd == VELO_COMPRESS) {
			state.velocity = high;
			state.low      = low;
		}
		else if (cmd == VELO_RAMP) {
			getCoveredClocks(&state.startclock, &d, this, reg);
			if (d != 0) {
				delta2 = (float)d;
				state.increment = (delta1 / delta2);
			}
		}
		else {
			count = (float) getRegionNoteCount(this, reg);
			if (count)
			  state.increment = delta1 / (count - 1);
		}
	}
	
	processRegion(reg, this, veloEvent, (void *)&state);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

