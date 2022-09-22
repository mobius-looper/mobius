/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Model for sending commands or "actions" to mobius.
 *
 * Once the Mobius engine is initialized, it is controlled primarily
 * by the posting of Actions.  An Action object is created and given
 * to Mobius with the doAction command.  The Action is carried out 
 * synchronously if possible, otherwise it is placed in an action
 * queue and processed at the beginning of the next audio interrupt.
 *
 * The Action model contains the following things, described here using
 * the classic W's model.
 *
 *   Trigger (Who)
 *
 *    Information about the trigger that is causing this action to
 *    be performed including the trigger type (midi, key, osc, script),
 *    trigger values (midi note number, velocity), and trigger
 *    behavior (sustainable, up, down).
 *
 *   Target (What)
 *
 *    Defines what is to be done.  Execute a function, change a control,
 *    set a parameter, select a configuration object.
 *
 *   Scope (Where)
 *
 *    Where the target is to be modified: global, track, or group.
 *
 *   Time (When)
 *
 *    When the target is to be modified: immediate, after latency delay,
 *    at a scheduled time, etc.
 *
 *   Arguments (How)
 *
 *    Additional information that may effect the processing of the action.
 *    Arguments may be specified in the binding or may be passed from
 *    scripts.
 *
 *   Results
 *
 *    When an action is being processed, several result properties may
 *    be set to let the caller how it was processed.  This is relevant
 *    only for the script interpreter.
 *
 * Actions may be created from scratch at runtime but it is more common
 * to create them once and "register" them so that they may be reused.
 * Using registered actions avoids the overhead of searching for the
 * system objects that define the target, Functions, Parameters, Bindables
 * etc.  Instead, when the action is registered, the target is resolved
 * and saved in the Action.
 *
 * Before you execute a registered action you must make a copy of it.
 * Actions submitted to Mobius are assumed to be autonomous objects
 * that will become owned by Mobius and deleted when the action is complete.
 * 
 */

#ifndef ACTION_H
#define ACTION_H

#include "SystemConstant.h"
#include "Binding.h"

// sigh, need this until we can figure out what to do with ExValue
#include "Expr.h"

/****************************************************************************
 *                                                                          *
 *                                  OPERATOR                                *
 *                                                                          *
 ****************************************************************************/

/**
 * Constants that describe operations that produce a relative change to
 * a control or parameter binding.
 */
class ActionOperator : public SystemConstant {
  public:
    static ActionOperator* get(const char* name);

    ActionOperator(const char* name, const char* display) :
        SystemConstant(name, display) {
    }
};

extern ActionOperator* OperatorMin;
extern ActionOperator* OperatorMax;
extern ActionOperator* OperatorCenter;
extern ActionOperator* OperatorUp;
extern ActionOperator* OperatorDown;
extern ActionOperator* OperatorSet;
extern ActionOperator* OperatorPermanent;

/****************************************************************************
 *                                                                          *
 *                              RESOLVED TARGET                             *
 *                                                                          *
 ****************************************************************************/

/**
 * Union of possible target pointers.
 * While the code deals with these as a void* and casts 
 * when necessary, it is nice in the debugger to have these
 * in a union so we can see what they are.
 */
typedef union {

    void* object;
    class Function* function;
    class Parameter* parameter;
    class Bindable* bindable;
    class UIControl* uicontrol;

} TargetPointer;

/**
 * A runtime representation of a binding target that has been resolved
 * to the internal Mobius object where possible.  This serves two purposes:
 *
 *    - It allows us to cache pointers to Functions, Parameters, and
 *      Controls so we don't have to do a linear search by name every time
 *      they are needed.
 *
 *    - It provides a level of indirection so that the Function and
 *      Bindable objects can be replaced if the configuration changes.
 *      For Function objects, this only happens as you add or remove 
 *      scripts, which are wrapped in dynamically allocated Functions.
 *      For Bindables (Preset, Setup, BindingConfig), they can change
 *      whenever  the configuration is edited.
 *
 * Once a target is resolved, it is normally registered with Mobius,
 * which means that the object will live for the duration of the Mobius
 * object and it will be refreshed as the configuration changes. 
 * Application level code is allowed to keep pointers to these objects
 * and be assured that they can always be used for Actions and Exports.
 */
