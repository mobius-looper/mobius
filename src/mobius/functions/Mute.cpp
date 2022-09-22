/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Mute en Regalia
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
#include "MobiusConfig.h"
#include "Mode.h"
#include "Messages.h"
#include "Segment.h"
#include "Synchronizer.h"
#include "Track.h"

//////////////////////////////////////////////////////////////////////
//
// MuteMode
//
//////////////////////////////////////////////////////////////////////

class MuteModeType : public MobiusMode {
  public:
	MuteModeType();
};

MuteModeType::MuteModeType() :
    MobiusMode("mute", MSG_MODE_MUTE)
{
}

MobiusMode* MuteMode = new MuteModeType();

/**
 * A minor mode displayed when the Mute major mode is
 * caused by GlobalMute.
 * I don't like this.
 */
class GlobalMuteModeType : public MobiusMode {
  public:
	GlobalMuteModeType();
};

GlobalMuteModeType::GlobalMuteModeType() :
    MobiusMode("globalMute", MSG_MODE_GLOBAL_MUTE)
{
}

MobiusMode* GlobalMuteMode = new GlobalMuteModeType();

//////////////////////////////////////////////////////////////////////
//
// PauseMode
//
//////////////////////////////////////////////////////////////////////

/**
 * This will never actually be set in the Track, we just report
 * it in the TrackState when in Mute mode with the Pause option.
 */
class PauseModeType : public MobiusMode {
  public:
	PauseModeType();
};

PauseModeType::PauseModeType() :
    MobiusMode("pause", MSG_MODE_PAUSE)
{
}

MobiusMode* PauseMode = new PauseModeType();

/**
 * A minor mode displayed when the Pause major mode is
 * caused by GlobalPause.
 * I don't like htis.
 */
class GlobalPauseModeType : public MobiusMode {
  public:
	GlobalPauseModeType();
};

GlobalPauseModeType::GlobalPauseModeType() :
    MobiusMode("globalPause", MSG_MODE_GLOBAL_PAUSE)
{
}

MobiusMode* GlobalPauseMode = new GlobalPauseModeType();

//////////////////////////////////////////////////////////////////////
//
// MuteEvent
//
//////////////////////////////////////////////////////////////////////

class MuteEventType : public EventType {
  public:
	MuteEventType();
};

PUBLIC MuteEventType::MuteEventType()
{
	name = "Mute";
}

PUBLIC EventType* MuteEvent = new MuteEventType();

/****************************************************************************
 *                                                                          *
 *   								 MUTE                                   *
 *                                                                          *
 ****************************************************************************/

class MuteFunction : public Function {
  public:
	MuteFunction(bool pause, bool sus, bool restart, bool glob, bool absolute);
    void invoke(Action* action, Mobius* m);
    Event* invoke(Action* action, Loop* l);
    void invokeLong(Action* action, Loop* l);
	Event* scheduleEvent(Action* action, Loop* l);
	Event* rescheduleEvent(Loop* l, Event* prev, Event* next);
	void prepareJump(Loop* l, Event* e, JumpContext* jump);
    void doEvent(Loop* l, Event* e);
    void globalPause(Action* action, Mobius* m);
    void globalMute(Action* action, Mobius* m);
  private:
	bool mToggle;
	bool mMute;
	bool mPause;
	bool mRestart;
};

// SUS first for longFunction
PUBLIC Function* SUSMute = new MuteFunction(false, true, false, false, false);
PUBLIC Function* SUSPause = new MuteFunction(true, true, false, false, false);

PUBLIC Function* Mute = new MuteFunction(false, false, false, false, false);
PUBLIC Function* MuteOn = new MuteFunction(false, true, false, false, true);
PUBLIC Function* MuteOff = new MuteFunction(false, false, false, false, true);
PUBLIC Function* Pause = new MuteFunction(true, false, false, false, false);
PUBLIC Function* SUSMuteRestart = new MuteFunction(false, true, true, false, false);
PUBLIC Function* GlobalMute = new MuteFunction(false, false, false, true, false);
PUBLIC Function* GlobalPause = new MuteFunction(true, false, false, true, false);

// TODO: SUSGlobalMute and SUSGlobalPause seem useful

