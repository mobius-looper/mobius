/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Platform-independent representation of a MIDI event.
 *
 * This is dependent on the "xom" XML utility for serialization.
 * Consider factoring out a MIDI model serializer.
 *
 */

#include <stdio.h>
#include <memory.h>
#include <string.h>

#include "Util.h"
#include "XmlModel.h"
#include "XomParser.h"
#include "XmlBuffer.h"

#include "MidiByte.h"
#include "MidiEvent.h"

/****************************************************************************
 *                                                                          *
 *   							 CONSTRUCTORS                               *
 *                                                                          *
 ****************************************************************************/

/**
 * Allocate a new event object.
 */
PUBLIC MidiEvent::MidiEvent()
{
	init();
}

/**
 * Initialize an event object from XML.
 */
PUBLIC MidiEvent::MidiEvent(XmlElement* e) 
{
	init();
	parseXml(e);
}

/**
 * Initialize a new event.
 */
PUBLIC void MidiEvent::init()
{
    mManager	= NULL;
    mNext		= NULL;
    mStack 		= NULL;
    mClock 		= 0;
    mStatus		= 0;
    mChannel 	= 0;
    mKey 		= 0;
    mVelocity 	= 0;
    mDuration 	= 0;
    mExtra 		= 0;
    mData 		= NULL;
}

/**
 * Initializes a previously constructed event for use as something else, 
 * or to be returned to the free pool.  Basically this just frees any
 * attached storage such as the name or sysex buffer.
 * 
 * Do NOT clear the "next" field here, we're typically being called
 * from a list processor to return a list of events to the pool.
 * Also do not clear the env field or else we won't know how
 * to return ourselves to the pool.
 */
PUBLIC void MidiEvent::reinit(void)
{
	mStack 		= NULL;
	mClock 		= 0;
	mStatus 		= 0;
	mChannel 	= 0;
	mKey 		= 0;
	mVelocity 	= 0;
	mDuration 	= 0;
	mExtra 		= 0;

	// if this is a name event, and data is set, free it?
	if (mData != NULL) {
		if (mStatus == MS_PROGRAM || mStatus == MS_NAME || mStatus == MS_SYSEX)
		  delete (char *)mData;
		mData = NULL;
	}
}

/**
 * Delete the event any all others on the list.
 * This is normally not called by an application, use the free()
 * method instead to allow the events to be returned to the manager's pool.
 */
PUBLIC MidiEvent::~MidiEvent(void)
{
	MidiEvent *el, *nextel;

	// release attached storage
	reinit();

	// free the remainder of the list
    for (el = mNext, nextel = NULL ; el != NULL ; el = nextel) {
        nextel = el->mNext;
        el->mNext = NULL;	// prevent recursion
        delete el;
    }
}

/**
 * Returns a list of events to the free pool.
 * If these events were created with new, rather than 
 * from a MidiEventManager, then they're just deleted.
 */
PUBLIC void MidiEvent::free(void)
{
	if (mManager != NULL)
	  mManager->freeMidiEvents(this);
	else
	  delete this;
}

/**
 * Copy an event, uses the same event pool if we have one.
 */
PUBLIC MidiEvent *MidiEvent::copy(void)
{
	MidiEvent *e = NULL;

	if (mManager != NULL)
	  e = mManager->newMidiEvent();
	else
	  e = new MidiEvent();

	if (e != NULL) {

		// mNext and mStack are not copied!

		e->mClock = mClock;
		e->mStatus = mStatus;
		e->mChannel = mChannel;
		e->mKey = mKey;
		e->mVelocity = mVelocity;
		e->mDuration = mDuration;
		e->mExtra = mExtra;

		// copy name if it exists
		if (mStatus == MS_PROGRAM || mStatus == MS_NAME) {
			if (mData != NULL)
			  e->mData = (const char *)CopyString((char *)mData);
		}
		else if (mStatus == MS_SYSEX) {
			if (mData != NULL) {
				e->mData = new char[mDuration];
				if (e->mData != NULL)
				  memcpy((char *)e->mData, (char *)mData, mDuration);
			}
		}
		else {
			// dangerous ??
			e->mData = mData;
		}
	}
	
	return e;
}

