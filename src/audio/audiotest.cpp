/*
 * Simple test of the audio interface abstraction
 * This has devolved to the point where it's the same
 * as devices.exe
 *
 */

#include <stdio.h>

#include "AudioInterface.h"

AUDIO_USE_NAMESPACE

//////////////////////////////////////////////////////////////////////
//
// Device Enumeration
//
//////////////////////////////////////////////////////////////////////

void showDevices()
{
	AudioInterface* ai = AudioInterface::getInterface();
	ai->printDevices();
}

/****************************************************************************
 *                                                                          *
 *                                    MAIN                                  *
 *                                                                          *
 ****************************************************************************/


int main(int argc, char *argv[])
{
	showDevices();

	return 0;
}


