/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Ladies and gentlemen, the Mobius looping engine.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#include "Util.h"
#include "Thread.h"
#include "List.h"
#include "MessageCatalog.h"

#include "MidiByte.h"
#include "MidiEvent.h"
#include "MidiInterface.h"
#include "HostMidiInterface.h"

#include "Action.h"
#include "Binding.h"
#include "BindingResolver.h"
#include "ControlSurface.h"
#include "Event.h"
#include "Export.h"
#include "Function.h"
#include "HostConfig.h"
#include "Launchpad.h"
#include "Layer.h"
#include "Loop.h"
#include "MidiExporter.h"
#include "MobiusThread.h"
#include "Mode.h"
#include "OscConfig.h"
#include "Parameter.h"
#include "Project.h"
#include "Sample.h"
#include "Script.h"
#include "Setup.h"
#include "Synchronizer.h"
#include "Track.h"
#include "TriggerState.h"
#include "UserVariable.h"
#include "WatchPoint.h"

// temporary
#include "OldBinding.h"

#include "Mobius.h"

// debugging, defined in WinAudioInterface
#ifdef _WIN32
extern bool WinAudioCatchCallbackExceptions;
#endif

/****************************************************************************
 *                                                                          *
 *   							MOBIUS CONTEXT                              *
 *                                                                          *
 ****************************************************************************/

MobiusContext::MobiusContext()
{
	mCommandLine = NULL;
	mInstallationDirectory = NULL;
	mConfigurationDirectory = NULL;
    mConfigFile = NULL;
	mAudio = NULL;
	mMidi = NULL;
    mHostMidi = NULL;
    mPlugin = false;
	mDebugging = false;
}

MobiusContext::~MobiusContext()
{
	delete mCommandLine;
	delete mInstallationDirectory;
	delete mConfigurationDirectory;
    delete mConfigFile;
}

PUBLIC void MobiusContext::setCommandLine(const char* s)
{
	delete mCommandLine;
	mCommandLine = CopyString(s);
}

PUBLIC const char* MobiusContext::getCommandLine()
{
	return mCommandLine;
}

PUBLIC void MobiusContext::setPlugin(bool b)
{
	mPlugin = b;
}

PUBLIC bool MobiusContext::isPlugin()
{
	return mPlugin;
}

PUBLIC void MobiusContext::setDebugging(bool b)
{
	mDebugging = b;
}

PUBLIC bool MobiusContext::isDebugging()
{
	return mDebugging;
}

PUBLIC void MobiusContext::setInstallationDirectory(const char* s)
{
	delete mInstallationDirectory;
	mInstallationDirectory = CopyString(s);
}

PUBLIC const char* MobiusContext::getInstallationDirectory()
{
	return mInstallationDirectory;
}

PUBLIC void MobiusContext::setConfigurationDirectory(const char* s)
{
	delete mConfigurationDirectory;
	mConfigurationDirectory = CopyString(s);
}

PUBLIC const char* MobiusContext::getConfigurationDirectory()
{
	return mConfigurationDirectory;
}

PUBLIC void MobiusContext::setConfigFile(const char* s)
{
	delete mConfigFile;
	mConfigFile = CopyString(s);
}

PUBLIC const char* MobiusContext::getConfigFile()
{
	return mConfigFile;
}

PUBLIC void MobiusContext::setAudioInterface(AudioInterface* a)
{
	mAudio = a;
}

PUBLIC AudioInterface* MobiusContext::getAudioInterface()
{
	return mAudio;
}

PUBLIC void MobiusContext::setMidiInterface(MidiInterface* m)
{
	mMidi = m;
}

PUBLIC MidiInterface* MobiusContext::getMidiInterface()
{
	return mMidi;
}

PUBLIC void MobiusContext::setHostMidiInterface(HostMidiInterface* m)
{
	mHostMidi = m;
}

PUBLIC HostMidiInterface* MobiusContext::getHostMidiInterface()
{
	return mHostMidi;
}

/**
 * Kludge to look for a few special command line args for debugging.
 * Normally the only command line arg is the name of a config file.
 * Really need to have a real command line parser.
 */
PUBLIC void MobiusContext::parseCommandLine()
{
	mDebugging = StringEqualNoCase(mCommandLine, "debugging");
}

/****************************************************************************
 *                                                                          *
 *                               MOBIUS ALERTS                              *
 *                                                                          *
 ****************************************************************************/

PUBLIC MobiusAlerts::MobiusAlerts()
{
    audioInputInvalid = false;
    audioOutputInvalid = false;
    midiInputError = NULL;
    midiOutputError = NULL;
    midiThroughError = NULL;
}

/****************************************************************************
 *                                                                          *
 *                              MOBIUS INTERFACE                            *
 *                                                                          *
 ****************************************************************************/

/**
 * Factory method for the environment glue to use to create the Mobius 
 * engine.  
 */
PUBLIC MobiusInterface* MobiusInterface::getMobius(MobiusContext* con)
{
    return new Mobius(con);
}

/****************************************************************************
 *                                                                          *
 *                                   MOBIUS                                 *
 *                                                                          *
 ****************************************************************************/

/**
 * Bring up enough of the Mobius engine that we can read our configuration
 * but don't open any devices or launch any threads.  When the application
 * is ready it will call start() to complete the initialization.
 *
 * This is necessary for plugin hosts that have a two-phase start process
 * that typically instantiate hosts to probe them and build a cache, then
 * later fully start them.
 * 
 * The context is expected to have the command line argument if 
 * run from the command line and possibly an OS handle to the "instance"
 * from which we can derive the install directory.    The command
 * line may be used to specify an alternate config file.
 *
 * The stream and midi objects are passed only when being created
 * as a plugin.
 */

PUBLIC Mobius::Mobius(MobiusContext* context)
{
    Trace(2, "Mobius::Mobius");

	// really must pass one in, but avoid crashes and fake one up
	if (context == NULL)
	  context = new MobiusContext();

	mContext = context;
	mPools = NULL;
    mAudioPool = new AudioPool();
    mLayerPool = new LayerPool(mAudioPool);
    mEventPool = new EventPool();
    mActionPool = new ActionPool();
	mMidi = NULL;
    mListener = NULL;
    mUIControls = NULL;
    mUIParameters = NULL;
	mConfig = NULL;
    mInterruptConfig = NULL;
    mPendingInterruptConfig = NULL;
    mPendingSetup = -1;
    mScriptThreadCounter = 0;
    mResolvedTargets = NULL;
    mBindingResolver = NULL;
    mMidiExporter = NULL;
	mOsc = NULL;
    mControlSurfaces = NULL;
    mTriggerState = new TriggerState();
	mRecorder = NULL;
	mThread = NULL;
	mTracks = NULL;
	mTrack = NULL;
	mTrackCount = 0;
	mSampleTrack = NULL;
	mVariables = new UserVariables();
    mFunctions = NULL;
	mScriptEnv = NULL;
	mScripts = NULL;
    mActions = NULL;
    mLastAction = NULL;
	mInterruptStream = NULL;
	mInterrupts = 0;
	mCustomMode[0] = 0;
	mPendingProject = NULL;
	mPendingSamples = NULL;
	mSaveProject = NULL;
	mAudio = NULL;
	mCapturing = false;
	mCaptureOffset = 0;
	mSynchronizer = NULL;
	mHalting = false;
	mNoExternalInput = false;
	mCatalog = NULL;
    mWatchers = new Watchers();
    mNewWatchers = new List();

	// need this to manage the action list
	mCsect = new CriticalSection("Mobius");

    // let's turn debug stream output on for now, what uses this??
    TraceToDebug = true;
    
    // initialize the static object tables
    MobiusMode::initModes();
    Function::initStaticFunctions();
    Parameter::initParameters();

	parseCommandLine();

	// reads the configuration file, but don't open devices
    // do not call installConfiguration yet, just save it for later
	mConfig = loadConfiguration();
    mHostConfigs = loadHostConfiguration();

	// set these early so we can trace errors during initialization
	TracePrintLevel = mConfig->getTracePrintLevel();
	TraceDebugLevel = mConfig->getTraceDebugLevel();

    // Too much code assumes this is non-null unfortuantely.
    // If we're not connected to an audio input code still
    // gets called for the UI update timer so we need to 
    // have enough state installed to respond
    mInterruptConfig = new MobiusConfig(true);
}

/**
 * Kludge to look for a few special command line args for debugging.
 * Normally the only command line arg is the name of a config file.
 * Really need to have a real command line parser.
 */
PRIVATE void Mobius::parseCommandLine()
{
	// context has its own parser
	mContext->parseCommandLine();
	if (mContext->isDebugging()) {
		// TODO: Should do this for the Thread::run method to...
		AudioInterfaceCatchExceptions = false;
	}
}

/****************************************************************************
 *                                                                          *
 *                              MOBIUS INTERFACE                            *
 *                                                                          *
 ****************************************************************************/

PUBLIC MobiusContext* Mobius::getContext()
{
    return mContext;
}

PUBLIC AudioStream* Mobius::getAudioStream()
{
	return (mRecorder != NULL) ? mRecorder->getStream() : NULL;
}

/**
 * Do internal preparations for exposing binding targets
 * including parameters, functions, and scripts.  Normally this
 * is deferred until the start() method is called because it can
 * be expensive.  
 *
 * Construction of the AU plugin requires that plugin parameters
 * be exposed immediately so this can't be delayed.  
 *
 * What this does is initialize all the function tables, load
 * all the scripts, and perform localization.
 *
 * !! Could at least whip over the parameters and skip this  
 * if none of them are function bingings?
 */
PUBLIC void Mobius::preparePluginBindings() 
{
    // load scripts and initialize function tables
    installScripts(mConfig->getScriptConfig(), false);

    // do localization early so we can publish display names
    localize();

    // Bootstrap an initial set of parameter bindings if we don't have any
    // Formerly done in MobiusPlugin but I don't want it editing configs.

    // in theory there can be overlays...we won't track those
    BindingConfig *bconfig = mConfig->getBaseBindingConfig();

    int hostBindings = 0;
    if (bconfig != NULL) {
        for (Binding* b = bconfig->getBindings() ; b != NULL ; 
             b = b->getNext()) {
            if (b->getTrigger() == TriggerHost)
              hostBindings++;
        }
    }

    if (hostBindings == 0) {
        if (bconfig == NULL) {
            bconfig = new BindingConfig();
            mConfig->addBindingConfig(bconfig);
        }

        // could also use controls for these, any preference?
        // !! yes, make these controls I want to deprecate the parameters
        int id = 0;
        addBinding(bconfig, InputLevelParameter, id++);
        addBinding(bconfig, OutputLevelParameter, id++);
        addBinding(bconfig, FeedbackLevelParameter, id++);
        addBinding(bconfig, AltFeedbackLevelParameter, id++);
        addBinding(bconfig, PanParameter, id++);
        addBinding(bconfig, TrackParameter, id++);
        addBinding(bconfig, QuantizeParameter, id++);
        addBinding(bconfig, SwitchQuantizeParameter, id++);

        writeConfiguration(mConfig);
    }
}

/**
 * Bootstrap a binding for a parameter.
 */
PRIVATE void Mobius::addBinding(BindingConfig* config, Parameter* param, int id)
{
    Binding* b = new Binding();
    b->setTarget(TargetParameter);
    b->setName(param->getName());
    b->setTrigger(TriggerHost);
    b->setValue(id);
    config->addBinding(b);
}

/**
 * Finish Mobius initialization, initialize tracks, and open devices.
 */
PUBLIC void Mobius::start()
{
	if (mRecorder == NULL) {

        // kludge for debugging during the interrupt, need to 
        // have a way to configure this
#ifdef _WIN32
        WinAudioCatchCallbackExceptions = false;
#endif

		initObjectPools();

        // listen for MIDI events
		mMidi = mContext->getMidiInterface();
		mMidi->setListener(this);
		// get the timer thread going so we don't have to initialize
		// it the moment we need to start sending clocks
		mMidi->timerStart();

        // this must not start interrupts yet
		mRecorder = new Recorder(mContext->getAudioInterface(), mMidi, 
                                 mAudioPool);
		mRecorder->setMonitor(this);
		
		mSynchronizer = new Synchronizer(this, mMidi);

		mThread = new MobiusThread(this);
		mThread->start();

		// once the thread starts we can start queueing trace messages
		if (!mContext->isDebugging())
		  mThread->setTraceListener(true);

		// put the sample track first so it may put things into the
		// input buffer for the loop tracks
		mSampleTrack = new SampleTrack(this);
		mRecorder->add(mSampleTrack);

		// this will trigger track initialization, open devices,
		// load scripts, etc.
		installConfiguration(mConfig, true);

		// start the recorder (opens streams) and begins interrupt
        mRecorder->start();

		updateControlSurfaces();

        // Formerly looked for an init.mos script and ran it.
        // Never used this and it didn't fit well in the new ScriptEnv world.
        // If we want an init script then it should be a registered event
        // script instead.

		// Open the message catalog and propagate display names to all
        // the internal objects, this may already have been done if 
        // preparePluginBidnings was called.  Could have done this earlier
        // after we intalled scripts.
		localize();

        // crank up OSC
        mOsc = new OscRuntime(this);
	}
}

/**
 * Return an object with information about unusual things that
 * have been happening so that the user can be notified.
 */
PUBLIC MobiusAlerts* Mobius::getAlerts()
{
    // always refresh device status
    mAlerts.audioInputInvalid = false;
    mAlerts.audioOutputInvalid = false;

	// ignore if we're a plugin, the fake VstStream will return  NULL device
	if (!mContext->isPlugin() && mRecorder != NULL) {
        if (mConfig->getAudioInput() != NULL) {
            AudioStream* s = mRecorder->getStream();
            mAlerts.audioInputInvalid = (s->getInputDevice() == NULL);
        }

        if (mConfig->getAudioOutput() != NULL) {
            AudioStream* s = mRecorder->getStream();
            mAlerts.audioOutputInvalid = (s->getOutputDevice() == NULL);
        }
    }

	// NOTE: mMidi may be null if the host has not yet called resume()
    // on the plugin.  Until the start() method is called, though there is
	// one in the MobiusContext.  Use this as an indication
	// not to check devices since they're not open anyway.
	// UPDATE: If Mobius isn't started all hell breaks loose because
	// various internal objects aren't initialized, so make sure
	// it has started earlier.

	if (mMidi != NULL) {
        // do we really need a message for these, isn't just a bool enough?
        mAlerts.midiInputError = mMidi->getInputError();
        mAlerts.midiOutputError = mMidi->getOutputError();
        mAlerts.midiThroughError = mMidi->getThroughError();
	}
    else {
        mAlerts.midiInputError = NULL;
        mAlerts.midiOutputError = NULL;
        mAlerts.midiThroughError = NULL;
    }

    return &mAlerts;
}

/**
 * Special latency calibration interface.
 * 
 */
PUBLIC CalibrationResult* Mobius::calibrateLatency()
{
	CalibrationResult* result = NULL;

	if (mRecorder != NULL) {
		// disable this since we won't be receiving interrupts
		// during the test
		if (mThread != NULL)
		  mThread->setCheckInterrupt(false);


        // ugh, stilly duplicate structures so the UI doesn't have to
        // be aware of Recorder and Recorder doesn't have to be aware of
        // Mobius.  Refactor this!
   
        RecorderCalibrationResult* rcr = mRecorder->calibrate();
        result = new CalibrationResult();
        result->timeout = rcr->timeout;
        result->noiseFloor = rcr->noiseFloor;
        result->latency = rcr->latency;
        delete rcr;

		// turn it back on
		mInterrupts++;
		if (mThread != NULL)
		  mThread->setCheckInterrupt(true);
	}

	return result;
}

PUBLIC int Mobius::getActiveTrack()
{
    return (mTrack != NULL) ? mTrack->getRawNumber() : 0;
}

/****************************************************************************
 *                                                                          *
 *                                UI BINDABLES                              *
 *                                                                          *
 ****************************************************************************/

/**
 * Called by the UI (down here represented as a MobiusListener)
 * to register the objects that are supported for binding.
 * To avoid having another resolver method, we'll kick off
 * resolution here after setting.  Enable warning messages about
 * missign bindings this time.
 *
 * TODO: Think about generalizing this so there is just one thing
 * that can behave like a parameter or a control!!
 */
PUBLIC void Mobius::setUIBindables(UIControl** controls, 
                                   UIParameter** parameters)
{
    mUIControls = controls;
    mUIParameters = parameters;

    // allow these to be localized
    localizeUIControls();

    // look for any resolved targets...hate how this works
    // can't bind to UIParameters yet
    for (ResolvedTarget* rt = mResolvedTargets ; rt != NULL ; rt = rt->getNext()) {
        if (rt->getTarget() == TargetUIControl && 
            rt->getObject() == NULL) {

            UIControl* uic = getUIControl(rt->getName());
            if (uic != NULL)
              rt->setObject(uic);
            else {
                // now we can complain
                Trace(1, "Unrecognized UIControl target: %s\n", rt->getName());
            }
        }
    }
}

PUBLIC UIControl** Mobius::getUIControls()
{
    return mUIControls;
}

PUBLIC UIControl* Mobius::getUIControl(const char* name)
{
    UIControl* found = NULL;
    if (name != NULL && mUIControls != NULL) {
        for (int i = 0 ; mUIControls[i] != NULL ; i++) {
            UIControl* c = mUIControls[i];
            if (StringEqualNoCase(name, c->getName()) ||
                StringEqualNoCase(name, c->getDisplayName())) {
                found = c;
                break;
            }
        }
    }
    return found;
}


/****************************************************************************
 *                                                                          *
 *                         MOBIUS PROTECTED INTERFACE                       *
 *                                                                          *
 ****************************************************************************/

/**
 * Return the sample rate.
 */
PUBLIC int Mobius::getSampleRate()
{
    int rate = CD_SAMPLE_RATE;
    if (mRecorder != NULL)
      rate = mRecorder->getStream()->getSampleRate();
    return rate;
}

/**
 * Return the set of user defined global variables.
 */
