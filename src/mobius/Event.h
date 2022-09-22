/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Model for events.
 *
 */

#ifndef MOBIUS_EVENT_H
#define MOBIUS_EVENT_H

#include "ObjectPool.h"

// for SyncSource
#include "Setup.h"

// for WaitType
#include "Script.h"

/****************************************************************************
 *                                                                          *
 *                                 CONSTANTS                                *
 *                                                                          *
 ****************************************************************************/

/**
 * Constant that may be passed as the frame number to Event::confirm and 
 * EventType::confirm.  This means that the event should be scheduled
 * to happen as soon as possible in the loop.
 */
#define CONFIRM_FRAME_IMMEDIATE -1

/**
 * Constant that may be passed as the frame number to Event::confirm and 
 * EventType::confirm.  This means that the event should be scheduled
 * on the next quantization boundary.
 */
#define CONFIRM_FRAME_QUANTIZED -2

/****************************************************************************
 *                                                                          *
 *   							  EVENT TYPE                                *
 *                                                                          *
 ****************************************************************************/

/**
 * Base class for constant event objects.
 * This is more general than it needs to be but I wanted to follow
 * the same style used to define Function constants in case we want
 * to add more interesting behavior later.
 */
class EventType {

  public:

	EventType();

    /**
     * First level event confirmation.
     */
    virtual void confirm(class Action* action, class Loop* l, 
                         class Event* e, long frame);

	/**
     * First level of event implementation.
     * Normally this simply forwards to the Function::doEvent method,
     * but a few events don't have underlying functions (Sync, ScriptWait).
	 */
	virtual void invoke(class Loop* l, class Event* e);

	/**
	 * Overloaded in a subclass to call an appropriate undo method to
	 * cancel the previous invocation of the event.
	 */
	virtual void undo(class Loop* l, class Event* e);

	/**
	 * Overloaded in a subclass to recalculate event state if it
	 * is moved to another frame.
	 */
	virtual void move(class Loop* l, class Event* e, long newFrame);

	/**
	 * Return the displayName or name.
	 */
	const char* getDisplayName();

	/** 
	 * Internal name for trace & scripts.
	 */
	const char* name;

	/**
	 * Alternate display name.  Normally localized if set.
	 * For function events usually the Function.displayName.
	 */
	const char* displayName;

	/**
	 * When true, processing this event requires rescheduling of
	 * the next event that is also marked as a reschedule event.
	 * This is the case for events that cause mode changes (MultiplyEvent)
	 * but not for child events that prepare for a mode change
	 * event (JumpPlayEvent).
	 */
	bool reschedules;

	/**
	 * When true, events of this type will be retained when
	 * an undo is performed.  Used for a few special events used
	 * to represent a sustained function release.
	 */
	bool noUndo;

	/**
	 * When true, this event is not to be treated as a "mode ending" 
	 * event for Multiply and Insert.  True only for special events that
	 * operate on the system as a whole (e.g. Bounce) rather than a specific 
	 * loop.
	 */
	bool noMode;

};

extern EventType* InvokeEvent;
extern EventType* ValidateEvent;
extern EventType* RecordEvent;
extern EventType* RecordStopEvent;
extern EventType* PlayEvent;
extern EventType* OverdubEvent;
extern EventType* MultiplyEvent;
extern EventType* MultiplyEndEvent;
extern EventType* InstantMultiplyEvent;
extern EventType* InstantDivideEvent;
extern EventType* InsertEvent;
extern EventType* InsertEndEvent;
extern EventType* StutterEvent;
extern EventType* ReplaceEvent;
extern EventType* SubstituteEvent;
extern EventType* LoopEvent;
extern EventType* CycleEvent;
extern EventType* SubCycleEvent;
extern EventType* ReverseEvent;
extern EventType* ReversePlayEvent;
extern EventType* SpeedEvent;
extern EventType* RateEvent;
extern EventType* PitchEvent;
extern EventType* BounceEvent;
extern EventType* MuteEvent;
extern EventType* PlayEvent;
extern EventType* JumpPlayEvent;
extern EventType* UndoEvent;
extern EventType* RedoEvent;
extern EventType* ScriptEvent;
extern EventType* StartPointEvent;
extern EventType* RealignEvent;
extern EventType* MidiStartEvent;
extern EventType* SwitchEvent;
extern EventType* ReturnEvent;
extern EventType* SUSReturnEvent;
extern EventType* TrackEvent;
extern EventType* RunScriptEvent;
extern EventType* SampleTriggerEvent;
extern EventType* SyncEvent;
extern EventType* SlipEvent;
extern EventType* MoveEvent;
extern EventType* ShuffleEvent;
extern EventType* SyncCheckEvent;
extern EventType* MidiOutEvent;

