/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Functions to send messages to the UI.
 *
 * I really hate this but we don't have a way right now to let
 * the UI register functions like we do UIControls.  Need to think
 * a lot more about generalizing the UI target interface, could
 * be a general thing like plugin parameters.
 *
 */

#include <stdio.h>
#include <memory.h>

#include "Action.h"
#include "Event.h"
#include "Function.h"
#include "Layer.h"
#include "Loop.h"
#include "Messages.h"
#include "Mobius.h"
#include "Mode.h"

//////////////////////////////////////////////////////////////////////
//
// UI Functions
//
//////////////////////////////////////////////////////////////////////

typedef enum {

    UI_REDRAW

} UIFunctionType;

class UIFunction : public Function {
  public:
	UIFunction(UIFunctionType type);
	void invoke(Action* action, Mobius* m);
  private:
    UIFunctionType mType;
};

PUBLIC Function* UIRedraw = new UIFunction(UI_REDRAW);


PUBLIC UIFunction::UIFunction(UIFunctionType type)
{
    mType = type;
	global = true;

	if (mType == UI_REDRAW) {
		setName("UIRedraw");
        setKey(MSG_UI_CMD_REDRAW);
	}
    else {
        setName("???");
    }
}

PUBLIC void UIFunction::invoke(Action* action, Mobius* m)
{
	if (action->down) {
		trace(action, m);

        if (mType == UI_REDRAW) {
            MobiusListener* l = m->getListener();
            l->MobiusRedraw();
        }
	}
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
