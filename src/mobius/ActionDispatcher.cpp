/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Helper class for Mobius, handles distribution of Actions.
 *
 */

#include "Action.h"
#include "Export.h"
#include "Mobius.h"
#include "MobiusConfig.h"
#include "MobiusThread.h"
#include "Script.h"
#include "ScriptRuntime.h"
#include "Track.h"
#include "TriggerState.h"

#include "ActionDispatcher.h"

/****************************************************************************
 *                                                                          *
 *                             ACTION DISPATCHER                            *
 *                                                                          *
 ****************************************************************************/

PUBLIC ActionDispatcher::ActionDispatcher(Mobius* m, CriticalSection* csect,
                                          ScriptRuntime* scripts)
{
    mMobius = m;
    mCsect = csect;
    mScripts = scripts;
    mTriggerState = new TriggerState();
    mActions = NULL;
    mLastAction = NULL;
}

PUBLIC ActionDispatcher::~ActionDispatcher()
{
    delete mTriggerState;
}

/**
 * Perform an action, either synchronously or scheduled for the next 
 * interrupt.  We assume ownership of the Action object and will free
 * it (or return it to the pool) when we're finished.
 *
 * This is the interface that must be called from anything "outside"
 * Mobius, which is any trigger that isn't the script interpreter.
 * Besides performing the Action, this is where we track down/up 
 * transitions and long presses.
 *
 * It may also be used by code "inside" the audio interrupt in which
 * case action->inInterrupt or TriggerEvent will be set.  
 *
 * If we're not in the interrupt, we usually defer all actions to the
 * beginning of the next interrupt.  The exceptions are small number
 * of global functions that have the "outsideInterrupt" option on.
 *
 * UI targets are always done synchronously since they don't effect 
 * the Mobius engine.
 * 
 * Originally we let TriggerHost run synchronously but that was wrong,
 * PluginParameter will track the last set value.
 *
 * Note that long press tracking is only done inside the interrupt
 * which means that the few functions that set outsideInterrupt and
 * the UI controls can't respond to long presses.  Seems fine.
 */
PUBLIC void ActionDispatcher::doAction(Action* a)
{
    bool ignore = false;
    bool defer = false;

    // catch auto-repeat on key triggers early
    // we can let these set controls and maybe parameters
    // but

    Target* target = a->getTarget();

    if (a->isRegistered()) {
        // have to clone these to do them...error in the UI
        Trace(1, "Attempt to execute a registered action!\n");
        ignore = true;
    }
    else if (a->repeat && a->triggerMode != TriggerModeContinuous) {
        Trace(3, "Ignoring auto-repeat action\n");
        ignore = true;
    }
    else if (a->isSustainable() && !a->down && 
             target != TargetFunction && target != TargetUIControl) {
        // Currently functions and UIControls are the only things that support 
        // up transitions.  UIControls are messy, generalize this to 
        // be more like a parameter with trigger properties.
        Trace(2, "Ignoring up transition action\n");
        ignore = true;
    }
    else if (a->down && a->longPress) {
        // this is the convention used by TriggerState to tell
        // us when a long-press has been reached on a previous trigger
        // we are in the interrupt and must immediately forward to the tracks
        // ?? would be better to do this as a new trigger type, 
        // like TriggerLong?  Not as easy to screw up but then we lose the 
        // original trigger type which might be interesting in scripts.
        // !! if we just use action->inInterrupt consistently we wouldn't
        // need to test this
        doActionNow(a);
    }
    else if (a->trigger == TriggerScript ||
             a->trigger == TriggerEvent ||
             // !! can't we use this reliably and not worry about trigger?
             a->inInterrupt ||
             target == TargetUIControl ||
             target == TargetUIConfig ||
             target == TargetBindings) {

        // Script and Event triggers are in the interrupt
        // The UI targets don't have restrictions on when they can change.
        // Bindings are used outside the interrupt.

        doActionNow(a);
    }
    else if (target == TargetFunction) {

        Function* f = (Function*)a->getTargetObject();
        if (f == NULL) {
            Trace(1, "Missing action Function\n");
        }
        else if (f->global && f->outsideInterrupt) {
            // can do these immediately
            f->invoke(a, mMobius);
        }
        else if (mMobius->getInterrupts() == 0) {
            // audio stream isn't running, suppress most functions
            // !! this is really dangerous, revisit this
            if (f->runsWithoutAudio) {
                // Have to be very careful here, current functions are:
                // FocusLock, TrackGroup, TrackSelect.
                // Maybe it would be better to ignore these and popup
                // a message? If these are sustainable or long-pressable
                // the time won't advance
                Trace(2, "Audio stream not running, executing %s\n", 
                      f->getName());
                doActionNow(a);
            }
            else {
                Trace(2, "Audio stream not running, ignoring %s",
                      f->getName());
            }
        }
        else
          defer = true;
    }
    else if (target == TargetParameter) {
        // TODO: Many parameters are safe to set outside
        // defrering may cause UI flicker if the change
        // doesn't happen right away and we immediately do a refresh
        // that puts it back to the previous value
        defer = true;
    }
    else {
        // controls are going away, Setup has to be inside, 
        // not sure about Preset
        defer = true;
    }
    
    if (!ignore && defer) {
        // pre 2.0 we used a ring buffer in Track for this that
        // didn't require a csect, consider resurecting that?
        // !! should have a maximum on this list?
        mCsect->enter("doAction");
        if (mLastAction == NULL)
          mActions = a;
        else 
          mLastAction->setNext(a);
        mLastAction = a;
        mCsect->leave("doAction");
    }
    else if (!a->isRegistered()) {
        mMobius->completeAction(a);
    }

}

