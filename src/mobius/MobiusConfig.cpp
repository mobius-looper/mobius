/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Model for the Mobius core configuration.
 * UIConfig has a model for most of the UI configuration.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "Util.h"
#include "MidiUtil.h"
#include "XmlModel.h"
#include "XmlBuffer.h"
#include "XomParser.h"
#include "Qwin.h"

#include "Binding.h"
#include "Function.h"
#include "Mobius.h"
#include "OscConfig.h"
#include "Parameter.h"
#include "Resampler.h"
#include "Sample.h"
#include "Script.h"
#include "Setup.h"

// temporary
#include "OldBinding.h"

#include "MobiusConfig.h"

//////////////////////////////////////////////////////////////////////
//
// XML Constants
//
//////////////////////////////////////////////////////////////////////

#define EL_CONFIG "MobiusConfig"
#define ATT_LANGUAGE "language"
#define ATT_SETUP "setup"
#define ATT_MIDI_CONFIG "midiConfig"
#define ATT_SUGGESTED_LATENCY "suggestedLatencyMsec"
#define ATT_UI_CONFIG  "uiConfig"
#define ATT_PLUGIN_PINS "pluginPins"
#define ATT_PLUGIN_HOST_REWINDS "pluginHostRewinds"

#define ATT_NO_SYNC_BEAT_ROUNDING "noSyncBeatRounding"

#define ATT_OVERLAY_BINDINGS "overlayBindings"

#define EL_FOCUS_LOCK_FUNCTIONS "FocusLockFunctions"
// old name for FocusLockFunctions
#define EL_GROUP_FUNCTIONS "GroupFunctions"
#define EL_MUTE_CANCEL_FUNCTIONS "MuteCancelFunctions"
#define EL_CONFIRMATION_FUNCTIONS "ConfirmationFunctions"
#define EL_ALT_FEEDBACK_DISABLES "AltFeedbackDisables"
#define EL_STRING "String"

#define EL_SCRIPT_CONFIG "ScriptConfig"
#define EL_SCRIPT_REF "ScripRef"
#define ATT_FILE "file"

#define EL_CONTROL_SURFACE "ControlSurface"
#define ATT_NAME "name"

#define EL_OSC_CONFIG "OscConfig"

#define ATT_LOG_STATUS "logStatus"
#define ATT_EDPISMS "edpisms"

/****************************************************************************
 *                                                                          *
 *   							  UTILITIES                                 *
 *                                                                          *
 ****************************************************************************/

PUBLIC int XmlGetEnum(XmlElement* e, const char *name, const char** names)
{
	int value = 0;
	const char *attval = e->getAttribute(name);
	if (attval != NULL) {
		for (int i = 0 ; names[i] != NULL ; i++) {
			if (!strcmp(attval, names[i])) {
				value = i;
				break;
			}
		}
	}
	return value;
}

PUBLIC int XmlGetEnum(const char* str, const char** names)
{
	int value = 0;
	if (str != NULL) {
		for (int i = 0 ; names[i] != NULL ; i++) {
			if (!strcmp(str, names[i])) {
				value = i;
				break;
			}
		}
	}
	return value;
}

/****************************************************************************
 *                                                                          *
 *   								CONFIG                                  *
 *                                                                          *
 ****************************************************************************/

PUBLIC MobiusConfig::MobiusConfig()
{
	init();
}

PUBLIC MobiusConfig::MobiusConfig(bool dflt)
{
	init();
    mDefault = dflt;
}

PUBLIC MobiusConfig::MobiusConfig(const char *xml)
{
	init();
	parseXml(xml);
}


PUBLIC void MobiusConfig::init()
{
    mError[0] = 0;
    mDefault = false;
    mHistory = NULL;
	mLanguage = NULL;
	mMidiInput = NULL;
	mMidiOutput = NULL;
	mMidiThrough = NULL;
	mPluginMidiInput = NULL;
	mPluginMidiOutput = NULL;
	mPluginMidiThrough = NULL;
	mAudioInput = NULL;
	mAudioOutput = NULL;
	mUIConfig = NULL;
	mQuickSave = NULL;
    mCustomMessageFile = NULL;
	mUnitTests = NULL;

	mNoiseFloor = DEFAULT_NOISE_FLOOR;
	mSuggestedLatency = 0;
	mInputLatency = 0;
	mOutputLatency = 0;
	mFadeFrames = AUDIO_DEFAULT_FADE_FRAMES;
	mMaxSyncDrift = DEFAULT_MAX_SYNC_DRIFT;
	mTracks = DEFAULT_TRACKS;
    mTrackGroups = DEFAULT_TRACK_GROUPS;
    mMaxLoops = DEFAULT_MAX_LOOPS;
    mLongPress = DEFAULT_LONG_PRESS_MSECS;

	mFocusLockFunctions = NULL;
	mMuteCancelFunctions = NULL;
	mConfirmationFunctions = NULL;
	mAltFeedbackDisables = NULL;

	mPresets = NULL;
	mPreset = NULL;
	mSetups = NULL;
	mSetup = NULL;
	mBindingConfigs = NULL;
    mOverlayBindings = NULL;
	mMidiConfigs = NULL;
    mSelectedMidiConfig = NULL;
    mScriptConfig = NULL;
	mControlSurfaces = NULL;
	mOscConfig = NULL;
	mSamples = NULL;
    mSampleRate = SAMPLE_RATE_44100;

	mMonitorAudio = false;
    mHostRewinds = false;
	mPluginPins = DEFAULT_PLUGIN_PINS;
    mAutoFeedbackReduction = false;
    mIsolateOverdubs = false;
    mIntegerWaveFile = false;
	mSpreadRange = DEFAULT_SPREAD_RANGE;
	mTracePrintLevel = 1;
	mTraceDebugLevel = 2;
	mSaveLayers = false;
	mDriftCheckPoint = DRIFT_CHECK_LOOP;
	mMidiRecordMode = MIDI_TEMPO_AVERAGE;
    mMidiExport = false;
    mHostMidiExport = false;
    mGroupFocusLock = false;

    mNoPresetChanges = false;
    mNoSetupChanges = false;

    // this causes confusion when not on since key bindings often don't work
#ifdef _WIN32
    mDualPluginWindow = true;
#else
    mDualPluginWindow = false;  
#endif

    mOscEnable = false;
    mOscTrace = false;
    mOscInputPort = 7000;
    mOscOutputPort = 8000;
    mOscOutputHost = NULL;

    mNoSyncBeatRounding = false;
    mLogStatus = false;

    mEdpisms = false;
}

PUBLIC MobiusConfig::~MobiusConfig()
{
    // delete the history list if we have one
	MobiusConfig *el, *next;
	for (el = mHistory ; el != NULL ; el = next) {
		next = el->getHistory();
		el->setHistory(NULL);
		delete el;
	}

	delete mLanguage;
    delete mMidiInput;
    delete mMidiOutput;
    delete mMidiThrough;
    delete mPluginMidiInput;
    delete mPluginMidiOutput;
    delete mPluginMidiThrough;
    delete mAudioInput;
    delete mAudioOutput;
	delete mUIConfig;
	delete mQuickSave;
    delete mCustomMessageFile;
	delete mUnitTests;

	delete mFocusLockFunctions;
	delete mMuteCancelFunctions;
	delete mConfirmationFunctions;
	delete mAltFeedbackDisables;
	delete mPresets;
    delete mSetups;
    delete mBindingConfigs;
    delete mMidiConfigs;
    delete mSelectedMidiConfig;
    delete mScriptConfig;
    delete mControlSurfaces;
	delete mOscConfig;
	delete mSamples;
}

PUBLIC bool MobiusConfig::isDefault()
{
    return mDefault;
}

