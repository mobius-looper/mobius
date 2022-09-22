/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Functions related to loop switching.
 *
 * SwitchDuration
 *
 * SWITCH_PERMANENT
 *   Stay in the next loop and let it play.
 *
 * SWITCH_ONCE
 *   Stay in the next loop, but mute it after it plays to the end.
 *
 * SWITCH_ONCE_RETURN
 *   Play the next loop to the end, then return to the original loop.
 *
 * SWITCH_SUSTAIN
 *   Stay in the next loop, but must when the trigger goes up.
 *
 * SWITCH_SUSTAIN_RETURN
 *   Play the next loop until the trigger goes up, then return
 *   to the original loop.
 *
 * 
 * SwitchDuration controls how the LoopX functions behave.
 *
 * SUSNextLoop and SUSPrevLoop always behave like SWITCH_SUSTAIN_RETURN,
 * they ignore SwitchDuration.
 * 
 * NextLoop and PrevLoop with SwitchDuration=SWITCH_SUSTAIN_RETURN is
 * identical to SUSNextLoop and SUSPrevLoop.
 *
 * 
 */

#include <stdio.h>
#include <memory.h>
#include <string.h>

#include "Action.h"
#include "Util.h"
#include "Event.h"
#include "EventManager.h"
#include "Function.h"
#include "Layer.h"
#include "Loop.h"
#include "Mobius.h"
#include "Track.h"
#include "Stream.h"
#include "Messages.h"
#include "Mode.h"

//////////////////////////////////////////////////////////////////////
//
// SoundCopy, TimeCopy
//
// Events under a SwitchEvent to cause a sound or time copy.
// When using Multiply during the quantize period, we'll use this
// event instead of the usual MultiplyEvent to make the displayed
// message clearer.
// CRAP the message comes from the function not the event type, think...
//
//////////////////////////////////////////////////////////////////////

#if 0
class SoundCopyEventType : public EventType {
  public:
	SoundCopyEventType();
};

PUBLIC SoundCopyEventType::SoundCopyEventType()
{
	name = "SoundCopy";
}
#endif

//////////////////////////////////////////////////////////////////////
//
// SwitchMode, ConfirmMode
//
//////////////////////////////////////////////////////////////////////

// Switch

class SwitchModeType : public MobiusMode {
  public:
	SwitchModeType();
};

SwitchModeType::SwitchModeType() :
    MobiusMode("switch", MSG_MODE_SWITCH)
{
}

MobiusMode* SwitchMode = new SwitchModeType();

// Confirm

class ConfirmModeType : public MobiusMode {
  public:
	ConfirmModeType();
};

ConfirmModeType::ConfirmModeType() :
    MobiusMode("confirm", MSG_MODE_CONFIRM)
{
}

MobiusMode* ConfirmMode = new ConfirmModeType();

//////////////////////////////////////////////////////////////////////
//
// SwitchEvent
//
//////////////////////////////////////////////////////////////////////

class SwitchEventType : public EventType {
  public:
	SwitchEventType();
};

PUBLIC SwitchEventType::SwitchEventType()
{
	name = "Switch";
}

PUBLIC EventType* SwitchEvent = new SwitchEventType();

//////////////////////////////////////////////////////////////////////
//
// ReturnEvent
//
//////////////////////////////////////////////////////////////////////

class ReturnEventType : public EventType {
  public:
	ReturnEventType();
};

PUBLIC ReturnEventType::ReturnEventType()
{
	name = "Return";
}

PUBLIC EventType* ReturnEvent = new ReturnEventType();

//////////////////////////////////////////////////////////////////////
//
// SUSReturnEvent
//
// A funny event used to represent an eventual return transition
// while SUSNestLoop or SUSPrevLoop are being sustained.  This will
// always be pending and never actually executed.  When the up transition
// is detected it is converted to a SwitchEvent.
//
//////////////////////////////////////////////////////////////////////

class SUSReturnEventType : public EventType {
  public:
	SUSReturnEventType();
};

PUBLIC SUSReturnEventType::SUSReturnEventType()
{
	name = "SUSReturn";
	noUndo = true;
}

PUBLIC EventType* SUSReturnEvent = new SUSReturnEventType();

/****************************************************************************
 *                                                                          *
 *   							 LOOP TRIGGER                               *
 *                                                                          *
 ****************************************************************************/

class LoopTriggerFunction : public ReplicatedFunction {
  public:
	LoopTriggerFunction(bool once);
	LoopTriggerFunction(int index, bool sus, bool relative);
	Event* invoke(Action* action, Loop* l);
    Event* scheduleEvent(Action* action, Loop* l);
    bool isMuteCancel(Preset* p);
    void invokeLong(Action* action, Loop* l);
    void doEvent(Loop* l, Event* e);
    void undoEvent(Loop* l, Event* e);
    void confirmEvent(Action* action, Loop* l, Event* e, long frame);

