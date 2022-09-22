/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * A model for track events and an event list.
 * Most events should be allocated and freed through EventManager.
 * A very few places (Synchronizer, MidiQueue, MidiTransport) may allocate
 * simple events to represent sync events.
 *
 * See EventManager for more on the relationship between events.
 *
 */

#include <stdio.h>
#include <memory.h>

#include "Trace.h"
#include "Util.h"

#include "Action.h"
#include "Event.h"
#include "EventManager.h"
#include "Loop.h"
#include "Mobius.h"
#include "Script.h"
#include "Track.h"

//////////////////////////////////////////////////////////////////////
//
// Globals
//
//////////////////////////////////////////////////////////////////////

/**
 * Return a static string representation of a SyncPulseType value.
 * Intended for trace.
 */
PUBLIC const char* GetSyncPulseTypeName(SyncPulseType type)
{
    const char* name = "???";
    switch (type) {
        case SYNC_PULSE_UNDEFINED: name = "Undefined"; break;
        case SYNC_PULSE_CLOCK: name = "Clock"; break;
        case SYNC_PULSE_BEAT: name = "Beat"; break;
        case SYNC_PULSE_BAR: name = "Bar"; break;
        case SYNC_PULSE_SUBCYCLE: name = "Subcycle"; break;
        case SYNC_PULSE_CYCLE: name = "Cycle"; break;
        case SYNC_PULSE_LOOP: name = "Loop"; break;
    }
    return name;
}

//////////////////////////////////////////////////////////////////////
//
// Base EventType
//
//////////////////////////////////////////////////////////////////////

PUBLIC EventType::EventType()
{
	name = NULL;
	displayName = NULL;
	reschedules = false;
	noUndo = false;
	noMode = false;
}

PUBLIC const char* EventType::getDisplayName()
{
	return ((displayName != NULL) ? displayName : name);
}

/**
 * By default we forward to the function's event handler.
 * This may also be overloaded in a subclass but it is rare.
 */
PUBLIC void EventType::invoke(Loop* l, Event* e)
{
	if (e->function == NULL)
	  Trace(l, 1, "Cannot do event, no associated function!\n");
	else
	  e->function->doEvent(l, e);
}

/**
 * By default we forward to the function's undo handler.
 * This may also be overloaded in a subclass.
 */
PUBLIC void EventType::undo(Loop* l, Event* e)
{
	if (e->function == NULL)
	  Trace(l, 1, "Cannot undo event, no associated function!\n");
	else
	  e->function->undoEvent(l, e);
}

/**
 * By default we forward to the function's confirm handler.
 * This may also be overloaded in a subclass.
 */
PUBLIC void EventType::confirm(Action* action, Loop* l, Event* e, long frame)
{
    if (e->function == NULL)
	  Trace(l, 1, "Cannot confrim event, no associated function!\n");
	else
	  e->function->confirmEvent(action, l, e, frame);
}

/**
 * Default move event handler.
 * This was originally here for a speed recaluation which it turns
 * out we can defer, so we don't really need this abstraction.
 */
PUBLIC void EventType::move(Loop* l, Event* e, long newFrame)
{
    EventManager* em = l->getTrack()->getEventManager();
    em->moveEvent(l, e, newFrame);
}

/****************************************************************************
 *                                                                          *
 *                                 EVENT POOL                               *
 *                                                                          *
 ****************************************************************************/

EventPool::EventPool()
{
    mEvents = NULL;
    mAllocated = 0;
}

EventPool::~EventPool()
{
}

/**
 * Allocate an event from the pool.
 */
Event* EventPool::newEvent()
{
    Event* e = NULL;

    if (mEvents != NULL) {
        e = mEvents->getEvents();
        if (e != NULL) {
            mEvents->remove(e);
            e->init();
            e->setPooled(false);
        }
    }

    if (e == NULL) {
        e = new Event(this);
        mAllocated++;
    }

	return e;
}

