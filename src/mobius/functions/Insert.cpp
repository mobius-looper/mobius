/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Insert and friends.
 * 
 * TODO: Long-press Insert = Replace
 * We'll have to unwind some machinery, but at least output
 * will have been muted so we won't hear the transition.
 *
 * TODO: If we're in a loop entered with SwitchDuration=OnceReturn
 * and there is a return Transtion to the previous loop, Insert
 * ReTriggers the current loop.  The transition is not removed.
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
#include "Mobius.h"
#include "Mode.h"
#include "Messages.h"
#include "Segment.h"
#include "Synchronizer.h"
#include "Track.h"

/**
 * Experiment.
 * It has been off for awhile, not sure what the thinking was.
 */
bool DeferInsertShift = false;

//////////////////////////////////////////////////////////////////////
//
// InsertMode
//
//////////////////////////////////////////////////////////////////////

class InsertModeType : public MobiusMode {
  public:
	InsertModeType();
};

InsertModeType::InsertModeType() :
    MobiusMode("insert", MSG_MODE_INSERT)
{
	extends = true;
	rounding = true;
	recording = true;
}

MobiusMode* InsertMode = new InsertModeType();

//////////////////////////////////////////////////////////////////////
//
// InsertEvent
//
//////////////////////////////////////////////////////////////////////

class InsertEventType : public EventType {
  public:
	InsertEventType();
};

PUBLIC InsertEventType::InsertEventType()
{
	name = "Insert";
	reschedules = true;
}

PUBLIC EventType* InsertEvent = new InsertEventType();

//////////////////////////////////////////////////////////////////////
//
// InsertEndEvent
//
//////////////////////////////////////////////////////////////////////

class InsertEndEventType : public EventType {
  public:
	InsertEndEventType();
};

PUBLIC InsertEndEventType::InsertEndEventType()
{
	name = "InsertEnd";
	reschedules = true;
}

PUBLIC EventType* InsertEndEvent = new InsertEndEventType();

/****************************************************************************
 *                                                                          *
 *   								INSERT                                  *
 *                                                                          *
 ****************************************************************************/

class InsertFunction : public Function {
  public:
	InsertFunction(bool sus, bool unrounded);
	bool isSustain(Preset* p);
	Event* invoke(Action* action, Loop* l);
    void invokeLong(Action* action, Loop* l);
	Event* scheduleEvent(Action* action, Loop* l);
	void prepareJump(Loop* l, Event* e, JumpContext* jump);
	void doEvent(Loop* l, Event* e);
  private:
	bool isUnroundedEnding(Preset* p, Function* f);
	bool only;
	bool unrounded;
};

// should we have an UnroundedInsert?
PUBLIC Function* Insert = new InsertFunction(false, false);
PUBLIC Function* SUSInsert = new InsertFunction(true, false);
PUBLIC Function* SUSUnroundedInsert = new InsertFunction(true, true);

PUBLIC InsertFunction::InsertFunction(bool sus, bool unrounded)
{
	eventType = InsertEvent;
    mMode = InsertMode;
	majorMode = true;
	mayCancelMute = true;
	quantized = true;
	switchStack = true;
	switchStackMutex = true;
	cancelReturn = true;

	this->sustain = sus;
	this->unrounded = unrounded;

	if (!sus) {
		setName("Insert");
		setKey(MSG_FUNC_INSERT);
        // controlled by SustainFunctions parameter
        maySustain = true;
	}
	else if (unrounded) {
		setName("SUSUnroundedInsert");
		setKey(MSG_FUNC_SUS_UINSERT);
	}
	else {
		setName("SUSInsert");
		setKey(MSG_FUNC_SUS_INSERT);
	}

}

PUBLIC bool InsertFunction::isSustain(Preset* p)
{
    bool isSustain = sustain;
    if (!isSustain) {
        // formerly sensntive to InsertMode=Sustain
        const char* susfuncs = p->getSustainFunctions();
        if (susfuncs != NULL)
          isSustain = (IndexOf(susfuncs, "Insert") >= 0);
    }
    return isSustain;
}

/**
 * Return true if the function being used to end the multiply
 * will result in an unrounded multiply.
 */
PRIVATE bool InsertFunction::isUnroundedEnding(Preset* p, Function* f)
{
    return (f == Record || 
            f == SUSUnroundedInsert);
}

/**
 * Formerly tried to implement EDPish InsertMode but that was removed.
 * The one remaining EDPism we have is that Insert during Reset mode
 * can be used to select the next preset.
 */
Event* InsertFunction::invoke(Action* action, Loop* l) 
{
    Event* event = NULL;
    MobiusConfig* config = l->getMobius()->getInterruptConfiguration();

    if (config->isEdpisms() && l->isReset() && action->down) {
        // EDPism
        // Insert in Reset selects the next preset
        changePreset(action, l, true);
    }
    else {
        MobiusMode* mode = l->getMode();

        // EDPism
        // Insert in Mute becomes SamplePlay
        // We now call this RestartOnce.  
        // If isMuteCancel is false, then just insert silently

        if (config->isEdpisms() && 
            mode == MuteMode && isMuteCancel(l->getPreset())) {
            
            // ignore up transitions of a SUSInsert
            if (action->down) {
                // change the Function so it looks right
                action->setFunction(RestartOnce);
                event = RestartOnce->invoke(action, l);
            }
        }
        else if (!isSustain(l->getPreset()) ||
                 (mode == InsertMode && !action->down) ||
                 (mode != InsertMode && action->down)) {

            Function::invoke(action, l);
        }
    }

	return action->getEvent();
}

