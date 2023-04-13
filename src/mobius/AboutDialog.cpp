/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 */

#include <stdio.h> 
#include <stdlib.h>
#include <math.h>

#include "Util.h"
#include "QwinExt.h"

#include "UI.h"

#define VERSION "Möbius version 2.5.1 [alpha Build 1 x86 / 2023]"

PUBLIC AboutDialog::AboutDialog(Window* parent)
{
	setParent(parent);
	setModal(true);
	setIcon("Mobius");
	setTitle("About Möbius");
	setInsets(20, 20, 20, 0);

	Panel* root = getPanel();

	Panel* text = new Panel();
	root->add(text, BORDER_LAYOUT_CENTER);
	text->setLayout(new VerticalLayout());

	text->add(new Label(VERSION));
	text->add(new Label("Copyright (c) 2005-2012 Jeffrey S. Larson"));
	text->add(new Label("Build 1/23 - 13/04/2023 | ClaudioCas"));
	text->add(new Label("All rights reserved."));

    // TODO: Need to credit Oli for pitch shifting, link
    // to LGPL libraries for relinking
}

PUBLIC const char* AboutDialog::getCancelName()
{
	return NULL;
}

PUBLIC AboutDialog::~AboutDialog()
{
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