/**
 * The core event freeer.
 *
 * Ignore if the event has a parent, the event will be freed later
 * when the parent is freed.  If there are any processed children
 * free them also.  If there are unprocessed children, leave them
 * alone unless the "freeAll" flag is set, they may still be scheduled.
 * I'm not sure this can happen, it's probably an error in event timing?
 */
void EventPool::freeEvent(Event* e, bool freeAll)
{
	// ignore if we have a parent, or are "owned"
	if (e != NULL && e->getParent() == NULL && !e->isOwned()) {

		if (e->isPooled()) {
			// shouldn't happen if we're managing correctly
			Trace(1, "Freeing event already in the pool!\n");
		}
		else {
			// Just to be safe, let the script interpreter know in case
			// it is still waiting on this.  Shouldn't happen if
			// we're processing events properly.
			ScriptInterpreter* script = e->getScript();
			if (script != NULL) {
				// return strue if we were actually waiting on this
				if (script->cancelEvent(e))
				  Trace(1, "Attempt to free an event a script is waiting on!\n");
				e->setScript(NULL);
			}

			// if we have children, set them free
			Event* next = NULL;
			for (Event* child = e->getChildren() ; child != NULL ; child = next) {
				next = child->getSibling();

				// NOTE: In a few special cases for shared events,
				// we may have something on our child list we don't own
				if (child->getParent() == e) {

					if (freeAll || child->processed) {
						child->setParent(NULL);
						freeEvent(child, freeAll);
					}
					else {
						Trace(1, "Freeing event with unprocessed children! %s/%s\n",
							  e->type->name, child->type->name);
						child->setParent(NULL);
					}
				}
			}

            // !! normally have a csect around list manipulations
			if (e->getList() != NULL) {
                Trace(1, "Freeing event still on a list!\n");
                e->getList()->remove(e);
            }

            // Should not still have an Action, if we do it is usually
            // an ownership error, be safe and let it leak 
            Action* action = e->getAction();
            if (action != NULL) {
                Trace(1, "Event::freeEvent leaking Action!\n");
                if (action->getEvent() == e)
                  action->detachEvent(e);
                e->setAction(NULL);
            }

            if (mEvents == NULL)
              mEvents = new EventList();
			mEvents->add(e);
		}
	}
}

/**
 * Static method to free a list of events chained on the next pointer.
 * This was added for Synchronizer and sync events, be careful because
 * this isn't always applicable to other event lists.
 */
void EventPool::freeEventList(Event* event)
{
    while (event != NULL) {
        Event* next = event->getNext();

        // second arg is freeAll
        freeEvent(event, true);

        event = next;
    }
}

/**
 * Presumably called during application shutdown to reclaim the event
 * pool.  It has never been implemented.  I guess this means it leaks
 * of you bring VST plugins up and down.
 */
void EventPool::flush()
{
	// do we really need this?
}

void EventPool::dump()
{
    int pooled = 0;
    if (mEvents != NULL) {
        for (Event* head = mEvents->getEvents() ; head != NULL ; 
             head = head->getNext())
          pooled++;
    }

    printf("EventPool: %d allocated, %d in the pool, %d in use\n",
           mAllocated, pooled, mAllocated - pooled);
    fflush(stdout);
}

/****************************************************************************
 *                                                                          *
 *                                   EVENT                                  *
 *                                                                          *
 ****************************************************************************/

Event::Event(EventPool* pool)
{
	init();

    mPool = pool;
    mPooled = false;

	// this is permanent
	mOwned = false;

	// this will be allocated as needed, but not reinitialized  every time
	mPreset = NULL;
}

