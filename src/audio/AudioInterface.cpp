/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * AN abstract interface for audio devices and services.
 *
 */

#include <stdio.h>
#include <memory.h>
#include <util.h>
#include <string.h>
#include <math.h>

#include "AudioInterface.h"

// OSX has AudioDevice so have to use namespaces
AUDIO_BEGIN_NAMESPACE

/****************************************************************************
 *                                                                          *
 *   								DEVICE                                  *
 *                                                                          *
 ****************************************************************************/

AudioDevice::AudioDevice() {
	mApi = API_MME;
	mId = 0;
	mName = NULL;
	mInputChannels = 2;
	mOutputChannels = 2;
	mDefaultInput = false;
	mDefaultOutput = false;
	mDefaultSampleRate = 0.0f;
}

void AudioDevice::setName(const char *name) {

	delete mName;
	mName = CopyString(name);
}

AudioDevice::~AudioDevice() 
{
	delete mName;
}


/****************************************************************************
 *                                                                          *
 *   							 PORT BUFFERS                               *
 *                                                                          *
 ****************************************************************************/

AudioPort::AudioPort()
{
    mNumber = 0;
    mChannels = 2;
    mFrameOffset = 0;
	mPrepared = false;
	mBuffer = new float[AUDIO_MAX_SAMPLES_PER_BUFFER];
}

AudioPort::~AudioPort()
{
	delete mBuffer;
}

void AudioPort::setNumber(int i)
{
    mNumber = i;
}

int AudioPort::getNumber()
{
    return mNumber;
}

void AudioPort::setChannels(int i)
{
	mChannels = i;
}

int AudioPort::getChannels()
{
    return mChannels;
}

void AudioPort::setFrameOffset(int i)
{
	mFrameOffset = i;
}

void AudioPort::reset()
{
	mPrepared = false;
}

/**
 * Extract the left and right channels for one audio port from the
 * combined buffer given on each interrupt.
 *
 * A 4 channel PortAudio interrupt buffers look like this:
 *
 *   ch1,ch2,ch3,ch4|ch1,ch2,ch3,ch4
 *
 * We logically group channel pairs into ports for:
 *
 *   p1l,p1r,p2l,p2r|p1l,p1r,p2l,p2r
 *
 */
float* AudioPort::extract(float* src, long frames, int channels)
{
	if (!mPrepared) {

		float* dest = mBuffer;

        // the last port on a device may have only one if this
        // is a mono device, duplicate the 
        bool mono = ((mFrameOffset + 1) == channels);

		for (int i = 0 ; i < frames ; i++) {

			float* portSrc = src + mFrameOffset;
            float sample = *portSrc++;

            *dest++ = sample;
            if (mono)
              *dest++ = sample;
            else
              *dest++ = *portSrc++;

			src += channels;
		}

		mPrepared = true;
	}

	return mBuffer;
}

/**
 * Prepare an output buffer.
 */
float* AudioPort::prepare(long frames)
{
	if (!mPrepared) {
		int samples = frames * mChannels;
		memset(mBuffer, 0, (sizeof(float) * samples));
		mPrepared = true;
	}
	return mBuffer;
}

/**
 * Copy the contents of one port's output into the multi-port 
 * interrupt buffer.
 */
void AudioPort::transfer(float* dest, long frames, int channels)
{
	if (!mPrepared) {
		// assume the target buffer was already initialized to zero
	}
	else {
		float* src = mBuffer;

        // shouldn't see mono ports on output but support
        // I guess we could be summing these but if we had a mono
        // that was split by extract() summing would end up doubling
        // the input
        bool mono = ((mFrameOffset + 1) == channels);

		for (int i = 0 ; i < frames ; i++) {

			float* portDest = dest + mFrameOffset;

            *portDest++ = *src++;
            if (!mono)
              *portDest++ = *src++;

			dest += channels;
		}
	}
}

/****************************************************************************
 *                                                                          *
 *   								STREAM                                  *
 *                                                                          *
 ****************************************************************************/

