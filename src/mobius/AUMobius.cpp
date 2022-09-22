/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * An AudioUnit plugin providing the glue around MobiusPlugin.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "Util.h"
#include "Trace.h"
#include "Thread.h"
#include "MacUtil.h"
#include "MidiEvent.h"

// currently from qwin but not dependent on the full qwin model
#include "Context.h"

#include "HostConfig.h"

#include "MacInstall.h"
#include "AUMobiusConstants.h"
#include "AUMobius.h"

/**
 * This is the CFBundleIdentifier from Info.plist
 * The names must match.
 */
#define BUNDLE_ID "circularlabs.mobiusau.2.5"

// until we have a more flexible way to cluster these
#define PORT_CHANNELS 2

//////////////////////////////////////////////////////////////////////
//
// Component Entry Points
//
//////////////////////////////////////////////////////////////////////

/*
 * Defined in ComponentBase.h
 * Goes through an obscenely byzantine process involving 
 * a template class (ComponentEntryPoint) and various levels
 * of "dispatching".  Somewhere in this mess the specified
 * class is instantiated and various overloadable initialization
 * methods called.
 */
COMPONENT_ENTRY(AUMobius)
COMPONENT_ENTRY(AUMobiusView)

//////////////////////////////////////////////////////////////////////
//
// AUTimeInfo
//
//////////////////////////////////////////////////////////////////////

AUTimeInfo::AUTimeInfo()
{
}

void AUTimeInfo::init()
{
	sampleTime = 0.0f;
	currentBeat = 0.0f;
	currentTempo = 0.0f;
	isPlaying = false;
	transportStateChanged = false;
	currentSampleInTimeLine = 0.0f;
	isCycling = false;
	cycleStartBeat = 0.0f;
	cycleEndBeat = 0.0f;
	deltaSampleOffsetToNextBeat = 0;
	timeSig_Numerator = 0.0f;
	timeSig_Denominator = 0;
	currentMeasureDownBeat = 0.0f;
}

void AUTimeInfo::trace()
{
	printf("*** AUTimeInfo ***\n");
	printf("sampleTime=%f\n", (float)sampleTime);
	printf("currentBeat=%f\n", (float)currentBeat);
	printf("currentTempo=%f\n", (float)currentTempo);
	printf("isPlaying=%s\n", (isPlaying) ? "true" : "false");
	printf("transportStateChanged=%s\n", (transportStateChanged) ? "true" : "false");
	printf("currentSampleInTimeLine=%f\n", (float)currentSampleInTimeLine);
	printf("isCycling=%s\n", (isCycling) ? "true" : "false");
	printf("cycleStartBeat=%f\n", (float)cycleStartBeat);
	printf("cycleEndBeat=%f\n", (float)cycleEndBeat);
	printf("deltaSampleOffsetToNextBeat=%d\n", (int)deltaSampleOffsetToNextBeat);
	printf("timeSig_Numerator=%f\n", (float)timeSig_Numerator);
	printf("timeSig_Denominator=%d\n", (int)timeSig_Denominator);
	printf("currentMeasureDownBeat=%f\n", (float)currentMeasureDownBeat);
	fflush(stdout);
}

/**
 * Assimilate sync information obtained from several AU host calls.
 * Originally I was going to do the AudioTime updates here, but
 * the interesting logic has been factored out to HostSyncState.
 * Here we just track changes and trace.
 */
void AUTimeInfo::assimilate(AUTimeInfo* src, UInt32 frames, int interrupt, 
							bool trace)
{
	bool resumed = false;
	bool stopped = false;

	// only use this when debugging
	if (false) {
		float delta = (src->currentBeat - currentBeat);
		printf("%d: currentSampleInTimeLine %f currentBeat %f beatIncrement %f\n", 
			   interrupt, (float)src->currentSampleInTimeLine, 
			   (float)src->currentBeat, 
			   delta);
	}

	// sampleTime
	//
	// This just seems to increment by the block size forever, not sure
	// what to use it for.
	sampleTime = src->sampleTime;

	// currentBeat
	//
	// Truncate this and pass through AudioTime for the sync display
	// This is done later when we handle beat boundaries.
	// This is the only thing that comes in reliably in Logic

	Float64 newBeat = src->currentBeat;
	if (trace && currentBeat != newBeat)
	  printf("%d: currentBeat=%f\n", interrupt, (float)newBeat);
	currentBeat = newBeat;
	
	// currentTempo
	//
	// Just pass this through to the AudioTime

	if (trace && currentTempo != src->currentTempo)
	  printf("%d: currentTempo=%f\n", interrupt, (float)src->currentTempo);
	currentTempo = src->currentTempo;

	// isPlaying
	//
    // Goes on and off with the transport, our primary means of detection.
	// This goes directly to AudioTime.playing

	if (trace && isPlaying != src->isPlaying)
	  printf("%d: isPlaying=%s\n", interrupt, (src->isPlaying) ? "true" : "false");

	// this is used later to update AudioTime
	resumed = (!isPlaying && src->isPlaying);
	stopped = (isPlaying && !src->isPlaying);
	isPlaying = src->isPlaying;

	// transportStateChanged
	//
	// I don't see a use for this, isPlaying does the job.

	if (trace && transportStateChanged != src->transportStateChanged) {
		if (!transportStateChanged)
		  printf("%d: transportStateChanged=true\n", interrupt);
	}
	transportStateChanged = src->transportStateChanged;

	// currentSampleInTimeLine
	//
	// Like sampleTime it changes every interrupt but is relateive
	// to the beat 0 of the transport. Haven't found a use for this.

	if (trace && currentSampleInTimeLine != src->currentSampleInTimeLine)
	  printf("%d: currentSampleInTimeLine=%f\n", interrupt,
			 (float)src->currentSampleInTimeLine);
	currentSampleInTimeLine = src->currentSampleInTimeLine;

	// isCycling, cycleStartBeat, cycleEndBeat
	//
	// These come from CallHostTransportState
	// I haven't witnessed this, but it probably goes true if the
	// host is in some sort of loop play mode.  We could try to be smart
	// about these but I haven't seen the need yet.

	if (trace && isCycling != src->isCycling) 
	  printf("%d: isCycling=%s\n", interrupt, (src->isCycling) ? "true" : "false");
	isCycling = src->isCycling;

	if (trace && cycleStartBeat != src->cycleStartBeat)
	  printf("%d: cycleStartBeat=%f\n", interrupt, (float)src->cycleStartBeat);
	cycleStartBeat = src->cycleStartBeat;

	if (trace && cycleEndBeat != src->cycleEndBeat)
	  printf("%d: cycleEndBeat=%f\n", interrupt, (float)src->cycleEndBeat);
	cycleEndBeat = src->cycleEndBeat;

	// deltaSampleOffsetToNextBeat
	//
	// It decrements every interrupt, when this is less than the block size
	// the beat will happen in this block.  
	// This is the most useful thing for determining beat/bar boundaries,
	// but unfortunately not all hosts seem to support this.  Aulab does.

	if (trace && deltaSampleOffsetToNextBeat != src->deltaSampleOffsetToNextBeat)
	  printf("%d: deltaSampleOffsetToNextBeat=%d\n", interrupt, 
			 (int)src->deltaSampleOffsetToNextBeat);
	deltaSampleOffsetToNextBeat = src->deltaSampleOffsetToNextBeat;

	// timeSig_Numerator
	//
	// Docs: The number of beats of the denominator value that are contained
	// in the current measure.
	// this seems to wobble around after two decimal places

	int ival = (int)(src->timeSig_Numerator * 100.0f);
	Float32 newval = (float)ival / 100.0f;
	if (trace && (resumed || (timeSig_Numerator != newval)))
	  printf("%d: timeSig_Numerator=%f\n", interrupt, (float)newval);
	timeSig_Numerator = newval;

	// timeSig_Denominator
	//
	// Docs: A whole (integer) beat in any of the beat values is generally
	// considered to be a quarter note.

	if (trace && (resumed || (timeSig_Denominator != src->timeSig_Denominator)))
	  printf("%d: timeSig_Denominator=%d\n", interrupt, (int)src->timeSig_Denominator);
	timeSig_Denominator = src->timeSig_Denominator;

	// currentMeasureDownBeat
	//
	// Happens on every bar boundary depending on timeSig.  Haven't
	// found a use yet.  Aulab doesn't seem to set this, Logic does

	if (trace && currentMeasureDownBeat != src->currentMeasureDownBeat)
	  printf("%d: currentMeasureDownBeat=%f\n", interrupt, 
			 (float)src->currentMeasureDownBeat);
	currentMeasureDownBeat = src->currentMeasureDownBeat;

	if (trace)
	  fflush(stdout);
}

