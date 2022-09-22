/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * An object that maintains the state of an interation over
 * the buffers in an Audio object. 
 *
 */

#include <stdio.h>
#include <memory.h>
#include <math.h>

#include "Util.h"
#include "Trace.h"
#include "Audio.h"

/****************************************************************************
 *                                                                          *
 *   							 AUDIO BUFFER                               *
 *                                                                          *
 ****************************************************************************/

PUBLIC AudioBuffer::AudioBuffer()
{
	initAudioBuffer();
}

PUBLIC void AudioBuffer::initAudioBuffer()
{
	buffer = NULL;
	frames = 0;
	channels = 2;
}

PUBLIC AudioBuffer::~AudioBuffer()
{
    // we don't own the buffer!
}

void AudioBuffer::setBuffer(float* b, long f)
{
    buffer = b;
    frames = f;
	channels = 2;
}

/****************************************************************************
 *                                                                          *
 *   							  AUDIO FADE                                *
 *                                                                          *
 ****************************************************************************/

/**
 * Static array of fade adjustments.  These are succesively multipled
 * to an audio signal to raise it up from or lower it down to zero.
 * The range is configurable though it is almost always left at 128.
 */
int AudioFade::Range = AUDIO_DEFAULT_FADE_FRAMES;
float AudioFade::Ramp[AUDIO_MAX_FADE_FRAMES];
float AudioFade::Ramp128[128];
bool AudioFade::RampInitialized = false;
bool AudioFade::Ramp128Initialized = false;
	
/**
 * For smoothing MIDI CC changes, the maximum amount to increment as
 * we try to reach the new level for input, output, and feedback.
 */
float AudioFade::SmoothingInterval = (float)(1.0f / 128.0f);

/**
 * A count of the number of fades that have been performed.
 * Used to qualify file names when saving fade artifacts for debugging.
 * Safe for multiple plugins.
 */
static int FadeCount = 1;

/**
 * Static method to set the default fade range, and calculate the ramp.
 * This modifies the static fade array so in theory it could
 * cause glitches if there are several plugins active and one of them
 * is fading at this moment.  Rare since we almost never change the
 * fade range.
 */
void AudioFade::setRange(int range)
{
    // zero usually means "default" in the config files
    if (range <= 0)
      range = AUDIO_DEFAULT_FADE_FRAMES;

    else if (range < AUDIO_MIN_FADE_FRAMES)
      range = AUDIO_MIN_FADE_FRAMES;

    else if (range > AUDIO_MAX_FADE_FRAMES)
      range = AUDIO_MAX_FADE_FRAMES;

    if (range != Range) {
        Range = range;
        initRamp(Ramp, Range);
        RampInitialized = true;
    }
}

/**
 * The linear fade unit which we don't used any more is:
 *   unit = (100.0f / (float)(range - 1)) / 100;
 */
void AudioFade::initRamp(float* ramp, int range)
{
	// So fading can produce the same curve as incremental feedback
	// changes, the 127 value must be 1.0, formerly the unity
	// value would be "range" rather than "range - 1"
	range--;
	for (int i = 0 ; i <= range ; i++) {
		// the "sqares" approxomation to a logarithmic curve
		float value = (float)i / (float)range;
		ramp[i] = value * value;
	}
}

int AudioFade::getRange()
{
	return Range;
}

float* AudioFade::getRamp()
{
	if (!RampInitialized) {
		initRamp(Ramp, Range);
		RampInitialized = true;
	}
	return Ramp;
}

float* AudioFade::getRamp128()
{
	if (!Ramp128Initialized) {
		initRamp(Ramp128, 128);
		Ramp128Initialized = true;
	}
	return Ramp128;
}

float AudioFade::getRampValue(int level)
{
	float* ramp = getRamp128();
	return ramp[level];
}

AudioFade::AudioFade()
{
	init();
}

AudioFade::~AudioFade()
{
}

void AudioFade::init()
{
	enabled = false;
	active = false;
	startFrame = 0;
	up = false;
	processed = 0;
	baseLevel = 1.0f;
}

void AudioFade::enable(long frame, bool direction)
{
	if (active)
     Trace(1, "AudioFade: fade already in progress!\n");

	init();
	enabled = true;
	startFrame = frame;
	up = direction;
}

void AudioFade::setBaseLevel(float level)
{
	baseLevel = level;
}

void AudioFade::activate(bool direction)
{
    activate(0, direction);
}