/**
 * Process the action list when we're inside the interrupt.
 */
PUBLIC void ActionDispatcher::startInterrupt(long frames)
{
    Action* actions = NULL;
    Action* next = NULL;

    // Advance the long-press tracker too, this may cause other 
    // actions to fire.
    mTriggerState->advance(mMobius, frames);

    mCsect->enter("doAction");
    actions = mActions;
    mActions = NULL;
    mLastAction = NULL;
    mCsect->leave("doAction");

    for (Action* action = actions ; action != NULL ; action = next) {
        next = action->getNext();

        action->setNext(NULL);
        action->inInterrupt = true;

        doActionNow(action);

        mMobius->completeAction(action);
    }
}

/**
 * Process one action within the interrupt.  
 * This is also called directly by ScriptInterpreter.
 *
 * The Action is both an input and an output to this function.
 * It will not be freed but it may be returned with either the
 * mEvent or mThreadEvent fields set.  This is used by the 
 * script interpreter to schedule "Wait last" and "Wait thread" 
 * events.
 *
 * If an Action comes back with mEvent set, then the Action is
 * now owned by the Event and must not be freed by the caller.
 * It will be freed when the event is handled.  If mEvent is null
 * then the caller of doActionNow must return it to the pool.
 *
 * If the action is returned with mThreadEvent set it is NOT
 * owned and must be returned to the pool.
 * 
 * This will replicate actions that use group scope or 
 * must obey focus lock.  If the action is replicated only the first
 * one is returned, the others are freed.  This is okay for scripts
 * since we'll never do replication if we're called from a script.
 *
 * TODO: Consider doing the replication outside the interrupt and
 * leave multiple Actions on the list.
 *
 * Internally the Action may be cloned if a function decides to 
 * schedule more than one event.  The Action object passed to 
 * Function::invoke must be returned with the "primary" event.
 */
