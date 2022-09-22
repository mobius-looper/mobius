/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 *
 * MOBIUS PUBLIC INTERFACE
 *
 * An abstract interface for audio devices and services.
 * 
 */


#ifndef AUDIO_INTERFACE_H
#define AUDIO_INTERFACE_H

#include <stdio.h>

// OSX has AudioDevice so have to use namespaces
// actually we don't need these, AudioDevice is really
// an abstraction
#ifdef OSX_NO
  #define AUDIO_BEGIN_NAMESPACE namespace Mobius {
  #define AUDIO_END_NAMESPACE }
  #define AUDIO_USE_NAMESPACE using namespace Mobius;
#else
  #define AUDIO_BEGIN_NAMESPACE
  #define AUDIO_END_NAMESPACE
  #define AUDIO_USE_NAMESPACE
#endif

AUDIO_BEGIN_NAMESPACE

/****************************************************************************
 *                                                                          *
 *   							  CONSTANTS                                 *
 *                                                                          *
 ****************************************************************************/

/**
 * The preferred number of frames in an audio interface buffer.
 * This is what we will request of PortAudio, and usually get, but
 * you should not assume this will be the case.  When using the VST
 * interface, the size is under control of the VST driver and it will
 * often be 512, but usually not higher.
 */
#define AUDIO_FRAMES_PER_BUFFER	(256)

/**
 * The maximum number of frames for an audio interface buffer.
 * This should as large as the higest expected ASIO buffer size.
 * 1024 is probably enough, but be safe.  
 * UPDATE: auval uses up to 4096, be consistent with 
 * MAX_HOST_BUFFER_FRAMES in HostInterface.h
 * !! don't like the duplication, move HostInterface over here?
 */
#define AUDIO_MAX_FRAMES_PER_BUFFER	(4096)

/**
 * The maximum number of channels we will have to deal with.
 * Since this is used for working buffer sizing, don't raise this until
 * we're ready to deal with surround channels everywhere.
 */
#define AUDIO_MAX_CHANNELS 2 


/**
 * The maximum size for an intermediate buffer used for transformation
 * of an audio interrupt buffer.  Used to pre-allocate buffers that
 * will always be large enough.
 */
#define AUDIO_MAX_SAMPLES_PER_BUFFER AUDIO_MAX_FRAMES_PER_BUFFER * AUDIO_MAX_CHANNELS

/**
 * Maximum number of ports (assumed to be stereo) we support
 * for a given audio device.
 */
#define AUDIO_MAX_PORTS 16

#define CD_SAMPLE_RATE 		(44100)

/**
 * Debugging kludge, set to disable catching stray exceptions in the
 * interrupt handler. Implementation dependent whether this works.
 */
extern bool AudioInterfaceCatchExceptions;

/****************************************************************************
 *                                                                          *
 *   								DEVICE                                  *
 *                                                                          *
 ****************************************************************************/

typedef enum {

    API_UNKNOWN,
    API_MME,
    API_DIRECT_SOUND,
    API_ASIO,
	API_CORE_AUDIO

} AudioApi;

/**
 * Information about an audio device.  Only really care about the names
 * right now, we'll always assume we can use 44100 sample rate.
 */
class AudioDevice {

  public:

	AudioDevice();
	~AudioDevice();

    AudioApi getApi() {
        return mApi;
    }

    void setApi(AudioApi api) {
        mApi = api;
    }

	void setName(const char *name);

	const char *getName() {
		return mName;
	}

	int getId() {
		return mId;
	}
	
	void setId(int i) {
		mId = i;
	}

    const char* getApiName() {
        const char* name = "unknown";
        switch (mApi) {
            case API_MME: name = "MME"; break;
            case API_DIRECT_SOUND: name = "Direct Sound"; break;
            case API_ASIO: name = "ASIO"; break;
            case API_CORE_AUDIO: name = "Core Audio"; break;
			// xcode 5 whines if we don't have this for API_UNKNOW
			default: name="unknown"; break;
        }
        return name;
    }