class ResolvedTarget {

    friend class Mobius;

  public:

	ResolvedTarget();
	~ResolvedTarget();
    void clone(ResolvedTarget* t);

    bool isInterned();

    class Target* getTarget();
    void setTarget(class Target* t);

    const char* getName();
    void setName(const char* name);

    int getTrack();
    void setTrack(int t);

    int getGroup();
    void setGroup(int t);

    void* getObject();
    void setObject(void* o);

    bool isResolved();
    const char* getDisplayName();
    const char* getTypeDisplayName();
    void getGroupName(char* buf);
    void getFullName(char* buffer, int max);

  protected:

    void setInterned(bool b);
    ResolvedTarget* getNext();
    void setNext(ResolvedTarget* rt);

  private:

    void init();

    bool mInterned;
    ResolvedTarget* mNext;
    class Target* mTarget;
    char* mName;
    TargetPointer mObject;
    int mTrack;
    int mGroup;

};

/****************************************************************************
 *                                                                          *
 *                                   ACTION                                 *
 *                                                                          *
 ****************************************************************************/

/**
 * Maximum length of a string argument in an Action.
 * There are four of these so make them relatively short.
 */
#define MAX_ARG_LENGTH 128

/**
 * An object containing information about an action that is to 
 * take place within the engine.
 * 
 * These are created in response to trigger events then passed to Mobius
 * for processing.
 */
class Action {

    friend class ActionPool;

  public:

	Action();
	Action(Action* src);
    Action(class ResolvedTarget* targ);
    Action(class Binding* src, class ResolvedTarget* targ);
    Action(class Function* func);

	~Action();
    void free();

    Action* getNext();
    void setNext(Action* a);

    bool isRegistered();
    void setRegistered(bool b);

    int getOverlay();
    void setOverlay(int i);

    void parseBindingArgs();

    int getMidiStatus();
    void setMidiStatus(int i);

    int getMidiChannel();
    void setMidiChannel(int i);

    int getMidiKey();
    void setMidiKey(int i);

    bool isSpread();
    void getDisplayName(char* buffer, int max);

    //////////////////////////////////////////////////////////////////////
    //
    // Trigger (Who)
    //
    //////////////////////////////////////////////////////////////////////

	/**
	 * A unique identifier for the action.
	 * This is used when matching the down and up transitions of
	 * sustainable triggers with script threads.
	 * The combination of the Trigger and this id must be unique.
	 * It is also exposed as a variable for scripts so we need to
     * lock down the meaning.  
     *
     * For MIDI triggers it will be the first byte containing both
     * the status and channel plus the second byte containing the note
     * number.  The format is:
     *
     *      ((status | channel) << 8) | key
     *
     * For Key triggers it will be the key code.
     *
     * For Host triggers it will be the host parameter id, but this
     * is not currently used since we handle parameter bindings at a higher
     * level in MobiusPlugin.
     *
     * For script triggers, this will be the address of the ScriptInterpreter.
     * This is only used for some special handling of the GlobalReset function.
	 */
	long id;

	/**
	 * The trigger that was detected.
	 */
	Trigger* trigger;

    /**
     * The behavior of this trigger if ambiguous.
     */
    TriggerMode* triggerMode;

    /**
     * True if we will be passing the OSC message argument
     * along as a function argument or using it as the parameter value.
     * This effects the triggerMode.
     */
    bool passOscArg;

    // From here down are dynamic values that change for every
    // invocation of the action.

    /**
     * A secondary value for the the trigger.
     * This is only used for TriggerMidi and contains the key velocity
     * for notes and the controller value for CCs.  It is used only
     * by the LoopSwitch function to set output levels.
     */
    int triggerValue;

	/**
	 * For ranged triggers, this is the relative location within the range.
     * Negative if less than center, positive if greater than center.
	 */
	int triggerOffset;

	/**
	 * True if the trigger was is logically down.  If the trigger
     * is not sustainable this should always be true.
	 */
	bool down;

