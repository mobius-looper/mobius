/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Model for function definitions.
 *
 */

#ifndef MOBIUS_FUNCTION_H
#define MOBIUS_FUNCTION_H

#include "MobiusInterface.h"

#include "SystemConstant.h"

/****************************************************************************
 *                                                                          *
 *                                  FUNCTION                                *
 *                                                                          *
 ****************************************************************************/

class Function : public SystemConstant {

    friend class Mobius;
    // don't like this
    friend class ScriptFunctionStatement;

  public:
    
    //////////////////////////////////////////////////////////////////////
    // Fields
    //////////////////////////////////////////////////////////////////////


	const char* alias1;			// optional names for scripts
	const char* alias2;			// optional names for scripts
	bool externalName;			// true if name is external (no key)
    int ordinal;				// internal number for indexing
	bool global;				// true for non-track specific functions
    bool outsideInterrupt;      // true if this can run in the UI thread
	int index;					// for replicated functions
	void* object;				// for replicated functions

	class EventType* eventType;	// type used when posting events
    class MobiusMode* mMode;     // mode we enter eventually enter
	class Function* longFunction; // alt function to use after long press
	bool majorMode;				// true if this is a "major mode" (for MuteCancel)
	bool minorMode;				// true if this is a "minor mode" (for MuteCancel)
	bool instant;				// true if this is an "instant edit" 
	bool trigger;				// true if this is a trigger  (for MuteCancel)
	bool quantized;				// true if function can be quantized
	bool quantizeStack;			// true if can be on same frame as another
    bool sustain;				// true if function always operates in SUS mode
	bool maySustain;			// true if we might operate in SUS mode
	bool longPressable;			// true if we have long press behavior

	bool resetEnabled;			// valid in Reset mode
	bool thresholdEnabled;		// valid in Threshold mode
	bool cancelReturn;			// cancels a return transition
	bool runsWithoutAudio;	    // function meaningful even if no audio device
	bool noFocusLock;			// not used with focus lock
	bool focusLockDisabled;		// focus lock possible but disabled
	bool scriptSync;			// true if scripts always wait for completion
	bool scriptOnly;			// true if callable only from scripts

	bool mayCancelMute;			// true if it is able to cancel mute
	bool cancelMute;			// true if it will cancel mute (MuteCancel=Custom)
    bool mayConfirm;            // true if this can be a switch confirmation
    bool confirms;              // true if this will confirm a switch
    bool silent;                // true if events are not traced

	/**
	 * True if this is a "spreading" function that will automatically
	 * be bound to a range of MIDI notes around a center note.
	 */
	bool spread;

	/** 
	 * True for functions that can be stacked after a loop switch.
	 */
	bool switchStack;

	/**
	 * True for functions that cancel each other when stacked
	 * after a loop switch.  
	 */
	bool switchStackMutex;

	/**
	 * This function must always be scheduled in the active track.
	 * This is an obscure case for TrackSelect and TrackCopy functions that
	 * need to make sure the active track closes off the recording before
	 * the tracks are changed or copied.  The only time might not be
	 * in the active track is when these functions are called from scripts.
	 */
	bool activeTrack;

    /**
     * True means that when the function is used in scripts, the arguments
     * are to be parsed using the Expr expression parser.  The default
     * parser creates ScriptArguments which can reference parameters
     * and variables, and use $() expansion but they cannot contain 
     * arithmetic operators.
     *
     * expressionArgs are set for only a few functions that need
     * to allow math in the argument, currently those are Move, Shuffle,
     * and MidiOut.  Most function arguments are simple strings and numbers
     * and don't need this but the ones that take numbers could be more
     * useful with expressions (TrackN, LoopN, etc.).  
     *
     * There isn't much extra overhead to using expressionArgs, 
     * as long as there is only one argument.
     *
     * When this is true and varArgs is false, the function will receive
     * the argument in the Action.expressionArg field.
     */
    bool expressionArgs;

    /**
     * When true it means that the function supports more than one argument,
     * and that the number may be variable.  The only functions
     * that use this at the moment are Shuffle and MidiOut.
     *
     * Since the list is variable, the values will be calculated at runtime
     * into a dynamically allocated ExValueList.  This does have more
     * overhead and memory complications than single argument functions
     * so it should be used sparingly.  This flag implies expressionArgs.
     *
     * When this is true the argument list is passed in the 
     * Action.expressionArgs property (note the plural).
     */
    bool variableArgs;

    //////////////////////////////////////////////////////////////////////
    // Methods
    //////////////////////////////////////////////////////////////////////

	Function();
	Function(const char* name, int key);
    virtual ~Function();

	void trace(class Action* action, class Mobius* m);
	void trace(class Action* action, class Loop* l);
    void changePreset(class Action* action, class Loop* l, bool after);