	void setDefaultInput(bool b) {
		mDefaultInput = b;
	}

	bool isDefaultInput() {
		return mDefaultInput;
	}

	void setDefaultOutput(bool b) {
		mDefaultOutput = b;
	}

	bool isDefaultOutput() {
		return mDefaultOutput;
	}

	bool isInput() {
		return (mInputChannels > 0 || mApi == API_ASIO);
	}

	bool isOutput() {
		return (mOutputChannels > 0 || mApi == API_ASIO);
	}

	int getInputChannels() {
		return mInputChannels;
	}

	void setInputChannels(int i) {
		mInputChannels = i;
	}

	int getOutputChannels() {
		return mOutputChannels;
	}

	void setOutputChannels(int i) {
		mOutputChannels = i;
	}

	float getDefaultSampleRate() {
		return mDefaultSampleRate;
	}

	void setDefaultSampleRate(float f) {
		mDefaultSampleRate = f;
	}

  protected:

    AudioApi mApi;
	int mId;
	char* mName;
	int mInputChannels;
	int mOutputChannels;
	bool mDefaultInput;
	bool mDefaultOutput;
	float mDefaultSampleRate;
};

/****************************************************************************
 *                                                                          *
 *   							   HANDLER                                  *
 *                                                                          *
 ****************************************************************************/

/**
 * The interface of an object that may be registered with a Stream
 * to receive audio interrupts.  The handler is expected to call
 * back to the stream to retrieve the input and output buffers for
 * each of the ports supported by the stream.
 */
class AudioHandler {

  public:

	virtual void processAudioBuffers(class AudioStream* stream) = 0;

};

/****************************************************************************
 *                                                                          *
 *   								 TIME                                   *
 *                                                                          *
 ****************************************************************************/

/**
 * VST and AU streams can also include synchronization info.
 * I don't really like having this in AudioInterface, but the things
 * that need AudioTime are currently given only an AudioStream and we would
 * have to retool several interfaces so Mobius could get an AudioTime without
 * knowing that it is a plugin.
 *
 * Since the point of all this is to accurately sync with the audio stream, 
 * it feels best to  encapsulate it here.
 *
 * This is the same data in the VstTimeInfo, plus some analysis.
 */
class AudioTime {

  public:

	/**
	 * Host tempo.
	 */
	double tempo;

	/**
	 * The "beat position" of the current audio buffer.
     * 
	 * For VST hosts, this is VstTimeInfo.ppqPos.
	 * It starts at 0.0 and increments by a fraction according
	 * to the tempo.  When it crosses a beat boundary the integrer
     * part is incremented.
     *
     * For AU host the currentBeat returned by CallHostBeatAndTempo
     * works the same way.
	 */
	double beatPosition;

	/**
	 * True if the host transport is "playing".
	 */
	bool playing;

	/**
	 * True if there is a beat boundary in this buffer.
	 */
	bool beatBoundary;

	/**
	 * True if there is a bar boundary in this buffer.
	 */
	bool barBoundary;

	/**
	 * Frame offset to the beat/bar boundary in this buffer.
	 */
	long boundaryOffset;

	/**
	 * Current beat.
	 */
	int beat;

	/**
	 * Current bar.
	 */
	//int bar;

    /**
     * Number of beats in one bar.  If zero it is undefined, beat should
     * increment without wrapping and bar should stay zero.
     */
    int beatsPerBar;

    // TODO: also capture host time signture if we can
    // may need some flags to say if it is reliable

	void init() {
		tempo = 0.0;
		beatPosition = -1.0;
		playing = false;
		beatBoundary = false;
		barBoundary = false;
		boundaryOffset = 0;
		beat = 0;
//		bar = 0;
        beatsPerBar = 0;
	}

};

/****************************************************************************
 *                                                                          *
 *                                    PORT                                  *
 *                                                                          *
 ****************************************************************************/