    /**
     * true if the trigger is in "auto repeat" mode.
     * This is relevant only for TriggerKey;
     */
    bool repeat;

	/**
	 * True if this is the up transition after a long press.
	 * Also true for invocations that are done at the moment
	 * the long-press time elapses.  
	 */
	bool longPress;

    //////////////////////////////////////////////////////////////////////
    //
    // Target (What)
    // Scope (Where)
    //
    // These are more complicated and must be accessed with methods.
    // If the Action was created from a Binding we'll have a ResolvedTarget.
    // IF the Action is created dynamically we'll have a private set
    // of properties that define the target.  We don't have interfaces
    // for the dynamic construction all possible targets only Functions.  
    //
    //////////////////////////////////////////////////////////////////////

    bool isResolved();
    bool isSustainable();
    Target* getTarget();
    void* getTargetObject();
    class Function* getFunction();
    int getTargetTrack();
    int getTargetGroup();

    bool isTargetEqual(Action* other);

    void setTarget(Target* t);
    void setTarget(Target* t, void* object);
    void setFunction(class Function* f);
    void setParameter(class Parameter* p);
    void setTargetTrack(int track);
    void setTargetGroup(int group);

    // kludge for long press, make this cleaner
    void setLongFunction(class Function*f);
    Function* getLongFunction();

    // internal use only, not for the UI
    void setResolvedTrack(class Track* t);
    Track* getResolvedTrack();

    ResolvedTarget* getResolvedTarget();

    class ThreadEvent* getThreadEvent();
    void setThreadEvent(class ThreadEvent* te);

    class Event* getEvent();

    void setEvent(class Event* e);
    void changeEvent(class Event* e);
    void detachEvent(class Event* e);
    void detachEvent();

    const char* getName();
    void setName(const char* name);

    //////////////////////////////////////////////////////////////////////
    //
    // Time (When)
    //
    // TODO: Might be interesting for OSC to schedule things in the future.  
    // 
    // Might be a good place to control latency adjustments.
    //
    //////////////////////////////////////////////////////////////////////

	/**
	 * True if quantization is to be disabled.
	 * Used only when rescheduling quantized functions whose
	 * quantization has been "escaped".
	 */
	bool escapeQuantization;

	/**
	 * True if input latency compensation is disabled.
	 * Used when invoking functions from scripts after we've
	 * entered "system time".
	 */
	bool noLatency;

    /**
     * True if the event should not be subject to synchronization
     * as it normally might.
     */
    bool noSynchronization;

    //////////////////////////////////////////////////////////////////////
    //
    // Arguments (How)
    //
    // TODO: Figure out a way to install new configration objects
    // through this interface so we don't have to have
    // Mobius methods setConfiguration, etc.  
    //
    //////////////////////////////////////////////////////////////////////

    /**
     * Optional binding arguments.
     * These are taken from Binding.arguments and are processed differently for
     * each target.  For numeric parameters they may be:
     *
     *    center 
     *      calculate the center value
     *    up X
     *      raise the value by X
     *    down X
     *      lower the value by X
     *    set X
     *      set the value to X
     *
     * Other parameter types and functions support different arguments.
     * If the args include both an operator and a numeric operand, the
     * operand is left in the "arg".
     */
    char bindingArgs[MAX_ARG_LENGTH];

    /**
     * Operator to apply to the current value of a parameter or control.
     * Normally this is parsed from bindingArgs.
     * sigh, "operator" is a reserved word
     */
    ActionOperator* actionOperator;

    /**
     * The primary argument of the action.
     * This must be set for Parameter targets.  For function targets
     * there are a few that require an argument: LoopN, TrackN, 
     * InstantMultiply etc.
     *
     * For Host bindings, this will be the scaled value of the
     * host parameter value.
     *
     * For Actions created by the UI it will be an integer.  For
     * controls knobs it will be 0-127, for replicated functions
     * like LoopN and TrackN it will be the object index.
     * 
     * For Key bindigns, this will start 0 but may be adjusted
     * by bindingArguments.
     *
     * For MIDI note bindings, this will start 0 but may be adjusted
     * for bindingArguments.
     *
     * For MIDI CC bindings, this will be scaled CC value.
     * 
     * For OSC bindings, this will be the scaled first message argument.
     *
     * In all cases bindingArgs may contain commands to recalculate
     * the value relative to the current value.
     */
    ExValue arg;