/****************************************************************************
 *                                                                          *
 *                                   EVENT                                  *
 *                                                                          *
 ****************************************************************************/

/**
 * For events of type SyncEvent, we overload an argument field
 * to contain one of these which specifies the type of event.
 * Another argument will have the SyncSource code.
 */
typedef enum {

	SYNC_EVENT_START,
	SYNC_EVENT_STOP,
	SYNC_EVENT_CONTINUE,
	SYNC_EVENT_PULSE

} SyncEventType;

/**
 * For events of type SyncEvent whose SyncEventType is SYNC_EVENT_PULSE, 
 * SYNC_EVENT_START, or SYNC_EVENT_CONTINUE, this
 * defines the unit of the pulse when it is ambiguous.
 * This is kind of ugly because it combines two different unit sets
 * those for MIDI (clock, beat, bar) and those for track sync
 * (subcycle, cycle, loop) but I really don't feel like having
 * another enumeration.
 */
typedef enum {

	SYNC_PULSE_UNDEFINED,
	SYNC_PULSE_CLOCK,
	SYNC_PULSE_BEAT,
	SYNC_PULSE_BAR,
	SYNC_PULSE_SUBCYCLE,
	SYNC_PULSE_CYCLE,
	SYNC_PULSE_LOOP

} SyncPulseType;

extern const char* GetSyncPulseTypeName(SyncPulseType type);

/**
 * An event to be processed by the interrupt handler.
 *
 * Some events are are scheduled in groups with one being the parent event
 * and others being child events.   When a parent event is undone, all
 * child events are also undone.   When a child event is processed, 
 * it is marked as being handled but not returned to the pool until
 * the parent event is processed.  This deferred pooling is necessary to
 * allow the undo of the parent event to know if one of its
 * child events has done something that must also now be undone.
 *
 * For easier memory management, we use the same class for all types
 * of events and provide type-specific accessor methods for the
 * generic fields.  This is a little error prone, but much easier to deal
 * with than pools of 85 different event classes.
 * 
 * !! Hmm, a good old fashioned union might be better here.
 */
class Event : public PooledObject {

    friend class EventPool;
    friend class EventObjectPool;
    friend class EventList;
    friend class EventManager;

    // LoopSwitch events call setNext, move this to EventManager!
    friend class Loop;
    // needs to call setNext
    friend class MidiQueue;
    // needs to call setNext
    friend class Synchronizer;

  public:

	void init();
	void init(EventType* type, long frame);

    bool isOwned();
    void setOwned(bool b);
    Event* getParent();
    Event* getChildren();
    Event* getNext();
    Event* getSibling();
    class EventList* getList();
    Track* getTrack();
    void setTrack(Track* t);
    
	void free();
	void freeAll();

    void setInfo(const char* s);
    const char* getInfo();

	void savePreset(class Preset* p);
	class Preset* getPreset();
	void addChild(Event* e);
	void removeChild(Event* e);
	Event* removeUndoChild();

	void invoke(class Loop* l);
	void undo(class Loop* l);
    void confirm(class Action* action, class Loop* l, long frame);

	const char* getName();
	const char* getFunctionName();
	bool inProgress();
	Event* findEvent(EventType* type);
	Event* findEvent(EventType* type, class Function* f);
	void cancelScriptWait();
	void finishScriptWait();
	void rescheduleScriptWait(Event* neu);

	// interpreter that scheduled the event
	class ScriptInterpreter* getScript();
	void setScript(class ScriptInterpreter* si);

    // complex arguments from a script
    class ExValueList* getArguments();
    void clearArguments();
    void setArguments(class ExValueList* args);

    void setAction(Action* a);
    Action* getAction();
    void setInvokingFunction(class Function* f);
    class Function* getInvokingFunction();

	//
	// Common Fields
	//

	/**
	 * Set when a child event is processed.  This lets the 
	 * parent event's undo processor know that the child did something
	 * that may also need to be undone.
	 */
	bool processed;

	/**
	 * Set when an event is scheduled that will need to be rescheduled
	 * after previous events are processed.
	 * !! UPDATE: Try to get rid of this and start using the
	 * new InvokeEventType for these instead.
	 */
	bool reschedule;

	/**
	 * Set when an event is scheduled that will need to be rescheduled
	 * at some unknown point in the future.  This happens for events
	 * related to loop switch and SUS operations that need to span
	 * other functions.
	 */
	bool pending;

	/**
	 * Set when the event is to be processed immediately upon the
	 * next inspection.  The frame is not relevant.  Used in a few cases
	 * where we we activate pending events that must happen immediately,	
	 * but the loop frame may change before the event is processed making
	 * it unreliable to just set the pending frame to whatever the current
	 * loop frame is.
	 */
	bool immediate;