//////////////////////////////////////////////////////////////////////
//
// AUMobius
//
//////////////////////////////////////////////////////////////////////

/**
 * AUMIDIEffectBase constructor has an optional second arg 
 * inProcessInPlace, not sure what it does..
 * 
 * Immediately after construction AUBase will call PostConstructor
 * which by default calls CreateElements.  The number if input
 * and output elements must be set by then.  I had to add setters
 * for these to AUBase.h since there appears to be no way to 
 * pass initializers through the AUMIDIEffectBase constructor.
 *
 * To let the number of elements be configurable create
 * the PluginInterface right away and ask it.
 *
 */
AUMobius::AUMobius(AudioUnit component)
    : AUMIDIEffectBase(component)
{
	// general plugin trace
	mTrace = false;
	// tracing parameters can clutter the log!
	mTraceParameters = false;
	// tracing sync even more
	mTraceSync = false;
	mWhined = false;
	mContext = NULL;
	mPlugin = NULL;
	mHandler = NULL;
	mInputPorts = 8;
	mOutputPorts = 8;
    // !! figure out what this is from the host
	mSampleRate = CD_SAMPLE_RATE;
	mInputLatency = 512;
	mOutputLatency = 512;
	mInterruptActionFlags = 0;
	mInterruptFrames = 0;
	mInterruptSliceFrames = 0;
	mInterruptOffset = 0;
	mInterrupts = 0;
	mTimeInfoAssimilations = 0;

	// MAX_HOST_PLUGIN_PORTS is 16
	for (int i = 0 ; i < MAX_HOST_PLUGIN_PORTS ; i++) {
		AudioStreamPort* port = &mPorts[i];
		port->input = new float[MAX_HOST_BUFFER_FRAMES * MAX_HOST_BUFFER_CHANNELS];
		port->inputPrepared = false;
		port->output = new float[MAX_HOST_BUFFER_FRAMES * MAX_HOST_BUFFER_CHANNELS];
		port->outputPrepared = false;
	}

	mTimeInfo.init();
	mTime.init();
	mSyncState = new HostSyncState();

	if (mTrace)
	  trace("AUMobius::AUMobius\n");

	// the host independent implementation is in here,
	// this must be linked in from another file
	// !! still have issues over who gets to build Context
	mPlugin = PluginInterface::newPlugin(this);
	int nPorts = mPlugin->getPluginPorts();

	// Figure out the host, one way is to get the id 
	// of the main bundle.  A more informative way
	// is to use kAudioUnitProperty_AUHostIdentifier
	// but I can't find a good example.
	char *host = NULL;
	CFBundleRef mainBundle = CFBundleGetMainBundle();
	if (mainBundle) {
		CFStringRef identifier = CFBundleGetIdentifier(mainBundle);
		if (identifier != NULL)
		  host = GetCString(identifier);
	}

	if (host != NULL) {
		printf("AUMobius: Host is %s\n", host);
		fflush(stdout);
	}
	
    // Read host configuration options from the host.xml file.
	HostConfigs* hostConfig = mPlugin->getHostConfigs();
	if (hostConfig != NULL) {
		// interface is weird, we set the "scope" given what
		// we know about the host then the methods change behavior
		// first argument is vendor, second product, and third version
		// since we only have the parent bundle id make this
		// the product
		hostConfig->setHost(NULL, host, NULL);
		mSyncState->setHost(hostConfig);

        // For a few hosts known to only support stereo, reduce the pin count.
        // We don't really need this since port counts are now configurable
        // but it's nice when trying out different hosts to not have
        // to reconfigure pins.
        if (hostConfig->isStereo()) {
            trace("AUMobius: Host only supports 2 channels\n");
            nPorts = 1;
        }
	}

	// KLUDGE: Until we can figure out how to distribute
	// HostConfigs and get them upgraded, hard code rules
	// for the few mac hosts that matter
	if (StringEqual(host, "com.apple.logic.pro"))
	  mSyncState->setHostRewindsOnResume(true);

	delete host;

	// I added these to AUBase.h so we could set things up on construction
    // !! should NOT be doing this
	SetInitNumInputEls(nPorts);
	SetInitNumOutputEls(nPorts);

	// SooperLooper does it this way which seems to work just as well?
	/*
	CreateElements();
	SetBusCount (kAudioUnitScope_Input, nPorts);
	SetBusCount (kAudioUnitScope_Output, nPorts);		
	*/

	// these are redundant, could get them from the element counts?
	mInputPorts = nPorts;
	mOutputPorts = nPorts;

	if (mTrace)
	  trace("AUMobius::declaring parameters\n");

	declareParameters();

	if (mTrace)
	  trace("AUMobius::AUMobius finished\n");
}

