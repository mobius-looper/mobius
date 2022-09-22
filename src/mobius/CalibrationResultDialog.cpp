/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Dialog to display the results of latency calibration.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "Util.h"
#include "Qwin.h"

#include "UI.h"

#define DEFAULT_ASIO_LATENCY_MSEC 10
#define DEFAULT_MME_LATENCY_MSEC 100

#define HELP1 "Connect the right channel output of your sound card"
#define HELP2 "to the left channel input of the same card."
#define HELP3 "When finished, click the Calibrate button."

PUBLIC CalibrationResultDialog::CalibrationResultDialog(Window* parent, 
														int total,
														int input, 
														int output)
{
	char buf[128];

	setParent(parent);
	setModal(true);
	setTitle("Calibration Result");
	setInsets(20, 20, 20, 0);

	Panel* root = getPanel();
	FormPanel* form = new FormPanel();
	root->add(form);

	sprintf(buf, "%d", total);
	form->add("Total measured latency frames", new Label(buf));

	sprintf(buf, "%d", input);
	form->add("Recommended input latency frames", new Label(buf));

	sprintf(buf, "%d", output);
	form->add("Recommended output latency frames", new Label(buf));

	root->add(new Strut(0, 20));
}

PUBLIC CalibrationResultDialog::~CalibrationResultDialog()
{
}

PUBLIC const char* CalibrationResultDialog::getOkName()
{
	return "Accept";
}

PUBLIC bool CalibrationResultDialog::commit()
{
	return true;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

