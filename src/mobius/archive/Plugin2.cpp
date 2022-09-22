/* Copyright (C) 2005 Jeff Larson.  All rights reserved. */
//
// A Plugin is an interface for an object that processes
// audio in blocks.  The external block size may vary on each call, 
// with the plugin buffering the results of the processing
// algorithm as necessary.
//
// Implementations of some plugins contain copyrighted material.
//
// SMB:
//
// COPYRIGHT 1999-2003 Stephan M. Bernsee <smb@dspdimension.com>
//
//  						The Wide Open License (WOL)
//
// Permission to use, copy, modify, distribute and sell this software and its
// documentation for any purpose is hereby granted without fee, provided that
// the above copyright notice and this license appear in all source copies. 
// THIS SOFTWARE IS PROVIDED "AS IS" WITHOUT EXPRESS OR IMPLIED WARRANTY OF
// ANY KIND. See http://www.dspguru.com/wol.htm for more information.
//
// SoundTouch:
//
// Author        : Copyright (c) Olli Parviainen
// Author e-mail : oparviai @ iki.fi
// SoundTouch WWW: http://www.iki.fi/oparviai/soundtouch
// 

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

// SoundTouch defines BOOL, but this conflicts later when we include
// windows includes, include this early to avoid
// UPDATE: not any more but can't hurt?
//#include <windows.h>

#include "SoundTouch.h"
using namespace soundtouch;

//#include "Dirac.h"

#include "util.h"
#include "Trace.h"
#include "WaveFile.h"
#include "Audio.h"

// !! this is only for FadeWindow, try to factor this out so we don't have
// so many dependencies
#include "mobius.h"


#include "Plugin.h"

Audio* Kludge = NULL;

/****************************************************************************
 *                                                                          *
 *                                   PLUGIN                                 *
 *                                                                          *
 ****************************************************************************/

PUBLIC Plugin::Plugin()
{
    mSampleRate = 44100;
    mChannels = 2;
	mBlocks = 0;
	mBatch = false;
	mStartupFade = false;
	mStartupFadeOffset = 0;
	mTailWindow = NULL;
}

PUBLIC Plugin::~Plugin()
{
	delete mTailWindow;
}

PUBLIC void Plugin::setBatch(bool b) 
{
	mBatch = b;
}

PUBLIC void Plugin::reset()
{
}

PUBLIC void Plugin::setSampleRate(int rate)
{
    mSampleRate = rate;
}

PUBLIC void Plugin::setChannels(int channels)
{
    mChannels = channels;
}

PUBLIC void Plugin::setTweak(int tweak, int value)
{
}

PUBLIC void Plugin::debug()
{
}

/**
 * Process an inplace buffer.
 * Now that we handle startup and shutdown fades, the subclass must NOT overload this.
 */
long Plugin::process(float* buffer, long frames)
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

PUBLIC void Plugin::split(float* source, float* left, float* right, long frames)
{
    float* src = source;
    for (int i = 0 ; i < frames ; i++) {
        left[i] = *src++;
        right[i] = *src++;
    }
}

PRIVATE void Plugin::merge(float* left, float* right, float* output, long frames)
{
    float* dest = output;
    for (int i = 0 ; i < frames ; i++) {
        *dest++ = left[i];
        *dest++ = right[i];
    }
}

/**
 * Setup a Plugin Startup Fade.
 *
 * - The plugin must be in a flushed state
 * - New content begins feeding into the plugin
 * - The output of the plugin is monitored until the first non-zero sample
 * - On detction of the first non-zero sample, an up fade is applied
 * - The up fade completes, the plugin proceeds normally
 */
PUBLIC void Plugin::startupFade()
{
	mStartupFade = true;
	mStartupFadeOffset = 0;
}

/**
 * If a startup fade is active, detect the first non-zero sample comming
 * out of the plugin, and begin a fade from there.
 * This must be called ONLY if the fade has been properly initialized.
 */
PRIVATE void Plugin::doStartupFade(float* output, long frames)
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
				Trace(1, "Plugin::doStartFade invalid fade offset!\n");
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
PUBLIC void Plugin::captureFadeTail(FadeTail* tail)
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
		long actual = getFrames(buffer, range);
		if (actual >= range) 
		  tail->add(buffer, range);
		else {
			// but you lied!!
			Trace(1, "Plugin lied about available frames\n");
		}
	}
	else {
		// TODO: could try feeding zeros until we get something, if the internal
		// buffers are large may be enough in there if we push hard enough...
	}

	if (tail->getFrames() == 0) {
		// not enough, punt and do a reverse tail
		if (mTailWindow == NULL)
		  Trace(1, "Attempt to capture plugin fade tail with no tail window!\n");
		else {
			long frames = mTailWindow->reverseFade(buffer);
			tail->add(buffer, frames);
		}
	}
}