	/**
     * True if it is possible to focus lock, and focus lock is not disabled.
     */
	bool isFocusable();

    /**
     * Tue if both down and up transitions must be known.
     */
	bool isSustainable();

    /**
     * True if spreading function or script.
     */
    bool isSpread() ;

    /**
     * True if this is a script.
     */
    bool isScript();

    //////////////////////////////////////////////////////////////////////
    // Virtual Behavior Methods
    //////////////////////////////////////////////////////////////////////

    /**
     * Localize the function name.
     * Overloaded by ReplicatedFunction so it can add a number suffix.
     */
	virtual void localize(class MessageCatalog* cat);

    /**
     * True if the name matches the function name.
     * Normally just matches the name fo the Function object but for
     * the RunScript function it is overloaded to match against the 
     * reference script name.
     */
	virtual bool isMatch(const char* name);


    /**
     * True if this function will cancel Mute mode.
     */
	virtual bool isMuteCancel(class Preset* p);

	/**
	 * True if the function in the context of this Preset is
	 * a SUS function.  maySustain will be on, but sustain may not.
	 * Overloaded by the Function to add preset tests.
	 */
	virtual bool isSustain(class Preset* p);

	/**
	 * True if this may be invoked during recording.
	 */
	virtual bool isRecordable(class Preset* p);

	/**
	 * Return an alternate function to invoke after a long press.
	 */
	virtual Function* getLongPressFunction(class Action* action);

    //////////////////////////////////////////////////////////////////////
    // Virtual Invocation Methods
    //////////////////////////////////////////////////////////////////////

	virtual void invoke(class Action* action, class Mobius* l);
	virtual class Event* invoke(class Action* action, class Loop* l);
    virtual void invokeLong(class Action* action, Mobius* m);
    virtual void invokeLong(class Action* action, Loop* l);
	virtual void invokeEvent(Loop* l, Event* e);

	virtual Event* scheduleEvent(class Action* action, class Loop* l);
	virtual Event* scheduleModeStop(class Action* action, class Loop* l);
	virtual bool undoModeStop(class Loop* l);
	virtual Event* scheduleSwitchStack(class Action* action, Loop* l);
    virtual Event* scheduleTransfer(class Loop* l);
	virtual Event* rescheduleEvent(class Loop* l, Event* previous, Event* next);
    virtual void confirmEvent(class Action* action, class Loop* l, 
                              Event* e, long frame);
	virtual void doEvent(class Loop* l, Event* e);
	virtual void undoEvent(class Loop* l, Event* e);
	virtual void escapeQuantization(class Action* action, Loop* l,
									Event* e);

	virtual void prepareJump(Loop* l, Event* e, class JumpContext* jump);
	virtual void prepareSwitch(Loop* l, Event* e, class SwitchContext* switchcon,
							   class JumpContext* jump);

	Event* scheduleEventDefault(class Action* action, class Loop* l);

    //////////////////////////////////////////////////////////////////////
    // Static Function Table Management
    //////////////////////////////////////////////////////////////////////

  protected:

    static void initStaticFunctions();
    static Function* getStaticFunction(const char * name);

	static void localizeAll(class MessageCatalog* cat);

    static Function* getFunction(Function** functions, const char * name);

  private:

    void init();

};

/****************************************************************************
 *                                                                          *
 *                            REPLICATED UFNCTION                           *
 *                                                                          *
 ****************************************************************************/

/**
 * Extenion used by functions that support a numeric multiplier.
 * Some functions have both a set of relative and absolute functions
 * so we multiply only when the replicated flag is on.
 */
class ReplicatedFunction : public Function {
  public:
	ReplicatedFunction();
	void localize(MessageCatalog* cat);
  protected:
	bool replicated;
	char fullName[32];
	char fullAlias1[32];
	char fullDisplayName[64];
};


/****************************************************************************
 *                                                                          *
 *                            RUN SCRIPT FUNCTION                           *
 *                                                                          *
 ****************************************************************************/

/**
 * Constant for RunScriptFunction.
 */
#define MAX_SCRIPT_NAME 1024

/**
 * This is the only specific function class that we define globally
 * because Function needs it to create Function wrappers for loaded
 * scripts.
 */
class RunScriptFunction : public Function {
  public:
	RunScriptFunction(class Script* s);
	void invoke(class Action* action, class Mobius* m);
	bool isMatch(const char* xname);
  private:
	// we have to maintain copies of these since the strings the
	// Script return can be reclained after an autoload
	char mScriptName[MAX_SCRIPT_NAME];
};

/****************************************************************************
 *                                                                          *
 *                             FUNCTION CONSTANTS                           *
 *                                                                          *
 ****************************************************************************/

