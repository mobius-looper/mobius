/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Instant move to a location within the loop.
 *
 */

#include <stdio.h>
#include <memory.h>

#include "Util.h"

#include "Action.h"
#include "Event.h"
#include "EventManager.h"
#include "Expr.h"
#include "Function.h"
#include "Layer.h"
#include "Loop.h"
#include "Messages.h"
#include "Synchronizer.h"
#include "Track.h"

//////////////////////////////////////////////////////////////////////
//
// MoveEvent
//
//////////////////////////////////////////////////////////////////////

class MoveEventType : public EventType {
  public:
	MoveEventType();
};

PUBLIC MoveEventType::MoveEventType()
{
	name = "Move";
}

PUBLIC EventType* MoveEvent = new MoveEventType();

//////////////////////////////////////////////////////////////////////
//
// MoveFunction
//
//////////////////////////////////////////////////////////////////////

/**
 * Move to an arbitrary location.
 * This is useful only in scripts since the location has to be
 * specified as an argument.  We also don't have to mess with quantization.
 */
class MoveFunction : public Function {
  public:
	MoveFunction(bool drift);
	Event* scheduleEvent(Action* action, Loop* l);
	void doEvent(Loop* loop, Event* event);
	void undoEvent(Loop* loop, Event* event);
	void prepareJump(Loop* l, Event* e, JumpContext* jump);
};

// NOTE: Originally used Move but that conflicts with something
// in the QD.framework on OSX
PUBLIC Function* MyMove = new MoveFunction(false);
PUBLIC Function* Drift = new MoveFunction(true);

PUBLIC MoveFunction::MoveFunction(bool drift)
{
	eventType = MoveEvent;
	quantized = false;

    // allow the argument to be a mathematical expression
    expressionArgs = true;

	if (drift) {
		setName("Drift");
		setKey(MSG_FUNC_DRIFT);
		scriptOnly = true;
	}
	else {
		setName("Move");
		setKey(MSG_FUNC_MOVE);

        // until we support binding arguments it doesn't make sense
        // to expose this
        scriptOnly = true;

		// considered a trigger function for Mute cancel
		mayCancelMute = true;
		trigger = true;
	}
}

PUBLIC Event* MoveFunction::scheduleEvent(Action* action, Loop* l)
{
	Event* event = NULL;
    EventManager* em = l->getTrack()->getEventManager();

	// Since we don't quantize don't need to bother with modifying
	// any previously scheduled events.

    // New location specified with an expression whose result was left
    // in scriptArg

    long frame = action->arg.getInt();

    event = Function::scheduleEvent(action, l);
    if (event != NULL) {
        event->number = frame;
        if (!event->reschedule)
          em->schedulePlayJump(l, event);
    }

	return event;
}

PUBLIC void MoveFunction::prepareJump(Loop* l, Event* e, JumpContext* jump)
{
	Event* parent = e->getParent();
	if (parent == NULL) {
		Trace(l, 1, "MoveFunction: jump event with no parent");
	}
	else {
		// !! why don't we just convey this in the newFrame field of 
		// the JumpEvent?
		long newFrame = parent->number;
		long loopFrames = l->getFrames();

		// Round if we overshoot.  Being exactly on loopFrames is
		// common in scripts.
		if (newFrame >= loopFrames)
		  newFrame = newFrame % loopFrames;
		else if (newFrame < 0) {   
			// this is less common and more likely a calculation error
			int delta = newFrame % loopFrames;
			if (delta < 0)
			  newFrame = loopFrames + delta;
			else
			  newFrame = 0;
		}

		jump->frame = newFrame;
	}
}

PUBLIC void MoveFunction::doEvent(Loop* loop, Event* event)
{
	// Jump play will have done the work, but we now need to resync
	// the record frame with new play frame.  If we had already
	// recorded into this layer, it may require a shift()
	loop->shift(true);

	long newFrame = loop->recalculateFrame(false);

	// If this is Drift, we have to update the tracker too
	if (event->function == Drift) {
        Synchronizer* sync = loop->getSynchronizer();
        sync->loopDrift(loop, newFrame - loop->getFrame());
    }

	loop->setFrame(newFrame);
	loop->checkMuteCancel(event);

	// always reset the current mode?
	loop->resumePlay();

	loop->validate(event);
}

void MoveFunction::undoEvent(Loop* loop, Event* event)
{
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
