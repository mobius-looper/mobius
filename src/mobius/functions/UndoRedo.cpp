/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Undo and Redo.
 *
 * EDP has the concept of "short" and "long" undo which we do not support.
 * From what I can tell, Mobius Undo is the same as EDP "long undo".  
 *
 * EDP also has the Undo function behave differently in Mute mode
 * which I don't really like.  UndoOnly was added to avoid this
 * but it would be better to control this with a preset parameter?
 * 
 */

#include <stdio.h>
#include <memory.h>

#include "Util.h"

#include "Action.h"
#include "Event.h"
#include "EventManager.h"
#include "Function.h"
#include "Layer.h"
#include "Loop.h"
#include "Messages.h"
#include "Mode.h"
#include "Synchronizer.h"
#include "Stream.h"
#include "Track.h"

//////////////////////////////////////////////////////////////////////
//
// UndoEvent
//
//////////////////////////////////////////////////////////////////////

class UndoEventType : public EventType {
  public:
	UndoEventType();
};

PUBLIC UndoEventType::UndoEventType()
{
	name = "Undo";
}

PUBLIC EventType* UndoEvent = new UndoEventType();

//////////////////////////////////////////////////////////////////////
//
// RedoEvent
//
//////////////////////////////////////////////////////////////////////

class RedoEventType : public EventType {
  public:
	RedoEventType();
};

PUBLIC RedoEventType::RedoEventType()
{
	name = "Redo";
}

PUBLIC EventType* RedoEvent = new RedoEventType();

/****************************************************************************
 *                                                                          *
 *   								 UNDO                                   *
 *                                                                          *
 ****************************************************************************/

class UndoFunction : public Function {
  public:
	UndoFunction(bool dynamic, bool shortpress, bool only);
	Event* scheduleEvent(Action* action, Loop* l);
 	Event* scheduleSwitchStack(Action* action, Loop* l);
    void invokeLong(Action* action, Loop* l);
    void doEvent(Loop* l, Event* e);
  private:
	bool mDynamic;
	bool mShort;
	bool mOnly;
};

PUBLIC Function* Undo = new UndoFunction(true, false, false);
PUBLIC Function* UndoOnly = new UndoFunction(true, false, true);
PUBLIC Function* ShortUndo = new UndoFunction(false, true, false);
PUBLIC Function* LongUndo = new UndoFunction(false, false, false);

PUBLIC UndoFunction::UndoFunction(bool dynamic, bool shortpress, bool only)
{
	eventType = UndoEvent;
	thresholdEnabled = true;
	mayCancelMute = true;
	mDynamic = dynamic;
	mShort = shortpress;
	mOnly = only;

	// this is considered an instant edit for the purposes of mute cancel
	instant = true;

	// scripts always want to automatically wait for this to complete
	scriptSync = true;

	if (mDynamic) {
		if (mOnly) {
			setName("UndoOnly");
			setKey(MSG_FUNC_UNDO_ONLY);
			// keep this hidden for awhile
			scriptOnly = true;
		}
		else {
			setName("Undo");
			setKey(MSG_FUNC_UNDO);
            mayConfirm = true;
		}
	}
	else if (mShort) {
		setName("ShortUndo");
		setKey(MSG_FUNC_SHORT_UNDO);
		// these don't work anyway so keep them hidden
		scriptOnly = true;
	}
	else {
		setName("LongUndo");
		setKey(MSG_FUNC_LONG_UNDO);
		// these don't work anyway so keep them hidden
		scriptOnly = true;
	}
}

/**
 * Undo during Mute mode acts like a second Mute function but
 * uses the opposite value of MuteMode, e.g. if MuteMode=Start then
 * Undo behaves like MuteMode=Continuous.
 *
 * This is a rather obscure feature need a way to turn it off.
 * Added UndoOnly to avoid this behavior, but less confusing if we had
 * a checkbox in the preset.
 * 
 * Note though that you can be in mute mode as a result of 
 * MuteMidiStart, here the undo first undoes the MidiStartEvent.
 * How should MuteCancel affect this??
 * 
 * If the loop is in Reset, Undo is supposed to send MIDI Start!!
 * 
 */
Event* UndoFunction::scheduleEvent(Action* action, Loop* l)
{
	Event* event = NULL;
    EventManager* em = l->getTrack()->getEventManager();
	MobiusMode* mode = l->getMode();
	Preset* preset = l->getPreset();

	if (mode == ThresholdMode || mode == SynchronizeMode) {
		// cancel the recording, but leave track controls as is
		l->reset(NULL);
	}
	else if (!mOnly && mode == MuteMode && isMuteCancel(preset) && 
			 !em->hasEvents()) {

		// Mute alternate ending, reverses the MuteMode
		// This is an obscure EDP feature, many people probably
		// want an option to disable this?
        // !! Yes, this is hard to understand and doesn't work the
        // way Undo usually does, make it an preset parameter

		event = Mute->scheduleEvent(action, l);
	}
	else if (mode != ResetMode) {
		// restore previous loop, but maintain current position
		// ?? In rehearse mode, we could in theory undo the
		// most recent recorded cycle and return to previously 
		// recorded cycles?
		// ?? what about a non-confirm quantized SwitchMode
		// is this supposed to be a confirmation action or should
		// we undo the switch?  Seems most useful to be able to 
		// cancel the switch, even on a confirm.
 		// NOTE: not doing a play jump though I supposed we could.

		event = em->newEvent(this, UndoEvent, l->getFrame());
		event->savePreset(l->getPreset());
		em->addEvent(event);
        // don't need to keep the Action
    }

	return event;
}

