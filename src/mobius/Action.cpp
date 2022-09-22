/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * A model for representing actions to be taken within the Mobius
 * engine.  These are created in response to triggers.
 */

#include <stdio.h>
#include <string.h>
#include <memory.h>
#include <stdlib.h>
#include <ctype.h>

#include "Util.h"
#include "Trace.h"
#include "List.h"
#include "MessageCatalog.h"

#include "MidiByte.h"

#include "Binding.h"
#include "Function.h"
#include "Event.h"
#include "Script.h"
#include "Parameter.h"

#include "Action.h"

/****************************************************************************
 *                                                                          *
 *                              ACTION OPERATOR                             *
 *                                                                          *
 ****************************************************************************/

ActionOperator* OperatorMin = new ActionOperator("min", "Minimum");
ActionOperator* OperatorMax = new ActionOperator("max", "Maximum");
ActionOperator* OperatorCenter = new ActionOperator("center", "Center");
ActionOperator* OperatorUp = new ActionOperator("up", "Up");
ActionOperator* OperatorDown = new ActionOperator("down", "Down");
ActionOperator* OperatorSet = new ActionOperator("set", "Set");
ActionOperator* OperatorPermanent = new ActionOperator("permanent", "Permanent");

ActionOperator* ActionOperators[] = {
	OperatorMin,
	OperatorMax,
	OperatorCenter,
	OperatorUp,
	OperatorDown,
	OperatorSet,
    // technically I would say this is a qualification to the above
    // operators rather than it's own operator...only using
    // for setup selection now
	OperatorPermanent,
	NULL
};

ActionOperator* ActionOperator::get(const char* name) 
{
	ActionOperator* found = NULL;
	if (name != NULL) {
		for (int i = 0 ; ActionOperators[i] != NULL ; i++) {
			ActionOperator* cc = ActionOperators[i];
			if (StringEqualNoCase(cc->getName(), name)) {
				found = cc;
				break;
			}
		}
	}
	return found;
}

/****************************************************************************
 *                                                                          *
 *                              RESOLVED TARGET                             *
 *                                                                          *
 ****************************************************************************/

PUBLIC void ResolvedTarget::init()
{
    mInterned = false;
    mNext = NULL;
    mTarget = NULL;
    mName = NULL;
    mObject.object = NULL;
    mTrack = 0;
    mGroup = 0;
}

PUBLIC ResolvedTarget::ResolvedTarget()
{
    init();
}

/**
 * Called by Action::clone, we're by definition
 * not interned.
 */
PUBLIC void ResolvedTarget::clone(ResolvedTarget* src)
{
    mTarget = src->mTarget;
    // names are not cloned
    mObject = src->mObject;
    mTrack = src->mTrack;
    mGroup = src->mGroup;
}


PUBLIC ResolvedTarget::~ResolvedTarget()
{
    // we can't stop it now, but warn if we try to do this
    if (mInterned) 
      Trace(1, "ResolvedTarget: deleting interned object!\n");

    delete mName;

	ResolvedTarget *el, *next;
	for (el = mNext ; el != NULL ; el = next) {
		next = el->getNext();
		el->setNext(NULL);
		delete el;
	}
}

PUBLIC bool ResolvedTarget::isInterned()
{
    return mInterned;
}

PUBLIC void ResolvedTarget::setInterned(bool b)
{
    mInterned = b;
}

PUBLIC ResolvedTarget* ResolvedTarget::getNext()
{
    return mNext;
}

PUBLIC void ResolvedTarget::setNext(ResolvedTarget* t)
{
    mNext = t;
}

PUBLIC Target* ResolvedTarget::getTarget()
{
    return mTarget;
}

PUBLIC void ResolvedTarget::setTarget(Target* t)
{
    mTarget = t;
}

PUBLIC const char* ResolvedTarget::getName()
{
    return mName;
}

PUBLIC void ResolvedTarget::setName(const char* name)
{
    delete mName;
    mName = CopyString(name);
}

PUBLIC void* ResolvedTarget::getObject()
{
    return mObject.object;
}

PUBLIC void ResolvedTarget::setObject(void* o)
{
    mObject.object = o;
}