PUBLIC MobiusConfig* MobiusConfig::clone()
{
    char* xml = toXml();
    MobiusConfig* clone = new MobiusConfig(xml);
    delete xml;

    clone->setCurrentPreset(getCurrentPresetIndex());
	clone->setCurrentSetup(getCurrentSetupIndex());
    clone->setOverlayBindingConfig(getOverlayBindingConfigIndex());

    // these aren't handled by XML serialization
    clone->mNoPresetChanges = mNoPresetChanges;
    clone->mNoSetupChanges = mNoSetupChanges;

    return clone;
}

PUBLIC void MobiusConfig::setHistory(MobiusConfig* config)
{
    mHistory = config;
}

PUBLIC MobiusConfig* MobiusConfig::getHistory()
{
    return mHistory;
}

PUBLIC int MobiusConfig::getHistoryCount()
{
    int count = 0;
    for (MobiusConfig* c = this ; c != NULL ; c = c->getHistory())
      count++;
    return count;
}

/**
 * Number the presets, setups, or binding configs after editing.
 */
PRIVATE void MobiusConfig::numberThings(Bindable* things)
{
	int count = 0;
	for (Bindable* b = things ; b != NULL ; b = b->getNextBindable())
	  b->setNumber(count++);
}

PRIVATE int MobiusConfig::countThings(Bindable* things)
{
    int count = 0;
    for (Bindable* b = things ; b != NULL ; b = b->getNextBindable())
      count++;
    return count;
}

PUBLIC const char* MobiusConfig::getLanguage()
{
	return mLanguage;
}

PUBLIC void MobiusConfig::setLanguage(const char* lang)
{
	delete mLanguage;
	mLanguage = CopyString(lang);
}

void MobiusConfig::setMonitorAudio(bool b)
{
	mMonitorAudio = b;
}

bool MobiusConfig::isMonitorAudio()
{
	return mMonitorAudio;
}

void MobiusConfig::setPluginPins(int i)
{
    // zero looks confusing in the UI, default it if we have 
    // an old config file
    if (i == 0) i = DEFAULT_PLUGIN_PINS;
	mPluginPins = i;
}

int MobiusConfig::getPluginPins()
{
	return mPluginPins;
}

/**
 * Pseudo property to expose the pin count as "ports" which
 * are sets of stereo pins.  Ports are what we deal within all other
 * places so this makes a more logical global parameter.
 */
int MobiusConfig::getPluginPorts()
{
	return (mPluginPins / 2);
}

void MobiusConfig::setPluginPorts(int i) 
{
	mPluginPins = i * 2;
}

void MobiusConfig::setHostRewinds(bool b)
{
	mHostRewinds = b;
}

bool MobiusConfig::isHostRewinds()
{
	return mHostRewinds;
}

void MobiusConfig::setAutoFeedbackReduction(bool b)
{
	mAutoFeedbackReduction = b;
}

bool MobiusConfig::isAutoFeedbackReduction()
{
	return mAutoFeedbackReduction;
}

void MobiusConfig::setIsolateOverdubs(bool b)
{
	mIsolateOverdubs = b;
}

bool MobiusConfig::isIsolateOverdubs()
{
	return mIsolateOverdubs;
}

void MobiusConfig::setIntegerWaveFile(bool b)
{
	mIntegerWaveFile = b;
}

bool MobiusConfig::isIntegerWaveFile()
{
	return mIntegerWaveFile;
}

void MobiusConfig::setSpreadRange(int i)
{
	// backward compatibility with old files
	if (i <= 0) 
      i = DEFAULT_SPREAD_RANGE;
    else if (i > MAX_RATE_STEP)
      i = MAX_RATE_STEP;

	mSpreadRange = i;
}

int MobiusConfig::getSpreadRange()
{
	return mSpreadRange;
}

PUBLIC const char* MobiusConfig::getMidiInput() {
	return mMidiInput;
}

PUBLIC void MobiusConfig::setMidiInput(const char* s) {
    delete mMidiInput;
	mMidiInput = CopyString(s);
}

PUBLIC const char* MobiusConfig::getMidiOutput() {
	return mMidiOutput;
}

PUBLIC void MobiusConfig::setMidiOutput(const char* s) {
    delete mMidiOutput;
	mMidiOutput = CopyString(s);
}

PUBLIC const char* MobiusConfig::getMidiThrough() {
	return mMidiThrough;
}

PUBLIC void MobiusConfig::setMidiThrough(const char* s) {
    delete mMidiThrough;
	mMidiThrough = CopyString(s);
}

PUBLIC const char* MobiusConfig::getPluginMidiInput() {
	return mPluginMidiInput;
}

PUBLIC void MobiusConfig::setPluginMidiInput(const char* s) {
    delete mPluginMidiInput;
	mPluginMidiInput = CopyString(s);
}

PUBLIC const char* MobiusConfig::getPluginMidiOutput() {
	return mPluginMidiOutput;
}

PUBLIC void MobiusConfig::setPluginMidiOutput(const char* s) {
    delete mPluginMidiOutput;
	mPluginMidiOutput = CopyString(s);
}

PUBLIC const char* MobiusConfig::getPluginMidiThrough() {
	return mPluginMidiThrough;
}

PUBLIC void MobiusConfig::setPluginMidiThrough(const char* s) {
    delete mPluginMidiThrough;
	mPluginMidiThrough = CopyString(s);
}

PUBLIC const char* MobiusConfig::getAudioInput() {
	return mAudioInput;
}

PUBLIC void MobiusConfig::setAudioInput(const char* s) {
	delete mAudioInput;
    mAudioInput = CopyString(s);
}

PUBLIC const char* MobiusConfig::getAudioOutput() {
	return mAudioOutput;
}

PUBLIC void MobiusConfig::setAudioOutput(const char* s) {
	delete mAudioOutput;
	mAudioOutput = CopyString(s);
}

PUBLIC AudioSampleRate MobiusConfig::getSampleRate() {
	return mSampleRate;
}

PUBLIC void MobiusConfig::setSampleRate(AudioSampleRate rate) {
	mSampleRate = rate;
}

PUBLIC void MobiusConfig::setTracePrintLevel(int i) {
	mTracePrintLevel = i;
}

PUBLIC int MobiusConfig::getTracePrintLevel() {
	return mTracePrintLevel;
}

PUBLIC void MobiusConfig::setTraceDebugLevel(int i) {
	mTraceDebugLevel = i;
}

PUBLIC int MobiusConfig::getTraceDebugLevel() {
	return mTraceDebugLevel;
}

PUBLIC void MobiusConfig::setSaveLayers(bool b) {
	mSaveLayers = b;
}

PUBLIC bool MobiusConfig::isSaveLayers() {
	return mSaveLayers;
}

PUBLIC int MobiusConfig::getNoiseFloor()
{
	return mNoiseFloor;
}

PUBLIC void MobiusConfig::setNoiseFloor(int i)
{
    // this has been stuck zero for quite awhile, initialize it
    if (i == 0) i = DEFAULT_NOISE_FLOOR;
	mNoiseFloor = i;
}

PUBLIC int MobiusConfig::getTracks()
{
	return mTracks;
}

PUBLIC void MobiusConfig::setTracks(int i)
{
	if (i == 0) i = DEFAULT_TRACKS;
	mTracks = i;
}

PUBLIC int MobiusConfig::getTrackGroups()
{
	return mTrackGroups;
}

PUBLIC void MobiusConfig::setTrackGroups(int i)
{
	mTrackGroups = i;
}

PUBLIC int MobiusConfig::getMaxLoops()
{
	return mMaxLoops;
}

PUBLIC void MobiusConfig::setMaxLoops(int i)
{
	mMaxLoops = i;
}

PUBLIC void MobiusConfig::setSuggestedLatencyMsec(int i)
{
	mSuggestedLatency = i;
}

PUBLIC int MobiusConfig::getSuggestedLatencyMsec()
{
	return mSuggestedLatency;
}

