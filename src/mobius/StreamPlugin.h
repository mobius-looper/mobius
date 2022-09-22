/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * StreamPlugin is an interface for an object that processes
 * audio in blocks.  The external block size may vary on each call, 
 * with the plugin buffering the results of the processing
 * algorithm as necessary.
 *
 * It is intended to be embedded in a Stream object to plug in processing
 * for the Mobius input and output streams.  It is general though the
 * only implementation we have right now is PitchPlugin.
 *
 */

#ifndef STREAM_PLUGIN_H
#define STREAM_PLUGIN_H

// for MAX_HOST_BUFFER_SIZE, need to be consistent
#include "HostInterface.h"

/****************************************************************************
 *                                                                          *
 *                                   PLUGIN                                 *
 *                                                                          *
 ****************************************************************************/

class StreamPlugin {

  public:

    StreamPlugin(int sampleRate);
    virtual ~StreamPlugin();

	void setBatch(bool b);

    virtual void reset();

    virtual void setSampleRate(int rate);
    virtual void setChannels(int channels);
	virtual void setTweak(int tweak, int value);
    virtual int getTweak(int tweak);
	virtual void startupFade();

	long process(float* buffer, long frames);

    virtual long process(float* input, float* output, long frames) = 0;

	// handle for triggering ad-hoc testing code
	virtual void debug();

	void split(float* source, float* left, float* right, long frames);
	void merge(float* left, float* right, float* output, long frames);

	void captureFadeTail(class FadeTail* tail);

  protected:

	void doStartupFade(float* output, long frames);
	virtual long getAvailableFrames();
	virtual long getFrames(float* buffer, long frames);
	virtual void putFrames(float* buffer, long frames);

	/**
	 * Temporary output buffer used if the plug doesn't support
	 * modification of the input buffer.
	 */
    float mOutput[MAX_HOST_BUFFER_FRAMES];    

    /**
     * Sample rate in Hz, for example 44100 for 44.1kHz.
     */
    int mSampleRate;

    /**
     * Number of channels (e.g. 2 for stereo)
     */
    int mChannels;

	/**
	 * Number of blocks we're processed (testing).
	 */
	int mBlocks;

	/**
	 * True if we're in batch mode.  This means that the process()
	 * function is allowed to return less than requested due to 
	 * internal buffering and latency.
	 *
	 * When on, the plugin will typically buffer some number of frames
	 * before emitting anything so that stream processing can proceed
	 * without breaks.  This will also result in an initial gap
	 * in the output which makes it unsuitable for file processing.
	 */
	int mBatch;

	/**
	 * Optional helper object used to implement a shutdown fade tail.
	 */
	class FadeWindow* mTailWindow;

	/**
	 * When true we're performing a startup fade.
	 */
	bool mStartupFade;

	/**
	 * When performing a startup fade, this is the offset into the fade range
	 * we've already performed.
	 */
	int mStartupFadeOffset;

};


/****************************************************************************
 *                                                                          *
 *                                PITCH PLUGIN                              *
 *                                                                          *
 ****************************************************************************/

/**
 * Extension of StreamPlugin for algorithms that alter pitch but not time.
 */
class PitchPlugin : public StreamPlugin {

  public:

    PitchPlugin(int sampleRate);
    virtual ~PitchPlugin();
   
	// platform specific factory method
	static PitchPlugin* getPlugin(int sampleRate);

	virtual void setPitch(float ratio);
    virtual void setPitch(float ratio, int semitones);
    virtual void setPitch(int semitones);

    float getPitchRatio();
    int getPitchSemitones();

    float semitonesToRatio(int semitones);
    int ratioToSemitones(float ratio);

	Audio* processToAudio(class AudioPool* pool, float* input, long frames);

	void simulate();

  protected:

	virtual void updatePitch() = 0;

	/**
	 * Shift factor in semitones.
	 */
	int mPitchStep;

    /**
     * Shift factor.  Values less than 1 shift down, values greater than
     * one shift up.  For example 0.5 is one octive down and 
     * 2.0 is one octave up.  A value of exactly 1.0 has no effect.
     */
    float mPitch;

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