/**
 * The channels in an AudioDevice can be arranged into ports.
 * Currently we require that ports always have 2 channels, eventuallyu
 * need a more flexible way to define ports.
 *
 * This serves both as a way to define the characteristics of a port
 * for the interface, and also some internal buffer interleaving utilities
 * for the engine.
 */
class AudioPort {

  public:

    AudioPort();
    ~AudioPort();

    void setNumber(int i);
    int getNumber();

    void setChannels(int i);
    int getChannels();

    void setFrameOffset(int i);

	void reset();
	float* extract(float* src, long frames, int channels);
	float* prepare(long frames);
	void transfer(float* dest, long frames, int channels);

  protected:

    /**
     * The number of this port.
     */
    int mNumber;

    /**
     * The number of channels in this port.
     * Currently this should always be 2.
     */
    int mChannels;

    /**
     * The offset within the the device buffer to the start
     * of this port's channels.  
     * Currently this should be port number * 2 since we only
     * have stereo ports.
     */
    int mFrameOffset;

	/**
	 * Set true once mBuffer has been prepared.
	 */
	bool mPrepared;

	/**
	 * The buffer with the extracted frames for one port.
	 */
	float* mBuffer;

};

/****************************************************************************
 *                                                                          *
 *   								STREAM                                  *
 *                                                                          *
 ****************************************************************************/

/**
 * An object representing one bi-directional audio stream.
 * These will be returned by AudioInterface.
 *
 * The stream parameters are normally set before you call open().
 * In theory they can be changed on the fly, but not all implementations
 * may support this.  
 *
 * The stream must be closed to release system resources.
 *
 * This is also used to represent a stream of IO buffers managed
 * by an AU or VST plugin host.  In that case, the device management
 * methods are ignored, it is only used for getInterruptFrames,
 * getInterruptBuffers, getTime, and getSampleRate.
 *
 */
class AudioStream {

  public:

	virtual class AudioInterface* getInterface() = 0;

	virtual ~AudioStream() {}

	virtual bool setInputDevice(int id) = 0;
	virtual bool setInputDevice(const char* name) = 0;
	virtual AudioDevice* getInputDevice() = 0;

	virtual bool setOutputDevice(int id) = 0;
	virtual bool setOutputDevice(const char* name) = 0;
	virtual AudioDevice* getOutputDevice() = 0;

	// NOTE: Currently assuming a stream may have several "ports"
	// each having 2 channels.
	// Need a more flexible port allocation interface.

	virtual int getInputChannels() = 0;
    virtual int getInputPorts() = 0;
    //virtual AudioPort* getInputPort(int p) = 0;

	virtual int getOutputChannels() = 0;
    virtual int getOutputPorts() = 0;
    //virtual AudioPort* getOutputPort(int p) = 0;

	virtual void setSampleRate(int i) = 0;
	virtual int getSampleRate() = 0;

	virtual void setHandler(AudioHandler* h) = 0;

	virtual bool open() = 0;
	virtual void close() = 0;

	virtual const char* getLastError() = 0;

	virtual void setSuggestedLatencyMsec(int i) = 0;
    virtual int getInputLatencyFrames() = 0;
    virtual void setInputLatencyFrames(int frames) = 0;
    virtual int getOutputLatencyFrames() = 0;
    virtual void setOutputLatencyFrames(int frames) = 0;

	virtual void printStatistics() = 0;

    // Stream time info, may be called outside the interrupt
    // to synchronize trigger events
    virtual double getStreamTime() = 0;
    virtual double getLastInterruptStreamTime() = 0;

	// these are called by the AudioHandler during an interrupt

	virtual long getInterruptFrames() = 0;
	virtual void getInterruptBuffers(int inport, float** input, 
									 int outport, float** output) = 0;
	
	virtual AudioTime* getTime() = 0;

};

/**
 * Skeleton implementation of AudioStream.
 */
class AbstractAudioStream : public AudioStream {

  public:

	AbstractAudioStream();
	virtual ~AbstractAudioStream();

	void setInterface(AudioInterface* ai);
	AudioInterface* getInterface();

	bool setInputDevice(int id);
	bool setInputDevice(const char* name);
	AudioDevice* getInputDevice();

