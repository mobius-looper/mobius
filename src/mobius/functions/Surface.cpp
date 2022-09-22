/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Surface control functions.
 *
 * This is still experimental, think about evolving this into a more
 * general communication interface for scripts and ControlSurfaces.
 */

#include <stdio.h>
#include <string.h>
#include <memory.h>

#include "Action.h"
#include "ControlSurface.h"
#include "Event.h"
#include "Function.h"
#include "Layer.h"
#include "Loop.h"
#include "Mobius.h"

/****************************************************************************
 *                                                                          *
 *                                 LAUNCHPAD                                *
 *                                                                          *
 ****************************************************************************/

class SurfaceFunction : public Function {
  public:
	SurfaceFunction();
    void invoke(Action* action, Mobius* m);
  private:
};

// event though it is hidden, don't conflict with the ControlSurface
// subclass name just in case we need to make it public
PUBLIC Function* Surface = new SurfaceFunction();

PUBLIC SurfaceFunction::SurfaceFunction() :
    Function("Surface", 0)
{
	noFocusLock = true;
	scriptOnly = true;
	global = true;

    // not sure if this is required, but since we're not
    // operating on Mobius state it should be okay?
    outsideInterrupt = true;

	// this keeps localize from complaining about a missing key
	externalName = true;

    // get args conveyed as an ExValue
    expressionArgs = false;
}

PUBLIC void SurfaceFunction::invoke(Action* action, Mobius* m)
{
    ControlSurface* cs = m->getControlSurfaces();

    // TODO: if we have more than one, should we search for the launchpad
    // or should we just send the args to all of them and let them figure
    // it out?

    for ( ; cs != NULL ; cs = cs->getNext())
      cs->scriptInvoke(action);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