/**
 * Expected to be overloaded to return the number of frames available in the 
 * internal buffers.  Used when capturing a fade tail.
 */
PUBLIC long Plugin::getAvailableFrames()
{
	return 0L;
}

PUBLIC long Plugin::getFrames(float* buffer, long frames)
{
	return 0L;
}

PUBLIC void Plugin::putFrames(float* buffer, long frames)
{
}

/****************************************************************************
 *                                                                          *
 *                                PITCH PLUGIN                              *
 *                                                                          *
 ****************************************************************************/

PUBLIC PitchPlugin::PitchPlugin()
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
    return 0;
}

/**
 * Set the shift rate.
 */
PUBLIC void PitchPlugin::setPitch(float ratio)
{
    // !! should be doing some bounds checking on this
    mPitch = ratio;
}

/**
 * Convenience method to allow the shift to be specified in semitones.
 * For example -5 would be a fifth down.
 */
PUBLIC void PitchPlugin::setPitchSemitones(int semitones)
{
    mPitch = semitonesToRatio(semitones);
}

PUBLIC float PitchPlugin::getPitchRatio()
{
    return mPitch;
}

PUBLIC int PitchPlugin::getPitchSemitones()
{
    return ratioToSemitones(mPitch);
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

Audio* PitchPlugin::processToAudio(float* input, long frames)
{
	Audio* out = new Audio();
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

/****************************************************************************
 *                                                                          *
 *   							 SMB CHANNEL                                *
 *                                                                          *
 ****************************************************************************/

PUBLIC SmbChannel::SmbChannel()
{
    memset(gInFIFO, 0, SMB_MAX_FRAME_LENGTH*sizeof(float));
    memset(gOutFIFO, 0, SMB_MAX_FRAME_LENGTH*sizeof(float));
    memset(gFFTworksp, 0, 2*SMB_MAX_FRAME_LENGTH*sizeof(float));
    memset(gLastPhase, 0, SMB_MAX_FRAME_LENGTH*sizeof(float)/2);
    memset(gSumPhase, 0, SMB_MAX_FRAME_LENGTH*sizeof(float)/2);
    memset(gOutputAccum, 0, 2*SMB_MAX_FRAME_LENGTH*sizeof(float));
    memset(gAnaFreq, 0, SMB_MAX_FRAME_LENGTH*sizeof(float));
    memset(gAnaMagn, 0, SMB_MAX_FRAME_LENGTH*sizeof(float));
    gRover = 0;
}

PUBLIC SmbChannel::~SmbChannel()
{
}
    
/**
 * Author: (c)1999-2002 Stephan M. Bernsee <smb@dspdimension.com>
 * Purpose: doing pitch shifting while maintaining duration using the Short
 * Time Fourier Transform.
 */
PRIVATE void SmbChannel::process(float pitchShift, 
								 long numSampsToProcess, 
								 long fftFrameSize, 
								 long osamp, 
								 float sampleRate, 
								 float *indata, 
								 float *outdata)
{

	double magn, phase, tmp, window, real, imag;
	double freqPerBin, expct;
	long i,k, qpd, index, inFifoLatency, stepSize, fftFrameSize2, fadeZoneLen;
	long stored = 0;

	/* set up some handy variables */
	fadeZoneLen = fftFrameSize/2;
	fftFrameSize2 = fftFrameSize/2;
	stepSize = fftFrameSize/osamp;
	freqPerBin = sampleRate/(double)fftFrameSize;
	expct = 2.*M_PI*(double)stepSize/(double)fftFrameSize;
	inFifoLatency = fftFrameSize-stepSize;
	if (gRover == 0) gRover = inFifoLatency;

	/* main processing loop */
	for (i = 0; i < numSampsToProcess; i++){

		/* As long as we have not yet collected enough data just read in */
		gInFIFO[gRover] = indata[i];
		outdata[i] = gOutFIFO[gRover-inFifoLatency];
		gRover++;

		/* now we have enough data for processing */
		if (gRover >= fftFrameSize) {
			gRover = inFifoLatency;

			/* do windowing and re,im interleave */
			for (k = 0; k < fftFrameSize;k++) {
				window = (float)(-.5*cos(2.*M_PI*(double)k/(double)fftFrameSize)+.5);
				gFFTworksp[2*k] = (float)(gInFIFO[k] * window);
				gFFTworksp[2*k+1] = 0.;
			}


			/* ***************** ANALYSIS ******************* */
			/* do transform */
			smbFft(gFFTworksp, fftFrameSize, -1);

			/* this is the analysis step */
			for (k = 0; k <= fftFrameSize2; k++) {

				/* de-interlace FFT buffer */
				real = gFFTworksp[2*k];
				imag = gFFTworksp[2*k+1];

				/* compute magnitude and phase */
				magn = 2.*sqrt(real*real + imag*imag);
				phase = atan2(imag,real);

				/* compute phase difference */
				tmp = phase - gLastPhase[k];
				gLastPhase[k] = (float)phase;

				/* subtract expected phase difference */
				tmp -= (double)k*expct;

				/* map delta phase into +/- Pi interval */
				qpd = (long)(tmp/M_PI);
				if (qpd >= 0) qpd += qpd&1;
				else qpd -= qpd&1;
				tmp -= M_PI*(double)qpd;

				/* get deviation from bin frequency from the +/- Pi interval */
				tmp = osamp*tmp/(2.*M_PI);

				/* compute the k-th partials' true frequency */
				tmp = (double)k*freqPerBin + tmp*freqPerBin;

				/* store magnitude and true frequency in analysis arrays */
				gAnaMagn[k] = (float)magn;
				gAnaFreq[k] = (float)tmp;

			}

			/* ***************** PROCESSING ******************* */
			/* this does the actual pitch shifting */
			memset(gSynMagn, 0, fftFrameSize*sizeof(float));
			memset(gSynFreq, 0, fftFrameSize*sizeof(float));
			for (k = 0; k <= fftFrameSize2; k++) {
				index = (long)(k/pitchShift);
				if (index <= fftFrameSize2) {
					gSynMagn[k] += gAnaMagn[index];
					gSynFreq[k] = gAnaFreq[index] * pitchShift;
				}
			}

			/* ***************** SYNTHESIS ******************* */
			/* this is the synthesis step */
			for (k = 0; k <= fftFrameSize2; k++) {

				/* get magnitude and true frequency from synthesis arrays */
				magn = gSynMagn[k];
				tmp = gSynFreq[k];

				/* subtract bin mid frequency */
				tmp -= (double)k*freqPerBin;

				/* get bin deviation from freq deviation */
				tmp /= freqPerBin;

				/* take osamp into account */
				tmp = 2.*M_PI*tmp/osamp;

				/* add the overlap phase advance back in */
				tmp += (double)k*expct;

				/* accumulate delta phase to get bin phase */
				gSumPhase[k] += (float)tmp;
				phase = gSumPhase[k];

				/* get real and imag part and re-interleave */
				gFFTworksp[2*k] = (float)(magn*cos(phase));
				gFFTworksp[2*k+1] = (float)(magn*sin(phase));
			} 

			/* zero negative frequencies */
			for (k = fftFrameSize+2; k < 2*fftFrameSize; k++) gFFTworksp[k] = 0.;

			/* do inverse transform */
			smbFft(gFFTworksp, fftFrameSize, 1);

			/* do windowing and add to output accumulator */ 
			for(k=0; k < fftFrameSize; k++) {
				window = -.5*cos(2.*M_PI*(double)k/(double)fftFrameSize)+.5;
				gOutputAccum[k] += (float)(2.*window*gFFTworksp[2*k]/(fftFrameSize2*osamp));
			}
			for (k = 0; k < stepSize; k++) gOutFIFO[k] = gOutputAccum[k];

			/* shift accumulator */
			memmove(gOutputAccum, gOutputAccum+stepSize, fftFrameSize*sizeof(float));

			/* move input FIFO */
			for (k = 0; k < inFifoLatency; k++) gInFIFO[k] = gInFIFO[k+stepSize];
		}
	}
}

/**
 * FFT routine, (C)1996 S.M.Bernsee. 
 *
 * Sign = -1 is FFT, 1 is iFFT (inverse)
 * 
 * Fills fftBuffer[0...2*fftFrameSize-1] with the Fourier transform of the
 * time domain data in fftBuffer[0...2*fftFrameSize-1]. The FFT array takes
 * and returns the cosine and sine parts in an interleaved manner, ie.
 * fftBuffer[0] = cosPart[0], fftBuffer[1] = sinPart[0], asf. 
 *
 * fftFrameSize	must be a power of 2. It expects a complex input signal
 * (see footnote 2), ie. when working with 'common' audio signals our
 * input signal has to be passed as {in[0],0.,in[1],0.,in[2],0.,...} asf. 
 * In that case, the transform of the frequencies of interest is in
 * fftBuffer[0...fftFrameSize].
 */
PRIVATE void SmbChannel::smbFft(float *fftBuffer, 
                                    long fftFrameSize, 
                                    long sign)
{
	float wr, wi, arg, *p1, *p2, temp;
	float tr, ti, ur, ui, *p1r, *p1i, *p2r, *p2i;
	long i, bitm, j, le, le2, k;

	for (i = 2; i < 2*fftFrameSize-2; i += 2) {
		for (bitm = 2, j = 0; bitm < 2*fftFrameSize; bitm <<= 1) {
			if (i & bitm) j++;
			j <<= 1;
		}
		if (i < j) {
			p1 = fftBuffer+i; p2 = fftBuffer+j;
			temp = *p1; *(p1++) = *p2;
			*(p2++) = temp; temp = *p1;
			*p1 = *p2; *p2 = temp;
		}
	}
	for (k = 0, le = 2; k < (long)(log((double)fftFrameSize)/log(2.)); k++) {
		le <<= 1;
		le2 = le>>1;
		ur = 1.0;
		ui = 0.0;
		arg = (float)(M_PI / (le2>>1));
		wr = (float)cos(arg);
		wi = (float)(sign*sin(arg));
		for (j = 0; j < le2; j += 2) {
			p1r = fftBuffer+j; p1i = p1r+1;
			p2r = p1r+le2; p2i = p2r+1;
			for (i = j; i < 2*fftFrameSize; i += le) {
				tr = *p2r * ur - *p2i * ui;
				ti = *p2r * ui + *p2i * ur;
				*p2r = *p1r - tr; *p2i = *p1i - ti;
				*p1r += tr; *p1i += ti;
				p1r += le; p1i += le;
				p2r += le; p2i += le;
			}
			tr = ur*wr - ui*wi;
			ui = ur*wi + ui*wr;
			ur = tr;
		}
	}
}

/**
 * 12/12/02, smb
 *
 * PLEASE NOTE:
 *     
 * There have been some reports on domain errors when the atan2() function 
 * was used as in the above code. Usually, a domain error should not
 * interrupt the program flow (maybe except in Debug mode) but rather be
 * handled "silently" and a global variable should be set according to this
 * error. However, on some occasions people ran into this kind of scenario, 
 * so a replacement atan2() function is provided here.
 *
 * If you are experiencing domain errors and your program stops, 
 * simply replace all instances of atan2() with calls to the smbAtan2() 
 * function below.
 */
double smbAtan2(double x, double y)
{
    double signx;
    if (x > 0.) signx = 1.;  
    else signx = -1.;
  
    if (x == 0.) return 0.;
    if (y == 0.) return signx * M_PI / 2.;
  
    return atan2(x, y);
}

/****************************************************************************
 *                                                                          *
 *   							  SMB PLUGIN                                *
 *                                                                          *
 ****************************************************************************/

PUBLIC SmbPitchPlugin::SmbPitchPlugin()
{
	// must be a power of two, typical values are 1024, 2048, 4096
	// but this requires an *extreme* amount of CPU
	// I was able to run with 1024/4 but just barely.  CPU was 66% and doing
	// ANYTHING else like changing window focus would cause clicks
    mFFTFrameSize = 512;

	// author recommends at least 4 for moderate ratios, and 32 for best quality
	// this also seems to have a dramatic effect on performance
	// I was not able to put this above 4
    mOversamplingFactor = 4;

	mLeftChannel = new SmbChannel();
	mRightChannel = new SmbChannel();
}

PUBLIC SmbPitchPlugin::~SmbPitchPlugin()
{
	delete mLeftChannel;
	delete mRightChannel;
}
    
/**
 * Set the shift rate.  According to the comments this
 * algorithm can only shift between 0.5 and 2.  Not sure if that's
 * true, but let's restrict it for now.
 */
PUBLIC void SmbPitchPlugin::setPitch(float ratio)
{
	if (ratio >= 0.5 && ratio <= 2.0)
      mPitch = ratio;
}

PUBLIC void SmbPitchPlugin::setFFTFrameSize(int size)
{
    if (size == 64 ||
        size == 128 ||
        size == 256 ||
        size == 512 ||
        size == 1024 ||
        size == 2048 ||
        size == 4096 ||
        size == 8192)
      mFFTFrameSize = size;
}

PUBLIC void SmbPitchPlugin::setOversamplingFactor(int factor)
{
    // not exactly sure what the range should be
    if (factor >= 4 && factor <= 64)
      mOversamplingFactor = factor;
}

PUBLIC long SmbPitchPlugin::process(float* input, float* output, 
                                    long frames)
{
    split(input, mLeftIn, mRightIn, frames);

	memset(mLeftOut, 0, frames);
	memset(mRightOut, 0, frames);

    mLeftChannel->process(mPitch, frames, mFFTFrameSize, mOversamplingFactor, 
						  (float)mSampleRate, mLeftIn, mLeftOut);

    mRightChannel->process(mPitch, frames, mFFTFrameSize, mOversamplingFactor, 
						   (float)mSampleRate, mRightIn, mRightOut);

    merge(mLeftOut, mRightOut, output, frames);
	return frames;
}

/**
 * jsl - use the SMB algorithm on a file, this is not part of the
 * original SMB code.
 */
void SmbPitchPlugin::process(WaveFile* file, int semitones)
{
	// convert semitones to factor
	float pitchShift = (float)pow(2., semitones/12.);	
	
	float* left = file->getChannelSamples(0);
	float* right = file->getChannelSamples(1);
	long frames = file->getFrames();
	float rate = (float)file->getSampleRate();

	if (left != NULL) 
	  mLeftChannel->process(pitchShift, frames, 2048, 4, rate, left, left);

	if (right != NULL)
	  mRightChannel->process(pitchShift, frames, 2048, 4, rate, right, right);

	file->setSamples(left, right, frames);
}

/****************************************************************************
 *                                                                          *
 *                             SOUND TOUCH PLUGIN                           *
 *                                                                          *
 ****************************************************************************/
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

PUBLIC SoundTouchPlugin::SoundTouchPlugin()
{
	mSoundTouch = new SoundTouch();
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
	mSoundTouch->flush();
}

PUBLIC SoundTouchPlugin::~SoundTouchPlugin()
{
    delete mSoundTouch;
}

PUBLIC void SoundTouchPlugin::reset()
{
	mFramesIn = 0;
	mFramesOut = 0;
	mSoundTouch->flush();
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
PUBLIC void SoundTouchPlugin::setPitch(float ratio)
{
	if (mPitch != ratio) {
		mPitch = ratio;
		mSoundTouch->setPitch(ratio);

		// a fade tail must have been drained from the plugin by now
		reset();

		// recalculate latency
		mLatency = deriveLatency();

		// arm a startup fade
		startupFade();
	}
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
PRIVATE long SoundTouchPlugin::deriveLatency()
{
	float buffer[256 * 2];		// !! channels
	long latency = 0;
	long added = 0;
	long avail = 0;

	mSoundTouch->flush();

	// !! dangerous, if something is misconfigured could infinite loop?
	while (avail == 0) {
		mSoundTouch->putSamples(buffer, 256);
		added += 256;
		avail = mSoundTouch->numSamples();
	}

	// this is a voodoo calculation, see notes above
	latency = added + 256;

	Trace(2, "Pitch shifter scale %ld latency %ld (%ld frames in, %ld available)\n", 
		  (long)ratioToSemitones(mPitch), latency, added, avail);
	
	mSoundTouch->flush();
	
	return latency;
}

PUBLIC void SoundTouchPlugin::setPitchSemitones(int semitones)
{
    mPitch = semitonesToRatio(semitones);
    mSoundTouch->setPitchSemiTones(semitones);
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
			if (mLatency == 0 || mFramesIn < mLatency) {
				request = 0;
				gap = frames;
			}
			else if (avail < frames) {
				// get what we can and add a gap just so we can continue
				// if the gap is small we could interpolate!
				request = avail;
				gap = frames - avail;
				Trace(1, "Pitch: stream shortfall %ld\n", gap);
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

/****************************************************************************
 *                                                                          *
 *   							DIRAC CHANNEL                               *
 *                                                                          *
 ****************************************************************************/
//
// NOTE: Dirac is not properly handling the mBatch flag for file
// processing so it will produce a gap at the beginning.
// Can fix, but we're not going to be using this for streams anyway.
//
#if 0
/**
 * Callback function we have to register with Dirac to call as it
 * needs input frames.
 *
 * The channels parameter is a array of float buffers, one for each
 * channel.  In the demo version we should only assume there is one
 * channel.
 */
PRIVATE long DiracCallback(float** channels, long frames, void* application)
{
	long read = 0;

	if (application != NULL)
	  read = ((DiracChannel*)application)->callback(channels, frames);

	return read;
}

PUBLIC DiracChannel::DiracChannel(int channel)
{
	mChannel = channel;
	mHead = mBuffer;
	mTail = mBuffer;
	mAvailable = 0;
	mLatency = 0;

	for (int i = 0 ; i < DIRAC_MAX_BUFFER ; i++)
	  mBuffer[i] = 0.0f;

	/**
	 * From the example file:
	 * First we set up DIRAC LE to process one channel of audio at 44.1kHz
	 * N.b.: The fastest option is kDiracLambdaPreview / kDiracQualityPreview
	 * The probably best default option for general purpose signals is 
	 * kDiracLambda3 / kDiracQualityGood
	 *
	 * jsl: third arg is number of channels, supposedly the free version
	 * can only tolerate 1
	 *
	 * Lambda3/Preview is too slow for realtime.  
	 * Preview/Preview still didn't work, but may be something else?
	 */
	mDirac = DiracCreate(kDiracLambdaPreview, kDiracQualityPreview, 1, 44100., 
						 &DiracCallback);

	if (mDirac == NULL) {
		// may happen if we try to ask for more than one channel in the
		// demo version, or use an unsupported sample rate
		Trace(1, "Could not create DIRAC instance");
	}
	else {
		// DIRAC is capable of both time and pitch shifting, example:
		//float time = 1.33;                 // 133% length
		//float pitch = pow(2., 0./12.);     // no pitch shift (0 semitones)
		//DiracSetProperty(kDiracPropertyTimeFactor, time, dirac);
		//DiracSetProperty(kDiracPropertyPitchFactor, pitch, dirac);

		// this seems to be no predictor on what the initial request
		// size will be, 
		//printf("Dirac input buffer size %ld\n", 
		//DiracGetInputBufferSizeInFrames(mDirac));
	}

}

PUBLIC DiracChannel::~DiracChannel()
{
	DiracDestroy(mDirac);
}

PUBLIC void DiracChannel::reset()
{
	mHead = mBuffer;
	mTail = mBuffer;
	mAvailable = 0;

	// no, this stays the same for file flushing?
	//mLatency = 0;

	DiracReset(mDirac);
}

PUBLIC void DiracChannel::setPitch(float ratio)
{
	DiracSetProperty(kDiracPropertyPitchFactor, ratio, mDirac);
}

PUBLIC void DiracChannel::setTempo(float tempo)
{
	DiracSetProperty(kDiracPropertyTimeFactor, tempo, mDirac);
}

PUBLIC int DiracChannel::getLatency()
{
    return mLatency;
}

/**
 * Called by DiracPlugin as we accumulate buffer samples, before
 * DIRAC asks us for more.
 */
PUBLIC void DiracChannel::add(float* input, long frames, int channels)
{
	long remaining = frames;

	// locate our channel
	float* src = &input[mChannel];

	// this should never happen?
	// what's the best thing, keep the current block and lose part of the
	// last one, or the other way around, I guess it doesn't matter because
	// either way there will be a discontinuity

	if (mAvailable + frames > DIRAC_MAX_BUFFER) {
		Trace(1, "Pitch channel %ld overflow %ld\n",
			  (long)mChannel, (mAvailable + frames) - DIRAC_MAX_BUFFER);
		remaining = DIRAC_MAX_BUFFER - mAvailable;
	}

	float* last = &(mBuffer[DIRAC_MAX_BUFFER]);
	while (remaining > 0) {
		*mHead++ = *src;
		if (mHead == last)
		  mHead = mBuffer;
		src += channels;
		remaining--;
		mAvailable++;
	}
}

/**
 * Called by DiracPlugin to process what is currently in the queue.
 * The output buffer must be non-interleaved.
 */
PUBLIC void DiracChannel::process(float* output, long frames)
{
	long actual = DiracProcess(output, frames, this, mDirac);

	if (actual < frames) {
		Trace(1, "Pitch channel %ld did not process enough frames:"
			  " expected %ld got %ld\n", mChannel, frames, actual);
	}
	else if (actual > frames) {
		Trace(1, "Pitch channel %ld processed too many frames:"
			  " expected %ld got %ld\n", mChannel, frames, actual);
	}
}

/**
 * Called by DIRAC (via the registered callback function) as it
 * needs frames for processing.
 *
 * The channels parameter is a array of float buffers, one for each
 * channel.  In the demo version we should only assume there is one
 * channel.
 *
 * Return the number of channels we gave it, can this actually be less?
 */
PUBLIC long DiracChannel::callback(float** outputs, long frames)
{
	float* dest = outputs[0];
	long remaining = frames;

	if (frames > mAvailable) {

		// this should only happen on the first call, after which
		// we keep the buffer full
		if (mAvailable == 0 && mLatency == 0 ) {
			Trace(1, "Pitch channel %ld initial latency %ld\n",
				  (long)mChannel, frames);
			mLatency = frames;
		}
		else {
			Trace(1, "Pitch channel %ld underflow %ld samples\n",
				  (long)mChannel, frames - mAvailable);
		}
	}

	// copy what we have
	float* last = &mBuffer[DIRAC_MAX_BUFFER];
	while (remaining > 0 && mAvailable > 0) {
		*dest++ = *mTail++;
		if (mTail == last)
		  mTail = mBuffer;
		remaining--;
		mAvailable--;
	}

	// then padd the overflow with zeros
	while (remaining > 0) {
		*dest++ = 0.0f;
		remaining--;
	}

	return frames;
}

/****************************************************************************
 *                                                                          *
 *   							 DIRAC PLUGIN                               *
 *                                                                          *
 ****************************************************************************/

PUBLIC DiracPlugin::DiracPlugin()
{
	mLeft = new DiracChannel(0);
	mRight = new DiracChannel(1);
}

PUBLIC DiracPlugin::~DiracPlugin()
{
	delete mLeft;
	delete mRight;
}

PUBLIC void DiracPlugin::reset()
{
	mLeft->reset();
	mRight->reset();
}

PUBLIC void DiracPlugin::setPitch(float ratio)
{
    mPitch = ratio;
	mLeft->setPitch(mPitch);
	mRight->setPitch(mPitch);
}

PUBLIC void DiracPlugin::setPitchSemitones(int semitones)
{
	setPitch(semitonesToRatio(semitones));
}

PUBLIC void DiracPlugin::setTempo(float tempo)
{
	mLeft->setTempo(tempo);
	mRight->setTempo(tempo);
}

PUBLIC int DiracPlugin::getLatency()
{
    return mLeft->getLatency();
}

/**
 * Called by a Stream to process one audio interrupt block.
 */
PUBLIC long DiracPlugin::process(float* input, float* output, long frames)
{
	memset(mLeftOut, 0, frames);
	memset(mRightOut, 0, frames);

	// accumulate the new frames in the channel buffers
	mLeft->add(input, frames, mChannels);
	mRight->add(input, frames, mChannels);

	// let each chanel process and produce output
	mLeft->process(mLeftOut, frames);
	mRight->process(mRightOut, frames);

	// interleave the results
    merge(mLeftOut, mRightOut, output, frames);

	return frames;
}

Audio* DiracPlugin::process(WaveFile* file, int semitones)
{
	Audio* audio = new Audio();
	float* input = file->getData();
	float output[PLUGIN_MAX_BLOCK_SIZE * 2];
	long remaining = file->getFrames();
	long blockSize = 256;
	long blocks = 1;

	reset();
	setPitchSemitones(semitones);

	while (remaining > 0) {

		printf("%ld: DiracProcess %ld\n", blocks, blockSize);
		long actual = process(input, output, blockSize);
		if (actual > 0) {
			audio->append(output, actual);
			remaining -= actual;
		}
		else {
			// something is horribly wrong!
			printf("process returned 0!\n");
			remaining = 0;
		}
		blocks++;
	}

	return audio;
}
#endif

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
