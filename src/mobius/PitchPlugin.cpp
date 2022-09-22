/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * SoundTouch:
 *
 * Author        : Copyright (c) Olli Parviainen
 * Author e-mail : oparviai @ iki.fi
 * SoundTouch WWW: http://www.iki.fi/oparviai/soundtouch
 *
 * ---------------------------------------------------------------------
 *
 * Implementations of the PitchPlugin interface.
 *
 * Originally we had several implementations on Windows, SoundTouch,
 * Dirac, and SMB.  In practice only SoundTouch was fast enough for
 * real-time use.
 *
 * During the OS X port PseudoPlugin was used until SoundTouch was
 * ported.  It should no longer be used but may come in handy.
 *
 *
 * We started with PseudoPlugin during initial porting, then added
 * SoundTouchPlugin.
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "SoundTouch.h"
using namespace soundtouch;

#include "Util.h"
#include "Trace.h"
#include "WaveFile.h"

#include "Audio.h"
#include "StreamPlugin.h"
#include "FadeWindow.h"

Audio* Kludge = NULL;

//////////////////////////////////////////////////////////////////////
//
// Pseudo plugin
//
//////////////////////////////////////////////////////////////////////

class PseudoPlugin : public PitchPlugin {

  public:

    PseudoPlugin(int sampleRate);
    ~PseudoPlugin();
   
	// pure virtual from Plugin
    long process(float* input, float* output, long frames);

  protected:

	// pure virtual from PitchPlugin
	void updatePitch();

};


PUBLIC PseudoPlugin::PseudoPlugin(int sampleRate)
    : PitchPlugin(sampleRate)
{
}

PUBLIC PseudoPlugin::~PseudoPlugin()
{
}

PUBLIC long PseudoPlugin::process(float* input, float* output, long frames)
{
	return frames;
}

PRIVATE void PseudoPlugin::updatePitch()
{
}

//////////////////////////////////////////////////////////////////////
//
// SoundTouchPlugin
//
//////////////////////////////////////////////////////////////////////

/**
 * This implements PitchPlugin, but it also has methods for
 * time stretch and rate change.  If we ever have more than
 * one of these factor out interfaces for time/rate plugins.
 */
class SoundTouchPlugin : public PitchPlugin {

  public:

    SoundTouchPlugin(int sampleRate);
    ~SoundTouchPlugin();

	void reset();
	void setTweak(int tweak, int value);
	void debug();

    // Time stretch
    void setTempo(float tempo);
    
    // Playback rate
    void setRate(float rate);

	long process(float* input, float* output, long frames);

    int getLatency();

  protected:

    void updatePitch();
	long getAvailableFrames();
	long getFrames(float* buffer, long frames);
	void putFrames(float* buffer, long frames);
	
  private:

	// cached scale latencies, 12 on either side and a center
	static bool Cached;
	static int CachedLatencies[];

	void cacheCalculations();
	long deriveLatency(int scale);
	void flush();

    /**
     * SoundTouch API object.
     */
    class SoundTouch* mSoundTouch;

    long mFramesIn;
    long mFramesOut;
    int mLatency;
	int mFixedLatency;


};

