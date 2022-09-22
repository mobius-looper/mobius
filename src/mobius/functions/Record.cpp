/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * All things recording...well most anyway.
 * Loop::recordEvent still has the handler for RecoerEvent, should move
 * that here but it's referenced by Loop::switchEvent, need a cleaner
 * interface.  Also have Loop::prepareLoop and Loop::finishRecording.
 *
 * This is the most complicated function and there are still
 * some lingering dependencies on Loop and Synchronizer, but it's
 * reasonably well encapsulated.  
 * UPDATE: Moving most of this to Synchronizer since it is too tightly
 * intertwined.
 *
 * 
 * General Recording Notes
 * 
 * Recording is complex due to synchronization and auto-record.
 * It is broken into these phases:
 *
 * 1) Synchronization wait
 *  
 *    Track enters Synchronize mode waiting for a pulse.  A pending
 * 	  RecordEvent is scheduled.  AutoRecord schedules a RecordStopEvent which
 *    will be pending if we're counting pulses, or have a frame calculated
 *    based on the tempo of the sync source or for track sync the size
 *    of the master loop.
 * 
 * 3) Synchronization start pulse
 *
 *    The pending RecordEvent is activated.
 *
 * 3) Latency wait
 * 
 *    After the sync start pulse arrives, certain sync modes (MIDI) must
 *    delay to compensate for input latency.
 *
 * 4) Record start
 *
 *    The RecordEvent scheduled during synchronization (or immediately
 *    if not synchronized) is reached and Record mode officially begins.
 *
 * 4) Recording
 * 
 *    Sync pulses for normal record may be used to increment the cycle
 *    count (could just do this as we record too...).  Sync pulses
 *    during auto record are ignored.
 *
 * 5) Record stop scheduling
 *
 *    When synchronizing, an ending of the record will schedule
 *    a RecordStopEvent, just like AutoRecord.
 *
 * 6) Record stop wait
 *
 *    Any functions entered during this period act as Record mode
 *    alternate endings, they may be stacked for a long time.
 *    Sync pulses are ignored, though we need to advance the external 
 *    pulse counters.
 * 
 * 7) Record stop
 *
 *    Call the machinery currently wrapped up under RecordStopEvent.
 *
 * Record Styles
 *
 * Legend:
 *
 * R - Record function invocation
 * A - Auto Record Function
 * S - SUSRecord down
 * U - SUSRecord up
 * s - RecordStartEvent
 * p - RecordStopEvent
 * x - RecordStopEvent moved
 *
 * 1) Simple
 *
 *    R------R
 *     s------p
 *
 * 2) Simple Auto
 *
 *    A--------
 *     s------p
 *
 * 3) Auto Extend
 *
 *    A---A---------
 *     s-----x-----p
 * 
 * 4) Auto Extend Undo
 *  
 *    A--AU-----
 *     s-----p-----x
 *
 * 5) 
 *    
 * THRESHOLD RECORDING
 *
 * Threshold recording is a bit like synchronized recording, in that
 * we enter a special mode and wait for an external event to begin
 * recording.  
 * 
 */

#include <stdio.h>
#include <memory.h>

#include "Action.h"
#include "Audio.h"
#include "Event.h"
#include "EventManager.h"
#include "Function.h"
#include "Layer.h"
#include "Loop.h"
#include "Messages.h"
#include "Mobius.h"
#include "Mode.h"
#include "Synchronizer.h"
#include "SyncState.h"
#include "Track.h"
#include "Setup.h"

/****************************************************************************
 *                                                                          *
 *   								EVENTS                                  *
 *                                                                          *
 ****************************************************************************/

//
// RecordEvent
//

class RecordEventType : public EventType {
  public:
	RecordEventType();
};

PUBLIC RecordEventType::RecordEventType()
{
	name = "Record";
}

PUBLIC EventType* RecordEvent = new RecordEventType();