void AudioFade::activate(int offset, bool direction)
{
	if (active)
	  Trace(1, "AudioFade: fade already in progress!\n");

	init();
	enabled = true;
    active = true;
	up = direction;
    processed = offset;
}

void AudioFade::copy(AudioFade* src)
{
	enabled = src->enabled;
	active = src->active;
	startFrame = src->startFrame;
	up = src->up;
	processed = src->processed;
	baseLevel = src->baseLevel;
}

/**
 * Apply a fade to a single sample.
 * !! Doing too much recalculation here, keep all this in 
 * sync with the processed counter.
 */
PUBLIC float AudioFade::fade(float sample)
{
	if (active) {
		float* ramp = getRamp();
		int index = ((up) ? processed : (Range - processed - 1));
		if (index >= 0) {
			float rampval = ramp[index];
			if (baseLevel != 1.0)
			  rampval = rampval + (baseLevel - (baseLevel * rampval));
			sample *= rampval;
		}
	}
	return sample;
}

/**
 * Increment the fade.
 * If a fade is active, increment the processed counter and if we
 * reach the end of the range deactivate it.  
 *
 * If the fade is enabled and inactive, and we've reached the
 * start frame, activate it.  In practice, the start frame checking
 * will only be relevant for down fades, up fades have to begin
 * immediately since we're recording.
 */
PUBLIC void AudioFade::inc(long frame, bool reverse)
{
	if (active) {
		processed++;
		if (processed >= Range) {
			enabled = false;
			active = false;
			processed = 0;
		}
	}
	else if (enabled && 
			 ((!reverse && frame >= startFrame) ||
			  (reverse && frame <= startFrame))) {
		// we can start now
		active = true;
	}
}

PRIVATE void AudioFade::saveFadeAudio(Audio* a, const char* type)
{
	char name[1024];
	sprintf(name, "fade-%d-%s-%s.wav",
			FadeCount,
			((up) ? "up" : "down"),
			type);

	a->write(name);
}

/**
 * Apply a forward fade to a block of frames in an AudioBuffer.
 */
PUBLIC void AudioFade::fade(AudioBuffer* buf, long curFrame)
{
	// the last source frame we processed
	long lastFrame = curFrame + buf->frames - 1;
	long fadeEndFrame = startFrame + Range - 1;

	if (fadeEndFrame < curFrame) {
		// we got past the fade without processing it
		// we used to see this in some modes that faded and muted
		// at the same time, but shouldn't any more
		Trace(1, "Encountered dormant fade!\n");
		init();
	}
	else if (startFrame <= lastFrame && fadeEndFrame >= curFrame) {
		// a portion in range

		float* fadeDest = buf->buffer;
		long destFrames = buf->frames;
		long fadeOffset = 0;

		if (startFrame < curFrame) {
			// truncate on the left 
			fadeOffset = curFrame - startFrame;
		}
		else {  
			// fade is at or after curFrame, shift output buffer
			long shift = startFrame - curFrame;
			fadeDest = (fadeDest + (shift * buf->channels));
			destFrames -= shift;
		}
			 
		// truncate on the right
		long fadeFrames = Range - fadeOffset;
		if (fadeFrames > destFrames)
		  fadeFrames = destFrames;
		else {
			// we've completed this fade (don't init() we still need stuff)
			enabled = false;
			active = false;
		}

		// perform the fade
		char filename[128];
		Audio* save = NULL;

		bool trace = false;
		if (trace) {
			printf("Layer fade %s: %ld offset %ld frames %s\n",
				   ((up) ? "up" : "down"), 
				   fadeOffset, fadeFrames,
				   ((enabled) ? "" : "finished"));
			fflush(stdout);
			save = new Audio();
			save->put(fadeDest, fadeFrames, 0);
			sprintf(filename, "fade-before-%d.wav", FadeCount);
			save->write(filename);
		}

		fade(fadeDest, buf->channels, 0, fadeFrames, fadeOffset, up);

		if (save != NULL) {
			save->reset();
			save->put(fadeDest, fadeFrames, 0);
			sprintf(filename, "fade-after-%d.wav", FadeCount);
			save->write(filename);
			delete save;
		}
		FadeCount++;
	}
}

/**
 * Apply a fade to a range of frames.  
 *
 * buffer - the buffer-o-frames
 * channels - channels per frame
 * startFrame - the offset within the buffer to begin the fade
 * frames - the number of frames to fade
 * fadeOffset - the relative offset of the first frame to be processed
 *              within the fade range.
 */