/*
 * Parameters and settings (from SoundTouch.h)
 *
 * void setRate(float newRate);
 * Sets new rate control value. Normal rate = 1.0, smaller values
 * represent slower rate, larger faster rates.
 *
 * void setTempo(float newTempo);
 * Sets new tempo control value. Normal tempo = 1.0, smaller values
 * represent slower tempo, larger faster tempo.
 *
 * void setRateChange(float newRate);
 * Sets new rate control value as a difference in percents compared
 * to the original rate (-50 .. +100 %).
 *
 * void setTempoChange(float newTempo);
 * Sets new tempo control value as a difference in percents compared
 * to the original tempo (-50 .. +100 %)
 *
 * void setPitch(float newPitch);
 * Sets new pitch control value. Original pitch = 1.0, smaller values
 * represent lower pitches, larger values higher pitch.
 * 
 * void setPitchOctaves(float newPitch);
 * Sets pitch change in octaves compared to the original pitch  
 * (-1.00 .. +1.00)
 *
 * void setPitchSemiTones(int newPitch);
 * void setPitchSemiTones(float newPitch);
 * Sets pitch change in semi-tones compared to the original pitch
 * (-12 .. +12)
 *
 * SETTING_USE_AA_FILTER
 * Enable/disable anti-alias filter in pitch transposer (0 = disable)
 *
 * SETTING_AA_FILTER_LENGTH
 * Pitch transposer anti-alias filter length (8 .. 128 taps, default = 32)
 *
 * SETTING_USE_QUICKSEEK
 * Enable/disable quick seeking algorithm in tempo changer routine
 * (enabling quick seeking lowers CPU utilization but causes a minor sound
 * quality compromising)
 *
 * SETTING_SEQUENCE_MS
 * Time-stretch algorithm single processing sequence length in milliseconds. 
 * This determines to how long sequences the original sound is chopped in the
 * time-stretch algorithm.
 *
 * SETTING_SEEKWINDOW_MS
 * Time-stretch algorithm seeking window length in milliseconds for algorithm
 * that finds the best possible overlapping location. This determines from
 * how wide window the algorithm may look for an optimal joining location when
 * mixing the sound sequences back together. 
 *
 * SETTING_OVERLAP_MS
 * Time-stretch algorithm overlap length in milliseconds. When the chopped
 * sound sequences are mixed back together, to form a continuous sound
 * stream, this parameter defines over how long period the two consecutive
 * sequences are let to overlap each other. 
 *
 */

bool SoundTouchPlugin::Cached = false;

/**
 * Latencies by scale degree.  Calculated with deriveLatency() but
 * that's too expensive to run every time
 */
int SoundTouchPlugin::CachedLatencies[] = {
	4352,
	4352,
	4352,
	4608,
	4608,
	4608,
	4608,
	4864,
	4864,
	4864,
	4864,
	5120,
	0,
	5120,
	5120,
	5120,
	5120,
	5120,
	5120,
	5120,
	5120,
	5120,
	5120,
	5120,
	5120
};

PUBLIC SoundTouchPlugin::SoundTouchPlugin(int sampleRate)
    : PitchPlugin(sampleRate)
{
	mSoundTouch = new SoundTouch();

    // !! ST throws std exceptions if it is misconfigured, should
    // try to capture those and disable the plugin
    mSoundTouch->setSampleRate(mSampleRate);
	mSoundTouch->setChannels(mChannels);

    mSoundTouch->setSetting(SETTING_USE_AA_FILTER, 1);

	// enable for better effeciency, poorer sound
    //mSoundTouch->setSetting(SETTING_USE_QUICKSEEK, 1);

	// default 32, 64 doesn't sound better
    //mSoundTouch->setSetting(SETTING_AA_FILTER_LENGTH, 64);

	// default 82, larger value better for slowing down tempo
	// larger value reduces CPU
    mSoundTouch->setSetting(SETTING_SEQUENCE_MS, 82);

	// default 28, relatively large default for slowing down tempo
	// larger value eases finding a good "mixing position" but
	// may cause "drifting" artifact
	// larger value increases CPU
    mSoundTouch->setSetting(SETTING_SEEKWINDOW_MS, 14);

	// default 12, relatively large to suit other defaults
	// lower this if SEQUENCE_MS is also lowered
    mSoundTouch->setSetting(SETTING_OVERLAP_MS, 12);
    
    mFramesIn = 0;
    mFramesOut = 0;
	mLatency = 0;

	// try using a fade window for shutdown fades
	mTailWindow = new FadeWindow();

	// doesn't seem to happen automatically for some reason?
	flush();

	// need control over when this happens?
	// as long as we don't do this during VST probing its probably ok, 
	// has to happen some time...
	cacheCalculations();
}