/**
 * Dumps debugging info about the event.
 */
PUBLIC void MidiEvent::dump(bool simple)
{
	int i;
  
	if (simple) {
	  printf("Event st=%d ch=%d cl=%d k=%d v=%d d=%d extra=%d\n",
			 (int)mStatus, 
			 (int)mChannel,
			 (int)mClock,
			 (int)mKey,
			 (int)mVelocity,
			 (int)mDuration,
			 (int)mExtra
		  );
    }
	else {
		switch (mStatus) {
			case MS_NOTEOFF:
				printf("OFF %d\n", (int)mKey);
				break;
			case MS_NOTEON:
				printf("ON %d V %d ", (int)mKey, (int)mVelocity);
				if (mDuration)
				  printf("D %d", (int)mDuration);
				printf("\n");
				break;
			case MS_POLYPRESSURE:
				printf("PP %d %d\n", (int)mKey, (int)mVelocity);
				break;
			case MS_CONTROL:
				printf("C %d %d\n", (int)mKey, (int)mVelocity);
				break;
			case MS_PROGRAM:
				printf("P %d ", (int)mKey);
				if (mData != NULL)
				  printf("%s", (char*)mData);
				printf("\n");
				break;
			case MS_NAME:
				printf("NAME %s\n", (char*)mData);
				break;
			case MS_TOUCH:
				printf("T %d\n", (int)mKey);
				break;
			case MS_BEND:
				printf("PB %d %d\n", (int)mKey, (int)mVelocity);
				break;

			case MS_SYSEX: {
				char *str = (char *)mData;
				printf("Sysex %d\n", (int)mDuration);
				for (i = 0 ; i < mDuration ; i++)
				  printf("%2x ", str[i]);
				printf("\n");
			}
			break;

			case MS_QTRFRAME:
				printf("Quarter frame\n");
				break;
			case MS_SONGPOSITION:
				printf("Song position %d %d\n", (int)mKey, (int)mVelocity);
				break;
			case MS_SONGSELECT:
				printf("Song select %d\n", (int)mKey);
				break;
			case MS_TUNEREQ:
				printf("Tune request\n");
				break;
			case MS_EOX:
				printf("EOX\n");
				break;

			case MS_CLOCK: 	printf("Clock\n"); break;
			case MS_START: 	printf("Start\n"); break;
			case MS_CONTINUE: 	printf("Continue\n"); break;
			case MS_STOP: 	printf("Stop\n"); break;
			case MS_SENSE: 	printf("Sense\n"); break;
			case MS_RESET: 	printf("Reset\n"); break;

			default:
				printf("Unknown status %d %d %d %d\n", 
					   (int)mStatus, (int)mChannel, (int)mKey, (int)mVelocity);
				break;
		}
	}
}

/****************************************************************************
 *                                                                          *
 *   						  LIST CONSTRUCTORS                             *
 *                                                                          *
 ****************************************************************************/

/**
 * Find the last event in the list with a particular status.
 * If stauts is zero, the last event in the list is returned.
 */
PUBLIC MidiEvent *MidiEvent::getLast(int status)
{
    MidiEvent* last = NULL;
    for (MidiEvent* e = this ; e != NULL ; e = e->getNext()) {
		if (status == 0 || status == e->getStatus()) {
            last = e;
            break;
        }
	}
	return last;
}

/**
 * Finds the next event of the same type as this one in the list.
 */
PUBLIC MidiEvent *MidiEvent::getNextEvent(void)
{
	MidiEvent *next;

	next = NULL;
	if (this != NULL) {
		for (next = this->getNext() ; next != NULL ; next = next->getNext()) {
			if (mStatus == next->getStatus())
			  break;	// this is it
		}
	}

	return next;
}

/**
 * Inserts an event in the list, ordered by clock.
 * "this" is assumed to be the head of the list, this is returned
 * if it continues to be the head, if "e" needs to be the new head, it
 * is returned.
 *
 * MS_CMD_LOOP events have a few special rules.  For all events on a clock, 
 * loops must be ordered according to decending duration and must be
 * before any other events on this clock.
 * Since the general insertion behavior is to always add events
 * at the front of the list for any given clock, this shouldn't require
 * any additional work.
 * 
 */
