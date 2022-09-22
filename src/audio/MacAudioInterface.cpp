/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Mac AudioInterface implemented on top of PortAudio 19.
 * Now that WinAudioInterface is on the same rev of PortAudio these
 * are almost identical but I want to keep them distinct so we acn
 * tinker with CoreAudio.
 *
 */

#include <stdio.h>
#include <memory.h>

#include "Trace.h"
#include "util.h"
#include "AudioInterface.h"
#include "MidiEnv.h"
#include "MidiTimer.h"

// conditional?
#include "portaudio.h"

#include "MacUtil.h"
#include <CoreServices/CoreServices.h>
#include <CoreAudio/CoreAudio.h>

/**
 * Turn on to enable a few trace messages.
 */
static bool LatencyTrace = false;

AUDIO_BEGIN_NAMESPACE

//////////////////////////////////////////////////////////////////////
//
// Classes
//
//////////////////////////////////////////////////////////////////////

class MacAudioInterface : public AbstractAudioInterface {
  public:

	MacAudioInterface();
	~MacAudioInterface();

	AudioDevice** getDevices();
	AudioStream* getStream();
	void terminate();

  private:

	void checkError(const char *function, int e);

	AudioDevice** getDevicesCore();
	AudioDevice** getDevicesPA();

	void getDeviceInfo(AudioDevice* dev);
	void getChannelInfo(AudioDevice* dev, bool input);

};

class MacAudioStream : public AbstractAudioStream {

  public:

	MacAudioStream(MacAudioInterface* ai);
	~MacAudioStream();

	bool open();
	void close();

    double getStreamTime();
    double getLastInterruptStreamTime();

	// AudioHandler callbacks

	AudioTime* getTime();
	long getInterruptFrames();
	void getInterruptBuffers(int inport, float** inbuf, 
							 int outport, float** outbuf);

	// Portaudio callback
	void processBuffers(float* input, float* output, long frames,
						const PaStreamCallbackTimeInfo* timeInfo,
						PaStreamCallbackFlags statusFlags);

    void checkStatusFlags(PaStreamCallbackFlags flags);

  private:

	void checkError(const char* function, int e);

	void start();
	void stop();

	// performance monitoring
	MidiTimer* mTimer;
	long mLastMilli;
    double mLastStreamTime;

};


//////////////////////////////////////////////////////////////////////
//
// Interface Factory
//
//////////////////////////////////////////////////////////////////////

/**
 * This is normally on, but may want to turn it off when debugging
 * so we can halt at the site of the exception.
 */
//PUBLIC bool AudioInterfaceCatchExceptions = true;

AudioInterface* AudioInterface::Interface = NULL;

AudioInterface* AudioInterface::getInterface()
{
	if (Interface == NULL) {
		Interface = new MacAudioInterface();
	}
	return Interface;
}

void AudioInterface::exit()
{
	if (Interface != NULL) {
		Interface->terminate();
		delete Interface;
		Interface = NULL;
	}
	// just to be sure
    Pa_Terminate();
}

/****************************************************************************
 *                                                                          *
 *   						 PORTAUDIO INTERRUPT                            *
 *                                                                          *
 ****************************************************************************/

/**
 * This is normally on, but may want to turn it off when debugging
 * so we can halt at the site of the exception.
 */
PUBLIC bool AudioInterfaceCatchExceptions = true;

/**
 * PortAudio interrupt handler, used for both playback and recording.
 * This is called from an interrupt handler so you must not make
 * any system calls.  printf seems to be ok, but do NOT allocate memory.
 * 
 * Returning 1 will terminate the audio stream.
 */
static int paCallback(const void *input, 
                      void *output,
                      unsigned long frames,
                      const PaStreamCallbackTimeInfo* timeInfo,
                      PaStreamCallbackFlags statusFlags,
                      void *userData)
{
	MacAudioStream* stream = (MacAudioStream*)userData;


	if (!AudioInterfaceCatchExceptions) {
		stream->processBuffers((float*)input, (float*)output, frames,
							   timeInfo, statusFlags);
	}
	else {

		static bool ignoreAfterException = true;
		static int exceptionsCaught = 0;

		try {
			if (exceptionsCaught == 0 || !ignoreAfterException)
			  stream->processBuffers((float*)input, (float*)output, frames,
									 timeInfo, statusFlags);
		}
		catch (...) {
			// just in case trace is hosed
			exceptionsCaught++;
			if (exceptionsCaught <= 100) {
				printf("Exception in audio interrupt!\n");
				fflush(stdout);
				Trace(1, "Caught exception in audio interrupt!\n");
			}
		}
	}

	return paContinue;
}

