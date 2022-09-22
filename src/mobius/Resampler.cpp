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

#include <stdio.h>
#include <math.h>

#include "Util.h"
#include "Trace.h"

#include "Resampler.h"

/****************************************************************************
 *                                                                          *
 *   							  UTILITIES                                 *
 *                                                                          *
 ****************************************************************************/

/**
 * Given a positive or negative chromatic scale degree, calculate the floating
 * point speed adjustment.
 *
 * A degree of 1 means one semitone up, -1 one semitone down, etc.
 *
 * We're using 12/-12 for half speed toggle now, and the pow() doesn't
 * come out exactly on .5 it's .500047 or something with a small amount
 * of noise.  Older tests assumed it would be exactly .5 so handle the octave
 * jumps without pow() and use pow() for the remainder.
 */
PRIVATE float Resampler::getSemitoneSpeed(int degree)
{
	float speed = 1.0;

    if (degree > 0) {
		// speed up
        // handle the octaves without pow
        int octave = (degree / 12);
        if (octave != 0) {
            speed = (float)pow(2.0, octave);
            degree -= (octave * 12);
        }
        if (degree != 0)
          speed *= (float)pow(SEMITONE_FACTOR, degree);
	}
	else if (degree < 0) {
		// slow down, abs and invert
        // handle the octaves without pow
        degree = -degree;
        int octave = (degree / 12);
        if (octave != 0) {
            speed = (1.0f / (float)pow(2.0, octave));
            degree -= (octave * 12);
        }
        if (degree != 0)
          speed *= (1.0f / (float)pow(SEMITONE_FACTOR, degree)); 
	}

	return speed;
}

/**
 * Given a positive or negative continuous speed shift level, calculate
 * the floating point speed adjustment.
 *
 * This is currently fixed to have a range 16384 internal steps to match
 * the MIDI pitch bend wheel.  We could make this higher but it would
 * only be useful in scripts or OSC.  Maybe plugin paramter bindings.
 *
 * That range of steps is then applied to a configurable shift range
 * specified in semitones.  A range of 48 is two octaves up and down.
 *
 * !! I'm not sure I like how the semitoneShiftRange is specified
 * as a semitone range rather than an octave range?
 *
 * The Semitone formula starts by determining the rate necessary to
 * get a one octave rise, 2.0.  Then it takes the 1/12th root of that
 * to get 1.059463.  To do something similar for bend, we start by
 * calculating the octave spread of the entire range, start with one
 * up so 2.0.  Then take the 8192th root of that for: 1.000085
 */
PRIVATE float Resampler::getContinuousSpeed(int degree)
{
	float speed = 1.0f;
    
	if (degree > 0) {
		// speed up
		speed = (float)pow(BEND_FACTOR, degree);
	}
	else if (degree < 0) {
		// slow down, abs and invert
		speed = (1.0f / (float)pow(BEND_FACTOR, -degree));
	}

	return speed;
}

/**
 * Caluclate an effective speed from the three components.
 * This is called by Stream passing all the things that can influence
 * rate shift.  Since these are additive it is important that we enforce
 * a min/max, this is the enforcement point.  
 *
 * Since time stretch has a corresponding pitch shift, this may interact
 * strangely with pitch if you overflow the allowable range.  If we wanted
 * to do this right, we would have to make a corresponding adjustment
 * the amount of pitch compensation being applied.
 */
PUBLIC float Resampler::getSpeed(int octave, int step, int bend, int stretch)
{
    float speed = getSemitoneSpeed(step);

    int effectiveBend = bend + stretch;

    if (effectiveBend != 0)
      speed *= getContinuousSpeed(effectiveBend);

    if (octave > 0)
      speed *= (float)pow(2.0, octave);

    else if (octave < 0)
      speed *= (1.0f / (float)pow(2.0, -octave));

    // enforce constraints
    if (speed > MAX_RATE_SHIFT)
      speed = MAX_RATE_SHIFT;

    else if (speed < MIN_RATE_SHIFT)
      speed = MIN_RATE_SHIFT;

    return speed;
}

/****************************************************************************
 *                                                                          *
 *                                 RESAMPLER                                *
 *                                                                          *
 ****************************************************************************/

PUBLIC Resampler::Resampler()
{
	init();
}

PUBLIC Resampler::Resampler(bool input)
{
	init();
	mInput = input;
}

PUBLIC void Resampler::init()
{
	mTrace = false;
	mInput = false;
    mSpeed = 1.0;
	mInverseSpeed = (float)(1.0f / mSpeed);
    mChannels = 2;

	reset();

	for (int i = 0 ; i < mChannels ; i++)
	  mLastFrame[i] = 0.0;
}

PUBLIC Resampler::~Resampler()
{
}

