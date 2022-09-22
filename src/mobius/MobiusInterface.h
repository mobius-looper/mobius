/*
 * Copyright (c) 2011 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * MOBIUS PUBLIC INTERFACE
 *
 * The primary public interface include for the Mobius engine.
 *
 * The only implementation of this is the Mobius class, but that has a lot
 * of stuff that needs to be accessible internally by function handlers
 * and I wanted to make the interface for the UI and plugin hosts
 * clearer.  This interface should be used by anything on the "outside"
 * that is hosting the mobius looping engine including the Mobius UI, 
 * host plugin adapters, OSC message handlers, MIDI event handlers.
 *
 */

#ifndef MOBIUS_INTERFACE_H
#define MOBIUS_INTERFACE_H

/****************************************************************************
 *                                                                          *
 *                               MOBIUS CONTEXT                             *
 *                                                                          *
 ****************************************************************************/

/**
 * Encapsulates a few things about the runtime environment that
 * are passed into the Mobius engine.
 * 
 * Do not to depend on qwin/Context here.
 *
 * Might want to evolve this into a package of OS specific methods, 
 * sort of like the util functions only encapsulated?
 *
 * One of these must be built by the application that wraps the
 * Mobius engine, currently there are three: 
 * Windows standalone (WinMain), Mac standalone (MacMain), 
 * VST or AU plugin (MobiusPlugin).
 */
class MobiusContext {

  public:

	MobiusContext();
	~MobiusContext();

	void setCommandLine(const char* s);
	const char* getCommandLine();

	void setInstallationDirectory(const char* s);
	const char* getInstallationDirectory();

	void setConfigurationDirectory(const char* s);
	const char* getConfigurationDirectory();

	void setConfigFile(const char* s);
	const char* getConfigFile();

	void setAudioInterface(class AudioInterface* a);
	class AudioInterface* getAudioInterface();

	void setMidiInterface(class MidiInterface* mi);
	class MidiInterface* getMidiInterface();

	void setHostMidiInterface(class HostMidiInterface* mi);
	class HostMidiInterface* getHostMidiInterface();

	void setDebugging(bool b);
	bool isDebugging();

    void setPlugin(bool b);
    bool isPlugin();

	void parseCommandLine();

  private:

    /**
     * The command line arguments, set when Mobius is run from
     * the command line.
     */
	char *mCommandLine;

    /**
     * The directory where Mobius is installed.
     * On Mac this is derived from the application package directory,
     * on Windows it is stored in the registry.
     */
	char* mInstallationDirectory;

    /**
     * The directory where the Mobius configuration files are stored.
     * On Windows this will be the same as mInstallationDirectory,
     * On Mac this is normally /Library/Application Support/Mobius. 
     */
	char* mConfigurationDirectory;

    /**
     * This full path name of the mobius.xml file.
     * This is not set when the context is created, it is set by
     * Mobius after it locates the mobius.xml file from one of the
     * above directories.  This is only used by the UI so that it
     * can locate the ui.xml file which by convention will always be
     * taken from the same directory as mobius.xml.
     */
    char* mConfigFile;
    
    /**
     * The object providing audio streams.
     * When running standalone this will be a platform-specific class
     * that interact directly with the audio devices.
     * When running as a plugin this will be a proxy to the host
     * application's audio buffers.
     */
	class AudioInterface* mAudio;

    /**
     * The object providing access to MIDI devices.
     * When running standalone this will be a platform-specific class
     * that interacts directly with the MIDI devices.  
     */
	class MidiInterface* mMidi;

    /**
     * The object providing access to MIDI devices when running as a plugin.
     * This is a temporary kludge, see comments in HostMidiInterface for
     * more information.
     */
    class HostMidiInterface* mHostMidi;

    /**
     * Flag set if we're a plugin.
     */
    bool mPlugin;

    /**
     * Special flag that when true enables some unspecified debugging
     * behavior.  Should only be used by Mobius developers.
     */
	bool mDebugging;

};

/****************************************************************************
 *                                                                          *
 *                                   PROMPT                                 *
 *                                                                          *
 ****************************************************************************/

