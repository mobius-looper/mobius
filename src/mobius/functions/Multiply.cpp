/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Multiply and friends.
 * 
 * TODO: Unrounded Multiply during Rounding (page 5-36)
 * During the rounding period, pressing Record should stop the multiply
 * and generate a new layer, *then* any alternate endings are executed.
 * Not sure if the end events are happening correctly.
 * 
 * TODO: If we're in a loop entered with SwitchDuration=OnceReturn and there
 * is a Return to the previous loop, cancel the return or ignore multiply?
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
#include "Stream.h"
#include "Layer.h"
#include "Loop.h"
#include "Track.h"
#include "Mobius.h"
#include "Mode.h"
#include "Messages.h"
#include "Segment.h"
#include "Synchronizer.h"
#include "SyncState.h"
#include "Track.h"

//////////////////////////////////////////////////////////////////////
//
// MultiplyMode
//
//////////////////////////////////////////////////////////////////////

class MultiplyModeType : public MobiusMode {
  public:
	MultiplyModeType();
};

MultiplyModeType::MultiplyModeType() :
    MobiusMode("multiply", MSG_MODE_MULTIPLY)
{
	extends = true;
	rounding = true;
	recording = true;
	altFeedbackSensitive = true;
}

MobiusMode* MultiplyMode = new MultiplyModeType();

//////////////////////////////////////////////////////////////////////
//
// MultiplyEvent
//
//////////////////////////////////////////////////////////////////////

class MultiplyEventType : public EventType {
  public:
	MultiplyEventType();
};

PUBLIC MultiplyEventType::MultiplyEventType()
{
	name = "Multiply";
	reschedules = true;
}

PUBLIC EventType* MultiplyEvent = new MultiplyEventType();

//////////////////////////////////////////////////////////////////////
//
// MultiplyEndEvent
//
//////////////////////////////////////////////////////////////////////

class MultiplyEndEventType : public EventType {
  public:
	MultiplyEndEventType();
};

PUBLIC MultiplyEndEventType::MultiplyEndEventType()
{
	name = "MultiplyEnd";
	reschedules = true;
}

PUBLIC EventType* MultiplyEndEvent = new MultiplyEndEventType();

/****************************************************************************
 *                                                                          *
 *   							   MULTIPLY                                 *
 *                                                                          *
 ****************************************************************************/

class MultiplyFunction : public Function {
  public:
	MultiplyFunction(bool sus, bool unrounded);
	bool isSustain(Preset* p);
    Event* invoke(Action* action, Loop* l);
	Event* scheduleEvent(Action* action, Loop* l);
    void invokeLong(Action* action, Loop* l);
    void doEvent(Loop* loop, Event* event);
  private:
	bool isUnroundedEnding(Preset* p, Function* f);
    void pruneCycles(Loop* l, int cycles, bool unrounded, bool remultiply);
	bool unrounded;
};

// should we have an UnroundedMultiply?
PUBLIC Function* Multiply = new MultiplyFunction(false, false);
PUBLIC Function* SUSMultiply = new MultiplyFunction(true, false);
PUBLIC Function* SUSUnroundedMultiply = new MultiplyFunction(true, true);

PUBLIC MultiplyFunction::MultiplyFunction(bool sus, bool unr)
{
	eventType = MultiplyEvent;
    mMode = MultiplyMode;
	majorMode = true;
	mayCancelMute = true;
	quantized = true;
	// normally causes a SoundCopy
	switchStack = true;
	switchStackMutex = true;
	cancelReturn = true;

	sustain = sus;
	unrounded = unr;

	if (!sus) {
		setName("Multiply");
		setKey(MSG_FUNC_MULTIPLY);
        maySustain = true;
	}
	else if (unrounded) {
		setName("SUSUnroundedMultiply");
		setKey(MSG_FUNC_SUS_UMULTIPLY);
	}
	else {
		setName("SUSMultiply");
		setKey(MSG_FUNC_SUS_MULTIPLY);
	}
}

PUBLIC bool MultiplyFunction::isSustain(Preset* p)
{
    bool isSustain = sustain;
    if (!isSustain) {
        // formerly sensntive to InsertMode=Sustain
        const char* susfuncs = p->getSustainFunctions();
        if (susfuncs != NULL)
          isSustain = (IndexOf(susfuncs, "Multiply") >= 0);
    }
    return isSustain;
}

/**
 * Return true if the function being used to end the multiply
 * will result in an unrounded multiply.
 */
PRIVATE bool MultiplyFunction::isUnroundedEnding(Preset* p, Function* f)
{
    return (f == Record || 
            f == SUSUnroundedMultiply);
}