PUBLIC int MobiusConfig::getInputLatency()
{
	return mInputLatency;
}

PUBLIC void MobiusConfig::setInputLatency(int i)
{
	mInputLatency = i;
}

PUBLIC int MobiusConfig::getOutputLatency()
{
	return mOutputLatency;
}

PUBLIC void MobiusConfig::setOutputLatency(int i)
{
	mOutputLatency = i;
}

/**
 * Hmm, wanted to let 0 default because upgrades won't have
 * this parameter set.  But this leaves no way to turn off long presses.
 */
PUBLIC void MobiusConfig::setLongPress(int i)
{
	if (i <= 0) 
	  i = DEFAULT_LONG_PRESS_MSECS;
	mLongPress = i;
}

PUBLIC int MobiusConfig::getLongPress()
{
	return mLongPress;
}

/**
 * Originally this was a configurable parameter but the
 * range had to be severly restricted to prevent stack
 * overflow since fade buffers are allocated on the stack.
 * With the reduced range there isn't much need to set this
 * so force it to 128.
 */
PUBLIC int MobiusConfig::getFadeFrames()
{
	return mFadeFrames;
}

PUBLIC void MobiusConfig::setFadeFrames(int i)
{
    // force this to a normal value
	if (i <= 0)
      i = AUDIO_DEFAULT_FADE_FRAMES;

    else if (i < AUDIO_MIN_FADE_FRAMES)
      i = AUDIO_MIN_FADE_FRAMES;

    else if (i > AUDIO_MAX_FADE_FRAMES)
      i = AUDIO_MAX_FADE_FRAMES;

    mFadeFrames = i;
}

PUBLIC int MobiusConfig::getMaxSyncDrift()
{
	return mMaxSyncDrift;
}

PUBLIC void MobiusConfig::setMaxSyncDrift(int i)
{
    // this was stuck low for many people, try to correct that
    if (i == 0) i = 512;
    mMaxSyncDrift = i;
}

void MobiusConfig::setDriftCheckPoint(DriftCheckPoint dcp)
{
	mDriftCheckPoint = dcp;
}

DriftCheckPoint MobiusConfig::getDriftCheckPoint()
{
	return mDriftCheckPoint;
}

PUBLIC ScriptConfig* MobiusConfig::getScriptConfig()
{
    if (mScriptConfig == NULL)
      mScriptConfig = new ScriptConfig();
    return mScriptConfig;
}

PUBLIC void MobiusConfig::setScriptConfig(ScriptConfig* dc)
{
    if (dc != mScriptConfig) {
        delete mScriptConfig;
        mScriptConfig = dc;
    }
}

PUBLIC ControlSurfaceConfig* MobiusConfig::getControlSurfaces()
{
    return mControlSurfaces;
}

PUBLIC void MobiusConfig::setControlSurfaces(ControlSurfaceConfig* list)
{
	if (list != mControlSurfaces) {
		delete mControlSurfaces;
		mControlSurfaces = list;
	}
}

PUBLIC void MobiusConfig::addControlSurface(ControlSurfaceConfig* cs)
{
	// keep them ordered
	ControlSurfaceConfig *prev;
	for (prev = mControlSurfaces ; prev != NULL && prev->getNext() != NULL ; 
		 prev = prev->getNext());

	if (prev == NULL)
	  mControlSurfaces = cs;
	else
	  prev->setNext(cs);
}

PUBLIC OscConfig* MobiusConfig::getOscConfig()
{
	return mOscConfig;
}

PUBLIC void MobiusConfig::setOscConfig(OscConfig* c)
{
	if (c != mOscConfig) {
		delete mOscConfig;
		mOscConfig = c;
	}
}

PUBLIC void MobiusConfig::setUIConfig(const char* s) 
{
	delete mUIConfig;
	mUIConfig = CopyString(s);
}

PUBLIC const char* MobiusConfig::getUIConfig()
{
	return mUIConfig;
}

PUBLIC void MobiusConfig::setQuickSave(const char* s) 
{
	delete mQuickSave;
	mQuickSave = CopyString(s);
}

PUBLIC const char* MobiusConfig::getQuickSave()
{
	return mQuickSave;
}

PUBLIC void MobiusConfig::setCustomMessageFile(const char* s) 
{
	delete mCustomMessageFile;
	mCustomMessageFile = CopyString(s);
}

PUBLIC const char* MobiusConfig::getCustomMessageFile()
{
	return mCustomMessageFile;
}

PUBLIC void MobiusConfig::setUnitTests(const char* s) 
{
	delete mUnitTests;
	mUnitTests = CopyString(s);
}

PUBLIC const char* MobiusConfig::getUnitTests()
{
	return mUnitTests;
}

PUBLIC void MobiusConfig::setSamples(Samples* s)
{
	if (mSamples != s) {
		delete mSamples;
		mSamples = s;
	}
}

PUBLIC Samples* MobiusConfig::getSamples()
{
	return mSamples;
}

PUBLIC StringList* MobiusConfig::getFocusLockFunctions()
{
	return mFocusLockFunctions;
}

PUBLIC void MobiusConfig::setFocusLockFunctions(StringList* l) 
{
	delete mFocusLockFunctions;
	mFocusLockFunctions = l;
}

PUBLIC StringList* MobiusConfig::getMuteCancelFunctions()
{
	return mMuteCancelFunctions;
}

PUBLIC void MobiusConfig::setMuteCancelFunctions(StringList* l) 
{
	delete mMuteCancelFunctions;
	mMuteCancelFunctions = l;
}

PUBLIC StringList* MobiusConfig::getConfirmationFunctions()
{
	return mConfirmationFunctions;
}

PUBLIC void MobiusConfig::setConfirmationFunctions(StringList* l) 
{
	delete mConfirmationFunctions;
	mConfirmationFunctions = l;
}

PUBLIC StringList* MobiusConfig::getAltFeedbackDisables() 
{
	return mAltFeedbackDisables;
}

PUBLIC void MobiusConfig::setAltFeedbackDisables(StringList* l) 
{
	delete mAltFeedbackDisables;
	mAltFeedbackDisables = l;
}

PUBLIC void MobiusConfig::setMidiRecordMode(MidiRecordMode mode) {
	mMidiRecordMode = mode;
}

PUBLIC MidiRecordMode MobiusConfig::getMidiRecordMode() {
	return mMidiRecordMode;
}

PUBLIC void MobiusConfig::setDualPluginWindow(bool b) {
	mDualPluginWindow = b;
}

PUBLIC bool MobiusConfig::isDualPluginWindow() {
	return mDualPluginWindow;
}

PUBLIC void MobiusConfig::setMidiExport(bool b) {
	mMidiExport = b;
}

PUBLIC bool MobiusConfig::isMidiExport() {
	return mMidiExport;
}

PUBLIC void MobiusConfig::setHostMidiExport(bool b) {
	mHostMidiExport = b;
}

PUBLIC bool MobiusConfig::isHostMidiExport() {
	return mHostMidiExport;
}

PUBLIC void MobiusConfig::setGroupFocusLock(bool b) {
	mGroupFocusLock = b;
}

PUBLIC bool MobiusConfig::isGroupFocusLock() {
	return mGroupFocusLock;
}

/**
 * Ensure that all of the presets and midi configs have names.
 * Necessary so they can be identified in a GUI.
 */
PUBLIC void MobiusConfig::generateNames()
{
    generateNames(mPresets, "Preset", NULL);
    generateNames(mSetups, "Setup", NULL);
    generateNames(mBindingConfigs, "Bindings", MIDI_COMMON_BINDINGS_NAME);
}

/**
 * Generate unique names for a list of bindables.
 * This isn't as simple as just genering "Foo N" names based
 * on list position since the previously generated names may still 
 * exist in the list but in a different position.
 *
 * In theory this is an ineffecient algorithm if the list is long
 * and the number of previously generated names is large.  That isn't
 * normal or advised, so screw 'em.
 */
