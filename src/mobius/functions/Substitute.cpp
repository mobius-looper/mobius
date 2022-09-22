/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Replace a loop section while hearing the current content.
 *
 */

#include <stdio.h>
#include <memory.h>
#include <string.h>

#include "Action.h"
#include "Util.h"
#include "Event.h"
#include "Function.h"
#include "Layer.h"
#include "Loop.h"
#include "Messages.h"
#include "Mode.h"

//////////////////////////////////////////////////////////////////////
//
// SubstituteMode
//
//////////////////////////////////////////////////////////////////////

class SubstituteModeType : public MobiusMode {
  public:
	SubstituteModeType();
};

SubstituteModeType::SubstituteModeType() :
    MobiusMode("substitute", MSG_MODE_SUBSTITUTE)
{
	recording = true;
	altFeedbackSensitive = true;
}

MobiusMode* SubstituteMode = new SubstituteModeType();

//////////////////////////////////////////////////////////////////////
//
// SubstituteEvent
//
//////////////////////////////////////////////////////////////////////

class SubstituteEventType : public EventType {
  public:
	SubstituteEventType();
};

PUBLIC SubstituteEventType::SubstituteEventType()
{
	name = "Substitute";
}

PUBLIC EventType* SubstituteEvent = new SubstituteEventType();

//////////////////////////////////////////////////////////////////////
//
// SubstituteFunction
//
//////////////////////////////////////////////////////////////////////

class SubstituteFunction : public Function {
  public:
	SubstituteFunction(bool sus);
	bool isSustain(Preset* p);
	void doEvent(Loop* l, Event* e);
};

// SUS first for longFunction
PUBLIC Function* SUSSubstitute = new SubstituteFunction(true);
PUBLIC Function* Substitute = new SubstituteFunction(false);

PUBLIC SubstituteFunction::SubstituteFunction(bool sus)
{
	eventType = SubstituteEvent;
    mMode = SubstituteMode;
	majorMode = true;
	mayCancelMute = true;
	quantized = true;
	cancelReturn = true;
	sustain = sus;

	// could do SoundCopy then enter Substitute?
	//switchStack = true;
	//switchStackMutex = true;

	if (!sus) {
		setName("Substitute");
		setKey(MSG_FUNC_SUBSTITUTE);
		longFunction = SUSSubstitute;
        // can also force this with SustainFunctions parameter
        maySustain = true;
        mayConfirm = true;
	}
	else {
		setName("SUSSubstitute");
		setKey(MSG_FUNC_SUS_SUBSTITUTE);
	}
}

PUBLIC bool SubstituteFunction::isSustain(Preset* p)
{
    bool isSustain = sustain;
    if (!isSustain) {
        // formerly sensitive to RecordMode
        // || (!mAuto && p->getRecordMode() == Preset::RECORD_SUSTAIN);
        const char* susfuncs = p->getSustainFunctions();
        if (susfuncs != NULL)
          isSustain = (IndexOf(susfuncs, "Substitute") >= 0);
    }
    return isSustain;
}

/**
 * SubstituteEvent event handler.
 * Like Replace except the original loop is audible.
 */
PUBLIC void SubstituteFunction::doEvent(Loop* loop, Event* event)
{
	MobiusMode* mode = loop->getMode();

	if (mode == SubstituteMode) {
        loop->finishRecording(event);
    }
    else {
        if (mode == RehearseMode)
          loop->cancelRehearse(event);
        else if (loop->isRecording())
          loop->finishRecording(event);

        loop->cancelPrePlay();
		loop->checkMuteCancel(event);

        loop->setRecording(true);
        loop->setMode(SubstituteMode);
    }

	loop->validate(event);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