AUMobius::~AUMobius()
{
	if (mTrace)
	  trace("AUMobius::~AUMobius %p\n", this);

	// have to detach the Recorder callback that Mobius added
	// to the stream, come up with a better interface!
	// ?? do we, this is a VST thing, not sure if it applies here...
	mHandler = NULL;

	for (int i = 0 ; i < MAX_HOST_PLUGIN_PORTS ; i++) {
		AudioStreamPort* port = &mPorts[i];
		delete port->input;
		delete port->output;
	}

	// make sure we're not in an interrupt
	SleepMillis(100);
	delete mPlugin;

	// shouldn't have to do this but leaving a thread behind causes
	// Live and other hosts to crash
	//ObjectPoolManager::exit(false);

	// originally had another SleepMillis here which I don't think
	// is necessary but mysteriously it causes auval to crash
	//	SleepMillis(100);
	delete mContext;

	if (mTrace)
	  trace("AUMobius::~AUMobius finished\n");
}

/**
 * This is where we're supposed to do expensive initialization.
 * There is also Cleanup() which is supposed to take it back
 * to an uninitialized-yet-still-open state.
 */
ComponentResult AUMobius::Initialize()
{
	if (mTrace)
	  trace("AUMobius::Initialize\n");

	// verify channel counts in case we used [-1,-1]
	int inputs = GetInput(0)->GetStreamFormat().mChannelsPerFrame;
	int outputs = GetOutput(0)->GetStreamFormat().mChannelsPerFrame;
	if (inputs != 2 || outputs != 2) {
		if (mTrace)
		  trace("AUMobius::Initialize: rejecting channel format %d %d\n",
				inputs, outputs);
		return kAudioUnitErr_FormatNotSupported;
	}

	ComponentResult result = AUEffectBase::Initialize();

	if (result == kAudioUnitErr_FormatNotSupported) {
		// this is normal with auval that tries to initialize
		// our stream to something we say we don't support
	}
	else if (result != noErr){
		printf("AUEffectBase::Initialize %d\n", (int)result);
		fflush(stdout);
	}

	if (result == noErr) {
        mPlugin->start();

        // VST calls resume and suspend when the plugin is bypassed
        // or processing stops, does AU have anything like that?
        //mPlugin->resume();
    }

	// Now that the configurations have been loaded we can set the
	// real values of parameters.  It would hae been better to do this
	// when the parameters were published in the constructor, but I didn't
	// want to call mPlugin->start there
	initParameters();
	
	// save to call this now?
	Float64 sampleRate = GetOutput(0)->GetStreamFormat().mSampleRate;
	if (mTrace)
	  trace("AUMobius: sampleRate %f\n", sampleRate);

	if (sampleRate > 0)
	  mSampleRate = (int)sampleRate;






	return result;
}

/**
 * Overload to expose a special property 
 */
ComponentResult	AUMobius::GetPropertyInfo(AudioUnitPropertyID inID,
										  AudioUnitScope inScope,
										  AudioUnitElement inElement,
										  UInt32&	outDataSize,
										  Boolean& outWritable)
{
	ComponentResult result = noErr;

	if (inID == kAUMobiusProperty_AUBase)
	  outDataSize = sizeof(void*);
	else
	  result = AUEffectBase::GetPropertyInfo(inID, inScope, inElement, outDataSize, outWritable);

	return result;
}

ComponentResult	AUMobius::GetProperty(AudioUnitPropertyID inID,
									  AudioUnitScope inScope,
									  AudioUnitElement inElement,
									  void* outData)
{
	ComponentResult result = noErr;

	if (inID == kAUMobiusProperty_AUBase) {
		void** pThis = (void**)(outData);
		*pThis = (void*)this;
	}
	else
	  result = AUEffectBase::GetProperty(inID, inScope, inElement, outData);

	return result;
}

/**
 * Return the number of custom UI components
 * Examples show 1 for a Carbon UI.
 */
int AUMobius::GetNumCustomUIComponents () 
{
	// todo, need overridable fields for name etc....
	if (mTrace)
	  trace("AUMobius::GetNumCustomUIComponents\n");
	return 1; 
}

/**
 * Return info about our UI.
 */
void AUMobius::GetUIComponentDescs(ComponentDescription* inDescArray)
{
	if (mTrace)
	  trace("AUMobius::GetUIComponentDescs\n");

	inDescArray[0].componentType = kAudioUnitCarbonViewComponentType;
	inDescArray[0].componentSubType = kAUMobiusSubType;
	inDescArray[0].componentManufacturer = kAUMobiusManufacturer;
	inDescArray[0].componentFlags = 0;
	inDescArray[0].componentFlagsMask = 0;
}

/**
 * Return the version number.
 * This is virtual.
 */
ComponentResult AUMobius::Version()
{ 
	if (mTrace)
	  trace("AUMobius::Version\n");

	return kAUMobiusVersion;
}

/**
 * Return true if we "support tail".
 * This is virtual.
 * Personally, I'm all for tail, not sure we're talking
 * about the same thing though.
 *
 * This is called by auval as part of the "recommended properties" section.
 */
bool AUMobius::SupportsTail()
{
	if (mTrace)
	  trace("AUMobius::SupportsTail\n");

	return true; 
}

/**
 * AUBase returns zero to mean "unsupported" and the host generally
 * provides a menu to select the desired number of channels.
 * AUEffectBase expects the return value to be the number of 
 * AUCHannelInfo's returned.  Typically this is one but you could
 * have several allowed configs.
 *
 * Do not confuse this with "busses".  What we return here are
 * the supported channel configurations for each bus, e.g.
 * mono, stereo, quad, 1 in 2 out, etc. 
 *
 * SooperLooper returns [-1,-1] under "Reported Channel Capabilities (explicit):"
 * then it goes through some handshaking to get multiple menu
 * items for various channel configurations.
 * 
 * Mobius only supports stereo so we return one config [2,2]
 * 
 */
UInt32 AUMobius::SupportedNumChannels (const AUChannelInfo** outInfo)
{
	int channels = PORT_CHANNELS;
	//int channels = -1;

	// I thought we had to return one of these for each bus, but no
	// only if there are different configs
	int configs = 1;
	for (int i = 0 ; i < configs ; i++) {
		mChannelInfo[i].inChannels = channels;
		mChannelInfo[i].outChannels = channels;
	}
	
	// outInfo will be null when getting property info to see how
	// may busses we have
	if (outInfo != NULL)
	  *outInfo = &mChannelInfo[0];

	return configs;
}

//////////////////////////////////////////////////////////////////////
//
// Rendering
//
//////////////////////////////////////////////////////////////////////


/**
 * Capture time information from the host into the generic model.
 * I'm asking for everything there is, we don't need all of it but
 * I'd like to monitor it for awhile to understand what they do
 * and also copy some documentation.
 */