PUBLIC void ActionDispatcher::doActionNow(Action* a)
{
    Target* t = a->getTarget();

    // not always set if comming from the outside
    a->mobius = mMobius;

    if (t == NULL) {
        Trace(1, "Action with no target!\n");
    }
    else if (t == TargetFunction) {
        doFunction(a);
    }
    else if (t == TargetParameter) {
        doParameter(a);
    }
    else if (t == TargetUIControl) {
        doUIControl(a);
    }
    else if (t == TargetScript) {
        mScripts->doScriptNotification(a);
    }
    else if (t == TargetPreset) {
        doPreset(a);
    }
    else if (t == TargetSetup) {
        doSetup(a);
    }
    else if (t == TargetBindings) {
        doBindings(a);
    }
    else if (t == TargetUIConfig) {
        // not supported yet, there is only one UIConfig
        Trace(1, "UIConfig action not supported\n");
    }
    else {
        Trace(1, "Invalid action target\n");
    }
}

/**
 * Handle a TargetPreset action.
 * Like the other config targets this is a bit messy because the
 * Action will have a resolved target pointing to a preset in the
 * external config, but we need to set one from the interrupt config.
 * Would be cleaner if we just referenced these by number.
 *
 * Prior to 2.0 we did not support focus on preset changes but since
 * we can bind them like any other target I think it makes sense now.
 * This may be a surprise for some users, consider a global parameter
 * similar to FocusLockFunctions to disable this?
 */
PRIVATE void ActionDispatcher::doPreset(Action* a)
{
    Preset* p = (Preset*)a->getTargetObject();
    if (p == NULL) {    
        // may be a dynamic action
        // support string args here too?
        int number = a->arg.getInt();
        if (number < 0)
          Trace(1, "Missing action Preset\n");
        else {
            p = mMobius->getConfiguration()->getPreset(number);
            if (p == NULL) 
              Trace(1, "Invalid preset number: %ld\n", (long)number);
        }
    }

    if (p != NULL) {
        int number = p->getNumber();

        Trace(2, "Preset action: %ld\n", (long)number);

        // determine the target track(s) and schedule events
        Track* track = mMobius->resolveTrack(a);

        if (track != NULL) {
            track->setPreset(number);
        }
        else if (a->noGroup) {
            // selected track only  
            mMobius->getTrack()->setPreset(number);
        }
        else {
            // Apply to the current track, all focused tracks
            // and all tracks in the Action scope.
            int targetGroup = a->getTargetGroup();

            // might want a global param for this?
            bool allowPresetFocus = true;

            if (targetGroup > 0) {
                // only tracks in this group
                for (int i = 0 ; i < mMobius->getTrackCount() ; i++) {
                    Track* t = mMobius->getTrack(i);
                    if (targetGroup == t->getGroup())
                      t->setPreset(number);
                }
            }
            else if (allowPresetFocus) {
                for (int i = 0 ; i < mMobius->getTrackCount() ; i++) {
                    Track* t = mMobius->getTrack(i);
                    if (mMobius->isFocused(t))
                      t->setPreset(number);
                }
            }
        }
    }
}

/**
 * Process a TargetSetup action.
 * We have to change the setup in both the external and interrupt config,
 * the first so it can be seen and the second so it can be used.
 */
PRIVATE void ActionDispatcher::doSetup(Action* a)
{
    // If we're here from a Binding should have resolved
    Setup* s = (Setup*)a->getTargetObject();
    if (s == NULL) {
        // may be a dynamic action
        int number = a->arg.getInt();
        if (number < 0)
          Trace(1, "Missing action Setup\n");
        else {
            s = mMobius->getConfiguration()->getSetup(number);
            if (s == NULL) 
              Trace(1, "Invalid setup number: %ld\n", (long)number);
        }
    }

    if (s != NULL) {
        int number = s->getNumber();
        Trace(2, "Setup action: %ld\n", (long)number);

        // This is messy, the resolved target will
        // point to an object from the external config but we have 
        // to set one from the interrupt config by number
        mMobius->getConfiguration()->setCurrentSetup(number);
        mMobius->setSetupInternal(number);

        // special operator just for setups to cause it to be saved
        if (a->actionOperator == OperatorPermanent) {
            // save it too, control flow is convoluted,
            // we could have done this when the Action
            // was recevied outside the interrupt
            ThreadEvent* te = new ThreadEvent(TE_SAVE_CONFIG);
            mMobius->getThread()->addEvent(te);
        }
    }
}

