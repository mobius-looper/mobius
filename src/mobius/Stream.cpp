
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
 * Terminology:
 *
 * Audio Block
 *   A block of audio being prepared/consumed for the audio interrupt.
 *   These will be of fixed size, and may contain content from many
 *   different loops and layers.
 * 
 * Track Block
 *   A fragment of the Audio Block between Loop Events.  If there
 *   are no loop events within range, the Track Block will be the
 *   same size as the Audio Block.
 *
 * Layer Block
 *   A fragment of the Track Block to which content from a single
 *   layer will be placed by the output stream.  A Track Block
 *   may have several layer blocks.  Layer blocks are non-overlapping but
 *   do not have to be adjacent.
 */

#include <stdio.h>
#include <memory.h>
#include <math.h>

#include "Util.h"
#include "Trace.h"
#include "Audio.h"

#include "Event.h"
#include "Layer.h"
#include "Loop.h"
#include "Mobius.h"
#include "Resampler.h"
#include "Script.h"
#include "Stream.h"
#include "StreamPlugin.h"
#include "Synchronizer.h"

/****************************************************************************
 *                                                                          *
 *   							   SMOOTHER                                 *
 *                                                                          *
 ****************************************************************************/

Smoother::Smoother()
{
	reset();
}

Smoother::~Smoother()
{
	// mRamp is shared, do not delete
}

void Smoother::reset()
{
	mActive = false;
	mRamp = AudioFade::getRamp128();
	mStep = 0;
	mValue = 1.0;
	mStart = 1.0;
	mTarget = 1.0;
	mDelta = 0.0;
}

void Smoother::setValue(float value)
{
	reset();
	mValue = value;
	mStart = value;
	mTarget = value;
}

void Smoother::setTarget(float target)
{
	if (mTarget != target) {
		mActive = true;
		mStart = mValue;
		mTarget = target;
		mDelta = mTarget - mValue;
		// assume the ramp starts over, but if we thought hard enough
		// we could probably move the location in the current ramp?
		// could start from step 1, since we're already at the level we
		// dont need another another frame at the same level, but
		// this changes the tests files!
		mStep = 0;
	}
}

void Smoother::setTarget(int endLevel)
{
	setTarget(AudioFade::getRampValue(endLevel));
}

bool Smoother::isActive()
{
	return mActive;
}

float Smoother::getValue()
{
	return mValue;
}

float Smoother::getTarget()
{
	return mTarget;
}

void Smoother::advance()
{
	if (mActive) {
		mStep++;
		if (mStep < 127) {
			float change;
			if (mDelta > 0) {
				// add the delta in gradually
				change = mDelta * mRamp[mStep];
			}
			else {
				// when going down, reverse the ramp and subtract
				// the delta gradually
				change = mDelta * (1.0f - mRamp[127 - mStep]);
			}
			mValue = mStart + change;
		}
		else {
			// avoid denormalization and rounding error by assigning
			// the desired target once we reach the end of the ramp
			mValue = mTarget;
			mActive = false;
			mStep = 0;
		}
	}
}

/****************************************************************************
 *                                                                          *
 *   								STREAM                                  *
 *                                                                          *
 ****************************************************************************/

Stream::Stream()
{
	latency = 0;
	mNormalLatency = 0;

    mSpeedOctave = 0;
    mSpeedStep = 0;
    mSpeedBend = 0;
    mTimeStretch = 0;
    mSpeed = 1.0;

    mPitchOctave = 0;
    mPitchStep = 0;
    mPitchBend = 0;
	mPitch = 1.0;

    mResampler = NULL;
	mAudioBuffer = NULL;
	mAudioBufferFrames = 0;
	mAudioPtr = NULL;
	mSmoother = new Smoother();

    mCorrection = 0;
}

Stream::~Stream()
{
	delete mSmoother;
	delete mResampler;
}

int Stream::getLatency()
{
	return latency;
}

int Stream::getNormalLatency()
{
	return mNormalLatency;
}

/**
 * This is only intended to be called by Track during initialization to convey
 * what the AudioInterface things the native latency will be.  It must
 * not be changed once we start running.
 * !! UI needs to prevent this from happening while we're running.
 */
void Stream::setLatency(int i)
{
	mNormalLatency = i;
	adjustSpeedLatency();
	
}

void Stream::setCorrection(int c)
{
    mCorrection = c;
}

int Stream::getCorrection()
{
    return mCorrection;
}

/**
 * Called when a loop resets itself.  This may turn off some 
 * track state too.
 * 
 * Returning to Reset cancels Reverse, though you can arm it again
 * EDP says nothing about half speed, assume yes.
 * Do the other loops get to remember their reset/speed status?
 */
PUBLIC void Stream::reset()
{
    // this gets mSpeedOctave, mSpeedStep, mSpeedBend, mTimeStretch, and mSpeed
    initSpeed();

    // same for pitch
    initPitch();

	setReverse(false);

	if (mResampler != NULL) {
		mResampler->reset();
		mResampler->setSpeed(1.0);
	}
}

//
// Speed
//

PUBLIC float Stream::getSpeed()
{
    return mSpeed;
}

/**
 * Called during reset to initialize rate state.
 * This also initializes time stretch.
 */
PUBLIC void Stream::initSpeed()
{
    mSpeed = 1.0;
    mSpeedOctave = 0;
    mSpeedStep = 0;
    mSpeedBend = 0;
    mTimeStretch = 0;
    adjustSpeedLatency();
}

/**
 * Recalculate the stream rate from the three components
 * and adjust latency.
 */
PRIVATE void Stream::recalculateSpeed()
{
    mSpeed = Resampler::getSpeed(mSpeedOctave, mSpeedStep, mSpeedBend, 
                                 mTimeStretch);

    adjustSpeedLatency();
}

PUBLIC int Stream::getSpeedOctave()
{
    return mSpeedOctave;
}

PUBLIC void Stream::setSpeedOctave(int degree)
{
    if (mSpeedOctave != degree) {
        mSpeedOctave = degree;
        recalculateSpeed();
    }
}

PUBLIC int Stream::getSpeedStep()
{
    return mSpeedStep;
}

PUBLIC void Stream::setSpeedStep(int degree)
{
    if (mSpeedStep != degree) {
        mSpeedStep = degree;
        recalculateSpeed();
    }
}

PUBLIC int Stream::getSpeedBend()
{
    return mSpeedBend;
}

PUBLIC void Stream::setSpeedBend(int degree)
{
    if (mSpeedBend != degree) {
        mSpeedBend = degree;
        recalculateSpeed();
    }
}

/**
 * Set all three rate components at once.  
 * Special rate setter for JumpPlayEvent.
 */
PUBLIC void Stream::setSpeed(int octave, int semitone, int bend)
{
    mSpeedOctave = octave;
    mSpeedStep = semitone;
    mSpeedBend = bend;
    recalculateSpeed();
}

PUBLIC int Stream::getTimeStretch()
{
    return mTimeStretch;
}

PUBLIC void Stream::setTimeStretch(int degree) 
{
    if (mTimeStretch != degree) {
        mTimeStretch = degree;
        recalculateSpeed();
        recalculatePitch();
    }
}

/**
 * Adjusts latency to account for a change in playback rate.
 * 
 * Subtlety: If latency isn't an even multiple of 2, round up.
 * Without this the play frame will be a little (one frame in half/full speed)
 * too far forward when the play/record layers are synchronized at the 
 * same speed resulting in an adjustment backward. Rounding up eliminates
 * the adjustment (always?) or at the least makes it an adjustment forward
 * which in theory is less likely to result in abrupt sample transitions than
 * adjusting backward.
 *
 * UPDATE: The previous comment should become irrelevant once we implement
 * "chasing" to correct record/playback frame alignment after rate shift.
 */
void Stream::adjustSpeedLatency()
{
    if (mSpeed == 1.0)
      latency = mNormalLatency;
	else {
        // round up
        latency = (int)ceil(mNormalLatency * mSpeed);
	}
}

/**
 * Helper for JumpPlayEvent to determine what latencies will eventually be.
 */
PUBLIC int Stream::getAdjustedLatency(int latency)
{
	if (mSpeed != 1.0)
	  latency = (int)ceil(latency * mSpeed);
	return latency;
}

/**
 * Helper for JumpPlayEvent to determine what latencies will eventually be.
 * This for the case where we can't update the stream latency yet,
 * but we need to know what it will be.
 */