/**
 * A class used to pass information related to user prompting
 * between the Mobius engine and the UI.  One of these is generated
 * by the script interpreter when evaluating a Prompt statement.  
 * The prompt has an associated with a ThreadEvent that the script will
 * be waiting on.
 *
 * The listener is responsible for displaying the prompt message in
 * a suitable way and soliciting a response.  The response is then
 * set in the Prompt object, and returned by caling Mobius::finishPrompt().
 */
class Prompt {
	
    friend class MobiusThread;

  public:

	Prompt();
	~Prompt();

	Prompt* getNext();
	void setNext(Prompt* p);

	const char* getText();
	void setText(const char* text);

	bool isOk();
	void setOk(bool b);
    
  protected:

	class ThreadEvent* getEvent();
	void setEvent(class ThreadEvent* e);

  private:

	Prompt* mNext;
	class ThreadEvent* mEvent;
	char* mText;
	bool mOk;
};


/****************************************************************************
 *                                                                          *
 *                                  LISTENER                                *
 *                                                                          *
 ****************************************************************************/

/**
 * The interface of an object that may receive notification of
 * interesting happenings within Mobius.
 *
 * The most important callback is MobiusRefresh will will be called
 * periodically to tell the UI to redisplay state.  This will be
 * called nearly once every 1/10 second but may be impacted 
 * by other things being done by the Mobius housekeeping thread.
 * It is conceptually similar to the VST "idle" callback, and saves
 * the UI from having to manage its own update timer.
 *
 * The MobiusTimeBoundary callback is called whenever a significant
 * synchronization boundary has passed: beat, bar, cycle, or loop.
 * This can be used by the UI to refresh time sensitive components
 * immediately rather than waiting for the next MobiusRefresh tick
 * or the next private timer tick.  This makes things like beat flashers
 * look more accurate.  
 *
 * MobiusRefresh was added after MobiusTimeBoundary, we could consider
 * merging them and just having MobiusRefresh be called early but I like
 * keeping them distinct for now so you can use MobiusRefresh as a relatively
 * accurate timer.
 * 
 */
class MobiusListener {

  public:

	/**
	 * A periodic refresh interval has been reached.
	 * This is normally called once every 1/10 second.
	 */
	virtual void MobiusRefresh() = 0;

	/**
	 * A significant time boundary has passed (beat, cycle, loop)
	 * so refresh time sensitive components now rather than waiting
	 * for the next timer event to make it look more accurate.
	 */
	virtual void MobiusTimeBoundary() = 0;

	/**
	 * Display some sort of exceptional alert message.
	 */
	virtual void MobiusAlert(const char *msg) = 0;

	/**
	 * Display a normal operational message.
	 */
	virtual void MobiusMessage(const char *msg) = 0;

	/**
	 * Receive notification of a MIDI event.
	 * Return true if Mobius is to continue processing the event.
	 */
	virtual bool MobiusMidiEvent(class MidiEvent* e) = 0;

	/**
	 * Prompt the user for information.
	 */
	virtual void MobiusPrompt(Prompt* p) = 0;

    /**
     * Notify of an internal configuration change, listener may want
     * to refresh displayed configuration state.
     */
    virtual void MobiusConfigChanged() = 0;

    /**
     * Notify of a global reset.
     * This is a hopefully temporary kludge for the message display
     * which we want to allow to persist for a long time, but still
     * clear it when you do a global reset.
     */
    virtual void MobiusGlobalReset() = 0;

    /**
     * Notify the UI of an action on a UIControl.
     */
    virtual void MobiusAction(class Action* action) = 0;


    /**
     * Notify the UI that something major has happened and it should
     * repaint the entire UI.
     */
    virtual void MobiusRedraw() = 0;

};

/****************************************************************************
 *                                                                          *
 *   						 LATENCY CALIBRATION                            *
 *                                                                          *
 ****************************************************************************/

/**
 * This is a duplicate of RecorderCalibrationResult from Recorder.h.
 * Think more about how we want this conveyed, or if we should share this.
 */ 
class CalibrationResult {

  public:

	CalibrationResult() {
		timeout = false;
		noiseFloor = 0.0;
		latency = 0;
	}

	~CalibrationResult() {
	}

	bool timeout;
	float noiseFloor;
	int latency;
};

/****************************************************************************
 *                                                                          *
 *                                   ALERTS                                 *
 *                                                                          *
 ****************************************************************************/

