/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Core support for functions.
 *
 * Action defines the environment for invoking functions.
 * Function is the base class for all functions.
 *
 * We are in the process of moving function to their own files
 * under the "function" directory, the ones that remain here have
 * dependencies that will take some time to clean up.  
 *  
 */

#include <stdio.h>
#include <string.h>
#include <memory.h>

#include "List.h"
#include "MessageCatalog.h"
#include "Util.h"

#include "Action.h"
#include "Audio.h"
#include "ControlSurface.h"
#include "Event.h"
#include "EventManager.h"
#include "Layer.h"
#include "Loop.h"
#include "Messages.h"
#include "Mobius.h"
#include "Mode.h"
#include "Project.h"
#include "Recorder.h"
#include "Script.h"
#include "Stream.h"
#include "Synchronizer.h"
#include "SystemConstant.h"
#include "Track.h"

#include "Function.h"

/****************************************************************************
 *                                                                          *
 *                            GENERAL EVENT TYPES                           *
 *                                                                          *
 ****************************************************************************/

//////////////////////////////////////////////////////////////////////
//
// InvokeEvent
//
//////////////////////////////////////////////////////////////////////

/**
 * This is a special event type used to queue up the invocation
 * and scheduling of a function at a certain point.  The original use
 * for this was stacking functions after a loop switch, it will grow
 * to take on other responsibilities later.
 *
 * This was necessary because several functions make some complicated 
 * decisions about how to schedule their events when they are invoked,
 * and it is important that they be in the right context when that happens.
 * 
 * Originally, when a function was invoked during the switch quantize
 * period, we would go through a special form of scheduling (usually
 * scheduleSwitchStack()) that would create an event using the
 * normally scheduled event type for the supplied function 
 * (e.g. RecordEvent for the Record function, OverdubEvent for Overdub, etc.) 
 * and "stack" it as a child of the SwitchEvent.  Later when we evaluted
 * the SwitchEvent,  we would complete the switch, then call the event
 * handlers for any stacked events.  This worked as long as the 
 * stacked functions did their interesting work in the event handler,
 * and they only scheduled one event.
 * 
 * But along came AutoRecord.  AutoRecord schedules more than
 * one event when it is invoked, and it makes some complex decisions in the
 * invoke() method to determine how those events should be scheduled.  It
 * is important that it be invoked after a loop switch in exactly the
 * same way it is invoked in an empty loop.  What we needed was a way
 * to queue up a function to go through its normal invoke() procedure
 * after the loop switch.  The InvokeEvent was born.
 *
 * An InvokeEvent is just a placeholder for a function that will be
 * invoked at a certain time.  In order to determine the "semantic type"
 * of the event, you have to go through the function, e.g. 
 *
 *   event->function->eventType
 *
 * In retrospect this is a much better way of handling function stacking,
 * but we're phasing this in gradually.  Initially just for AutoRecord
 * but eventually this should replace scheduleSwitchStack and the 
 * logic under Loop::jumpPlayEvent needs to understand them.
 */
class InvokeEventType : public EventType {
  public:
	InvokeEventType();
	void invoke(Loop* l, Event* e);
};

PUBLIC InvokeEventType::InvokeEventType()
{
	name = "Invoke";

	// forces rescheduling of any events after this one
	// this shouldn't be necessary for the initial case of stacked
	// loop switch events, but may be later
	reschedules = true;
}

/**
 * NOTE: The InvokeEvent may be on the event list at the same
 * frame as other events.  For switches, the AutoRecord for example
 * will be before any generated mode transfer events (Forward, SpeedCancel,
 * etc.) because we move the stacked events to the new loop first.  
 * Now when we evaluate the InvokeEvent we'll schedule a new event
 * on the same frame, but it will be inserted into the event list
 * after any others on this frame.  In the AutoRecord example, 
 * it will be added after all the mode transfer events.  This isn't
 * necessarily bad and in this example it seems like the right thing
 * but there may be cases where the ordering of events is important,
 * in which case we would need to find a way to get the new event
 * inserted at the same list position as the InvokeEvent.
 */
PUBLIC void InvokeEventType::invoke(Loop* l, Event* e)
{
	Function* f = e->function;
	if (f != NULL)
	  f->invokeEvent(l, e);
	else
	  Trace(l, 1, "InvokeEvent called with no function!");
}

PUBLIC EventType* InvokeEvent = new InvokeEventType();

//////////////////////////////////////////////////////////////////////
//
// LoopEvent
//
//////////////////////////////////////////////////////////////////////

/**
 * Pseudo event generated dynamically by Loop when it reaches the loop boundary.
 */
class LoopEventType : public EventType {
  public:
	LoopEventType();
	void invoke(Loop* l, Event* e);
};

PUBLIC LoopEventType::LoopEventType()
{
	name = "Loop";
}

/**
 * This one has some fairly complicated work to do that
 * is still encapsulated in Loop.
 */
PUBLIC void LoopEventType::invoke(Loop* l, Event* e)
{
	l->loopEvent(e);
}

PUBLIC EventType* LoopEvent = new LoopEventType();

//////////////////////////////////////////////////////////////////////
//
// CycleEvent
//
//////////////////////////////////////////////////////////////////////

/**
 * Pseudo event generated dynamically by Loop when it reaches 
 * a cycle boundary.
 */
class CycleEventType : public EventType {
  public:
	CycleEventType();
	void invoke(Loop* l, Event* e);
};

PUBLIC CycleEventType::CycleEventType()
{
	name = "Cycle";
}

/**
 * Track will catch this and record the location for brother sync.
 * Here we check for Stutter mode and insert another cycle.
 *
 * !! For single cycle loops we won't see this event, need to handle
 * in loopEvent.
 */