/**
 * Overload invoke() to support some EDPisms:
 *  
 * When Sync=In, Mute/Multiply performs a Realign (equivalent to MuteRealign).
 *
 * When Sync=OutUserStart, sends MIDI START at the next local start point
 * (equivalent to MuteMidiStart).
 * 
 * But the phrase "from the front panel" is used a lot in the docs, so maybe
 * these only work from the front panel?
 *
 * The new MuteCancel parameter affects these.  Only have EDP semantics
 * when Multiply is a mute cancel function.  This is counter intuitive
 * since Multiply will not cancel the mute, but it doesn't seem worth
 * another obscure parameter to control this.
 * 
 * TODO: Long-press Multiply makes it a SUS multiply.  First 400ms of
 * audio will play however since we'll think it is a Multiply at first.
 * Muultiply in 
 */
PUBLIC Event* MultiplyFunction::invoke(Action* action, Loop* l) 
{
    Event* event = NULL;
    MobiusConfig* config = l->getMobius()->getInterruptConfiguration();

	// If we're in Realign mode, cancel the realign.
	// Not sure if this is supposed to happen but since Mute/Multiply
	// puts you into a realign, it seems reasonable to have another
	// Multiply cancel it.  Note that Realign isn't a mode, it's just an event.

    EventManager* em = l->getTrack()->getEventManager();
	Event* realign = em->findEvent(RealignEvent);
	if (realign != NULL) {
		// We're in Realign "mode", cancel it
		if (action->down)
          em->freeEvent(realign);
	}
    else if (config->isEdpisms() && 
             l->getMode() == MuteMode && isMuteCancel(l->getPreset())) {
        // EDPism: Multiply in Mute becomes MuteRealign
        // !! Hey what about MuteMidiStart not supporting that

        // RealignFunction will not schedule an event if
        // this track is unsynced, in that case we could either
        // ignore it or do a normal multiply and break out of mute.
        // I'm thinking I'd like a parameter to specify whether
        // Multiply in Mute does Realign, or just Multiply.

        if (action->down) {
            // we don't have to clone the action since
            // we're not going to schedule a Multiply event
            event = MuteRealign->invoke(action, l);
        }
    }
    else {
        // normal invoke
        event = Function::invoke(action, l);
    }

    return event;
}

/**
 * Multiply event scheduler.
 */
Event* MultiplyFunction::scheduleEvent(Action* action, Loop* l)
{
	Event* event = NULL;

    event = Function::scheduleEvent(action, l);
    if (event != NULL) {
        // if we're not in multiply and we're quantized
        // to a loop boundary, be sure to process it 
        // after the loop back to frame zero
        // !! does it make sense for this to be a Function flag?
        // yes then we wouldn't have to overload scheduleEvent
        event->afterLoop = true;
    }

	return event;
}

/**
 * Performs TrackReset (aka GeneralReset on the EDP) if current 
 * loop is in Reset, otherwise do a Substitute.
 * 
 * NOTE: But the Multiply still runs for 400ms.
 * NOTE: Some ambiguity on 5-37, suggests Long-Multiply becomes SUSMultiply.
 *
 * !! If the loop has entered Multiply mode, then a long press is supposed
 * to convert it to SUSMultiply.
 * 
 */
PUBLIC void MultiplyFunction::invokeLong(Action* action, Loop* l)
{
    if (l->getMode() == ResetMode) {
        Track* t = l->getTrack();

        t->reset(NULL);
		// inform any scripts with a TrackReset function wait
		t->getMobius()->resumeScript(t, TrackReset);
    }
}

/****************************************************************************
 *                                                                          *
 *                                   EVENTS                                 *
 *                                                                          *
 ****************************************************************************/