void AUMobius::captureHostTime(const AudioTimeStamp& inTimeStamp, UInt32 frames)
{
	OSStatus err;
	AUTimeInfo info;

	mInterrupts++;

	info.sampleTime = inTimeStamp.mSampleTime;

	// NOTE: AUlab will return error on these calls until the transport
	// window is brought up and a sync source is selected.  Must ignore
	// until we don't get errors.  

	err = CallHostBeatAndTempo(&info.currentBeat, &info.currentTempo);

	// getting errors under Aulab, is this a temporary setup condition?
	if (err) {
		//trace("AUMobius::CallHostBeatAndTempo %d\n", err);
	}
	else {
		err = CallHostTransportState(&info.isPlaying,
									 &info.transportStateChanged,
									 &info.currentSampleInTimeLine,
									 &info.isCycling,
									 &info.cycleStartBeat,
									 &info.cycleEndBeat);
		if (err) {
			//trace("AUMobius::CallHostTransportState %d\n", err);
		}
		else {
			err = CallHostMusicalTimeLocation(&info.deltaSampleOffsetToNextBeat,
											  &info.timeSig_Numerator,
											  &info.timeSig_Denominator,
											  &info.currentMeasureDownBeat);
			if (err) {
				//trace("AUMobius::CallHostMusicalTimeLocation %d\n", err);
			}
			else {
				mTimeInfoAssimilations++;
				if (mTimeInfoAssimilations > 1)
				  mTimeInfo.assimilate(&info, frames, mInterrupts, mTraceSync);
				else {
					if (mTraceSync) {
						info.trace();
						printf("%d: blockSize=%d\n", mInterrupts, (int)frames);
						fflush(stdout);
					}
					mTimeInfo.assimilate(&info, frames, mInterrupts, false);
				}

				mSyncState->updateTempo(mSampleRate, 
										info.currentTempo, 
										info.timeSig_Numerator,
										info.timeSig_Denominator);

				mSyncState->advance((int)frames, 
									info.currentSampleInTimeLine,
									info.currentBeat,
									info.transportStateChanged,
									info.isPlaying);

				mSyncState->transfer(&mTime);
			}
		}
	}
}

/**
 * Overloaded from AUBase.h  The default implementation just calls 
 * NeedsToRender then Render without passing down the bus number.
 * AUEffectBase::Render does the interesting work of slicing the buffer
 * up for each scheduled parameter, but it only handles one bus.
 * We have to duplicate some of the AUEffectBase logic here.
 *
 * From AUBase.h:
 *   N.B. Implementations of this method can assume that the output's buffer list has already been
 *   prepared and access it with GetOutput(inBusNumber)->GetBufferList() instead of 
 *   GetOutput(inBusNumber)->PrepareBuffer(nFrames) -- if PrepareBuffer is called, a
 *   copy may occur after rendering.
 * 
 * We do a full rendering the for the first bus on each cycle, then let subsequent
 * calls get the rendered blocks.
 * 
 * RenderActionFlags can be:
 *   kAudioUnitRenderAction_PreRender
 *   kAudioUnitRenderAction_PostRender
 *   kAudioUnitRenderAction_OutputIsSilence
 *
 * I don't think the Pre/Post render flags are meaningful here, 
 * they're only used for callbacks registered by the host.
 *
 * AUEffectBase uses the OutputIsSilence flag for the kernel's to 
 * pass back that the output buffer should be zeroed.
 * 
 * In theory buffers may be interleaved or non-interleaved but 
 * the SDK asks for non-interleaved by default and this apparently
 * has been the standard since v2 in 10.2.
 * 
 */
ComponentResult	AUMobius::RenderBus(AudioUnitRenderActionFlags& ioActionFlags,
									const AudioTimeStamp&	inTimeStamp,
									UInt32 inBusNumber,
									UInt32 inNumberFrames)
{
	ComponentResult result = noErr;

	// this returns true whenever inTimestamp.mSampleTime changes
	// AUBase has mLastRenderedSampleTime to keep track of this

	if (NeedsToRender(inTimeStamp.mSampleTime)) {
		// capture host time
		captureHostTime(inTimeStamp, inNumberFrames);
 
		// capture changes to AU parameters since the last render cycle
		importParameters();

		// reset our port buffers
		for (int i = 0 ; i < MAX_HOST_PLUGIN_PORTS ; i++) {
			AudioStreamPort* port = &mPorts[i];
			port->inputPrepared = false;
			port->outputPrepared = false;
		}

		// pull from each connected input, we'll convert the results later
		// in calls to getInterruptBuffers
		AUScope& inputs = Inputs();
		int nInputs = inputs.GetNumberOfElements();
		for (int i = 0 ; i < nInputs && result == noErr ; i++) {
			AUInputElement* input = GetInput(i);
			if (input == NULL) {
				// not supposed to happen
				whine("NULL input element during rendering\n");
			}
			else {
				// need to pass element number in case this is handled
				// by a callback
				ComponentResult r = input->PullInput(ioActionFlags, inTimeStamp, i, inNumberFrames);
				if (r == kAudioUnitErr_NoConnection) {
					// this is okay, just ignore this input
				}
				else if (r != noErr) {
					// AUEffectBase would skip processing if the input
					// couldn't be rendered, I suppose we could try to 
					// do the others?
					whine("Unable to pull input from bus\n");
					result = r;
				}
			}
		}

		// render input ports to output ports
		if (result == noErr) {
			if (ShouldBypassEffect()) {
				// AUEffectBase uses this to pass inputs diretly to
				// the corresponding output but that doesn't necessarily
				// make sense for us, just leave the outputs silent and don't
				// advance Mobius
			}
			else if (mHandler == NULL) {
				// no where to go
			}
			else if (inNumberFrames > MAX_HOST_BUFFER_FRAMES) {
				// this would cause an internal buffer overflow
				Trace(1, "Too many AU buffer frames!\n");
			}
			else if (inNumberFrames == 0) {
				//Trace(1, "No frames to process!\n");
			}	
			else {
				// rendering needs to be sliced up by the scheduled events
				// AUBase does the slicing and calls back to ProcessScheduledSlice
				// mParamList defined on AUBase
				
				mInterruptActionFlags = ioActionFlags;
				mInterruptFrames = inNumberFrames;

				if (mParamList.size() == 0) {
					// AUEffectBase makes this optimization to avoid a little
					// parameter list stdlib calls

					result = ProcessScheduledSlice(NULL, 0, inNumberFrames, inNumberFrames);
				}
				else {
					// third arg is a void* we can use to pass state, 
					// we'll just capture what we need in some transient fields
					// instead
					result = ProcessForScheduledParams(mParamList, inNumberFrames, NULL);
				}
			}
		}

		// send parameter changes made during this render cycle
		// back to the view
		exportParameters();

        // send MIDI events accumulated during this render cycle
        sendMidiEvents();
	}

	// Deinterleave the stream buffers into the output buffer for the requested bus.
	// Outputs left in the AudioStream buffers.  Stream buffers will be unprepared
	// if there was a problem or if they weren't targeted by any track.
	bool zero = false;

	// this will throw if error
	AUOutputElement* output = GetOutput(inBusNumber);

	// AUEffectBase uses a ProcessInPlace flag to just use the 
	// input buffers as the output buffers as an optimization.
	// We might be able to that here since we render to intermediate
	// buffers anyway, I guess this would avoid some allocations?
	AudioBufferList& outbuffers = output->GetBufferList();

	if (result != noErr) {
		// trouble pulling inputs, do we need to zero?  
		zero = true;
	}
	else if (ShouldBypassEffect()) {
		// AUEffectBase uses this to pass inputs diretly to
		// the corresponding output but that doesn't necessarily
		// make sense for us, just leave the outputs silent and don't
		// advance Mobius
		zero = true;
		/*
		  if(!ProcessesInPlace() )
		  theInput->CopyBufferContentsTo (theOutput->GetBufferList());
		*/
	}
	else if (inBusNumber >= 0 && inBusNumber < mOutputPorts) {
		AudioStreamPort* port = &mPorts[inBusNumber];
		if (!port->outputPrepared) {
			// this is normal if no track targeted this port
			zero = true;
		}
		else if (outbuffers.mNumberBuffers != PORT_CHANNELS) {	
			whine("Unexpected number of output channel buffers\n");
			zero = true;
		}
		else if (outbuffers.mBuffers[0].mNumberChannels != 1){
			whine("Unexpected number of output buffer channels\n");
			zero = true;
		}
		else {
			deinterleaveBuffers(port->output, inNumberFrames, &outbuffers);
		}
	}
	else {
		whine("Bus number out of range\n");
		zero = true;
	}

	// whatever the cause, zero output buffers if we couldn't fill them
	// AUEffectBase has a convention for passing OutputIsSilence around in ioActionFlags
	// and using it to zero the buffer, so I guess it is important that we put
	// something in it.  
	// What if result != noErr, do we still need to zero the buffer?
	if (zero)
	  AUBufferList::ZeroBuffer(outbuffers);

	return result;
}