void MacAudioStream::processBuffers(float* input, float* output, long frames,
									const PaStreamCallbackTimeInfo* timeInfo,
									PaStreamCallbackFlags statusFlags)
{
	mInterrupts++;
	
	// at 44100 with a 256 buffer, 5.805 milliseconds per buffer
	// this seems to reliably come in at 6
	long start = mTimer->getMilliseconds();
	long delta = start - mLastMilli;
	// 5 and 6 are normal, 4 and 7 happens on occasion
	// I've never seen 8 or above normally, and 4 shouldn't be possible with
	// a 256 block size
	if (delta < 4 || delta > 8)
	  Trace(2, "%ld millis between interrupts\n", delta);
	mLastMilli = start;

    mLastStreamTime = timeInfo->currentTime;

    checkStatusFlags(statusFlags);

    // find a pattern and watch them...
    if (LatencyTrace && mInterrupts < 101) {
		
		// only the fraction appears interesting
		double outtime = timeInfo->outputBufferDacTime - 
			(long)timeInfo->outputBufferDacTime;

		if (mInterrupts == 1)
		  printf("paCallback initial output time %lf (%d frames)\n",
				 outtime, calcLatency(outtime));

        printf("paCallback %lf %lf %lf\n", 
               timeInfo->inputBufferAdcTime,
               timeInfo->currentTime,
               timeInfo->outputBufferDacTime);

		mAverageLatency += timeInfo->inputBufferAdcTime;
		if (mInterrupts == 100) {
			mAverageLatency /= 100.0;
			printf("Average input latency %lf (%d)\n", mAverageLatency,
				   calcLatency(mAverageLatency));
		}

        fflush(stdout);
    }

	if (mHandler != NULL) {
		mInput = input;
		mOutput = output;
		mFrames = frames;

		int i;
		for (i = 0 ; i < mInputPorts ; i++)
		  mInputs[i].reset();

		for (i = 0 ; i < mOutputPorts ; i++)
		  mOutputs[i].reset();

		// make sure the output buffer is initialized to zero
		// check PortAudio to see if we can avoid this
		if (output != NULL) {
			long samples = frames * mOutputChannels;
			float *ptr = mOutput;
			for (int i = 0 ; i < samples ; i++)
			  *ptr++ = 0;
		}

		// this will make calls to getInterruptBuffers
		mHandler->processAudioBuffers(this);

		// Now have to merge the output buffers that were filled
		// back into the combined output buffer.  If we only had one pair
        // of channels we gave direct access to the PA buffer so we don't
        // need to interleave
		if (output != NULL && mOutputChannels != 2) {
			for (int i = 0 ; i < mOutputPorts ; i++)
			  mOutputs[i].transfer(mOutput, frames, mOutputChannels);
		}
	}

	long end = mTimer->getMilliseconds();
	delta = end - start;
    if (delta > 4) {
		Trace(2, "%ld milliseconds to process audio interrupt!\n", delta);
	}

}

void MacAudioStream::checkStatusFlags(PaStreamCallbackFlags flags)
{
    // how often will we get these?

    if (flags & paInputUnderflow) {
		if (mTraceDropouts)
		  Trace(1, "Audio input underflow!\n");
		mInputUnderflows++;
    }
    
	// this seems to happen all the time?
    if (flags & paInputOverflow) {
		if (mTraceDropouts)
		  Trace(1, "Audio input overflow!\n");
		mInputOverflows++;
    }

	// this seems to happen all the time
    if (flags & paOutputUnderflow) {
		if (mTraceDropouts)
		  Trace(1, "Audio output underflow!\n");
		mOutputUnderflows++;
    }

    if (flags & paOutputOverflow) {
		if (mTraceDropouts)
		  Trace(1, "Audio output overflow!\n");
		mOutputOverflows++;
    }

    // should only see this if we set the paPrimeOutputBuffersUsingStreamCallback
    // flag when the stream was opened?
    if (flags & paPrimingOutput) {
    }

}

/**
 * Called by the handler for each set of ports it is interested in.
 */
long MacAudioStream::getInterruptFrames()
{
	return mFrames;
}

AudioTime* MacAudioStream::getTime()
{	
	return NULL;
}

