/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * An extension of RecorderTrack that adds Mobius functionality.
 *
 */

#ifndef TRACK_H
#define TRACK_H

#include "Trace.h"
#include "Recorder.h"

// needed for TrackState
#include "MobiusState.h"

/****************************************************************************
 *                                                                          *
 *                                   TRACK                                  *
 *                                                                          *
 ****************************************************************************/

#define MAX_PENDING_ACTIONS 10

/**
 * The maximum number of loops in a track.
 * This needs to be fixed and relatively small so we can preallocate
 * the maximum number of Loop objects and simply enable or disable
 * them based on the Preset::loopCount parameter.  This saves memory churn
 * and ensures that we won't delete an object out from under a thread that
 * may still be referencing it, mostly this is the UI refresh thread.
 *
 * Prior to 2.0 this awa 128 which is insanely large.  16 is about the most
 * that is manageable and even then the UI for the loop list is practically
 * useless.  Still we could have this as a hidden global parameter.
 */
#define MAX_LOOPS 16

/**
 * Maximum name we can assign to a track.
 */
#define MAX_TRACK_NAME 128


class Track : public RecorderTrack, public TraceContext
{

    friend class Mobius;
	friend class ScriptInterpreter;
	friend class ScriptSetupStatement;
    friend class Loop;
	friend class Synchronizer;
	friend class Function;
	friend class RecordFunction;
    friend class EventManager;
    friend class StreamState;

  public:

	Track(class Mobius* mob, class Synchronizer* sync, int number);
	~Track();
	void setHalting(bool b);

	void getTraceContext(int* context, long* time);

    void doFunction(Action* action);
    
	// Parameters

    void setName(const char* name);
    char* getName();

    void setGroup(int i);
    int getGroup();

	void setInputLevel(int i);
	int  getInputLevel();

	void setOutputLevel(int i);
	int  getOutputLevel();

    void setFeedback(int i);
	int  getFeedback();

    void setAltFeedback(int i);
	int  getAltFeedback();

	void setPan(int i);
	int  getPan();

	void setMono(bool b);
	bool isMono();

    void setMidi(bool b);
    bool isMidi();

    void setSpeedToggle(int degree);
    int getSpeedToggle();

    // these have to be set with Functions and Events
    int getSpeedOctave();
    int getSpeedStep();
    int getSpeedBend();
    int getTimeStretch();
    int getPitchOctave();
    int getPitchStep();
    int getPitchBend();

	void setPitchTweak(int tweak, int value);
    int getPitchTweak(int tweak);

	void setGroupOutputBasis(int i);
	int getGroupOutputBasis();

	void setFocusLock(bool b);
	bool isFocusLock();

    void setSetup(class Setup* setup);
	void setPreset(class Preset* p);
	void setPreset(int number);
	class Preset* getPreset();

	//
    // status 
	//

	Mobius* getMobius();
    PUBLIC SetupTrack* getSetup();
    class SyncState* getSyncState();
	class Synchronizer* getSynchronizer();
    class EventManager* getEventManager();

	int getLoopCount();
	class Loop* getLoop(int index);
	int getRawNumber();
	int getDisplayNumber();
	bool isEmpty();
	int getInputLatency();
	int getOutputLatency();
	MobiusMode* getMode();
	long getFrame();
    class TrackState* getState();
	int getCurrentLevel();
	bool isTrackSyncMaster();

	float getEffectiveSpeed();
	float getEffectivePitch();

	long getProcessedFrames();
	long getRemainingFrames();

	//
	// Mobius control
	//

	void loadProject(class ProjectTrack* t);
	void setBounceRecording(Audio* a, int cycles);
	void setMuteKludge(Function* f, bool b);

	/**
	 * Called by Mobius at the start of an interrupt to assimilate 
     * configuration changes.
	 */
	void updateConfiguration(class MobiusConfig* config);

    /**
     * Special function called within the script interpreter to
     * assimilate just global parameter changes.
     */
	void updateGlobalParameters(class MobiusConfig* config);

    /**
     * Set by various sources outside the interrupt (bindings, the UI)
     * to select a track preset.  The preset will be phased in on the
     * next interrupt.
     */
    void setPendingPreset(int number);

