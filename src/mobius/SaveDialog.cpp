/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * NOT USED
 *
 * Dialog for granular state save.
 * This was an old experiment that never went anywhere and it is not
 * included in the menus.  The notion was that we would present
 * some kind of grid of checkboxes where you could check what you wanted
 * to save:
 *
 *   Track 1 2 3 ... 8 All
 *    ""   X X X     X X
 *   All   X X X ... X ""
 *   Loop1 X X X ... X ""
 *   ...
 *   Loop8 X X X ...X ""
 *
 * In practice I'm not sure how useful this would be, if we did do this it should
 * be part of the project options?
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "util.h"
#include "qwin.h"

#include "ui.h"

PUBLIC SaveDialog::SaveDialog(Window* parent, MobiusInterface* m)
{
	int i;

	setParent(parent);
	setModal(true);
	setIcon("Mobius");
	setTitle("Save");
	setInsets(20, 20, 20, 0);

	mMobius = m;

	Panel* root = getPanel();

	mGrid = new Panel();
	GridLayout* gl = new GridLayout(11, 10);
	gl->setCenter(true);
	mGrid->setLayout(gl);
	root->add(mGrid, BORDER_LAYOUT_CENTER);

	// track headers
	// !! need to have independent vert and horiz gaps in GridLayout
	// have to add spaces around these labels
	mGrid->add(new Label("Track"));
	for (i = 0 ; i < 8 ; i++) {
		char buf[256];
		//sprintf(buf, " Track %d ", i+1);
		sprintf(buf, "%d", i+1);
		mGrid->add(new Label(buf));
	}
	mGrid->add(new Label("All"));

	// track selectors
	mGrid->add(new Label(""));
	for (i = 0 ; i < 8 ; i++)
	  mGrid->add(newCheckbox());
	mGrid->add(newCheckbox());
		
	// loop selectors
	addLoopChecks(mGrid, "All");
	for (i = 0 ; i < 8 ; i++) {
		char buf[256];
		sprintf(buf, "Loop %d", i+1);
		addLoopChecks(mGrid, buf);
	}

}

PRIVATE void SaveDialog::addLoopChecks(Panel* grid, const char* name)
{
	grid->add(new Label(name));
	for (int i = 0 ; i < 8 ; i++)
	  grid->add(newCheckbox());
	// and an extra for the All tracks column
	grid->add(new Label(""));

}

PRIVATE Checkbox* SaveDialog::newCheckbox()
{
	Checkbox* cb = new Checkbox();
	cb->addActionListener(this);
	return cb;
}

PUBLIC SaveDialog::~SaveDialog()
{
}

bool SaveDialog::commit()
{
	return true;
}


void SaveDialog::actionPerformed(void* src)
{
	SimpleDialog::actionPerformed(src);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