AbstractAudioStream::AbstractAudioStream()
{
	mHandler = NULL;
	mStream = NULL;
	mInputDevice = -1;
	mOutputDevice = -1;
    mInputChannels = 0;
    mOutputChannels = 0;
	mSampleRate = CD_SAMPLE_RATE;
	mSuggestedLatency = 0;
	mInputLatency = 0;
	mOutputLatency = 0;
	mStreamStarted = false;
	mInputUnderflows = 0;
	mInputOverflows = 0;
	mOutputUnderflows = 0;
	mOutputOverflows = 0;
	mInterrupts = 0;
	mAverageLatency = 0.0;
	mInput = NULL;
	mOutput = NULL;
	mFrames = 0;
	strcpy(mError, "");

	// these are annoying during debugging
	// !! need a dynamic way to set this from Mobius scripts
	mTraceDropouts = false;

	// get one of these so we can monitor interrupt timing
	// but *never* free this, that will happen elsewhere
	//MidiEnv* env = MidiEnv::getEnv();
	//mTimer = env->getTimer();
	//mLastMilli = 0;

    // until we have a way to specify arbitrary channel clusters
    // for ports, asssume they will always have two channels

	for (int i = 0 ; i < AUDIO_MAX_PORTS ; i++) {
        mInputs[i].setNumber(i);
        mInputs[i].setChannels(2);
        mInputs[i].setFrameOffset(i * 2);

        mOutputs[i].setNumber(i);
		mOutputs[i].setChannels(2);
		mOutputs[i].setFrameOffset(i * 2);
	}
    mInputPorts = 0;
    mOutputPorts = 0;
}

void AbstractAudioStream::setInterface(AudioInterface* ai)
{
	mInterface = ai;
}

AbstractAudioStream::~AbstractAudioStream()
{
}

void AbstractAudioStream::printStatistics()
{
	printf("%ld interrupts %ld input underflows %ld input overflows"
		   " %ld output underflows %ld output overflows\n",
		   mInterrupts,
		   mInputUnderflows, mInputOverflows,
		   mOutputUnderflows, mOutputOverflows);

}

AudioInterface* AbstractAudioStream::getInterface()
{
	return mInterface;
}

bool AbstractAudioStream::setInputDevice(const char* name)
{
    bool valid = false;
	if (name != NULL) {
		AudioDevice* dev = mInterface->getDevice(name, false);
		if (dev != NULL && dev->isInput())
		  valid = setInputDevice(dev->getId());
    }
    return valid;
}

bool AbstractAudioStream::setInputDevice(int id)
{
    if (mInputDevice != id) {
        close();
        mInputDevice = -1;
		AudioDevice* dev = mInterface->getDevice(id);
        if (dev != NULL && dev->isInput()) {
			mInputDevice = id;
			setInputChannels(dev->getInputChannels());
			// ASIO requires they be the same, CoreAudio can be different
			if (dev->getApi() == API_ASIO) {
				// must select the corresponding output device too
				mOutputDevice = id;
				setOutputChannels(dev->getOutputChannels());
			}
		}
    }
    return (mInputDevice == id);
}

/**
 * For now derive the port count by grouping them into stereo
 * pairs of channels.  Need more flexibility.
 */
void AbstractAudioStream::setInputChannels(int channels)
{
    mInputChannels = channels;
    // allow mono devices
    mInputPorts = (int)ceil((float)mInputChannels / 2.0);
    if (mInputPorts > AUDIO_MAX_PORTS)
      mInputPorts = AUDIO_MAX_PORTS;
}

int AbstractAudioStream::getInputChannels()
{
    return mInputChannels;
}

int AbstractAudioStream::getInputPorts()
{
    return mInputPorts;
}

bool AbstractAudioStream::setOutputDevice(const char* name)
{
    bool valid = false;
	if (name != NULL) {
		AudioDevice* dev = mInterface->getDevice(name, true);
		if (dev != NULL && dev->isOutput())
		  valid = setOutputDevice(dev->getId());
    }
    return valid;
}

bool AbstractAudioStream::setOutputDevice(int id)
{
    if (mOutputDevice != id) {
        close();
        mOutputDevice = -1;
		AudioDevice* dev = mInterface->getDevice(id);
        if (dev != NULL && dev->isOutput()) {
			mOutputDevice = id;
			setOutputChannels(dev->getOutputChannels());
			if (dev->getApi() == API_ASIO) {
				// must select the corresponding input device too
				mInputDevice = id;
				setInputChannels(dev->getInputChannels());
			}
		}
    }
    return (mOutputDevice == id);
}

/**
 * For now derive the port count by grouping them into stereo
 * pairs of channels.  Need more flexibility.
 */
