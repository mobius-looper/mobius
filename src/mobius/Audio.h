/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 */

#ifndef AUDIO_H
#define AUDIO_H

/****************************************************************************
 *                                                                          *
 *   							  CONSTANTS                                 *
 *                                                                          *
 ****************************************************************************/

/**
 * Maximum number of frames that may be used for cross fading.
 */
#define AUDIO_MAX_FADE_FRAMES 256

/**
 * Minimum number of frames that may be used for cross fading.
 * Formerly we allowed zero here, but it's easy for bad config files
 * to leave this out or set it to zero since zero usually means "default".
 * 
 */
#define AUDIO_MIN_FADE_FRAMES 16

/**
 * Default number of frames to use during fade in/out
 * of a newly recorded segment.  At a sample rate of 44100, 
 * a value of 441 results in a 1/10 second fade.  But
 * we don't really need that much.    If the range is too large
 * you hear "breathing".
 *
 * UDPATE: Since we use static buffers for this and the max is only 256, it
 * isn't really possible to set large values for effect.  While fade frames
 * can be set in a global parameter this is no longer exposed in the user interface.
 */
#define AUDIO_DEFAULT_FADE_FRAMES 128

/****************************************************************************
 *                                                                          *
 *   							  UTILITIES                                 *
 *                                                                          *
 ****************************************************************************/

extern short SampleFloatToInt16(float sample);

/****************************************************************************
 *                                                                          *
 *   							 AUDIO BUFFER                               *
 *                                                                          *
 ****************************************************************************/

/**
 * Encapsulates a set of values that describe an audio buffer
 * used for transfer into and out of an Audio object.
 */
class AudioBuffer {

  public:

	AudioBuffer();
	virtual ~AudioBuffer();

	void initAudioBuffer();

	void setBuffer(float* b, long f);

	/**
	 * The buffer of samples.
	 */
	float* buffer;

	/**
	 * The number of relevent frames.  For transfers into the Audio,
	 * this is the number of frames in the buffer.  For transfers
	 * out, this is the number of frames to put into the buffer.
	 */
	long frames;

	/**
	 * The number of channels.  This times frames is the number of
	 * samples that will be transfered through the buffer.
	 */
	int channels;

};

/****************************************************************************
 *                                                                          *
 *                                    FADE                                  *
 *                                                                          *
 ****************************************************************************/

/**
 * Encapsulates state related to a fade.
 *
 * This includes a static array of fade adjustment values.  
 * It is safe to share this with multiple plugins.
 */
class AudioFade {

  public:

	/**
	 * Static utility method to apply an immediate fade to a buffer.
	 */
	static void fade(float* buffer, int channels, long startFrame, 
					 long frames, long fadeOffset, bool up);

	static void fade(float* buffer, int channels, long startFrame, 
					 long frames, long fadeOffset, bool up,
					 float adjust);

	static void fadePartial(float* buffer, int channels, long startFrame, 
							long frames, long fadeOffset, bool up,
							float adjust);

	static float SmoothingInterval;

	static int getRange();
	static void setRange(int range);
	static float* getRamp();
	static float* getRamp128();
	static float getRampValue(int level);

    AudioFade();
    ~AudioFade();

	void init();
	void setBaseLevel(float level);
	void enable(long frame, bool direction);
	void activate(int offset, bool direction);
	void activate(bool direction);
	void copy(AudioFade* src);

	float fade(float sample);
	void inc(long frame, bool reverse);
	void fade(AudioBuffer* buf, long curFrame);

    /**
     * True to enable the fade.
     */
	bool enabled;

	/**
	 * True when we've reached the start point of the fade.
	 */
	bool active;

    /**
     * The frame to begin the fade.
     */
    long startFrame;

    /**
     * The direction of the fade.
     */
	bool up;

    /**
     * The number of frames processed so far.
     */
    int processed;

	/**
	 * The base level of the fade, usually 1.0 for a full fade.
	 * For up fades, this is the initial multiplier for the first frame
	 * and we fade up to 1.0.  For down fades, this is the ending multipler
	 * for the last frame, and we fade down from 1.0.
	 */
	float baseLevel;

  private:

	/**
	 * Number of frames over which to perform the fade.
	 * This is configurable, but once set is always the same so maintain
	 * it in a static to avoid having to pass it around.
	 */
	static int Range;

	/**
	 * Precomputed fade ramp values.  Range defines the size.
	 */
	static float Ramp[];

