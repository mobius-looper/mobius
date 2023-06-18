/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Dialog for specification of MIDI event bindings.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <vector>
#include <algorithm>

#include "Util.h"
#include "Trace.h"
#include "MessageCatalog.h"
#include "Qwin.h"

#include "MidiByte.h"
#include "MidiEvent.h"
#include "MidiUtil.h"

#include "Binding.h"
#include "Event.h"
#include "Function.h"
#include "Messages.h"
#include "MobiusConfig.h"
#include "Setup.h"

#include "UI.h"

using namespace std;

//////////////////////////////////////////////////////////////////////
//
// Constants
//
//////////////////////////////////////////////////////////////////////

PUBLIC const char* MidiChannelNames[] = {
	"1",
	"2",
	"3",
	"4",
	"5",
	"6",
	"7",
	"8",
	"9",
	"10",
	"11",
	"12",
	"13",
	"14",
	"15",
	"16",
	NULL
};

//////////////////////////////////////////////////////////////////////
//
// MidiBindingTableModel
//
// This is nearly identical to KeyBindingTableModel except for the
// way we render the MIDI event in the second column.
// Opportunity for sharage.
//
//////////////////////////////////////////////////////////////////////

class MidiBindingTableModel : public BindingTableModel {
  public:

    int getColumnCount();
    int getColumnPreferredWidth(int index);
    const char* getColumnName(int index);
    const char* getCellText(int row, int column);

  private:

    void getMidiString(Binding* b, char* buffer);

};

int MidiBindingTableModel::getColumnCount()
{
    return 3;
}

int MidiBindingTableModel::getColumnPreferredWidth(int index)
{
    int width = 20;

    switch (index) {
        case 0: width = 30; break;
        case 1: width = 20; break;
        case 2: width = 20; break;
    }
    return width;
}

const char* MidiBindingTableModel::getColumnName(int index)
{
    const char* name = "???";

    switch (index) {
        case 0: name = "Target";  break;
        case 1: name = "Trigger"; break;
        case 2: name = "Arguments"; break;
    }
    return name;
}

const char* MidiBindingTableModel::getCellText(int row, int column)
{
    const char* text = NULL;
    static char buffer[128];

    if (mBindings != NULL) {
        BindingDefinition* def = (BindingDefinition*)(mBindings->get(row));
        if (def != NULL) {
            Binding* b = def->getBinding();
            switch (column) {
                case 0: 
                    text = def->getName();
                    break;
                case 1: 
                    getMidiString(b, buffer);
                    text = buffer;
                    break;
                case 2:
                    text = b->getArgs();
                    break;
            }
        }
    }
    return text;
}