/**
 * Clear out any lingering samples buffered in the plugin.
 * Tried to do this with SoundTouch::flush, then draining the output buffers,
 * but it didn't work.  Added the reset() method.
 */
PUBLIC void SoundTouchPlugin::flush()
{
	mSoundTouch->reset();
}

PUBLIC void SoundTouchPlugin::cacheCalculations()
{
	if (!Cached) {
		for (int i = -12 ; i <= 12 ; i++) {

			// this is too expensive, use the precalculated ones
			//Latencies[i] = deriveLatency(i);
		}
		Cached = true;
	}
}

PUBLIC SoundTouchPlugin::~SoundTouchPlugin()
{
    delete mSoundTouch;
}

PUBLIC void SoundTouchPlugin::reset()
{
	mFramesIn = 0;
	mFramesOut = 0;
	flush();
	mTailWindow->reset();
}

PUBLIC void SoundTouchPlugin::debug()
{
	if (Kludge != NULL) {
		Kludge->write("touch.wav");
		Kludge->reset();
	}
}

PUBLIC void SoundTouchPlugin::setTweak(int tweak, int value)
{
	// !! here we can do something
}

/**
 * Changing pitch in this algoithm seems to alter the latency as well
 * so derive it every time.  Shouldn't be that expensive.  Changes in pitch
 * also appear to disrupt the envelope so we have to reset and force a 
 * startup fade in.  Might be able to avoid some of this if I understood
 * the algorithm better, but this is a good worst case scenario that needs
 * to be handled.
 */
PUBLIC void SoundTouchPlugin::updatePitch()
{
	// a fade tail must have been drained from the plugin by now
	reset();

	// not reliable?
    //mSoundTouch->setPitchSemiTones(mScalePitch);
	mSoundTouch->setPitch(mPitch);

	// recalculate latency
	if (mPitchStep >= -12 && mPitchStep <= 12)
	  mLatency = CachedLatencies[mPitchStep + 12];
    else {
        // !! need to guess
    }

	// arm a startup fade
	startupFade();
}

/**
 * Derive plugin latency by passing garbage through it until something comes out.
 *
 * Still not sure on exactly the right formula for this, but just counting
 * the number of frames in until something squirts out isn't enough, there are
 * still periodic shortfalls.  This seems to be fairly accurate with negative shifts,
 * but for positive shifts need much more.  
 *
 * For an up shift of 1, it takes 4864 frames (19 blocks) of inputs then
 * we suddenly get 2882 frames available.  Unclear how we can find out the minimum
 * number of input frames to cause some output, but it really doesn't matter.  Be
 * conservative and assume the worst.  Unfortunately this doesn't seem to be enough
 * in all cases.
 */
PRIVATE long SoundTouchPlugin::deriveLatency(int scale)
{
	long latency = 0;

	if (scale != 0) {
		
		long saveScale = mPitchStep;

		float buffer[256 * 2];		// !! channels
		long added = 0;
		long avail = 0;

		mSoundTouch->reset();
		mSoundTouch->setPitch(semitonesToRatio(scale));

		// !! dangerous, if something is misconfigured could infinite loop?
		while (avail == 0) {
			mSoundTouch->putSamples(buffer, 256);
			added += 256;
			avail = mSoundTouch->numSamples();
		}

		// this is a voodoo calculation, see notes above
		latency = added + 256;

		Trace(2, "Pitch shifter scale %ld latency %ld (%ld frames in, %ld available)\n", 
			  (long)scale, (long)latency, (long)added, (long)avail);
			
		mSoundTouch->reset();
		mSoundTouch->setPitch(semitonesToRatio(saveScale));
	}

	return latency;
}

PUBLIC void SoundTouchPlugin::setTempo(float tempo)
{
    mSoundTouch->setTempo(tempo);
}

