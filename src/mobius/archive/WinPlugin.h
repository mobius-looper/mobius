/* Copyright (C) 2004 Jeff Larson.  All rights reserved. */
//
// Windows implementations of the PitchPlugin interfaces.
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

#ifndef WIN_PLUGIN_H
#define WIN_PLUGIN_H

#include "Plugin.h"

/****************************************************************************
 *                                                                          *
 *                              SMB PITCH PLUGIN                            *
 *                                                                          *
 ****************************************************************************/

#define M_PI 3.14159265358979323846
#define SMB_MAX_FRAME_LENGTH 8192

/**
 * Encapsulates the "SMB" algorithm by Stephan M. Bernsee.
 * See copyright notices at the top of this file.
 * 
 * The code has been modified somewhat to eliminate the static buffers
 * so that multiple instances of the plugin may be used at the same time.
 * Note that the original code only processes one channel.  So we can process
 * in interupt blocks we have to create two instances of a single channel shifter
 * so the state for each channel is maintained properly.
 *
 * From his documentation:
 *
 * The algorithm takes a pitchShift factor value which is between 0.5
 * (one octave down) and 2. (one octave up). A value of exactly 1 does 
 * not change the pitch. 
 *
 * numSampsToProcess tells the routine how many samples in
 * indata[0...numSampsToProcess-1] should be pitch shifted and moved to
 * outdata[0...numSampsToProcess-1]. The two buffers can be identical
 * (ie. it can process the data in-place). 
 *
 * fftFrameSize defines the FFT frame size used for the processing. 
 * Typical values are 1024, 2048 and 4096. It may be any value 
 * <= MAX_FFT_FRAME_LENGTH (8192) but it MUST be a power of 2. 
 *
 * osamp is the STFT oversampling factor which also determines the overlap
 * between adjacent STFT frames. It should at least be 4 for moderate
 * scaling ratios.   A value of 32 is recommended for best quality. 
 *
 * sampleRate takes the sample rate for the signal in unit Hz, 
 * ie. 44100 for 44.1 kHz audio. 
 *
 * The data passed to the routine in indata[] should be in the range
 * [-1.0, 1.0), which is also the output range for the data, make sure
 * you scale the data accordingly (for 16bit signed integers you would
 * have to divide (and multiply) by 32768). 
 *
 */
class SmbChannel {

  public:

    SmbChannel();
    ~SmbChannel();

	void process(float pitchShift, long numSampsToProcess, 
				 long fftFrameSize, long osamp, float sampleRate, 
				 float *indata, float *outdata);

  private:

	void smbFft(float *fftBuffer, long fftFrameSize, long sign);


    //
    // In the original code these were static, here they have been moved
    // to member variables so we can run multiple instances of the plugin
    //

	float gInFIFO[SMB_MAX_FRAME_LENGTH];
	float gOutFIFO[SMB_MAX_FRAME_LENGTH];
	float gFFTworksp[2*SMB_MAX_FRAME_LENGTH];
	float gLastPhase[SMB_MAX_FRAME_LENGTH/2+1];
	float gSumPhase[SMB_MAX_FRAME_LENGTH/2+1];
	float gOutputAccum[2*SMB_MAX_FRAME_LENGTH];
	float gAnaFreq[SMB_MAX_FRAME_LENGTH];
	float gAnaMagn[SMB_MAX_FRAME_LENGTH];
	float gSynFreq[SMB_MAX_FRAME_LENGTH];
	float gSynMagn[SMB_MAX_FRAME_LENGTH];
	long gRover;

};

class SmbPitchPlugin : public PitchPlugin {

  public:

    SmbPitchPlugin();
    ~SmbPitchPlugin();

    void setFFTFrameSize(int size);
    void setOversamplingFactor(int factor);

	long process(float* input, float* output, long frames);
	void process(class WaveFile* file, int semitones);

  protected:

    void updatePitch();

  private:

	SmbChannel* mLeftChannel;
	SmbChannel* mRightChannel;

    /**
     * Defines the FFT frame size.  Typical values are 1024, 2048, 
     * and 4096.  It may be any value <= 8192 but it must be a power
     * of two.
     * !! See if we can abstract this into an integer power.
     */
    int mFFTFrameSize;

    /**
     * The STFT oversampling factor.  It should be at least 4 for
     * moderate scaling ratios.  32 is recommended for best quality.
     */
    int mOversamplingFactor;

    //
    // The original code wants the left and right channels to 
    // be in different buffers rather than being interleaved.
    // The code could be modified without too much difficulty to
    // support interleaved buffers, but I'm trying not to disrupt
    // it too much right now.
    //
    
    float mLeftIn[MAX_HOST_BUFFER_FRAMES];    
    float mRightIn[MAX_HOST_BUFFER_FRAMES];
    float mLeftOut[MAX_HOST_BUFFER_FRAMES];    
    float mRightOut[MAX_HOST_BUFFER_FRAMES];

};

/****************************************************************************
 *                                                                          *
 *                                SOUND TOUCH                               *
 *                                                                          *
 ****************************************************************************/

/**
 * This implements PitchPlugin, but it also has methods for
 * time stretch and rate change.  If we ever have more than
 * one of these factor out interfaces for time/rate plugins.
 */
class SoundTouchPlugin : public PitchPlugin {

  public:

    SoundTouchPlugin();
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

/****************************************************************************
 *                                                                          *
 *   								DIRAC                                   *
 *                                                                          *
 ****************************************************************************/

/**
 * Maximum DIRAC stream buffer size in samples.
 */
#define DIRAC_MAX_BUFFER 1024 * 10

class DiracChannel {

  public:

    DiracChannel(int channel);
    ~DiracChannel();

	void reset();
    void setPitch(float ratio);
    void setTempo(float tempo);
    int getLatency();

    // Playback rate
	// should be able to implement this as a combination
	// of stretch and pitch if we need to
    //void setRate(float rate);

	void add(float* input, long frames, int channels);
	void process(float* output, long frames);
	long callback(float** outputs, long frames);

  private:
	
	// channel processed by this object in an interleaved frame buffer
	int mChannel;

	// internal device
    void* mDirac;

	// stream buffer
	float mBuffer[DIRAC_MAX_BUFFER];

	// first available sample in mBuffer
	float* mHead;

	// last available sample
	float* mTail;

	// number of buffered samples
	long mAvailable;

	// initial request gap, should stay constant
	int mLatency;

};

class DiracPlugin : public PitchPlugin {

  public:

    DiracPlugin();
    ~DiracPlugin();

	void reset();
    void setTempo(float tempo);
    
    // Playback rate
	// should be able to implement this as a combination
	// of stretch and pitch if we need to
    //void setRate(float rate);

    int getLatency();

	long process(float* input, float* output, long frames);

	// temporary test hack
	Audio* process(WaveFile* file, int semitones);

  protected:

	void updatePitch();

  private:
	
	// todo: if we go multi-channel will need an array
    DiracChannel* mLeft;
    DiracChannel* mRight;

	// Have to deinterleave channels like SMB
    float mLeftIn[MAX_HOST_BUFFER_FRAMES];    
    float mRightIn[MAX_HOST_BUFFER_FRAMES];
    float mLeftOut[MAX_HOST_BUFFER_FRAMES];    
    float mRightOut[MAX_HOST_BUFFER_FRAMES];

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
