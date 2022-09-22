/*
 * Copyright (c) 2012 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * 
 * A model for defining "watch points" where a Mobius client can
 * be notified when various interesting things happen.
 * 
 */

#include <stdio.h>
#include <string.h>
#include <memory.h>
#include <ctype.h>

#include "MessageCatalog.h"
#include "List.h"

#include "Mobius.h"
#include "Track.h"
#include "Loop.h"

#include "WatchPoint.h"

//////////////////////////////////////////////////////////////////////
//
// Watchers
//
//////////////////////////////////////////////////////////////////////

PUBLIC Watchers::Watchers()
{
    loopLocation = new List();
    loopStart = new List();
    loopSubcycle = new List();
    loopCycle = new List();
    modeRecord = new List();
}

PUBLIC Watchers::~Watchers()
{
    delete loopLocation;
    delete loopStart;
    delete loopSubcycle;
    delete loopCycle;
    delete modeRecord;
}

//////////////////////////////////////////////////////////////////////
//
// WatchPointListener
//
//////////////////////////////////////////////////////////////////////

PUBLIC WatchPointListener::WatchPointListener()
{
    mRemoving = false;
}

PUBLIC WatchPointListener::~WatchPointListener()
{
    // we're virtual, subclass needs to clean itself up
}

PUBLIC void WatchPointListener::remove()
{
    mRemoving = true;
}

PUBLIC bool WatchPointListener::isRemoving()
{
    return mRemoving;
}

//////////////////////////////////////////////////////////////////////
//
// WatchPoint
//
//////////////////////////////////////////////////////////////////////

PUBLIC WatchPoint::WatchPoint(const char* name, int key) :
    SystemConstant(name, key)
{
    mBehavior = WATCH_MOMENTARY;
    mMin = 0;
    mMax = 1;
}

PUBLIC WatchPoint::~WatchPoint()
{
}

PUBLIC WatchBehavior WatchPoint::getBehavior()
{
    return mBehavior;
}

PUBLIC int WatchPoint::getMin(MobiusInterface* m)
{
    return mMin;
}

PUBLIC int WatchPoint::getMax(MobiusInterface* m)
{
    return mMax;
}

/**
 * Called internally to notify the listeners of a state change.
 * Delgates up to Mobius to manage the listener list.
 */
PUBLIC void WatchPoint::notify(Mobius* m, Loop* l)
{
    int value = getValue(m, l);
    m->notifyWatchers(this, value);
}

/****************************************************************************
 *                                                                          *
 *                               LOOP LOCATION                              *
 *                                                                          *
 ****************************************************************************/

class LoopLocationType : public WatchPoint {

  public:

    LoopLocationType();
    
    List* getListeners(Watchers* w);
    int getValue(Mobius* m, Loop* l);

};

PUBLIC LoopLocationType::LoopLocationType() :
    WatchPoint("loopLocation", 0)
{
    mBehavior = WATCH_CONTINUOUS;
    // set up a pseudo range and scale to it
    mMax = 1000;
}

PUBLIC List* LoopLocationType::getListeners(Watchers* w)
{
    return w->loopLocation;
}

PUBLIC int LoopLocationType::getValue(Mobius* m, Loop* l)
{
    int value = 0;
    long max = l->getFrames();

    // frame may advance during recording, but we can't
    // scale a value until we stop and have a size
    if (max > 0) {
        // technically the max should be frames - 1 since
        // that's the end, but we can actually reach the frame
        // count when wrapping
        max--;

        long frame = l->getFrame();
        // allowed to be negative for latency comp
        if (frame < 0) frame = 0;

        value = ScaleValue(frame, 0, max, 0, mMax);
    }
    return value;
}

PUBLIC WatchPoint* LoopLocationPoint = new LoopLocationType();

/****************************************************************************
 *                                                                          *
 *                                 LOOP START                               *
 *                                                                          *
 ****************************************************************************/

class LoopStartType : public WatchPoint {

  public:
    LoopStartType();
    List* getListeners(Watchers* w);
    int getValue(Mobius* m, Loop* l);
};

PUBLIC LoopStartType::LoopStartType() :
    WatchPoint("loopStart", 0)
{
    mBehavior = WATCH_MOMENTARY;
}

PUBLIC List* LoopStartType::getListeners(Watchers* w)
{
    return w->loopStart;
}

PUBLIC int LoopStartType::getValue(Mobius* m, Loop* l)
{
    return 1;
}

PUBLIC WatchPoint* LoopStartPoint = new LoopStartType();

/****************************************************************************
 *                                                                          *
 *                                 LOOP CYCLE                               *
 *                                                                          *
 ****************************************************************************/

class LoopCycleType : public WatchPoint {

  public:
    LoopCycleType();
    List* getListeners(Watchers* w);
    int getValue(Mobius* m, Loop* l);
};

PUBLIC LoopCycleType::LoopCycleType() :
    WatchPoint("loopCycle", 0)
{
    mBehavior = WATCH_MOMENTARY;
}

PUBLIC List* LoopCycleType::getListeners(Watchers* w)
{
    return w->loopCycle;
}

PUBLIC int LoopCycleType::getValue(Mobius* m, Loop* l)
{
    return 1;
}

PUBLIC WatchPoint* LoopCyclePoint = new LoopCycleType();

/****************************************************************************
 *                                                                          *
 *                               LOOP SUBCYCLE                              *
 *                                                                          *
 ****************************************************************************/

class LoopSubcycleType : public WatchPoint {

  public:

    LoopSubcycleType();
    List* getListeners(Watchers* w);
    int getValue(Mobius* m, Loop* l);
};

PUBLIC LoopSubcycleType::LoopSubcycleType() :
    WatchPoint("loopSubcycle", 0)
{
    mBehavior = WATCH_MOMENTARY;
}

PUBLIC List* LoopSubcycleType::getListeners(Watchers* w)
{
    return w->loopSubcycle;
}

PUBLIC int LoopSubcycleType::getValue(Mobius* m, Loop* l)
{
    return 1;
}

PUBLIC WatchPoint* LoopSubcyclePoint = new LoopSubcycleType();

/****************************************************************************
 *                                                                          *
 *                                   STATIC                                 *
 *                                                                          *
 ****************************************************************************/

WatchPoint* WatchPoints[] = {

	LoopLocationPoint,
	LoopStartPoint,
	LoopCyclePoint,
	LoopSubcyclePoint,

    NULL
};

/**
 * Refresh the cached display names from the message catalog.
 */
PUBLIC void WatchPoint::localizeAll(MessageCatalog* cat)
{
    // punt, don't have a ui yet so no need for keys
}

PUBLIC WatchPoint** WatchPoint::getWatchPoints()
{
	return WatchPoints;
}

PUBLIC WatchPoint* WatchPoint::getWatchPoint(const char* name)
{
	WatchPoint* found = NULL;

	if (WatchPoints != NULL) {
		for (int i = 0 ; WatchPoints[i] != NULL ; i++) {
			WatchPoint* c = WatchPoints[i];
            if (!strcmp(name, c->getName()) ||
				(c->getDisplayName() != NULL &&
				 !strcmp(name, c->getDisplayName()))) {
                found = c;
				break;
			}
		}
	}
	return found;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