//
// RecordStopEvent
//
// For normal unsynchronized recordings, this will be scheduled
// for Input Latency frames after the second Record trigger.
//
// For synchronized recordings, this will be scheduled after the
// second Record trigger, but adjusted for the ideal size based on
// the sync tempo.
// 
// For AutoRecord, it will be scheduled immediately based on the
// number of configured auto-record bars.
//
//

class RecordStopEventType : public EventType {
  public:
	RecordStopEventType();
};

PUBLIC RecordStopEventType::RecordStopEventType()
{
	name = "RecordStop";
}

PUBLIC EventType* RecordStopEvent = new RecordStopEventType();

/****************************************************************************
 *                                                                          *
 *                                    MODE                                  *
 *                                                                          *
 ****************************************************************************/

//
// Record
//

class RecordModeType : public MobiusMode {
  public:
	RecordModeType();
};

RecordModeType::RecordModeType() :
    MobiusMode("record", MSG_MODE_RECORD)
{
	extends = true;
	recording = true;
}

MobiusMode* RecordMode = new RecordModeType();

//
// Synchronize
//

class SynchronizeModeType : public MobiusMode {
  public:
	SynchronizeModeType();
};

SynchronizeModeType::SynchronizeModeType() :
    MobiusMode("synchronize", MSG_MODE_SYNCHRONIZE)
{
}

MobiusMode* SynchronizeMode = new SynchronizeModeType();

//
// Threshold
//

class ThresholdModeType : public MobiusMode {
  public:
	ThresholdModeType();
};

ThresholdModeType::ThresholdModeType() :
    MobiusMode("threshold", MSG_MODE_THRESHOLD)
{
}

MobiusMode* ThresholdMode = new ThresholdModeType();

//
// Run
//

class RunModeType : public MobiusMode {
  public:
	RunModeType();
};

RunModeType::RunModeType() :
    MobiusMode("run", MSG_MODE_RUN)
{
}

MobiusMode* RunMode = new RunModeType();

/****************************************************************************
 *                                                                          *
 *   								RECORD                                  *
 *                                                                          *
 ****************************************************************************/

class RecordFunction : public Function {
  public:
	RecordFunction(bool sus, bool aut);
	bool isSustain(Preset* p);

	Event* scheduleEvent(Action* action, Loop* l);
    Event* scheduleModeStop(Action* action, Loop* l);
    bool undoModeStop(Loop* l);

	void doEvent(Loop* loop, Event* event);

    void invokeLong(Action* action, Loop* l);
	void prepareSwitch(Loop* l, Event* e, SwitchContext* sc, JumpContext *jc);

  private:

	bool mAuto;
};

PUBLIC Function* Record = new RecordFunction(false, false);
PUBLIC Function* SUSRecord = new RecordFunction(true, false);
PUBLIC Function* AutoRecord = new RecordFunction(false, true);

PUBLIC RecordFunction::RecordFunction(bool sus, bool aut)
{
	eventType = RecordEvent;
    mMode = RecordMode;         // actually it depends
	majorMode = true;
	mayCancelMute = true;
	thresholdEnabled = true;
	resetEnabled = true;
	sustain = sus;
	mAuto = aut;
	switchStack = true;
	switchStackMutex = true;

	if (sustain) {
		setName("SUSRecord");
		setKey(MSG_FUNC_SUS_RECORD);
	}
	else if (mAuto) {
		setName("AutoRecord");
		setKey(MSG_FUNC_AUTO_RECORD);
		longPressable = true;
	}
	else {
		setName("Record");
		setKey(MSG_FUNC_RECORD);
		longPressable = true;
        // controlled by RecordFunctions parameter
        maySustain = true;
	}
}

PUBLIC bool RecordFunction::isSustain(Preset* p)
{
    bool isSustain = sustain;
    if (!isSustain) {
        // formerly sensitive to RecordMode
        // || (!mAuto && p->getRecordMode() == Preset::RECORD_SUSTAIN);
        const char* susfuncs = p->getSustainFunctions();
        if (susfuncs != NULL)
          isSustain = (IndexOf(susfuncs, "Record") >= 0);
    }
    return isSustain;
}

