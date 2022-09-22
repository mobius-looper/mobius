/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Various utilities which when combined will convert
 * an audio stream from one sample rate to another.  Normally this
 * is done to convert audio for transmission between systems with
 * different sample rates, but here it is used to obtain a transposition
 * of the pitch.
 *
 * Can eventually factor this out into a set of utility classes, but
 * for now I want to keep everything encapsulated.
 *
 */

#ifndef RESAMPLER_H
#define RESAMPLER_H

//#include "Audio.h"
#include "AudioInterface.h"

//////////////////////////////////////////////////////////////////////
// 
// Constants
//
//////////////////////////////////////////////////////////////////////

/**
 * Maximum number of remainder frames we will maintain.
 * For 1/2 speed, I've only ever seen 1 frame of remainder.
 */
#define MAX_REMAINDER 32

/**
 * The frequency factor between two semitones.  
 * This is 2^1/12, This^12 = 2 for one octave.
 */
#define SEMITONE_FACTOR 1.059463f

/**
 * Maximum number of octaves of rate shift in one direction.
 * It is important that we constraint this or else the intermediate
 * buffers used for interpolation and decimation become extremely large.
 *
 * For decimation during up shifts the multiplication to the buffer is:
 *
 *   octave 1, multiplier 2
 *   octave 2, multiplier 4
 *   octave 3, multiplier 8
 *   ocatve 4, multiplier 16
 *
 * So for a normal 256 frame interrupt buffer, we would need working
 * buffers of 4096 frames, times the number of channels, so 8192 for stereo.
 */
#define MAX_RATE_OCTAVE 4

/**
 * Maximum rate step away from center.  
 * This is just MAX_RATE_OCTAVE * 12
 */
#define MAX_RATE_STEP 48

/**
 * The maximum possible rate shift up.  This is also the multplier
 * used for internal buffer sizes so that they are large enough
 * to handle the maximum alloweable rate shift.
 * 
 * This is pow(2.0, MAX_RATE_OCTAVE) or 
 * pow(SEMITONE_FACTOR, MAX_RATE_OCTAVE * 12)
 */
#define MAX_RATE_SHIFT 16

/**
 * The minimum possible rate shift down.
 * This is 1 / MAX_RATE_SHIFT.
 */
#define MIN_RATE_SHIFT .0625f

/**
 * The rate/pitch bend range.
 * This is currently fixed to have a range 16384 internal steps to match
 * the MIDI pitch bend wheel.  We could make this higher but it would
 * only be useful in scripts or OSC.  Maybe plugin paramter bindings.
 */
#define RATE_BEND_RANGE 16384
#define MIN_RATE_BEND -8192
#define MAX_RATE_BEND 8191

/**
 * The maximum effectve semitone steps in one direction in the
 * bend range.  Unlike step range, this is not adjustable without
 * recalculating some a root each time.
 *
 * This must match the BEND_FACTOR below.
 */
#define MAX_BEND_STEP 12

/**
 * The Semitone formula starts by determining the rate necessary to
 * get a one octave rise, 2.0.  Then it takes the 1/12th root of that
 * to get 1.059463.  To do something similar for bend, we start by
 * calculating the maximum octave spread in one direction then 
 * take the 8192th root of that.
 *
 * For bend sensitivity of one octave up or down we take the 8192th
 * root of 2.0 for 1.000085.
 */
#define BEND_FACTOR 1.000085f

//////////////////////////////////////////////////////////////////////
// 
// Resampler
//
//////////////////////////////////////////////////////////////////////

class Resampler {

  public:

    Resampler();
    Resampler(bool input);
    ~Resampler();

	//
	// Utilities
	//

    static float getSpeed(int octave, int step, int bend, int stretch);

    // 
    // Methods called by Stream
    //
    
	void reset();
    void setSpeed(float speed);
    long addRemainder(float* buffer, long maxFrames);
	float getThreshold();

	long scaleInputFrames(long srcFrames);
	long scaleOutputFrames(long destFrames);
	long scaleFromInputFrames(float initialThreshold, long inputFrames);

    long resample(float* src, long srcFrames, float* dest, long destFrames);

	// 
	// misc methods for completness
	//

	void setSpeedSemitone(int degree);
	float getSpeed();

	//
	// Tests & Experiments
	//

    float* generateSine(int seconds, long* frames);
    void writeSine(int seconds, const char* file);
    void decimate2x(float* src, long frames, float* dest);
    void interpolate2x(float* src, long frames, float* dest);
	void transposeOnce(float* src, float* dest, long frames, float speed);
	long transpose(float* src, long srcFrames, float* dest, long destFrames,
				   float speed);

  private:

	void init();

	static float getSemitoneSpeed(int degree);
	static float getContinuousSpeed(int level);

	long scaleToDestFrames(float speed, float threshold, long srcFrames);
	long scaleToSourceFrames(float speed, float threshold, long destFrames);

	//
	// Fields
	//

	bool mTrace;
	bool mInput;
    float mSpeed;
	float mInverseSpeed;
    int mChannels;
    float mRemainder[MAX_REMAINDER * AUDIO_MAX_CHANNELS];    
    int mRemainderFrames;
	float mLastFrame[AUDIO_MAX_CHANNELS];
	float mThreshold;

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
