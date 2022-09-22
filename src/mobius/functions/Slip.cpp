/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Jump to another point in the loop.
 * This is similar to Move but move limited in the types of jump.
 * This is a lot like Restart except it doesn't have the LoopSwitch 
 * bagage.
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
#include "Synchronizer.h"
#include "Track.h"

//////////////////////////////////////////////////////////////////////
//
// SlipEvent
//
//////////////////////////////////////////////////////////////////////

class SlipEventType : public EventType {
  public:
	SlipEventType();
};

PUBLIC SlipEventType::SlipEventType()
{
	name = "Slip";
}

PUBLIC EventType* SlipEvent = new SlipEventType();

//////////////////////////////////////////////////////////////////////
//
// SlipFunction
//
//////////////////////////////////////////////////////////////////////

class SlipFunction : public Function {
  public:
	SlipFunction(int direction);
	Event* scheduleEvent(Action* action, Loop* l);
	void prepareJump(Loop* l, Event* e, JumpContext* jump);
	void doEvent(Loop* l, Event* e);

  private:
    bool forward;
};

PUBLIC Function* Slip = new SlipFunction(0);
PUBLIC Function* SlipForward = new SlipFunction(1);
PUBLIC Function* SlipBackward = new SlipFunction(-1);

// TODO: Some possible SUS functions
// Slip forward/backward, then resume where we were OR where
// we would have been before the slip

PUBLIC SlipFunction::SlipFunction(int direction)
{
	eventType = SlipEvent;
	mayCancelMute = true;
	cancelReturn = true;
	quantized = true;

	// considered a trigger function for Mute cancel
	trigger = true;

	if (direction == 0) { 
		setName("Slip");
		externalName = true;
		scriptOnly = true;
	}
	else if (direction > 0) {
		setName("SlipForward");
		setKey(MSG_FUNC_SLIP_FORWARD);
	}
	else {
		setName("SlipBackward");
		setKey(MSG_FUNC_SLIP_BACKWARD);
	}
}

PUBLIC Event* SlipFunction::scheduleEvent(Action* action, Loop* l)
{
	Event* event = NULL;
    EventManager* em = l->getTrack()->getEventManager();

    int slip = 0;
    if (action->arg.getType() == EX_INT)
      slip = action->arg.getInt();
    else if (this == SlipForward)
      slip = 1;
    else if (this == SlipBackward)
      slip = -1;

	Event* prev = em->findEvent(SlipEvent);
	if (prev != NULL) {
		// normally quantized, but could just be comming in fast
		// adjust the slip delta of the existing event, this may cause
		// a change in direction
		// NOTE: if we've already taken the jump, this won't do anything
		// we could undo/redo the jump but it seems like an obscure case
		// and I don't want to bother with it now.

		prev->number += slip;
	}
	else {
		event = Function::scheduleEvent(action, l);
		if (event != NULL) {
			event->number = slip;
			if (!event->reschedule) {
				// defer the calculation of the slip amount to prepareJump
				em->schedulePlayJump(l, event);
			}
		}
	}

	return event;
}

/**
 * Event frame was left at zero, calcuate the correct frame now.
 * Could have done this when the event was scheduled, but seems
 * more robust to defer it?
 */
PUBLIC void SlipFunction::prepareJump(Loop* l, Event* e, JumpContext* jump)
{
    EventManager* em = l->getTrack()->getEventManager();
	Event* parent = e->getParent();

	if (parent == NULL)
	  Trace(l, 1, "Loop: SlipEvent with no parent!\n");
	else {
		long playFrame = l->getPlayFrame();
		long newFrame = playFrame;
		long units = parent->number;
		long loopFrames = l->getFrames();
		long unitFrames = 0;
		long relativeFrames = 0;
		Preset::QuantizeMode absoluteQ = Preset::QUANTIZE_OFF;
		Preset* preset = l->getPreset();

		Preset::SlipMode smode = preset->getSlipMode();
		switch (smode) {
			case Preset::SLIP_SUBCYCLE: {
				absoluteQ = Preset::QUANTIZE_SUBCYCLE;
			}
				break;
			case Preset::SLIP_CYCLE: {
				absoluteQ = Preset::QUANTIZE_CYCLE;
			}
				break;
			case Preset::SLIP_LOOP: {
				absoluteQ = Preset::QUANTIZE_LOOP;
			}
				break;
			case Preset::SLIP_REL_SUBCYCLE: {
				unitFrames = l->getSubCycleFrames();
				relativeFrames = unitFrames * units;
			}
				break;
			case Preset::SLIP_REL_CYCLE: {
				unitFrames = l->getCycleFrames();
				relativeFrames = unitFrames * units;
			}
				break;
			case Preset::SLIP_MSEC: {
				// this is complicated by variable speeds!
				int msecs = preset->getSlipTime();
				float speed = l->getTrack()->getEffectiveSpeed();
				// should we ceil()?
				unitFrames = (long)(MSEC_TO_FRAMES(msecs) * speed);
				relativeFrames = unitFrames * units;
			}
				break;
		}

		if (loopFrames == 0) {
			// probably can't be here, don't go into the calculation weeds
			newFrame = 0;
		}
		else if (absoluteQ == Preset::QUANTIZE_OFF) {
			// a relative move
			newFrame = playFrame + relativeFrames;
		}
		else if (absoluteQ != Preset::QUANTIZE_OFF) {
			newFrame = playFrame;
			if (units > 0) {
				for (int i = 0 ; i < units ; i++)
				  newFrame = em->getQuantizedFrame(l, newFrame, absoluteQ, true);
			}
			else if (units < 0) {
				units = -units;
				for (int i = 0 ; i < units ; i++)
				  newFrame = em->getPrevQuantizedFrame(l, newFrame, absoluteQ, true);
			}
		}

		jump->frame = l->wrapFrame(newFrame);

		Trace(l, 2, "SlipFunction: %ld units %ld frames to %ld",
			  units, unitFrames, jump->frame);

	}
}

PUBLIC void SlipFunction::doEvent(Loop* loop, Event* event)
{
	// Jump play will have done the work, but we now need to resync
	// the record frame with new play frame.  If we had already
	// recorded into this layer, it may require a shift()
	loop->shift(true);

	long newFrame = loop->recalculateFrame(false);

	loop->setFrame(newFrame);
	loop->checkMuteCancel(event);

	// always reset the current mode?
	loop->resumePlay();

	loop->validate(event);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