/**
 * Process a TargetBindings action.
 * We can be outside the interrupt here.  All this does is
 * set the current overlay binding in mConfig which, we don't have
 * to phase it in, it will just be used on the next trigger.
 */
PRIVATE void ActionDispatcher::doBindings(Action* a)
{
    // If we're here from a Binding should have resolved
    BindingConfig* bc = (BindingConfig*)a->getTargetObject();
    if (bc == NULL) {
        // may be a dynamic action
        int number = a->arg.getInt();
        if (number < 0)
          Trace(1, "Missing action BindingConfig\n");
        else {
            bc = mMobius->getConfiguration()->getBindingConfig(number);
            if (bc == NULL) 
              Trace(1, "Invalid binding overlay number: %ld\n", (long)number);
        }
    }

    if (bc != NULL) {
        int number = bc->getNumber();
        Trace(2, "Bindings action: %ld\n", (long)number);
        mMobius->getConfiguration()->setOverlayBindingConfig(bc);

        // sigh, since getState doesn't export 

    }
}

/**
 * Process a function action.
 *
 * We will replicate the action if it needs to be sent to more than
 * one track due to group scope or focus lock.
 *
 * If a->down and a->longPress are both true, we're being called
 * after long-press detection.
 *
 */
PRIVATE void ActionDispatcher::doFunction(Action* a)
{
    // Client's won't set down in some trigger modes, but there is a lot
    // of code from here on down that looks at it
    if (a->triggerMode != TriggerModeMomentary)
      a->down = true;

    // Only functions track long-presses, though we could
    // in theory do this for other targets.  This may set a->longPress
    // on up transitions
    mTriggerState->assimilate(a);

    Function* f = (Function*)a->getTargetObject();
    if (f == NULL) {
        // should have caught this in doAction
        Trace(1, "Missing action Function\n");
    }
    else if (f->global) {
        // These are normally not track-specific and don't schedule events.
        // The one exception is RunScriptFunction which can be both
        // global and track-specififc.  If this is a script we'll
        // end up in runScript()
        if (!a->longPress)
          f->invoke(a, mMobius);
        else {
            // Most global functions don't handle long presses but
            // TrackGroup does.  Since we'll get longpress actions regardless
            // have to be sure not to call the normal invoke() method
            // ?? what about scripts
            f->invokeLong(a, mMobius);
        }
    }
    else {
        // determine the target track(s) and schedule events
        Track* track = mMobius->resolveTrack(a);

        if (track != NULL) {
            doFunction(a, f, track);
        }
        else if (a->noGroup) {
            // selected track only
            doFunction(a, f, mMobius->getTrack());
        }
        else {
            // Apply to tracks in a group or focused
            Action* ta = a;
            int nactions = 0;
            int targetGroup = a->getTargetGroup();

            for (int i = 0 ; i < mMobius->getTrackCount() ; i++) {
                Track* t = mMobius->getTrack(i);

                if ((targetGroup > 0 && targetGroup == t->getGroup()) ||
                    (targetGroup <= 0 &&
                     (t == mMobius->getTrack() || (f->isFocusable() && mMobius->isFocused(t))))) {

                    // if we have more than one, have to clone the
                    // action so it can have independent life
                    if (nactions > 0)
                      ta = mMobius->cloneAction(a);

                    doFunction(ta, f, t);

                    // since we only "return" the first one free the 
                    // replicants
                    if (nactions > 0)
                      mMobius->completeAction(ta);

                    nactions++;
                }
            }
        }
    }
}