	/**
	 * Event type.
	 */
	EventType* type;

	/**
	 * The "semantic" function associated with the event.
	 * Depending on the event, this may alter the way the event
	 * is processed.  The same event may be used with several functions,
	 * but they must all be in the same "semantic family", e.g. Mute functions
	 * Trigger functions, etc.
	 *
	 * For minor events, notably JumpPlayEvent, there may be no function
	 * in which case there must be a parent event with a function. Stutter
	 * is the only current case of a JumpPlay without a parent.
	 */
	Function* function;

	/**
	 * A few functions have a single integer argument that conveys
	 * extra information, such as the number of cycles in MultiIncrease
	 * or the loop number in a switch.
     * !! can we use Action for this?
	 */
	long number;

	/**
	 * Whether this was an up or down transition of a SUS function.
	 * Only set for functions uch as SUSNextLoop where this information
	 * is meaningful, not necessarily accurate for all functions.
     * !! use Action for this
	 */
	bool down;

	/**
	 * True for the up transition of a sustainable function that was held
	 * beyond the long press interval.  In some cases, this converts
	 * normal toggle functions into SUS functions.
     * !! use Action for this
	 */
	bool longPress;

	/**
	 * Record frame on which the event occurs.
	 */
	long frame;

	/**
	 * For JumpPlayEvent and ReversePlayEvent, the number of frames lost due
	 * to output latency.
	 */
	long latencyLoss;

	/**
	 * True if the event was quantized.  If false, the event frame
     * is usually mInputLatency after the initial function frame.
     * Quantized events are subject to undo.
	 */
	bool quantized;

	/**
	 * When true, the event is to be processed after
	 * a LoopEvent event on the same frame.  Normally 
	 * an event on the same frame as LoopEvent is processed
	 * berore so it is "inside" the loop.  This is used for
	 * Multiply to prevent it from adding an extra cycle if
	 * it exactly on the loop frame, we want to do the loop first
	 * and process the multiply on frame 0.
	 */
	bool afterLoop;

	/**
	 * When true, the event frame decrements during pause mode so the
	 * event will eventually be brought within range of the paused frame.
	 */
	bool pauseEnabled;

	/**
	 * When true, indiciates that the event was scheduled automatically
	 * as a side effect of something else rather than directly by the user.
	 * This is a semi-kludge used when stacking events under a 
	 * SwitchEvent to implement pitch and rate transfer modes.  Since the
	 * user did not initiate these events, we have the option to ignore
	 * them when processing undo, it also makes "Wait last" on a switch
	 * function wait until all of the stacked events have been rather than
	 * immediately after the SwitchEvent which is important for testing.
	 */
	bool automatic;

	/**
	 * When true, disable the frame position sanity checks that 
	 * might be made by this event handler.
	 * Used in special cases.
	 */
	bool insane;

    /**
     * When set and we're finishing Record mode, avoid a fade out
     * on the right edge.
     * This should be used only in the unit tests, but I'm having
     * trouble findig them.  Originally I tried to put this in 
     * the fields.record union but there was some old code that
     * allowed it to be set for SwitchEvents too.
     */
    bool fadeOverride;

    /**
     * Set for events that are scheduled frequently and we don't
     * want to trace at level 2 to avoid clutter.  Currently this
     * is only SpeedEvent.
     */
    bool silent;

    //
    // EventType specific state 
    // 

    union {
        
        /**
         * JumpPlayEvent, ReverseEvent
         * 
         * For events that change the nature of playback, have to remember
         * the old playback state for undo.
         * This will go away once we have the concept of a "committed"
         * event.
         *
         * !! KLUDGE: Shouldn't have to save both ratios and integer
         * scale degrees, but can't calculate one from the other yet...
         */
        struct {
            class Layer* nextLayer;
            long nextFrame;
            /**
             * True if the jump is considered "seamless". Even though the layer
             * and/or frame may change from what was last played, the content
             * is logically the same so do not fade.
             */
            bool nextShift;
            /**
             * The play frame before the event was processed.
             */
            long undoFrame;
            class Layer* undoLayer;
            int undoSpeedToggle;
            int undoSpeedOctave;
            int undoSpeedStep;
            int undoSpeedBend;
            int undoTimeStretch;
            int undoPitchOctave;
            int undoPitchStep;
            int undoPitchBend;
            bool undoMute;
            bool undoReverse;
        } jump;

        /**
         * SwitchEvent, ReturnEvent
         */
        struct {
            class Loop* nextLoop;
            long nextFrame;
            bool recordCanceled;
            bool upTransition;
        } loopSwitch;

        /**
         * ScriptEvent
         */
        struct {
            WaitType waitType;
        } script;
        