PUBLIC int Stream::getAdjustedLatency(int octave, int semitone, int bend, 
                                      int stretch)
{
	int latency = mNormalLatency;

    float rate = Resampler::getSpeed(octave, semitone, bend, stretch);
	if (rate != 1.0)
	  latency = (int)ceil(latency * rate);

	return latency;
}

//
// Pitch
//

PUBLIC float Stream::getPitch()
{
    return mPitch;
}

/**
 * Called during reset to initialize rate state.
 */
PUBLIC void Stream::initPitch()
{
    mPitch = 1.0;
    mPitchOctave = 0;
    mPitchStep = 0;
    mPitchBend = 0;
}

/**
 * Recalculate the stream pitch from the three components.
 */
PRIVATE void Stream::recalculatePitch()
{
    // invert the stretch, when the speed gets slower the 
    // pitch gets faster
    int stretch = -mTimeStretch;
    mPitch = Resampler::getSpeed(mPitchOctave, mPitchStep, mPitchBend, stretch);
}

PUBLIC int Stream::getPitchOctave()
{
    return mPitchOctave;
}

PUBLIC void Stream::setPitchOctave(int degree)
{
    if (mPitchOctave != degree) {
        mPitchOctave = degree;
        recalculatePitch();
    }
}

PUBLIC int Stream::getPitchStep()
{
    return mPitchStep;
}

PUBLIC void Stream::setPitchStep(int degree)
{
    if (mPitchStep != degree) {
        mPitchStep = degree;
        recalculatePitch();
    }
}

PUBLIC int Stream::getPitchBend()
{
    return mPitchBend;
}

PUBLIC void Stream::setPitchBend(int degree)
{
    if (mPitchBend != degree) {
        mPitchBend = degree;
        recalculatePitch();
    }
}

/**
 * Set all three rate components at once.  
 * Special rate setter for JumpPlayEvent.
 */
PUBLIC void Stream::setPitch(int octave, int step, int bend)
{
    mPitchOctave = octave;
    mPitchStep = step;
    mPitchBend = bend;
    recalculatePitch();
}

PUBLIC void Stream::setPitchTweak(int tweak, int value)
{
}

PUBLIC int Stream::getPitchTweak(int tweak)
{
    return 0;
}

//
// Frame Position
//

PUBLIC long Stream::deltaFrames(float* start, float* end)
{
	long bytes = (long)((long)end - (long)start);
	long samples = bytes / sizeof(float);
	long frames = samples / channels;
	return frames;
}

/**
 * Kludge for Track/Script/StartCapture interaction.
 * Audio recorder can ask the track for the number of processed
 * frames *before* we assign the buffers for this interrupt.  Need
 * to make sure this returns zero.
 */
PUBLIC void Stream::initProcessedFrames()
{
	mAudioBuffer = NULL;
	mAudioPtr = NULL;
}

PUBLIC long Stream::getProcessedFrames()
{
	return deltaFrames(mAudioBuffer, mAudioPtr);
}

/**
 * Rather obscure accessor for Synchronizer to compute the
 * drift between the track sync master and its sync master.
 */
PUBLIC long Stream::getInterruptFrames()
{
	return mAudioBufferFrames;
}

PUBLIC long Stream::getRemainingFrames()
{
	return mAudioBufferFrames - deltaFrames(mAudioBuffer, mAudioPtr);
}

/**
 * Set a new target level for the stream.  The actual mLevel
 * value inherited from LayerContext will be changed gradually
 * as the interrupt buffer is processed.
 */
PUBLIC void Stream::setTargetLevel(int level)
{
	mSmoother->setTarget(level);
}

/**
 * Calculate drift away from a target frame.
 * This isn't as simple as just comparing two values since we have
 * to take into account wrapping at the loop boundary.
 *
 * This is essentially the same as what SyncTracker::calcDrift does
 * but without less confusing words.
 */
PUBLIC long Stream::calcDrift(long targetFrame, long currentFrame, 
                              long loopFrames)
{
    long drift = 0;

    if (targetFrame != currentFrame) {
		long ahead;
		long behind;

        // find the "nearest" drift
		if (currentFrame > targetFrame) {
			ahead = currentFrame - targetFrame;
			behind = (loopFrames - currentFrame) + targetFrame;
		}
		else {
			behind = targetFrame - currentFrame;
			ahead = (loopFrames - targetFrame) + currentFrame;
		}

		if (ahead <= behind)
		  drift = ahead;
		else
		  drift = -behind;
    }

	return drift;
}

/**
 * Kludge for record ending.  In 2.2 we started setting the output stream
 * speed immediately when speed was changed before recording.  This 
 * meant that we would go through the motions of a speed adjustment even
 * though there was nothing to play.  When the recording finally ended,
 * the Resampler could have had a frame of remainder from all this empty
 * playing, inserting that caused one frame of play frame advance difference
 * which caused a test file diff.
 *
 * This is technically okay, but I added this to make it look like 2.1
 * until we're ready to regen all the test files.
 */
PUBLIC void Stream::resetResampler()
{
    mResampler->reset();
}

/****************************************************************************/
/****************************************************************************
 *                                                                          *
 *   							OUTPUT STREAM                               *
 *                                                                          *
 ****************************************************************************/
/****************************************************************************/

PUBLIC OutputStream::OutputStream(InputStream* in, AudioPool* aupool)
{
	mInput = in;
    mAudioPool = aupool;
    mResampler = new Resampler(false);
	mPitchShifter = PitchPlugin::getPlugin(in->getSampleRate());
	mPlugin = NULL;
	mPan = 64;
	mMono = false;
	mLoopBuffer = NULL;
    mSpeedBuffer = NULL;
	mMaxSample = 0.0;

	mLastLayer = NULL;
	mLastFrame = 0;
	mLayerShift = false;

	// a pair of smoothers for the left/right pan levels
	mLeft = new Smoother();
	mRight = new Smoother();

    // The "tail buffer" is used to capture fade tails when jumping th
    // playback cursor around.
	mTail = new FadeTail();
	mOuterTail = new FadeTail();
	mForceFadeIn = false;

    // The "loop buffer" needs to be as large as the maximum audio buffer 
    // since we can never return more than that, but add a little extra
    // for rounding errors.
    // !! This is 4096 which multipled by the shift adjustment is 64K
    // times 2 for each track times 8 tracks.  We shouldn't need this much,
    // make Recorder call Track with smaller buffer sizes so we can consistently
    // design for 256.
	long loopBufferFrames = AUDIO_MAX_FRAMES_PER_BUFFER + 16;
	long loopBufferSamples = loopBufferFrames * AUDIO_MAX_CHANNELS;

	mLoopBuffer = new float[loopBufferSamples];

    // The "rate buffer" needs to be sas large as the loop buffer times
    // the highest speed multiplier.  +8 to guard against remainders
	long rateBufferSamples = (long)((loopBufferSamples * MAX_RATE_SHIFT) + 4);
	mSpeedBuffer = new float[rateBufferSamples];

	mCapture = false;
	mCaptureAudio = NULL;
	mCaptureTotal = 0;
	mCaptureMax = 50000;
}

PUBLIC OutputStream::~OutputStream()
{
	delete mTail;
	delete mOuterTail;
	delete mLoopBuffer;
	delete mSpeedBuffer;
	delete mPitchShifter;
    delete mPlugin;
	delete mLeft;
	delete mRight;

    // !! release mCaptureAudio to the pool
}

/**
 * If we get to do this at runtime, then we'll need to be more careful
 * about letting the existing plugin "drain" and possibly do some fades
 * between them.
 */
PUBLIC void OutputStream::setPlugin(StreamPlugin* p)
{
    delete mPlugin;
    mPlugin = p;
}

PUBLIC void OutputStream::setCapture(bool b)
{
	mCapture = b;
}

PUBLIC void OutputStream::setPitchTweak(int tweak, int value)
{
	if (mPitchShifter != NULL)
	  mPitchShifter->setTweak(tweak, value);
}

PUBLIC int OutputStream::getPitchTweak(int tweak)
{
    int value = 0;
	if (mPitchShifter != NULL)
	  value = mPitchShifter->getTweak(tweak);
    return value;
}

PUBLIC void OutputStream::setPan(int p)
{
	mPan = p;

	if (mPan == 64) {
		mLeft->setTarget(127);
		mRight->setTarget(127);
	}
	else if (mPan > 64) {
		// linear
		//mLeft->setTarget((float)(127 - mPan) / 63.f);
		mLeft->setTarget((127 - mPan) * 2);
		mRight->setTarget(127);
	}
	else if (mPan < 64) {
		// linear
		//mRight->setTarget((float)mPan / 64.0f);
		mRight->setTarget(mPan * 2);
		mLeft->setTarget(127);
	}
}