PUBLIC int ResolvedTarget::getTrack()
{
    return mTrack;
}

PUBLIC void ResolvedTarget::setTrack(int t)
{
    mTrack = t;
}

PUBLIC int ResolvedTarget::getGroup()
{
    return mGroup;
}

PUBLIC void ResolvedTarget::setGroup(int g)
{
    mGroup = g;
}

PUBLIC bool ResolvedTarget::isResolved()
{
    return (mObject.object != NULL);
}

/**
 * The UI likes to resolve targets so it can get from the
 * raw binding name to a nicer display name.
 */
PUBLIC const char* ResolvedTarget::getDisplayName()
{
    const char* dname = mName;
            
    if (mObject.object != NULL) {

        if (mTarget == TargetFunction) {
            Function* f = mObject.function;
            dname = f->getDisplayName();
        }
        else if (mTarget == TargetUIControl) {
            UIControl* uic = mObject.uicontrol;
            dname = uic->getDisplayName();
        }
        else if (mTarget == TargetParameter) {
            Parameter* p = mObject.parameter;
            dname = p->getDisplayName();
        }
        else if (mTarget == TargetSetup ||
                 mTarget == TargetPreset ||
                 mTarget == TargetBindings) {
            Bindable* b = mObject.bindable;
            dname = b->getName();
        }
    }

    return dname;
}

/**
 * Return a nice name to display for the type of this target.
 */
PUBLIC const char* ResolvedTarget::getTypeDisplayName()
{
    const char* type = mTarget->getDisplayName();

    // Scripts are TargetFunction but we'd like a more specicic name
    if (mTarget == TargetFunction) {
        Function* f = mObject.function;
        if (f != NULL && f->eventType == RunScriptEvent)
          type = "Script";
    }
    else if (mTarget == TargetParameter) {
        Parameter* p = mObject.parameter;
        if (p->control)
          type = "Control";
    }

    return type;
}

/**
 * Return the group name as a letter.
 * Supplied buffer must be at least 2 characters long.
 */
PUBLIC void ResolvedTarget::getGroupName(char* buf)
{
    strcpy(buf, "");
    if (mGroup > 0) {
		// naughty ASCII hack
		char letter = (char)((int)'A' + (mGroup - 1));
		sprintf(buf, "%c", letter);
    }
}

/**
 * Return a full description of the resolved target, suitable
 * for presentation in the UI.
 *
 * This was designed for the two help dialogs (MIDI, Key).
 * There is a similar rendering used in the binding dialogs.
 * !! Try to merge these?
 */
PUBLIC void ResolvedTarget::getFullName(char* buffer, int max)
{
    strcpy(buffer, "");

    if (mTrack > 0) {
        char buf[8];
        sprintf(buf, "%d", mTrack);
        AppendString(buf, buffer, max);
        AppendString(":", buffer, max);
    }
    else if (mGroup > 0) {
        char buf[8];
        getGroupName(buf);
        AppendString(buf, buffer, max);
        AppendString(":", buffer, max);
    }

    // Leave the type off since this is usually unambiguous
    /*
    if (mTarget != TargetFunction) {
        AppendString(getTypeDisplayName(), buffer, max);
        AppendString(":", buffer, max);
    }
    */

    AppendString(getDisplayName(), buffer, max);
}

/****************************************************************************
 *                                                                          *
 *                                   ACTION                                 *
 *                                                                          *
 ****************************************************************************/

PRIVATE void Action::init()
{
    // Trigger
    id = 0;
    trigger = NULL;
    triggerMode = NULL;
    passOscArg = false;
    triggerValue = 0;
    triggerOffset = 0;
    down = false;
    repeat = false;
    longPress = false;

    // Target, Scope
    mInternedTarget = NULL;
    mResolvedTrack = NULL;
    mLongFunction = NULL;

    // Time
    escapeQuantization = false;
    noLatency = false;
    noSynchronization = false;

    // Arguments
    bindingArgs[0] = 0;
    scriptArgs = NULL;
    actionOperator = NULL;
    arg.setNull();
    scriptArgs = NULL;

    // Status
    rescheduling = NULL;
    reschedulingReason = NULL;
    mobius = NULL;
    inInterrupt = false;
    noGroup = false;
    noTrace = false;
    millisecond = 0;
    streamTime = 0.0f;

    // private

    mNext = NULL;
    mPooled = false;
    mRegistered = false;

    mEvent = NULL;
    mThreadEvent = NULL;

    mOverlay = 0;
    mName = NULL;
}

