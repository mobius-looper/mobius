/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Checkpoints in the layer list.
 *
 */

#include <stdio.h>
#include <memory.h>
#include <string.h>

#include "Action.h"
#include "Event.h"
#include "Function.h"
#include "Layer.h"
#include "Loop.h"
#include "Messages.h"
#include "Mode.h"

/****************************************************************************
 *                                                                          *
 *   							  CHECKPOINT                                *
 *                                                                          *
 ****************************************************************************/

/**
 * When we implement this what should scheduleSwitchStack do?
 *   - stack the checkpoint for the first layer in the next loop
 *   - checkpoint the layer in the current loop we're leaving
 */
class CheckpointFunction : public Function {
  public:
	CheckpointFunction();
	Event* invoke(Action* action, Loop* l);
  private:
};

PUBLIC Function* Checkpoint = new CheckpointFunction();

PUBLIC CheckpointFunction::CheckpointFunction() :
    Function("Checkpoint", MSG_FUNC_CHECKPOINT)
{
	mayCancelMute = true;
}

/**
 * Don't need an event since this doesn't affect recording?
 * Mark the current layer and collapse on the next getState call.
 * Note that due to the possibility of segment flattening, we don't 
 * actually remove any layers, though we should if we can ensure that
 * we don't need any of the backing layers.
 *
 */
PUBLIC Event* CheckpointFunction::invoke(Action* action, Loop* loop)
{
	Layer* l = loop->getRecordLayer();
	if (l != NULL) {
		// shift any pending change to the play layer
		loop->shift(true);
		l = loop->getPlayLayer();
		if (l != NULL) {
			if (l->getCheckpoint() == CHECKPOINT_ON)
			  l->setCheckpoint(CHECKPOINT_OFF);
			else
			  l->setCheckpoint(CHECKPOINT_ON);
		}
	}

	return NULL;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