PRIVATE void MobiusConfig::generateNames(Bindable* bindables, 
                                         const char* prefix,
                                         const char* baseName)
{
    char buf[128];
    int count = 1;

	for (Bindable* b = bindables ; b != NULL ; b = b->getNextBindable()) {
        if (baseName && b == bindables) {
            // force the name of the first one
            if (!StringEqual(baseName, b->getName()))
              b->setName(baseName);
        }
        else if (b->getName() == NULL) {
            Bindable* existing;
            do {
                // search for name in use
                existing = NULL;
                sprintf(buf, "%s %d", prefix, count);
                for (Bindable* b2 = bindables ; b2 != NULL ; 
                     b2 = b2->getNextBindable()) {
                    if (StringEqual(buf, b2->getName())) {
                        existing = b2;
                        break;
                    }
                }
                if (existing != NULL)
                  count++;
            } while (existing != NULL);

            b->setName(buf);
        }
    }
}

PUBLIC void MobiusConfig::setNoPresetChanges(bool b) {
	mNoPresetChanges = b;
}

PUBLIC bool MobiusConfig::isNoPresetChanges() {
	return mNoPresetChanges;
}

PUBLIC void MobiusConfig::setNoSetupChanges(bool b) {
	mNoSetupChanges = b;
}

PUBLIC bool MobiusConfig::isNoSetupChanges() {
	return mNoSetupChanges;
}

PUBLIC void MobiusConfig::setNoSyncBeatRounding(bool b) {
	mNoSyncBeatRounding = b;
}

PUBLIC bool MobiusConfig::isNoSyncBeatRounding() {
	return mNoSyncBeatRounding;
}

PUBLIC void MobiusConfig::setLogStatus(bool b) {
	mLogStatus = b;
}

PUBLIC bool MobiusConfig::isLogStatus() {
	return mLogStatus;
}

PUBLIC void MobiusConfig::setEdpisms(bool b) {
	mEdpisms = b;
}

PUBLIC bool MobiusConfig::isEdpisms() {
	return mEdpisms;
}

/****************************************************************************
 *                                                                          *
 *                                    OSC                                   *
 *                                                                          *
 ****************************************************************************/

PUBLIC void MobiusConfig::setOscInputPort(int port)
{
    mOscInputPort = port;
}

PUBLIC int MobiusConfig::getOscInputPort()
{
    return mOscInputPort;
}

PUBLIC void MobiusConfig::setOscOutputPort(int port)
{
    mOscOutputPort = port;
}

PUBLIC int MobiusConfig::getOscOutputPort()
{
    return mOscOutputPort;
}

PUBLIC void MobiusConfig::setOscOutputHost(const char* s)
{
    delete mOscOutputHost;
    mOscOutputHost = CopyString(s);
}

PUBLIC const char* MobiusConfig::getOscOutputHost()
{
    return mOscOutputHost;
}

PUBLIC void MobiusConfig::setOscTrace(bool b)
{
    mOscTrace = b;
}

PUBLIC bool MobiusConfig::isOscTrace()
{
    return mOscTrace;
}

PUBLIC void MobiusConfig::setOscEnable(bool b)
{
    mOscEnable = b;
}

PUBLIC bool MobiusConfig::isOscEnable()
{
    return mOscEnable;
}

/****************************************************************************
 *                                                                          *
 *                             PRESET MANAGEMENT                            *
 *                                                                          *
 ****************************************************************************/

PUBLIC Preset* MobiusConfig:: getPresets() 
{
	return mPresets;
}

PUBLIC int MobiusConfig::getPresetCount()
{
    return countThings(mPresets);
}

PUBLIC void MobiusConfig::setPresets(Preset* list)
{
    if (list != mPresets) {
		delete mPresets;
		mPresets = list;
		numberThings(mPresets);
    }
}
	
PUBLIC void MobiusConfig::addPreset(Preset* p) 
{
	int count = 0;

	// keep them ordered
	Preset *prev;
	for (prev = mPresets ; prev != NULL && prev->getNext() != NULL ; 
		 prev = prev->getNext());

	if (prev == NULL)
	  mPresets = p;
	else
	  prev->setNext(p);

    if (mPreset == NULL)
      mPreset = p;

	numberThings(mPresets);
}

/**
 * Note that this should only be called on a cloned MobiusConfig that
 * the interrupt handler can't be using.
 */
PUBLIC void MobiusConfig::removePreset(Preset* preset) 
{
	Preset* prev = NULL;
	for (Preset* p = mPresets ; p != NULL ; p = p->getNext()) {
		if (p != preset)
		  prev = p;
		else {
			if (prev == NULL)
			  mPresets = p->getNext();
			else 
			  prev->setNext(p->getNext());
			p->setNext(NULL);

			if (p == mPreset)
			  mPreset = mPresets;
		}
	}
	numberThings(mPresets);
}

PUBLIC Preset* MobiusConfig::getPreset(const char* name)
{
	Preset* found = NULL;
	if (name != NULL) {
		for (Preset* p = mPresets ; p != NULL ; p = p->getNext()) {
            if (StringEqualNoCase(name, p->getName())) {
				found = p;
				break;
			}
		}
	}
	return found;
}

PUBLIC Preset* MobiusConfig::getPreset(int index)
{
    Preset* found = NULL;
    int i = 0;

    for (Preset* p = mPresets ; p != NULL ; p = p->getNext(), i++) {
        if (i == index) {
            found = p;
            break;
        }
    }
    return found;
}

/**
 * Get the first preset, bootstrapping if we have to.
 */
PUBLIC Preset* MobiusConfig::getDefaultPreset()
{
    if (mPresets == NULL)
      mPresets = new Preset("Default");
    return mPresets;
}

/**
 * Get what is considered to be the current preset.
 * This is used only when conveying preset selection between
 * Mobius and the PresetDialog.
 */
PUBLIC Preset* MobiusConfig::getCurrentPreset()
{
	if (mPreset == NULL) {
		if (mPresets == NULL)
		  mPresets = new Preset("Default");
		mPreset = mPresets;
	}
	return mPreset;
}

PUBLIC int MobiusConfig::getCurrentPresetIndex()
{
    int index = 0;
    int i = 0;

	if (mPreset == NULL)
	  mPreset = mPresets;

    // don't need to do it this way if we can assume they're numbered!?
    for (Preset* p = mPresets ; p != NULL ; p = p->getNext(), i++) {
        if (p == mPreset) {
            index = i;
            break;
        }
    }
    return index;
}

PUBLIC void MobiusConfig::setCurrentPreset(Preset* p)
{
	mPreset = p;
}

PUBLIC Preset* MobiusConfig::setCurrentPreset(int index)
{
    Preset* p = getPreset(index);
    if (p != NULL) 
	  mPreset = p;
    return mPreset;
}

PUBLIC Preset* MobiusConfig::setCurrentPreset(const char* name)
{
	mPreset = getPreset(name);

	// would it be more useful to return the previous preset?
	return mPreset;
}

/****************************************************************************
 *                                                                          *
 *   						   SETUP MANAGEMENT                             *
 *                                                                          *
 ****************************************************************************/

PUBLIC Setup* MobiusConfig::getSetups() 
{
	return mSetups;
}

PUBLIC int MobiusConfig::getSetupCount()
{
    return countThings(mSetups);
}

PUBLIC void MobiusConfig::setSetups(Setup* list)
{
    if (list != mSetups) {
		delete mSetups;
		mSetups = list;
		numberThings(mSetups);
    }
}
	
PUBLIC void MobiusConfig::addSetup(Setup* p) 
{
	int count = 0;

	// keep them ordered
	Setup *prev;
	for (prev = mSetups ; prev != NULL && prev->getNext() != NULL ; 
		 prev = prev->getNext());

	if (prev == NULL)
	  mSetups = p;
	else
	  prev->setNext(p);

    if (mSetup == NULL)
      mSetup = p;

    numberThings(mSetups);
}