void MacAudioStream::getInterruptBuffers(int inport, float** inbuf, 
										  int outport, float** outbuf)
{
	if (inbuf != NULL) {

		if (mInputChannels == 2) {
			// special case, direct passthrough of single port buffer
			*inbuf = mInput;
		}
		else {
			// have to deinterleave
			// if the port is out of range, use the first one
			// this sometimes happens if you swap audio devices
			if (inport < 0 || inport >= mInputPorts)
			  inport = 0;

			*inbuf = mInputs[inport].extract(mInput, mFrames, mInputChannels);
		}
	}

	if (outbuf != NULL) {

		if (mOutputChannels == 2) {
			// special case, direct passthrough of single port buffer
			*outbuf = mOutput;
		}
		else {
			if (outport < 0 || outport >= mOutputPorts)
			  outport = 0;

			*outbuf = mOutputs[outport].prepare(mFrames);
		}
	}
}

/****************************************************************************
 *                                                                          *
 *                                STREAM TIME                               *
 *                                                                          *
 ****************************************************************************/

PUBLIC double MacAudioStream::getStreamTime()
{
    return Pa_GetStreamTime((PaStream*)mStream);
}

PUBLIC double MacAudioStream::getLastInterruptStreamTime()
{
    return mLastStreamTime;
}

/****************************************************************************
 *                                                                          *
 *   								STREAM                                  *
 *                                                                          *
 ****************************************************************************/

MacAudioStream::MacAudioStream(MacAudioInterface* ai)
{
	setInterface(ai);

	// get one of these so we can monitor interrupt timing
	// but *never* free this, that will happen elsewhere
	MidiEnv* env = MidiEnv::getEnv();
	mTimer = env->getTimer();
	mLastMilli = 0;
    mLastStreamTime = 0.0;
}

MacAudioStream::~MacAudioStream()
{
	close();
}

/**
 * Open (and start) the PA stream.  Return false if we could not and
 * leave an error in mError.
 */
bool MacAudioStream::open()
{
	if (mStream == NULL) {
		strcpy(mError, "");

        // both devices must be specified
        if (mInputDevice == -1) {
            if (mOutputDevice == -1)
              sprintf(mError, "Unspecified audio input and output devices");
            else
              sprintf(mError, "Unspecified audio input device");
        }
        else if (mOutputDevice == -1) {
            sprintf(mError, "Unspecified audio output device");
        }
        else {
			AudioDevice* indev = mInterface->getDevice(mInputDevice);
			AudioDevice* outdev = mInterface->getDevice(mOutputDevice);

            // any interesting checks to do here?
            //const PaDeviceInfo *info = Pa_GetDeviceInfo(mOutputDevice);
            //printf("Opening input %s, output %s\n", 
			//getDeviceName(mInputDevice),
			//getDeviceName(mOutputDevice));

            PaStreamParameters input, output;

			// in theory input and output latency suggestions could be
			// different, but maybe not if you use an aggregating device?
			float latency;
			if (mSuggestedLatency > 0) {
				// convert number of milliseconds to seconds
				latency = (float)mSuggestedLatency / 1000.0f;
			}
			else if (outdev->getApi() == API_CORE_AUDIO)
			  latency = 0.001f;
			else 
			  latency = 0.2f;

            // formerly alloweed you to open a small number of
            // channels than the device supported, now we always open
            // the max and let you choose to use them with ports

            input.device = mInputDevice;
			input.suggestedLatency = latency;
            input.channelCount = mInputChannels;
            input.sampleFormat = paFloat32;
            input.hostApiSpecificStreamInfo = NULL;

            output.device = mOutputDevice;
			output.suggestedLatency = latency;
            output.channelCount = mOutputChannels;
            output.sampleFormat = paFloat32;
            output.hostApiSpecificStreamInfo = NULL;

            int error = (int)
                Pa_OpenStream(&mStream,
                              &input, 
                              &output,
                              mSampleRate,

                              // !! not optimal, need to support variable
                              // buffer sizes to reduce latency
							  // using "unspecified" locks the system
							  //paFramesPerBufferUnspecified,
                              AUDIO_FRAMES_PER_BUFFER,
							  
                              paClipOff, // stream flags
                              paCallback,
                              this);

            checkError("Pa_OpenStream", error);

            // save this for later display
            if (error != paNoError) {
                sprintf(mError, "%s: Input ID %d Output ID %d\n",
                        Pa_GetErrorText((PaError)error), 
                        mInputDevice,
                        mOutputDevice);
            }
            else {
                const PaStreamInfo* info = Pa_GetStreamInfo((PaStream*)mStream);
                mInputLatency = calcLatency(info->inputLatency);
                mOutputLatency = calcLatency(info->outputLatency);

				if (LatencyTrace) {
                    printf("PortAudio reported input latency %lf (%d frames), "
                           "output latency %lf (%d frames)\n",
                           info->inputLatency, mInputLatency,
                           info->outputLatency, mOutputLatency);
                    fflush(stdout);
                }

				// if we can't open the stream, should remember that somewhere
				// so we don't keep trying
				start();
            }
        }
    }

	return (mStream != NULL);
}