/**
 * Convert one of our interleaved AudioStream buffers into
 * a non-interleaved AudioBufferList.
 * 
 * AudioBufferList
 *   UInt32 mNumberBuffers
 *   AudioBuffer mBuffers[]
 *
 * AudioBuffer
 *   UInt32 mNumberChannels
 *   UInt32 mDataByteSize
 *   void* mData
 *
 * Like interleaveBuffers we expect to be dealing with one
 * non-interleaved buffer per channel.  mNumberChannels will be 1
 * and mNumberBuffers should be 2.
 */
PRIVATE void AUMobius::deinterleaveBuffers(float* input, int frames,
										   AudioBufferList* outputs)
{
	int channels = outputs->mNumberBuffers;
	int channelsPerPort = PORT_CHANNELS;
	AudioBuffer* outBuffers = outputs->mBuffers;

	for (int channel = 0 ; channel < channels ; channel++) {
		float* dest = (float*)((outBuffers + channel)->mData);
		float* src = input + channel;;

		for (int i = 0 ; i < frames ; i++) {
			if (src == NULL)
			  *dest = 0.0;
			else {
				*dest = *src;
				src += channelsPerPort;
			}
			dest++;
		}
	}
}

/**
 * Called by AUBase::ProcessForScheduledParams for each "slice" between
 * scheduled parameter events.
 */
ComponentResult	AUMobius::ProcessScheduledSlice(void *inUserData,
												UInt32 inStartFrameInBuffer,
												UInt32 inSliceFramesToProcess,
												UInt32 inTotalBufferFrames )
{
	// this is used by getInterruptBuffers to know where the slice begins
	mInterruptOffset = inStartFrameInBuffer;

	// this is returned by getInteruptFrames for the mHandler to know 
	// how many frames to process
	mInterruptSliceFrames = inSliceFramesToProcess;

	// This does the Mobius work calling back to getInterruptBuffers
	// to do the interleaving of input buffers.
	mHandler->processAudioBuffers(this);

	// don't have a way to return errors from getInterruptBuffers, 
	// assume they worked
	return noErr;
}

/**
 * This is part of the AudioStream interface but I moved it up here to 
 * be next to the other rendering code.
 * 
 * Return a pair of frame buffers for one input and output port.
 * Ports buffers must have interleaved stereo frames.
 * !! need more flexibility.
 *
 * Ports correspond to "elements" or "busses" in AU. When asking
 * for input ports we take the buffers that were pulled from the
 * input busses in the first call to RenderBus for this render cycle.
 * Output buffers are maintained as stream buffers then returned
 * at the end of each RenderBus call.
 *
 * When asking for an invalid port we formerly returned NULL which
 * would make Track ignore this interrupt.  Not bad but tracks
 * can advance inconsistently.  Out of range port numbers can
 * happen if you lower the plugin port number in global config
 * but still have track setups that reference higher port numbers.
 *
 * Seems better to be resiliant and return an empty buffer or
 * convert it to one of the available buffers. We've got a few extras
 * so use one of those.  If we're maxed out force it to the first port.
 */
PUBLIC void AUMobius::getInterruptBuffers(int inport, float** inbuf,
										  int outport, float** outbuf)
{
	int channels = PORT_CHANNELS;

	if (inbuf != NULL) {

		if (inport < 0 || inport >= MAX_HOST_PLUGIN_PORTS) {
			// this really shouldn't happen
			inport = 0;	
		}

		AudioStreamPort* port = &mPorts[inport];
		if (!port->inputPrepared) {
			if (inport < mInputPorts) {
				AudioBufferList* aubuffers = NULL;
				AUInputElement* auinput = GetInput(inport);
				if (auinput != NULL) {
					// !! if pull returned k...NoConnection do we need
					// to remember that or will the buffers be empty?
					aubuffers = &(auinput->GetBufferList());
				}
				else {
					// Shouldn't happen unless there is a mismatch
					// between the number of busses advertised at the AU
					// interface, and the number we think we're dealing
					// with internally
					whine("Unable to get input buffer list for port\n");
				}
				interleaveBuffers(aubuffers, mInterruptFrames, port->input);
			}
			else {
				// not attached to anything, return an empty buffer
				int floats = mInterruptFrames * channels;
				memset(port->input, 0, (sizeof(float) * floats));
			}
			port->inputPrepared = true;
		}

		*inbuf = port->input + (mInterruptOffset * channels);
	}

	if (outbuf != NULL) {

		if (outport < 0 || outport >= MAX_HOST_PLUGIN_PORTS)
		  outport = 0;	

		AudioStreamPort* port = &mPorts[outport];
		if (!port->outputPrepared) {
			int floats = mInterruptFrames * channels;
			memset(port->output, 0, (sizeof(float) * floats));
			port->outputPrepared = true;
		}
		*outbuf = port->output + (mInterruptOffset * channels);
	}
}

