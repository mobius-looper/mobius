/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * A small utility class used to monitor the up/down transitions
 * of a trigger in order to detect "long presses".
 *
 */

#include <stdio.h>
#include <memory.h>

#include "Trace.h"
#include "Util.h"

#include "Action.h"
#include "Function.h"
#include "Mobius.h"

#include "TriggerState.h"

//////////////////////////////////////////////////////////////////////
//
// TriggerWatcher
//
//////////////////////////////////////////////////////////////////////

PRIVATE void TriggerWatcher::init()
{
    next = NULL;
    trigger = NULL;
    triggerId = NULL;
    function = NULL;
    frames = 0;
    longPress = false;
}

PUBLIC TriggerWatcher::TriggerWatcher()
{
    init();
}

PUBLIC TriggerWatcher::~TriggerWatcher()
{
	TriggerWatcher *el, *nextel;

	for (el = next ; el != NULL ; el = nextel) {
		nextel = el->next;
		el->next = NULL;
		delete el;
	}
}

PUBLIC void TriggerWatcher::init(Action* a)
{
    trigger = a->trigger;
    triggerId = a->id;
    // !! shouldn't we just be able to use the ResolvedTarget here?
    function = a->getFunction();
    track = a->getTargetTrack();
    group = a->getTargetGroup();
    frames = 0;
    longPress = false;
}

/****************************************************************************
 *                                                                          *
 *                               TRIGGER STATE                              *
 *                                                                          *
 ****************************************************************************/

/**
 * Let the max be two per track, way more than needed in practice.
 */
#define MAX_TRIGGER_WATCHERS 16

PUBLIC TriggerState::TriggerState()
{
    mPool = NULL;
    mWatchers = NULL;
    mLastWatcher = NULL;

    // default to 1/2 second at 44100
    mLongPressFrames = 22050;

    // go ahead and flesh out the pool now, we can use this
    // to enforce the maximum
    for (int i = 0 ; i < MAX_TRIGGER_WATCHERS ; i++) {
        TriggerWatcher* tw = new TriggerWatcher();
        tw->next = mPool;
        mPool = tw;
    }
}

PUBLIC TriggerState::~TriggerState()
{
    delete mWatchers;
    delete mPool;
}

/**
 * Must be set by the owner when it knows the long press frame length.
 */
PUBLIC void TriggerState::setLongPressFrames(int frames)
{
    mLongPressFrames = frames;
}

/**
 * Must be set by the owner when it knows the long press frame length.
 */
PUBLIC void TriggerState::setLongPressTime(int msecs, int sampleRate)
{
    // TODO: Calculate frame from time and sample rate
}

/**
 * Assimilate an action. 
 * If this action is sustainable add a TriggerWatcher to the list.
 */
