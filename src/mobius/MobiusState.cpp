/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Model for conveying Mobius engine state to the UI.
 *
 */

#include <stdio.h>
#include <string.h>

#include "Mode.h"
#include "MobiusState.h"

/****************************************************************************
 *                                                                          *
 *   							 MOBIUS STATE                               *
 *                                                                          *
 ****************************************************************************/

MobiusState::MobiusState()
{
	init();
}

MobiusState::~MobiusState()
{
}

void MobiusState::init()
{
	// not sure why but this doesn't seem to work reliably?
	// getting crashes in LoopMeter because it thinks it has a non-zero
	// event count and goes into space...
	//memset(this, 0, sizeof(MobiusState));

	bindings = NULL;
	globalRecording = false;
	strcpy(customMode, "");
	track = NULL;
};

/****************************************************************************
 *                                                                          *
 *   							 TRACK STATE                                *
 *                                                                          *
 ****************************************************************************/

TrackState::TrackState()
{
	init();
}

TrackState::~TrackState()
{
}

void TrackState::init()
{
	// not sure why but this doesn't seem to work reliably?
	// getting crashes in LoopMeter because it thinks it has a non-zero
	// event count and goes into space...
	//memset(this, 0, sizeof(TrackState));

	number = 0;
    name = NULL;
	preset = NULL;
	loops = 1;
	inputMonitorLevel = 0;
	outputMonitorLevel = 0;
	inputLevel = 0;
	outputLevel = 0;
	feedback = 0;
	altFeedback = 0;
	pan = 0;
    speedToggle = 0;
    speedOctave = 0;
	speedStep = 0;
	speedBend = 0;
    pitchOctave = 0;
    pitchStep = 0;
    pitchBend = 0;
    timeStretch = 0;
	reverse = false;
	focusLock = false;
    solo = false;
    globalMute = false;
    globalPause = false;
	group = 0;

	tempo = 0.0f;
	beat = 0;
	bar = 0;
	outSyncMaster = false;
	trackSyncMaster = false;

	loop = NULL;
};

/****************************************************************************
 *                                                                          *
 *   							  LOOP STATE                                *
 *                                                                          *
 ****************************************************************************/

LoopState::LoopState()
{
	init();
}

LoopState::~LoopState()
{
}

void LoopState::init()
{
	// not sure why but this doesn't seem to work reliably?
	// getting crashes in LoopMeter because it thinks it has a non-zero
	// event count and goes into space...
	//memset(this, 0, sizeof(LoopState));

	number = 1;
	mode = ResetMode;
	recording = false;
	paused = false;
    frame = 0;
	cycle = 0;
	cycles = 0;
	frames = 0;
	nextLoop = 0;
	returnLoop = 0;
	overdub = false;
	mute = false;

	// controls the size of the events array
	eventCount = 0;

	// controls the size of the layers array
	layerCount = 0;
	lostLayers = 0;

	// controls the size of the redo array
	redoCount = 0;
	lostRedo = 0;

	beatLoop = false;
	beatCycle = false;
	beatSubCycle = false;
};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
