/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Terminate any recording mode and return to play.
 *
 */

#include <memory.h>
#include <string.h>

#include "Action.h"
#include "Event.h"
#include "EventManager.h"
#include "Function.h"
#include "Layer.h"
#include "Loop.h"
#include "Messages.h"
#include "Mode.h"
#include "Track.h"

//////////////////////////////////////////////////////////////////////
//
// PlayMode
//
//////////////////////////////////////////////////////////////////////

class PlayModeType : public MobiusMode {
  public:
	PlayModeType();
};

PlayModeType::PlayModeType() :
    MobiusMode("play", MSG_MODE_PLAY)
{
}

MobiusMode* PlayMode = new PlayModeType();

//////////////////////////////////////////////////////////////////////
//
// PlayEvent
//
//////////////////////////////////////////////////////////////////////

class PlayEventType : public EventType {
  public:
	PlayEventType();
};

PUBLIC PlayEventType::PlayEventType()
{
	name = "Play";
}

PUBLIC EventType* PlayEvent = new PlayEventType();

//////////////////////////////////////////////////////////////////////
//
// PlayFunction
//
//////////////////////////////////////////////////////////////////////

class PlayFunction : public Function {
  public:
	PlayFunction();
	Event* scheduleSwitchStack(Action* action, Loop* l);
	void doEvent(Loop* loop, Event* event);
	void undoEvent(Loop* loop, Event* event);
};

PUBLIC Function* Play = new PlayFunction();

PUBLIC PlayFunction::PlayFunction() :
    Function("Play", MSG_FUNC_PLAY)
{
	eventType = PlayEvent;
    mMode = PlayMode;

	// this is not a mayCancelMute function, it always unmutes
}

/**
 * Cancel the switch and all stacked events
 */
Event* PlayFunction::scheduleSwitchStack(Action* action, Loop* l)
{
    EventManager* em = l->getTrack()->getEventManager();
	em->cancelSwitch();

	return NULL;
}

PUBLIC void PlayFunction::undoEvent(Loop* loop, Event* event)
{
}

PUBLIC void PlayFunction::doEvent(Loop* loop, Event* event)
{
    MobiusMode* mode = loop->getMode();
	if (mode == RehearseMode) {
		// calls finishRecording or resumePlay
		loop->cancelRehearse(event);
	}
	else if (loop->isRecording()) {
		loop->finishRecording(event);
	}

	loop->setOverdub(false);
	loop->setMuteMode(false);
	loop->setMute(false);
	loop->setPause(false);

	loop->resumePlay();
	loop->validate(event);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
