/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Track selection functions with added "track copy" features.
 *
 * NOTE: the port of this isn't complete, some lingering dependencies
 * with loop switch and sound/timing copy
 * 
 */

#include <stdio.h>
#include <string.h>
#include <memory.h>

#include "Action.h"
#include "Event.h"
#include "EventManager.h"
#include "Function.h"
#include "Layer.h"
#include "Loop.h"
#include "Mobius.h"
#include "Mode.h"
#include "Messages.h"
#include "Preset.h"
#include "Synchronizer.h"
#include "Track.h"

//////////////////////////////////////////////////////////////////////
//
// TrackEvent
//
//////////////////////////////////////////////////////////////////////

class TrackEventType : public EventType {
  public:
	TrackEventType();
};

PUBLIC TrackEventType::TrackEventType()
{
	name = "Track";
}

PUBLIC EventType* TrackEvent = new TrackEventType();

/****************************************************************************
 *                                                                          *
 *                                TRACK SELECT                              *
 *                                                                          *
 ****************************************************************************/

class TrackSelectFunction : public ReplicatedFunction {
  public:
	TrackSelectFunction(int index, bool relative);
	Event* invoke(Action* action, Loop* l);
    void doEvent(Loop* loop, Event* event);
  private:
    Track* getNextTrack(Action* action, Track* t);
};

PUBLIC Function* NextTrack = new TrackSelectFunction(1, true);
PUBLIC Function* PrevTrack = new TrackSelectFunction(-1, true);

// TODO: need a way to define these on the fly

PUBLIC Function* TrackN = new TrackSelectFunction(-1, false);
PUBLIC Function* Track1 = new TrackSelectFunction(0, false);
PUBLIC Function* Track2 = new TrackSelectFunction(1, false);
PUBLIC Function* Track3 = new TrackSelectFunction(2, false);
PUBLIC Function* Track4 = new TrackSelectFunction(3, false);
PUBLIC Function* Track5 = new TrackSelectFunction(4, false);
PUBLIC Function* Track6 = new TrackSelectFunction(5, false);
PUBLIC Function* Track7 = new TrackSelectFunction(6, false);
PUBLIC Function* Track8 = new TrackSelectFunction(7, false);


PUBLIC TrackSelectFunction::TrackSelectFunction(int i, bool relative)
{
	eventType = TrackEvent;
	index = i;
	replicated = !relative;
	noFocusLock = true;
	activeTrack = true;
	// set this so we can respond visually to track select even
	// when there is no audio device pumping interrupts
	runsWithoutAudio = true;
    resetEnabled = true;

	// scripts always want to automatically wait for this to complete
	scriptSync = true;

	if (relative) {
		if (i > 0) {
			setName("NextTrack");
			setKey(MSG_FUNC_NEXT_TRACK);
		}
		else {
			setName("PrevTrack");
			setKey(MSG_FUNC_PREV_TRACK);
		}
	}
	else if (index < 0) {
		// must be used with an argument
		scriptOnly = true;
		setName("Track");
		setKey(MSG_FUNC_SELECT_TRACK);
	}
	else {
		sprintf(fullName, "Track%d", i + 1);
		setName(fullName);
		setKey(MSG_FUNC_TRACK);

		// the older name 
		sprintf(fullAlias1, "TrackSelect%d", i + 1);
		alias1 = fullAlias1;
	}
}

/**
 * Determine what the next Track should be.
 * For relative moves the starting track is passeed in.  This will
 * be the current track the first time, but it may be other
 * tracks if there is a pending TrackSelect event we're modifying
 */
PRIVATE Track* TrackSelectFunction::getNextTrack(Action* action, Track* track)
{
	Track* nextTrack = NULL;
    Mobius* mobius = track->getMobius();
	int trackCount = mobius->getTrackCount();
    int nextIndex = index;

	if (replicated) {
		if (nextIndex < 0) {
			// Must have an argument, in Bindings, Actions, and the UI
            nextIndex = action->arg.getInt();
            // since these are visible, require 1 based indexing
            nextIndex--;
            if (nextIndex < 0) nextIndex = 0;
		}
	}
	else {
        // unlike Loop::mNumber, this is zero based instead of 1 based
        nextIndex = track->getRawNumber();

        if (index > 0) {
            if (nextIndex < trackCount - 1)
              nextIndex++;
            else
              nextIndex = 0;
        }
        else {
            if (nextIndex > 0)
              nextIndex--;
            else
              nextIndex = trackCount - 1;
        }
    }

	if (nextIndex >= 0)
	  nextTrack = mobius->getTrack(nextIndex);

	return nextTrack;
}