  private:
    Event* scheduleTrigger(Action* action, Loop* current, Loop* next);
    Event* scheduleSwitch(Action* action, Loop* current, Loop* next, 
                          Event* modeEnd);
    Event* promoteSUSReturn(Action* action, Loop* loop, Event* susret);
    Event* addSwitchEvent(Action* action, Loop* current, Loop* next);
    bool isSustainableLoopTrigger(Loop*loop, Function* f);

    bool mRestart;
    bool mOnce;
};

PUBLIC Function* NextLoop = new LoopTriggerFunction(1, false, true);
PUBLIC Function* PrevLoop = new LoopTriggerFunction(-1, false, true);
PUBLIC Function* SUSNextLoop = new LoopTriggerFunction(1, true, true);
PUBLIC Function* SUSPrevLoop = new LoopTriggerFunction(-1, true, true);
PUBLIC Function* Restart = new LoopTriggerFunction(false);
PUBLIC Function* RestartOnce = new LoopTriggerFunction(true);

// TODO: need a way to define these on the fly

PUBLIC Function* LoopN = new LoopTriggerFunction(-1, false, false);
PUBLIC Function* Loop1 = new LoopTriggerFunction(0, false, false);
PUBLIC Function* Loop2 = new LoopTriggerFunction(1, false, false);
PUBLIC Function* Loop3 = new LoopTriggerFunction(2, false, false);
PUBLIC Function* Loop4 = new LoopTriggerFunction(3, false, false);
PUBLIC Function* Loop5 = new LoopTriggerFunction(4, false, false);
PUBLIC Function* Loop6 = new LoopTriggerFunction(5, false, false);
PUBLIC Function* Loop7 = new LoopTriggerFunction(6, false, false);
PUBLIC Function* Loop8 = new LoopTriggerFunction(7, false, false);

/**
 * Constructor for absolute and relative triggers.
 */
PUBLIC LoopTriggerFunction::LoopTriggerFunction(int i, bool sus, bool relative)
{
	eventType = SwitchEvent;
	trigger = true;
	mayCancelMute = true;
	index = i;
	replicated = !relative;
    mRestart = false;
    mOnce = false;
    sustain = sus;

    // These look messy in the SustainFunctions parameter list
    // and we really should do all or nothing, until we can find a way
    // to SUS overdide the entire family, leave them out
    //maySustain = !sus;

    // Have to set this so that Return events scheduled in reset
    // loops get processed.  See logic in Loop::processEvent
    resetEnabled = true;

	if (relative) {
		if (index > 0) {
			if (sus) {
				setName("SUSNextLoop");
				setKey(MSG_FUNC_SUS_NEXT);
				setHelp("Note On = Next Loop, Note Off = Previous Loop");
			}
			else {
				setName("NextLoop");
				setKey(MSG_FUNC_NEXT);
			}
		}
		else {
			if (sus) {
				setName("SUSPrevLoop");
				setKey(MSG_FUNC_SUS_PREV);
				setHelp("Note On = Previous Loop, Note Off = Next Loop");
			}
			else {
				setName("PrevLoop");
				setKey(MSG_FUNC_PREV);
			}
		}
	}
	else if (index < 0) {
		setName("Loop");
		setKey(MSG_FUNC_TRIGGER);
		scriptOnly = true;
        maySustain = false;
	}
	else {
		sprintf(fullName, "Loop%d", i + 1);
		setName(fullName);
		setKey(MSG_FUNC_TRIGGER);

        // an older longer name, keep for backward compatibility
		sprintf(fullAlias1, "LoopTrigger%d", i + 1);
		alias1 = fullAlias1;
	}
}

/**
 * Constructor for restarts.
 */
PUBLIC LoopTriggerFunction::LoopTriggerFunction(bool once)
{
	eventType = SwitchEvent;
	trigger = true;
	mayCancelMute = true;
    mRestart = true;
    mOnce = once;

    // Have to set this so that Return events scheduled in reset
    // loops get processed.  See logic in Loop::processEvent
    resetEnabled = true;

	if (mOnce) {
		setName("RestartOnce");
		setKey(MSG_FUNC_RESTART_ONCE);
		setHelp("Restart loop and play once");

        // this is what the EDP calls it and what we used to call it
        // prior to 1.43
        alias1 = "SamplePlay";
	}
	else {
		setName("Restart");
		setKey(MSG_FUNC_RESTART);
		setHelp("Restart loop and play forever");
        mayConfirm = true;
        alias1 = "Retrigger";
	}
}

/**
 * This is more complicated than most so we override the entire
 * invoke method.  Determine which loop to trigger, then call Loop
 * to setup the transition.
 *
 * There is still some lingering dependencies on logic in Loop that
 * can't be brought over here easily....keep working at it.
 */