void Event::init()
{
	processed	= false;
	reschedule	= false;
	pending		= false;
	immediate	= false;
	type		= RecordEvent;
	function  	= NULL;
	number		= 0;
	down		= true;
	longPress	= false;
	frame		= 0;
	latencyLoss = 0;
	quantized 	= false;
	afterLoop 	= false;
	pauseEnabled = false;
	automatic	= false;
	insane		= false;
    fadeOverride = false;
    silent      = false;

	mOwned		= false;
    mList       = NULL;
	mNext		= NULL;
	mParent		= NULL;
	mChildren	= NULL;
	mSibling	= NULL;
	mPresetValid = false;
	mScript     = NULL;
    mAction     = NULL;
	mInvokingFunction = NULL;
    mArguments  = NULL;

    mInfo[0] = 0;

    int fsize = sizeof(fields);
    memset(&fields, 0, fsize);
}

void Event::init(EventType* etype, long eframe)
{
	init();
	type = etype;
	frame = eframe;
}

/**
 * This should only be called by EventPool.
 * Most system code should use one of the free methods.
 */
Event::~Event()
{
	//printf("deleted %p\n", this);
}

/**
 * Free this event and the processed children, but leave the unprocessed
 * children.
 */
void Event::free()
{
    if (mPool != NULL)
      mPool->freeEvent(this, false);
    else
      Trace(1, "Event::free no pool!\n");
}

/**
 * Free this event and all children even if not processed.
 */
void Event::freeAll()
{
    if (mPool != NULL)
      mPool->freeEvent(this, true);
    else
      Trace(1, "Event::freeAll no pool!\n");
}

PUBLIC void Event::setPooled(bool b)
{
    mPooled = b;
}

PUBLIC bool Event::isPooled()
{
    return mPooled;
}

PUBLIC void Event::setOwned(bool b)
{
    mOwned = b;
}

PUBLIC bool Event::isOwned()
{
    return mOwned;
}

PUBLIC void Event::setList(EventList* list)
{
    mList = list;
}

PUBLIC EventList* Event::getList()
{
    return mList;
}

PUBLIC void Event::setNext(Event* e) 
{
    mNext = e;
}

PUBLIC Event* Event::getNext()
{
    return mNext;
}

PUBLIC void Event::setSibling(Event* e) 
{
    mSibling = e;
}

PUBLIC Event* Event::getSibling()
{
    return mSibling;
}

PUBLIC void Event::setParent(Event* parent) 
{
    mParent = parent;
}

PUBLIC Event* Event::getParent()
{
    return mParent;
}

PUBLIC Event* Event::getChildren()
{
    return mChildren;
}

Track* Event::getTrack() 
{
	return mTrack;
}

void Event::setTrack(Track* t) 
{
	mTrack = t;
}

/**
 * The interpreter that scheduled the event.
 */
ScriptInterpreter* Event::getScript() 
{
	return mScript;
}

void Event::setScript(ScriptInterpreter* si) 
{
	mScript = si;
}

/**
 * The script arguments.
 */
ExValueList* Event::getArguments()
{
    return mArguments;
}

/**
 * Release the script arguments.
 * This isn't done automatically, why!?
 * Intead of having every function do this just do it when we return
 * the event to the pool.
 */
void Event::clearArguments()
{
    delete mArguments;
    mArguments = NULL;
}

void Event::setArguments(ExValueList* args)
{
    // shouldn't see this?
    if (mArguments != NULL) {
        Trace(1, "Replacing arguments in event");
        delete mArguments;
    }
    mArguments = args;
}

PUBLIC void Event::setAction(Action* a)
{
    // this is probably okay but I want to start removing this
    // yes, it happens with Action::changeEvent
    if (mAction == NULL && mInvokingFunction != NULL) 
      Trace(2, "Event::setAction already had an invoking function\n");

    if (a != NULL && mInvokingFunction != NULL && 
        mInvokingFunction != a->getFunction())
      Trace(1, "Event::setAction mismatched action/invoking function\n");

    mAction = a;
}

PUBLIC Action* Event::getAction()
{
    return mAction;
}

PUBLIC void Event::setInvokingFunction(Function* f)
{
    mInvokingFunction = f;
    if (mAction != NULL && mAction->getFunction() != f) {
        // I don't think this should allowed
        Trace(1, "Event::setInvokingFunction mismatched action/invoking function\n");
    }
    else if (mAction == NULL) {
        // this is okay, but I want to start weeding them out
        Trace(2, "Event::setInvokingFunction without action\n");
    }
}