/**
 * Long Record resets the current loop.
 *
 * Control flow could be done in several ways, each at a higher
 * level of abstraction.
 *
 * 1) Call Track::loopReset as would be done in by Reset::invoke.
 * 2) Call Reset::invoke
 * 3) Call Track::doFunction or Track::setPendingFunction.
 *
 * doFunction is attractive as it makes it look like a special
 * form of trigger and we get some of the stuff Track does like
 * calling Mobius::resumeScript for free.  Track::loopReset is
 * more direct but it feels like long presses that convert into
 * functions should try to behave as much like the other function
 * as possible.
 *
 * Track::setPendingFunction doesn't work because it does management
 * of the mDownFunction which is the thing that got us into
 * invokeLong in the first place, so don't mess that up.
 */
PUBLIC void RecordFunction::invokeLong(Action* action, Loop* l)
{
	if (longPressable) {
        Track* track = l->getTrack();

		Trace(l, 2, "RecordFunction: long-press converts to Reset\n");

		// The action here will be a partial capture of the original 
		// Record down action.  It will have the function and the
		// trigger.  I don't think we should trash that by changing
		// the function, there is an "id" field in here that may need 
		// to be used with the function for tracking?

        // !! think about how all long press handlers should be
        // invoking other functions
        Mobius* m = l->getMobius();
        Action* a = m->newAction();

		// It might be nice to have a TriggerLong but the function
		// handlers may stil need to know script vs midi.  Another
		// "longPress" flag in TriggerEvent could be used.
        // !! No, if we go through Mobius::doAction we can't use the 
        // original trigger.  think about this...

        a->trigger = TriggerEvent;
        a->inInterrupt = true;
        a->down = true;
        a->setFunction(Reset);
        a->setResolvedTrack(l->getTrack());

        m->doAction(a);
	}
}

/**
 * Schedule a recording event.
 * If were already in Record mode should have called scheduleModStop first.
 * See file header comments about nuances.
 */
PUBLIC Event* RecordFunction::scheduleEvent(Action* action, Loop* l)
{
	Event* event = NULL;
	MobiusMode* mode = l->getMode();

	l->checkBreak();

	if (mode == MultiplyMode || mode == InsertMode) {
        // Unrounded multiply/insert alternate ending
		// default scheduler has the necessary logic
        if (action->down)
          event = Function::scheduleEvent(action, l);
	}
	else {
        // Synchronizer does all the work for sync recording,
        // auto record, and threshold record
        Synchronizer* sync = l->getSynchronizer();
        event = sync->scheduleRecordStart(action, this, l);
    }

    return event;
}

/**
 * Currently calling this only for the InvokeEvent that contains an
 * AutoRecord function but will eventually be doing this for all stacked
 * records.
 */
PUBLIC void RecordFunction::prepareSwitch(Loop* l, Event* e, 
                                          SwitchContext* actions,
										  JumpContext *jump)
{

	actions->loopCopy = false;
	actions->timeCopy = false;
	actions->record = true;
	actions->mute = false;
}

/****************************************************************************
 *                                                                          *
 *                           RECORD STOP SCHEDULING                         *
 *                                                                          *
 ****************************************************************************/

/**
 * This is currently the only overloaded implementation of
 * Function::scheduleStop but we need to start using this interface
 * for other modes.  Ideally should be asking the Mode to stop instead
 * of knowing which Functions go with certain modes.
 *
 * Forward to Synchronizer which handles everything related to record
 * start and stop.
 *
 */
PUBLIC Event* RecordFunction::scheduleModeStop(Action* action, Loop* loop)
{
    // Synchronizer handles everything
    Synchronizer* sync = loop->getSynchronizer();

    return sync->scheduleRecordStop(action, loop);
}