/**
 * An object containing various problems that have happened during
 * Mobius execution that should be presented to the user.
 * Originally a bunch of discrete methods on Mobius, think more about
 * using this for other severe occurrences, the kind of things we would
 * trace with level 1.
 */
class MobiusAlerts {
  public:

    MobiusAlerts();

    /**
     * True if we could not open the configured audio input device.
     */
    bool audioInputInvalid;
    
    /**
     * True if we would not open the configured audio output device.
     */
    bool audioOutputInvalid;

    const char* midiInputError;
    const char* midiOutputError;
    const char* midiThroughError;

};

/****************************************************************************
 *                                                                          *
 *                              MOBIUS INTERFACE                            *
 *                                                                          *
 ****************************************************************************/

/**
 * This defines the public interface for Mobius.  The primary use for this
 * is in the implementation of the Mobius UI, but in theory it could be used
 * to embed the Mobius engine in something else.
 *
 * This was factored out of the Mobius class to make it clearer which
 * methods were considered part of the public API.  The Mobius class
 * has a lot of other methods that are intended for by the function and
 * event handlers.  Normally these would be declared "protected" but there
 * are so many classes that need them it's a pain to maintain the
 * friend list.  
 */
class MobiusInterface {

  public:

	virtual ~MobiusInterface() {}

    //////////////////////////////////////////////////////////////////////
    //
    // Configuration
    //
    //////////////////////////////////////////////////////////////////////

    /**
     * Factory method for the mobius engine.
     * You should only make one of these.
     */
    static MobiusInterface* getMobius(MobiusContext* con);

    /** 
     * Return the MobiusContext object that was passed to the constructor.
     * This must not to be modified.  
     */
    virtual MobiusContext* getContext() = 0;

    /**
     * Return the AudioStream being used, this must not be modified and
     * may be NULL if no devices have been specified.
     */
    virtual class AudioStream* getAudioStream() = 0;

    /**
     * Called by the UI to register a set of UIControl objects that 
     * can be bound to triggers.  
     * !! Firm up who owns these and what the lifespan is.
     * This must be called before calling preparePluginBindings()
     * or start().
     */
    virtual void setUIBindables(class UIControl** controls, 
                                class UIParameter** params) = 0;

    /**
     * Return the registered UIControls.  This is intended for use by
     * binding UIs that need to present the UI controls for binding.
     * !! If the UI gives them to Mobius, then it should already 
     * know what these are.  Rethink this so we can handle UIControls
     * and UIParameters the same way.
     */
    virtual class UIControl** getUIControls() = 0;

    /**
     * Lookup a registered UIControl by name.
     * !! Again, the UI should be able to do this.
     */
    virtual class UIControl* getUIControl(const char* name) = 0;

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
     */
    virtual void preparePluginBindings() = 0;

    /**
     * Do a full initialization, including reading the config files
     * and opening devices.  Construction of the Mobius object will
     * do minimal setup and read the configuration files but will not
     * do anything "expensive".  This is so plugin hosts can instantiate
     * it to probe for information without actually using it.
     */
	virtual void start() = 0;

    /**
     * Get the sample rate of the audio stream.
     */
    virtual int getSampleRate() = 0;

    /**
     * Install an object that will be notified when special things happen.
     */
	virtual void setListener(MobiusListener* mon) = 0;

    /**
     * Return the current listener.  This is typically used only when you
     * want to temporarly override the listener with a different one tne
     * restore it later.
     */
	virtual MobiusListener* getListener() = 0;

    /**
     * Locate a configuration file.
     * The argument is the leaf file name.
     * TODO: Added this for osc.xml, could be using this
     * for ui.xml too.  Return true if the file was found.
     */
    virtual bool findConfigurationFile(const char* file, char* path, int max) = 0;

    /**
     * Return a read-only copy of the host configuration object.
     */
    virtual class HostConfigs* getHostConfigs() = 0;

    /**
     * Return a read-only copy of the current configuration object.
     * If you want to modify this, you must first clone it.
     */
	virtual class MobiusConfig* getConfiguration() = 0;

    /**
     * Return a writable copy of the current configuration object.
     */
	virtual class MobiusConfig* editConfiguration() = 0;

