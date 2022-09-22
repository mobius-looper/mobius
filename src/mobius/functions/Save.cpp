/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * SaveLoop - a "quick save" of the active loop,.
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

//////////////////////////////////////////////////////////////////////
//
// SaveLoopFunction
//
//////////////////////////////////////////////////////////////////////

class SaveLoopFunction : public Function {
  public:
	SaveLoopFunction();
	void invoke(Action* action, Mobius* m);
  private:
	bool stop;
	bool save;
};

PUBLIC Function* SaveLoop = new SaveLoopFunction();

PUBLIC SaveLoopFunction::SaveLoopFunction() :
    Function("SaveLoop", MSG_FUNC_SAVE_LOOP)
{
	global = true;
	noFocusLock = true;
}

PUBLIC void SaveLoopFunction::invoke(Action* action, Mobius* m)
{
	if (action->down) {
		trace(action, m);
		m->saveLoop(action);
	}
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