/**
 * Do a function action within a resolved track.
 *
 * We've got this weird legacy EDP feature where the behavior of the up
 * transition can be different if it was sustained long.  This is mostly
 * used to convret non-sustained functions into sustained functions, 
 * for example Long-Overdub becomes SUSOverdub and stops as soon as the
 * trigger is released.  I don't really like this 
 *
 */
PRIVATE void ActionDispatcher::doFunction(Action* action, Function* f, Track* t)
{
    // set this so if we need to reschedule it will always go back
    // here and not try to do group/focus lock replication
    action->setResolvedTrack(t);

    if (action->down) { 
        if (action->longPress) {
            // Here via TriggerState when we detect a long-press,
            // call a different invocation method.
            // TODO: Think about just having Funcion::invoke check for the
            // longPress flag so we don't need two methods...
            // 
            // We're here if the Function said it supported long-press
            // but because of the Sustain Functions preset parameter,
            // there may be a track-specific override.  If the function
            // is sustainable (e.g. Record becomes SUSRecord) then this
            // disables long-press behavoir.

            Preset* p = t->getPreset();
            if (f->isSustain(p)) {
                // In this track, function is sustainable
                Trace(t, 2, "Ignoring long-press action for function that has become sustainable\n");
            }
            else {
                // f->invokeLong(action, t->getLoop());
                // will need to handle MIDI tracks higher? what focus lock
                // and groups mean might be different for midi vs audio tracks
                t->doFunction(action);
            }
        }
        else {
            // normal down invocation
            //f->invoke(action, t->getLoop());
            t->doFunction(action);

            // notify the script interpreter on each new invoke
            // !! sort out whether we wait for invokes or events
            // !! Script could want the entire Action
            // TODO: some (most?) manual functions should cancel
            // a script in progress?
            mMobius->resumeScript(t, f);
        }
    }
    else if (!action->isSustainable() || !f->isSustainable()) {
        // Up transition with a non-sustainable trigger or function, 
        // ignore the action.  Should have filtered these eariler?
        Trace(3, "ActionDispatcher::doFunction not a sustainable action\n");
    }
    else {
        // he's up!
        // let the function change how it ends
        if (action->longPress) {
            Function* alt = f->getLongPressFunction(action);
            if (alt != NULL && alt != f) {
                Trace(2, "ActionDispatcher::doFunction Long-press %s converts to %s\n",
                      f->getDisplayName(),
                      alt->getDisplayName());
            
                f = alt;
                // I guess put it back here just in case?
                // Not sure, this will lose the ResolvedTarget but 
                // that should be okay, the only thing we would lose is the
                // ability to know what the real target function was.
                //action->setFunction(alt);
                action->setLongFunction(alt);
            }
        }

        //f->invoke(action, t->getLoop());
        t->doFunction(action);
    }
}

/**
 * Process a parameter action.
 *
 * These are always processed synchronously, we may be inside or
 * outside the interrupt.  These don't schedule Events so the caller
 * is responsible for freeing the action.
 *
 * Also since these don't schedule Events, we can reuse the same
 * action if it needs to be replicated due to group scope or focus lock.
 */