PUBLIC void AudioFade::fade(float* buffer, int channels, 
							long startFrame, long frames, 
							long fadeOffset, bool up)
{
	long samples = frames * channels;
	float* ptr = &buffer[startFrame * channels];
	float* ramp = getRamp();
	int rampIndex = ((up) ? fadeOffset : (Range - fadeOffset - 1));
	int incIndex = ((up) ? 1 : -1);

 	for (int i = 0 ; i < samples && rampIndex < Range ; i += channels) {
		float rampval = ramp[rampIndex];
		for (int j = 0 ; j < channels ; j++) {
			float sample = *ptr;
			sample *= rampval;
			*ptr = sample;
			ptr++;
		}
		rampIndex += incIndex;
	}
}

/**
 * The "match" argument is used in special cases where we are applying
 * a fade to content that will be merged with a copy of the same content
 * that was copied at a reduced level (i.e. feedback).  matchLevel is
 * the level of feedback that was applied from 0 to 127, with 127 meaning
 * that this is a normal fade.
 */
PUBLIC void AudioFade::fade(float* buffer, int channels, 
							long startFrame, long frames, 
							long fadeOffset, bool up,
							float adjust)
{
	long samples = frames * channels;
	float* ptr = &buffer[startFrame * channels];
	float* ramp = getRamp();
	int rampIndex = ((up) ? fadeOffset : (Range - fadeOffset - 1));
	int incIndex = ((up) ? 1 : -1);

 	for (int i = 0 ; i < samples && rampIndex < Range ; i += channels) {
		float rampval = ramp[rampIndex] * adjust;
		for (int j = 0 ; j < channels ; j++) {
			float sample = *ptr;
			sample *= rampval;
			*ptr = sample;
			ptr++;
		}
		rampIndex += incIndex;
	}
}

/**
 * A different kind of leveling fade, used to reduce the starting level
 * of a block.  The baseLevel argument has the multiplier for the
 * first frame if we're fading up, or the last frame if we're fading down.
 * The result is that an up fade will begin at the baseLevel and raise to 1.0,
 * and a down fade will begin at 1.0 and descend to baseLevel.
 */
PUBLIC void AudioFade::fadePartial(float* buffer, int channels, 
								   long startFrame, long frames, 
								   long fadeOffset, bool up,
								   float baseLevel)
{
	long samples = frames * channels;
	float* ptr = &buffer[startFrame * channels];
	float* ramp = getRamp();
	int rampIndex = ((up) ? fadeOffset : (Range - fadeOffset - 1));
	int incIndex = ((up) ? 1 : -1);

 	for (int i = 0 ; i < samples && rampIndex < Range ; i += channels) {

		// here's the magic, go through the ramp factoring in decreasing
		// amounts of the baseLevel
		// sample * ramp + (base - (base * ramp))
		float rampval = ramp[rampIndex];
		rampval = rampval + (baseLevel - (baseLevel * rampval));

		for (int j = 0 ; j < channels ; j++) {
			float sample = *ptr;
			sample *= rampval;
			*ptr = sample;
			ptr++;
		}
		rampIndex += incIndex;
	}
}

/****************************************************************************
 *                                                                          *
 *   								CURSOR                                  *
 *                                                                          *
 ****************************************************************************/

AudioCursor::AudioCursor()
{
	init();
}

AudioCursor::AudioCursor(const char* name)
{
	init();
	setName(name);
}

AudioCursor::AudioCursor(const char* name, Audio* a)
{
	init();
	setName(name);
	setAudio(a);
}

AudioCursor::~AudioCursor()
{
	delete mName;
}

void AudioCursor::init()
{
	mName = NULL;
	mAudio = NULL;
	mVersion = 0;
	mReverse = false;
	mFrame = 0;
	mBufferIndex = 0;
	mBufferOffset = 0;
	mBuffer = NULL;
	mAutoExtend = false;
    mOverflowTraced = false;
	mFade.init();
}

void AudioCursor::setName(const char* name)
{
	delete mName;
	mName = CopyString(name);
}

void AudioCursor::setReverse(bool b) 
{
	mReverse = b;
}

bool AudioCursor::isReverse()
{
    return mReverse;
}

/**
 * Decache buffer location.
 */
