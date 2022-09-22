/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * An object used by Track to maintain state related to 
 * the audio input and output streams.
 *
 */

#ifndef STREAM_H
#define STREAM_H

// for LayerContext
#include "Layer.h"

/****************************************************************************
 *                                                                          *
 *                                 FADE TAIL                                *
 *                                                                          *
 ****************************************************************************/

/**
 * Utility class used by OutputStream to maintain a "fade tail" to bring
 * the output wave to a zero crossing.  Since we can't always predict
 * when we will need a fade out, we capture one at the moment we need it
 * by extracting a short section from the current output source, fading
 * it to zero, then buffering it in the PlayTail so we can include 
 * it in the next interrupt block.  
 *
 * The stream will maintain two of these, one is for normal content 
 * that is sent through the effects plugins, the other is for output
 * from the plugin during significant plugin parameter changes that may
 * cause a break in the plugin output.  
 */
class FadeTail {

  public:
	
	FadeTail();
	~FadeTail();

	void initRecordOffset();
	void incRecordOffset(int frames);

	void reset();
	int getFrames();
	void add(float* tail, long frames);
	long play(float* outbuf, long frames);

  private:

	float* playRegion(float* outbuf, long frames);

	/**
	 * Buffer to hold fade tail frames.
	 */
	float* mTail;

	/**
	 * Maximum number of frames in mTail.
	 */
	int mMaxFrames;

	/**
	 * Samples per frame.
	 */
	int mChannels;

	/**
	 * Offset into mTail to the next frame in the tail to be played.
	 */
	int mStart;

	/**
	 * Number of frames in the tail, from mStart.
	 */
	int mFrames;

	/**
	 * Offset from mTailFrame where the next tail is to be recorded.
	 */
	int mRecordOffset;
	
};

/****************************************************************************
 *                                                                          *
 *                                  SMOOTHER                                *
 *                                                                          *
 ****************************************************************************/

/**
 * Utility class to perform gradual smoothing of level adjustment values.
 * Factored out of Stream so it can be used by Layer for feedback
 * smoothing.
 */
class Smoother {
  public:

	Smoother();
	~Smoother();

	void reset();
	void setValue(float level);
	void setTarget(float end);
	void setTarget(int endLevel);

	bool isActive();
	float getValue();
	float getTarget();
	void advance();

  private:

	bool mActive;
	float* mRamp;
	int mStep;
	float mStart;
	float mTarget;
	float mDelta;
	float mValue;

};

/****************************************************************************
 *                                                                          *
 *                                   STREAM                                 *
 *                                                                          *
 ****************************************************************************/

/**
 * An extension of LayerContext that adds some Track and Loop state.
 * These is further extended by InputStream and OutputStream below.
 */
class Stream : public LayerContext {

  public:

	Stream();
	virtual ~Stream();
	
    void reset();

	// call this rather than LayerContext::setLevel to get smoothing
	void setTargetLevel(int level);

	float getSpeed();
    void initSpeed();

	void setSpeedOctave(int degree);
	int getSpeedOctave();

	void setSpeedStep(int degree);
	int getSpeedStep();

    void setSpeedBend(int level);
    int getSpeedBend(void);

	void setSpeed(int octave, int step, int bend);

    // Pitch is only used by OutputStream but keep it
    // up in Stream so TimeStretch can manage it

	float getPitch();
    void initPitch();

	void setPitchOctave(int degree);
	int getPitchOctave();

	void setPitchStep(int degree);
	int getPitchStep();

    void setPitchBend(int level);
    int getPitchBend(void);

	void setPitch(int octave, int step, int bend);

    void setTimeStretch(int level);
    int getTimeStretch();

	virtual void setPitchTweak(int tweak, int value);
	virtual int getPitchTweak(int tweak);

	void setLatency(int i);
	int getLatency();
	int getNormalLatency();
	int getAdjustedLatency(int latency);
	int getAdjustedLatency(int octave, int semitone, int bend, int stretch);

	long getInterruptFrames();
	virtual void initProcessedFrames();
	virtual long getProcessedFrames();
	virtual long getRemainingFrames();

    void setCorrection(int c);
    int getCorrection();

    long calcDrift(long targetFrame, long currentFrame, long loopFrames);

	/**
	 * Effective latency frames.  Adjusted by speed shift.
	 */
	int latency;

	float interpolate(float* last, float* src, float* dest, 
					  long frames, int channels, float speed, float ramp);

    // kludge for ending a Recording
    void resetResampler();

  protected:

    void recalculateSpeed();
    void recalculatePitch();

	long deltaFrames(float* start, float* end);
	void adjustSpeedLatency();

	/**
	 * The non-adjusted latency for this stream.
	 * The "latency" field may be speed adjusted, and since this
	 * can result in roudning loss, always need to save the original value.
	 */
	int mNormalLatency;

	/**
	 * The speed adjustment.  This is always calculated
     * from mSpeedOctave, mSpeedStep, and mSpeedBend.
	 */
	float mSpeed;
	
    /**
     * The amount of octave speed shift being applied.
     * Zero is center and means no shift.
	 */
	int mSpeedOctave;

	/**  
     * The amount of semitone speed shift being applied.
     * Zero is center and means no shift.
	 */
	int mSpeedStep;

    /**
     * The amount of continuous speed shift being applied.
     * Zero is center and means no shift.
     */
    int mSpeedBend;

	/**
	 * The pitch adjustment.
	 */
	float mPitch;

    /**
     * The amount of octave pitch shift being applied.
     * Zero is center and means no shift.
	 */
	int mPitchOctave;

	/**  
     * The amount of semitone pitch shift being applied.
     * Zero is center and means no shift.
	 */
	int mPitchStep;

    /**
     * The amount of continuous pitch shift being applied.
     * Zero is center and means no shift.
     */
    int mPitchBend;

    /**
     * The amount of positive or negative time stretch 
     * being applied.  This effects both rate and pitch.    
     */
    int mTimeStretch;

	/**
	 * An object that performs the speed transposition.
	 */
	class Resampler* mResampler;

	/**
	 * Helper object to smooth out level changes.
	 */
	Smoother* mSmoother;

	/**
	 * Set to the audio interrupt buffer for every interrupt.
	 */
    float* mAudioBuffer;

	/**
	 * The number of frames available in mAudioBuffer.
	 */
	long mAudioBufferFrames;

	/**
	 * A pointer into mAudioBuffer we increment as we place
	 * frames in the output buffer.
	 */
	float* mAudioPtr;

    /**
     * Stream correction goal.  
     *
     * When negative the stream is too far ahead of the other stream
     * and needs to be brought backward by the correction frames.  
     * A small amount of speed reduction will be applied so that the
     * stream advances more slowly than real time.
     * 
     * When positive the stream is too far behind the other stream and
     * needs to be brought forward. A small amount of speed increase
     * is applied.
     */
    int mCorrection;
};

/****************************************************************************
 *                                                                          *
 *                                INPUT STREAM                              *
 *                                                                          *
 ****************************************************************************/

/**
 * An extension of Stream that adds more state need by Track
 * to perform automatic fades.  
 */
class InputStream : public Stream {

  public:

	InputStream(class Synchronizer* sync, int sampleRate);
	~InputStream();

	class Synchronizer* getSynchronizer();
    void setPlugin(class StreamPlugin* plugin);
	void setInputBuffer(class AudioStream* stream, float* input, long frames, 
						float* echo);
	void bufferModified(float* buffer);

    void rescaleInput();
    long getScaledRemainingFrames();
    long getOriginalFramesConsumed();
	long record(class Loop* loop, class Event* e);

	void resetHistory(Loop* l);

    void monitor(float* echo);
    int getMonitorLevel();
	void initProcessedFrames();
	long getProcessedFrames();
	long getRemainingFrames();

	int getSampleRate();

  private:

	void scaleInput();

	/**
	 * Last known sample rate.
	 * In practice this shouldn't change, but the host could change it
	 * on each buffer.
	 */
	int mSampleRate;

	/**
	 * Shared synchronization event generator.
	 */
	class Synchronizer* mSynchronizer;

    /**
     * Optional plugin.
     */
    class StreamPlugin* mPlugin;

    /**
     * Maximum sample detected in a buffer (absolute value
     * after attenuation).
     */
    int mMonitorLevel;

	/**
	 * Intermediate buffer to hold level adjusted frames.
	 */
	float* mLevelBuffer;

	/**
	 * Intermedate buffer used to hold speed adjusted frames.
	 */
	float* mSpeedBuffer;

	/**
	 * Last speed used for recording.
	 */
	float mLastSpeed;

	/**
	 * Resampler threshold at the beginning of the last block.
	 */
	float mLastThreshold;

	/**
	 * The number of frames from the original unscaled input buffer
	 * we have consumed so far.  Necessary to keep this since 
	 * diffing the mAudioBuffer and mAudioPtr pointers doesn't work here.
	 */
	long mOriginalFramesConsumed;