/**
 * The EDP uses undo as the simplest confirmation action when in
 * ConfirmMode.  That isn't enabled by default, if you want that
 * you need to add Undo to the list of ConfirmationFunctions in the
 * global parameters.  If that happened, then Function::invoke
 * will have rerouted this to Confirm::invoke so if we get here
 * we know we can undo the confirmation.
 */
PUBLIC Event* UndoFunction::scheduleSwitchStack(Action* action, Loop* l)
{
	if (action->down) {
        EventManager* em = l->getTrack()->getEventManager();
        // should have checked Confirm mode in invoke(), undo the stack
        if (!em->undoSwitchStack()) {
            // no events to undo, we get to cancel the switch
            em->cancelSwitch();
        }
	}
	return NULL;
}

/**
 * TODO: 
 * Jumps back a complete loop window when set.
 * Jumps back a layer when loop window not set.
 * Exits TempoSelect.
 *
 * If currently in Mute mode, unmutes but the behavior is the
 * opposite of what is in MuteMode.
 * 
 */
PUBLIC void UndoFunction::invokeLong(Action* action, Loop* l)
{
}

/**
 * RedoEvent event handler.
 * The redo list is a fifo, see addRedo for comments.
 */
PUBLIC void UndoFunction::doEvent(Loop* l, Event* e)
{
	l->undoEvent(e);
}

/****************************************************************************
 *                                                                          *
 *   								 REDO                                   *
 *                                                                          *
 ****************************************************************************/

/**
 * For scheduleSwitchStack, we could cancel everything?
 * Probably best to ingore.
 */
class RedoFunction : public Function {
  public:
	RedoFunction();
	Event* scheduleEvent(Action* action, Loop* l);
    void doEvent(Loop* l, Event* e);
  private:
};

PUBLIC Function* Redo = new RedoFunction();

PUBLIC RedoFunction::RedoFunction() :
    Function("Redo", MSG_FUNC_REDO)
{
	eventType = RedoEvent;
	mayCancelMute = true;
    mayConfirm = true;
	instant = true;
}

Event* RedoFunction::scheduleEvent(Action* action, Loop* l)
{
	Event* event = NULL;
    EventManager* em = l->getTrack()->getEventManager();

	MobiusMode* mode = l->getMode();

	if (mode != ResetMode) {
		event = em->newEvent(this, RedoEvent, l->getFrame());
		event->savePreset(l->getPreset());
		em->addEvent(event);
        // any need to save the action?
    }

	return event;
}

/**
 * RedoEvent event handler.
 * The redo list is a fifo, see addRedo for comments.
 *
 * !! What about return transitions, and scheduled events, 
 * do we just let them hang into the next layer?
 */
PUBLIC void RedoFunction::doEvent(Loop* l, Event* e)
{
    EventManager* em = l->getTrack()->getEventManager();

    Layer* play = l->getPlayLayer();
    Layer* redo = l->getRedoLayer();

	if (play == NULL) {
		// must be an initial recording, not sure what redo means
		// here, ignore for now
	}
	else if (redo != NULL) {

		// capture a fade tail now in case the current layer will be pooled
		// actually, this shouldn't happen on a redo but be safe
		l->getOutputStream()->captureTail();

		// splice out the redo layer(s)
        l->setRedoLayer(redo->getRedo());

		// it is important that we clear this, getEventSummary uses
		// it as an indicator that we're in the redo list
		redo->setRedo(NULL);

		// Let's have a redo flush all remaining events, unlike undo
		// which does them one at a time
		em->flushEventsExceptScripts();

        Layer* record = l->getRecordLayer();
		Trace(l, 3, "Loop: Redoing layer %ld, resetting record layer %ld\n",
			  (long)redo->getNumber(), (long)record->getNumber());

		// may be a checkpoint chain, find the end
		Layer* redoTail = redo->getTail();
		redoTail->setPrev(play);
		l->setPlayLayer(redo);
		l->setPrePlayLayer(NULL);

		// recalculate segment fades
		redo->restore(false);

        // if this had been a windowing layer, make sure that's off
        redo->setWindowOffset(-1);

		record->setPrev(redo);
		record->copy(redo);

		long loopFrames = record->getFrames();
		if (loopFrames == 0) {
			// can this happen?
			Trace(l, 1, "Loop: Redo anomoly 32!\n");

			l->setFrame(-(l->getInputStream()->latency));
		}
		else if (l->getFrame() >= loopFrames) {
			// Returning to a loop that may be of a different size,
			// warp the frame to a sensible location
			l->warpFrame();
		}

		l->recalculatePlayFrame();

        // this state is no longer relevant, clear it to avoid trying
        // to fade something that isn't there any more
		l->getInputStream()->resetHistory(l);

		l->checkMuteCancel(e);

		// should redo cancel overdub mode?  undo doesn't
		l->resumePlay();

        // treat this as a resize for out sync
        l->getSynchronizer()->loopResize(l, false);

		Trace(l, 2, "Loop: Redo resuming at frame %ld play frame %ld\n", 
			  l->getFrame(), l->getPlayFrame());
	}
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