Event* LoopTriggerFunction::invoke(Action* action, Loop* l)
{
	Event* event = NULL;
    EventManager* em = l->getTrack()->getEventManager();
    Function* function = action->getFunction();

    trace(action, l);

    Preset* p = l->getPreset();
    Preset::SwitchDuration duration = p->getSwitchDuration();
    Event* susret = em->findEvent(SUSReturnEvent);

    if (!action->down) {
        // up transitions are only interesting for SUSNext, SUSPrev
        // switchDuration=sustain and switchDuration=sustainReturn

        if (sustain ||
            (!mRestart && 
             (duration == Preset::SWITCH_SUSTAIN ||
              duration == Preset::SWITCH_SUSTAIN_RETURN))) {
            
            // sustainable
            // !! KLUDGE we're using a SUSReturn for SWITCH_SUSTAIN
            // even though it will mute rather than return, should
            // have a special event for these

            if (susret != NULL) {
                // we made it to the other side, promote it
                if (susret->function != function) {
                    // We either missed an up transition or another loop
                    // switch function was pressed and released while the
                    // original one is still pressed.  The later is more
                    // likely, especially when using keyboard triggers.  Ignore
                    // this one and wait for the one that started it.
                    // !! Ideally we should be keeping track of the trigger id
                    // rather than the function since the same function can
                    // be assigned to different triggers.
                    Trace(l, 2, "LoopTriggerFunction: Overlapping %s and %s functions\n",
                          susret->function->getName(), function->getName());
                }
                else {
                    // takes ownership of the Action
                    event = promoteSUSReturn(action, l, susret);
                }
            }
            else {
                Event* switche = em->getSwitchEvent();
                if (switche != NULL) {
                    // must be quantized, record the fact that we had an 
                    // up transition in the event so the switchEvent handler
                    // will schedule the appropriate return event
                    // formerly did this by setting Event.down, but I like
                    // making this clearer
                    // !! what I don't like about this is that you
                    // won't see a Return event under the SUS event,
                    // should be schedulign a normal Return event or
                    // at least a placeholder event so we can see something
                    // !! we almost certainly have issues if you're 
                    // sustaining overlapping triggers, should be testing
                    // for the invoking trigger id
                    switche->fields.loopSwitch.upTransition = true;
                }
                else if (l->getMode() != ResetMode) {
                    // Need to handle EmptyLoopAction=Record 
                    // RecordFollow and convert this into a SUSRecord
                    // with a Return.   
                    // !! What does the above comment even mean?  
                    // This can happen when SwitchDuration=Sustain
                    // and you have overlapping prev/next triggers which
                    // is easy to do with keyboard bindings.  When the last
                    // up transition comes in, the loop is already muted and
                    // there is no more switch event, we should just ignore
                    // the orphaned up event.
                    Trace(l, 2, "LoopTriggerFunction: Orphaned up transition for %s encountered\n",
                          function->getName());
                }
            }
        }
        else {
            // else not sustainable, ignore
            if (susret != NULL) {
                // shouldn't have one of these missed a transition?
                Trace(l, 1, "LoopTriggerFunction: Unexpected SUSReturn!\n");
                em->freeEvent(susret);
            }
        }
    }
    else {
        if (susret != NULL) {
            // We either missed an up transition or another
            // loop switch function was executed while the SUS trigger
            // for the last switch is still being held which is more likely, 
            // especially when using keyboard bindings.
            // This cancels the return.  It was pending so we don't
            // have to worry about a JumpPlayEvent.
            Trace(l, 2, "Loop: Loop switch during SUSReturn wait\n");
            em->freeEvent(susret);
        }

        Preset* p = l->getPreset();
        int maxLoops = p->getLoops();
        int curIndex = l->getNumber() - 1;
        int nextIndex = index;

        if (mRestart) {
            nextIndex = curIndex;
        }
		else if (replicated) {
			if (index < 0) {
				// this one is expected to have an argument
                nextIndex = action->arg.getInt();
                // since these are visible in scripts and binding 
                // args expect 1 based indexing
                nextIndex--;
                if (nextIndex < 0) nextIndex = 0;
			}
		}
		else {
            // start from the current loop, 
            // if maxLoops == 1 it's just a restart
            nextIndex = curIndex;
            if (maxLoops > 1) {
                // if we've already setup a switch, increment from there
				// if there is a return transition, cancel it??

				Event* e = em->findEvent(ReturnEvent);
				if (e != NULL) {
                    // remove from list, clean up side effects, and free
					em->freeEvent(e);
				}

				e = em->findEvent(SwitchEvent);
				if (e != NULL) {
					Loop* l = e->fields.loopSwitch.nextLoop;
					if (l != NULL)
                      nextIndex = l->getNumber() - 1;
				}

                // if we're sussing, reverse the direction on the up transition
				int direction = index;
				if (sustain || 
                    (duration == Preset::SWITCH_SUSTAIN ||
                     duration == Preset::SWITCH_SUSTAIN_RETURN))
				  direction = (action->down) ? index : -index;

                if (direction > 0) {
                    nextIndex++;
                    if (nextIndex >= maxLoops)
                      nextIndex = 0;
                }
                else {
                    nextIndex--;
                    if (nextIndex < 0)
                      nextIndex = maxLoops - 1;
                }
				
				// Q: if we cycle back around assume this is always
				// a restart?
            }
        }

		if (nextIndex >= 0) {
			Track* t = l->getTrack();
			Loop* next = t->getLoop(nextIndex);
            if (next != NULL) {
                event = scheduleTrigger(action,  l, next);
			
                // If SwitchVelocity is enabled and this was a MIDI trigger,
                // adjust the output volume.  
                // !! Do this only if the loop has content?
                // !! Technically we should defer this to the eavluation
                // of the event which might be undone, but that would
                // require that we include the full Action in the Event
                // !! we have that now, figure it out
                if (replicated &&
                    action->trigger == TriggerMidi &&
                    p->isSwitchVelocity()) {
                    t->setOutputLevel(action->triggerValue);
                }
            }
        }
    }

    // promoteSUSReturn or scheduleTrigger should have done this already
    if (event != NULL) {
        if (action->getEvent() == NULL)
          action->setEvent(event);

        else if (action->getEvent() != event)
          Trace(l, 1, "LoopSwitch::invoke unexpected action/event binding!\n");
    }

	return event;
}