PUBLIC Action::Action()
{
    init();
}

PUBLIC Action::Action(Action* src)
{
    init();
    if (src != NULL)
      clone(src);
}

PUBLIC Action::Action(ResolvedTarget* t)
{
    init();
    mInternedTarget = t;
}

/**
 * We own nothing except the chain pointer.
 * scrptArgs is transient and owned by the script interpreter that
 * built the action
 */
PUBLIC Action::~Action()
{
    if (mRegistered)
      Trace(1, "Atttempt to delete registered action!\n");

    // scriptArgs is dynamically allocated and must be freed
    delete scriptArgs;
    delete mName;

	Action *el, *next;
	for (el = mNext ; el != NULL ; el = next) {
		next = el->getNext();
		el->setNext(NULL);
		delete el;
	}
}

/**
 * Return an action to it's pool.
 */
PUBLIC void Action::free()
{   
    if (mPool != NULL)
      mPool->freeAction(this);
    else {
        // let this be okay
        //Trace(1, "Action::free with no pool!\n");
        delete this;
    }
}

/**
 * Reset clears a previously initialized action.
 * The difference here is that we have to release the
 * scriptArgs.
 */
PUBLIC void Action::reset()
{
    delete scriptArgs;
    init();
}

PUBLIC const char* Action::getName()
{
    return mName;
}

PUBLIC void Action::setName(const char* name)
{
    delete mName;
    mName = CopyString(name);
}

/**
 * Note that this is called instead of reset() when returning
 * something from the pool for cloning so we must initialize
 * every field!
 */
PRIVATE void Action::clone(Action* src)
{
    mobius = src->mobius;

    // assume names don't need to convey

    // Trigger
    id = src->id;
    trigger = src->trigger;
    triggerMode = src->triggerMode;
    passOscArg = src->passOscArg;
    triggerValue = src->triggerValue;
    triggerOffset = src->triggerOffset;
    down = src->down;
    repeat = src->repeat;
    longPress = src->longPress;

    // Target, Scope
    // take the private target if we have one
    mInternedTarget = src->mInternedTarget;
    mPrivateTarget.clone(&(src->mPrivateTarget));
    mLongFunction = src->mLongFunction;

    // Should we clone these?  They're supposed to be transient!
    mResolvedTrack = src->mResolvedTrack;
    noGroup = src->noGroup;
    noTrace = src->noTrace;

    // Time
    escapeQuantization = src->escapeQuantization;
    noLatency = src->noLatency;
    noSynchronization = src->noSynchronization;

    // Arguments
    strcpy(bindingArgs, src->bindingArgs);
    actionOperator = src->actionOperator;
    arg.set(&(src->arg));

    // Long script args are NOT cloned.  Since script actions 
    // are created on the fly we do not need to clone an interned
    // Action.  We could but we would have to copy the ExValueList
    // which is a pain.
    //scriptArgs = someKindOfCopy(src->scriptArgs);
    if (src->scriptArgs != NULL)
      Trace(1, "Cloning action with script arguments!\n");
    delete scriptArgs;
    scriptArgs = NULL;

    // relevant runtime status
    inInterrupt = src->inInterrupt;

    // are these really necessary?
    millisecond = src->millisecond;
    streamTime = src->streamTime;

    // absolutely not these
    mEvent = NULL;
    mThreadEvent = NULL;

    // !! not sure, probably should reset
    rescheduling = src->rescheduling;
    reschedulingReason = src->reschedulingReason;

    // mNext and mPooled maintained by the pool functions

    // mRegistered and mOverlay are not cloned, they are only used
    // by BindingResolver for actions we do clone
    mRegistered = false;
    mOverlay = 0;
}
 
