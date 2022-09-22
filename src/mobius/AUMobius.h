/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * A AudioUnit plugin that also implements the Mobius 
 * AudioInterface and AudioStream interfaces.
 *  
 * Mobius is designed around AudioInterface and AudioStream to 
 * encapsulate theh OS interface to audio hardware.  When running
 * as a plugin, we don't have direct hardware access, the host
 * provides a stream with a configurable number of channels.
 * 
 * Since the AudioInterface and AudioStream interfaces are largely
 * stubs here we'll put everything on AUMobius, which in turn
 * is an AUMIDIEffectBase to be an audio unit.
 *
 * TODO: 
 * THere is a "bypass" concept in AUEffectBase we need to support.
 *
 * 
 * TODO: 
 * Consider making this more flexible so there can be several 
 * AudioStreams allowing tracks to either be connected to the 
 * AU/VST host or to a device if our own, or to ReWire, etc.
 *
 * TODO: 
 * Need to think HARD about our "port" concept (interleaved stereo frames)
 * and make this more flexible.  The host interface should always
 * be non-interleaved, interleaving if required should be done in the
 * plugin or maybe AudioStream.
 *
 * Channels
 *
 */

#ifndef AU_MOBIUS
#define AU_MOBIUS

#include <AudioUnit/AudioUnit.h>
#include "AUMIDIEffectBase.h"
#include "AUCarbonViewBase.h"

#include "Context.h"
#include "AudioInterface.h"
#include "HostInterface.h"

//////////////////////////////////////////////////////////////////////
//
// Ports
//
//////////////////////////////////////////////////////////////////////

/**
 * Helper structure used to maintain processing state for
 * each "port" we expose through the stream.  These are created
 * on demand from the AU AudioBufferLists.  Each buffer will
 * contain interleaved frames for one port.  Normally a port
 * is a pair of stereo channels, eventually need to allow
 * more channels.
 *
 * NOTE: THis is identical to VstPort but there's not much to share.
 */
typedef struct {

	float* input;
	bool inputPrepared;

	float* output;
	bool outputPrepared;

} AudioStreamPort;

//////////////////////////////////////////////////////////////////////
//
// AUTimeInfo
//
//////////////////////////////////////////////////////////////////////

/**
 * Class to capture all the various time related information at the
 * start of each render cycle.
 */
class AUTimeInfo {
  public:

	AUTimeInfo();
	void init();
	void trace();
	void assimilate(AUTimeInfo* src, UInt32 frames, int interrupts, 
					bool trace);

	//
	// DoRender
	//

	// This comes from the input AudioTimeStamp passed to RenderBus
	// It seems to just increment by the block size forever.
	// Not sure what use this will be.

	Float64 sampleTime;

	//
	// CallHostBeatAndTempo
	//

	// Docs: The exact beat value that applies to the *start* of the 
	// current buffer that the audio unit has been asked to render.
	// this may be (usually) a fractional beat value
	//
	// Jeff: It is a fractional beat counter that changes on every
	// interrupt.   When this reaches 1 we're on a beat.  This would be a good
	// beat decetor but I'm not sure how to convert this into frames.

	Float64 currentBeat;

	// Docs: The current tempo at the time of the first sample in the 
	// current buffer.  If there is a tempo change within the buffer itself 
	// this cannot be communicated.  Tempo is defined as the number of
	// whole-number (integer) beat values per minute.
	//
	// Jeff: this is definitely not a whole-number, it is a normal
	// fractional tempo that can be copied directly to AudioTime

	Float64 currentTempo;

	//
	// CallHostTransportState
	//

	// Docs: The timeline of the host's transport is advancing.
	//
	// Jeff: Goes on and off with the transport, this is our primary
	// means of transport detection though we could also watch for an
	// advancing currentBeat.

	Boolean isPlaying;

	// Docs: Time-line has started or stopped or the position within the
	// time-line has chaged.
	//
	// Jeff: This is a momentary, it goes true when something changes then
	// returns to false.  I don't see much use for this since isPlaying
	// reflects it?

	Boolean transportStateChanged;

	// Docs: The number of samples from the start of the song, that the AU's
	// current render cycle starts at.
	//
	// Jeff: Like sampleTime it changes every interrupt but is relateive
	// to the beat 0 of the transport. Haven't found a use for this.

