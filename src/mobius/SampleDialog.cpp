/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Dialog for the specification of Samples.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "Util.h"
#include "MessageCatalog.h"
#include "Qwin.h"

#include "Messages.h"
#include "Mobius.h"
#include "MobiusConfig.h"
#include "Sample.h"

#include "UI.h"

PUBLIC SampleDialog::SampleDialog(Window* parent, MobiusInterface* mob, 
                                  MobiusConfig* c)
{
	mCatalog = mob->getMessageCatalog();
	mConfig = c;

	setParent(parent);
	setModal(true);
	setIcon("Mobius");
	setTitle(mCatalog->get(MSG_DLG_SAMPLE_TITLE));
	setInsets(20, 20, 20, 0);

	// odd level of indirection now that all we do is wrap a list!!
    Sample* samples = NULL;
	Samples* sampleConfig = c->getSamples();
    if (sampleConfig != NULL)
      samples = sampleConfig->getSamples();

	Panel* root = getPanel();
	VerticalLayout *vl = new VerticalLayout(10);
	vl->setCenterX(true);
	root->setLayout(vl);

	mSelector = new ListBox();
	mSelector->setColumns(40);
	mSelector->setRows(20);
	mSelector->addActionListener(this);

	StringList* values = new StringList();
	if (samples != NULL) {
		for (Sample* s = samples ; s != NULL ; s = s->getNext()) {
			values->add(s->getFilename());
		}
	}

	mSelector->setValues(values);

	Panel* buttons = new Panel();
	buttons->setLayout(new HorizontalLayout(4));

	mAdd = new Button(mCatalog->get(MSG_DLG_ADD));
	mAdd->addActionListener(this);
	//mAdd->setFont(Font::getFont("Arial", 0, 10));
	buttons->add(mAdd);
	mDelete = new Button(mCatalog->get(MSG_DLG_DELETE));
	mDelete->addActionListener(this);
	//mDelete->setFont(Font::getFont("Arial", 0, 10));
	buttons->add(mDelete);
	mUp = new Button(mCatalog->get(MSG_DLG_MOVE_UP));
	mUp->addActionListener(this);
	//mUp->setFont(Font::getFont("Arial", 0, 10));
	buttons->add(mUp);
	mDown = new Button(mCatalog->get(MSG_DLG_MOVE_DOWN));
	mDown->addActionListener(this);
	//mDown->setFont(Font::getFont("Arial", 0, 10));
	buttons->add(mDown);

	root->add(buttons);
	root->add(mSelector);
}

PUBLIC SampleDialog::~SampleDialog()
{
}

void SampleDialog::actionPerformed(void *c) 
{
	if (c == mAdd) {
		char filter[1024];
		OpenDialog* d = new OpenDialog(this);
		d->setTitle(mCatalog->get(MSG_DLG_SAMPLE_TITLE));
		sprintf(filter, "%s|*.WAV", 
				mCatalog->get(MSG_DLG_OPEN_LOOP_FILTER));
		d->setFilter(filter);
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
	else if (c == mUp) {
		mSelector->moveUp(mSelector->getSelectedIndex());
	}
	else if (c == mDown) {
		mSelector->moveDown(mSelector->getSelectedIndex());
	}
	else
	  SimpleDialog::actionPerformed(c);
}

bool SampleDialog::commit()
{
	Samples* sc = mConfig->getSamples();
    if (sc != NULL) 
      sc->clear();
    else {
        sc = new Samples();
        mConfig->setSamples(sc);
    }

	StringList* files = mSelector->getValues();
	if (files != NULL) {
		for (int i = 0 ; i < files->size() ; i++) {
			sc->add(new Sample(files->getString(i)));
		}
	}

	return true;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