PUBLIC void CycleEventType::invoke(Loop* l, Event* e)
{
    MobiusMode* mode = l->getMode();
    if (mode == StutterMode)
      l->stutterCycle();
}

PUBLIC EventType* CycleEvent = new CycleEventType();

//////////////////////////////////////////////////////////////////////
//
// SubCycleEvent
//
//////////////////////////////////////////////////////////////////////

/**
 * Pseudo event generated dynamically by Loop when it reaches 
 * a subCycle boundary.
 */
class SubCycleEventType : public EventType {
  public:
	SubCycleEventType();
	void invoke(Loop* l, Event* e);
};

PUBLIC SubCycleEventType::SubCycleEventType()
{
	name = "SubCycle";
}

/**
 * Called for the SubCycleEvent pseudo event.
 * We don't have anything special to do here, but Track will catch
 * this and record the location for brother sync'd tracks.
 */
PUBLIC void SubCycleEventType::invoke(Loop* l, Event* e)
{
}

PUBLIC EventType* SubCycleEvent = new SubCycleEventType();

//////////////////////////////////////////////////////////////////////
//
// JumpPlayEvent
//
//////////////////////////////////////////////////////////////////////

class JumpPlayEventType : public EventType {
  public:
	JumpPlayEventType();
	void invoke(Loop* l, Event* e);
	void undo(Loop* l, Event* e);
};

PUBLIC JumpPlayEventType::JumpPlayEventType()
{
	name = "JumpPlay";
}

PUBLIC void JumpPlayEventType::invoke(Loop* l, Event* e)
{
	l->jumpPlayEvent(e);
}

PUBLIC void JumpPlayEventType::undo(Loop* l, Event* e)
{
	l->jumpPlayEventUndo(e);
}

PUBLIC EventType* JumpPlayEvent = new JumpPlayEventType();

//////////////////////////////////////////////////////////////////////
//
// ValidateEvent
//
// This is scheduled during a loop switch after all of the other
// stacked events to be processed after the switch.  Its presence
// prevents the Loop::validate method from emitting any warning messages.
// 
//////////////////////////////////////////////////////////////////////

class ValidateEventType : public EventType {
  public:
	ValidateEventType();
	void invoke(Loop* l, Event* e);
};

PUBLIC ValidateEventType::ValidateEventType()
{
	name = "Validate";
}

PUBLIC void ValidateEventType::invoke(Loop* l, Event* e)
{
	l->validateEvent(e);
}

PUBLIC EventType* ValidateEvent = new ValidateEventType();

/****************************************************************************
 *                                                                          *
 *   							   FUNCTION                                 *
 *                                                                          *
 ****************************************************************************/

PUBLIC Function::Function()
{
    init();
}

PUBLIC Function::Function(const char* name, int key) :
    SystemConstant(name, key)
{
    init();
}

PRIVATE void Function::init()
{
	alias1 = NULL;
	alias2 = NULL;
	externalName = false;
	ordinal = 0;
    global = false;
    outsideInterrupt = false;
	index = 0;
	object = NULL;

	eventType = NULL;
    mMode = PlayMode;
	longFunction = NULL;

	majorMode = false;
	minorMode = false;
	instant = false;
	trigger = false;
	quantized = false;
	quantizeStack = false;
	sustain = false;
	maySustain = false;
	longPressable = false;
	resetEnabled = false;
	thresholdEnabled = false;
    cancelReturn = false;
	runsWithoutAudio = false;
	noFocusLock = false;
	focusLockDisabled = false;
	scriptSync = false;
	scriptOnly = false;
	mayCancelMute = false;
	cancelMute = false;
    mayConfirm = false;
    confirms = false;
    silent = false;

	spread = false;
	switchStack = false;
	switchStackMutex = false;
	activeTrack = false;
    expressionArgs = false;
    variableArgs = false;
}

PUBLIC Function::~Function()
{
}

PUBLIC bool Function::isFocusable()
{
	return (!noFocusLock && !focusLockDisabled);
}

PUBLIC bool Function::isScript()
{
    // hmm, is the best we have?
    return (eventType == RunScriptEvent);
}

/**
 * Refresh the cached display names from the message catalog.
 * Overload the one in SystemConstant so we can avoid warnings
 * about some function types that don't need display names.
 */
PUBLIC void Function::localize(MessageCatalog* cat)
{
    int key = getKey();
	if (key == 0) {
		if (!externalName && !scriptOnly)
		  Trace(1, "No catalog key for function %s\n", getName());
		// don't trash previously built display names for RunScriptFunction
        if (getDisplayName() == NULL)
		  setDisplayName(getName());
	}
	else {
		const char* msg = cat->get(key);
		if (msg != NULL)
		  setDisplayName(msg);
		else {
			Trace(1, "No localization for function %s\n", getName());
			setDisplayName(getName());
		}
	}
}

/**
 * This is true if the function can do something meaningful with
 * both a down and up transition.  Used by higher levels to determine
 * whether to send down "up" events.
 */
PUBLIC bool Function::isSustainable()
{
	return (sustain || maySustain || longPressable || longFunction != NULL);
}

/**
 * This is true if the function is considered a SUS function which
 * starts on the down transition and stops on the up transition.
 * In a few cases this is sensitive to the preset.
 */
PUBLIC bool Function::isSustain(Preset* p)
{
	return sustain;
}

/**
 * This is true if the function is can be used during recording.
 */
PUBLIC bool Function::isRecordable(Preset* p)
{
	return false;
}

/**
 * Helper function to determine if we're a mute cancel function.
 * Note that we'll treat MuteOn as an "edit" function even though
 * it can never cancel.  jumpPlayEvent will figure it out.
 */