/**
 * Start the stream.
 * This is what causes the stream to start pumping bufferes
 * to our interrupt handler.  We'll normally keep this 
 * started all the time so we can monitor the input level while paused.
 */
PRIVATE void MacAudioStream::start()
{
    if (!mStreamStarted) {
        open();
        if (mStream != NULL) {
            int error = (int)Pa_StartStream((PaStream*)mStream);
            checkError("Pa_StartStream", error);
            if (!error)
              mStreamStarted = true;
        }
    }
}

/**
 * Stop the stream.
 * PortAudio stops calling the interrupt handler.
 * 
 * StopStream will let any queued buffers flush
 * AbortStream will not wait for queued buffers
 * Here, I think we want to finish the capture of input buffers.
 */
PRIVATE void MacAudioStream::stop()
{
    if (mStream != NULL) {
        PaStream* stream = (PaStream*)mStream;
        if (Pa_IsStreamActive(stream) == 1) {
            int error = (int)Pa_StopStream(stream);
            checkError("Pa_StopStream", error);
        }
    }
    mStreamStarted = false;
}

/**
 * Close the stream.
 */
void MacAudioStream::close()
{
	if (mStream != NULL) {
		int error = 0;
		PaStream* stream = (PaStream*)mStream;
		if (Pa_IsStreamActive(stream) == 1) {
			error = (int)Pa_AbortStream(stream);
			checkError("Pa_AbortStream", error);
		}
        error = (int)Pa_CloseStream(stream);
        checkError("Pa_CloseStream", error);
		mStream = NULL;
        mStreamStarted = false;

		mInterrupts = 0;
		mAverageLatency = 0;
		mInputUnderflows = 0;
		mInputOverflows = 0;
		mOutputUnderflows = 0;
		mOutputOverflows = 0;
	}
}

void MacAudioStream::checkError(const char *function, int e) 
{
	if (e != paNoError) {
		sprintf(mError, "PortAudio Error: %s: %s\n", 
				function, Pa_GetErrorText((PaError)e));

		printf("%s", mError);
		fflush(stdout);
	}
}

/****************************************************************************
 *                                                                          *
 *   							  INTERFACE                                 *
 *                                                                          *
 ****************************************************************************/

MacAudioInterface::MacAudioInterface()
{
	int error = (int)Pa_Initialize();
	checkError("Pa_Initialize", error);
}

MacAudioInterface::~MacAudioInterface()
{
	// terminate()?
}

AudioStream* MacAudioInterface::getStream()
{
	return new MacAudioStream(this);
}

void MacAudioInterface::terminate()
{
	// TODO: close all the outstanding streams?
    int error = (int)Pa_Terminate();
	checkError("Pa_Terminate", error);
}

void MacAudioInterface::checkError(const char *function, int e) 
{
	if (e != paNoError) {
		fprintf(stderr, "PortAudio Error: %s: %s\n", 
				function, Pa_GetErrorText((PaError)e));
		fflush(stderr);
	}
}

//////////////////////////////////////////////////////////////////////
//
// Device enumeration using PortAudio
//
//////////////////////////////////////////////////////////////////////

AudioDevice** MacAudioInterface::getDevices()
{
	//return getDevicesCore();
	return getDevicesPA();
}

