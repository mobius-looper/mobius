/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * The mobius looping engine, primary interface class.
 *
 */

#ifndef MOBIUS_H
#define MOBIUS_H

#include "Trace.h"

#include "MobiusInterface.h"

#include "Binding.h"
#include "MidiListener.h"
#include "MobiusInterface.h"
#include "MobiusState.h"
#include "Recorder.h"

/****************************************************************************
 *                                                                          *
 *                                 CONSTANTS                                *
 *                                                                          *
 ****************************************************************************/

#define UNIT_TEST_SETUP_NAME "Unit Test Setup"
#define UNIT_TEST_PRESET_NAME "Unit Test Preset"

/****************************************************************************
 *                                                                          *
 *                                   MOBIUS                                 *
 *                                                                          *
 ****************************************************************************/

class Mobius : 
    public MobiusInterface,
    public TraceContext, 
    public MidiEventListener, 
    public RecorderMonitor 
{
	friend class ScriptInterpreter;
	friend class ScriptSetupStatement;
	friend class ScriptPresetStatement;
	friend class ScriptFunctionStatement;
	friend class MobiusThread;
	friend class Loop;
	friend class Track;
	friend class Synchronizer;
	friend class EventManager;
    friend class Function;
    friend class Parameter;

  public:

	Mobius(MobiusContext* con);
	virtual ~Mobius();

    //////////////////////////////////////////////////////////////////////
    //
    // MobiusInterface
    // These are the only methods that applications should use
    // all the others are "protected" for use in function invocation
    //
    //////////////////////////////////////////////////////////////////////

    // Configuration

    MobiusContext* getContext();
    class AudioStream* getAudioStream();
    void preparePluginBindings();
	void start();
	void setListener(MobiusListener* mon);
	MobiusListener* getListener();
    void setUIBindables(UIControl** controls, UIParameter** parameters);
    UIControl** getUIControls();
    UIControl* getUIControl(const char* name);

    class HostConfigs* getHostConfigs();
	class MobiusConfig* getConfiguration();
	class MobiusConfig* editConfiguration();
    
    bool findConfigurationFile(const char* file, char* path, int max);

	void setFullConfiguration(class MobiusConfig* config);
	void setGeneralConfiguration(class MobiusConfig* config);
	void setPresetConfiguration(class MobiusConfig* config);
	void setSetupConfiguration(class MobiusConfig* config);
	void setBindingConfiguration(class MobiusConfig* config);
    void reloadOscConfiguration();
    void reloadScripts();

    // Triggers and Actions

    Action* newAction();
    Action* cloneAction(Action* a);
    void freeAction(Action* a);
    void doAction(Action* a);
    // for ScriptInterpreter, some Parameters
    void doActionNow(Action* a);
    void completeAction(Action* a);
    
    void doKeyEvent(int key, bool down, bool repeat);
	void doMidiEvent(class MidiEvent* e);

	void setCheckInterrupt(bool b);
	class CalibrationResult* calibrateLatency();
	void finishPrompt(Prompt* p);

    WatchPoint* addWatcher(class WatchPointListener* l);
    void notifyWatchers(class WatchPoint* wp, int value);

    // Status

	class MessageCatalog* getMessageCatalog();
    class MobiusState* getState(int track);
    class MobiusAlerts* getAlerts();

	int getReportedInputLatency();
	int getReportedOutputLatency();
	int getEffectiveInputLatency();
	int getEffectiveOutputLatency();

    int getTrackCount();
    int getActiveTrack();
    class Track* getTrack(int index);
	int getTrackPreset();

    int getSampleRate();

    // Load/Save

	void loadLoop(class Audio* a);
	void loadProject(class Project* a);
	class Project* saveProject();
	void saveLoop(const char* name);
    void saveLoop(class Action* action);

    // External bindings

    class ResolvedTarget* resolveTarget(Binding* b);
    class Action* resolveAction(Binding* b);
    class Export* resolveExport(Binding* b);
    class Export* resolveExport(ResolvedTarget* t);
    class Export* resolveExport(Action* a);

    Action* getScriptButtonActions();

    // Object pools

    class AudioPool* getAudioPool();
    class LayerPool* getLayerPool();
    class EventPool* getEventPool();

    //////////////////////////////////////////////////////////////////////
    //
    // Semi-protected methods for function invocation
    //
    //////////////////////////////////////////////////////////////////////
    
	void writeConfiguration();
	class MobiusConfig* getMasterConfiguration();
    class MobiusConfig* getInterruptConfiguration();
    class Recorder* getRecorder();

    // Used by MobiusThread when it needs to access files
	const char* getHomeDirectory();

	void setOverlayBindings(class BindingConfig* c);

    class ControlSurface* getControlSurfaces();

	class MobiusMode* getMode();
	long getFrame();

	void setCustomMode(const char* s);
	const char* getCustomMode();

    class Watchers* getWatchers();

	// MidiHandler interface

	void midiEvent(class MidiEvent* e);

	// RecorderMonitor interface
	void recorderMonitorEnter(AudioStream* stream);
	void recorderMonitorExit(AudioStream* stream);

    // Object constants

    Parameter** getParameters();
    Parameter* getParameter(const char* name);
    Parameter* getParameterWithDisplayName(const char* name);

    Function** getFunctions();
    Function* getFunction(const char* name);
    void updateGlobalFunctionPreferences();

    MobiusMode** getModes();
    MobiusMode* getMode(const char* name);

	// Function Invocation


    //void run(class Script* s);

	// Global functions
	// Only need to be public for the Function handlers

    class Track* resolveTrack(Action* a);

	void globalReset(class Action* action);
	void globalMute(class Action* action);
	void cancelGlobalMute(class Action* action);
	void globalPause(class Action* action);
	void sampleTrigger(class Action* action, int index);
	long getLastSampleFrames();
	void addMessage(const char* msg);
	void runScript(class Action* action);

	void startCapture(class Action* action);
	void stopCapture(class Action* action);
	void saveCapture(class Action* action);

	void toggleBounceRecording(class Action* action);

    void unitTestSetup();

	void resumeScript(class Track* t, class Function* f);
	void cancelScripts(class Action* action, class Track* t);

    // needed by TrackSetupParameter to change setups within the interrupt
    void setSetupInternal(int index);

    // Unit Test Interface

    void setOutputLatency(int l);
	class Track* getSourceTrack();
    void setTrack(int i);
	void stopRecorder();

	// user defined variables
    class UserVariables* getVariables();

	// script control variables

	// has to be public for NoExternalInputVarialbe
	bool isNoExternalInput();
	void setNoExternalInput(bool b);
	
    // trace

	void getTraceContext(int* context, long* time);
	void logStatus();
    
    // utilities

    class Track* getTrack();
	class Synchronizer* getSynchronizer();
	bool isInInterrupt();
	long getInterrupts();
	void setInterrupts(long i);
	long getClock();

    // for Synchronizer and a few Functions
    Setup* getInterruptSetup();

  protected:

	// for MobiusThread and others

	Audio* getCapture();
	Audio* getPlaybackAudio();
	void loadProjectInternal(class Project* p);
    class MobiusThread* getThread();
	void emergencyExit();
    void exportStatus(bool inThread);
	void notifyGlobalReset();

    // Need these for the Setup and Preset script statements
    void setSetupInternal(class Setup* setup);

    // for some Functions
    void setPresetInternal(int p);

  private:

	void stop();
    bool installScripts(class ScriptConfig* config, bool force);
    void installWatchers();
	void localize();
	class MessageCatalog* readCatalog(const char* language);
    void localizeUIControls();
	void updateBindings();
    void propagateInterruptConfig();
    void propagateSetupGlobals(class Setup* setup);
    bool unitTestSetup(MobiusConfig* config);

    bool isFocused(class Track* t);
    bool isBindableDifference(Bindable* orig, Bindable* neu);
	void setConfiguration(class MobiusConfig* config, bool doBindings);
	void installConfiguration(class MobiusConfig* config, bool doBindings);
	void writeConfiguration(MobiusConfig* config);
	void parseCommandLine();
	class MobiusConfig* loadConfiguration();
    class HostConfigs* loadHostConfiguration();
    class OscConfig* loadOscConfiguration();
	void updateControlSurfaces();
    void initFunctions();
    void initScriptParameters();
    void addScriptParameter(class ScriptParamStatement* s);
	void initObjectPools();
	void dumpObjectPools();
	void flushObjectPools();
	void buildTracks(int count);
	void tracePrefix();
	bool isInUse(class Script* s);
	void startScript(class Action* action, Script* s);
	void startScript(class Action* action, Script* s, class Track* t);
	void addScript(class ScriptInterpreter* si);
	class ScriptInterpreter* findScript(class Action* action, class Script* s, class Track* t);
    void doScriptMaintenance();
	void freeScripts();
    void addBinding(class BindingConfig* config, class Parameter* param, int id);

    void resolveTrigger(Binding* b, Action* a);
    class Action* resolveOscAction(Binding* b);
    void parseBindingScope(const char* scope, int* track, int* group);
    const char* getToken(const char* ptr, char* token);
    void oscUnescape(const char* src, char* dest, int max);
    ResolvedTarget* internTarget(Target* target, const char* name,
                                 int track, int group);

    void doInterruptActions();
    void doPreset(Action* a);
    void doSetup(Action* a);
    void doBindings(Action* a);
    void doFunction(Action* a);
    void doFunction(Action* action, Function* f, class Track* t);
    void doScriptNotification(Action* a);
    void doParameter(Action* a);
    void doParameter(Action* a, Parameter*p, class Track* t);
    void doControl(Action* a);
    void doUIControl(Action* a);
    void invoke(Action* a, class Track* t);

	MobiusContext* mContext;
	class ObjectPoolManager* mPools;
    class AudioPool* mAudioPool;
    class LayerPool* mLayerPool;
    class EventPool* mEventPool;
    class ActionPool* mActionPool;
	class MessageCatalog* mCatalog;
	bool mLocalized;
	MobiusListener* mListener;
    Watchers* mWatchers;
    class List* mNewWatchers;
    UIControl** mUIControls;
    UIParameter** mUIParameters;
	char* mConfigFile;
	class MobiusConfig *mConfig;
	class MobiusConfig *mInterruptConfig;
	class MobiusConfig *mPendingInterruptConfig;
	class MidiInterface* mMidi;
    class HostConfigs* mHostConfigs;

    class ResolvedTarget* mResolvedTargets;
    class BindingResolver* mBindingResolver;
    class TriggerState* mTriggerState;
    class MidiExporter* mMidiExporter;
    class OscConfig* mOscConfig;
	class OscRuntime* mOsc;
    class ControlSurface* mControlSurfaces;

	Recorder* mRecorder;
    class MobiusThread* mThread;
    class Track** mTracks;
	class Track* mTrack;
	int mTrackCount;
    int mTrackIndex;
	class SampleTrack* mSampleTrack;
	class UserVariables* mVariables;
	class ScriptEnv* mScriptEnv;
    class Function** mFunctions;
	class ScriptInterpreter* mScripts;
    class Action* mRegisteredActions;
    class Action *mActions;
    class Action *mLastAction;
	bool mHalting;
	bool mNoExternalInput;
	AudioStream* mInterruptStream;
	long mInterrupts;
	char mCustomMode[MAX_CUSTOM_MODE];
	class Synchronizer* mSynchronizer;
	class CriticalSection* mCsect;

	// pending project to be loaded
	class Project* mPendingProject;

    // pending samples to install
	class SamplePack* mPendingSamples;

	// pending project to be saved
	class Project* mSaveProject;
	
    // pending setup to switch to
    int mPendingSetup;

    // number of script threads launched
    int mScriptThreadCounter;

	// state related to realtime audio capture
	Audio* mAudio;
	bool mCapturing;
	long mCaptureOffset;
	
	// state exposed to the outside world
	MobiusState mState;
    MobiusAlerts mAlerts;

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