PUBLIC void SoundTouchPlugin::setRate(float rate)
{
    mSoundTouch->setRate(rate);
}

PUBLIC int SoundTouchPlugin::getLatency()
{
    return mLatency;
}

/**
 * Expected to be overloaded to return the number of frames available in the 
 * internal buffers.  Used when capturing a fade tail.
 */
PUBLIC long SoundTouchPlugin::getAvailableFrames()
{
	return mSoundTouch->numSamples();
}

/**
 * Return some number of already buffered frames.  Used when capturing
 * a fade tail.
 */
PUBLIC long SoundTouchPlugin::getFrames(float* buffer, long frames)
{
	return mSoundTouch->receiveSamples(buffer, frames);
}

/**
 * Force some frames into the internal buffers.
 * Used only during capturing of a fade tail.
 */
PUBLIC void SoundTouchPlugin::putFrames(float* buffer, long frames)
{
	mSoundTouch->putSamples(buffer, frames);
}

/**
 * SoundTouch does not guarantee that there will be the desired
 * number of frames available on each call due to internal buffering, 
 * and sometimes it may have more than requested.  It looks like it
 * the internal buffering is nicely done so we don't have to worry about
 * overflow at this level.  And thankfullly it deals with 
 * interleaved channels.
 *
 * There does however appear to be some additional buffering latency
 * beyond that reported on the first call.  Periodically there can
 * be a shortfall of 1, and occasionally as high as 241.  I'm guessing
 * that the initial latency may be off by up to 256.
 *
 * The initial latency varies by shift, -12 reports 3840 (15 * 256) 
 * and this rises gradually to 4608 at +12 (18 * 256).
 */
PUBLIC long SoundTouchPlugin::process(float* input, float* output, 
                                      long frames)
{
	long returned = 0;
	
    if (frames > 0) {

        // always feed in
		if (input != NULL) {

			if (Kludge != NULL)
			  Kludge->append(input, frames);

			mSoundTouch->putSamples(input, frames);
			mFramesIn += frames;
		}

		// number available may not be enough
        long avail = mSoundTouch->numSamples();

		//Trace(1, "Block %ld requested %ld available %ld out %ld\n",
		//mBlocks, frames, avail, mFramesOut);

		long request = frames;
		long gap = 0;

		if (mBatch) {
			// In batch mode, request up to the desired amount, but if
			// fewer are available, get what we can.
			if (avail < frames)
			  request = avail;
		}
		else {
			// In stream mode, don't begin asking for samples until we've
			// buffered a sufficient amount.  After which there should
			// always be enough! (cheers Jordan)
			if (mFramesIn < mLatency) {
				request = 0;
				gap = frames;
			}
			else if (avail < frames) {
				// get what we can and add a gap just so we can continue
				// if the gap is small we could interpolate!
				request = avail;
				gap = frames - avail;
				Trace(1, "Pitch: stream shortfall %ld\n", (long)gap);
			}

			// whether we're buffering or have a shortfall, add a gap
			// does it matter which side this goes on?
			if (gap > 0) {
				long emptySamples = gap * mChannels;
				for (int i = 0 ; i < emptySamples ; i++)
				  output[i] = 0.0f;
				output += emptySamples;
			}
		}

		// now ask
		long received = 0;
		if (request > 0) {
			received = mSoundTouch->receiveSamples(output, request);
			if (received != request) {
				Trace(1, "SoundTouch: numSamples/receiveSamples mismatch!\n");
				// could try to be smart and add another gap, but this
				// really should not happen
			}
		}

		mFramesOut += received;
		returned = received + gap;
	}

	mBlocks++;
	return returned;
}

//////////////////////////////////////////////////////////////////////
//
// Factory method
//
//////////////////////////////////////////////////////////////////////

PUBLIC PitchPlugin* PitchPlugin::getPlugin(int sampleRate)
{
	//return new PseudoPlugin();
	return new SoundTouchPlugin(sampleRate);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
