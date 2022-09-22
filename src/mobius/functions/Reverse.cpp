/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Reverse and related functions.
 *
 * Implmenting reverse by reversing the direction of the frame
 * counter and all of the frame calculations adds a great deal
 * of complexity and is very error prone.  Instead, we push
 * the implementation down into the Layer and Audio classes to make it behave
 * as if the frame sequence had been instantly mirrored.  We can
 * then continue to move "forward" in all of the calculations up here, 
 * though Audio will actually be applying the changes in reverse.
 * Since Audio has a relatively simple job to do, this greatly
 * reduces complexity.
 *
 * There is one thing we have to do out here though.  After
 * Audio flips the sequence of frames, we have to flip the frame
 * times of any events or transitions that had been previously calculated.
 * For example say there is a loop of 1000 frames and the record
 * frame counter is 800.  When Audio flips the processing of the
 * frames, we have to convert the frame counter of 800 to 200 since
 * instead than being 200 from the "end" we're now 800 from the
 * end going in reverse.  
 * 
 * THINK: If there are quantized events we haven't reached when we
 * go into reverse, their event frames will be reflected so we'll
 * encounter them in reverse at a different relative distance.  You'll
 * have to "loop around" once to encounter them them. Example:
 *
 * loopFrames = 8, currentFrame=2, reverseEvent = 2, otherEvent = 4
 *
 * After reflection currentFrame=5 and otherEvent=3.  If there was no
 * reverseEvent, we would have encountered otherEvent in 2 frames.  Now
 * we have to wait 6 frames since we have to loop around once.
 *
 * This may be interesting, but it isn't what you want for a pair
 * of quantized SUSReverse events.  You expect the reverse to 
 * start at the first event, and end 2 frames later.  Here you want
 * to reflect around the reverseEvent frame, maintaining the same
 * relative distance.   That seems more musically useful.
 *
 * If there are events quantized beyond the end of the loop, simple 
 * size-based reflection will result in events with negative frames that
 * will never be reached until we reverse again.  I actually sort of
 * like that, but it does conflict with reverseEvent reflection we would
 * do for other events.  This may not be an issue since reverse should
 * cancel most modes?
 * 
 * TODO: There is the notion of a long-press with ToggleReverse that
 * changes it from a toggle to a SUS toggle if the MIDI note is held down
 * long enough.
 * 
 * Only the Insert and ToggleReverse functions can be used as a Record
 * alternate ending.
 */

#include <stdio.h>
#include <memory.h>
#include <string.h>

#include "Action.h"
#include "Event.h"
#include "EventManager.h"
#include "Function.h"
#include "Layer.h"
#include "Loop.h"
#include "Messages.h"
#include "Mode.h"
#include "Stream.h"
#include "Track.h"

//////////////////////////////////////////////////////////////////////
//
// ReverseMode
//
// Minor mode active when in reverse.
//
//////////////////////////////////////////////////////////////////////

class ReverseModeType : public MobiusMode {
  public:
	ReverseModeType();
};

ReverseModeType::ReverseModeType() :
    MobiusMode("reverse", MSG_MODE_REVERSE)
{
	minor = true;
}

MobiusMode* ReverseMode = new ReverseModeType();

//////////////////////////////////////////////////////////////////////
//
// ReverseEvent
//
//////////////////////////////////////////////////////////////////////

class ReverseEventType : public EventType {
  public:
	ReverseEventType();
};

PUBLIC ReverseEventType::ReverseEventType()
{
	name = "Reverse";
	// !! have historically done this, I tried to avoid it when
	// we introduced the quantizeStack flag, but some of the calculations
	// are still too complicated
	reschedules = true;
}

PUBLIC EventType* ReverseEvent = new ReverseEventType();

//////////////////////////////////////////////////////////////////////
//
// ReversePlayEvent
//
//////////////////////////////////////////////////////////////////////

class ReversePlayEventType : public EventType {
  public:
	ReversePlayEventType();
	void invoke(Loop* l, Event* e);
    void invokeLong(Action* action, Loop* l);
	void undo(Loop* l, Event* e);
};