    /**
     * Apply changes to an external copy of the configuration object.
     * Normally you will call getConfiguration, then clone it, then 
     * change it, then finally call setConfiguration.  This interface
     * will assume that anything could have been changed and will completely
     * rebuild internal Mobius state.  
     */
	virtual void setFullConfiguration(class MobiusConfig* config) = 0;
    
    /**
     * Apply changes to an external copy of the configuration object
     * but not anything related to presets, setups, or bindings.
     */
	virtual void setGeneralConfiguration(class MobiusConfig* config) = 0;

    /**
     * Apply changes to an external copy of the configuration object
     * related to presets only.
     */
	virtual void setPresetConfiguration(class MobiusConfig* config) = 0;

    /**
     * Apply changes to an external copy of the configuration object
     * related to setups only.
     */
	virtual void setSetupConfiguration(class MobiusConfig* config) = 0;

    /**
     * Apply changes to an external copy of the configuration object
     * related to bindings only.
     */
	virtual void setBindingConfiguration(class MobiusConfig* config) = 0;

    /**
     * Reload the OSC configuration file after editing.
     * Temporary until we have a bidirectional editing interface.
     */
    virtual void reloadOscConfiguration() = 0;

    /**
     * Reload all scripts.
     */
    virtual void reloadScripts() = 0;

    //////////////////////////////////////////////////////////////////////
    //
    // Binding Targets
    //
    //////////////////////////////////////////////////////////////////////

    virtual class Function** getFunctions() = 0;
    virtual class Function* getFunction(const char* name) = 0;

    virtual class Parameter** getParameters() = 0;
    virtual class Parameter* getParameter(const char* name) = 0;
    virtual class Parameter* getParameterWithDisplayName(const char* name) = 0;

    virtual class MobiusMode** getModes() = 0;
    virtual class MobiusMode* getMode(const char* name) = 0;

    //////////////////////////////////////////////////////////////////////
    //
    // Bindings, Actions, and Exports
    //
    //////////////////////////////////////////////////////////////////////
    
    /**
     * Resolve a binding target.
     * The returned object will not be released until the MobiusInterface
     * object is deleted so application level code may retain references
     * to these.  They should not be modified or deleted.  Returning
     * null means the target was unresolved.  This is intended for use
     * by the binding dialogs to validate bindings.
     */
    virtual class ResolvedTarget* resolveTarget(class Binding* b) = 0;
    
    /**
     * Resolve a binding into an Action. The Action is owned by the caller.
     * Returning NULL means the binding was unresolved.
     */
    virtual Action* resolveAction(class Binding* b) = 0;

    /**
     * Allocate a new dynamic action.
     */
    virtual Action* newAction() = 0;

    /**
     * Resolve an export.
     */
    virtual class Export* resolveExport(class Binding* b) = 0;
    virtual class Export* resolveExport(class ResolvedTarget* rt) = 0;
    virtual class Export* resolveExport(class Action* a) = 0;
    
    /**
     * Special interface just for the standard UI that returns
     * Actions for every script that declare itself as a !button
     */
    virtual Action* getScriptButtonActions() = 0;

    /**
     * Clone an action for processing.
     */
    virtual class Action* cloneAction(class Action* src) = 0;

    /**
     * Execute an action.
     * Ownership of the object is taken and it will be deleted.
     * This should only be called on actions that have been cloned.
     */
    virtual void doAction(Action* a) = 0;

    /**
     * Process a MIDI event.
     * This is only used by MobiusPlugin to convert events from the host
     * into MidiEvents.  When dealing directly with MIDI devices, Mobius
     * will internally register itself as a MidiListener which bypass the
     * MobiusInterface.
     */
	virtual void doMidiEvent(class MidiEvent* e) = 0;

    /**
     * Process a key event.
     * Called by the UI when keys are pressed and released.
     * Mobius internally maintains a BindingResolver to quickly map
     * keys to previous build Actions.
     */
    virtual void doKeyEvent(int key, bool down, bool repeat) = 0;

    /**
     * Register a watch point listener.
     * If NULL is returned the name returned by the WatchPointListener
     * was invalid.  If a WatchPoint is returned it means the registration
     * was successful and the listener must not be deleted.  Mobius
     * owns the WatchPointListener and it will be deleted when Mobius
     * desctructs.  If you no longer need the listener, call 
     * WatchPointListener::remove() and it will be deactivated and
     * reclaimed on the next audio interrupt.
     */
    virtual class WatchPoint* addWatcher(class WatchPointListener* listener) = 0;