PUBLIC MidiEvent *MidiEvent::insert(MidiEvent *neu)
{
	MidiEvent *list, *event, *prev;

	list 	= this;
	prev	= NULL;
	event	= list;

	while (event && (event->mClock < neu->mClock)) {
		prev  = event;
		event = event->mNext;
	}

	// If this isn't a MS_CMD_LOOP event and there are loop events on this
	// clock, make sure the event goes after the loops.

	if (neu->mStatus != MS_CMD_LOOP) {
		while (event != NULL && event->mClock == neu->mClock && 
			   event->mStatus == MS_CMD_LOOP) {
			prev  = event;
			event = event->mNext;
		}
	}
	else {
		// For MS_CMD_LOOP events, loops on the same clock must be ordered in 
		// decending order of length (e.g., those with the highest loop end
		// times must be first.

		while (event != NULL && 
			   event->mClock == neu->mClock &&
			   event->mStatus == MS_CMD_LOOP && 
			   event->mDuration > neu->mDuration) {
			prev = event;
			event = event->mNext;
		}
	}

	neu->mNext = event;
	if (prev)
	  prev->mNext = neu;
	else
	  list = neu;

	return list;
}

/**
 * Like insert(), but only allows one event at this key/clock position.
 * Used for control, program and name events.
 * This cannot be used for MS_CMD_LOOP events, if you try, it
 * will call the normal insert() method.
 */
PUBLIC MidiEvent *MidiEvent::replace(MidiEvent *neu)
{
	MidiEvent *list, *event, *prev, *next;
	int found;

	// no such thing as loop override, do a normal insert
	if (neu->mStatus == MS_CMD_LOOP) {
		list = insert(neu);
	}
	else {
		list  = this;
		event = list;
		prev  = NULL;

		while ((event != NULL) && (event->mClock < neu->mClock)) {
			prev  = event;
			event = event->mNext;
		}

		while ((event != NULL) && (event->mClock == neu->mClock)) {
			next = event->mNext;
			found = 0;

			// is this the same kind of thing?
			if (event->mStatus == neu->mStatus) {

                // these match without qualification
				if (neu->mStatus == MS_PROGRAM ||
					neu->mStatus == MS_BEND || 
					neu->mStatus == MS_TOUCH)
				  found = 1;

                else if (neu->mStatus == MS_NOTEON ||
                         neu->mStatus == MS_NOTEOFF ||
                         neu->mStatus == MS_CONTROL ||
                         neu->mStatus == MS_POLYPRESSURE) {
                    
                    // key/controller must also match
                    if (neu->mKey == event->mKey)
                      found = 1;
                }
            }

			if (found) {
				// remove the event being replaced
 				if (prev != NULL)
				  prev->mNext = next;
				else
				  list = next;

				event->mNext = NULL;
				delete event;
			}
			else
			  prev = event;

			event = next;
		}

		neu->mNext = event;
		if (prev != NULL)
		  prev->mNext = neu;
		else
		  list = neu;
	}

	return list;
}

/**
 * Removes an event from the list.
 * The event is NOT deleted, that's up to the caller.
 * "This" is the head of the list, the new list head is returned.
 */
PUBLIC MidiEvent *MidiEvent::remove(MidiEvent *e)
{
	MidiEvent *list, *ev, *prev;

	list = this;
	for (ev = list ; ev != NULL && ev != e ; ev = ev->mNext)
	  prev = ev;

	if (ev == NULL) {
		// not on the list, ignore
	}
	else {
		if (prev != NULL)
		  prev->mNext = e->mNext;
		else
		  list = e->mNext;

		e->mNext = NULL;
	}

	return list;
}

/****************************************************************************
 *                                                                          *
 *   								 XML                                    *
 *                                                                          *
 ****************************************************************************/