AudioDevice** MacAudioInterface::getDevicesPA()
{
	if (mDevices == NULL) {

		mDeviceCount = Pa_GetDeviceCount();
		int defaultInput = Pa_GetDefaultInputDevice();
		int defaultOutput = Pa_GetDefaultOutputDevice();

		mDevices = new AudioDevice*[mDeviceCount + 1];
		mDevices[mDeviceCount] = NULL;

		for (int i = 0 ; i < mDeviceCount ; i++) {
			
			const PaDeviceInfo* info = Pa_GetDeviceInfo(i);
			const PaHostApiInfo* api = Pa_GetHostApiInfo(info->hostApi);
            if (api != NULL) {
                AudioApi apiType = API_UNKNOWN;
                const char* apiName = NULL;

                // only pay attention to APIs we recognize
				if (api->type == paCoreAudio) {
					apiType = API_CORE_AUDIO;
					apiName = "CoreAudio";
				}
				else {
					Trace(1, "MacAudioInterface: Unknown api type %ld\n", 
                          (long)api->type);
				}

				// Since most things are designed for 2 channel "ports",
				// should ignore devices that don't have at least 2 channels
                // like the "Modem #1 Line Record" device
                // Microsoft Sound Mapper advertises 0 channels,
                // not sure what that does.

                if (apiType != API_UNKNOWN) {
                    char fullname[256];
                    sprintf(fullname, "%s:%s", apiName, info->name);

                    AudioDevice* dev = new AudioDevice();
                    mDevices[i] = dev;

                    dev->setApi(apiType);
                    dev->setId(i);
                    dev->setName(fullname);
					dev->setDefaultInput(i == defaultInput);
					dev->setDefaultOutput(i == defaultOutput);

                    dev->setInputChannels(info->maxInputChannels);
					dev->setOutputChannels(info->maxOutputChannels);

                    // we allow mono inptus for headsets, but
                    // don't allow mono outputs until we can work
                    // out how to merge the port buffers
					int outchannels = info->maxOutputChannels;
					if (outchannels > 0) {
						int outports = outchannels / 2;
						if ((outports * 2) != outchannels) {
							// this is more likely for output channels?
							Trace(2, "Audio: Device with odd number of output channels: %s %ld\n",
								  fullname, (long)outchannels);
						}

						if (outports > AUDIO_MAX_PORTS)
						  outports = AUDIO_MAX_PORTS;

						outchannels = outports * 2;
					}
					dev->setOutputChannels(outchannels);
                }
            }
        }
    }

	return mDevices;
}

//////////////////////////////////////////////////////////////////////
//
// Device enumeration using CoreAudio
//
//////////////////////////////////////////////////////////////////////

PRIVATE AudioDevice** MacAudioInterface::getDevicesCore()
{
	if (mDevices == NULL) {
		UInt32 propSize;
		OSErr err = AudioHardwareGetPropertyInfo(kAudioHardwarePropertyDevices, &propSize, NULL);
		if (CheckErr(err, "kAudioHardwarePropertyDevices Info")) {

			mDeviceCount = propSize / sizeof(AudioDeviceID);
			AudioDeviceID devids[mDeviceCount];
			mDevices = new AudioDevice*[mDeviceCount + 1];
			int devIndex = 0;

			err = AudioHardwareGetProperty(kAudioHardwarePropertyDevices, &propSize, &devids);
			if (CheckErr(err, "AudioHardwareGetProperty")) {

				// Get the default in/out device ids, if this fails for 
				// some reason we'll pick the first available.
				AudioDeviceID defaultIn = kAudioDeviceUnknown;
				propSize = sizeof(defaultIn);
				err = AudioHardwareGetProperty(kAudioHardwarePropertyDefaultInputDevice, &propSize, &defaultIn);
				CheckErr(err, "kAudioHardwarePropertyDefaultInputDevice");

				AudioDeviceID defaultOut = kAudioDeviceUnknown;
				propSize = sizeof(defaultOut);
				err = AudioHardwareGetProperty(kAudioHardwarePropertyDefaultOutputDevice, &propSize, &defaultOut);
				CheckErr(err, "kAudioHardwarePropertyDefaultOutputDevice");

				//printf("Default in %d out %d\n", defaultIn, defaultOut);

				for (int i = 0 ; i < mDeviceCount ; i++) {
					AudioDeviceID id = devids[i];

					if (id == kAudioDeviceUnknown) {
						// some example code checks this, not sure why it
                        // would happen
                        Trace(1, "MacAudioInterface: Invalid device id!\n");
					}
					else {
						char buffer[2048];
						UInt32 maxlen = sizeof(buffer) - 4;
						// in examples channel is always zero and input always false
						err = AudioDeviceGetProperty(id, 0, false, kAudioDevicePropertyDeviceName, &maxlen, buffer);
						if (CheckErr(err, "kAudioDevicePropertyDeviceName")) {
							
							AudioDevice* dev = new AudioDevice();
							dev->setId(id);
							dev->setName(buffer);
							// need something
							dev->setApi(API_CORE_AUDIO);
							mDevices[devIndex++] = dev;

							getDeviceInfo(dev);

							if (defaultIn == kAudioDeviceUnknown && dev->getInputChannels() > 0)
							  defaultIn = id;

							if (defaultOut == kAudioDeviceUnknown && dev->getOutputChannels() > 0)
							  defaultOut = id;

							dev->setDefaultInput(id == defaultIn);
							dev->setDefaultOutput(id == defaultOut);
						}
					}
				}
			}

			mDevices[devIndex] = NULL;
			// in theory this may be lower if we found some DeviceUnknown ids
			mDeviceCount = devIndex;
		}
	}

	return mDevices;
}

