/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Dialog for the selection of display components.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "Util.h"
#include "MessageCatalog.h"
#include "Qwin.h"

#include "Messages.h"
#include "Parameter.h"

#include "UIConfig.h"
#include "UITypes.h"
#include "UI.h"

PUBLIC DisplayDialog::DisplayDialog(Window* parent, MobiusInterface* mob, UIConfig* c)
{
	int i;

	MessageCatalog* cat = mob->getMessageCatalog();

	setParent(parent);

	// !! setting this non-modal causes crashes deep in the window proc
	// need to figure out why
	setModal(true);

	setIcon("Mobius");
	setTitle(cat->get(MSG_DLG_DISPLAY_TITLE));
	setInsets(20, 20, 20, 0);

    mMobius = mob;
	mConfig = c;

	Panel* root = getPanel();
	TabbedPane* tabs = new TabbedPane();
	root->add(tabs);

	Panel* main = new Panel("Main");
	main->setLayout(new VerticalLayout(10));
	tabs->add(main);

	mSelector = new MultiSelect(true);
	mSelector->setColumns(20);
	mSelector->setRows(7);
	mSelector->addActionListener(this);

    StringList* allowed = new StringList();
    StringList* selected = new StringList();

    for (i = 0 ; SpaceElements[i] != NULL ; i++) {
        DisplayElement *def = SpaceElements[i];
        allowed->add(def->getDisplayName());
        Location* l = mConfig->getLocation(def->getName());
        if (l != NULL && !l->isDisabled())
          selected->add(def->getDisplayName());
    }

    mSelector->setAllowedValues(allowed);
    mSelector->setValues(selected);

	main->add(new Label(cat->get(MSG_DLG_DISPLAY_COMPONENTS)));
    main->add(mSelector);

	mParameters = new MultiSelect(true);
	mParameters->setColumns(20);
	mParameters->setRows(7);
	mParameters->addActionListener(this);

    allowed = new StringList();
    selected = new StringList();

	// all bindable parameters are displayable
    for (i = 0 ; Parameters[i] != NULL ; i++) {
        Parameter* p = Parameters[i];
		if (p->bindable)
          allowed->add(p->getDisplayName());
    }

    // filter deprecated or invalid values out of the selected list
	StringList* current = mConfig->getParameters();
    if (current != NULL) {
        for (int i = 0 ; i < current->size() ; i++) {
            const char* s = current->getString(i);
            Parameter* p = mob->getParameter(s);
            if (p != NULL && allowed->contains(p->getDisplayName()))
              selected->add(p->getDisplayName());
        }
    }

    allowed->sort();
    mParameters->setAllowedValues(allowed);
    mParameters->setValues(selected);

	main->add(new Label(cat->get(MSG_DLG_DISPLAY_PARAMS)));
    main->add(mParameters);

	Panel* strip = new Panel("Track Strips");
	strip->setLayout(new VerticalLayout(10));
	tabs->add(strip);

	mFloatingStrip = new MultiSelect(true);
    buildControlSelector(mFloatingStrip, mConfig->getFloatingStrip());
	strip->add(new Label(cat->get(MSG_DLG_DISPLAY_FLOATING_STRIP)));
    strip->add(mFloatingStrip);

	mFloatingStrip2 = new MultiSelect(true);
    buildControlSelector(mFloatingStrip2, mConfig->getFloatingStrip2());
    // too may keys for the same thing, just use this...
	strip->add(new Label(cat->get(MSG_DLG_DISPLAY_FLOATING_STRIP2)));
    strip->add(mFloatingStrip2);

	mDockedStrip = new MultiSelect(true);
    buildControlSelector(mDockedStrip, mConfig->getDockedStrip());
	strip->add(new Label(cat->get(MSG_DLG_DISPLAY_DOCKED_STRIP)));
    strip->add(mDockedStrip);

}

PRIVATE void DisplayDialog::buildControlSelector(MultiSelect* ms, 
                                                 StringList* current)
{
	ms->setColumns(20);
	ms->setRows(7);
	ms->addActionListener(this);

    StringList* allowed = new StringList();
	StringList* selected = new StringList();
	
    for (int i = 0 ; TrackStripElements[i] != NULL ; i++) {
        DisplayElement* el = TrackStripElements[i];
        allowed->add(el->getDisplayName());
	}

	// note that we need to iterate over the current values to preserve order
	if (current != NULL) {
		for (int i = 0 ; i < current->size() ; i++) {
			const char* name = current->getString(i);
			DisplayElement* el = DisplayElement::get(name);
			if (el != NULL)
			  selected->add(el->getDisplayName());
		}
	}

    ms->setAllowedValues(allowed);
    ms->setValues(selected);
}

PUBLIC DisplayDialog::~DisplayDialog()
{
}

void DisplayDialog::actionPerformed(void *c) 
{
    SimpleDialog::actionPerformed(c);
}

bool DisplayDialog::commit()
{
    // first disable all components
    List* locations = mConfig->getLocations();
    if (locations != NULL) {
        for (int i = 0 ; i < locations->size() ; i++) {
            Location* l = (Location*)locations->get(i);
            l->setDisabled(true);
        }
    }

    StringList* selected = mSelector->getValues();
    if (selected != NULL) {
        for (int i = 0 ; i < selected->size() ; i++) {
            const char* dname = selected->getString(i);
            DisplayElement* e = getDisplayElement(dname);
            if (e != NULL) {
                Location* l = mConfig->getLocation(e->getName());
                if (l != NULL)
                  l->setDisabled(false);
                else {
                    // haven't been displaying this
                    l = new Location(e->getName());
                    mConfig->addLocation(l);
                }
            }
        }
    }

	StringList* parameters = NULL;
	selected = mParameters->getValues();
	if (selected != NULL) {
        for (int i = 0 ; i < selected->size() ; i++) {
            const char* dname = selected->getString(i);
			Parameter* p = mMobius->getParameterWithDisplayName(dname);
			if (p != NULL) {
				if (parameters == NULL)
				  parameters = new StringList();
				parameters->add(p->getName());
			}
		}
	}
	mConfig->setParameters(parameters);

    mConfig->setFloatingStrip(convertControls(mFloatingStrip->getValues()));
    mConfig->setFloatingStrip2(convertControls(mFloatingStrip2->getValues()));
    mConfig->setDockedStrip(convertControls(mDockedStrip->getValues()));

    return true;
}

/**
 * Convert a list of DisplayElement display names into a list
 * of internal names.
 */
PRIVATE StringList* DisplayDialog::convertControls(StringList* selected)
{
    StringList* controls = NULL;
	if (selected != NULL) {
        for (int i = 0 ; i < selected->size() ; i++) {
            const char* dname = selected->getString(i);
			DisplayElement* el = DisplayElement::getWithDisplayName(TrackStripElements, dname);
			if (el != NULL) {
				if (controls == NULL)
				  controls = new StringList();
				controls->add(el->getName());
			}
		}
	}
    return controls;
}

/**
 * Locate a DisplayElement definition by display name.
 */
DisplayElement* DisplayDialog::getDisplayElement(const char *dname)
{
    return DisplayElement::getWithDisplayName(SpaceElements, dname);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