PUBLIC ReversePlayEventType::ReversePlayEventType()
{
	name = "ReversePlay";
}

PUBLIC void ReversePlayEventType::invoke(Loop* l, Event* e)
{
	l->reversePlayEvent(e);
}

PUBLIC void ReversePlayEventType::undo(Loop* l, Event* e)
{
	l->reversePlayEventUndo(e);
}

PUBLIC EventType* ReversePlayEvent = new ReversePlayEventType();

/****************************************************************************
 *                                                                          *
 *   							   REVERSE                                  *
 *                                                                          *
 ****************************************************************************/

class ReverseFunction : public Function {
  public:
	ReverseFunction(bool sus, bool toggle, bool forward);
	Event* scheduleEvent(Action* action, Loop* l);
	Event* scheduleSwitchStack(Action* action, Loop* l);
    Event* scheduleTransfer(Loop* l);
	void prepareJump(Loop* l, Event* e, JumpContext* jump);
    void invokeLong(Action* action, Loop* l);
    void doEvent(Loop* loop, Event* event);
  private:
    long reverseFrame(Loop* l, long frame);
	bool toggle;
	bool forward;
};

PUBLIC Function* SUSReverse = new ReverseFunction(true, true, false);
PUBLIC Function* Reverse = new ReverseFunction(false, true, false);
PUBLIC Function* Forward = new ReverseFunction(false, false, true);
PUBLIC Function* Backward = new ReverseFunction(false, false, false);

PUBLIC ReverseFunction::ReverseFunction(bool sus, bool tog, bool fwd)
{
	eventType = ReverseEvent;
	minorMode = true;
	mayCancelMute = true;
	quantized = true;
	quantizeStack = true;
	sustain = sus;
	toggle = tog;
	forward = fwd;
	resetEnabled = true;
	cancelReturn = true;
	thresholdEnabled = true;
	switchStack = true;

	if (!toggle) {
		if (forward) {
			setName("Forward");
			setKey(MSG_FUNC_FORWARD);
		}
		else {
			setName("Backward");
			setKey(MSG_FUNC_BACKWARD);
		}
	}
	else if (sustain) {
		setName("SUSReverse");
		setKey(MSG_FUNC_SUS_REVERSE);
	}
	else {
		setName("Reverse");
		setKey(MSG_FUNC_REVERSE);
		longFunction = SUSReverse;
        // can also force this with SustainFunctions preset parameter
        maySustain = true;
        mayConfirm = true;
	}

}

PUBLIC Event* ReverseFunction::scheduleEvent(Action* action , Loop* l)
{
	Event* event = NULL;
	MobiusMode* mode = l->getMode();

	if (mode == ResetMode || 
		mode == ThresholdMode || 
		mode == SynchronizeMode) {

		// allowed to toggle for the next Record
		if (!toggle && forward)
		  l->setReverse(false);
		else if (!toggle && !forward)
		  l->setReverse(true);
		else
		  l->setReverse(!l->isReverse());
	}
	else {
		// must schedule even if it looks like we're already going the
		// right direction because there may be unprocessed events comming up
		// that will change that

		event = Function::scheduleEvent(action, l);
		if (event != NULL) {

			// if the event is quantized to a loop boundary,
			// process it after we loop back to zero to prevent
			// negative frame calculations when reflecting
			event->afterLoop = true;

			// !! if this is SUSReverse and the quantization period
			// is very short, this could result in latency loss we
			// don't really need?  Reverse isn't really mode sensitive,
			// don't need to reschedule?
			if (!event->reschedule) {

                if (l->getMode() == RecordMode) {
                    // don't need to mess with transition events, just do it now
                    // scheduleRecordStop will have been called and left us 
                    // playing the record layer, don't need to reflect the
                    // play frame, just toggle the flag
                    // !! what if we're synchronzing will are we sure to
                    // have been rescheduled?
                    OutputStream* ostream = l->getOutputStream();
                    ostream->setReverse(!ostream->isReverse());
                }
                else {
                    // schedule a transition when the output stream needs
                    // to begin reversing

                    EventManager* em = l->getTrack()->getEventManager();
                    Event* rev = em->schedulePlayJumpType(l, event, ReversePlayEvent);
                    rev->afterLoop = true;

                    Trace(l, 2, "Loop: Reverse transition frame %ld latency loss %ld\n", 
                          event->frame, rev->latencyLoss);
                }
			}
		}
	}

	return event;
}

