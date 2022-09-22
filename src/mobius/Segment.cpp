/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * An object maintaining a "window" into a Layer.  Used by layers
 * define their content by referencing other layers.  See commentary
 * in Layer.cpp for more.
 *
 */

#include <stdio.h>
#include <memory.h>

#include "Util.h"

#include "Layer.h"
#include "Mobius.h"
#include "Segment.h"

/****************************************************************************
 *                                                                          *
 *   							   SEGMENT                                  *
 *                                                                          *
 ****************************************************************************/

Segment::Segment()
{
    init();
}

Segment::Segment(Layer* src)
{
    init();
    if (src != NULL) {
        mLayer = src;
        mLayer->incReferences();
        mFrames = src->getFrames();
    }
}

Segment::Segment(Audio* src)
{
    init();
    if (src != NULL) {
        mAudio = src;
		mCursor = new AudioCursor();
        mFrames = src->getFrames();
    }
}

Segment::Segment(Segment* src)
{
    init();
    if (src != NULL) {
		// we can't clone local Audio, shouldn't be an issue now
		// since we don't use local segment Audio
		if (src->mAudio != NULL)
		  Trace(1, "Unable to clone segment audio\n");

		if (src->mLayer != NULL) {
			mLayer = src->mLayer;
			mLayer->incReferences();
		}

		mOffset = src->mOffset;
		mStartFrame = src->mStartFrame;
		mFrames = src->mFrames;
		mFeedback = src->mFeedback;
        mReverse = src->mReverse;
        mLocalCopyLeft = src->mLocalCopyLeft;
        mLocalCopyRight = src->mLocalCopyRight;
        mFadeLeft = src->mFadeLeft;
        mFadeRight = src->mFadeRight;
    }
}

Segment::~Segment()
{
	delete mAudio;
	delete mCursor;
    if (mLayer != NULL)
	  mLayer->free();
}

void Segment::init()
{
    mNext = NULL;
    mOffset = 0;
    mLayer = NULL;
    mAudio = NULL;
	mCursor = NULL;
    mStartFrame = 0;
    mFrames = 0;
    mFeedback = 127;
    mReverse = false;
	mLocalCopyLeft = 0;
	mLocalCopyRight = 0;
	mFadeLeft = false;
	mFadeRight = false;
    mSaveFadeLeft = false;
    mSaveFadeRight = false;
	mUnused = false;
}

void Segment::setNext(Segment* seg)
{
    mNext = seg;
}

Segment* Segment::getNext()
{
    return mNext;
}

void Segment::setOffset(long f)
{
    mOffset = f;
}

long Segment::getOffset()
{
    return mOffset;
}

void Segment::setLayer(Layer* l)
{
    if (mLayer != NULL)
	  mLayer->free();
    mLayer = l;
    mLayer->incReferences();
}

Layer* Segment::getLayer()
{
    return mLayer;
}

void Segment::setAudio(Audio* a)
{
	delete mAudio;
    mAudio = a;
	if (mCursor == NULL)
	  mCursor = new AudioCursor();
}

Audio* Segment::getAudio()
{
    return mAudio;
}

void Segment::setStartFrame(long f)
{
    mStartFrame = f;
}

long Segment::getStartFrame()
{
    return mStartFrame;
}

void Segment::setFrames(long l)
{
    mFrames = l;
}

long Segment::getFrames()
{
    return mFrames;
}

void Segment::setFeedback(int f)
{
    mFeedback = f;
}

int Segment::getFeedback()
{
    return mFeedback;
}

void Segment::setReverse(bool b)
{
    mReverse = b;
}

bool Segment::isReverse()
{
    return mReverse;
}

/**
 * The number of frames to the the left of this segment that 
 * have been copied into the owning Layer's local Audio during
 * flattening.  When this is non-zero, it indicates that the
 * segment does not need a left fade because the adjacent
 * content is found in the layer.
 */
void Segment::setLocalCopyLeft(long frames)
{
	mLocalCopyLeft = frames;
}

long Segment::getLocalCopyLeft()
{
	return mLocalCopyLeft;
}

