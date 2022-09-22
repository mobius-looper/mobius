/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * TrackCopy
 *
 */

#include <stdio.h>
#include <string.h>
#include <memory.h>

#include "Action.h"
#include "Event.h"
#include "Function.h"
#include "Layer.h"
#include "Loop.h"
#include "Mobius.h"
#include "Messages.h"
#include "Synchronizer.h"
#include "Track.h"
#include "Mode.h"

/****************************************************************************
 *                                                                          *
 *                                 TRACK COPY                               *
 *                                                                          *
 ****************************************************************************/

class TrackCopyFunction : public Function {
  public:
	TrackCopyFunction(bool timing);
	Event* invoke(Action* action, Loop* l);
  private:
	bool timing;
};

PUBLIC Function* TrackCopy = new TrackCopyFunction(false);
PUBLIC Function* TrackCopyTiming = new TrackCopyFunction(true);

PUBLIC TrackCopyFunction::TrackCopyFunction(bool b)
{
	timing = b;
	noFocusLock = true;
	activeTrack = true;

	if (timing) {
		setName("TrackCopyTiming");
		setKey(MSG_FUNC_TRACK_COPY_TIMING);
	}
	else {
		setName("TrackCopy");
		setKey(MSG_FUNC_TRACK_COPY);
	}
}

PUBLIC Event* TrackCopyFunction::invoke(Action* action, Loop* l)
{
    if (action->down) {
		trace(action, l);
        // any quant on this?  
        // what about undo?

        // which one? assume the one adjacent to the left
        Track* track = l->getTrack();
        int srcIndex = track->getRawNumber() - 1;
        if (srcIndex >= 0) {
            Mobius* mobius = l->getMobius();
            Track* src = mobius->getTrack(srcIndex);

            if (timing)
              l->trackCopyTiming(src);
            else
              l->trackCopySound(src);
        }
    }

	return NULL;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