/**
 * Continue supporting an explicitly set function, but
 * fall back to the Action if we have one.
 */
PUBLIC Function* Event::getInvokingFunction()
{
    Function* f = mInvokingFunction;
    if (f == NULL && mAction != NULL)
      f = mAction->getFunction();
    return f;
}

/**
 * Make a copy of the current preset parameter values. 
 * Leave the copy around so we gradually have one for all events in 
 * the pool.
 *
 * !! This should be an inline object!
 */
void Event::savePreset(Preset* p)
{
	if (p == NULL)
	  mPresetValid = false;
	else {
		if (mPreset == NULL)
		  mPreset = new Preset();
		mPreset->copy(p);
		mPresetValid = true;
	}
}

Preset* Event::getPreset()
{
	return ((mPresetValid) ? mPreset : NULL);
}

const char* Event::getName()
{
	return type->name;	
}

const char* Event::getFunctionName()
{
	// since this is only used for trace, always return a non-null value
	return (function != NULL) ? function->getName() : "";
}

const char* Event::getInfo()
{
    return (mInfo[0] != 0) ? mInfo : NULL;
}

void Event::setInfo(const char* src)
{
    if (src == NULL)
      mInfo[0] = 0;
    else
      CopyString(src, mInfo, sizeof(mInfo));
}

/**
 * Add a child event to the end of the list.
 */
void Event::addChild(Event* e)
{
	// this is now happening when we stack events under a SwitchEvent
	// probably not necessary but make them consistent
	if (pending && !e->pending)
	  e->pending = true;

	if (e != NULL) {
		// order these for undo and display
		Event* last = NULL;
		for (last = mChildren ; last != NULL && last->mSibling != NULL ; 
			 last = last->mSibling);
		if (last == NULL)
		  mChildren = e;
		else
		  last->mSibling = e;

		e->mParent = this;
	}
}

/**
 * Remove a child event. The event is not freed.
 */
void Event::removeChild(Event* event)
{
	Event* prev = NULL;
	Event* found = NULL;

	for (Event* e = mChildren ; e != NULL ; e = e->mSibling) {
		if (e != event)
		  prev = e;
		else {
			found = e;
			break;
		}
	}

	if (found != NULL) {
		if (prev == NULL)
		  mChildren = found->mSibling;
		else
		  prev->mSibling = found->mSibling;

		found->mSibling = NULL;
		found->mParent = NULL;
	}
	else {
	  Trace(1, "Expected child event not found\n");
    }
}

/**
 * Remove the last child event that isn't a JumpPlayEvent.
 * This is used when undoing events stacked for application 
 * after a loop switch.  JumpPlayEvent cannot be undone 
 * until the parent SwitchEvent is undone.
 *
 * The event is not freed, and no undo semantics happen.
 */
Event* Event::removeUndoChild()
{
	Event* undo = NULL;

	for (Event* e = mChildren ; e != NULL ; e = e->mSibling) {
		if (e->type != JumpPlayEvent)
		  undo = e;
	}

	if (undo != NULL)
	  removeChild(undo);

	return undo;
}

/**
 * Search the child event list for one of a given type.
 */
Event* Event::findEvent(EventType* type)
{
	Event* found = NULL;
	for (Event* e = mChildren ; e != NULL ; e = e->mSibling) {
		if (e->type == type) {
			found = e;
			break;
		}
	}
	return found;
}

/**
 * Search the child event list for an event of a given type and function.
 * In practice used only for finding InvokeEvents.
 */
Event* Event::findEvent(EventType* type, Function* function)
{
	Event* found = NULL;
	for (Event* e = mChildren ; e != NULL ; e = e->mSibling) {
		if (e->type == type && e->function == function) {
			found = e;
			break;
		}
	}
	return found;
}