PRIVATE void AudioCursor::decache()
{
	mBuffer = NULL;
	mBufferIndex = 0;
	mBufferOffset = 0;
	mVersion = 0;
}

/**
 * Can be called for independent cursors that iterate over
 * more than one Audio.
 */
void AudioCursor::setAudio(Audio* a)
{
	if (mAudio != a) {
		mAudio = a;
		decache();
		mFrame = 0;
	}
}

Audio* AudioCursor::getAudio()
{
    return mAudio;
}

/**
 * The usual reflection utility, though we don't do any reflect
 * in in this class, it is convenient to have the method here
 * for higher level classes.
 */
long AudioCursor::reflectFrame(long frame)
{
    if (mReverse)
      frame = mAudio->getFrames() - frame - 1;
    return frame;
}

/**
 * Reset all cursor state.
 */
PUBLIC void AudioCursor::reset()
{
	mFrame = 0;
	mReverse = false;
	decache();
	mFade.init();
}

PUBLIC void AudioCursor::setAutoExtend(bool b)
{
	mAutoExtend = b;
}

/****************************************************************************
 *                                                                          *
 *   							   LOCATION                                 *
 *                                                                          *
 ****************************************************************************/

PUBLIC long AudioCursor::getFrame()
{
	return mFrame;
}

/**
 * Set the cursor to a specific frame.  This may be negative if
 * we've reflected the requested frame in reverse mode.
 * Just decache the location if the frame is different and let
 * locateFrame or prepareFrame do the heavy lifting later.
 */
PUBLIC void AudioCursor::setFrame(long frame)
{
    if (frame != mFrame) {
		mFrame = frame;
		decache();
    }
}

/**
 * Locate the position of a frame for reading.
 * If the frame is out of range, DO NOT extend the audio.
 * This differs from prepareFrame which will extend.
 * If the buffer is already set, assume it is current and
 * left behind by setFrame.
 *
 * If we've got a sparse audio with gaps in the buffer list,
 * we may end up here on each get() call until we advance to the
 * point where there is a non-null buffer.  If we also have a non-zero
 * buffer offset assume that we've already checked.
 */
PRIVATE void AudioCursor::locateFrame()
{
	if (mBuffer == NULL && mBufferOffset == 0) {
		// Trace(2, "locateFrame\n");
		mAudio->locate(mFrame, &mBufferIndex, &mBufferOffset);
		mBuffer = mAudio->getBuffer(mBufferIndex);
		mVersion = mAudio->mVersion;
	}
}

/**
 * Called when we need to ensure that the frame identified by
 * the current mFrame counter is writable.  This is called whenever
 * put() is called and mBuffer is NULL.
 *
 */
PRIVATE void AudioCursor::prepareFrame()
{
    if (mBuffer == NULL) {
		// Trace(2, "prepareFrame\n");
        // potentially complex extension
        mFrame = mAudio->prepareFrame(mFrame, &mBufferIndex, &mBufferOffset, 
									  &mBuffer);
		mVersion = mAudio->mVersion;
    }
    else if (mFrame < 0) {
        // incFrame located the frame before the beginning of the range,
        // but we don't extend until we know we need it.  Bypass
        // the complex prepareFrame logic and make direct changes.

        if (mFrame != -1)
          Trace(1, "AudioCursor: start frame adjust anomoly\n");

        mAudio->mStartFrame += mFrame;
        mAudio->mFrames -= mFrame;
        mFrame = 0;

		if (mAudio->mStartFrame < 0) {
			// logic error somewhere, mBuffer should have been NULL
			Trace(1, "Negative start frame!\n");
			decache();
		}
    }
    else if (mFrame >= mAudio->mFrames) {
        // incFrame located the frame after the end of the range
        mAudio->mFrames = mFrame + 1;
    }
}

/**
 * Move to the next frame.
 *
 * Try to keep mBuffer set, but if we fall off the end, just leave
 * it NULL and let prepareFrame do the work if it needs to.
 *
 * If this is a reverse play cursor, mFrame may go negative, just
 * keep incrementing and ignore it.  If this is a reverse record cursor,
 * we may go to -1 but we should never start negative since prepareFrame
 * will keep adjusting it so that we're always on frame zero.
 *
 * Note that we assume that more than one cursor cannot be incrementing
 * simultaneously so we don't have to keep checking the Audio version number 
 * here, only in the public methods.
 */
