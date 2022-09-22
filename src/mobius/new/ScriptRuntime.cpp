/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Script runtime execution state.  Only one of these owned by Mobius.
 *
 */

#include "Action.h"
#include "Mobius.h"
#include "MobiusThread.h"
#include "Script.h"
#include "Track.h"

#include "ScriptRuntime.h"

/****************************************************************************
 *                                                                          *
 *                               SCRIPT RUNTIME                             *
 *                                                                          *
 ****************************************************************************/

ScriptRuntime::ScriptRuntime(class Mobius* m)
{
    mMobius = m;
    mScripts = NULL;
    mScriptThreadCounter = 0;
}

ScriptRuntime::~ScriptRuntime()
{
    // !! we have historically not freed the script interpteter list,   
    // maybe because we force cancel first?
    if (mScripts != NULL)
      Trace(1, "Leaking script interpreters!");
}

/**
 * RunScriptFunction global function handler.
 * RunScriptFunction::invoke calls back to to this.
 */
PUBLIC void ScriptRuntime::runScript(Action* action)
{
    Function* function = NULL;
    Script* script = NULL;

	// shoudln't happen but be careful
	if (action == NULL) {
        Trace(1, "Mobius::runScript without an Action!\n");
    }
    else {
        function = action->getFunction();
        if (function != NULL)
          script = (Script*)function->object;
    }

    if (script == NULL) {
        Trace(1, "Mobius::runScript without a script!\n");
    }
    else if (script->isContinuous()) {
        // These are called for every change of a controller.
        // Assume options like !quantize are not relevant.
        startScript(action, script);
    }
    else if (action->down || script->isSustainAllowed()) {
			
        if (action->down)
          Trace(mMobius, 2, "Mobius: runScript %s\n", 
                script->getDisplayName());
        else
          Trace(mMobius, 2, "Mobius: runScript %s UP\n",
                script->getDisplayName());

        // If the script is marked for quantize, then we schedule
        // an event, the event handler will eventually call back
        // here, but with TriggerEvent so we know not to do it again.

        if ((script->isQuantize() || script->isSwitchQuantize()) &&
            action->trigger != TriggerEvent) {

            // Schedule it for a quantization boundary and come back later.
            // This may look like what we do in doFunction() but  there
            // are subtle differences.  We don't want to go through
            // doFunction(Action,Function,Track)

            Track* track = mMobius->resolveTrack(action);
            if (track != NULL) {
                action->setResolvedTrack(track);
                function->invoke(action, track->getLoop());
            }
            else if (!script->isFocusLockAllowed()) {
                // script invocations are normally not propagated
                // to focus lock tracks
                Track* t = mMobius->getTrack();
                action->setResolvedTrack(t);
                function->invoke(action, t->getLoop());
            }
            else {
                // like doFunction, we have to clone the Action
                // if there is more than one destination track
                int nactions = 0;
                for (int i = 0 ; i < mMobius->getTrackCount() ; i++) {
                    Track* t = mMobius->getTrack(i);
                    if (mMobius->isFocused(t)) {
                        if (nactions > 0)
                          action = mMobius->cloneAction(action);

                        action->setResolvedTrack(t);
                        function->invoke(action, t->getLoop());

                        nactions++;
                    }
                }
            }
        }
        else {
            // normal global script, or quantized script after
            // we receive the RunScriptEvent
            startScript(action, script);
        }
    }
}

/**
 * Helper to run the script in all interested tracks.
 * Even though we're processed as a global function, scripts can
 * use focus lock and may be run in multiple tracks and the action
 * may target a group.
 */
PRIVATE void ScriptRuntime::startScript(Action* action, Script* script)
{
	Track* track = mMobius->resolveTrack(action);

	if (track != NULL) {
        // a track specific binding
		startScript(action, script, track);
	}
    else if (action->getTargetGroup() > 0) {
        // a group specific binding
        int group = action->getTargetGroup();
        int nactions = 0;
        for (int i = 0 ; i < mMobius->getTrackCount() ; i++) {
            Track* t = mMobius->getTrack(i);
            if (t->getGroup() == group) {
                if (nactions > 0)
                  action = mMobius->cloneAction(action);
                startScript(action, script, t);
            }
        }
    }
	else if (!script->isFocusLockAllowed()) {
		// script invocations are normally not propagated
		// to focus lock tracks
		startScript(action, script, mMobius->getTrack());
	}
	else {
        int nactions = 0;
		for (int i = 0 ; i < mMobius->getTrackCount() ; i++) {
			Track* t = mMobius->getTrack(i);
            if (mMobius->isFocused(t)) {
                if (nactions > 0)
                  action = mMobius->cloneAction(action);
                startScript(action, script, t);
                nactions++;
            }
		}
	}
}