/**
 * Function overload to undo some aspect of the mode.
 * Return true if we were able to undo something.
 * 
 * This was added so we could encapsulate all of the sync/auto record ending
 * frame management in RecordFunction rather than in Loop or UndoFunction.
 * Currently RecordFunction is the only thing that implements this, to 
 * make it truly generic Loop should ask the MobiusMode to undo.
 *
 */
PUBLIC bool RecordFunction::undoModeStop(Loop* loop)
{
    Synchronizer* sync = loop->getSynchronizer();

    return sync->undoRecordStop(loop);
}

/****************************************************************************
 *                                                                          *
 *                               EVENT HANDLING                             *
 *                                                                          *
 ****************************************************************************/

/**
 * The function can schedule two event types: RecordEvent and RecordStopEvent.
 * RecordEvent is currently handled by redircting to Loop::recordEvent
 * because there are still some dependencies on that.
 *
 * RecordSTopEvent is usually scheduled to end Record mode but Multiply 
 * and Insert also use it since they can stop recording before the mode ending.
 * I HATE using the same event, Multiply/Insert should have their own.
 *
 * This is most often used to stop a Record or AutoRecord function but 
 * it is also used incorrectly by TrackSelect function to stop any kind
 * of recording (Overdub, Replace, etc) before changing tracks.
 * This is actually incorrect because Multiply/Insert are much harder
 * to shut down.
 * 
 */