PUBLIC UserVariables* Mobius::getVariables()
{
    return mVariables;
}

/**
 * Return what we consider to be the "home" directory.
 * This is where we expect to find configuration files, 
 * where we put captured audio files, and where we expect
 * to find scripts when using relative paths.
 */
PUBLIC const char* Mobius::getHomeDirectory()
{
	// TODO: MobiusThread supports a MOBIUS_HOME environment
	// variable override, should we do that too?

	const char* home = mContext->getConfigurationDirectory();
	if (home == NULL)
	  home = mContext->getInstallationDirectory();

	return home;
}

/**
 * We're a trace context, supply track/loop/time.
 */
PUBLIC void Mobius::getTraceContext(int* context, long* time)
{
	*context = 0;
	*time = 0;
}

/**
 * Install control surface handlers for the globally registered
 * control surfaces.
 *
 * Just a stub right now for the Launchpad, need to figure out
 * how to make this more pluggable.
 */
PRIVATE void Mobius::updateControlSurfaces()
{
	delete mControlSurfaces;
	mControlSurfaces = NULL;

	ControlSurfaceConfig* configs = mConfig->getControlSurfaces();
	for (ControlSurfaceConfig* config = configs ; config != NULL ; 
		 config = config->getNext()) {

		if (StringEqual(config->getName(), "launchpad")) {
			ControlSurface* cs = new Launchpad(this);
			cs->setNext(mControlSurfaces);
			mControlSurfaces = cs;
		}
	}
}

/**
 * Shut mobius down, but leave most of the structure intact.
 * Factored this out of the destructor to track down an annoying
 * race condition when the VST plugin is deleted.
 */
PRIVATE void Mobius::stop()
{
	mHalting = true;

	// no more events, especially important if clocks are being received
	if (mMidi != NULL) {
		mMidi->setListener(NULL);
		// Transport should have done this but make sure
		mMidi->setClockListener(NULL);
	}

	if (mThread != NULL && mThread->isRunning()) {
		if (!mThread->stopAndWait()) {
			// unusual, must be stuck, continuing may crash
			NewTraceListener = NULL;
			Trace(1, "Mobius: Unable to stop Mobius thread!\n");
		}
	}

	// shutting down the Recorder will stop the timer which will send
	// a final MIDI stop event if the timer has a MidiOutput port,
	// not sure how necessary that is if we're being deleted, but
	// may as well
	if (mRecorder != NULL)
	  mRecorder->shutdown();

	// sleep to make sure we're not in a timer or midi interrupt
	SleepMillis(100);

	// paranioa to help catch shutdown errors
	for (int i = 0 ; i < mTrackCount ; i++) {
		Track* t = mTracks[i];
		t->setHalting(true);
	}

    // !! clear the Layer pool?  Not if we're in a VST and will
	// resume again later...
	// this could cause large leaks

	// NOTE: Do not assume that we can shut down the MidiInterface,
	// this may be shared if the VST DLL is open more than once?
	// Or if the VST is brought up again after closing.
}

/**
 * Called by MobiusThread when we think the interrupt handler looks
 * stuck.  Simply calling exit() usually leave the process alive in
 * some limbo state for a few minutes, but it eventually dies.  The problem
 * is that while it is in limbo, the audio and midi devices are left open
 * and you can't start another mobius process.  Try to close the devices first.
 */
PUBLIC void Mobius::emergencyExit()
{
	Trace(1, "Mobius: emergency exit!\n");
	Trace(1, "Mobius: Shutting down MIDI...\n");
	MidiInterface::exit();

	Trace(1, "Mobius: Shutting down Audio...\n");
	AudioInterface::exit();

	Trace(1, "Mobius: Attempting to exit...\n");
	exit(1);	
}

PUBLIC Mobius::~Mobius()
{
	if (!mHalting)
	  stop();
	else {
		printf("Mobius::~Mobius mHalting was set!\n");
		fflush(stdout);
	}

    // interesting stats
    Trace(2, "Mobius: %ld MobiusConfigs on the history list\n",
          (long)mConfig->getHistoryCount());

    // Assume mUIControls was set from a static array
    // and does not need to be freed.

    delete mWatchers;
    delete mTriggerState;
	delete mRecorder;	// will delete the Tracks too
	delete mThread;
	delete mContext;
	delete mConfig;
    delete mInterruptConfig;
    delete mPendingInterruptConfig;
	delete mBindingResolver;
    delete mMidiExporter;
	delete mOsc;
    delete mControlSurfaces;
    delete mFunctions;
	delete mScriptEnv;
	delete mTracks;
	delete mSynchronizer;
	delete mCatalog;
    delete mVariables;

    // avoid a warning message
    for (ResolvedTarget* t = mResolvedTargets ; t != NULL ; t = t->getNext())
      t->setInterned(false);
    delete mResolvedTargets;

	flushObjectPools();
    
    mActionPool->dump();
    delete mActionPool;

    mEventPool->dump();
    delete mEventPool;

    mLayerPool->dump();
    delete mLayerPool;

    mAudioPool->dump();
    delete mAudioPool;
}

PUBLIC MessageCatalog* Mobius::getMessageCatalog()
{
    // no one should need this until we're fully initialized, 
    // if so trace it and figure out why

	return mCatalog;
}

PUBLIC void Mobius::setCheckInterrupt(bool b)
{
	if (mThread != NULL)
	  mThread->setCheckInterrupt(b);
}

PUBLIC void Mobius::setListener(MobiusListener* l)
{
	mListener = l;
}

/**
 * Internal use only.
 */
PUBLIC Watchers* Mobius::getWatchers()
{
    return mWatchers;
}

/**
 * For MobiusThread only.
 */
PUBLIC MobiusListener* Mobius::getListener()
{
	return mListener;
}

/**
 * Thread access for internal components.
 */
PUBLIC class MobiusThread* Mobius::getThread() 
{
    return mThread;
}

/*
PUBLIC void Mobius::addEvent(ThreadEvent* te)
{
	if (mThread != NULL)
	  mThread->addEvent(te);
	else
	  delete te;
}

PUBLIC void Mobius::addEvent(ThreadEventType type)
{
	if (mThread != NULL)
	  mThread->addEvent(type);
}
*/

/**
 * True if we're currently processing an audio interrupt.
 * Used with getInterrupts to determine whether the interrupt
 * handler is stuck in an infinite loop.
 */
PUBLIC bool Mobius::isInInterrupt()
{
	return (mInterruptStream != NULL);
}

/**
 * The number of audio interrupts we've serviced.
 * Used by MobiusThread to detect infinite loops during interrupts which
 * will lock up the machine.
 */
PUBLIC long Mobius::getInterrupts()
{
	return mInterrupts;
}

/**
 * The current value of the millisecond clock.
 */
PUBLIC long Mobius::getClock()
{
	return ((mMidi != NULL) ? mMidi->getMilliseconds() : 0);
}

PUBLIC Synchronizer* Mobius::getSynchronizer()
{
	return mSynchronizer;
}

/**
 * Return true if the given track has input focus.
 * Prior to 1.43 track groups had automatic focus
 * beheavior, now you have to ask for that with the
 * groupFocusLock global parameter.
 */
PRIVATE bool Mobius::isFocused(Track* t) 
{
    int group = t->getGroup();

    return (t == mTrack || 
            t->isFocusLock() ||
            (mConfig->isGroupFocusLock() && 
             group > 0 && 
             group == mTrack->getGroup()));
}

/**
 * Return the Setup from the interrupt configuration.
 * Used by Synchronizer when it needs to get setup parameters.
 */
PUBLIC Setup* Mobius::getInterruptSetup()
{
    return mInterruptConfig->getCurrentSetup();
}

/****************************************************************************
 *                                                                          *
 *                               CONFIGURATION                              *
 *                                                                          *
 ****************************************************************************/

/**
 * Return a read-only configuration object describing how various
 * plugin hosts behave.  This is not editable and intended only
 * for use by the VST and AU plugin implmeentations.
 */
PUBLIC HostConfigs* Mobius::getHostConfigs()
{
    // formerly inside MobiusConfig, now standalone
    return mHostConfigs;
}

/**
 * Return the read-only configuration for the UI threads, MobiusThread, 
 * and anything else "outside the interrupt".
 * 
 * If you accidentally call this from within the interrupt it will probably
 * work but you're not ensured that the same MobiusConfig object will
 * be valid for the duration of the interrupt.
 */
PUBLIC MobiusConfig* Mobius::getConfiguration()
{
    if (mConfig == NULL) {
        Trace(1, "Bootstrapping empty configuration!\n");
        mConfig = new MobiusConfig();
    }
	return mConfig;
}

/**
 * This is what all non-UI code should call to make it clear what it wants.
 */
PUBLIC MobiusConfig* Mobius::getMasterConfiguration()
{
    return getConfiguration();
}

/**
 * Get the MobiusConfig object for use by code within the interrupt handler.
 * This is guarenteed not to change for the duration of the interrupt.
 */
PUBLIC MobiusConfig* Mobius::getInterruptConfiguration()
{
    return mInterruptConfig;
}

/**
 * Get the inner Recorder.  This is exposed only for MonitorAudioParameter.
 * Think about adding a special method to propagate this?
 */
PUBLIC Recorder* Mobius::getRecorder()
{
    return mRecorder;
}

/**
 * Get the preset currently being used by the selected track.
 * We return an index because the Preset stored on the track is part
 * of mInterruptConfig and that can't escape.  If caller needs the
 * Preset object they have to search in the public MobiusConfig.
 */
PUBLIC int Mobius::getTrackPreset()
{
    // this is from the InterruptConfig
    Preset* p = mTrack->getPreset();
    // !! potential race condition if we're shifting the interrupt
    // config at this moment, p could be deleted
    // ugh, may have to maintain history here too
    int index = p->getNumber();

    return index;
}

/**
 * Change the current binding overlay.  
 * Note that this does not need to rebuild the BindingResolver model,
 * the next time we get a trigger we just look in the new overlay.
 * 
 * This is public only for BindingsParameter.
 */
PUBLIC void Mobius::setOverlayBindings(BindingConfig* config)
{
    mConfig->setOverlayBindingConfig(config);
}

/**
 * Locate and read the configuration file.
 * There are two places this can be, a "configuration" directory and
 * an "installation" directory.  On Windows these will always be the same,
 * on OSX the installation directory will normally be the Resources
 * directory of the application bundle and the configuration directory
 * will be under /Library/Application Support.  We try to bootstrap a
 * support directory, if we fail the config directory defaults to the
 * installation directory.
 */
PRIVATE MobiusConfig* Mobius::loadConfiguration()
{
	MobiusConfig *config = NULL;

	// locate config file
    const char* file = mContext->getConfigFile();
	if (file == NULL) {
        char buffer[1024 * 8];

		if (findConfigurationFile("mobius.xml", buffer, sizeof(buffer))) {
            mContext->setConfigFile(buffer);
        }
		else {
			// punt and create one here
			// this is almost always wrong if we're a VST!!
            // !! Need some way to store serious errors so they can
            // be presented when the UI finally comes up
			Trace(1, "No mobius.xml file found, creating one!\n");
			GetFullPath("mobius.xml", buffer, sizeof(buffer));
            mContext->setConfigFile(buffer);
		}
        file = mContext->getConfigFile();
	}

    printf("Reading Mobius configuration file: %s\n", file);
    fflush(stdout);
    char* xml = ReadFile(file);
    if (xml == NULL || strlen(xml) == 0) {
        // leave an error message behind to show when the UI
        // eventually comes up?
        Trace(1, "Mobius: Empty mobius.xml file\n");
    }
    else {
        config = new MobiusConfig(xml);
        delete xml;
        xml = NULL;

        if (config->getError() != NULL) {
            // save error for later display?
            Trace(1, "Mobius: Exception loading configuration %s\n", 
                  config->getError());
        }

        // kludge: we took this out of the UI so ignore it in XML files
        // until we can find a use for it.  Have to do this up here
        // rather than in the constructor because we parse XML for cloning
        // and we do need to turn this on and off in the unit tests
        for (Preset* p = config->getPresets() ; p != NULL ; p = p->getNext())
          p->setNoLayerFlattening(false);
    }

	// if at the end of all this we were'nt able to load anything
	// create some dummy objects so we don't have to check anymore
    bool upgrade = false;
	if (config == NULL) {
		config = new MobiusConfig();
        upgrade = true;
	}

    if (config->getPresets() == NULL) {
        Trace(2, "Mobius: Bootstrapping preset\n");
        Preset* p = new Preset();
        p->reset();
        p->setName("Default");
        config->addPreset(p);
        upgrade = true;
    }

    // Upgrade the old track groups parameter from the MidiConfig
    // to the global config.
    if (config->getTrackGroups() == 0) {
        int groups = DEFAULT_TRACK_GROUPS;
        MidiConfig* mcs = config->getMidiConfigs();
        if (mcs != NULL) {
            for (MidiConfig* mc = mcs ; mc != NULL ; mc = mc->getNext()) {
                int count = mc->getTrackGroups();
                if (count > groups) 
                  groups = count;
            }
        }
        Trace(2, "Mobius: Upgrading track groups parameter to %ld\n", (long)groups);
        config->setTrackGroups(groups);
        upgrade = true;
    }

    // since the default on this is low always look through the presets
    // to make sure it will be large enough
    int maxLoops = 0;
    Preset* presets = config->getPresets();
    if (presets != NULL) {
        for (Preset* p = presets ; p != NULL ; p = p->getNext()) {
            int pcount = p->getLoops();
            if (pcount > maxLoops)
              maxLoops = pcount;
        }
    }
    if (maxLoops > config->getMaxLoops()) {
        Trace(2, "Mobius: Upgrading max loops parameter to %ld\n", (long)maxLoops);
        config->setMaxLoops(maxLoops);
        upgrade = true;
    }

    // upgrade MidiConfigs to BindingConfig overlays
    // don't merge into the base config since we can't tell if they're
    // intended to be completely distinct
    MidiConfig* mconfigs = config->getMidiConfigs();
    if (mconfigs != NULL) {
        Trace(2, "Upgrading MidiConfigs\n");
        BindingConfig* bconfigs = config->getBindingConfigs();
        if (bconfigs != NULL && bconfigs->getNext() != NULL) {
            // already had overlays, odd
            Trace(1, "Upgrading MidiConfigs into existing overlays\n");
        }

        for (MidiConfig* mc = mconfigs ; mc != NULL ; mc = mc->getNext()) {
            BindingConfig* neu = mc->upgrade();
            // in theory we have to worry about name collisions with
            // the existing overlays, but we shouldn't have overlays
            // if we're upgradeing MidiConfigs
            config->addBindingConfig(neu);
        }

        if (config->getOverlayBindingConfig() == NULL) {
            const char* selected = config->getSelectedMidiConfig();
            if (selected != NULL)
              config->setOverlayBindingConfig(selected);
        }

        // don't leave them around, only convert once
        config->clearMidiConfigs();
        upgrade = true;
    }

    // have to do this after setting mConfig
    if (upgrade)
      writeConfiguration(config);

    // KLUDGE: we have one Parameter whose maximum value can 
    // be configured: selectedTrack.  Since we have to know the
    // maximum value for the parameter this either has to be
    // set after we load the configuration, or we have to look
    // it up at run time.  It's much easier to set it here,
    // runtime lookup requires passing in an Export or some
    // other way to get to the MobiusConfig.  This can't be changed
    // without a restart anyway.
    TrackParameter->high = config->getTracks();

    return config;
}

PRIVATE HostConfigs* Mobius::loadHostConfiguration()
{
	HostConfigs *config = NULL;
    
	// locate config file
    char buffer[1024 * 8];

    if (!findConfigurationFile("host.xml", buffer, sizeof(buffer))) {
        // could bootstrap one but it really should exist
        Trace(1, "No host.xml file found!\n");
        config = new HostConfigs();
    }
    else {
        printf("Reading Mobius host configuration file: %s\n", buffer);
        fflush(stdout);
        char* xml = ReadFile(buffer);
        if (xml == NULL || strlen(xml) == 0) {
            // leave an error message behind to show when the UI
            // eventually comes up?
            Trace(1, "Empty host.xml file\n");
        }
        else {
            config = new HostConfigs(xml);
            delete xml;
            xml = NULL;

            if (config->getError() != NULL) {
                // save error for later display when we have a window?
                Trace(1, "Exception loading host.xml %s\n", config->getError());
            }
        }
    }

    return config;
}

/**
 * This is public so the UI can all it from a menu.
 */
PUBLIC void Mobius::reloadOscConfiguration()
{
    if (mOsc != NULL)
      mOsc->reloadConfigurationFile(this);
}

/**
 * Locate a configuration file.  
 * Prefer the alternate configuration directory if we have one, 
 * otherwise default to the installation directory.
 */
PUBLIC bool Mobius::findConfigurationFile(const char* name, char* path, int max)
{
	bool found = false;

	strcpy(path, "");

    // !! TODO range check the path

	// check configuration directory
	const char* srcdir = mContext->getConfigurationDirectory();
	if (srcdir != NULL) {
		sprintf(path, "%s/%s", srcdir, name);
		found = IsFile(path);
	}

	if (!found) {
		// check installation directory
		const char* srcdir = mContext->getInstallationDirectory();
		if (srcdir != NULL) {
			sprintf(path, "%s/%s", srcdir, name);
			found = IsFile(path);
		}
	}
	
	return found;
}

/**
 * Return true if the list of bindables has changed in a way that
 * is significant for bindings.  The significant changes are adding,
 * removing, or renaming objects.
 */
PRIVATE bool Mobius::isBindableDifference(Bindable* orig,
                                          Bindable* neu)
{
    bool difference = false;
    Bindable* b;
    
    int origCount = 0;
    for (b = orig ; b != NULL ; b = b->getNextBindable())
      origCount++;

    int newCount = 0;
    for (b = neu ; b != NULL ; b = b->getNextBindable())
      newCount++;

    if (newCount != origCount) {
        difference = true;
    }
    else {
        // technicacally we shouldn't let order make a difference
        // but this is just an configuration update optimization
        while (orig != NULL && !difference) {
            if (!StringEqual(orig->getName(), neu->getName()))
              difference = true;
            else {
                orig = orig->getNextBindable();
                neu = neu->getNextBindable();
            }
        }
    }

    return difference;
}