bool Function::isMuteCancel(Preset* p)
{
	bool isCancel = false;

	switch (p->getMuteCancel()) {

		case Preset::MUTE_CANCEL_NEVER:
			break;

		case Preset::MUTE_CANCEL_EDIT:
			isCancel = majorMode || instant;
			break;

		case Preset::MUTE_CANCEL_TRIGGER:
			isCancel = (majorMode || instant || trigger);
			break;

		case Preset::MUTE_CANCEL_EFFECT:
			isCancel = (majorMode || instant || trigger || minorMode);
			break;

		case Preset::MUTE_CANCEL_CUSTOM:
			isCancel = (mayCancelMute && cancelMute);
			break;

		case Preset::MUTE_CANCEL_ALWAYS:
			isCancel = true;
			break;
	}

	return isCancel;
}

/**
 * True if this is a spreading function, or a reference to a spread script.
 */
PUBLIC bool Function::isSpread() 
{
    bool result = this->spread;
    if (eventType == RunScriptEvent) {
        Script* script = (Script*)object;
        if (script != NULL)
          result = script->isSpread();
    }
	return result;
}

/**
 * Called by Track immediately before invoking a function during the
 * up transition of a trigger has been sustained passed the long press
 * interval.  Here the function may want to substitute another function
 * before invoking.  Typically this is the SUS variant of the 
 * trigger function.
 */
PUBLIC Function* Function::getLongPressFunction(Action* action)
{
	return ((longFunction != NULL) ? longFunction : this);
}

PUBLIC void Function::trace(Action* action, Mobius* m)
{
	// suppress if we're rescheduling since we've already
	// done a rescheduling message and it looks like a function came in
	if (action->rescheduling == NULL && !action->noTrace)
	  Trace(m, 2, "Function %s %s\n", getName(), ((action->down) ? "down" : "up"));
}

PUBLIC void Function::trace(Action* action, Loop* l)
{
	if (action->rescheduling == NULL && !action->noTrace)
	  Trace(l, 2, "Function %s %s\n", getName(), ((action->down) ? "down" : "up"));
}

/**
 * This must be overloaded in the functions that claim to be global.
 */
PUBLIC void Function::invoke(Action* action, Mobius* m)
{
	Trace(m, 2, "Unimplemented global function %s\n", getName());
}

/**
 * Base function processor.  Simpler functions will use this
 * and overload the scheduleEvent method.  More complex functions will
 * overload the entire invoke method.
 *
 * NOTE: When we reschedule functions due to escaping quantization, 
 * we'll call this again after undoing the previous event.  But if the
 * previous event was scheduled by a SUS function, it may have left a
 * reschedulable event for the up transition which we'll find again here,
 * and think we need to escape THAT.  
 * ?? Not sure what the right thing is, having escaping blow away both
 * SUS events feels right in some cases, but might be nice to leave the
 * up transition in place?  
 * !! Either way, if we want to remove the up event with the down event,
 * then there should be a relationship between the events rather than 
 * finding it now?
 */ 
PUBLIC Event* Function::invoke(Action* action, Loop* loop)
{
	Event* event = NULL;
    Track* track = loop->getTrack();
    EventManager* em = track->getEventManager();
	Preset* preset = track->getPreset();
	MobiusMode* mode = loop->getMode();
	bool sus = isSustain(preset);

	// it is ok to call global functions on loops, but only if they have
	// an event that can be scheduled, necessary for FullMute
	if (global && eventType == NULL) {
        Trace(1, "Cannot invoking global function %s on a loop\n", getName());
        return NULL;
    }

	if (action->down || sus) {
		
		trace(action, loop);

		if (mode == ThresholdMode && !thresholdEnabled) {
			// still waiting
            Trace(loop, 2, "Ignoring Action in Threshold mode\n");
		}
		else if (mode == SynchronizeMode && !thresholdEnabled) {
			// waiting for a sync boundary, this is a lot like
			// threshold mode so we'll use the same thresholdEnabled flag 
            Trace(loop, 2, "Ignoring Action in Synchronize mode\n");
		}
        else if (em->isSwitching()) {
			// functions are handled differently inSwitchMode or ConfirmMode
            if (!em->isSwitchConfirmed() && confirms) {
                // this is a switch confirmation action
                Confirm->invoke(action, loop);
            }
            else
              event = scheduleSwitchStack(action, loop);
        }   
/*
        else if (mode == RealignMode && !realignEnabled) {
            // waiting for the external loop start point to realign
        }
*/
		else if (!resetEnabled && mode == ResetMode) {
			// ignore
		}
		else {
			// need to conditionalize this?
            // Several functions have special handling for previous events,
            // try to encapsulate that so we can overload that without 
            // overloading scheduleEvent() ?
			Event* prev = em->findEvent(eventType);

			// !! if this is a "reschedulable" event, then it is proabaly
			// a SUS up transition so leave it alone, is this
			// what we want always?
			if (prev != NULL && prev->reschedule) {
				Trace(loop, 2, "Ignoring escape of reschedulable event %s(%s) %ld\n",
					  prev->getName(), prev->getFunctionName(), prev->frame);
				prev = NULL;
			}

			// If we're comming from a script, treat it like a SUS and let
			// it be scheduled on the next quantization boundary rather than
			// escaping, since escaping doesn't really make sense in scripts.
			// Oh, I supposed it could, but it is clearer to turn quantization
			// off temporarily if that's what you want.  This does however
			// mean that the script recorder may record somethign that
			// was actually escaped, but won't be played back that way. 	
			// Will need a flag in the script that says whether to perform
			// quantize escaping or not and test it here

			if (prev != NULL && !sus && action->trigger != TriggerScript) {

				// an event was already posted, treat the second invocation
				// as a "double click" and process the event immediately

				if (prev->quantized) {
					escapeQuantization(action, loop, prev);
				}
				else {
					// comming in too fast, ignore? stack?
					Trace(loop, 1, "Function %s comming in to fast, ignoring\n",
						  getName());
				}
			}
			else {
				// If we're in a loop entered with SwitchDuration=OnceReturn
                // or SustainReturn and there is a return Transtion to the
                // previous loop, cancel it
				if (cancelReturn)
				  em->cancelReturn();

                // end the recording if this is not a Record function
                // (may have already ended it)
                // !! I hate having this here, should redirect 
                // through MobiusMode and let it end

                // !! Ugh this is messy.  Ending RecordMode is similar
                // to ending MultiplyMode in that we'll unconditionally
                // schedule the RecordStopEvent, then go through
                // normal event scheduing which if this is the Record
                // function will decide to ignore it since we already
                // scheduled the RecordStopEvent.  In that case, the
                // primary event is the RecordStopEvent.  When
                // scheduleEventDefault calls Record::scheduleEvent
                // it calls Synchronizer::scheduleRecordStart which
                // recognizes this and just returns the previously
                // schedule event.  But that one will have the cloned
                // action and attempting to set it to the primary 
                // action gets an error.  This really needs to be redesigned
                // so that we handle all mode endings consistently.

				if (mode == RecordMode) {
                    // A few functions like rate shift are allowed to
                    // happen during recording, most end the recording.
                    // Currently only Midi, Rate, Speed.
					if (!isRecordable(loop->getPreset())) {
                        // an internal event, need to clone the action
                        // unless this is Record itself, see mess above
                        Mobius* m = loop->getMobius();
                        Action* stopAction = action;
                        if (this != Record && this != SUSRecord &&
                            this != Rehearse) {
                            stopAction = m->cloneAction(action);
                        }

                        (void)Record->scheduleModeStop(stopAction, loop);

                        if (stopAction != action)
                          m->completeAction(stopAction);
                    }
				}

				// perform function-specific processing and scheduling
                // if we're ending RecordMode with Record this will be ignored
                // since we've already scheduled the stop event above, 
                // but if this is AutoRecord have to work through the 
                // machinery so Synchronizer can extend the previous stop
				event = scheduleEvent(action, loop);
			}
		}
	}

    // Bind the event and action if not already bound.  Usually
    // they will already be bound but scheduleSwitchStack isn't doing it so 
    // rather than track down all the places this is our final catch
    // on the way out.
    if (event != NULL && action->getEvent() == NULL)
      action->setEvent(event);

	return event;
}

