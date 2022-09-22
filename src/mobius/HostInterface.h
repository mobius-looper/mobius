/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * An abstract interface that hides the implementation of 
 * an audio plugin from the host application.  This was designed
 * initially so that we could compile AudioUnit glue without having
 * to combine CoreAudio and Mobius headers which had some conlicts:
 * Component, AudioBuffer, EventType, Move, etc.
 *
 * These could have been addressed with namespaces but the abstraction
 * can also be used for VST hosts as well letting us share a little more
 * code.
 *
 * This was designed for Mobius but can be used with any plugin.  Think
 * about factoring this out into a standalone AU/VST framework.
 *
 * This is closely related to AudioInterface and AudioStream from
 * the "audio" package.  Consider merging?
 *
 * !! Yes, at least the constants for max channels and buffer frames.
 */

#ifndef HOST_INTERFACE
#define HOST_INTERFACE

//////////////////////////////////////////////////////////////////////
//
// Constants
//
//////////////////////////////////////////////////////////////////////

/**
 * Maximum number of channels we will allow in the AU/VST callback.
 * Fixed at 2 (stereo) for now, eventually allow 5 or more for surround.
 */
#define MAX_HOST_BUFFER_CHANNELS 2

/**
 * Maximum number of frames we'll allow in the AU/VST callback.
 * Determines the sizes of the interleaved frame buffers.  
 * 
 * auval uses up to 4096, this causes a segfault!
 */
#define MAX_HOST_BUFFER_FRAMES 4096

/**
 * Maximum number of "ports" supported by the plugin.  
 * Each port is currently a pair of stereo channels.
 * !! Need more flexibility in port definition.
 *
 * VST doesn't use this, we have historically used 8.
 */
#define MAX_HOST_PLUGIN_PORTS 16

//////////////////////////////////////////////////////////////////////
//
// HostSyncState
//
//////////////////////////////////////////////////////////////////////

/**
 * A generic representation of host synchronization state.
 * Besides maintaining sync state, this is also where we implement
 * the beat detction algorithm since it is the same for AU and VST.
 *
 * Much of what is in here is the same as AudioTime but we keep 
 * extra state that we don't want to expose to the plugin.
 */
class HostSyncState {

  public:

    HostSyncState();
    ~HostSyncState();

	/**
	 * Adjust for optional host options.
	 */
	void setHost(class HostConfigs* config);
	void setHostRewindsOnResume(bool b);

    /**
     * Update state related to host tempo and time signature.
     */
    void updateTempo(int sampleRate, double tempo, 
                     int timeSigNumerator, int timeSigDenomonator);

    /**
     * Update audio stream state.
     */
    void advance(int frames, double samplePosition, double beatPosition,
                 bool transportChanged, bool transportPlaying);

    /**
     * Transfer our internal state into an AudioTime for the plugin.
     */
    void transfer(class AudioTime* autime);

  private:

    void updateTransport(double samplePosition, double beatPosition,
                         bool transportChanged, bool transportPlaying);


	/**
	 * True to enable general state change trace.
	 */
	bool mTraceChanges;

    /**
     * True to enable beat trace.
     */
    bool mTraceBeats;

    //
    // things copied from HostConfig
    //

    /**
     * When true it means that the host transport rewinds a bit after a resume.
     * This was noticed in an old version of Cubase...
     *
     * "Hmm, Cubase as usual throws a wrench into this.  Because of it's odd
     * pre-roll, ppqPos can actually go negative briefly when starting from
     * zero. But it is -0.xxxxx which when you truncate is just 0 so we can't
     * tell when the beat changes given the lastBeat formula above."
     *
     * When this is set it tries to compensate for this pre-roll, not sure
     * if modern versions of Cubase do this.
     */
    bool mHostRewindsOnResume;

    /**
     * When true, we check for stop/play by monitoring the ppqPos
     * rather than expecting kVstTransportChanged events.
     * This was originally added for Usine around 2006, not sure if
     * it's still necessary.
     */
    bool mHostPpqPosTransport;

    /**
     * When true we check for stop/play by monitoring the samplePos.
     * rather than expecting kVstTransportChanged events.
     * This was added a long time ago and hasn't been enabled for several
     * releases.
     */
    bool mHostSamplePosTransport;

    //
    // Things passed to updateTempo()
    //

    /**
     * The current sample rate reported by the host.  This is not
     * expected to change though we track it.
     */
    int mSampleRate;
    
    /** 
     * The current tempo reported by the host.  This is expected to change.
     */
    double mTempo;

    /**
     * The current time signagure reported by the host.
     */
    int mTimeSigNumerator;
    int mTimeSigDenominator;

    // 
    // Things derived from updateTempo()
    //

    /**
     * The fraction of a beat represented by one frame.
     * Typically a very small number.  This is used in the conversion
     * of mBeatPosition into a buffer offset.
     */
    double mBeatsPerFrame;

    /**
     * Calculated from TimeSigNumerator and TimeSigDenominator.
     *
     *   bpb = mTimeSigNumerator / (mTimeSigDenominator / 4);
     * 
     * What is this doing!?  
     */
    double mBeatsPerBar;