/**
 * What should SUSReverse do?
 * For now treat like non-SUS, but could let it carry over and
 * schedule a Return?
 */
PUBLIC Event* ReverseFunction::scheduleSwitchStack(Action* action, Loop* l)
{
	Event* event = NULL;
    EventManager* em = l->getTrack()->getEventManager();

	if (action->down) {
		Event* switche = em->getUncomittedSwitch();
		if (switche != NULL) {
			Event* prev = switche->findEvent(eventType);
			if (prev == NULL) {
				// ignore Forward since we're already going that way
				if (toggle || !forward)
				  event = Function::scheduleSwitchStack(action, l);
			}
			else if (toggle || forward) {
				// cancel the previous one
				em->cancelSwitchStack(prev);
			}
			else {
				// must be Backward and we're already reversing, ignore
				event = prev;
			}
		}
	}

	return event;
}

/**
 * Schedule events after a loop switch for pitch state.
 * If we're using TRANSFER_FOLLOW we don't have to do anything since
 * stream state is kept on the track, we just change loops and it stays.
 */
PUBLIC Event* ReverseFunction::scheduleTransfer(Loop* l)
{
    Event* event = NULL;
    Preset* p = l->getPreset();
    Preset::TransferMode tm = p->getReverseTransfer();

    if (tm == Preset::XFER_OFF || tm == Preset::XFER_RESTORE) {

        Event* event = NULL;
        EventManager* em = l->getTrack()->getEventManager();

        // If we have any stacked pitch events assume that overrides
        // transfer.  May want more here, for example overriding step
        // but not bend.

        Event* prev = em->findEvent(eventType);
        if (prev == NULL) {
            if (tm == Preset::XFER_OFF) {
                event = em->newEvent(Forward, l->getFrame());
            }
            else {
                StreamState* state = l->getRestoreState();
                if (state->reverse) 
                  event = em->newEvent(Backward, l->getFrame());
                else
                  event = em->newEvent(Forward, l->getFrame());

            }

            if (event != NULL) {
                event->automatic = true;
                em->addEvent(event);
            }
        }
    }

    return event;
}

/**
 *
 * NOTE: should only be here for Reverse stacked on a Switch.
 * Normal reverse still uses ReversePlayEvent that has some very subtle
 * latency loss calculations.
 * !! need to merge these
 */
PUBLIC void ReverseFunction::prepareJump(Loop* l, Event* e, JumpContext* jump)
{
	// should only be here if we're going to toggle, but make sure
	if (this == Reverse || this == SUSReverse ||
		(this == Forward && jump->reverse) ||
		(this == Backward && !jump->reverse)) {
			
		jump->reverse = !jump->reverse;
	}
}

/**
 * Long-Reverse is supposed to become SUSReverse
 */
PUBLIC void ReverseFunction::invokeLong(Action* action, Loop* l)
{
}

/**
 * ReverseEvent event handler.
 *
 * If we're quantized to a loop boundary, the event is expected to have
 * the afterLoop flag set  so we process the LoopEvent first.  Without that,
 * mFrame will be the same as mLoopFrames, and reflection will result in -1
 * which then messes up mPlayFrame calculation.  I guess we could detect
 * that here and set mFrame to zero before reflecting, but I think I'd also
 * like to have LoopEvent processing fire too since we did in fact reach 
 * the loop end.
 * 
 * Like ReversePlayEvent the frame the ReverseEvent event is on is 
 * not "in" the region, we have to backup mFrame by 1 before reflecting.
 */