/**
 * Return true if this is a sustainable loop trigger function.
 *
 * UPDATE: This isn't used any more, why?
 */
PRIVATE bool LoopTriggerFunction::isSustainableLoopTrigger(Loop*loop,
                                                           Function* f)
{
    // this gets SUSNextLoop and SUSPrevLoop
    bool sustainable = sustain;

    if (!sustain && !mRestart) {
        // all the others can be the parameter says so
        // !! to support long press we should always let these
        // be sustainable, then let invokeLong check the preset
        Preset* preset = loop->getPreset();
        Preset::SwitchDuration duration = preset->getSwitchDuration();
        sustainable = (duration == Preset::SWITCH_SUSTAIN ||
                       duration == Preset::SWITCH_SUSTAIN_RETURN);
    }
    return sustainable;
}

/**
 * We're unusual in that we overload the isMuteCancel method and go
 * beyond just the MuteCancel mode in the preset.  We're a trigger
 * fucntion so if muteCancel=trigger or above then this will cancel
 * mute.
 * 
 * The special cases are for RestartOnce, switchDuration=Once 
 * or switchDuration=Sustain.  In those cases we schedule a Mute to
 * shut them off after they play once or the sustain ends.  Since we'll 
 * always be in mute mode when that happens we don't want this to
 * "follow" every time we trigger another loop.   These duration modes are
 * therefore implicitly mute cancel modes.
 *
 * This is different than releases prior to 1.43 where "SamplePlay" would
 * obey MuteCancelFunctions.  If we don't do this then you can never
 * play a loop like a sampler, it plays once, goes into mute, then triggering
 * it again won't cancel the mute.  
 */
PUBLIC bool LoopTriggerFunction::isMuteCancel(Preset* p)
{
    bool cancel = Function::isMuteCancel(p);
    if (!cancel) {
        Preset::SwitchDuration duration = p->getSwitchDuration();
        if (this == RestartOnce ||
            (this != Restart &&
             (duration == Preset::SWITCH_ONCE ||
              duration == Preset::SWITCH_SUSTAIN))) {
            cancel = true;
        }
    }
    return cancel;
}

/**
 * Called by invoke() to setup a transition to another loop.
 * We are NOT quantized yet.
 *
 * If we're in Reset, we can do an immediate transition, but to make
 * the machinery work consistently, we still have to schedule events
 * and "play" a little of this loop.  RunMode was added for this
 * purpose so we don't confuse things that look at PlayMode, and we
 * know to put ourselves back in ResetMode when we eventually process
 * the SwitchEvent.  An alternative would be to jump immediately 
 * to the next loop setting mFrame to -InputLatency but this will
 * complicate the other logic which doesn't expect to run until IL
 * has passed.
 *
 * Q: It is unclear whether Restart and RestartOnce should be subject
 * to SwitchQuant.
 * 
 * If an event is returned it represents the primary switch event
 * and it will ownt he Action.
 */