/**
 * Convert an AU input buffer into an interleaved AudioStreamPort buffer.
 *
 * AudioBufferList
 *   UInt32 mNumberBuffers
 *   AudioBuffer mBuffers[]
 *
 * AudioBuffer
 *   UInt32 mNumberChannels
 *   UInt32 mDataByteSize
 *   void* mData
 *
 * We expect to get non-interleved inputs meaning there will
 * be one AudioBuffer per channel and mNumberChannels will be 1.
 * Since we currently work only in stereo mNumberBuffers should
 * always be 2.
 *
 * In theory we could be configured as one bus with 16 channels
 * rather than 8x2 in which case we would have to offset into
 * mBuffers by the port base.  Let's hope we don't have to go there.
 */
PRIVATE void AUMobius::interleaveBuffers(const AudioBufferList* sources,
										 long frames, 
										 float* output)
{
	const AudioBuffer* srcBuffer = NULL;
	int channels = PORT_CHANNELS;

	if (sources != NULL) {
		if (sources->mNumberBuffers != channels ||
			sources->mBuffers[0].mNumberChannels != 1) {
			// interleaved or mono, we should be neither
			whine("interleaved audio buffers!\n");
		}
		else {
			srcBuffer = sources->mBuffers;   // + portBase
		}
	}

	for (int channel = 0 ; channel < channels ; channel++) {
		float* src = NULL;
		float* dest = output + channel;
		if (srcBuffer != NULL)
		  src = (float*)((srcBuffer + channel)->mData);

		for (int j = 0 ; j < frames ; j++) {
			if (src == NULL)
			  *dest = 0.0;
			else {
				*dest = *src;
				src++;
			}
			dest += channels;
		}
	}
}

PRIVATE void AUMobius::whine(const char* msg)
{
	if (!mWhined) {
        // Note that the msg MUST be static, Trace does not copy it!
		Trace(1, msg);
		mWhined = true;
	}
}

/**
 * Called at the end of the render cycle to pass MIDI events
 * queued during the cycle to the host.
 *
 * It is important to drain these even if we decide not to send them.
 *
 * Apparently AU has not had a way until Leopard for a plugin
 * to be a generator of MIDI events.  See notes/aumidi.txt for
 * an example but it can't be used until everyone upgrades
 * to Leopard.
 */
PRIVATE void AUMobius::sendMidiEvents()
{
    MidiEvent* events = mPlugin->getMidiEvents();
    MidiEvent* next = NULL;

    for (MidiEvent* event = events ; event != NULL ; event = next) {
        next = event->getNext();
        event->setNext(NULL);

        // TODO: call something in AU...
        event->free();
    }
}

//////////////////////////////////////////////////////////////////////
// 
// Parameters
//
//////////////////////////////////////////////////////////////////////

ComponentResult	AUMobius::SetParameter(AudioUnitParameterID	inID,
									   AudioUnitScope 		inScope,
									   AudioUnitElement 	inElement,
									   Float32				inValue,
									   UInt32				inBufferOffsetInFrames)
{
	//trace("SetParameter: %d %f\n", (int)inID, inValue);

	return AUBase::SetParameter(inID, inScope, inElement, inValue, inBufferOffsetInFrames);
}

/**
 * Convert the abstract PluginParameter definitions into
 * AU parameter definitions.
 *
 * See AudioUnitProperties.h in 
 * /Sytem/Library/Frameworks/AudioUnit.frameworks/Headers
 *
 * kAudioUnitParameterUnit_Generic 
 *   - untyped value generally between 0.0 and 1.0 
 * kAudioUnitParameterUnit_Boolean
 *   - 0.0 means FALSE, non-zero means TRUE
 * kAudioUnitParameterUnit_MIDIController
 *   - a generic MIDI controller value from 0 -> 127
 * kAudioUnitParameterUnit_CustomUnit
 *   - this is the parameter unit type for parameters that 
 *     present a custom unit name
 */
ComponentResult AUMobius::GetParameterInfo(AudioUnitScope scope,
										   AudioUnitParameterID id,
										   AudioUnitParameterInfo& info)
{
	ComponentResult result = noErr;

	if (mTrace)
	  trace("AUMobius::GetParameterInfo %d\n", id);

	// other code does this, seems to be an unconditional declaration of R/W
	info.flags =
		kAudioUnitParameterFlag_IsWritable +
		kAudioUnitParameterFlag_IsReadable;	

	// info.flags also has some bits for how the name is set
	// ORd into as a side effect of methods like FillInParameterName
	
	// only interested in global scope
	if (scope != kAudioUnitScope_Global) {
		result = kAudioUnitErr_InvalidParameter;
	}
	else {
		PluginParameter* p = mPlugin->getParameter(id);
		if (p == NULL) 
		  result = kAudioUnitErr_InvalidParameter;

		else {
			// third arg says should release string when done
			// there is also just a "char name[52];" we can put utf8 into,
			// do we really need to mess with CFSTRs?
			// auval whines if we don't use CFStrings
			AUBase::FillInParameterName(info, MakeCFStringRef(p->getName()), true);
			//strcpy(info.name, p->getName());

			info.minValue = p->getMinimum();
			info.maxValue = p->getMaximum();
			info.defaultValue = p->getDefault();

			PluginParameterType type = p->getType();
			if (type == PluginParameterContinuous) {
				// Generic is rendered in Live as a scaled value
				// with a two digit fraction, ugly.  
				// MIDIController stays a nice integer.
				// TODO: There is a special Pan unit we may want
				// to use for pan?
				info.unit = kAudioUnitParameterUnit_MIDIController;
			}
			else if (type == PluginParameterEnumeration) {
				info.unit = kAudioUnitParameterUnit_Indexed;
			}
			else if (type == PluginParameterBoolean) {
				info.unit = kAudioUnitParameterUnit_Boolean;
			}
			else if (type == PluginParameterButton) {
				// this gives a checkbox in AULab
				info.unit = kAudioUnitParameterUnit_Boolean;
			}
			else {
				info.unit = kAudioUnitParameterUnit_Generic;
			}
		}
	}

	return result;
}

