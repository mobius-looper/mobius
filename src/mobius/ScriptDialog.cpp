/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Dialog for the specification of Scripts.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "Util.h"
#include "MessageCatalog.h"
#include "Qwin.h"

#include "Messages.h"
#include "MobiusConfig.h"
#include "Recorder.h"
#include "Script.h"

#include "UI.h"

PUBLIC ScriptDialog::ScriptDialog(Window* parent, MobiusInterface* mob, 
                                  MobiusConfig* c)
{
	mCatalog = mob->getMessageCatalog();
	mConfig = c;

	setParent(parent);
	setModal(true);
	setIcon("Mobius");
	setTitle(mCatalog->get(MSG_DLG_SCRIPT_TITLE));
	setInsets(20, 20, 20, 0);

	ScriptConfig* sc = c->getScriptConfig();
	ScriptRef* scripts = sc->getScripts();

	Panel* root = getPanel();
	VerticalLayout *vl = new VerticalLayout(10);
	vl->setCenterX(true);
	root->setLayout(vl);

	mSelector = new ListBox();
	mSelector->setColumns(40);
	mSelector->setRows(20);
	mSelector->addActionListener(this);

	StringList* values = new StringList();
	for (ScriptRef* ref = scripts ; ref != NULL ; ref = ref->getNext())
	  values->add(ref->getFile());

	mSelector->setValues(values);

	Panel* buttons = new Panel();
	buttons->setLayout(new HorizontalLayout(4));

    // Windows can't select both files and directories in the
    // same dialog.  Mac doesn't have AFAIK a directory only browser
    // so we'll conditionlize the UI.
    mAddDir = NULL;
#ifdef _WIN32
	mAdd = new Button(mCatalog->get(MSG_DLG_SCRIPT_ADD_FILE));
	mAdd->addActionListener(this);
	buttons->add(mAdd);
	mAddDir = new Button(mCatalog->get(MSG_DLG_SCRIPT_ADD_DIRECTORY));
	mAddDir->addActionListener(this);
	buttons->add(mAddDir);
#else
	mAdd = new Button(mCatalog->get(MSG_DLG_ADD));
	mAdd->addActionListener(this);
	//mAdd->setFont(Font::getFont("Arial", 0, 10));
	buttons->add(mAdd);
#endif

	mDelete = new Button(mCatalog->get(MSG_DLG_DELETE));
	mDelete->addActionListener(this);
	//mDelete->setFont(Font::getFont("Arial", 0, 10));
	buttons->add(mDelete);

	root->add(buttons);
	root->add(mSelector);
}

PUBLIC ScriptDialog::~ScriptDialog()
{
}

void ScriptDialog::actionPerformed(void *c) 
{
	if (c == mAdd) {
		char filter[1024];
		OpenDialog* d = new OpenDialog(this);
		d->setTitle(mCatalog->get(MSG_DLG_SCRIPT_OPEN));
		sprintf(filter, "%s|*.mos|%s|*.*", 
				mCatalog->get(MSG_DLG_SCRIPT_FILTER),
				mCatalog->get(MSG_DLG_ALL));
		d->setFilter(filter);

        // sigh, Windows can't select both files and directories in the 
        // same dialog, have to use two
#ifndef _WIN32
		d->setAllowDirectories(true);
#endif
        d->setAllowMultiple(true);
		d->show();
		if (!d->isCanceled()) {
			const char* filename = d->getFile();
			if (filename != NULL)
			  mSelector->addValue(filename);
		}
		delete d;
	}
	else if (c == mAddDir) {
        // should only be here on Windows
		OpenDialog* d = new OpenDialog(this);
		d->setTitle(mCatalog->get(MSG_DLG_SCRIPT_OPEN));
		d->setAllowDirectories(true);
		d->show();
		if (!d->isCanceled()) {
			const char* filename = d->getFile();
			if (filename != NULL)
			  mSelector->addValue(filename);
		}
		delete d;
	}
	else if (c == mDelete) {
		mSelector->deleteValue(mSelector->getSelectedIndex());
	}
	else
	  SimpleDialog::actionPerformed(c);
}

bool ScriptDialog::commit()
{
	ScriptConfig* sc = mConfig->getScriptConfig();
	sc->setScripts(NULL);

	StringList* files = mSelector->getValues();
	if (files != NULL) {
		for (int i = 0 ; i < files->size() ; i++) {
			sc->add(files->getString(i));
		}
	}

	return true;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
