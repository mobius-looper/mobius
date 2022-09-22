/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Windows AudioInterface implemented on top of PortAudio 19.
 * Now that both Mac and Win use the same rev of PortAudio these
 * are almost identical but I want to keep them distinct so we acn
 * tinker with CoreAudio.
 *
 * v19 things of interest:
 * 
 *  PaHostApiIndex Pa_HostApiTypeIdToHostApiIndex( PaHostApiTypeId type );
 *   - Maps one of the api type constants (MME, ASIO, etc.) to the api index
 *     for this particular machine.  Can also derive this by iterating
 *     for the PaHostApiInfos.
 * 
 *  paPrimeOutputBuffersUsingStreamCallback ((PaStreamFlags) 0x00000008)
 *   - allows the initial set of output buffers to be primed by the application
 *     rather than filled with zeros
 *  
 *  PaStreamCallbackTimeInfo
 *  
 *  Contains the time in seconds when the first sample of the input buffer was
 *  received at the audio input (inputBufferAdcTime), the time in seconds when
 *  the first sample of the output buffer will begin being played at the audio
 *  output (outputBufferDacTime), and the time in seconds when the stream
 *  callback was called (currentTime).
 * 
 *  Pa_GetStreamTime
 *  
 *  Looks like it corresponds to the currentTime field of 
 *  the PaStreamCallbackTimeInfo struct passed to the interrupt.
 * 
 *  Times are all double floats which is how you get milliseconds.
 * 
 *  Pa_GetStreamInfo returns PaStreamInfo
 * 
 *  Contains actual inputLatency, outputLatency, and sampleRate.
 *  These are supposed to be the most accurate estimates and may differ
 *  significantly from the suggestedLatency when the stream was open.
 *  
 *  
 */

#include <stdio.h>
#include <string.h>
#include <memory.h>

#include "portaudio.h"

#include "Trace.h"
#include "util.h"
#include "AudioInterface.h"

#include "MidiEnv.h"
#include "MidiTimer.h"


/**
 * Emit a warning message if too many milliseconds go by between
 * calls to processBuffers.
 */
static bool TraceInterruptDelays = false;

/**
 * Turn on to enable a few trace messages.
 */
static bool LatencyTrace = true;

/**
 * Debugging flag to disable catching exceptions in the PA callback.
 * Allows MSVC to halt at the site of the error rather than back
 * in the catch tag.
 */
bool WinAudioCatchCallbackExceptions = true;

/****************************************************************************
 *                                                                          *
 *   						  PORTAUDIO CLASSES                             *
 *                                                                          *
 ****************************************************************************/

class PortaudioInterface : public AbstractAudioInterface {
  public:

	PortaudioInterface();
	~PortaudioInterface();
	
	AudioDevice** getDevices();
	AudioStream* getStream();
	void terminate();

	void printDevices();

  private:

	void checkError(const char *function, int e);
    void printHostApis();
};

class PortaudioStream : public AbstractAudioStream {

  public:

	PortaudioStream(PortaudioInterface* ai);
	~PortaudioStream();

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

	void checkError(const char* function, int code);
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
		Interface = new PortaudioInterface();
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
	PortaudioStream* stream = (PortaudioStream*)userData;


