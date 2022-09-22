/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Bouncing one or more source tracks to a target track.
 *
 */

#include <stdio.h>
#include <memory.h>
#include <string.h>

#include "Action.h"
#include "Event.h"
#include "Function.h"
#include "Layer.h"
#include "Loop.h"
#include "Messages.h"
#include "Mobius.h"
#include "Mode.h"

//////////////////////////////////////////////////////////////////////
//
// BounceEvent
//
//////////////////////////////////////////////////////////////////////

class BounceEventType : public EventType {
  public:
	BounceEventType();
};

PUBLIC BounceEventType::BounceEventType()
{
	name = "Bounce";
	noMode = true;
}

PUBLIC EventType* BounceEvent = new BounceEventType();

//////////////////////////////////////////////////////////////////////
//
// BounceFunction
//
//////////////////////////////////////////////////////////////////////

class BounceFunction : public Function {
  public:
	BounceFunction();
	Event* invoke(Action* action, Loop* l);
	void doEvent(Loop* l, Event* e);
  private:
};

PUBLIC Function* Bounce = new BounceFunction();

PUBLIC BounceFunction::BounceFunction() :
    Function("Bounce", MSG_FUNC_BOUNCE)
{
	setHelp("Start or end a bounce down recording");

	// this is not a "global" function, since we try to schedule events
	// in the current track
	noFocusLock = true;
	quantized = true;
	eventType = BounceEvent;

}

PUBLIC Event* BounceFunction::invoke(Action* action, Loop* loop)
{
	Mobius* mobius = loop->getMobius();
	MobiusMode* mode = loop->getMode();
	Event* event = NULL;

	if (action->down) {
		if (mode == ThresholdMode || mode == SynchronizeMode) {
			// it feels most useful to schedule an event for frame 0
			// so it starts as soon as we reach the Threshold/Sync boundary?
			// actually I can't think of a good use for this in these modes, 
			// we're not capturing the input signal so there will just
			// be dead space until the first loop is finished, maybe better
			// to just ignore it here?
			event = Function::scheduleEvent(action, loop);
			event->frame = 0;
		}
		else if (mode == ResetMode) {
			// what does this mean?  
			// I suppose we could just be getting ready to start triggering
			// so start recording now
			mobius->toggleBounceRecording(action);
		}
		else if (loop->isPaused()) {
			// what does this mean, we could just start recording now?
			mobius->toggleBounceRecording(action);
		}
		else {
			// this should not come back pending if we're in multiply/insert
			// mode since the "noMode" flag is set in the EventType
			event = Function::scheduleEvent(action, loop);

			// if we're quantized, this is the "record frame"
			// in this special case we need to reduce this since we're 
			// recording the output stream, it's complicated...
			if (event->quantized) {
				long frame = event->frame;
				long desired = frame - loop->getInputLatency() - loop->getOutputLatency();
				if (desired < 0)
				  desired = 0;
				event->frame = desired;
			}
		}
	}

	return event;
}

/**
 * Ugh, all thelogic is up in Mobius which will then call
 * down to Loop::setBounceRecording in a different track.
 */
PUBLIC void BounceFunction::doEvent(Loop* loop, Event* event)
{
	Mobius* mobius = loop->getMobius();
	mobius->toggleBounceRecording(NULL);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