	Float64	currentSampleInTimeLine;

	// Docs: True if cycling (looping).
	//Jeff: I haven't witnessed this, but it probably goes true if the
	// host is in some sort of loop play mode.  We could try to be smart
	// about these but I haven't seen the need yet.

	Boolean	isCycling;

	// Docs: If cycling, the beat of the start of the cycle.
	Float64	cycleStartBeat;

	// Docs: If cycling, the beat of the end of the cycle.
	Float64	cycleEndBeat;

	//
	// CallHostMusicalTimeLocation
	//

	// Docs: Number of samples until the next whole beat from
	// the start sample of the current rendering buffer.
	//
	// Jeff: It decrements every interrupt, when this is less than
	// the block size the beat will happen in this block.  
	// This is the most useful thing for determining beat/bar boundaries,
	// but unfortunately not all hosts seem to support this.  Aulab does.

	UInt32 deltaSampleOffsetToNextBeat;

	// Docs: The number of beats of the denominator value that are contained
	// in the current measure.
	// Jeff: this seems to wobble around after two decimal places
	Float32 timeSig_Numerator;

	// Docs: A whole (integer) beat in any of the beat values is generally
	// considered to be a quarter note.
	UInt32 timeSig_Denominator;

	// Docs: The beat that corresponds to the downbeat of the current
	// measure that is being rendered.
	// Jeff: happens on every bar boundary depending on timeSig

	Float64 currentMeasureDownBeat;

};

//////////////////////////////////////////////////////////////////////
//
// AUMobius
//
//////////////////////////////////////////////////////////////////////

/**
 * Private property to get to the AUBase from the AUCarbonViewBase
 * From the headers:
 *   "Apple reserves property values from 0 -> 63999"
 *    Developers are free to use property IDs above this range at their 
 *    own discretion"
 *
 * Technically we're not supposed to do this but I'm sick of trying
 * to get anything done with the over-engineered framework.
 * This should be fine as long as both classes are in the same library.
 */
#define kAUMobiusProperty_AUBase 64000

class AUMobius : public AUMIDIEffectBase, public HostInterface, public AudioInterface, public AudioStream {
  public:

	//
	// AUBase
	//

	AUMobius(AudioUnit component);
	~AUMobius();

    int	GetNumCustomUIComponents();
    void GetUIComponentDescs(ComponentDescription* inDescArray);

	ComponentResult	Version();
	bool SupportsTail();

	UInt32 SupportedNumChannels (const AUChannelInfo** outInfo);

	ComponentResult GetParameterInfo(AudioUnitScope	scope,
									 AudioUnitParameterID id,
									 AudioUnitParameterInfo	&info);
	
	ComponentResult GetParameterValueStrings(AudioUnitScope scope,
											 AudioUnitParameterID id,
											 CFArrayRef *outStrings);

	ComponentResult	GetPropertyInfo(AudioUnitPropertyID	inID,
									AudioUnitScope inScope,
									AudioUnitElement inElement,
									UInt32&	outDataSize,
									Boolean& outWritable);

	ComponentResult	GetProperty(AudioUnitPropertyID	inID,
								AudioUnitScope inScope,
								AudioUnitElement inElement,
								void* outData);

	ComponentResult	RenderBus(AudioUnitRenderActionFlags& ioActionFlags,
							  const AudioTimeStamp&	inTimeStamp,
							  UInt32 inBusNumber,
							  UInt32 inNumberFrames);

	ComponentResult	ProcessScheduledSlice(void *inUserData,
										  UInt32 inStartFrameInBuffer,
										  UInt32 inSliceFramesToProcess,
										  UInt32 inTotalBufferFrames);

	ComponentResult	SetParameter(AudioUnitParameterID inID,
								 AudioUnitScope inScope,
								 AudioUnitElement inElement,
								 Float32 inValue,
								 UInt32 inBufferOffsetInFrames);

	//
	// AUEffectBase
	// GetNumberOfChannels looks interesting?
	// 

	ComponentResult Initialize();

	// 
	// AUMIDIBase
	// Also have a SysEx(inData, inLength) if you need sysex
	//

	OSStatus HandleMidiEvent(UInt8 inStatus,
							 UInt8 inChannel,
							 UInt8 inData1,
							 UInt8 inData2,
							 long inStartFrame);