PRIVATE void MidiBindingTableModel::getMidiString(Binding* b, char* buffer)
{
    Trigger* trig = b->getTrigger();
    int value = b->getValue();

	// we display channel consistenly everywhere as 1-16
	int channel = b->getChannel() + 1;

	strcpy(buffer, "");

    if (value >= 0 && value < 128) {
        if (trig == TriggerNote) {
            // have a midi.h utility to render note name
            char note[128];
			MidiNoteName(value, note);
			sprintf(buffer, "%d:%s", channel, note);
		}
        else if (trig == TriggerControl) {
            sprintf(buffer, "%d:Control %d", channel, value);
        }
        else if (trig == TriggerProgram) {
            sprintf(buffer, "%d:Program %d", channel, value);
        }
        else if (trig == TriggerPitch) {
            sprintf(buffer, "%d:Pitch %d", channel, value);
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
// MidiBindingDialog
//
//////////////////////////////////////////////////////////////////////

PUBLIC MidiBindingDialog::MidiBindingDialog(Window* parent, 
                                            UI* ui,
                                            MobiusInterface* mobius,
                                            MobiusConfig* config)
{
    mTrigger = NULL;
    mChannel = NULL;
    mValue = NULL;
    mMidiCapture = NULL;
    mMidiDisplay = NULL;
    mSaveListener = NULL;

    init(parent, ui, mobius, config);

	// would be nice to have push/pop listener?
	mSaveListener = ui->setMidiEventListener(this);
}

PUBLIC MidiBindingDialog::~MidiBindingDialog()
{
}

PUBLIC void MidiBindingDialog::closing()
{
	mUI->setMidiEventListener(mSaveListener);
	BindingDialog::closing();
}

/**
 * Get the title for the window.  Intended to be overloaded
 * by subclasses.
 */
PUBLIC const char* MidiBindingDialog::getDialogTitle()
{
	return "MIDI Bindings";  // #C? Menu entry is MIDI Control... change that??
}

PUBLIC const char* MidiBindingDialog::getBindingsPanelLabel()
{
    
	return "MIDI Bindings";
}

PUBLIC const char* MidiBindingDialog::getSelectorLabel()
{
    // The old MidiControlDialog used this, should repurpose it
    // return cat->get(MSG_DLG_MIDI_CONTROL_SELECTED);

    return "Active Bindings";
}

PUBLIC bool MidiBindingDialog::isMultipleConfigurations()
{
    return true;
}

PUBLIC bool MidiBindingDialog::isUpdateButton()
{
    return true;
}

PUBLIC BindingTableModel* MidiBindingDialog::newTableModel()
{
    return new MidiBindingTableModel();
}

//////////////////////////////////////////////////////////////////////
//
// Trigger Edit Fields
//
//////////////////////////////////////////////////////////////////////

/**
 * Add binding-specific target components to the target form.
 */
PUBLIC void MidiBindingDialog::addTriggerComponents(FormPanel* form)
{
    MessageCatalog* cat = mMobius->getMessageCatalog();

    mTrigger = new ComboBox();
    mTrigger->setColumns(7);
    mTrigger->addValue(TriggerNote->getDisplayName());
    mTrigger->addValue(TriggerControl->getDisplayName());
    mTrigger->addValue(TriggerProgram->getDisplayName());
    mTrigger->addValue(TriggerPitch->getDisplayName());
    mTrigger->setSelectedIndex(0);
    form->add(cat->get(MSG_DLG_TYPE), mTrigger);

    mChannel = new ComboBox(MidiChannelNames);
    mChannel->setColumns(7);
    form->add(cat->get(MSG_DLG_CHANNEL), mChannel);

    mValue = new NumberField(0, 127);
    mValue->setNullValue(-1);
    mValue->setHideNull(true);
    form->add(cat->get(MSG_DLG_VALUE), mValue);

    // midi capture
    Panel* capture = new Panel();
    capture->setLayout(new HorizontalLayout());
    form->add("", capture);

    mMidiDisplay = new Text();
    mMidiDisplay->setColumns(15);
    mMidiDisplay->setEditable(false);
    capture->add(mMidiDisplay);
    capture->add(new Strut(10, 0));
    mMidiCapture = new Checkbox(cat->get(MSG_DLG_MIDI_CONTROL_CAPTURE));
    mMidiCapture->setValue(false);
    mMidiCapture->addActionListener(this);
    capture->add(mMidiCapture);
}

/**
 * Update the currently selected binding based on the current
 * values of the editing fields.
 */
PUBLIC void MidiBindingDialog::updateBinding(Binding* b)
{
    b->setTrigger(getSelectedTrigger());
    b->setChannel(mChannel->getSelectedIndex());

    // value may be negative to indiciate no selection, let
    // that be set in the binding which will make it invalid
    // and we can filter it
    b->setValue(mValue->getValue());

    // let the superclass handle the common stuff
    BindingDialog::updateBinding(b);
}

Trigger* MidiBindingDialog::getSelectedTrigger()
{
    Trigger* trig = NULL;
    int index = mTrigger->getSelectedIndex();
    switch (index) {
        case 0: trig = TriggerNote; break;
        case 1: trig = TriggerControl; break;
        case 2: trig = TriggerProgram; break;
        case 3: trig = TriggerPitch; break;
    }
    return trig;
}

int MidiBindingDialog::getTriggerIndex(Trigger* trig)
{
	int ordinal = -1;
    if (trig == TriggerNote)
      ordinal = 0;
    else if (trig == TriggerControl)
      ordinal = 1;
    else if (trig == TriggerProgram)
      ordinal = 2;
    else if (trig == TriggerPitch)
      ordinal = 3;
    return ordinal;
}

/**
 * Refresh editing fields to reflect the currently selected binding.
 * May be overloaded in the subclass to refresh binding-specific fields,
 * if so it must call back up to this one.
 */
PUBLIC void MidiBindingDialog::refreshFields() 
{
    BindingDefinition* def = getSelectedBinding();
    Binding* b = (def != NULL) ? def->getBinding() : NULL;

    if (b == NULL) {
        mTrigger->setSelectedIndex(0);	// default to Note
        mChannel->setSelectedIndex(0);
        mValue->setValue(-1);
    }
    else {
        int index = getTriggerIndex(b->getTrigger());
        mTrigger->setSelectedIndex(index);

        mChannel->setSelectedIndex(b->getChannel());
        mValue->setValue(b->getValue());
    }

    // let the superclass handle the common stuff
    BindingDialog::refreshFields();
}

PUBLIC void MidiBindingDialog::actionPerformed(void* c)
{
    BindingDialog::actionPerformed(c);
}

//////////////////////////////////////////////////////////////////////
//
// Binding Filter
//
//////////////////////////////////////////////////////////////////////


PUBLIC List* MidiBindingDialog::getRelevantBindings(BindingConfig* config)
{
    List* bindings = new List();
    if (config != NULL) {
        for (Binding* b = config->getBindings() ; b != NULL ; 
             b = b->getNext()) {
            if (b->isMidi()) {
                BindingDefinition* def = newBindingDefinition(b);
                bindings->add(def);
            }
        }
    }
    return bindings;
}

PUBLIC Binding* MidiBindingDialog::newBinding()
{
    Binding* b = new Binding();

    b->setTrigger(getSelectedTrigger());

    return b;
}


//////////////////////////////////////////////////////////////////////
//
// Commit
//
//////////////////////////////////////////////////////////////////////

/**
 * Put all the MIDI bindings in a contiguous area within the binding list.
 */
PUBLIC void MidiBindingDialog::prepareCommit()
{
    BindingConfig* edited = mConfig->getBindingConfigs();
    if (edited == NULL) {
        // can't happen
        Trace(1, "No BindingConfig to commit!\n");
    }
    else {
        Binding* bindings = edited->getBindings();
		List* midi = new List();
		List* others = new List();

        for (Binding* b = bindings ; b != NULL ; b = b->getNext()) {
            if (b->isMidi())
			  midi->add(b);
			else
			  others->add(b);
        }

		Binding* prev = NULL;
		for (int i = 0 ; i < others->size() ; i++) {
			Binding* b = (Binding*)others->get(i);
			b->setNext(NULL);
			if (prev == NULL)
			  bindings = b;
			else
			  prev->setNext(b);
            prev = b;
		}

		for (int i = 0 ; i < midi->size() ; i++) {
			Binding* b = (Binding*)midi->get(i);
			b->setNext(NULL);
			if (prev == NULL)
			  bindings = b;
			else
			  prev->setNext(b);
            prev = b;
		}
		edited->setBindings(bindings);


        delete midi;
        delete others;
	}
}

//////////////////////////////////////////////////////////////////////
//
// UIMidiEventListener
//
//////////////////////////////////////////////////////////////////////

PUBLIC bool MidiBindingDialog::midiEvent(MidiEvent* e)
{
 	// ignore aftertouch & pressure
 	int status = e->getStatus();
 	if (mMidiDisplay != NULL && 
        status != MS_TOUCH && status != MS_POLYPRESSURE) {

		// always update the MIDI tracker
 		char buffer[128];
 		renderMidi(e, buffer);
 		mMidiDisplay->setText(buffer);

        if (mMidiCapture->isSelected()) {

            Trigger* trigger = NULL;
			int channel = e->getChannel();
			int value = 0;

			if (status == MS_NOTEON) {
                trigger = TriggerNote;
				value = e->getKey();
			}
			else if (status == MS_CONTROL) {
				trigger = TriggerControl;
				value = e->getController();
			}
			else if (status == MS_PROGRAM) {
				trigger = TriggerProgram;
				value = e->getProgram();
			}
			else if (status == MS_BEND) {
				trigger = TriggerPitch;
				value = e->getPitchBend();
			}

			if (trigger != NULL) {
                // remember the first one in the combo box is "none"
                int index = getTriggerIndex(trigger);
                mTrigger->setSelectedIndex(index);
                mChannel->setSelectedIndex(channel);
                // value for bend doesn't matter
                if (status == MS_BEND)
                  mValue->setValue(0);
                else
                  mValue->setValue(value);
            }
		}
	}

	// never allow Mobius to process this
	return false;
}

/**
 * Given an event, print the basic event type and number.
 */
void MidiBindingDialog::renderMidi(MidiEvent* e, char* buffer)
{
	int channel = e->getChannel();
	int key = e->getKey();

	// make the displayed channel match the selector text
	channel++;

	switch (e->getStatus()) {
		case MS_NOTEOFF:
		case MS_NOTEON:
			sprintf(buffer, "Channel %d Note %d", channel, key);
			break;
		case MS_POLYPRESSURE:
			sprintf(buffer, "Channel %d Pressure %d", channel, key);
			break;
		case MS_CONTROL:
			sprintf(buffer, "Channel %d Control %d", channel, key);
			break;
		case MS_PROGRAM:
			sprintf(buffer, "Channel %d Program %d", channel, key);
			break;
		case MS_TOUCH:
			sprintf(buffer, "Channel %d Touch %d", channel, key);
			break;
		case MS_BEND:
			sprintf(buffer, "Channel %d Pitch Bend %d", channel, e->getPitchBend());
			break;
		default:
			sprintf(buffer, "");
			break;
	}
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
