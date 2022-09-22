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
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "Util.h"
#include "Trace.h"
#include "WaveFile.h"
#include "AudioInterface.h"

#include "Audio.h"

// !! this is for FadeWindow and FadeTail, try to factor this out so we don't have
// so many dependencies
#include "FadeWindow.h"
#include "Stream.h"

#include "StreamPlugin.h"

/****************************************************************************
 *                                                                          *
 *                                   PLUGIN                                 *
 *                                                                          *
 ****************************************************************************/

PUBLIC StreamPlugin::StreamPlugin(int sampleRate)
{
    mSampleRate = sampleRate;
    mChannels = 2;
	mBlocks = 0;
	mBatch = false;
	mStartupFade = false;
	mStartupFadeOffset = 0;
	mTailWindow = NULL;
}

PUBLIC StreamPlugin::~StreamPlugin()
{
	delete mTailWindow;
}

PUBLIC void StreamPlugin::setBatch(bool b) 
{
	mBatch = b;
}

PUBLIC void StreamPlugin::reset()
{
}

PUBLIC void StreamPlugin::setSampleRate(int rate)
{
    mSampleRate = rate;
}

PUBLIC void StreamPlugin::setChannels(int channels)
{
    mChannels = channels;
}

PUBLIC void StreamPlugin::setTweak(int tweak, int value)
{
}

PUBLIC int StreamPlugin::getTweak(int tweak)
{
    return 0;
}

PUBLIC void StreamPlugin::debug()
{
}

/**
 * Process an inplace buffer.
 * Now that we handle startup and shutdown fades, the subclass must NOT overload this.
 */
long StreamPlugin::process(float* buffer, long frames)
{
	// need a flag to indiciate if the algorithm supports inplace changes!!
	long actual = process(buffer, mOutput, frames);

	// apply the startup fade if we're in one
	if (mStartupFade)
	  doStartupFade(mOutput, actual);

	// and keep a tail window for a shutdown fade
	if (mTailWindow != NULL)
	  mTailWindow->add(mOutput, actual);

	memcpy(buffer, mOutput, (actual * mChannels) * sizeof(float));
	return actual;
}

PUBLIC void StreamPlugin::split(float* source, float* left, float* right, long frames)
{
    float* src = source;
    for (int i = 0 ; i < frames ; i++) {
        left[i] = *src++;
        right[i] = *src++;
    }
}

PRIVATE void StreamPlugin::merge(float* left, float* right, float* output, long frames)
{
    float* dest = output;
    for (int i = 0 ; i < frames ; i++) {
        *dest++ = left[i];
        *dest++ = right[i];
    }
}

/**
 * Setup a StreamPlugin Startup Fade.
 *
 * - The plugin must be in a flushed state
 * - New content begins feeding into the plugin
 * - The output of the plugin is monitored until the first non-zero sample
 * - On detction of the first non-zero sample, an up fade is applied
 * - The up fade completes, the plugin proceeds normally
 */
PUBLIC void StreamPlugin::startupFade()
{
	mStartupFade = true;
	mStartupFadeOffset = 0;
}

/**
 * If a startup fade is active, detect the first non-zero sample comming
 * out of the plugin, and begin a fade from there.
 * This must be called ONLY if the fade has been properly initialized.
 */
PRIVATE void StreamPlugin::doStartupFade(float* output, long frames)
{
	if (mStartupFade) {
		// locate the first frame containing a non-zero sample
		float* start = NULL;
		float* ptr = output;
		long offset = 0;

		for (int i = 0 ; i < frames && start == NULL ; i++) {
			for (int j = 0 ; j < mChannels && start == NULL ; j++) {
				if (ptr[j] != 0.0) {
					start = ptr;
					offset = i;
				}
			}
			ptr += mChannels;
		}
				
		if (start != NULL) {
			long avail = frames - offset;
			int range = AudioFade::getRange();
			long need = range - mStartupFadeOffset;
			if (need <= 0) {
				Trace(1, "StreamPlugin::doStartFade invalid fade offset!\n");
				mStartupFade = false;
			}
			else {
				long toFade = (avail < need) ? avail : need;
				AudioFade::fade(start, mChannels, 0, toFade, mStartupFadeOffset, true);
				mStartupFadeOffset += toFade;
				if (mStartupFadeOffset >= range)
				  mStartupFade = false;
			}
			// keep this zero once we've finished to avoid debugger confusion
			if (!mStartupFade)
			  mStartupFadeOffset = 0;
		}
	}
}

