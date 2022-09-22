/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Stutter, sort of a cross between Muultiply and Insert.
 * 
 */

#include <stdio.h>
#include <memory.h>
#include <string.h>

#include "Util.h"

#include "Action.h"
#include "Event.h"
#include "EventManager.h"
#include "Function.h"
#include "Stream.h"
#include "Layer.h"
#include "Loop.h"
#include "Messages.h"
#include "Mode.h"
#include "Segment.h"
#include "Synchronizer.h"
#include "Track.h"

//////////////////////////////////////////////////////////////////////
//
// StutterMode
//
//////////////////////////////////////////////////////////////////////

class StutterModeType : public MobiusMode {
  public:
	StutterModeType();
};

StutterModeType::StutterModeType() :
    MobiusMode("stutter", MSG_MODE_STUTTER)
{
	recording = true;
	extends = true;
	altFeedbackSensitive = true;
}

MobiusMode* StutterMode = new StutterModeType();

//////////////////////////////////////////////////////////////////////
//
// StutterEvent
//
//////////////////////////////////////////////////////////////////////

class StutterEventType : public EventType {
  public:
	StutterEventType();
};

PUBLIC StutterEventType::StutterEventType()
{
	name = "Stutter";
    // need this?
	//reschedules = true;
}

PUBLIC EventType* StutterEvent = new StutterEventType();

/****************************************************************************
 *                                                                          *
 *                                  STUTTER                                 *
 *                                                                          *
 ****************************************************************************/
/*
 * Potentially have play jump if we're toward the end of the cycle
 * we will be stuttering!!
 */

class StutterFunction : public Function {
  public:
	StutterFunction(bool sus);
	bool isSustain(Preset* p);
	Event* scheduleEvent(Action* action, Loop* l);
    void doEvent(Loop* loop, Event* event);
};

// SUS first for longFunction
PUBLIC Function* SUSStutter = new StutterFunction(true);
PUBLIC Function* Stutter = new StutterFunction(false);

PUBLIC StutterFunction::StutterFunction(bool sus)
{
	eventType = StutterEvent;
    mMode = StutterMode;
	majorMode = true;
	mayCancelMute = true;
	quantized = true;
	cancelReturn = true;
	sustain = sus;
 
	// on switch copy current cycle, enter stutter/multiply mode
	switchStack = true;
	switchStackMutex = true;

	if (!sus) {
		setName("Stutter");
		setKey(MSG_FUNC_STUTTER);
		longFunction = SUSStutter;
	}
	else {
		setName("SUSStutter");
		setKey(MSG_FUNC_SUS_STUTTER);
	}
}

PUBLIC bool StutterFunction::isSustain(Preset* p)
{
    bool isSustain = sustain;
    if (!isSustain) {
        // formerly sensitive to RecordMode
        // || (!mAuto && p->getRecordMode() == Preset::RECORD_SUSTAIN);
        const char* susfuncs = p->getSustainFunctions();
        if (susfuncs != NULL)
          isSustain = (IndexOf(susfuncs, "Stutter") >= 0);
    }
    return isSustain;
}

Event* StutterFunction::scheduleEvent(Action* action, Loop* l)
{
	Event* event = NULL;
	MobiusMode* mode = l->getMode();

	event = Function::scheduleEvent(action, l);
	if (event != NULL && !event->pending) {
		if (mode == StutterMode) {
			// when we leave StutterMode need to resume playing
			// at the cycle after the cycle
			Event* jump = l->scheduleStutterTransition(true);
			event->addChild(jump);
			// NOTE: Jumps are not supposed to have Functions, but during
			// stutter mode they do.  Now that we're ending the stutter and
			// the jump has a proper parent, remove the function to avoid
			// a warning trace messages.
			jump->function = NULL;

		}
		else {
			// make sure this happens after the boundary so we won't
			// start stuttering untl the stuttered cycle plays once
			// will schedule a transition when we get here
			event->afterLoop = true;

			// !! could schedule the play jump now in case we're very close
			// the end of the cycle that will be stuttered, currently 
			// we wait for stutterEvent, which could result in latency
			// loss (a play blip) but we'd have to be really close
			// to the end
		}
	}

	return event;
}

PUBLIC void StutterFunction::doEvent(Loop* l, Event* e)
{
    if (e->type == StutterEvent) {

        MobiusMode* mode = l->getMode();

        if (mode == StutterMode) {

            // shift immediately so we can undo 
            l->shift(false);

            Synchronizer* sync = l->getSynchronizer();
            sync->loopResize(l, false);

            // now we have to jump play back to the cycle after the stutter
            // should have scheduled this earlier, but be sure
            l->recalculatePlayFrame();
            l->resumePlay();

            // We will have scheduled a play jump at the end of the current
            // cycle to jump back to the start of the stuttered cycle.  Since
            // we're not going to get there now, remove it from the event list
            // and mark it processed to avoid a warning when we free the
            // parent event.  
            Event* jump = e->findEvent(JumpPlayEvent);
            if (jump != NULL) {
                EventManager* em = l->getTrack()->getEventManager();
                em->removeEvent(jump);
                jump->processed = true;
            }
        }
        else {
            if (mode== RehearseMode)
              l->cancelRehearse(e);

            else if (l->isRecording())
              l->finishRecording(e);

            l->cancelPrePlay();
            l->checkMuteCancel(e);

            // normally this would be the current frame but in Stutter mode,
            // we always want the base of the stuttered cycle
            long cycleFrames = l->getCycleFrames();
            if (cycleFrames > 0) {

                long newFrame = (l->getFrame() / cycleFrames) * cycleFrames;
                l->setModeStartFrame(newFrame);
                l->scheduleStutterTransition(false);
                l->setRecording(true);
                l->setMode(StutterMode);
            }
            else {
                // shouldn't be here?
                l->resumePlay();
            }
        }
    }

}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