/**
 * Note that this should only be called on a cloned MobiusConfig that
 * the interrupt handler can't be using.
 */
PUBLIC void MobiusConfig::removeSetup(Setup* preset) 
{
	Setup* prev = NULL;
	for (Setup* p = mSetups ; p != NULL ; p = p->getNext()) {
		if (p != preset)
		  prev = p;
		else {
			if (prev == NULL)
			  mSetups = p->getNext();
			else 
			  prev->setNext(p->getNext());
			p->setNext(NULL);

			if (p == mSetup)
			  mSetup = mSetups;
		}
	}
    numberThings(mSetups);
}

PUBLIC Setup* MobiusConfig::getSetup(const char* name)
{
	Setup* found = NULL;
	if (name != NULL) {
		for (Setup* p = mSetups ; p != NULL ; p = p->getNext()) {
			if (StringEqualNoCase(name, p->getName())) {
				found = p;
				break;
			}
		}
	}
	return found;
}

PUBLIC Setup* MobiusConfig::getSetup(int index)
{
    Setup* found = NULL;
    int i = 0;

    for (Setup* p = mSetups ; p != NULL ; p = p->getNext(), i++) {
        if (i == index) {
            found = p;
            break;
        }
    }
    return found;
}

/**
 * If there is no currently selected setup, we pick the first one.
 */
PUBLIC Setup* MobiusConfig::getCurrentSetup()
{
	if (mSetup == NULL) {
		if (mSetups == NULL)
		  mSetups = new Setup();
		mSetup = mSetups;
	}
	return mSetup;
}

PUBLIC int MobiusConfig::getCurrentSetupIndex()
{
    int index = 0;
    int i = 0;

	if (mSetup == NULL)
	  mSetup = mSetups;

    for (Setup* p = mSetups ; p != NULL ; p = p->getNext(), i++) {
        if (p == mSetup) {
            index = i;
            break;
        }
    }
    return index;
}

/**
 * Normally we'll be given an object that is on our list
 * but we make sure.  We have historically chosen the object
 * with a matching name whether or not it was the same object.
 * Note that this means you have to generate names first if you've
 * just added something.
 */
PUBLIC void MobiusConfig::setCurrentSetup(Setup* p)
{
	if (p != NULL) {
		// these should be the same object, but make sure
 Setup* cur = getSetup(p->getName());
		if (cur != NULL) 
		  mSetup= cur;
	}
}

PUBLIC Setup* MobiusConfig::setCurrentSetup(int index)
{
    Setup* p = getSetup(index);
    if (p != NULL) 
	  mSetup = p;
    return mSetup;
}

PUBLIC Setup* MobiusConfig::setCurrentSetup(const char* name)
{
	mSetup = getSetup(name);

	// would it be more useful to return the previous preset?
	return mSetup;
}

/****************************************************************************
 *                                                                          *
 *   						 BINDINGS MANAGEMENT                            *
 *                                                                          *
 ****************************************************************************/
/*
 * The first object on the list is always considered to be the "base"
 * configuration and is always active.  One additional "overlay"
 * configuration may also be selected.
 */

PUBLIC BindingConfig* MobiusConfig::getBindingConfigs()
{
	return mBindingConfigs;
}

/**
 * Number of possible binding configs.
 * Currently used only by OscConfig to gether tha max value for
 * selectable binding configs.
 */
PUBLIC int MobiusConfig::getBindingConfigCount()
{
    return countThings(mBindingConfigs);
}

PUBLIC void MobiusConfig::addBindingConfig(BindingConfig* c) 
{
	// keep them ordered
	BindingConfig *prev;
	for (prev = mBindingConfigs ; prev != NULL && prev->getNext() != NULL ; 
		 prev = prev->getNext());
	if (prev == NULL)
	  mBindingConfigs = c;
	else
	  prev->setNext(c);

    numberThings(mBindingConfigs);
}

/**
 * This should ONLY be called for secondary BindingConfigs, the first
 * one on the list is not supposed to be removable.
 */
PUBLIC void MobiusConfig::removeBindingConfig(BindingConfig* config) 
{
	BindingConfig* prev = NULL;
	for (BindingConfig* p = mBindingConfigs ; p != NULL ; p = p->getNext()) {
		if (p != config)
		  prev = p;
		else {
			if (prev == NULL) {
                // UI should have prevented this
                Trace(1, "Removing base BindingConfig!!\n");
                mBindingConfigs = p->getNext();
            }
			else 
			  prev->setNext(p->getNext());

			p->setNext(NULL);

			if (p == mOverlayBindings)
			  mOverlayBindings = NULL;
		}
	}
    numberThings(mBindingConfigs);
}

PUBLIC BindingConfig* MobiusConfig::getBindingConfig(const char* name)
{
	BindingConfig* found = NULL;
    if (name == NULL) {
        // always the base config
        found = mBindingConfigs;
    }
    else {
		for (BindingConfig* p = mBindingConfigs ; p != NULL ; p = p->getNext()) {
			if (StringEqualNoCase(name, p->getName())) {
				found = p;
				break;
			}
		}
	}
	return found;
}

PUBLIC BindingConfig* MobiusConfig::getBindingConfig(int index)
{
    BindingConfig* found = NULL;
    int i = 0;

    for (BindingConfig* c = mBindingConfigs ; c != NULL ; c = c->getNext(), i++) {
        if (i == index) {
            found = c;
            break;
        }
    }
    return found;
}

/**
 * The "base" binding config is always the first.
 */
PUBLIC BindingConfig* MobiusConfig::getBaseBindingConfig()
{
    if (mBindingConfigs == NULL)
      mBindingConfigs = new BindingConfig();
	return mBindingConfigs;
}

PUBLIC BindingConfig* MobiusConfig::getOverlayBindingConfig()
{
    // it is important this self-heal if it got corrupted
    if (mOverlayBindings == mBindingConfigs)
      mOverlayBindings = NULL;
	return mOverlayBindings;
}

PUBLIC int MobiusConfig::getOverlayBindingConfigIndex()
{
    BindingConfig* overlay = getOverlayBindingConfig();

    int index = 0;
    int i = 0;
    for (BindingConfig* b = mBindingConfigs ; b != NULL ; 
         b = b->getNext(), i++) {

        if (b == overlay) {
            index = i;
            break;
        }
    }
    return index;
}

PUBLIC void MobiusConfig::setOverlayBindingConfig(BindingConfig* b)
{
    // ignore if it's the base
    // it is important we do this so it can self-heal if the
    // XML got screwed up or when processing dynamic Actions with a
    // bad overlay number
    if (b == mBindingConfigs)
      mOverlayBindings = NULL;
    else
      mOverlayBindings = b;
}

PUBLIC BindingConfig* MobiusConfig::setOverlayBindingConfig(const char* name)
{
    setOverlayBindingConfig(getBindingConfig(name));
	// would it be more useful to return the previous config?
	return mOverlayBindings;
}

PUBLIC BindingConfig* MobiusConfig::setOverlayBindingConfig(int index)
{
    BindingConfig* b = getBindingConfig(index);
    // ignore invalid indexes, don't reset to the base?
    if (b != NULL)
      setOverlayBindingConfig(b);

    return mOverlayBindings;
}

/****************************************************************************
 *                                                                          *
 *   								 XML                                    *
 *                                                                          *
 ****************************************************************************/

void MobiusConfig::parseXml(const char *src) 
{
    mError[0] = 0;
	XomParser* p = new XomParser();
	XmlDocument* d = p->parse(src);
    XmlElement* e = NULL;

	if (d != NULL)
      e = d->getChildElement();

    if (e != NULL)
      parseXml(e);
    else {
        // must have been a parse error
        CopyString(p->getError(), mError, sizeof(mError));
    }
    delete d;
	delete p;
}

