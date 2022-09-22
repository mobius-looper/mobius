/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * A model for defining "watch points" where a Mobius client can
 * be notified when various interesting things happen.
 *
 * WatchPoints are similar to Variables in that they have a value that
 * can be read by a Mobius client, but they are designed to model
 * things that are useful to export through OSC or MIDI rather than
 * variables which are designed to be used in scripts and are
 * more numerous.
 *
 * Parameters and a few variables are read/write, while WatchPoints
 * are read-only.   The client asks for Parameter or Variable values
 * with Exports.  WatchPoints have listeners and the client is notified
 * as soon after the watched value changes as possible.
 *
 */

#ifndef WATCHPOINT_H
#define WATCHPOINT_H

#include "SystemConstant.h"

//////////////////////////////////////////////////////////////////////
//
// WatchBehavior
//
//////////////////////////////////////////////////////////////////////

/**
 * Watch points are always exposed as numbers, but the numbers may 
 * change in different ways.
 */
typedef enum {
    
    /**
     * The state is instantaneous.  The value will go to 1
     * and back down to 0 immediately.  Client may want to 
     * add a decay so that lights stay lit long enough to see.
     *
     * Examples: loopStart, loopCycle
     */
    WATCH_MOMENTARY,

    /**
     * The state value persists for a period of time.
     *
     * Examples: recordMode, muteMode
     */
    WATCH_LATCH,

    /**
     * The state is continually sweeping over a range of values.
     *
     * Example: loopLocation
     */
	WATCH_CONTINUOUS

} WatchBehavior;

//////////////////////////////////////////////////////////////////////
//
// WatchPoint
//
//////////////////////////////////////////////////////////////////////

/**
 * The definition of a watch point.
 */
class WatchPoint : public SystemConstant {

    friend class Mobius;
    friend class Export;
    friend class Loop;

  public:

    static WatchPoint** getWatchPoints();
    static WatchPoint* getWatchPoint(const char* name);

	WatchPoint(const char* name, int key);
	virtual ~WatchPoint();

    // these can be used by the client
    WatchBehavior getBehavior();
    virtual int getMin(class MobiusInterface* m);
    virtual int getMax(class MobiusInterface* m);


  protected:

    static void localizeAll(class MessageCatalog* c);

    virtual class List* getListeners(class Watchers* watchers) = 0;
    virtual int getValue(class Mobius* m, class Loop* l) = 0;

    void notify(Mobius* m, Loop* l);

    WatchBehavior mBehavior;
    int mMin;
    int mMax;

};

//////////////////////////////////////////////////////////////////////
//
// WatchPoint Objects
//
//////////////////////////////////////////////////////////////////////

/**
 * Mobius needs direct access to these, but the UI should go through
 * MobiusInterface::getWatchPoints.
 */

extern WatchPoint* LoopLocationPoint;
extern WatchPoint* LoopStartPoint;
extern WatchPoint* LoopCyclePoint;
extern WatchPoint* LoopSubcyclePoint;

//////////////////////////////////////////////////////////////////////
//
// WatchPointListener
//
//////////////////////////////////////////////////////////////////////

/**
 * The interface of an object that may be registered to recieve
 * notifications when a WatchPoint changes.  The ownership
 * of this object is a bit unusual to avoid needing
 * a csect around the listener list traversals.  
 *
 * A subclass of WatchPointListener is defined by the
 * Mobius cleint and must overload the three virtual methods.
 * Once the listener has been registered using 
 * Mobius::addWatchPointListner, the object MUST NOT BE DELETED.
 *
 * If the client can reconfigure the set of listeners, they
 * must call the remove() method on the current listener
 * objects they no longer need. Once this is done the client
 * MUST NOT USE the listener object in any way.  Mobius will
 * garbage collect deactivated listener methods inside the
 * interrupt.
 */
class WatchPointListener {

    friend class Mobius;

  public:

    WatchPointListener();
    virtual ~WatchPointListener();

    /**
     * Return the name of the point you want to watch.
     * This will only be called during registration.
     */
    virtual const char* getWatchPointName() = 0;

    /**
     * Return track number starting from 1 if you want the
     * watch scoped to a track.  This method will be called
     * as watch points are processed and must be fast and
     * not do anything that can't be done inside the audio 
     * interrupt.
     *
     * Hmm, kind of not liking this interface, but avoids a bunch
     * of arguments and a wrapper.
     */
    virtual int getWatchPointTrack() = 0;

    /**
     * Handle a watch point event.
     */
    virtual void watchPointEvent(int value) = 0;

    /**
     * Mark the listener as inactive.
     * Once this is called, the listener object will
     * be deleted by Mobius at a later time.  The
     * client must no longer use the object in any way.
     */
    void remove();

  protected:

    bool isRemoving();

  private:
    
    bool mRemoving;

};

//////////////////////////////////////////////////////////////////////
//
// Watchers
//
//////////////////////////////////////////////////////////////////////

/**
 * An object maintained by Mobius that contains the registered
 * listeners for every watch point.
 * 
 * Dispensing with the usual getters and setters here since it
 * is only used internally and we need to be able to add these
 * without much fuss.
 *
 * The Lists will contain WatchPointListeners.
 */
class Watchers {
  public:

    Watchers();
    ~Watchers();

    class List* loopLocation;
    class List* loopStart;
    class List* loopSubcycle;
    class List* loopCycle;

    class List* modeRecord;

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