/*
 *
 * This is not meant for management of large sequences, but I wanted
 * a readable text format for diagnostics.  A reader/writer for standard
 * MIDI files will also be available.
 *
 * I decided to go with a very terse vocabulary to cut down on file
 * size.  It's not "purist" XML, but easier to sift through.
 *
 * <note t='11111' c='1' k='42' v='127' d='23'/>		   
 *  - time(clock), channel, key, velocity, duration
 * 
 * <prog t='1111' c='1' p='100' n='Grand Piano'/>
 *  - time, channel, program, name
 * 
 * <ctrl t='1111' c='1' n='7' v='127'/>
 *  - time, channel, controller, value
 *
 * <touch t='111' c='1' k='42' v='127'/>
 *  - time, key, value, if k is missing its channel pressure
 *
 * <bend t='111' v='123123'/>
 *  - time, value
 *
 * <mode t='111' m='localControl' v='127'/>
 *  - time, channel, mode, value 
 *    mode: localControl, allNotesOff, omniOff, omniOn, monoOn, polyOn 
 *
 * <psn t='111' v='11111'/>
 *  - time, song position
 * 
 * <song t='111' v='1'/>
 *  - time, song select
 * 
 * <tune t='111'/>
 *
 * <sysex t='111'>....</sysex>
 *
 * <start t='111'/>
 * <continue t='111'/>
 * <stop t='111'/>
 * <reset t='111/>
 */

#define EL_NOTE "note"
#define EL_PROGRAM "prog"
#define EL_CONTROL "ctrl"
#define EL_TOUCH "touch"
#define EL_BEND "bend"
#define EL_MODE "mode"
#define EL_SONGPSN "psn"
#define EL_SONGSEL "song"
#define EL_TUNE "tune"
#define EL_SYSEX "sysex"
#define EL_START "start"
#define EL_STOP "stop"
#define EL_CONTINUE "continue"
#define EL_RESET "reset"
  
#define ATT_TIME "t"
#define ATT_CHANNEL "c"
#define ATT_KEY "k"
#define ATT_VELOCITY "v"
#define ATT_DURATION "d"
#define ATT_PROGRAM "p"
#define ATT_NAME "n"
#define ATT_CONTROLLER "n"
#define ATT_VALUE "v"
#define ATT_MODE "m"

PUBLIC void MidiEvent::parseXml(XmlElement* e)
{
	mClock = e->getIntAttribute(ATT_TIME);
	mChannel = e->getIntAttribute(ATT_CHANNEL);

	const char *tag = e->getName();
	if (!strcmp(tag, EL_NOTE)) {
		mStatus = MS_NOTEON;
		mKey = e->getIntAttribute(ATT_KEY);
		mVelocity = e->getIntAttribute(ATT_VELOCITY);
		mDuration = e->getIntAttribute(ATT_DURATION);
	}
	else if (!strcmp(tag, EL_PROGRAM)) {
		mStatus = MS_PROGRAM;
		mKey = e->getIntAttribute(ATT_PROGRAM);
	}
	else if (!strcmp(tag, EL_CONTROL)) {
		mStatus = MS_CONTROL;
		mKey = e->getIntAttribute(ATT_CONTROLLER);
		mVelocity = e->getIntAttribute(ATT_VALUE);
	}
	else if (!strcmp(tag, EL_TOUCH)) {
		// ! need to distinguish between key value zero and missing attribute
		mKey = e->getIntAttribute(ATT_KEY);
		if (mKey == 0)
		  mStatus = MS_TOUCH;
		else
		  mStatus = MS_POLYPRESSURE;
		mVelocity = e->getIntAttribute(ATT_VALUE);
	}
	else if (!strcmp(tag, EL_BEND)) {
		mStatus = MS_BEND;
		mVelocity = e->getIntAttribute(ATT_VALUE);
	}
	else if (!strcmp(tag, EL_MODE)) {
		// not supported right now, these are rare
		mStatus = MS_SENSE;
	}
	else if (!strcmp(tag, EL_SONGPSN)) {
		mStatus = MS_SONGPOSITION;
		mKey = e->getIntAttribute(ATT_VALUE);
	}
	else if (!strcmp(tag, EL_SONGSEL)) {
		mStatus = MS_SONGSELECT;
		mKey = e->getIntAttribute(ATT_VALUE);
	}
	else if (!strcmp(tag, EL_TUNE)) {
		mStatus = MS_TUNEREQ;
	}
	else if (!strcmp(tag, EL_SYSEX)) {
		// todo...
	}
	else if (!strcmp(tag, EL_START)) {
		mStatus = MS_START;
	}
	else if (!strcmp(tag, EL_STOP)) {
		mStatus = MS_STOP;
	}
	else if (!strcmp(tag, EL_CONTINUE)) {
		mStatus = MS_CONTINUE;
	}
	else if (!strcmp(tag, EL_RESET)) {
		mStatus = MS_RESET;
	}
	else {	
		// somethign we don't recognize, filter these later
		mStatus = MS_SENSE;
	}
}