    //
    // Things passed to adavnce()
    //

    /**
     * True if the transport is currently playing.
     */
    bool mPlaying;

    /**
     * The sample position of the last buffer.  This is normally
     * advances by the buffer size with zero being the start of the
     * host's timeline.
     */
    double mLastSamplePosition;

    /**
     * The beat position of the last buffer.  The integer portion
     * of this number is the current beat number in the host transport.
     * The fractional portion represents the distance to the next beat
     * boundary.  In VST this is ppqPos, in AU this is currentBeat.
     */
    double mLastBeatPosition;

    //
    // State derived from advance()
    //

    /**
     * Becomes true if the transport was resumed in the current buffer.
     */
    bool mResumed;

    /**
     * Becomes true if the transport was stopped in the current buffer.
     */
    bool mStopped;

    /**
     * Kludge for Cubase that likes to rewind AFTER 
     * the transport status changes to play.  Set if we see the transport
     * change and mRewindsOnResume is set.
     */
    bool mAwaitingRewind;

    /**
     * The beat range calculated on the last buffer.
     * Not actually used but could be to detect some obscure
     * edge conditions when the transport is jumping around.
     */
    double mLastBeatRange;

    /**
     * Becomes true if there is a beat within the current buffer.
     */
    bool mBeatBoundary;

    /**
     * Becomes true if there is a bar within the current buffer.
     * mBeatBoundary will also be true.
     */
    bool mBarBoundary;

    /**
     * The offset into the buffer of the beat/bar.
     */
    int mBeatOffset;

    /**
     * The last integer beat we detected.
     */
    int mLastBeat;

    /**
     * The beat count relative to the start of the bar.
     * The downbeat of the bar is beat zero.
     */
    int mBeatCount;

    /**
     * The number of buffers since the last one with a 
     * beat boundary.  Used to suppress beats that come in 
     * too quickly when the host transport isn't implemented properly.
     * This was for an old Usine bug.
     */
    int mBeatDecay;

};

//////////////////////////////////////////////////////////////////////
//
// HostInterface
//
//////////////////////////////////////////////////////////////////////

/**
 * An interface defining services provided by the host application
 * to be called from the plugin.
 */
class HostInterface {

  public:

	virtual ~HostInterface() {}

	/**
	 * Get an application context.
	 */
	virtual class Context* getContext() = 0;
	
	/**
	 * Get the product name of the host if possible.	
	 * Should be used in rare occasions where we conditionalize things
	 * based on the host.
	 */
	virtual const char* getHostName() = 0;
	virtual const char* getHostVersion() = 0;

	/**
	 * Get an implementation of AudioInterface (and more important
	 * AudioStream) that hides the host details.
	 * This isn't the best fit for plugin hosts, there's a bunch
	 * of stuff we don't need, consider refactoring AudioInterface someday.
	 */
	virtual class AudioInterface* getAudioInterface() = 0;

	/**
	 * Just a stub for now, will need to think more about this.
	 */
	virtual void notifyParameter(int id, float value) = 0;
};

//////////////////////////////////////////////////////////////////////
//
// PluginParameter
//
//////////////////////////////////////////////////////////////////////

/**
 * Types of plugin parameters.
 */
typedef enum {

	/**
	 * A range from values from min/max suitable for
	 * control with a slider.
	 */
	PluginParameterContinuous,

	/**
	 * A fixed set of values suitable for selection
	 * with a menu.
	 */
	PluginParameterEnumeration,

	/**
	 * A boolean value suitable for selection with 
	 * a checkbox.
	 */
	PluginParameterBoolean,

	/**
	 * A function.  This may be exposed as a boolean or something
	 * else more suitable for momentary buttons.
	 */
	PluginParameterButton

} PluginParameterType;

/**
 * The definition of a parameter supported by a plugin.
 * Also serves as an interface to get/set the current value.
 *
 * Could make this totally pure, but we're going to have
 * some method implementations so we may as well define
 * holders for the common properties.
 */
class PluginParameter {
  public:

	PluginParameter();
	virtual ~PluginParameter();

	PluginParameter* getNext();
	void setNext(class PluginParameter* next);

	int getId();
	void setId(int i);
	
	const char* getName();
	void setName(const char* name);

	PluginParameterType getType();
	void setType(PluginParameterType t);

	float getMinimum();
	void setMinimum(float f);

	float getMaximum();
	void setMaximum(float f);

	float getDefault();
	void setDefault(float f);

	float getLast();
	void setLast(float f);

	/**
	 * Since we're going to be subclassing these all the time,
	 * let the subclass handle this.  We could do this for most
	 * if not all the other properties so this could be
	 * a pure interface?
	 */
	virtual const char** getValueLabels() = 0;

	/**
	 * Update the plugin parameter value if we notice a change
	 * since the last time it was updated.  An optimization so the 
	 * plugin doesn't have to filter out a bunch of redundant 
	 * parameter events on every render cycle.
	 */
	bool setValueIfChanged(float f);