/**
 * Return a standalone read/write version of the configuration.
 * This is intended for use by the UI that will make changes
 * and then call setPresetConfiguration, setSetupConfiguration, 
 * setGeneralConfiguration, setBindingConfiguration, or
 * setFullConfiguration.
 */
PUBLIC MobiusConfig* Mobius::editConfiguration()
{
    // bootstrap if necessary
    MobiusConfig* config = getConfiguration();

    // make sure these are clear, actually it doesn't matter
    config->setNoSetupChanges(false);
    config->setNoPresetChanges(false);

    // make the selected preset reflect the current track
    Preset* p = mTrack->getPreset();
    config->setCurrentPreset(p->getNumber());

    return config->clone();
}

/**
 * Called by the UI to update our configuration after preset editing.
 * This is a kludge to prevent transient track and setup parameters from
 * being lost if we did not edit presets.  I don't like this "scoping"
 * of config edits.
 */
PUBLIC void Mobius::setPresetConfiguration(MobiusConfig* config)
{
    config->setNoSetupChanges(true);
    config->setNoPresetChanges(false);

    // do bindings only if the presets have changed
    bool doBindings = isBindableDifference(mConfig->getPresets(), 
                                           config->getPresets());

    setConfiguration(config, doBindings);
}

/**
 * Called by the UI to update our configuration after setup editing.
 * This is a kludge to prevent transient parameters from
 * being lost if we did not edit setups.  I don't like this "scoping"
 * of config edits.
 */
PUBLIC void Mobius::setSetupConfiguration(MobiusConfig* config)
{
    config->setNoSetupChanges(false);
    config->setNoPresetChanges(true);

    // do bindings only if the setups have changed
    bool doBindings = isBindableDifference(mConfig->getSetups(), 
                                           config->getSetups());

    setConfiguration(config, false);
}

/**
 * KLUDGE: Configuration updater that sets two flags to prevent
 * refreshing preset and setup state in tracks.
 * Call this when you edit anything in the config except presets
 * and setups.
 *
 * Currently used by UI after editing the MIDI Devices, Audio Devices,
 * global parameters, scripts, and samples.
 */
PUBLIC void Mobius::setGeneralConfiguration(MobiusConfig* config)
{
    config->setNoSetupChanges(true);
    config->setNoPresetChanges(true);
    setConfiguration(config, false);
}

/**
 * Apply changes to the congiguration that are only related to 
 * bindings.
 *
 * Currently used by UI after editing the MIDI bindings, Keyboard bindings,
 * and plugin parameters.
 */
PUBLIC void Mobius::setBindingConfiguration(MobiusConfig* config)
{
    config->setNoSetupChanges(true);
    config->setNoPresetChanges(true);
    setConfiguration(config, true);
}

PUBLIC void Mobius::setFullConfiguration(MobiusConfig* config)
{
    config->setNoSetupChanges(false);
    config->setNoPresetChanges(false);
    setConfiguration(config, true);
}

/**
 * Assimilate changes made to an external copy of the configuration object.
 * This is intended for use by the UI after it has created a clone
 * of the system config object and modified it.  
 *
 * !! Consider passing in the parts that were modified so we can avoid
 * unnecessary work?
 *
 */
PRIVATE void Mobius::setConfiguration(MobiusConfig* config, bool doBindings)
{
    if (config == mConfig) {
        // not supposed to be doing this!!
        Trace(1, "Mobius: setConfiguration called without a clone!\n");
        delete config;
    }
    else {
        installConfiguration(config, doBindings);

        writeConfiguration(config);

        // If the track count changed, send the UI an alert so the user
        // knows they have to restart.  This can only be done from the UI thread
        // which is the only thing that should be calling setConfiguration.

        if (config->getTracks() != mTrackCount) {

            // Alert handler must either process the message immediately or
            // copy it so we can use a stack buffer
            char message[1024];
            sprintf(message, "You must restart Mobius to change the track count to %d", config->getTracks());
            if (mListener != NULL)
              mListener->MobiusAlert(message);
        }
    }
}

/**
 * Save whatever is in the current configuration.
 */
PUBLIC void Mobius::writeConfiguration()
{
    writeConfiguration(mConfig);
}

/**
 * Write the configuration back to the file it was read from.
 */
PRIVATE void Mobius::writeConfiguration(MobiusConfig* config)
{
	const char* file = mContext->getConfigFile();
	if (config != NULL && file != NULL) {
		printf("Writing Mobius configuation file: %s\n", file);
        fflush(stdout);
		char* xml = config->toXml();
		WriteFile(file, xml);
		delete xml;
	}
}

/**
 * Install the configuration. This can be called in two contexts.
 * First by start() after we've read the config file and now want
 * to process it.  In this case the passed MobiusConfig object will
 * be the same as what's in mConfig.
 *
 * Second by the UI after it has edited an external copy of the config object.
 * In this case we need to splice it in carefully since the
 * interrupt handler, MobiusThread, and the trigger threads can still be using
 * the old one.
 *
 * !! KLUDGE
 * Since we don't have a reliable way to to know whether the current
 * config object is in use by the UI, MobiusThread, or trigger threads
 * we can't safely delete the old config object immediately.  Insteaed
 * maintain a history of them.  Eventually the old ones can be removed
 * though it's still a guess as to when nothing will be referencing it.
 * Since setConfiguration is only called when you interact with the
 * UI dialogs in practice there shouldn't be very many of these and
 * comapred to the audio buffers they don't take up much space
 *
 * TODO: to be completely safe we need a csect around this to prevent
 * concurrent mods to the history list. In practice that's almost impossible
 * because all dialogs are modal.
 *
 */
PRIVATE void Mobius::installConfiguration(MobiusConfig* config, bool doBindings)
{
    // Push the new one onto the history list
    // Need to be smarter about detecting loops in case the UI isn't
    // behaving well and giving us old objects
    if (config != mConfig) {
        config->setHistory(mConfig);
        mConfig = config;
    }
    
    // Sanity check on some important parameters
    // TODO: Need more of these...
    if (config->getTracks() <= 0) {
        Trace(1, "Fixing track count\n");
        config->setTracks(1);
    }

	// Build the track list if this is the first time
	buildTracks(config->getTracks());

	// load the samples
    // note that installation has to be deferred to the interrupt handler
    SamplePack* newSamples = NULL;
	Samples* samples = config->getSamples();
    if (samples != NULL) {
        // only reload if there was a difference in order or files 
        // we could be smarter and only reread things that are new
        // but this isn't a commonly used features
        if (mSampleTrack->isDifference(samples))
          newSamples = new SamplePack(mAudioPool, getHomeDirectory(), samples);
    }
    else {
        // in order to remove current samples we need a non-null
        // SamplePack object to pass to the interrupt
        if (mSampleTrack->getSampleCount() > 0)
          newSamples = new SamplePack();
    }

    if (newSamples != NULL) {
        // this is bad, it would be safer just to ignore the shift
        // but then we couldn't edit samples before we add audio devices
        // !! ignore if we're receiving interrupts but allow otherwise
        // this can happen if you're messing with configs and don't have
        // an audio device selected
        Trace(2, "Mobius: phasing in sample changes\n");
        if (mPendingSamples != NULL) {
            if (mInterrupts > 0)
              Trace(1, "Mobius: Overflow installing samples\n");
            else
              delete mPendingSamples;
        }
        mPendingSamples = newSamples;
    }

    // shift this into the interrupt thread
    // !! I'm sure there are some small race conditions below where
    // we're making structural changes to tracks and such that may
    // not match what is in the active mInterruptConfig.
    // Find out what those are and move them into the interrupt.
    Trace(2, "Mobius: phasing in MobiusConfig changes\n");
    if (mPendingInterruptConfig != NULL) {
        if (mInterrupts > 0) 
          Trace(1, "Mobius: Overflow installing interrupt configuration!\n");
        else
          delete mPendingInterruptConfig;
    }
    mPendingInterruptConfig = config->clone();

	// load the scripts and setup function tables
    if (installScripts(config->getScriptConfig(), false)) {
        // if scripts changed, then force the bindings to be rebuilt too
        // !! should also force the MobiusPluginParameters to be rebuilt
        // since they can be referencing the old RunScriptFunction objects,
        // as it is they will continue to ref the old scripts
        doBindings = true;
    }

    // update focus lock/mute cancel limits
    updateGlobalFunctionPreferences();

	// global settings
    // These are safe to set from anywhere don't have to wait for an interrupt
	TracePrintLevel = config->getTracePrintLevel();
	TraceDebugLevel = config->getTraceDebugLevel();

    // !! this could cause problems if we're in the middle of saving
    // a project?  Would need to coordinate this with MobiusThread
	Audio::setWriteFormatPCM(config->isIntegerWaveFile());

    // Open devices 
	// Avoid messing with actual devices if we're in test mode
    // Recorder is smart to not open/close devices if nothing changed
	if (mRecorder != NULL) {

		bool success = false;
		
		// If we're a VST, then we normally don't open MIDI devices unless
		// specifically directed.  If there is an "external" MidiInterface it
		// means we're a plugin

		// note that it is important to call the MidiInterface set*Port
		// methods even if the port is null so we can close the previous ports

		const char* input = mConfig->getMidiInput();
		if (mContext->isPlugin())
		  input = mConfig->getPluginMidiInput();

		success = mMidi->setInput(input);
		if (!success) {
			Trace(1, "Mobius: Unable to open MIDI input: %s\n", input);
			// would be nice to popup a dialog, but don't assume we have a UI
			// if we do it will call one of the get*Error methods to
			// get the error messages
		}

		const char* output = mConfig->getMidiOutput();
		if (mContext->isPlugin())
		  output = mConfig->getPluginMidiOutput();

		success = mMidi->setOutput(output);
		if (!success)
		  Trace(1, "Mobius: Unable to open MIDI output: %s\n", output);

		const char* thru = mConfig->getMidiThrough();
		if (mContext->isPlugin())
		  thru = mConfig->getPluginMidiThrough();

		success = mMidi->setThrough(thru);
		if (!success)
		  Trace(1, "Mobius: Unable to open MIDI through: %s\n", thru);

		// audio devices
		mRecorder->setSuggestedLatencyMsec(mConfig->getSuggestedLatencyMsec());
		mRecorder->setInputDevice(mConfig->getAudioInput());
		mRecorder->setOutputDevice(mConfig->getAudioOutput());

        // safe to do always?
        AudioSampleRate rate = mConfig->getSampleRate();
        if (rate == SAMPLE_RATE_48000)
          mRecorder->setSampleRate(48000);
        else
          mRecorder->setSampleRate(44100);
                                   
		// reset this so we trace new latency values
		mInterrupts = 0;
		mRecorder->start();
	}

	// rebuild the handler jump table
    // only have to do this when the MobiusConfiguration changes
    // so the ResolvedTargets point to the new Bindables
    if (doBindings)
      updateBindings();

    // OSC gets network parameters from the global config
    if (mOsc != NULL)
      mOsc->updateGlobalConfiguration(this);

	// If we were editing the Setups, then it is expected that we
	// change the selected track if nothing else is going on
    // !! seems like there should be more here, for every track in reset
    // the setup changes should be immediately propagated?
    if (!config->isNoSetupChanges()) {
        bool allReset = true;
        for (int i = 0 ; i < mTrackCount ; i++) {
            Track* t = mTracks[i];
            if (!t->getLoop()->isReset()) {
                allReset = false;
                break;
            }
        }
        if (allReset) {
            int initialTrack = 0;
            Setup* setup = mConfig->getCurrentSetup();
            if (setup != NULL)
              initialTrack = setup->getActiveTrack();
            setTrack(initialTrack);
        }
    }
}

/**
 * Called by installConfiguration whenever the configuration changes.
 * Originally we tried to follow the track count from the configuration
 * at runtime.  Unfortunately this has race conditions with the 
 * interrupt thread that may be using those tracks at the same time.
 * 
 * We could probably work through those but it's safest just to require
 * a restart after changing the track count.  Until the restart we'll
 * continue using the original track count.
 * as they may have changed.
 */
PRIVATE void Mobius::buildTracks(int count)
{
	int i;

    if (mTracks != NULL) {
        // Another way to make this safer is to just preallocate mTracks at
        // the maximum size and don't ever reallocate it, then just
        // change mTrackCount?
        if (mTrackCount != count) {
            Trace(2, "Mobius: Ignoring track count change to %ld until after restart\n",
                  (long)count);
        }
    }
    else {
        // must have at least one, should have fixed this by now
        if (count <= 0) count = 1;

        Track** tracks = new Track*[count];

        for (i = 0 ; i < count ; i++) {
            Track* t = new Track(this, mSynchronizer, i);
            tracks[i] = t;
            mRecorder->add(t);
        }
	
        mTracks = tracks;
        mTrackCount = count;
        mTrack = tracks[0];
    }
}

/**
 * Rebuild the binding cache to reflect changes made to the binding definitions,
 * the scripts, or one of the bindable config objects
 * (presets, setups, overlays).
 * 
 * Have to be careful since the MIDI thread can be using the current
 * binding cache, so build and set the new one before deleting the old one.
 *
 * !! This is messy.  Need a more encapsulated environment for ui level threads
 * that gets phased in consistently instead of several pieces.
 */
PRIVATE void Mobius::updateBindings()
{
    BindingResolver* old = mBindingResolver;
    mBindingResolver = new BindingResolver(this);

    // pause to make sure the new one is being used
    // would be better if we assigned it as a pending change and
    // processed it on the next MIDI interrupt
    SleepMillis(100);
        
    delete old;

    // This could be in use by MobiusThread so have to phase
    // it out and let MobiusThread reclaim it.
    MidiExporter* exporter = new MidiExporter(this);
    exporter->setHistory(mMidiExporter);
    mMidiExporter = exporter;

    // refresh the previously resolved targets
    for (ResolvedTarget* t = mResolvedTargets ; t != NULL ; t = t->getNext()) {
        Target* target = t->getTarget();

        // The new target may no longer exist in which case the binding
        // goes to null.  Trigger processing needs to deal with this.

        if (target == TargetFunction) {
            // !! is this safe?  shouldn't be be getting a new 
            // RunScriptFunction wrapper too?
            Function* f = (Function*)t->getObject();
            if (f != NULL && f->isScript()) {
                Script* script = (Script*)f->object;
                f->object = mScriptEnv->getScript(script);
            }
        }
        else if (target == TargetSetup) {
            t->setObject(mConfig->getSetup(t->getName()));
        }
        else if (target == TargetPreset) {
            t->setObject(mConfig->getPreset(t->getName()));
        }
        else if (target == TargetBindings) {
            t->setObject(mConfig->getBindingConfig(t->getName()));
        }
    }

}

/****************************************************************************
 *                                                                          *
 *                            SCRIPT CONFIGURATION                          *
 *                                                                          *
 ****************************************************************************/

/**
 * Return a list of Actions for each Script that used the !button declaration.
 * This is a kludge to get buttons for scripts automatically added to the UI
 * so we don't have to do it manually. I ALWAYS want this so I win.  The UI
 * is expected to call this at appropriate times, like initialization and
 * whenever the script config changes.  The actions become owned by the caller
 * and must be returned to the pool.
 */
PUBLIC Action* Mobius::getScriptButtonActions()
{
    Action* actions = NULL;
    Action* last = NULL;

    for (Script* script = mScriptEnv->getScripts() ; script != NULL ;
         script = script->getNext()) {

        if (script->isButton()) {
            Function* f = script->getFunction();
            if (f != NULL) {

                // resolution is still messy, need more ways
                // to get a ResolvedTarget
                Binding* b = new Binding();
                b->setTrigger(TriggerUI);
                b->setTarget(TargetFunction);
                b->setName(f->getName());
                
                ResolvedTarget* t = resolveTarget(b);
                if (t != NULL) {
                    Action* action = new Action(t);

                    resolveTrigger(b, action);

                    if (last != NULL)
                      last->setNext(action);
                    else
                      actions = action;
                    last = action;
                }
                delete b;
            }
        }
    }

    return actions;
}

/**
 * Force a reload of all scripts, useful for debugging when
 * you forgot !autoload.
 */
PUBLIC void Mobius::reloadScripts()
{
    installScripts(mConfig->getScriptConfig(), true);
}

/**
 * Load the scripts and initialize the global function tables.
 * This is called every time the configuration changes so try to be smart
 * about detecting differences to avoid excessive script loading.
 * 
 * We have a host of dependency issues on the old environment so deleting
 * the old one is very difficult to do reliably.  In particular, 
 * the current BindingResolver will be referencing Script objects and
 * the plugins may have some MobiusPluginParameter proxies that reference
 * Scripts too.  And of course there may also be active script threads.
 * 
 * Until we can refine the interfaces to the point where we have a single
 * place to reliably phase in parts of the config and update the 
 * dependencies, we'll have to maintain these on a history list.
 */
PRIVATE bool Mobius::installScripts(ScriptConfig* config, bool force)
{
    bool changed = false;

    if (mScriptEnv == NULL || force || mScriptEnv->isDifference(config)) {
        changed = true;

        if (mScriptEnv == NULL)
          Trace(2, "Mobius: Loading scripts and function tables\n");
        else
          Trace(2, "Mobius: Reloading scripts and function tables\n");

        ScriptCompiler* sc = new ScriptCompiler();
        ScriptEnv* env = sc->compile(this, config);
        delete sc;

        // add it to the history, should use a csect but script configs
        // can't come in that fast
        env->setNext(mScriptEnv);
        mScriptEnv = env;

        // rebuild the global Function table
        // in theory we could have an outstanding reference to mFunctions
        // at the moment, but that would only be for the UI dialogs and script
        // compilation which in practice we won't be doing right now
        // ugh, please don't make me have another history list...
        initFunctions();

        // rebuild the global parameter table
        initScriptParameters();

        // I wanted to install script buttons here but we need to
        // call back to the UI.  Instead have the UI call 
        // getScriptButtonActions when appropriate

        // have to update ResolvedTargets to point to the new scripts
        // if we're not forcing, then let the caller do it
        if (force)
          updateBindings();
    }

    return changed;
}