PRIVATE Event* LoopTriggerFunction::scheduleTrigger(Action* action, 
                                                    Loop* current,
                                                    Loop* next)
{
	Event* event = NULL;
    EventManager* em = current->getTrack()->getEventManager();
    Event* switche = em->getSwitchEvent();
    MobiusMode* mode = current->getMode();

	if (mode == ResetMode) {
		// ignore if we're restarting a reset loop
		if (next != current) {

            // schedule it, this also takes ownership of the Action
			event = addSwitchEvent(action, current, next);

            // and immediately confirm it
            // ?? do the "confirm" modes apply here
            // event->confirm(action, current, frame) would have
            // the same effect but we can just call it directly
            confirmEvent(action, current, event, CONFIRM_FRAME_QUANTIZED);
            
            if (action->getEvent() != event) {
                // a roudning mode decided not to use this event, 
                // this should not happen, it was already traced
                event = NULL;
            }
            else {
                // play InputLatency frames
                current->setMode(RunMode);
            }
		}
	}
	else if (mode == PlayMode || switche != NULL) {

		// for switch and confirm, this just changes the existing transition
		event = scheduleSwitch(action, current, next, NULL);
	}
	else if (mode == MuteMode) {
		// ?? manual is unclear, just let it happen and check
		// MuteCancel later
		event = scheduleSwitch(action, current, next, NULL);
	}
	else if (mode == RecordMode) {

        if (next == current) {
            // not switching but end Record mode
            Event* recordEnd = Record->scheduleModeStop(action, current);

			// !! this is supposed to behave the same as ending with Record
			// !! may have issues with SUSNextLoop?
        }
        else {
            // secondary action, have to clone the Action
            Mobius* m = current->getMobius();
            Action* stopAction = m->cloneAction(action);

            Event* recordEnd = Record->scheduleModeStop(stopAction, current);

            m->completeAction(stopAction);

            // schedule the primary switch event
            event = scheduleSwitch(action, current, next, recordEnd);

            // KLUDGE: In order to support RecordTransfer=Follow
            // we have to put something in the switch event that tells
            // it that we used to be in record mode, because by the time
            // sthe SwitchEvent is executed, the RecordStopEvent will
            // be done and we'll no longer be in Record mode so we can't
            // just look at mMode. This feels wrong, but I don't see a better
            // alternative without adding more event dependencies or making
            // SwitchEvent effectively be the RecordEndEvent, but that would
            // screw up many things
            event->fields.loopSwitch.recordCanceled = true;
        }
	}
	else if (mode == OverdubMode) {
		// ?? manual is unclear though it seems reasonable to 
		// do the transition and stay in overdub?
		event = scheduleSwitch(action, current, next, NULL);
	}
	else if (mode == RehearseMode) {
        // ?? should we cancel rehearse mode now or wait for switchEvent
		event = scheduleSwitch(action, current, next, NULL);
	}
	else if (mode == ReplaceMode || mode == SubstituteMode) {
		// ?? manual is unclear what happens here, wait for switchEvent
        // or stop it now?  
        InputStream* input = current->getInputStream();
		int latency = (action->noLatency) ? 0 : input->latency;
        long frame = current->getFrame();
        Event* e = em->newEvent(Record, RecordStopEvent, frame + latency);
		e->savePreset(current->getPreset());
        em->addEvent(e);
		event = scheduleSwitch(action, current, next, NULL);
	}
	else if (mode->rounding) {
		// Insert/Multiply
		// The only documented case I could find is the up transition
		// of SUSReturn, typically after LoopCopy=Timing caused by
		// SUSNextLoop.  
		// Per says that the EDP allows NextLoop as a mode end event,
		// which sounds reasonable, but it is unclear how
		// SwitchQuant plays into this
        // TODO: Bring mode ending logic up here so we can handle
        // like Record
		event = scheduleSwitch(action, current, next, NULL);
	}
	else {
		Trace(current, 1, "Loop: In mode %s ignoring switch to %ld\n",
			  mode->getName(), (long)next->getNumber());
	}

	return event;
}

/**
 * Called by scheduleTrigger to setup a PlayJumpEvent and SwitchEvent
 * for the next loop.  If we're in a confirmation mode, the SwitchEvent
 * will be pending.
 *
 * TODO: if next == this and MoreLoops=1 this is supposed to restart.
 * If next == this && MoreLooops > 1 and there is a SwitchEvent
 * then we must be in SamplerStyle=Once (jsl - now called SwitchDuratin=Once)
 * and cycled back to the first loop.
 *
 * Page 4-46 says we're supposed to "stop the first time it returns to
 * that loop and ignore the previous steps in the sequence."  
 * Not sure I understand that we'll just ignore it.
 *
 */