	/**
	 * Precomputed fade ramp values for a fixed range of 128.
	 * Used when applying level adjustments specified by MIDI 
	 * continuous controllers with a range of 128.
	 */
	static float Ramp128[];

	/**
	 * Since Ramp is a statically allocated array, have to keep
	 * a flag to indicate that it has been initialized.
	 */
	static bool RampInitialized;
	static bool Ramp128Initialized;

	static void initRamp(float* ramp, int range);

	void saveFadeAudio(class Audio* a, const char* type);
};

/****************************************************************************
 *                                                                          *
 *                                   CURSOR                                 *
 *                                                                          *
 ****************************************************************************/

/**
 * An enumeration of possible audio combinations.
 */
typedef enum {

	OpAdd,
	OpRemove,
	OpReplace

} AudioOp;

/**
 * Maintains a location within an Audio for playback or recording.
 * Provides various transfer operations.
 */
class AudioCursor {
	
	friend class Audio;

  public:

	AudioCursor();
	AudioCursor(const char* name);
	AudioCursor(const char* name, class Audio* a);
	~AudioCursor();
	void reset();
	
	void setName(const char* name);
	void setAutoExtend(bool b);
	void setReverse(bool b);
	bool isReverse();

	void setAudio(Audio* a);
	Audio* getAudio();
	void setFrame(long f);
	long getFrame();
	long reflectFrame(long frame);

	void get(AudioBuffer* b);
	void get(AudioBuffer* b, float level);
	void get(AudioBuffer* b, Audio* a, long frame, float level);

	void put(AudioBuffer* b, AudioOp op);
	void put(AudioBuffer* b, AudioOp op, long frame);
	void put(AudioBuffer* b, AudioOp op, Audio* a, long frame);

	void startFadeIn();
	void setFadeIn(long frame);
	void setFadeOut(long frame);
	bool isFading();
	void transferFade(AudioCursor* dest);
	void resetFade();
	void fadeIn();
	void fadeIn(Audio* a);
	void fadeOut();
	void fadeOut(Audio* a);

	void fade(int offset, int frames, bool up);
	void fade(int offset, int frames, bool up, float baseLevel);
	void fade(bool up);

  protected:

	void init();
	void decache();
	void prepareFrame();
	void locateFrame();
	void incFrame();
	void get(AudioBuffer* buf, float* dest, float modifier);

	char* mName;
	class Audio* mAudio;
	AudioFade mFade;

	/**
	 * True if transfering in reverse.  This does not cause frame
	 * reflection in the transfer operations, it simply controls
	 * the direction of the iteration.  Caller must perform
	 * reflection if necessary.
	 */
	bool mReverse;

	int mFrame;				// current frame
	int	mVersion;			// last known version number of the Audio
	int mBufferIndex;		// index of buffer containing mFrame
	int mBufferOffset;		// offset (in samples) in mBuffer to mFrame
	float* mBuffer;			// buffer containing mFrame

	/**
	 * When true, causes the automatic extension of the Audio buffers
	 * when setting a frame that is outside the current range.  This
	 * is normally set for recording cursors, but not play cursors.
	 */
	bool mAutoExtend;

    /**
     * Set after we've traced an overflow error.
     * These tend to happen a lot once they start happening so don't
     * overwhelm the trace buffer.
     */
    bool mOverflowTraced;

};

/****************************************************************************
 *                                                                          *
 *                                   AUDIO                                  *
 *                                                                          *
 ****************************************************************************/

/**
 * Core memory model for digital audio in memory.
 * Incremental transfer operations are implemented in the AudioCursor class.
 * A set of convenience methods is defined in this class which use
 * internal AudioCursor objects, but it is usually best for an application
 * to maintain its own cursors.
 */
class Audio { 

	friend class AudioCursor;

  public:

	Audio();
	Audio(class AudioPool* pool);
	Audio(class AudioPool* pool, const char *filename);
	~Audio();
    void free();
	
    class AudioPool* getPool();

    // set the global default file write format
	static void setWriteFormat(int fmt);
	static void setWriteFormatPCM(bool b);

	// Audio Characteristics

	int getSampleRate();
	void setSampleRate(int i);

	int getChannels();
	void setChannels(int i);

	//  Sizing, normally used only in conjunction with an AudioCursor

	long getFrames();
	void setFrames(long frames);
	void setFramesReverse(long frames);
	long getSamples();
	bool isEmpty();

	// Simple operations, normally used in conjunction with an AudioCursor

	void reset();
	void zero();
	void splice(long startFrame, long frames);
	void copy(Audio* src);
	void copy(Audio* src, int feedback);