/**
 * Configure the stream for mono mode.
 * This doesn't reduce the number of channels (still always 2).
 * It sums the 2 input channels, and then does a "true" pan of the combined
 * audio within the 2 output channels.
 *
 * NOTE: Because this can affect the continiuty of the output, there may
 * be clicks of you turn mono on and off while something is playing.  It is intended
 * to be set once in the track setup and not changed.
 */
PUBLIC void OutputStream::setMono(bool b)
{
	mMono = b;
}

PUBLIC void OutputStream::clearMaxSample()
{
	mMaxSample = 0.0f;
}

PUBLIC float OutputStream::getMaxSample()
{
	return mMaxSample;
}

PUBLIC int OutputStream::getMonitorLevel()
{
	// convert to 16 bit integer
	return (int)(mMaxSample * 32767.0f);
}

PUBLIC void OutputStream::setLayerShift(bool b)
{
	mLayerShift = b;
}

PUBLIC Layer* OutputStream::getLastLayer()
{
	return mLastLayer;
}

/**
 * Special case used only when squelching a record layer that had
 * no meaningful content added.  Since we may have been preplaying
 * this layer, we have to move back to the previous play layer so 
 * that the preplay layer may be modified before we need to capture
 * a fadeTail if we go into mute.  Under some conditions (muting)
 * we have already captured a tail so ignore this if mLastLayer is
 * already NULL.
 *
 * Sigh, the playFrame argument was added to pass the current play
 * frame and compare it to mLastFrame to dedice whether or not to 
 * set the mLayerShift flag.  If a JumpPlayEvent happens to be exactly
 * on a loop boundary, we may have changed the playback position and need
 * to do a fade even though we're squelching the layer.
 *
 * Subtlety: If we just finished a Multiply exactly on the loop boundary,
 * we will have done a shift, and then immediately do another shift
 * and try to squelch the unused record layer.  In that case, we were never
 * playing the record layer and may still need a fade.  So only squelch
 * of the last layer was also the record layer we're squelcing.
 */
PUBLIC void OutputStream::squelchLastLayer(Layer* rec, Layer* play, long playFrame)
{
	if (mLastLayer != NULL && play != NULL && mLastLayer == rec) {
		mLastLayer = play;
		if (playFrame == mLastFrame)
		  mLayerShift = true;
	}
}

PUBLIC long OutputStream::getLastFrame()
{
	return mLastFrame;
}

/**
 * Adjust the last frame counter to reflect a fundamental change
 * in the layer, such as unrounded multiply/insert.
 */
PUBLIC void OutputStream::adjustLastFrame(int delta)
{
    mLastFrame += delta;
}

PUBLIC void OutputStream::setLastFrame(long frame)
{
    mLastFrame = frame;
}

/**
 * Called by Loop when it is reset to ensure that we're no longer
 * pointing to Layers that have been freed.  
 *
 * Only pay attention to this if our layer belongs to this loop.
 * The loop will be different when a pending loop is cleared 
 * for LoopCopy.
 */
PUBLIC void OutputStream::resetHistory(Loop* l)
{
	if (mLastLayer != NULL && mLastLayer->getLoop() == l) {

		// capture a fade tail if we were playing
		captureTail();
	}
}

/****************************************************************************
 *                                                                          *
 *                                    PLAY                                  *
 *                                                                          *
 ****************************************************************************/

/**
 * Initialize the stream for processing a new audio interrupt buffer.
 */
PUBLIC void OutputStream::setOutputBuffer(AudioStream* aus, float* b, long l)
{
    // the above are the original master values, these
    // AudioContext values may change as we go

    mAudioBuffer = b;
	mAudioBufferFrames = l;
	mAudioPtr = b;
	mMaxSample = 0.0f;
}

/**
 * Called by Track to add frames from a Loop to an output buffer.
 * setOutputBuffer must have been called by now.
 *
 * The blockFrames value is the number interrupt buffer frames we need to fill.
 * This number must be scaled according to playback rate when extracting
 * frames from the loop, then the result either interpolated or decimated.
 * 
 * For example in half speed, a block of 256 frames will have been decimated
 * by InputStream to 128 frames.  Here we ask for 128 frames and interpolate
 * them to 256 frames.
 */
PUBLIC void OutputStream::play(Loop* loop, long blockFrames, bool last)
{
	long remaining = getRemainingFrames();

	if (blockFrames < 0) {
		// InputStream will return -1 if it overflows and has a scaling 
		// error.  I don't think this can happen but if it does, just play
		// whatever we have left.
		if (blockFrames != -1) 
		  Trace(loop, 1, "Negative frame count in output stream %ld\n",
				blockFrames);
	}
	else if (blockFrames > remaining) {
		// might happen due to rate scaling rounding errors?
		Trace(loop, 1, "Corrected play request overflow %ld\n", 
			  blockFrames - remaining);
		blockFrames = remaining;
	}
	else if (last && blockFrames < remaining) {
		// this seems to happen a lot, figure out why
		//Trace(loop, 1, "Corrected play request underflow %ld\n", 
		//remaining - blockFrames);
		blockFrames = remaining;
	}

    if (mAudioBuffer != NULL && blockFrames > 0) {

		// add tails at the beginning of the buffer until we start playing
		// the layer, then they have to be offset
		mTail->initRecordOffset();

		// use the latest rate
		mResampler->setSpeed(mSpeed);

        // Play into the intermediate mLoopBuffer.  If we have to 
        // resample, then we'll also use mSpeedBuffer temporarily.
        float* loopBuffer = mLoopBuffer;
		long playFrames = blockFrames;

        // First transfer any resampling remainder from the last time.
        long lastRemainder = mResampler->addRemainder(loopBuffer, playFrames);
        if (lastRemainder > 0) {
            playFrames -= lastRemainder;
            loopBuffer += (lastRemainder * channels);
        }

        // if we're 

		// If we're rate adjusting, play into the rate buffer and resample
		// back to the loop buffer, otherwise play directly into loop buffer.
		float* playBuffer = loopBuffer;
        if (mSpeed != 1.0)
		  playBuffer = mSpeedBuffer;

		// note: now that we handle output leveling in the Stream, the
		// level inherited from LayerContext should always stay 1.0, 
		// though currently Segment will override it temporarly.
		// Just to be safe make sure it starts at 1.0 before every traversal.
		setLevel(1.0);

		// If we're changing pitch, capture an outside fade tail.  Plugin edge
		// fades are complicated see the notes for details.
		mForceFadeIn = false;
		if (mPitchShifter != NULL) {
			float lastRatio = mPitchShifter->getPitchRatio();
			if (lastRatio != mPitch) {
				if (lastRatio == 1.0) {
					// beginning a shift
					captureOutsideFadeTail();
				}
				else if (mPitch == 1.0) {
					// ending a shift
					capturePitchShutdownFadeTail();
					// set this to force a layer fade in in the play() callback below
					mForceFadeIn = true;
				}
				else {
					// shift changing
					capturePitchShutdownFadeTail();
				}
			}
			mPitchShifter->setPitch(mPitch, mPitchStep);
		}

		// If we're rate adjusting, there is the possibility of an underflow
		// (not getting enough frames from the loop) due to floating point
		// rounding errors.  It is very rare, but will happen if you
		// wait long enough.  So we have to loop until we've filled the
		// interrupt block.
		// UPDATE: Not sure this can happen with the new transposition
		// algorithm, but be safe.

		long remaining = playFrames;
		int iterations = 0;

		while (remaining > 0 && iterations < 4) {

			if (iterations > 0) {
				int x = 0;
			}

			// If we're rate adjusting, scale the number of frames requested
			// from Loop
			long scaledFrames = mResampler->scaleOutputFrames(remaining);

			// In rare cases we can begin to slowly go out of sync
			// at some rates, probably due to floating point rouding error.
			// 3 seems to be the average constant rate, one extra for lookahead
			// on each side, and one for periodic drift corrected quickly.
			// Do not do this check if we're in a mode where the play/record
			// frames are allowed to diverge, this includes the latency 
			// period during a rate change.  Had to introduce a dependency
			// on InputStream to detect this, if the stream rates are not the
			// same then assume we're in the latency adjustment period.

			int insertions = 0;
			int ignores = 0;
            // fucking VisualStudio won't expand the Stream subclass
            // so get the speed out here so we can see it
            float speed = mSpeed;
			if (mSpeed != 1.0f && mSpeed == mInput->getSpeed() &&
                loop->isAdvancingNormally() &&
                // this means there is something to play, we're not still recording
                loop->isPlaying()) {
				
				// sigh, need the input latency for this calc so we had
				// to introduce a dependency on InputStream, could have it
				// cached here?
                int inputLatency = mInput->latency;
				long expected = loop->addFrames(loop->getFrame(), 
												inputLatency + latency);
				long actual = loop->addFrames(loop->getPlayFrame(),	
											  scaledFrames);

                // positive if we're ahead
                long drift = calcDrift(expected, actual, loop->getFrames());

                if (drift > 2) {
                    // play frame is rushing, read one less and
                    // duplicate the last one
                    Trace(loop, 2, "Corrected rushing play cursor: expected %ld actual %ld drift %ld\n",
                          expected, actual, (long)drift);
                    insertions = 1;
                }
                else if (drift < -2) {
                    // play frame is lagging, read one extra and ignore it
                    Trace(loop, 2, "Corrected lagging play cursor: expected %ld actual %ld drift %ld\n",
                          expected, actual, (long)drift);
                    ignores = 1;
                }
			}

			// can't have both a non-zero ignores and insertions
			long adjustedFrames = scaledFrames + ignores - insertions;

			// reinit AudioContext values and let the Loop go through its
			// logic, it will callback to play(Layer) below, sometimes 
			// more than once, note that "buffer" advances after this.
			memset(playBuffer, 0, (adjustedFrames * channels) * sizeof(float));
			buffer = playBuffer;
			frames = adjustedFrames;
			loop->play();

			// Next merge inner fade tail
			mTail->play(playBuffer, adjustedFrames);

			// apply pitch shift
			if (mPitchShifter != NULL && mPitch != 1.0)
			  mPitchShifter->process(playBuffer, adjustedFrames);

			// apply other plugins
			// this is just a stub for later, need to generalize this since
			// other plugins may have the same latency issues as the pitch shifter
			if (mPlugin != NULL)
			  mPlugin->process(playBuffer, adjustedFrames);

			// merge outside tail
			mOuterTail->play(playBuffer, adjustedFrames);

			// now apply rate adjustments, note that the remainder from
			// the previous resampler call is not included 
			if (mSpeed == 1.0)
			  remaining = 0;
			else {
				// If we have an ignore count, transpose with the non
				// adjusted frame count which will ignore the extra one
				// we played.  If we have an insert count, duplicate the
				// last frame before transposing.

				if (insertions > 0) {
					float* last = &playBuffer[(adjustedFrames - 1) * channels];
					float* insert = &playBuffer[adjustedFrames * channels];
					for (int i = 0 ; i < insertions ; i++) {
						for (int j = 0 ; j < channels ; j++)
						  insert[j] = last[j];
						insert += channels;
					}
				}

				// we played into the rate buffer, transpose to the loop buffer
				long actual = mResampler->resample(playBuffer, scaledFrames,
												   loopBuffer, remaining);

				// in rare conditions we may not have actually filled
				// the buffer due to floating point round off, so loop again
				remaining -= actual;
				if (remaining > 0) {
					long samples = actual * channels;
					playBuffer += samples;
					loopBuffer += samples;
				}
			}
			iterations++;
		}

		if (remaining > 0) {
			Trace(loop, 1, "Unable to fill interrupt block after %ld iterations!\n",
				  (long)iterations);
		}
		else if (iterations > 1) {
			// should only be level 2, but I want to see it for awhile
			Trace(loop, 1, "Speed scale underflow, required %ld iterations\n",
				  (long)iterations);
		}

        // temporary debug, remove eventually
		//if (mCapture)
		//capture(mLoopBuffer, blockFrames);
		
		// Apply panning and output level and copy to interrupt buffer
		adjustLevel(blockFrames);

		// sanity check
		if (getRemainingFrames() < 0)
		  Trace(loop, 1, "Output stream buffer overflow!\n");
    }
}