PRIVATE Event* LoopTriggerFunction::scheduleSwitch(Action* action, 
                                                   Loop* current,
                                                   Loop* next, 
                                                   Event* modeEnd)
{
	Event* event = NULL;
    EventManager* em = current->getTrack()->getEventManager();

    Event* switche = em->getSwitchEvent();
	if (switche != NULL) {
        // already have a switch event, adjust it

        // shouldn't be here with a mode end
        if (modeEnd != NULL)
          Trace(current, 1, "LoopSwitch: adjusting previous switch with a mode ending event!\n");

        // modifying an existing switch
		switche->fields.loopSwitch.nextLoop = next;

        // If this is a replicated function the name of the function
        // has the loop number, otherwise we have to set the "number"
        // field of the event to convey the number.  This is displayed
        // by the loop meter.  Because we can use both replicated LoopX
        // functions as well as PrevLoop NextLoop during the switch
        // quantize period, need to update the function too.

        switche->function = this;

        // instead of this could release the previous Action and take this one?
        if (!replicated)
          switche->number = next->getNumber();
        else
          switche->number = 0;

        // if the function is SUSNext/PrevLoop, then this flag will
        // have been set so that switchEvent knows to schedule the
        // Return event.  If we trigger again to change the number
        // need to reset that flag.  If we ever switch to scheduling
        // a real Return event rather than this goofy flag will need
        // to find that and delete it
        switche->fields.loopSwitch.upTransition = false;

		// If we were near the boundary, we may have already 
		// begun playing the wrong next loop.  Could stop it and 
		// point it at the new loop, but this is a very small window,
		// wait for the switchEvent and fix it there.

		Trace(current, 2, "Loop: Changing next loop: loop=%ld startFrame=%ld\n", 
			  (long)next->getNumber(), next->getPlayFrame());
        
        // Replace the previous action so the script can wait on this one.
        Action* prevAction = switche->getAction();
        if (prevAction != NULL) {
            prevAction->detachEvent(switche);
            current->getMobius()->completeAction(prevAction);
        }
        action->setEvent(switche);
		event = switche;
	}
	else {
		// scheduling a new switch
        Preset* preset = current->getPreset();
		Preset::SwitchQuantize q = preset->getSwitchQuantize();
		bool needsConfirm = (q == Preset::SWITCH_QUANT_CONFIRM ||
                             q == Preset::SWITCH_QUANT_CONFIRM_CYCLE ||
                             q == Preset::SWITCH_QUANT_CONFIRM_SUBCYCLE ||
                             q == Preset::SWITCH_QUANT_CONFIRM_LOOP);

        // this also takes ownership of the Action
		event = addSwitchEvent(action, current, next);

        // currently here only when ending a Record
        if (modeEnd != NULL) {
            // add as a child event so we can track later movements
            // of the parent event
            modeEnd->addChild(event);
            // if the ending is pending (such as a sync pulse) then
            // we have to hold the switch in confirmation mode
            if (modeEnd->pending)
              needsConfirm = true;
        }

		if (!needsConfirm) {
            if (modeEnd == NULL) {
                // go through switch quantization
                confirmEvent(action, current, event, CONFIRM_FRAME_QUANTIZED);

                if (action->getEvent() != event) {
                    // a rounding mode decided not to use this event
                    // this should not happen and has already been traced
                    event = NULL;
                }
            }
            else {
                // enable the event immediately after the mode end
                // NOTE: this is fine for Record but if we ever use
                // this mechanism for a rounding mode like Multiply/Insert 
                // switch quantize is still relevant, but it needs to be 
                // calculated from the end of the mode, not the 
                // current loop position.
                
                event->frame = modeEnd->frame;
                event->pending = false;

                // In theory we could set up a preplay, but
                // I'm afraid this will confuse the play jump for the
                // RecordStopEvent.  This needs to be another place
                // where a stacking event (Record) has one play jump
                // with a complex analysis of the stacked events.
                Event* jump = em->schedulePlayJump(current, event);
                if (jump->latencyLoss > 0)
                  Trace(current, 2, "Loop: Switch latency loss %ld\n", (long)jump->latencyLoss);
            }
        }
	}

	return event;
}

/**
 * Helper to build and schedule a pending SwitchEvent.
 * This         
 * 
 * We used to also stack events for the various transfer modes
 * (restoring previous direction, speed, etc.) on the switch event
 * so you could see them in the UI, and in theory undo them.  
 * Transfer mode events are now generated by switchEvent when the
 * switch happens.  This makes them invisible and you can't undo them.
 * Not sure which I like better but it certainly results in less
 * event clutter to defer them to switchEvent.
 * 
 * Note that when we did stack them early deciding whether to dipslay
 * them is tricky.  The current modes may be the same as what they will
 * be in the next loop so showing the events (Forward, Speed 0, etc.)
 * is redundant and confusing.  We can't make a visibility decision
 * now though because the current modes are not necessarily going to stay
 * the same until the switch happens.  
 *
 * getState (actually getEventSummary) has logic to filter out
 * "meaningless" events so we can schedule them early but not clutter
 * the UI.
 */
PRIVATE Event* LoopTriggerFunction::addSwitchEvent(Action* action, 
                                                   Loop* current, 
                                                   Loop* next)
{
    EventManager* em = current->getTrack()->getEventManager();
    Event* switche = em->getSwitchEvent();

	if (switche != NULL)
	  Trace(current, 1, "Loop: Overlapping switch events!\n");
    
	switche = em->newEvent(action->getFunction(), SwitchEvent, 0);

	switche->savePreset(current->getPreset());

	switche->pending = true;
	switche->quantized = true;	// so it can be undone
	switche->fields.loopSwitch.nextLoop = next;
    
    // save the number only if this is a relative switch function,
    // if it is replicated then the number is already in the name
    // this number is shown in the event summary which looks redundant
    // e.g. Loop 2 2
    // !! don't need this if we take the action
    if (!replicated)
      switche->number = next->getNumber();
      
	// this is vital in order to process synchronization events
	// at the loop boundary when SwitchQuant=Loop
	switche->afterLoop = true;

	em->addEvent(switche);
    em->setSwitchEvent(switche);

    // this takes ownership of the action since it is now scheduled
    action->setEvent(switche);

	return switche;
}