/**
 * Initialize script parameters after installing a ScriptEnv.
 */
PRIVATE void Mobius::initScriptParameters()
{
    if (mScriptEnv != NULL) {
        for (Script* script = mScriptEnv->getScripts() ; script != NULL ; 
             script = script->getNext()) {

            if (script->isParameter()) {

                ScriptBlock* b = script->getBlock();
                if (b != NULL) {
                    for (ScriptStatement* st = b->getStatements() ; st != NULL ; 
                         st = st->getNext()) {

                        if (st->isParam()) {
                            // where should this logic go?
                            addScriptParameter((ScriptParamStatement*)st);
                        }
                    }
                }
            }
        }
    }
}

/**
 * Promote one script parameter we found in a script.
 * Currently this is only being done with the entire ScriptConfig
 * is reloaded.  Due to the awkward cross reference between
 * Parameter and ScriptParamStatement !autoload is disabled for 
 * any file that contains a Param.  Need to work this out...
 */
PRIVATE void Mobius::addScriptParameter(ScriptParamStatement* s)
{
    const char* name = s->getName();

    if (name != NULL) {
        printf("Promoting user defined parameter: %s\n", s->getName());

        ScriptBlock* block = s->getChildBlock();
        if (block != NULL) {
            ScriptDeclaration* decls = block->getDeclarations();
            for (ScriptDeclaration* d = decls ; d != NULL ; d = d->getNext()) {
                printf("   %s %s\n", d->getName(), d->getArgs());
            }
        }
    }
    else {
        printf("Ignoring Param statement without name\n");
    }

    fflush(stdout);
}


/****************************************************************************
 *                                                                          *
 *                              OBJECT CONSTANTS                            *
 *                                                                          *
 ****************************************************************************/

PUBLIC AudioPool* Mobius::getAudioPool()
{
    return mAudioPool;
}

PUBLIC LayerPool* Mobius::getLayerPool()
{
    return mLayerPool;
}

PUBLIC EventPool* Mobius::getEventPool()
{
    return mEventPool;
}

/**
 * Return the list of all functions.
 * Should only be used by the binding UI.
 */
PUBLIC Function** Mobius::getFunctions()
{
    return mFunctions;
}

PUBLIC Parameter** Mobius::getParameters()
{
    return Parameters;
}

PUBLIC Parameter* Mobius::getParameter(const char* name)
{
    return Parameter::getParameter(name);
}

PUBLIC Parameter* Mobius::getParameterWithDisplayName(const char* name)
{
    return Parameter::getParameterWithDisplayName(name);
}

PUBLIC MobiusMode** Mobius::getModes()
{
    return Modes;
}

PUBLIC MobiusMode* Mobius::getMode(const char* name)
{
    return MobiusMode::getMode(name);
}

/****************************************************************************
 *                                                                          *
 *                                  FUNTIONS                                *
 *                                                                          *
 ****************************************************************************/

/**
 * Search the dynamic function list.
 */
PUBLIC Function* Mobius::getFunction(const char * name)
{
    Function* found = Function::getFunction(mFunctions, name);
    
    // one last try with hidden functions
    // can't we just have a hidden flag for these rather than
    // two arrays?
    if (found == NULL)
      found = Function::getFunction(HiddenFunctions, name);

    return found;
}

/**
 * Build out the function list by combining the static function definitions
 * with the scripts.  Called during initialization and whenever the
 * script config changes.
 *
 * NOTE: In theory we could be doing a UI dialog, or compiling a script or
 * something else that is searching the global Functions list at this
 * exact moment but in practice it won't happen and I don't want to mess
 * with another csect for this.
 *
 * This was formerly a static array but this caused problems when the
 * plugin was instantiated more than once because the Script objects
 * would be deleted when one Mobius plugin was shut down but they were
 * still referenced by the other plugin.   We've got similar issues
 * for any system constant that keeps localized names, but those can be
 * copied to private arrays.
 */
PRIVATE void Mobius::initFunctions()
{
    Function** functions = NULL;
	int i;

    // should already be initialized but make sure
    Function::initStaticFunctions();

	// first count the static functions
	// eventually make loop and track triggers dynamnic too
	int staticCount = 0;
	for ( ; StaticFunctions[staticCount] != NULL ; staticCount++);

	// add script triggers
	int scriptCount = 0;
	List* scripts = NULL;
	if (mScriptEnv != NULL) {
		scripts = mScriptEnv->getScriptFunctions();
		if (scripts != NULL)
		  scriptCount = scripts->size();
	}

    // allocate a new array
	functions = new Function*[staticCount + scriptCount + 1];

    // add statics
    int psn = 0;
    for (i = 0 ; i < staticCount ; i++)
	  functions[psn++] = StaticFunctions[i];

    // add scripts
    for (i = 0 ; i < scriptCount ; i++)
      functions[psn++] = (RunScriptFunction*)scripts->get(i);

    // and terminate it
    functions[psn] = NULL;

    // now splice in the new array
    Function** old = mFunctions;
    mFunctions = functions;

    // pause for a moment?
	delete old;

	updateGlobalFunctionPreferences();
}

/**
 * Check the global configuration for functions that are
 * designated as obeying focus lock and track groups.
 * Update the Function objects for later reference.
 */
PUBLIC void Mobius::updateGlobalFunctionPreferences()
{
	StringList* names = mConfig->getFocusLockFunctions();
	
	if (names == NULL) {
		// shouldn't happen, but if so return to the defaults
		for (int i = 0 ; mFunctions[i] != NULL ; i++) {
			Function* f = mFunctions[i];
			f->focusLockDisabled = false;
		}
	}
	else {
		for (int i = 0 ; mFunctions[i] != NULL ; i++) {
			Function* f = mFunctions[i];
			f->focusLockDisabled = false;
			// remember to only pay attention to functions that were
			// displayed for selection in the UI, in particular
			// RunScript must be allowed!
			if (!f->noFocusLock && f->eventType != RunScriptEvent)
			  f->focusLockDisabled = !(names->containsNoCase(f->getName()));
		}
	}

	// and also those selected for customized mute cancel
	names = mConfig->getMuteCancelFunctions();
	for (int i = 0 ; mFunctions[i] != NULL ; i++) {
		Function* f = mFunctions[i];
		if (f->mayCancelMute) {
			if (names == NULL)
			  f->cancelMute = false;
			else
			  f->cancelMute = names->containsNoCase(f->getName());
		}
	}

	// and also those selected for switch confirmation
	names = mConfig->getConfirmationFunctions();
	for (int i = 0 ; mFunctions[i] != NULL ; i++) {
		Function* f = mFunctions[i];
		if (f->mayConfirm) {
			if (names == NULL)
			  f->confirms = false;
			else
			  f->confirms = names->containsNoCase(f->getName());
		}
	}
}

/****************************************************************************
 *                                                                          *
 *   							  SAVE/LOAD                                 *
 *                                                                          *
 ****************************************************************************/

/**
 * Load a new project, this must be processed in the interrupt handler
 * to avoid contention.  See loadProjectInternal below.
 */
PUBLIC void Mobius::loadProject(Project* p)
{
	// not bothering with a csect since you really can't laod these that fast
	if (mPendingProject == NULL)
	  mPendingProject = p;
	else {
        // Need to send an alert back to the UI !!
		Trace(1, "Mobius: A project is already being loaded.\n");
		delete p;
	}
}

/**
 * Convenience method to load a project containing a single layer
 * into the active loop.
 */
PUBLIC void Mobius::loadLoop(Audio* a)
{
    if (mTrack != NULL) {
        Loop* loop = mTrack->getLoop();
        // sigh, Track number is zero based, Loop number is one based
        Project* p = new Project(a, mTrack->getRawNumber(), loop->getNumber() - 1);
        // this causes it to merge rather than reset
        p->setIncremental(true);

        loadProject(p);
    }
}

/**
 * Eventually called by the interrupt handler after we set mPendingProject.
 *
 * This must be done inside the interrupt handler.
 *
 * Layer references in segments are complicated because there is
 * no assurance that layer ids are in order or that layers appear
 * int the same loop or track.  Have to first traverse the project
 * instantiating Layer objects.  Then make another pass to instantiate
 * Segments with resolved Layer references.  Then a final pass to 
 * stitch them to the Track/Loop hierarchy.
 *
 * !! This looks like a LOT of work, can we pre-compile any of this before
 * we pass it into the interrupt handler?  setSetup() and 
 * setOverlayBindingConfig() come to mind.  But if we're in generalreset
 * I guess it doesn't matter if we miss a few interrupts.
 */
PRIVATE void Mobius::loadProjectInternal(Project* p)
{
	p->resolveLayers(mLayerPool);

	List* tracks = p->getTracks();

    if (tracks == NULL) {
        Trace(2, "Mobius::loadProjectInternal empty project\n");
    }
    else if (!p->isIncremental()) {
		// globalReset to start from a clean slate
		globalReset(NULL);

		const char* name = p->getSetup();
		if (name != NULL) {
            // remember to locate the Setup from the interrupt config
            Setup* s = mInterruptConfig->getSetup(name);
            if (s != NULL)
              setSetupInternal(s);
        }

		// Global reset again to get the tracks adjusted to the 
		// state in the Setup.
		globalReset(NULL);

        // change the selected binding overlay
        // this is an unusual case where we're in an interrupt but we
        // must set the master MobiusConfig object to change the
        // binding overlay since that is not used inside the interrupt
        // !! this will override what was in the Setup which I guess
        // is okay if you changed it before saving the project, but most
        // of the time this will already have been set during setSetupInternal
		name = p->getBindings();
		if (name != NULL) {
			BindingConfig* bindings = mConfig->getBindingConfig(name);
			if (bindings != NULL)
			  setOverlayBindings(bindings);
		}
        
        // should we let the project determine the track count
        // or force the project to fit the configured tracks?g
		for (int i = 0 ; i < mTrackCount ; i++) {
			if (i < tracks->size()) {
				ProjectTrack* pt = (ProjectTrack*)tracks->get(i);
				mTracks[i]->loadProject(pt);
				if (pt->isActive())
				  setTrack(i);
			}
		}

        // may now have master tracks
        mSynchronizer->loadProject(p);
	}
	else {
        // Replace only the loops in the project identified by number.
        // Currently used only when loading individual loops.  Could beef
        // this up so we can set more of the track.

		for (int i = 0 ; i < tracks->size() ; i++) {
			ProjectTrack* pt = (ProjectTrack*)tracks->get(i);
            int tnum = pt->getNumber();
            if (tnum < 0 || tnum >= mTrackCount)
              Trace(1, "Incremental project load: track %ld is out of range\n",
                    (long)tnum);
            else {
                Track* track = mTracks[tnum];

                List* loops = pt->getLoops();
                if (loops == NULL) 
                  Trace(2, "Mobius::loadProjectInternal empty track\n");
                else {
                    for (int j = 0 ; j < loops->size() ; j++) {
                        ProjectLoop* pl = (ProjectLoop*)loops->get(j);
                        int lnum = pl->getNumber();
                        // don't allow extending LoopCount
                        if (lnum < 0 || lnum >= track->getLoopCount())
                          Trace(1, "Incremental project load: loop %ld is out of range\n",
                                (long)lnum);
                        else {
                            Loop* loop = track->getLoop(lnum);
                            if (pl->isActive())
                              track->setLoop(loop);
                            else {
                                // this is important for Loop::loadProject
                                // to start it in Pause mode
                                if (loop == track->getLoop())
                                  pl->setActive(true);
                            }

                            loop->reset(NULL);
                            loop->loadProject(pl);

                            // Kludge: Synchronizer wants to be notified when
                            // we load individual loops, but we're using
                            // incremental projects to do that. Rather than
                            // calling loadProject() call loadLoop() for
                            // each track.
                            // !! Revisit this, it would be nice to handle
                            // these the same way
                            if (loop == track->getLoop())
                                mSynchronizer->loadLoop(loop);
                        }
                    }
                }
            }
		}
	}

	delete p;
}

/**
 * Capture the state of the Mobius in a Project.
 * Tried to do this in the interupt handler, but if we have to flatten
 * layers it's too time consuming.  In theory we could have contention
 * with functions being applied while the save is in progress, but 
 * that would be rare.  
 * !! At least ensure that we won't crash.
 * 
 * Note that we're getting copies of Audio objects that are still
 * technically owned by the Layers.  As long as you save the project
 * before any radical change, like a Reset, it will be ok.  But if
 * you Reset or TrackReset and start recording a new loop before
 * the Project is saved, the Audio's that end up being saved may
 * not be what you started with.  
 *
 * The most important thing is that they remain valid heap objects, which
 * will be true since we always pool Layer objects.  So, while you
 * may get the wrong audio, you at least won't crash.  
 *
 * Providing an absolutely accurate snapshot requires that we make a copy
 * of all the Audio objects when building the Project, this may be
 * a very expensive operation which would cause us to miss interrupts.
 * 
 * So, we compromise and give you pointers to "live" objects that will
 * usuallly remain valid until the project is saved.  The only time
 * the objects would be modified is if you were running a script that
 * didn't wait for the save, or if you were using MIDI control at the
 * same time you were saving the project.  Both are unlikely and avoidable.
 */
PUBLIC Project* Mobius::saveProject()
{
    Project* p = new Project();

    BindingConfig* overlay = mConfig->getOverlayBindingConfig();
    if (overlay != NULL)
      p->setBindings(overlay->getName());

	Setup* s = mConfig->getCurrentSetup();
	if (s != NULL)
	  p->setSetup(s->getName());

	p->setTracks(this);
	p->setFinished(true);

    return p;
}

/****************************************************************************
 *                                                                          *
 *                                   STATE                                  *
 *                                                                          *
 ****************************************************************************/

PUBLIC MobiusState* Mobius::getState(int track)
{
	MobiusState* s = &mState;

	// don't like returning structures, can we return just the name?
    // it doesn't look like anyone uses this 
	mState.bindings = mConfig->getOverlayBindingConfig();

	// why not just keep it here?
	strcpy(mState.customMode, mCustomMode);

	mState.globalRecording = mCapturing;

    if (track >= 0 && track < mTrackCount)
	  mState.track = mTracks[track]->getState();
	else {
		// else, fake something up so the UI doesn't get a NULL pointer?
		mState.track = NULL;
	}

	return &mState;
}

PUBLIC int Mobius::getReportedInputLatency()
{
	int latency = 0;
	if (mRecorder != NULL) {
		AudioStream* stream = mRecorder->getStream();
		latency = stream->getInputLatencyFrames();
	}
	return latency;
}

/**
 * Return the effective input latency.
 * The configuration may override what the audio device reports
 * in order to fine tune actual latency.
 */
PUBLIC int Mobius::getEffectiveInputLatency()
{
	int latency = mConfig->getInputLatency();
	if (latency == 0)
	  latency = getReportedInputLatency();
	return latency;
}

PUBLIC int Mobius::getReportedOutputLatency()
{
	int latency = 0;
	if (mRecorder != NULL) {
		AudioStream* stream = mRecorder->getStream();
		latency = stream->getOutputLatencyFrames();
	}
	return latency;
}

PUBLIC int Mobius::getEffectiveOutputLatency()
{
	int latency = mConfig->getOutputLatency();
	if (latency == 0)
	  latency = getReportedOutputLatency();
	return latency;
}

PUBLIC long Mobius::getFrame()
{
	return mTrack->getFrame();
}

PUBLIC MobiusMode* Mobius::getMode()
{
	return mTrack->getMode();
}

PUBLIC void Mobius::logStatus()
{
    // !!!!!!!!!!!!!!!!!!!!!!!!
    // we are leaking audio buffers and all kinds of shit
    // if this is a plugin, figure out how we reference count
    // static caches

    printf("*** Mobius engine status:\n");

	if (mRecorder != NULL) {
		AudioStream* s = mRecorder->getStream();
		s->printStatistics();
	}

    mActionPool->dump();
    mEventPool->dump();
    mLayerPool->dump();
    mAudioPool->dump();

    // this has never been used and looks confusing
    //dumpObjectPools();

    TraceBuffer* b = new TraceBuffer();
	for (int i = 0 ; i < mTrackCount ; i++) {
		Track* t = mTracks[i];
		t->dump(b);
	}
    b->print();
    delete b;

    fflush(stdout);
}

/**
 * Intended for use in scripts to override the usual mode display
 * if the script enters some arbitrary user-defined mode.
 * !! should this be persisted?
 */
PUBLIC void Mobius::setCustomMode(const char* s)
{
	strcpy(mCustomMode, "");
	if (s != NULL) {
		int len = strlen(s);
		if (len < MAX_CUSTOM_MODE - 1) 
		  strcpy(mCustomMode, s);
	}
}

PUBLIC const char* Mobius::getCustomMode()
{
	const char* mode = NULL;
	if (mCustomMode[0] != 0)
	  mode = mCustomMode;
	return mode;
}

/**
 * Called by the MobiusListener after it finishes processing a Prompt.
 */
PUBLIC void Mobius::finishPrompt(Prompt* p)
{
	if (mThread != NULL) 
	  mThread->finishPrompt(p);
	else
	  delete p;
}

PUBLIC ControlSurface* Mobius::getControlSurfaces()
{
    return mControlSurfaces;
}

/****************************************************************************
 *                                                                          *
 *                             ACTION RESOLUTION                            *
 *                                                                          *
 ****************************************************************************/

/**
 * Resolve a Binding to an Action.
 * First we intern a ResolvedTarget, then we build the Action around it.
 * This handles both normal bindings and OSC bindings.
 */