/**
 * Capture an "outside" fade tail.
 * This is captured from the last layer like a normal fade tail, the difference
 * is that it is merged into the interrupt block without being passed
 * through the pitch shifter.  The shifter causes a latency delay, this tail
 * needs to be merged immediately.
 */
PRIVATE void OutputStream::captureOutsideFadeTail()
{
	captureTail(mOuterTail, 1.0);
}

/**
 * Capture an "outside" fade tail by draining the pitch shifter.
 * This is used when the shifter is being deactivated or the pitch ratio is changed.
 * Usually there will be enough buffered in the shifter we can just drain what we need.
 * Occasionally it may be necessary to feed it additional frames until we have enough.
 *
 * NOTE: Trying an experimental fade technique that is a lot easier than capturing
 * a true forward fade tail and feeding it into the plugin.
 */
PRIVATE void OutputStream::capturePitchShutdownFadeTail()
{
	// first capture a fade tail from the current location
	// this may or may not be necessary, but if there isn't enough in the
	// plugin buffers we need something to feed
	captureTail(mOuterTail, 1.0);

	// now ask the shifter for its fade tail
	// If it needs more samples it can get them from the tail object, but
	// it will always write a new tail.  Don't really like overloading the FadeTail
    // as an input/output parameter but it's a convenient place to pass this
	// without having more temporary buffers
	mPitchShifter->captureFadeTail(mOuterTail);
}

/**
 * Copy the result of a Loop play into the interrupt buffer applying
 * output level adjustment and panning.
 *
 * Multiplied the logic to reduce the number of multiplies.
 * May not save much but it just feels better.
 */
PRIVATE void OutputStream::adjustLevel(long frames)
{
	long samples = frames * channels;
	float outLevel = mSmoother->getValue();
	float* src = mLoopBuffer;

	bool noSmoothing = 
		!mSmoother->isActive() && !mLeft->isActive() && !mRight->isActive();

	if (mMono) {
		// Special mono mode, pan operates as a "true" pan positioning a portion
		// of each input channel into each output channel.  Usually in this mode
		// only one input channel will have non-zero content, but if they do, sum them.
		// Start with the complex case, simplify later if necessary.

		float leftLevel, rightLevel;
		float leftMod, rightMod;

		for (int i = 0 ; i < samples ; i += 2) {

			leftLevel = mLeft->getValue();
			rightLevel = mRight->getValue();

			if (leftLevel == 1.0 && rightLevel == 1.0) {
				// dead center
				leftMod = 0.5f;
				rightMod = 0.5f;
				// could advance either way, but better not be both!
				mLeft->advance();
				mRight->advance();
			}
			else {
				// we're panning in one direction
				// must always complete a seep on one side before beginning the next
				bool left;

				// redundant logic, but important to see

				if (leftLevel < 1.0 && mLeft->getTarget() == 1.0) {
					// panned right, but crossed back over to the left
					// advance left level only
					left = true;
				}
				else if (rightLevel < 1.0 && mRight->getTarget() == 1.0) {
					// panned left, but crossed back over to the right
					// advance right level only
					left = false;
				}
				else if (leftLevel < 1.0) {
					// panning right
					left = true;
				}
				else if (rightLevel < 1.0) {
					// panning left
					left = false;
				}

				if (left) {
					leftMod = leftLevel * 0.5f;
					rightMod = 1.0f - leftMod;
					mLeft->advance();
				}
				else {
					rightMod = rightLevel * 0.5f;
					leftMod = 1.0f - rightMod;
					mRight->advance();
				}
			}

			// sum the inputs
			float sample = *src++;
			sample += *src++;

			// adjust for output level
			sample *= mSmoother->getValue();
			
			// pan
			float psample = sample * leftMod;
			checkMax(psample);
			*mAudioPtr++ += psample;

			psample = sample * rightMod;
			checkMax(psample);
			*mAudioPtr++ += psample;

			mSmoother->advance();
		}
	}
	else if (mPan == 64 && outLevel == 1.0 && noSmoothing) {
		// the usual case
		for (int i = 0 ; i < samples ; i++) {
			float sample = *src++;
			checkMax(sample);
			*mAudioPtr++ += sample;
		}
	}
	else {
		float leftMod = mLeft->getValue();
		float rightMod = mRight->getValue();

		// !! channel issues: pan only makes sense with two channels, 
		// if we have more than two which samples are L and R?

		if (noSmoothing) {
			// can reduce to one multiply per sample
			leftMod *= outLevel;
			rightMod *= outLevel;
			for (int i = 0 ; i < samples ; i += 2) {
				float sample = *src++ * leftMod;
				checkMax(sample);
				*mAudioPtr++ += sample;
				sample = *src++ * rightMod;
				checkMax(sample);
				*mAudioPtr++ += sample;
			}
		}
		else {
			// need a pair of multiplies per sample
			for (int i = 0 ; i < samples ; i += 2) {
				leftMod = mLeft->getValue();
				rightMod = mRight->getValue();
				outLevel = mSmoother->getValue();
				float sample = *src++ * (leftMod * outLevel);
				checkMax(sample);
				*mAudioPtr++ += sample;
				sample = *src++ * (rightMod * outLevel);
				checkMax(sample);
				*mAudioPtr++ += sample;
				mSmoother->advance();
				mLeft->advance();
				mRight->advance();
			}
		}
	}
}

