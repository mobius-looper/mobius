/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * A function to confirm a loop switch during Switch mode.
 * When SwitchQuantize is set to one of the confirmation modes,
 * this function will end the confirmation period and start the
 * switch quantization period.  If the loop is in the switch confirmation
 * period it will cause the switch to happen immediately.
 * 
 * This function has no other purpose outside of Switch mode.
 * The EDP does not have this function, instead the Undo function has
 * this behavior during the switch quantization period.  I wanted
 * to have an explicit function for this so we could let Undo behave
 * normally.  
 */

#include <stdio.h>
#include <memory.h>
#include <string.h>

#include "Util.h"

#include "Action.h"
#include "Event.h"
#include "EventManager.h"
#include "Function.h"
#include "Layer.h"
#include "Loop.h"
#include "Track.h"
#include "Stream.h"
#include "Messages.h"
#include "Mode.h"

/****************************************************************************
 *                                                                          *
 *                                  CONFIRM                                 *
 *                                                                          *
 ****************************************************************************/

class ConfirmFunction : public Function {
  public:
	ConfirmFunction();
    Event* invoke(Action* action, Loop* l);
};

PUBLIC Function* Confirm = new ConfirmFunction();

PUBLIC ConfirmFunction::ConfirmFunction() :
    Function("Confirm", MSG_FUNC_CONFIRM)
{
}

Event* ConfirmFunction::invoke(Action* action, Loop* l)
{
    if (action->down) {
        EventManager* em = l->getTrack()->getEventManager();
        Event* switche = em->getSwitchEvent();

        if (switche == NULL) {
            Trace(l, 2, "Ignoring Confirm function outside of Switch mode\n");
        }
        else if (!switche->pending) {
            // Not in confirm mode, supposed to force it out of quantization
            // and switch immediately.  Unfortunately this is rather
            // complcated, we have to adjust the JumpPlay event and
            // maybe a rounding mode ending event.
            Trace(l, 2, "Confirm to cancel switch quantization not implemented\n");
        }
        else {
            // Confirming a switch this way still quantizes the
            // switch frame.  Passing CONFIRM_FRAME_QUANTIZED indicates
            // this, LoopTriggerFunction::confirmEvent will eventuall
            // handle this and do the quantization.
            switche->confirm(action, l, CONFIRM_FRAME_QUANTIZED);
        }
    }

    return NULL;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
