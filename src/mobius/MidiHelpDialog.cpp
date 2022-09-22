/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Dialog that displays the current MIDI bindings.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "Util.h"
#include "MessageCatalog.h"
#include "MidiUtil.h"
#include "Qwin.h"

#include "Action.h"
#include "Function.h"
#include "Messages.h"
#include "Mobius.h"
#include "MobiusConfig.h"

#include "UI.h"

#define MAX_ROWS 30
#define MAX_COLUMNS 4

PUBLIC MidiHelpDialog::MidiHelpDialog(Window* parent, MobiusInterface* mob)
{
	MessageCatalog* cat = mob->getMessageCatalog();

	setParent(parent);
	setModal(false);
	setIcon("Mobius");
	setTitle(cat->get(MSG_DLG_HELP_MIDI));
	setInsets(20, 20, 20, 0);

	Panel* root = getPanel();
	root->setLayout(new HorizontalLayout(20));

    mForm = NULL;
    mRow = 0;
    mColumn = 0;

    MobiusConfig* config = mob->getConfiguration();
    addBindings(mob, config->getBaseBindingConfig());
    addBindings(mob, config->getOverlayBindingConfig());

}

PRIVATE void MidiHelpDialog::addBindings(MobiusInterface* mob, 
                                         BindingConfig* config)
{
    if (config != NULL) {
        Binding* bindings = config->getBindings();

        for (Binding* b = config->getBindings() ; b != NULL ; 
             b = b->getNext()) {

            if (b->isMidi()) {

                if (mForm == NULL) {
                    mForm = new FormPanel();
                    mForm->setHorizontalGap(20);
                    getPanel()->add(mForm);
                    mRow = 0;
                }

                // resolve the target to get the accurate names
                ResolvedTarget* rt = mob->resolveTarget(b);
                if (rt != NULL) {
                    char midibuf[128];
                    b->getMidiString(midibuf, true);

                    Label* l = new Label(midibuf);
                    l->setForeground(Color::Red);
						
                    char namebuf[1024];
                    rt->getFullName(namebuf, sizeof(namebuf));
                    if (b->getArgs() != NULL) {
                        AppendString(" ", namebuf, sizeof(namebuf));
                        AppendString(b->getArgs(), namebuf, sizeof(namebuf));
                    }

                    mForm->add(namebuf, l);

                    mRow++;
                    if (mRow == MAX_ROWS) {
                        mForm = NULL;
                        mColumn++;
                        if (mColumn >= MAX_COLUMNS)
                          break;
                    }
                }
            }
        }
    }
}

PUBLIC const char* MidiHelpDialog::getCancelName()
{
	return NULL;
}

PUBLIC MidiHelpDialog::~MidiHelpDialog()
{
}