/**
 * Default method called when an InvokeEvent is evaluated.
 * These are placeholder events for functions that needs to go through
 * their normal invoke() processing after something significant happens
 * (such as a loop switch).
 *
 * This is very much like rescheduleEvent, but the subtlety is how
 * quantization works with the action->rescheduling event.
 */
PUBLIC void Function::invokeEvent(Loop* l, Event* e) 
{
    // Original Action must be left on the event, steal it
    Action* action = e->getAction();

    if (action == NULL)
      Trace(l, 1, "Function::invokeEvent event with no action!\n");
    else {
        action->detachEvent(e);

        action->inInterrupt = true;

        // never a latency adjust at this point
        action->noLatency = true;

        // This is what rescheduleEvent would do but I don't think
        // it applies here.  This will try to reuse the current event
        // which may be okay but be safe and reevaluate it.
        // action->rescheduling = e;

        Event* realEvent = invoke(action, l);

        if (realEvent != NULL) {
            // if we had a Wait last on the pending event, switch it to waiting
            // for the new event
            ScriptInterpreter* si = e->getScript();
            if (si != NULL)
              si->rescheduleEvent(e, realEvent);
        }
        
        // reclaim the action if the new event doesn't want it
        if (realEvent == NULL || realEvent->getAction() != action)
          l->getMobius()->completeAction(action);
    }
}

/**
 * Called when an existing quantized function event event was
 * found.  The second invocation of the function "escapes" the 
 * quantized event.
 *
 * Note that just shifting the events isn't enough.   Some events like
 * JumpPlayEvent or switches with SwitchLocation=Follow need to
 * have their nextFrame reclaculated.
 *
 * The most robust thing is to undo the current event and reschedule, but
 * have to be careful to disable quantization.
 *
 * Not a good way to disable quantization without passing another
 * argument through invoke & scheduleEvent.  Altering the preset wouldn't be
 * disruptive, but is a little mysterious.  Could also hang it in 
 * InputStream.
 * 
 */
PUBLIC void Function::escapeQuantization(Action* action, Loop* loop,
										 Event* prev)
{
	// !! Should we even be allowing an up transition to escape quant?
	if (!action->down)
	  Trace(loop, 1, "Ignoring SUS up transition for quantization escape\n");
	else {
		Trace(loop, 2, "Escaping quantized event %s(%s) %ld\n", 
			  prev->getName(), prev->getFunctionName(), prev->frame);

		// remove, cancel side effects, and free
        EventManager* em = loop->getTrack()->getEventManager();
		em->undoEvent(prev);

		// then replay the function invocation, without quantization
		action->escapeQuantization = true;

		// !! not sure how the up/down value here corresponds to what we're
		// rescheduling, assume we can force it down?  May need to 
		// remember the state of the original invocation.  
		invoke(action, loop);
	}
}

