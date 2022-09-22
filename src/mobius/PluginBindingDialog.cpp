/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Dialog for specification of VST and AU plugin parameters.
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
// PluginBindingDialog
//
//////////////////////////////////////////////////////////////////////

PUBLIC PluginBindingDialog::PluginBindingDialog(Window* parent, 
                                                UI* ui,
                                                MobiusInterface* mobius,
                                                MobiusConfig* config)
{
    init(parent, ui, mobius, config);
}

PUBLIC PluginBindingDialog::~PluginBindingDialog()
{
}

/**
 * Get the title for the window.  Intended to be overloaded
 * by subclasses.
 */
PUBLIC const char* PluginBindingDialog::getDialogTitle()
{
    
	return "Plugin Parameters";
}

PUBLIC const char* PluginBindingDialog::getBindingsPanelLabel()
{
    
	return "Plugin Parameters";
}

PUBLIC bool PluginBindingDialog::isMultipleConfigurations()
{
    return false;
}

PUBLIC bool PluginBindingDialog::isUpdateButton()
{
    return false;
}

//////////////////////////////////////////////////////////////////////
//
// Trigger Edit Fields
//
//////////////////////////////////////////////////////////////////////

void PluginBindingDialog::addTriggerComponents(FormPanel* form)
{
}

void PluginBindingDialog::updateBinding(Binding* b)
{
    BindingDialog::updateBinding(b);
}

//////////////////////////////////////////////////////////////////////
//
// Binding Filter
//
//////////////////////////////////////////////////////////////////////

PUBLIC List* PluginBindingDialog::getRelevantBindings(BindingConfig* config)
{
    List* bindings = new List();
    if (config != NULL) {
        for (Binding* b = config->getBindings() ; b != NULL ; 
             b = b->getNext()) {
            if (b->getTrigger() == TriggerHost) {
                BindingDefinition* def = newBindingDefinition(b);
                bindings->add(def);
            }
        }
    }
    return bindings;
}

PUBLIC Binding* PluginBindingDialog::newBinding()
{
    Binding* b = new Binding();
    b->setTrigger(TriggerHost);
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
PUBLIC void PluginBindingDialog::prepareCommit()
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
            if (b->getTrigger() == TriggerHost)
              b->setValue(-1);
        }

        // transfer original ids over and rember the maximum
        int maxid = -1;
        if (original != NULL) {
            for (Binding* orig = original->getBindings() ; orig != NULL ; 
                 orig = orig->getNext()) {
                if (orig->getTrigger() == TriggerHost) {
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
            if (b->getTrigger() == TriggerHost) {
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
PRIVATE Binding* PluginBindingDialog::getBinding(Binding* list, Binding* orig)
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
