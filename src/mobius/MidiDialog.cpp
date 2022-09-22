/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Dialog for selection of the MIDI input and output devices.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "Util.h"
#include "MessageCatalog.h"
#include "MidiInterface.h"
#include "MidiPort.h"

#include "Qwin.h"

#include "Messages.h"
#include "Mobius.h"
#include "MobiusConfig.h"

#include "UI.h"

#define BOX_ROWS 5
#define BOX_COLS 20

PUBLIC MidiDialog::MidiDialog(Window* parent, 
                              MobiusInterface* mob, 
							  MobiusConfig* config)
{
	MessageCatalog* cat = mob->getMessageCatalog();

	setParent(parent);
	setModal(true);
	setTitle(cat->get(MSG_DLG_MIDI_TITLE));
	setInsets(20, 20, 20, 0);

    // this must be a clone
	mConfig = config;

	Panel* root = getPanel();
	root->setLayout(new HorizontalLayout(8));
	
	Panel* p = new Panel();
	p->setLayout(new VerticalLayout());
	root->add(p);
	p->add(new Label(cat->get(MSG_DLG_MIDI_INPUT)));
	mInputs = new ListBox();
	mInputs->setRows(BOX_ROWS);
	mInputs->setColumns(BOX_COLS);
	p->add(mInputs);
   
	p->add(new Strut(0, 10));
	p->add(new Label(cat->get(MSG_DLG_MIDI_OUTPUT)));
	mOutputs = new ListBox();
	mOutputs->setRows(BOX_ROWS);
	mOutputs->setColumns(BOX_COLS);
	p->add(mOutputs);

	p->add(new Strut(0, 10));
	p->add(new Label(cat->get(MSG_DLG_MIDI_THRU)));
	mThrus = new ListBox();
	mThrus->setRows(BOX_ROWS);
	mThrus->setColumns(BOX_COLS);
	p->add(mThrus);

	p = new Panel();
	p->setLayout(new VerticalLayout());
	root->add(p);
	p->add(new Label(cat->get(MSG_DLG_PLUGIN_MIDI_INPUT)));
	mPluginInputs = new ListBox();
	mPluginInputs->setRows(BOX_ROWS);
	mPluginInputs->setColumns(BOX_COLS);
	p->add(mPluginInputs);
   
	p->add(new Strut(0, 10));
	p->add(new Label(cat->get(MSG_DLG_PLUGIN_MIDI_OUTPUT)));
	mPluginOutputs = new ListBox();
	mPluginOutputs->setRows(BOX_ROWS);
	mPluginOutputs->setColumns(BOX_COLS);
	p->add(mPluginOutputs);

	p->add(new Strut(0, 10));
	p->add(new Label(cat->get(MSG_DLG_PLUGIN_MIDI_THRU)));
	mPluginThrus = new ListBox();
	mPluginThrus->setRows(BOX_ROWS);
	mPluginThrus->setColumns(BOX_COLS);
	p->add(mPluginThrus);

    MobiusContext* mc = mob->getContext();
	MidiInterface* midi = mc->getMidiInterface();
	MidiPort* devs = midi->getInputPorts();
	addDevices(cat, devs, mInputs, config->getMidiInput(), true);
	addDevices(cat, devs, mPluginInputs, config->getPluginMidiInput(), true);
	devs = midi->getOutputPorts();
	addDevices(cat, devs, mOutputs, config->getMidiOutput(), true);
	addDevices(cat, devs, mThrus, config->getMidiThrough(), false);
	addDevices(cat, devs, mPluginOutputs, config->getPluginMidiOutput(), true);
	addDevices(cat, devs, mPluginThrus, config->getPluginMidiThrough(), false);
}

PRIVATE void MidiDialog::addDevices(MessageCatalog* cat,
									MidiPort* devs, 
									ListBox* box,
									const char* current, 
									bool multi)
{
	MidiPort* d;
	int i;

	// leave a (none) indicator so we can deselect something,
	// seems to be no way to do this with a single select ListBox??
	if (!multi)
	  box->addValue(cat->get(MSG_DLG_SELECT_NONE));
	
	box->setMultiSelect(multi);

	if (!multi || current == NULL) {
		for (i = 0, d = devs ; d != NULL ; d = d->getNext(), i++) {
			box->addValue(d->getName());
			if (current != NULL && !strcmp(current, d->getName()))
			  // remember to adjust for "none"
			  box->setSelectedIndex(i + 1);
		}
	}
	else {
		StringList* names = new StringList(current);
		for (i = 0, d = devs ; d != NULL ; d = d->getNext(), i++) {
			box->addValue(d->getName());
			if (names->contains(d->getName()))
			  // note that we have no "none" adjustment here
			  box->setSelectedIndex(i);
		}
		delete names;
	}

	// Try to make the box large enough to display everything without
	// a scroll bar.  For some odd reason, calling setSelectedIndex
	// causes the Listbox to be scrolled to the first item after
	// the selected one.  Need a way to scroll this back to zero, or
	// better yet have the selected one be one top.

	// !! need a governor here
	//box->setRows(i);
}

PUBLIC MidiDialog::~MidiDialog()
{
    // caller owns mConfig
}

PUBLIC bool MidiDialog::commit()
{
	// Single selects
	// Note that ListBox item 0 is "none" which we want to ignore

	const char* value = NULL;
	if (mThrus->getSelectedIndex() > 0)
	  value = mThrus->getSelectedValue();
	mConfig->setMidiThrough(value);

	value = NULL;
	if (mPluginThrus->getSelectedIndex() > 0)
	  value = mPluginThrus->getSelectedValue();
	mConfig->setPluginMidiThrough(value);

	// Multi selects

	char* csv = mInputs->getSelectedCsv();
	mConfig->setMidiInput(csv);
	delete csv;

	csv = mPluginInputs->getSelectedCsv();
	mConfig->setPluginMidiInput(csv);
	delete csv;

	csv = mOutputs->getSelectedCsv();
	mConfig->setMidiOutput(csv);
	delete csv;

	csv = mPluginOutputs->getSelectedCsv();
	mConfig->setPluginMidiOutput(csv);
	delete csv;

	return true;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