        /**
         * SyncEvent
         */
        struct {
            SyncSource source;
            SyncEventType eventType;
            SyncPulseType pulseType;
            long pulseFrame;
            long continuePulse;
            long millisecond;
            int pulseNumber;
            int beat;
            bool syncStartPoint;
            bool syncTrackerEvent;
        } sync;

        /**
         * TrackSwitchEvent
         */
        struct {
            Track* nextTrack;
            bool latencyDelay;
        } trackSwitch;

        /**
         * SpeedEvent
         */
        struct {
            // SpeedUnit
            int unit;
        } speed;

        struct {
            // true if number represents a toggle change
            int toggle;
            int octave;
            int step;
            int bend;
            int stretch;
        } speedRestore;

        /**
         * PitchEvent
         */
        struct {
            int unit;
        } pitch;

        struct {
            int octave;
            int step;
            int bend;
        } pitchRestore;

    } fields;

  protected:

	Event(class EventPool* p);
	~Event();

    void setPooled(bool b);
    bool isPooled();

    void setList(class EventList* list);

    void setNext(Event* e);

  private:

    void setParent(Event* e);
    void setSibling(Event* e);

    /**
     * Pool this event came from.
     */
    class EventPool* mPool;

    /**
     * True if the event is currently in the pool.
     */
    bool mPooled;

	/**
	 * The list the event is in. When non-null, this will be EventPool
	 * if the event is pooled, or a scheduled event list maintained by a Loop.
	 */
	class EventList* mList;

    /**
     * Some complex function families use the same event for many things.
     * These may set this to a non-empty string to provide more
     * information in the UI.  If empty the UI is expected to show
     * the event number.
     */
    char mInfo[32];

	/**
	 * Set if this is a shared event.
	 * Such events are managed by an owner and are never in the pool.
	 */
	bool mOwned;

    /**
     * The track the event is scheduled in. 
     * NULL if the event has not been scheduled, or if it one of the
     * special "owned" events like SyncEvent.
     */
    Track* mTrack;

	/** 
	 * The event list chain pointer.
	 * Maintained in addition order, not time order.
	 */
	Event* mNext;

	/**
	 * Set when an event is considered a child of another event.
	 * Such events will not be returned to the pool until their
	 * parent event is processed.
	 */
	Event* mParent;

	/**
	 * Set when an event is the parent of one or more child events.
	 * When a parent event is undone, all the child events are also
	 * undone.  There may be more than one child event, linked
	 * through the "sibling" pointer.
	 */
	Event* mChildren;

	/**
	 * The child event chain pointer.
	 * This will be set in a few cases where a parent event may
	 * have more than one child event. (Insert).
	 */
	Event* mSibling;

    /**
     * Private copy of the preset at the moment this event was scheduled.
     */
	class Preset* mPreset;

    /**
     * True if the preset has been captured.
     */
	bool mPresetValid;

    /**
     * Script interpreter waiting for this event to finish.
     */
	class ScriptInterpreter* mScript;

    /**
     * Action that caused this event, if this is the "primary" event.
     * This will not be set for secondary events like JumpPlayEvent.
     * This is private so we can better track how it is set.
     */
    Action* mAction;

	/**
	 * The "invoking" function associated with the event.
     * This is the old way to pass the function that scheduled the event
     * that should be unnecessary most of the time now that we have Action.
     * Try to weed this out.  Unfortunately there are a few places
     * where we schedule events without Actions, I'm looking at you
     * LoopSwitch event handler.
	 */
	Function* mInvokingFunction;

    /**
     * The result of an argument expression from a script.
     * This is currently used only for a few function invocation events
     * like Shuffle that support complex options specified
     * through one or more expressions in the script.
     * The ExValue and the ExValueList it may contained are owned
     * by this event and will be deleted with it.
     */
    ExValueList* mArguments;


};

/**
 * An object that encapsulates a list of events and provides some
 * utilities for managing them.
 */
class EventList {

  public:

    EventList();
    ~EventList();

    Event* getEvents();

    void add(Event* e);
    void insert(Event* event);
	void remove(Event* event);
	EventList* transfer();

	bool contains(Event* e);
	Event* find(long frame);
	Event* find(EventType* type);
	Event* find(Function* func);
	Event* find(EventType* type, long frame);

    void flush(bool reset, bool keepScriptEvents);

  private:

    Event* mEvents;

};

/**
 * Event pool.
 */
class EventPool {
    
  public:

    EventPool();
    ~EventPool();

    Event* newEvent();
    void freeEvent(Event* e, bool freeAll);
    void freeEventList(Event* event);

    void flush();
    void dump();

  private:

    EventList* mEvents;
    int mAllocated;

};

#endif
/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