PUBLIC MuteFunction::MuteFunction(bool pause, bool sus, bool start, bool glob,
								  bool absolute)
{
	eventType = MuteEvent;
    mMode = MuteMode;
	majorMode = true;
	minorMode = true;
	quantized = true;
	switchStack = true;
	cancelReturn = true;
	mToggle = true;
	mMute = true;
	mPause = pause;
	mRestart = start;
	global = glob;

	// Added MuteOn for RestartOnce, may as well have MuteOff now that
	// we're a minor mode.  For the "absolute" functions, the SUS flag
	// becomes the on/off state I suppose you could want SUSMuteOn that 
	// does't toggle, but it's rare and you can use scripts.

	if (absolute) {
		mToggle = false;
		mMute = sus;
	}
	else {
		sustain = sus;
	}

	// don't need all combinations, but could have
	if (global) {
		noFocusLock = true;
		if (mPause) {
			setName("GlobalPause");
			setKey(MSG_FUNC_GLOBAL_PAUSE);
		}
		else {
			setName("GlobalMute");
			setKey(MSG_FUNC_GLOBAL_MUTE);
		}
	}
	else if (mRestart) {
		setName("SUSMuteRestart");
		setKey(MSG_FUNC_SUS_MUTE_RESTART);
	}
	else if (mPause) {
		if (sustain) {
			setName("SUSPause");
			setKey(MSG_FUNC_SUS_PAUSE);
		}
		else {
			setName("Pause");
			setKey(MSG_FUNC_PAUSE);
			longFunction = SUSPause;
		}
	}
	else if (sustain) {
		setName("SUSMute");
		setKey(MSG_FUNC_SUS_MUTE);
	}
	else if (mToggle) {
		// toggle, or force on
		setName("Mute");
		setKey(MSG_FUNC_MUTE);

		// !! in addition to switching to SUSMute, this is also supposed
		// to force MuteMode=Continuous, the only way for Loop to know
		// that is to pass down the longPress status in Action
		// and Event
		longFunction = SUSMute;

		// On switch, if loop is not empty, enter mute.
		// If loop is empty, LoopCopy=Sound then mute.
		// Toggle of mute already stacked.
		// Cancel all other record modes.
		switchStack = true;
		switchStackMutex = true;
	}
	else if (mMute) {
		setName("MuteOn");
		setKey(MSG_FUNC_MUTE_ON);
		switchStack = true;
		switchStackMutex = true;
		scriptOnly = true;
	}
	else {
		setName("MuteOff");
		setKey(MSG_FUNC_MUTE_OFF);
		scriptOnly = true;
	}
}

/**
 * EDPism: Mute in reset selects the previous preset.
 * UPDATE: Now that mute is a minor mode, I'm removing this feature
 * unless a hidden flag is set.
 *
 */
PUBLIC Event* MuteFunction::invoke(Action* action, Loop* loop)
{
	Event* event = NULL;
    MobiusConfig* config = loop->getMobius()->getInterruptConfiguration();

	// !! Note how we use the static function pointer rather than checking
	// mToggle, this is actually potentially simpler way to do function
	// variants since Loop and others are using them that way..

	if (this == Mute && loop->isReset() && action->down) {
		trace(action, loop);

        if (config->isEdpisms()) {
            changePreset(action, loop, false);
        }
        else {
            bool newMode = !loop->isMuteMode();
            loop->setMuteMode(newMode);
            loop->setMute(newMode);
        }
    }
    else {
		// formerly ignored if the global flag was set but we need to 
		// pass this down and have it handled by Loop::muteEvent
		event = Function::invoke(action, loop);
	}

	return event;
}

/**
 * If we're recording, don't schedule a mute since we won't
 * have played anything yet.
 * !! This should be a noop since invoke() called scheduleRecordStop?
 * 
 */
Event* MuteFunction::scheduleEvent(Action* action, Loop* l)
{
    EventManager* em = l->getTrack()->getEventManager();

    // do basic event scheduling
    Event* event = Function::scheduleEvent(action, l);

	// and a play transition event
	if (event != NULL && !event->reschedule) {
		if (!mRestart || action->down) {
			// this will toggle mute
			em->schedulePlayJump(l, event);
		}
		else {
			// The up transition of a SUSMuteRestart
			// could have a RestartEvent to make this easier?
			// !! this is a MIDI START condition
			// !! this is no longer taking us out of mute??
			Event* jump = em->schedulePlayJump(l, event);

			// !! why are we doing this here, shouldn't this be part
			// of the jumpPlayEvent handler?
			jump->fields.jump.nextLayer = l->getPlayLayer();
			jump->fields.jump.nextFrame = 0;
		}
	}

	return event;
}

/**
 * This one is slightly complicated because the Mute event might
 * have been created for the MidiStart function and we need to 
 * retain the reference to that function.
 */