    /** 
	 * Optional arguments, only valid in scripts.
     * This is used for a small number of script functions that
     * take more than one argument.  The list is of variable length,
     * dynamically allocated, and must be freed.
	 */
    class ExValueList* scriptArgs;

    //////////////////////////////////////////////////////////////////////
    //
    // Runtime
    //
    // Various transient things maintained while the action is 
    // being processed.
    //
    //////////////////////////////////////////////////////////////////////

	/**
	 * True if we're rescheduling this after a previously scheduled
	 * function event has completed.
	 */
	class Event* rescheduling;

    /**
     * When reschedulign is true, this should have the event that 
     * we just finished that caused the rescheduling.
     */
    class Event* reschedulingReason;

    // can we get by without this?
    class Mobius* mobius;

    /**
     * Trasnsient flag set true if this action is being evaluated inside
     * the interrupt.    
     */ 
    bool inInterrupt;

    /**
     * Transient flag to disable focus lock and groups.  Used only
     * for some error handling in scripts.
     */
    bool noGroup;

    /**
     * Don't trace invocation of this function.  
     * A kludge for Speed shift parameters that convert themselves to 
     * many function invocations.   
     */
    bool noTrace;


    // temporary for debugging trigger timing
    long millisecond;
    double streamTime;

    //////////////////////////////////////////////////////////////////////
    //
    // Protected
    //
    //////////////////////////////////////////////////////////////////////

  protected:

    void setPooled(bool b);
    bool isPooled();
    void setPool(class ActionPool* p);

    //////////////////////////////////////////////////////////////////////
    //
    // Private
    //
    //////////////////////////////////////////////////////////////////////

  private:

	void init();
    void reset();
    void clone(Action* src);
    char* advance(char* start, bool stopAtSpace);

    Action* mNext;
    bool mPooled;
    bool mRegistered;

    /**
     * The pool we came from.
     */
    class ActionPool* mPool;

	/**
	 * Set as a side effect of function scheduling to the event
	 * that represents the end of processing for this function.
	 * There may be play jump child events and other similar things
	 * that happen first.
	 */
	class Event* mEvent;

	/**
	 * Set as a side effect of function scheduling a Mobius
	 * thread event scheduled to process this function outside
	 * the interrupt handler.
	 * !! have to be careful with this one, it could in theory be
	 * processed before we have a chance to deal with it in the
	 * interpreter.
	 */
	class ThreadEvent* mThreadEvent;

    /**
     * Reference to an interned target when the action is created from
     * a Binding.
     */
    ResolvedTarget* mInternedTarget;
    
    /**
     * Private target properties for actions that are not associated 
     * with bindings.  These are typically created on the fly by the UI.
     */
    ResolvedTarget mPrivateTarget;

    /**
     * Set during internal processing to the resolved Track
     * in which this action will run.  Overrides whatever is specified
     * in the target.
     */
    class Track* mResolvedTrack;

    /**
     * Internal field set by BindingResolver to indicate which
     * BindingConfig overlay this action came from.
     */
    int mOverlay;

    /**
     * Allow the client to specify a name, convenient for
     * OSC debugging.
     */
    char* mName;

    /**
     * Alternate function to have the up transition after
     * a long press.  
     * !! get rid of this, we should either just replace Function
     * at invocation time, or have Function test the flags and figure it out.
     */
    Function* mLongFunction;

};

/****************************************************************************
 *                                                                          *
 *                                ACTION POOL                               *
 *                                                                          *
 ****************************************************************************/

class ActionPool {

  public:

    ActionPool();
    ~ActionPool();

    Action* newAction();
    Action* newAction(Action* src);
    void freeAction(Action* a);

    void dump();

  private:

    Action* allocAction(Action* src);

    Action* mActions;
    int mAllocated;


};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