extern Function* AutoRecord;
extern Function* Backward;
extern Function* Bounce;
extern Function* Breakpoint;
extern Function* Checkpoint;
extern Function* Clear;
extern Function* Confirm;
extern Function* Coverage;
extern Function* Debug;
extern Function* DebugStatus;
extern Function* Divide;
extern Function* Divide3;
extern Function* Divide4;
extern Function* Drift;
extern Function* DriftCorrect;
extern Function* FocusLock;
extern Function* Forward;
extern Function* GlobalMute;
extern Function* GlobalPause;
extern Function* GlobalReset;
extern Function* Halfspeed;
extern Function* Ignore;
extern Function* InitCoverage;
extern Function* Insert;
extern Function* InstantMultiply;
extern Function* InstantMultiply3;
extern Function* InstantMultiply4;
extern Function* LongUndo;
extern Function* LoopN;
extern Function* Loop1;
extern Function* Loop2;
extern Function* Loop3;
extern Function* Loop4;
extern Function* Loop5;
extern Function* Loop6;
extern Function* Loop7;
extern Function* Loop8;
extern Function* MidiOut;
extern Function* MidiStart;
extern Function* MidiStop;
extern Function* MyMove;
extern Function* Multiply;
extern Function* Mute;
extern Function* MuteOff;
extern Function* MuteOn;
extern Function* MuteRealign;
extern Function* MuteMidiStart;
extern Function* NextLoop;
extern Function* NextTrack;
extern Function* Overdub;
extern Function* OverdubOff;
extern Function* OverdubOn;
extern Function* Pause;
extern Function* PitchBend;
extern Function* PitchDown;
extern Function* PitchNext;
extern Function* PitchCancel;
extern Function* PitchOctave;
extern Function* PitchPrev;
extern Function* PitchStep;
extern Function* PitchUp;
extern Function* Play;
extern Function* PrevLoop;
extern Function* PrevTrack;
extern Function* Realign;
extern Function* Record;
extern Function* Redo;
extern Function* Rehearse;
extern Function* ReloadScripts;
extern Function* Replace;
extern Function* Reset;
extern Function* Restart;
extern Function* RestartOnce;
extern Function* ResumeScript;
extern Function* Reverse;
extern Function* SampleN;
extern Function* Sample1;
extern Function* Sample2;
extern Function* Sample3;
extern Function* Sample4;
extern Function* Sample5;
extern Function* Sample6;
extern Function* Sample7;
extern Function* Sample8;
extern Function* SaveCapture;
extern Function* SaveLoop;
extern Function* Shuffle;
extern Function* ShortUndo;
extern Function* Slip;
extern Function* SlipForward;
extern Function* SlipBackward;
extern Function* Solo;
extern Function* SpeedDown;
extern Function* SpeedNext;
extern Function* SpeedCancel;
extern Function* SpeedPrev;
extern Function* SpeedShift;
extern Function* SpeedOctave;
extern Function* SpeedStep;
extern Function* SpeedBend;
extern Function* SpeedUp;
extern Function* SpeedToggle;
extern Function* StartCapture;
extern Function* StartPoint;
extern Function* StopCapture;
extern Function* Stutter;
extern Function* Substitute;
extern Function* Surface;
extern Function* SUSInsert;
extern Function* SUSMultiply;
extern Function* SUSMute;
extern Function* SUSMuteRestart;
extern Function* SUSNextLoop;
extern Function* SUSOverdub;
extern Function* SUSPrevLoop;
extern Function* SUSRecord;
extern Function* SUSRehearse;
extern Function* SUSReplace;
extern Function* SUSReverse;
extern Function* SUSSpeedToggle;
extern Function* SUSStutter;
extern Function* SUSSubstitute;
extern Function* SUSUnroundedInsert;
extern Function* SUSUnroundedMultiply;
extern Function* SyncMaster;
extern Function* SyncMasterTrack;
extern Function* SyncMasterMidi;
extern Function* SyncStartPoint;
extern Function* TimeStretch;
extern Function* TrackN;
extern Function* Track1;
extern Function* Track2;
extern Function* Track3;
extern Function* Track4;
extern Function* Track5;
extern Function* Track6;
extern Function* Track7;
extern Function* Track8;
extern Function* TrackCopy;
extern Function* TrackCopyTiming;
extern Function* TrackGroup;
extern Function* TrackReset;
extern Function* TrimEnd;
extern Function* TrimStart;
extern Function* UIRedraw;
extern Function* Undo;
extern Function* UndoOnly;
extern Function* WindowBackward;
extern Function* WindowForward;
extern Function* WindowStartBackward;
extern Function* WindowStartForward;
extern Function* WindowEndBackward;
extern Function* WindowEndForward;
extern Function* WindowMove;
extern Function* WindowResize;

extern Function* StaticFunctions[];
extern Function* HiddenFunctions[];

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
