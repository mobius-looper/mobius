/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * A VstPlugin that also implements the Mobius AudioStream interface.
 * 
 * One of these may be given to Mobius during construction, it will
 * in turn be given to the Recorder and overide the AudioStream 
 * returned by the AudioInterface.  
 *
 * Need to make this more flexible so there can be several audio streams
 * allowing tracks to either be connected to VST or to another
 * port on the machine.
 *
 * NOTE: The following comments are relevant for the original "dual window"
 * mode.  This is no longer used.
 *
 * Structure is confusing here:
 *
 * VST Host creates a VstMobius which implements the plugin API.
 * VstMobius creates a VstMobiusEditor which implements the editor API.
 * VST Host asks VstMobius for it's editor.
 * VST Host calls VstMobiusEditor::open
 *    - creates a HostFrame for the native window handle
 *    - creates a VstChildWindow, a subclass of ChildWindow
 *       makes it a child of HostFrame and opens it
 *    - creates a VstThread
 *      - creates the mobius UIFrame
 *      - waits for the UIFrame to close
 *      - only the VstThread knows about UIFrame
 * 
 * 
 * Scenario 1 : Close Mobius Window
 *  - UIFrame::run method returns
 *  - UIFrame deleted
 *  - VstThread::run method returns
 *  - VstThread eventually freed by either openMobiusEditor or close
 * 
 * Scenario 2 : Close Host Window
 *  - intercepted by host, eventually calls VstMobiusEditor::close
 *  - VstThread is stopped if running
 *    - results in closure of UIFrame 
 *  - Close called on VstChildWindow, not absolutely sure if this is 
 *    necessary won't the host window do this?
 *  - VstChildWindow deleted
 *  - HostWindow deleted
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "Thread.h"
#include "Trace.h"
#include "Util.h"

#include "Context.h"
#include "KeyCode.h"
#include "MidiEvent.h"

#include "ObjectPool.h"
#include "VstMobius.h"
#include "HostConfig.h"
#include "HostInterface.h"

/****************************************************************************
 *                                                                          *
 *   							  CONSTANTS                                 *
 *                                                                          *
 ****************************************************************************/

/**
 * Maximum number of frames we'll allow in the VST callback.
 * Determines the sizes of the interleaved frame buffers.  
 */
#define MAX_VST_FRAMES 1024 * 2

/**
 * Maximum number of channels we will allow in the VST callback.
 */
#define MAX_VST_CHANNELS 2

/**
 * The number of blocks we let go by before checking host tempo.
 * This is because supposedly host tempo checks are very expensive
 * for some hosts.
 */
#define TEMPO_CHECK_BLOCKS 10

/****************************************************************************
 *                                                                          *
 *   							MOBIUS PLUGIN                               *
 *                                                                          *
 ****************************************************************************/

/**
 * Second and third args to AudioEffectX constructor are 
 * kNumPrograms, kNumParams.
 */

VstMobius::VstMobius (Context* context, audioMasterCallback audioMaster)
	: VstPlugin (audioMaster, 0, 0)
{
	mTrace = true;
	if (mTrace)
	  trace("VstMobius::VstMobius %p\n", this);

	mContext = context;
    mPlugin = NULL;
    mStream = NULL;
	mHandler = NULL;
    mEditor = NULL;

	mInputLatency = 512;
	mOutputLatency = 512;
	mSampleRate = CD_SAMPLE_RATE;
	// defined in VstConfig, not used any more now that
	// we have dynamic pin config
	mInputPins = VstInputPins;
	mOutputPins = VstOutputPins;
    mParameters = 0;
    mParameterTable = NULL;
    mDummyParameter = NULL;
    mPrograms = 0;

	strcpy(mError, "");

    mInterruptInputs = NULL;
    mInterruptOutputs = NULL;
    mInterruptFrames = 0;
	mProcessing = true;
	mBypass = false;
	mDummy = false;
    mExporting = false;

	mTempoBlocks = 0;

    // old implementation
	mBeatsPerFrame = 0.0;
	mBeatsPerBar = 0.0;
	mLastSample = 0.0;
    mLastPpqRange = 0.0;
    mLastBeat = 0;
    mLastBar = 0;
    mBeatCount = 0;
    mBeatDecay = 0;
    mAwaitingRewind = false;
	mHostRewinds = false;
	mCheckSamplePosTransport = false;
	mCheckPpqPosTransport = false;
	mTraceBeats = false;

    // new implementation, setting this non-null disables
    // the old one
    mSyncState = new HostSyncState();

	for (int i = 0 ; i < MAX_VST_PORTS ; i++) {
		VstPort* port = &mPorts[i];
		port->input = new float[MAX_VST_FRAMES * MAX_VST_CHANNELS];
		port->inputPrepared = false;
		port->output = new float[MAX_VST_FRAMES * MAX_VST_CHANNELS];
		port->outputPrepared = false;
	}

	initSync();

    // kludge: can't implement AudioStream directly because of
    // conflicts with VstMobius (and indirectly the VST API classes)
    mStream = new AudioStreamProxy(this);

	// the host independent implementation is in here,
	// this must be linked in from another file
	// !! still have issues over who gets to build Context
	mPlugin = PluginInterface::newPlugin(this);

    // Constructing this causes a reference from this class
    // to the editor up in the AudioEffectX model, it must
    // not be deleted by us, the host will delete it.
	mEditor = new VstMobiusEditor(this);

    // ports are configurable
    // this comes from Mobius config and is user settable 
	int nPorts = mPlugin->getPluginPorts();

    // Plugin config files may have one of these which we can use
    // to adjust our behavior.
    HostConfigs* hostConfig = mPlugin->getHostConfigs();
    if (hostConfig != NULL) {

        // set our scope based on things passed in to the base constructor
        hostConfig->setHost(mHostVendor, mHostProduct, mHostVersion);

        // new way
        if (mSyncState != NULL)
          mSyncState->setHost(hostConfig);

        // old way

        // options for Cubase
        mHostRewinds = hostConfig->isRewindsOnResume();

        // option for Usine, perhaps this should be the default?
        mCheckPpqPosTransport = hostConfig->isPpqPosTransport();

        mCheckSamplePosTransport = hostConfig->isSamplePosTransport();

        // for a few hosts known to only support stereo, reduce the pin count
        // we don't really need this since port counts are now configurable
        // but it's nice when trying out different hosts to not have
        // to reconfigure pins
        if (hostConfig->isStereo()) {
            if (mTrace)
              trace("VstMobius: host only supports 2 pins\n");
            nPorts = 1;
        }
    }

    mInputPins = nPorts * 2;
    mOutputPins = nPorts * 2;

	if (mTrace)
	  trace("VstMobius::VstMobius ports %d\n", nPorts);

	// determine the available parameters
    initParameters();

	// determine the number of programs (presets)
	mPrograms = 0;

    // sure would like to defer these so they can be configurable!!
    setProgramCount(mPrograms);
    setParameterCount(mParameters);

	// defined in VstConfig: "Mob2"
	setUniqueID(VstUniqueId);

	setNumInputs(mInputPins);
	setNumOutputs(mOutputPins);

	canProcessReplacing();
#ifdef VST_2_1
	canMono();  // for mono->stereo fx busses
#endif

    // tells host we will be calling wantEvents
    // may also be necessary for wiring in Sonar?
	// no, this screws up being treated as an insert in Live and Cubase
    // this has been off for a long time
	//if (CLAIM_SYNTH)
    //isSynth();

	if (mTrace)
	  trace("VstMobius::VstMobius finished");
}

/**
 * DO NOT DELETE THE VstMobiusEditor!
 * This is a subclass of VstEditor which subclasses AEffEditor.
 * Creating it leaves a reference somewhere in AudioEffectX
 * which VstMobius subclasses.  The VST host will delete this
 * after deleting the AudioEffect.
 * 
 * To be safe disconnect the reference from VstMobiusEditor back
 * to VstMobius so it doesn't try to use it after we're destructed.
 */
VstMobius::~VstMobius ()
{
	if (mTrace)
	  trace("VstMobius::~VstMobius %p\n", this);
    
    // make sure the editor can't call back to us
    mEditor->disconnect();

    // any race conditions on this?  shouldn't be
    delete mParameterTable;
    delete mDummyParameter;
    mParameterTable = NULL;
    mParameters = 0;

	// have to detach the Recorder callback that Mobius added
	// to the stream, come up with a better interface!
	// ?? do we, this is a VST thing, not sure if it applies here...
	mHandler = NULL;

	for (int i = 0 ; i < MAX_VST_PORTS ; i++) {
		VstPort* port = &mPorts[i];
		delete port->input;
		delete port->output;
	}

	// make sure we're not in an interrupt
	SleepMillis(100);

    // this will also close the window
	delete mPlugin;
    mPlugin = NULL;

    delete mSyncState;

    // in theory could be something touching this?
    delete mStream;

	// shouldn't have to do this but leaving a thread behind causes
	// Live and other hosts to crash
	//ObjectPoolManager::exit(false);

	// this shouldn't be allowed to unregister the classes in case
	// there is more than one Mobius DLL open
	SleepMillis(100);

    /*
    int status = _CrtCheckMemory();
    if (!status)
      Trace(1, "_CrtCheckMemory failed\n");
    */

	delete mContext;

	if (mTrace)
	  trace("VstMobius::~VstMobius finished\n");
}

/**
 * Reset the synchronization state.  This should be called
 * when we're first initialized, whenever the transport stops, or
 * whenever a sync anonoly happens so we'll try to resync.
 *
 * Hmm, except it's only called when first initialized.  I don't
 * remember what the intent was calling it after transport stop, but
 * we haven't done that in years so it doesn't appear necessary.  
 */