/**
 * This one is complicated because of TrackLeaveAction and mode sensitivity.
 * Another case where it would be nice if we could make the MobiusMode
 * have this logic.
 *
 * The behavior is influenced by the TrackLeaveAction paramter which
 * may have these values:
 *
 *   none
 *     - Just change tracks, leave the current track in whatever
 *       mode it is in.
 *
 *   cancel
 *     - Schecule appropriate mode ending events to leave the current
 *       track in Play mode
 *
 *   cancelWait
 *     - Schedule mode ending events and wait for them to finish
 *
 * If the next track triggers EmptyTrackAction and the previous track
 * was still recording then we take the play layer rather than the 
 * record layer.  
 *
 * For all recording modes except rounding modes, cancel and cancelWait
 * behave the same.  We will always wait for the latency adjusted mode
 * ending event so that track copy works as expected.
 *
 * NOTE: We set the activeTrack flag in the Function definition to make sure
 * the event is scheduled in the active track, even if the script's
 * target track is somewhere else.
 */
Event* TrackSelectFunction::invoke(Action* action, Loop* l)
{
	Event* event = NULL;

    if (action->down) {
		trace(action, l);
        
        EventManager* em = l->getTrack()->getEventManager();
        Event* prev = em->findEvent(eventType);

        if (prev != NULL) {
            // We're already waiting for a track switch.
            // like loop switch we can make repeated invocations
            // just change the destination track.
            // !! Could also treat this like "escape quantization"
            // like we do for other functions
            Track* last = prev->fields.trackSwitch.nextTrack;
            if (last == NULL)
              Trace(1, "TrackSelect event without track pointer!\n");
            else {
                Track* next = getNextTrack(action, last);
                prev->fields.trackSwitch.nextTrack = next;
                prev->number = next->getRawNumber() + 1;
            }
        }
        else {
            Track* next = getNextTrack(action, l->getTrack());
            if (next != NULL && next != l->getTrack()) {

                Preset* p = l->getPreset();
                Preset::TrackLeaveAction leaveAction = p->getTrackLeaveAction();
                MobiusMode* mode = l->getMode();

                bool schedule = true;
                bool immediate = false;
                long selectFrame = l->getFrame();

                // Yes, I know most of this logic is unnecessary but this
                // also serves to document each of the modes to explain why
                // it can happen immediately.

                if (leaveAction == Preset::TRACK_LEAVE_NONE) {
                    immediate = true;
                }
                else if (l->isPaused()) {
                    // Pause is a mess, what if we're in a paused
                    // recording mode? We can't wait for that.  
                    immediate = true;
                }
                else if (mode == ResetMode || mode == PlayMode) {
                    // nothing to do
                    immediate = true;
                }
                else if (mode == RunMode) {
                    // This is a temporary state only used with switching loops
                    // and the current loop is in Reset.  The event has
                    // already happened so we can't cancel it now.
                    immediate = true;
                }
                else if (mode == ConfirmMode) {
                    // On TRACK_LEAVE_WAIT we could be waiting
                    // for an indefinite amount of time.  This one
                    // seems like it should cancel immediately
                    em->cancelSwitch();
                    immediate = true;
                }
                else if (mode == SwitchMode) {
                    // I guess we can cancel these but since there are usually
                    // just playback decisions it may make sense to leave them
                    // especially if there is quantization.  But then it gets
                    // complicated if EmptyLoopAction will happen which
                    // can cause recording to start which you might want
                    // canceled. Assume for now that we leave it.
                    // parameters would be awkward: cancelRecording, cancelAll,
                    // cancelRecordingWait, cancelAllWait.  It would have to be
                    // broken up into a leave action and an optional wait
                    // boolean.

                    // !! Need to find the existing switch event and 
                    // stack this?
                    //em->cancelSwitch();

                    immediate = true;
                }
                else if (mode == ThresholdMode || mode == SynchronizeMode) {
                    // just cancel, no waiting
                    l->reset(NULL);
                    immediate = false;
                }
                else if (mode == MuteMode) {
                    // Another funny one, mute is both major and minor
                    // If major, just leave it alone
                    immediate = true;
                }
                else if (mode == OverdubMode || 
                         mode == ReplaceMode ||
                         mode == SubstituteMode) {

                    // Schedule the mode ending.  If we're not quantizing
                    // and trackLeaveMode=cancel, the user thinks of this
                    // has happening immediately and expects emptyTrackAction
                    // to pick up the recording we're going to end.  So these
                    // all behave like trackLeaveMode=wait.
                    // REALLY need a consistent ending method on MobiusMode...
                    
                    Mobius* m = l->getMobius();
                    Action* stopAction = m->cloneAction(action);

                    bool oldWay = false;
                    if (oldWay) {
                        // We just schedule RecordStopEvent which
                        // will stop any kind of recording.
                        
                        selectFrame = l->getFrame() + l->getInputLatency();
                        // I HATE how you have to create events with the
                        // right "family function" out here.  This should
                        // be encapsulated in the MobiusMode or RecordFunction.
                        Event* e = em->newEvent(Record, RecordStopEvent, selectFrame);
                        stopAction->setEvent(e);
                        e->savePreset(l->getPreset());
                        em->addEvent(e);
                    }
                    else {
                        // Pretend we're invoking function again so we can
                        // get quantization.  
                        // !! May have to be smarter about ending a SUS
                        // function with a normal function.  Does the
                        // function need to be SUS with action->down false?

                        if (mode == OverdubMode)
                          stopAction->setFunction(OverdubOff);

                        else if (mode == ReplaceMode)
                          stopAction->setFunction(Replace);

                        else if (mode == SubstituteMode)
                          stopAction->setFunction(Substitute);

                        // Build the fundamental function event, possibly
                        // quantized but not scheduled
                        Event* stop = em->getFunctionEvent(stopAction, l, NULL);
                        if (stop == NULL) { 
                            // shouldn't happen here
                            Trace(1, "TrackSelect: Unable to end mode!\n");
                        }
                        else {
                            em->addEvent(stop);
                            if (!stop->quantized || leaveAction == Preset::TRACK_LEAVE_WAIT)
                              selectFrame = stop->frame;
                        }
                    }

                    m->completeAction(stopAction);
                }
                else if (mode->rounding) {
                    // Insert, Multiply, Stutter

                    // Let the mode decide how to handle the triggr event, 
                    // it may use it or free it.  Ugh, flow of control is
                    // so ugly here.

                    // have to pretend we're going to end here
                    event = em->newEvent(this, TrackEvent, selectFrame);
                    event->savePreset(l->getPreset());
                    event->fields.trackSwitch.nextTrack = next;
                    action->setEvent(event);

                    // this will schedule it and may change the event frame
                    Event* modeEnd = l->scheduleRoundingModeEnd(action, event);

                    if (modeEnd != NULL && modeEnd->getParent() == NULL) {
                        // Mode decided not to use the triggering event
                        // and deleted it. This should not happen here
                        Trace(1, "TrackSelect: Rounding mode deleted track select event!\n");
                        event = NULL;
                    }
                    else if (leaveAction == Preset::TRACK_LEAVE_CANCEL) {
                        // we supposed to cancel but not wait,  
                        // restore the original frame
                        event->frame = selectFrame;
                    }

                    // in all cases don't schedule another one
                    schedule = false;
                }
                else if (mode == RecordMode || mode == RehearseRecordMode) {
                    // secondary event, have to clone the Action
                    Mobius* m = l->getMobius();

                    Action* stopAction = m->cloneAction(action);

                    Event* stop = Record->scheduleModeStop(stopAction, l);

                    m->completeAction(stopAction);

                    if (stop->pending) {
                        if (leaveAction == Preset::TRACK_LEAVE_WAIT) {
                            // !! waiting on a sync pulse
                            // we should be able to handle this with
                            // stacking or rescheduling, punt for now
                            Trace(1, "TrackSelect: unable to wait for sync pulse!\n");
                        }
                    }
                    else {
                        // Always wait for latency delay since this is
                        // expected to be immediate.  Actually sync
                        // rounding probably is too.
                        selectFrame = stop->frame;
                    }
                }
                else if (mode == RehearseMode) {
                    // fall back to play mode
                    l->cancelRehearse(NULL);
                    immediate = true;
                }
                else {
                    Trace(1, "TrackSelect: unexpected mode %ld\n", (long)mode);
                    immediate = true;
                }

                // normal function scheduling would do this
                // does it make sense here?
                em->cancelReturn();

                if (immediate) {
                    // !! I don't like how we bypass normal Event processing
                    // logic.  Think about refactoring this down into 
                    // EventManager::processEvent and using event->immediate.

                    Event* e = em->newEvent(this, TrackEvent, 0);
                    e->savePreset(l->getPreset());
                    e->fields.trackSwitch.nextTrack = next;
                    action->setEvent(e);

                    l->trackEvent(e);

                    action->detachEvent(e);
                    e->free();
                }
                else if (schedule) {
                    // It doesn't matter what the function is since we
                    // store the next track pointer in the event.  Use
                    // TrackN plus a number that will be seedn in the UI
                    // rather than NextTrack/PrevTrack.
                    event = em->newEvent(TrackN, TrackEvent, selectFrame);
                    event->number = next->getRawNumber() + 1;
                    event->savePreset(l->getPreset());
                    event->fields.trackSwitch.nextTrack = next;

                    // set this if we're latency delayed, is selectFrame
                    // reliable?
                    event->fields.trackSwitch.latencyDelay = 
                        (selectFrame != l->getFrame());

                    action->setEvent(event);
                    em->addEvent(event);
                }
            }
        }
    }

	return event;
}

PUBLIC void TrackSelectFunction::doEvent(Loop* l, Event* e)
{
	l->trackEvent(e);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