PUBLIC Action* Mobius::resolveAction(Binding* b)
{
    Action* a = NULL;

    // we make assumptions about the trigger so it must be set
    if (b->getTrigger() == NULL) {
        Trace(1, "Mobius::resolveAction binding with no trigger\n");
    }
    else if (b->getTargetPath() != NULL) {
        // an OSC binding
        a = resolveOscAction(b);
    }
    else {
        ResolvedTarget* t = resolveTarget(b);
        if (t != NULL) {
            a = new Action(t);

            // parse binding arguments
            CopyString(b->getArgs(), a->bindingArgs, sizeof(a->bindingArgs));
            a->parseBindingArgs();

            resolveTrigger(b, a);
        }
    }

    return a;
}

/**
 * Resolve the target represented in a Binding and return
 * an interned ResolvedTarget if we could resolve the target.  
 * The returned object remains owned by Mobius and must not be
 * modified by the caller.
 *
 * This will NOT handle Bindings that use targetPath.  For those
 * you must call resolveAction.  This is only public so that it
 * may be used by the binding windows to validate selections.
 */
PUBLIC ResolvedTarget* Mobius::resolveTarget(Binding* b)
{
    ResolvedTarget* resolved = NULL;

    if (b->getTargetPath()) {
        Trace(1, "resolveTarget called with targetPath!\n");
    }
    else {
        int track, group;
        parseBindingScope(b->getScope(), &track, &group);
        resolved = internTarget(b->getTarget(), b->getName(), track, group);
    }

    return resolved;
}

/**
 * Parse a scope into track an group numbers.
 * Tracks are expected to be identified with integers starting
 * from 1.  Groups are identified with upper case letters A-Z.
 */
PRIVATE void Mobius::parseBindingScope(const char* scope, int* track, int* group)
{
    *track = 0;
    *group = 0;

    if (scope != NULL) {
        int len = strlen(scope);
        if (len > 1) {
            // must be a number 
            *track = atoi(scope);
        }
        else if (len == 1) {
            char ch = scope[0];
            if (ch >= 'A') {
                *group = (ch - 'A') + 1;
            }
            else {
                // normally an integer, anything else
                // collapses to zero
                *track = atoi(scope);
            }
        }
    }
}

/**
 * Resolve and intern a target using given it's properties.
 * Returns NULL if the target name is invalid, or if this is a UIControl
 * and we don't know what they are yet.
 *
 * Config object handling is messy.  We resolve to the 
 * external config not the interrupt config.  But when we
 * need to use this action we have to convert that to the
 * interrupt config object, so resolving it here doesn't accomplish
 * anything other than to make sure the name is valid.  Should just
 * leave the number in the Action instead?
 * !!
 */
PRIVATE ResolvedTarget* Mobius::internTarget(Target* target, 
                                             const char* name,
                                             int track,
                                             int group)
{
    ResolvedTarget* resolved = NULL;
    MobiusConfig* config = getConfiguration();
    void* resolvedTarget = NULL;
    bool tolerate = false;

    if (target == NULL) {
        Trace(1, "Unable to resolve Binding: no target\n");
    }
    else if (name == NULL) {
        Trace(1, "Unable to resolve Binding: no name\n");
    }
    else if (target == TargetFunction) {
        Function* f = getFunction(name);
        // these can have aliases, upgrade the name
        if (f != NULL)
          name = f->getName();
        resolvedTarget = f;
    }
    else if (target == TargetParameter) {
        Parameter* p = Parameter::getParameter(name);
        // these can have aliases, upgrade the name
        if (p != NULL)
          name = p->getName();
        resolvedTarget = p;
    }
    else if (target == TargetSetup) {
        resolvedTarget = config->getSetup(name);
    }
    else if (target == TargetPreset) {
        resolvedTarget = config->getPreset(name);
    }
    else if (target == TargetBindings) {
        resolvedTarget = config->getBindingConfig(name);
    }
    else if (target == TargetUIControl) {
        resolvedTarget = getUIControl(name);
        // tolerate this at first
        tolerate = true;
    }
    else if (target == TargetUIConfig) {
        // where??
        Trace(1, "Unable to resolve Binding: UIConfig\n");
    }
    else {
        Trace(1, "Unable to resolve Binding: unsupported target %ld\n",
              (long)target);
    }

    // must have at least the name, some we'll defer
    if (name != NULL) {
        if (resolvedTarget == NULL && !tolerate)
          Trace(1, "Unrecognized binding target: %s\n", name);
        else {
            // see if we already have one
            for (ResolvedTarget* t = mResolvedTargets ; t != NULL ; t = t->getNext()) {
                if (t->getTarget() == target &&
                    StringEqual(t->getName(), name) &&
                    t->getTrack() == track &&
                    t->getGroup() == group) {
                    resolved = t;
                    break;
                }
            }
            
            if (resolved == NULL) {
                resolved = new ResolvedTarget();
                resolved->setTarget(target);
                resolved->setName(name);
                resolved->setObject(resolvedTarget);
                resolved->setTrack(track);
                resolved->setGroup(group);

                mCsect->enter("internTarget");
                resolved->setInterned(true);
                resolved->setNext(mResolvedTargets);
                mResolvedTargets = resolved;
                mCsect->leave("internTarget");
            }
        }
    }
            
    return resolved;
}

/**
 * Resolve an Action from an OSC path.
 * 
 * /mobius/trigger/scope/target/value
 *
 * /mobius must be at the front of the path.
 *
 * Second container is optional and contains
 * information about the trigger:
 *
 *     range(low,high)
 *       - specifies the value range if not 0.0 to 1.0
 *
 *     noup
 *       - implies TriggerModeOnce
 *
 * The scope container is optional and may contain:
 *
 *    global, track number, group letter
 *
 * If not specified it defaults to global.
 *
 * After scope is the target name which will either
 * be a Parameter or a Function.
 *
 * After target name is an optional value which may take
 * these forms.  All but two are used only with parameter targets.
 *
 *   <enumeration>      
 *   <name>             + substituted for space
 *   <number>           parameter or function
 *   min                
 *   max                
 *   center             
 *   up             
 *   down
 *   up/<number>
 *   down/<number>
 *   arg                value taken from the OSC argument
 *
 */
PRIVATE Action* Mobius::resolveOscAction(Binding* b)
{
    Action* action = NULL;
    bool error = false;
    char token[128];
    char name[128];
    Target* target = NULL;
    Parameter* parameter = NULL;
    int track = 0;
    int group = 0;
    ActionOperator* op = NULL;
    ExValue argument;
    bool noup = false;
    bool passArg = false;

    name[0] = 0;
    argument.setNull();

    const char* path = b->getTargetPath();
    const char* ptr = path;

    // osc.xml often has example Bindings with no path so ignore them
    if (path == NULL || strlen(path) == 0) {
        Trace(3, "resolveOscAction: Empty path\n");
        error = true;
    }

    // I don't normally like the !error pattern but nesting gets
    // too deep without it and I can't stand inline returns
    if (!error) {
        // skip over /mobius
        ptr = getToken(ptr, token);
        if (!StringEqualNoCase(token, "mobius")) {
            Trace(2, "resolveOscAction: /mobius prefix not found\n");
            error = true;
        }
        else
          ptr = getToken(ptr, token);
    }
    
    //
    // Trigger
    //

    if (!error) {
        if (StringEqualNoCase(token, "noup")) {
            noup = true;
            ptr = getToken(ptr, token);
        }
        else if (StartsWithNoCase(token, "range")) {
            Trace(1, "Not supporting OSC trigger ranges yet\n");
            ptr = getToken(ptr, token);
        }
    }

    //
    // Scope
    //

    if (!error) {
        bool skip = true;
        if (isdigit(token[0])) {
            // must be a track number starting at 1
            int i = atoi(token);
            if (i >= 1 && i <= mTrackCount)
              track = i;
            else {
                Trace(2, "resolveOscAction: Invalid track number %s\n", token);
                error = true;
            }
        }
        else if (token[1] == 0) {
            // single letter, must be group number
            int i = (int)(token[0]) - (int)'A';
            if (i < 0 || i >= 26) {
                // not a letter
                Trace(2, "resolveOscAction: Invalid group letter %s\n", token);
                error = true;
            }
            else {
                // group in the binding starts from 1
                // TODO: check config for max group
                group = i + 1;
            }
        }
        else if (!StringEqualNoCase(token, "global")) {
            // global is optional, consider this the target
            skip = false;
        }
        if (skip)
          ptr = getToken(ptr, token);
    }

    //
    // Target
    //
    // Orignally we had a type here, but now we're assuming
    // that all targets have unique names.  This does mean that
    // we'll search the name lists twice, once here and again
    // in internTarget.  Oh well.
    // !! what about scripts with user defined names, 
    // use the "script:" prefix?
    //

    // do parameters first so we get SpeedStep and PitchStep
    // as parameters rather than spread functions
    if (!error) {
        // remember this for later
        parameter = Parameter::getParameter(token);
        if (parameter != NULL) {
            target = TargetParameter;
            CopyString(token, name, sizeof(name));
        }
    }

    if (!error && target == NULL) {
        // TODO: include UIControls?
    }

    if (!error && target == NULL) {
        // script names may have escaped spaces!
        char namebuf[128];
        oscUnescape(token, namebuf, sizeof(namebuf));

        Function* f = getFunction(namebuf);
        if (f != NULL) {
            target = TargetFunction;
            // save the unescaped name
            CopyString(namebuf, name, sizeof(name));
        }
    }

    if (!error) {
        if (target != NULL)
          ptr = getToken(ptr, token);
        else {
            Trace(2, "resolveOscAction: Unknown target %s\n", token);
            error = true;
        }
    }

    //
    // Special values
    //

    if (!error) {
        // up, down, min, max, center, arg
        op = ActionOperator::get(token);
        if (op == NULL && StringEqualNoCase(token, "arg"))
          passArg = true;

        if (op != NULL || passArg)
          ptr = getToken(ptr, token);
    }

    if (!error && op != NULL) {
        // operator can also use "arg" for it's operand
        if (StringEqualNoCase(token, "arg")) {
            passArg = true;
            ptr = getToken(ptr, token);
        }
    }

    //
    // Value
    // enumeration name, user defined name, number
    // if passArg became true there shouldn't be anything left
    //

    if (!error) {
        if (IsInteger(token)) {
            // Leave the value as an int. 
            // For config objects in theory you can name something "123" 
            // which needs to be searched as a string, but we're not 
            // allowing that.
            argument.setInt(ToInt(token));

            // TODO: Could validate parameter ranges...
        }
        else {
            char valbuf[128];
            oscUnescape(token, valbuf, sizeof(valbuf));
            if (strlen(valbuf) > 0) {
                // For config objects, resolve directly to the object
                // internTarget will log errors
                MobiusConfig* config = getConfiguration();
                if (parameter == BindingsParameter) {
                    target = TargetBindings;
                    CopyString(valbuf, name, sizeof(name));
                }
                else if (parameter == SetupNameParameter) {
                    target = TargetSetup;
                    CopyString(valbuf, name, sizeof(name));
                }
                else if (parameter == TrackPresetParameter) {
                    target = TargetPreset;
                    CopyString(valbuf, name, sizeof(name));
                }
                else {
                    // just leave it as a string argument
                    argument.setString(valbuf);
                }
            }
        }
    }

    // finally!
    if (!error) {
        // this will trace errors
        ResolvedTarget* rt = internTarget(target, name, track, group);
        if (rt != NULL) {
            // the id must be set by the caller
            action = new Action(rt);
            action->trigger = TriggerOsc;
            action->arg.set(&argument);
            action->actionOperator = op;
            action->passOscArg = passArg;

            // Binding contains a TriggerMode but we don't need it, 
            // though it might be important to convey Toggle.
            // Mode is implied by the target and options on the path and
            // you are expected to write paths that match the trigger mode.

            if (target == TargetParameter) {
                // parameters expect continuous triggers unless they
                // have an explicit value
                if (op == NULL && argument.isNull() && !passArg)
                  action->triggerMode = TriggerModeContinuous;
                else
                  action->triggerMode = TriggerModeOnce;
            }
            else if (target == TargetFunction) {
                if (noup || passArg)
                  action->triggerMode = TriggerModeOnce;
                else
                  action->triggerMode = TriggerModeMomentary;
            }
            else {
                // config objects were orignially params with values
                action->triggerMode = TriggerModeOnce;
            }

            // A binding can have args for min/max/set etc. 
            // We could support those if someone bothered to edit
            // OscConfig XML, but now that we can do it in paths
            // it isn't necessary and just confuses things.
        }
    }

    return action;
}

/**
 * Helper for target path parsing.
 */
PRIVATE const char* Mobius::getToken(const char* ptr, char* token)
{
    // skip over initial / if we're there
    if (*ptr == '/') ptr++;

    char* dest = token;
    while (*ptr && *ptr != '/')
      *dest++ = *ptr++;

    *dest = 0;

    return ptr;
}

/**
 * Unescape an OSC name which has + substituted for space.
 */
PRIVATE void Mobius::oscUnescape(const char* src, char* dest, int max)
{
    int len = strlen(src);
    if (len > (max - 1))
      Trace(1, "oscUnescape: Token too long %s\n", src);
    else {
        const char* srcptr = src;
        char* destptr = dest;
        while (*srcptr) {
            char ch = *srcptr;
            if (ch == '+') ch = ' ';
            *destptr++ = ch;
            srcptr++;
        }
        *destptr = 0;
    }
}

/**
 * After resolving a Binding for a non-OSC target, initialize
 * trigger properties.
 *
 * Arguably this could go in each of the "clients": OscConfig, 
 * MobiusPlugin, and BindingResolver, but we have all the OSC parsing
 * stuff down here, and that defines trigger properties so keep it
 * all in one place.
 *
 * The one exception is the UI since it is less predictable, it must
 * set the trigger properties after resolving the action.
 *
 * MIDI is ugly because the Action model was designed long after
 * the Binding model.  Bindings maintain specific fields for MIDI 
 * triggers, Actions are more generic so we have to convert them.
 * 
 * Bindings have so far used three target constants for MIDI notes, 
 * programs, and CCs.  Actions collapse this into a single TargetMidi.
 * Actions also do not have fields for values in a MIDI message but they
 * have accessors that can dig them out of the action id.
 */
PRIVATE void Mobius::resolveTrigger(Binding* binding, Action* action)
{
    int midiStatus = 0;

    // defaults usually convey
    Trigger* trigger = binding->getTrigger();
    TriggerMode* mode = binding->getTriggerMode();

    if (trigger == TriggerNote) {
        trigger = TriggerMidi;
        midiStatus = MS_NOTEON;
        if (mode == NULL) {
          mode = TriggerModeMomentary;
        }
        else if (mode != TriggerModeMomentary &&
                 mode != TriggerModeOnce) {
            Trace(1, "Overriding invalid note trigger mode %s\n",
                  mode->getName());
            mode = TriggerModeMomentary;
        }
    }
    else if (trigger == TriggerProgram) {
        trigger = TriggerMidi;
        midiStatus = MS_PROGRAM;
        mode = TriggerModeOnce;
    }
    else if (trigger == TriggerControl) {
        trigger = TriggerMidi;
        midiStatus = MS_CONTROL;
        // some controllers can be programmed to send zero/nono-zero
        // assume that if it is bound to anything other than a parameter
        // it is momentary
        Target* t = action->getTarget();
        if (t == TargetParameter) {
            if (mode == NULL)
              mode = TriggerModeContinuous;
            else if (mode != TriggerModeContinuous &&
                     mode != TriggerModeMomentary &&
                     mode != TriggerModeOnce) {
                Trace(1, "Overriding invalid control trigger mode %s\n",
                      mode->getName());
                mode = TriggerModeContinuous;
            }
        }
        else {
            if (mode != NULL && mode != TriggerModeMomentary)
              Trace(1, "Overriding invalid control trigger mode %s\n",
                    mode->getName());
            mode = TriggerModeMomentary;
        }
    }
    else if (trigger == TriggerPitch) {
        trigger = TriggerMidi;
        midiStatus = MS_BEND;
        // some controllers can be programmed to send zero/nono-zero
        mode = TriggerModeContinuous;
    }
    else if (trigger == TriggerKey) {
        mode = TriggerModeMomentary;
    }
    else if (trigger == TriggerUI) {
        // this can be either momentary or continuous
        // make UI set it appropriately
    }
    else if (trigger == TriggerHost) {
        // We don't need triggerType in the Binding do we?  Host
        // parameters always behave this way.
        Target* t = action->getTarget();
        if (t == TargetParameter &&
            action->actionOperator == NULL && 
            action->arg.isNull()) {
            mode = TriggerModeContinuous;
        }
        else {
            // Functions and config objects are assumed to behave
            // like buttons, can change this later for !continuous scripts
            mode = TriggerModeMomentary;
        }
    }
    else if (trigger == TriggerOsc) {
        // parsing the path will have already handled this
    }

    // If we've bound to a !continuous script, make it feel
    // like a Parameter.
    // NOTE: We'll never call this for TriggerOsc but in theory
    // it could work the same way.  Do we need that?
    if (trigger == TriggerHost || trigger == TriggerOsc) {
        if (action->getTarget() == TargetFunction) {
            Function* f = (Function*)action->getTargetObject();
            if (f != NULL && f->isScript()) {
                Script* s = (Script*)f->object;
                if (s != NULL && s->isContinuous())
                  mode = TriggerModeContinuous;
            }
        }
    }

    // save what we came up with
    action->trigger = trigger;
    action->triggerMode = mode;

    if (trigger != TriggerMidi) {
        action->id = binding->getValue();
    }
    else {
        // for MIDI triggers compress the MIDI message fields into
        // the action id which will already have the MS_ status code
        action->setMidiStatus(midiStatus);
        action->setMidiChannel(binding->getChannel());
        action->setMidiKey(binding->getValue());
    }
}

/****************************************************************************
 *                                                                          *
 *                                  EXPORTS                                 *
 *                                                                          *
 ****************************************************************************/

/**
 * Create an Export for a Binding.
 */
PUBLIC Export* Mobius::resolveExport(Binding* b)
{
    Export* exp = NULL;
    ResolvedTarget* target = resolveTarget(b);
    if (target != NULL)
      exp = resolveExport(target);
    return exp;
}