/**
 * Return the error message if it is set.
 */
PUBLIC const char* MobiusConfig::getError()
{
    return (mError[0] != 0) ? mError : NULL;
}

PUBLIC void MobiusConfig::parseXml(XmlElement* e)
{
    const char* setup = e->getAttribute(ATT_SETUP);
	const char* bconfig = e->getAttribute(ATT_OVERLAY_BINDINGS);

    // save this for upgrade
    setSelectedMidiConfig(e->getAttribute(ATT_MIDI_CONFIG));

	// !! need to start iterating over GlobalParameters to 
	// automatic some of this

	setLanguage(e->getAttribute(ATT_LANGUAGE));
	setMidiInput(e->getAttribute(MidiInputParameter->getName()));
	setMidiOutput(e->getAttribute(MidiOutputParameter->getName()));
	setMidiThrough(e->getAttribute(MidiThroughParameter->getName()));
	setPluginMidiInput(e->getAttribute(PluginMidiInputParameter->getName()));
	setPluginMidiOutput(e->getAttribute(PluginMidiOutputParameter->getName()));
	setPluginMidiThrough(e->getAttribute(PluginMidiThroughParameter->getName()));
	setAudioInput(e->getAttribute(AudioInputParameter->getName()));
	setAudioOutput(e->getAttribute(AudioOutputParameter->getName()));
	setUIConfig(e->getAttribute(ATT_UI_CONFIG));
	setQuickSave(e->getAttribute(QuickSaveParameter->getName()));
	setUnitTests(e->getAttribute(UnitTestsParameter->getName()));
	setCustomMessageFile(e->getAttribute(CustomMessageFileParameter->getName()));

	setNoiseFloor(e->getIntAttribute(NoiseFloorParameter->getName()));
	setSuggestedLatencyMsec(e->getIntAttribute(ATT_SUGGESTED_LATENCY));
	setInputLatency(e->getIntAttribute(InputLatencyParameter->getName()));
	setOutputLatency(e->getIntAttribute(OutputLatencyParameter->getName()));
	setMaxSyncDrift(e->getIntAttribute(MaxSyncDriftParameter->getName()));
	setTracks(e->getIntAttribute(TracksParameter->getName()));
	setTrackGroups(e->getIntAttribute(TrackGroupsParameter->getName()));
	setMaxLoops(e->getIntAttribute(MaxLoopsParameter->getName()));
	setLongPress(e->getIntAttribute(LongPressParameter->getName()));

	setMonitorAudio(e->getBoolAttribute(MonitorAudioParameter->getName()));
	setHostRewinds(e->getBoolAttribute(ATT_PLUGIN_HOST_REWINDS));
	setPluginPins(e->getIntAttribute(ATT_PLUGIN_PINS));
	setAutoFeedbackReduction(e->getBoolAttribute(AutoFeedbackReductionParameter->getName()));
    // don't allow this to be persisted any more, can only be set in scripts
	//setIsolateOverdubs(e->getBoolAttribute(IsolateOverdubsParameter->getName()));
	setIntegerWaveFile(e->getBoolAttribute(IntegerWaveFileParameter->getName()));
	setSpreadRange(e->getIntAttribute(SpreadRangeParameter->getName()));
	setTracePrintLevel(e->getIntAttribute(TracePrintLevelParameter->getName()));
	setTraceDebugLevel(e->getIntAttribute(TraceDebugLevelParameter->getName()));
	setSaveLayers(e->getBoolAttribute(SaveLayersParameter->getName()));
	setDriftCheckPoint((DriftCheckPoint)XmlGetEnum(e, DriftCheckPointParameter->getName(), DriftCheckPointParameter->values));
	setMidiRecordMode((MidiRecordMode)XmlGetEnum(e, MidiRecordModeParameter->getName(), MidiRecordModeParameter->values));
    setDualPluginWindow(e->getBoolAttribute(DualPluginWindowParameter->getName()));
    setMidiExport(e->getBoolAttribute(MidiExportParameter->getName()));
    setHostMidiExport(e->getBoolAttribute(HostMidiExportParameter->getName()));

    setOscInputPort(e->getIntAttribute(OscInputPortParameter->getName()));
    setOscOutputPort(e->getIntAttribute(OscOutputPortParameter->getName()));
    setOscOutputHost(e->getAttribute(OscOutputHostParameter->getName()));
    setOscTrace(e->getBoolAttribute(OscTraceParameter->getName()));
    setOscEnable(e->getBoolAttribute(OscEnableParameter->getName()));

    // this isn't a parameter yet
    setNoSyncBeatRounding(e->getBoolAttribute(ATT_NO_SYNC_BEAT_ROUNDING));
    setLogStatus(e->getBoolAttribute(ATT_LOG_STATUS));

    // not an official parameter yet
    setEdpisms(e->getBoolAttribute(ATT_EDPISMS));

	setSampleRate((AudioSampleRate)XmlGetEnum(e, SampleRateParameter->getName(), SampleRateParameter->values));

    // fade frames can no longer be set high so we don't bother exposing it
	//setFadeFrames(e->getIntAttribute(FadeFramesParameter->getName()));

	for (XmlElement* child = e->getChildElement() ; child != NULL ; 
		 child = child->getNextElement()) {

		if (child->isName(EL_PRESET)) {
			Preset* p = new Preset(child);
			addPreset(p);
		}
		else if (child->isName(EL_SETUP)) {
			Setup* s = new Setup(child);
			addSetup(s);
		}
		else if (child->isName(EL_BINDING_CONFIG)) {
			BindingConfig* c = new BindingConfig(child);
			addBindingConfig(c);
		}
		else if (child->isName(EL_MIDI_CONFIG)) {
			MidiConfig* c = new MidiConfig(child);
			addMidiConfig(c);
		}
		else if (child->isName(EL_SCRIPT_CONFIG)) {
			mScriptConfig = new ScriptConfig(child);
		}
		else if (child->isName(EL_CONTROL_SURFACE)) {
			ControlSurfaceConfig* cs = new ControlSurfaceConfig(child);
			addControlSurface(cs);
		}
		else if (child->isName(EL_OSC_CONFIG)) {
			setOscConfig(new OscConfig(child));
		}
		else if (child->isName(EL_SAMPLES)) {
			mSamples = new Samples(child);
        }
		else if (child->isName(EL_FOCUS_LOCK_FUNCTIONS) ||
                 child->isName(EL_GROUP_FUNCTIONS)) {
            // changed the name in 1.43
			StringList* functions = new StringList();
			for (XmlElement* gchild = child->getChildElement() ; 
				 gchild != NULL ; 
				 gchild = gchild->getNextElement()) {
				// assumed to be <String>xxx</String>
				const char* name = gchild->getContent();
				if (name != NULL) 
				  functions->add(name);
			}
			setFocusLockFunctions(functions);
		}
		else if (child->isName(EL_MUTE_CANCEL_FUNCTIONS)) {
			StringList* functions = new StringList();
			for (XmlElement* gchild = child->getChildElement() ; 
				 gchild != NULL ; 
				 gchild = gchild->getNextElement()) {
				// assumed to be <String>xxx</String>
				const char* name = gchild->getContent();
				if (name != NULL) 
				  functions->add(name);
			}
			setMuteCancelFunctions(functions);
		}
		else if (child->isName(EL_CONFIRMATION_FUNCTIONS)) {
			StringList* functions = new StringList();
			for (XmlElement* gchild = child->getChildElement() ; 
				 gchild != NULL ; 
				 gchild = gchild->getNextElement()) {
				// assumed to be <String>xxx</String>
				const char* name = gchild->getContent();
				if (name != NULL) 
				  functions->add(name);
			}
			setConfirmationFunctions(functions);
		}
		else if (child->isName(EL_ALT_FEEDBACK_DISABLES)) {
			StringList* controls = new StringList();
			for (XmlElement* gchild = child->getChildElement() ; 
				 gchild != NULL ; 
				 gchild = gchild->getNextElement()) {
				// assumed to be <String>xxx</String>
				const char* name = gchild->getContent();
				if (name != NULL) 
				  controls->add(name);
			}
			setAltFeedbackDisables(controls);
		}
	}

	// have to wait until these are populated
	setOverlayBindingConfig(bconfig);
    setCurrentSetup(setup);
}