/**
 * Return true if any of our child events have already been processed.
 */
bool Event::inProgress()
{
	bool started = false;
	
	for (Event* e = mChildren ; e != NULL ; e = e->mSibling) {
		if (e->processed) {
			started = true;
			break;
		}
	}

	if (started) {
		// should also not be in these states
		if (pending)
		  Trace(1, "Pending event considered in progress!\n");

		if (reschedule)
		  Trace(1, "Reschedulable event considered in progress!\n");
	}

	return started;
}

/****************************************************************************
 *                                                                          *
 *                              EVENT PROCESSING                            *
 *                                                                          *
 ****************************************************************************/

/**
 * Execute an event. 
 * Have to redirect through the EventType since not all events
 * will be associated with Functions.
 */
PUBLIC void Event::invoke(Loop* l)
{
    if (type != NULL) {
        // some types may overload this
        type->invoke(l, this);
    }
    else if (function != NULL) {
        // appeared in a few cases in 1.45 but shouldn't happen now?
        Trace(1, "Event without type!\n");
        function->doEvent(l, this);
    }
    else {
        Trace(1, "No way to invoke event!\n");
    }
}

/**
 * Undo an event. 
 * Have to redirect through the EventType since not all events
 * will be associated with Functions.
 */
void Event::undo(Loop* l)
{
	type->undo(l, this);
}

/**
 * Confirm the event on the given frame.  If frame is EVENT_FRAME_IMMEDIATE (-1)
 * the event is expected to be scheduled immediately in the target loop.
 * If the event frame is EVENT_FRAME_CALCULATED (-2) the event handler
 * is allowed to calculate the frame, though usually this will behave 
 * the same as IMMEDIATE.
 *
 * If the frame is positive the event is activated for that frame.
 */
void Event::confirm(Action* action, Loop* l, long frame)
{
    type->confirm(action, l, this, frame);
}

/**
 * Tell the interpreter the event has finished.
 * This will cause the script to be resumed after the wait the
 * next time it runs.
 *
 * !! NO it runs synchronously.  I don't like that at all...
 */
PUBLIC void Event::finishScriptWait() 
{
	if (mScript != NULL)
	  mScript->finishEvent(this);
}

/**
 * Tell the interpreter the event has been rescheduled.
 * If the interpreter was waiting on this event, then it can
 * switch to waiting on the new event.
 */
PUBLIC void Event::rescheduleScriptWait(Event* neu) 
{
	if (mScript != NULL)
	  mScript->rescheduleEvent(this, neu);
}

/**
 * If this event is being monitored by a ScriptInterpreter, let it
 * know that the event is being caneled.  
 *
 * !! I'm not sure if this is working.  This was being called by
 * accident from Synchronier::doRealign on the Realign event and the
 * script wasn't waking up!
 *
 */
void Event::cancelScriptWait()
{
    if (mScript != NULL) {
		mScript->cancelEvent(this);
        mScript = NULL;
	}
}

/****************************************************************************
 *                                                                          *
 *                                 EVENT LIST                               *
 *                                                                          *
 ****************************************************************************/

EventList::EventList()
{
    mEvents = NULL;
}

EventList::~EventList()
{
    flush(true, false);
}

/**
 * If the "reset" flag is on, then we flush everything.  If the flag
 * is off then we only flush "undoable" events.  The reset flag will
 * be off in cases where we're making a transition that invalidates
 * major scheduled events, but needs to keep play jumps and other
 * invisible housekeeping events.
 *
 * If the keepScriptEvents flag is on, we will retain script wait events
 * when resetting.  This is normally off, and should only be on if we 
 * know we're running a script that is waiting for a Reset, or otherwise
 * expected to be running after the Reset.  
 *
 */