/**
 * Create an Export for the target of an Action.
 */
PUBLIC Export* Mobius::resolveExport(Action* a)
{
    return resolveExport(a->getResolvedTarget());
}

/**
 * Create an Export for a ResolvedTarget.
 * This is the core export resolver used by all the other
 * resolution interfaces.
 *
 * Returns NULL if the target can't be exported.  This is 
 * okay since OscRuntime calls this for everything.
 */
PUBLIC Export* Mobius::resolveExport(ResolvedTarget* resolved)
{
    Export* exp = NULL;
    bool exportable = false;

    Target* t = resolved->getTarget();

    if (t == TargetParameter) {
        // Since OSC is configured in text, ignore some things
        // we don't want to get out
        Parameter* p = (Parameter*)resolved->getObject();
        exportable = (p->bindable || p->control);
    }
    
    if (exportable) {
        exp = new Export(this);
        exp->setTarget(resolved);
        // nothing else to save, Export has logic to call
        // back to us for interesting things
    }

    return exp;
}

/**
 * Called periodically by MobiusThread to export status to bi-directional
 * MIDI controllers, control surfaces, and OSC clients.
 *
 * mobiusThread is true if we're being called by MobiusThread which means
 * it is safe to clean up a previous exporter that is being phased out.
 * NOTE: This is always true since we're never called outside the thread,
 * I don't remember why this was here.
 *
 */
PUBLIC void Mobius::exportStatus(bool inThread)
{
    // nab a copy to it doesn't change out from under us
    // maybe it would be better if MobiusThread managed it's own copy
    // and we just posted a new version 
    MidiExporter *exporter = mMidiExporter;
    if (exporter != NULL) {

        if (inThread) {
            // reclaim old versions
            MidiExporter* old = exporter->getHistory();
            if (old != NULL) {
                exporter->setHistory(NULL);
                delete old;
            }
        }

        exporter->sendEvents();
    }

	// don't have a mechanism for editing these yet so we don't
	// have to deal with the old/new thing like MidiExporter
	// this will change...
 
    for (ControlSurface* cs = mControlSurfaces ; cs != NULL ; 
         cs = cs->getNext()) {
        cs->refresh();
    }

    // the thread starts running before we're fully initialized so
    // always check for null here
    if (mOsc != NULL)
      mOsc->exportStatus();
}

/****************************************************************************
 *                                                                          *
 *                                WATCH POINTS                              *
 *                                                                          *
 ****************************************************************************/

/**
 * Register a watch point listener.
 * The listener object becomes owned by Mobius and must not be deleted
 * by the caller.  If the caller no longer wants the listener it
 * must call the remove() method on the listener.
 */
PUBLIC WatchPoint* Mobius::addWatcher(WatchPointListener* l)
{
    const char* name = l->getWatchPointName();
    WatchPoint* wp = WatchPoint::getWatchPoint(name);
    if (wp == NULL)
      Trace(1, "Invalid watch point name: %s\n", name);
    else {
        mCsect->enter("addWatchPoint");
        mNewWatchers->add(l);
        mCsect->leave();
    }
    return wp;
}

/**
 * Called inside the interrupt to transition in new watch point listeners.
 */
PRIVATE void Mobius::installWatchers()
{
    if (mNewWatchers->size() > 0) {
        mCsect->enter("installWatcher");
        // need to check the size again once we're in the csect
        int max = mNewWatchers->size();
        for (int i = 0 ; i < max ; i++) {
            WatchPointListener* l = (WatchPointListener*)mNewWatchers->get(i);
            // it won't have made it to the list if the name was bad
            const char* name = l->getWatchPointName();
            WatchPoint* wp = WatchPoint::getWatchPoint(name);
            if (wp != NULL) {
                List* list = wp->getListeners(mWatchers);
                if (list != NULL) {
                    Trace(2, "Adding watch point listener for %s\n",
                          l->getWatchPointName());
                    list->add(l);
                }
            }
        }
        mNewWatchers->reset();
        mCsect->leave();
    }
}

/**
 * Called internally to notify the watch point listeners.
 * This is IN THE INTERRUPT.
 */
PUBLIC void Mobius::notifyWatchers(WatchPoint* wp, int value)
{
    List* listeners = wp->getListeners(mWatchers);
    if (listeners != NULL) {
        int max = listeners->size();
        for (int i = 0 ; i < max ; i++) {
            WatchPointListener* l = (WatchPointListener*)listeners->get(i);
            // gc listeners marked removable
            if (!l->isRemoving())
              l->watchPointEvent(value);
            else {
                Trace(2, "Removing watch point listener for %s\n",
                      l->getWatchPointName());
                listeners->remove(i);
                max--;
            }
        }
    }
}

/****************************************************************************
 *                                                                          *
 *                                  ACTIONS                                 *
 *                                                                          *
 ****************************************************************************/

/**
 * Allocate an action. 
 * The caller is expected to fill this out and execute it with doAction.
 * If the caller doesn't want it they must call freeAction.
 * These are maintained in a pool that both the application threads
 * and the interrupt threads can access so have to use a Csect.
 */
PUBLIC Action* Mobius::newAction()
{
    Action* action = NULL;

    mCsect->enter("newAction");
    action = mActionPool->newAction();
    mCsect->leave("newAction");

    // always need this
    action->mobius = this;

    return action;
}

PUBLIC void Mobius::freeAction(Action* a)
{
    // you normally don't do this, just delete them
    if (a->isRegistered())
      Trace(1, "Freeing a registered action!\n");

    mCsect->enter("newAction");
    mActionPool->freeAction(a);
    mCsect->leave("newAction");
}

PUBLIC Action* Mobius::cloneAction(Action* src)
{
    Action* action = NULL;

    mCsect->enter("cloneAction");
    action = mActionPool->newAction(src);
    mCsect->leave("cloneAction");

    // not always set if allocated outside
    action->mobius = this;

    // make sure this is off
    action->setRegistered(false);

    return action;
}

/****************************************************************************
 *                                                                          *
 *                              ACTION EXECUTION                            *
 *                                                                          *
 ****************************************************************************/

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
PUBLIC void Mobius::doAction(Action* a)
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
            f->invoke(a, this);
        }
        else if (mInterrupts == 0) {
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
        completeAction(a);
    }

}

/**
 * Process the action list when we're inside the interrupt.
 */
PRIVATE void Mobius::doInterruptActions()
{
    Action* actions = NULL;
    Action* next = NULL;

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

        completeAction(action);
    }
}

/**
 * Called when the action has finished processing.
 * Notify the listener if there is one.
 */
