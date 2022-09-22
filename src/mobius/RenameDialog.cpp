/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * A dialog for prompting for a name for something.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "Util.h"
#include "MessageCatalog.h"
#include "Qwin.h"

#include "Messages.h"
#include "UI.h"

PUBLIC RenameDialog::RenameDialog(Window* parent, 
                                  const char* title,
                                  const char* prompt,
                                  const char* current)
{
    mText = NULL;
    mValue = NULL;

	setParent(parent);
	setModal(true);
	setIcon("Mobius");
	setTitle(title);
	setInsets(20, 20, 20, 0);

	Panel* root = getPanel();
	root->setLayout(new BorderLayout());
	
    FormPanel* form = new FormPanel();
    root->add(form, BORDER_LAYOUT_CENTER);

    mText = form->addText(this, prompt);
    if (current != NULL)
      mText->setText(current);
}

PUBLIC RenameDialog::~RenameDialog()
{
    delete mValue;
}

/**
 * Overload this to suppress the cancel button.
 * For sync testing I like having a Cancel button so we can go
 * into loops.  Ideally the desired buttons should be passed
 * as an option in the ThreadEvent to the Rename.
 */
PUBLIC const char* RenameDialog::getCancelName()
{
	return "Cancel";
}

/**
 * Have To capture this in the close dialog before the components 
 * are closed.
 */
PUBLIC void RenameDialog::closing()
{
    mValue = CopyString(mText->getText());
}

const char* RenameDialog::getValue()
{
	return mValue;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