PRIVATE void AudioCursor::incFrame()
{
	int channels = mAudio->mChannels;

	if (mReverse) {

        mFrame--;

        if (mFrame < 0 && !mAutoExtend) {
            // Ran of the edge of a non-extendable cursor. This can
            // happen once when we read the last frame, but we shouldn't
            // go beyond that.
            if (mFrame < -1)
              Trace(1, "AudioCursor: reverse record frame too negative\n");
			decache();
        }
        else {
            // we're either in range, or auto extending
            if (mFrame < -1)
              Trace(1, "AudioCursor: reverse record frame too negative\n");

            mBufferOffset -= channels;
            if (mBufferOffset < 0) {
                // fell off the end of this buffer
                mBufferIndex--;
                mBufferOffset = mAudio->mBufferSize - channels;
                int bufferCount = mAudio->mBufferCount;
                if (mBufferIndex >= 0 && mBufferIndex < bufferCount &&
                    bufferCount > 0) {
                    // may or may not be a buffer here, 
                    // wait and let prepareFrame allocate it, since
                    // we may not need it
                    mBuffer = mAudio->mBuffers[mBufferIndex];
                }
                else {
                    // fell off the edge of the index
                    // null the buffer and let prepareFrame handle it
					decache();
                }
            }
        }
	}
	else {

		mFrame++;

        if (mFrame >= mAudio->mFrames && !mAutoExtend) {
            // Ran of the edge of a non-extendable cursor. This can
            // happen once when we read the last frame, but we shouldn't
            // go beyond that. 
			if (mFrame > mAudio->mFrames) {
                // Once this happens, it can happen a lot so only trace once
                if (!mOverflowTraced) {
                    Trace(1, "AudioCursor: %s, play frame overflow\n", mName);
                    //printf("Overflow %p!\n", mAudio);
                    mOverflowTraced = true;
                }
            }
			decache();
        }
        else {
            // we're in range or extending
            mBufferOffset += channels;
            if (mBufferOffset >= mAudio->mBufferSize) {
                mBufferIndex++;
                mBufferOffset = 0;
                if (mBufferIndex < mAudio->mBufferCount)
                    mBuffer = mAudio->mBuffers[mBufferIndex];
                else {
                    // fell off the edge of the index
                    // let prepareFrame handle it
                    decache();
                }
            }
		} 
	}

	mFade.inc(mFrame, mReverse);
}

/****************************************************************************
 *                                                                          *
 *   								 GET                                    *
 *                                                                          *
 ****************************************************************************/


/**
 * Copy a range of frames into an audio buffer.
 *
 * ?? Try to push level adjustments up to the stream level?
 */
PUBLIC void AudioCursor::get(AudioBuffer* buf, float level)
{
	int channels = buf->channels;
    float* dest = buf->buffer;
    long length = buf->frames;

	// if the version number changed, we have to recalculate position
	if (mVersion != mAudio->mVersion)
	  decache();

	locateFrame();

	for (int i = 0 ; i < length ; i++) {
		get(buf, dest, level);
		if (dest != NULL)
		  dest += channels;
	}
}

PUBLIC void AudioCursor::get(AudioBuffer* buf)
{
	get(buf, 1.0);
}

PUBLIC void AudioCursor::get(AudioBuffer* buf, Audio* a, long frame, 
							 float level)
{
	setAudio(a);
	setFrame(frame);
	get(buf, level);
}

/**
 * Copy the next frame into a buffer and increment the frame position.
 * Note that when playing in reverse, we can't just iterate over
 * the samples in reverse order because that would swap the left and
 * right channels.  
 *
 * Go through the machinery even if we don't have a buffer since
 * we may be iterating through a sparse array, but eventually find
 * some content.
 */
PRIVATE void AudioCursor::get(AudioBuffer* buf, float* dest, float level)
{
	// formerly were able to pass a replace flag through an AudioContext,
	// don't really need this, but if we did it would be better to pass
	// in a feedback value that could be 0
	bool replace = false;
	bool doLevel = (level != 1.0f);

	for (int i = 0 ; i < buf->channels ; i++) {
		float sample = 0.0f;
		if (mBuffer != NULL)
		  sample = mBuffer[mBufferOffset + i];
            
		if (doLevel)
		  sample *= level;

		sample = mFade.fade(sample);

		if (dest != NULL) {
			if (replace)
			  dest[i] = sample;
			else
			  dest[i] += sample;
		}
	}

	incFrame();
}