PUBLIC bool Action::isSustainable()
{
    return (triggerMode == TriggerModeMomentary ||
            triggerMode == TriggerModeToggle);
}

PUBLIC void Action::setPooled(bool b)
{
    mPooled = b;
}

PUBLIC bool Action::isPooled()
{
    return mPooled;
}

PUBLIC void Action::setPool(ActionPool* p)
{
    mPool = p;
}

PUBLIC Action* Action::getNext() 
{
    return mNext;
}

PUBLIC void Action::setNext(Action* a)
{
    mNext = a;
}

PUBLIC bool Action::isRegistered() 
{
    return mRegistered;
}

PUBLIC void Action::setRegistered(bool b)
{
    mRegistered = b;
}

PUBLIC int Action::getOverlay()
{
    return mOverlay;
}

PUBLIC void Action::setOverlay(int i)
{
    mOverlay = i;
}

PUBLIC bool Action::isResolved()
{
    return (getTargetObject() != NULL);
}

PRIVATE ResolvedTarget* Action::getResolvedTarget()
{
    ResolvedTarget* t = mInternedTarget;
    if (t == NULL)
      t = &mPrivateTarget;
    return t;
}

PUBLIC Target* Action::getTarget()
{
    ResolvedTarget* rt = getResolvedTarget();
    return (rt != NULL) ? rt->getTarget() : NULL;
}

PUBLIC void* Action::getTargetObject()
{
    ResolvedTarget* rt = getResolvedTarget();
    return (rt != NULL) ? rt->getObject() : NULL;
}

PUBLIC int Action::getTargetTrack()
{
    ResolvedTarget* rt = getResolvedTarget();
    return (rt != NULL) ? rt->getTrack() : 0;
}

PUBLIC int Action::getTargetGroup()
{
    ResolvedTarget* rt = getResolvedTarget();
    return (rt != NULL) ? rt->getGroup() : 0;
}

/**
 * If the action has bindingArgs, parse them into an
 * ActionOperator and argument value.
 */
PUBLIC void Action::parseBindingArgs()
{
    if (strlen(bindingArgs) > 0) {
        actionOperator = NULL;
        
        char* psn = bindingArgs;

        // skip leading
        psn = advance(psn, false);

        // find end of token
        char* end = advance(psn, true);

        // terminate it to search for operators
        char save = *end;
        *end = 0;

        actionOperator = ActionOperator::get(psn);
        if (actionOperator != NULL) {
            // skip to the operand
            *end = save;
            psn = end;
            psn = advance(psn, false);
            end = advance(psn, true);
            *end = 0;
        }

        if (strlen(psn) > 0) {
            if (IsInteger(psn))
              arg.setInt(atoi(psn));
            else
              arg.setString(psn);
        }

        // leave this empty so we don't do it again
        bindingArgs[0] = 0;
    }
}

/**
 * Helper for parseBindingArgs
 */
PRIVATE char* Action::advance(char* start, bool stopAtSpace)
{
    while (*start) {
        char ch = *start;
        if ((isspace(ch) != 0) == stopAtSpace)
          break;
        else
          start++;
    }

    return start;
}

/**
 * Return true if our target is the same as another.
 * The action must be resolved by now.
 * Used by BindingResolver to filter redundant bindings.
 */
PUBLIC bool Action::isTargetEqual(Action* other)
{
    // ugh, names and indirection suck
    return (getTarget() == other->getTarget() &&
            getTargetObject() == other->getTargetObject() &&
            getTargetTrack() == other->getTargetTrack() &&
            getTargetGroup() == other->getTargetGroup());
}

/**
 * Dynamically set a target.
 * This should only be used for a small number of internally
 * constructed actions.
 */
PUBLIC void Action::setTarget(Target* t)
{
    setTarget(t, NULL);
}

PUBLIC void Action::setTarget(Target* t, void* object)
{
    // we may have started with an interned target, switch
    mInternedTarget = NULL;
    mPrivateTarget.setTarget(t);
    mPrivateTarget.setObject(object);
}

/**
 * Dynamically set a target function.
 * This is used when building Actions on the fly rather than from Bindings.
 * This can only be used with static functions, you can't use this
 * for scripts, those are only accessible through ResolvedTargets.
 */