PRIVATE void OutputStream::capture(float* buffer, long frames)
{
	if (mCaptureTotal < mCaptureMax) {
		if (mCaptureAudio == NULL)
          mCaptureAudio = mAudioPool->newAudio();
		mCaptureAudio->append(buffer, frames);
		mCaptureTotal += frames;
		if (mCaptureTotal >= mCaptureMax)
		  mCaptureAudio->write("capture.wav");
	}
}

PRIVATE void OutputStream::checkMax(float sample)
{
	if (sample < 0)
	  sample = -sample;

	if (sample > mMaxSample)
	  mMaxSample = sample;
}

/**
 * Transfer frames from a layer into the loop buffer.
 * Keep track of where we left off and automatically add fades.
 * 
 * Don't go through the logic unless we have a postive frame count.
 * Can get here with zero frames if we've got several events
 * stacked up on the same frame, only adjust mLastPlayLayer etc. if
 * we actually play something.
 *
 * Normally we fade if either the layer or play frame differ, but not if
 * the special "shift" flag is on which is set when shifting between layers
 * with the same content.   But shift is meaningful only if the last layer
 * is non-null, it can be set when we come out of mute quantized on a 
 * loop boundary, so we still need  to fade.
 *
 * If we are beginning the pre-play of the record layer we have to be
 * careful with a deferred fade from the end of the play layer to the
 * beginning of the record layer.  We cannot do a fade in because
 * it would cause a content break at the loop point.  But there may also
 * be content from older layers that also had a deferred fade.  If feedback
 * is reduced significantly at the start of the record layer, this
 * can cause a break in the old content.  So we have to take a hybrid
 * approach of capturing a fade tail so the old content won't have a break,
 * but we do NOT start a fade into the record layer so the deferred
 * fade from the previous layer can be preserved.
 * 
 * Pitch Shift Subtlety
 *
 * If we're entering a pitch shift, there will be a break due to plugin
 * latency that we're not yet compensating for.  Failing compensation, 
 * we at least need to capture a fade tail and fade in even if it looks
 * like we're playing seamlessly.
 *
 * When we leave pitch shift, there won't be a gap, but we're going to 
 * suddeny stop using the shifter and pick up playing from where we
 * think we are (audibly).  This will result in a jump forward from
 * what is actually being sent to the sound card since there are still
 * frames buffered in the plugin.  Here we need to capture a tail
 * to feed to the plugin so the buffered frames fade nicely.  Then we
 * need to force a fadeIn because of the jump.
 *
 * Later, the higher-level play method needs to know that just
 * because we've stopped using the plugin there may still be stuff
 * in it that needs to be drained.
 *
 * If we're changing pitch shift, but not going to or from 1.0, then
 * just play normally.
 */
PUBLIC void OutputStream::play(Layer* layer, long playFrame, long playFrames,
                               bool mute)
{
	if (playFrames > 0) {

		if (mute) {
            // an indication that we're in mute
			captureTail();
		}
		else {
			bool fadeIn = false;
			bool fadeTail = false;

			// This can be set by the outer play() method when the pitch
			// plugin is disabled and we need to force a fade in.  This
			// is a one shot deal, it doesn't carry over to the next callback.
			if (mForceFadeIn) {
				fadeIn = true;
				mForceFadeIn = false;
			}

			if (mLastLayer == NULL ||
				(!mLayerShift &&
				 (mLastLayer != layer || mLastFrame != playFrame))) {

				if (ScriptBreak) {
					int x = 0;
				}

                if (mLastLayer != NULL && 
					layer->getPrev() == mLastLayer &&
                    playFrame == 0 &&
                    mLastFrame == mLastLayer->getFrames()) {
                    // we're jumping from the play layer to the record layer
                    // !! is this enough, what about redo?

					// since we're about to begin preplay, lock the starting
					// feedback level for segments when we're not flattening
					int feedback = layer->lockStartingFeedback();

                    if (mLastLayer->isContainsDeferredFadeOut()) {

						if (feedback < 127) {
							
							// have to capture an adjusted fade tail
							float* ramp = AudioFade::getRamp128();
							// this is what we already have
							float feedbackFactor = ramp[feedback];
							// this is what we need to combine the tail
							float adjust = 1.0f - feedbackFactor;

							Trace(layer, 2, "Capturing fade tail for feedback leveling on preplay\n");
							captureTail(adjust);

							// since we did an early capture, don't do it
							// again if we're also pitch shifting
							fadeTail = false;

							// alternate method, a full cross fade
							//captureTail();
							//fadeIn = true;
                        }
                    }
                    else if (layer->isContainsDeferredFadeIn()) {
                        // consistency check, shouldn't happen?
                        // if we contain a deferred fade in,
                        // then the previous layer should have contained
                        // a deferred fade out caught above
                        Trace(layer, 1, "Inconsistent deferred fades detected in output stream\n");
                        // only do this if we do not have a deferred
                        // from the previous layer into this one
                        if (!layer->isDeferredFadeIn())
                          fadeIn = true;
                    }
                    else {
                        // we don't need a tail or a fade in,  there
                        // may or may not be a deferred fade from the
                        // last layer to the new one, leave it alone
						// NOTE: If we're about to do an Insert at the front
						// of the next layer we will preplay something,
						// then what we were playing will be shifted right
						// one cycle.  If mLastFrame is not adjusted when
						// we capture the fade tail, the original content
						// won't be there.  This is hard to do out here
						// since the Insert event could still be canceled.
						// Have to let Loop do it.
                    }
                }
                else {
                    // a random jump in the middle
                    fadeTail = true;
                    if (playFrame > 0 || layer->hasDeferredFadeIn(this))
                      fadeIn = true;
                }
            }

			if (fadeTail)
			  captureTail();

			frames = playFrames;
			layer->play(this, playFrame, fadeIn);

			// Any tail we may need to capture now has to be offset after
			// what we just played
			mTail->incRecordOffset(playFrames);

			mLastLayer = layer;
			mLastFrame = playFrame + playFrames;
			mLayerShift = false;
		}

		// advance the mLoopBuffer pointer
		buffer += (playFrames * channels);
	}
}

/****************************************************************************
 *                                                                          *
 *   								TAILS                                   *
 *                                                                          *
 ****************************************************************************/

/**
 * Capture the tail from the current layer position.    
 *
 * It is easy to have logic errors elsewhere that result in redundant
 * tail captures.  To prevent this I think it's safe to assume
 * that once the tail is captured, we can no longer be playing
 * from the previous layer so it is ok to clear the last play layer
 * state.  
 *
 * A tail can be captured both during playing when we notice
 * a layer transition, and during Loop event processing when
 * we enter a mode like Mute or when we're doing undo processing
 * and the last layer may be deallocated before the stream
 * has a chance to detect the layer change.
 *
 * The adjust is normally 1.0, but may be less if we're capturing a tail
 * for the purpose of merging it with the same content copied at a lower
 * feedback level.  This will result in a partial fade, necessary to 
 * avoid a break when moving from the play layer to the record layer and
 * the record layer has reduced feedback.
 */
PUBLIC void OutputStream::captureTail(FadeTail* tail, float adjust)
{
	if (mLastLayer != NULL) {

		captureTail(tail, mLastLayer, mLastFrame, adjust);

		// since we're leaving this layer, don't leave pending fade state
		mLastLayer->cancelPlayFade();

		// clear history so we force a fade in on the next play
		mLastLayer = NULL;
        mLastFrame = 0;
        mLayerShift = false;
	}
}

/**
 * Capture a normal fade tail.  This is public so it can be called by Loop
 * and Function when it needs to make a change to the layer structure so we
 * can't defer capturing the fade until the next interrupt.
 */
PUBLIC void OutputStream::captureTail()
{
    captureTail(mTail, 1.0f);
}

/**
 * Capture a normal fade tail with a feedback adjust. 
 * Only used internally when we cross an edge during preplay.
 */