void VstMobius::initSync()
{
	mTime.init();
	mTempoBlocks = 0;

    // old stuff
	mCheckSamplePosTransport = false;
	mCheckPpqPosTransport = false;
	mBeatsPerFrame = 0.0;
	mBeatsPerBar = 0.0;
	mBeatCount = 0;
	mBeatDecay = 0;
	mLastPpqRange = 0.0;
	mLastSample = -1.0;
	mLastBeat = -1;
	mLastBar = -1;
	mAwaitingRewind = false;
}

VstInt32 VstMobius::getVstVersion()
{ 
	if (mTrace)
	  trace("VstMobius::getVersion\n");

#ifdef VST_2_1
	// if we say 2300, Live will use stopProcess/startProcess calls
	// rather than setBypass which means we can't keep the clock running
	return 2000;
	//return 2300; 
#else
    // this is what we should always be using now
    // must be at 2.4 to load under Live on Mac
    return 2400;
#endif	
}

/**
 * NOTE: This is all that Reaper calls before it starts asking
 * for parameters, it does not call resume.  Since Mobius hasn't
 * been started yet we won't have initialized the tracks and will have
 * no track parameteres.  The Parameters need to handle this, but we probably
 * want to start Mobius here?
 */
void VstMobius::open()
{
	if (mTrace)
	  trace("VstMobius::open\n");
}

void VstMobius::resume()
{
	if (mTrace)
	  trace("VstMobius::resume\n");

	// docs say to call this, not sure if we have to if we ask
	// for events in other ways
#ifdef VST_2_1
	wantEvents();
#endif

    // expensive initialization
	mPlugin->start();

	// isInputConnected and isOutputConnected went away...
#ifdef VST_2_1
	if (mTrace) {
		int i;
		for (i = 0 ; i < mInputPins ; i++) {
			if (isInputConnected(i))
			  trace("VstMobius: Input pin %ld connected\n", (long)i);
			else
			  trace("VstMobius: Input pin %ld not connected\n", (long)i);
		}

		for (i = 0 ; i < mOutputPins ; i++) {
			if (isOutputConnected(i))
			  trace("VstMobius: Output pin %ld connected\n", (long)i);
			else
			  trace("VstMobius: Output pin %ld not connected\n", (long)i);
		}
	}
#endif

}

void VstMobius::suspend()
{
	if (mTrace)
	  trace("VstMobius::suspend\n");
	
	// Formerly closed the Mobius window, but that should wait for close()?
	// Presuably we should stop processing, how is this different
	// from a bypass?  Chainer does suspend, Live does setBypass
	// if 2.0, and stopProcess if 2.3.
	// Cubase calls suspend/resume several times during initialization

	mPlugin->suspend();
}

void VstMobius::close()
{
	if (mTrace)
	  trace("VstMobius::close\n");

	// turn this off so we don't try to call Mobius for any
	// lingering midi events, can that happen?
	mProcessing = false;
}

/**
 * Live calls this like a suspend?
 * This seems to always cause Live to crash, when I backed version
 * down to 2000 it calls bypass instead and doesn't crash.
 */
VstInt32 VstMobius::stopProcess()
{ 
	if (mTrace)
	  trace("VstMobius::stopProcess\n");

	mPlugin->suspend();

	mProcessing = false;

	return 1;
}

VstInt32 VstMobius::startProcess()
{ 
	if (mTrace)
	  trace("VstMobius::startProcess\n");

	mPlugin->resume();
	mProcessing = true;

	return 1;
}

bool VstMobius::getEffectName(char* name)
{
	VstPlugin::getEffectName(name);
	// defined in VstConfig: "Mobius"
	strcpy (name, VstProductName);
	return true;
}

bool VstMobius::getProductString(char* text)
{
	VstPlugin::getProductString(text);
	// defined in VstConfig: "Mobius"
	strcpy(text, VstProductName);
	return true;
}

bool VstMobius::getVendorString(char* text)
{
	VstPlugin::getVendorString(text);
	strcpy(text, "Circular Labs");
	return true;
}

VstInt32 VstMobius::getVendorVersion()
{ 
	VstPlugin::getVendorVersion();
	return 2000; 
}

VstPlugCategory VstMobius::getPlugCategory()
{ 
	VstPlugCategory cat = VstPlugin::getPlugCategory();

	// Live must have an Effect for this to be dropped into audio tracks
    // we haven't used CLAIM_SYNTH in years
	//if (CLAIM_SYNTH) 
    //cat = kPlugCategSynth;
	//else 

    cat = kPlugCategEffect; 

	return cat;
}

/**
 * returns 0 (don't know), 1 (yes), -1 (no)
 *
 * Be careful with 2in4out, and other combinations with
 * more than 2 ins or outs.  I thought this was the same as the
 * pin count, but this confuses some hosts like Cubase which will
 * refuse to load it as an insert effect.  Cryptic comments make it
 * sound like this is more like declaring surround support than just
 * pin counts.  
 *
 * It's possible Cubase just didn't like having pint counts > 2 though,
 * I didn't investigate further.
 */
VstInt32 VstMobius::canDo(char* text) 
{
	VstPlugin::canDo(text);

	long cando = -1;

    // !! Sonar confused, try taking out plugAsChannelInsert?


	if (!strcmp(text, "sendVstMidiEvent") ||
		!strcmp(text, "sendVstTimeInfo") ||
		!strcmp(text, "receiveVstEvents") ||
		!strcmp(text, "receiveVstMidiEvent") ||
		!strcmp(text, "receiveVstTimeInfo") ||
		!strcmp(text, "plugAsChannelInsert") ||
		!strcmp(text, "plugAsSend") ||
		!strcmp(text, "mixDryWet") ||
		!strcmp(text, "1in1out") ||
		!strcmp(text, "1in2out") ||
		!strcmp(text, "2in2out") ||
		!strcmp(text, "bypass") ) {
		cando = 1;
	}
	else if (!strcmp(text, "sendVstEvents") ||
			 !strcmp(text, "offline") ||
			 !strcmp(text, "noRealTime") ||
			 !strcmp(text, "multipass") ||
			 !strcmp(text, "metapass") ||
			 !strcmp(text, "midiProgramNames") ||
			 !strcmp(text, "conformsToWindowRules")) {
		cando = -1;
	}
	else {
		cando = 0;
	}

	return cando;
}

bool VstMobius::getInputProperties(VstInt32 index, VstPinProperties* properties)
{ 
	bool status = false;

	VstPlugin::getInputProperties(index, properties);
	
	if (index >= 0 && index < mInputPins) {
		properties->flags = kVstPinIsActive;
		bool leftchan = !(index & 1);
		char buffer[64];

		sprintf(buffer, "%s In %d", (leftchan) ? "Left" : "Right", 
				(index >> 1) + 1);
		strcpy(properties->label, buffer);

		// the example in the docs sets VstPinIsStereo for EVERY
		// pin, not just the even pins like we do for output ports
		// not sure which is correct

		if (!(index & 1)) {
			sprintf(properties->shortLabel, "L");
			properties->flags |= kVstPinIsStereo;
			properties->arrangementType = kSpeakerArrStereo;
		}
		else {
			sprintf(properties->shortLabel, "R");
		}

		status = true;
	}

	return status;
}

bool VstMobius::getOutputProperties(VstInt32 index, VstPinProperties* properties)
{ 
	bool status = false;

	VstPlugin::getOutputProperties(index, properties);
	
	if (index >= 0 && index < mOutputPins) {
		properties->flags = kVstPinIsActive;
		bool leftchan = !(index & 1);
		char buffer[64];

		sprintf(buffer, "%s Out %d", (leftchan) ? "Left" : "Right", 
				(index >> 1) + 1);
		strcpy(properties->label, buffer);

		if (!(index & 1)) {
			properties->flags |= kVstPinIsStereo;
			properties->arrangementType = kSpeakerArrStereo;
		}
		status = true;
	}

	return status;
}

/**
 * Presumably to display a message after something goes wrong.
 * What triggers this?
 */
bool VstMobius::getErrorText(char* text)
{ 
	VstPlugin::getErrorText(text);
	return false;
}

/**
 * Doc says "return if keys are needed or not", I'm assuming this
 * means that we want keyboard VstEvents
 * 
 * One site says 0 = needs keys and 1 = don't need and that
 * this is deprecated in 2.4  VstEditor seems to get onKeyDown
 * and onKeyUp with this returning false.
 */
bool VstMobius::keysRequired()
{
	VstPlugin::keysRequired();
	return false;
}

/**
 * For "soft bypass" process() is still called.  Some plugs need to 
 * stay alive even when bypassed.  A canDo("bypass") indiciates that
 * the plug supports soft bypass.
 * return: true - supports soft bypass, process() is called, the
 *  plug should compensate it's delay, and copy inputs to outputs
 *  false - doesn't support soft bypass, process() will not be called,
 *  the host should do the bypass
 */
bool VstMobius::setBypass(bool onOff)
{
	VstPlugin::setBypass(onOff);
	mBypass = onOff;

	// need a mode where we either keep running or pause
    /*
    if (onOff)
      mPlugin->suspend();
    else
      mPlugin->resume();
    */

	return true;
}

void VstMobius::setBlockSizeAndSampleRate(VstInt32 size, float rate)
{
	VstPlugin::setBlockSizeAndSampleRate(size, rate);

	setBlockSizeInternal(size);
	setSampleRateInternal(rate);
}