/**
 * The number of frames to the right of this segment that 
 * have been copied into the owning Layer's local Audio during
 * flattening.  When this is non-zero, it indicates that the
 * segment does not need a right fade because the adjacent
 * content is found in the layer.
 */
void Segment::setLocalCopyRight(long frames)
{
	mLocalCopyRight = frames;
}

long Segment::getLocalCopyRight()
{
	return mLocalCopyRight;
}

/**
 * When set indiciates that we must fade the left edge of the segment.
 */
void Segment::setFadeLeft(bool b)
{
	mFadeLeft = b;
}

bool Segment::isFadeLeft()
{
	return mFadeLeft;
}

void Segment::setFadeRight(bool b)
{
	mFadeRight = b;
}

bool Segment::isFadeRight()
{
	return mFadeRight;
}

/**
 * Temporarily save fades to verify that they were being 
 * calculated correctly.
 */
void Segment::saveFades()
{
    mSaveFadeLeft = mFadeLeft;
    mSaveFadeRight = mFadeRight;
}

bool Segment::isSaveFadeRight()
{
    return mSaveFadeRight;
}

bool Segment::isSaveFadeLeft()
{
    return mSaveFadeLeft;
}

void Segment::setUnused(bool b)
{
    mUnused = b;
}

bool Segment::isUnused()
{
    return mUnused;
}

/**
 * Logically truncate the segment on the left with the remainder
 * maintaining the same relative position within the owning layer.
 *
 * When the copy flag is true, it indiciates that we're trimming because
 * this number of frames on the left edge of the segment have been
 * copied into the local Audio and we no longer need to reference them.
 * This increments the local copy count so we know that we don't need
 * a left fade on this segment.
 *
 * If we're not copying, this will normally require a left fade since
 * we've occluded the left edge of the reference.  The only exception 
 * to that rule would be if we allowed the original edge to be restored
 * later, or if we added another segment that included the occluded content.
 * This can't happen now, but I'd like to allow it so this may be 
 * recalculated by Layer::compileSegmentFades.
 */
void Segment::trimLeft(long frames, bool copy)
{
    mOffset += frames;
    mStartFrame += frames;
    mFrames -= frames;
    if (copy) {
        mLocalCopyLeft += frames;
		// note that it must exceed the fade range before we can
		// turn off the fade
        if (mLocalCopyLeft >= AudioFade::getRange())
            mFadeLeft = false;
    }
    else {
        mLocalCopyLeft = 0;
        mFadeLeft = true;
    }
}

/**
 * Logically truncate the segment on the right while maintaining the
 * same relative position.
 */
void Segment::trimRight(long frames, bool copy)
{
	mFrames -= frames;
    if (copy) {
        mLocalCopyRight += frames;
        if (mLocalCopyRight >= AudioFade::getRange())
		  mFadeRight = false;
    }
    else {
        mLocalCopyRight = 0;
        mFadeRight = true;
    }
}

void Segment::dump(TraceBuffer* b)
{
	b->add("Segment: offset %ld start %ld length %ld feedback %d\n",
		   mOffset, mStartFrame, mFrames, mFeedback);

	b->incIndent();

	if (mAudio != NULL)
	  mAudio->dump(b);

	if (mLayer != NULL)
	  mLayer->dump(b);

	b->decIndent();
}

/**
 * Utility to see if this segment is aligned both to the end
 * of the containing layer and the end of the referenced layer.
 */
PUBLIC bool Segment::isAtEnd(Layer* parent)
{
	long localEnd = mOffset + mFrames;
	long refEnd = mStartFrame + mFrames;
	return (localEnd == parent->getFrames() && refEnd == mLayer->getFrames());
}

PUBLIC bool Segment::isAtStart(Layer* parent)
{
	// don't really need the parent layer
	return (mOffset == 0 && mStartFrame == 0);
}

/****************************************************************************
 *                                                                          *
 *   								FETCH                                   *
 *                                                                          *
 ****************************************************************************/