PUBLIC void OutputStream::captureTail(float adjust)
{
    captureTail(mTail, adjust);
}

/**
 * Capture the tail of a layer.
 * 
 * If we're near the end and there is no deferred fade, the tail only
 * has to go to the end not the full extent of mFadeFrames, since the
 * layer will have already have a fade at the end.
 *
 * If there is a deferred fade it's more complicated.  We have to play
 * the remainder of this layer AND a section of the beginning of the
 * layer to create a gradual fade of sufficient length.  This is now
 * handled by Layer.
 */
PRIVATE void OutputStream::captureTail(FadeTail* tail, Layer* src, long playFrame,
                                       float adjust)
{
    long remainder = src->getFrames() - playFrame;

    if (remainder < 0) {
        // something isn't right
        Trace(src, 1, "captureTail: negative remainder\n");
    }
    else {
		// capture the non-speed adjusted tail, keep one of these allocated?
		// buffer must be multiplied for expansion
		// !! keep this allocated
		float tailBuffer[(AUDIO_MAX_FADE_FRAMES * 2) * AUDIO_MAX_CHANNELS];

		memset(tailBuffer, 0, sizeof(tailBuffer));

		// carefully replace the buffer, do not disturb other play state!
		float* saveBuffer = buffer;
		long saveFrames = frames;
		buffer = tailBuffer;
		frames = AudioFade::getRange();

		long tailFrames = src->captureTail(this, playFrame, adjust);

		buffer = saveBuffer;
		frames = saveFrames;

		if (false) {
			Audio* trace = new Audio();
			trace->append(tailBuffer, tailFrames);
			trace->write("tail.wav");
			delete trace;
		}

		tail->add(tailBuffer, tailFrames);
    }
}

/****************************************************************************/
/****************************************************************************
 *                                                                          *
 *   							 INPUT STREAM                               *
 *                                                                          *
 ****************************************************************************/
/****************************************************************************/
/*
 * Speed Change
 *
 * Speed shift when there are no scheduled events in range is easy, 
 * just take all of the available frames, pass them through the 
 * Resampler, and record however many you get back.
 *
 * Speed changes in the input stream are much more complicated than in 
 * the output stream because the input stream is where we process
 * scheduled events which can change the rate.
 * 
 * When there is an event in range it will "break up" the block of interrupt
 * frames.  We must determine exactly how many input frames we will
 * consume to "reach" the event, Resample that many frames, record the result,
 * then switch over to OutputStream to play a corresponding number of
 * frames, then when both streams are back in sync, process the event.
 *
 * The hard part is determining the minimum number of frames frames we need
 * to consume to reach the event.  If we overshoot, and the event
 * changes the rate, we're technically out of alignment with
 * the output stream by a few frames.  This may not be that bad if we
 * can do a gradual adjustment later?
 *
 * DECIMATION
 *
 * When we are playing at reduced speed, we will drop input frames
 * to match the rate that we're playing.  In half speed we will
 * advance the play frame only 128 in order to produce 256 interpolated
 * frames, so we must decimate 256 input frames down to 128 in order
 * to stay in sync.
 *
 * The decimation algorithm will result in keeping one frame then
 * dropping one or more following frames.  This set of one kept
 * frame and n dropped frames is called the "decimation range".  
 * The entire range logically represents one frame after decimation or
 * one "decimated frame".  The decimation range does not need to be 
 * constant.  In half speed the range will always be 2 frames, in 
 * quarter speed 4 frames, etc.  But rates that don't divide evenly
 * may result in range patterns like 2,3,2,3 or 4,5,5,4,5,5 etc.
 *
 * This is represented graphically as:
 *
 *   x-x-x-x-
 *
 * where 'x' represents a sample from the input stream that we will
 * keep and '-' is a sample we will skip.  Other examples:
 *
 * x-x--x-x--x-x--
 * x--x---x--x---
 * 
 * There are two ways to think about "reaching" a decimated frame,
 * we can reach it by having enough source frames such that the first
 * frame in the decimation range will be reached or that the
 * last frame in the decimation range is reached.  It seems most intuitive
 * to me to have it be reaching the first frame.  This will result
 * in a "remainder" that must be carried over into calculations in the
 * next stream block.
 *
 * To find the number of decimated frames in a stream block first
 * take the number of frames in the block times the sample rate.
 * For a rate of .5 and a block of 3 we get 1.5, which can be thought
 * of as one full decimation range then half of another.
 *
 *     x-x|-x-|x-x|-x-
 *
 * Since we consider reaching the first frame in a decimation range 
 * the same as reaching the decimated frame, we can round up and consder
 * there to be 2 decimated frames in the first block.  But note that the
 * rest of the decimation range "spills over" to the next block.  The first
 * frame of the next block must be considered part of the decimation range
 * of the last frame in the first block and ignored.
 *
 * The number of frames to ignore in the next block is determined
 * by computing a "remainder".  The remainder is 1.0 minus the fractional
 * part of the decimated frame calculation.  In this example 1.0 - .5 = .5.
 * In the next block the remainder is factored in as:
 *
 *   Decimated Frames = ceil((Block Frames * Speed) - Remainder)
 *
 * or:
 *
 *   3 * .5 = 1.5 - .5 = 1 ceil = 1, remainder 0
 * 
 * Consider a rate of .25 and a stream block of 5 frames.
 * 
 *     x---x|---x-|--x--|-x---|x---x
 * 
 * The number of decimated frames in the first block is 5 * .25 = 1.25 rounded
 * up to 2.  The remainder is 1.0 - .25 = .75, meaning that there is .75 
 * of the decimation range that spills over into the next block.  
 *
 * If the block sizes are irregular due to event processing, we can encounter
 * a situation like this:
 * 
 *     x---x|---|x-|--x--|-x---|x---x
 * 
 * There is a remainder of .75 going into the second block, the number of
 * decimated frames in the second block is calculated as:
 *
 *    3 * .25 = .75 - .75 = 0
 *
 * All of the frames in this block are in the decimation range of the
 * last frame in the previous block and must be ignored.  So the entire
 * block is ignored.
 *
 * For event processing, we must determine the minimum number of block
 * frames that must be consumed to reach the event frame.  At rate .25:
 *
 *             e
 *   |x--|-x---x--|-x---x---x---
 *    0    1   2
 *       
 * The first block has 3 frames, the second has 8, there is an event on
 * decimated frame 2.  In the first block we had:
 *
 *   3 * .25 = .75 ceil = 1, remainder .25
 * 
 * In the second block we have:
 *
 *   8 * .25 = 2 - .25 = 1.75 ceil = 2, remainder .25
 * 
 * We know there will be 2 decimated frames in this block so the event
 * on frame 2 is within range.  To determine the minimum number of
 * frames to consume, return to the first 1.75 value which says there
 * is .75 of the a decimation range at the end of the block.  To determine
 * how many frames are in that range divide this by the rate.
 *
 *   .75 / .25 = 3
 *
 * Then subtract one since the first frame in the range is the one we have
 * to reach.  So with 8 frames in the block, and 3 - 1 extras we can
 * consume 6 frames to reach the beginning of the decimated frame 2.
 *
 * Hmm, no.  It isn't that easy.  Consider a rate of .75, where 20
 * frames become 15.  This can be decimated in several ways:
 *
 *   xxx-xxx-xxx-xxx-xxx-
 *   x-xxx-xxx-xxx-xxx-xx
 *   xx-xxx-xxx-xxx-xxx-x
 * 
 * We need to know exactly how the algorithm will be choosing frames to skip
 * to determine if an event frame will be in range.  In a rate of .33, 
 * 20 frames becomes 6.66
 *
 *   x--x---x--x---x--x--
 *   x---x--x---x--x---x-
 *
 * If the skip ranges are of different size, we have to know how the
 * algorithm will select the size of the range.
 *
 * Must this be pushed into the Resampler?
 * 
 * INTERPOLATION
 *
 * If the rate is 2.0 we're in double speed.  To fill a 256 frame block
 * the output stream will have to consume 512 frames and decimate them
 * to 256.  The input stream will have to make a corresponding interpolation
 * of 256 frames into 512 to match.  We'll represent that graphically as:
 *
 * x+x+x+x+
 *
 * Where 'x' is a sample from the source block and '+' is an interpolation.
 * In the simplest form of interpolation, it would be a duplication
 * of the previous frame.   Quad speed would be:
 *
 * x+++x+++x+++x+++
 *
 * And some rates may not result in evenly spaced interpolations:
 *
 * x++x+++x++x+++x++
 *
 * Again we have to be able to detect events within range of the stream block.
 * Consider a block size of 4 with an event scheduled on frame 6:
 * 
 *        e
 * xxxx|xxxx|xxxx|xxxx
 *
 * The input samples will be interpolated into this for recording:
 *
 *       e     
 * x+x++x+x++|x+x++x+x++|x+x++x+x++|x+x++x+x++
 *
 * Start by multiplying the block size by the rate to get the effective number
 * of frames in the block.  4 * 2.5 = 10 so we know the event on frame 6 is
 * in range.  So the question again is, what is the minimum number of
 * frames to consume from the stream block to reach the expanded frame 6.
 * Take the number of frames we're trying to reach divided by the rate:
 *
 *    6 / 2.5 = 2.4 ceil = 3
 *
 * Since we have to perform this computation anyway, we don't need to 
 * make the (block size * rate) calculation to determine if the event
 * is within the block.  If this is true:
 *
 *     ceil(((event frame - record frame) / rate)) < block size
 *
 * Then the event is in range.
 *
 * ALTERNATE APPROACH
 *
 * Upon receiving the input buffer, immediately perform a rate transposition
 * of the entire buffer.  Then knowing the full size of the adjusted buffer
 * look for events as usual.  If there are events in range, break up the
 * block as usual.
 *
 * Only if an event changes the rate do we have more work to do.  Stream
 * keeps the rate at which the last block was transposed.  When it changes
 * we correlate the frame where the rate change happened to the original
 * stream block, then do a new rate transposition from there and continue.
 * This can also result in rounding errors but may be easier overall to code?
 * The tradeoff is that there is the potential to do too much work if we
 * have several rate changes in the buffer.  But this would be very unusual and
 * we probably should be collapsing them if they occur?
 *
 * Interpolation, rate 2.5
 *
 *   xxxxxxxx
 *   x+x++x+x++x+x++x+x++
 *            e
 *
 * Where 'e' is a rate change on frame 9.  To correlate to the corresponding
 * source frame divide by the rate, 9 / 2.5 = 3.6 ceil = 4.  
 *
 * Interpolation, rate 2.0
 *
 *   xxxxxxxx
 *   x+x+x+x+x+x+x+x+
 *            e
 * 
 * 9 / 2 = 4.5 ceil 5
 * 8 / 4 = 4 ceil 4
 * 
 * When an event is on frame 9, there is some ambiguity over where the
 * new rate should be applied.  Is the "full interpolation range" of source
 * frame 4 to be kept, or can we truncate the interpolation range and start
 * the new rate.  The later is easier and seems fine.
 *
 * It sure seems like we're going to have rates that produce rouding
 * errors such that when we're done splitting the input buffer we will
 * either not have filled enough of the output buffer, or have gone too far.
 * In the case where we haven't filled enough, we'll need to go back and
 * play at least one more frame.  If we've gone too far, we can either backup
 * the play frame and pretend it didn't happen or keep the remainder for
 * the next interrupt?
 * 
 * Decimation, rate 0.5, event on 2
 *
 *   xxxxxxxx
 *   x-x-x-x-
 *       e
 *
 *   2 / .5 = 4
 *   3 / .5 = 6
 *
 * This looks right.
 *
 * 
 * Decimation, rate 0.33
 *
 *   xxxxxxxx
 *   x--x---x--x---x--x---x--x---
 *             e
 *
 * Event on 3: 3 / .33 = 9.09 ceil = 10
 * Event on 2: 2 / .33 = 6.06 ceil = 7
 * 
 * This just feels easier and less error prone to me.
 */