	bool setOutputDevice(int id);
	bool setOutputDevice(const char* name);
	AudioDevice* getOutputDevice();

	int getInputChannels();
    int getInputPorts();
	int getOutputChannels();
    int getOutputPorts();

	void setSampleRate(int i);
	int getSampleRate();

	void setHandler(AudioHandler* h);
	AudioHandler* getHandler();

	// these are still pure virtual
	//bool open();
	//void close();

	const char* getLastError();
	void setSuggestedLatencyMsec(int msec);
    int getInputLatencyFrames();
    void setInputLatencyFrames(int frames);
    int getOutputLatencyFrames();
    void setOutputLatencyFrames(int frames);
	void printStatistics();

  protected:

    int calcLatency(double ltime);
    void setInputChannels(int i);
    void setOutputChannels(int i);

	AudioInterface* mInterface;
	AudioHandler* mHandler;
	void* mStream;
	bool mTraceDropouts;

	int mInputDevice;		// current input device
	int mOutputDevice;		// current output device
	int mInputChannels;		// channels supported by input device
	int mOutputChannels;	// channels supported by output device
	int mSampleRate;		// current sample rate
	int mSuggestedLatency;
    int mInputLatency;
    int mOutputLatency;
	bool mStreamStarted;
	char mError[256];

	long mInputUnderflows;
	long mInputOverflows;
	long mOutputUnderflows;
	long mOutputOverflows;
	long mInterrupts;
	double mAverageLatency;

	// transient state for the AudioHandler callback

	float* mInput;
	float* mOutput;
	long mFrames;

	AudioPort mInputs[AUDIO_MAX_PORTS];
    int mInputPorts;

	AudioPort mOutputs[AUDIO_MAX_PORTS];
    int mOutputPorts;

	// performance monitoring
	//Timer* mTimer;
	//long mLastMilli;

};

/****************************************************************************
 *                                                                          *
 *   							  INTERFACE                                 *
 *                                                                          *
 ****************************************************************************/

/**
 * An interface that provides access to the audio devices.
 * A simplification of portaudio.  Added to ease the migration
 * from v18 to v19 but it's nice to have the dependencies
 * encapsulated anyway.
 */
class AudioInterface {

  public:

	/** 
	 * Return an implementation of the interface.
	 */
	static AudioInterface* getInterface();

	/**
	 * Global interface flush.
	 * Should be used only when the progam is exiting, useful to 
	 * reduce memory leak clutter.
	 */
	static void exit();
	
	/**
	 * Have to have one of these to get the subclass destructor to run.
	 */
	virtual ~AudioInterface(){}

	/**
	 * Called when the application closes to release any
	 * resources held by the interface.
	 */
	virtual void terminate() = 0;

	/**
	 * Return information about available audio devices.
	 * The array is NULL terminated.
	 */
	virtual AudioDevice** getDevices() = 0;
	
	/**
	 * Return information about an audio device by id.
	 */
	virtual AudioDevice* getDevice(int id) = 0;

	/**
	 * Return information about an audio device by name.
	 */
	virtual AudioDevice* getDevice(const char* name, bool output) = 0;

	/**
	 * Print debug information about available devices to the console.
	 */
	virtual void printDevices() = 0;

	/**
	 * Create a bi-directional audio stream.
	 */
	virtual AudioStream* getStream() = 0;

  protected:

	static AudioInterface* Interface;
};

/**
 * Skeleton implementation of AudioInterface.
 */
class AbstractAudioInterface : public AudioInterface {
  public:

	AbstractAudioInterface();
	~AbstractAudioInterface();
	
	AudioDevice* getDevice(int id);
	AudioDevice* getDevice(const char* name, bool output);
	void printDevices();

	// these are still pure virtual
	//AudioDevice** getDevices();
	//AudioStream* getStream();
	//void terminate();

  protected:

	AudioDevice** mDevices;
	int mDeviceCount;
};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
AUDIO_END_NAMESPACE
#endif