/**
 * Gather various interesting things about a device.
 */
PRIVATE void MacAudioInterface::getDeviceInfo(AudioDevice* dev) 
{
	int id = dev->getId();

    // Default sample rate
    Float64 sampleRate;
    UInt32 propSize = sizeof(sampleRate);
    OSErr err = AudioDeviceGetProperty(id, 0, 0, kAudioDevicePropertyNominalSampleRate,
									   &propSize, &sampleRate);
	if (CheckErr(err, "kAudioDevicePropertyNominalSampleRate")) {
		dev->setDefaultSampleRate((float)sampleRate);
		printf("kAudioDevicePropertyNominalSampleRate %s %f\n",
			   dev->getName(), dev->getDefaultSampleRate());
        fflush(stdout);
	}

	getChannelInfo(dev, true);
	getChannelInfo(dev, false);
}

/**
 * Gather information about device channels.
 * This is basically ripped off of PortAudio, comments
 * about latency calculations are from there.
 */
PRIVATE void MacAudioInterface::getChannelInfo(AudioDevice* dev, bool input)
{
	int id = dev->getId();

	// PA would fail if we can't get the channel count, need to filter devices
	// with no channels?
    UInt32 propSize;
    OSErr err = AudioDeviceGetPropertyInfo(id, 0, input, kAudioDevicePropertyStreamConfiguration,
									 &propSize, NULL);
	if (CheckErr(err, "kAudioDevicePropertyStreamConfiguration Info")) {

		AudioBufferList* buflist = (AudioBufferList*)malloc(propSize);
		err = AudioDeviceGetProperty(id, 0, input, kAudioDevicePropertyStreamConfiguration, 
									 &propSize, buflist);
		if (CheckErr(err, "kAudioDevicePropertyStreamConfiguration")) {

			// don't understand the concept of buffers vs channels
			int buffers = 0;
			int channels = 0;
			for (int i = 0 ; i < buflist->mNumberBuffers; i++) {
				buffers++;
				channels += buflist->mBuffers[i].mNumberChannels;
			}

			//printf("%d channels in %d buffers\n", channels, buffers);

			if (input)
			  dev->setInputChannels(channels);
			else
			  dev->setOutputChannels(channels);

			if (channels > 0) {
				float lowLatency = .01;
				float highLatency = .10;
				UInt32 frameLatency;
				propSize = sizeof(UInt32);
				err = AudioDeviceGetProperty(id, 0, input, kAudioDevicePropertyLatency, 
											 &propSize, &frameLatency);
				// should be a warning?
				if (CheckErr(err, "kAudioDevicePropertyLatency")) {
					// comments from PortAudio
					/** FEEDBACK:
					 * This code was arrived at by trial and error, and some extentive, but 
					 * not exhaustive testing. Sebastien Beaulieu <seb@plogue.com> has suggested using
					 * kAudioDevicePropertyLatency + kAudioDevicePropertySafetyOffset + buffer 
					 * size instead. At the time this code was written, many users were 
					 * reporting dropouts with audio programs that probably used this formula. 
					 * This was probably around 10.4.4, and the problem is probably fixed now. 
					 * So perhaps his formula should be reviewed and used.
					 * */
					float sampleRate = dev->getDefaultSampleRate();
					if (sampleRate > 0.0f) {
						double secondLatency = frameLatency / sampleRate;
						lowLatency = 3 * secondLatency;
						highLatency = 3 * 10 * secondLatency;
					}
					/*
					if (input) {
						dev->setLowInputLatency(lowLatency);
						dev->setHighInputLatency(highLatency);
					}
					else {
						dev->setLowOutputLatency(lowLatency);
						dev->setHighOutputLatency(highLatency);
					}
					*/
				}
			}
		}
		free(buflist);
    }
}

AUDIO_END_NAMESPACE
/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