PUBLIC void Action::setFunction(Function* f)
{
    setTarget(TargetFunction, f);
}

PUBLIC Function* Action::getFunction()
{
    Function* f = NULL;
    if (getTarget() == TargetFunction)
      f = (Function*)getTargetObject();
    return f;
}

PUBLIC void Action::setLongFunction(Function* f)
{
    mLongFunction = f;
}

PUBLIC Function* Action::getLongFunction()
{
    return mLongFunction;
}

/**
 * Dynamically set a target parameter.
 * This is used when building Actions on the fly rather than from Bindings.
 */
PUBLIC void Action::setParameter(Parameter* p)
{
    setTarget(TargetParameter, p);
}

/**
 * Convenience method for things that create Actions on the fly with 
 * function targets.
 *
 * Note that the track argument is 1 based like a Binding.
 * This does not switch from mInternedTarget to mPrivate target,
 * you need to call setTarget() first.
 */
PUBLIC void Action::setTargetTrack(int track)
{
    mPrivateTarget.setTrack(track);
}

PUBLIC void Action::setTargetGroup(int group)
{
    mPrivateTarget.setGroup(group);
}

/**
 * When actions are processed internally we use this to
 * force it to a certain track.
 */
PUBLIC void Action::setResolvedTrack(Track* t)
{
    mResolvedTrack = t;
}

PUBLIC Track* Action::getResolvedTrack()
{
    return mResolvedTrack;
}

PUBLIC Event* Action::getEvent()
{
    return mEvent;
}

PUBLIC ThreadEvent* Action::getThreadEvent()
{
    return mThreadEvent;
}

PUBLIC void Action::setThreadEvent(ThreadEvent* te)
{
    mThreadEvent = te;
}

/****************************************************************************
 *                                                                          *
 *                                 UTILITIES                                *
 *                                                                          *
 ****************************************************************************/

/**
 * Get the MIDI status code from the action id.
 * Format: ((status | channel) << 8) | key
 *
 * We expect these to be MS_ constaints so zero out the channel.
 */
PUBLIC int Action::getMidiStatus()
{
    return ((id >> 8) & 0xF0);
}

/**
 * Get the MIDI status code from the action id.
 * Format: ((status | channel) << 8) | key
 *
 * We expect the argument to be an MS_ constaints so it is
 * already 8 bits with a zero channel.
 */
PUBLIC void Action::setMidiStatus(int i)
{
    id = ((i << 8) | (id & 0xFFF));
}

/**
 * Get the MIDI channel from the action id.
 * Format: ((status | channel) << 8) | key
 */
PUBLIC int Action::getMidiChannel()
{
    return ((id >> 8) & 0xF);
}

PUBLIC void Action::setMidiChannel(int i)
{
    id = ((i << 8) | (id & 0xF0FF));
}

/**
 * Get the MIDI key, program, or CC number from the action id.
 * Format: ((status | channel) << 8) | key
 */
PUBLIC int Action::getMidiKey()
{
    return (id & 0xFF);
}

PUBLIC void Action::setMidiKey(int i)
{
    id = (i | (id & 0xFF00));
}

/**
 * Return true if this action is bound to a function or script that
 * supports spreading.
 */
bool Action::isSpread() 
{
	bool spread = false;
    if (getTarget() == TargetFunction) {
        Function* f = (Function*)getTargetObject();
        if (f != NULL)
          spread = f->isSpread();
    }
	return spread;
}

/**
 * Calculate a display name for this action.
 * Used in the KeyHelp dialog, possibly others.
 */
PUBLIC void Action::getDisplayName(char* buffer, int max)
{
    // TODO: add a trigger prefix!
    buffer[0] = 0;

    if (mInternedTarget != NULL) {
        mInternedTarget->getFullName(buffer, max);

        if (strlen(bindingArgs) > 0) {
            // unparsed, unusual
            AppendString(" ", buffer, max);
            AppendString(bindingArgs, buffer, max);
        }
        else {
            // already parsed
            if (actionOperator != NULL && 
                actionOperator != OperatorSet) {
                AppendString(" ", buffer, max);
                AppendString(actionOperator->getName(), buffer, max);
            }

            if (!arg.isNull()) {
                AppendString(" ", buffer, max);
                int start = strlen(buffer);
                arg.getString(&buffer[start], max - start);
            }
        }
    }
}

