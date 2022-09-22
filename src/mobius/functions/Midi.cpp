/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Functions related to MIDI messages.
 * 
 *  MidiStart
 *  MidiStop
 *  MidiOut
 *
 */

#include <stdio.h>
#include <string.h>
#include <memory.h>

#include "Util.h"

#include "MidiByte.h"
#include "MidiEvent.h"
#include "MidiInterface.h"

#include "Action.h"
#include "Event.h"
#include "EventManager.h"
#include "Expr.h"
#include "Function.h"
#include "Layer.h"
#include "Loop.h"
#include "Messages.h"
#include "Mobius.h"
#include "Mode.h"
#include "Synchronizer.h"
#include "Track.h"

//////////////////////////////////////////////////////////////////////
//
// MidiStartEvent
//
//////////////////////////////////////////////////////////////////////

class MidiStartEventType : public EventType {
  public:
	MidiStartEventType();
};

PUBLIC MidiStartEventType::MidiStartEventType()
{
	name = "MidiStart";
}

PUBLIC EventType* MidiStartEvent = new MidiStartEventType();

//////////////////////////////////////////////////////////////////////
//
// MidiStartFunction
//
//////////////////////////////////////////////////////////////////////

class MidiStartFunction : public Function {
  public:
	MidiStartFunction(bool mute);
	Event* scheduleEvent(Action* action, Loop* l);
	void prepareJump(Loop* l, Event* e, JumpContext* jump);
    void doEvent(Loop* l, Event* e);
    void undoEvent(Loop* l, Event* e);
  private:
	bool mute;
};

PUBLIC Function* MidiStart = new MidiStartFunction(false);
PUBLIC Function* MuteMidiStart = new MidiStartFunction(true);

PUBLIC MidiStartFunction::MidiStartFunction(bool b)
{
	eventType = MidiStartEvent;
	resetEnabled = true;
	noFocusLock = true;
	mute = b;

	// let it stack for after the switch
	switchStack = true;

	if (mute) {
		setName("MuteMidiStart");
		setKey(MSG_FUNC_MUTE_MIDI_START);
		setHelp("Mute, wait for the loop start point, then send MIDI start");
        alias1 = "MuteStartSong";
	}
	else {
		setName("MidiStart");
		setKey(MSG_FUNC_MIDI_START);
		setHelp("Wait for the loop start point, then send MIDI Start");
        alias1 = "StartSong";
	}
}

/**
 * This one is funny because we schedule two events, an immediate Mute
 * and a MidiStart at the end of the loop.  This could be a new mode, but
 * I'd rather it just be an event so it can be undone as usual.
 *
 * Like other functions if the Mute or MidiStart event comes back with the
 * reschedule flag set, do NOT schedule a play jump.  Note that
 * after the Mute is processed and we reschedule the MidiStart, we'll end
 * up back here, DO NOT schedule another Mute event or we'll go into 
 * a scheduling loop.  
 *
 * It is possible to keep overdubbing and otherwise mutating the loop
 * while there is a MidiStart event at the end, if the loop length is
 * changed we should try to reschedule the event.
 */
PUBLIC Event* MidiStartFunction::scheduleEvent(Action* action, Loop* l)
{
	Event* startEvent = NULL;
    EventManager* em = l->getTrack()->getEventManager();
	MobiusMode* mode = l->getMode();

	if (mode == ResetMode) {
		// send MidiStart regardless of Sync mode
		startEvent = Function::scheduleEvent(action, l);
		startEvent->frame = l->getFrame();
	}
	else {
		// since this isn't a mode, catch redundant invocations
		startEvent = em->findEvent(MidiStartEvent);
		if (startEvent != NULL) {
			// ignore
			startEvent = NULL;
		}
		else {
			Event* muteEvent = NULL;

			// disable quantization of the mute event
			action->escapeQuantization = true;

			// no MuteEvent if we're already muted, see comments above
			// !! but a Mute event may be scheduled, need to look for those too
			if (mute && !l->isMuteMode()) {

                // an internal event, have to clone the action
                Mobius* m = l->getMobius();
                Action* muteAction = m->cloneAction(action);

                // normally this takes ownership of the action
				muteEvent = Mute->scheduleEvent(muteAction, l);

                // a formality, the action should own it now
                m->completeAction(muteAction);
			}

			// go through the usual scheduling, but change the frame
			startEvent = Function::scheduleEvent(action, l);
			if (startEvent != NULL && !startEvent->reschedule) {

				// !! should this be the "end frame" or zero?
				startEvent->frame = l->getFrames();
				startEvent->quantized = true;

				// could remember this for undo?  
				// hmm, kind of like having them be independent
				//startEvent->addChild(muteEvent);

				if (!startEvent->reschedule && mute) {
					// schedule a play transition to come out of mute
					Event* trans = em->schedulePlayJump(l, startEvent);
				}
			}
		}
	}

	return startEvent;
}

void MidiStartFunction::prepareJump(Loop* l, Event* e, JumpContext* jump)
{
	// not sure what this would mean on a switch stack?
	// by current convention, e will always be a JumpPlayEvent unless
	// we're stacked
	bool switchStack = (e->type != JumpPlayEvent);

	if (this == MuteMidiStart && !switchStack) {

		// comming out of mute before a MidiStart is sent
		jump->unmute = true;
	}
}

/**
 * Handler for MidiStartEvent.
 * Normally this will be scheduled for the start point, but there's
 * nothing preventing them from going anywhere.
 *
 * Like MuteRealign, we have the possibility of activating the
 * MidiStartEvent event before we get to the MuteEvent.  So search for it
 * and remove it.
 * 
 */