void VstMobius::setBlockSize(VstInt32 size)
{
	VstPlugin::setBlockSize(size);
	setBlockSizeInternal(size);
}

void VstMobius::setSampleRate(float rate)
{
	VstPlugin::setSampleRate(rate);
	setSampleRateInternal(rate);
}

void VstMobius::setBlockSizeInternal(int size)
{
	// we don't get seperate input and output block sizes so have
	// to assume the latency is the same
    setInputLatencyFrames(size);
	setOutputLatencyFrames(size);
}

void VstMobius::setSampleRateInternal(float rate)
{
	// it seems to be ok to truncate the fraction?
	int irate = (int)rate;
	if ((rate - irate) > 0) 
	  Trace(1, "VstMobius::setSampleRateInternal Fractional sample rate!\n");

    /*
	if (irate != CD_SAMPLE_RATE)
	  Trace(1, "VstMoboius::setSampleRateInternal Unsupported sample rate %ld\n", (long)irate);
    */

    setSampleRateInt(irate);
}

/**
 * VstPlugin has getSampleRate/setSampleRate with a float
 * we like to maintain it as an int for AudioStream.
 */
void VstMobius::setSampleRateInt(int i)
{
    mSampleRate = i;
}
    
int VstMobius::getSampleRateInt()
{
    return mSampleRate;
}

//////////////////////////////////////////////////////////////////////
//
// Internal Parameter Management 
//
//////////////////////////////////////////////////////////////////////

/**
 * PluginInterface returns a list of parameters with numeric ids, 
 * these are not necessarily indexes into the parameter list.
 * AU Parameters are assigned unique numeric ids that are saved 
 * in automation curves.
 * 
 * VST wants a "parameter count" and assumes that the parameter ids
 * are indexes within that range.  The parameter indexes are saved
 * in automation curves.
 *
 * There is no guarentee that there won't be holes in the
 * PluginParameter id range though the UI will try to keep it compact.
 * VST doesn't like holes, if we say there are 10 parameters, there
 * must be 10.
 *
 * We could handle holes in two ways:
 *
 *    - leave dummy parameters that have a name like "empty 3" that
 *      don't do anything
 *
 *    - compress the range so that the VST parameter ids are not
 *      necessarily the same as the PluginParameter id
 *
 * The first allows us to reconfigure parameters but keep their ids
 * but looks funny.
 *
 * The second doesn't have any funky mystery parameters but if you
 * save parameter values in the host session the ids they are saved
 * under may not match the PluginParameters if you edit the list.
 * This also effects parameter automation curves.
 *
 * I tried 1 but it's easy to end up with holes after a few rounds
 * of parameter configuration so we have to use 2.  This shouldn't be too bad,
 * it just means that the order of the parameters in the config window
 * define the ids and if you change the order automation curves become
 * undefined.  If we're going to do that we may as well do it for AU
 * so they both behave the same.
 */

/*
class DummyParameter : public PluginParameter {
  public:
    
	DummyParameter() {
        setName("Unused");
        setType(PluginParameterContinuous);
        setMinimum(0);
        setMaximum(127);
    }

	~DummyParameter() {
    }

    const char** getValueLabels() {
        return NULL;
    }

	float getValue() {
        return 0.0;
    }

    void setValue(float f) {
    }
};
*/

PRIVATE void VstMobius::initParameters()
{
    // count them, ignore ids
    int count = 0;
    PluginParameter* params = mPlugin->getParameters();
    for (PluginParameter* p = params ; p != NULL ; p = p->getNext())
	  count++;

    // disable for awhile
    //count = 0;
    
    if (count == 0) {
        Trace(2, "VstMobius::initParameters no parameters\n");
        mParameters = 0;
    }
    else {
        mParameters = count;
        Trace(2, "VstMobius::initParameters %ld parameters\n", (long)mParameters);
        // assume the plugin isn't allowed to free PluginParamters
        // once returned
        mParameterTable = new PluginParameter*[mParameters];

        int i = 0;
        for (PluginParameter* p = params ; p != NULL ; p = p->getNext(), i++)
		  mParameterTable[i] = p;
	}
}

/**
 * Called at the end of each buffer to tell the host about
 * changes to parameters made by the plugin.
 */
PRIVATE void VstMobius::exportParameters()
{
    // set this to ignore the redundant call to setParameter
    // that setParameterAutomated will make before it notifies
    // the host
    mExporting = true;
	for (int i = 0 ; i < mParameters ; i++) {
		PluginParameter* p = mParameterTable[i];
		if (p->refreshValue()) {
            float neu = (float)scaleParameterOut(p, (int)p->getLast());
            setParameterAutomated(i, neu);
		}
	}
    mExporting = false;
}

//////////////////////////////////////////////////////////////////////
// 
// VST Parameter Interface
//
//////////////////////////////////////////////////////////////////////

//
// Parameter values must all ultimately be represented by floats, so
// if any of them are arbitrary strings, such as project names, we will
// have to assign each project a number and map the number back into 
// a name.  This will be hard for projects.  We'll have to keep a 
// project registration list like we do for scripts.  The easiest thing
// is to map then according to the order in the registration list, but
// this is fragile if you make any order changes.  Better if we could
// assign each project an ever increasing number, but then we have to 
// store the number somewhere, I guess ui.xml.
// 
// 

/**
 * Setting integer min/max doesn't seem to help, at least not in Live
 * which still gives values from 0.0 to 1.0.
 */
int VstMobius::scaleParameterIn(PluginParameter* p, float value)
{
    int ivalue = 0;

    PluginParameterType type = p->getType();

    if (type == PluginParameterContinuous ||
        type == PluginParameterEnumeration) {

        int min = (int)p->getMinimum();
        int max = (int)p->getMaximum();

        ivalue = ScaleValueIn(value, min, max);
    }
    else if (type == PluginParameterBoolean || 
             type == PluginParameterButton) {
        // these used IsSwitch, still need to scale
        ivalue = (int)value;
    }

    return ivalue;
}

/**
 * On the way out, the float values will be quantized
 * to the beginning of their "chunk".  This makes zero
 * align with the left edge, but makes the max value slightly
 * less than the right edge.
 */
float VstMobius::scaleParameterOut(PluginParameter* p, int value)
{
    float fvalue = 0.0;

    PluginParameterType type = p->getType();

    if (type == PluginParameterContinuous ||
        type == PluginParameterEnumeration) {
        int min = (int)p->getMinimum();
        int max = (int)p->getMaximum();

        fvalue = ScaleValueOut(value, min, max);
    }
    else if (type == PluginParameterBoolean || 
             type == PluginParameterButton) {
        // these used IsSwitch, still need to scale
        fvalue = (float)value;
    }

    return fvalue;
}

/**
 * !! PluginParameters expect to be modified "in the interrupt" which
 * means you can't be calling setParameter and be in process()
 * at the same time.  Does the VST spec say that?  If not we'll have
 * to queue these till the next process like AU does.
 */
void VstMobius::setParameter(VstInt32 index, float value)
{
	if (mTrace)
	  trace("VstMobius::setParameter %ld %f\n", index, value);

    // Ignore if we're exporting since setParameterAutomated
    // will call this, and we already have the value.
    if (!mExporting) {
        if (index < mParameters) {
            PluginParameter* p = mParameterTable[index];
            if (p != NULL) {
                float scaled = (float)scaleParameterIn(p, value);

                trace("setParameter %d %f scaled %f\n", (int)index, value, scaled);

                p->setValueIfChanged(scaled);
            }
        }
    }
}

float VstMobius::getParameter(VstInt32 index)
{
    float value = 0.0f;

	if (mTrace)
	  trace("VstMobius::getParameter %ld of %ld\n", index, mParameters);

    if (index < mParameters) {
        PluginParameter* p = mParameterTable[index];
        if (p != NULL) {
            // You must use getLast() rather than getValue() here
            // Some calls to setParameter won't be synchronous so we
            // must return the last value set by the host
            int current = (int)p->getLast();
            value = (float)scaleParameterOut(p, current);

            trace("getParameter %d %d scaled %f\n", (int)index, current, value);
        }
	}

	return value;
}

/**
 * Return a "units" qualifier such as "db", "sec", etc.
 */
void VstMobius::getParameterLabel(VstInt32 index, char *label) 
{
	if (mTrace)
	  trace("VstMobius::getParameterLabel %ld\n", index);
	
	// don't have any labels yet
    strcpy(label, "");
}

/**
 * Return the parameter value as a string
 * Spec of course doesn't say what the maximum length is.
 * This is where we get to show strings rather than integers.
 */
void VstMobius::getParameterDisplay(VstInt32 index, char *text)
{ 
	if (mTrace)
	  trace("VstMobius::getParameterDisplay %ld\n", index);
	
	strcpy(text, "");

    if (index < mParameters) {
        PluginParameter* p = mParameterTable[index];
        if (p != NULL) {
            // TODO: some range checking!!
            // we have historically limited this to 32
            float value = p->getLast();
            p->getValueString(value, text, 32);

            trace("getParameterDisplay %s\n", text);
        }
    }
}

/**
 * Return the parameter name you'd like to see in the UI.
 */
void VstMobius::getParameterName(VstInt32 index, char *text)
{ 
	if (mTrace)
	  trace("VstMobius::getParameterName %ld\n", index);
	
	strcpy(text, "");

    if (index < mParameters) {
        PluginParameter* p = mParameterTable[index];
        if (p != NULL) {
            // it is crucial that we bound this since there
            // can be function names in here!
            // kVstMaxLabelLen is 64 (63 plus terminator)
            // kVstMaxShortLabelLen is 8 (various comments say
            // this should be "6 + delimiter"
            CopyString(p->getName(), text, 60);
        }
    }
}
	