void EventList::flush(bool reset, bool keepScriptEvents)
{
	Event* next = NULL;
	for (Event* e = mEvents ; e != NULL ; e = next) {
		next = e->getNext();
		if (reset || !e->type->noUndo) {
			if (e->type != ScriptEvent || (reset && !keepScriptEvents)) {

				remove(e);
				// remove doesn't free but free can remove children
				// which may be the next on the list, so we have to 
				// start over from the beginning after any free
				// if we're resetting turn on the freeAll flag
                if (reset)
                  e->freeAll();
                else
                  e->free();
				next = mEvents;
			}
		}
	}
}

/**
 * Specialty function for loop switch to transfer
 * all of the current events to a new list.
 */
EventList* EventList::transfer()
{
	EventList* list = new EventList();

	for (Event* e = mEvents ; e != NULL ; e = e->getNext())
	  e->setList(list);

	list->mEvents = mEvents;
	mEvents = NULL;

	return list;
}

Event* EventList::getEvents()
{
	return mEvents;
}

/**
 * Add an event to the end of the list.
 * !! name this append to make it clear how this is different
 * than insert()
 */
void EventList::add(Event* event)
{
	if (event != NULL) {

		if (event->getList() != NULL) {
            Trace(1, "Attempt to add an event already on another list!\n");
        }
		else {
			Event* last = NULL;
			for (last = mEvents ; last != NULL && last->getNext() != NULL ; 
				 last = last->getNext());

			if (last != NULL)
			  last->setNext(event);
			else
			  mEvents = event;

			event->setList(this);
		}
	}
}

/**
 * Insert an event into the list, ordering by frame.
 */
void EventList::insert(Event* event)
{
	if (event != NULL) {

		if (event->getList() != NULL) {
            Trace(1, "Attempt to add an event already on another list!\n");
        }
		else {
            Event* prev = NULL;
            for (Event* e = mEvents ; e != NULL ; e = e->getNext()) {
                if (e->frame > event->frame)
                  break;
                else 
                  prev = e;
            }   

            if (prev != NULL) {
                event->setNext(prev->getNext());
                prev->setNext(event);
            }
            else {
                event->setNext(mEvents);
                mEvents = event;
            }

            event->setList(this);
        }
    }
}
                
/**
 * Remove an event from the list.
 * The event is not freed, it is simply removed.
 */
void EventList::remove(Event* event)
{
	if (event != NULL) {
		Event* prev = NULL;
		Event* e;

		for (e = mEvents ; e != NULL && e != event ; e = e->getNext())
		  prev = e;

		if (e == event) {
			if (prev == NULL)
			  mEvents = e->getNext();
			else 
			  prev->setNext(e->getNext());

			e->setList(NULL);
			e->setNext(NULL);
		}
	}
}

/**
 * Return true if the event is in the list.
 */
bool EventList::contains(Event* event)
{
	bool contains = false;

	for (Event* e = mEvents ; e != NULL ; e = e->getNext()) {
		if (e == event) {
			contains = true;
			break;
		}
	}
	return contains;
}

/**
 * Return the first event on a frame.
 */
Event* EventList::find(long frame)
{
	Event* event = NULL;
	for (Event* e = mEvents ; e != NULL ; e = e->getNext()) {
		if (e->frame == frame) {
			event = e;
			break;
		}
	}
	return event;	
}

/**
 * Return the next event of a given type.
 */
Event* EventList::find(EventType* type)
{
	Event* event = NULL;
	for (Event* e = mEvents ; e != NULL ; e = e->getNext()) {
		if (e->type == type) {
			event = e;
			break;
		}
	}
	return event;	
}

Event* EventList::find(Function* f)
{
	Event* event = NULL;
	for (Event* e = mEvents ; e != NULL ; e = e->getNext()) {
		if (e->function == f) {
			event = e;
			break;
		}
	}
	return event;	
}

/**
 * Return an event of the given type on the given frame.
 */
Event* EventList::find(EventType* type, long frame)
{
	Event* event = NULL;
	for (Event* e = mEvents ; e != NULL ; e = e->getNext()) {
		if (e->type == type && e->frame == frame) {
			event = e;
			break;
		}
	}
	return event;	
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