PRIVATE void AbstractAudioStream::setOutputChannels(int channels)
{
    mOutputChannels = channels;
    // do not allow mono devices for now...
    //mOutputPorts = (int)ceil(mOutputChannels / 2);
    mOutputPorts = mOutputChannels / 2;
    if (mOutputPorts > AUDIO_MAX_PORTS)
      mOutputPorts = AUDIO_MAX_PORTS;

}

int AbstractAudioStream::getOutputChannels()
{
    return mOutputChannels;
}

int AbstractAudioStream::getOutputPorts()
{
    return mOutputPorts;
}

/**
 * If the stream is already opened, it is closed.
 */
void AbstractAudioStream::setSuggestedLatencyMsec(int i)
{
	if (mSuggestedLatency != i) {
		close();
		mSuggestedLatency = i;
	}
}

AudioDevice* AbstractAudioStream::getInputDevice() 
{
	return mInterface->getDevice(mInputDevice);
}

AudioDevice* AbstractAudioStream::getOutputDevice() 
{
	return mInterface->getDevice(mOutputDevice);
}

void AbstractAudioStream::setSampleRate(int rate)
{
	close();
	mSampleRate = rate;
}

int AbstractAudioStream::getSampleRate() 
{
	return mSampleRate;
}

void AbstractAudioStream::setHandler(AudioHandler* h)
{
	mHandler = h;
}

AudioHandler* AbstractAudioStream::getHandler()
{
	return mHandler;
}

const char* AbstractAudioStream::getLastError()
{
	return mError;
}

/**
 * Given a latency estimate as a fraction of seconds, calculate the
 * latency in number of frames.
 */
PRIVATE int AbstractAudioStream::calcLatency(double ltime)
{
    return (int)((double)mSampleRate * ltime);
}

PUBLIC int AbstractAudioStream::getInputLatencyFrames()
{
    return mInputLatency;
}

void AbstractAudioStream::setInputLatencyFrames(int frames)
{
    mInputLatency = frames;
}

PUBLIC int AbstractAudioStream::getOutputLatencyFrames()
{
    return mOutputLatency;
}

void AbstractAudioStream::setOutputLatencyFrames(int frames)
{
    mOutputLatency = frames;
}

/****************************************************************************
 *                                                                          *
 *   							  INTERFACE                                 *
 *                                                                          *
 ****************************************************************************/

AbstractAudioInterface::AbstractAudioInterface()
{
	mDevices = NULL;
	mDeviceCount = 0;
}

AbstractAudioInterface::~AbstractAudioInterface()
{
	// terminate?
	if (mDevices != NULL) {
		for (int i = 0 ; i < mDeviceCount ; i++)
		  delete mDevices[i];
		delete mDevices;
	}
}

void AbstractAudioInterface::printDevices()
{
	// Windows calls PA directly, should just do this?
	getDevices();

	if (mDevices == NULL) {
		printf("No audio devices detected!\n");
	}
	else {
		printf("%d audio devices.\n", mDeviceCount);

		for (int i = 0 ; i < mDeviceCount ; i++) {
			AudioDevice* d = mDevices[i];
			printf("----------------------------------------------\n");
			printf("Device %d name '%s' api %s", d->getId(), d->getName(), 
				   d->getApiName());

			if (d->isDefaultInput())
			  printf(" (default input)");
			if (d->isDefaultOutput())
			  printf(" (default output)");
			printf("\n");

			printf("Input channels %d, Output channels %d\n", 
				   d->getInputChannels(), d->getOutputChannels());
		}
	}
}

AudioDevice* AbstractAudioInterface::getDevice(int id)
{
	AudioDevice* dev = NULL;

	getDevices();   // called side effect, initializes mDeviceCount
	if (id >= 0 && id < mDeviceCount)
	  dev = mDevices[id];
	return dev;
}

AudioDevice* AbstractAudioInterface::getDevice(const char* name, bool output)
{
	AudioDevice* found = NULL;

	if (name != NULL) {
        getDevices();   // initializes mDevices and mDeviceCount
        for (int i = 0 ; i < mDeviceCount ; i++) {
            AudioDevice* dev = mDevices[i];
			// subtlty: ASIO devices are both inputs and outputs
            if (!strcmp(name, dev->getName()) &&
				((output && dev->isOutput()) ||
				 (!output && dev->isInput()))) {
				found = dev;
				break;
			}
		}
	}
    return found;
}

AUDIO_END_NAMESPACE
/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