// 
// VST 2.0
//

/**
 * Return true if the parameter can be automated, if false I'm guessing
 * that the host won't bother recording changes.
 */
bool VstMobius::canParameterBeAutomated(VstInt32 index)
{ 
    bool automated = false;

	if (mTrace)
	  trace("VstMobius::canParameterBeAutomated %ld\n", index);

    if (index < mParameters) {
        PluginParameter* p = mParameterTable[index];
        if (p != NULL) {
            PluginParameterType type = p->getType();
            // I guess let all of them in.
            // Continuous and Enumeration are useful,
            // Boolean probaby so.
            // Function is debatable.
            automated = true;
        }
    }

	return automated;
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
bool VstMobius::string2parameter(VstInt32 index, char* text)
{ 
    bool success = false;

	if (mTrace)
	  trace("VstMobius::string2parameter %ld %s\n", index, text);

    if (text == NULL) {
        // means we support setting by name
        success = true;
    }
    else if (index < mParameters) {
        PluginParameter* p = mParameterTable[index];
        if (p != NULL) {
            p->setValueString(text);
            success = true;
        }
    }

	return success;
}

/**
 * Get a channel specific parameter?  Docs say "for internal
 * use of soft synth voices, as a voice may be channel dependent".
 */
float VstMobius::getChannelParameter(VstInt32 channel, VstInt32 index)
{ 
	if (mTrace)
	  trace("VstMobius::getChannelParameter %ld %ld\n", channel, index);
	return 0.0f; 
}

/**
 * Fields include:
 *
 * stepFloat
 * smallStepFloat
 * largeStepFloat
 * label (up to 63 characters)
 * flags:
 *    kVstParameterIsSwitch
 *    kVstParameterUsesIntegerMinMax
 *    kVstParameterUsesFloatStep
 *    kVstParameterUsesIntStep
 *    kVstParameterSupportsDisplayIndex
 *    kVstParameterSupportsDisplayCategory
 *    kVstParameterCanRamp
 * minInteger
 * maxInteger
 * stepInteger
 * largeStepInteger
 * shortLabel: (max 7 recommended 6 chars plus delimiter)
 * displayIndex
 *  "for remote controllers, the index where this parameter should
 *   be displayed (from 0), SupportsDisplayIndex must be set"
 *
 * category (0 none, else group index + 1)
 * numParametersInCategory
 * reserved
 * categoryLabel
 * future
 *
 * NOTES
 *
 * displayIndex can be used to control the order in which they are 
 * displayed in the host.
 *
 * categoryLabel can be used to group them into categories, presumably
 * displayed near each other with a bounding box.
 * 
 * If kVstParameterUsesIntStep is on, then stepInteger and largeStep
 * integer must be set.
 *
 * kVstParameterCanRamp might be interesting for Continuous?
 *
 * UPDATE: The InterMinMax and such doesn't seem to make any difference
 * for live, it still gives parameter values from float 0.0 to 1.0 
 * so have to scale.
 */
bool VstMobius::getParameterProperties(VstInt32 index, 
                                       VstParameterProperties* vpp)
{ 
    bool success = false;

	if (mTrace)
	  trace("VstPlugin::getParameterProperties %ld\n", index);

    if (index < mParameters) {
        PluginParameter* p = mParameterTable[index];
        if (p != NULL) {
            PluginParameterType type = p->getType();
            success = true;

            // kVstMaxLabelLen is 64 (63 plus terminator)
            CopyString(p->getName(), vpp->label, 60);

            if (type == PluginParameterContinuous) {
                vpp->flags = kVstParameterUsesIntegerMinMax | 
                    kVstParameterUsesIntStep;
                vpp->minInteger = (int)p->getMinimum();
                vpp->maxInteger = (int)p->getMaximum();
                vpp->stepInteger = 1;
                vpp->largeStepInteger = 10;
            }
            else if (type == PluginParameterEnumeration) {
                vpp->flags = kVstParameterUsesIntegerMinMax | 
                    kVstParameterUsesIntStep;
                vpp->minInteger = (int)p->getMinimum();
                vpp->maxInteger = (int)p->getMaximum();
                vpp->stepInteger = 1;
                vpp->largeStepInteger = 1;
            }
            else if (type == PluginParameterBoolean) {
                // do we need integer min/max for IsSwitch?
                vpp->flags = kVstParameterUsesIntegerMinMax | 
                    kVstParameterIsSwitch;
                vpp->minInteger = 0;
                vpp->maxInteger = 1;
            }
            else if (type == PluginParameterButton) {
                vpp->flags = kVstParameterUsesIntegerMinMax |
                    kVstParameterIsSwitch;
                vpp->minInteger = 0;
                vpp->maxInteger = 1;
            }
            else {
                success = false;
            }
        }
    }

	return success;
}

/****************************************************************************
 *                                                                          *
 *                                  PROGRAMS                                *
 *                                                                          *
 ****************************************************************************/

VstInt32 VstMobius::getProgram () 
{ 
	if (mTrace)
	  trace("VstMobius::getProgram\n");

	return AudioEffectX::getProgram();
}

void VstMobius::setProgram (VstInt32 program) 
{ 
	if (mTrace)
	  trace("VstMobius::setProgram %ld\n", program);

	AudioEffectX::setProgram(program);
}

void VstMobius::setProgramName(char* name)
{ 
	if (mTrace)
	  trace("VstMobius::setProgramName %s\n", name);

	// NOTE: The default implementation of this is *broken*
	// it will do "*name = 0" like getProgramName, it must not touch
	// this as it is sometimes a constant
	//AudioEffectX::setProgramName(name);
}

/**
 * Docs indicate that most string buffers are 24 characters
 * so be careful with long names!
 */
void VstMobius::getProgramName(char* name)
{ 
	if (mTrace)
	  trace("VstMobius::getProgramName\n");
	
	AudioEffectX::getProgramName(name);
}

// 
// VST 2.0
//

/**
 * Copy the currently selected program to the program destination
 */
bool VstMobius::copyProgram(VstInt32 destination)
{
	if (mTrace)
	  trace("VstMobius::copyProgram %ld\n", destination);
	return false; 
}

/**
 * Called before a program is loaded.
 * Return true if "the plug took the notification into account"
 */
bool  VstMobius::beginSetProgram()
{
	if (mTrace)
	  trace("VstMobius::beginSetProgram\n");
	return false;
}

/**
 * Called after a program is loaded
 * Return true if "the plug took the notification into account"
 */
bool  VstMobius::endSetProgram()
{
	if (mTrace)
	  trace("VstMobius::endSetProgram\n");
	return false;
}

/**
 * Return more than 1 if you support more than one category.
 * Docs say "i.e. if programs are divided into groups like General MIDI"
 */
VstInt32 VstMobius::getNumCategories()
{ 
	if (mTrace)
	  trace("VstMobius::getNumCategories\n");
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
bool VstMobius::getProgramNameIndexed(VstInt32 category, VstInt32 index, char* text)
{ 
	if (mTrace)
	  trace("VstMobius::getProgramNameIndexed %ld %ld\n", 
			 category, index);
	return false; 
}

/****************************************************************************
 *                                                                          *
 *   								EVENTS                                  *
 *                                                                          *
 ****************************************************************************/

/**
 * Convert a VST midi event into one that looks like it
 * comes from the MidiInterface.
 * 
 * !! The external EDP feature isn't working here since we're
 * not using a MidiMap down in the MidiIn object.
 */
VstInt32 VstMobius::processEvents(VstEvents* events)
{
	// trace them 
	VstPlugin::processEvents(events);

	if (mProcessing) {
		for (int i = 0 ; i < events->numEvents ; i++) {
			VstEvent* e = events->events[i];
			if (e->type == kVstMidiType) {
				VstMidiEvent* me = (VstMidiEvent*)e;
				char* bytes = me->midiData;
		
				int status  = bytes[0] & 0xFF;
                int channel = 0;
                bool pass = false;
                
				if (status >= 0xF0) {
					// a non-channel event, always filter out active 
					// sense garbage WindowsMidiInterface also allows
					// filtering of all realtime events
					// do "commons" come in here??

                    pass = (status != 0xFE);
				}
				else {
					// its an channel event that may be mapped
                    channel = status & 0x0F;
					status  = status & 0xF0;

					// WindowsMidiInterface allows filtering of
					// POLYPRESSURE, CONTROL, TOUCH, and PROGRAM
                    pass = true;
				}

                if (pass) {
                    // the interface takes a final argument frame
                    // which AU uses, need to do the same for VST!!
                    long frame = 0;
					mPlugin->midiEvent(status, channel, bytes[1], bytes[2], frame); 
                }
			}
		}
	}

	// return value is undocumented
	return 1;
}

/**
 * Called at the end of each process() to send MIDI messages generated during
 * this cycle to the host.
 * 
 * The PluginInterface gives us a list of MidiEvent objects.  This isn't
 * symetical with PluginInterface::midiEvent but it's easier than having
 * a HostInterface callback and make us manage our own event list.
 *
 */
void VstMobius::sendMidiEvents()
{
	//Trace(3,"VstMobius::sendMidiEvents");  //<--- Cas #014
    MidiEvent* events = mPlugin->getMidiEvents();
    MidiEvent* next = NULL;

    for (MidiEvent* event = events ; event != NULL ; event = next) {
        
		
		next = event->getNext();
        event->setNext(NULL);

		Trace(3,"VstMobius::sendVstEventsToHost ! |status %i|channel %i|key %i|velocity %i|",event->getStatus(), event->getChannel(),event->getKey(), event->getVelocity());  //<--- Cas #014
		Trace(3,"NextEvent isNull? %s", next == NULL ? "true" : "false");

        // this sends them to the host one at a time, supposedly
        // it is better to send them in an array but it's awkward
        // and the host needs to deal with this anyway

        VstMidiEvent me;
        //memset(&me, 0, sizeof(VstMidiEvent));
        me.type = kVstMidiType;
        me.byteSize = sizeof(VstMidiEvent);

        // aeffectx.h says: 
        //   sample frames related to the current block start sample position
        // guessing this means "relative to the current block start"
        // should be looking at MidiEvent::mClock!?
        me.deltaFrames = 0;

        // the only defined flag is kVstMidiEventIsRealtime which
        // apparently means the event is supposed to be played with
        // a higher priority, probably for clocks
        me.flags = 0;
        // length (in sample frames) of entire note, if available, else 0
        me.noteLength = 0;
        // offset (in sample frames) into note from note start if available, else 0
        me.noteOffset = 0;

        // 1 to 3 MIDI bytes; midiData[3] is reserved (zero)
        me.midiData[0] = event->getStatus() | event->getChannel();
        me.midiData[1] = event->getKey();
        me.midiData[2] = event->getVelocity();
        me.midiData[3] = 0;

        // -64 to +63 cents; for scales other than 'well-tempered' ('microtuning')
        me.detune = 0;
        // Note Off Velocity [0, 127]
        me.noteOffVelocity = 0;
        me.reserved1 = 0;
        me.reserved2 = 0;

        VstEvents ve;
        ve.numEvents = 1;
        ve.events[0] = (VstEvent *)&me;
        ve.reserved = 0;

		
        // not sure what the return value means
        bool rc = sendVstEventsToHost(&ve);			//<--- Cas #014 Issue VstMidi Host Cantabile not working

		if(rc)
			Trace(3,"VstMobius::sendVstEventsToHost->Res=true;"); 
		else
			Trace(3,"VstMobius::sendVstEventsToHost->Res=false;"); 

        event->free();
    }
}

/****************************************************************************
 *                                                                          *
 *                               OLD TIME CHECK                             *
 *                                                                          *
 ****************************************************************************/

/**
 * We need to determine the frame offset within this buffer where
 * the next beat boundary will occur.  The first thing to calculate
 * is the number of beats per frame which will generally be
 * a small fraction.   This is a function of the host tempo:
 *
 *     beatsPerFrame = tempo / (60.0 * sampleRate)
 *
 * The number of frames per minute can be held constant, so this
 * reduces to a division and the overhead of asking for the host
 * tempo.  Forum comments indiciate that returning the tempo may
 * be expensive for some hosts, so since we are expecting it not
 * to change, we could only sample it and calculate beatsPerFrame
 * once every 10 blocks.
 *
 * Next we need to determine if the next beat boundary is within this
 * buffer.  One method is to calculate the beat of the last frame:
 *
 *    lastFrameBeat = ppqPos + (beatsPerFrame * (bufferFrames - 1))
 *
 * Which is one multiply.  Then to get the last beat in this buffer
 * for comparison with the beat counter you need a truncation.
 *
 *    lastBeatInBuffer = (int)lastFrameBeat
 *
 * That must be calculated every buffer.  If the beat increments,
 * then the frame is determined with:
 *
 *    boundaryFrame = ((float)lastBeatInBuffer - ppqPos) / beatsPerFrame
 * 
 * A somewhat simpler method is to do the boundaryFrame calculation
 * first instead of lastFrameBeat, and if it is less than the buffer
 * size, we're done.  
 *
 *   boundaryFrame = ((float)(lastBeat + 1) - ppqPos) / beatsPerFrame
 *
 * The tradeoff is that we're doing a division on every block rather
 * than a multiply.  I don't know which is faster but I doubt it matters.
 *
 * Hmm, Cubase as usual throws a wrench into this.  Because of it's odd
 * pre-roll, ppqPos can actually go negative briefly when starting from
 * zero. But it is -0.xxxxx which when you truncate is just 0 so we can't
 * tell when the beat changes given the lastBeat formula avove.
 *
 * Easier to use the first formula, then detect a beat if the
 * lastBeatInBuffer is different than lastBeat OR if the sign changes.
 * Hmm, this may be easier if we force lastBeat to -1 which is really what
 * it should be?
 * 
 * barStartPos turns out to be useless because hosts don't implement
 * it consistently.  In AudioMulch and Live, it is the base beat
 * of the current bar, e.g. 0, 4, 8...  In energyXt it is the current
 * beat without a fraction, 0, 1, 2, 3...  In Cubase it is the current
 * beat with a mysterious fraction I don't understand.
 *
 * Rather than try to detect which style is being used, just blow off
 * barStartPos and maintain our own bar beat counter.  This requires
 * though that we get accurate time signature info.  
 *
 * Also do not assume that ppqPos will always increase, if there is a loop
 * in the sequencer it may jump back to the beginning beat.
 *
 * CUBASE ODDITY:
 *
 * In the Cubase SX 1.02 demo (not sure about later ones) when starting
 * the transport the current transport location is passed in ppqPos
 * for one or two buffers, then it rewinds a bit (seems to be 32 buffers),
 * then plays steadily.  I'm not sure why this is, perhaps it is doing
 * latency adjustments for all the tracks.  This means though that we
 * have to defer the generation of a BEAT/BAR event because we may not
 * actually stay on one.  It seems this only happens on play, and once
 * ppqPos has advanced, it won't go backward again.  
 *
 */
PRIVATE void VstMobius::checkTimeOld(VstInt32 bufferFrames)
{
	bool tempoRequested = false;
	int flags = kVstPpqPosValid | kVstBarsValid;

	// check for tempo changes every few blocks, actually we
	// could even skip checking time at all under the assumption
	// that ppqPos is going to advance at a constant rate assuming
	// tempo isn't changing
	
	mTempoBlocks++;
	if (mTempoBlocks >= TEMPO_CHECK_BLOCKS || mTime.tempo == 0) {
		tempoRequested = true;
		flags = flags | kVstTempoValid | kVstTimeSigValid;
		mTempoBlocks = 0;
	}

	VstTimeInfo* time = getTimeInfo(flags);
	if (time != NULL) {

		double ppqPos = time->ppqPos;
		double prevPpqPos = mTime.beatPosition;
		double ppqRange;
		bool beatBoundary = false;
		bool barBoundary = false;
		long boundaryOffset = 0;
		int beat = 0;
		int bar = 0;

		// detect tempo changes, may set mTime fields related to tempo
		if (tempoRequested)
		  checkTempoOld(time);

		// detect transport changes (play/stop), may set mTime.playing
		bool resumed = checkTransportOld(time);

		bool traceBuffers = false;
		if (traceBuffers && mTime.playing) {
			trace("VstMobius: ppqPos %lf barStartPos %lf frames %ld\n", 
				  time->ppqPos, time->barStartPos, bufferFrames);
		}

		// kludge for Cubase that likes to rewind AFTER the transport 
		// status changes to play
		if (resumed) {
			if (mHostRewinds) {
				trace("VstMobius: awaiting host rewind\n");
				mAwaitingRewind = true;
			}
		}
		else if (mAwaitingRewind) {
			if (prevPpqPos != ppqPos) {
				mAwaitingRewind = false;
				// make it look like a resume for the beat logic below
				resumed = true;
				trace("VstMobius: host rewind detected\n");
			}
		}

		// Determine if there is a beat boundary in this buffer
		if (mTime.playing && !mAwaitingRewind) {

			// remove the fraction
			long baseBeat = (long)ppqPos;
			long newBeat = baseBeat;

			if (baseBeat == 128) {
				int x = 0;
			}

			// determine the last ppqPos within this buffer
			ppqRange = ppqPos + (mBeatsPerFrame * (bufferFrames - 1));

			// determine if there is a beat boundary at the beginning
			// or within the current buffer, and set beatBoundary
			if (ppqPos == (double)newBeat) {
				// no fraction, first frame is exactly on the beat
				// NOTE: this calculation, like any involving direct equality
				// of floats may fail due to rounding error, in one case
				// AudioMulch seems to reliably hit beat 128 with a ppqPos
				// of 128.00000000002 this will have to be caught in
				// the jump detector below, which means we really don't
				// need this clause
				if (!mTime.beatBoundary)
				  beatBoundary = true;
				else { 
					// we advanced the beat in the previous buffer,
					// must be an error in the edge condition?
					// UPDATE: this might happen due to float rounding
					// so we should probably drop it to level 2?
					Trace(1, "VstMobius::checkTime Ignoring redundant beat edge condition!\n");
				}
			}
			else {
				// detect beat crossing within this buffer
				long lastBeatInBuffer = (long)ppqRange;
				if (baseBeat != lastBeatInBuffer ||
					// fringe case, crossing zero
					(ppqPos < 0 && ppqRange > 0)) {
					beatBoundary = true;
					boundaryOffset = (long)
						(((double)lastBeatInBuffer - ppqPos) / mBeatsPerFrame);
					newBeat = lastBeatInBuffer;
				}
			}

			// check for jumps and missed beats
			// when checking forward movement look at beat counts rather
			// than expected ppqPos to avoid rounding errors
			bool jumped = false;
			if (ppqPos <= prevPpqPos) {
				// the transport was rewound, this happens with some hosts
				// such as Usine that maintain a "cycle" and wrap the
				// beat counter from the end of the cycle back to the front
				trace("VstMobius: Transport was rewound\n");
				jumped = true;
			}
			else if (newBeat > (mLastBeat + 1)) {
				// a jump of more than one beat, transport must be forwarding
				trace("VstMobius: Transport was forwarded\n");
				jumped = true;
			}
			else if (!beatBoundary && newBeat != mLastBeat) {
				// A single beat jump, without detecting a beat boundary.
				// This can happen when the beat falls exactly on the first
				// frame of the buffer, but due to float rounding we didn't
				// catch it in the (ppqPos == (double)newBeat) clause above.
				// In theory, we should check to see if mLastPpqRange is
				// "close enough" to the current ppqPos to prove they are
				// adjacent, otherwise, we could have done a fast forward
				// from the middle of the previous beat to the start of this 
				// one, and should treat that as a jump?  I guess it doesn't
				// hurt the state machine, we just won't get accurately sized
				// loops if we're doing sync at the moment.
				if (!mTime.beatBoundary) 
				  beatBoundary = true;
				else {
					// this could only happen if we had generated a beat on the
					// previous buffer, then instantly jumped to the next beat
					// it is a special case of checking mLastPpqRange, the
					// two buffers cannot be adjacent in time
					trace("VstMobius: Transport was forwarded one beat\n");
					jumped = true;
				}
			}

			// when we resume or jump, have to recalculate the beat counter
			if (resumed || jumped) {
				// !! this will be wrong if mBeatsPerBar is not an integer,
				// when would that happen?
				mBeatCount = (int)(baseBeat % (long)mBeatsPerBar);
				if (resumed)
				  trace("VstMobius: Resuming playback at bar beat %d\n",
						mBeatCount);
				else 
				  trace("VstMobius: Playback jumped to bar beat %d\n",
						mBeatCount);
			}

			// For hosts like Usine that rewind to the beginning of a cycle,
			// have to suppress detection of the beat at the start of the
			// cycle since we already generated one for the end of the cycle
			// on the last buffer.  This will also catch odd situations
			// like instantly moving the location from one beat to another.
			if (beatBoundary) {
				if (mTime.beatBoundary) {
					beatBoundary = false;
					if (!resumed && !jumped) 
					  Trace(1, "VstMobius::checkTime Supressed double beat, possible calculation error!\n");
					// sanity check, mBeatDecay == 0 should be the same as
					// mTime.beatBoundary since it happened on the last buffer
					if (mBeatDecay != 0)
					  Trace(1, "VstMobius::checkTime Unexpected beat decay value!\n");
				}
				else {
					int minDecay = 4; // need configurable maximum?
					if (mBeatDecay < minDecay) {
						// We generated a beat/bar a few buffers ago, this
						// happens in Usine when it rewinds to the start
						// of the cycle, but let's it play a buffer past the
						// end of the cycle before rewinding.  This is a host
						// error since the bar length Mobius belives is
						// actually shorter than the one Usine will be playing.
						Trace(1, "VstMobius::checkTime Suppressed double beat, host is not advancing the transport correctly!\n");
						beatBoundary = false;
					}
				}
			}


			// Detect bars
			// barStartPos is useless because hosts don't implement
			// it consistently, see vst notes for more details.
			if (beatBoundary) {
				if ((resumed || jumped) && boundaryOffset == 0) {
					// don't need to update the beat counter, but we may
					// be starting on a bar
					if (mBeatCount == 0 || mBeatCount >= mBeatsPerBar) {
						barBoundary = true;
						mBeatCount = 0;
					}
				}
				else {
					mBeatCount++;
					if (mBeatCount >= mBeatsPerBar) {
						barBoundary = true;
						mBeatCount = 0;
					}
				}
			}

			// selectively enable these to reduce clutter in the stream
			if (mTraceBeats) {

				if (barBoundary)
				  trace("VstMobius: BAR: ppqPos: %lf range: %lf barStartPos %lf offset %d\n", 
						time->ppqPos, ppqRange, time->barStartPos,
						boundaryOffset);
				else if (beatBoundary)
				  trace("VstMobius: BEAT: ppqPos: %lf range: %lf barStartPos %lf offset %d\n", 
						time->ppqPos, ppqRange, time->barStartPos,
						boundaryOffset);
			}

			mLastBeat = newBeat;
		}

		// update this last so we can check previous status as
		// we calculate the new status
		mTime.beatPosition = ppqPos;
		mTime.beatBoundary = beatBoundary;
		mTime.barBoundary = barBoundary;
		mTime.boundaryOffset = boundaryOffset;
		mTime.beat = mLastBeat;
		mLastSample = time->samplePos;
		mLastPpqRange = ppqRange;

		if (beatBoundary)
		  mBeatDecay = 0;
		else
		  mBeatDecay++;
	}
	else {
		// full reset of AudioTime?
		mTime.playing = false;
	}

    // if we are a sync master, let the host know
    bool isSyncMaster = false;
    if (isSyncMaster) {
        setHostTempo(69.0);
    }

}

/**
 * Detect changes to the host transport (play/stop).
 * Usually this is done by examining the kVstTransportChanged
 * and kVstTransportPlaying fields, but comments on the forum 
 * suggest that not all hosts do this and you also have to watch
 * for changes in the samplePos to infer the transport state.  
 * Monitoring samplePos doesn't work for some hosts like Cubase
 * which change samplePos and ppqPos when the transport isn't
 * running, probably as you set the track cursor.
 *
 * samplePos detection is therefore disabled and if we ever
 * do need to enable it it will have to be host specific.
 *
 * I also added transport detection by monitoring ppqPos while
 * debugging in Cubase, this really should never be necessary
 * but leave it around for awhile.
 *
 * UPDATE: A relatively new host Usine uses does not use transport
 * controls, it simply starts/stops ppqPos and samplePos.
 *
 * Ableton 8.2: If you have set up a transport loop, when you
 * start the transport the beat position starts over at 0 but
 * when you take the loop it goes to it's correct position.  For
 * example a cycle from beat 8 to beat 12.  At transport start
 * you'll count from 0 up but when you reach what would be 4,
 * it jumps to 8. Not much we can do about that though Ableton 
 * does seem to set the cycleStartPos correctly.  If we get ppqPos
 * of 0 and cycleStartPos is non-zero can use that to adjust the
 * beat position.
 * 
 */
PRIVATE bool VstMobius::checkTransportOld(VstTimeInfo* time)
{
	bool resumed = false;

	if (time->flags & kVstTransportChanged) {
		bool playing = (time->flags & kVstTransportPlaying) ? true : false;
		if (playing != mTime.playing) {
			if (playing) {
				trace("VstMobius: PLAY\n");
				resumed = true;
			}
			else {
				trace("VstMobius: STOP\n");
				// Clear out all sync status
				// or just keep going pretending there are beats and bars?
			}
			mTime.playing = playing;
		}
		else {
			// shouldn't be getting redundant signals?
		}
	}
	else if (mCheckSamplePosTransport) {
		if (mLastSample >= 0.0) { 
			bool playing = (mLastSample != time->samplePos);
			if (playing != mTime.playing) {
				mTime.playing = playing;
				if (mTime.playing) {
					trace("VstMobius: PLAY (via sample position) %lf %lf\n",
						  mLastSample,
						  time->samplePos);
					resumed = true;
				}
				else {
					trace("VstMobius: STOP (via sample position)\n");
					// Clear out all sync status
					// or just keep going pretending there are beats and bars?
				}
			}
		}
	}

	// Similar to mCheckSamplePosTransport we could try to detect
	// this with movement of ppqPos.  This seems even less likely
	// to be necessary
	if (mCheckPpqPosTransport) {
		double lastPos = mTime.beatPosition;
		double newPos = time->ppqPos;
		if (lastPos >= 0.0) {
			bool playing = (lastPos != newPos);
			if (playing != mTime.playing) {
				mTime.playing = playing;
				if (playing) {
					trace("VstMobius: PLAY (via ppqPos) %lf %lf\n",
						  lastPos, newPos);
					resumed = true;
				}
				else {
					trace("VstMobius: STOP (via ppqPos)\n");
				}
			}
		}
	}

	return resumed;
}

/**
 * Track host tempo.
 */
PRIVATE void VstMobius::checkTempoOld(VstTimeInfo* time)
{
	if (mTime.tempo != time->tempo) {
		mTime.tempo = time->tempo;
		trace("VstMobius: TEMPO: tempo %lf timeSigNumerator %ld timeSigDenominator %d\n", 
			  mTime.tempo, time->timeSigNumerator, time->timeSigDenominator);
	}



	// calculate the number of beats per frame
	// !! we have the true sample rate in the TimeInfo
	//mBeatsPerFrame = time->tempo / (60.0 * time->sampleRate);

    int framesPerMinute = 60 * mSampleRate;
	double bpf = time->tempo / framesPerMinute;
	if (bpf != mBeatsPerFrame) {
		trace("VstMobius: BeatsPerFrame changing to %lf\n", bpf);
		mBeatsPerFrame = bpf;
	}

	// calculate the number of quarter note beats in a bar
	// this can be fractional for things like 5/8
    // ?? really, it's typed as a long in VstTimeInfo
	if (time->timeSigDenominator == 0) {
		// shouldn't happen but prevent runtime exception just in case
		time->timeSigDenominator = 4;
	}

	double bpb = time->timeSigNumerator / (time->timeSigDenominator / 4);
	if (bpb != mBeatsPerBar) {
		trace("VstMobius: BeatsPerBar changing to %lf\n", bpb);
		mBeatsPerBar = bpb;
	}

    // export this too
    // !! this effects how we wrap mTime.beat, need to be setting this
    // immediately, not waiting for a few interrupts
    // in theory this can be a fraction, but the upper layers don't handle that
    bpb = (double)((int)mBeatsPerBar);
    if (bpb != mBeatsPerBar)
      Trace(1, "VstMobius::checkTempo beatsPerBar not integer %ld (x100)\n",
            (long)(mBeatsPerBar * 100));

    mTime.beatsPerBar = (int)mBeatsPerBar;

}

/****************************************************************************
 *                                                                          *
 *                               NEW TIME CHECK                             *
 *                                                                          *
 ****************************************************************************/

PRIVATE void VstMobius::checkTime(VstInt32 bufferFrames)
{
	bool tempoRequested = false;
	int flags = kVstPpqPosValid | kVstBarsValid;

	// Check for tempo changes every few blocks, actually we
	// could even skip checking time at all under the assumption
	// that ppqPos is going to advance at a constant rate assuming
	// tempo isn't changing.
	
	mTempoBlocks++;
	if (mTempoBlocks >= TEMPO_CHECK_BLOCKS || mTime.tempo == 0) {
		tempoRequested = true;
		flags = flags | kVstTempoValid | kVstTimeSigValid;
		mTempoBlocks = 0;
	}

	VstTimeInfo* time = getTimeInfo(flags);
	if (time != NULL) {
		if (tempoRequested)
          mSyncState->updateTempo(mSampleRate,
                                  time->tempo,
                                  time->timeSigNumerator, 
                                  time->timeSigDenominator);

        mSyncState->advance(bufferFrames, 
                            time->samplePos,
                            time->ppqPos,
                            ((time->flags & kVstTransportChanged) != 0),
                            ((time->flags & kVstTransportPlaying) != 0));

        mSyncState->transfer(&mTime);
    }
	else {
		// full reset of AudioTime?
        Trace(1, "VstMobius:getTimeInfo returned null!\n");
	}

    // An experiment at having the plugin set the host tempo
    // crashes
    bool isSyncMaster = false;
    if (isSyncMaster) {
        setHostTempo(69.0);
    }
}

/****************************************************************************
 *                                                                          *
 *   						VST BUFFER PROCESSING                           *
 *                                                                          *
 ****************************************************************************/

void VstMobius::process(float** inputs, float** outputs, VstInt32 sampleFrames)
{
	processInternal(inputs, outputs, sampleFrames, false);
}

void VstMobius::processReplacing(float** inputs, float** outputs, 
								 VstInt32 sampleFrames)
{
	processInternal(inputs, outputs, sampleFrames, true);
}


PRIVATE void VstMobius::processInternal(float** inputs, float** outputs, 
										VstInt32 sampleFrames, bool replace)
{
    if (mSyncState != NULL)
      checkTime(sampleFrames);
    else
      checkTimeOld(sampleFrames);

	if (inputs == NULL) 
	  Trace(1, "VstMobius::processInternal null input array\n");
	else if (outputs == NULL)
	  Trace(1, "VstMobius::processInternal null input array\n");
	else if (!mProcessing) {
		// supposed to not get called if stopProcess was called
		trace("VstMobius not processing\n");
	}
	else if (mHandler != NULL) {

		if (sampleFrames > MAX_VST_FRAMES)
		  Trace(1, "VstMobius::processInternal Too many VST frames!\n");
		else if (sampleFrames == 0) {
		  Trace(1, "VstMobius::processInternal No frames to process!\n");
		}
		else {
			// todo: may want different channels per port
			int channels = getPortChannels();

			mInterruptInputs = inputs;
			mInterruptOutputs = outputs;
			mInterruptFrames = sampleFrames;

			for (int i = 0 ; i < MAX_VST_PORTS ; i++) {
				VstPort* port = &mPorts[i];
				port->inputPrepared = false;
				port->outputPrepared = false;
			}

			// have to call this even if in bypass to keep the
			// machinery running, if necessary could figure
			// out a lighter weight way to do this?
			// even though we ignore outputs, should we also
			// ignore inputs?

            // mHandler is normally the same as mPlugin
            // but it registers itself through the AudioStream interface
            // it calls back to getInterruptBuffers
			mHandler->processAudioBuffers(mStream);

            // tell the host about parameters changed during this
            // processing cycle
            exportParameters();

			if (mBypass) {
				// copy inputs to outputs
				// !! need to support in/out ports of different size
				int inports = mInputPins / channels;
				for (int p = 0 ; p < inports ; p++) {
					int portbase = p * channels;
					for (int c = 0 ; c < channels ; c++) {
						float* output = outputs[portbase + c];
						if (output != NULL) {
							float* input = inputs[portbase + c];
							if (input != NULL) {
								for (int i = 0 ; i < sampleFrames ; i++) {
									if (replace)
									  output[i] = input[i];
									else
									  output[i] += input[i];
								}
							}
							else if (replace) {
								// if replace on, should we erase
								// current contents?
								for (int i = 0 ; i < sampleFrames ; i++)
								  output[i] = 0;
							}
						}
					}
				}
			}
			else {
				// !! need to support variable numbers if in/out pins
				int inports = mInputPins / channels;
				for (int p = 0 ; p < inports ; p++) {
					VstPort* port = &mPorts[p];
					int portbase = p * channels;
					for (int c = 0 ; c < channels ; c++) {
						float* output = outputs[portbase + c];
						if (output != NULL) {
							if (port->outputPrepared) {
								float* src = port->output;
								int sample = c;
								for (int i = 0 ; i < sampleFrames ; i++) {
									if (replace)
									  output[i] = src[sample];
									else
									  output[i] += src[sample];
									sample += channels;
								}
							}
							else if (replace) {
								// if replace on, should we erase
								// current contents?
								for (int i = 0 ; i < sampleFrames ; i++)
								  output[i] = 0;
							}
						}
					}
				}
			}
		}

        // send MIDI events that accumulated during this cycle
        sendMidiEvents();
	}
}

/**
 * AudioStream callback.
 */
PUBLIC void VstMobius::getInterruptBuffers(int inport, float** inbuf,
										   int outport, float** outbuf)
{
	int channels = getPortChannels();

	if (inbuf != NULL) {
		int inports = mInputPins / channels;
		if (inport >= 0 && inport < inports) {
			VstPort* port = &mPorts[inport];
			if (!port->inputPrepared) {
				mergeBuffers(port->input, mInterruptInputs, 
							 inport, mInterruptFrames);
				port->inputPrepared = true;
			}
			*inbuf = port->input;
		}
		else {
			// !! invalid port, return an empty buffer?
		}
	}

	if (outbuf != NULL) {
		int outports = mOutputPins / channels;
		if (outport >= 0 && outport < outports) {
			VstPort* port = &mPorts[outport];
			if (!port->outputPrepared) {
				int channels = getPortChannels();
				int floats = mInterruptFrames * channels;
				memset(port->output, 0, (sizeof(float) * floats));
				port->outputPrepared = true;
			}
			*outbuf = port->output;
		}
		else {
			// !! invalid port, return dummy buffer?
		}
	}
}

PRIVATE void VstMobius::mergeBuffers(float* dest, float** sources, 
									 int port, long frames)
{
	int channels = getPortChannels();
	int portbase = port * channels;
	int sample = 0;

	// !! fix this, not pipelining well

	for (int i = 0 ; i < frames ; i++) {
		for (int j = 0 ; j < channels ; j++) {
			float* src = sources[portbase + j];
			if (src != NULL) 
			  dest[sample++] = src[i];
			else 
			  dest[sample++] = 0.0;
		}
	}
}

//////////////////////////////////////////////////////////////////////
//
// HostInterface
//
//////////////////////////////////////////////////////////////////////

PluginInterface* VstMobius::getPlugin()
{
	return mPlugin;
}

Context* VstMobius::getContext()
{
	return mContext;
}

const char* VstMobius::getHostName()
{
	return mHostProduct;
}

const char* VstMobius::getHostVersion()
{
	return mHostVersion;
}

AudioInterface* VstMobius::getAudioInterface()
{
	return this;
}

/**
 * Who calls this?  If this is for the plugin to convey
 * parameter changes to the host we're doing that via the
 * PluginParameter interface now so we don't need this!!
 */
void VstMobius::notifyParameter(int id, float value)
{
}

//////////////////////////////////////////////////////////////////////
//
// AudioInterface
//
// Stubbed out implementation of AudioStream to pass to Mobius
// via the MobiusContext.  The only interesting thing for VST is
// the AudioStream class.
// 
//////////////////////////////////////////////////////////////////////

void VstMobius::terminate()
{
}

AudioDevice** VstMobius::getDevices()
{
	return NULL;
}

AudioDevice* VstMobius::getDevice(int id)
{
	return NULL;
}

AudioDevice* VstMobius::getDevice(const char* name, bool output)
{
	return NULL;
}

void VstMobius::printDevices()
{
}

AudioStream* VstMobius::getStream()
{
	return mStream;
}

//////////////////////////////////////////////////////////////////////
//
// AudioStreamProxy
//
//////////////////////////////////////////////////////////////////////

AudioStreamProxy::AudioStreamProxy(VstMobius* vst)
{
    mVst = vst;
}

AudioStreamProxy::~AudioStreamProxy()
{
}

AudioInterface* AudioStreamProxy::getInterface()
{
    // back at ya
	return mVst;
}

int AudioStreamProxy::getInputChannels()
{
    // 2 channel port assumption!!
    return getInputPorts() * 2;
}

int AudioStreamProxy::getInputPorts()
{
    // AU uses this which is 16
	//return MAX_HOST_PLUGIN_PORTS;

    // we have historically used this which is 8
	return MAX_VST_PORTS;
}

int AudioStreamProxy::getOutputChannels()
{
    // 2 channel port assumption!!
    return getOutputPorts() * 2;
}

int AudioStreamProxy::getOutputPorts()
{
    // AU uses this which is 16
	//return MAX_HOST_PLUGIN_PORTS;

    // we have historically used this which is 8
	return MAX_VST_PORTS;
}

bool AudioStreamProxy::setInputDevice(int id)
{
	// have to implement these but they have no effect
	return true;
}

bool AudioStreamProxy::setInputDevice(const char* name)
{
	return true;
}

bool AudioStreamProxy::setOutputDevice(int id)
{
	return true;
}

bool AudioStreamProxy::setOutputDevice(const char* name)
{
	return true;
}

void AudioStreamProxy::setSuggestedLatencyMsec(int i)
{
}

/**
 * !! Could fake up a device to represent the AU/VST ports?
 */
AudioDevice* AudioStreamProxy::getInputDevice() 
{
	return NULL;
}

AudioDevice* AudioStreamProxy::getOutputDevice() 
{
	return NULL;
}

int AudioStreamProxy::getSampleRate() 
{
    // AudioEffect::getSampleRate returns a float
	return mVst->getSampleRateInt();
}

void AudioStreamProxy::setSampleRate(int rate)
{
	// can't be set
}

AudioHandler* AudioStreamProxy::getHandler()
{
	return mVst->getHandler();
}

void AudioStreamProxy::setHandler(AudioHandler* h)
{
	mVst->setHandler(h);
}

const char* AudioStreamProxy::getLastError()
{
    return mVst->getLastError();
}

bool AudioStreamProxy::open()
{
	return true;
}

int AudioStreamProxy::getInputLatencyFrames()
{
	return mVst->getInputLatencyFrames();
}

void AudioStreamProxy::setInputLatencyFrames(int frames)
{
    mVst->setInputLatencyFrames(frames);
}

int AudioStreamProxy::getOutputLatencyFrames()
{
    return mVst->getOutputLatencyFrames();
}

void AudioStreamProxy::setOutputLatencyFrames(int frames)
{
    mVst->setOutputLatencyFrames(frames);
}

void AudioStreamProxy::close()
{
	printStatistics();
}

void AudioStreamProxy::printStatistics()
{
}

//
// Buffer Processing
//

PUBLIC long AudioStreamProxy::getInterruptFrames()
{
	return mVst->getInterruptFrames();
}

PUBLIC void AudioStreamProxy::getInterruptBuffers(int inport, float** inbuf, 
                                                  int outport, float** outbuf)
{
    mVst->getInterruptBuffers(inport, inbuf, outport, outbuf);
}

PUBLIC AudioTime* AudioStreamProxy::getTime()
{
	return mVst->getTime();
}

PUBLIC double AudioStreamProxy::getStreamTime()
{
    return 0.0f;
}

PUBLIC double AudioStreamProxy::getLastInterruptStreamTime()
{
    return 0.0f;
}

//////////////////////////////////////////////////////////////////////
//
// AudioStreamProxy -> VstMobius callbacks
//
//////////////////////////////////////////////////////////////////////

int VstMobius::getPortChannels() 
{
    // !! need more flexibility
	return 2;
}

AudioHandler* VstMobius::getHandler()
{
	return mHandler;
}

void VstMobius::setHandler(AudioHandler* h)
{
	mHandler = h;
}

const char* VstMobius::getLastError()
{
	const char* msg = NULL;
	if (mError[0])
	  msg = mError;
	return msg;
}

int VstMobius::getInputLatencyFrames()
{
	return mInputLatency;
}

void VstMobius::setInputLatencyFrames(int frames)
{
	if (frames > 0)
	  mInputLatency = frames;
	else
	  mInputLatency = 512;
}

int VstMobius::getOutputLatencyFrames()
{
    return mOutputLatency;
}

void VstMobius::setOutputLatencyFrames(int frames)
{
	if (frames > 0)
	  mOutputLatency = frames;
	else
	  mOutputLatency = 512;
}

PUBLIC long VstMobius::getInterruptFrames()
{
    return mInterruptFrames;
}

PUBLIC AudioTime* VstMobius::getTime()
{
	return &mTime;
}

//////////////////////////////////////////////////////////////////////
//
// EDITOR
//
//////////////////////////////////////////////////////////////////////

VstMobiusEditor::VstMobiusEditor(VstMobius* vst) : VstEditor(vst) 
{
	mTrace = true;
	mVst = vst;
	mVst->setEditor(this);
}

/**
 * Called by VstMobius when it is being destructed.
 * This may not be necessary but I'm paranoid since the host
 * deletes the editor after the plugin.  
 */
void VstMobiusEditor::disconnect()
{
    mVst = NULL;
}

/**
 * This is supposed to be called by the host after destructing the
 * parent (VstMobius an AudioEffectX subclass).
 * 
 * After the MobiusPlugin refactoring we've got a bit of an ordering
 * problem.  MobiusPlugin wants to reclaim resources for the 
 * editor window, but it isn't technically deleted until we get here.
 * If we're careful and detach everything from the host window
 * it should be okay.  If not, we'll have to transition the 
 * MobiusPlugin from VstMobius to VstMobiusEditor when VstMobius
 * is destructed and wait for VstMobiusEditor to delete
 * the MobiusPlugin.  Whew.
 */
VstMobiusEditor::~VstMobiusEditor()
{
    if (mVst != NULL) {
        // editor deleted before parent, not supposed to happen
        Trace(1, "VstMobiusEditor destructing before parent");

        // I guess it's okay to try this as a last resort, 
        // probably means that VstMobius is leaking or the 
        // host is getting the order wrong
        PluginInterface* plugin = mVst->getPlugin();
        if (plugin != NULL)
          plugin->closeWindow();
    }
}

VstLongBool VstMobiusEditor::getRect(ERect **rect)
{ 
    if (mVst == NULL)
      Trace(1, "VstMobiusEditor::getRect called after being disconnected");
    else {

        // this happens a lot under OrionPro, avoid clutter in the trace
        //if (mTrace)
        //trace("VstEditor::getRect\n");

        // Chainer crashes if you don't return something
        // AEffGUIEditor uses a member, so we apparently own this
    
        PluginInterface* plugin = mVst->getPlugin();
        int left, top, width, height;
        plugin->getWindowRect(&left, &top, &width, &height);

        mRect.top = top;
        mRect.left = left;
        mRect.bottom = mRect.top + height;
        mRect.right = mRect.left + width;

        // who owns this?
        // tried copying to apease Reaper but it didn't work
        bool copyRect = false;
        if (!copyRect) {
            *rect = &mRect;
        }
        else {
            ERect* r = (ERect*)malloc(sizeof(ERect));
            r->top = mRect.top;
            r->left = mRect.left;
            r->bottom = mRect.bottom;
            r->right = mRect.right;
            *rect = r;
        }

    }
    
	// VSTGUI returns true, does that mean we get to make our own window?
	return true;
}

/**
 * I don't know how Bidule does this on the Mac but the WindowRef we get
 * has a content size of the height we request, but the width may be larger
 * if the request was narrower than the default row of control bottoms
 * at the top.  The control buttons do not however appear to be part of the
 * window.  The "structure region" does not include them and the 
 * "content region" has the same dimension as the structure region,
 * and "title bar height" is zero.
 *
 * The x/y coordinate of structure region is however 4,87 which
 * looks about right for a left/top inset in a parent window.  
 * But I don't know how you make parent  windows on the Mac.
 * AFAIK you can't embed windows as a HIView, maybe it is a funny
 * kind of "drawer" or fixed position MDI document window.
 * 
 * At any rate it looks like we can just treat this as a borderless
 * window, mouse coords come in right.
 *
 */
VstLongBool VstMobiusEditor::open(void *ptr)
{ 
	VstLongBool status = VstEditor::open(ptr);

    if (mVst == NULL)
      Trace(1, "VstMobiusEditor::open called after being disconnected");
    else {
        PluginInterface* plugin = mVst->getPlugin();
        plugin->openWindow(ptr, NULL);
    }

	return status;
}

/**
 * I'm pretty sure this has to be called before the VstPlugin
 * is destructued, otherwise mVst will be invalid!
 */
void VstMobiusEditor::close()
{ 
	VstEditor::close();

    if (mVst == NULL) {
        // this is unusual, we expect the close call before 
        // the parent plugin is deleted
        Trace(1, "VstMobiusEditor::close called after being disconnected");
    }
    else {
        PluginInterface* plugin = mVst->getPlugin();
        plugin->closeWindow();
    }
}

/**
 * Handle a key down key event.  Return true if key was used.
 */
VstLongBool VstMobiusEditor::onKeyDown(VstKeyCode &keyCode)
{
    VstEditor::onKeyDown(keyCode);
    char buffer[128];

    int key = TranslateVstKeyCode(keyCode.character, keyCode.virt, keyCode.modifier);
    GetKeyString(key, buffer);

    trace("keyDown %d %d %d %d %s\n", 
          (int)keyCode.character,
          (int)keyCode.virt,
          (int)keyCode.modifier,
          key,
          buffer);

    return false;
}

/**
 * Handle a key up event.  Return true if key was used.
 */
VstLongBool VstMobiusEditor::onKeyUp(VstKeyCode &keyCode)
{
    VstEditor::onKeyUp(keyCode);

    int key = TranslateVstKeyCode(keyCode.character, keyCode.virt, keyCode.modifier);
    //trace("keyUp %d %s\n", key, Character::getString(key));

    return false;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