InputStream::InputStream(Synchronizer* sync, int sampleRate)
{
    mResampler = new Resampler(true);
	mSynchronizer = sync;
    mSampleRate= sampleRate;
    mPlugin = NULL;
    mMonitorLevel = 0;
	mLastLayer = NULL;
	mLevelBuffer = NULL;
    mSpeedBuffer = NULL;
    mLastSpeed = 1.0f;
	mLastThreshold = 1.0f;
    mOriginalFramesConsumed = 0;
	mRemainingFrames = 0;

	// Temporary buffer used for level adjusted frames.
	long levelBufferSamples = 	
		((AUDIO_MAX_FRAMES_PER_BUFFER + 16) * AUDIO_MAX_CHANNELS);
	mLevelBuffer = new float[levelBufferSamples];

    // Temporary buffer used for rate transposition and level adjustment
    // for level adjustment needs to be as long as the interrupt buffer
    // for rate shift needs to be as long as the interrupt buffer times
    // the maximum rate multiplier.
    // I'm paranoid about boundary errors on rate transposition so
    // add a few safety frames (16) to the calculations
    
	long rateBufferSamples =
        ((AUDIO_MAX_FRAMES_PER_BUFFER + 16) * AUDIO_MAX_CHANNELS) * 
        MAX_RATE_SHIFT;

	mSpeedBuffer = new float[rateBufferSamples];
}

InputStream::~InputStream()
{
	delete mLevelBuffer;
    delete mSpeedBuffer;
    delete mPlugin;
}

/**
 * Kludge for Track/Script/StartCapture interaction.
 * Audio recorder can ask the track for the number of processed
 * frames *before* we assign the buffers for this interrupt.  Need
 * to make sure this returns zero.
 */
PUBLIC void InputStream::initProcessedFrames()
{
	mOriginalFramesConsumed = 0;
}

/**
 * Stream overload, since we maintain processed frames in a more
 * complicated way due to rate scaling and do not maintain mAudioPtr
 * in the same way.
 * 
 * !! getProcessedFrames is used by Track::checkSyncEvent, it probably
 * does need to be scaled back to "interrupt" time?  Actually no, 
 * it looks like there are assumptions handling the sync event "offset"
 * that the buffer rate will remain constant.  This may be a larger problem.
 */
PUBLIC long InputStream::getProcessedFrames()
{
	return mOriginalFramesConsumed;
}

/**
 * Retrun the number of remaining original input frames to be processed.
 * It turns out we don't actually need to overload this since only
 * Track calls it on OutputStream as a sanity check.
 */
PUBLIC long InputStream::getRemainingFrames()
{
	return mAudioBufferFrames - mOriginalFramesConsumed;
}

/**
 * Get the scaled remaining frames.
 */
PUBLIC long InputStream::getScaledRemainingFrames()
{
    return mRemainingFrames;
}

/**
 * For InputStream this is the same as getProcessedFrames(), but
 * this is needed by EventManager and I want to make sure we're always
 * returning the right thing.
 */
PUBLIC long InputStream::getOriginalFramesConsumed()
{
    return mOriginalFramesConsumed;
}

/**
 * If we get to do this at runtime, then we'll need to be more careful
 * about letting the existing plugin "drain" and possibly do some fades
 * between them.
 */
PUBLIC void InputStream::setPlugin(StreamPlugin* p)
{
    delete mPlugin;
    mPlugin = p;
}

PUBLIC Synchronizer* InputStream::getSynchronizer()
{
	return mSynchronizer;
}

PUBLIC int InputStream::getMonitorLevel()
{
    return mMonitorLevel;
}

PUBLIC int InputStream::getSampleRate()
{
	return mSampleRate;
}

/**
 * Called by Loop when it is reset to ensure that we're no longer
 * pointing to Layers that have been freed.  
 *
 * Only pay attention to this if our layer belongs to this loop.
 * The loop will be different when a pending loop is cleared 
 * for LoopCopy.
 */
PUBLIC void InputStream::resetHistory(Loop* l)
{
	if (mLastLayer != NULL && mLastLayer->getLoop() == l)
	  mLastLayer = NULL;
}

/**
 * Initialize the stream with an input buffer for one interrupt.
 * The echo buffer is optional, if non-null we are supposed
 * to echo the level adjusted input frames to this buffer, which
 * is ususally the interrupt output buffer.
 * 
 * The first thing we do is make a copy of the buffer adjusted
 * for input level.  This is done even if the level is 100% because
 * we also calculate the max level for the meter, and echo the the
 * input to the output if enabled, so we might as well make the copy
 * and handle it consistently.
 * 
 * If a new target level has been specified, the level is changed
 * gradually a frame at a time to prevent zipper noise.  Ordinarilly,
 * the buffer will be as large or larger than the fade range.  If it is less
 * then we may spread out the 
 *
 * Next if the rate is not 1.0, we make another copy of the level
 * buffer to the rate buffer, scaled for the rate.
 *
 * The current level will apply to the entire buffer unless
 * we break the buffer into sections for events that cause the script
 * to change the level (hmm, even so with smoothing we shouldn't see
 * level changes in the current buffer).
 *
 * If we are using the SampleTrack to inject prerecorded audio into
 * the input buffer for testing, we may need to reprocess the buffer in
 * the middle.  The bufferModified method will be called by Recorder.
 * 
 */
