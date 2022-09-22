/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * A class encapsulating event scheduling tools for a track.
 * This contains an EventList which contains Events.  EventList is
 * a relatively pure list manager, up here we know more about the
 * semantics of the events and their relationships.
 *
 * An event may be a "primary" event (aka top-level or parent), or it
 * may be a "child" event owned by a primary event.  
 *
 * The primary event list is linked with the "next" field, the child event
 * list for a given parent is linked with the "sibling" field.  An event
 * may be on both the primary event list and a child list, but a child
 * event is not necessarily on the primary event list.
 *
 * The most common child event is JumpPlayEvent which if possible is
 * scheduled before its parent event to compensate for audio latency.  
 * A JumpPlayEvent may be on the same frame as the parent event if we were
 * not able to schedule the parent far enough in advance, but it will never
 * be later than the parent event.
 *
 * The sibling list is maintained in creation order, it is undone
 * in the reverse order.  Sibling lists are used for "stacking" functions
 * that defer the functions untl after something happens.
 *
 * The primary event list is also maintained in creation order for undo, 
 * though in practice it will usually also be in time order except for
 * ScriptEvents (waits).
 *
 * Child events are found on:
 *
 *   StutterEvent
 *     JumpPlayEvent at the end of the stuttered cycle.
 *
 *   InsertEvent
 *   MultiplyEvent
 *       Adds either MultiplyEndEvent or InsertEndEvent
 *       A RecordStopEvent to stops recording before the end event
 *
 *   MultiplyEndEvent
 *   InsertEventEvent
 *       RecordStopEvent at the end of the recorded section of the
 *       multiply or insert.
 *
 *   Multiply/Insert end triggers.
 *       Any function performed during a multiply/insert that is not
 *       handled as a special mode ending function.  Will be given
 *       to the the Multiply/InsertEndEvent as a child.
 *
 *   SwitchEvent
 *       Anything that can be stacked as a control event or that makes
 *       sense after the switch.
 *       A single JumpPlayEvent shared by all stacked events.
 * 
 *   RecordStopEvent
 *       Can be a stacking event if the record ending is synchronized.
 *       I see at least have SwitchEvent in LoopSwitch.cpp, but we should
 *       be doing this consistently for others!!
 *
 *   ReverseEvent
 *       Has a ReversePlayEvent, a special case of JumpPlayEvent.
 *
 *   MuteEvent
 *       Has a JumpPlayEvent.
 * 
 *   MidiStartEvent
 *       Has a JumpPlayEvent if this is MuteMidiStart function.
 * 
 * Generic play jumps are the most common, found on:
 *
 *   MuteEvent
 *   MoveEvent
 *   RateEvent
 *   ReplaceEvent
 *   SlipEvent
 *   SpeedEvent
 * 
 * Potentially used by, but disabled:
 *
 *   RealignEvent
 *     MuteEvent scheduled if function is MuteRealign.
 *
 *   MidiStartEvent
 *     MuteEvent scheduled if function is MuteMidiStart.
 *
 * INSERTION RULES
 *
 * With one exception, the current rule is that primary events will be
 * sorted in both creation and time order.  The exception is ScriptWait
 * events which may be scheduled for any time.
 *
 * Another rule is that a primary event cannot be inserted into the list
 * at a time *before* any other primary event on the list.  This means
 * that it is possible to calculate the effective mode and latency when
 * an event (and it's play jump) is scheduled, and this cannot be changed
 * by later event insertions.
 *
 * This rule will no longer hold if we allow functions to have different
 * quantization levels, for example InsertQuant=Loop and RateQuant=Subcycle.
 * We would schedule a PlayJump to mute before the InsertEvent, but if
 * a RateEvent was then scheduled at an earlier time, it would change the
 * effective latency to be used in scheduling the PlayJump.  To handle
 * the general case, we would need to be able to reschedule all mode
 * and latency sensitive events after any insertion into the event list.
 *
 * EVENT FREEING
 *
 * Any event that still has a parent in the event list must not be freed.
 * They will be marked processed and freed when the parent event is freed.
 * 
 * When a parent event is freed, we also free processed children.  We do
 * not expect to find unprocessed children, if we do they leak for safety.
 * 
 * In a few rare cases (what are they!) an event may be on the child list
 * of two parents, but it only points to the primary parent.  Care must
 * be taken when freeing the secondary parent not to free the shared child.
 * 
 */

#include <stdio.h>
#include <memory.h>

#include "Trace.h"
#include "Util.h"

#include "Action.h"
#include "Event.h"
#include "Loop.h"
#include "Mobius.h"
#include "Mode.h"
#include "Script.h"
#include "Stream.h"
#include "Synchronizer.h"
#include "Track.h"

#include "EventManager.h"

/****************************************************************************
 *                                                                          *
 *                               EVENT MANAGER                              *
 *                                                                          *
 ****************************************************************************/

PUBLIC EventManager::EventManager(Track* track)
{
    mTrack = track;
	mEvents = new EventList();
    mSwitch = NULL;

    // special event we can inject at sync boundaries
	mSyncEvent = newEvent();
	mSyncEvent->type = SyncEvent;
	// this keeps it from being returned to the free pool
	mSyncEvent->setOwned(true);
	mLastSyncEventFrame = -1;
}

PUBLIC EventManager::~EventManager()
{
    flushAllEvents();
    delete mEvents;
    mSyncEvent->setOwned(false); 
    mSyncEvent->free();
}

PUBLIC void EventManager::reset()
{
	flushAllEvents();
    resetLastSyncEventFrame();
}

/**
 * This must be called whenever the loop frame is set.
 * !! Ugly dependency.
 */
PUBLIC void EventManager::resetLastSyncEventFrame()
{
    mLastSyncEventFrame = -1;
}

PUBLIC long EventManager::getLastSyncEventFrame()
{
    return mLastSyncEventFrame;
}

PUBLIC void EventManager::setLastSyncEventFrame(long frame)
{
    mLastSyncEventFrame = frame;
}

/**
 * Allow the event list out for inspection but don't overuse this!
 */
PUBLIC Event* EventManager::getEvents()
{
    return mEvents->getEvents();
}

PUBLIC bool EventManager::hasEvents()
{
    return (mEvents->getEvents() != NULL);
}

PUBLIC Event* EventManager::getSwitchEvent()
{
    return mSwitch;
}

PUBLIC void EventManager::setSwitchEvent(Event* e)
{
    mSwitch = e;
}

PUBLIC bool EventManager::isSwitching()
{
	return (mSwitch != NULL);
}

PUBLIC bool EventManager::isSwitchConfirmed()
{
	return (mSwitch != NULL && !mSwitch->pending);
}

Event* EventManager::findEvent(long frame)
{
	return mEvents->find(frame);
}

Event* EventManager::findEvent(EventType* type)
{
	return mEvents->find(type);
}

Event* EventManager::findEvent(Function* func)
{
	return mEvents->find(func);
}

/**
 * Helper to determine if validation should be suppressed.
 * Passed the event we just finished processing, or NULL if we're
 * not finishing up a particular function.
 */
PUBLIC bool EventManager::isValidationSuppressed(Event* finished)
{
	bool ignore = false;

	// !! the insane flag predates ValidateEvent, try to merge!!
	if (finished != NULL && finished->insane)
	  ignore = true;
	else {
		for (Event* e = mEvents->getEvents() ; e != NULL ; e = e->getNext()) {
			if (e != finished && 
				(e->type == ValidateEvent || e->inProgress())) {
				ignore = true;	
				break;
			}
		}
	}
	return ignore;
}

/**
 * Return true if the event is already scheduled.
 * Don't need a csect here because we're always in the interrupt and
 * we're not modifying the list.
 */
PUBLIC bool EventManager::isEventScheduled(Event* e) 
{
	return mEvents->contains(e);
}

/****************************************************************************
 *                                                                          *
 *                              EVENT SCHEDULING                            *
 *                                                                          *
 ****************************************************************************/

Event* EventManager::newEvent()
{
    EventPool* p = mTrack->getMobius()->getEventPool();
	return p->newEvent();
}

Event* EventManager::newEvent(EventType* type, long frame)
{
    Event* e = newEvent();
	e->type = type;
	e->frame = frame;
	return e;
}

Event* EventManager::newEvent(Function* f, long frame)
{
    // this is more important now, catch early
    if (f->eventType == NULL)
      Trace(mTrack, 1, "EventManager::newEvent Function without event type: %s!\n",
            f->getName());

	return newEvent(f, f->eventType, frame);
}

Event* EventManager::newEvent(Function* f, EventType* type, long frame)
{
	Event* e = newEvent();
	e->function = f;
	e->type = type;
	e->frame = frame;
    e->silent = f->silent;
	return e;
}

/**
 * Schedule an event.
 * 
 * Formerly called processEvent immediately if event->frame == mFrame
 * but that screws up some of the more complex function handlers that
 * need to ensure event processing doesn't happen until after the
 * handler finishes.
 */
PUBLIC void EventManager::addEvent(Event* event)
{
	const char* name = event->getName();
	const char* func = event->getFunctionName();

    if (!event->silent) {
        if (event->reschedule)
          Trace(mTrack, 2, "EventManager: Add rescuedule event %s(%s) %ld\n", name, func, event->frame);
        else if (event->pending)
          Trace(mTrack, 2, "EventManager: Add pending event %s(%s) %ld\n", name, func, event->frame);
        else
          Trace(mTrack, 2, "EventManager: Add event %s(%s) %ld\n", name, func, event->frame);
    }

	mTrack->enterCriticalSection("addEvent");

	mEvents->add(event);
    event->setTrack(mTrack);

	mTrack->leaveCriticalSection();
}

/**
 * Called as scripts terminate and we reclaim their interpreters.
 * The interpreter may have been set as a listener for events
 * scheduled while it was running.  Need to remove this reference.
 *
 * This MUST be called in the interrupt handler.
 */
PUBLIC void EventManager::removeScriptReferences(ScriptInterpreter* si)
{
	for (Event* e = mEvents->getEvents() ; e != NULL ; e = e->getNext()) {
		if (e->getScript() == si)
		  e->setScript(NULL);
		for (Event* c = e->getChildren() ; c != NULL ; c = c->getSibling()) {
			if (c->getScript() == si)
			  c->setScript(NULL);
		}
	}
}

/**
 * Build a primary function event, scheduled for the next available frame.
 * 
 * ?? consider individual quantize settings for each fundamental function
 *
 * The event is NOT added to the event list, caller may decide to ignore it.
 * Ownership of the Action is taken.
 */
PUBLIC Event* EventManager::getFunctionEvent(Action* action,
                                             Loop* loop, 
                                             Function* func)
{
    if (func == NULL)
      func = action->getFunction();
    else {
        // can this happen?
        // sigh, yes it can...in a few places that have
        // secondary actions we clone the original action
        // and redirect it through a different function.  Lok
        // for anything that calls cloneAction.  We could
        // probably have them change the Action function but then
        // we would lose the invoking function which may be interesting...
        // UndoRedo does this as an alternate ending to Mute mode without cloning
        if (action->getFunction() != func)
          Trace(2, "EventManager: functions don't match!\n");
    }

	Event* event = newEvent(func, 0);
	Preset::QuantizeMode q = Preset::QUANTIZE_OFF;
    long frame = 0;
    Preset* preset = mTrack->getPreset();

	// Quantize may be temorarily disabled if we're "escaping" quantization
	// or for certain forms of mute scheduling.
	bool checkQuantize = !action->escapeQuantization;

	// If we're muted with MuteMode=Pause, then do not quantize the unmute
	// Really don't like the mode-specific logic, but this is working
	// out to be a hard one.  Might be better to factor this into
	// Function with subclass overrides?
	if (loop->isPaused() && func->eventType == MuteEvent)
	  checkQuantize = false;

	if (checkQuantize) {
		if (func == Bounce) {
			// special case that has its own	
			q = preset->getBounceQuantize();
		}
		else if (func->quantized) {
			q = preset->getQuantize();
		}
		else if (func->eventType == OverdubEvent) {
			// EDP does not quantize overdub but we can
			if (preset->isOverdubQuantized())
			  q = preset->getQuantize();
		}
		else if (func->eventType == RecordEvent && loop->getMode()->rounding) {
			// It's useful to be able to quantize the end of an unrounded
			// multiply/insert.  This is NOT what the EDP does but I think
            // if you've got quant on it makes sense to use it here too,
            // not worth another mode, use a script of you want an unquantized
            // unrounded ending
            q = preset->getQuantize();
		}
	}

	// When we're being driven by a script, functions after
	// a Wait statement have to performed at exactly the current time not, 
	// not after adding in input latency.   Interpreter will set this flag
	int latency = (action->noLatency) ? 0 : loop->getInputStream()->latency;

    // calculate the frame

    if (action->rescheduling != NULL) {
        // We're resceduling a previously scheduled event.
        // Usually we just keep the same frame.  There are some previous
        // events however that require us to recalculate the frame if   
        // quantization is enabled.  One is Reverse which may have
        // uneven subcycle sizes.  
        //
        // !! What about events that change the loop size like 
        // Insert/Multiply

         frame = action->rescheduling->frame;

        if (q == Preset::QUANTIZE_OFF) {
            // This is what we would do below to "catch up to real time"
            // Is This relevant here?
            // Note that by definition latency is not included when 
            // rescheduling.
            long altFrame = loop->getFrame();
            if (frame != altFrame) 
              Trace(loop, 1, "Unexpected rescheduling frame mismatch: %ld %ld\n",
                    frame, altFrame);
        }
        else {
            // Here we have the problem mentioned above, we could be exactly
            // on a quantization boundary and the logic below would
            // have pushed us to the next one.
            bool nextQuant = false;
            if (action->reschedulingReason != NULL) {
                Event* prevEvent = action->reschedulingReason;
                Function* prevfunc = prevEvent->function;

                // push if the function doesn't quantize stack, or if
                // this was the same function on the current frame
                nextQuant = (!func->quantizeStack ||
                             prevEvent->type == event->type);
            }
            
            long qframe = getQuantizedFrame(loop, loop->getFrame(), q, nextQuant);

            if (frame != qframe) {
                Trace(loop, 2, "Adjusting rescheduled event frame from %ld to %ld\n",
                      frame, qframe);
                frame = qframe;
            }

            // set this for visibility
            event->quantized = true;
        }
    }
    else if (loop->getMode() == RecordMode) {
		// if we're still recording, then this is usually an ending event,
		// schedule it after the RecordStopEvent.  If we don't have one, then
		// assume it is one of the rare events we allow during recording. 
		// SpeedEvent should be the only one at the moment.

		long loopFrames = loop->getFrames();
		if (loopFrames > 0) {
			// have already closed off the loop, schedule at the end
			frame = loopFrames;
		}
		else {
			Event* end = findEvent(RecordStopEvent);
			if (end == NULL) {
				// speed shift during recording
				frame = loop->getFrame() + latency;
			}
			else {
				// This can happen if we've ended the recording with
                // an alternate ending, and the RecordStop event is
                // pending waiting for a sync pulse. Schedule it
                // for frame zero.  It doesn't have to be pending because
                // when the recording finally ends we'll be at frame zero.
                if (end->pending)
                  frame = 0;
                else {
                    // ending was scheduled, put it after the end
					frame = end->frame;
				}
			}
		}
	}
	else if (isSwitchConfirmed()) {
		// If we're in SwitchQuantize, unconditionally quantize
		// after the switch.  switchEvent will transfer these after the switch
		frame = mSwitch->frame;
		event->quantized = true;
	}
	else if (q != Preset::QUANTIZE_OFF) {
        // quantization must be done relative to "realtime" which
        // is mFrame + InputLatency since we're always behind
        frame = loop->getFrame() + latency;
		frame = getQuantizedFrame(loop, frame, q, false);
		event->quantized = true;
	}
    else {
        // The function waits to catch up to "real time".
        // This isn't quantization, but looks similar to the event handlers.
        frame = loop->getFrame() + latency;
    }

    // Now the frame is calculated, 
	// see if there is already an event on this frame
    // ?? should we be looking only for quantized events?
    // !! should we be doing this if we're rescheduling?

	Event* prev = mEvents->find(frame);
	// !! how to pending events interact here?
	if (!event->pending && prev != NULL && !prev->pending) {
		if (!event->quantized) {
			if (prev->type == event->type) {
				// An extremely short "tap" of a SUS function.  It is important
                // that we handle this to avoid missing an up transition
                // and getting stuck in the SUS.  For functions that
                // define the loop length (Record, Unrounded Multiply)
                // this may result in a zero length cycle which we can't
                // support, but this will be caught later.  This can happen
				// in scripts with Insert/Insert and Multiply/Multiply
				// which handle their own rounding so ignore this if
				// we're in a script.  
				if (action->trigger != TriggerScript)
				  Trace(mTrack, 1, "EventManager: Extremely short function duration: %s\n", 
						func->getDisplayName());
			}
			else {
                // Formerly tried to warn about stacking events
                // on the same frame that would be meaningless, like
                // having a Record followed by a Multiply.  But the rules
                // are too complex to handle with a simple "allowsOverlap"
                // flag on the event so we don't bother any more.  
			}
		}
		else if (loop->getMode()->rounding) {
			// It doesn't really matter where this goes since it's
			// going to be rescheduled eventually
		}
        else if (isSwitchConfirmed()) {
			// hmm, shouldn't really be pushing events in switch quant
			// because the quantization needs to be calculated for the 
			// NEXT loop, which can have a different size (and be
			// changed at any time), leave them stacked up on the switch
			// boundary and have switchEvent reschedule them
		}
		else if (prev->type == ScriptEvent) {
			// don't make script wait events push this one to the next
			// quantization boundary
		}
		else if (!func->quantizeStack || prev->type == event->type) {
			// Advance to the next unoccupied quantization boundary
			// note that we ignore the quantizeStack flag if we're trying
			// to stack two events of the same type.
            // !! revisit ths quantizeStack thing, the functions that
            // support it now are: Overdub, Speed, Rate, Reverse, and RunScript
            // These seem reasonable but rather arbitrary.  
            // May also want a flag on the previous function that forces
            // a push even if the new function has quantizeStack?

			while (prev != NULL) {
				long nextFrame = getQuantizedFrame(loop, frame, q, true);
				if (nextFrame != frame) {
					frame = nextFrame;
					prev = mEvents->find(frame);
				}
				else {
					// q doesn't seem to be taking us anywhere
					// might happen if we quantized based on something
					// other than q
					break;
				}
            }
        }
		else {
			// This event can stack on top of another, but there
			// may be another event scheduled after the one we found
			// at the next quantization point.  This happens if you
			// schedule several non-stackable events.  We have to find
			// the last non-stackable event we scheduled and put the
			// stackable event there.
			// Example: hit Mute twice and then Reverse.  The Reverse
			// needs to happen on the second quantized Mute, not the first.

			Event* highest = prev;
			for (Event* e = mEvents->getEvents() ; e != NULL ; e = e->getNext()) {
				// they can be reschedule'd but not pending
				// also ignore script waits just in case
				if (e->frame > highest->frame && !e->pending &&
					e->type != ScriptEvent) {
					highest = e;
				}
			}

			frame = highest->frame;
		}
	}
    
    event->frame = frame;

	// If there are any events preceeding this one whose
	// event type indiciates that it will reschedule events,
	// mark this event as reschedulable.  This will prevent a JumpPlayEvent
	// from being sceduled because we're not sure where it will go yet.

	for (Event* e = mEvents->getEvents() ; e != NULL ; e = e->getNext()) {
		if (e->frame <= event->frame && e->type->reschedules) {
			event->reschedule = true;
			break;
		}
	}

	// save a copy of the current parameter values so we can override
	// them in scripts then restore them before the function actually
	// runs, do we always want this?
	event->savePreset(preset);

    // ownership of the action transfers to the event
    action->setEvent(event);

	return event;
}

/****************************************************************************
 *                                                                          *
 *                                ADJUSTMENTS                               *
 *                                                                          *
 ****************************************************************************/

/**
 * Shift currently scheduled events to adjust for loop disruptions
 * like looping or unrounded multiply.
 *
 * Note that there can be events scheduled within the new length,
 * only shift those that fall outside the new length.
 */
PUBLIC void EventManager::shiftEvents(long frames)
{
	if (frames > 0) {
		Event* events = mEvents->getEvents();
		for (Event* e = events ; e != NULL ; e = e->getNext()) {
			if (!e->pending && e->frame >= frames)
              e->frame -= frames;
		}
	}
}

/**
 * In rare cases (SUSReturn) we may have to move a multiply/insert
 * alternate ending that was scheduled BEFORE the mode end event.
 * When this happens we also have to reposition the events so they
 * (and their children) are AFTER the mode end event and its children.
 */
PUBLIC void EventManager::reorderEvent(Event* e)
{
	for (Event* child = e->getChildren() ; child != NULL ; 
         child = child->getSibling())
	  reorderEvent(child);

	mTrack->enterCriticalSection("reorderEvent");
	mEvents->remove(e);
	mEvents->add(e);
	mTrack->leaveCriticalSection();
}

/**
 * If we have Script wait events scheduled, allow them to advance
 * when the loop is in Reset or Pause mode.
 */
PUBLIC void EventManager::advanceScriptWaits(long frames)
{
    Loop* loop = mTrack->getLoop();

    // We allow waits to be scheduled in ResetMode and Pause mode
    for (Event* e = mEvents->getEvents() ; e != NULL ; e = e->getNext()) {
        if (!e->pending && 
            (e->type == ScriptEvent || e->type == RunScriptEvent) &&
            (loop->getMode() == ResetMode || 
             (loop->isPaused() && e->pauseEnabled))) {

            long newFrame = e->frame - frames;
            long loopFrame = loop->getFrame();
            if (newFrame < loopFrame)
              newFrame = loopFrame;
            e->frame = newFrame;
        }
    }
}

/**
 * Adjust script wait event frames during a loop switch.
 *
 * Try to maintain the same relative remaining wait in the new loop.
 * Depending on the wait type this will be inaccurate, for example
 * absolute waits like "Wait until subcycle 1" would ideally be recalculated
 * from scratch given the new loop length but we've lost the wait unit
 * and the unit count in the Event so the best we can do is maintain
 * the same relative wait.  See waitswitch.txt for more analysis.
 */
PUBLIC void EventManager::loopSwitchScriptWaits(Loop* current, long nextFrame)
{
	Event* e = findEvent(ScriptEvent);
	if (e != NULL && !e->pending) {

        long currentFrame = current->getFrame();
        if (e->frame >= currentFrame) {
            // it was most likely a relative wait retain the same 
            // relative wait
            long remaining = e->frame - currentFrame;
            long newFrame = nextFrame + remaining;
            Trace(mTrack, 2, "EventManager: rescheduling wait event frame from %ld to %ld\n",
                  e->frame, newFrame);

            e->frame = newFrame;
        }
        else {
            // If the event was scheduled before the switch frame
            // it must have been an absolute wait like "Wait until subcycle 1".
            // If the loop cycle lengths are the same we can just leave
            // it alone.
            Trace(mTrack, 2, "EventManager: retaining wait event frame %ld\n",
                  e->frame);
        }
    }
}

/**
 * Move an event to a new frame, and move child events to maintain
 * the same relative distance.
 */
PUBLIC void EventManager::moveEventHierarchy(Loop* loop, Event* e, long newFrame)
{
	long delta = newFrame - e->frame;

	// do this top down so the children are undone in reverse order
	moveEvent(loop, e, newFrame);

	for (Event* child = e->getChildren() ; child != NULL ; 
         child = child->getSibling()) {
		// if child has a latencyLoss, then restore the ideal frame 
		long childFrame = (child->frame - child->latencyLoss) + delta;
		moveEventHierarchy(loop, child, childFrame);
	}
	
}

/**
 * Move a previously scheduled event to a new frame and recalculate
 * latency loss.
 *
 * Note that this event may have already been processed, this is 
 * common for RecordStopEvent when RoundingOverdub=off, and also for 
 * JumpPlayEvent when using multi-increase near the end of the insert.
 * Undo the effect of the event, and schedule it again.
 *
 * If the new frame is less than the current frame assume
 * we're being processed as the result of a forced unrounded operation,
 * and are therefore no longer quantized.  This prevents the event
 * from being undone and visible.
 *
 * This is now being called indirectly From EventType::move, this
 * allows complicated events like SpeedEvent and ReverseEvent to 
 * perform additional adjustments.
 */
PUBLIC void EventManager::moveEvent(Loop* loop, Event* e, long newFrame)
{
	long latencyLoss = 0;
    long loopFrame = loop->getFrame();

	if (newFrame < loopFrame) {
		latencyLoss = loopFrame - newFrame;
		newFrame = loopFrame;
	}

	if (!e->processed)
	  Trace(mTrack, 2, "EventManager: Shifting %s to %ld\n", e->getName(), newFrame);
	else {
        // potentially very complex undo 
		e->undo(loop);
		e->processed = false;

		// hmm, this will make the child event follow the parent
		// on the event list which is unusual but should be ok
		// as long as it's frame is less
        Event* p = e->getParent();
		if (p != NULL && e->frame >= p->frame)
		  Trace(mTrack, 1, "EventManager: Rescheduling event after parent!\n");
		addEvent(e);
	}

	if (newFrame <= e->frame)
	  e->quantized = false;

    if (newFrame < 0)
      Trace(mTrack, 1, "EventManager::moveEvent frame went negative!\n");

	e->frame = newFrame;
	e->latencyLoss = latencyLoss;
}

/**
 * Called when we change direction.
 * The events keep their same relative position in the new direction.
 */
PUBLIC void EventManager::reverseEvents(long originalFrame, long newFrame)
{
    for (Event* e = mEvents->getEvents() ; e != NULL ; e = e->getNext()) {
        if (!e->pending)
          e->frame = reverseFrame(originalFrame, newFrame, e->frame);
    }
}

/**
 * Perform an "event" reflection of a frame.  
 * Rather than a script reflection based on the size of the loop, this
 * one reflects to maintain the same relative distance from an origin.
 *
 * !!: if this is a ScriptEvent, we may need to do a full loop reflection
 * rather than relative to the origin if the wait frame was calcualted
 * with an absolute expression, one common one is "Wait loop" to get us
 * to the end of the loop.  Hmm, maybe its ok, but its confusing.  
 * Will need to use the "pending" waits instead of absolute waits if
 * you want this to work in reverse mode.
 */
PRIVATE long EventManager::reverseFrame(long origin, long newOrigin, long frame)
{
    long delta = frame - origin;
    if (delta < 0) {
        // the event preceeded the current record frame, shouldn't happen
        Trace(mTrack, 1, "EventManager: reverseEventFrame anomoly!\n");
    }
    return newOrigin + delta;
}

/****************************************************************************
 *                                                                          *
 *                                 EVENT FREE                               *
 *                                                                          *
 ****************************************************************************/

/**
 * Remove an event from the list.
 * Note that this only removes the parent event, child events
 * may still be on the list.
 */
PUBLIC void EventManager::removeEvent(Event* e)
{
    mTrack->enterCriticalSection("removeEvent");

	mEvents->remove(e);
    e->setTrack(NULL);

    mTrack->leaveCriticalSection();
}

/**
 * Remove all of the events and return them in a private event list.
 * This is used during loop switch to filter the events we want to 
 * carry over to the new loop.
 * 
 * !! Try to move loop switch event transfer in here since we're
 * just going to put them back on the same list.
 */
PUBLIC EventList* EventManager::stealEvents()
{
    EventList* copy = NULL;

	mTrack->enterCriticalSection("stealEvents");

    copy = mEvents->transfer();

    mTrack->leaveCriticalSection();

    return copy;
}

/**
 * Flush everything on the event list.
 * This is used when we're deleted and when doing a Reset.
 * We don't care what is on the list and what relationships there
 * are, just get rid of everything.
 */
PRIVATE void EventManager::flushAllEvents()
{
    bool oldWay = true;

    if (oldWay) {
        // !! to avoid warnings should call ScriptInterpreter::cancelEvent

        mTrack->enterCriticalSection("flushAllEvents");

        // Release state for all events or else EventPool will
        // complain
		for (Event* e = mEvents->getEvents() ; e != NULL ; e = e->getNext()) {
            releaseAll(e);
        }

        // First flag is "reset" which means to remove everything.
        // Second flag is keepScriptEvents, if true we would retain
        // script wait events after the reset.  I can't think of a reason
        // to have this on except maybe a script that resumes after a reset.
        mEvents->flush(true, false);

        mTrack->leaveCriticalSection();
    }
    else {
        // freeEvent will unwind relationships but since this can also
        // remove things from the list we won't have a stable iterator.
        while (mEvents->getEvents() != NULL)
          free(mEvents->getEvents(), true);
    }

	mSwitch = NULL;
}

/**
 * Flush the event list except for script events.
 * We've had this for awhile, I'm not enturely sure why this is important
 * but be careful with it.
 */
PUBLIC void EventManager::flushEventsExceptScripts()
{
	mTrack->enterCriticalSection("flushEventsExceptScripts");

	mEvents->flush(false, false);
	mSwitch = NULL;

	mTrack->leaveCriticalSection();
}

/****************************************************************************
 *                                                                          *
 *                                    UNDO                                  *
 *                                                                          *
 ****************************************************************************/

/**
 * Free an event that has been processed or is no longer necessary.
 * Must also release resources and unwind relationships.
 * The event is not "undone" if it has been processed we let the
 * processing stand.
 */
PUBLIC void EventManager::freeEvent(Event* event)
{
    if (event != NULL) {
		// remove the event and all of its children
		mTrack->enterCriticalSection("freeEvent event");
		removeAll(event);
		mTrack->leaveCriticalSection();

        // let the interpreter know in case it is waiting
        event->cancelScriptWait();

        // Reclaim the action
        Action* action = event->getAction();
        if (action != NULL) {
            action->detachEvent(event);
            mTrack->getMobius()->completeAction(action);
        }

        // note that we call freeAll rather than free to ensure
        // that all children are freed both processed and unprocessed
        event->freeAll();
    }
}

/**
 * NEW - NOT CURRENTLY USED
 *
 * This is how I want event free to work, everything comes through
 * here and we unwind all of the relationships so Event and EventPool
 * don't have to.
 * ---
 * Free an event that has been processed or is no longer necessary.
 * Must also release resources and unwind relationships.
 * The event is not "undone" if it has been processed we let the
 * processing stand.
 *
 * The flush argument is true if we're processing a Reset, don't
 * be alarmed by unprocessed events.
 */
PRIVATE void EventManager::free(Event* event, bool flush)
{
    if (event != NULL) {

        // remove it from the event list if it isn't already
        removeEvent(event);

        // remove children from the event list
        Event* next = NULL;
        for (Event* child = event->getChildren() ; child != NULL ; child = next) {
            next = child->getSibling();

            if (child->getList() != NULL) {
                // child is also on the event list
                if (child->processed || flush) {
                    removeEvent(child);
                }
                else {
                    // When can this happen?  Maybe the SwitchEvent
                    // stacked after RecordStopEvent?
                    // Leave it scheduled for safety but I don't like this!
                    Trace(1, "EventManager: Leaving unprocessed child event!\n");
                    // splice it out of the list, sibling we found above
                    // will still be valid
                    mTrack->enterCriticalSection("freeEvent abandon child");
                    event->removeChild(child);
                    mTrack->leaveCriticalSection();
                }
            }
        }

        // We are now free of list entanglements
        // free the children one by one.  This would be easier if we
        // had direct access to setSibling but I want to keep the
        // interfaces tight to makes sure no one else does list surgery
        for (Event* child = event->getChildren() ; child != NULL ; 
             child = event->getChildren()) {

            event->removeChild(child);

            // We are not expecting another level of child list, 
            // EventPool will trace this
            release(child);
            child->free();
        }
        
        release(event);
        event->free();
    }
}

/**
 * Release resources held by this event.
 */
PRIVATE void EventManager::release(Event* event)
{
    // let the interpreter know in case it is waiting
    event->cancelScriptWait();

    // Reclaim the action
    Action* action = event->getAction();
    if (action != NULL) {
        action->detachEvent(event);
        mTrack->getMobius()->completeAction(action);
    }
}

PRIVATE void EventManager::releaseAll(Event* event)
{
    release(event);
    for (Event* child = event->getChildren() ; child != NULL ; 
         child = event->getSibling())
      release(child);
}

/**
 * Undo the last quantized event.
 * Return true if we removed something.
 *
 * !! The difference between this and undoEvent(Event)
 * is very subtle due to the difference between EventList::removeUndoEvent
 * and EventList::removeAll(Event).  Try to eliminate this.
 *
 * !! Since undoAndFree is going to call Event::freeAll they're
 * going to be removed from the list anyway so the difference 
 * will be short lived.
 */ 
PUBLIC bool EventManager::undoLastEvent()
{
	Event* undo = NULL;

	mTrack->enterCriticalSection("undoScheduledEvent");
	undo = removeUndoEvent();
	mTrack->leaveCriticalSection();

	if (undo != NULL)
      undoAndFree(undo);

    return (undo != NULL);
}

/**
 * Part of the implementation of undo.
 * Formerly in EventList, moved to EventManager because it has
 * complex relationships.
 * 
 * Remove the last undoable event on the list, along with any child events.
 *
 * The event needs to be quantized, be a root event (no parent), and not
 * be of a few types that don't support undo like SUSReturn and SUSNextLoop.
 *
 * Don't assume that child events will always preceed the parent event,
 * this was once the case, but it simplifies event scheduling if they
 * can be on either side.
 *
 * The event is simply removed from the list, it is not freed and has
 * had no undo side effects.
 */
PRIVATE Event* EventManager::removeUndoEvent()
{
	Event* last = NULL;
	
    // locate the last quantized parent event
    for (Event* e = mEvents->getEvents() ; e != NULL ; e = e->getNext()) {
        if (e->quantized && e->getParent() == NULL && !e->type->noUndo)
          last = e;
    }

    // also remove unprocessed siblings
	// we may be sharing this event with another parent, if so leave it
    // !! This is identical to undo::(Event) below except for the
    // !child->processed test, is this really necessary?

    if (last != NULL) {

        mEvents->remove(last);
        last->setTrack(NULL);

		for (Event* child = last->getChildren() ; child != NULL ; 
			 child = child->getSibling()) {

			if (child->getParent() == last) {
                
                if (!child->processed) {
                    mEvents->remove(child);
                    child->setTrack(NULL);
                }
                else {
                    // I'm curious if this can ever happen so it can be 
                    // the same as undo(Event) below
                    Trace(1, "EventManager: Leaving processed child event, why?\n");
                }

                // if this can happen, we'll need to recurse?
                if (child->getChildren() != NULL)
                  Trace(1, "EventManager: Found multi-level children!\n");
            }
		}
	}

	return last;
}

/**
 * Remove an event and its children from the scheduled list and
 * undo any effects.
 */
PUBLIC void EventManager::undoEvent(Event* event)
{
	if (event != NULL) {

		// remove the event and all of its children
		mTrack->enterCriticalSection("undoScheduledEvent event");
		removeAll(event);
		mTrack->leaveCriticalSection();

        undoAndFree(event);
    }
}

/**
 * Part of the implementation of undo and some forms of cancel.
 * Remove an event and any child events from the list.
 *
 * Formerly in EventList, moved up to EventManager to keep
 * it with the other undo logic.
 * 
 * Don't assume that child events will always preceed the parent event,
 * this was once the case, but it simplifies event scheduling if they
 * can be on either side.
 *
 * !! This is basically the same as the second half of removeUndoEvent
 * except that it also removes processed child events.  It's been that
 * way for awhile and I'd like to know why.
 */
void EventManager::removeAll(Event* e)
{
	if (e != NULL) {
		Trace(2, "Remove event %s(%s) %ld\n", 
			  e->getName(), e->getFunctionName(), e->frame);

		mEvents->remove(e);
        e->setTrack(NULL);

		for (Event* child = e->getChildren() ; child != NULL ; 
			 child = child->getSibling()) {

			if (child->getParent() == e) {

                // this is what makes it different than removeUndoEvent
                // find out when this can happen
                if (child->processed)
                  Trace(1, "EventManager: Removing processed child event!\n");
                removeAll(child);
            }
		}
	}
}

/**
 * After removing an event from the list, cancel any side effects.
 * If the event owns an action it will be freed.
 * If the event has a script wait it will be canceled.
 */
PRIVATE void EventManager::undoAndFree(Event* event)
{
    Trace(mTrack->getLoop(), 2, "EventManager: Undo event %s\n", 
          event->getName());

    // let the interpreter know in case it is waiting
    event->cancelScriptWait();

    if (event == mSwitch) {
        // it's the switch quantize event, cancel the switch
        cancelSwitch();
    }
    else {
        // If the event was processed, undo it's effect
        // recursively undo child events.  Do children first?
        undoProcessedEvents(event);

        // Reclaim the action
        Action* action = event->getAction();
        if (action != NULL) {
            action->detachEvent(event);
            mTrack->getMobius()->completeAction(action);
        }

        // note that we call freeAll rather than free to ensure
        // that child events marked unprocessed are also freed
        event->freeAll();
    }
}

/**
 * Walk over a hierarchy of events, undoing the effects of any
 * that have been processed.  Usually there is only one level, but
 * there can be more for things like Insert/Multiply alternate endings.
 */
PRIVATE void EventManager::undoProcessedEvents(Event* event)
{
	// assume depth first processing?
	for (Event* child = event->getChildren() ; child != NULL ; 
		 child = child->getSibling()) {

		undoProcessedEvents(child);
	}

	if (event->processed)
      event->undo(mTrack->getLoop());
}

/****************************************************************************
 *                                                                          *
 *                              STACK SCHEDULING                            *
 *                                                                          *
 ****************************************************************************/

/**
 * Called by functions to stack events to be performed after the switch.
 */
PUBLIC void EventManager::scheduleSwitchStack(Event* event)
{
	Event* switche = getUncomittedSwitch();

	if (switche != NULL) {

		// do we really need to do this?  
		// should the preset affect all stacked events
		event->savePreset(mTrack->getPreset());
		event->pending = true;

		mTrack->enterCriticalSection("scheduleSwitchStack");

		if (event->function->switchStackMutex) {
			// remove all other mutex events
			Event* next = NULL;
			for (Event* e = switche->getChildren() ; e != NULL ; e = next) {
				next = e->getSibling();
				if (e->function != NULL && e->function->switchStackMutex) {

                    // cancel the previous one before adding the new one
                    // !! what about Action transfer?
					switche->removeChild(e);
                    freeEvent(e);
				}
			}
		}

		switche->addChild(event);

		mTrack->leaveCriticalSection();

		Trace(mTrack, 2, "EventManger: Added switch stack event %s\n", event->type->name);
	}
	else {
		Trace(mTrack, 2, "EventManager: Switch already committed, ignoring stacking of %s!\n",
			  event->type->name);
	}
}

/**
 * Returns the SwitchEvent if we're able to modify the events stacked
 * for execution after a loop switch.
 *
 * Originally I tried to be fancy and allow an early play jump
 * that could then be undone and completely altered before finishing
 * the switch after the latency delay.  This was more imporant
 * with MME latency but with ASIO the window is so small that it just
 * doesn't seem worth the effort.
 *
 * Once we take the play jump before the switch, consider the
 * switch as being "committed" at which point it cannot be modified.
 *
 * Unclear what we should do with functions that come in during the
 * committed period, but it should be safe to ignore them for now.
 */
PUBLIC Event* EventManager::getUncomittedSwitch()
{
	Event* e = NULL;

	// also return false if we're not in a switch mode
	if (mSwitch != NULL) {
		// jump may be null if we're using a "confirm" mode
		Event* jump = mSwitch->findEvent(JumpPlayEvent);
		if (jump != NULL && jump->processed)
		  Trace(mTrack, 1, "EventManager: Ignoring function after switch commit!\n");
		else 
		  e = mSwitch;
	}
	return e;
}

/**
 * Called when the Undo function is used during SwitchMode.
 * This is a confirmation action in ConfirmMode, but not sure about
 * when we're in the switch quantize period.  I would rather it 
 * incrementally undo the switch events.
 *
 * Note that since these aren't on the main event list, the usual
 * undo method doesn't work.
 */
PUBLIC bool EventManager::undoSwitchStack()
{
    bool undone = false;

	if (getUncomittedSwitch() != NULL) {

		mTrack->enterCriticalSection("undoSwitchStack");
		// !! add an option to preserve "automatic" events that were
		// put there to implement transfer modes?
		Event* undo = mSwitch->removeUndoChild();
		mTrack->leaveCriticalSection();

		if (undo != NULL) {
			Trace(mTrack, 2, "EventManager: Undo switch stack event %s(%s)\n", 
				  undo->getName(), undo->getFunctionName());
            undoAndFree(undo);
            undone = true;
		}
	}	
    return undone;
}

PUBLIC void EventManager::cancelSwitchStack(Event* e)
{
	if (e != NULL) {
		Event* switche = getUncomittedSwitch();
		if (switche != NULL) {
			Trace(mTrack, 2, "EventManager: Canceling switch stack event %s\n", e->type->name);
			mTrack->enterCriticalSection("cancelSwitchStack");
			switche->removeChild(e);
			mEvents->remove(e);
            e->setTrack(NULL);
			mTrack->leaveCriticalSection();

            // should we call undoEvent here?
            freeEvent(e);
		}	
	}
}

/**
 * Called in various places to cancel a pending switch.
 */
PUBLIC void EventManager::cancelSwitch()
{
	if (mSwitch != NULL) {

		mTrack->enterCriticalSection("cancelSwitch");
		removeAll(mSwitch);
		mTrack->leaveCriticalSection();

		// undo handler has the logic we need
        // but have to null mSwitch first!
		Event* e = mSwitch;
        mSwitch = NULL;
		switchEventUndo(e);

		Trace(mTrack, 2, "EventManager: switch canceled\n");
	}
}

/**
 * SwitchEvent undo handler.
 */
PUBLIC void EventManager::switchEventUndo(Event* e)
{
	// This will run though undo logic for our child events
	// the only interesting one is JumpPlayEvent which will
	// restore playback if we had already begun playing
	undoEvent(e);
}

/****************************************************************************
 *                                                                          *
 *                            PLAY JUMP SCHEDULING                          *
 *                                                                          *
 ****************************************************************************/

/**
 * An ideal play jump must be scheduled before the primary event
 * by the sum of the input and output latencies.  In the usual
 * case the input and output stream latencies will not change between
 * the time the jump is scheduled and when it is evaluated.  It is however
 * possible for the jump to be scheduled after an event that will
 * change the playback speed, and therefore the latencies that need to 
 * be used in the calculations to locate the jump.
 *
 * Here we calculate the effective latencies at a given frame by examining
 * the event list.
 *
 * NOTE: This is operating under the assumption that events that affect
 * the speed must be inserted in frame order.  
 * 
 */
PUBLIC void EventManager::getEffectiveLatencies(Loop* loop, 
                                        Event* parent, long frame, 
										int* retInput, int* retOutput)
{
    // these actually come from Track
    InputStream* istream = loop->getInputStream();
    OutputStream* ostream = loop->getOutputStream();

	int inputLatency = istream->latency;
	int outputLatency = ostream->latency;
    int octave = istream->getSpeedOctave();
	int step = istream->getSpeedStep();
    int bend = istream->getSpeedBend();
    int stretch = istream->getTimeStretch();

	// look for changes to the speed
	// TODO: need a polymorphic way to check for speed adjustments.
    // Speed shift as set by the SpeedParameter is not scheduled
    // so always use the stream. (is that right??)
    // !! This feels wrong, what if we have several events scheduled?
    // Need to be looking at the last one within the range

	for (Event* e = mEvents->getEvents() ; e != NULL ; e = e->getNext()) {
		// ignore the primary event we've already scheduled
		if (e != parent) {
			if (e->type == SpeedEvent) {
				step = (int)e->number;
			}
		}
	}

	if (step != istream->getSpeedStep() ||
		octave != istream->getSpeedOctave()) {
		// note that even though the stream speeds change at different times
		// for the purposes of this calculation we can assume they
		// will be at the same speed (when we reach the primary event)

		inputLatency = istream->getAdjustedLatency(octave, step, bend, stretch);
		outputLatency = ostream->getAdjustedLatency(octave, step, bend, stretch);
	}

	if (retInput != NULL) *retInput = inputLatency;
	if (retOutput != NULL) *retOutput = outputLatency;
}

/**
 * Schedule a generic "play jump" prior to a parent event.
 * Various events need to schedule a jump on a prior frame
 * to redirect playback buffering early to compensate for latency.
 * Note that the Loop is passed in because we may be using this
 * to schedule things in the next loop before a switch completes.
 * 
 * The only thing we need to care about here is the location
 * of the event, what the jump actually does will be determined
 * when the event is eventually evaluated based on the context
 * in effect at that time.
 *
 * NOTE: Some of the older functions may still set the
 * nextLayer and nextFrame fields because they're not yet set up
 * to deal with deferred evaluation.
 * 
 * There may be more than one jump on the same frame in cases
 * where several functions are stacked on the same frame.  The
 * jumps and their parents need to be maintained in insertion order.
 * The events must also be undone in reverse order.
 *
 * SUBTLETY: If this is the child of a Record alternate ending,
 * and we're synchronizing so the RecordStopEvent is deferred waiting
 * for a sync pulse, the loop length will not have been set yet.  In this
 * case, the jump has to happen at frame zero with full latency loss.
 * It *must not* be calculated relative to the current mFrame because that
 * is still advancing and we don't know when it will stop.
 *
 * FUTURE:
 * 
 * I really dislike explicit play jump scheduling.  It would be easier
 * for Functions if we could automatically create a jump pseudo event whenever
 * we notice that we're within the latency period of a primary function event.
 * 
 * BRAKEAGE!
 *
 * I don't think the latency calculaions are correct for jumps associated
 * with speed changes. IO latency needs to be adjusted for what the
 * speed will become, not what it is now?  This makes speed jumps variable
 * and may cause them to be after jumps for other events scheduled
 * on the same frame.
 *
 * !! think about this.  Once we schedule a speed switch event
 * and determine where the jump should go, any further events stacked
 * on that frame need have their jumps on the speed event's jump frame.
 * There has to be awareness of the previous events jump frame
 * Ugh, this is just more goddamn trouble than it's worth...
 * 
 * Hmm, maybe getEffectiveLatencies is enough...
 */
PUBLIC Event* EventManager::schedulePlayJump(Loop* loop, Event* parent)
{
	Event* jump = NULL;
	int inputLatency, outputLatency;

	getEffectiveLatencies(loop, parent, parent->frame, 
                          &inputLatency, &outputLatency);

	long latencyLoss = 0;
	long transitionFrame = parent->frame - inputLatency - outputLatency;
	bool ignore = false;
    long loopFrames = loop->getFrames();
    long loopFrame = loop->getFrame();

	if (loopFrames > 0) {
		if (transitionFrame < loopFrame) {
			// too late, we've already played a bit too far
			latencyLoss = loopFrame - transitionFrame;
			transitionFrame = loopFrame;
		}
	}
	else {
		// still recording, there are two cases here
		Event* end = findEvent(RecordStopEvent);
		if (end == NULL) {
			// recording not closed, should only be here for the few functions
			// that are allowed during recording, right now only SpeedStep
			// there is no play jump for these
			// sigh, actually there is, the jump event is where we update
			// the output stream's latency, that still needs to happen
			// gak this is messy
			//ignore = true;
			if (transitionFrame < loopFrame) {
				// too late, we've already played a bit too far
				latencyLoss = loopFrame - transitionFrame;
				transitionFrame = loopFrame;
			}
		}
		else {
			// Recording is closing but we're waiting for a sync pulse
            // or this is AutoRecord with a scheduled ending.
			// Parent event will usually be pending on frame zero so 
			// transitionFrame will be negative.  For events stacked
            // after an AutoRecord ending they won't be pending.

			if (transitionFrame < 0) {
				latencyLoss = parent->frame - transitionFrame;
				transitionFrame = parent->frame;
			}
		}
	}

	if (!ignore) {

		jump = newEvent(JumpPlayEvent, transitionFrame);
		jump->savePreset(mTrack->getPreset());
		jump->latencyLoss = latencyLoss;

        // if the parent doesn't trace, neither do we
        jump->silent = parent->silent;

		// if we slammed into the parent, and the parent wants to be after
		// the loop point, so must we!
		if (jump->frame == parent->frame)
		  jump->afterLoop = parent->afterLoop;

		// Setting this negative tells jumpPlayEvent() to keep the current
		// playback position, used with functions that change playback
		// character but don't jump (speed, reverse, mute, etc.)
		jump->fields.jump.nextFrame = -1;

		parent->addChild(jump);

		Event* prev = findEvent(transitionFrame);
		addEvent(jump);

        if (!parent->silent) {
            if (jump->latencyLoss > 0)
              Trace(mTrack, 2, "EventManager: Jump frame %ld latency loss %ld\n", 
                    parent->frame, jump->latencyLoss);

            if (prev != NULL && prev->type == JumpPlayEvent)
              Trace(mTrack, 2, "EventManager: Overlapping play jumps %s/%s\n",
                    prev->getName(), jump->getName());
        }
	}

	return jump;
}

/**
 * Temporary method to schedule a particular kind of play jump,
 * used until we can get everything switched over to the new 
 * polymorphic jumps.
 *
 * Now used only for ReversePlayEvent.
 */
PUBLIC Event* EventManager::schedulePlayJumpType(Loop* loop, Event* parent, EventType* type)
{
	Event* jump = schedulePlayJump(loop, parent);
	if (jump != NULL)
	  jump->type = type;
	return jump;
}

/**
 * Schedule a jump within the current play layer.
 * Currently used by Replace and Speed.
 * !! Try to get rid of this and handle the jump location
 * in the event handler.
 */
PUBLIC Event* EventManager::schedulePlayJumpAt(Loop* loop, Event* parent, long frame)
{
	Event* jump = schedulePlayJump(loop, parent);
	if (jump != NULL) {
		jump->fields.jump.nextLayer = loop->getPlayLayer();
		jump->fields.jump.nextFrame = frame;
	}
	return jump;
}

/****************************************************************************
 *                                                                          *
 *                             RETURN SCHEDULING                            *
 *                                                                          *
 ****************************************************************************/

/**
 * Called to schedule a ReturnEvent to return to the previous loop
 * after a loop switch.  This can be called in two situations, 
 * first after we've processed a SwitchEvent and finished
 * the switch to a new loop but SwitchDuration is OnceReturn.
 * 
 * Second when we're in sustained switch due to either SUSNextLoop,
 * SUSPrevLoop, or LoopTrigger with SwitchDuration=SustainReturn.
 * In those cases we first schedule a pending SUSReturn event, then
 * when the up transition is received we cancel that and schedule
 * a ReturnEvent.
 *
 * If we already have a ReturnEvent, then if things are working properly
 * it means that you pressed NextLoop or PrevLoop successively until
 * you wrapped around to the original loop.  We keep the returns in a chain,
 * so Loop1:NextLoop->Loop2:NextLoop->Loop3:NextLoop will play loop 3
 * then return to loop2, then return to loop 1. If there are only 3 loops, 
 * then pressing NextLoop two more times would bring you back to Loop 2 which
 * already has a ReturnEvent to Loop 1.  I'm not sure how the EDP handles this.
 * It would be nice if we could maintain a "stack" of return events to any
 * level but it's overkill.  If we find a return event back to the same loop
 * we just leave it in place and don't schedule a new one.  
 *
 * If we find a return event to a different loop, then it could mean
 * that LoopTrigger was used to make random loops followed by NextLoop.
 * Again we'll leave the previous return behind but it may make more sense
 * to cancel the previous one and replace it with the new return so 
 * that NextLoop chains will work as expected?
 */
PUBLIC Event* EventManager::scheduleReturnEvent(Loop* loop, Event* trigger, 
                                                Loop* prev, bool sustain)
{
	Event* re = findEvent(ReturnEvent);
    Preset* preset = mTrack->getPreset();

	if (re != NULL)
	  Trace(mTrack, 1, "EventManager: Already have a return event!\n");

	else {
        // "sustain" is true if we're here due to the up transition
        // of SUSNextLoop or one of the other sustain/return functions.
        // These obey Switch Quanttize.
        // Otherwise we must be here for SWITCH_ONCE_RETURN which
        // always returns at the end.

        long returnFrame;
        if (sustain) {
            // SUS switches use SwitchQuantize to determine when to return
            // Assume you don't have to confirm the return
            Preset::SwitchQuantize q = preset->getSwitchQuantize();
            long loopFrame = loop->getFrame();
            switch (q) {
                case Preset::SWITCH_QUANT_CYCLE:
                case Preset::SWITCH_QUANT_CONFIRM_CYCLE: {
                    returnFrame = getQuantizedFrame(loop, loopFrame, Preset::QUANTIZE_CYCLE, true);
                }
                break;
                case Preset::SWITCH_QUANT_SUBCYCLE:
                case Preset::SWITCH_QUANT_CONFIRM_SUBCYCLE: {
                    returnFrame = getQuantizedFrame(loop, loopFrame, Preset::QUANTIZE_SUBCYCLE, true);
                }
                break;
                case Preset::SWITCH_QUANT_LOOP:
                case Preset::SWITCH_QUANT_CONFIRM_LOOP: {
                    returnFrame = getQuantizedFrame(loop, loopFrame, Preset::QUANTIZE_LOOP, true);
                }
                break;
                default:
                    // must be OFF
                    returnFrame = loopFrame;
                break;
            }
        }
        else {
            // must be SWITCH_ONCE_RETURN
            returnFrame = loop->getFrames();
        }

        re = newEvent(trigger->function, ReturnEvent, returnFrame);
        re->savePreset(preset);
        re->fields.loopSwitch.nextLoop = prev;
        re->quantized = true;	// so it can be undone

        // like SwitchEvent, this one needs to happen after the loop
        // so we can process sync events at the loop boundary just in
        // case this happens to be on the loop point
        re->afterLoop = true;

        long nextFrame = 0;
        Preset::SwitchLocation location = preset->getReturnLocation();
        if (location == Preset::SWITCH_RESTORE) {
            // restore playback to what the record frame was when we left 
            // this feels wrong, but it will be on the right quantization
            // boundary, and that's where we want to resume play
            // !! this is different than what we do for SwitchLocation
            // when we first switch to the loop which uses mSaveFrame,
            // should be using that here too?
            nextFrame = wrapFrame(prev->getFrame(), prev->getFrames());
        }
        else if (location == Preset::SWITCH_FOLLOW) {
            // carry the current frame over to the next loop
            int frames = prev->getFrames();
            if (frames > 0)
              nextFrame = wrapFrame(loop->getFrames(), frames);
        }
        else if (location == Preset::SWITCH_RANDOM) {
            // RANDOM_SUBCYCLE would be more useful?
            int frames = prev->getFrames();
            if (frames > 0)
              nextFrame = Random(0, frames - 1);
        }
        // else SWITCH_START, leave zero

        re->fields.loopSwitch.nextFrame = nextFrame;

        // If the next loop hasn't been recorded yet, then we have
        // to defer further scheduling to scheduleRecordStop
        // note that having an empty frame count can also mean we're in 
        // Reset so have to check the mode too
        if (loop->getFrames() == 0 && loop->getMode() == RecordMode) {
            Trace(mTrack, 2, "EventManager: Deferring return transition scheduling till end of record\n");
            re->pending = true;
            addEvent(re);
        }
        else
          finishReturnEvent(loop, re);
    }

	return re;
}

/**
 * Called to complete the scheduling of a Return event after
 * we know the loop length.
 */
PUBLIC void EventManager::finishReturnEvent(Loop* loop)
{
	Event* re = findEvent(ReturnEvent);
	if (re != NULL)
	  finishReturnEvent(loop, re);
}

/**
 * Called to complete the scheduling of a Return transition.
 * Called by setupReturnEvent if we know the loop length, otherwise
 * deferred to scheduleRecordStop.
 */
PRIVATE void EventManager::finishReturnEvent(Loop* loop, Event* re)
{
	if (re != NULL) {

        // it will be pending if we had to wait for the initial recording
        // to finish, otherwise the frame has been set
        if (re->pending) {
            re->frame = loop->getFrames();
            re->pending = false;
        }

        // should already be set, make sure
		re->quantized = true;

		// make sure this falls AFTER the RecordStopEvent
		if (isEventScheduled(re))
		  reorderEvent(re);
		else
		  addEvent(re);

        Loop* nextLoop = re->fields.loopSwitch.nextLoop;
		Trace(mTrack, 2, "EventManager: Scheduled return transition to frame %ld of loop %ld\n", 
			  re->fields.loopSwitch.nextFrame, 
              (long)nextLoop->getNumber());

		Event* jump = schedulePlayJump(loop, re);
		Layer* nextLayer = nextLoop->getPlayLayer();
		if (nextLayer == NULL)
		  nextLayer = loop->getMuteLayer();
		jump->fields.jump.nextLayer = nextLayer;
		jump->fields.jump.nextFrame = re->fields.loopSwitch.nextFrame;
	}
}

/**
 * ReturnEvent undo handler.
 */
PUBLIC void EventManager::returnEventUndo(Event* e)
{
	// exactly like a SwitchEvent
	switchEventUndo(e);
}

/**
 * Called from various places to cancel a return transition.
 */
PUBLIC bool EventManager::cancelReturn()
{
	Event* ret = NULL;

	// remove the event and all of its children
	mTrack->enterCriticalSection("cancelReturn");
	ret = findEvent(ReturnEvent);
	if (ret != NULL)
	  removeAll(ret);
	mTrack->leaveCriticalSection();

	if (ret != NULL) {
		returnEventUndo(ret);
		Trace(mTrack, 2, "EventManager: Return canceled\n");
	}

	return (ret != NULL);
}

/**
 * Retain events we want to carry over after a ReturnEvent.
 * !! Try to generalize this, how similar is this to switch?
 *
 * I think in practice we shouldn't have anything here other than
 * ScriptEvents and pending events. If we do they won't be transferred.
 */
PUBLIC void EventManager::cleanReturnEvents()
{
	Event* nexte;
	for (Event* e = mEvents->getEvents() ; e != NULL ; e = nexte) {
		nexte = e->getNext();

		bool transfer = false;
		if (e->pending) {
			// remains pending in the next loop
			transfer = true;
		}
		else if (e->type == ScriptEvent) {
			// !! if this was an "until" wait, may need to adjust the frame
			// but we've lost the context, just leave it where it is
			//e->frame = (e->frame - event->frame) + next->mFrame;
			transfer = true;
		}
		else {
            // What would this be? Trace this 
			Trace(mTrack, 1, "EventManager: Canceling event %s on loop during return!\n",
				  e->type->name);
		}
		
		if (!transfer)
          removeEvent(e);
	}
}

/****************************************************************************
 *                                                                          *
 *                               EVENT SUMMARY                              *
 *                                                                          *
 ****************************************************************************/

/**
 * Describe the scheduled events in a way convenient for display.
 *
 * This is called OUTSIDE the interrupt, usually from MobiusThread,
 * so we have to be careful about csects.
 * 
 * TODO: We're leaving this in a LoopState but really this belongs
 * in TrackState.
 */
PUBLIC void EventManager::getEventSummary(LoopState* s)
{
	s->eventCount = 0;
	Event* events = mEvents->getEvents();
    if (events != NULL) {
        mTrack->enterCriticalSection("getEventSummary");
        events = mEvents->getEvents();
        if (events != NULL) {
            for (Event* e = events ; e != NULL && s->eventCount < MAX_INFO_EVENTS ; 
                 e = e->getNext()) {

                getEventSummary(s, e, false);

                Loop* nextLoop = e->fields.loopSwitch.nextLoop;
                if (e->type == ReturnEvent)
                  s->returnLoop = nextLoop->getNumber();

                else if (e->type == SwitchEvent) {
                    s->nextLoop = nextLoop->getNumber();
                    // and the events stacked after the switch
                    for (Event* se = e->getChildren() ; 
                         se != NULL && s->eventCount < MAX_INFO_EVENTS ;
                         se = se->getSibling())
                      getEventSummary(s, se, true);
                }
            }
        }
        mTrack->leaveCriticalSection();
    }
}

/**
 * Helper for getEventSummary to get the summary for a single event.
 * 
 * Display only "meaningful" events, not things like JumpPlay that
 * we schedule as a child of another event.  In most cases, 
 * if an event is quantized or pending, it should be displayed.
 * Displaying unquantized events, even if they are the primary event
 * for a function can result in flicker since we will be processing
 * them almost immediately.  
 *
 * One exception is Script events, which are used to wait for a 
 * specific frame.  These aren't quantized, but they do need to be visible.
 *
 */
PRIVATE void EventManager::getEventSummary(LoopState* s, Event* e, bool stacked)
{
    if (isEventVisible(e, stacked)) {

        EventSummary* sum = &(s->events[s->eventCount]);
        sum->type = e->type;
        sum->function = e->function;

        //Trace(mTrack, 2, "Adding event summary %s\n", e->function->name);

		// Events with a meaningful integer argument are expected to
		// put it here.  The UI does not understand the difference between
		// events so set this non-zero only if relevant.
        sum->argument = e->number;

		// usually defines its own frame
		long frame = e->frame;

        Loop* loop = mTrack->getLoop();

		if (stacked) {
			// frame dependent on parent
            Event* p = e->getParent();
			if (!p->pending)
			  frame = p->frame;
			else {
				// must be in Confirm mode make it look pending
				frame = loop->getFrames();
			}
		}
		else if (e->pending) {
			// make it look like it is after the loop
			frame = loop->getFrames();
		}

        if (loop->isReverse())
          frame = reflectFrame(loop, frame);
        sum->frame = frame;

        s->eventCount++;
    }
}

/**
 * Helper for getEventSummary, determine if an event is supposed 
 * to be visible in the UI.
 * 
 * Display only "meaningful" events, not things like JumpPlay that
 * we schedule as a child of another event.  In most cases, 
 * if an event is quantized or pending, it should be displayed.
 * Displaying unquantized events, even if they are the primary event
 * for a function can result in flicker since we will be processing
 * them almost immediately.  
 *
 * One exception is Script events, which are used to wait for a 
 * specific frame.  These aren't quantized, but they do need to be visible.
 *
 * The "stacked" arg is false when we have a top-level event, and true
 * when we have a child event of one of the top level events.  This
 * indicates that the event is in a "stack" such as with LoopSwitchEvent.
 *
 * UPDATE 2012
 * 
 * This had some confusing logic that was preventing rescheduled
 * events from being drawn because they had a frame in the future
 * but were not quantized.  I don't like the idea of being smart
 * about hiding, if the goal for this was just to reduce flicker
 * then we can try looking at the distance between the event and the
 * current frame.  If they're too close then don't bother displaying.
 *
 * OLD COMMENTS:
 *
 * The trick here is for "automatic" events stacked after
 * the SwitchEvent to implement transfer modes.  We will always
 * generate a full set of events to force the loop into the desired
 * play state, but to avoid clutter in the UI, only display those that
 * are different than the current play state.
 *
 * UPDATE: no longer scheduling automatic events but keep this
 * around for awhile in case we need it.
 */
PRIVATE bool EventManager::isEventVisible(Event* e, bool stacked)
{
	bool visible = false;
    bool useOldLogic = false;

    if (useOldLogic) {
        if (!stacked)
          visible = (e->quantized || e->pending || e->type == ScriptEvent);

        // jump is always invisible
        else if (e->type != JumpPlayEvent) {
            if (!e->automatic)
              visible = true;
            else {
                // TODO, determine if the stacked event is meaningful
                // sigh, to get encapsulation will need another Function methods
                visible = true;
            }
        }
    }
    else {
        // jump is always invisible
        if (e->type != JumpPlayEvent) {
            if (stacked)
              visible = true;
            else {
                Loop* loop = mTrack->getLoop();
                long frame = loop->getFrame();
                long delta = e->frame - frame;

                visible = (e->quantized || e->pending || e->
                           type == ScriptEvent ||
                           // negative might be for reverse reflection?
                           delta < 0 ||
                           // this should be sensntive to latency?
                           delta > 1024);
            }
        }
    }

	return visible;
}

/**
 * Perform a simple loop size reflection of a frame.  Note that
 * events scheduled beyond the loop end will have negative reflected frames.
 * This is ok for getState the UI is expected to visualize these hanging
 * in space on the left.  
 *
 * This differs from Loop::reverseFrame() which is called during 
 * reverse processing and DOES care about frames going negative.
 *
 */
PRIVATE long EventManager::reflectFrame(Loop* loop, long frame)
{
	return (loop->getFrames() - frame - 1);
}


/****************************************************************************
 *                                                                          *
 *                              EVENT SELECTION                             *
 *                                                                          *
 ****************************************************************************/

/**
 * Return the next event in this track.
 */
PUBLIC Event* EventManager::getNextEvent()
{
	Event* event = NULL;
    Synchronizer* synchronizer = mTrack->getSynchronizer();

    // adjust the input stream for speed shifts performed
    // by the last event
    InputStream* istream = mTrack->getInputStream();
    istream->rescaleInput();

    // note that this is adjusted for speed scaling
    long remaining = istream->getScaledRemainingFrames();
	if (remaining > 0) {

        Loop* loop = mTrack->getLoop();

		// merge the sync events with the loop events
		// this is rather ugly, but at least it's encapsulated

        // Calculate the next available sync event.
        // For some events, the frame will be the offset within the current
        // interrupt buffer where this event should take place
		Event* sync = synchronizer->getNextEvent(loop);

		// Recalculate the frame relative to the loop.  This is the only
        // modification we're allowed to do to the event.
		if (sync != NULL) {

			long newFrame = loop->getFrame();

			if (!sync->immediate) {
				
 				long offset = sync->frame;
				if (offset < 0) {
                    // Pre 1.46 this might have happened if we tried to 
                    // position MIDI clocks relative to the start, but
                    // we never actually did that.
					Trace(mTrack, 1, "EventManager: Sync event offset lagging %ld!\n", offset);
					sync->frame = 0;
				}
                else if (offset == 0) {
                    // we always do these as soon as we can
                    // effectively the same as sync->immediate
                }
                else {
                    long consumed = istream->getOriginalFramesConsumed();
                    long delta = offset - consumed;
                    if (delta < 0) {
                        // This happened before 1.46 when a MIDI event would be
                        // scheduled in the middle of an interrupt, 
                        // and was added to the MidiQueue so that it would be
                        // processed immediately, it should have been left 
                        // at zero. It shouldn't happen in 1.46.
                        if (offset != 0) {
                            Trace(mTrack, 1, "EventManager: Sync event offset funny %ld, interrupt frames consumed %ld\n", 
                                  offset, consumed);
                            sync->frame = 0;
                        }
                    }
                    else if (delta > 0) {
                        // in practice this should only be true for HOST sync
                        // have to speed adjust the advance
                        float speed = istream->getSpeed();
                        if (speed != 1.0f) {
                            delta = (long)((float)delta * speed);
                        }

                        // It seems like we'll have ocaasional float
                        // rounding problems where delta will overshoot
                        // the end of the speed adjusted buffer and we won't
                        // process it in this interrupt.  Prevent this.
                        // Note that we often will be exactly on the remaining
                        // because we process events that fall at the beginning
                        // of the next buffer at the end of the current one
                        // so don't warn on this case.  See comments
                        // above EventManager::getNextScheduledEvent.  Instead
                        // of (delta >= remaining) use (delta > remaining).
                        if (delta > remaining) {
                            Trace(mTrack, 2, "EventManager: WARNING: Correcting speed adjusted sync event frame\n");
                            delta = remaining;
                        }
                        
                        newFrame += delta;
                    }
                }
			}

			sync->frame = newFrame;
		}

        // look for scheduled events that may preceed the sync event
		event = getNextScheduledEvent(remaining, sync);

		if (sync != NULL) {
			// advance if we decided to use it, otheriwise keep it till
            // next time
			if (event == sync)
			  synchronizer->useEvent(sync);
		}
	}
	
	return event;
}

/**
 * Remove and return the next scheduled event that is within range of an
 * input buffer.  We also inject pseudo events for subcycle, cycle,
 * and loop boundaries.
 *
 * The event list is ordered according to the time of addition, not sorted
 * by frame time.
 *
 * Also passing in a pending SyncEvent that will happen during this 
 * interrupt.  If this will occur before any other scheduled event return it.
 *
 * NOTE: The Extra Frame Range
 *
 * When determing the events that are within the range of this buffer
 * you would ordinarilly do:
 *
 *    lastFrame = startFrame + availFrames - 1
 *
 * For example for a starting frame of zero and a buffer of 128 the 
 * frames in the buffer are numbered 0 through 127.  Frame 128 won't 
 * technically be reached until the next interrupt.  For reasons I don't
 * remember we have for a long time not done this subtraction and considered
 * events that fall just outside the current buffer to be processed
 * in the current interrupt.  Comments indiciate this was "so we can
 * detect loop events".  

 * Although arguably incorrect, it actually doesn't matter since we
 * always process events BEFORE consuming the frame we are currently on.
 * So whether we process the event at the end of the current buffer
 * or at the beginning of the next buffer makes no difference.  At it
 * shouldn't.  There might be some subtle legacy logic that requires
 * the loop event to be processed early because the frame is about
 * to wrap but I can't see why that would be.  We should experiment
 * with that someday.  The side effect of this is that range checking
 * on events in EventManager have to allow them to fall one frame
 * outside the buffer without logging an error.
 */
PRIVATE Event* EventManager::getNextScheduledEvent(int availFrames, 
                                                   Event* syncEvent)
{
	Event* event = NULL;
	bool pseudo = false;

    Loop* loop = mTrack->getLoop();

	// if we're in "pause" mode during a Mute, we don't return events
	// !! how will we ever get out then, it's ok to process events,
	// we're just not advancing the clock?
	// what about sync events?
	//if (mPause) return syncEvent;

	// note that we consider the frame 1 greater than the actual
	// range so we can detect loop events
    // UPDATE: See method comments for subtles about this
	long startFrame = loop->getFrame();
	long lastFrame = startFrame + availFrames;

	// look for pending script events that happen at the loop boundary
	Event* pendingScript = NULL;

	// Locate the event nearest to the startFrame, or the first
	// event marked "immediate"

    for (Event* e = mEvents->getEvents() ; e != NULL ; e = e->getNext()) {
        if ((!loop->isPaused() || e->pauseEnabled) &&
            !e->pending && 
			(e->immediate ||
			 (e->frame >= startFrame && e->frame <= lastFrame))) {
			// within range
			if (event == NULL || e->immediate || e->frame < event->frame) {
				event = e;
				// stop on the first immediate event
				if (e->immediate)
				  break;
			}
			else if (e->getParent() == event && e->frame == event->frame) {
				// found a child on the same frame as it's parent,
				// but scheduled after, always do children first
                // NO! Only do this for JumpPlayEvent, now that
                // we stack things under Record a SwitchEvent may
                // be here too and we don't wan't that before the 
                // RecordEndEvent.  I'm really hating the child list...
                // Oh and ReversePlayEvent is another play jump
                if (e->type == JumpPlayEvent || e->type == ReversePlayEvent)
                  event = e;
                else if (e->type != SwitchEvent) {
                    // trace this for awhile to make sure we don't need
                    // to allow others
                    Trace(mTrack, 1, "EventManager: Child event on the same frame!\n");
                    event = e;
                }
            }
		}
		else if (e->pending && e->type == ScriptEvent &&
				 (e->fields.script.waitType == WAIT_START || 
				  e->fields.script.waitType == WAIT_END))
		  pendingScript = e;
	}

	// check the sync event
	// If a sync event and an immediate event get into a fight, who wins?
	// Assuming immediate event wins.  
	// NOTE: If the syncEvent and the scheduled event are on the same frame,
	// we prefer the scheduled event.
	if (syncEvent != NULL && 
		(event == NULL || 
		 (!event->immediate && syncEvent->frame < event->frame))) {
		event = syncEvent;
		pseudo = true;
	}

	// check for the pseudo synchronization events
	// we won't advance the clock on subCycle and cycle events, and 
	// sometimes won't on loop events so we must check the time of the
	// last reported sync event so we don't do it again!

	long loopFrames = loop->getFrames();

	// look for the loop start/end events
	// ignore unless the loop length has been set
	// also if we have an immediate event, it always runs before the
	// pseudo events, correct??

	if (loopFrames > 0 && (event == NULL || !event->immediate)) {
        bool found = false;

		if (loopFrames >= startFrame && loopFrames <= lastFrame &&
			loopFrames != mLastSyncEventFrame) {
			// the loop end is within range of the buffer
			if (event == NULL || 
				loopFrames < event->frame ||
				(loopFrames == event->frame && event->afterLoop)) {
				// the loop event is before any real events

				// If we found a pending script event, activate it
				// There are two types one for "Wait start" and another
				// for "Wait end".  Wait end will be processed immediately,
				// Wait start will be processed after we loop back to zero.
				event = NULL;
				if (pendingScript != NULL) {
					Trace(mTrack, 2, "EventManager: Activating pending script event\n");
					pendingScript->pending = false;
					if (pendingScript->fields.script.waitType == WAIT_START) {
						// the loop still happens first
						pendingScript->frame = 0;
					}
					else if (pendingScript->fields.script.waitType == WAIT_END) {
						// the event happens before the loop
						pendingScript->frame = loopFrames;
						event = pendingScript;
					}
				}

				if (event == NULL) {
					event = mSyncEvent;
					event->type = LoopEvent;
					event->frame = loopFrames;
					pseudo = true;
					mLastSyncEventFrame = loopFrames;
				}
				found = true;
			}
		}

        // if we're not on a loop boundary, check cycle boundary
		// since we don't advance the clock after this, have to be careful
		// not to emit the event again on the next call
		// note that we obey the afterLoop flag here for cycle and subcycle
		// boundaries too
		// don't treat frame 0 as a cycle boundary, this is actually
		// the same as the loop event above
        if (!found && startFrame > 0) {
            long next = getQuantizedFrame(loop, startFrame, Preset::QUANTIZE_CYCLE, false);
            if (next >= startFrame && next <= lastFrame && 
				next != mLastSyncEventFrame) {
                if (event == NULL || 
					next < event->frame ||
					(next == event->frame && event->afterLoop)) {
                    event = mSyncEvent;
					event->type = CycleEvent;
					event->frame = next;
                    pseudo = true;
                    found = true;
					mLastSyncEventFrame = next;
                }
            }
        }

        // if we're not on a cycle boundary, check subcycle boundary
        if (!found && startFrame > 0) {
            long next = getQuantizedFrame(loop, startFrame, Preset::QUANTIZE_SUBCYCLE, false);
            if (next >= startFrame && next <= lastFrame &&
				next != mLastSyncEventFrame) {
                if (event == NULL || 
					next < event->frame ||
					(next == event->frame && event->afterLoop)) {
                    event = mSyncEvent;
					event->type = SubCycleEvent;
					event->frame = next;
                    pseudo = true;
                    found = true;
					mLastSyncEventFrame = next;
                }
            }
        }
	}

    if (event != NULL) {

		if (!pseudo) {
			// this was a real event, splice it out of the list
			removeEvent(event);
		}

		if (event->immediate) {
			// this did not have a meaningful frame, but set the actual
			// frame before returning so we can use it in calculations
			event->frame = loop->getFrame();
		}
	}

	return event;
}

/****************************************************************************
 *                                                                          *
 *   						   EVENT PROCESSING                             *
 *                                                                          *
 ****************************************************************************/

/**
 * Call the handler for an event.  This may change the Track's loop object.
 * 
 * The event is freed at the end of this, scripts will be notified
 * if any are waiting on it, and if the event contains an Action
 * it will be returned to the pool.
 */
PUBLIC void EventManager::processEvent(Event* e)
{
    Loop* loop = mTrack->getLoop();
    Event* parent = e->getParent();

    if (loop->getMode() == ResetMode && 
		((e->function != NULL && !e->function->resetEnabled) ||
		 (parent != NULL && parent->function != NULL && 
		  !parent->function->resetEnabled))) {

		// If we hit the "play frame anomoly" condition, the play() method
		// will call reset().  If Track is hanging onto an Event when that
		// happens, it will still call this and the handlers get confused.
		// I think this can only happen with LoopEvent events?
		// UPDATE: If the event has no function, assume its valid for Reset,
		// necessary for SyncEvent, maybe others
        Trace(mTrack, 1, "EventManager: Ignoring event %s in reset\n", e->getName());
    }
	else if (e->reschedule) {
		// Should have handled these by now
		Trace(mTrack, 1, "EventManager: Attempt to process unscheduled event!\n");
	}
	else if (e->pending) {
		Trace(mTrack, 1, "EventManager: Attempt to process pending event!\n");
  	}
	else {
		// will callback to a handler
		e->invoke(loop);

		e->processed = true;

		// if this was a mode change event, reschedule vents
		rescheduleEvents(loop, e);
	}
    
    //!! should be able to call freeEvent here but I'm scared about
    // the difference between e->free that we've always done here
    // and e->freeAll tht freeEvent does...

	// script may be waiting on this specific event
    // NOTE: This will cause the script to run, would prefer that callers
    // resume them at a higher level
	e->finishScriptWait();

    // return the action to the pool
    Action* action = e->getAction();
    if (action != NULL) {
        //Trace(mTrack, 2, "Completing action\n");
        // detach them to avoid warnings
        action->detachEvent(e);
        mTrack->getMobius()->completeAction(action);
    }

	e->free();
}

/**
 * If we just did a mode change event, reschedule events.
 */
PRIVATE void EventManager::rescheduleEvents(Loop* loop, Event* previous) 
{
	if (previous->type->reschedules) {

		Event* resched = getRescheduleEvents(loop, previous);
		if (resched != NULL) {

			// formerly pick the closest one, can this ever not be the
			// first one?
			// !! if feels like it should be enough to do these in 
			// insertion order, if we never see any of the reschedule
			// warnings below can simplify this...

			Event* closest = NULL;
			Event* e;
			for (e = resched ; e != NULL ; e = e->getNext()) {
				if (e->function != NULL && 
					(closest == NULL || e->frame < closest->frame))
				  closest = e;
			}

			if (closest == NULL) {
				// something is horribly wrong, pick the first
				Trace(mTrack, 1, "EventManager: Reschedulable event went back in time!\n");
				closest = resched;
			}
			else if (closest != resched) {
				// I don't think this can happen, but need to make sure
				Trace(mTrack, 1, "EventManager: Reschedulable event order anomoly!\n");
			}

			// prune it out of the list
			Event* prev = NULL;
			for (e = resched ; e != closest ; e = e->getNext())
			  prev = e;

			if (prev == NULL)
			  resched = closest->getNext();
			else
			  prev->setNext(closest->getNext());
			closest->setNext(NULL);

			if (closest->function != NULL)
			  closest->function->rescheduleEvent(loop, previous, closest);

            // will the Action have been transfered?  
			closest->free();

			// then do the remainder in insertion order
			Event* next = NULL;
			for (e = resched ; e != NULL ; e = next) {
				next = e->getNext();
				e->setNext(NULL);
				if (e->function != NULL)
				  e->function->rescheduleEvent(loop, previous, e);
				e->free();
			}
		}
	}
}

/**
 * After processing an event for a function that reschedules the following
 * events, remove all remaining events that are marked for rescheduling.
 * Keep them in order!
 */
PRIVATE Event* EventManager::getRescheduleEvents(Loop* loop, Event* previous) 
{
	Event* events = NULL;
    Event* last = NULL;

	// if the previous event was scheduled at the loop end, consider it
	// at zero since the event frames should have been shifted by now
	// this happens for LoopEvent and RecordStopEvent
	long frame = previous->frame;
	if (frame == loop->getFrames())
		frame = 0;

	// prune out the reschedulable events
	mTrack->enterCriticalSection("getRescheduleEvents");
	Event* next = NULL;
    for (Event* e = mEvents->getEvents() ; e != NULL ; e = next) {
		next = e->getNext();
        if (!e->processed && !e->pending) {
			if (e->frame < frame) {
				// The only time this should happen is for Script events,
				// waiting for a specific frame to come around.  For anything
				// else it is probably an error?
				if (e->type != ScriptEvent)
				  Trace(mTrack, 1, "EventManager: Unexpected event order!\n");
            }
			else if (e->reschedule) {

				// shouldn't happen, will be ignored above
				if (e->function == NULL)
				  Trace(mTrack, 1, "EventManager: Rescheduleable event with no function!\n");	
				mEvents->remove(e);
                e->setTrack(NULL);
				if (last != NULL)
				  last->setNext(e);
				else
				  events = e;
				last = e;
			}
		}
	}
	mTrack->leaveCriticalSection();

    return events;
}

/****************************************************************************
 *                                                                          *
 *                                QUANTIZATION                              *
 *                                                                          *
 ****************************************************************************/

/**
 * Relatively general utility to calculate quantization boundaries.
 *
 * TODO: Here is where we need to add some form of "gravity" to bring
 * quantized back to the previous boundary rather than the next one
 * if we're within a few milliseonds of the previous one.
 *
 * If "after" is false, we'll return the current frame if it is already
 * on a quantization boundary, otherwise we advance to the next one.
 * 
 * Subcycle quant is harder because the Subcycles divisor can result
 * in a roundoff error where subCycleFrames * subcycles != cycleFrames
 * Example:
 *
 *     cycleFrames = 10000
 *     subcycles = 7
 *     subCycleFrames = 1428.57, rounds to 1428
 *     cycleFrames * subcycles = 1428 * 7 = 9996
 * 
 * So when quantizing after the last subcycle, we won't get to the 
 * true end of the cycle.  Rather than the usual arithmetic, we just
 * special case when subCycle == subcycles.  This will mean that the
 * last subcycle will be slightly longer than the others but I don't
 * think this should be audible.  
 *
 * But note that for loops with many cycles, this calculation needs to be
 * performed within each cycle, rather than leaving it for the last
 * subcycle in the loop.  Leaving it to the last subcycle would result
 * in a multiplication of the roundoff error which could be quite audible,
 * and furthermore would result in noticeable shifting of the 
 * later subcycles.
 * 
 * This isn't a problem for cycle quant since a loop is always by definition
 * an even multiply of the cycle frames.
 * 
 */
PUBLIC long EventManager::getQuantizedFrame(Loop* loop, long frame,
                                            Preset::QuantizeMode q, 
                                            bool after)
{
    long qframe = frame;
    long loopFrames = loop->getFrames();

	// if loopFrames is zero, then we haven't ended the record yet
	// so there is no quantization
	if (loopFrames > 0) {

		switch (q) {
			case Preset::QUANTIZE_CYCLE: {
				long cycleFrames = loop->getCycleFrames();
				int cycle = frame / cycleFrames;
				if (after || ((cycle * cycleFrames) != frame))


				  qframe = (cycle + 1) * cycleFrames;
			}
			break;

			case Preset::QUANTIZE_SUBCYCLE: {
				// this is harder due to float roudning
				// all subcycles except the last are the same size,
				// the last may need to be adjusted so that the combination
				// of all subcycles is equal to the cycle size

				long cycleFrames = loop->getCycleFrames();
                Preset* p = mTrack->getPreset();
				int subCycles = p->getSubcycles();
				// sanity check to avoid divide by zero
				if (subCycles == 0) subCycles = 1;
				long subCycleFrames = cycleFrames / subCycles;

				// determine which cycle we're in
				int cycle = frame / cycleFrames;
				long cycleBase = cycle * cycleFrames;
				
				// now calculate which subcycle we're in
				long relativeFrame = frame - cycleBase;
				int subCycle = (relativeFrame / subCycleFrames);
				long subCycleBase = subCycle * subCycleFrames;

				if (after || (subCycleBase != relativeFrame)) {
					int nextSubCycle = subCycle + 1;
					if (nextSubCycle < subCycles)
					  qframe = nextSubCycle * subCycleFrames;
					else {
						// special case wrap to true end of cycle
						qframe = cycleFrames;
					}
					// we just did a relative quant, now restore the base
					qframe += cycleBase;
				}
			}
			break;

			case Preset::QUANTIZE_LOOP: {
				int loop = (frame / loopFrames);
				if (after || ((loop * loopFrames) != frame))
				  qframe = (loop + 1) * loopFrames;
			}
			break;
			
			case Preset::QUANTIZE_OFF: {
				// xcode 5 complains if we don't have this
			}
			break;
		}
	}

    return qframe;
}

/**
 * For SlipBackward, locate the previous quantization boundary frame.
 * If the "before" flag is set, it means that if we're already on
 * a quantization frame, to find the one before the current frame.
 */
PUBLIC long EventManager::getPrevQuantizedFrame(Loop* loop, long frame, 
                                                Preset::QuantizeMode q,
                                                bool before)
{
    long qframe = frame;
	long loopFrames = loop->getFrames();

	// if loopFrames is zero, then we haven't ended the record yet
	// so there is no quantization
	if (loopFrames > 0) {

		switch (q) {
			case Preset::QUANTIZE_CYCLE: {
				long cycleFrames = loop->getCycleFrames();
				int cycle = frame / cycleFrames;
				long cycleBase = cycle * cycleFrames;

				if (frame > cycleBase)
				  qframe = cycleBase;
				else if (before) {
					// this may go negative, will wrap later
					qframe = (cycle - 1) * cycleFrames;
				}
			}
			break;

			case Preset::QUANTIZE_SUBCYCLE: {
				// this is harder due to float roudning
				// all subcycles except the last are the same size,
				// the last may need to be adjusted so that the combination
				// of all subcycles is equal to the cycle size

				long cycleFrames = loop->getCycleFrames();
                Preset* p = mTrack->getPreset();
				int subCycles = p->getSubcycles();
				// sanity check to avoid divide by zero
				if (subCycles == 0) subCycles = 1;
				long subCycleFrames = cycleFrames / subCycles;

				// determine which cycle we're in
				int cycle = frame / cycleFrames;
				long cycleBase = cycle * cycleFrames;
				
				// now calculate which subcycle we're in
				long relativeFrame = frame - cycleBase;
				int subCycle = (relativeFrame / subCycleFrames);
				long subCycleBase = subCycle * subCycleFrames;

				if (relativeFrame > subCycleBase) {
					// snap back to prev subcycle
					qframe = cycleBase + subCycleBase;
				}
				else if (before) {
					// on a subcycle, go to the previous one
					if (subCycle > 0)
					  qframe = cycleBase + ((subCycle - 1) * subCycleFrames);
					else {
						// the next to last subcycle is harder, it may
						// be a different size
						cycleBase -= cycleFrames;
						qframe = cycleBase + ((subCycles - 1) * subCycleFrames);
					}
				}
			}
			break;

			case Preset::QUANTIZE_LOOP: {
				int loop = (frame / loopFrames);
				long loopBase = loop * loopFrames;
				if (frame > loopBase)
				  qframe = loopBase;
				else if (before)
				  qframe = loopBase - loopFrames;
			}
			break;

			case Preset::QUANTIZE_OFF: {
				// xcode 5 complains if we don't have this
			}
			break;
		}

		// unlike calculating the next quantization boundary, 
		// here we have to wrap if we went negative
		qframe = wrapFrame(qframe, loopFrames);
	}

    return qframe;
}

/**
 * Common utility to wrap a calculated frame within the available
 * loop frames.  Used in a few places, move to a static util class...
 */
PUBLIC long EventManager::wrapFrame(long frame, long loopFrames)
{
	// !! use modulo here idiot
	if (loopFrames > 0) {
		while (frame >= loopFrames)
		  frame -= loopFrames;

		while (frame < 0)
		  frame += loopFrames;
	}
	return frame;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

