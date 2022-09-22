/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Dialog that displays the current key bindings.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "Util.h"
#include "MessageCatalog.h"
#include "Qwin.h"

#include "Action.h"
#include "Messages.h"
#include "MobiusConfig.h"

#include "UIConfig.h"
#include "UI.h"

#define MAX_ROWS 30
#define MAX_COLS 4

PUBLIC KeyHelpDialog::KeyHelpDialog(Window* parent, MobiusInterface* mob)
{
	MessageCatalog* cat = mob->getMessageCatalog();

	setParent(parent);
	setModal(false);
	setIcon("Mobius");
	setTitle(cat->get(MSG_DLG_HELP_KEYS));
	setInsets(20, 20, 20, 0);

    // If we work from the BindingConfig then we have to do our
    // own resolution and potentially merge the base and selected
    // BindingConfigs.  If we worked from the BindignResolver we could
    // skip that...
    MobiusConfig* config = mob->getConfiguration();
    BindingConfig* bconfig = config->getBaseBindingConfig();
    Binding* bindings = bconfig->getBindings();

	Panel* root = getPanel();
	root->setLayout(new HorizontalLayout(20));

    int total = 0;
    for (Binding* b = bindings ; b != NULL ; b = b->getNext()) {
        if (b->getTrigger() == TriggerKey)
          total++;
    }

    int columns = total / MAX_ROWS;

    // Shouldn't have this many but just in case keep the
    // dialog from exploding.
    // !! Sort these in some logical way.

    if (columns < MAX_COLS) {
        FormPanel* form = NULL;
        int row = 0;
        for (Binding* b = bindings ; b != NULL ; b = b->getNext()) {
            if (b->getTrigger() == TriggerKey) {

                if (form == NULL) {
                    form = new FormPanel();
                    form->setHorizontalGap(20);
                    root->add(form);
                    row = 0;
                }

                // resolve to get accurate names
                ResolvedTarget* rt = mob->resolveTarget(b);
                if (rt != NULL) {
                    char keybuf[128];
                    b->getKeyString(keybuf, sizeof(keybuf));

                    Label* l = new Label(keybuf);
                    l->setForeground(Color::Red);

                    char namebuf[1024];
                    rt->getFullName(namebuf, sizeof(namebuf));
                    if (b->getArgs() != NULL) {
                        AppendString(" ", namebuf, sizeof(namebuf));
                        AppendString(b->getArgs(), namebuf, sizeof(namebuf));
                    }

                    form->add(namebuf, l);

                    row++;
                    if (row == MAX_ROWS)
                      form = NULL;
                }
            }
        }
    }
}

PUBLIC const char* KeyHelpDialog::getCancelName()
{
	return NULL;
}

PUBLIC KeyHelpDialog::~KeyHelpDialog()
{
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