void MidiStartFunction::doEvent(Loop* l, Event* e)
{
    MobiusMode* mode = l->getMode();

 	if (e->function == MuteMidiStart && mode != ResetMode) {
        // would be nice to bring this over here but we also need
        // it for RealignEvent
        l->cancelSyncMute(e);
    }

    Synchronizer* sync = l->getSynchronizer();
	sync->loopMidiStart(l);
}

void MidiStartFunction::undoEvent(Loop* l, Event* e)
{
	// our children do all the work
    // ?? what does this mean?
}


//////////////////////////////////////////////////////////////////////
//
// MidiStopEvent
//
//////////////////////////////////////////////////////////////////////

class MidiStopEventType : public EventType {
  public:
	MidiStopEventType();
};

PUBLIC MidiStopEventType::MidiStopEventType()
{
	name = "MidiStop";
}

PUBLIC EventType* MidiStopEvent = new MidiStopEventType();

//////////////////////////////////////////////////////////////////////
//
// MidiStopFunction
//
//////////////////////////////////////////////////////////////////////

class MidiStopFunction : public Function {
  public:
	MidiStopFunction();
	Event* scheduleEvent(Action* action, Loop* l);
    void doEvent(Loop* l, Event* e);
};

PUBLIC Function* MidiStop = new MidiStopFunction();

PUBLIC MidiStopFunction::MidiStopFunction() :
    Function("MidiStop", MSG_FUNC_MIDI_STOP)
{
	setHelp("Send MIDI Stop");
    alias1 = "StopSong";

	eventType = MidiStopEvent;
	resetEnabled = true;
	noFocusLock = true;
	// let it stack for after the switch
	switchStack = true;
}

PUBLIC Event* MidiStopFunction::scheduleEvent(Action* action, Loop* l)
{
	Event* e = Function::scheduleEvent(action, l);
	if (l->getMode() == ResetMode)
	  e->frame = l->getFrame();
	return e;
}

/**
 * Handler for MidiStopEvent.
 * Normally this will be scheduled for the start point, but there's
 * nothing preventing them from going anywhere.
 *
 * Like MuteRealign, we have the possibility of activating the
 * MidiStartEvent event before we get to the MuteEvent.  So search for it
 * and remove it.
 * 
 */
void MidiStopFunction::doEvent(Loop* l, Event* e)
{
    Synchronizer* sync = l->getSynchronizer();
	sync->loopMidiStop(l, true);
}

//////////////////////////////////////////////////////////////////////
//
// MidiOut
//
//////////////////////////////////////////////////////////////////////
/**
 * MidiOut is only usd in scripts.
 * It is treated as a global function so it will not cancel modes
 * or be quantized.
 */

class MidiOutFunction : public Function {
  public:
	MidiOutFunction();
	void invoke(Action* action, Mobius* m);
};

PUBLIC Function* MidiOut = new MidiOutFunction();

PUBLIC MidiOutFunction::MidiOutFunction() :
    Function("MidiOut", MSG_FUNC_MIDI_OUT)
{
	setHelp("Send MIDI message");

    global = true;

    // until we support binding arguments this
    // can only be called from scripts
    scriptOnly = true;

    // we have more than 1 arg so have to evaluate to an ExValueList
    variableArgs = true;
}

/**
 * MidiOut <status> <channel> <value> <velocity>
 * status: noteon noteoff control program 
 * channel: 0-15
 * value: 0-127
 * velocity: 0-127
 */
PUBLIC void MidiOutFunction::invoke(Action* action, Mobius* m)
{
	int status = 0;
	int channel = 0;
	int value = 0;
	int velocity = 0;

    ExValueList* args = action->scriptArgs;
    if (args != NULL) {

        if (args->size() > 0) {
            ExValue* arg = args->getValue(0);
            const char* type = arg->getString();

            if (StringEqualNoCase(type, "noteon")) {
                status = MS_NOTEON;
                // kludge: need a way to detect "not specified"
                if (velocity == 0)
                  velocity = 127;
            }
            else if (StringEqualNoCase(type, "noteoff")) {
                status = MS_NOTEOFF;
            }
            else if (StringEqualNoCase(type, "poly")) {
                status = MS_POLYPRESSURE;
            }
            else if (StringEqualNoCase(type, "control")) {
                status = MS_CONTROL;
            }
            else if (StringEqualNoCase(type, "program")) {
                status = MS_PROGRAM;
            }
            else if (StringEqualNoCase(type, "touch")) {
                status = MS_TOUCH;
            }
            else if (StringEqualNoCase(type, "bend")) {
                status = MS_BEND;
            }
            else if (StringEqualNoCase(type, "start")) {
                status = MS_START;
            }
            else if (StringEqualNoCase(type, "continue")) {
                status = MS_CONTINUE;
            }
            else if (StringEqualNoCase(type, "stop")) {
                status = MS_STOP;
            }
            else {
                Trace(1, "MidiOutFunction: invalid status %s\n", type);
            }
        }

        if (args->size() > 1) {
            ExValue* arg = args->getValue(1);
            channel = arg->getInt();
        }

        if (args->size() > 2) {
            ExValue* arg = args->getValue(2);
            value = arg->getInt();
        }

        if (args->size() > 3) {
            ExValue* arg = args->getValue(3);
            velocity = arg->getInt();
        }
    }
            
	if (status > 0) {

        MobiusContext* con = m->getContext();
        MidiInterface* midi = con->getMidiInterface();

        MidiEvent* mevent = midi->newEvent(status, channel, value, velocity);
        midi->send(mevent);
        mevent->free();
    }

}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