PUBLIC void TriggerState::assimilate(Action* action)
{
    Function* func = action->getFunction();
    
    if (func == NULL) {
        // should have been caught by now
        Trace(1, "TriggerState::assimilate missing function!\n");
    }
    else if (!action->down) {
        // an up transition
        TriggerWatcher* tw = remove(action);
        if (tw != NULL) {
            const char* msg;
            if (tw->longPress)
              msg = "TriggerState: ending long press for %s\n";
            else
              msg = "TriggerState: ending press for %s\n";

            Trace(2, msg, tw->function->getDisplayName());

            // convey long press state in the action
            action->longPress = tw->longPress;
            tw->next = mPool;
            mPool = tw;
        }
    }
    else {
        // A down transition, decide if this is something we can track
        // NOTE: If source is TriggerScript, triggerMode will be sustainable
        // if we're using the "up" or "down" arguments to simulate
        // SUS functions.  We could track long presses for those but it's less
        // useful for scripts, they can do their own timing.

        Trigger* trigger = action->trigger;
        bool longTrigger = (trigger == TriggerUI ||
                            trigger == TriggerKey ||
                            trigger == TriggerMidi ||
                            trigger == TriggerHost || 
                            trigger == TriggerOsc);

        bool longFunction = (func->longPressable || func->longFunction);

		// note we can get here during the invokeLong of a function, 
		// in which case it should set action->longPress to prevent
        // recursive tracking
		if (longTrigger && 
            longFunction &&
			!action->longPress &&   
            action->isSustainable()) {

            // Triggers of the same id can't overlap, this
            // sometimes happens in debugging.  Reclaim them.
            TriggerWatcher* tw = remove(action);
            if (tw != NULL) {
                Trace(2, "TriggerState: Cleaning dangling trigger for %s\n",
                      tw->function->getDisplayName());
                tw->next = mPool;
                mPool = tw;
            }

            if (mPool == NULL) {
                // Shouldn't get here unless there is a misconfigured
                // switch that isn't sending note offs.  We can either
                // start ignoring new ones or start losing old ones.
                // The later is maybe nicer for the user but there will
                // be unexpected long-presses so maybe it's best to die 
                // suddenly.
                Trace(1, "TriggerState: Pool exhausted, ignoring long press tracking for %s\n",
                      func->getDisplayName());
            }
            else {
                Trace(2, "TriggerState: Beginning press for %s\n",
                      func->getDisplayName());

                TriggerWatcher* tw = mPool;
                mPool = tw->next;
                tw->next = NULL;
                tw->init(action);

                if (mLastWatcher != NULL)
                  mLastWatcher->next = tw;
                else 
                  mWatchers = tw;

                mLastWatcher = tw;
            }
		}
    }
}

/**
 * Search for a TriggerWatcher that matches and remove it.
 * Triggers match on the Trigger type plus the id.
 * 
 * !! TODO: Should also have a timeout for these...
 */
PRIVATE TriggerWatcher* TriggerState::remove(Action* action)
{
    TriggerWatcher* found = NULL;
    TriggerWatcher* prev = NULL;

    for (TriggerWatcher* t = mWatchers ; t != NULL ; t = t->next) {
        
        // a trigger is always uniquely identified by the Trigger type
        // and the id
        if (t->trigger == action->trigger && t->triggerId == action->id) {
            found = t;
            if (prev != NULL)
              prev->next = t->next;
            else 
              mWatchers = t->next;

            if (mLastWatcher == t) {
                if (t->next != NULL)
                  Trace(1, "TriggerState: malformed watcher list!\n");
                mLastWatcher = prev;
            }

            break;
        }
        else
          prev = t;
    }

    return found;
}

/**
 * Advance the time of all pending triggers.  If any of them
 * reach the long-press threshold notify the functions.
 *
 * For each trigger we determined to be sustained long, create
 * an Action containing the relevant parts of the original down
 * Action and pass it to the special Function::invokeLong method.
 * !! Think about whether this can't just be a normal Action sent
 * to Mobius::doAction, with action->down = true and 
 * action->longPress = true it means to do the long press behvaior.
 */
PUBLIC void TriggerState::advance(Mobius* mobius, int frames)
{
    for (TriggerWatcher* t = mWatchers ; t != NULL ; t = t->next) {

        //Trace(2, "TriggerState: advancing %s %ld\n", 
        //t->function->getDisplayName(), (long)t->frames);

        t->frames += frames;

        // ignore if we've already long-pressed 
        if (!t->longPress) {
            if (t->frames > mLongPressFrames) {
                // exceeded the threshold
                t->longPress = true;

                Trace(2, "TriggerState: Long-press %s\n", 
                      t->function->getDisplayName());

                Action* a = mobius->newAction();
                a->inInterrupt = true;

                // trigger
                // what about triggerValue and triggerOffset?
                a->trigger = t->trigger;
                a->id = t->triggerId;

                // target
                // sigh, we need everything in ResolvedTarget 
                // for this 
                a->setFunction(t->function);
                a->setTargetTrack(t->track);
                a->setTargetGroup(t->group);

                // arguments
                // not carrying any of these yet, if we start needing this
                // then just clone the damn Action 
                
                // this tells Mobius to call Function::invokeLong
                a->down = true;
                a->longPress = true;

                mobius->doAction(a);
            }
        }
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
