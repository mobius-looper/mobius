/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Realign.
 *
 * NOTE: I DON'T LIKE THIS
 *
 * Revisit Realign and the Realign Time parameter!
 * It is too confusing and Track Sync Mode behaves differently than
 * the other sync modes.
 *
 * With Track Sync, the realign times SUBCYCLE, CYCLE, and LOOP
 * apply to the *master track*.  With all other sync modes these
 * apply to the *slave track*.  Think about making this consistent
 * or providing a flag to let the other sync modes use master-relative
 * realign points though that may be overkill.
 *
 * Also for all the non-track sync modes, the realign happens exactly
 * on a pulse, not when the slave loop reaches that point.  For coarse
 * grained pulses like HostBar, SUBCYCLE will always be the same as CYCLE
 * or LOOP since we won't be getting pulses on every subcycle.  
 *
 * The basic options we need are:
 *  
 *     Realign at external loop start point
 *     Realign on next pulse (immediate)
 *     
 *
 * We could get loop-local realign points by letting Realign be quantized,
 * then using immediate to wait for the next pulse.  
 *
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
#include "Messages.h"
#include "Mobius.h"
#include "MobiusConfig.h"
#include "Mode.h"
#include "Setup.h"
#include "Synchronizer.h"
#include "SyncState.h"
#include "Track.h"

//////////////////////////////////////////////////////////////////////
//
// RealignEvent
//
//////////////////////////////////////////////////////////////////////

class RealignEventType : public EventType {
  public:
	RealignEventType();
};

PUBLIC RealignEventType::RealignEventType()
{
	name = "Realign";
}

PUBLIC EventType* RealignEvent = new RealignEventType();

/****************************************************************************
 *                                                                          *
 *   							   REALIGN                                  *
 *                                                                          *
 ****************************************************************************/

/**
 * What should these do for scheduleSwitchStack?
 * Could just stack them.
 */
class RealignFunction : public Function {
  public:
	RealignFunction(bool mute);
	Event* scheduleEvent(Action* action, Loop* l);
	Event* scheduleSwitchStack(Action* action, Loop* l);
  private:
	bool mute;
};

PUBLIC Function* Realign = new RealignFunction(false);
PUBLIC Function* MuteRealign = new RealignFunction(true);

PUBLIC RealignFunction::RealignFunction(bool b)
{
	eventType = RealignEvent;
	cancelReturn = true;
	mayCancelMute = true;
	switchStack = true;

	mute = b;

	if (mute) {
		setName("MuteRealign");
		setKey(MSG_FUNC_MUTE_REALIGN);
		setHelp("Mute and restart loop at next global MIDI start point");
	}
	else {
		setName("Realign");
		setKey(MSG_FUNC_REALIGN);
		setHelp("Restart loop at next global MIDI start point");
        mayConfirm = true;
	}
}

/**
 * When exactly can you do one of these?  Assuming it can be a multiply/insert
 * mode end event which means it may be rescheduled.
 *
 * MuteRealign like MuteMidiStart is funny because we schedule two events, 
 * an immediate Mute and a pending Realign.  This could be a new mode, but
 * I'd rather it just be an event so it can be undone as usual.
 *
 * Like other functions if the Mute or Realign event comes back with the
 * reschedule flag set, do NOT schedule a play jump.  Note that
 * after the Mute is processed and we reschedule the Realign, we'll end
 * up back here, DO NOT schedule another Mute event or we'll go into 
 * a scheduling loop.  
 *
 * It is possible to keep overdubbing and otherwise mutating the loop
 * while there is a Realign event at the end, if the loop length is
 * changed we should try to reschedule the event.
 *
 * 
 */
PUBLIC Event* RealignFunction::scheduleEvent(Action* action, Loop* l)
{
    EventManager* em = l->getTrack()->getEventManager();
	Event* realignEvent = NULL;
	Event* muteEvent = NULL;

	// since this isn't a mode, try to catch reduncant invocations
	realignEvent = em->findEvent(RealignEvent);
	if (realignEvent != NULL) {
		// already one scheduled, ignore it
		realignEvent = NULL;
	}
	else {
        Setup* setup = l->getMobius()->getInterruptSetup();
        Track* t = l->getTrack();
        SyncState* state = t->getSyncState();
        SyncSource src = state->getEffectiveSyncSource();
		Synchronizer* sync = l->getSynchronizer();

		if (src == SYNC_NONE) {
			// the track is not syncing, realign is meaningless
			Trace(l, 2, "Ignoring Realign in unsyned track\n");
		}
		else if (src == SYNC_TRACK && 
                 setup->getRealignTime() == REALIGN_NOW) {

			// here we don't need an event, immediately jump to the
			// appropriate frame

            // !! WOAH we need to gracefully end the current mode first
            // this will totally screw up Multiply
			sync->loopRealignSlave(l);
		}
		else {
			// all others schedule an event

			// disable quantization of the mute event
			action->escapeQuantization = true;

			// no MuteEvent if we're already muted, see comments above
			// !! but mute may be scheduled

			if (mute && !l->isMuteMode()) {

                // Schedule an internal event to mute, must clone
                // the action
                Mobius* m = l->getMobius();
                Action* muteAction = m->cloneAction(action);

				muteEvent = Mute->scheduleEvent(muteAction, l);

                // a formality, the action should own it now
                m->completeAction(muteAction);
			}

			// go through the usual scheduling, but make it pending
			realignEvent = Function::scheduleEvent(action, l);
			if (realignEvent != NULL && !realignEvent->reschedule) {
				realignEvent->pending = true;
				realignEvent->quantized = true;

				// could remember this for undo?  
				// hmm, kind of like having them be independent
				//realignEvent->addChild(muteEvent);

				// NOTE: Unlike MuteMidiStart, we can't schedule a play
				// transition to come out of mute because we don't know
				// exactly when the external start point will happen.
				// We could make a guess then adjust it later, but at ASIO
				// latency, it will be hard to hear so just blow it off.
			}
            
            // On the EDP MuteRealign or Mute/Multiply is supposed
            // to stop sending clocks when Sync=Out, I like to keep
            // clocks going but send a MIDI Stop event.  
            // !! this needs to be sensitive to MuteSyncMode
            if (mute && sync->getOutSyncMaster() == l->getTrack())
              sync->loopMidiStop(l, false);
		}
	}

	return realignEvent;
}

PUBLIC Event* RealignFunction::scheduleSwitchStack(Action* action, Loop* l)
{
	Event* event = Function::scheduleSwitchStack(action, l);

	// have historically returned NULL here, is that important?
	return NULL;
}

/****************************************************************************
 *                                                                          *
 *                               DRIFT CORRECT                              *
 *                                                                          *
 ****************************************************************************/

class DriftCorrectFunction : public Function {
  public:
	DriftCorrectFunction();
    void invoke(Action* action, Mobius* m);
  private:
};

PUBLIC Function* DriftCorrect = new DriftCorrectFunction();

PUBLIC DriftCorrectFunction::DriftCorrectFunction() :
    Function("DriftCorrect", 0)
{
    global = true;
    // This is one of the few functions we allow outside the interrupt
    // it is safe because Synchronizer::forceDriftCorrect just sets a flag
    // to do correction on the next interrupt.  But now that we do most
    // global functions in the interrupt we don't need this level of
    // indirection.
    outsideInterrupt = true;
	noFocusLock = true;
    scriptOnly = true;
}

PUBLIC void DriftCorrectFunction::invoke(Action* action, Mobius* m)
{
	if (action->down) {
		trace(action, m);
        Synchronizer* sync = m->getSynchronizer();
        sync->forceDriftCorrect();
	}
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
