/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * An abstract class all control surface interface classes
 * should extend.  Not much here now but it we'll need this as soon
 * as we support another surface like the APC40.
 *
 * The fundamental assumption is that a control surface is a
 * non-programmable device that sends and receives a fixed set
 * of MIDI events.  At least some of the MIDI events sent by the
 * surface pass through an extra level of mapping before they are
 * bound to Mobius targets.  This allows for "virtual pages" of bindings
 * to be applied to controls.  The effect is similar to switching
 * between BindingConfigs.
 *
 * Control surface events take precidence over normal MIDI bindings
 * there are two ways this could work:
 *
 *   - have the surface insert Actions into the BindingResolver
 *     that redirect the event to the surface handler
 *
 *   - pass incomming events through the surface handler and then
 *     if it doesn't want them send them through the BindingResolver
 *
 * The second is simpler to set up so we'll go with that for awhile.
 * During Mobius startup there should be some kind of warning if a 
 * surface overrides a MIDI binding.
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#include "Util.h"
#include "MidiByte.h"
#include "MidiEvent.h"

#include "ControlSurface.h"


PUBLIC ControlSurface::ControlSurface()
{
    mNext = NULL;
}

PUBLIC ControlSurface::~ControlSurface()
{
    ControlSurface* next = NULL;
    
    for (ControlSurface* el = mNext ; el != NULL ; el = next) {
        next = el->getNext();
        el->setNext(NULL);
        delete el;
    }
}

PUBLIC void ControlSurface::setNext(ControlSurface* c)
{
    mNext = c;
}

PUBLIC ControlSurface* ControlSurface::getNext()
{
    return mNext;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