PUBLIC void RecordFunction::doEvent(Loop* loop, Event* event)
{
    EventManager* em = loop->getTrack()->getEventManager();

    if (event->type == RecordEvent) {

        // !! This code is duplicated from Loop::recordEvent, if youi
        // chage that you'll need to change this.
        // this is a temporary situation until we can figure out how
        // to allow SwitchEvent to force a record start without scheduling
        // an event.  Or maybe it should just schedule an event...
        // The key is whether the passed event is significant.  Here
        // it will always be a RecordEventType but when Loop::switchEvent
        // calls Loop::recordEvent, the passed event will be SwitchEventType.

        // if this is the master track and we already had content, 
        // stop the clocks
        Synchronizer* sync = loop->getSynchronizer();
        sync->loopRecordStart(loop);

        // stop the current recording cleanly for undo
        loop->finishRecording(event);

        // If we were in Play mode, have to handle this like
        // a loop and reset mFrame back to zero, if we're starting
        // the initial recording it should already be there.
        loop->setFrame(0);
        loop->setPlayFrame(0);
        loop->setPlayLayer(NULL);			// should already be NULL
        loop->setPrePlayLayer(NULL);
        Layer* reclayer = loop->getRecordLayer();
        if (reclayer != NULL)
          reclayer->reset();		// should already be reset
        else {
            LayerPool* lp = loop->getMobius()->getLayerPool();
            reclayer = lp->newLayer(loop);
            loop->setRecordLayer(reclayer);
        }

        // Script Kludge: If this flag is set then we're doing audio
        // insertion and should suppress the usual fade in on the
        // next recording.
        reclayer->setFadeOverride(event->fadeOverride);

        // NOTE: When RecordMode=Sustain, very short taps can result
        // in both the start and end events being scheduled, need to preserve
        // the end event if one is there.
        // Also need to preserve the RecordStopEvent if this is an AutoRecord.
        // !! geez, is it ok for the function handler to flush the events
        // before scheduling us?  Unless there is the possibility of something
        // important happening before the ReordEvent is reached, we wouldn't
        // have to mess with this.   What if there are other events that need
        // to be flushed?  
	
        Event* end = em->findEvent(RecordEvent);
        Event* stop = em->findEvent(RecordStopEvent);
        if (end == NULL && stop == NULL)
          em->flushEventsExceptScripts();

        // If this is an AutoRecord, set the cycle count to give visual
        // clue as to the length of the loop.  Only do this for an unsync'd 
        // recording.  When syncing we will get pulse events and can increment
        // the cycle count during recording which looks nicer.
        if (stop != NULL && !stop->pending) {
            // bars is also the loop cycle count
            int bars = (int)stop->number;
            loop->setRecordCycles(bars);
        }

        loop->setRecording(true);
        // will already be set if this was a true event, but
        // not from pseudo-events like SwitchEvent
        loop->setMode(RecordMode);

        loop->checkMuteCancel(event);
        loop->setMute(loop->isMuteMode());
    }
    else if (event->type == RecordStopEvent) {

        // If this was the initial recording, and we haven't called 
        // prepareLoop yet do it now. This happens with synchronized
        // recordings that schedule the end a long time into the future
        // and can move it after scheduling so we can't prepare early.
        if (loop->getFrames() == 0) {
            Trace(loop, 2, "RecordStopEvent: Preparing loop\n");
            loop->prepareLoop(false, 0);
        }

        if (loop->getMode() != RecordMode) {
            // RecordStopEvent is scheduled by many actions as a universal
            // stopper for recording modes like Replace, Substitute in 
            // LoopTriggerFunction::scheduleTrigger.
            // !! I would really prefer that we have a generic mode stop
            // scheduling for all modes.
            loop->finishRecording(event);
        }
        else {      
            // turn this off now so alternate endings may turn it back on
            loop->setRecording(false);

            // Script Kludge: avoid a fade out on the right edge for
            // some tests
            Layer* record = loop->getRecordLayer();
            record->setFadeOverride(event->fadeOverride);

            // Normally will drop into play mode, but some modes
            // (Multiply, Insert) need to stop recording but stay
            // in that mode.    
            MobiusMode* newMode = PlayMode;
            long newFrame = loop->getFrame();

            // ?? Does the new recording reset all the existing layers
            // or do we keep them around for undo?

            // check for the REHEARSE alternate ending
            Function* endfunc = event->getInvokingFunction();
 
            if (endfunc == Rehearse) {
                // enter rehearse mode
                // setMode before shifting so shift() knows to
                // zero the new record layer rather than copying it
				// !! think about the logic here, cleanup
                Trace(loop, 2, "RecordStopEvent: Entering rehearse mode play phase\n");
                newMode = RehearseMode;
                loop->setMode(newMode);
                newFrame = 0;
            }

			// Let the synchronizer know so that it may start sending
			// MIDI clocks.  This is technically the wrong place, we should
			// have started clocks InputLatency frames earlier, but it is
			// not currently possible to reliably control the internal
			// MIDI clock in advance from all the necessary places.
            Synchronizer* sync = loop->getSynchronizer();
			sync->loopRecordStop(loop, event);

            // shift the current record layer so we can undo it
            loop->shift(false);

            if (endfunc == Insert) {
                // Insert a second cycle and continue recording in
                // insert mode.  Cycle insertion is performed
                // as the insert progresses, all we need to do here
                // is simiulate an insert start event.
				// !! don't want this here, just schedule an Insert?
                loop->insertEvent(NULL);

                // InsertEvent changed the mode, keep it
                newMode = loop->getMode();
            }
            else {
                // "loop" back to the start frame
                newFrame = 0;
            }

            // EDP had something called RecordMode=Safe that would
            // push feedback back to 127 whenever recording started.
            // We no longer have RecordMode but we do have  
            // RecordResetsFeedback.  
            // !! Why is this done here, can't we do it when the recording
            // is started?
            Preset* preset = loop->getPreset();
            if (preset->isRecordResetsFeedback()) {
                // note that we don't just put it to 127 like the EDP
                // it goes back to what is defined in the Setup
                int feedback = 127;
                Track* track = loop->getTrack();
                Mobius* mobius = loop->getMobius();
                MobiusConfig* config = mobius->getConfiguration();
                Setup* setup = config->getCurrentSetup();
                SetupTrack* strack = setup->getTrack(track->getRawNumber());
                if (strack != NULL)
                  feedback = strack->getFeedback();

                track->setFeedback(feedback);
            }

            // if we looped back to the start frame, shift any future events
            loop->setFrame(newFrame);
            if (newFrame == 0)
              em->shiftEvents(loop->getFrames());

            // if we're switching remember the new mode, but don't display it
            if (newMode != PlayMode)
              loop->setMode(newMode);
            else {
                // drop out of recording mode, resume overdub if left on
                loop->resumePlay();
            }
            
            // Loop::finishRecording must NOT call validate() here since
            // the mode may still be active (Multiply and Insert only?)
            // since we know we're ending a Record it's probably safe
            // to do that?  Well maybe not if we entered Rehearse

            // process stacked events
            // This is the new way for handling loop switches that
            // end a synchronized recording.  Need to evolve this
            // into a more general stacking event concept that both
            // Record and LoopSwitch use. 

            // !! This is all relatively generic and applies to all 
            // stacking events.  Move to EventManager

            Track* track = loop->getTrack();
            Event* nextChild = NULL;
            for (Event* child = event->getChildren() ; child != NULL ; 
                 child = nextChild) {
                nextChild = child->getSibling();

                if (child->type == JumpPlayEvent) {
                    // I don't think we can have these but ignore if we do
                    Trace(loop, 1, "RecordStopEvent: Unexpected JumpPlayEvent!\n");
                }
                else {
                    // in all cases these are removed from the parent event
                    track->enterCriticalSection("RecordStopEvent");
                    event->removeChild(child);
                    track->leaveCriticalSection();

                    // only expecting switches right now
                    if (child->type != SwitchEvent)
                      Trace(loop, 1, "RecordStopEvent: unexpected child event %s!\n",
                            child->type->name);
                    
                    // if we have a pending event, confirm it
                    // treat an unscheduled event like pending
                    if (child->pending || child->getList() == NULL) {
                        // should already be scheduled
                        if (child->pending && child->getList() == NULL) {
                            // not a problem, but I don't think it can happen
                            Trace(loop, 1, "RecordStopEvent: pending child not scheduled!\n");
                            em->addEvent(child);
                        }
                        else if (!child->pending && child->getList() == NULL) {
                            // we treat these like pending, should
                            // we require that the pending flag be set?
                            Trace(loop, 1, "RecordStopEvent: unscheduled child not pending!\n");
                        }
                        
                        // and confirm it
                        Trace(loop, 2, "RecordStopEvent: confirming pending child event: %s\n", child->type->name);
                        child->confirm(NULL, loop, CONFIRM_FRAME_IMMEDIATE);
                    }
                    else {
                        // already scheduled, leave it alone
                    }
                }
            }
        }
    }
    else {
        Trace(loop, 1, "RecordFunction::doEvent unexpected event type\n");
    }
}

