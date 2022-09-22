/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Reset a loop to its initial state.
 *
 */

#include <stdio.h>
#include <memory.h>
#include <string.h>

#include "Action.h"
#include "Event.h"
#include "Function.h"
#include "Loop.h"
#include "Layer.h"
#include "Messages.h"
#include "Mobius.h"
#include "Mode.h"
#include "Track.h"

//////////////////////////////////////////////////////////////////////
//
// ResetMode
//
//////////////////////////////////////////////////////////////////////

class ResetModeType : public MobiusMode {
  public:
	ResetModeType();
};

ResetModeType::ResetModeType() :
    MobiusMode("reset", MSG_MODE_RESET)
{
}

MobiusMode* ResetMode = new ResetModeType();

//////////////////////////////////////////////////////////////////////
//
// ResetFunction
//
//////////////////////////////////////////////////////////////////////

class ResetFunction : public Function {
  public:
	ResetFunction(bool gen, bool glob);
	Event* invoke(Action* action, Loop* l);
  private:
};

PUBLIC Function* Reset = new ResetFunction(false, false);
PUBLIC Function* TrackReset = new ResetFunction(true, false);
PUBLIC Function* GlobalReset = new ResetFunction(false, true);

PUBLIC ResetFunction::ResetFunction(bool gen, bool glob)
{
    mMode = ResetMode;
	majorMode = true;
	cancelMute = true;
	thresholdEnabled = true;
    //realignController = true;

	// Note that "glob" only controls how the function is named,
	// this does *not* become a global function, it must still be
	// deferred to the audio interrupt

	if (gen) {
		setName("TrackReset");
		setKey(MSG_FUNC_TRACK_RESET);
		setHelp("Immediately reset all loops");
        alias1 = "GeneralReset";
	}
	else if (glob) {
		setName("GlobalReset");
		setKey(MSG_FUNC_GLOBAL_RESET);
		setHelp("Immediately reset all tracks");
		noFocusLock = true;
	}
	else {
		setName("Reset");
		setKey(MSG_FUNC_RESET);
		setHelp("Immediately reset current loop");
        mayConfirm = true;
	}
}

PUBLIC Event* ResetFunction::invoke(Action* action, Loop* loop)
{
	if (action->down) {
		trace(action, loop);

		Function* func = action->getFunction();

		if (func == GlobalReset) {
			// shouldn't have called this
			Mobius* m = loop->getMobius();
			m->globalReset(action);
		}
		else if (func == TrackReset) {
			Track* track = loop->getTrack();
			track->reset(action);
		}
		else {
			Track* track = loop->getTrack();
			track->loopReset(action, loop);
		}
	}
	return NULL;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