PUBLIC void MidiEvent::toXml(XmlBuffer* b)
{
	if (mStatus == MS_NOTEON) {
		b->addOpenStartTag(EL_NOTE);
		b->addAttribute(ATT_TIME, mClock);
		b->addAttribute(ATT_KEY, mKey);
		b->addAttribute(ATT_VELOCITY, mVelocity);
		b->addAttribute(ATT_DURATION, mDuration);
		b->add("/>\n");
	}
	else if (mStatus == MS_PROGRAM) {
		b->addOpenStartTag(EL_PROGRAM);
		b->addAttribute(ATT_TIME, mClock);
		b->addAttribute(ATT_PROGRAM, mKey);
		b->add("/>\n");
	}
	else if (mStatus == MS_CONTROL) {
		b->addOpenStartTag(EL_CONTROL);
		b->addAttribute(ATT_TIME, mClock);
		b->addAttribute(ATT_CONTROLLER, mKey);
		b->addAttribute(ATT_VALUE, mVelocity);
		b->add("/>\n");
	}
	else if (mStatus == MS_TOUCH) {
		b->addOpenStartTag(EL_TOUCH);
		b->addAttribute(ATT_TIME, mClock);
		b->addAttribute(ATT_VALUE, mVelocity);
		b->add("/>\n");
	}
	else if (mStatus == MS_POLYPRESSURE) {
		b->addOpenStartTag(EL_TOUCH);
		b->addAttribute(ATT_TIME, mClock);
		b->addAttribute(ATT_KEY, mKey);
		b->addAttribute(ATT_VALUE, mVelocity);
		b->add("/>\n");
	}
	else if (mStatus == MS_BEND) {
		b->addOpenStartTag(EL_BEND);
		b->addAttribute(ATT_TIME, mClock);
		b->addAttribute(ATT_VALUE, mVelocity);
		b->add("/>\n");
	}
	else if (mStatus == MS_SONGPOSITION) {
		b->addOpenStartTag(EL_SONGPSN);
		b->addAttribute(ATT_TIME, mClock);
		b->addAttribute(ATT_VALUE, mVelocity);
		b->add("/>\n");
	}
	else if (mStatus == MS_SONGSELECT) {
		b->addOpenStartTag(EL_SONGSEL);
		b->addAttribute(ATT_TIME, mClock);
		b->addAttribute(ATT_VALUE, mVelocity);
		b->add("/>\n");
	}
	else if (mStatus == MS_TUNEREQ) {
		b->addOpenStartTag(EL_TUNE);
		b->addAttribute(ATT_TIME, mClock);
		b->add("/>\n");
	}
	else if (mStatus == MS_SYSEX) {
	}
	else if (mStatus == MS_START) {
		b->addOpenStartTag(EL_START);
		b->addAttribute(ATT_TIME, mClock);
		b->add("/>\n");
	}
	else if (mStatus == MS_STOP) {
		b->addOpenStartTag(EL_STOP);
		b->addAttribute(ATT_TIME, mClock);
		b->add("/>\n");
	}
	else if (mStatus == MS_CONTINUE) {
		b->addOpenStartTag(EL_CONTINUE);
		b->addAttribute(ATT_TIME, mClock);
		b->add("/>\n");
	}
	else if (mStatus == MS_RESET) {
		b->addOpenStartTag(EL_RESET);
		b->addAttribute(ATT_TIME, mClock);
		b->add("/>\n");
	}
	else {
		// filter all others
	}
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