// First implemntation that did time shift
#if 0
PUBLIC void Function::escapeQuantization(Loop* loop, Event* prev)
{
	Trace(loop, 2, "Escaping quantized %s\n", name);

	MobiusMode* mode = loop->getMode();

	// You can't just start it immediately, it has to go after InputLatency

	// Kludge like Loop::getFunctionEvent we have to know if
	// this is a script function with an absolute time
	// !! figure out a cleaner way to detect this
	long latencyAdjust = loop->getInputLatency();
	if (context->noLatency)
	  latencyAdjust = 0;

	long currentFrame = loop->getFrame();
	long prevFrame = prev->frame;
	long newFrame = currentFrame + latencyAdjust;
	long delta = prevFrame - newFrame;

	// this can only be less right
	if (delta < 0)
	  Trace(loop, 1, "Negative quantization escape delta!\n");
	else {
		prev->frame -= delta;
		Trace(loop, 2, "Adjusting previous %s event frame from %ld to %ld\n",
			  prev->getName(), prevFrame, prev->frame);

		// this is also no longer quantized so it can't be undone and
		// if we get the function again ignore it
		prev->quantized = false;

		// if we have any child events, slide them by a similar amount
		// but don't let them go back in time
		for (Event* child = prev->children ; child != NULL ; 
			 child = child->sibling) {

			if (!child->processed) {

				long newChildFrame = child->frame - delta;
				if (newChildFrame < currentFrame)
				  newChildFrame = currentFrame;

				Trace(loop, 2, "Adjusting previous %s child event frame from %ld to %ld\n",
					  child->getName(), child->frame, newChildFrame);

				child->frame = newChildFrame;
			}
		}
	}
}
#endif

/**
 * Defualt event scheduler that may be overloaded.
 * If you overload this, you should still call up here to 
 * setup the multiply/insert ending.
 *
 * scheduleEventDefault factored out so that we could call it from
 * Synchronizer since we three levels of handlers, RecordFunction, 
 * Synchronizer and then back to Function.
 */
PUBLIC Event* Function::scheduleEvent(Action* action, Loop* loop)
{
    return scheduleEventDefault(action, loop);
}

/**
 * Defualt event scheduler.
 *
 * If we're in a rounding mode, a mode ending event may be scheduled.
 * In these cases the trigger event may be stacked to run after the
 * mode end event, or it may simply be ignored.  For example when you
 * end Multiply mode with the Multiply function, we only need to end the
 * mode, we don't want another Multiply event to put us back into multiply.
 * In these cases the trigger event will be freed and this method must
 * return NULL so that the Function handlers don't think they have
 * a normal function event and try to do things like schedule a play jump.
 *
 * In these cases the Action will point to the mode end event.
 * The control flow is a little weird, but fixing it requires some complicate
 * refactoring.
 */
PUBLIC Event* Function::scheduleEventDefault(Action* action, Loop* loop)
{
    Track* track = loop->getTrack();
    EventManager* em = track->getEventManager();

    // build the fundamental function event, possibly quantized
    // it is not scheduled
	Event* event = em->getFunctionEvent(action, loop, this);

    if (event != NULL) {

        MobiusMode* mode = loop->getMode();
		Event* modeEnd = NULL;

		if (!event->reschedule && !event->type->noMode && mode->rounding) {
            // Let the mode decide how to handle the triggr event, 
            // it may use it or free it.  
			modeEnd = loop->scheduleRoundingModeEnd(action, event);
		}
		else {
            // normal trigger event, add to list
            em->addEvent(event);
        }

		if (modeEnd != NULL && modeEnd->getParent() == NULL) {

            // This indicates that the mode end scheduling decided it did
            // not need to keep the triggering event and has deleted it.
            // Must return null to prevent further event processing.
            // Action will now be owned by modeEnd.
            event = NULL;
		}
	}

	// if we're in a pause mute, always come out?
	loop->setPause(false);

	return event;
}

/**
 * Default mode stop scheduler.
 * This was added so we could encapsulate all the complex end scheduling
 * logic for the Record function in the RecordFunction class rather than
 * having bits and pieces in Loop.
 *
 * This isn't used anywhere else but it's a step toward a generic
 * "end your mode" interface that could be used for other functions
 * that have their own modes (Multiply, etc).
 */
PUBLIC Event* Function::scheduleModeStop(Action* action, Loop* l)
{
    return NULL;
}

/**
 * Undo some aspect of the stop event of the current mode.
 * Return true if we were able to undo something.
 * 
 * This was added so we could encapsulate all of the sync/auto record ending
 * frame management in RecordFunction rather than in Loop or UndoFunction.
 * Currently RecordFunction is the only thing that implements this, to 
 * make it truly generic Loop should ask the MobiusMode to undo.
 */
PUBLIC bool Function::undoModeStop(Loop* l)
{
    return false;
}

/**
 * Default implementation of scheduleTransfer.
 * This is only implemented by things that restore themselves after
 * loop switch.
 */
PUBLIC Event* Function::scheduleTransfer(Loop* l)
{
	Trace(l, 1, "scheduleTransfer not implemented for %s\n", getName());
    return NULL;
}

/**
 * Default long press handler for global functions.
 */
PUBLIC void Function::invokeLong(Action* action, Mobius* m)
{
}

/**
 * Default long press handler for track functions.
 */
PUBLIC void Function::invokeLong(Action* action, Loop* l)
{
	// TODO: If this is a long-pressable function, can emit
	// a temorary message to indiciate the mode transition
}

/**
 * Reschedule a function start event that had been previously scheduled.
 * This is called by Loop as it processes events that may change the
 * mode or other characteristics of the loop that would effect previously
 * scheduled events after this one.  
 * 
 * In some cases, the event handlers may be smart enough to detect
 * that if we're already in a mode we should end the mode rather than
 * start it again.  But rescheduling is still desireable so we can
 * setup fades and transitions before we reach the event frame.
 *
 * Loop will free the source event after we return.
 */
