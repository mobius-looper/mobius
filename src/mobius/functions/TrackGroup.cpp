/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Track group assignment.
 *
 */

#include <stdio.h>
#include <string.h>
#include <memory.h>

#include "Action.h"
#include "Event.h"
#include "Function.h"
#include "Loop.h"
#include "Messages.h"
#include "Mobius.h"
#include "MobiusConfig.h"
#include "Track.h"

/****************************************************************************
 *                                                                          *
 *                                TRACK GROUP                               *
 *                                                                          *
 ****************************************************************************/

class TrackGroupFunction : public Function {
  public:
	TrackGroupFunction();
	Event* invoke(Action* action, Loop* l);
	void invokeLong(Action* action, Loop* l);
};

PUBLIC Function* TrackGroup = new TrackGroupFunction();

PUBLIC TrackGroupFunction::TrackGroupFunction() :
    Function("TrackGroup", MSG_FUNC_TRACK_GROUP)
{
	longPressable = true;
}

PUBLIC void TrackGroupFunction::invokeLong(Action* action, Loop* l)
{
    Track* t = l->getTrack();

	t->setGroup(0);
}

PUBLIC Event* TrackGroupFunction::invoke(Action* action, Loop* l)
{
    Track* t = l->getTrack();
    Mobius* m = l->getMobius();
    MobiusConfig* config = m->getConfiguration();
    int groupCount = config->getTrackGroups();
    int group = -1;

    // Allow numbers from 1 but since we show them as letters allow those too
    if (action->arg.getType() == EX_INT) {
        int g = action->arg.getInt();
        if (g >= 0 && g <= groupCount)
          group = g;
    }
    else if (action->arg.getType() == EX_STRING) {
        const char* s = action->arg.getString();
        if (strlen(s) == 1) {
            // A is 65. a is 97.
            int letter = (int)s[0];
            // todo should support lower case!
            int offset = 65;
            if (letter >= 97) offset = 97;
            int g = letter - offset;
            if (g >= 0 && g <= groupCount)
              group = g;
        }
    }

    if (group < 0) {
        // cycle throug the available groups
        group = t->getGroup() + 1;
        // wrap back around if it goes (or already was) out of range
        if (group < 0 || group > groupCount)
          group  = 0;
    }
    
	t->setGroup(group);

    return NULL;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