PRIVATE void ActionDispatcher::doParameter(Action* a)
{
    Parameter* p = (Parameter*)a->getTargetObject();
    if (p == NULL) {
        Trace(1, "Missing action Parameter\n");
    }
    else if (p->scope == PARAM_SCOPE_GLOBAL) {
        // Action scope doesn't matter, there is only one
        doParameter(a, p, NULL);
    }
    else if (a->getTargetTrack() > 0) {
        // track specific binding
        Track* t = mMobius->getTrack(a->getTargetTrack() - 1);
        if (t != NULL)
          doParameter(a, p, t);
    }
    else if (a->getTargetGroup() > 0) {
        // group specific binding
        // !! We used to have some special handling for 
        // OutputLevel where it would remember relative positions
        // among the group.
        Action* ta = a;
        int nactions = 0;
        int group = a->getTargetGroup();
        for (int i = 0 ; i < mMobius->getTrackCount() ; i++) {
            Track* t = mMobius->getTrack(i);
            if (t->getGroup() == group) {
                if (p->scheduled && nactions > 0)
                  ta = mMobius->cloneAction(a);
                  
                doParameter(ta, p, t);

                if (p->scheduled && nactions > 0)
                  mMobius->completeAction(ta);
                nactions++;
            }
        }
    }
    else {
        // current track and focused
        // !! Only track parameters have historically obeyed focus lock
        // Preset parameters could be useful but I'm scared about   
        // changing this now
        if (p->scope == PARAM_SCOPE_PRESET) {
            doParameter(a, p, mMobius->getTrack());
        }
        else {
            Action* ta = a;
            int nactions = 0;
            for (int i = 0 ; i < mMobius->getTrackCount() ; i++) {
                Track* t = mMobius->getTrack(i);
                if (mMobius->isFocused(t)) {
                    if (p->scheduled && nactions > 0)
                      ta = mMobius->cloneAction(a);

                    doParameter(ta, p, t);

                    if (p->scheduled && nactions > 0)
                      mMobius->completeAction(ta);
                    nactions++;
                }
            }
        }
    }
}

/**
 * Process a parameter action once we've determined the target track.
 *
 * MIDI bindings pass the CC value or note velocity unscaled.
 * 
 * Key bindings will always have a zero value but may have bindingArgs
 * for relative operators.
 *
 * OSC bindings convert the float to an int scaled from 0 to 127.
 * !! If we let the float value come through we could do scaling
 * with a larger range which would be useful in few cases like
 * min/max tempo.
 *
 * Host bindings convert the float to an int scaled from 0 to 127.
 * 
 * When we pass the Action to the Parameter, the value in the
 * Action must have been properly scaled.  The value will be in
 * bindingArgs for strings and action.value for ints and bools.
 *
 */
PRIVATE void ActionDispatcher::doParameter(Action* a, Parameter* p, Track* t)
{
    ParameterType type = p->type;

    // set this so if we need to reschedule it will always go back
    // here and not try to do group/focus lock replication
    a->setResolvedTrack(t);

    if (type == TYPE_STRING) {
        // bindingArgs must be set
        // I suppose we could allow action.value be coerced to 
        // a string?
        p->setValue(a);
    }
    else { 
        int min = p->getLow();
        int max = p->getHigh(mMobius);
       
        if (min == 0 && max == 0) {
            // not a ranged type
            Trace(1, "Invalid parameter range\n");
        }
        else {
            // numeric parameters support binding args for relative changes
            a->parseBindingArgs();
            
            ActionOperator* op = a->actionOperator;
            if (op != NULL) {
                // apply relative commands
                Export exp(a);
                int current = p->getOrdinalValue(&exp);
                int neu = a->arg.getInt();

                if (op == OperatorMin) {
                    neu = min;
                }
                else if (op == OperatorMax) {
                    neu = max;
                }
                else if (op == OperatorCenter) {
                    neu = ((max - min) + 1) / 2;
                }
                else if (op == OperatorUp) {
                    int amount = neu;
                    if (amount == 0) amount = 1;
                    neu = current + amount;
                }
                else if (op == OperatorDown) {
                    int amount = neu;
                    if (amount == 0) amount = 1;
                    neu = current - amount;
                }
                // don't need to handle OperatorSet, just use the arg

                if (neu > max) neu = max;
                if (neu < min) neu = min;
                a->arg.setInt(neu);
            }

            p->setValue(a);
        }
    }
}

/**
 * Process a UI action.
 * We just forward the Action to the listener, ownership
 * is not passed and we free it here.
 */
PRIVATE void ActionDispatcher::doUIControl(Action* a)
{
    UIControl* c = (UIControl*)a->getTargetObject();
    if (c == NULL) {
        Trace(1, "Missing action UI Control\n");
    }
    else {
        MobiusListener* listener = mMobius->getListener();
        if (listener != NULL)
          listener->MobiusAction(a);
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