PUBLIC Event* Function::rescheduleEvent(Loop* l, Event* prev, Event* next)
{
	Event* newEvent = NULL;

    // Original Action must be left on the event, steal it and replay it
    Action* action = next->getAction();
    
    if (action == NULL)
      Trace(l, 1, "Function::rescheduleEvent: event with no action!\n");
    else {
        action->detachEvent(next);

        // This lets the event scheduler know that we had done this
        // before and should keep the same frame.  Do we need the
        // event that caused the reschedule?
        action->rescheduling = next;
        action->reschedulingReason = prev;

        // FunctionContext used to do this, shouldn't be necessary?
        if (!action->down) {
            Trace(l, 1, "Forcing rescheduled action down!\n");
            action->down = true;
        }

        // FunctionContext used to do this
        if (action->getFunction() != this)
          Trace(l, 1, "Rescheduled action has wrong function!\n");

        Trace(l, 2, "Rescheduling %s\n", getName());

        newEvent = invoke(action, l);

        bool traceReschedule = false;
        if (traceReschedule) {
            if (newEvent != NULL) {
                printf("Rescheduled %s event from %ld to %ld\n",
                       getName(), next->frame, newEvent->frame);
            }
            else {
                printf("Rescheduled %s event from %ld to nothing!n",
                       getName(), next->frame);
            }
            fflush(stdout);
        }

        if (newEvent != NULL) {
            // if we had a Wait last on the pending event, switch it to waiting
            // for the new event
            ScriptInterpreter* si = next->getScript();
            if (si != NULL)
              si->rescheduleEvent(next, newEvent);
        }

        // reclaim the action if the new event doesn't want it
        if (newEvent == NULL || newEvent->getAction() != action)
          l->getMobius()->completeAction(action);

        // this event is going to be freed, so even though we shouldn't
        // use this again take away the reference so we aren't tempted
        action->rescheduling = NULL;
        action->reschedulingReason = NULL;
    }

	return newEvent;
}

/**
 * Default handler for an event scheduled by this function, called
 * by the generic EventType when the event time is reached.
 * If a function schedules an event with the generic EventType, it must
 * overload this method.
 */
PUBLIC void Function::doEvent(Loop* loop, Event* event)
{
	Trace(loop, 1, "Unimplemented doEvent method for %s\n", event->type->name);
}

/**
 * Default handler to activate pending events.
 * This is a transitional interface, not all pending events are activated
 * through this method but eventually they will.
 */
PUBLIC void Function::confirmEvent(Action* action, Loop* loop, 
                                   Event* event, long frame)
{
	Trace(loop, 1, "Unimplemented confirmEvent method for %s\n", event->type->name);
}

/**
 * Default undo handler for an event scheduled by this function, called
 * by the generic EventType when the the event is undone.
 * If a function schedules an event with the generic EventType, it must
 * overload this method.
 */
PUBLIC void Function::undoEvent(Loop* l, Event* e)
{
	Trace(l, 1, "No undo handler for event %s\n", e->type->name);
}

/**
 * Default handler for function-specific adjustments to a play jump.
 * This is typically overloaded by any class that may schedule
 * a JumpPlay event or may be stacked on a SwitchEvent. In simple cases,
 * we dont' need any special preparation, the next layer and frame
 * were just left on the jump event.
 */
PUBLIC void Function::prepareJump(Loop* loop, Event* event, JumpContext *jump)
{
}

/**
 * Default handler for function-specific adjustments to a play jump
 * that occurs during a loop switch.
 * 
 * This should be overloaded by any function that lets itself be stacked
 * on a switch.
 *
 * Loop::adjustSwitchJump still has most of the logic, we're phasing
 * this in gradually.
 */
PUBLIC void Function::prepareSwitch(Loop* loop, Event* event, 
									SwitchContext* actions, JumpContext *jump)
{
}

/**
 * Select the next or previous preset.
 * This is an EDPism used by a few function event handlers (Insert, Mute)
 * which can change presets when in Reset mode.
 * Mute doesn't do that any more so this is only half implemented and since
 * it's obscure consider taking it out.
 */
PUBLIC void Function::changePreset(Action* action, Loop* loop, bool after)
{
    Mobius* m = loop->getMobius();
    MobiusConfig* config = m->getConfiguration();
    Preset* presets = config->getPresets();
    Preset* current = loop->getPreset();
    Preset* next = NULL;

    if (current != NULL && presets != NULL) {
        if (after)
          next = current->getNext();
        else if (current == presets) {
            // get the last one
            for (Preset* p = presets ; p != NULL ; p = p->getNext())
              next = p;
        }
        else {
            for (Preset* p = presets ; p != NULL ; p = p->getNext()) {
                if (p->getNext() == current) {
                    next = p;
                    break;
                }
            }
        }

        if (next != NULL && next != current)
          m->setPresetInternal(next->getNumber());
    }
}

/****************************************************************************
 *                                                                          *
 *   						  LOOP SWITCH STACK                             *
 *                                                                          *
 ****************************************************************************/

/**
 * Default event scheduler when in SwitchMode or ConfirmMode.
 * 
 * Called for functions that "stack" and are performed after
 * the loop switch.  If we see the function more than once
 * it cancels.
 *
 * Originally I treated any stacked function as a confirmation event,
 * but I'd rather wait for a specific confirmation so we can stack
 * several functions.  May want an option?
 */
Event* Function::scheduleSwitchStack(Action* action, Loop* l)
{
	Event* event = NULL;
    Track* track = l->getTrack();
    EventManager* em = track->getEventManager();

	if (action->down && switchStack) {

		Event* switche = em->getUncomittedSwitch();
		if (switche == NULL) {
			Trace(l, 2, "Loop: Switch already committed, ignoring stacking of %s!\n",
				  getName());
		}
		else if (this == AutoRecord) {
			// kludge: schedule certaion functions as InvokeEvents, should be
			// doing all of them this way!!

			// successive invocations multiplies the recording
			Event* prev = switche->findEvent(InvokeEvent, this);
			if (prev != NULL) {
				// !! this should be multipled by RecordBars
				// which means the Function needs to have a method
				// to adjust the event
				prev->number = prev->number + 1;
			}
			else {
				event = em->newEvent(this, InvokeEvent, 0);
				em->scheduleSwitchStack(event);
			}
		}
        else {
			// the old way
			Event* prev = switche->findEvent(eventType);
			if (prev != NULL)
			  em->cancelSwitchStack(prev);
			else {
				event = em->newEvent(this, 0);
				em->scheduleSwitchStack(event);
			}
		}
	}

    if (event != NULL)
      action->setEvent(event);

	return event;
}

