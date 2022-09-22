/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * A VST plugin base class that provides trace and other common
 * services.  This is not specific to Mobius, consider moving this to vst?
 *
 * AudioEffect has these member fields:
 * 
 *   float sampleRate;
 *   AEffEditor *editor;
 *   audioMasterCallback audioMaster;
 *   long numPrograms;
 *   long numParams;
 *   long curProgram;
 *   long blockSize;
 *   AEffect cEffect;
 * 
 * The API methods can be divided into these categories:
 *
 *   PLUGIN DEFINITION
 *
 *    Methods called by the plugin to define itself to the host.
 *    Most of these set fields in the Aeffect structure.
 *
 *   HOST QUERY
 *
 *    Method called by the plugin to find out things about the host.
 * 
 *   HOST CONTROL
 *
 *     Methods called by the plugin to request services of the host.
 *
 *   PLUGIN CONTROL
 *
 *     Methods called by the host to tell the plugin to do something
 *     or find out about plugin status.
 *
 *   TOOLS
 *
 *     Utility methods the plugin may do to perform common
 *     computations or operations.
 *
 * You normally only overload PLUGIN CONTROL methods.
 *
 * PLUGIN DEFINITION
 *
 *   VST 1.0 
 * 
 *   setEditor
 *     Set if the plugin exposes an editor that can be controlled
 *     by the host.
 *   setUniqueID
 *     A 4 byte value, usually defined as 4 characters in a long
 *     The Steinberg site supposedly has a registry of ids
 *   setNumInputs
 *   setNumOutputs
 *     Set the number of "pins" the plugin supports.  Once set
 *     this cannot be changed without creating a new instance
 *     of the plugin.
 *   hasVu
 *     Doc says "return vu value in getVu(); > 1 means clipped
 *     I guess this means that you can call getVu and expect
 *     a meaningful value.
 *   hasClip
 *     Doc says "return > 1 in getVu if clipped
 *     I guess this means that you can call getVu and expect
 *     a meaningful value, and that it uses the > 1 clipping
 *     convention.
 *   canMono
 *     Tells the host the plugin can function as a 1 in 2 out device.
 *     When the host as a mono effect send and a stereo return, it can
 *     check this flag to decide whether to add this plugin to a list being
 *     displayed for this purpose.
 *   canProcessReplacing
 *     Indicates that the processReplacing can be called.
 *   setRealtimeQualities
 *     Doc says "number of realtime qualities (0: realtime)"
 *   setOfflineQualities
 *     Doc says "number of offline qualities (0: realtime only)"
 *   setInitialDelay
 *     Used to report the plugin's latency (group delay)
 *     or as the docs so charmingly phrase it
 *     "For algorithms which need input in the first place"
 *   programsAreChunks
 *     Doc says "Program data are handled in formatless chunks"
 *     whatever that means...
 *
 * HOST QUERY/HOST CONTROL
 *
 *   VST 1.0
 *
 *   setParameterAutomated	 	
 *     called when a parameter is changed in the editor, to tell the
 *     host to record it for autmoation
 *   getMasterVersion
 *     return the VST version supported by the host (2200 for 2.2)
 *   getCurrentUniqueId
 *     code comments: return the unique id of a plug that's currently loading
 *   masterIdle
 *     no return value, code comments say it will call the application
 *     idle routine which will call effEditIdle for all open editors)
 *   isInputConnected
 *   isOutputConnected
 *     returns true if the "pin" is being used, some variance in what
 *     different hosts mean by this
 *
 *   VST 2.0
 *
 *   canHostDo(char*)
 *     ask if a host implements a feature identified by name
 *     names defined by hostCanDos enumeration (e.g. sendVstEvents,
 *     receiveVstMidiEvent, acceptIOChanges)
 *   closeWindow
 *   openWindow
 *     open/closes a VstWindow, probably used in the implementation
 *     of a portable editor?
 *   ioChanged
 *     tell host the number of input or output pins has changed,
 *     host may suspend/resume the plug, there is a lot of discussion
 *     of this on the forum, best to avoid
 *   getTimeInfo
 *     returns a VstTimeInfo which has a lot of things: samplePos,
 *     sampleRate, ppqPos, tempo, etc.
 *   sendVstEventsToHost
 *     obscure, "See discussion of VstEvents, and Steinberg products
 *     implementation issues"
 *   needIdle
 *     tell host the plug needs idle calls (outside it's editor window)
 *     some plugs need idle calls even when editor is closed
 *   sizeWindow
 *     ask host to resize the editor window
 *   updateSampleRate
 *     get the current sample rate the host (not really an update!)
 *   updateBlockSize
 *     get the current block size from the host
 *   getAutomationState
 *     values: not supported, off, read, write, read/write
 *   getCurrentProcessLevel
 *     info about the plug's execution context: not supported,
 *     in GUI thread, in audio thread, in sequencer thread,
 *     offline processing
 *   getDirectory
 *     return plug's containing directory, value is FSSpec on Mac or char*
 *   getHostProductString
 *      string identifying host's name, up to 64 chars
 *   getHostVendorVersion
 *     vendor-specific version as a long
 *   getHostVendorString
 *     string identifying host's vendor
 *   getHostLanguage
 *     return a VstHostLanguage value, english, german, french, etc.
 *   getInputLatency
 *   getOutputLatency
 *     return the ASIO latency values, presumably a frame count?
 *   getNextPlug
 *   getPreviousPlug
 *     comments say "for future expansion", returns an AEffect*
 *     for a given pin index
 *   getNumAutomatableParameters
 *     return the maximum number of automatable parameters
 *   getParameterQuantization
 *      obscurely specified as: Parameter index in <value> (-1: all, any).
 *      When the application stores and restores automation data, or when a
 *      user control (like a fader or knob) is quantized to integer values, 
 *      there is some quantization applied related to the floating point value.
 *   getSpeakerArrangement
 *     returns two VstSpeakerArrangement structure for the input and output
 *   hasExternalBuffer
 *     docs: Notifies the host that the plug uses an external buffer, 
 *     as external dsp, may have their own output buffer (32 bit float). 
 *     The host then requests this via effGetDestinationBuffer. For dma access
 *     it may be more efficient for a plug with external dsp to provide its own buffers.
 *   hostVendorSpecific
 *     no definition, takes a bunch of generic args
 *   isSynth
 *     tell the host the plug is an instrument, i.e. that it will call the 
 *     wantEvents method
 *   noTail
 *     when arg is true means that host can stop calling process() if there
 *     is no data, when false process() is always called
 *   offlineGetCurrentMetaPass
 *   offlineGetCurrentPass
 *   offlineRead
 *   offlineStart
 *   offlineWrite
 *     all related to offline processing
 *   setOutputSampleRate
 *     docs: used for variable I/O processing
 *     unclear when this would be called
 *   tempoAt
 *     calculates the tempo at a position specified as a sample frame,
 *     may cause heavy calculations, presumably used in the presence
 *     of tempo automation
 *   updateDisplay
 *     indicates that something has changed and host needs
 *     to update "multi-fx" display
 *   wantAsyncOperation
 *     tell host we want to operate asynchronously, process() returns immediately
 *     host will poll with reportCurrentPosition,
 *     this is for plugins that whose processing is actually carried out by 
 *     external DSPs
 *   wantEvents
 *     inform the host we want to receive events, this must be called
 *     in the resume() method
 *   willProcessReplacing
 *     returns indiciation of whether host will call the processReplacing method
 * 
 *   VST 2.1
 *
 *   beginEdit
 *   endEdit
 *     called around a setParameterAutomated with mouse motion
 *   openFileSelector
 *     open a host file specified with a VstFileSelect object
 *
 *   VST 2.2
 *  
 *   closeFileSelector
 *     close a VstFileSelect
 *   getChunkFile
 *     not sure, seems to return a chunk file path
 * 
 *   VST 2.3
 *
 *   setPanLaw
 *     set panning law used by host: linear, equal power
 *   beginLoadBank
 *     called before a bank is loaded, passed VstPathChunkInfo
 *   beginLoadProgram
 *     called before a program is loaded, VstPathChunkInfo
 *
 * PLUGIN CONTROL
 *
 *   VST 1.0
 *
 *   getAeffect?
 *     Called by host to look at bits set by the plugin definition methods.
 *   setParameter
 *   getParameter
 *   getParameterLabel
 *   getParameterDisplay
 *   getParameterName
 *     Host calls these only if using a default UI?  "label" is a 
 *     units annotation like "dB", "Khz", etc.  "display" is the value.
 *   setProgram
 *     change program
 *   getProgram
 *     return the current progream
 *   setProgramName
 *     set the current program name
 *   getProgramName
 *   process
 *   processReplacing
 *     the primary audio processing methods
 *   dispatcher
 *     a lot of magic happens in here, but seems to be mostly internal?
 *   open
 *   close
 *     presumably when the plugin is first opened and closed but may
 *     be suspended and resumed while open?
 *   suspend
 *     called "when the effect is turned off" 
 *   resume
 *     called "when the effect is turned on" 
 *   getVu
 *     presumably to return some sort of metering value, valid only
 *     if hasVu was called?
 *   getChunk
 *   setChunk
 *     not sure, presumably only valid if programsAreChunks was called?
 *   setSampleRate
 *   setBlockSize
 *     called by the host, when?
 *
 *   VST 2.0
 *
 *   canDo(char* text)
 *     to "report what the plug is able to do", not required but recommended
 *     the plugCanDos array defines the possible strings, such things
 *     as "sendVstEvents", "offline", etc.
 *   canParameterBeAutomated
 *     return true if the host can automate changes to the parameter
 *   copyProgram(long destination)
 *     copy currently selected program to the destination
 *   fxIdle
 *     no documentation
 *   getChannelParameter
 *     for internal use of soft synth voices, as voice may be
 *     channel dependent
 *   getNumCategories
 *     if programs are divided into groups like general MIDI, return > 1
 *   getProgramNameIndexed(category, index, text)
 *     get program name in category
 *   getInputProperties
 *   getOutputProperties
 *     fill in a VstPinProperties structure for each "pin"
 *   getIcon
 *     not yet defined
 *   getEffectName
 *     a string identifying the object
 *   getErrorText
 *     presumably the last error message
 *   getTailSize
 *     "return after what tiemn the response to an input sample will have
 *      die to zero", the application may decide not to call the process
 *      method if there is no input after the tail, for looping return 0
 *      to force the process methods to be called always
 *   getParameterProperties
 *     fill a VstParameterProperties object with info about a parameter
 *   getPlugCategory
 *     return a VstPlugCategory describing what we are, probably "effect"
 *   getProductString
 *     text identifying the prouduct name (e.g. Mobius)
 *   getVendorString
 *     text identifying the vendor (e.g. Jeffrey S. Larson)
 *   getVendorVersion
 *     text identifying the version (e.g. 1 beta 3)
 *   getVstVersion
 *     the vst version we support, 2
 *   inputConnected(index, state)
 *   outputConnected(index, state)
 *     host tells us the status of the pins
 *   keysRequired
 *     true if "keys are needed", not sure what that means, this was the
 *     default for 1.0 but they're not required in 2.0
 *   offlineNotify
 *   offlineGetNumPasses
 *   offlineGetNumMetaPasses
 *   offlinePrepare
 *   offlineRun
 *     I think we can ignore these as long as we're not an offline plug
 *   processEvents
 *     called in response to wantEvents call, process a block of events
 *     events types are midi, audio, video, parameter, trigger
 *   processVariableIo
 *     obscure, no documentation
 *   reportCurrentPosition
 *   reportDestinationBuffer
 *     return how much data has been processed in an external buffer,
 *     used only with async operations started by wantAsyncOperation
 *   setBlockSizeAndSampleRate
 *     seems to be the same as setBlockSize and setSampleRate
 *   setBypass
 *     for "soft bypass", process() is still called, called only if
 *     canDo("bypass") returned true
 *   setSpeakerArrangement
 *     for surround sound
 *   setViewPosition(x, y)
 *     set view position in window (editor?)
 *   string2parameter
 *     convert a string to a parameter value, used with plugins with
 *     no user interface
 *   vendorSpecific
 *     no definition
 * 
 *   VST 2.1
 *
 *   getMidiProgramName
 *   getCurrentMidiProgram
 *   getMidiProgramCategory
 *   hasMidiProgramsChanged
 *   getMidiKeyName
 *     a more complicated program interface, only for synths
 *   beginSetProgram
 *   endSetProgram
 *     called before/after a program is loaded
 *
 *   VST 2.3
 *
 *   setTotalSampleToProcess
 *     offline operation
 *   getNextShellPlugin
 *     only if type is kPlugCategShell
 *   startProcess
 *   stopProcess
 *     called around a process() call, not sure why this would be useful
 *
 *  TOOLS
 *
 *   VST 1.0
 *
 *   getSampleRate
 *   getBlockSize
 *     Accessors for the values the host will set by calling
 *     setSampleRate and setBlockSize
 * 
 *   db2string
 *   Hz2string
 *   ms2string
 *   float2string
 *   long2string
 *     various string converters
 * 
 *   VST 2.3
 *   allocateArrangement
 *   deallocateArrangement
 *      allocate/free memory for a VstSpeakerArrangement object
 *   copySpeaker
 *      copy one VstSpeakerProperties object to another
 *   matchArrangement
 *      another VstSpeakerProperites util, not sure
 *   
 * --------------------------------------------------------------------------------
 *
 * Relevant Functions
 *
 *
 * PLUGIN DEFINITION
 *
 *   VST 1.0 
 * 
 *   setUniqueID
 *   setNumInputs
 *   setNumOutputs
 *   canMono
 *   canProcessReplacing
 *   setRealtimeQualities
 *   setOfflineQualities
 *
 * HOST QUERY/HOST CONTROL
 *
 *   getMasterVersion
 *   getCurrentUniqueId
 *   masterIdle?
 *   canHostDo(char*)
 *   getTimeInfo
 *   sendVstEventsToHost
 *   needIdle
 *   getDirectory
 *   getHostProductString
 *   getHostVendorVersion
 *   getHostVendorString
 *   getInputLatency
 *   getOutputLatency
 *   noTail
 *   wantEvents
 *   willProcessReplacing
 *
 * PLUGIN CONTROL
 *
 *   getAeffect?
 *   setParameter
 *   getParameter
 *   getParameterLabel
 *   getParameterDisplay
 *   getParameterName
 *   setProgram
 *   getProgram
 *   setProgramName
 *   getProgramName
 *   process
 *   processReplacing
 *   dispatcher
 *   open
 *   close
 *   suspend
 *   resume
 *   setSampleRate
 *   setBlockSize
 *
 *   VST 2.0
 *
 *   canDo(char* text)
 *   canParameterBeAutomated
 *   fxIdle
 *   getInputProperties
 *   getOutputProperties
 *   getEffectName
 *   getErrorText
 *   getTailSize
 *   getPlugCategory
 *   getProductString
 *   getVendorString
 *   getVendorVersion
 *   getVstVersion
 *   inputConnected(index, state)
 *   outputConnected(index, state)
 *   keysRequired
 *   processEvents
 *   setBlockSizeAndSampleRate
 *   setBypass
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include "Trace.h"
#include "Util.h"

#include "VstPlugin.h"

/****************************************************************************
 *                                                                          *
 *   								PLUGIN                                  *
 *                                                                          *
 ****************************************************************************/

/**
 * Second and third args to AudioEffectX constructor are 
 * kNumPrograms, kNumParams.
 */
VstPlugin::VstPlugin (audioMasterCallback audioMaster, int progs, int params)
	: AudioEffectX (audioMaster, progs, params) 
{
	mTrace = true;

	strcpy(mHostVendor, "");
	strcpy(mHostProduct, "");
    strcpy(mHostVersion, "");

	getHostVendorString(mHostVendor);
	getHostProductString(mHostProduct);

    VstInt32 version = getHostVendorVersion();
    sprintf(mHostVersion, "%d", (int)version);

	trace("VstPlugin: Host vendor %s product %s version %s\n",
		  mHostVendor, mHostProduct, mHostVersion);

}

VstPlugin::~VstPlugin ()
{
}

/**
 * EXTENSION: Set the parameter count after construction.
 * This may not work but worth a shot.
 */
void VstPlugin::setParameterCount(int i)
{
    numParams = i;
    getAeffect()->numParams = i;
}

/**
 * EXTENSION: Set the program count after construction.
 */
void VstPlugin::setProgramCount(int i)
{
    numPrograms = i;
    getAeffect()->numPrograms = i;
}

/****************************************************************************
 *                                                                          *
 *   							 AUDIO EFFECT                               *
 *                                                                          *
 ****************************************************************************/

//
// Parameters
//

void VstPlugin::setParameter(VstInt32 index, float value)
{
	if (mTrace)
	  trace("VstPlugin::setParameter %ld %f\n", index, value);
}

float VstPlugin::getParameter(VstInt32 index)
{
	if (mTrace)
	  trace("VstPlugin::getParameter %ld\n", index);
	return 0.0f;
}

/**
 * Return a "units" qualifier such as "db", "sec", etc.
 */
void VstPlugin::getParameterLabel(VstInt32 index, char *label) 
{
	if (mTrace)
	  trace("VstPlugin::getParameterLabel %ld\n", index);
	
	AudioEffectX::getParameterLabel(index, label);
}

/**
 * Return the parameter value as a string
 */
void VstPlugin::getParameterDisplay(VstInt32 index, char *text)
{ 
	if (mTrace)
	  trace("VstPlugin::getParameterDisplay %ld\n", index);
	
	AudioEffectX::getParameterDisplay(index, text);
}

void VstPlugin::getParameterName(VstInt32 index, char *text)
{ 
	if (mTrace)
	  trace("VstPlugin::getParameterName %ld\n", index);
	
	AudioEffectX::getParameterName(index, text);
}
	
//
// Programs
// These are sets of parameter values.
// Banks are collections of programs.
//

VstInt32 VstPlugin::getProgram () 
{ 
    // avoid trace, Reaper calls this about once a second
	//if (mTrace)
    //trace("VstPlugin::getProgram\n");

	return AudioEffectX::getProgram();
}

void VstPlugin::setProgram (VstInt32 program) 
{ 
	if (mTrace)
	  trace("VstPlugin::setProgram %ld\n", program);

	AudioEffectX::setProgram(program);
}

void VstPlugin::setProgramName(char* name)
{ 
	if (mTrace)
	  trace("VstPlugin::setProgramName %s\n", name);

	AudioEffectX::setProgramName(name);
}

/**
 * Docs indicate that most string buffers are 24 characters
 * so be careful with long names!
 */
void VstPlugin::getProgramName(char* name)
{ 
	if (mTrace)
	  trace("VstPlugin::getProgramName\n");
	
	AudioEffectX::getProgramName(name);
}

//
// Called from audio master (Host -> Plug)
//

void VstPlugin::process(float** inputs, float** outputs, VstInt32 sampleFrames)
{
	if (mTrace)
	  trace("VstPlugin::process\n");
}

void VstPlugin::processReplacing(float** inputs, float** outputs, 
								 VstInt32 sampleFrames)
{
	if (mTrace)
	  trace("VstPlugin::processReplacing\n");
}

/**
 * opcode dispatcher, this one looks interesting
 */
VstInt32 VstPlugin::dispatcher(VstInt32 opCode, VstInt32 index, VstInt32 value, void *ptr, 
						   float opt)
{
	// doesn't seem to be a need to trace this, it just ends up
	// calling another method
	//if (mTrace)
	//trace("VstPlugin::dispatcher %ld %ld %ld %p %f\n",
	//opCode, index, value, ptr, opt);

	return AudioEffectX::dispatcher(opCode, index, value, ptr, opt);
}

/**
 * Called when the plugin is initialized
 */
void VstPlugin::open()
{
	if (mTrace)
	  trace("VstPlugin::open\n");
}

/**
 * Called when the plugin will be released
 */
void VstPlugin::close()
{
	if (mTrace)
	  trace("VstPlugin::close\n");
}

/**
 * Called when the plugin is "switched to off"
 * Example uses this to flush an internal buffer so that
 * so "pending data won't sound when the effect is
 * switched back on".
 */
void VstPlugin::suspend()
{
	if (mTrace)
	  trace("VstPlugin::suspend\n");
}

/**
 * Called when the plugin is "switch to On"
 */
void VstPlugin::resume()
{
	if (mTrace)
	  trace("VstPlugin::resume\n");
}

/**
 * Not sure what this does, but the name suggests
 * it is for a VU meter of some kind.
 */
float VstPlugin::getVu()
{
	if (mTrace)
	  trace("VstPlugin::getVu\n");
	return 0.0f;
}

/**
 * Code comments say "Returns the size in bytes of the chunk
 * (Plugin allocates the data array)"
 */
VstInt32 VstPlugin::getChunk(void** data, bool isPreset)
{ 
	if (mTrace)
	  trace("VstPlugin::getChunk %p %d\n", data, isPreset);

	// this just returns zero
	return AudioEffectX::getChunk(data, isPreset);
}

VstInt32 VstPlugin::setChunk(void* data, VstInt32 byteSize, bool isPreset) 
{ 
	if (mTrace)
	  trace("VstPlugin::setChunk %p %ld %d\n", data, byteSize, isPreset);

	// this just returns zero
	return AudioEffectX::setChunk(data, byteSize, isPreset);
}
	
void VstPlugin::setSampleRate(float sampleRate)
{ 
	if (mTrace)
	  trace("VstPlugin::setSampleRate %f\n", sampleRate);

	// this stores it in a member field
	AudioEffectX::setSampleRate(sampleRate);
}

/**
 * Called by the host with the maximum block size that
 * will be passed to the two process methods.  
 */
void VstPlugin::setBlockSize(VstInt32 blockSize)
{ 
	if (mTrace)
	  trace("VstPlugin::setBlockSize %ld\n", blockSize);

	// this stores it in a member field
	AudioEffectX::setBlockSize(blockSize);
}

/****************************************************************************
 *                                                                          *
 *   							AUDIO EFFECT X                              *
 *                                                                          *
 ****************************************************************************/

AEffEditor* VstPlugin::getEditor () 
{
	if (mTrace)
	  trace("VstPlugin::getEditor\n");

	return AudioEffectX::getEditor();
}

VstInt32 VstPlugin::canDo(char* text) 
{
	if (mTrace)
	  trace("VstPlugin::canDo %s\n", text);

	// !! todo , look at the plugCanDos list 
	return 0;
}


//
// Events & Time
//

static bool receivingClocks = false;
static bool receivingSense = false;

/**
 * Returning 0 emans "want no more", else return 1.
 * See aeffectx.h for the definition of VstEvents and VstMidiEvents
 */
VstInt32 VstPlugin::processEvents(VstEvents* events)
{ 
	if (mTrace) {
		for (int i = 0 ; i < events->numEvents ; i++) {
			VstEvent* e = events->events[i];
			if (e->type == kVstMidiType) {
				VstMidiEvent* me = (VstMidiEvent*)e;
				int byte1 = me->midiData[0] & 0xFF;
				if (byte1 == 0xF8) {
					if (!receivingClocks) {
						trace("VstPlugin::processEvents receiving clocks\n");
						receivingClocks = true;
					}
				}
				else if (byte1 == 0xFE) {
					if (!receivingSense) {
						trace("VstPlugin::processEvents receiving active sensing\n");
						receivingSense = true;
					}
				}
				else {
					trace("VstPlugin::processEvents midi\n");
					trace("%x %x %x\n", (int)(me->midiData[0] & 0xFF),
						  (int)(me->midiData[1] & 0xFF),
						  (int)(me->midiData[2] & 0xFF));
#if 0
					if (me->deltaFrames != 0 || me->noteLength != 0 ||
						me->noteOffset != 0) {
						trace("  deltaFrames=%ld noteLength=%ld noteOffset=%ld detune=%d noteOffVelocity=%d\n",
							  me->deltaFrames, me->noteLength, me->noteOffset,
							  (int)me->detune, (int)me->noteOffVelocity);
					}
#endif
				}
			}

// no longer defined in 2.4?
#ifdef VST_2_1
			else if (e->type == kVstAudioType) {
				trace("VstPlugin::processEvents audio\n");
				trace("  byteSize=%ld deltaFrames=%ld\n", 
					  e->byteSize, e->deltaFrames);
			}
			else if (e->type == kVstVideoType) {
				trace("VstPlugin::processEvents video\n");
				trace("  byteSize=%ld deltaFrames=%ld\n", 
					  e->byteSize, e->deltaFrames);
			}
			else if (e->type == kVstParameterType) {
				trace("VstPlugin::processEvents parameter\n");
				trace("  byteSize=%ld deltaFrames=%ld\n", 
					  e->byteSize, e->deltaFrames);
			}
			else if (e->type == kVstTriggerType) {
				trace("VstPlugin::processEvents trigger\n");
				trace("  byteSize=%ld deltaFrames=%ld\n", 
					  e->byteSize, e->deltaFrames);
			}
#endif
			else {
				trace("VstPlugin::processEvents unknown\n");
			}
		}
	}

	// return value is undocumented
	return 1;
}

//
// Parameters and Programs
//

/**
 * Return true if the parameter can be automated, if false I'm guessing
 * that the host won't bother recording changes.
 */
bool VstPlugin::canParameterBeAutomated(VstInt32 index)
{ 
	if (mTrace)
	  trace("VstPlugin::canParameterBeAutomated %ld\n", index);
	return true; 
}

/**
 * Convert a string representation of a parameter to a value,
 * can be used by the host to provide a crude UI if the plugin
 * doesn't have one.  This implies a call to setParameter
 * after the conversion.
 *
 * Return true on success.  Code comments indiciate "text == 0
 * is to be expected to check the capability (returns true)"
 * which I guess means if text is null, but we support
 * setting parameters by string, return true.
 */
bool VstPlugin::string2parameter(VstInt32 index, char* text)
{ 
	if (mTrace)
	  trace("VstPlugin::string2parameter %ld %s\n", index, text);
	return false; 
}

/**
 * Get a channel specific parameter?  Docs say "for internal
 * use of soft synth voices, as a voice may be channel dependent".
 */
float VstPlugin::getChannelParameter(VstInt32 channel, VstInt32 index)
{ 
	if (mTrace)
	  trace("VstPlugin::getChannelParameter %ld %ld\n", channel, index);
	return 0.f; 
}

/**
 * Return more than 1 if you support more than one category.
 * Docs say "i.e. if programs are divided into groups like General MIDI"
 */
VstInt32 VstPlugin::getNumCategories()
{ 
	if (mTrace)
	  trace("VstPlugin::getNumCategories\n");
	return 1L; 
}

/**
 * Allows host app to list programs in more than one category.
 * If category is -1, "program indicies are enumerated linearly
 * otherwise each category starts over with index 0"
 * Do not return more than 24. 
 *
 * Not sure how you're supposed to select a program by category.
 */
bool VstPlugin::getProgramNameIndexed(VstInt32 category, VstInt32 index, char* text)
{ 
	if (mTrace)
	  trace("VstPlugin::getProgramNameIndexed %ld %ld\n", 
			 category, index);
	return false; 
}

/**
 * Copy the currently selected program to the program destination
 */
bool VstPlugin::copyProgram(VstInt32 destination)
{
	if (mTrace)
	  trace("VstPlugin::copyProgram %ld\n", destination);
	return false; 
}

/**
 * Called before a program is loaded.
 * Return true if "the plug took the notification into account"
 */
bool  VstPlugin::beginSetProgram()
{
	if (mTrace)
	  trace("VstPlugin::beginSetProgram\n");
	return false;
}

/**
 * Called after a program is loaded
 * Return true if "the plug took the notification into account"
 */
bool  VstPlugin::endSetProgram()
{
	if (mTrace)
	  trace("VstPlugin::endSetProgram\n");
	return false;
}

//
// Connections, Configuration
//

/**
 * Called when an input is connected or disconnected.
 */
void VstPlugin::inputConnected(VstInt32 index, bool state)
{
	if (mTrace)
	  trace("VstPlugin::inputConnected %ld %d\n", index, state);
}

void VstPlugin::outputConnected(VstInt32 index, bool state)
{
	if (mTrace)
	  trace("VstPlugin::outputConnected %ld %d\n", index, state);
}

/**
 * Place information about the "pin" in the properties object.
 * It is recommended that this be supported.  If it is return true.
 */
bool VstPlugin::getInputProperties(VstInt32 index, VstPinProperties* properties)
{ 
	if (mTrace)
	  trace("VstPlugin::getInputProperties %ld\n", index);
	return false; 
}

bool VstPlugin::getOutputProperties(VstInt32 index, VstPinProperties* properties)
{ 
	if (mTrace)
	  trace("VstPlugin::getOutputProperties %ld\n", index);
	return false; 
}

/**
 * The inherited implementation checks effFlagIsSynt flag
 * and returns kPlugCategSynth or kPlugCategUnknown
 */
VstPlugCategory VstPlugin::getPlugCategory ()
{ 

	// avoid the trace, Reaper calls this on *every* interrupt
	//if (mTrace)
	//trace("VstPlugin::getPlugCategory\n");

	return AudioEffectX::getPlugCategory();
}

//
// Realtime
//

VstInt32 VstPlugin::reportCurrentPosition()
{
	if (mTrace)
	  trace("VstPlugin::reportCurrentPosition\n");

	return 0;
}

float* VstPlugin::reportDestinationBuffer()
{
	if (mTrace)
	  trace("VstPlugin::reportDestinationBuffer\n");

	return 0;
}

//
// Other
//

bool VstPlugin::processVariableIo(VstVariableIo* varIo)
{ 
	if (mTrace)
	  trace("VstPlugin::processVariableIo\n");

	return false;
}

bool VstPlugin::setSpeakerArrangement(VstSpeakerArrangement* pluginInput,
									  VstSpeakerArrangement* pluginOutput)
{
	if (mTrace)
	  trace("VstPlugin::setSpeakerArrangement\n");

	return false; 
}

bool VstPlugin::getSpeakerArrangement(VstSpeakerArrangement** pluginInput, 
									  VstSpeakerArrangement** pluginOutput)
{ 
	if (mTrace)
	  trace("VstPlugin::getSpeakerArrangement\n");

	*pluginInput = 0; 
	*pluginOutput = 0; 
	return false; 
}

void VstPlugin::setBlockSizeAndSampleRate(VstInt32 blockSize, float sampleRate)
{
	if (mTrace)
	  trace("VstPlugin::setBlockSizeAndSampleRate %ld %ld\n", 
			 blockSize, sampleRate);

	this->blockSize = blockSize; 
	this->sampleRate = sampleRate; 
}

/**
 * for "soft bypass", process() is still called, called only if
 * canDo("bypass") returned true
 */
bool VstPlugin::setBypass(bool onOff) 
{
	if (mTrace)
	  trace("VstPlugin::setBypass %d\n", onOff);

	return false; 
}

bool VstPlugin::getEffectName(char* name)
{ 
	if (mTrace)
	  trace("VstPlugin::getEffectName\n");

	strcpy(name, "VstPlugin");

	return false;
}

bool VstPlugin::getErrorText(char* text)
{ 
	if (mTrace)
	  trace("VstPlugin::getErrorText");
	strcpy(text, "");
	return false;
}

bool VstPlugin::getVendorString(char* text)
{ 
	if (mTrace)
	  trace("VstPlugin::getVendorString\n");

	return false; 
}

bool VstPlugin::getProductString(char* text)
{ 
	if (mTrace)
	  trace("VstPlugin::getProductString\n");

	return false; 
}

VstInt32 VstPlugin::getVendorVersion() 
{
	if (mTrace)
	  trace("VstPlugin::getVendorVersion\n");

	return 0;
}

VstInt32 VstPlugin::vendorSpecific(VstInt32 lArg, VstInt32 lArg2, void* ptrArg, 
							   float floatArg) 
{ 
	if (mTrace)
	  trace("VstPlugin::vendorSpecific %ld %ld %p %f\n",
			 lArg, lArg2, ptrArg, floatArg);

	return 0; 
}

void* VstPlugin::getIcon() 
{ 
	if (mTrace)
	  trace("VstPlugin::getIcon\n");

	return 0; 
}

bool VstPlugin::setViewPosition(VstInt32 x, VstInt32 y)
{ 
	if (mTrace)
	  trace("VstPlugin::setViewPosition %ld %ld\n", x, y);

	return false; 
}

/**
 * "return after what tiemn the response to an input sample will have
 * die to zero", the application may decide not to call the process
 * method if there is no input after the tail, for looping return 0
 * to force the process methods to be called always
 */
VstInt32 VstPlugin::getTailSize()
{ 
	if (mTrace)
	  trace("VstPlugin::getTailSize\n");

	return 0; 
}

/**
 * This seems to be called a lot by some hosts (EnergyXT)
 */
VstInt32 VstPlugin::fxIdle()
{ 
	//if (mTrace)
	//trace("VstPlugin::fxIdle\n");

	return 0; 
}

bool VstPlugin::getParameterProperties(VstInt32 index, VstParameterProperties* p)
{ 
	if (mTrace)
	  trace("VstPlugin::getParameterProperties %ld\n", index);
	return false; 
}

bool VstPlugin::keysRequired()
{ 
	if (mTrace)
	  trace("VstPlugin::keysRequired\n");
	return false;
}

VstInt32 VstPlugin::getVstVersion()
{ 
	if (mTrace)
	  trace("VstPlugin::getVersion\n");

	// Returns the current VST Version
	// 2 apparently means 2.0
	// 2300 means 2.3?
	// claiming 2.3 makes Live use startProcess/stopProcess which
	// caused problems, don't remember why

	return 2;
}

//
// MIDI program names
// 

/**
 * Struct will be filled with information for 'thisProgramIndex'.
 * returns number of used programIndexes.
 * If 0 is returned, no MidiProgramNames supported.
 */
VstInt32 VstPlugin::getMidiProgramName(VstInt32 channel, 
								   MidiProgramName* midiProgramName)
{
	if (mTrace)
	  trace("VstPlugin::getMidiProgramName %ld\n", channel);
	return 0; 
}

/**
 * Struct will be filled with information for the current program.
 * Returns the programIndex of the current program. -1 means not supported.
 */
VstInt32 VstPlugin::getCurrentMidiProgram(VstInt32 channel, 
									  MidiProgramName* currentProgram)
{ 
	if (mTrace)
	  trace("VstPlugin::getCurrentMidiProgram %ld\n", channel);
	return -1; 
}

/**
 * Struct will be filled with information for 'thisCategoryIndex'.
 * returns number of used categoryIndexes. 
 * if 0 is returned, no MidiProgramCategories supported/used.
 */
VstInt32 VstPlugin::getMidiProgramCategory(VstInt32 channel, 
										MidiProgramCategory* category)
{ 
	if (mTrace)
	  trace("VstPlugin::getMidiProgramCategory %ld\n", channel);
	return 0; 
}

/**
 * Returns true if the MidiProgramNames, MidiKeyNames or 
 * MidiControllerNames had changed on this channel.
 */
bool VstPlugin::hasMidiProgramsChanged(VstInt32 channel)
{ 
	if (mTrace)
	  trace("VstPlugin::hasMidiProgramsChanged %ld\n", channel);
	return false;
}

/**
 * Struct will be filled with information for 'thisProgramIndex' and 
 * 'thisKeyNumber' if keyName is "" the standard name of the key will
 * be displayed.  If false is returned, no MidiKeyNames defined for
 * 'thisProgramIndex'.
 */
bool VstPlugin::getMidiKeyName(VstInt32 channel, MidiKeyName* keyName)
{
	if (mTrace)
	  trace("VstPlugin::getMidiKeyName %ld\n", channel);
	return false;
}

/**
 * This opcode is only called, if Plugin is of type kPlugCategShell.
 * should return the next plugin's uniqueID.
 * name points to a char buffer of size 64, which is to be filled
 * with the name of the plugin including the terminating zero.
 */
VstInt32 VstPlugin::getNextShellPlugin(char* name)
{
	if (mTrace)
	  trace("VstPlugin::getNextShellPlugin\n");
	return 0;
}

/**
 * Called one time before the start of process call
 */
VstInt32 VstPlugin::startProcess()
{ 
	if (mTrace)
	  trace("VstPlugin::startProcess\n");
	return 0; 
}

/**
 * Called after the stop of process call
 */
VstInt32 VstPlugin::stopProcess()
{ 
	if (mTrace)
	  trace("VstPlugin::stopProcess\n");
	return 0;
}

/**
 * Set the Panning Law used by the Host
 */
bool VstPlugin::setPanLaw(VstInt32 type, float val)
{ 
	if (mTrace)
	  trace("VstPlugin::setPanLaw %ld %f\n", type, val);
	return false; 
}
	
/**
 * Called before a Bank is loaded.
 * returns -1 if the Bank cannot be loaded, returns 1 if it can be
 * loaded else 0 (for compatibility)
 */
VstInt32 VstPlugin::beginLoadBank(VstPatchChunkInfo* ptr)
{
	if (mTrace)
	  trace("VstPlugin::beginLoadBank\n");
	return 0;
}

/**
 * Called before a Program is loaded. (called before beginSetProgram)
 * returns -1 if the Program cannot be loaded, returns 1 if it can be
 * loaded else 0 (for compatibility)
 */
VstInt32 VstPlugin::beginLoadProgram(VstPatchChunkInfo* ptr)
{ 
	if (mTrace)
	  trace("VstPlugin::beginLoadProgram\n");
	return 0; 
}

/****************************************************************************
 *                                                                          *
 *   							  VST EDITOR                                *
 *                                                                          *
 ****************************************************************************/

VstEditor::VstEditor(AudioEffect* e) : AEffEditor(e)
{
	mTrace = false;

	if (mTrace)
	  trace("VstEditor::VstEditor\n");

	effect = e;
	systemWindow = NULL;
#ifdef VST_2_1
	updateFlag = false;
#endif
	mIdleCount = 0;
	mRect.left = 0;
	mRect.top = 0;
	mRect.right = 0;
	mRect.bottom = 0;
	mHalting = false;
}

VstEditor::~VstEditor()
{
	if (mTrace)
	  trace("VstEditor::~VstEditor\n");
}

void VstEditor::setHalting(bool b)
{
	mHalting = b;
}

VstLongBool VstEditor::getRect(ERect **rect)
{ 
	// this happens a lot in Orion Pro, don't trace
	//if (mTrace)
	//trace("VstEditor::getRect\n");

	// Chainer crashes if we don't return something
	*rect = &mRect;

	return 0; 
}

VstLongBool VstEditor::open(void *ptr)
{ 
	if (mTrace)
	  trace("VstEditor::open\n");

	return AEffEditor::open(ptr);
}

void VstEditor::close()
{
	if (mTrace)
	  trace("VstEditor::close\n");
}

void VstEditor::idle()
{
	if (mHalting)
	  Trace(1, "VstEditpr::idle called during shutdown!\n");
	else {

		// this get's called a LOT, don't print every time
		mIdleCount++;
		if (mIdleCount > 100) {

			// this is annoying when trying to analyze timing, disable
			// for awhile
			//if (mTrace)
			//trace("VstEditor::idle\n");
			mIdleCount = 0;
		}

		// this will check updateFlag and call update();
		AEffEditor::idle();

#ifdef _WIN32
		// aefguieditor.cpp does this, not sure why
		struct tagMSG windowsMessage;
		if (PeekMessage (&windowsMessage, NULL, WM_PAINT, WM_PAINT, PM_REMOVE))
		  DispatchMessage (&windowsMessage);
#endif

	}
}

#ifdef VST_2_1
void VstEditor::update()
{
	if (mTrace)
	  trace("VstEditor::update\n");

	if (mHalting)
	  Trace(1, "VstEditor::update called during shutdown!\n");
	else
	  AEffEditor::update();
}

void VstEditor::postUpdate()
{
	if (mTrace)
	  trace("VstEditor::postUpdate\n");
	updateFlag = 1; 
}
#endif

/**
 * 2.1 extension
 */
VstLongBool VstEditor::onKeyDown(VstKeyCode &keyCode)
{ 
	if (mTrace)
	  trace("VstEditor::onKeyDown\n");
	keyCode = keyCode; 

#ifdef VST_2_1
	return -1; 
#else
	return false;
#endif
}

/**
 * 2.1 extension
 */
VstLongBool VstEditor::onKeyUp(VstKeyCode &keyCode)
{ 
	if (mTrace)
	  trace("VstEditor::onKeyUp\n");
	keyCode = keyCode; 
#ifdef VST_2_1
	return -1; 
#else
	return false;
#endif
}

/**
 * 2.1 extension
 */
VstLongBool VstEditor::setKnobMode(int val)
{ 
	if (mTrace)
	  trace("VstEditor::setKnobMode\n");
#ifdef VST_2_1
	return 0;
#else
	return false;
#endif
}

/**
 * 2.1 extension
 */
bool VstEditor::onWheel(float distance)
{ 
	if (mTrace)
	  trace("VstEditor::onWheel\n");
	return false; 
}

/**
 * Only for MAC
 */
void VstEditor::draw(ERect *rect)
{
	if (mTrace)
	  trace("VstEditor::draw\n");
	rect = rect; 
}

/**
 * Only for MAC
 */
VstInt32 VstEditor::mouse(VstInt32 x, VstInt32 y)
{
	if (mTrace)
	  trace("VstEditor::mouse\n");

	x = x; 
	y = y; 
	return 0; 
}

/**
 * Only for MAC
 */
VstInt32 VstEditor::key(VstInt32 keyCode)
{ 
	if (mTrace)
	  trace("VstEditor::key\n");

	keyCode = keyCode; 
	return 0; 
}

/**
 * Only for MAC
 */
void VstEditor::top()
{
	if (mTrace)
	  trace("VstEditor::top\n");
}

/**
 * Only for MAC
 */
void VstEditor::sleep()
{
	if (mTrace)
	  trace("VstEditor::sleep\n");
}

/****************************************************************************
 *                                                                          *
 *                                 EXTENSIONS                               *
 *                                                                          *
 ****************************************************************************/

/**
 * Should be called only within the process/processReplacing method
 * when we think we are the sync master and want to give the host our
 * desired tempo.  This uses deprecated callbacks but they still work.
 */
void VstPlugin::setHostTempo(float tempo)
{
    mTimeInfo.flags = kVstTempoValid; 
    mTimeInfo.tempo = tempo;

    // !! probably could pass time signature?
    
    audioMaster((AEffect*)this, DECLARE_VST_DEPRECATED(audioMasterSetTime), 
                0, (int)(mTimeInfo.flags), &mTimeInfo, 0.0);

}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
