/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Main routine for the standalone application.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "MidiInterface.h"
#include "Qwin.h"
#include "Util.h"
#include "UIWindows.h"

#include "Mobius.h"
#include "ObjectPool.h"
#include "UI.h"

// in WinInit
extern void WinMobiusInit(WindowsContext* wc);

/****************************************************************************
 *                                                                          *
 *   							   WINMAIN                                  *
 *                                                                          *
 ****************************************************************************/

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prevInstance,
				   LPSTR commandLine, int cmdShow)
{
    int result = 0;

    // useful to debug layout problems, but too slow afterward
	//Component::TraceEnabled = true;
	//Component::PaintTraceEnabled = true;

	WindowsContext* con = new WindowsContext(instance, commandLine, cmdShow);
	UIFrame* frame = NULL;

    // This adds the installation directory
    WinMobiusInit(con);

	// have to convert some things so Mobius doesn't depend on qwin
	MobiusContext* mcon = new MobiusContext();
	mcon->setCommandLine(con->getCommandLine());
	mcon->setInstallationDirectory(con->getInstallationDirectory());

    // standard device interfaces
    mcon->setMidiInterface(MidiInterface::getInterface("Mobius"));
    mcon->setAudioInterface(AudioInterface::getInterface());

	Mobius* mobius = new Mobius(mcon);

    // always enable this in standalone mode
    mobius->setCheckInterrupt(true);

	// at this point, the command line has been parsed and
	// we know if we're supposed to catch all exceptions

	if (mcon->isDebugging()) {
		mobius->start();
		frame = new UIFrame(con, mobius);
		result = frame->run();
	}
	else {
		try {
			mobius->start();
			frame = new UIFrame(con, mobius);
			result = frame->run();
		}
		catch (...) {
			Trace(1, "Exception running Mobius!\n");
		}
	}

	// be very careful about stopping here, we *must* clean up
	// or else the application hangs

	try {
		if (frame != NULL)
		  delete frame;
	}
	catch (...) {
		Trace(1, "Exception deleting frame!\n");
	}

	try {
		delete con;
	}
	catch (...) {
		Trace(1, "Exception deleting context!\n");
	}

	try {
		printf("Deleting Mobius...\n");
		fflush(stdout);
        // this will print ending pool diagnostics
		delete mobius;
	}
	catch (...) {
		Trace(1, "Exception deleting Mobius!\n");
	}

	try {
		printf("Shutting down MIDI...\n");
		fflush(stdout);
		MidiInterface::exit();
	}
	catch (...) {
		Trace(1, "Exception shutting down MIDI!\n");
	}

	try {
		printf("Shutting down Audio...\n");
		fflush(stdout);
		AudioInterface::exit();
	}
	catch (...) {
		Trace(1, "Exception shutting down Audio!\n");
	}

	try {
		// pass true to dump font info
		Qwin::exit(false);
	}
	catch (...) {
		Trace(1, "Exception shutting down Qwin!\n");
	}

	try {
		// pass true to dump statistics
		ObjectPoolManager::exit(true);
	}
	catch (...) {
		Trace(1, "Exception dumping pool statistics!\n");
	}

    return result;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
