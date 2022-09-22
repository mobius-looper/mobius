/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Dialog for specification of computer keyboard bindings.
 * This is the new style that saves bindings in the MobuisConfig
 * rather than the UIConfig.
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
// Utilities
//
//////////////////////////////////////////////////////////////////////

/**
 * Render the key code as a meaningful string.
 * There is also one of these in qwin.h which is actually 
 * what is called by Character::getString, don't think we need the
 * extra layer...
 */
void GetKeyString2(int code, char* buffer)
{
    if (code == 0) {
        // this can't be bound  
        strcpy(buffer, "");
    }
    else {
        Character::getString(code, buffer);
        if (strlen(buffer) == 0)
          sprintf(buffer, "%d", code);
    }
}

//////////////////////////////////////////////////////////////////////
//
// KeyBindingTableModel
//
//////////////////////////////////////////////////////////////////////

class KeyBindingTableModel : public BindingTableModel {
  public:

    int getColumnCount();
    int getColumnPreferredWidth(int index);
    const char* getColumnName(int index);
    const char* getCellText(int row, int column);

  private:

};

int KeyBindingTableModel::getColumnCount()
{
    return 3;
}

int KeyBindingTableModel::getColumnPreferredWidth(int index)
{
    int width = 20;

    switch (index) {
        case 0: width = 30; break;
        case 1: width = 15; break;
        case 2: width = 10; break;
    }
    return width;
}

const char* KeyBindingTableModel::getColumnName(int index)
{
    const char* name = "???";

    switch (index) {
        case 0: name = "Target";  break;
        case 1: name = "Key"; break;
        case 2: name = "Arguments"; break;
    }
    return name;
}

const char* KeyBindingTableModel::getCellText(int row, int column)
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
                    GetKeyString2(b->getValue(), buffer);
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

//////////////////////////////////////////////////////////////////////
//
// KeyBindingDialog
//
//////////////////////////////////////////////////////////////////////

PUBLIC KeyBindingDialog::KeyBindingDialog(Window* parent, 
                                          UI* ui,
                                          MobiusInterface* mobius, 
                                          MobiusConfig* config)
{
	mKeyCapture = NULL;
	mKey = NULL;

    init(parent, ui, mobius, config);
}

PUBLIC KeyBindingDialog::~KeyBindingDialog()
{
}

/**
 * Get the title for the window.  Intended to be overloaded
 * by subclasses.
 */
PUBLIC const char* KeyBindingDialog::getDialogTitle()
{
	return "Key Bindings";
}

PUBLIC const char* KeyBindingDialog::getBindingsPanelLabel()
{
    
	return "Key Bindings";
}

PUBLIC bool KeyBindingDialog::isMultipleConfigurations()
{
    return false;
}

PUBLIC bool KeyBindingDialog::isUpdateButton()
{
    return true;
}

PUBLIC BindingTableModel* KeyBindingDialog::newTableModel()
{
    return new KeyBindingTableModel();
}

//////////////////////////////////////////////////////////////////////
//
// Trigger Edit Fields
//
//////////////////////////////////////////////////////////////////////

/**
 * Add binding-specific target components to the target form.
 */
void KeyBindingDialog::addTriggerComponents(FormPanel* form)
{
    MessageCatalog* cat = mMobius->getMessageCatalog();

    Panel* keystuff = new Panel();
    keystuff->setLayout(new HorizontalLayout(10));
    form->add(cat->get(MSG_DLG_KEY_KEY), keystuff);

    mKey = new Text();
    mKey->setColumns(15);
    keystuff->add(mKey);

    //mKeyCapture = new Checkbox(cat->get(MSG_DLG_KEY_CAPTURE));
    mKeyCapture = new Checkbox("Capture");
    mKeyCapture->addActionListener(this);
    mKeyCapture->setValue(false);
    keystuff->add(mKeyCapture);

    addKeyListener(this);
    setFocusRequested(true);
}

/**
 * Update the currently selected binding based on the current
 * values of the editing fields.
 */
void KeyBindingDialog::updateBinding(Binding* b)
{
    int key = Character::getCode(mKey->getValue());
    b->setValue(key);

    BindingDialog::updateBinding(b);
}

/**
 * Refresh editing fields to reflect the currently selected binding.
 * May be overloaded in the subclass to refresh binding-specific fields,
 * if so it must call back up to this one.
 */
void KeyBindingDialog::refreshFields() 
{
    BindingDefinition* def = getSelectedBinding();
    Binding* b = (def != NULL) ? def->getBinding() : NULL;

    if (b == NULL) {
        mKey->setValue(NULL);
    }
    else {
        char buffer[128];
        GetKeyString2(b->getValue(), buffer);
        mKey->setValue(buffer);
    }

    BindingDialog::refreshFields();
}

PUBLIC void KeyBindingDialog::actionPerformed(void* c)
{
    if (c == mKeyCapture) {
        // Disable selection of the text field during capture, otherwise
        // the keys can end up being inserted here along with the 
        // auto-generated name.
        mKey->setEnabled(!mKeyCapture->isSelected());
    }

    BindingDialog::actionPerformed(c);
}

//////////////////////////////////////////////////////////////////////
//
// Binding Filter
//
//////////////////////////////////////////////////////////////////////

PUBLIC List* KeyBindingDialog::getRelevantBindings(BindingConfig* config)
{
    List* bindings = new List();
    if (config != NULL) {
        for (Binding* b = config->getBindings() ; b != NULL ; 
             b = b->getNext()) {
            if (b->getTrigger() == TriggerKey) {
                BindingDefinition* def = newBindingDefinition(b);
                bindings->add(def);
            }
        }
    }
    return bindings;
}

PUBLIC Binding* KeyBindingDialog::newBinding()
{
    Binding* b = new Binding();
    b->setTrigger(TriggerKey);
    return b;
}

//////////////////////////////////////////////////////////////////////
//
// Commit
//
//////////////////////////////////////////////////////////////////////

/**
 * Put all the key bindings in a contiguous area within the binding list.
 */
PUBLIC void KeyBindingDialog::prepareCommit()
{
    BindingConfig* edited = mConfig->getBindingConfigs();
    if (edited == NULL) {
        // can't happen
        Trace(1, "No BindingConfig to commit!\n");
    }
    else {
        Binding* bindings = edited->getBindings();
		List* keys = new List();
		List* others = new List();

        for (Binding* b = bindings ; b != NULL ; b = b->getNext()) {
            if (b->getTrigger() == TriggerKey)
			  keys->add(b);
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

		for (int i = 0 ; i < keys->size() ; i++) {
			Binding* b = (Binding*)keys->get(i);
			b->setNext(NULL);
			if (prev == NULL)
			  bindings = b;
			else
			  prev->setNext(b);
            prev = b;
		}
		edited->setBindings(bindings);
	}
}

//////////////////////////////////////////////////////////////////////
//
// KeyListener
//
//////////////////////////////////////////////////////////////////////

PUBLIC void KeyBindingDialog::keyPressed(KeyEvent* e) 
{
	if (mKeyCapture->isSelected()) {

        // ignore events for the modifier keys themselves
        // ?? should these even generate events?
        // also ignore the toggle keys
        
        if (!e->isModifier() && !e->isToggle()) {

			char buffer[128];
			GetKeyString2(e->getFullKeyCode(), buffer);
			mKey->setValue(buffer);
		}
	}
}

PUBLIC void KeyBindingDialog::keyReleased(KeyEvent* e)
{
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