/****************************************************************************
 *                                                                          *
 *   								 PUT                                    *
 *                                                                          *
 ****************************************************************************/

PUBLIC void AudioCursor::put(AudioBuffer* buf, AudioOp op)
{
	int channels = buf->channels;
    float* src = buf->buffer;
	long frames = buf->frames;

	// if the version number changed, we have to recalculate position
	if (mVersion != mAudio->mVersion)
	  decache();

	for (int i = 0 ; i < frames ; i++) {

		// since we're recording, have to flesh out the buffers as we go
		prepareFrame();

		for (int j = 0 ; j < channels ; j++) {
			float sample = (src != NULL) ? src[j] : 0.0f;

			sample = mFade.fade(sample);

			if (op == OpReplace)
			  mBuffer[mBufferOffset + j] = sample;
			else if (op == OpRemove)
			  mBuffer[mBufferOffset + j] -= sample;
			else
			  mBuffer[mBufferOffset + j] += sample;
		}
		
		incFrame();

		if (src != NULL)
		  src += channels;
	}
}

PUBLIC void AudioCursor::put(AudioBuffer* buf, AudioOp op, long frame)
{
	setFrame(frame);
	put(buf, op);
}

PUBLIC void AudioCursor::put(AudioBuffer* buf, AudioOp op, Audio* a, 
							 long frame)
{
	setAudio(a);
	setFrame(frame);
	put(buf, op);
}

/****************************************************************************
 *                                                                          *
 *   								 FADE                                   *
 *                                                                          *
 ****************************************************************************/

PUBLIC void AudioCursor::startFadeIn()
{
	mFade.activate(true);
}

/**
 * Deprecated: we no longer use "scheduled" fades.
 */
PUBLIC void AudioCursor::setFadeIn(long frame)
{
	mFade.enable(frame, true);
}

PUBLIC bool AudioCursor::isFading()
{
	return mFade.active;
}

/**
 * Deprecated: we no longer use "scheduled" fades.
 */
PUBLIC void AudioCursor::setFadeOut(long frame)
{
	int range = AudioFade::getRange();
	long start = frame < range;
	if (start < 0) {
		range += start;
		start = 0;
	}
	if (range > 0) {
		mFade.enable(start, false);
		mFade.setRange(range);
	}
}

PUBLIC void AudioCursor::transferFade(AudioCursor* dest)
{
	dest->mFade = this->mFade;
	mFade.init();
}

PUBLIC void AudioCursor::resetFade()
{
    mFade.init();
}

/**
 * Perform a permanent fade from the current position.
 * We use the local AudioFade object, so there must not be another
 * active fade at this time.
 */
PUBLIC void AudioCursor::fade(int offset, int frames, bool up)
{
	fade(offset, frames, up, 1.0f);
}

PUBLIC void AudioCursor::fade(int offset, int frames, bool up, float baseLevel)
{
	locateFrame();
	if (mBuffer != NULL) {
		mFade.activate(offset, up);
		mFade.setBaseLevel(baseLevel);

		int channels = mAudio->mChannels;

		for (int i = 0 ; i < frames ; i++) {
			for (int j = 0 ; j < channels ; j++) {
				// if mBuffer goes null, we fell off the end
				if (mBuffer != NULL) {
					float* loc = &(mBuffer[mBufferOffset + j]);
					*loc = mFade.fade(*loc);
				}
			}
			incFrame();
		}

		mFade.init();
	}
}

PUBLIC void AudioCursor::fade(bool up)
{
    fade(0, AudioFade::getRange(), up);
}

/**
 * Perform an up fade at the beginning.
 * Special operation for Audio::splice, would be nice if it could
 * be encapsulated there?
 */
PUBLIC void AudioCursor::fadeIn()
{
	setFrame(0);
	fade(true);
}

PUBLIC void AudioCursor::fadeIn(Audio* a)
{
	setAudio(a);
    fadeIn();
}

/**
 * Perform a fade in of the front of the audio.
 * Special operation for Audio::splice, would be nice if it could
 * be encapsulated there?
 */
PUBLIC void AudioCursor::fadeOut()
{
	int range = AudioFade::getRange();
	long start = mAudio->getFrames() - range;
	if (start < 0) {
		range += start;
		start = 0;
	}
	if (range > 0) {
		setFrame(start);
		fade(false);
	}
}
	
PUBLIC void AudioCursor::fadeOut(Audio* a)
{
    setAudio(a);
    fadeOut();
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