/**
 * Process the up transition of a SUSNextLoop, SUSPrevLoop,
 * or Loop trigger with SwitchDuration=SustainReturn
 * Promote the placeholder event to a normal return.
 *
 * KLUDGE: We also schedule one of these to handle SwitchDuration=Sustain
 * which when the trigger goes up will mute rather than return.
 * 
 */
PRIVATE Event* LoopTriggerFunction::promoteSUSReturn(Action* action,
                                                     Loop* loop, 
                                                     Event* susret)
{
	Event* event = NULL;
    EventManager* em = loop->getTrack()->getEventManager();

    Function* func = action->getFunction();
    Preset* p = loop->getPreset();
    Preset::SwitchDuration duration = p->getSwitchDuration();

    if (func->sustain || duration == Preset::SWITCH_SUSTAIN_RETURN) {

        // we're returning
        // TODO: rather than return to the original loop, I think
        // the EDP just decrements from wherever we are now
        
        Loop* prev = susret->fields.loopSwitch.nextLoop;

        // This one is unusual becase we schedule the end event
        // BEFORE we schedule the MultiplyEnd and then move it later.
        // !! compare this with Function::scheduleEventDefault, we need
        // to be encapsulating and sharing this...

        event = em->scheduleReturnEvent(loop, susret, prev, true);

        MobiusMode* mode = loop->getMode();
        if (mode->rounding) {
            // this will take the Action
            Event* modeEnd = loop->scheduleRoundingModeEnd(action, event);
            if (modeEnd != NULL && modeEnd->getParent() == NULL) {
                // Mode ending decided to eat the trigger event,
                // this shouldn't happen here.
                Trace(loop, 1, "promoteSUSReturn: lost return event ending rounding mode!\n");
                // Action will now be owned by modeEnd
                event = NULL;
            }
        }
    }
    else {
        // We're muting immediately, obey quantization?
        // Ignore if we're in reset
        // !! what if we're in a rounding mode?
        MobiusMode* mode = loop->getMode();
        if (mode->rounding) {
            // we could handle this...
            Trace(loop, 1, "promoteSUSReturn: ignoring during rounding mode!\n");        
        }
        else if (loop->getMode() != ResetMode) {
            event = em->newEvent(MuteOn, loop->getFrame());
            event->savePreset(loop->getPreset());
            em->addEvent(event);
            em->schedulePlayJump(loop, event);
        }
    }

    // this is never needed
    em->freeEvent(susret);

    // attach the action
    if (event != NULL)
      action->setEvent(event);

    // caller will attach it to the Action
	return event;
}

/**
 * TODO: EDP resets the triggered loop (unless SwitchDuration=Sustain).
 * Is this true for NextLoop?  Could convert to SUSNextLoop
 */
PUBLIC void LoopTriggerFunction::invokeLong(Action* action, Loop* l)
{
}

/****************************************************************************
 *                                                                          *
 *                                   EVENTS                                 *
 *                                                                          *
 ****************************************************************************/

/**
 * Unfortunately switch event handling is still closely wound up in Loop
 * so we continue to implement it there.
 */
PUBLIC void LoopTriggerFunction::doEvent(Loop* l, Event* e)
{
    if (e->type == SwitchEvent)
      l->switchEvent(e);

    else if (e->type == ReturnEvent)
      l->returnEvent(e);

    else if (e->type == SUSReturnEvent) {
        // always pending, replaced with a real ReturnEvent
    }

}

PUBLIC void LoopTriggerFunction::undoEvent(Loop* l, Event* e)
{
    EventManager* em = l->getTrack()->getEventManager();

    if (e->type == SwitchEvent)
      em->switchEventUndo(e);

    else if (e->type == ReturnEvent)
      em->returnEventUndo(e);
}

/**
 * Called indirectly by Event::confirm when we find a pending
 * event we want to confirm.  Action may be null if we're
 * confirming from another pending event such as RecordStopEvent.
 *
 * There are two ways we can get here.  First if you have
 * switch confirmation on, the switch event will be scheduled pending
 * and you have to confirm it by invoking the Confirm function
 * (or an alias like Undo if configured).  When this happens
 * we then schedule the switch quantized *from the point of confirmation*.
 *
 * The second way we can get here is if the switch were pending
 * waiting for a synchronized recording to complete.  You would get into this
 * state if you started a sync recording, then did a loop switch to
 * end the recording.  If we have to wait for a sync pulse, the
 * switch is marked pending and we confirm it in the RecordStopEvent handler.
 * In this case we do NOT want to quantize the switch, it should happen
 * immediately.
 * 
 * Event::confirm has these comments:
 *
 * Confirm the event on the given frame.  If frame is EVENT_FRAME_IMMEDIATE (-1)
 * the event is expected to be scheduled immediately in the target loop.
 * If the event frame is EVENT_FRAME_CALCULATED (-2) the event handler
 * is allowed to calculate the frame, though usually this will behave
 * the same as IMMEDIATE.  If the frame is positive the event is activated
 * for that frame.
 *
 * This works for our purposes the Confirm function needs to use -2
 * and RecordStopEvent needs to use -1.  Think more about this though.
 * If only Confirm needs to calculate quantization then we could just move
 * it up there and require that a frame be passed here?
 * 
 * If we're in Insert or Multiply mode, it is unclear how SwitchQuant
 * is to behave.  Options:
 *
 * - ignore it, switch immediately after the mode end event
 * - schedule mode end, then reschedule the switch afterward
 * - schedule normally, then schedule mode end, and move switch
 *   event after the mode end if it preceeds it.
 *
 * The third way seems most natural to me, so if SwitchQuant is Loop or
 * Cycle, we'll end the multiply and wait for the expected point.
 *
 */