	/**
	 * Called by Mobius as scripts terminate.
	 */
	void removeScriptReferences(class ScriptInterpreter* si);

	//	
	// Function handlers
	//

	void reset(class Action* action);
	void loopReset(class Action* action, class Loop* loop);

	// sequence step for the SpeedNext function
	int getSpeedSequenceIndex();
	void setSpeedSequenceIndex(int s);

	// sequence step for the PitchNext function
	int getPitchSequenceIndex();
	void setPitchSequenceIndex(int s);

	//
	// Recorder interrupt handler
	//

    bool isPriority();

	void prepareForInterrupt();
	void processBuffers(AudioStream* stream, 
						float* in, float *out, long frames, 
						long frameOffset);

	void inputBufferModified(float* buffer);

	//
    // Unit test interface
	//

    Audio* getPlaybackAudio();
	class Loop* getLoop();
    void setInterruptBreakpoint(bool b);
    void interruptBreakpoint();
	int getProcessedOutputFrames();

    class UserVariables* getVariables();

	void setGlobalMute(bool b);
	bool isGlobalMute();
	bool isMute();
	void setSolo(bool b);
	bool isSolo();

	long getInterrupts();
	void setInterrupts(long i);

    void dump(TraceBuffer* b);

  protected:

    void setLoop(class Loop* l);
	void addTail(float* tail, long frames);

	void setUISignal();
	bool isUISignal();

    //
    // EventManager
    //
    
    void enterCriticalSection(const char* reason);
    void leaveCriticalSection();
    class InputStream* getInputStream();
    class OutputStream* getOutputStream();

  private:

	void init(Mobius* mob, class Synchronizer* sync, int number);
    class Preset* getStartingPreset(MobiusConfig* config, Setup* setup);
    void doPendingConfiguration();
    void setSetup(class Setup* setup, bool doPreset);
	void resetParameters(class Setup* setup, bool global, bool doPreset);
	void resetPorts(class SetupTrack* st);
	void trackReset(class Action* action);
	void setupLoops();
    bool checkSyncEvent(class Event* e);
    void switchLoop(Function* f, bool forward);
    void switchLoop(Function* f, int index);
	void checkFrames(float* buffer, long frames);
	void playTail(float* outbuf, long frames);
	float* playTailRegion(float* outbuf, long frames);

	void advanceControllers();

    //
    // Fields
    //

	int	 mRawNumber;        // zero based
    char mName[MAX_TRACK_NAME];

	class Mobius* mMobius;
	class Synchronizer* mSynchronizer;
    class SyncState*  mSyncState;
    class EventManager* mEventManager;
    class SetupTrack* mSetup;
	class InputStream* mInput;
	class OutputStream* mOutput;
	class CriticalSection* mCsect;
	class UserVariables* mVariables;
	class Preset* mPreset;		// private copy

	class Loop*	mLoops[MAX_LOOPS];
    class Loop* mLoop;
	int			mLoopCount;

    int         mGroup;
	bool 		mFocusLock;
	bool		mHalting;
	bool		mRunning;
	long		mInterrupts;
    int         mPendingPreset;
    int         mMonitorLevel;
	bool		mGlobalMute;
	bool 		mSolo;
	// used to cycle between a "full reset" and a "setup reset"
	// in theory can have more than one config we cycle through, 
    // but only two now
	int			mResetConfig;
	int         mInputLevel;
	int         mOutputLevel;
	int         mFeedbackLevel;
	int         mAltFeedbackLevel;
	int         mPan;
    int         mSpeedToggle;
	bool        mMono;
	bool        mUISignal;
	int         mSpeedSequenceIndex;
	int         mPitchSequenceIndex;

    /**
     * Support for an old feature where we could move the controls
     * for a group (only output level) keeping the same relative
     * mix.  This no longer works.
     */
	int         mGroupOutputBasis;

    // Track sync event encountered during the last interrupt
    Event* mTrackSyncEvent;

	// debug/test state

    bool mInterruptBreakpoint;

	// state exposed to the outside world
	TrackState mState;

    // true if this is a MIDI track
    bool mMidi;
};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