/**
 * Create a shutdown fade tail and transfer it into the FadeTail object for
 * eventual transfer into the output stream.
 *
 * This is an experimental technique that relies on keeping a copy of the audio
 * that was last sent out from the plugin.  To produce the fade tail, we extract
 * a section of the tail window as large as the fade range, reverse it, then fade it.
 * This isn't a true "forward" fade tail but it is a lot easier to produce than
 * making the output stream keep feeding us content until we have enough to drain.
 * If this works, consider using it for other tails.
 *
 * !! Don't really like the dependency on FadeTail but avoids having to 
 * deal with temporary buffer ownership.
 *
 * First try to produce a tail from the currently buffered content.  If there
 * isn't enough, there may be samples passed in through the FadeTail object, 
 * feed those.  If there still isn't enough, punt and do a reverse fade using
 * our tail window.
 */
PUBLIC void StreamPlugin::captureFadeTail(FadeTail* tail)
{
	float buffer[AUDIO_MAX_FADE_FRAMES * AUDIO_MAX_CHANNELS];
	int range = AudioFade::getRange();

	// add the tail given to us, it's possible this isn't enough
	long added = tail->play(buffer, range);
	putFrames(buffer, added);
	tail->reset();

	// see what we have left
	long avail = getAvailableFrames();

	if (avail >= range) {
		// we're in luck, there is enough
		avail = getFrames(buffer, range);
		if (avail < range) {
			// but you lied!!
			Trace(1, "StreamPlugin lied about available frames\n");
		}
	}
	else {
		// TODO: could try feeding zeros until we get something, if the internal
		// buffers are large may be enough in there if we push hard enough...
	}

	if (avail >= range) {
		AudioFade::fade(buffer, mChannels, 0, range, 0, false);
		tail->add(buffer, range);
	}
	else {
		// not enough, punt and do a reverse tail
		if (mTailWindow == NULL)
		  Trace(1, "Attempt to capture plugin fade tail with no tail window!\n");
		else {
			Trace(1, "StreamPlugin had to use a reverse fade tail!\n");
			long frames = mTailWindow->reverseFade(buffer);
			tail->add(buffer, frames);
		}
	}
}

/**
 * Expected to be overloaded to return the number of frames available in the 
 * internal buffers.  Used when capturing a fade tail.
 */
PUBLIC long StreamPlugin::getAvailableFrames()
{
	return 0L;
}

PUBLIC long StreamPlugin::getFrames(float* buffer, long frames)
{
	return 0L;
}

PUBLIC void StreamPlugin::putFrames(float* buffer, long frames)
{
}

/****************************************************************************
 *                                                                          *
 *                                PITCH PLUGIN                              *
 *                                                                          *
 ****************************************************************************/

PUBLIC PitchPlugin::PitchPlugin(int sampleRate)
    : StreamPlugin(sampleRate)
{
    mPitch = 1.0f;
}

PUBLIC PitchPlugin::~PitchPlugin()
{
}

PUBLIC float PitchPlugin::semitonesToRatio(int semitones)
{
    // SoundTouch does it like this
    // (float)exp(0.69314718056f * (semis / 12.0f));

	// smb did this
    return (float)pow(2.0, semitones / 12.0);
}

PUBLIC int PitchPlugin::ratioToSemitones(float ratio)
{
    // !! do something
	// is this the same as the rate shift calculations?
    return 0;
}

/**
 * Set the shift rate.
 */
PUBLIC void PitchPlugin::setPitch(float ratio)
{
	if (ratio != mPitch) {
		// !! should be doing some bounds checking on this
		mPitch = ratio;
		mPitchStep = ratioToSemitones(ratio);
		updatePitch();
	}
}

/**
 * Convenience method to allow the shift to be specified in semitones.
 * For example -5 would be a fifth down.
 */
