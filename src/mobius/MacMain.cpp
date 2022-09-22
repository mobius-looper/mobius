/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * OSX Main routine for the standalone application
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "Util.h"
#include "XmlBuffer.h"
#include "List.h"
#include "MidiInterface.h"
#include "Context.h"

#include "Mobius.h"
#include "ObjectPool.h"
#include "UI.h"

#include "MacInstall.h"

int main(int argc, char *argv[])
{
	int result = 0;
	
	Context* con = Context::getContext(argc, argv);

	// Boostrap Application Support directory
	MacInstall(con);

	//con->printContext();

	UIFrame* frame = NULL;

	// have to convert some things so Mobius doesn't depend on qwin
	MobiusContext* mcon = new MobiusContext();
	mcon->setCommandLine(con->getCommandLine());
	mcon->setInstallationDirectory(con->getInstallationDirectory());
	mcon->setConfigurationDirectory(con->getConfigurationDirectory());

    // standard device interfaces
    mcon->setMidiInterface(MidiInterface::getInterface("Mobius"));
    mcon->setAudioInterface(AudioInterface::getInterface());

	Mobius* mobius = new Mobius(mcon);

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
		QwinExit(false);
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
