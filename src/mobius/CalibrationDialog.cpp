/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Dialog for performing latency calibration.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "Util.h"
#include "Qwin.h"

#include "MobiusConfig.h"
#include "UI.h"

#define DEFAULT_ASIO_LATENCY_MSEC 10
#define DEFAULT_MME_LATENCY_MSEC 100

#define HELP1 "Connect the right channel output of your sound card"
#define HELP2 "to the left channel input of the same card."
#define HELP3 "Click the Start button to begin the test."

PUBLIC CalibrationDialog::CalibrationDialog(Window* parent, MobiusInterface* m, 
											MobiusConfig* config)
{
	char buf[128];

	setParent(parent);
	setModal(true);
	setTitle("Latency Calibration");
	setInsets(20, 20, 20, 0);

	mMobius = m;
	mConfig = config;
	mResult = NULL;

	const char* input = config->getAudioInput();
	const char* output = config->getAudioOutput();

	Panel* root = getPanel();
	FormPanel* form = new FormPanel();
	root->add(form);

	form->add("Input device", new Label(input));
	sprintf(buf, "%d", m->getReportedInputLatency());
	form->add("Reported latency frames", new Label(buf));

	form->add("", new Strut(0, 20));

	form->add("Output device", new Label(output));
	sprintf(buf, "%d", m->getReportedOutputLatency());
	form->add("Reported latency frames", new Label(buf));

	root->add(new Strut(0, 20));

	root->add(new Label(HELP1));
	root->add(new Label(HELP2));
	root->add(new Label(HELP3));

	root->add(new Strut(0, 20));
}

PUBLIC const char* CalibrationDialog::getOkName()
{
	return "Start";
}

PUBLIC CalibrationDialog::~CalibrationDialog()
{
	delete mResult;
}

PUBLIC bool CalibrationDialog::commit()
{
	delete mResult;
	mResult = mMobius->calibrateLatency();
	return true;
}

PUBLIC CalibrationResult* CalibrationDialog::getResult()
{
	return mResult;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