PUBLIC char* MobiusConfig::toXml()
{
	char* xml = NULL;
	XmlBuffer* b = new XmlBuffer();
	toXml(b);
	xml = b->stealString();
	delete b;
	return xml;
}

PUBLIC void MobiusConfig::toXml(XmlBuffer* b)
{
	// !! this really needs to be table driven like Preset parameters

	b->addOpenStartTag(EL_CONFIG);

    b->addAttribute(ATT_LANGUAGE, mLanguage);
    b->addAttribute(MidiInputParameter->getName(), mMidiInput);
    b->addAttribute(MidiOutputParameter->getName(), mMidiOutput);
    b->addAttribute(MidiThroughParameter->getName(), mMidiThrough);
    b->addAttribute(PluginMidiInputParameter->getName(), mPluginMidiInput);
    b->addAttribute(PluginMidiOutputParameter->getName(), mPluginMidiOutput);
    b->addAttribute(PluginMidiThroughParameter->getName(), mPluginMidiThrough);
    b->addAttribute(AudioInputParameter->getName(), mAudioInput);
    b->addAttribute(AudioOutputParameter->getName(), mAudioOutput);
	b->addAttribute(ATT_UI_CONFIG, mUIConfig);
	b->addAttribute(QuickSaveParameter->getName(), mQuickSave);
	b->addAttribute(CustomMessageFileParameter->getName(), mCustomMessageFile);
	b->addAttribute(UnitTestsParameter->getName(), mUnitTests);

    b->addAttribute(NoiseFloorParameter->getName(), mNoiseFloor);
	b->addAttribute(ATT_SUGGESTED_LATENCY, mSuggestedLatency);
	b->addAttribute(InputLatencyParameter->getName(), mInputLatency);
	b->addAttribute(OutputLatencyParameter->getName(), mOutputLatency);
    // don't bother saving this until it can have a more useful range
	//b->addAttribute(FadeFramesParameter->getName(), mFadeFrames);
	b->addAttribute(MaxSyncDriftParameter->getName(), mMaxSyncDrift);
    b->addAttribute(TracksParameter->getName(), mTracks);
    b->addAttribute(TrackGroupsParameter->getName(), mTrackGroups);
    b->addAttribute(MaxLoopsParameter->getName(), mMaxLoops);
	b->addAttribute(LongPressParameter->getName(), mLongPress);
	b->addAttribute(MonitorAudioParameter->getName(), mMonitorAudio);
	b->addAttribute(ATT_PLUGIN_HOST_REWINDS, mHostRewinds);
	b->addAttribute(ATT_PLUGIN_PINS, mPluginPins);
	b->addAttribute(AutoFeedbackReductionParameter->getName(), mAutoFeedbackReduction);
    // don't allow this to be persisted any more, can only be set in scripts
	//b->addAttribute(IsolateOverdubsParameter->getName(), mIsolateOverdubs);
	b->addAttribute(IntegerWaveFileParameter->getName(), mIntegerWaveFile);
	b->addAttribute(SpreadRangeParameter->getName(), mSpreadRange);
	b->addAttribute(TracePrintLevelParameter->getName(), mTracePrintLevel);
	b->addAttribute(TraceDebugLevelParameter->getName(), mTraceDebugLevel);
	b->addAttribute(SaveLayersParameter->getName(), mSaveLayers);
	b->addAttribute(DriftCheckPointParameter->getName(), DriftCheckPointParameter->values[mDriftCheckPoint]);
	b->addAttribute(MidiRecordModeParameter->getName(), MidiRecordModeParameter->values[mMidiRecordMode]);
	b->addAttribute(DualPluginWindowParameter->getName(), mDualPluginWindow);
	b->addAttribute(MidiExportParameter->getName(), mMidiExport);
	b->addAttribute(HostMidiExportParameter->getName(), mHostMidiExport);
	b->addAttribute(GroupFocusLockParameter->getName(), mGroupFocusLock);

    b->addAttribute(ATT_NO_SYNC_BEAT_ROUNDING, mNoSyncBeatRounding);
    b->addAttribute(ATT_LOG_STATUS, mLogStatus);

	b->addAttribute(OscInputPortParameter->getName(), mOscInputPort);
	b->addAttribute(OscOutputPortParameter->getName(), mOscOutputPort);
	b->addAttribute(OscOutputHostParameter->getName(), mOscOutputHost);
    b->addAttribute(OscTraceParameter->getName(), mOscTrace);
    b->addAttribute(OscEnableParameter->getName(), mOscEnable);

	b->addAttribute(SampleRateParameter->getName(), SampleRateParameter->values[mSampleRate]);

    // The setup is all we store, if the preset has been overridden
    // this is not saved in the config.
	if (mSetup != NULL)
	  b->addAttribute(ATT_SETUP, mSetup->getName());

    BindingConfig* overlay = getOverlayBindingConfig();
	if (overlay != NULL)
	  b->addAttribute(ATT_OVERLAY_BINDINGS, mOverlayBindings->getName());

    // not an official Parameter yet
    if (mEdpisms)
      b->addAttribute(ATT_EDPISMS, "true");

	b->add(">\n");
	b->incIndent();

	if (mScriptConfig != NULL)
      mScriptConfig->toXml(b);

	for (Preset* p = mPresets ; p != NULL ; p = p->getNext())
	  p->toXml(b);

	for (Setup* s = mSetups ; s != NULL ; s = s->getNext())
	  s->toXml(b);

	for (BindingConfig* c = mBindingConfigs ; c != NULL ; c = c->getNext())
	  c->toXml(b);

    // should have cleaned these up by now
    if (mMidiConfigs != NULL) {
        Trace(1, "Still have MidiConfigs!!\n");
        for (MidiConfig* mc = mMidiConfigs ; mc != NULL ; mc = mc->getNext())
          mc->toXml(b);
    }

	for (ControlSurfaceConfig* cs = mControlSurfaces ; cs != NULL ; cs = cs->getNext())
	  cs->toXml(b);

	if (mSamples != NULL)
	  mSamples->toXml(b);

	if (mFocusLockFunctions != NULL && mFocusLockFunctions->size() > 0) {
		b->addStartTag(EL_FOCUS_LOCK_FUNCTIONS, true);
		b->incIndent();
		for (int i = 0 ; i < mFocusLockFunctions->size() ; i++) {
			const char* name = mFocusLockFunctions->getString(i);
			b->addElement(EL_STRING, name);
		}
		b->decIndent();
		b->addEndTag(EL_FOCUS_LOCK_FUNCTIONS, true);
	}		

	if (mMuteCancelFunctions != NULL && mMuteCancelFunctions->size() > 0) {
		b->addStartTag(EL_MUTE_CANCEL_FUNCTIONS, true);
		b->incIndent();
		for (int i = 0 ; i < mMuteCancelFunctions->size() ; i++) {
			const char* name = mMuteCancelFunctions->getString(i);
			b->addElement(EL_STRING, name);
		}
		b->decIndent();
		b->addEndTag(EL_MUTE_CANCEL_FUNCTIONS, true);
	}		

	if (mConfirmationFunctions != NULL && mConfirmationFunctions->size() > 0) {
		b->addStartTag(EL_CONFIRMATION_FUNCTIONS, true);
		b->incIndent();
		for (int i = 0 ; i < mConfirmationFunctions->size() ; i++) {
			const char* name = mConfirmationFunctions->getString(i);
			b->addElement(EL_STRING, name);
		}
		b->decIndent();
		b->addEndTag(EL_CONFIRMATION_FUNCTIONS, true);
	}		

	if (mAltFeedbackDisables != NULL && mAltFeedbackDisables->size() > 0) {
		b->addStartTag(EL_ALT_FEEDBACK_DISABLES, true);
		b->incIndent();
		for (int i = 0 ; i < mAltFeedbackDisables->size() ; i++) {
			const char* name = mAltFeedbackDisables->getString(i);
			b->addElement(EL_STRING, name);
		}
		b->decIndent();
		b->addEndTag(EL_ALT_FEEDBACK_DISABLES, true);
	}		

	b->decIndent();

	b->addEndTag(EL_CONFIG);
}