	//
	// HostInterface
	//
	
	Context* getContext();
	const char* getHostName();
	const char* getHostVersion();
	AudioInterface* getAudioInterface();
	void notifyParameter(int id, float value);

	// 
	// AudioInterface
	//

	void terminate();
	AudioDevice** getDevices();
	AudioDevice* getDevice(int id);
	AudioDevice* getDevice(const char* name, bool output);
	void printDevices();

	// only thing interesting
	AudioStream* getStream();

	//
	// AudioStream
	//

	AudioInterface* getInterface();

	bool setInputDevice(int id);
	bool setInputDevice(const char* name);
	AudioDevice* getInputDevice();

	bool setOutputDevice(int id);
	bool setOutputDevice(const char* name);
	AudioDevice* getOutputDevice();

    int getInputChannels();
	int getInputPorts();

    int getOutputChannels();
	int getOutputPorts();

	void setSampleRate(int i);
	int getSampleRate();

	void setHandler(AudioHandler* h);
	AudioHandler* getHandler();

	bool open();
	void close();

	// this is PortAudio specific, weed this out!!
	const char* getLastError();
	void setSuggestedLatencyMsec(int msec);
    int getInputLatencyFrames();
    void setInputLatencyFrames(int frames);
    int getOutputLatencyFrames();
    void setOutputLatencyFrames(int frames);
	void printStatistics();

	// AudioHandler callbacks
	long getInterruptFrames();
	void getInterruptBuffers(int inport, float** inbuf, 
							 int outport, float** outbuf);
	AudioTime* getTime();
	double getStreamTime();
	double getLastInterruptStreamTime();

	//
	// Our extra stuff
	//
	
	PluginInterface* getPlugin();

  private:

	void declareParameters();
	void initParameters();
	void importParameters();
	void exportParameters();
	void exportParameter(class PluginParameter* p);
	void whine(const char* msg);
    void sendMidiEvents();

	void captureHostTime(const AudioTimeStamp& inTimeStamp, UInt32 frames);

	OSStatus processBuffers(AudioUnitRenderActionFlags& ioActionFlags,
							const AudioBufferList& inBuffer,
							AudioBufferList& outBuffer,
							UInt32 inFramesToProcess);

	void interleaveBuffers(const AudioBufferList* sources, long frames, float* dest);
	void deinterleaveBuffers(float* src, int frames, AudioBufferList* outputs);

	bool mTrace;
	bool mTraceParameters;
	bool mTraceSync;
	bool mWhined;

	AUChannelInfo mChannelInfo[MAX_HOST_PLUGIN_PORTS];
	AUTimeInfo mTimeInfo;
	HostSyncState* mSyncState;

	Context* mContext;
	PluginInterface* mPlugin;
	
	AudioHandler* mHandler;
	AudioTime mTime;
	AudioStreamPort mPorts[MAX_HOST_PLUGIN_PORTS];
	int mInputPorts;
	int mOutputPorts;
	int mSampleRate;
	int mInputLatency;
	int mOutputLatency;

	AudioUnitRenderActionFlags mInterruptActionFlags;
	int mInterruptFrames;
	int mInterruptSliceFrames;
	int mInterruptOffset;

	//const AudioBufferList* mInterruptInputs;

	int mInterrupts;
	int mTimeInfoAssimilations;
};

//////////////////////////////////////////////////////////////////////
//
// AUMobiusView
//
//////////////////////////////////////////////////////////////////////

/**
 * ComponentBase has these interesting virtuals:
 *    PostConstructor
 *    PreDestructor
 *    Version
 *
 * AUCarbonViewBase has these:
 *    CreateCarbonView - if you don't want to use the auto-sizing stuff
 *    CreateUI - the usual place build things
 *    HandleEvent - but we register our ownhandler
 *    RespondToEventTimer
 *
 */
class AUMobiusView : public AUCarbonViewBase
{

  public:

	AUMobiusView(AudioUnitCarbonView auv);
	~AUMobiusView();
	
	/**
	 * Opens everything.
	 * Destructor handles cleanup.
	 */
	virtual OSStatus CreateUI(Float32 xoffset, Float32 yoffset);

  private:
	
	AUMobius* getAUMobius();

	bool mTrace;

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