	/**
	 * Return true if the value currently stored in mLast is different
	 * than the current value of the parameter from the plugin, and
	 * store the current value in mLast.
	 */
	bool refreshValue();

    /**
     * Utility for VST to get the value as a string if it's an enumeration.
     */
    void getValueString(float value, char* buffer, int max);
    void setValueString(const char* value);

  protected:

	/**
	 * Get the current value of this parameter inside the
	 * encapsulated plugin.  Host interfaces should always call getLast().
	 */
	virtual float getValueInternal() = 0;

	/**
	 * Set the current value of this parameter.  Note that
     * for some parameters this not be immediately reflected
     * by getValue().  If the host asks for the current value
     * it should always call getLastValue() which will be refreshed
     * at the end of the audio cycle.
     *
     * Host interfaces should always call setValueIfChanged().
	 */
	virtual void setValueInternal(float f) = 0;

	/**
	 * Maintained on a list.
	 * Not used any more, these are always in an array indexed
	 * by id for fast lookup.
	 */
	PluginParameter* mNext;

	/**
	 * Cannonical numeric id of this parameter.
	 * Since these can be stored by the host in "presets" you
	 * should try not to let these change.
	 */
	int mId;

	/**
	 * Symbolic name of the parameter.
	 * VST has both a "label" and a "shortLabel".
	 */
	char* mName;

	/**
	 * Fundamental type of the parameter.
	 */
	PluginParameterType mType;

	/**
	 * Minimum value.
	 * Though we represent these with floats they will
	 * usually be integers.
	 */
	float mMinimum;

	/**
	 * Maximum value.
	 */
	float mMaximum;

	/**
	 * Default value.
	 */
	float mDefault;

	/**
	 * The last value synchronized between the host and the plugin.
	 * 
	 * At the beginning of a render cycle, this is the current state
	 * of the plugin, and should be the last value given to the host
	 * through a notification on the previous render cycle.  If the
	 * value from the host is different than this value, then the
	 * plugin should be notified of the change.  This is how the
	 * plugin tracks changes made in the AU view.
	 *
	 * At the end of a render cycle, this value is compared to the
	 * current value managed internally by the plugin and if it differs
	 * a change notification is sent to the host.  This is how the AU View
	 * tracks changes made internally by the plugin, possibly by another UI.
	 */
	float mLast;

    /**
     * Set at the start of the render cycle if the value was changed
     * by the host.  For some hosts it is important that we
     * immediately echo the value they set, so the usual
     * comparison for current != mLast doesn't work.
     */
    bool mChanged;

};

//////////////////////////////////////////////////////////////////////
//
// PluginInterface
//
//////////////////////////////////////////////////////////////////////

/**
 * An interface to be implemened by the audio plugin called
 * from the HostInterface.
 *
 * These are instantiated by an implementation of HostInterface
 * using the newPlugin static factory method that must be overloaded
 * appropriately.  Note that this means there is only one plugin 
 * type per platform, need to revisit this for OSX so we have
 * both VST and AU!!
 *
 * The plugin is expected to get the AudioStream from the 
 * HostInterface and install an AudioHandler.
 * 
 */
class PluginInterface {

  public:

	PluginInterface() {}
	virtual ~PluginInterface() {}

	/**
	 * Instantiate the plugin.  This must be implemented in a file
	 * specific to the plugin and return a subclass of PluginInterface.
	 */
	static PluginInterface* newPlugin(HostInterface* host);

    /**
     * Return host configuration managed by the plugin.
     * This is needed for a few options the plugin wrapper may need
     * but the plugin itself doesn't.  Sort of strange, but I
     * don't want HostInterface implementations to have to 
     * manage their own config storage.
     */
    virtual class HostConfigs* getHostConfigs() = 0;

	/**
	 * Return the number of ports supported by this plugin.
	 * Currently these are assumed to pairs of stereo channels,
	 * and there will be an equal number of inputs and outputs.
	 */
	virtual int getPluginPorts() = 0;

	/**
	 * Perform the expensive initialization.
	 */
	virtual void start() = 0;

    /**
     * Called when the host knows that buffers will be comming in.
     */
    virtual void resume() = 0;

    /**
     * Called when the host knows that buffers are stopping.
     */
    virtual void suspend() = 0;
	
	/**
	 * Handle a MIDI event.
	 */ 
	virtual void midiEvent(int status, int channel, int data1, int data2, long frame) = 0;

    /**
     * Return the MIDI events to send in this cycle.
     */
    virtual class MidiEvent* getMidiEvents() = 0;

    /**
     * Get the preferred size of the window (VST only).
     */
    virtual void getWindowRect(int* left, int* top, int* width, int* height) = 0;

	/**
	 * Open the editor window.
	 */
	virtual void openWindow(void* window, void* pane) = 0;

	/**
	 * Close the editor window.
	 */
	virtual void closeWindow() = 0;

	/**
	 * Get parameter definitions.
	 */
	virtual PluginParameter* getParameters() = 0;
	virtual PluginParameter* getParameter(int id) = 0;
	
};

#endif
/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