/****************************************************************************
 *                                                                          *
 *                               SCRIPT CONFIG                              *
 *                                                                          *
 ****************************************************************************/

PUBLIC ScriptConfig::ScriptConfig()
{
    mScripts = NULL;
}

PUBLIC ScriptConfig::ScriptConfig(XmlElement* e)
{
    mScripts = NULL;
    parseXml(e);
}

PUBLIC ScriptConfig::~ScriptConfig()
{
	clear();
}

/**
 * Clone for difference detection.
 * All we really need are the original file names.
 */
PUBLIC ScriptConfig* ScriptConfig::clone()
{
    ScriptConfig* clone = new ScriptConfig();
    for (ScriptRef* s = mScripts ; s != NULL ; s = s->getNext()) {
        ScriptRef* s2 = new ScriptRef(s);
        clone->add(s2);
    }
    return clone;
}

PUBLIC void ScriptConfig::clear()
{
    ScriptRef* ref = NULL;
    ScriptRef* next = NULL;
    for (ref = mScripts ; ref != NULL ; ref = next) {
        next = ref->getNext();
        delete ref;
    }
	mScripts = NULL;
}

PUBLIC ScriptRef* ScriptConfig::getScripts()
{
    return mScripts;
}

PUBLIC void ScriptConfig::setScripts(ScriptRef* refs) 
{
	clear();
	mScripts = refs;
}

PUBLIC void ScriptConfig::add(ScriptRef* neu) 
{
    ScriptRef* last = NULL;
    for (last = mScripts ; last != NULL && last->getNext() != NULL ; 
         last = last->getNext());

	if (last == NULL)
	  mScripts = neu;
	else
	  last->setNext(neu);
}

PUBLIC void ScriptConfig::add(const char* file) 
{
	add(new ScriptRef(file));
}

PUBLIC void ScriptConfig::toXml(XmlBuffer* b)
{
    b->addStartTag(EL_SCRIPT_CONFIG);
    b->incIndent();

    for (ScriptRef* ref = mScripts ; ref != NULL ; ref = ref->getNext())
      ref->toXml(b);

    b->decIndent();
    b->addEndTag(EL_SCRIPT_CONFIG);
}

PUBLIC void ScriptConfig::parseXml(XmlElement* e)
{
    ScriptRef* last = NULL;
    for (ScriptRef* ref = mScripts ; ref != NULL && ref->getNext() != NULL ; 
         ref = ref->getNext());

    for (XmlElement* child = e->getChildElement() ; child != NULL ; 
         child = child->getNextElement()) {
        ScriptRef* ref = new ScriptRef(child);
        if (last == NULL)
          mScripts = ref;   
        else
          last->setNext(ref);
        last = ref;
    }
}

/**
 * Utility for difference detection.
 */
PUBLIC bool ScriptConfig::isDifference(ScriptConfig* other)
{
    bool difference = false;

    int myCount = 0;
    for (ScriptRef* s = mScripts ; s != NULL ; s = s->getNext())
      myCount++;

    int otherCount = 0;
    if (other != NULL) {
        for (ScriptRef* s = other->getScripts() ; s != NULL ; s = s->getNext())
          otherCount++;
    }

    if (myCount != otherCount) {
        difference = true;
    }
    else {
        for (ScriptRef* s = mScripts ; s != NULL ; s = s->getNext()) {
            ScriptRef* ref = other->get(s->getFile());
            if (ref == NULL) {
                difference = true;
                break;
            }
        }
    }
    return difference;
}

PUBLIC ScriptRef* ScriptConfig::get(const char* file)
{
    ScriptRef* found = NULL;

    for (ScriptRef* s = mScripts ; s != NULL ; s = s->getNext()) {
        if (StringEqual(s->getFile(), file)) {
            found = s;
            break;
        }
    }
    return found;
}

//////////////////////////////////////////////////////////////////////
//
// ScriptRef
//
//////////////////////////////////////////////////////////////////////

PUBLIC ScriptRef::ScriptRef()
{
    init();
}

PUBLIC ScriptRef::ScriptRef(XmlElement* e)
{
    init();
    parseXml(e);
}

PUBLIC ScriptRef::ScriptRef(const char* file)
{
    init();
    setFile(file);
}

PUBLIC ScriptRef::ScriptRef(ScriptRef* src)
{
    init();
    setFile(src->getFile());
}

PUBLIC void ScriptRef::init()
{
    mNext = NULL;
    mFile = NULL;
}

PUBLIC ScriptRef::~ScriptRef()
{
	delete mFile;
}

PUBLIC void ScriptRef::setNext(ScriptRef* ref)
{
    mNext = ref;
}

PUBLIC ScriptRef* ScriptRef::getNext()
{
    return mNext;
}

PUBLIC void ScriptRef::setFile(const char* file)
{
    delete mFile;
    mFile = CopyString(file);
}

PUBLIC const char* ScriptRef::getFile()
{
    return mFile;
}

PUBLIC void ScriptRef::toXml(XmlBuffer* b)
{
    b->addOpenStartTag(EL_SCRIPT_REF);
    b->addAttribute(ATT_FILE, mFile);
    b->add("/>\n");
}

PUBLIC void ScriptRef::parseXml(XmlElement* e)
{
    setFile(e->getAttribute(ATT_FILE));
}

//////////////////////////////////////////////////////////////////////
//
// Control Surface
//
//////////////////////////////////////////////////////////////////////

PUBLIC ControlSurfaceConfig::ControlSurfaceConfig()
{
	init();
}

PUBLIC ControlSurfaceConfig::ControlSurfaceConfig(XmlElement* e)
{
	init();
	parseXml(e);
}

PUBLIC void ControlSurfaceConfig::init()
{
	mNext = NULL;
	mName = NULL;
}

PUBLIC ControlSurfaceConfig::~ControlSurfaceConfig()
{
	delete mName;

    ControlSurfaceConfig* el;
    ControlSurfaceConfig* next = NULL;

    for (el = mNext ; el != NULL ; el = next) {
        next = el->getNext();
		el->setNext(NULL);
        delete el;
    }
}

PUBLIC ControlSurfaceConfig* ControlSurfaceConfig::getNext()
{
	return mNext;
}

PUBLIC void ControlSurfaceConfig::setNext(ControlSurfaceConfig* cs)
{
	mNext = cs;
}

PUBLIC const char* ControlSurfaceConfig::getName()
{
	return mName;
}

PUBLIC void ControlSurfaceConfig::setName(const char* s)
{
	delete mName;
	mName = CopyString(s);
}

PRIVATE void ControlSurfaceConfig::parseXml(XmlElement* e)
{
    setName(e->getAttribute(ATT_NAME));
}

PUBLIC void ControlSurfaceConfig::toXml(XmlBuffer* b)
{
    b->addOpenStartTag(EL_CONTROL_SURFACE);
    b->addAttribute(ATT_NAME, mName);
    b->add("/>\n");
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