PUBLIC void LoopTriggerFunction::confirmEvent(Action* action,
                                              class Loop* l, 
                                              Event* switche, 
                                              long frame)
{
    EventManager* em = l->getTrack()->getEventManager();

    if (switche == NULL || !switche->pending) {
        // should this be an error?
        Trace(l, 2, "Confirm: no pending switch event\n");
    }
    else {
        long switchFrame = frame;
        bool quantized = false;

        if (switchFrame == CONFIRM_FRAME_IMMEDIATE) {
            switchFrame = l->getFrame();
            Trace(l, 2, "Confirm: Switch at current frame %ld\n", 
                  switchFrame);
        }
        else if (switchFrame == CONFIRM_FRAME_QUANTIZED) {

            Preset* p = l->getPreset();
            Preset::SwitchQuantize q = p->getSwitchQuantize();
            int latency = 0;
            if (!action->noLatency) {
                InputStream* is = l->getInputStream();
                latency = is->latency;
            }
            long loopFrame = l->getFrame();
            long realFrame = loopFrame + latency;
            switchFrame = realFrame;

            switch (q) {
                case Preset::SWITCH_QUANT_CYCLE:
                case Preset::SWITCH_QUANT_CONFIRM_CYCLE: {
                    switchFrame = 
                        em->getQuantizedFrame(l, realFrame, Preset::QUANTIZE_CYCLE, false);
                }
                break;

                case Preset::SWITCH_QUANT_SUBCYCLE:
                case Preset::SWITCH_QUANT_CONFIRM_SUBCYCLE: {
                    switchFrame = 
                        em->getQuantizedFrame(l, realFrame, Preset::QUANTIZE_SUBCYCLE, false);
                }
                break;

                case Preset::SWITCH_QUANT_LOOP:
                case Preset::SWITCH_QUANT_CONFIRM_LOOP: {
                    switchFrame = 
                        em->getQuantizedFrame(l, realFrame, Preset::QUANTIZE_LOOP, false);
				}
                break;

				case Preset::SWITCH_QUANT_OFF:
				case Preset::SWITCH_QUANT_CONFIRM:
					break;
            }

            if (switchFrame == realFrame)
              Trace(l, 2, "Confirm: Switch at %ld\n", switchFrame);
            else {
                quantized = true;
                Trace(l, 2, "Confirm: Switch at %ld quantized from %ld\n", 
                      switchFrame, realFrame);
            }
        }
        else {
            // !! an absolute frame wrap it
            Trace(l, 2, "Confirm: Switch at %ld\n", switchFrame);
        }

		// sanity check on the frame pointers before we continue
		// UPDATE: Now that we let Switch be an ending for Insert/Multiply,
		// can't adjust record/play frame positions if we're in an 
		// "extending" mode
        MobiusMode* mode = l->getMode();
		if (!mode->extends)
		  l->validate(NULL);

		// activate the switch event
		switche->frame = switchFrame;
		switche->pending = false;
		switche->quantized = quantized;

		// setup a jump event for early playback
		Event* jump = em->schedulePlayJump(l, switche);
		if (jump->latencyLoss > 0)
		  Trace(l, 2, "Loop: Switch latency loss %ld\n", (long)jump->latencyLoss);

		// now if we're in one of the rouding modes, end the mode
		// which may also move our switch event if it preceeds the
		// mode end
        // !!!!! this is bass ackwards make this use pending events
        // similar logic in Function::scheduleEventDefault and 
        // promoteSUSReturn above...messy
		if (mode->rounding) {

			Event* modeEnd = l->scheduleRoundingModeEnd(action, switche);
				
            if (modeEnd != NULL && modeEnd->getParent() == NULL) {
                // Mode ending decided to eat the trigger event,
                // this should not happen here.  
                Trace(l, 1, "Switch Confirmation: ending rounding mode lost switch event!\n");
                // Action will now be owned by modeEnd
                // switche will have been unscheduled and freed
                // caller needs to check this, if Action.event != switche
                // then don't rely on it
            }
		}

		// do not set SwitchMode, that's a virtual mode we only
		// report in MobiusState, having a non-null mSwitch
		// causes the redirection of events
	}
}

/**
 * We should never get here since all scheduling is done in the invoke()
 * method.
 */
Event* LoopTriggerFunction::scheduleEvent(Action* action, Loop* l)
{
    Trace(l, 1, "LoopTriggerFunction::scheduleEvent should not be here!\n");
    return NULL;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