PUBLIC Event* MuteFunction::rescheduleEvent(Loop* l, Event* previous, Event* next)
{
	Event* neu = Function::rescheduleEvent(l, previous, next);
	neu->function = next->function;

	return neu;
}

/**
 * Adjust jump properties when entering or leaving mute mode.
 * Event currently must be the JumpPlayEvent for a MuteEvent.
 * 
 * This is complicated by the MuteMode preset parameter.
 */
PUBLIC void MuteFunction::prepareJump(Loop* l, Event* e, JumpContext* jump)
{
	// by current convention, e will always be a JumpPlayEvent unless
	// we're stacked
	bool switchStack = (e->type != JumpPlayEvent);

	if (switchStack) {
		// The switch case is complicated because of MuteCancel handling,
		// but we shouldn't be here
		Trace(l, 1, "MuteFunction: A place we shouldn't be!\n");
	}
	else {
		// !! hey some of the other prepareJump handlers aren't looking
		// at the event preset, should they?
		Preset* preset = e->getPreset();
		if (preset == NULL)
		  preset = l->getPreset();

		Event* primary = e;
		if (e->getParent() != NULL)
		  primary = e->getParent();

		// logic is complicated by the two confusing mute flags
		bool muteFlag = l->isMute();
		bool muteModeFlag = l->isMuteMode();

        Function* invoker = primary->getInvokingFunction();

		if (invoker == MuteMidiStart ||
			invoker == MuteRealign) {

			// enter mute if we're not already there
			// note that we're testing the mMute flag!
			if (!muteFlag)
			  jump->mute = true;
		}
		else if (muteModeFlag && primary->function == MuteOn) {
			// a noop, but since we may be considered a MuteCancel function,
			// jumpPlayEvent may have set the unmute flag
			jump->mute = true;
			jump->unmute = false;
		}
		else if (!muteModeFlag && primary->function == MuteOff) {
			// should be a noop
			jump->unmute = true;
		}
		else if (!muteModeFlag) {
			// entering mute
			jump->mute = true;
		}
		else if (l->getMode() != MuteMode) {
			// Must be a mute minor mode with something else going on
			// can't use Preset::MuteMode here because the current mode
			// may not have been ended properly yet, just turn it off 
			// and leave the position along.  Could be smarter about this.
			jump->unmute = true;
		}
		else {
			// Leaving mute mode

			Preset::MuteMode muteMode = preset->getMuteMode();

			// Mute/Undo toggles mute mode
			if (invoker == Undo) {
				if (muteMode == Preset::MUTE_START)
				  muteMode = Preset::MUTE_CONTINUE;
				else
				  muteMode = Preset::MUTE_START;
			}

			if (muteMode == Preset::MUTE_CONTINUE) {
				// will not have been advancing mPlayFrame so have to resync
				jump->frame = e->frame + 
					jump->inputLatency + jump->outputLatency;
				jump->frame = l->wrapFrame(jump->frame, jump->layer->getFrames());

				// we've already factored in latency loss so don't do it again
				jump->latencyLossOverride = true;
			}
			else if (muteMode == Preset::MUTE_START) {
				// start playing from the very beginning, but may have
				// had latency loss
				long latencyLoss = 0;

				// should always have a parent
				long muteFrame = e->frame;
                Event* parent = e->getParent();
				if (parent != NULL)
				  muteFrame = parent->frame;

				long transitionFrame = muteFrame - 
					jump->outputLatency - jump->inputLatency;
				if (transitionFrame < l->getFrame())
				  latencyLoss = e->frame - transitionFrame;
				
				if (latencyLoss < 0) {
					Trace(1, "MuteFunction: Invalid latency calculation during MuteMode=Start!\n");
					latencyLoss = 0;
				}

				// we've already factored in latency loss so don't do it again
				jump->latencyLossOverride = true;
				jump->frame = latencyLoss;
			}

			jump->unmute = true;
		}
	}
}

/**
 * TODO: Long-Mute is supposed to become SUSMultiply
 */
PUBLIC void MuteFunction::invokeLong(Action* action, Loop* l)
{
}

/**
 * Mute event handler.
 *
 * We will already have scheduled a JumpPlayEvent to change
 * the play status, here we just change modes.
 *
 * TODO: if cmd=SUSMuteRestart, then a down transition enters Mute, 
 * and an up transition does ReTrigger.  
 * ?? If we happened to already be in Mute when the function
 * was received should we trigger, or ignore it and wait for the up
 * transition?
 *
 * !! The flow here sucks out loud!
 * Solo makes a weird event with Solo as the invokingFunction, 
 * and MuteOn as the event function.  Need to retool this to use actions
 * or make Solo part of the same function family!
 */