PUBLIC void Resampler::reset()
{
	// leave speed alone, but clear any remainders
	mRemainderFrames = 0;
	mThreshold = 1.0f;

	// what about mLastFrame?  since we're going to continue using
	// it leave it alone
	//for (int i = 0 ; i < mChannels ; i++)
	//mLastFrame[i] = 0.0;
}

PUBLIC float Resampler::getSpeed()
{
	return mSpeed;
}

/**
 * Set the resampling speed and recalculate some related values.
 * This is called on every interrupt so ignore if we're already there.
 */
PUBLIC void Resampler::setSpeed(float speed)
{
    if (speed != mSpeed) {
        // do we get to start over now?
		// !! InputStream::scaleInput assumes this will reset the 
		// threshold, if not, we have a complicated threshold state
		// that must be maintained or calculated by Stream
        mRemainderFrames = 0;
		mThreshold = 1.0f;
		mSpeed = speed;
		mInverseSpeed = (float)(1.0 / mSpeed);
	}
}

/**
 * Set the speed as a scale degree.
 */
PUBLIC void Resampler::setSpeedSemitone(int degree)
{
	setSpeed((float)pow(SEMITONE_FACTOR, degree));
}

/**
 * Used by InputStream to remember the starting threshold 
 * used to resample a section of the interrupt block.
 */
PUBLIC float Resampler::getThreshold()
{
	return mThreshold;
}

/**
 * If the last call to resample() resulted in a remainder, copy the remainder
 * to the buffer and return its length.  
 */
PUBLIC long Resampler::addRemainder(float* buffer, long maxFrames)
{
    int remainder = mRemainderFrames;

    if (remainder > 0) {
        if (remainder > maxFrames)
          remainder = maxFrames;

        int samples = remainder * mChannels;
        for (int i = 0 ; i < samples ; i++) {
			// NOTE: We're assuming we get to replace here, which
			// Stream currently requires since it does not zero out
			// mLoopBuffer before calling us.  This seems symetrical
			// with the resample methods which also assume they can replace.
			buffer[i] = mRemainder[i];
		}

        mRemainderFrames -= remainder;

        if (mRemainderFrames > 0) {
            // This shouldn't happen unless we have a really short buffer,
            // which I guess can happen if the buffer is being carved
            // up by events.  So we don't have to maintain a head pointer
            // shift the remaining remainder down.
            int offset = samples;
            samples = mRemainderFrames * mChannels;
            for (int i = 0 ; i < samples ; i++)
              mRemainder[i] = mRemainder[i + offset];
        }
    }

    return remainder;
}

/**
 * Perform speed scaling on a block of frames.
 * The destFrames argument may be 0 (or less) to indicate that
 * the dest buffer will always be large enough, so go as far as we can.
 *
 * If speed is 1.0, just do a copy, otherwise call the tranpose() method
 * with the appropriate speed.  If we're the input stream, the speed
 * we use for transposition has to be the inverse of the play speed.
 */
PUBLIC long Resampler::resample(float* src, long srcFrames, 
                                float* dest, long destFrames)
{
	long actual = 0;

    if (mSpeed == 1.0) {
        // shouldn't get here, but pass through
        long samples = srcFrames * mChannels;
		if (destFrames <= 0)
		  actual = srcFrames;
		else {
			actual = destFrames;
			if (srcFrames < destFrames)
			  Trace(1, "Resampler copy underflow!\n");
			else if (srcFrames > destFrames) {
				// oh, we could try to use the remainder, but this 
				// shouldn't happen
				Trace(1, "Resampler copy overflow!\n");
				samples = destFrames * mChannels;
			}
		}

        for (int i = 0 ; i < samples ; i++)
          *dest++ = *src++;

        mRemainderFrames = 0;
		mThreshold = 1.0;

		// save the last frame in case the speed starts changing
		if (srcFrames > 0) {
			int last = (srcFrames - 1) * mChannels;
			for (int i = 0 ; i < mChannels ; i++) 
			  mLastFrame[i] = src[last + i];
		}
    }
    else {
		float speed = ((mInput) ? mInverseSpeed : mSpeed);
		actual = transpose(src, srcFrames, dest, destFrames, speed);
	}

	return actual;
}

/****************************************************************************
 *                                                                          *
 *   							TRANSPOSITION                               *
 *                                                                          *
 ****************************************************************************/

/**
 * Given a number of output frames, determine how many frames we need to
 * consume to achieve that number.  Speed should be the playback speed.
 * 
 * I was unable to come up with a simple calculation for this due
 * to my limited math skills and what I suspect are float rounding
 * errors that always caused periodic anamolies.  Instead, do it the brute
 * force way and simulate what the transpose() method will do.
 */