	/**
	 * The number of frames remaining in the buffer referenced by mAudioPtr.
	 * For input streams, this may be longer or shorter than the remaining
	 * frames in the original input buffer due to speed scaling.
	 */
	long mRemainingFrames;

	/**
	 * The last layer we recorded into.  Used to detect the final
	 * shift from one layer to another so the old layer can be finalized.
	 */
	Layer* mLastLayer;

};

/****************************************************************************
 *                                                                          *
 *                               OUTPUT STREAM                              *
 *                                                                          *
 ****************************************************************************/

/**
 * An extension of Stream that adds more state need by Track
 * to perform automatic fades.  
 */
class OutputStream : public Stream {

  public:

	OutputStream(InputStream* is, class AudioPool* pool);
	~OutputStream();
	
    void setPlugin(class StreamPlugin* plugin);
	void setPan(int p);
	void setMono(bool b);
	void setLayerShift(bool b);
	void squelchLastLayer(Layer* rec, Layer* play, long playFrame);
	void setLastFrame(long frame);
	void adjustLastFrame(int delta);
	void resetHistory(Loop* l);

	void setPitchTweak(int tweak, int value);
    int getPitchTweak(int tweak);

	Layer* getLastLayer();
	long getLastFrame();

	void setOutputBuffer(class AudioStream* stream, float* b, long l);

	// called by Track
	void play(Loop* loop, long outframes, bool last);

	// called by Loop
	void play(Layer* layer, long playFrame, long frames, bool mute);

	// Loop needs this in a few places
	void captureTail();

	// output level monitoring
	void clearMaxSample();
	float getMaxSample();
    int getMonitorLevel();

	void setCapture(bool b);

  private:

	void expandFrames(float* buffer, long frames);
	void captureTail(float adjust);
	void captureTail(FadeTail* tail, float adjust);
	void captureTail(FadeTail* tail, Layer* src, long playFrame, float adjust);
	void addTail(float* tail, long frames);
	void playTail(float* outbuf, long frames);
	float* playTailRegion(float* outbuf, long frames);
	void checkMax(float sample);
	void capture(float* buffer, long frames);
	void adjustLevel(long frames);
	void captureOutsideFadeTail();
	void capturePitchShutdownFadeTail();

    /**
     * Audio pool we use when capturing.
     */
    class AudioPool* mAudioPool;

	/**
	 * Corresponding input stream, necessary only to correct dealignment
	 * during speed changes.
	 */
	InputStream* mInput;

    /**
     * Pitch shifting plugin.
     */
    class PitchPlugin* mPitchShifter;

    /**
     * Optional random plugin.
     */
    class StreamPlugin* mPlugin;

	/**
	 * Pan value to apply.
	 */
	int mPan;

	/**
	 * Flag indiciating we're in mono mode.
	 */
	bool mMono;

	/**
	 * A pair of smoothers for each channel in the pan.
	 */
	Smoother* mLeft;
	Smoother* mRight;

	/**
	 * A buffer managed by output streams that captures the
	 * output of the Loop, and is then merged with mAudioBuffer.
	 * Necessary because Loop wants to apply fades to the content of the
	 * output buffer which may contain frames added by other tracks.
	 * This is also larger than the interrupt buffer so we can perform
	 * speed shift expansion.
	 */
    float* mLoopBuffer;

	/**
	 * A buffer managed by output streams that captures the result
	 * of a speed transposition.
	 */
	float* mSpeedBuffer;

	/**
	 * The last layer from which frames were taken.
	 */
	Layer* mLastLayer;

	/**
	 * The frame immediately after the last one taken.
	 */
	long mLastFrame;

	/**
	 * Flag set to indicate that no fade should be performed
	 * even if the layers or frames differ.
	 */
	bool mLayerShift;

	/**
	 * Normaly play jump fade tail buffer.
	 */
	FadeTail* mTail;

	/**
	 * Tail for fades that must be processed outside of the plugin chain.
	 */
	FadeTail* mOuterTail;

	/**
	 * Transient flag set on each interrupt to indiciate that we need to
	 * force a layer fade in because the pitch shifter (or another plugin) 
	 * has been deactivated and there will be a discontinuity caused by latency
	 * in the plugin.
	 */
	bool mForceFadeIn;

	/**
	 * Maximum sample level processed.
	 */
	float mMaxSample;

	// Diagnostics

	bool mCapture;
	Audio* mCaptureAudio;
	long mCaptureTotal;
	long mCaptureMax;
	

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
