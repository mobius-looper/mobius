/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Dialog for specification of UI Button bindings.
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
// ButtonBindingDialog
//
//////////////////////////////////////////////////////////////////////

PUBLIC ButtonBindingDialog::ButtonBindingDialog(Window* parent, 
                                                UI* ui,
                                                MobiusInterface* mobius,
                                                MobiusConfig* config)
{
    init(parent, ui, mobius, config);
}

PUBLIC ButtonBindingDialog::~ButtonBindingDialog()
{
}

/**
 * Get the title for the window.  Intended to be overloaded
 * by subclasses.
 */
PUBLIC const char* ButtonBindingDialog::getDialogTitle()
{
    // figure out how to use this key...
	//setTitle(cat->get(MSG_DLG_BUTTON_TITLE));
	return "Buttons";
}

PUBLIC const char* ButtonBindingDialog::getBindingsPanelLabel()
{
    
	return "Buttons";
}

PUBLIC bool ButtonBindingDialog::isMultipleConfigurations()
{
    return false;
}

PUBLIC bool ButtonBindingDialog::isUpdateButton()
{
    return true;
}

//////////////////////////////////////////////////////////////////////
//
// Trigger Edit Fields
//
//////////////////////////////////////////////////////////////////////

void ButtonBindingDialog::addTriggerComponents(FormPanel* form)
{
}

void ButtonBindingDialog::updateBinding(Binding* b)
{
    BindingDialog::updateBinding(b);
}

//////////////////////////////////////////////////////////////////////
//
// Binding Filter
//
//////////////////////////////////////////////////////////////////////

PUBLIC List* ButtonBindingDialog::getRelevantBindings(BindingConfig* config)
{
    List* bindings = new List();
    if (config != NULL) {
        for (Binding* b = config->getBindings() ; b != NULL ; 
             b = b->getNext()) {
            if (b->getTrigger() == TriggerUI) {
                BindingDefinition* def = newBindingDefinition(b);
                bindings->add(def);
            }
        }
    }
    return bindings;
}

PUBLIC Binding* ButtonBindingDialog::newBinding()
{
    Binding* b = new Binding();
    b->setTrigger(TriggerUI);
    return b;
}

//////////////////////////////////////////////////////////////////////
//
// Commit
//
//////////////////////////////////////////////////////////////////////

/**
 * Compare the old host parameter list with the new ones and try
 * to preserve previously assigned numberes.  Assign new numbers
 * as necessary.
 *
 * Only the default configuration (the first on the list)
 * is relevant here.
 */
PUBLIC void ButtonBindingDialog::prepareCommit()
{
    BindingConfig* edited = mConfig->getBindingConfigs();
    if (edited == NULL) {
        // can't happen
        Trace(1, "No BindingConfig to commit!\n");
    }
    else {
        // go back to the master configuration so we can preserve ids
        // could have saved another copy of it but it's okay since 
        // we don't have to worry about concurrent editing windows
        MobiusConfig* mc = mMobius->getConfiguration();
        BindingConfig* original = mc->getBindingConfigs();
        Binding* newlist = edited->getBindings();

        // first make sure all new ids are reset so we can get
        // a reliable reallocation
        for (Binding* b = newlist ; b != NULL ; b = b->getNext()) {
            if (b->getTrigger() == TriggerUI)
              b->setValue(-1);
        }

        // transfer original ids over and rember the maximum
        int maxid = -1;
        if (original != NULL) {
            for (Binding* orig = original->getBindings() ; orig != NULL ; 
                 orig = orig->getNext()) {
                if (orig->getTrigger() == TriggerUI) {
                    int id = orig->getValue();
                    if (id > maxid) maxid = id;
                    Binding* neu = getBinding(newlist, orig);
                    if (neu != NULL)
                      neu->setValue(id);
                }
            }
        }

        // now assign ids to the new ones
        for (Binding* b = newlist ; b != NULL ; b = b->getNext()) {
            if (b->getTrigger() == TriggerUI) {
                if (b->getValue() == -1) {
                    b->setValue(++maxid);
                }
            }
        }
    }
}

/**
 * Helper for prepareCommit, locate a binding in a list that matches
 * another binding.
 *
 * Channel isn't relevant here, ignore value since that's what
 * we're trynig to transfer.
 */
PRIVATE Binding* ButtonBindingDialog::getBinding(Binding* list, Binding* orig)
{
    Binding* match = NULL;

    if (list != NULL && orig != NULL) {
        for (Binding* b = list ; b != NULL ; b = b->getNext()) {

            if (b->getTrigger() == orig->getTrigger() &&
                b->getTarget() == orig->getTarget() &&
                StringEqual(b->getName(), orig->getName()) &&
                StringEqual(b->getScope(), orig->getScope()) &&
                StringEqual(b->getArgs(), orig->getArgs())) {

                match = b;
                break;
            }
        }
    }

    return match;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