	// FIle IO

	int read(const char *filename);
	int write(const char *filename);
	int write(const char *filename, int format);

	// Convenience operations using builtin AudioCursor
	// These should only be used in simple cases

	long getPlayFrame();
	void rewind();

	void get(AudioBuffer* buf, long frame);
	void get(float* src, long frames, long frame);

	void put(AudioBuffer* buf, long frame);
	void put(float* src, long frames, long frame);
	void put(Audio* src, long frame);

	void append(AudioBuffer* c);
	void append(float* src, long frames);
	void append(Audio* src);

    void insert(Audio* src, long frame);

	void fadeEdges();

	// Diagnostics

	void dump();
	void dump(class TraceBuffer* b);
	void diff(Audio* a);

	/**
	 * Quickly write a sample buffer to a file.
	 */
	static void write(const char* filename, float* buffer, long length);

  protected:

	static int WriteFormat;

	void init();
	void decacheCursors();
	void freeBuffers();
    void freeBuffer(float* b);

	void initIndex();
	void prepareIndex(int index);
	void prepareIndexFrame(long frame);
	void growIndex(int count, bool up);
	void addBuffer(float* buffer, int index);
	float* allocBuffer();
	float* allocBuffer(int index);
	bool isEmpty(float* buffer);
	void setStartFrame(long frame);
	void applyFeedback(float* buffer, int feedback);

	// allow these to be directly accessible by AudioCursor

	float* getBuffer(int i);
	long prepareFrame(long frame, int* retIndex, int* retOffset, 
					  float** retBuffer);

	void locate(long frame, int* buffer, int* offset);
	void locateStart(int* buffer, int* offset);

    /**
     * The pool for the buffers.
     */
    class AudioPool* mPool;

	/**
	 * The number of frames per second.
	 * Normally 44100, CD quality.
	 */
	int mSampleRate;

	/**
	 * The number of channels, normally 2.
	 * This can also be though of as the number of samples per frame.
	 */
	int mChannels;

	/**
	 * Number of samples per buffer.  To get frames per buffer
	 * divide this by mChannels;
	 */
	int mBufferSize;

	/**
	 * The buffer index array.  This may be a sparse array, meaning that
	 * there may be a NULL pointer in any given element.  On playback
	 * this is to be treated as silence.  On record, buffers are normally
	 * allocated incrementally.
	 */
	float **mBuffers;

	/**
	 * Total number of elements in the mBuffers array.
	 */
	int mBufferCount;

	/**
	 * A counter that increments any time the the buffer array changes.
	 * This must be monitored by AudioCursor to detect structural changes
	 * that would invalidate the cached location.
	 */
	int mVersion;

	/**
	 * The first frame that is considered to have valid content.
	 * This plus mFrames defines the range of valid frames.
	 * This is not necessarily zero, we may start it in the
	 * middle of the available buffers so we can record in reverse
	 * without having to keep growing the index array.
	 */
	long mStartFrame;

	/**
	 * The number of valid frames.  This is often less than the
	 * total number of available buffer frames because we may pre-allocate
	 * buffers or pool Audio objects for use in several contexts.	
	 * The frame count may also be greater than the allocated buffer frames
	 * or may define a range of buffers, some of which are missing.  Missing
	 * buffers are semantically the same as a buffer full of zeros.  
	 */
	long mFrames;
	
	/**
	 * Internal cursors used with the convenience transfer methods.
	 * For most flexibility, create your own cursors.
	 */
	AudioCursor* mPlay;
	AudioCursor* mRecord;

};

/****************************************************************************
 *                                                                          *
 *                                    POOL                                  *
 *                                                                          *
 ****************************************************************************/

/**
 * This structure is allocated at the top of every Audio buffer.
 */
struct OldPooledBuffer {

	// todo, pool status, statistics
	OldPooledBuffer* next;
	int pooled;

};

/**
 * Maintains a pool of audio buffers.
 * There is normally only one of these in a Mobius instance.
 */
class AudioPool {
    
  public:

    AudioPool();
    ~AudioPool();

    void init(int buffers);
    void dump();

    Audio* newAudio();
    Audio* newAudio(const char* file);
    void freeAudio(Audio* a);

    float* newBuffer();
    void freeBuffer(float* b);

  private:

    class CriticalSection* mCsect;
    OldPooledBuffer* mPool;
    class SampleBufferPool* mNewPool;
    int mAllocated;
	int mInUse;

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