/**
 * Event scheduler for Insert.
 */
Event* InsertFunction::scheduleEvent(Action* action, Loop* l)
{
	Event* event = NULL;
    EventManager* em = l->getTrack()->getEventManager();
	MobiusMode* mode = l->getMode();

	if (mode == RecordMode) {
		// Logic to handle an Insert alternate ending is currently
		// burried in the RecordStopEvent handler.  Would be nice to 
		// factor this out but Record/Insert is rather special.  
		// Don't schedule an InsertEvent event, but still have
        // to return the RecordStopEvent for the script wait.
		if (action->down)
		  event = em->findEvent(RecordStopEvent);
	}
	else if (mode == RehearseMode) {
		// docs unclear, supposed to stop and keep the last loop if
		// still recording, not sure what happens if we're playing
		// I don't think this is subject to quantization, but all the
		// other rehearse endings are, may want to move this up into
		// Function::invoke
        // !! This can't possibly work, we're not setting up the
        // right play jump

		if (action->down) {
			event = em->getFunctionEvent(action, l, this);
			// make it unquantized, could have this logic in getFunctionEvent?
			event->frame = l->getFrame() + l->getInputLatency();
			em->addEvent(event);
		}
	}
	else {
        event = Function::scheduleEvent(action, l);

        if (event != NULL) {

            // formerly set event->afterLoop here, I guess to be
            // like Multiply, but this causes the insert to happen
            // at the front of the loop which is wrong
            //event->afterLoop = true;

            // need to mute at the insert point
            //if (!event->reschedule && !event->pending)
            if (!event->reschedule) {

                // ignore if we're already muted?
                // would testing the mMute flag be more reliable? 
                // perhaps but that might come back on before this
                // transition frame?

                if (!l->isMuteMode()) {
                    MobiusMode* mode = l->getMode();
                    if (mode != RecordMode && 
                        mode != ReplaceMode && 
                        mode != InsertMode) {

                        em->schedulePlayJump(l, event);
                    }
                }
            }
        }
	}

	return event;
}

/**
 * Perform a Replace instead.  Not compatible with conversion to SUSInsert?
 * Have to set up an insert for 400ms?
 *
 * !! If the current mode is Mute, this is supposed to restart the
 * loop, let it play once, then mute again.
 */
PUBLIC void InsertFunction::invokeLong(Action* action, Loop* l)
{
}

/**
 * This one is more complicated than most because the jump event
 * can be associated with either an InsertEvent to start the insert,
 * or InsertEventEvent to end it.
 */
PUBLIC void InsertFunction::prepareJump(Loop* l, Event* e, JumpContext* jump)
{
	Event* parent = e->getParent();

	if (parent == NULL) {
		Trace(l, 1, "InsertFunction: jump event with no parent!");
	}
	else if (parent->type == InsertEndEvent) {
		// We're ending the insert mute.
		// If mMuteMode is on, it must mean that MuteCancel does not
		// include the Insert function, so we have to preserve the current
		// mute state.  Don't need to check MuteCancel, we must have done
		// that when entering Insert.

		if (!l->isMuteMode()) {
			jump->unmute = true;
			// a mute can't be stacked here, right?
			jump->mute = false;
		}
	}
	else {
		// starting the insert
		jump->mute = true;
	}
}


/****************************************************************************
 *                                                                          *
 *                                   EVENTS                                 *
 *                                                                          *
 ****************************************************************************/

// KLUDGE: selects new insert behavior
PUBLIC void InsertFunction::doEvent(Loop* l, Event* e)
{
    // unfortunately this is still too tightlyl wound around Loop

    if (e->type == InsertEvent) {

      l->insertEvent(e);
    }

    else if (e->type == InsertEndEvent) {

        bool unrounded = l->isUnroundedEnding(e->getInvokingFunction());
        if (unrounded)
          Trace(l, 2, "Loop: Unrounded insertion of %ld frames\n",
                l->getFrame() - l->getModeStartFrame());

        Layer* record = l->getRecordLayer();
        InputStream* istream = l->getInputStream();
        record->endInsert(istream, l->getFrame(), unrounded);

        if (unrounded) {
            // we had been preplaying the record layer above the inserted
            // cycles, unrounding chopped a section out so we have to resync
            l->recalculatePlayFrame();
            OutputStream* ostream = l->getOutputStream();
            ostream->setLayerShift(true);
        }

        if (!DeferInsertShift)
          l->shift(false);

        Synchronizer* sync = l->getSynchronizer();
        sync->loopResize(l, false);

        Trace(l, 2, "Loop: Resuming playback at %ld\n", l->getPlayFrame());

        // resume play or overdub, should already have unmuted
        if (l->isMute() && !l->isMuteMode()) {
            Trace(l, 1, "Loop: Still muted at end of Insert!\n");
            l->setMute(false);
        }

        l->resumePlay();
        l->setModeStartFrame(0);
        l->validate(e);
    }

}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