	if (!AudioInterfaceCatchExceptions) {
		stream->processBuffers((float*)input, (float*)output, frames,
							   timeInfo, statusFlags);
	}
	else {

		static bool ignoreAfterException = true;
		static int exceptionsCaught = 0;

        if (WinAudioCatchCallbackExceptions) {
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
        else {
            stream->processBuffers((float*)input, (float*)output, frames,
                                   timeInfo, statusFlags);
        }
	}

	return paContinue;
}

void PortaudioStream::processBuffers(float* input, float* output, long frames,
                                     const PaStreamCallbackTimeInfo* timeInfo,
                                     PaStreamCallbackFlags statusFlags)
{
	mInterrupts++;
	
	// at 44100 with a 256 buffer, 5.805 milliseconds per buffer
	// this seems to reliably come in at 6
	long start = mTimer->getMilliseconds();
	long delta = start - mLastMilli;
	// 5 and 6 are normal, 4 and 7 happens on occasion
	// I've never seen 8 or above normally.  
    // 4 shouldn't be possible with a 256 block size but you often 
    // see zero or 1 after a particularly large delay, presumably because
    // the next block is queued and ready.  Reduce some trace clutter by
    // only complaining about the overflows
	//if (delta < 4 || delta > 8)
	if (TraceInterruptDelays && delta > 8)
	  trace("%ld millis between interrupts\n", delta);
	mLastMilli = start;

    mLastStreamTime = timeInfo->currentTime;

    checkStatusFlags(statusFlags);

    // find a pattern and watch them...
    if (LatencyTrace && mInterrupts < 101) {
		
		// only the fraction appears interesting
		double outtime = timeInfo->outputBufferDacTime - 
			(long)timeInfo->outputBufferDacTime;

        char buffer[128];
		if (mInterrupts == 1) {
            sprintf(buffer, "paCallback initial output time %lf (%d frames)\n",
                    outtime, calcLatency(outtime));
            trace(buffer);
        }

        sprintf(buffer, "paCallback %lf %lf %lf %lf %ld\n", 
                timeInfo->inputBufferAdcTime,
                timeInfo->currentTime,
                timeInfo->outputBufferDacTime,
                getStreamTime(),
                mTimer->getMilliseconds()
            );
        trace(buffer);

		mAverageLatency += timeInfo->inputBufferAdcTime;
		if (mInterrupts == 100) {
			mAverageLatency /= 100.0;
			sprintf(buffer, "Average input latency %lf (%d)\n", mAverageLatency,
                    calcLatency(mAverageLatency));
            trace(buffer);
		}
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
    if (TraceInterruptDelays && delta > 4) {
		Trace(2, "%ld milliseconds to process audio interrupt!\n", delta);
	}

}

void PortaudioStream::checkStatusFlags(PaStreamCallbackFlags flags)
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
long PortaudioStream::getInterruptFrames()
{
	return mFrames;
}

AudioTime* PortaudioStream::getTime()
{	
	return NULL;
}

void PortaudioStream::getInterruptBuffers(int inport, float** inbuf, 
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

PUBLIC double PortaudioStream::getStreamTime()
{
    return Pa_GetStreamTime((PaStream*)mStream);
}

PUBLIC double PortaudioStream::getLastInterruptStreamTime()
{
    return mLastStreamTime;
}

/****************************************************************************
 *                                                                          *
 *   								STREAM                                  *
 *                                                                          *
 ****************************************************************************/

PortaudioStream::PortaudioStream(PortaudioInterface* ai)
{
    setInterface(ai);

	// get one of these so we can monitor interrupt timing
	// but *never* free this, that will happen elsewhere
	MidiEnv* env = MidiEnv::getEnv();
	mTimer = env->getTimer();
	mLastMilli = 0;
    mLastStreamTime = 0.0;
}

PortaudioStream::~PortaudioStream()
{
	close();
}

/**
 * Open (and start) the PA stream.  Return false if we could not and
 * leave an error in mError.
 */
bool PortaudioStream::open()
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
			// different, but you can't have an MME/ASIO combo, and
			// MME/DS are similar.  REALLY?
			float latency;
			if (mSuggestedLatency > 0) {
				// convert number of milliseconds to seconds
				latency = (float)mSuggestedLatency / 1000.0f;
			}
			else if (outdev->getApi() == API_ASIO)
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

                              // This is the size of the buffers we will
                              // receive in our callback, it is NOT NECESSARILY
                              // the size of the device buffer which must
                              // be set using suggested latency.
                              // We're supposed to use Unspecified, but old
                              // comments indiciated that was causing a hang
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

                // Latency is often higher than what we asked for and you
                // can't get directly to the host buffer size.  You can usuall
                // just round down though.  

				if (LatencyTrace) {
                    char buffer[256];
                    sprintf(buffer, "PortAudio reports input latency %lf output latency %lf\n",
                            info->inputLatency, info->outputLatency);
                    trace(buffer);

                    sprintf(buffer, "Converted latency frames input %d output %d\n",
                            mInputLatency,mOutputLatency);
                    trace(buffer);
                }


                // this is an extension I made to an older rev of v19 to 
                // return the framesPerHostBuffer actually chosen after opening
/* 
				if (indev->getApi() == API_MME) {
					// Actual input latency is usually much lower than
					// what PA reports, should be close to one host buffer?
					// Extended PA to provide this.
					// Exactly one buffer doesn't sound right, half of
					// one sounds better.

					mInputLatency = info->framesPerHostBuffer;
				}
				else if (indev->getApi() == API_DIRECT_SOUND) {
					// reported input latency is always zero, a bug?
					// DS interface uses the PA buffer size as the DS 
					// host buffer size.  Reported output latency is accurate.
					mInputLatency = AUDIO_FRAMES_PER_BUFFER;
				}
*/

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
PRIVATE void PortaudioStream::start()
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
PRIVATE void PortaudioStream::stop()
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
void PortaudioStream::close()
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

void PortaudioStream::checkError(const char *function, int e) 
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

PortaudioInterface::PortaudioInterface()
{
	int error = (int)Pa_Initialize();
	checkError("Pa_Initialize", error);
}

PortaudioInterface::~PortaudioInterface()
{
	// terminate()?
}

AudioStream* PortaudioInterface::getStream()
{
	return new PortaudioStream(this);
}

void PortaudioInterface::terminate()
{
	// TODO: close all the outstanding streams?
    int error = (int)Pa_Terminate();
	checkError("Pa_Terminate", error);
}

void PortaudioInterface::checkError(const char *function, int e) 
{
	if (e != paNoError) {
		fprintf(stderr, "PortAudio Error: %s: %s\n", 
				function, Pa_GetErrorText((PaError)e));
		fflush(stderr);
	}
}

//////////////////////////////////////////////////////////////////////
//
// Device enumeration
//
//////////////////////////////////////////////////////////////////////

/**
 * So we can easily get to any host api's devices by
 * name, the names we represent here will always be
 * qualified of the form "api:device" example:
 *
 *    DS:SoundMAX Digital Audio
 */
AudioDevice** PortaudioInterface::getDevices()
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
                if (api->type == paMME) {
                    apiType = API_MME;
                    apiName = "MME";
                }
                else if (api->type == paDirectSound) {
                    apiType = API_DIRECT_SOUND;
                    apiName = "DS";
                }
                else if (api->type == paASIO) {
                    apiType = API_ASIO;
                    apiName = "ASIO";
                }
				else {
                    Trace(1, "PortAudioInterface: Unknown api type %ld\n", 
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

/**
 * We overload the method from AbstractAudioInterface to provide
 * more information that PA gives us.
 */
void PortaudioInterface::printDevices()
{
    printHostApis();

	int count = Pa_GetDeviceCount();
	if (count < 0)
	  printf("No audio devices detected!\n");
	else {
		printf("%d audio devices.\n", count);
		int defaultInput = Pa_GetDefaultInputDevice();
		int defaultOutput = Pa_GetDefaultOutputDevice();

		for (int i = 0 ; i < count ; i++) {

			const PaDeviceInfo* info = Pa_GetDeviceInfo(i);

			printf("----------------------------------------------\n");
			printf("Device %d api %d '%s'", i, info->hostApi, info->name);
			if (i == defaultInput)
			  printf(" (default input)");
			if (i == defaultOutput)
			  printf(" (default output)");
			printf("\n");

			printf("Max inputs %d, Max outputs %d, Default sample rate %lf\n",
				   info->maxInputChannels, info->maxOutputChannels,
                   info->defaultSampleRate);

            printf("low input latency %lf, low output latency %lf\n",
                   info->defaultLowInputLatency, 
                   info->defaultLowOutputLatency);

            printf("high input latency %lf, high output latency %lf\n",
                   info->defaultHighInputLatency, 
                   info->defaultHighOutputLatency);
		}
	}
}

void PortaudioInterface::printHostApis()
{
	int count = Pa_GetHostApiCount();
	if (count <= 0)
	  printf("No audio host APIs detected!\n");
	else {
		printf("%d audio host APIs.\n", count);
		int defaultApi = Pa_GetDefaultHostApi();

		for (int i = 0 ; i < count ; i++) {

			const PaHostApiInfo* info = Pa_GetHostApiInfo(i);

			printf("----------------------------------------------\n");
			printf("API %d type %d name '%s'", i, info->type, info->name);
			if (i == defaultApi)
			  printf(" (default API)");
			printf("\n");

            printf("%d devices, default input %d, default output %d\n",
                   info->deviceCount, 
                   info->defaultInputDevice, 
                   info->defaultOutputDevice);
        }
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