/**
 * Fetch the samples within range of an output buffer.
 * If we are in reverse, startFrame will be the beginning of the
 * reflected region.  The end of the region is starFrame+frames-1,
 * but it must be processed in reverse.
 * 
 * Factor segment feedback into the output level.
 * This isn't as simple as summing the feedback and output levels,
 * logically feedback is applied first, then output.  So feedback of 50%
 * and level of 50% would result in a 75% level (50%, then 50% of that)
 * !! I'm still not convinced this is right, the way the calculations
 * are handled now, we apply output level first, then accumulate feedback.
 * I think that should be effectively the same?  If not, then 
 * we'll need to pass feedback in LayerContext seperately and
 * combine down in Audio.
 * UPDATE: Shouldn't actually matter now that we're flattening layers.
 *
 * If the edges of the segment aren't exactly on the start or end, 
 * we normally apply fades.  This can't be done in the AudioCursor
 * because we can in theory could be using the same cursor traversing
 * through several levels of Segment and each may need a different fade.
 * So we fetch the referenced content into a local buffer, apply
 * the fade, then copy the fade buffer into the return buffer.
 * Hmm, it seems like if we though hard enough we could still
 * do this in the cursor, reinitializing the fade state at each level, 
 * but doing it here it is easier to see what's happening.
 * UPDATE: Now that play and copy cursors will only be used for one layer,
 * could make some optimizations.
 * 
 * Originally avoided a fade if we were exactly at the start or end
 * but with seamless overdubs across the loop point we may not
 * be faded nicely.  Don't however add a fade if the segment
 * contains the ENTIRE layer because there may be a seamless
 * overdub we want to preserve.
 */