PUBLIC void MultiplyFunction::doEvent(Loop* l, Event* e)
{
    // unfortunately this is still too tightlyl wound around Loop

    if (e->type == MultiplyEvent) {
        MobiusMode* mode = l->getMode();
        if (mode == RehearseMode)
          l->cancelRehearse(e);

        else if (l->isRecording())
          l->finishRecording(e);

        l->cancelPrePlay();
        l->checkMuteCancel(e);

        l->setModeStartFrame(l->getFrame());
        l->setRecording(true);
        l->setMode(MultiplyMode);
    }
    else if (e->type == MultiplyEndEvent) {

        bool pruned = false;
        Preset* p = l->getPreset();
        Preset::MultiplyMode mmode = p->getMultiplyMode();
        Layer* play = l->getPlayLayer();

        // I'm not liking the uncontrollable nature of unrounded multiply,
        // it's either cycle quantize or nothing.  Either allow the ending
        // Record to be quantized or add a new Cut function.

        if (l->isUnroundedEnding(e->getInvokingFunction())) {
             pruneCycles(l, 1, true, false);
             pruned = true;
        }
        else if (mmode == Preset::MULTIPLY_NORMAL &&
                 (play != NULL && play->getCycles() > 1)) {

            OutputStream* output = l->getOutputStream();

            // adjust the loop to contain only those cycles that
            // were within the multiply zone

            long multiplyLength = l->getFrame() - l->getModeStartFrame();
            if (multiplyLength < output->latency)
              multiplyLength = output->latency;

            long cycleFrames = l->getCycleFrames();
            if (cycleFrames > 0) {
                int saveCycles = multiplyLength / cycleFrames;
                if (saveCycles > 0) {
                    // the only difference between this and unrounded multiply
                    // is that we're quantized differently and preserve
                    // the cycle count
                    pruneCycles(l, saveCycles, false, true);
                    pruned = true;
                }
            }
        }
        else if (play != NULL) {
            // ?? can we really not have a play layer here, doubt it

            // Formerly did not shift here, but if we don't and another
            // multiply is done before we shift, playLocal gets confused,
            // probably similar issues if we insert.  It makes sense to always
            // shift after a multiply since the structure changed.  
            // !! defer shift if we didn't actually add a cycle?
            l->shift(false);

            // warp the frame relative to the new layer, shift() will
            // have set the flag to prevent a fade
            l->recalculatePlayFrame();

            Synchronizer* s = l->getSynchronizer();
            s->loopResize(l, false);
        }

        // we're now at frame zero, to avoid event timing warnings 
        // in EventManager::processEvent, set the event frame back to zero too
        if (pruned)
          e->frame = 0;

        // resume play/overdub
        l->resumePlay();
        l->setModeStartFrame(0);
        l->validate(e);
    }

}

/**
 * Helper for MultipyEndEvent.
 * Restructure the loop after a multiply and shift.
 */
PRIVATE void MultiplyFunction::pruneCycles(Loop* l, int cycles, 
                                           bool unrounded, bool remultiply)
{
    OutputStream* output = l->getOutputStream();
    long modeStartFrame = l->getModeStartFrame();

	long multiplyLength = l->getFrame() - modeStartFrame;
	if (multiplyLength < output->latency) {
		Trace(l, 2, "Multiply: Unrounded multiply less than output latency %ld to %ld\n",
			  multiplyLength, output->latency);
		multiplyLength = output->latency;
	}

	if (unrounded)
	  Trace(l, 2, "Multiply: Unrounded multiply to %ld frames\n", 
			multiplyLength);

	if (remultiply)
	  Trace(l, 2, "Multiply: Remultiply to %ld cycles, %ld frames\n", 
			(long)cycles, multiplyLength);
	  
	// We will normally have started preplay of the frames at the
	// beginning of the multiply region.  If we started playing
	// after, then we're about to remove them and it will be impossible
	// to do a tail fade (and we would get a click anyway).
	if (l->getPlayFrame() >= (modeStartFrame + multiplyLength))
	  Trace(l, 1, "Loop: Multiply play frame too high!\n");
   
    Layer* record = l->getRecordLayer();
	record->splice(l->getInputStream(), modeStartFrame, multiplyLength, cycles);

	l->shift(false);

    // unrounded multiply on EDP sends MS_START, second arg true
    Synchronizer* sync = l->getSynchronizer();
	sync->loopResize(l, unrounded);

	// Subtlety: shift() set the Stream's layer shift flag to prevent a fade in
	// which is what you usually want when transitioning from the record
	// layer back to the play layer.  Here though, we've restructured
	// the layer so we may need to fade in based on layer/frame info
	// !! is this enough, feels like there is a case where we don't
	// want a fade but the layer changed
	output->setLayerShift(false);

	// this was a fundamental disruption of the loop
	// we've been pre-playing the record loop but now have to resync
	l->setFrame(0);
	l->recalculatePlayFrame();

	// two shifts, the first to adjust for truncation at the beginning
	// of the loop, then another to bring the events for the next loop
	// into the window
    EventManager* em = l->getTrack()->getEventManager();

	if (modeStartFrame > 0) {

		em->shiftEvents(modeStartFrame);

		// adjust the frame counter the stream is maintaining
		// ?? will this ever not be true
		if (output->getLastLayer() == l->getPlayLayer())
		  output->adjustLastFrame(-modeStartFrame);
	}

    em->shiftEvents(l->getFrames());
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