/****************************************************************************
 *                                                                          *
 *   							   REHEARSE                                 *
 *                                                                          *
 ****************************************************************************/

/*
 * We define two modes, one for the play phase and another for
 * the record phase.  Only the play mode is actually set on the loop, the
 * UI uses the other one just to encapsulate the name.  Don't really
 * like this, perhaps give MobiusMode two sets of names?
 *
 */

class RehearseModeType : public MobiusMode {
  public:
	RehearseModeType(bool record);
};

RehearseModeType::RehearseModeType(bool record)
{
	if (record) {
		setName("rehearseRecord");
		setKey(MSG_MODE_REHEARSE_RECORD);
	}
	else {
		setName("rehearse");
		setKey(MSG_MODE_REHEARSE);
	}

	recording = true;
}

MobiusMode* RehearseMode = new RehearseModeType(false);
MobiusMode* RehearseRecordMode = new RehearseModeType(true);

/*
 * A function that will end a recording and go into Rehearse mode. 
 * Otherwise it is identical to Record.
 */
class RehearseFunction : public RecordFunction {
  public:
	RehearseFunction();
};

PUBLIC RehearseFunction::RehearseFunction() : RecordFunction(false, false)
{
    setName("Rehearse");
    setKey(MSG_FUNC_REHEARSE);
    maySustain = false;
}

PUBLIC Function* Rehearse = new RehearseFunction();


/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