void Segment::get(LayerContext* con, long startFrame, 
				  AudioCursor* cursor,  bool play)
{
	// Factor segment feedback into the output level
    float level = con->getLevel();
    if (mFeedback < 127) {
		// NOTE: previously used a linear ramp, changed to use
		// the same ramp as everyone else, may cause some unit tests
		// to fail?
        //float adjust = mFeedback / 127.0f;
		float* ramp = AudioFade::getRamp128();
		level = level * ramp[mFeedback];
    }

    // If level went to zero or then we're past audibility and can stop
	// the traversal.  Since we're dealing with floats here we usually
	// won't actually get to zero. 0.000062 is the 128 ramp value closest
	// to zero so assume we have to be at or beyond that.

    if (level > 0.000062) {
		// In case we need to fade, this needs to be at least as large
		// as the audio interrupt buffer.  Don't want one in every
		// Segment, and can't put it in the LayerContext since in theory 
		// we may have several levels of Segment to iterate over and 
		// each one requires its own buffer.  
		// !! Need a pool of smaller interrupt buffers, could
		// use the Audio buffer pool but those are too big
		float temp[AUDIO_MAX_FRAMES_PER_BUFFER * AUDIO_MAX_CHANNELS];
		float* buffer = con->buffer;
		long bufferFrames = con->frames;
        float saveLevel = con->getLevel();
        con->setLevel(level);

        // startFrame is from zero to our length, 
        // warp it relative to the underlying object
        long realStartFrame = startFrame + mStartFrame;
		long lastFrame = startFrame + bufferFrames - 1;

		// See if we need to do any fading, note that when flattening
		// we may have already faded some portion outside the edges
		bool fadeLeft = false;
		bool fadeRight = false;
		long fadeRange = AudioFade::getRange();
		long leftFadeRange = 0;
		long rightFadeRange = 0;

		if (mFadeLeft) {
			leftFadeRange = fadeRange - mLocalCopyLeft;
			if (leftFadeRange <= 0) {
				// should have turned this off by now
				Trace(mLayer, 1, "Detected obsolete segment left fade\n");
				mFadeLeft = false;
			}
			else
			  fadeLeft = startFrame < leftFadeRange;
		}

		if (mFadeRight) {
			rightFadeRange = fadeRange - mLocalCopyRight;
			if (rightFadeRange <= 0) {
				// should have turned this off by now
				Trace(mLayer, 1, "Detected obsolete segment right fade\n");
				mFadeRight = false;
			}
			else {
				long fadeOutStartFrame = mFrames - rightFadeRange;
				fadeRight = lastFrame >= fadeOutStartFrame;
			}
		}

		if (fadeLeft || fadeRight) {
			memset(temp, 0, sizeof(temp));
			con->buffer = temp;
		}
		  
		if (mLayer != NULL) {
			// Note that we must call getNoReflect here to avoid reflecting
			// the region again when in reverse.  Reflection only
			// happens the first tine Loop calls Layer::get
			// root is false from here on down, but pass along the play
			// flag so the layer can choose the right cursor
			mLayer->getNoReflect(con, realStartFrame, cursor, false, play);
		}
		else if (mAudio != NULL) {
			// Since we have a reflected region, we have to calculate
			// the end frame since AudioCursor iterates in reverse.
			long audioFrame = realStartFrame;
			if (con->isReverse())
			  audioFrame = realStartFrame + con->frames - 1;

			// use our own private cursor if none passed in
			AudioCursor* cur = ((cursor != NULL) ? cursor : mCursor);
			cur->setReverse(con->isReverse());
			cur->get(con, mAudio, audioFrame, con->getLevel());
		}

		if (fadeLeft) {

			bool up = true;
			long bufferOffset = 0;
			long fadeOffset = startFrame + mLocalCopyLeft;
			long fadeFrames = leftFadeRange - startFrame;
			if (fadeFrames > bufferFrames) {
				// can happen normally if we're close the end of an interrupt
				// just shorten our range
				fadeFrames = bufferFrames;
			}

			if (con->isReverse()) {
				// fade direction changes
				up = false;
				// the fade region is at the end of buffer
				bufferOffset = bufferFrames - fadeFrames;
				// and the fade offset reflects within the fade range
				long lastFadeOffset = fadeOffset + fadeFrames - 1;
				fadeOffset = fadeRange - lastFadeOffset - 1;
			}

			Trace(4, "Segment fade %s bufferOffset=%ld fadeOffset=%ld fadeFrames=%ld\n", 
				  ((up) ? "up" : "down"),
				  bufferOffset, fadeOffset, fadeFrames);
			AudioFade::fade(temp, con->channels, bufferOffset, 
							fadeFrames, fadeOffset, up);
		}

		if (fadeRight) {

			bool up = false;
			long bufferOffset = 0;
			long fadeOffset = 0;
			long fadeOutStartFrame = mFrames - rightFadeRange;

			if (startFrame < fadeOutStartFrame)
			  bufferOffset = fadeOutStartFrame - startFrame;
			else {
				// Not enough room to do a full fade, some must have been
				// done in the previous buffer to advance the offset
				fadeOffset = startFrame - fadeOutStartFrame;
			}

			// this is the maximum number of fade frames we have
			long fadeFrames = bufferFrames - bufferOffset;
			if (fadeFrames > rightFadeRange) {
				// often it will be more than we need
				fadeFrames = rightFadeRange;
			}

			if (con->isReverse()) {
				// region is at the start of the buffer
				up = true;
				bufferOffset = 0;
				// and the fade offset reflects within the fade range
				long lastFadeOffset = fadeOffset + fadeFrames - 1;
				fadeOffset = fadeRange - lastFadeOffset - 1;
			}

			Trace(4, "Segment fade %s bufferOffset=%ld fadeOffset=%ld fadeFrames=%ld\n", 
				  ((up) ? "up" : "down"),
				  bufferOffset, fadeOffset, fadeFrames);
			AudioFade::fade(temp, con->channels, 
							bufferOffset, fadeFrames, fadeOffset, up);
		}

		// after processing the fade(s) merge with the output
		if (fadeLeft || fadeRight) {
			long samples = bufferFrames * con->channels;
			for (int i = 0 ; i < samples ; i++) {
				float sample = buffer[i];
				sample += temp[i];
				buffer[i] = sample;
			}
		}

        con->setLevel(saveLevel);
		con->buffer = buffer;
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