PUBLIC void MuteFunction::doEvent(Loop* l, Event* e)
{
    Function* invoker = e->getInvokingFunction();

	if (invoker == MuteMidiStart ||	invoker == MuteRealign) {

		// enter mute if we're not already there
		// should this be a "minor" mode?

		if (!l->isMuteMode()) {
            EventManager* em = l->getTrack()->getEventManager();
            em->cancelReturn();
			if (l->getMode() == RehearseMode)
			  l->cancelRehearse(e);
			else if (l->isRecording())
			  l->finishRecording(e);
			l->setMute(true);
			l->setMode(MuteMode);
			l->setMuteMode(true);
		}
	}
	else {
		Preset* preset = e->getPreset();
		if (preset == NULL)
		  preset = l->getPreset();

		// pause mode can come from the preset or from specific functions
		Preset::MuteMode muteMode = preset->getMuteMode();
		if (e->function == Pause || e->function == GlobalPause)	
		  muteMode = Preset::MUTE_PAUSE;

        // ignore if we're already there
        if ((e->function == MuteOn && l->isMuteMode()) ||
            (e->function == MuteOff && !l->isMuteMode())) {
            Trace(l, 2, "Ignoring Mute event, already in desired state\n");
        }
		else if (l->isMuteMode()) {
			// turn mute off

            MobiusMode* mode = l->getMode();
			l->setMuteMode(false);

			if (mode != MuteMode) {
				// a "minor" mute
				// not supporting restart options and alternate endings
				// since we don't know what we're in
				// !! ugh, try to do the obvious thing in a few places
				// need more flags on the mode to let us know how to behave

				if (mode == ReplaceMode || mode == InsertMode) {
					// have to stay muted
				}
				else {
					l->setMute(false);
					l->resumePlay();
				}
			}
			else {
				// jumpPlayEvent should have already set this
				l->setMute(false);
				l->resumePlay();

				// undo alternate ending toggles mode
				if (e->getInvokingFunction() == Undo) {
					if (muteMode == Preset::MUTE_START)
					  muteMode = Preset::MUTE_CONTINUE;
					else
					  muteMode = Preset::MUTE_START;
				}

                Synchronizer* sync = l->getSynchronizer();

				if (muteMode == Preset::MUTE_START || 
					(e->function == SUSMuteRestart && !e->down)) {

					// will already have processed a mutePlayEvent and be
					// playing from the beginning, but there may have 
					// been latency loss so rederive from the play frame
					long newFrame = l->recalculateFrame(false);
					l->setFrame(newFrame);

					// Synchronizer may need to send MIDI START
					sync->loopRestart(l);
				}	
				else if (muteMode == Preset::MUTE_PAUSE) {
					// Resume sending MIDI clocks if we're the OutSyncMaster.
					// NOTE: Like many other places, we should have been
					// doing clock control InputLatency frames ago.
					sync->loopResume(l);
				}
			}
		}
		else if (!l->isMuteMode()) {

			// !! think about a "soft mute" that doesn't cancel the current
			// mode, we're almost there already with the mute minor mode

			// If we're in a loop entered with SwitchDuration=OnceReturn
			// and there is a ReturnEvent to the previous loop, Mute
			// cancels the transition as well as muting.
            EventManager* em = l->getTrack()->getEventManager();
            em->cancelReturn();

			if (l->getMode() == RehearseMode)
			  l->cancelRehearse(e);
			else if (l->isRecording())
			  l->finishRecording(e);

			l->setMode(MuteMode);
			l->setMuteMode(true);

			// JumpPlayEvent should have already set this
			l->setMute(true);

            Synchronizer* sync = l->getSynchronizer();

			// Should we stop the sequencer on SUSMuteRestart?
			if (muteMode == Preset::MUTE_PAUSE) {
				l->setPause(true);
				sync->loopPause(l);
			}
			else if (muteMode == Preset::MUTE_START) {
				// EDP stops clocks when we enter a mute in Start mode
				sync->loopMute(l);
			}
		}
	}

	// if this is not a GlobalMute, then GlobalMute is canceled
	if (e->function != GlobalMute && invoker != Solo)
	  l->getMobius()->cancelGlobalMute(NULL);

	l->validate(e);
}

/****************************************************************************
 *                                                                          *
 *                                GLOBAL MUTE                               *
 *                                                                          *
 ****************************************************************************/

