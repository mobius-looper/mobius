/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Functions for capturing the audio stream and saving to a file.
 * These are all global functions.
 *
 */

#include <stdio.h>
#include <memory.h>

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
// CaptureMode
//
//////////////////////////////////////////////////////////////////////

class CaptureModeType : public MobiusMode {
  public:
	CaptureModeType();
};

CaptureModeType::CaptureModeType() : 
    MobiusMode("capture", "Capture")
{
	minor = true;
}

MobiusMode* CaptureMode = new CaptureModeType();

//////////////////////////////////////////////////////////////////////
//
// Capture Functions
//
//////////////////////////////////////////////////////////////////////

class CaptureFunction : public Function {
  public:
	CaptureFunction(bool stop, bool save);
	void invoke(Action* action, Mobius* m);
  private:
	bool mStop;
	bool mSave;
};

PUBLIC Function* StartCapture = new CaptureFunction(false, false);
PUBLIC Function* StopCapture = new CaptureFunction(true, false);
PUBLIC Function* SaveCapture = new CaptureFunction(false, true);

PUBLIC CaptureFunction::CaptureFunction(bool stop, bool save)
{
	global = true;
	mStop = stop;
	mSave = save;

	if (mSave) {
		setName("SaveCapture");
		setKey(MSG_FUNC_SAVE_CAPTURE);
        alias1 = "SaveAudioRecording";
	}
	else if (mStop) {
		setName("StopCapture");
		setKey(MSG_FUNC_STOP_CAPTURE);
		alias1 = "StopAudioRecording";
	}
	else {
		setName("StartCapture");
		setKey(MSG_FUNC_START_CAPTURE);
		alias1 = "StartAudioRecording";
	}
}

PUBLIC void CaptureFunction::invoke(Action* action, Mobius* m)
{
	if (action->down) {
		trace(action, m);
		if (mSave) 
		  m->saveCapture(action);
		else if (mStop)
		  m->stopCapture(action);
		else
		  m->startCapture(action);
	}
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
