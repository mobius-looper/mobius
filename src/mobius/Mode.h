/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Static objects representing Mobius operating modes.
 *
 */

#ifndef MOBIUS_MODE_H
#define MOBIUS_MODE_H

#include "SystemConstant.h"

/****************************************************************************
 *                                                                          *
 *   								 MODE                                   *
 *                                                                          *
 ****************************************************************************/

class MobiusMode : public SystemConstant {

    friend class Mobius;

  public:

	MobiusMode();
	MobiusMode(const char* name, int key);
	MobiusMode(const char* name, const char* display);
    void init();
	virtual ~MobiusMode();

    void format(char* buffer);
    void format(char* buffer, int arg);

    //
    // New style function invocation.  Let the Mode have the first crack.
    //
    
    virtual void invoke(class Action* action, class Loop* l);

    // name, displayName, key inherited from SystemConstant

	/**
	 * True for "minor" modes any of which can be in effect at the
	 * same time as a major mode.
	 */
	bool minor;

	/**
	 * True if this is a "recording" mode, where the loop content
	 * may be modified.
	 */
	bool recording;

	/**
	 * True if this mode can extend the loop (insert, multiply, stutter).
	 */
	bool extends;

	/**
	 * True if this is a "rounding" mode that needs to continue
	 * until it reaches a certain boundary, typically a cycle.
	 * (insert, multiply(
	 */
	bool rounding;
	
	/**
	 * True if secondary feedback is relevant in this mode.
	 * Even if it is relevant it may not be enabled.  
	 * This is used by the UI to enable secondary feedback, but only
	 * in modes where that makes sense.
	 */
	bool altFeedbackSensitive;

	/**
	 * True to force secondary feedback sensitivity off.  This is set
     * from the config files and overrides altFeedbackSensitive which
     * is a static part of the mode definition.
     */
	bool altFeedbackDisabled;

    /**
     * True if this Mode handles function invocation.
     *
     * This is the new way of having mode-specific behavior, rather than
     * making all the Function::invoke methods look at modes, we can
     * have at least some of the modes change the way the Function behaves.
     * 
     * The transition will be gradual.
     */
    bool invokeHandler;

  protected:

    static void initModes();
	static void updateConfiguration(class MobiusConfig* config);
	static void localizeAll(class MessageCatalog* cat);
	static MobiusMode** getModes();
	static MobiusMode* getMode(const char* name);

};

//
// Major Modes
//

extern MobiusMode* ConfirmMode;
extern MobiusMode* InsertMode;
extern MobiusMode* MultiplyMode;
extern MobiusMode* MuteMode;
extern MobiusMode* OverdubMode;
extern MobiusMode* PauseMode;
extern MobiusMode* PlayMode;
extern MobiusMode* RecordMode;
extern MobiusMode* RehearseMode;
extern MobiusMode* RehearseRecordMode;
extern MobiusMode* ReplaceMode;
extern MobiusMode* ResetMode;
extern MobiusMode* RunMode;
extern MobiusMode* StutterMode;
extern MobiusMode* SubstituteMode;
extern MobiusMode* SwitchMode;
extern MobiusMode* SynchronizeMode;
extern MobiusMode* ThresholdMode;

//
// Minor Modes
// 
// Mute and Overdub are both major and minor modes
//

extern MobiusMode* CaptureMode;
extern MobiusMode* GlobalMuteMode;
extern MobiusMode* GlobalPauseMode;
extern MobiusMode* HalfSpeedMode;
extern MobiusMode* MIDISyncMasterMode;

extern MobiusMode* PitchOctaveMode;
extern MobiusMode* PitchStepMode;
extern MobiusMode* PitchBendMode;
extern MobiusMode* SpeedOctaveMode;
extern MobiusMode* SpeedStepMode;
extern MobiusMode* SpeedBendMode;
extern MobiusMode* SpeedToggleMode;
extern MobiusMode* TimeStretchMode;

extern MobiusMode* ReverseMode;
extern MobiusMode* SoloMode;
extern MobiusMode* SyncMasterMode;
extern MobiusMode* TrackSyncMasterMode;
extern MobiusMode* WindowMode;

extern MobiusMode* Modes[];

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