/**
 * Internal method to launch a new script.
 *
 * !! Think more about how reentrant scripts and sustain scripts interact,
 * feels like we have more work here.
 */
PRIVATE void ScriptRuntime::startScript(Action* action, Script* s, Track* t)
{

	if (s->isContinuous()) {
        // ignore up/down, down will be true whenever the CC value is > 0

		// Note that we do not care if there is a script with this
		// trigger already running.  Controller events come in rapidly,
		// it is common to have several of them come in before the next
		// audio interrupt.  Schedule all of them, but must keep them in order
		// (append to the interpreter list rather than push).  
		// We could locate existing scripts that have not yet been
		// processed and change their trigger values, but there are race
		// conditions with the audio interrupt.

		//Trace(mMobius, 2, "Mobius: Controller script %ld\n",
		//(long)(action->triggerValue));

        // can SI point back to Runtieme?
		ScriptInterpreter* si = new ScriptInterpreter(mMobius, t);
        si->setNumber(++mScriptThreadCounter);

		// Setting the script will cause a refresh if !autoload was on.
		// Pass true for the inUse arg if we're still referencing it.
		si->setScript(s, isInUse(s));

		// pass trigger info for several built-in variables
        si->setTrigger(action);

		addScript(si);
	}
	else if (!action->down) {
		// an up transition, should be an existing interpreter
		ScriptInterpreter* si = findScript(action, s, t);
		if (si == NULL) {
            if (s->isSustainAllowed()) {
                // shouldn't have removed this
                Trace(mMobius, 1, "Mobius: SUS script not found!\n");
            }
            else {
                // shouldn't have called this method
                Trace(mMobius, 1, "Mobius: Ignoring up transition of non-sustainable script\n");
            }
		}
		else {
			ScriptLabelStatement* l = s->getEndSustainLabel();
			if (l != NULL) {
                Trace(mMobius, 2, "Mobius: Script thread %s: notify end sustain\n", 
                      si->getTraceName());
                si->notify(l);
            }

			// script can end now
			si->setSustaining(false);
		}
	}
	else {
		// can only be here on down transitions
		ScriptInterpreter* si = findScript(action, s, t);

		if (si != NULL) {

			// Look for a label to handle the additional trigger
			// !! potential ambiguity between the click and reentry labels
			// The click label should be used if the script is in an end state
			// waiting for a click.  The reentry label should be used if
			// the script is in a wait state?

			ScriptLabelStatement* l = s->getClickLabel();
			if (l != NULL) {
				si->setClickCount(si->getClickCount() + 1);
				si->setClickedMsecs(0);
                if (l != NULL)
                  Trace(mMobius, 2, "Mobius: Script thread %s: notify multiclick\n",
                        si->getTraceName());
			}
			else {
				l = s->getReentryLabel();
                if (l != NULL)
                  Trace(mMobius, 2, "Mobius: Script thread %s notify reentry\n",
                        si->getTraceName());
			}

			if (l != NULL) {
				// notify the previous interpreter
				// TODO: might want some context here to make decisions?
				si->notify(l);
			}
			else {
				// no interested label, just launch another copy
				si = NULL;
			}
		}

		if (si == NULL) {
			// !! need to pool these
			si = new ScriptInterpreter(mMobius, t);
            si->setNumber(++mScriptThreadCounter);

			// Setting the script will cause a refresh if !autoload was on.
			// Pass true for the inUse arg if we're still referencing it.
			si->setScript(s, isInUse(s));
            si->setTrigger(action);

			// to be elibible for sustaining, we must be in a context
			// that supports it *and* we have to have a non zero trigger id
			if (s->isSustainAllowed() &&
				action != NULL && 
                action->isSustainable() && 
				action->id > 0) {

				si->setSustaining(true);
			}

			// to be elibible for multi-clicking, we don't need anything
			// special from the action context
			if (s->isClickAllowed() && 
				action != NULL && action->id > 0) {

				si->setClicking(true);
			}

			// !! if we're in TriggerEvent, then we need to 
			// mark the interpreter as being past latency compensation

			// !! what if we're in the Script function context?  
			// shouldn't we just evalute this immediately and add it to 
			// the list only if it suspends? that would make it behave 
			// like Call and like other normal function calls...

			addScript(si);
		}
	}
}

/**
 * Add a script to the end of the interpretation list.
 *
 * Keeping these in invocation order is important for !continuous
 * scripts where we may be queueing several for the next interrupt but
 * they must be done in invocation order.
 */