ComponentResult AUMobius::GetParameterValueStrings(AudioUnitScope scope,
												   AudioUnitParameterID id,
												   CFArrayRef *outStrings)
{
	ComponentResult result = kAudioUnitErr_InvalidParameter;

	if (mTrace)
	  trace("AUMobius::GetParameterValueStrings %d\n", id);

	if (outStrings == NULL) {
		// examples do this, apparently required, see Tremolo example
		result = noErr;
	}
	else if (scope == kAudioUnitScope_Global) {

		PluginParameter* p = mPlugin->getParameter(id);
		if (p != NULL && p->getType() == PluginParameterEnumeration) {

			// require this to match the maximum or probe for NULL?
			const char** labels = p->getValueLabels();
			int range = p->getMaximum() - p->getMinimum() + 1;

			if (labels != NULL && range > 0) {
				
				CFStringRef* strings = new CFStringRef[range];

				for (int i = 0 ; i < range ;i++) {
					const char* label = labels[i];
					if (label == NULL) label = "???";
					strings[i] = MakeCFStringRef(label);
				}

				*outStrings = CFArrayCreate(NULL, // allocator
											(const void **) strings,
											range,
											NULL); // callbacks

				// !! example showed the CFArrayCreate call with a static 	
				// input array so we should need to free this?
				// probably need to free the CFStringRefs inside this too?!
				// spec is unclear
				delete strings;
				
				result = noErr;
			}
		}
    }

    return result;
}

/**
 * Called during construction to register the parameters the plugin supports.
 * These will always be in the global scope.
 * 
 * We have to call AUBase::SetParameter with the default values for each.  
 * Using AUEFFECTBase::SetParameter for convenience which always uses the
 * global scope.  The host then calls back to GetParameterInfo for more.
 *
 * Since the plugin may not be initialized to the point where it has
 * valid values for the parameters we call initParameters later during
 * Initialize() to set them to their true initial values.
 */
PRIVATE void AUMobius::declareParameters()
{
 	PluginParameter* params = mPlugin->getParameters();
	for (PluginParameter* p = params ; p != NULL ; p = p->getNext()) {
		if (mTrace)
		  trace("AUMobius::declareParameters %d %s %f\n", 
				p->getId(), p->getName(), p->getDefault());
		AUEffectBase::SetParameter(p->getId(), p->getDefault());
	}
}

/**
 * Called during Initialize() to set the parameters to the "real" values
 * rather than the defaults given in the constructor.  
 *
 * Note though that the host (in particular auval) may have changed parameters
 * between the constructor and Initialize() and we have to preserve those 
 * values or else auval will fail.  We detect this by checking to see if the
 * default parameter value is different than the current value.  Technically 
 * this isn't enough because the host could have set the parameter to the
 * default value (usually zero) which should then stick.  
 *
 * But for auval this should be enough since it doesn't use zero for the
 * "retain set value when Initialized" test. To do this right, we would have
 * to overload SetParameter and set a flag somewhere like the PluginParameter
 * to indiciate that it was set.
 * 
 */
PRIVATE void AUMobius::initParameters()
{
 	PluginParameter* params = mPlugin->getParameters();
	for (PluginParameter* p = params ; p != NULL ; p = p->getNext()) {

		// always call this to get mLast syncd up with the real initial value
		bool changed = p->refreshValue();

		Float32 current = GetParameter(p->getId());
		float dflt = p->getDefault();
		if (current == dflt) {
			// external value hasn't changed since constructing
			if (changed) {
				// but internal value changed
				if (mTrace)
				  trace("AUMobius::initParameter exporting %d %s %f\n", 
						p->getId(), p->getName(), p->getLast());
				exportParameter(p);
			}
		}
		else {
			// host set external value since constructing
			if (mTrace)
			  trace("AUMobius::initParameter importing %d %s %f\n", 
					p->getId(), p->getName(), current);
			p->setValueIfChanged(current);
		}
	}
}

/**
 * Called at the beginning of each render cycle to capture parameters
 * changed by the AU view.  Between render cycles the view will post
 * parameter change events.  These will be processed by AUBase, sometimes
 * slicing up an interrupt block so the events can be aligned on 
 * specific frames.
 * 
 */
PRIVATE void AUMobius::importParameters()
{
 	PluginParameter* params = mPlugin->getParameters();
	for (PluginParameter* p = params ; p != NULL ; p = p->getNext()) {
		Float32 value = GetParameter(p->getId());

		if (p->setValueIfChanged((float)value)) {
			// this can happen a LOT don't clutter the log
			if (mTraceParameters)
			  trace("AUMobius::importParameters %d %s %f\n", 
					p->getId(), p->getName(), value);
		}
	}
}

/**
 * Called at the end of each render cycle to tell the host about any parameter
 * changes made during the cycle. The host is notified of any parameter that
 * changed since the last export.
 *
 */
PRIVATE void AUMobius::exportParameters()
{
 	PluginParameter* params = mPlugin->getParameters();
	for (PluginParameter* p = params ; p != NULL ; p = p->getNext()) {
		if (p->refreshValue()) {

			// this can happen a LOT if the host is using parameter automation
			// don't clutter the log
			if (mTraceParameters)
			  trace("AUMobius::exportParameter %d %s %f\n", 
					p->getId(), p->getName(), p->getLast());

			exportParameter(p);
		}
	}
}

/**
 * Copy the value of a PluginParameter to the host and notify
 * the host of the change.
 */
PRIVATE void AUMobius::exportParameter(PluginParameter* p)
{
	Float32 value = (Float32)p->getLast();

	AUEffectBase::SetParameter(p->getId(), value);

	// Notify the host, the reference I could find for this
	// was in "Defining and Using Parameters" in the audio
	// unit programming guide.  Only example passed NULL for the
	// first two args.
	// Arg1 is "AUParameterListenerRef inSendingListener"
	// Arg2 is "void* inSendingObject"

	AudioUnitParameter msg;
	msg.mAudioUnit = (AudioUnit)GetComponentInstance();
	msg.mScope = kAudioUnitScope_Global;
	msg.mElement = 0;

	// AUCarbonViewControl sets this to _AnyParameter to update
	// all of them after a preset change
	msg.mParameterID = p->getId();

	OSStatus status = AUParameterListenerNotify(NULL, NULL, &msg);
	/*
	  if (status == kAudioUnitErr_InvalidParameter) {
	  // seems to happen regularly during startup?
	  }
	  else
	*/
	CheckStatus(status, "Problem with parameter notification");
}

//////////////////////////////////////////////////////////////////////
//
// AUMIDIBase
//
//////////////////////////////////////////////////////////////////////

/**
 * Send a MIDI event through to the abstract plugin.
 * NOTE: inStartFrame is currently ignored but in Bidule
 * it seems to always be zero or one anyway.
 */
OSStatus AUMobius::HandleMidiEvent(UInt8 status,
								   UInt8 channel,
								   UInt8 data1,
								   UInt8 data2,
								   long startFrame)
{
	mPlugin->midiEvent(status, channel, data1, data2, startFrame);
	return noErr;
}

//////////////////////////////////////////////////////////////////////
// 
// HostInterface
//
//////////////////////////////////////////////////////////////////////

PluginInterface* AUMobius::getPlugin()
{
	return mPlugin;
}

/**
 * Build an application context for the plugin.
 * This relies on the fact that MacContext is now defined
 * in Context.h and doesn't drag in any of the other qwin
 * stuff which conflicts with various things in CoreAudio
 * and Carbon.
 */
