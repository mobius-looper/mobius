/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * A dialog for handling the "prompt" debugging function.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "Util.h"
#include "MessageCatalog.h"
#include "Qwin.h"

#include "Messages.h"
#include "UI.h"

PUBLIC PromptDialog::PromptDialog(Window* parent, UI* ui, Prompt* prompt)
{
	mNext = NULL;
	mUI = ui;
	mPrompt = prompt;

	setParent(parent);
	setModal(false);
	setIcon("Mobius");
	setTitle("Prompt");
	setInsets(20, 20, 20, 0);

	Panel* root = getPanel();
	root->setLayout(new BorderLayout());
	
	Panel* p = new Panel();
	p->setLayout(new VerticalLayout());
	p->add(new Strut(0, 20));
	p->add(new Label(prompt->getText()));
	p->add(new Strut(0, 20));
	root->add(p, BORDER_LAYOUT_CENTER);
}

void PromptDialog::setNext(PromptDialog* d)
{
	mNext = d;
}

PromptDialog* PromptDialog::getNext()
{
	return mNext;
}

Prompt* PromptDialog::getPrompt()
{
	return mPrompt;
}

/**
 * Overload this to suppress the cancel button.
 * For sync testing I like having a Cancel button so we can go
 * into loops.  Ideally the desired buttons should be passed
 * as an option in the ThreadEvent to the Prompt.
 */
PUBLIC const char* PromptDialog::getCancelName()
{
	return "Cancel";
}

PUBLIC PromptDialog::~PromptDialog()
{
}

/**
 * Called when the window is closed for any reason, including
 * clicking on the red X.
 * The mCommitted flag is set if the Ok button was pressed.
 */
void PromptDialog::closing()
{
	if (mPrompt != NULL) {
		mPrompt->setOk(isCommitted());
		mUI->finishPrompt(this, mPrompt);
	}
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