PRIVATE void ScriptRuntime::addScript(ScriptInterpreter* si)
{
	ScriptInterpreter* last = NULL;
	for (ScriptInterpreter* s = mScripts ; s != NULL ; s = s->getNext())
	  last = s;

	if (last == NULL)
	  mScripts = si;
	else
	  last->setNext(si);
    
    Trace(2, "Mobius: Starting script thread %ls",
          si->getTraceName());
}

/**
 * Return true if the script is currently being run.
 *
 * Setting the script will cause a refresh if !autoload was on.
 * We don't want to do that if there are any other interpreters
 * using this script!
 * 
 * !! This is bad, need to think more about how autoload scripts die gracefully.
 */
PRIVATE bool ScriptRuntime::isInUse(Script* s)
{
	bool inuse = false;

	for (ScriptInterpreter* running = mScripts ; running != NULL ; 
		 running = running->getNext()) {
		if (running->getScript() == s) {
			inuse = true;
			break;
		}
	}
	
	return inuse;
}

/**
 * On the up transition of a script trigger, look for an existing script
 * waiting for that transition.
 *
 * NOTE: Some obscure but possible problems if we're using a !focuslock
 * script and the script itself plays with focuslock.  The script may
 * not receive retrancy or sustain callbacks if it turns off focus lock.
 *
 */
PRIVATE ScriptInterpreter* ScriptRuntime::findScript(Action* action, Script* s,
                                                     Track* t)
{
	ScriptInterpreter* found = NULL;

	for (ScriptInterpreter* si = mScripts ; si != NULL ; si = si->getNext()) {

		// Note that we use getTrack here rather than getTargetTrack since
		// the script may have changed focus.
		// Q: Need to distinguish between scripts called from within
		// scripts and those triggered by MIDI?

		if (si->getScript() == s && 
			si->getTrack() == t &&
			si->isTriggerEqual(action)) {

			found = si;
			break;
		}
	}

	return found;
}

/**
 * Called by Mobius after a Function has completed.  
 * Must be called in the interrupt.
 * 
 * Used in the implementation of Function waits which are broken, need
 * to think more about this.
 *
 * Also called by MultiplyFunction when long-Multiply converts to 
 * a reset?
 * 
 */
PUBLIC void ScriptRuntime::resumeScript(Track* t, Function* f)
{
	for (ScriptInterpreter* si = mScripts ; si != NULL ; si = si->getNext()) {
		if (si->getTargetTrack() == t) {

            // Don't trace this, we see them after every function and this
            // doesn't work anyway.  If we ever make it work, this should first
            // check to see if the script is actually waiting on this function
            // before saying anything.
            //Trace(2, "Mobius: Script thread %s: resuming\n",
            //si->getTraceName());
            si->resume(f);
        }
	}
}

/**
 * Called by Track::trackReset.  This must be called in the interrupt.
 * 
 * Normally when a track is reset, we cancel all scripts running in the track.
 * The exception is when the action is being performed BY a script which
 * is important for the unit tests.  Old logic in trackReset was:
 *
 *   	if (action != NULL && action->trigger != TriggerScript)
 *   	  mMobius->cancelScripts(action, this);
 *
 * I'm not sure under what conditions action can be null, but I'm worried
 * about changing that so we'll leave it as it was and not cancel
 * anything unless we have an Action.
 *
 * The second part is being made more restrictive so now we only keep
 * the script that is DOING the reset alive.  This means that if we have
 * scripts running in other tracks they will be canceled which is usually
 * what you want.  If necessary we can add a !noreset option.
 *
 * Also note that if the script uses "for" statements the track it may actually
 * be "in" is not necessarily the target track.
 *
 *     for 2
 *        Wait foo
 *     next
 *
 * If the script is waiting in track 2 and track 2 is reset the script has
 * to be canceled.  
 *
 */
PUBLIC void ScriptRuntime::cancelScripts(Action* action, Track* t)
{
    if (action == NULL) {
        // we had been ignoring these, when can this happen?
        Trace(mMobius, 2, "Mobius::cancelScripts NULL action\n");
    }
    else {
        // this will be the interpreter doing the action
        // hmm, rather than pass this through the Action, we could have
        // doScriptMaintenance set a local variable for the thread
        // it is currently running
        ScriptInterpreter* src = (ScriptInterpreter*)(action->id);
        bool global = (action->getFunction() == GlobalReset);

        for (ScriptInterpreter* si = mScripts ; si != NULL ; si = si->getNext()) {

            if (si != src && (global || si->getTargetTrack() == t)) {
                Trace(mMobius, 2, "Mobius: Script thread %s: canceling\n",
                      si->getTraceName());
                si->stop();
            }
        }
    }
}