PUBLIC void ReverseFunction::doEvent(Loop* l, Event* e)
{
    if (e->type == ReverseEvent) {

        MobiusMode* mode = l->getMode();
        EventManager* em = l->getTrack()->getEventManager();
        Function* func = e->function;
        long origFrame = l->getFrame();

        // !! hey, can't we just stay in rehearse?
        if (mode == RehearseMode)
          l->cancelRehearse(e);

        if ((func == Forward && !l->isReverse()) ||
            (func == Backward && l->isReverse())) {

            // Ignore if we're already going in the right direction
            // Originally tried to avoid having Function schedule this in 
            // the first place, but it needs to be in the machinery to act
            // as an ending event for other modes.  
            Trace(l, 2, "Reverse: Ignoring scheduled reverse event\n");
        }
        else if (mode == ResetMode) {
            // we let this be scheduled for stacked switch events

            // do event reflection relative to the current frame
            // !! this shouldn't be necessary, there are no other events
            em->reverseEvents(origFrame, l->getFrame());

            l->setReverse(!l->isReverse());
        }
        else {
            // keep recording?  
            // makes sense for overdub, but what about replace/substitute?
            // !! Layer isn't going to like this

            // calling setFrame will set mLastSyncEventFrame to -1, so remember it
            long lastSyncFrame = em->getLastSyncEventFrame();

            // like mPlayFrame, mFrame has to be decremented before reversing
            // since it is outside the region
            long adjustFrame = l->getFrame() - 1;
            if (adjustFrame < 0)
              adjustFrame = l->getFrames() - 1;
            l->setFrame(adjustFrame);

            long newFrame = reverseFrame(l, l->getFrame());
            l->setFrame(newFrame);

            // don't bother reflecting this, we're just trying to prevent
            // multiple sync events on the same frame
            if (lastSyncFrame == origFrame)
              em->setLastSyncEventFrame(newFrame);

            // need to reflect the ReverseEvent frame so that later
            // rescheduling of pending events has the right origin
            // NB: if the event was quantized after the end of the loop, 
            // we will have shifted the events and the frame will be zero,
            // in that case don't reverse the frame which will leave it
            // at the last frame and confuse pending event scheduling
            if (e->frame == origFrame)
              e->frame = newFrame;
            else {
                // !! hey, what about the -1 adjustment we do for mFrame, 
                // isn't that needed here too?
                Trace(l, 1, "Loop: Possible event reflection error!\n");
                e->frame = reverseFrame(l, e->frame);
            }

            // I don't think these are issues because we'd cancel
            // the mode before entering reverse?
            // but wouldn't we have the same -1 issue with mFrame?
            l->setModeStartFrame(reverseFrame(l, l->getModeStartFrame()));

            // do event reflection relative to the current frame
            em->reverseEvents(origFrame, l->getFrame());

            l->setReverse(!l->isReverse());

            // normally we will stay in mute 
            l->checkMuteCancel(e);
            l->validate(e);
        }
    }
}

/**
 * Perform a "loop size" reflection of a frame, warning in some conditions.
 */
PRIVATE long ReverseFunction::reverseFrame(Loop* l, long frame)
{
    long loopFrames = l->getFrames();

    if (frame > loopFrames) {
        // Reflecting this will result in a negative frame counter 
        // which we will never reach until you reverse again and reflect
        // them back into this dimension.  I actually kind of like that,
        // but it probably shouldn't happen if Reverse cancels the
        // current mode.
        // This shouldn't happen for non-event frames
        Trace(l, 1, "Reverse: Attempting to reflect frame greater than loop size!\n");
    }
    else if (frame == loopFrames) {
        // Must be quantized to a loop boundary, assume
        // we can "loop" this back to zero.  Shouldn't see this
        // if afterLoop is on in the ReverseEvent event,  but see it a lot
		// handling quantized transition frames.
        frame = 0;
    }

	return (loopFrames - frame - 1);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