Context* AUMobius::getContext()
{
	if (mContext == NULL) {
		mContext = new MacContext(0, NULL);

		// The default getInstallationDirectory in MacContext
		// will use CFBundleGetMainBundle which will be the bundle
		// of the host, not Mobius.vst.  We're allowed to override this,
		// but the control flow feels messy.  I kind of like not having 
		// this kind of stuff buried in qwin, refactor someday when you're bored.
		// NOTE!! this is idential to VstMain

		CFStringRef cfbundleId = MakeCFStringRef(BUNDLE_ID);
		CFBundleRef bundle = CFBundleGetBundleWithIdentifier(cfbundleId);
		if (bundle != NULL) {
			static char path[PATH_MAX];
			strcpy(path, "");
			CFURLRef url = CFBundleCopyResourcesDirectoryURL(bundle);
			if (!CFURLGetFileSystemRepresentation(url, TRUE, (UInt8*)path, PATH_MAX)) {
				Trace(1, "Unable to get bundle Resources path!\n");
			}
			CFRelease(url);
			if (strlen(path) > 0)
			  mContext->setInstallationDirectory(CopyString(path));
		}
		else {
			// hmm, really shouldn't happen
			Trace(1, "Unable to locate bundle %s!\n", BUNDLE_ID);
		}

		// technically we shouldbe deferring this till start?
		MacInstall(mContext);
	}
	
	return mContext;
}

AudioInterface* AUMobius::getAudioInterface()
{
	return this;
}

const char* AUMobius::getHostName()
{
	return NULL;
}

const char* AUMobius::getHostVersion()
{
	return NULL;
}

/**
 * Who calls this?  If this is for the plugin to convey
 * parameter changes to the host we're doing that via the
 * PluginParameter interface now so we don't need this!!
 */
void AUMobius::notifyParameter(int id, float value)
{
}

//////////////////////////////////////////////////////////////////////
//
// AudioInterface
//
// Stubbed out implementation of AudioStream to pass to Mobius
// via the MobiusContext.  The only interesting thing for AU is
// the AudioStream class.
// 
//////////////////////////////////////////////////////////////////////

void AUMobius::terminate()
{
}

AudioDevice** AUMobius::getDevices()
{
	return NULL;
}

AudioDevice* AUMobius::getDevice(int id)
{
	return NULL;
}

AudioDevice* AUMobius::getDevice(const char* name, bool output)
{
	return NULL;
}

void AUMobius::printDevices()
{
}

AudioStream* AUMobius::getStream()
{
	return this;
}

//////////////////////////////////////////////////////////////////////
//
// AudioStream
//
//////////////////////////////////////////////////////////////////////

AudioInterface* AUMobius::getInterface()
{
	return this;
}

int AUMobius::getInputChannels()
{
	return getInputPorts() * PORT_CHANNELS;
}

int AUMobius::getInputPorts()
{
	return MAX_HOST_PLUGIN_PORTS;
}

int AUMobius::getOutputChannels()
{
	return getOutputPorts() * PORT_CHANNELS;
}

int AUMobius::getOutputPorts()
{
	return MAX_HOST_PLUGIN_PORTS;
}

bool AUMobius::setInputDevice(int id)
{
	// have to implement these but they have no effect
	return true;
}

bool AUMobius::setInputDevice(const char* name)
{
	return true;
}

bool AUMobius::setOutputDevice(int id)
{
	return true;
}

bool AUMobius::setOutputDevice(const char* name)
{
	return true;
}

void AUMobius::setSuggestedLatencyMsec(int i)
{
}

/**
 * !! Could fake up a device to represent the AU/VST ports?
 */
AudioDevice* AUMobius::getInputDevice() 
{
	return NULL;
}

AudioDevice* AUMobius::getOutputDevice() 
{
	return NULL;
}

int AUMobius::getSampleRate() 
{
	return mSampleRate;
}

void AUMobius::setSampleRate(int rate)
{
	// can't be set
}

AudioHandler* AUMobius::getHandler()
{
	return mHandler;
}

void AUMobius::setHandler(AudioHandler* h)
{
	mHandler = h;
}

const char* AUMobius::getLastError()
{
	//return mUnit->getLastError();
	return NULL;
}

bool AUMobius::open()
{
	return true;
}

int AUMobius::getInputLatencyFrames()
{
	return mInputLatency;
}

void AUMobius::setInputLatencyFrames(int frames)
{
	if (frames > 0)
	  mInputLatency = frames;
	else
	  mInputLatency = 512;
}

int AUMobius::getOutputLatencyFrames()
{
    return mOutputLatency;
}

void AUMobius::setOutputLatencyFrames(int frames)
{
	if (frames > 0)
	  mOutputLatency = frames;
	else
	  mOutputLatency = 512;
}

void AUMobius::close()
{
	printStatistics();
}

void AUMobius::printStatistics()
{
}

//
// Buffer Processing
//

PUBLIC long AUMobius::getInterruptFrames()
{
	return mInterruptSliceFrames;
}

PUBLIC AudioTime* AUMobius::getTime()
{
	return &mTime;
}

//
// Stream Time
// This was added to debug some things in the Windows VST, not
// sure if they're relevant for AU
//

PUBLIC double AUMobius::getStreamTime()
{
	return 0.0;
}

PUBLIC double AUMobius::getLastInterruptStreamTime()
{
	return 0.0;
}

//////////////////////////////////////////////////////////////////////
//
// View
//
//////////////////////////////////////////////////////////////////////

AUMobiusView::AUMobiusView(AudioUnitCarbonView auv) : AUCarbonViewBase(auv)
{
	mTrace = true;
	// let framework eventually call CreateUI
}

AUMobiusView::~AUMobiusView()
{
	AUMobius* unit = getAUMobius();
	PluginInterface* plugin = unit->getPlugin();
	plugin->closeWindow();
}

/**
 * Private property hack to get to the AUBase from the view.
 * Supposedly there's also a RefCon on the ComponentInstance 
 * but I couldn't find the right incantations.  Custom properties
 * are relatively clean.
 */
AUMobius* AUMobiusView::getAUMobius()
{
	void* pluginAddr;
	UInt32 dataSize = sizeof(pluginAddr);

	ComponentResult err = AudioUnitGetProperty(mEditAudioUnit,
											   kAUMobiusProperty_AUBase,
											   kAudioUnitScope_Global,
											   0,
											   &pluginAddr,
											   &dataSize);

	return (AUMobius*)pluginAddr;
}

OSStatus AUMobiusView::CreateUI(Float32 xoffset, Float32 yoffset)
{
	AUMobius* unit = getAUMobius();
	PluginInterface* plugin = unit->getPlugin();
	plugin->openWindow(mCarbonWindow, mCarbonPane);

	return noErr;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