PUBLIC long Resampler::scaleToSourceFrames(float speed, float threshold, 
										   long destFrames)
{
	if (speed == 1.0) return destFrames;

	long srcFrames = 1;  // always need at least one
	long destFrame = 0;

	// special case, if destFrames is zero then we won't be doing any
	// combinations so do not return 1
	if (destFrames <= 0) return 0;

    // combine last frame from previous block with first frame of this block
	while (threshold <= 1.0f && destFrame < destFrames) {
		destFrame++;
		threshold += speed;
	}
	threshold -= 1.0f;

    // may have an initial skip
    while (threshold > 1.0f) {
        threshold -= 1.0f;
		srcFrames++;
    }

	// from this point on we're combining the current source
	// frame with the next so need an extra
	if (destFrame < destFrames)
	  srcFrames++;

    while (destFrame < destFrames) {
		destFrame++;
        threshold += speed;
		if (destFrame < destFrames) {
			while (threshold > 1.0f) {
				threshold -= 1.0f;
				srcFrames++;
			}
		}
	}

	return srcFrames;
}

/**
 * Given a number of input frames, calculate the resulting number
 * of frames after speed adjustment.  Speed here must be the inverse
 * of the playback speed.
 *
 * I was unable to come up with a simple calculation for this due
 * to my limited math skills and what I suspect are float rounding
 * errors that always caused periodic anamolies.  Instead, do it the brute
 * force way and simulate what the transpose() method will do.
 */
PUBLIC long Resampler::scaleToDestFrames(float speed, float threshold, 
										 long srcFrames)
{
	if (speed == 1.0) return srcFrames;

	long destFrames = 0;
	long srcFrame = 0;
	long lastFrame = srcFrames - 1;

	// special case, if there are no source frames will do no combinations
	if (srcFrames <= 0) return 0;

    // combine last frame from previous block with first frame of this block
	while (threshold <= 1.0f) {
		destFrames++;
		threshold += speed;
	}
	threshold -= 1.0f;

    // may have an initial skip
    while (threshold > 1.0f && srcFrame < srcFrames) {
        threshold -= 1.0f;
		srcFrame++;
    }

    while (srcFrame < lastFrame) {
		destFrames++;
        threshold += speed;
        while (threshold > 1.0f && srcFrame < lastFrame) {
            threshold -= 1.0f;
			srcFrame++;
        }
    }

	return destFrames;
}

/**
 * Given a number of input frames, calculate the resulting number
 * of frames after speed adjustment.  Speed here must be the inverse
 * of the playback speed.
 *
 * Once used by InputStream to determine how may frames it should
 * expectd to record, but this is no longer used.  Intead, InputStream
 * just passes 0 as the destination buffer size and let's resample()
 * fill as much as it needs.
 */
PUBLIC long Resampler::scaleInputFrames(long srcFrames)
{
	return scaleToDestFrames(mInverseSpeed, mThreshold, srcFrames);
}

/**
 * Given a number of output frames, determine how many frames we need
 * to read from the loop and scale to achieve that number.
 */
PUBLIC long Resampler::scaleOutputFrames(long destFrames)
{
	return scaleToSourceFrames(mSpeed, mThreshold, destFrames);
}

/**
 * Given a number of frames recorded, determine how many source
 * frames we had to consume to get there. 
 *
 * Used by InputStream when the block is broken up by events to
 * find the number of interrupt frames that must be consumed by
 * the OutputStream in order to stay in sync.
 *
 * The calculation is the same as that used by scaleOutputFrames
 * to determine how many frames to read from the loop to fill 
 * the output buffer.
 */
PUBLIC long Resampler::scaleFromInputFrames(float initialThreshold, 
											long inputFrames)
{
	return scaleToSourceFrames(mInverseSpeed, initialThreshold, inputFrames);
}

/**
 * General purpose sample speed conversion using linear interpolation.
 * The last frame in the source buffer must be saved and returned on
 * the next call.  
 *
 * Made public for algorithm experiments.
 */
