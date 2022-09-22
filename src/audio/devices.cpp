/* Copyright (C) 2004 Jeff Larson.  All rights reserved. */
/**
 * Use the Recorder classe to emit information about the audio devices.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "AudioInterface.h"

AUDIO_USE_NAMESPACE

int main(int argc, char **argv)
{
	AudioInterface* ai = AudioInterface::getInterface();
	ai->printDevices();
	return 0;
}