/**
 * Called at the start of each audio interrupt to process
 * script timeouts and remove finished scripts from the run list.
 * Some of the scripts need to know the millisecond size of the buffer
 * so the sampleRate and frame count is passed.
 */
PUBLIC void ScriptRuntime::doScriptMaintenance(int sampleRate, long frames)
{
	int msecsInBuffer = (int)((float)frames / ((float)sampleRate / 1000.0));
	// just in case we're having rounding errors, make sure this advances
	if (msecsInBuffer == 0) msecsInBuffer = 1;

	for (ScriptInterpreter* si = mScripts ; si != NULL ; si = si->getNext()) {

		// run any pending statements
		si->run();

		if (si->isSustaining()) {
			// still holding down the trigger, check sustain events
			Script* script = si->getScript();
			ScriptLabelStatement* label = script->getSustainLabel();
			if (label != NULL) {
			
				// total we've waited so far
				int msecs = si->getSustainedMsecs() + msecsInBuffer;

				// number of msecs in a "long press" unit
				int max = script->getSustainMsecs();

				if (msecs < max) {
					// not at the boundary yet
					si->setSustainedMsecs(msecs);
				}
				else {
					// passed a long press boundary
					int ticks = si->getSustainCount();
					si->setSustainCount(ticks + 1);
					// don't have to be real accurate with this
					si->setSustainedMsecs(0);
                    Trace(mMobius, 2, "Mobius: Script thread %s: notify sustain\n",
                          si->getTraceName());
					si->notify(label);
				}
			}
		}

		if (si->isClicking()) {
			// still waiting for a double click
			Script* script = si->getScript();
			ScriptLabelStatement* label = script->getEndClickLabel();
			
            // total we've waited so far
            int msecs = si->getClickedMsecs() + msecsInBuffer;

            // number of msecs to wait for a double click
            int max = script->getClickMsecs();

            if (msecs < max) {
                // not at the boundary yet
                si->setClickedMsecs(msecs);
            }
            else {
                // waited long enough
                si->setClicking(false);
                si->setClickedMsecs(0);
                // don't have to have one of these
                if (label != NULL) {
                    Trace(mMobius, 2, "Mobius: Script thread %s: notify end multiclick\n",
                          si->getTraceName());
                    si->notify(label);
                }
            }
		}
	}

	freeScripts();
}

/**
 * Remove any scripts that have completed.
 * Because we call track/loop to free references to this interpreter,
 * this may only be called from within the interrupt handler.
 * Further, this should now only be called by doScriptMaintenance,
 * anywhere else we run the risk of freeing a thread that 
 * doScriptMaintenance is still iterating over.
 */
PRIVATE void ScriptRuntime::freeScripts()
{
	ScriptInterpreter* next = NULL;
	ScriptInterpreter* prev = NULL;

	for (ScriptInterpreter* si = mScripts ; si != NULL ; si = next) {
		next = si->getNext();
		if (!si->isFinished())
		  prev = si;
		else {
			if (prev == NULL)
			  mScripts = next;
			else
			  prev->setNext(next);

			// sigh, a reference to this got left on Events scheduled
			// while it was running, even if not Wait'ing, have to clean up
			for (int i = 0 ; i < mMobius->getTrackCount() ; i++)
			  mMobius->getTrack(i)->removeScriptReferences(si);

			// !! need to pool these
			// !! are we absolutely sure there can't be any ScriptEvents
			// pointing at this?  These used to live forever, it scares me

            Trace(mMobius, 2, "Mobius: Script thread %s: ending\n",
                  si->getTraceName());

			delete si;
		}
	}
}

/**
 * Special internal target used to notify running scripts when
 * something interesting happens on the outside.
 * 
 * Currently there is only one of these, from MobiusTread when
 * it finishes processing a ThreadEvent that a script might be waiting on.
 *
 * Note that this has to be done by probing the active scripts rather than
 * remembering the invoking ScriptInterpreter in the event, because
 * ScriptInterpreters can die before the events they launch are finished.
 */
PUBLIC void ScriptRuntime::doScriptNotification(Action* a)
{
    if (a->trigger != TriggerThread)
      Trace(1, "Unexpected script notification trigger!\n");

    // unusual way of passing this in, but target object didn't seem
    // to make sense
    ThreadEvent* te = a->getThreadEvent();
    if (te == NULL)
      Trace(1, "Script notification action without ThreadEvent!\n");
    else {
        for (ScriptInterpreter* si = mScripts ; si != NULL ; 
             si = si->getNext()) {

            // this won't advance the script, it just prunes the reference
            si->finishEvent(te);
        }

        // The ThreadEvent is officially over, we get to reclaim it
        a->setThreadEvent(NULL);
        delete te;
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