PUBLIC long Resampler::transpose(float* src, long srcFrames,
								 float* dest, long destFrames,
								 float speed)
{
    float* srcFrame = src;
    float* nextFrame = &src[mChannels];
    float* lastFrame = &src[(srcFrames - 1) * mChannels];
    float* destFrame = dest;
	float* lastDestFrame = NULL;
	long advance = 0;
	bool remainder = false;

	// special case, if srcFrames is zero, then there is nothing to do
	// might happen if we're processing events stacked on the same frame
	if (srcFrames <= 0) return 0;

	// if this comes in less than 1 assume there is enough
	if (destFrames > 0)
	  lastDestFrame = &dest[(destFrames - 1) * mChannels];

	mRemainderFrames = 0;

    // combine last frame from previous block with first frame of this block
    while (mThreshold <= 1.0f) {
		for (int i = 0 ; i < mChannels ; i++) {
			float f1 = (1.0f - mThreshold) * mLastFrame[i];
			float f2 = mThreshold * srcFrame[i];
			*destFrame++ = f1 + f2;
		}
		advance++;
        mThreshold += speed;
    }
    mThreshold -= 1.0f;

    // may have an initial skip if decimating
    while (mThreshold > 1.0f && srcFrame <= lastFrame) {
        mThreshold -= 1.0f;
        srcFrame += mChannels;
        nextFrame += mChannels;
    }

	// Now process the remaining frames, interpolating between the
	// current frame and the next one, then skipping more than one
	// if we're decimating.
	// Note that since we're always combining two frames, we don't actually
	// advance to the last input frame, keep it for the next call.

    while (srcFrame < lastFrame) {

		// !! if we're going to be skipping a frame, shouldn't we be
		// interpolating between the current frame and what will eventually
		// be the next frame?

		if (remainder && mRemainderFrames >= MAX_REMAINDER) {
			// overflowed the remainder buffer, mayhem ensues
			Trace(1, "Transposition remainder overflow!\n");
		}
		else { 
			for (int i = 0 ; i < mChannels ; i++) {
				float f1 = (1.0f - mThreshold) * srcFrame[i];
				float f2 = mThreshold * nextFrame[i];
				*destFrame++ = f1 + f2;
			}
			advance++;

			if (remainder)
			  mRemainderFrames++;
			else if (lastDestFrame != NULL && destFrame > lastDestFrame) {
				remainder = true;
				destFrame = mRemainder;
			}
		}

        mThreshold += speed;

        // once we increment beyond 1, advance to the next source frame
		// if we're decimating this may skip more than one frame
        while (mThreshold > 1.0f && srcFrame < lastFrame) {
            mThreshold -= 1.0f;
            srcFrame += mChannels;
            nextFrame += mChannels;
        }
    }
	
    // Replace the last frame
    if (srcFrames > 0) {
        for (int i = 0 ; i < mChannels ; i++)
          mLastFrame[i] = lastFrame[i];
    }

	if (destFrames > 0 && advance < destFrames)
	  Trace(1, "Transposition underflow!\n");

	return advance;
}

/**
 * Convenience method to transpose in one pass.
 */
PUBLIC void Resampler::transposeOnce(float* src, float* dest, 
									 long frames, float speed)
{
	mThreshold = 1.0f;
	for (int i = 0 ; i < mChannels ; i++)
	  mLastFrame[i] = 0.0;

    transpose(src, frames, dest, 0, speed);
}

/****************************************************************************
 *                                                                          *
 *   								TESTS                                   *
 *                                                                          *
 ****************************************************************************/

#define TWO_PI (6.283185307179586)

/**
 * Generate a 1000Khz stereo sine wave for a specified number of seconds.
 */
PUBLIC float* Resampler::generateSine(int seconds, long* retsamples)
{
    // this isn't part of core Mobius, it's only used for testing
    // so we can hard code the rate
    int rate = 44100;
    float phase = 0.0;

	// 1000 is pretty high
    float frequency = 500.0;

	// careful with this, 1.0 is as loud as it gets
	// .25 is painful in headphones
    float amplitude = 0.0175f;

    long samples = (seconds * rate) * 2;
    double magic = (TWO_PI * frequency) / rate;

    float* buffer = new float[samples];
    for (int i = 0 ; i < samples ; i += 2) {
        float sample = (float)(amplitude * sin((magic * i) + phase));
        buffer[i] = sample;
        buffer[i+1] = sample;
    }

    if (retsamples != NULL)
      *retsamples = samples;

    return buffer;
}

PUBLIC void Resampler::writeSine(int seconds, const char* file)
{
    FILE* fp = fopen(file, "w");
    if (fp == NULL)
      printf("Unable to open file %s\n", file);
    else {
        long samples;
        float* sine = generateSine(seconds, &samples);
        for (int i = 0 ; i < samples ; i++)
          fprintf(fp, "%f\n", sine[i]);
        fclose(fp);
        delete sine;
    }
}

PUBLIC void Resampler::interpolate2x(float* src, long frames, float* dest)
{
    long samples = frames * 2;
    long destsample = 0;

    for (int i = 0 ; i < samples ; i += 2) {
        dest[destsample++] = src[i];
        dest[destsample++] = src[i+1];
        dest[destsample++] = src[i];
        dest[destsample++] = src[i+1];
    }
}

PUBLIC void Resampler::decimate2x(float* src, long frames, float* dest)
{
    long samples = frames * 2;
    long destsample = 0;

    for (int i = 0 ; i < samples ; i += 4) {
        dest[destsample++] = src[i];
        dest[destsample++] = src[i+1];
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