PUBLIC void InputStream::setInputBuffer(AudioStream* aus, float* input,
										long frames, float* echo)
{
	mAudioBuffer = input;
	mAudioBufferFrames = frames;
	mOriginalFramesConsumed = 0;
	mAudioPtr = mAudioBuffer;
	mRemainingFrames = frames;

	// save for later, this shouldn't change on the fly but we have
	// to save it here so the rest of the system doesn't have to know
	// about AudioStream.
	// !! should be refreshing channels here too?
	//mSampleRate = aus->getSampleRate();

	// do an initial level adjustment, echo, and calculate the max level
    int samples = frames * 2;
    float max = 0.0f;

	// Probably doesn't add much but it feels bad to call the smoother
	// all the time when we don't need it.
	// when we don't need it

	if (mSmoother->isActive()) {
		int chan = 0;
		for (int i = 0 ; i < samples ; i ++) {
			float sample = mAudioBuffer[i];

			mLevelBuffer[i] = sample * mSmoother->getValue();
			chan++;
			if (chan >= channels) {
				mSmoother->advance();
				chan = 0;
			}

			if (echo != NULL)
			  echo[i] += sample;
        
			if (sample < 0)
			  sample = -sample;

			if (sample > max)
			  max = sample;
		}
	}
	else {
		float inLevel = mSmoother->getValue();
		for (int i = 0 ; i < samples ; i ++) {
			float sample = mAudioBuffer[i];
			mLevelBuffer[i] = sample * inLevel;

			if (echo != NULL)
			  echo[i] += sample;
        
			if (sample < 0)
			  sample = -sample;

			if (sample > max)
			  max = sample;
		}
	}

    // convert to 16 bit integer
    mMonitorLevel = (int)(max * 32767.0f);

	// do rate processing
	scaleInput();
}

/**
 * Called indirectly by Recorder when another Track (in this case SampleTrack)
 * has modified an input buffer.  If this is the one we've been processing
 * need to recapture the modified content.
 *
 * !! This really complicates smoothing since we will already have
 * advanced it and in theory now have to reset it to its original location.
 * Since this is required only for audio insertion in 
 * the unit tests assume for now that we don't have to deal with it.
 */
PUBLIC void InputStream::bufferModified(float* buffer)
{
	if (buffer == mAudioBuffer) {

		// capture the potentially new audio and level adjust
		float inLevel = mSmoother->getValue();
		long sample = mOriginalFramesConsumed * channels;
		float* src = &mAudioBuffer[sample];
		float* dest = &mLevelBuffer[sample];
		long remaining = mAudioBufferFrames - mOriginalFramesConsumed;
		long samples = remaining * channels;

		for (int i = 0 ; i < samples ; i++)
		  dest[i] = src[i] * inLevel;

		// then rate scale
		// !! the threshold is all wrong now, need to rewind it to the
		// value at the start of the buffer
		scaleInput();
	}
}


/**
 * Apply input buffer rate adjustments if the rate changed on 
 * the last event.
 */
PUBLIC void InputStream::rescaleInput()
{
	if (mSpeed != mLastSpeed) {
		// last event changed the rate, resample the remainder
		// note that this may change the buffer pointer and frame count
		scaleInput();
	}
}

/**
 * Apply rate adjustments to the remainder of the input buffer.
 */
PRIVATE void InputStream::scaleInput()
{
	float* src = &mLevelBuffer[mOriginalFramesConsumed * channels];
	long remaining = mAudioBufferFrames - mOriginalFramesConsumed;

	if (mSpeed == 1.0) {
		// we may be returning to 1.0 after being away to reset refs
		mAudioPtr = src;
		mRemainingFrames = remaining;
		mLastThreshold = 1.0f;
	}
	else {
		// !! should we reset the threshold on each rate change or
		// try to keep it constant from the last rate?
		// If we don't reset it, then we need to calculate what it 
		// would have been if the last resampling stopped at 
		// mOriginalFramesConsumed
		
        mResampler->setSpeed(mSpeed);
		mLastThreshold = mResampler->getThreshold();

		// As long as we're large enough, we don't really need to 
		// pass in an accurate destination frame number, just apply
		// the rate and deal with what comes back
		//long scaled = mResampler->scaleInputFrames(remaining);
		long scaled = 0;

		// if there is an underflow, just reduce the internal remaining
		// frame count
        mRemainingFrames = mResampler->resample(src, remaining, 
												mSpeedBuffer, scaled);
		mAudioPtr = mSpeedBuffer;
	}

	mLastSpeed = mSpeed;
}

/**
 * Consume input buffer frames and pass them to the Loop.
 * Scheduled events break up the input buffer into blocks.
 * Return the number of Loop frames we advanced so OutputStream can advance
 * the same number.
 */
PUBLIC long InputStream::record(Loop* loop, Event* event)
{
	long recordFrames = mRemainingFrames;

	mResampler->setSpeed(mSpeed);

	if (recordFrames < 0) {
		Trace(loop, 1, "InputStream advanced beyond end of buffer!\n");
        recordFrames = 0;
	}
	else if (recordFrames == 0) {
		// reached the end
		if (event != NULL)
		  Trace(loop, 1, "InputStream at end with event!\n");
	}
	else {
        if (mSpeed != mLastSpeed) {
            // last event changed the rate, resample the remainder
			// note that this may change the buffer pointer and frame count
			// since we also scaleInput when looking for events, we shouldn't
			// get here?
			scaleInput();
			recordFrames = mRemainingFrames;
		}

		// adjust the frame count if an event breaks up the input buffer
		if (event != NULL) {
			long actualFrames = event->frame - loop->getFrame();
			if (actualFrames < 0) {
				Trace(loop, 1, "Sync event frame calculation underflow!\n");
				actualFrames = 0;
			}
			else if (actualFrames > recordFrames) {
				Trace(loop, 1, "Sync event frame calculation overflow!\n");
				actualFrames = recordFrames;
			}
			recordFrames = actualFrames;
		}

		// when processing events stacked on the same frame, do not
		// go through the record/fade machinery
		if (recordFrames > 0) {

			// Detect changes to the record layer and finalize the previous
			// layer.  Can't do this at the time of the shift because
			// it is hard to know for certain what mode we'll be in when
			// we eventually get around to recording the next block.
			// This could be done within Loop::record except that out here
			// we can also check transitions to other loops.

			Layer* rec = loop->getRecordLayer();
			if (mLastLayer != NULL && mLastLayer != rec)
			  mLastLayer->finalize(this, rec);
			mLastLayer = rec;

			// reinit the AudioBuffer fields
			frames = recordFrames;
			buffer = mAudioPtr;

			// now ask the Loop to record the speed adjusted frames
			loop->record();
		}
	}

    mAudioPtr += (recordFrames * channels);
	mRemainingFrames -= recordFrames;

	// return the number of interrupt frames consumed
	// mLastTreshold was left at the threshold before the last 
	// resampling of the input buffer
	long consumed = recordFrames;
	if (mSpeed != 1.0)
	  consumed = mResampler->scaleFromInputFrames(mLastThreshold, recordFrames);

    mOriginalFramesConsumed += consumed;

	if (mOriginalFramesConsumed > mAudioBufferFrames) {
		// This might happen due to float rouding errors?  But we shouldn't
		// be off by more than one?  
		// Actually, off by 2 seems to be very common, need to figure out why
		//Trace(loop, 1, "Scaling overflow corrected %ld!\n", mOriginalFramesConsumed);
		long delta = mOriginalFramesConsumed - mAudioBufferFrames;
		consumed -= delta;
		if (consumed <= 0) {
			// I don't think this can happen, must be a calculation error
			Trace(loop, 1, "Frame scaling error!\n");
			// leave it -1 as a signal to OutputStream to play whatever
			// it has remaining
			consumed = -1;
		}
		mOriginalFramesConsumed = mAudioBufferFrames;
	}
	else if (event == NULL && mOriginalFramesConsumed < mAudioBufferFrames) {
		// must have been a rounding error?
		// this seems to happen a lot figure out why!
		//Trace(loop, 1, "Unable to consume input buffer, remainder %ld\n",
		//mAudioBufferFrames - mOriginalFramesConsumed);
		mOriginalFramesConsumed = mAudioBufferFrames;
	}

	return consumed;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