/**
 * Set the event that owns this action, checking for error conditions.
 * Check a bunch of "not supposed to happen" integrity constraints to
 * find bugs.
 */
PUBLIC void Action::setEvent(Event* e)
{
    if (e != NULL) {
        if (mEvent != NULL) {
            if (mEvent != e) {
                Trace(1, "Action already owned by another event!\n");
                // steal it?
            }
            else {
                Trace(1, "Action already owned by this event!\n");
                if (e->getAction() != this) {
                    Trace(1, "Action/Event reference not circular!\n");
                    e->setAction(this);
                }
            }
        }
        else if (e->getAction() != NULL) {
            if (e->getAction() != this) {
                Trace(1, "Event already owns another action!\n");
                // steal it?
            }
            else if (e->getAction() == this) {
                Trace(1, "Event already owns this action!\n");
                if (mEvent != e) {
                    Trace(1, "Event/Action reference not circular!\n");
                    mEvent = e;
                }
            }
        }
        else {
            // we hope to be here
            e->setAction(this);
            mEvent = e;
        }
    }
}

/**
 * Move ownership of the Action from one event to another.
 * Again we're being really careful about detecting structure errors,
 * since event/action relationships gets fuzzy in complex cases like
 * Multiply/Insert modes and loop switch.
 */
PUBLIC void Action::changeEvent(Event* e)
{
    detachEvent(mEvent);
    setEvent(e);
}

/**
 * Remove the relationship between an action and event.
 */
PUBLIC void Action::detachEvent(Event* e)
{
    if (e != NULL && mEvent != e)
      Trace(1, "detachEvent: expected event not attached!\n");
    
    if (mEvent != NULL) {
        if (mEvent->getAction() != this)
          Trace(1, "detachEvent: Current event doesn't own this action!\n");
        mEvent->setAction(NULL);
        mEvent = NULL;
    }
}

PUBLIC void Action::detachEvent()
{
    detachEvent(mEvent);
}

/****************************************************************************
 *                                                                          *
 *                                ACTION POOL                               *
 *                                                                          *
 ****************************************************************************/

ActionPool::ActionPool()
{
    mActions = NULL;
    mAllocated = 0;
}

ActionPool::~ActionPool()
{
    delete mActions;
}

/**
 * Allocate a new action, using the pool if possible.
 * Note that this is not managed a csect, it should only be called
 * by Mobius which uses maintains a single app/interrupt coordination
 * csect.
 */
PUBLIC Action* ActionPool::newAction()
{
    return allocAction(NULL);
}

PUBLIC Action* ActionPool::newAction(Action* src)
{
    return allocAction(src);
}

PRIVATE Action* ActionPool::allocAction(Action* src)
{
    Action* action = mActions;

    if (action == NULL) {
        action = new Action(src);
        action->setPool(this);
        mAllocated++;
    }
    else {
        mActions = action->getNext();
        action->setNext(NULL);
        action->setPooled(false);
        if (src != NULL)
          action->clone(src);
        else
          action->reset();
    }

    return action;
}

PUBLIC void ActionPool::freeAction(Action* action)
{
    if (action != NULL) {
        if (action->isPooled())
          Trace(1, "Ignoring attempt to free pooled action\n");
        else {
            action->setNext(mActions);
            mActions = action;
            action->setPooled(true);

            // Release script args now or wait till it is brought
            // out of the pool?  Might as well do them now
            delete action->scriptArgs;
            action->scriptArgs = NULL;
            // this is transient
            action->setTargetTrack(NULL);
        }
    }
}

PUBLIC void ActionPool::dump()
{
    int count = 0;

    for (Action* a = mActions ; a != NULL ; a = a->getNext())
      count++;

    printf("ActionPool: %d allocated, %d in the pool, %d in use\n", 
           mAllocated, count, mAllocated - count);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
