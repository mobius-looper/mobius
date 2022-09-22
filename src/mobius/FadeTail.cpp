/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Helper class for Layer to keep track of a short segment of
 * recorded audio over which a deferred fade may need to be applied.
 *
 */

#include <stdio.h>
#include <memory.h>

#include "Util.h"

// for AUDIO_MAX_CHANNELS, etc.
#include "AudioInterface.h"

#include "Stream.h"

/****************************************************************************
 *                                                                          *
 *   							  FADE TAIL                                 *
 *                                                                          *
 ****************************************************************************/

/**
 * The tail buffer is used to capture fade tails when the playback
 * cursor jumps around.  Recording into the tail may be offset relative
 * to our base location in the audio interrupt buffer.  So the buffer must
 * be ast least as large as one interrupt buffer, plus the fade length.
 */
PUBLIC FadeTail::FadeTail()
{
	mMaxFrames = AUDIO_MAX_FRAMES_PER_BUFFER + AUDIO_MAX_FADE_FRAMES;
	mTail = NULL;
	mStart = 0;
	mFrames = 0;
	mRecordOffset = 0;

	// !! channels: get this from the stream or pass it in
	mChannels = 2;

	long tailSamples = mMaxFrames * AUDIO_MAX_CHANNELS;
	mTail = new float[tailSamples];
	memset(mTail, 0, tailSamples * sizeof(float));
}

PUBLIC FadeTail::~FadeTail()
{
	delete mTail;
}

PUBLIC void FadeTail::reset()
{
	mStart = 0;
	mFrames = 0;
	mRecordOffset = 0;
}

PUBLIC int FadeTail::getFrames()
{
	return mFrames;
}

/**
 * Reset the record offset at the start of a new interrupt.
 */
PUBLIC void FadeTail::initRecordOffset()
{
	mRecordOffset = 0;
}

/**
 * Add the number of frames processed from the last interrupt block
 * to the record offset.  This will happen only if the interrupt block is being
 * broken up with events.
 */
PUBLIC void FadeTail::incRecordOffset(int frames)
{
	mRecordOffset += frames;
}

static int AddTailCount = 1;

/**
 * Add a captured tail to the tail buffer.
 */
PUBLIC void FadeTail::add(float* tail, long frames)
{
	if (frames > 0) {
		bool trace = false;
		char file[256];

		// loop already has a trace message, don't really need another
		Trace(4, "OutputStream::addTail tailFrame=%ld, framesToAdd=%ld tailCount %ld\n",
			  (long)mStart, frames, (long)AddTailCount);

		if (trace) {
			sprintf(file, "addTail%d.wav", AddTailCount);
			Audio::write(file, tail, frames);
		}

		// !! detect overflow

        int destFrame = mStart + mRecordOffset;
		if (destFrame >= mMaxFrames) {
			destFrame -= mMaxFrames;
			// better not require more than one wrap, mRecordOffset
			// calculation must be wrong
			if (destFrame >= mMaxFrames) {
				Trace(1, "Tail offset overflow!\n");
				// just leave it here, it probably won't sound good,
				// but it may be better than nothing
				destFrame = mStart;
			}
		}

        long framesToAdd = frames;

		long finalFrame = destFrame + framesToAdd;
		if (finalFrame > mMaxFrames) {
            // not enough room, have to wrap
			long availFrames = mMaxFrames - destFrame;
			long samples = availFrames * mChannels;
			float* dest = &mTail[destFrame * mChannels];

			memcpy(dest, tail, samples * sizeof(float));

			tail += samples;
            framesToAdd -= availFrames;
            destFrame = 0;
        }

		float* dest = &mTail[destFrame * mChannels];
		memcpy(dest, tail, (framesToAdd * mChannels) * sizeof(float));

		// may already have some, so increment only if we added more
		long newFrames = mRecordOffset + frames;
		if (newFrames > mFrames)
		  mFrames = newFrames;

		if (trace) {
			sprintf(file, "trackTail%d.wav", AddTailCount);
			Audio::write(file, mTail, mMaxFrames);
		}

		AddTailCount++;
    }
}

/**
 * A tail may be setup in order to do an orderly fade out from 
 * a layer before a disruptive transition. We're already
 * playing the new layer merge in the tail.  The tail is normally
 * processed with a down fade, the new layer is normally processed
 * with an up fade, the effect is a cross fade.
 *
 * If the tail was created by a Reverse transition, it will have
 * be captured in the correct direction, do not reverse it again.
 * I think the same is true for a Speed transition.
 *
 * This is expected to be called for an entire "track block" so
 * we can merge all the tails captured during the block, and can
 * therefore reset the record offset.
 */
PUBLIC long FadeTail::play(float* outbuf, long frames) 
{
	long playFrames = 0;

	if (mFrames > 0) {
		playFrames = mFrames;
		if (playFrames > frames)
		  playFrames = frames;

		// careful, "final" frame is 1+ the actual last frame index
		// since we're dealing with frame counts
		long finalFrame = mStart + playFrames;
		float* dest = outbuf;

		if (finalFrame > mMaxFrames) {
			long availFrames = mMaxFrames - mStart;
			dest = playRegion(dest, availFrames);
			playFrames -= availFrames;
		}

		playRegion(dest, playFrames);
	}

	mRecordOffset = 0;
	return playFrames;
}

/**
 * Helper for playTail, play a contiguous range of frames
 * in the tail.  Besides copying the frames, we also zero
 * the tail source so we can wrap and keep accumulating new tails.
 */
PRIVATE float* FadeTail::playRegion(float* outbuf, long frames)
{
	int samples = frames * mChannels;

	float* src = &mTail[mStart * mChannels];

	for (int i = 0 ; i < samples ; i++) {
		outbuf[i] += src[i];
		src[i] = 0.0f;
	}
	
	mStart += frames;
	mFrames -= frames;
	if (mStart >= mMaxFrames)
	  mStart = 0;

	// return the next output buffer location
	outbuf += samples;

	return outbuf;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