PUBLIC void PitchPlugin::setPitch(int semitones)
{
	if (semitones != mPitchStep) {
		mPitchStep = semitones;
		mPitch = semitonesToRatio(semitones);
		updatePitch();
	}
}

PUBLIC void PitchPlugin::setPitch(float pitch, int semitones)
{
	if (pitch != mPitch || semitones != mPitchStep) {
		mPitch = pitch;
		mPitchStep = semitones;
		updatePitch();
	}
}

PUBLIC float PitchPlugin::getPitchRatio()
{
    return mPitch;
}

PUBLIC int PitchPlugin::getPitchSemitones()
{
    return mPitchStep;
}

/**
 * Test function to simulate the processing of interrupt blocks
 */
#define ST_BLOCK 256
#define ST_CHANNELS 2

void PitchPlugin::simulate()
{
	float input[ST_BLOCK * ST_CHANNELS];
	float output[ST_BLOCK * ST_CHANNELS];
	long spill = 0;

	long frames = 1000000;

	mBlocks = 0;

	memset(input, 0, sizeof(input));

	// note that due to periodic underflow in SoundTouch, 
	// we may get less back but becuase we continue to cram zeros
	// into the input there will be some padding on the end
	long remainingInput = frames;
	long remainingOutput = frames;

	while (remainingOutput > 0) {

		// hmm, would be nice if process took two frame counts?
		long blocksize = ST_BLOCK;
		if (remainingOutput < blocksize)
		  blocksize = remainingOutput;

		// once we fully consume the input buffer, just stuff zeros
		if (remainingInput > 0 && remainingInput < blocksize)
		  blocksize = remainingInput;

		long processed = process(input, output, blocksize);
		//out->append(buffer, processed);

		remainingOutput -= processed;
		if (remainingInput > 0)
		  remainingInput -= blocksize;
		else
		  spill += processed;

		// if we've been receiving samples but suddenly stop then
		// assume we're done, but have to flush the fifo first
		if (remainingOutput < frames && processed == 0) {
			// this should no longer happen now that we keep feeding zeros!!
			printf("PitchPlugin processing halted early %ld remaining!\n", remainingOutput);
			remainingOutput = 0;
		}
	}

	if (spill)
	  printf("Processed %ld frames after consuming input\n", spill);
}

Audio* PitchPlugin::processToAudio(AudioPool* pool, float* input, long frames)
{
	Audio* out = pool->newAudio();
	float buffer[ST_BLOCK * ST_CHANNELS];
	float empty[ST_BLOCK * ST_CHANNELS];
	long spill = 0;

	mBlocks = 0;

	// since input will be consumed before we receive all of the output
	// have to keep pushing empty bits until the fifo is flushed
	memset(empty, 0, sizeof(empty));

	// note that due to periodic underflow, we'll actually get less back
	// but becuase we continue to cram zeros into the input there will
	// be some padding on the end
	long remainingInput = frames;
	long remainingOutput = frames;

	while (remainingOutput > 0) {

		// hmm, would be nice if process took two frame counts?
		long blocksize = ST_BLOCK;
		if (remainingOutput < blocksize)
		  blocksize = remainingOutput;

		// once we fully consume the input buffer, just stuff zeros
		if (remainingInput > 0 && remainingInput < blocksize)
		  blocksize = remainingInput;

		long processed = process(input, buffer, blocksize);
		out->append(buffer, processed);

		remainingOutput -= processed;
		if (remainingInput > 0) {
			remainingInput -= blocksize;
			if (remainingInput > 0)
			  input += (blocksize * ST_CHANNELS);
			else {
				// start sending zeros once the input buffer is consumed
				input = empty;
			}
		}
		else {
			spill += processed;
		}

		// if we've been receiving samples but suddenly stop then
		// assume we're done, but have to flush the fifo first
		if (remainingOutput < frames && processed == 0) {
			// this should no longer happen now that we keep feeding zeros!!
			printf("PitchPlugin processing halted early with %ld remaining!\n", 
				   remainingOutput);
			remainingOutput = 0;
		}
	}

	if (spill > 0)
	  printf("Processed %ld frames after consuming input\n", spill);

	return out;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