PUBLIC void MuteFunction::invoke(Action* action, Mobius* m)
{
	if (action->down) {
		trace(action, m);

		if (mPause)
          globalPause(action, m);

		else
		  globalMute(action, m);
	}
}

/**
 * GlobalPause function handler.
 * 
 * This doesn't have any complex state like GlobalMute, it just
 * schedules the Pause functions in each track.  This wouldn't need to be a 
 * global function.
 */
PRIVATE void MuteFunction::globalPause(Action* action, Mobius* m)
{
    // punt and assume for now that we don't have to deal with
    // tracks that are already paused
    if (action->down) {
        for (int i = 0 ; i < m->getTrackCount() ; i++) {
            Track* t = m->getTrack(i);
            invoke(action, t->getLoop());
        }
    }
}

/**
 * GlobalMute global function handler.
 *
 * This is not just a simple invocation of Mute for all tracks.
 * It will mute any tracks that are currently playing, but leave muted
 * any tracks that are currently muted.  It then remembers the tracks
 * that were playing before the mute, and on the next mute will unmute just
 * those tracks. 
 */
PRIVATE void MuteFunction::globalMute(Action* action, Mobius* m)
{
	if (action->down) {

		// figure out what state we're in
		// we are in "global mute" state if any one of the tracks 
		// has a true "global mute restore" flag
		bool globalMuteMode = false;
		bool somePlaying = false;
		bool solo = false;

        int tracks = m->getTrackCount();

		for (int i = 0 ; i < tracks ; i++) {
			Track* t = m->getTrack(i);
			if (t->isGlobalMute())
			  globalMuteMode = true;
			if (t->isSolo())
			  solo = true;

			Loop* l = t->getLoop();
			if (!l->isReset() && !l->isMuteMode())
			  somePlaying = true;
		}

		// since we use global mute flags for solo, we've got 
		// some ambiguity over what this means
		// 1) mute the solo'd track, and restore the solo on the
		// next GlobalMute
		// 2) cancel the solo (unmuting the original tracks),
		// muting them all again, then restoring the original tracks
		// on the next GlobalMute
		// The second way feels more natural to me.
		if (solo) {
			// cancel solo, turn off globalMuteMode, 
			// and recalculate somePlaying
			solo = false;
			globalMuteMode = false;
			somePlaying = false;

			for (int i = 0 ; i < tracks ; i++) {
				Track* t = m->getTrack(i);
				Loop* l = t->getLoop();
				if (t->isGlobalMute()) {

                    // !! try to get rid of this, move it to Mute or
                    // make it schedule an event
					t->setMuteKludge(this, false);
					t->setGlobalMute(false);

				}
				else {
					// should only be unmuted if this
					// is the solo track
					t->setMuteKludge(this, true);
				}
				t->setSolo(false);
				if (!l->isReset() && !l->isMuteMode())
				  somePlaying = true;
			}
		}

		if (globalMuteMode) {
			// we're leaving global mute mode, only those tracks
			// that were on before, come back on
			for (int i = 0 ; i < tracks ; i++) {
				Track* t = m->getTrack(i);
				if (t->isGlobalMute()) {
					Loop* l = t->getLoop();
					if (!l->isReset()) {
						if (l->isMuteMode()) {
							// this was playing on the last GlobalMute
                            invoke(action, t->getLoop());
						}
						else {
							// track is playing, but the global mute flag
                            // is on, logic error somewhere
							Trace(l, 1, "Mobius: Dangling global mute flag!\n");
						}
					}
					t->setGlobalMute(false);
				}
			}
		}
		else if (somePlaying) {
			// entering global mute mode
			for (int i = 0 ; i < tracks ; i++) {
				Track* t = m->getTrack(i);
				Loop* l = t->getLoop();
				if (!l->isReset()) {
					if (l->isMuteMode()) {
						// make sure this is off
						t->setGlobalMute(false);
					}
					else {
						// remember we were playing, then mute
						// !! should we wait for the event handler in case this
						// is quantized and undone?
						// !! more to the point, shold GlobalMute even 
                        // be quantized?
						t->setGlobalMute(true);
						invoke(action, t->getLoop());
					}
				}
			}
		}
		else {
			// this is a special state requested by Matthias L.
			// if we're not in GlobalMute mode and everythign is muted
			// then unmute everything
			for (int i = 0 ; i < tracks ; i++) {
				Track* t = m->getTrack(i);
				Loop* l = t->getLoop();
				if (!l->isReset() && l->isMuteMode()) {
                    invoke(action, t->getLoop());
                }
            }
		}
	}
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