/****************************************************************************
 *                                                                          *
 *   						 REPLICATED FUNCTION                            *
 *                                                                          *
 ****************************************************************************/

/**
 * Extenion used by functions that support a numeric multiplier.
 * Some functions have both a set of relative and absolute functions
 * so we multiply only when the replicated flag is on.
 */
ReplicatedFunction::ReplicatedFunction()
{
	replicated = false;
}

PUBLIC void ReplicatedFunction::localize(MessageCatalog* cat)
{
	Function::localize(cat);
	if (replicated) {
		const char* pattern = cat->get(getKey());
		if (pattern == NULL) {
			Trace(1, "No localization for function %s\n", getName());
			pattern = getName();
		}
		sprintf(fullDisplayName, pattern, index + 1);
		setDisplayName(fullDisplayName);
	}
}

/****************************************************************************
 *                                                                          *
 *                           STATIC FUNCTION ARRAYS                         *
 *                                                                          *
 ****************************************************************************/
/*
 * Originally used static arrays of Function object pointers, but once
 * we started moving Function subclasses out to different files, this
 * wasn't reliable because the static Function objects did not always
 * exist when this array was initialized.  It wasn't as simple as moving
 * the subclasses earlier in the compilation list.
 *
 * Instead, we'll build the arrays at runtime.   Before doign any searches
 * on static functions, Mobius needs to call Function::initStaticFunctions.
 *
 * NOTE: In theory this could be concurrently access by more than one plugin
 * but that would be very rare since hosts would have to create them
 * in different threads.  If that can happen will need a csect around this.
 */

#define MAX_STATIC_FUNCTIONS 256

PUBLIC Function* StaticFunctions[MAX_STATIC_FUNCTIONS];
PUBLIC Function* HiddenFunctions[MAX_STATIC_FUNCTIONS];
PRIVATE int FunctionIndex = 0;

PRIVATE void add(Function** array, Function* func)
{
	if (FunctionIndex >= MAX_STATIC_FUNCTIONS - 1) {
		printf("Static function array overflow!\n");
		fflush(stdout);
	}
	else {
		array[FunctionIndex++] = func;
		// keep it NULL terminated
		array[FunctionIndex] = NULL;
	}
}

/**
 * Called early during Mobius initializations to initialize the 
 * static function arrays.  This must be called before you attempt
 * to compile scripts.  These arrays never changed once initialized.
 */