PUBLIC void Mobius::completeAction(Action* a)
{
    // TODO: listener

    // if an event is still set we're owned by the event
    // threadEvents don't imply ownership
    if (!a->isRegistered() && a->getEvent() == NULL)
      freeAction(a);
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
PUBLIC void Mobius::doActionNow(Action* a)
{
    Target* t = a->getTarget();

    // not always set if comming from the outside
    a->mobius = this;

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
        doScriptNotification(a);
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
PRIVATE void Mobius::doPreset(Action* a)
{
    Preset* p = (Preset*)a->getTargetObject();
    if (p == NULL) {    
        // may be a dynamic action
        // support string args here too?
        int number = a->arg.getInt();
        if (number < 0)
          Trace(1, "Missing action Preset\n");
        else {
            p = mConfig->getPreset(number);
            if (p == NULL) 
              Trace(1, "Invalid preset number: %ld\n", (long)number);
        }
    }

    if (p != NULL) {
        int number = p->getNumber();

        Trace(2, "Preset action: %ld\n", (long)number);

        // determine the target track(s) and schedule events
        Track* track = resolveTrack(a);

        if (track != NULL) {
            track->setPreset(number);
        }
        else if (a->noGroup) {
            // selected track only  
            mTrack->setPreset(number);
        }
        else {
            // Apply to the current track, all focused tracks
            // and all tracks in the Action scope.
            int targetGroup = a->getTargetGroup();

            // might want a global param for this?
            bool allowPresetFocus = true;

            if (targetGroup > 0) {
                // only tracks in this group
                for (int i = 0 ; i < mTrackCount ; i++) {
                    Track* t = mTracks[i];
                    if (targetGroup == t->getGroup())
                      t->setPreset(number);
                }
            }
            else if (allowPresetFocus) {
                for (int i = 0 ; i < mTrackCount ; i++) {
                    Track* t = mTracks[i];
                    if (isFocused(t))
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
PRIVATE void Mobius::doSetup(Action* a)
{
    // If we're here from a Binding should have resolved
    Setup* s = (Setup*)a->getTargetObject();
    if (s == NULL) {
        // may be a dynamic action
        int number = a->arg.getInt();
        if (number < 0)
          Trace(1, "Missing action Setup\n");
        else {
            s = mConfig->getSetup(number);
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
        mConfig->setCurrentSetup(number);
        setSetupInternal(number);

        // special operator just for setups to cause it to be saved
        if (a->actionOperator == OperatorPermanent) {
            // save it too, control flow is convoluted,
            // we could have done this when the Action
            // was recevied outside the interrupt
            ThreadEvent* te = new ThreadEvent(TE_SAVE_CONFIG);
            mThread->addEvent(te);
        }
    }
}

/**
 * Process a TargetBindings action.
 * We can be outside the interrupt here.  All this does is
 * set the current overlay binding in mConfig which, we don't have
 * to phase it in, it will just be used on the next trigger.
 */
PRIVATE void Mobius::doBindings(Action* a)
{
    // If we're here from a Binding should have resolved
    BindingConfig* bc = (BindingConfig*)a->getTargetObject();
    if (bc == NULL) {
        // may be a dynamic action
        int number = a->arg.getInt();
        if (number < 0)
          Trace(1, "Missing action BindingConfig\n");
        else {
            bc = mConfig->getBindingConfig(number);
            if (bc == NULL) 
              Trace(1, "Invalid binding overlay number: %ld\n", (long)number);
        }
    }

    if (bc != NULL) {
        int number = bc->getNumber();
        Trace(2, "Bindings action: %ld\n", (long)number);
        mConfig->setOverlayBindingConfig(bc);

        // sigh, since getState doesn't export 

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
PRIVATE void Mobius::doScriptNotification(Action* a)
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
PRIVATE void Mobius::doFunction(Action* a)
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
          f->invoke(a, this);
        else {
            // Most global functions don't handle long presses but
            // TrackGroup does.  Since we'll get longpress actions regardless
            // have to be sure not to call the normal invoke() method
            // ?? what about scripts
            f->invokeLong(a, this);
        }
    }
    else {
        // determine the target track(s) and schedule events
        Track* track = resolveTrack(a);

        if (track != NULL) {
            doFunction(a, f, track);
        }
        else if (a->noGroup) {
            // selected track only
            doFunction(a, f, mTrack);
        }
        else {
            // Apply to tracks in a group or focused
            Action* ta = a;
            int nactions = 0;
            int targetGroup = a->getTargetGroup();

            for (int i = 0 ; i < mTrackCount ; i++) {
                Track* t = mTracks[i];

                if ((targetGroup > 0 && targetGroup == t->getGroup()) ||
                    (targetGroup <= 0 &&
                     (t == mTrack || (f->isFocusable() && isFocused(t))))) {

                    // if we have more than one, have to clone the
                    // action so it can have independent life
                    if (nactions > 0)
                      ta = cloneAction(a);

                    doFunction(ta, f, t);

                    // since we only "return" the first one free the 
                    // replicants
                    if (nactions > 0)
                      completeAction(ta);

                    nactions++;
                }
            }
        }
    }
}

/**
 * Determine the destination Track for an Action.
 * Return NULL if the action does not specify a destination track.
 * This can be called by a few function handlers that declare
 * themselves global but may want to target the current track.
 */
PUBLIC Track* Mobius::resolveTrack(Action* action)
{
    Track* track = NULL;

    if (action != NULL) {

        // This trumps all, it should only be set after the
        // action has been partially processed and replicated
        // for focus lock or groups.
        track = action->getResolvedTrack();
        
        if (track == NULL) {

            // note that the track number in an action is 1 based
            // zero means "current"
            int tnum = action->getTargetTrack();
            if (tnum > 0) {
                track = getTrack(tnum - 1);
                if (track == NULL) {
                    Trace(1, "Track index out of range");
                    // could either return NULL or force it to the lowest
                    // or higest
                    track = mTrack;
                }
            }

            // Force a track change if this function says it must run in the 
            // active track.  This will usually be the same, but when calling
            // some of the track management functions from scripts, they may
            // be different.
            Function* f = action->getFunction();
            if (f != NULL && f->activeTrack) {
                if (track != mTrack) {
                    if (track != NULL)
                      Trace(this, 2, "Mobius: Adjusting target track for activeTrack function %s\n", f->getName());
                    track = mTrack;
                }
            }
        }
    }

    return track;
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
PRIVATE void Mobius::doFunction(Action* action, Function* f, Track* t)
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
                f->invokeLong(action, t->getLoop());
            }
        }
        else {
            // normal down invocation
            f->invoke(action, t->getLoop());

            // notify the script interpreter on each new invoke
            // !! sort out whether we wait for invokes or events
            // !! Script could want the entire Action
            // TODO: some (most?) manual functions should cancel
            // a script in progress?
            resumeScript(t, f);
        }
    }
    else if (!action->isSustainable() || !f->isSustainable()) {
        // Up transition with a non-sustainable trigger or function, 
        // ignore the action.  Should have filtered these eariler?
        Trace(3, "Mobius::doFunction not a sustainable action\n");
    }
    else {
        // he's up!
        // let the function change how it ends
        if (action->longPress) {
            Function* alt = f->getLongPressFunction(action);
            if (alt != NULL && alt != f) {
                Trace(2, "Mobius::doFunction Long-press %s converts to %s\n",
                      f->getDisplayName(),
                      alt->getDisplayName());
            
                f = alt;
                // I guess put it back here just in case?
                // Not sure, this will lose the ResolvedTarget but 
                // that should be okay, the only thing we would lose is the
                // ability to know what the real target function was.
                //action->setFunction(alt);
            }
        }

        f->invoke(action, t->getLoop());
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
PRIVATE void Mobius::doParameter(Action* a)
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
        Track* t = getTrack(a->getTargetTrack() - 1);
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
        for (int i = 0 ; i < mTrackCount ; i++) {
            Track* t = getTrack(i);
            if (t->getGroup() == group) {
                if (p->scheduled && nactions > 0)
                  ta = cloneAction(a);
                  
                doParameter(ta, p, t);

                if (p->scheduled && nactions > 0)
                  completeAction(ta);
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
            doParameter(a, p, mTrack);
        }
        else {
            Action* ta = a;
            int nactions = 0;
            for (int i = 0 ; i < mTrackCount ; i++) {
                Track* t = getTrack(i);
                if (isFocused(t)) {
                    if (p->scheduled && nactions > 0)
                      ta = cloneAction(a);

                    doParameter(ta, p, t);

                    if (p->scheduled && nactions > 0)
                      completeAction(ta);
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
PRIVATE void Mobius::doParameter(Action* a, Parameter* p, Track* t)
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
        int max = p->getHigh(this);
       
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
PRIVATE void Mobius::doUIControl(Action* a)
{
    UIControl* c = (UIControl*)a->getTargetObject();
    if (c == NULL) {
        Trace(1, "Missing action UI Control\n");
    }
    else {
        MobiusListener* listener = getListener();
        if (listener != NULL)
          listener->MobiusAction(a);
    }
}

/****************************************************************************
 *                                                                          *
 *   							   SCRIPTS                                  *
 *                                                                          *
 ****************************************************************************/

/**
 * Convey a message to the UI.
 * This isn't necessarily just for scripts, think about other uses
 * for this now that we have it
 */
PUBLIC void Mobius::addMessage(const char* msg)
{
	// farm this out to MobiusThread?
	if (mListener != NULL)
	  mListener->MobiusMessage(msg);
}

/**
 * RunScriptFunction global function handler.
 * RunScriptFunction::invoke calls back to to this.
 */
PUBLIC void Mobius::runScript(Action* action)
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
          Trace(this, 2, "Mobius: runScript %s\n", 
                script->getDisplayName());
        else
          Trace(this, 2, "Mobius: runScript %s UP\n",
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

            Track* track = resolveTrack(action);
            if (track != NULL) {
                action->setResolvedTrack(track);
                function->invoke(action, track->getLoop());
            }
            else if (!script->isFocusLockAllowed()) {
                // script invocations are normally not propagated
                // to focus lock tracks
                action->setResolvedTrack(mTrack);
                function->invoke(action, mTrack->getLoop());
            }
            else {
                // like doFunction, we have to clone the Action
                // if there is more than one destination track
                int nactions = 0;
                for (int i = 0 ; i < mTrackCount ; i++) {
                    Track* t = mTracks[i];
                    if (isFocused(t)) {
                        if (nactions > 0)
                          action = cloneAction(action);

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
PRIVATE void Mobius::startScript(Action* action, Script* script)
{
	Track* track = resolveTrack(action);

	if (track != NULL) {
        // a track specific binding
		startScript(action, script, track);
	}
    else if (action->getTargetGroup() > 0) {
        // a group specific binding
        int group = action->getTargetGroup();
        int nactions = 0;
        for (int i = 0 ; i < mTrackCount ; i++) {
            Track* t = getTrack(i);
            if (t->getGroup() == group) {
                if (nactions > 0)
                  action = cloneAction(action);
                startScript(action, script, t);
            }
        }
    }
	else if (!script->isFocusLockAllowed()) {
		// script invocations are normally not propagated
		// to focus lock tracks
		startScript(action, script, mTrack);
	}
	else {
        int nactions = 0;
		for (int i = 0 ; i < mTrackCount ; i++) {
			Track* t = mTracks[i];
            if (isFocused(t)) {
                if (nactions > 0)
                  action = cloneAction(action);
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
PRIVATE void Mobius::startScript(Action* action, Script* s, Track* t)
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

		//Trace(this, 2, "Mobius: Controller script %ld\n",
		//(long)(action->triggerValue));

		ScriptInterpreter* si = new ScriptInterpreter(this, t);
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
                Trace(this, 1, "Mobius: SUS script not found!\n");
            }
            else {
                // shouldn't have called this method
                Trace(this, 1, "Mobius: Ignoring up transition of non-sustainable script\n");
            }
		}
		else {
			ScriptLabelStatement* l = s->getEndSustainLabel();
			if (l != NULL) {
                Trace(this, 2, "Mobius: Script thread %s: notify end sustain\n", 
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
                  Trace(this, 2, "Mobius: Script thread %s: notify multiclick\n",
                        si->getTraceName());
			}
			else {
				l = s->getReentryLabel();
                if (l != NULL)
                  Trace(this, 2, "Mobius: Script thread %s notify reentry\n",
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
			si = new ScriptInterpreter(this, t);
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
PRIVATE void Mobius::addScript(ScriptInterpreter* si)
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
PRIVATE bool Mobius::isInUse(Script* s) 
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
PRIVATE ScriptInterpreter* Mobius::findScript(Action* action, Script* s,
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
PUBLIC void Mobius::resumeScript(Track* t, Function* f)
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
PUBLIC void Mobius::cancelScripts(Action* action, Track* t)
{
    if (action == NULL) {
        // we had been ignoring these, when can this happen?
        Trace(this, 2, "Mobius::cancelScripts NULL action\n");
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
                Trace(this, 2, "Mobius: Script thread %s: canceling\n",
                      si->getTraceName());
                si->stop();
            }
        }
    }
}

/**
 * Called at the start of each audio interrupt to process
 * script timeouts and remove finished scripts from the run list.
 */
void Mobius::doScriptMaintenance()
{
	// some of the scripts need to know the millisecond size of the buffer
	int rate = mInterruptStream->getSampleRate();	
	long frames = mInterruptStream->getInterruptFrames();
	int msecsInBuffer = (int)((float)frames / ((float)rate / 1000.0));
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
                    Trace(this, 2, "Mobius: Script thread %s: notify sustain\n",
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
                    Trace(this, 2, "Mobius: Script thread %s: notify end multiclick\n",
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
PRIVATE void Mobius::freeScripts()
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
			for (int i = 0 ; i < mTrackCount ; i++)
			  mTracks[i]->removeScriptReferences(si);

			// !! need to pool these
			// !! are we absolutely sure there can't be any ScriptEvents
			// pointing at this?  These used to live forever, it scares me

            Trace(this, 2, "Mobius: Script thread %s: ending\n",
                  si->getTraceName());

			delete si;
		}
	}
}

/****************************************************************************
 *                                                                          *
 *   					   SCRIPT CONTROL VARIABLES                         *
 *                                                                          *
 ****************************************************************************/

PUBLIC bool Mobius::isNoExternalInput()
{
	return mNoExternalInput;
}

/**
 * Called indirectly by the NoExternalAudio script variable setter.
 */
PUBLIC void Mobius::setNoExternalInput(bool b)
{
	mNoExternalInput = b;

	// test hack, if we're still in an interrupt, zero out the last 
	// input buffer so we can begin recording immediately
	if (mInterruptStream != NULL && b) {
		long frames = mInterruptStream->getInterruptFrames();
        // !! assuming 2 channel ports
		long samples = frames * 2;
		float* inbuf;
		float* outbuf;

		// always port 0, any need to change?
		mInterruptStream->getInterruptBuffers(0, &inbuf, 0, &outbuf);

		memset(inbuf, 0, sizeof(float) * samples);
		// Recorder may need to inform the others?
		//mRecorder->inputBufferModified(inbuf);
	}
}

/****************************************************************************
 *                                                                          *
 *                              GLOBAL FUNCTIONS                            *
 *                                                                          *
 ****************************************************************************/

/**
 * May be called by the UI to save the current loop to a file.
 * This is part of MobiusInterface.
 *
 * The name is optional and will default to the "quick save" file
 * global parameter.
 */
PUBLIC void Mobius::saveLoop(const char* name)
{
	ThreadEvent* te = new ThreadEvent(TE_SAVE_LOOP);
    if (name != NULL)
      te->setArg(0, name);
	mThread->addEvent(te);
}

/**
 * Called by the invocation of the SaveLoop global function.
 * 
 * SaveLoop is one of the few that could be declared with 
 * outsideInterrupt since all we do is schedule a MobiusThread event.
 */
PUBLIC void Mobius::saveLoop(Action* action)
{
	ThreadEvent* te = new ThreadEvent(TE_SAVE_LOOP);

    // optional file name
    if (action->arg.getType() == EX_STRING)
      te->setArg(0, action->arg.getString());
    action->setThreadEvent(te);

	mThread->addEvent(te);
}

/**
 * Eventually called by MobiusThread to implement SaveLoop.
 *
 * Obvsiously serious race conditions here, but relatively safe
 * as long as you don't do a Reset while it is being saved.  Even then
 * the buffers will be returned to the pool so we should at least
 * not have invalid pointers.
 *
 * !! The Rehearse test scripts can get into a race condition
 * of they SaveLoop at the exact end of the loop when we're
 * about to enter another record phase.
 */
Audio* Mobius::getPlaybackAudio()
{
    Audio* audio = mTrack->getPlaybackAudio();

    // since this might be saved to a file make sure the
    // sample rate is correct
	if (audio != NULL)
	  audio->setSampleRate(getSampleRate());

    return audio;
}

/**
 * GlobalReset function handler.  This isn't a "global" function
 * even though it has global in the name.  This will always be scheduled
 * on a track and be called from within the interrupt.
 */
PUBLIC void Mobius::globalReset(Action* action)
{
	// let action be null so we can call it internally
	if (action == NULL || action->down) {

        // reset global variables
        mVariables->reset();

		// reset all tracks
		for (int i = 0 ; i < mTrackCount ; i++) {
			Track* t = mTracks[i];
			t->reset(action);

            // also reset the variables until we can determine
            // whether TrackReset should do this
            UserVariables* vars = t->getVariables();
            vars->reset();
		}

		// return to the track selected int the setup
		int initialTrack = 0;
		Setup* setup = mConfig->getCurrentSetup();
		if (setup != NULL)
		  initialTrack = setup->getActiveTrack();
		setTrack(initialTrack);

		// cancel in progress audio recordings	
		// or should we leave the last one behind?
		if (mAudio != NULL)
		  mAudio->reset();
		mCapturing = false;

		// post a thread event to notify the UI
		ThreadEvent* te = new ThreadEvent(TE_GLOBAL_RESET);
		mThread->addEvent(te);

        // Should we reset all sync pulses too?
        mSynchronizer->globalReset();
	}
}

/**
 * Called by MobiusThread when it processes a TE_GLOBAL_RESET event.
 * This is kludgey and used to notify the UI in case it is keeping
 * it's own global state.  Can't do this directly from globalReset() 
 * because we can't touch the UI from within the audio interrupt.
 */
PUBLIC void Mobius::notifyGlobalReset()
{
	Trace(mTrack, 2, "Mobius::notifyGlobalReset\n");

	if (mListener != NULL)
	  mListener->MobiusGlobalReset();
}

/**
 * Called by some function handlers to cancel global mute mode.
 * This happens whenever we start altering mute status in tracks
 * directly before using GlobalMute to restore the last mute state.
 *
 * Giving this an Action for symetry, though since we're called
 * from an event handler won't have one.
 */
PUBLIC void Mobius::cancelGlobalMute(Action* action)
{
	for (int i = 0 ; i < mTrackCount ; i++) {
		Track* t = mTracks[i];
		t->setGlobalMute(false);
		t->setSolo(false);
	}
}


/**
 * SampleTrigger global function handler.
 * 
 * OLD NOTES: not accurate?
 * We will copy the sample content into both the input and output
 * buffers of the interrupt handler, the input buffer so we can inject
 * content for testing, and the output buffer so we can hear it.
 * But the input and output "cursors" are both starting from the first
 * frame in the same when they should be offset by
 * InputLatecny + OutputLatency.  What this means is that any recorded
 * content will play back at a slightly different location than what
 * was heard during recording.   This is generally not noticeable
 * except for a slight difference in the character of the "phasing" at
 * the start of the loop if the recording was ended with an overdub
 * and the overdub continues into the next layer.  It will sound
 * one way when first recorded and different on the next playback.  
 * Fixing this requires that we maintain a pair of record/play cursors
 * like we do for Loops.  I don't think that's worth messing with.
 */
PUBLIC void Mobius::sampleTrigger(Action* action, int index)
{
	mSampleTrack->trigger(mInterruptStream, index, action->down);
}

/**
 * This if for the script interpreter so it can know
 * the number of frames in the last triggered sample.
 */
PUBLIC long Mobius::getLastSampleFrames()
{
	return mSampleTrack->getLastSampleFrames();
}

/**
 * Bootstrap and select a standard unit test setup.
 * This is called only by evaluation of the UnitTestSetup script statement.
 *
 * This is unusual because we're in the interrupt handler but we'll
 * also perform an edit to the master config.
 * 
 * We first bootstrap a Setup named "Unit Test Setup" and "Unit Test Preset"
 * if they don't already exist.  If the setup or preset already exist they
 * are initialized to a standard state.  This initialization prevents
 * test anomolies that can happen if the unit test setup is manually edited.
 *
 * TODO: Ideally we would have a way to install the
 * samples the tests require, for now assume we've got a 
 * captured mobius.xml file for unit tests.  But if we do that
 * then why bother with this?
 *
 */
PUBLIC void Mobius::unitTestSetup()
{
    bool saveConfig = false;
	bool saveSamples = false;

    // first bootstrap the master config
    // !! ordinarilly we try not to do things like write files 
    // in the interrupt handler but since this is just for testing don't
    // bother bifurcating this into a MobiusThread part and an interrupt part
    if (unitTestSetup(mConfig))
      writeConfiguration(mConfig);

    // then apply the same changes to the interrupt config so we
    // can avoid pushing another thing on the history
    unitTestSetup(mInterruptConfig);

    // then set and propagate the setup and preset
    // note that all loops have to be reset for the preset to be refreshed
    Setup* setup = mInterruptConfig->getSetup(UNIT_TEST_SETUP_NAME);
    setSetupInternal(setup);

    // !! not supposed to do anything in the UI thread from within
    // the interrupt handler, again for unit tests this is probably
    // okay but really should be routing this through MobiusThread 
    if (mListener)
      mListener->MobiusConfigChanged();
}

/**
 * Initialize the unit test setup and preset within a config object.
 * This is called twice, once for the master config and once for
 * the interrupt config to make sure they're both in sync without
 * having to worry about cloning and adding to the history list.
 */
PRIVATE bool Mobius::unitTestSetup(MobiusConfig* config)
{
    bool needsSaving = false;

    // boostrap a preset
    Preset* p = config->getPreset(UNIT_TEST_PRESET_NAME);
    if (p != NULL) {
        p->reset();
    }
    else {
        p = new Preset();
        p->setName(UNIT_TEST_PRESET_NAME);
        config->addPreset(p);
        needsSaving = true;
    }
    config->setCurrentPreset(p);

    // boostrap a setup
    Setup* s = config->getSetup(UNIT_TEST_SETUP_NAME);
    if (s != NULL) {
        s->reset(p);
    }
    else {
        s = new Setup();
        s->setName(UNIT_TEST_SETUP_NAME);
        s->reset(p);
        config->addSetup(s);
        needsSaving = true;
    }
    config->setCurrentSetup(s);

    return needsSaving;
}

/****************************************************************************
 *                                                                          *
 *                                  CAPTURE                                 *
 *                                                                          *
 ****************************************************************************/

/**
 * StartCapture global function handler.
 *
 * Also called by the BounceEvent handler to begin a bounce recording.
 * May want to have different Audios for StartCapture and Bounce,
 * but it's simpler to reuse the same mechanism for both.
 *
 * Here we just set the mCapturing flag to enable recording, 
 * appending the samples to mAudio actually happens down in
 * recorderMonitorExit after all the tracks have had a chance to 
 * contribute.  Note though that on the first block we may
 * actually be somewhere in the middle due event scheduling, and the first
 * part of the block is technically not part of the recording.  The test
 * scripts currently use "Wait block" to avoid this, but BouceEvent needs
 * to be more precise.  The block offset for the first block is stored
 * in mCaptureOffset, used once then reset back to zero.
 */
PUBLIC void Mobius::startCapture(Action* action)
{
	if (!mCapturing) {
		if (mAudio != NULL)
		  mAudio->reset();
        else {
            mAudio = mAudioPool->newAudio();
            mAudio->setSampleRate(getSampleRate());
        }
		mCapturing = true;

		Track* t = resolveTrack(action);
        if (t == NULL)
          t = mTrack;

		mCaptureOffset = t->getProcessedOutputFrames();
	}
}

/**
 * StopCapture global function handler.
 * 
 * Also now used by the BounceEvent handler when we end a bouce record.
 * 
 * If we're in a script, try to be precise about where we end the
 * recording.  Simply turning the flag off will remove all of the
 * current block from the recording, and a portion of it may
 * actually have been included.
 * 
 * UPDATE: Any reason why we should only do this from a script?
 * Seems like something we should do all the time, especially for bounces.
 */
PUBLIC void Mobius::stopCapture(Action* action)
{

	if (mCapturing && mAudio != NULL && mInterruptStream != NULL
		// && action->trigger == TriggerScript
		) {
		float* output = NULL;
		// TODO: merge the interrupt buffers for all port sets
		// that are being used by any of the tracks
		mInterruptStream->getInterruptBuffers(0, NULL, 0, &output);
		if (output != NULL) {
			Track* t = resolveTrack(action);
            if (t == NULL)
              t = mTrack;
			mAudio->append(output, t->getProcessedOutputFrames());
		}
	}

	mCapturing = false;
}

/**
 * SaveCapture global function handler.
 * 
 * Since this involves file IO, have to pass it to the thread.
 */
PUBLIC void Mobius::saveCapture(Action* action)
{
    const char* file = NULL;
    if (action != NULL && action->arg.getType() == EX_STRING)
      file = action->arg.getString();

	ThreadEvent* te = new ThreadEvent(TE_SAVE_AUDIO, file);
    if (action != NULL)
      action->setThreadEvent(te);

	mThread->addEvent(te);
}

/**
 * Eventually called by MobiusThread to implement the SaveCapture function.
 * 
 * !! We have a race condition with the interrupt handler.  
 * Tell it to stop recording and pause for at least one interupt.
 *
 * Caller MUST NOT DELETE the returned Audio object.  We keep it around
 * for the next time.  
 */
Audio* Mobius::getCapture()
{
	if (mAudio != NULL) {
		mCapturing = false;
		SleepMillis(100);
	}
	return mAudio;
}

/**
 * Hander for BounceEvent.
 *
 * NOTE: Since this relies on the audio recording stuff above have to
 * reconcile the inside/outside interrupt issues.  Think more about this
 * when you redesign bounce.
 *
 * Since all the logic is up here in Mobius, the event handler doesn't
 * do anything other than provide a mechanism for scheduling the call
 * at a specific time.
 *
 * Note that if we are called by the event handler rather than
 * directly by BounceFunction, we won't have a Action so the
 * things we call need to deal with that.
 *
 * Currently using the same mechanism as audio recording, the only difference
 * is that the start/end times may be quantized and how we process the
 * recording after it has finished.
 * 
 * TODO: I was going to support a BounceMode preset parameter that
 * would let you customize the bounce. The default would be to mute all
 * source tracks, another option would be to reset them.  Should we do
 * this we need to decide which of the possible source tracks provides
 * the Preset.  Assume the current track if not changed by the script.
 *
 * Selecting the target track could also be controlled with parameters.
 * Currently we pick the first non-empty track from the left.
 *
 * Try to preserve the cycle length in the bounce target track.  If the
 * length of the bounce track is an even multiple of the cycle length 
 * of the source track(s) preserve the cycle length.
 * 
 * Determining the cycle length of the source tracks is ambiguous because
 * all tracks could have a different cycle length.  Some methods are:
 *
 *  - Let tracks "vote" and the ones with the most common cycle length win.  
 *    Muted tracks should not be allowed to vote.  
 *
 *  - The first unmuted track from the left wins.
 *
 *  - The current track (or script target track) wins, but it may be empty.
 *
 *  - The current track wins if not empty, otherwise first unmuted
 *    track from the left.
 *
 * It feels right to favor the current track if it is not empty.
 * Voting would be nice but complicated, assume for now we can pick
 * the first one from the left.
 */
PUBLIC void Mobius::toggleBounceRecording(Action* action)
{
	if (!mCapturing) {
		// start one, use the same function that StartCapture uses
		startCapture(action);
	}
	else {
		// stop and capture it
		stopCapture(action);
		Audio* bounce = mAudio;
		mAudio = NULL;
		mCapturing = false;

		if (bounce == NULL)
		  Trace(this, 1, "Mobius: No audio after end of bounce recording!\n");
		else {
			// Determine the track that supplies the preset parameters
			// (not actually used right now)
			Track* source = resolveTrack(action);
            if (source == NULL)
              source = mTrack;
			Preset* p = source->getPreset();

			// TODO: p->getBounceMode() should tell us whether
			// to simply mute the source tracks or reset them,
			// for now assume mute
			
			// locate the target track for the bounce
			Track* target = NULL;
			int targetIndex = 0;
			for (int i = 0 ; i < mTrackCount ; i++) {
				Track* t = mTracks[i];
				// formerly would not select the "source" track
				// but if it is empty we should use it?
				//if (t != source && t->isEmpty()) {
				if (t->isEmpty()) {
					target = t;
					targetIndex = i;
					break;
				}
			}

			// determine the number of cycles in the bounce track
			Track* cycleTrack = source;
			if (cycleTrack == NULL || cycleTrack->isEmpty()) {
				for (int i = 0 ; i < mTrackCount ; i++) {
					Track* t = mTracks[i];
					// ignore muted tracks?
					if (!t->isEmpty()) {
						cycleTrack = t;
						break;
					}
				}
			}

			int cycles = 1;
			if (cycleTrack != NULL) {
				Loop* l = cycleTrack->getLoop();
				long cycleFrames = l->getCycleFrames();
				long recordedFrames = bounce->getFrames();
				if ((recordedFrames % cycleFrames) == 0)
				  cycles = recordedFrames / cycleFrames;
			}

			if (target == NULL) {
				// all dressed up, nowhere to go
				delete bounce;
			}
			else {
				// this is raw, have to fade the edge
				bounce->fadeEdges();
				target->setBounceRecording(bounce, cycles);

				// all other tracks go dark
				// technically we should have prepared for this by scheduling
				// a mute jump in all the tracks at the moment the
				// BounceFunction was called.  But that's hard, and at
				// ASIO latencies, it will be hard to notice the latency
				// adjustment.

				for (int i = 0 ; i < mTrackCount ; i++) {
					Track* t = mTracks[i];
					if (t != target)
					  t->setMuteKludge(NULL, true);
				}

				// and make it the active track
				// sigh, the tooling is all set up to do this by index
				setTrack(targetIndex);
			}
		}
	}
}

/****************************************************************************
 *                                                                          *
 *   						   TRACK CALLBACKS                              *
 *                                                                          *
 ****************************************************************************/
/*
 * These are methods calledby Track in order to process functions.
 *
 */

/**
 * Called by Track when it processes a TrackCopySound or
 * TrackCopyTiming function.  Return the track that is the source
 * of the copy.  Currently defining this as the adjacent track on the left,
 * could be fancier, but it might require saving some state?
 */
PUBLIC Track* Mobius::getSourceTrack()
{
	Track* src = NULL;

    if (mTrackCount > 1) {
        int index = mTrack->getRawNumber();
        if (mTrackIndex > 0)
          src = mTracks[index - 1];
        else {
            // wrap back to the last track or just prevent a copy?
            src = mTracks[mTrackCount - 1];
        }
    }

	return src;
}

/**
 * Unconditionally changes the active track.  
 *
 * This is not part of the public interface.  If you want to change
 * tracks with EmptyTrackAction behavior create an Action.
 *
 * This must be called in the interrupt, currently it is only used by Loop.
 */
PUBLIC void Mobius::setTrack(int index)
{
    if (index >= 0 && index < mTrackCount) {
        mTrack = mTracks[index];
		if (mRecorder != NULL)
		  mRecorder->select(mTrack);
    }
}

/****************************************************************************
 *                                                                          *
 *   						   INTERUPT HANDLER                             *
 *                                                                          *
 ****************************************************************************/

/**
 * Mobius installs itself as a RecorderMonitor in the Recorder.
 * This method will be called once in each audio interrupt before
 * any of the tracks are processed.
 *
 * !! Script Recording Inconsistency
 *
 * This is implemented assuming that we only record functions for the
 * active track.  In theory, if a burst of functions came in within
 * the same interrupt, something like this could happen:
 *
 *      NextTrack
 *      Record
 *      NextTrack
 *      Record
 *
 * The effect would be that there are now pending functions
 * on two tracks, but the script recorder doesn't know how
 * to interleave them with the NextTrack function handled
 * by Mobius.  The script would end up with:
 *
 *      NextTrack
 *      NextTrack
 *      Record
 *      Record
 *
 * We could address this by always posting functions to a list
 * in the order they come in and wait for the interrupt
 * handler to consume them.  But it's complicated because we have
 * to synchronize access to the list.    In practice, it is very
 * hard to get functions to come in this rapidly so there
 * are more important things to do right now.  Also, Track
 * only allows one function at a time.
 */
void Mobius::recorderMonitorEnter(AudioStream* stream)
{
	if (mHalting) return;

	// this turns out to be useful for a few special testing
	// operations eventually performed during track processing, so save it
	// it also serves as the "in an interrupt" flag
	mInterruptStream = stream;

	// trace effective latency the first time we're here
	mInterrupts++;
	if (mInterrupts == 1)
	  Trace(2, "Mobius: Receiving interrupts, input latency %ld output %ld\n",
			(long)getEffectiveInputLatency(), (long)getEffectiveOutputLatency());

    // Shift in a new MobiusConfiguration object

    if (mPendingInterruptConfig != NULL) {
        Trace(2, "Mobius: Installing interrupt MobiusConfig\n");
        // Have to maintain the old config on the history list because
        // getState() needs to get information about the track preset and
        // if we delete it now it could be at the exact moment that the
        // UI thread is refreshing state.  The easiest way to prevent this
        // is to keep a history but ideally we should be pushing status
        // at the UI rather than having it poll us for it.
        // The length of the history needs to be at least as long as the UI
        // polling interval.  Once we start using ObjectPool we could free it
        // with a "keepalive" value of a second or more.
        mPendingInterruptConfig->setHistory(mInterruptConfig);
        mInterruptConfig = mPendingInterruptConfig;
        mPendingInterruptConfig = NULL;

        // propagate changes to interested parts
        propagateInterruptConfig();
    }

    // interrupts may come in during initialization before we've had
    // a chance to install the configuration, ignore these interrupts
    // KLUDGE: Need a better way of detecting this than the stupid
    // default flag...
    if (mInterruptConfig->isDefault()) {
        Trace(2, "Mobius: Igoring audio interrupt before config loaded\n");
        return;
    }

    // install new watchers
    installWatchers();

    // change setups
    if (mPendingSetup >= 0) {
        setSetupInternal(mPendingSetup);
        mPendingSetup = -1;
    }

    // Shift in a new pack of samples

	SamplePack* samples = mPendingSamples;
	mPendingSamples = NULL;
	if (samples != NULL) 
	  mSampleTrack->setSamples(samples);

    // Shift in a new project

	Project* p = mPendingProject;
	mPendingProject = NULL;
	if (p != NULL)
      loadProjectInternal(p);

	// Hack for testing, when this flag is set remove all external input
	// and only pass through sample content.  Necessary for repeatable
	// tests so we don't get random noise in the input.
	if (mNoExternalInput) {
		long frames = stream->getInterruptFrames();
        // !! assuming 2 channel ports
		long samples = frames * 2;
		float* input;
		stream->getInterruptBuffers(0, &input, 0, NULL);
		memset(input, 0, sizeof(float) * samples);
	}

	mSynchronizer->interruptStart(stream);

	// prepare the tracks before running scripts
	mSampleTrack->prepareForInterrupt();
	for (int i = 0 ; i < mTrackCount ; i++) {
		Track* t = mTracks[i];
		t->prepareForInterrupt();
	}

    // do the queued actions
    doInterruptActions();

    // Advance the long-press tracker too, this may cause other 
    // actions to fire.
    mTriggerState->advance(this, stream->getInterruptFrames());

	// process scripts
    doScriptMaintenance();
}

/**
 * Propagate pieces of a new MobiusConfig that has being installed
 * in the interrupt handler.  This should go down here rather than
 * in Mobius::installCofiguration if they can effect the operation of
 * code in the interrupt handler and this could cause inconsistencies
 * or other problems if changed in the middle of an interrupt.
 */
PRIVATE void Mobius::propagateInterruptConfig() 
{
    // turn monitoring on or off
	if (mRecorder != NULL)
      mRecorder->setEcho(mInterruptConfig->isMonitorAudio());

    // track changes to input and output latency
    if (mSampleTrack != NULL)
      mSampleTrack->updateConfiguration(mInterruptConfig);

    // Synchronizer needs maxSyncDrift, driftCheckPoint
    if (mSynchronizer != NULL)
      mSynchronizer->updateConfiguration(mInterruptConfig);

    // Modes track altFeedbackDisables
    MobiusMode::updateConfiguration(mInterruptConfig);

    // thankfully it is hidden now and can't be changed
	AudioFade::setRange(mInterruptConfig->getFadeFrames());

    // tracks are sensitive to lots of things including prests and setups
	for (int i = 0 ; i < mTrackCount ; i++) {
		Track* t = mTracks[i];
		t->updateConfiguration(mInterruptConfig);
	}

    // Update some things in the Setup that are done by
    // setSetupInternal but aren't handled by Track::updateConfiguration
    if (!mInterruptConfig->isNoSetupChanges()) {
        Setup* setup = mInterruptConfig->getCurrentSetup();
        propagateSetupGlobals(setup);
    }
}

/**
 * Called from within the interrupt to change setups.
 */
PUBLIC void Mobius::setSetupInternal(int index)
{
    Setup* setup = mInterruptConfig->getSetup(index);
    if (setup == NULL)
      Trace(1, "ERROR: Invalid setup number %ld\n", (long)index);
    else
      setSetupInternal(setup);
}

/**
 * Activate a new setup.
 * This MUST be called within the interrupt and the passed Setup
 * object must be within mInterruptConfig.
 * This can be called from these places:
 *
 *     - loadProjectInternal to select the setup stored in the project
 *     - ScriptSetupStatement to select a setup in a script
 *     - recorderMonitorEnter to process mPendingSetup
 *     - unitTestSetup to select the unit test setup
 * 
 */
PUBLIC void Mobius::setSetupInternal(Setup* setup)
{
	if (setup != NULL) {
        // need to track the selection here so Reset processing
        // can go back to the last setup
        mInterruptConfig->setCurrentSetup(setup);

        for (int i = 0 ; i < mTrackCount ; i++) {
            Track* t = mTracks[i];
            t->setSetup(setup);
        }

        // things that aren't track specific
        propagateSetupGlobals(setup);   
    }
}

/**
 * Called from a few locations within the interrupt to install
 * Things in the setup that are not track specific.
 * Mostly this is the the overlay bindings.
 * 
 * This is new in 2.0.  We also have a binding overlay value in the
 * Project, which will capture whatever the overlay was at the time the
 * project was saved.  This may be different than what is in the Setup but
 * not usually.
 * THINK: It may be confusing not to have loading a project put everything
 * back into the Setup state?
 *
 * This is an unusual case where we're in an interrupt but we
 * must set the master MobiusConfig object to change the
 * binding overlay since that is not used inside the interrupt.
 */
PRIVATE void Mobius::propagateSetupGlobals(Setup* setup)
{
    // changes the active track without TrackCopy semantics
    setTrack(setup->getActiveTrack());

    // A NULL binding value means "keep the current one", if you
    // always want the setup to remove the current binding overlay you
    // need to set it to a special value.
    const char* name = setup->getBindings();
    if (name != NULL) {
        BindingConfig* bindings = mConfig->getBindingConfig(name);
        if (bindings != NULL) {
            setOverlayBindings(bindings);
        }
        else if (StringEqual(name, SETUP_OVERLAY_CANCEL)) {
            // special value that means to always cancel the current bindings
            setOverlayBindings(NULL);
        }
        else {
            // could just let any invalid name cancel the bindings?
        }
    }
}

/**
 * Called at the end of the Recorder interrupt for each buffer.
 * All tracks have been processed.
 */
PUBLIC void Mobius::recorderMonitorExit(AudioStream* stream)
{
	if (mHalting) return;

	long frames = stream->getInterruptFrames();
	mSynchronizer->interruptEnd();
	
	// if we're recording, capture whatever was left in the output buffer
	// !! need to support merging of all of the output buffers for
	// each port selected in each track
	if (mCapturing && mAudio != NULL) {
		float* output = NULL;
		stream->getInterruptBuffers(0, NULL, 0, &output);
		if (output != NULL) {

			// debugging capture
			static int bufcount = 1;
			if (false && bufcount < 5) {
				char file[128];
				sprintf(file, "record%d-%ld.wav", bufcount++, mAudio->getFrames());
                Audio* temp = mAudioPool->newAudio();
                temp->append(output, frames);
                temp->write(file);
                temp->free();
			}

			// the first block in the recording may be a partial block
			if (mCaptureOffset > 0) {
                // !! assuming 2 channel ports
                int channels = 2;
				output += (mCaptureOffset * channels);
				frames -= mCaptureOffset;
				if (frames < 0) {
					Trace(this, 1, "Mobius: Recording offset calculation error!\n");
					frames = 0;
				}
				mCaptureOffset = 0;
			}

			mAudio->append(output, frames);
		}
	}

	// if any of the tracks have requested a UI update, post a message
	// since we're only displaying the beat counter for one track, we don't
	// need to do this for all of them?
	bool uiSignal = false;
	for (int i = 0 ; i < mTrackCount ; i++) {
		if (mTracks[i]->isUISignal())
		  uiSignal = true;
	}
	if (uiSignal)
	  mThread->addEvent(TE_TIME_BOUNDARY);

    // turn off the "in an interrupt" flag
	mInterruptStream = NULL;
}

/**
 * Called by a few function handlers (originally Mute and Insert, now
 * just Insert to change the preset.  This is an old EDPism that I
 * don't really like.  We are inside the interrupt.
 */
PUBLIC void Mobius::setPresetInternal(int number)
{
    mTrack->setPreset(number);
}

/****************************************************************************
 *                                                                          *
 *                                  TRIGGERS                                *
 *                                                                          *
 ****************************************************************************/

/**
 * Process an key event.
 */
PUBLIC void Mobius::doKeyEvent(int key, bool down, bool repeat)
{
    mBindingResolver->doKeyEvent(this, key, down, repeat);
}

/**
 * Intended for MobiusPlugin or anything else that wants to simulate
 * a MIDI trigger that didn't come directly from a MIDI device.
 * When dealgin directly with MIDI devices, Mobius will register itself
 * as a MidiListener which calls midiEvent() below.
 */
PUBLIC void Mobius::doMidiEvent(MidiEvent* e)
{
    // just forward it to the MidiListener method
    midiEvent(e);
}

/**
 * MidiHandler interface.
 * Clocks are not propagated, instead we just count them and wait
 * for the next audio interrupt to process.  Would be better if we could
 * try to determine the offsets into the buffer of the MIDI events!!
 *
 * NOTE: If we allow more than one input device to be opened, this will
 * be called from different "monitor threads" without any synchronization.
 * We either need to put Csects around sensitive areas or better yet, make
 * MidiInterface manage a single monitor thread for all input devices.
 */
PUBLIC void Mobius::midiEvent(MidiEvent* e)
{
	int status = e->getStatus();

	// ignore if the sync monitor says its a realtime event
	if (!mHalting && !mSynchronizer->event(e)) {

		bool processIt = true;
		if (mListener != NULL)
		  processIt = mListener->MobiusMidiEvent(e);
			
		if (processIt) {

            // next let the control surfaces have a crack
            for (ControlSurface* cs = mControlSurfaces ; cs != NULL ; 
                 cs = cs->getNext()) {
                     
                if (cs->handleEvent(e)) {
                    processIt = false;
                    break;
                }
            }
            
            if (processIt) {
                // this will eventually call Mobius::doAction
                mBindingResolver->doMidiEvent(this, e);
            }
        }
    }
}

/****************************************************************************
 *                                                                          *
 *                            UNIT TEST INTERFACE                           *
 *                                                                          *
 ****************************************************************************/
/*
 * Originally just for the unit tests, but not used by Project too.
 */

PUBLIC Track* Mobius::getTrack()
{
    return mTrack;
}

PUBLIC int Mobius::getTrackCount()
{
	return mTrackCount;
}

PUBLIC Track* Mobius::getTrack(int index)
{
	return ((index >= 0 && index < mTrackCount) ? mTracks[index] : NULL);
}

/****************************************************************************
 *                                                                          *
 *                                LOCALIZATION                              *
 *                                                                          *
 ****************************************************************************/

/**
 * Load the message catalogs and perform localization of various
 * internal objects like modes, functions, and parameters.
 *
 * This is normally deferred until start() but for the AU plugin
 * we have to be able to do this earlier so we can publish dipslay
 * names for things immediately during plugin construction.
 * I really don't like this.  One alternative would be
 * to have a parameter cache pre-calculated with display names
 * that you have to refresh after the plugin comes all the way up.
 *
 * The language can be overridded in the MobiusConfig, in theory we could
 * be tracking this and relocalize after every edit, but it's too much
 * work and no one uses this.
 */
PRIVATE void Mobius::localize()
{
    if (mCatalog == NULL) {

		const char* lang = mConfig->getLanguage();
		if (lang != NULL)
		  mCatalog = readCatalog(lang);

		// default to English
		if (mCatalog == NULL)
		  mCatalog = readCatalog(DEFAULT_LANGUAGE);

		// if we're misconfigured have to have something
		if (mCatalog == NULL) {
            Trace(1, "ERROR: Unable to read message catalog!!\n");
            mCatalog = new MessageCatalog();
        }

        // propagate the catalog to the internal objects
		MobiusMode::localizeAll(mCatalog);
		Parameter::localizeAll(mCatalog);
		Function::localizeAll(mCatalog);
        WatchPoint::localizeAll(mCatalog);

        localizeUIControls();
	}
}

/**
 * Read the message catalog for a given language.
 * These are normally only in the installation directory, but
 * support alternate langs in the config directory too.
 */
PRIVATE MessageCatalog* Mobius::readCatalog(const char* language)
{
	MessageCatalog* cat = NULL;
	char catalog[256];
	char path[1024];
	
	sprintf(catalog, "Catalog_%s.txt", language);
	findConfigurationFile(catalog, path, sizeof(path));

	cat = new MessageCatalog();
	if (!cat->read(path)) {
		// problems reading catalog
		printf("ERROR: Mobius: Unable to read message catalog: %s\n", path);
		fflush(stdout);
		delete cat;
		cat = NULL;
	}

	return cat;
}

PRIVATE void Mobius::localizeUIControls()
{
	if (mCatalog != NULL) {
        if (mUIControls != NULL) {
            for (int i = 0 ; mUIControls[i] != NULL ; i++) {
                UIControl* c = mUIControls[i];
                c->localize(mCatalog);
            }
        }

        if (mUIParameters != NULL) {
            for (int i = 0 ; mUIParameters[i] != NULL ; i++) {
                UIParameter* p = mUIParameters[i];
                p->localize(mCatalog);
            }
        }
    }

}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