    //////////////////////////////////////////////////////////////////////
    //
    // Misc Control
    //
    //////////////////////////////////////////////////////////////////////

    /**
     * Called by the host plugin adapter to turn checking for a steady
     * audio stream on and off.  Turns off when the plugin is bypassed.
     */
    virtual void setCheckInterrupt(bool b) = 0;

    /**
     * Used by CalibrationDialog to start the calibration process and
     * display the results.  Think about a better interface for this!!
     * We're currently duplicating RecorderCalibrationResult so UI doesn't
     * have to know about Recorder.h.
     */
	virtual CalibrationResult* calibrateLatency() = 0;

    /**
     * Called by the UI when it is done processing a prompt.
     * Ownership of the Prompt passes to Mobius.
     */
	virtual void finishPrompt(Prompt* p) = 0;

    //////////////////////////////////////////////////////////////////////
    //
    // Status
    //
    //////////////////////////////////////////////////////////////////////

    /**
     * Return the message catalog that may have things for the UI 
     * as well as the engine.
     * !! Should break this in two, with one catalog for the engine
     * and another for the user interfaces?
     */
	virtual class MessageCatalog* getMessageCatalog() = 0;

    /**
     * Return an object holding the state of the requested track.
     * The returned object is still owned by Mobius and must not be freed.
     */
    virtual class MobiusState* getState(int track) = 0;

    /**
     * Return an object holding information about problems the engine
     * is having.
     * The returned object is still owned by Mobius and must not be freed.
     */
    virtual class MobiusAlerts* getAlerts() = 0;

    // The interaction between Mobius, AudioInterface, AudioStream
    // and Recorder needs work!

    /**
     * Get the input latency reported by the configured input device.
     * Used by AudioDialog.
     * !! This should be part of AudioInterface?
     */
	virtual int getReportedInputLatency() = 0;

    /**
     * Get the input latency reported by the configured output device.
     * Used by AudioDialog.
     * !! This should be part of AudioInterface?
     */
	virtual int getReportedOutputLatency() = 0;

    /**
     * This is either the latency override from MobiusConfig or
     * if that isn't set the reported latency.
     * !! Move to AudioInterface?
     * Used by AudioDialog for calibration.
     * Used by SampleTrack to initialize the sample players.
     */
	virtual int getEffectiveInputLatency() = 0;

    /**
     * This is either the latency override from MobiusConfig or
     * if that isn't set the reported latency.
     * !! Move to AudioInterface?
     * Used by AudioDialog for calibration.
     * Used by SampleTrack to initialize the sample players.
     */
	virtual int getEffectiveOutputLatency() = 0;

    /**
     * Return the number of tracks.
     */
    virtual int getTrackCount() = 0;

    /**
     * Get the index of the currently active track.
     * The first track has index zero.
     */
    virtual int getActiveTrack() = 0;

    /**
     * Return the number of the preset that the active track is actually using.
     * This may be different than the one that is selected in  the MobiusConfig.
     */
    virtual int getTrackPreset() = 0;

    //////////////////////////////////////////////////////////////////////
    // 
    // Save/Load
    //
    //////////////////////////////////////////////////////////////////////
       
    /**
     * Return the AudioPool for use in creating Audio and Projects objects.
     * I would rather this not be here but then we'll need interfaces
     * that take path names.
     */
    virtual class AudioPool* getAudioPool() = 0;

    /**
     * Set the contents of the active loop in the active track.
     * Ownership of the Audio object is taken.
     * If the loop is not empty it will be reset first.
     * !! Should have more control over the track/loop number?
     */
	virtual void loadLoop(class Audio* a) = 0;

    /**
     * Load a project.
     * Ownership of the Project object is taken.
     */
	virtual void loadProject(class Project* a) = 0;

    /**
     * Return the current Mobius state as a project.
     */
	virtual Project* saveProject() = 0;

    /**
     * Used by the UI to implement the Quick Save and Save Loop menu items.
     * Name is optional and will default to the "quick save" path.
     */
	virtual void saveLoop(const char* name) = 0;

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