PUBLIC void Function::initStaticFunctions()
{
    // ignore if already initialized
    if (FunctionIndex == 0) {
        add(StaticFunctions, GlobalReset);
        add(StaticFunctions, GlobalMute);
        add(StaticFunctions, GlobalPause);
        add(StaticFunctions, Reset);
        add(StaticFunctions, TrackReset);
        add(StaticFunctions, Clear);
        add(StaticFunctions, Confirm);
        add(StaticFunctions, Record);
        add(StaticFunctions, AutoRecord);
        add(StaticFunctions, Rehearse);
        add(StaticFunctions, Bounce);
        add(StaticFunctions, Play);
        add(StaticFunctions, Overdub);
        add(StaticFunctions, OverdubOn);
        add(StaticFunctions, OverdubOff);
        add(StaticFunctions, Multiply);
        add(StaticFunctions, InstantMultiply);
        add(StaticFunctions, InstantMultiply3);
        add(StaticFunctions, InstantMultiply4);
        add(StaticFunctions, Divide);
        add(StaticFunctions, Divide3);
        add(StaticFunctions, Divide4);
        add(StaticFunctions, Insert);
        add(StaticFunctions, Stutter);
        add(StaticFunctions, Replace);
        add(StaticFunctions, Substitute);
        add(StaticFunctions, Shuffle);
        add(StaticFunctions, Mute);
        add(StaticFunctions, MuteOn);
        add(StaticFunctions, MuteOff);
        add(StaticFunctions, Pause);
        add(StaticFunctions, Solo);
        add(StaticFunctions, Undo);
        add(StaticFunctions, ShortUndo);
        add(StaticFunctions, LongUndo);
        add(StaticFunctions, UndoOnly);
        add(StaticFunctions, Redo);
        add(StaticFunctions, SpeedCancel);
        add(StaticFunctions, SpeedOctave);
        add(StaticFunctions, SpeedStep);
        add(StaticFunctions, SpeedBend);
        add(StaticFunctions, SpeedUp);
        add(StaticFunctions, SpeedDown);
        add(StaticFunctions, SpeedNext);
        add(StaticFunctions, SpeedPrev);
        add(StaticFunctions, SpeedToggle);
        add(StaticFunctions, TimeStretch);
        add(StaticFunctions, Halfspeed);
        add(StaticFunctions, PitchCancel);
        add(StaticFunctions, PitchOctave);
        add(StaticFunctions, PitchStep);
        add(StaticFunctions, PitchBend);
        add(StaticFunctions, PitchUp);
        add(StaticFunctions, PitchDown);
        add(StaticFunctions, PitchNext);
        add(StaticFunctions, PitchPrev);
        add(StaticFunctions, Reverse);
        add(StaticFunctions, Forward);
        add(StaticFunctions, Backward);
        add(StaticFunctions, Slip);
        add(StaticFunctions, SlipForward);
        add(StaticFunctions, SlipBackward);
        add(StaticFunctions, MyMove);
        add(StaticFunctions, Drift);
        add(StaticFunctions, DriftCorrect);
        add(StaticFunctions, StartPoint);
        add(StaticFunctions, TrimStart);
        add(StaticFunctions, TrimEnd);
        add(StaticFunctions, Restart);
        add(StaticFunctions, RestartOnce);
        add(StaticFunctions, NextLoop);
        add(StaticFunctions, PrevLoop);
        add(StaticFunctions, LoopN);
        add(StaticFunctions, Loop1);
        add(StaticFunctions, Loop2);
        add(StaticFunctions, Loop3);
        add(StaticFunctions, Loop4);
        add(StaticFunctions, Loop5);
        add(StaticFunctions, Loop6);
        add(StaticFunctions, Loop7);
        add(StaticFunctions, Loop8);
        add(StaticFunctions, NextTrack);
        add(StaticFunctions, PrevTrack);
        add(StaticFunctions, TrackN);
        add(StaticFunctions, Track1);
        add(StaticFunctions, Track2);
        add(StaticFunctions, Track3);
        add(StaticFunctions, Track4);
        add(StaticFunctions, Track5);
        add(StaticFunctions, Track6);
        add(StaticFunctions, Track7);
        add(StaticFunctions, Track8);
        add(StaticFunctions, FocusLock);
        add(StaticFunctions, TrackGroup);
        add(StaticFunctions, TrackCopy);
        add(StaticFunctions, TrackCopyTiming);
        add(StaticFunctions, Checkpoint);
        add(StaticFunctions, SUSRecord);
        add(StaticFunctions, SUSOverdub);
        add(StaticFunctions, SUSMultiply);
        add(StaticFunctions, SUSUnroundedMultiply);
        add(StaticFunctions, SUSInsert);
        add(StaticFunctions, SUSUnroundedInsert);
        add(StaticFunctions, SUSStutter);
        add(StaticFunctions, SUSReplace);
        add(StaticFunctions, SUSSubstitute);
        add(StaticFunctions, SUSMute);
        add(StaticFunctions, SUSNextLoop);
        add(StaticFunctions, SUSPrevLoop);
        add(StaticFunctions, SUSReverse);
        add(StaticFunctions, SUSSpeedToggle);
        add(StaticFunctions, SUSMuteRestart);
        add(StaticFunctions, SampleN);
        add(StaticFunctions, Sample1);
        add(StaticFunctions, Sample2);
        add(StaticFunctions, Sample3);
        add(StaticFunctions, Sample4);
        add(StaticFunctions, Sample5);
        add(StaticFunctions, Sample6);
        add(StaticFunctions, Sample7);
        add(StaticFunctions, Sample8);
        add(StaticFunctions, Realign);
        add(StaticFunctions, MuteRealign);
        add(StaticFunctions, MidiStart);
        add(StaticFunctions, MuteMidiStart);
        add(StaticFunctions, MidiStop);
        add(StaticFunctions, MidiOut);
        add(StaticFunctions, SyncMaster);
        add(StaticFunctions, SyncMasterTrack);
        add(StaticFunctions, SyncMasterMidi);
        add(StaticFunctions, SyncStartPoint);
        add(StaticFunctions, ResumeScript);
        add(StaticFunctions, StartCapture);
        add(StaticFunctions, SaveCapture);
        add(StaticFunctions, StopCapture);
        add(StaticFunctions, SaveLoop);
        add(StaticFunctions, WindowBackward);
        add(StaticFunctions, WindowForward);
        add(StaticFunctions, WindowStartBackward);
        add(StaticFunctions, WindowStartForward);
        add(StaticFunctions, WindowEndBackward);
        add(StaticFunctions, WindowEndForward);
        add(StaticFunctions, WindowMove);
        add(StaticFunctions, WindowResize);

        add(StaticFunctions, DebugStatus);

        add(StaticFunctions, UIRedraw);
        add(StaticFunctions, ReloadScripts);

        // 
        // Special list of hidden debugging function callable from scripts.
        //

        FunctionIndex = 0;
        add(HiddenFunctions, Breakpoint);
        add(HiddenFunctions, Coverage);
        add(HiddenFunctions, Debug);
        add(HiddenFunctions, InitCoverage);
        add(HiddenFunctions, Surface);
    }
}

/**
 * Search for a function on one of the function arrays.
 */
PUBLIC Function* Function::getFunction(Function** functions, const char * name)
{
    Function* found = NULL;

    if (functions != NULL && name != NULL) {
        for (int i = 0 ; functions[i] != NULL ; i++) {
            Function* f = functions[i];
			if (f->isMatch(name)) {
                found = f;
                break;
            }
        }
    }

    return found;
}

/**
 * Return true if there is a logical match of a name with this function.
 */
PUBLIC bool Function::isMatch(const char* xname)
{
	return (StringEqualNoCase(xname, getName()) ||	
			StringEqualNoCase(xname, alias1) ||
			StringEqualNoCase(xname, alias2) ||
			StringEqualNoCase(xname, getDisplayName()));
}

/**
 * Search for one of the static functions.
 */
PUBLIC Function* Function::getStaticFunction(const char * name)
{
    Function* found = getFunction(StaticFunctions, name);

    // for script resolution, we allow access to the hidden functions
    if (found == NULL)
      found = getFunction(HiddenFunctions, name);

    return found;
}

/**
 * Set the display names for each static function from a message catalog.
 * This should be called once during Mobius initialization.
 */
PUBLIC void Function::localizeAll(MessageCatalog* cat)
{
    for (int i = 0 ; StaticFunctions[i] != NULL ; i++)
      StaticFunctions[i]->localize(cat);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
