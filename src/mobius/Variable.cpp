/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Mobius Variables.
 *
 * These are sort of like Parameters except they are typically read-only
 * and accessible only in scripts.
 *
 * A few things are represented as both variables and parameters
 * (LoopFrames, LoopCycles).
 *
 * I'm leaning toward moving most of the read-only "track parameters"
 * from being ParameterDefs to script variables.  They're easier to 
 * maintain and they're really only for use in scripts anyway.
 */

#include <stdio.h>
#include <memory.h>

#include "Util.h"
#include "Vbuf.h"

#include "Event.h"
#include "EventManager.h"
#include "Expr.h"
#include "Function.h"
#include "Layer.h"
#include "Loop.h"
#include "Mobius.h"
#include "Mode.h"
#include "Project.h"
#include "Recorder.h"
#include "Script.h"
#include "Synchronizer.h"
#include "SyncState.h"
#include "SyncTracker.h"
#include "Track.h"

#include "Variable.h"

/****************************************************************************
 *                                                                          *
 *   						  INTERNAL VARIABLE                             *
 *                                                                          *
 ****************************************************************************/

PUBLIC ScriptInternalVariable::ScriptInternalVariable()
{
    mName = NULL;
	mAlias = NULL;
	mType = TYPE_INT;
}

PUBLIC ScriptInternalVariable::~ScriptInternalVariable()
{
    delete mName;
	delete mAlias;
}

PUBLIC void ScriptInternalVariable::setName(const char* name)
{
    delete mName;
    mName = CopyString(name);
}

PUBLIC const char* ScriptInternalVariable::getName()
{
    return mName;
}

PUBLIC void ScriptInternalVariable::setAlias(const char* name)
{
    delete mAlias;
    mAlias = CopyString(name);
}

/**
 * Compare the external name against the name and the alias.
 * Kludge to handle renames of a few variables.  Should support
 * multiple aliases.
 */
PUBLIC bool ScriptInternalVariable::isMatch(const char* name)
{
	return (StringEqualNoCase(name, mName) ||
			StringEqualNoCase(name, mAlias));
}

/**
 * The base implementation of getValue.  
 * We almost always forward this so the active track, but in a few cases
 * it will be overloaded to extract information from the interpreter.
 */
PUBLIC void ScriptInternalVariable::getValue(ScriptInterpreter* si, 	
											 ExValue* value)
{
	getTrackValue(si->getTargetTrack(), value);
}

PUBLIC void ScriptInternalVariable::getTrackValue(Track* t, ExValue* value)
{
	value->setInt(0);
}

/**
 * Verify few variables can be set, the ones that can are usually
 * just for unit tests and debugging.
 */
PUBLIC void ScriptInternalVariable::setValue(ScriptInterpreter* si, 
											 ExValue* value)
{
	Trace(1, "Attempt to set script internal variable %s\n", mName);
}

/****************************************************************************
 *                                                                          *
 *   						SCRIPT EXECUTION STATE                          *
 *                                                                          *
 ****************************************************************************/

//////////////////////////////////////////////////////////////////////
//
// sustainCount
//
// Number of times the script has been notified of a sustain.
//
//////////////////////////////////////////////////////////////////////

class SustainCountVariableType : public ScriptInternalVariable {
  public:
    SustainCountVariableType();
    void getValue(ScriptInterpreter* si, ExValue* value);
};

SustainCountVariableType::SustainCountVariableType()
{
    setName("sustainCount");
}

void SustainCountVariableType::getValue(ScriptInterpreter* si, ExValue* value)
{
	value->setInt(si->getSustainCount());
}

PUBLIC SustainCountVariableType* SustainCountVariable = 
new SustainCountVariableType();

//////////////////////////////////////////////////////////////////////
//
// clickCount
//
// Number of times the script has been reentred due to multi-clicks.
//
//////////////////////////////////////////////////////////////////////

class ClickCountVariableType : public ScriptInternalVariable {
  public:
    ClickCountVariableType();
    void getValue(ScriptInterpreter* si, ExValue* value);
};

ClickCountVariableType::ClickCountVariableType()
{
    setName("clickCount");
}

void ClickCountVariableType::getValue(ScriptInterpreter* si, ExValue* value)
{
	value->setInt(si->getClickCount());
}

PUBLIC ClickCountVariableType* ClickCountVariable = 
new ClickCountVariableType();

//////////////////////////////////////////////////////////////////////
//
// triggerSource
//
// The source of the trigger.  Originally this was the name
// of a FunctionSource enumeration item, now it is the name of
// a Trigger constant.
//
//////////////////////////////////////////////////////////////////////

class TriggerSourceValueVariableType : public ScriptInternalVariable {
  public:
    TriggerSourceValueVariableType();
    void getValue(ScriptInterpreter* si, ExValue* value);
};

TriggerSourceValueVariableType::TriggerSourceValueVariableType()
{
    setName("triggerSource");
}

void TriggerSourceValueVariableType::getValue(ScriptInterpreter* si, ExValue* value)
{
	Trigger* t = si->getTrigger();
    if (t != NULL)
      value->setString(t->getName());
    else
      value->setNull();
}

PUBLIC TriggerSourceValueVariableType* TriggerSourceValueVariable = 
new TriggerSourceValueVariableType();

//////////////////////////////////////////////////////////////////////
//
// triggerNumber
//
// The unique id of the trigger.  For TriggerMidi this will
// be a combination of the MIDI status, channel, and number.  For other
// sources it will be a key code or other simple number.
//
//////////////////////////////////////////////////////////////////////

class TriggerNumberVariableType : public ScriptInternalVariable {
  public:
    TriggerNumberVariableType();
    void getValue(ScriptInterpreter* si, ExValue* value);
};

TriggerNumberVariableType::TriggerNumberVariableType()
{
    setName("triggerNumber");
}

void TriggerNumberVariableType::getValue(ScriptInterpreter* si, ExValue* value)
{
	value->setInt(si->getTriggerId());
}

PUBLIC TriggerNumberVariableType* TriggerNumberVariable = 
new TriggerNumberVariableType();

//////////////////////////////////////////////////////////////////////
//
// triggerValue, triggerVelocity
//
// An optional extra value associated with the trigger.
// For MIDI triggers this will be the second byte, the note velocity
// for notes or the controller value for controllers.
//
//////////////////////////////////////////////////////////////////////

class TriggerValueVariableType : public ScriptInternalVariable {
  public:
    TriggerValueVariableType();
    void getValue(ScriptInterpreter* si, ExValue* value);
};

TriggerValueVariableType::TriggerValueVariableType()
{
    setName("triggerValue");
    setAlias("triggerVelocity");
}

void TriggerValueVariableType::getValue(ScriptInterpreter* si, ExValue* value)
{
	value->setInt(si->getTriggerValue());
}

PUBLIC TriggerValueVariableType* TriggerValueVariable = 
new TriggerValueVariableType();

//////////////////////////////////////////////////////////////////////
//
// triggerOffset
//
// An optional extra value associated with the spread functions.
// This will have the relative position of the trigger from the
// center of the range.
//
//////////////////////////////////////////////////////////////////////

class TriggerOffsetVariableType : public ScriptInternalVariable {
  public:
    TriggerOffsetVariableType();
    void getValue(ScriptInterpreter* si, ExValue* value);
};

TriggerOffsetVariableType::TriggerOffsetVariableType()
{
    setName("triggerOffset");
}

void TriggerOffsetVariableType::getValue(ScriptInterpreter* si, ExValue* value)
{
	value->setInt(si->getTriggerOffset());
}

PUBLIC TriggerOffsetVariableType* TriggerOffsetVariable = 
new TriggerOffsetVariableType();

//////////////////////////////////////////////////////////////////////
//
// midiType
//
// The type of MIDI trigger: note, control, program.
//
//////////////////////////////////////////////////////////////////////

class MidiTypeVariableType : public ScriptInternalVariable {
  public:
    MidiTypeVariableType();
    void getValue(ScriptInterpreter* si, ExValue* value);
};

MidiTypeVariableType::MidiTypeVariableType()
{
    setName("midiType");
}

void MidiTypeVariableType::getValue(ScriptInterpreter* si, ExValue* value)
{
	const char* type = "unknown";

    int id = si->getTriggerId();
	int status = (id >> 12);

	switch (status) {
		case 0x9: type = "note"; break;
		case 0xB: type = "control"; break;
		case 0xC: type = "program"; break;
		case 0xD: type = "touch"; break;
		case 0xE: type = "bend"; break;
	}

	value->setString(type);
}

PUBLIC MidiTypeVariableType* MidiTypeVariable = 
new MidiTypeVariableType();

//////////////////////////////////////////////////////////////////////
//
// midiChannel
//
// The MIDI channel number of the trigger event.
// This is also embedded in triggerNumber, but 
//
//////////////////////////////////////////////////////////////////////

class MidiChannelVariableType : public ScriptInternalVariable {
  public:
    MidiChannelVariableType();
    void getValue(ScriptInterpreter* si, ExValue* value);
};

MidiChannelVariableType::MidiChannelVariableType()
{
    setName("midiChannel");
}

void MidiChannelVariableType::getValue(ScriptInterpreter* si, ExValue* value)
{
	int id = si->getTriggerId();
	int channel = (id >> 8) & 0xF;
	value->setInt(channel);
}

PUBLIC MidiChannelVariableType* MidiChannelVariable = 
new MidiChannelVariableType();

//////////////////////////////////////////////////////////////////////
//
// midiNumber
//
// The MIDI key/controller number of the trigger event.
//
//////////////////////////////////////////////////////////////////////

class MidiNumberVariableType : public ScriptInternalVariable {
  public:
    MidiNumberVariableType();
    void getValue(ScriptInterpreter* si, ExValue* value);
};

MidiNumberVariableType::MidiNumberVariableType()
{
    setName("midiNumber");
}

void MidiNumberVariableType::getValue(ScriptInterpreter* si, ExValue* value)
{
	int id = si->getTriggerId();
	int number = (id & 0xFF);
	value->setInt(number);
}

PUBLIC MidiNumberVariableType* MidiNumberVariable = 
new MidiNumberVariableType();

//////////////////////////////////////////////////////////////////////
//
// midiValue
//
// The same as triggerValue but has a more obvious name for
// use in !controller scripts.
//
//////////////////////////////////////////////////////////////////////

class MidiValueVariableType : public ScriptInternalVariable {
  public:
    MidiValueVariableType();
    void getValue(ScriptInterpreter* si, ExValue* value);
};

MidiValueVariableType::MidiValueVariableType()
{
    setName("midiValue");
}

void MidiValueVariableType::getValue(ScriptInterpreter* si, ExValue* value)
{
	value->setInt(si->getTriggerValue());
}

PUBLIC MidiValueVariableType* MidiValueVariable = 
new MidiValueVariableType();

//////////////////////////////////////////////////////////////////////
//
// returnCode
//
// The return code of the last ThreadEvent.
// Currently used only by Prompt statements to convey the 
// selected button.  0 means Ok, 1 means cancel.
//
//////////////////////////////////////////////////////////////////////

class ReturnCodeVariableType : public ScriptInternalVariable {
  public:
    ReturnCodeVariableType();
    void getValue(ScriptInterpreter* si, ExValue* value);
	void setValue(ScriptInterpreter* si, ExValue* value);
};

ReturnCodeVariableType::ReturnCodeVariableType()
{
    setName("returnCode");
}

void ReturnCodeVariableType::getValue(ScriptInterpreter* si, ExValue* value)
{
	value->setInt(si->getReturnCode());
}

PUBLIC void ReturnCodeVariableType::setValue(ScriptInterpreter* si, 
											 ExValue* value)
{
	si->setReturnCode(value->getInt());
}

PUBLIC ReturnCodeVariableType* ReturnCodeVariable = 
new ReturnCodeVariableType();

/****************************************************************************
 *                                                                          *
 *   							INTERNAL STATE                              *
 *                                                                          *
 ****************************************************************************/

//////////////////////////////////////////////////////////////////////
//
// blockFrames
//
// The number of frames in one audio interrupt block.
//
//////////////////////////////////////////////////////////////////////

class BlockFramesVariableType : public ScriptInternalVariable {
  public:
    BlockFramesVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

BlockFramesVariableType::BlockFramesVariableType()
{
    setName("blockFrames");
}

void BlockFramesVariableType::getTrackValue(Track* t, ExValue* value)
{
    // !! need to be checking the AudioInterface
	value->setLong(AUDIO_FRAMES_PER_BUFFER);
}

PUBLIC BlockFramesVariableType* BlockFramesVariable = 
new BlockFramesVariableType();

//////////////////////////////////////////////////////////////////////
//
// sampleFrames
//
// The number of frames in the last sample we played.
//
//////////////////////////////////////////////////////////////////////

class SampleFramesVariableType : public ScriptInternalVariable {
  public:
    SampleFramesVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

SampleFramesVariableType::SampleFramesVariableType()
{
    setName("sampleFrames");
}

void SampleFramesVariableType::getTrackValue(Track* t, ExValue* value)
{
	value->setLong(t->getMobius()->getLastSampleFrames());
}

PUBLIC SampleFramesVariableType* SampleFramesVariable = 
new SampleFramesVariableType();

/****************************************************************************
 *                                                                          *
 *   						  CONTROL VARIABLES                             *
 *                                                                          *
 ****************************************************************************/

//////////////////////////////////////////////////////////////////////
//
// noExternalAudio
//
// When set disables the pass through of audio received
// on the first port.  This is used in the unit tests that do their
// own audio injection, and we don't want random noise comming
// in from the sound card to pollute it.
//
//
//////////////////////////////////////////////////////////////////////

class NoExternalAudioVariableType : public ScriptInternalVariable {
  public:
    NoExternalAudioVariableType();
    void getValue(ScriptInterpreter* si, ExValue* value);
	void setValue(ScriptInterpreter* si, ExValue* value);
};

NoExternalAudioVariableType::NoExternalAudioVariableType()
{
    setName("noExternalAudio");
}

void NoExternalAudioVariableType::getValue(ScriptInterpreter* si, ExValue* value)
{
	Mobius* m = si->getMobius();
	value->setBool(m->isNoExternalInput());
}

void NoExternalAudioVariableType::setValue(ScriptInterpreter* si, 
										   ExValue* value)
{
	Mobius* m = si->getMobius();
	m->setNoExternalInput(value->getBool());
}

PUBLIC NoExternalAudioVariableType* NoExternalAudioVariable = 
new NoExternalAudioVariableType();

/****************************************************************************
 *                                                                          *
 *   							  LOOP STATE                                *
 *                                                                          *
 ****************************************************************************/

//////////////////////////////////////////////////////////////////////
//
// loopCount
//
// The current loop count.
// This is effectively the same as the "moreLoops" parameter but
// I like this name better.  This should really be an alias of moreLoops
// so we can get and set it using the same name!!
//
//////////////////////////////////////////////////////////////////////

class LoopCountVariableType : public ScriptInternalVariable {

  public:
    LoopCountVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

LoopCountVariableType::LoopCountVariableType()
{
    setName("loopCount");
}

void LoopCountVariableType::getTrackValue(Track* t, ExValue* value)
{
    value->setInt(t->getLoopCount());
}

PUBLIC LoopCountVariableType* LoopCountVariable = 
new LoopCountVariableType();

//////////////////////////////////////////////////////////////////////
//
// loopNumber
//
// The number of the curerent loop within the track.  The first
// loop number is one for consistency with the trigger functions
// Loop1, Loop2, etc.
//
// This is similar to "mode" in that it conveys read-only information
// about the current loop.  Mode is however implemented as a Parameter.
// Now that we have internal script variables, mode would make more sense
// over here, but being a Parameter gives it some localization support
// so leave it there for now.
//
//////////////////////////////////////////////////////////////////////

class LoopNumberVariableType : public ScriptInternalVariable {
  public:
    LoopNumberVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

LoopNumberVariableType::LoopNumberVariableType()
{
    setName("loopNumber");
}

void LoopNumberVariableType::getTrackValue(Track* t, ExValue* value)
{
	// note that internally loops are numbered from 1
    value->setInt(t->getLoop()->getNumber());
}

PUBLIC LoopNumberVariableType* LoopNumberVariable = 
new LoopNumberVariableType();

//////////////////////////////////////////////////////////////////////
//
// loopFrames
//
// The number of frames in the loop.
//
//////////////////////////////////////////////////////////////////////

class LoopFramesVariableType : public ScriptInternalVariable {
  public:
    LoopFramesVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

LoopFramesVariableType::LoopFramesVariableType()
{
    setName("loopFrames");
}

void LoopFramesVariableType::getTrackValue(Track* t, ExValue* value)
{
	value->setLong(t->getLoop()->getFrames());
}

PUBLIC LoopFramesVariableType* LoopFramesVariable = 
new LoopFramesVariableType();

//////////////////////////////////////////////////////////////////////
// 
// loopFrame
// 
// The current record frame.
//
//////////////////////////////////////////////////////////////////////

class LoopFrameVariableType : public ScriptInternalVariable {
  public:
    LoopFrameVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

LoopFrameVariableType::LoopFrameVariableType()
{
    setName("loopFrame");
}

void LoopFrameVariableType::getTrackValue(Track* t, ExValue* value)
{
	value->setLong(t->getLoop()->getFrame());
}

PUBLIC LoopFrameVariableType* LoopFrameVariable = 
new LoopFrameVariableType();

//////////////////////////////////////////////////////////////////////
//
// cycleCount
//
// The number of cycles in the loop.
//
//////////////////////////////////////////////////////////////////////

class CycleCountVariableType : public ScriptInternalVariable {
  public:
    CycleCountVariableType();
    void getTrackValue(Track* t, ExValue* value);
    void setValue(ScriptInterpreter* si, ExValue* value);
};

CycleCountVariableType::CycleCountVariableType()
{
    setName("cycleCount");
}

void CycleCountVariableType::getTrackValue(Track* t, ExValue* value)
{
	value->setLong(t->getLoop()->getCycles());
}

/**
 * This is one of the few variables that has a setter.
 * 
 * Changing the cycle size can have all sorts of subtle consequences
 * for synchronization so you should only do this if sync is off or
 * we've already locked the trackers.  That's hard to prevent here, 
 * could potentially schedule a pending Event to change the cycle count
 * when the sync dust settles.  
 *
 * Trying to dynamically adjust Synchronizer and SyncTracker for cycle
 * length changes is even harder.  We don't call 
 * Synchronizer::resizeOutSyncTracker since the size hasn't actually 
 * changed and the new cycleFrames could cause those calculations to
 * pick a different tempo.
 *
 * This will not change quantization of previously scheduled events.
 * 
 * This will change the record layer cycle count but not the play layer.
 * It currently does not shift a layer so this is not an undoable action.
 * If you undo the cycle count will revert to what it is in the play layer.
 *
 */
PUBLIC void CycleCountVariableType::setValue(ScriptInterpreter* si, 
                                             ExValue* value)
{
    Track* t = si->getTargetTrack();
    Loop* l = t->getLoop();
    l->setCycles(value->getInt());
}

PUBLIC CycleCountVariableType* CycleCountVariable = 
new CycleCountVariableType();
 
//////////////////////////////////////////////////////////////////////
//
// cycleNumber
//
// The current cycle number, relative to the beginning of the loop.
//
//////////////////////////////////////////////////////////////////////

class CycleNumberVariableType : public ScriptInternalVariable {
  public:
    CycleNumberVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

CycleNumberVariableType::CycleNumberVariableType()
{
    setName("cycleNumber");
}

void CycleNumberVariableType::getTrackValue(Track* t, ExValue* value)
{
    Loop* l = t->getLoop();
    long frame = l->getFrame();
    long cycleFrames = l->getCycleFrames();
    value->setLong(frame / cycleFrames);
}

PUBLIC CycleNumberVariableType* CycleNumberVariable = 
new CycleNumberVariableType();

//////////////////////////////////////////////////////////////////////
//
// cycleFrames
//
// The number of frames in one cycle.
// 
//////////////////////////////////////////////////////////////////////

class CycleFramesVariableType : public ScriptInternalVariable {
  public:
    CycleFramesVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

CycleFramesVariableType::CycleFramesVariableType()
{
    setName("cycleFrames");
}

void CycleFramesVariableType::getTrackValue(Track* t, ExValue* value)
{
    value->setLong(t->getLoop()->getCycleFrames());
}

PUBLIC CycleFramesVariableType* CycleFramesVariable = 
new CycleFramesVariableType();

//////////////////////////////////////////////////////////////////////
// 
// cycleFrame
//
// The current frame relative the current cycle.
//
//////////////////////////////////////////////////////////////////////

class CycleFrameVariableType : public ScriptInternalVariable {
  public:
    CycleFrameVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

CycleFrameVariableType::CycleFrameVariableType()
{
    setName("cycleFrame");
}

void CycleFrameVariableType::getTrackValue(Track* t, ExValue* value)
{
    Loop* l = t->getLoop();
    long frame = l->getFrame();
    long cycleFrames = l->getCycleFrames();
	value->setLong(frame % cycleFrames);
}

PUBLIC CycleFrameVariableType* CycleFrameVariable = 
new CycleFrameVariableType();

//////////////////////////////////////////////////////////////////////
//
// subCycleCount
// 
// The number of subCycles in a cycle.
// This is actually the same as the "subcycles" preset parameter and
// can change with the preset, but we expose it as an internal variable
// so it is consistent with the other loop divisions.
//
//////////////////////////////////////////////////////////////////////

class SubCycleCountVariableType : public ScriptInternalVariable {
  public:
    SubCycleCountVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

SubCycleCountVariableType::SubCycleCountVariableType()
{
    setName("subCycleCount");
}

void SubCycleCountVariableType::getTrackValue(Track* t, ExValue* value)
{
	Preset* p = t->getPreset();
	value->setLong(p->getSubcycles());
}

PUBLIC SubCycleCountVariableType* SubCycleCountVariable = 
new SubCycleCountVariableType();

//////////////////////////////////////////////////////////////////////
//
// subCycleNumber
// 
// The current subcycle number, relative to the current cycle.
// !! Should this be relative to the start of the loop?
//
//////////////////////////////////////////////////////////////////////

class SubCycleNumberVariableType : public ScriptInternalVariable {
  public:
    SubCycleNumberVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

SubCycleNumberVariableType::SubCycleNumberVariableType()
{
    setName("subCycleNumber");
}

void SubCycleNumberVariableType::getTrackValue(Track* t, ExValue* value)
{
    Loop* l = t->getLoop();
    Preset* p = l->getPreset();
    long frame = l->getFrame();
    long subCycleFrames = l->getSubCycleFrames();

    // absolute subCycle with in loop
    long subCycle = frame / subCycleFrames;

    // adjust to be relative to start of cycle
    subCycle %= p->getSubcycles();

	value->setLong(subCycle);
}

PUBLIC SubCycleNumberVariableType* SubCycleNumberVariable = 
new SubCycleNumberVariableType();

//////////////////////////////////////////////////////////////////////
//
// subCycleFrames
// 
// The number of frames in one subcycle.
//
//////////////////////////////////////////////////////////////////////

class SubCycleFramesVariableType : public ScriptInternalVariable {
  public:
    SubCycleFramesVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

SubCycleFramesVariableType::SubCycleFramesVariableType()
{
    setName("subCycleFrames");
}

void SubCycleFramesVariableType::getTrackValue(Track* t, ExValue* value)
{
	value->setLong(t->getLoop()->getSubCycleFrames());
}

PUBLIC SubCycleFramesVariableType* SubCycleFramesVariable = 
new SubCycleFramesVariableType();

//////////////////////////////////////////////////////////////////////
//
// subCycleFrame
//
// The current frame relative the current subcycle.
//
//////////////////////////////////////////////////////////////////////

class SubCycleFrameVariableType : public ScriptInternalVariable {
  public:
    SubCycleFrameVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

SubCycleFrameVariableType::SubCycleFrameVariableType()
{
    setName("subCycleFrame");
}

void SubCycleFrameVariableType::getTrackValue(Track* t, ExValue* value)
{
    Loop* l = t->getLoop();
    long frame = l->getFrame();
    long subCycleFrames = l->getSubCycleFrames();

	value->setLong(frame % subCycleFrames);
}

PUBLIC SubCycleFrameVariableType* SubCycleFrameVariable = 
new SubCycleFrameVariableType();

//////////////////////////////////////////////////////////////////////
//
// layerCount
// 
// The number of layers in the current loop.  This is also
// in effect the current layer number since we are always "on"
// the last layer of the loop.  This does not include the number
// of available redo layers.
// 
//////////////////////////////////////////////////////////////////////

class LayerCountVariableType : public ScriptInternalVariable {
  public:
    LayerCountVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

LayerCountVariableType::LayerCountVariableType()
{
    setName("layerCount");
}

void LayerCountVariableType::getTrackValue(Track* t, ExValue* value)
{
	Loop* loop = t->getLoop();
	long count = 0;

	// count backwards from the play layer, the record layer is invisible
	// ?? might want a variable to display the number of *visible*
	// layers if checkpoints are being used

	for (Layer* l = loop->getPlayLayer() ; l != NULL ; l = l->getPrev())
	  count++;

	value->setInt(count);
}

PUBLIC LayerCountVariableType* LayerCountVariable = 
new LayerCountVariableType();

//////////////////////////////////////////////////////////////////////
//
// redoCount
// 
// The number of redo layers in the current loop.  
// 
//////////////////////////////////////////////////////////////////////

class RedoCountVariableType : public ScriptInternalVariable {
  public:
    RedoCountVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

RedoCountVariableType::RedoCountVariableType()
{
    setName("redoCount");
}

void RedoCountVariableType::getTrackValue(Track* t, ExValue* value)
{
	Loop* loop = t->getLoop();
	long count = 0;

	// The redo list uses the mRedo field with each link 
	// being a possible checkpoint chain using the mPrev field.
	// Since layerCount returns all layers, not just the visible ones
	// we probably need to do the same here.

	for (Layer* redo = loop->getRedoLayer(); redo != NULL ; 
		 redo = redo->getRedo()) {

		for (Layer* l = redo ; l != NULL ; l = l->getPrev())
		  count++;
	}

	value->setInt(count);
}

PUBLIC RedoCountVariableType* RedoCountVariable = 
new RedoCountVariableType();

//////////////////////////////////////////////////////////////////////
//
// effectiveFeedback
// 
// The value of the feedback currently being applied.  This
// will either be the FeedbackLevel or AltFeedbackLevel parameter values
// depending on AltFeedbackEnable.  It will be zero if we're in Replace, Insert
// or another mode that does not bring forward any content from the
// previous loop.
// 
//////////////////////////////////////////////////////////////////////

class EffectiveFeedbackVariableType : public ScriptInternalVariable {
  public:
    EffectiveFeedbackVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

EffectiveFeedbackVariableType::EffectiveFeedbackVariableType()
{
    setName("effectiveFeedback");
}

void EffectiveFeedbackVariableType::getTrackValue(Track* t, ExValue* value)
{
	Loop* loop = t->getLoop();
    value->setInt(loop->getEffectiveFeedback());
}

PUBLIC EffectiveFeedbackVariableType* EffectiveFeedbackVariable = 
new EffectiveFeedbackVariableType();

/****************************************************************************
 *                                                                          *
 *   								EVENTS                                  *
 *                                                                          *
 ****************************************************************************/

//////////////////////////////////////////////////////////////////////
//
// nextEvent
//
// Returns the type name of the next event.  Child events are ignored
// so we will skip over JumpPlayEvents.  
// Now that we have this, could eliminate InReturn and InRealign.
//
//////////////////////////////////////////////////////////////////////

class NextEventVariableType : public ScriptInternalVariable {
  public:
    NextEventVariableType();
    void getTrackValue(Track* t, ExValue* value);
    virtual const char* getReturnValue(Event* e);
};

NextEventVariableType::NextEventVariableType()
{
    setName("nextEvent");
}

void NextEventVariableType::getTrackValue(Track* t, ExValue* value)
{
	Event* found = NULL;
    EventManager* em = t->getEventManager();

	// Return the next parent event.  Assuming that these will
	// be scheduled in time order so we don't have to sort them.
	// Since we're "in the interrupt" and not modifying the list, we
	// don't have to worry about csects

	for (Event* e = em->getEvents() ; e != NULL ; e = e->getNext()) {
		if (e->getParent() == NULL) {
			found = e;
			break;
		}
	}

	if (found == NULL)
	  value->setNull();
	else 
	  value->setString(getReturnValue(found));
}

const char* NextEventVariableType::getReturnValue(Event* e)
{
	return e->type->name;
}

PUBLIC NextEventVariableType* NextEventVariable = new NextEventVariableType();

//////////////////////////////////////////////////////////////////////
//
// nextEventFunction
//
// Returns the function name associated with the next event.
// We subclass NextEventVariableType for the getTrackValue logic.
//
//////////////////////////////////////////////////////////////////////

class NextEventFunctionVariableType : public NextEventVariableType {
  public:
    NextEventFunctionVariableType();
    const char* getReturnValue(Event* e);
};

NextEventFunctionVariableType::NextEventFunctionVariableType()
{
    setName("nextEventFunction");
}

/**
 * Overloaded from NextEventVariableType to return the 
 * associated function name rather than the event type name.
 */
const char* NextEventFunctionVariableType::getReturnValue(Event* e)
{
	const char* value = NULL;
	Function* f = e->function;
	if (f != NULL)
	  value = f->getName();
	return value;
}

PUBLIC NextEventFunctionVariableType* NextEventFunctionVariable = 
new NextEventFunctionVariableType();

//////////////////////////////////////////////////////////////////////
// 
// nextLoop
//
// The number of the next loop if we're in loop switch mode.
// Loops are numbered from 1.  Returns zero if we're not loop switching.
// 
// !! This is something that would be useful to change.
// 
//////////////////////////////////////////////////////////////////////

class NextLoopVariableType : public ScriptInternalVariable {
  public:
    NextLoopVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

NextLoopVariableType::NextLoopVariableType()
{
    setName("nextLoop");
}

void NextLoopVariableType::getTrackValue(Track* t, ExValue* value)
{
	value->setInt(t->getLoop()->getNextLoop());
}

PUBLIC NextLoopVariableType* NextLoopVariable = new NextLoopVariableType();

//////////////////////////////////////////////////////////////////////
//
// eventSummary
//
// Returns a string representation of all scheduled events.
// This is intended only for testing, the syntax is undefined.
//
//////////////////////////////////////////////////////////////////////

class EventSummaryVariableType : public ScriptInternalVariable {
  public:
    EventSummaryVariableType();
    void getTrackValue(Track* t, ExValue* value);
  private:
    int getEventIndex(Event* list, Event* event);

};

EventSummaryVariableType::EventSummaryVariableType()
{
    setName("eventSummary");
}

void EventSummaryVariableType::getTrackValue(Track* t, ExValue* value)
{
    EventManager* em = t->getEventManager();

    // in theory this can be large, so use a Vbuf
    Vbuf* buf = new Vbuf();

    Event* eventList = em->getEvents();
    int ecount = 0;
	for (Event* e = eventList ; e != NULL ; e = e->getNext()) {

        if (ecount > 0)
          buf->add(",");
        ecount++;

        buf->add(e->type->name);
        buf->add("(");
        if (e->pending)
          buf->add("pending");
        else {
            buf->add("f=");
            buf->add(e->frame);
        }

        if (e->getChildren() != NULL) {
            int ccount = 0;
            buf->add(",c=");
            for (Event* c = e->getChildren() ; c != NULL ; c = c->getSibling()) {
                if (ccount > 0)
                  buf->add(",");
                ccount++;
                // prefix scheduled events with a number so we can
                // see sharing
                if (c->getList() != NULL) {
                    buf->add(getEventIndex(eventList, c));
                    buf->add(":");
                }
                buf->add(c->type->name);
            }
        }

        buf->add(")");
	}
    
    if (buf->getSize() == 0)
      value->setNull();
    else
      value->setString(buf->getBuffer());

    delete buf;
}

PRIVATE int EventSummaryVariableType::getEventIndex(Event* list, Event* event)
{
    int index = 0;

    if (list != NULL && event != NULL) {
        int i = 1;
        for (Event* e = list ; e != NULL ; e = e->getNext()) {
            if (e != event)
              i++;
            else {
                index = i;
                break;
            }
        }
    }

    return index;
}


PUBLIC EventSummaryVariableType* EventSummaryVariable = new EventSummaryVariableType();

/****************************************************************************
 *                                                                          *
 *   								MODES                                   *
 *                                                                          *
 ****************************************************************************/

//////////////////////////////////////////////////////////////////////
// 
// mode
//
// Name of the current mode.
//
//////////////////////////////////////////////////////////////////////

class ModeVariableType : public ScriptInternalVariable {
  public:
    ModeVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

ModeVariableType::ModeVariableType()
{
    setName("mode");
}

void ModeVariableType::getTrackValue(Track* t, ExValue* value)
{
	value->setString(t->getLoop()->getMode()->getName());
}

PUBLIC ModeVariableType* ModeVariable = new ModeVariableType();

//////////////////////////////////////////////////////////////////////
// 
// isRecording
//
// True any form of recording is being performed.  Note that this
// does not necessarily mean you are in Record mode, you could be in 
// Overdub, Multiply, Insert, etc.
//
//////////////////////////////////////////////////////////////////////

class IsRecordingVariableType : public ScriptInternalVariable {
  public:
    IsRecordingVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

IsRecordingVariableType::IsRecordingVariableType()
{
    setName("isRecording");
}

void IsRecordingVariableType::getTrackValue(Track* t, ExValue* value)
{
	value->setBool(t->getLoop()->isRecording());
}

PUBLIC IsRecordingVariableType* IsRecordingVariable = new IsRecordingVariableType();

//////////////////////////////////////////////////////////////////////
// 
// inOverdub
//
// True if overdub is enabled.   Note that this doesn't necessarily
// mean that the mode is overdub, only that overdub is enabled when
// we fall back into Play mode.
//
//////////////////////////////////////////////////////////////////////

class InOverdubVariableType : public ScriptInternalVariable {
  public:
    InOverdubVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

InOverdubVariableType::InOverdubVariableType()
{
    setName("inOverdub");
}

void InOverdubVariableType::getTrackValue(Track* t, ExValue* value)
{
	value->setBool(t->getLoop()->isOverdub());
}

PUBLIC InOverdubVariableType* InOverdubVariable = new InOverdubVariableType();

//////////////////////////////////////////////////////////////////////
// 
// inHalfspeed
//
// True if half-speed is enabled.
//
//////////////////////////////////////////////////////////////////////

class InHalfspeedVariableType : public ScriptInternalVariable {
  public:
    InHalfspeedVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

InHalfspeedVariableType::InHalfspeedVariableType()
{
    setName("inHalfspeed");
}

/**
 * This is more complicated now that we've genearlized speed shift.
 * Assume that if the rate toggle is -12 we're in half speed.
 */
void InHalfspeedVariableType::getTrackValue(Track* t, ExValue* value)
{
	value->setBool(t->getSpeedToggle() == -12);
}

PUBLIC InHalfspeedVariableType* InHalfspeedVariable = 
new InHalfspeedVariableType();

//////////////////////////////////////////////////////////////////////
// 
// inReverse
//
// True if reverse is enabled.
// Would be nice to have "direction" with values "reverse" and "forward"?
//
//////////////////////////////////////////////////////////////////////

class InReverseVariableType : public ScriptInternalVariable {
  public:
    InReverseVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

InReverseVariableType::InReverseVariableType()
{
    setName("inReverse");
}

void InReverseVariableType::getTrackValue(Track* t, ExValue* value)
{
	value->setBool(t->getLoop()->isReverse());
}

PUBLIC InReverseVariableType* InReverseVariable = new InReverseVariableType();

//////////////////////////////////////////////////////////////////////
// 
// inMute
//
// True if playback is muted.  This usually means that we're
// also in Mute mode, but if Overdub is also on, mode
// will be Overdub.  Note also that this tests the isMute flag
// which can be on for other reasons than being in Mute mode.
// 
// !! Think more about who should win.
// Do we need both inMuteMode and inMute or maybe
// isMuted and inMuteMode
//
//////////////////////////////////////////////////////////////////////

class InMuteVariableType : public ScriptInternalVariable {
  public:
    InMuteVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

InMuteVariableType::InMuteVariableType()
{
    setName("inMute");
}

void InMuteVariableType::getTrackValue(Track* t, ExValue* value)
{
	value->setBool(t->getLoop()->isMuteMode());
}

PUBLIC InMuteVariableType* InMuteVariable = new InMuteVariableType();

//////////////////////////////////////////////////////////////////////
// 
// inPause
//
// True if we're in Pause or Pause mode.
// This is available because the "mode" parameter is not always
// set to Pause.  Once case is if Pause and Overdub are on at the same
// time mode will be Overdub (I think this is the only case).
//
//////////////////////////////////////////////////////////////////////

class InPauseVariableType : public ScriptInternalVariable {
  public:
    InPauseVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

InPauseVariableType::InPauseVariableType()
{
    setName("inPause");
}

void InPauseVariableType::getTrackValue(Track* t, ExValue* value)
{
	value->setBool(t->getLoop()->isPaused());
}

PUBLIC InPauseVariableType* InPauseVariable = new InPauseVariableType();

//////////////////////////////////////////////////////////////////////
// 
// inRealign
//
// True if we're realigning.  This similar to a mode, but 
// it is indiciated by having a Realign event scheduled.
//
//////////////////////////////////////////////////////////////////////

class InRealignVariableType : public ScriptInternalVariable {
  public:
    InRealignVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

InRealignVariableType::InRealignVariableType()
{
    setName("inRealign");
}

void InRealignVariableType::getTrackValue(Track* t, ExValue* value)
{
    EventManager* em = t->getEventManager();
	Event* e = em->findEvent(RealignEvent);
	value->setBool(e != NULL);
}

PUBLIC InRealignVariableType* InRealignVariable = new InRealignVariableType();

//////////////////////////////////////////////////////////////////////
// 
// inReturn
//
// True if we're in "return" mode.  This is a special minor mode that
// happens after a loop switch with SwitchDuration=OnceReturn, 
// SwitchDuration=SustainReturn, or the RestartOnce function.  
// It is indiciated by the presence of a pending Return event.
//
//////////////////////////////////////////////////////////////////////

class InReturnVariableType : public ScriptInternalVariable {
  public:
    InReturnVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

InReturnVariableType::InReturnVariableType()
{
    setName("inReturn");
}

void InReturnVariableType::getTrackValue(Track* t, ExValue* value)
{
    EventManager* em = t->getEventManager();
	Event* e = em->findEvent(ReturnEvent);
	value->setBool(e != NULL);
}

PUBLIC InReturnVariableType* InReturnVariable = new InReturnVariableType();

//////////////////////////////////////////////////////////////////////
// 
// rate
//
// Same as the speedStep parameter.  I would rather not have this but it's
// been used for a long time.
//
// We also exposed "scaleRate" for awhile but I don't think that
// was in wide use.  Get everyone to move over to speedStep.
//
//////////////////////////////////////////////////////////////////////

class RateVariableType : public ScriptInternalVariable {
  public:
    RateVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

RateVariableType::RateVariableType()
{
    setName("rate");
}

void RateVariableType::getTrackValue(Track* t, ExValue* value)
{
	value->setInt(t->getSpeedStep());
}

PUBLIC RateVariableType* RateVariable = new RateVariableType();

//////////////////////////////////////////////////////////////////////
// 
// rawSpeed, rawRate
//
// Playback speed, expressed as a float x1000000.
// rawRate was the original name but we can probably get rid of that
// as it's only useful in scripts.
//
// !! effectiveSpeed would be better
//
//////////////////////////////////////////////////////////////////////

class RawSpeedVariableType : public ScriptInternalVariable {
  public:
    RawSpeedVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

RawSpeedVariableType::RawSpeedVariableType()
{
    setName("rawSpeed");
    setAlias("rawRate");
}

void RawSpeedVariableType::getTrackValue(Track* t, ExValue* value)
{
	value->setLong((long)(t->getEffectiveSpeed() * 1000000));
}

PUBLIC RawSpeedVariableType* RawSpeedVariable = new RawSpeedVariableType();

//////////////////////////////////////////////////////////////////////
// 
// rawPitch
//
// Playback pitch, expressed as a float x1000000.
//
//////////////////////////////////////////////////////////////////////

class RawPitchVariableType : public ScriptInternalVariable {
  public:
    RawPitchVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

RawPitchVariableType::RawPitchVariableType()
{
    setName("rawPitch");
}

void RawPitchVariableType::getTrackValue(Track* t, ExValue* value)
{
	value->setLong((long)(t->getEffectivePitch() * 1000000));
}

PUBLIC RawPitchVariableType* RawPitchVariable = new RawPitchVariableType();

//////////////////////////////////////////////////////////////////////
//
// speedToggle
//
// The effective speed toggle in a track.
// This is a generalization of Halfspeed, the SpeedToggle script function
// can be used to toggle on or off at any step interval.
//
//////////////////////////////////////////////////////////////////////

class SpeedToggleVariableType : public ScriptInternalVariable {
  public:
    SpeedToggleVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

SpeedToggleVariableType::SpeedToggleVariableType()
{
    setName("speedToggle");
}

void SpeedToggleVariableType::getTrackValue(Track* t, ExValue* value)
{
	value->setInt(t->getSpeedToggle());
}

PUBLIC SpeedToggleVariableType* SpeedToggleVariable = new SpeedToggleVariableType();

//////////////////////////////////////////////////////////////////////
//
// speedSequenceIndex
//
// The speed sequence index in a track.
// This is normally changed by the SpeedNext and SpeedPrev functions,
// and may be set to a specific value through this variable.
//
//////////////////////////////////////////////////////////////////////

class SpeedSequenceIndexVariableType : public ScriptInternalVariable {
  public:
    SpeedSequenceIndexVariableType();
    void getTrackValue(Track* t, ExValue* value);
    void setValue(ScriptInterpreter* si, ExValue* value);
};

SpeedSequenceIndexVariableType::SpeedSequenceIndexVariableType()
{
    setName("speedSequenceIndex");
}

void SpeedSequenceIndexVariableType::getTrackValue(Track* t, ExValue* value)
{
	value->setInt(t->getSpeedSequenceIndex());
}

void SpeedSequenceIndexVariableType::setValue(ScriptInterpreter* si, 
                                              ExValue* value)
{
    int index = value->getInt();
    // Track doesn't do any range checking, at least
    // catch negatives, could check the sequence parameter
    if (index < 0) index = 0;

    Track* t = si->getTargetTrack();
    t->setSpeedSequenceIndex(index);
}

PUBLIC SpeedSequenceIndexVariableType* SpeedSequenceIndexVariable = new SpeedSequenceIndexVariableType();

//////////////////////////////////////////////////////////////////////
//
// pitchSequenceIndex
//
// The pitch sequence index in a track.
// This is normally changed by the PitchNext and PitchPrev functions,
// and may be set to a specific value through this variable.
//
//////////////////////////////////////////////////////////////////////

class PitchSequenceIndexVariableType : public ScriptInternalVariable {
  public:
    PitchSequenceIndexVariableType();
    void getTrackValue(Track* t, ExValue* value);
    void setValue(ScriptInterpreter* si, ExValue* value);
};

PitchSequenceIndexVariableType::PitchSequenceIndexVariableType()
{
    setName("pitchSequenceIndex");
}

void PitchSequenceIndexVariableType::getTrackValue(Track* t, ExValue* value)
{
	value->setInt(t->getPitchSequenceIndex());
}

void PitchSequenceIndexVariableType::setValue(ScriptInterpreter* si, 
                                              ExValue* value)
{
    int index = value->getInt();
    // Track doesn't do any range checking, at least
    // catch negatives, could check the sequence parameter
    if (index < 0) index = 0;

    Track* t = si->getTargetTrack();
    t->setPitchSequenceIndex(index);
}

PUBLIC PitchSequenceIndexVariableType* PitchSequenceIndexVariable = new PitchSequenceIndexVariableType();

//////////////////////////////////////////////////////////////////////
// 
// historyFrames
//
// The total number of frames in all loop layers.
// Used to determine the relative location of the loop window.
//
//////////////////////////////////////////////////////////////////////

class HistoryFramesVariableType : public ScriptInternalVariable {
  public:
    HistoryFramesVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

HistoryFramesVariableType::HistoryFramesVariableType()
{
    setName("historyFrames");
}

void HistoryFramesVariableType::getTrackValue(Track* t, ExValue* value)
{
	value->setLong(t->getLoop()->getHistoryFrames());
}

PUBLIC HistoryFramesVariableType* HistoryFramesVariable = 
new HistoryFramesVariableType();

//////////////////////////////////////////////////////////////////////
// 
// windowOffset
//
// The offset in frames of the current loop window within the
// entire loop history.  If a window is not active the value is -1.
//
//////////////////////////////////////////////////////////////////////

class WindowOffsetVariableType : public ScriptInternalVariable {
  public:
    WindowOffsetVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

WindowOffsetVariableType::WindowOffsetVariableType()
{
    setName("windowOffset");
}

void WindowOffsetVariableType::getTrackValue(Track* t, ExValue* value)
{
	value->setLong(t->getLoop()->getWindowOffset());
}

PUBLIC WindowOffsetVariableType* WindowOffsetVariable = new WindowOffsetVariableType();

/****************************************************************************
 *                                                                          *
 *   							 TRACK STATE                                *
 *                                                                          *
 ****************************************************************************/

//////////////////////////////////////////////////////////////////////
//
// trackCount
//
// The number of tracks configured.
//
//////////////////////////////////////////////////////////////////////

class TrackCountVariableType : public ScriptInternalVariable {

  public:
    TrackCountVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

TrackCountVariableType::TrackCountVariableType()
{
    setName("trackCount");
}

void TrackCountVariableType::getTrackValue(Track* t, ExValue* value)
{
	Mobius* m = t->getMobius();
    value->setInt(m->getTrackCount());
}

PUBLIC TrackCountVariableType* TrackCountVariable = 
new TrackCountVariableType();


//////////////////////////////////////////////////////////////////////
//
// track, trackNumber
// 
// The number of the current track.  The first track is 1.
// 
//////////////////////////////////////////////////////////////////////

class TrackVariableType : public ScriptInternalVariable {
  public:
    TrackVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

TrackVariableType::TrackVariableType()
{
    setName("track");
	// for consistency with loopNumber and layerNumber
    setAlias("trackNumber");
}

void TrackVariableType::getTrackValue(Track* t, ExValue* value)
{
	value->setLong(t->getDisplayNumber());
}

PUBLIC TrackVariableType* TrackVariable = new TrackVariableType();

//////////////////////////////////////////////////////////////////////
// 
// globalMute
// 
// True if the track will be unmuted when Global Mute mode is over.
//
// ?? Do we need a simple global variable to indiciate that we're in
// global mute mode?
//
//////////////////////////////////////////////////////////////////////

class GlobalMuteVariableType : public ScriptInternalVariable {
  public:
    GlobalMuteVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

GlobalMuteVariableType::GlobalMuteVariableType()
{
    setName("globalMute");
}

void GlobalMuteVariableType::getTrackValue(Track* t, ExValue* value)
{
	value->setBool(t->isGlobalMute());
}

PUBLIC GlobalMuteVariableType* GlobalMuteVariable = 
new GlobalMuteVariableType();

//////////////////////////////////////////////////////////////////////
// 
// solo
// 
//
// True if the track will be unmuted when Global Mute mode is over.
//
// ?? Do we need a simple global variable to indiciate that we're in
// global mute mode?
//
//////////////////////////////////////////////////////////////////////

class SoloVariableType : public ScriptInternalVariable {
  public:
    SoloVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

SoloVariableType::SoloVariableType()
{
    setName("solo");
}

void SoloVariableType::getTrackValue(Track* t, ExValue* value)
{
	value->setBool(t->isSolo());
}

PUBLIC SoloVariableType* SoloVariable = new SoloVariableType();

//////////////////////////////////////////////////////////////////////
//
// trackSyncMaster
//
// The number of the track operating as the track sync master,
// 0 if there is no master.
// 
// Note that internal track number are zero based, but the track variables
// are all 1 based for consistency with "for" statements.
//////////////////////////////////////////////////////////////////////

class TrackSyncMasterVariableType : public ScriptInternalVariable {
  public:
    TrackSyncMasterVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

TrackSyncMasterVariableType::TrackSyncMasterVariableType()
{
    setName("trackSyncMaster");
}

void TrackSyncMasterVariableType::getTrackValue(Track* t, ExValue* value)
{
	int number = 0;
	Synchronizer* s = t->getSynchronizer();
	Track* master = s->getTrackSyncMaster();
	if (master != NULL)
	  number = master->getDisplayNumber();
	value->setInt(number);
}

PUBLIC TrackSyncMasterVariableType* TrackSyncMasterVariable = 
new TrackSyncMasterVariableType();

//////////////////////////////////////////////////////////////////////
//
// outSyncMaster
//
// The number of the track operating as the output sync master,
// 0 if there is no master.
// 
// Note that internal track number are zero based, but the track variables
// are all 1 based for consistency with "for" statements.
//
//////////////////////////////////////////////////////////////////////

class OutSyncMasterVariableType : public ScriptInternalVariable {
  public:
    OutSyncMasterVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

OutSyncMasterVariableType::OutSyncMasterVariableType()
{
    setName("outSyncMaster");
}

void OutSyncMasterVariableType::getTrackValue(Track* t, ExValue* value)
{
	int number = 0;
	Synchronizer* s = t->getSynchronizer();
	Track* master = s->getOutSyncMaster();
	if (master != NULL)
	  number = master->getDisplayNumber();
	value->setInt(number);
}

PUBLIC OutSyncMasterVariableType* OutSyncMasterVariable = 
new OutSyncMasterVariableType();

/****************************************************************************
 *                                                                          *
 *   						  COMMON SYNC STATE                             *
 *                                                                          *
 ****************************************************************************/

//////////////////////////////////////////////////////////////////////
//
// tempo
//
// The current sync tempo.  For Sync=Out this is the tempo we calculated.
// For Sync=In this is the tempo we're smoothing from the external source.
// For Sync=Host this is the tempo reported by the host.
//
//////////////////////////////////////////////////////////////////////

class SyncTempoVariableType : public ScriptInternalVariable {
  public:
    SyncTempoVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

SyncTempoVariableType::SyncTempoVariableType()
{
    setName("syncTempo");
}

void SyncTempoVariableType::getTrackValue(Track* t, ExValue* value)
{
	Synchronizer* s = t->getSynchronizer();
	float tempo = s->getTempo(t);

	// assume its ok to truncate this one, if you want something
	// more accurate could have a RealSyncTempoVariable?
	value->setLong((long)tempo);
}

PUBLIC SyncTempoVariableType* SyncTempoVariable = new SyncTempoVariableType();

//////////////////////////////////////////////////////////////////////
//
// syncRawBeat
//
// The current absolute beat count.
// This will be the same as syncOutRawBeat, syncInRawBeat, 
// or syncHostRawBeat depending on the SyncMode of the current track.
//
//////////////////////////////////////////////////////////////////////

class SyncRawBeatVariableType : public ScriptInternalVariable {
  public:
    SyncRawBeatVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

SyncRawBeatVariableType::SyncRawBeatVariableType()
{
    setName("syncRawBeat");
}

void SyncRawBeatVariableType::getTrackValue(Track* t, ExValue* value)
{
	Synchronizer* s = t->getSynchronizer();
    value->setInt(s->getRawBeat(t));
}

PUBLIC SyncRawBeatVariableType* SyncRawBeatVariable = 
new SyncRawBeatVariableType();

//////////////////////////////////////////////////////////////////////
//
// syncBeat
//
// The current bar relative beat count.
// This will be the same as syncOutBeat, syncInBeat, or syncHostBeat
// depending on the SyncMode of the current track.
//
//////////////////////////////////////////////////////////////////////

class SyncBeatVariableType : public ScriptInternalVariable {
  public:
    SyncBeatVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

SyncBeatVariableType::SyncBeatVariableType()
{
    setName("syncBeat");
}

void SyncBeatVariableType::getTrackValue(Track* t, ExValue* value)
{
	Synchronizer* s = t->getSynchronizer();
    value->setInt(s->getBeat(t));
}

PUBLIC SyncBeatVariableType* SyncBeatVariable = new SyncBeatVariableType();

//////////////////////////////////////////////////////////////////////
//
// syncBar
//
// The current bar count.
// This will be the same as syncOutBar, syncInBar, or syncHostBar
// depending on the SyncMode of the current track.
//
//////////////////////////////////////////////////////////////////////

class SyncBarVariableType : public ScriptInternalVariable {
  public:
    SyncBarVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

SyncBarVariableType::SyncBarVariableType()
{
    setName("syncBar");
}

void SyncBarVariableType::getTrackValue(Track* t, ExValue* value)
{
	Synchronizer* s = t->getSynchronizer();
    value->setInt(s->getBar(t));
}

PUBLIC SyncBarVariableType* SyncBarVariable = new SyncBarVariableType();

//////////////////////////////////////////////////////////////////////
//
// syncPulses
//
// The number of pulses in the sync tracker.
//
//////////////////////////////////////////////////////////////////////

class SyncPulsesVariableType : public ScriptInternalVariable {
  public:
    SyncPulsesVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

SyncPulsesVariableType::SyncPulsesVariableType()
{
    setName("syncPulses");
}

void SyncPulsesVariableType::getTrackValue(Track* t, ExValue* value)
{
    Synchronizer* s = t->getSynchronizer();
    SyncTracker* tracker = s->getSyncTracker(t);
    if (tracker == NULL)
      value->setInt(0);
    else {
        // since resizes are deferred until the next pulse, look there first
        value->setInt(tracker->getFutureLoopPulses());
    }
}

PUBLIC SyncPulsesVariableType* SyncPulsesVariable = 
new SyncPulsesVariableType();

//////////////////////////////////////////////////////////////////////
//
// syncPulse
//
// The current pulse in the sync tracker for this track.
//
//////////////////////////////////////////////////////////////////////

class SyncPulseVariableType : public ScriptInternalVariable {
  public:
    SyncPulseVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

SyncPulseVariableType::SyncPulseVariableType()
{
    setName("syncPulse");
}

void SyncPulseVariableType::getTrackValue(Track* t, ExValue* value)
{
    Synchronizer* s = t->getSynchronizer();
    SyncTracker* tracker = s->getSyncTracker(t);
    if (tracker != NULL)
      value->setLong(tracker->getPulse());
    else
      value->setNull();
}

PUBLIC SyncPulseVariableType* SyncPulseVariable = 
new SyncPulseVariableType();

//////////////////////////////////////////////////////////////////////
//
// syncPulseFrames
//
// The length of the sync pulse in frames.
//
//////////////////////////////////////////////////////////////////////

class SyncPulseFramesVariableType : public ScriptInternalVariable {
  public:
    SyncPulseFramesVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

SyncPulseFramesVariableType::SyncPulseFramesVariableType()
{
    setName("syncPulseFrames");
}

void SyncPulseFramesVariableType::getTrackValue(Track* t, ExValue* value)
{
    Synchronizer* s = t->getSynchronizer();
    SyncTracker* tracker = s->getSyncTracker(t);
    if (tracker != NULL)
      value->setFloat(tracker->getPulseFrames());
    else
      value->setNull();
}

PUBLIC SyncPulseFramesVariableType* SyncPulseFramesVariable = 
new SyncPulseFramesVariableType();

//////////////////////////////////////////////////////////////////////
//
// syncLoopFrames
//
// The length of the sync loop in frames.
//
//////////////////////////////////////////////////////////////////////

class SyncLoopFramesVariableType : public ScriptInternalVariable {
  public:
    SyncLoopFramesVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

SyncLoopFramesVariableType::SyncLoopFramesVariableType()
{
    setName("syncLoopFrames");
}

void SyncLoopFramesVariableType::getTrackValue(Track* t, ExValue* value)
{
    Synchronizer* s = t->getSynchronizer();
    SyncTracker* tracker = s->getSyncTracker(t);
    if (tracker != NULL)
      value->setLong(tracker->getFutureLoopFrames());
    else
      value->setNull();
}

PUBLIC SyncLoopFramesVariableType* SyncLoopFramesVariable = 
new SyncLoopFramesVariableType();

//////////////////////////////////////////////////////////////////////
//
// syncAudioFrame
//
// The actual Loop frame at the last pulse.  The difference between
// this and SyncPulseFrame is the amount of drift (after wrapping).
//
//////////////////////////////////////////////////////////////////////

class SyncAudioFrameVariableType : public ScriptInternalVariable {
  public:
    SyncAudioFrameVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

SyncAudioFrameVariableType::SyncAudioFrameVariableType()
{
    setName("syncAudioFrame");
}

void SyncAudioFrameVariableType::getTrackValue(Track* t, ExValue* value)
{
    Synchronizer* s = t->getSynchronizer();
    SyncTracker* tracker = s->getSyncTracker(t);
    if (tracker != NULL)
      value->setLong(tracker->getAudioFrame());
    else
      value->setNull();
}

PUBLIC SyncAudioFrameVariableType* SyncAudioFrameVariable = 
new SyncAudioFrameVariableType();

//////////////////////////////////////////////////////////////////////
//
// syncDrift
//
// The current amount of drift calculated on the last pulse.
// This will be a positive or negative number.
//
//////////////////////////////////////////////////////////////////////

class SyncDriftVariableType : public ScriptInternalVariable {
  public:
    SyncDriftVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

SyncDriftVariableType::SyncDriftVariableType()
{
    setName("syncDrift");
}

void SyncDriftVariableType::getTrackValue(Track* t, ExValue* value)
{
    Synchronizer* s = t->getSynchronizer();
    SyncTracker* tracker = s->getSyncTracker(t);
    if (tracker != NULL)
      value->setLong((long)(tracker->getDrift()));
    else
      value->setNull();
}

PUBLIC SyncDriftVariableType* SyncDriftVariable = 
new SyncDriftVariableType();

//////////////////////////////////////////////////////////////////////
//
// syncAverageDrift
//
// The average amount of drift over the last 96 pulses.
// This may be more accurate than the last drift sometimes though
// we're not using it in the tests.
//
//////////////////////////////////////////////////////////////////////

class SyncAverageDriftVariableType : public ScriptInternalVariable {
  public:
    SyncAverageDriftVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

SyncAverageDriftVariableType::SyncAverageDriftVariableType()
{
    setName("syncAverageDrift");
}

void SyncAverageDriftVariableType::getTrackValue(Track* t, ExValue* value)
{
    Synchronizer* s = t->getSynchronizer();
    SyncTracker* tracker = s->getSyncTracker(t);
    if (tracker != NULL)
      value->setLong((long)(tracker->getAverageDrift()));
    else
      value->setNull();
}

PUBLIC SyncAverageDriftVariableType* SyncAverageDriftVariable = 
new SyncAverageDriftVariableType();

//////////////////////////////////////////////////////////////////////
//
// syncDriftChecks
//
// The number of sync drift checks that have been perfomed with
// this tracker.
//
//////////////////////////////////////////////////////////////////////

class SyncDriftChecksVariableType : public ScriptInternalVariable {
  public:
    SyncDriftChecksVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

SyncDriftChecksVariableType::SyncDriftChecksVariableType()
{
    setName("syncDriftChecks");
}

void SyncDriftChecksVariableType::getTrackValue(Track* t, ExValue* value)
{
    Synchronizer* s = t->getSynchronizer();
    SyncTracker* tracker = s->getSyncTracker(t);
    if (tracker != NULL)
      value->setInt(tracker->getDriftChecks());
    else
      value->setNull();
}

PUBLIC SyncDriftChecksVariableType* SyncDriftChecksVariable = 
new SyncDriftChecksVariableType();

//////////////////////////////////////////////////////////////////////
//
// syncCorrections
//
// The number of sync drift corrections that have been perfomed with
// this tracker.
//
//////////////////////////////////////////////////////////////////////

class SyncCorrectionsVariableType : public ScriptInternalVariable {
  public:
    SyncCorrectionsVariableType();
    void getTrackValue(Track* t, ExValue* value);
	void setValue(ScriptInterpreter* si, ExValue* value);
};

SyncCorrectionsVariableType::SyncCorrectionsVariableType()
{
    setName("syncCorrections");
}

void SyncCorrectionsVariableType::getTrackValue(Track* t, ExValue* value)
{
    Synchronizer* s = t->getSynchronizer();
    SyncTracker* tracker = s->getSyncTracker(t);
    if (tracker != NULL)
      value->setInt(tracker->getDriftCorrections());
    else
      value->setNull();
}

/**
 * This is one of the few variables that has a setter.
 * We allow this so we can force a drift realign, then reset the counter
 * so we can continue testing for zero in other parts of the test.
 */
PUBLIC void SyncCorrectionsVariableType::setValue(ScriptInterpreter* si, 
                                                  ExValue* value)
{
    Track* t = si->getTargetTrack();
    Synchronizer* s = t->getSynchronizer();
    SyncTracker* tracker = s->getSyncTracker(t);
    if (tracker != NULL)
      tracker->setDriftCorrections(value->getInt());
}

PUBLIC SyncCorrectionsVariableType* SyncCorrectionsVariable = 
new SyncCorrectionsVariableType();

//////////////////////////////////////////////////////////////////////
//
// syncDealign
//
// The number of frames the current track is dealigned from the
// sync tracker for this track.
//
//////////////////////////////////////////////////////////////////////

class SyncDealignVariableType : public ScriptInternalVariable {
  public:
    SyncDealignVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

SyncDealignVariableType::SyncDealignVariableType()
{
    setName("syncDealign");
}

void SyncDealignVariableType::getTrackValue(Track* t, ExValue* value)
{
    Synchronizer* s = t->getSynchronizer();
    SyncTracker* tracker = s->getSyncTracker(t);
    if (tracker == NULL)
      value->setInt(0);
    else
      value->setLong(tracker->getDealign(t));
}

PUBLIC SyncDealignVariableType* SyncDealignVariable = 
new SyncDealignVariableType();

//////////////////////////////////////////////////////////////////////
//
// syncPreRealignFrame
//
// The Loop frame prior to the last Realign.
//
//////////////////////////////////////////////////////////////////////

class SyncPreRealignFrameVariableType : public ScriptInternalVariable {
  public:
    SyncPreRealignFrameVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

SyncPreRealignFrameVariableType::SyncPreRealignFrameVariableType()
{
    setName("syncPreRealignFrame");
}

void SyncPreRealignFrameVariableType::getTrackValue(Track* t, ExValue* value)
{
    SyncState* state = t->getSyncState();
	value->setInt(state->getPreRealignFrame());
}

PUBLIC SyncPreRealignFrameVariableType* SyncPreRealignFrameVariable = 
new SyncPreRealignFrameVariableType();

//////////////////////////////////////////////////////////////////////
//
// syncCyclePulses
//
// The number of external sync pulses counted during recording.
//
//////////////////////////////////////////////////////////////////////

class SyncCyclePulsesVariableType : public ScriptInternalVariable {
  public:
    SyncCyclePulsesVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

SyncCyclePulsesVariableType::SyncCyclePulsesVariableType()
{
    setName("syncCyclePulses");
}

void SyncCyclePulsesVariableType::getTrackValue(Track* t, ExValue* value)
{
    SyncState* state = t->getSyncState();
	value->setInt(state->getCyclePulses());
}

PUBLIC SyncCyclePulsesVariableType* SyncCyclePulsesVariable = 
new SyncCyclePulsesVariableType();

/****************************************************************************
 *                                                                          *
 *   							   OUT SYNC                                 *
 *                                                                          *
 ****************************************************************************/

//////////////////////////////////////////////////////////////////////
//
// syncOutTempo
//
// The tempo of the internal clock used for out sync.
// This is the same value returned by "tempo" but only if the
// current track is in Sync=Out or Sync=OutUserStart.
//
//////////////////////////////////////////////////////////////////////

class SyncOutTempoVariableType : public ScriptInternalVariable {
  public:
    SyncOutTempoVariableType();
	void getTrackValue(Track* t, ExValue* value);
};

SyncOutTempoVariableType::SyncOutTempoVariableType()
{
    setName("syncOutTempo");
}

void SyncOutTempoVariableType::getTrackValue(Track* t, ExValue* value)
{
	float tempo = t->getSynchronizer()->getOutTempo();

	// assume its ok to truncate this one, if you want something
	// more accurate could have a RealTempoVariable?
	value->setLong((long)tempo);
}

PUBLIC SyncOutTempoVariableType* SyncOutTempoVariable = 
new SyncOutTempoVariableType();

//////////////////////////////////////////////////////////////////////
//
// syncOutRawBeat
//
// The current raw beat count maintained by the internal clock.
// This will be zero if the internal clock is not running.
//
//////////////////////////////////////////////////////////////////////

class SyncOutRawBeatVariableType : public ScriptInternalVariable {
  public:
    SyncOutRawBeatVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

SyncOutRawBeatVariableType::SyncOutRawBeatVariableType()
{
    setName("syncOutRawBeat");
}

void SyncOutRawBeatVariableType::getTrackValue(Track* t, ExValue* value)
{
	Synchronizer* s = t->getSynchronizer();
    value->setInt(s->getOutRawBeat());
}

PUBLIC SyncOutRawBeatVariableType* SyncOutRawBeatVariable = 
new SyncOutRawBeatVariableType();

//////////////////////////////////////////////////////////////////////
//
// syncOutBeat
//
// The current beat count maintained by the internal clock,
// relative to the bar.
//
//////////////////////////////////////////////////////////////////////

class SyncOutBeatVariableType : public ScriptInternalVariable {
  public:
    SyncOutBeatVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

SyncOutBeatVariableType::SyncOutBeatVariableType()
{
    setName("syncOutBeat");
}

void SyncOutBeatVariableType::getTrackValue(Track* t, ExValue* value)
{
	Synchronizer* s = t->getSynchronizer();
    value->setInt(s->getOutBeat());
}

PUBLIC SyncOutBeatVariableType* SyncOutBeatVariable = 
new SyncOutBeatVariableType();

//////////////////////////////////////////////////////////////////////
//
// syncOutBar
//
// The current bar count maintained by the internal clock.
//
//////////////////////////////////////////////////////////////////////

class SyncOutBarVariableType : public ScriptInternalVariable {
  public:
    SyncOutBarVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

SyncOutBarVariableType::SyncOutBarVariableType()
{
    setName("syncOutBar");
}

void SyncOutBarVariableType::getTrackValue(Track* t, ExValue* value)
{
	Synchronizer* s = t->getSynchronizer();
    value->setInt(s->getOutBar());
}

PUBLIC SyncOutBarVariableType* SyncOutBarVariable = 
new SyncOutBarVariableType();

//////////////////////////////////////////////////////////////////////
//
// syncOutSending
//
// "true" if we are currently sending MIDI clocks, "false" if not.
//
//////////////////////////////////////////////////////////////////////

class SyncOutSendingVariableType : public ScriptInternalVariable {
  public:
    SyncOutSendingVariableType();
	void getTrackValue(Track* t, ExValue* value);
};

SyncOutSendingVariableType::SyncOutSendingVariableType()
{
    setName("syncOutSending");
}

void SyncOutSendingVariableType::getTrackValue(Track* t, ExValue* value)
{
	value->setBool(t->getSynchronizer()->isSending());
}

PUBLIC SyncOutSendingVariableType* SyncOutSendingVariable = 
new SyncOutSendingVariableType();

//////////////////////////////////////////////////////////////////////
//
// syncOutStarted
//
// "true" if we have send a MIDI Start message, "false" if not.
//
//////////////////////////////////////////////////////////////////////

class SyncOutStartedVariableType : public ScriptInternalVariable {
  public:
    SyncOutStartedVariableType();
	void getTrackValue(Track* t, ExValue* value);
};

SyncOutStartedVariableType::SyncOutStartedVariableType()
{
    setName("syncOutStarted");
}

void SyncOutStartedVariableType::getTrackValue(Track* t, ExValue* value)
{
	value->setBool(t->getSynchronizer()->isStarted());
}

PUBLIC SyncOutStartedVariableType* SyncOutStartedVariable = 
new SyncOutStartedVariableType();

//////////////////////////////////////////////////////////////////////
//
// syncOutStarts
//
// The number of MIDI Start messages we've sent since the last
// time we were stopped.
//
//////////////////////////////////////////////////////////////////////

class SyncOutStartsVariableType : public ScriptInternalVariable {
  public:
    SyncOutStartsVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

SyncOutStartsVariableType::SyncOutStartsVariableType()
{
    setName("syncOutStarts");
}

void SyncOutStartsVariableType::getTrackValue(Track* t, ExValue* value)
{
	value->setInt(t->getSynchronizer()->getStarts());
}

PUBLIC SyncOutStartsVariableType* SyncOutStartsVariable = 
new SyncOutStartsVariableType();

/****************************************************************************
 *                                                                          *
 *   							  MIDI SYNC                                 *
 *                                                                          *
 ****************************************************************************/

//////////////////////////////////////////////////////////////////////
//
// syncInTempo
//
// The tempo of the external MIDI clock being received.
// This is the same value returned by "tempo" but only if the
// current track SyncMode is In, MIDIBeat, or MIDIBar.
//
//////////////////////////////////////////////////////////////////////

class SyncInTempoVariableType : public ScriptInternalVariable {
  public:
    SyncInTempoVariableType();
	void getTrackValue(Track* t, ExValue* value);
};

SyncInTempoVariableType::SyncInTempoVariableType()
{
    setName("syncInTempo");
}

void SyncInTempoVariableType::getTrackValue(Track* t, ExValue* value)
{
	float tempo = t->getSynchronizer()->getInTempo();

	// assume its ok to truncate this one, if you want something
	// more accurate could have a RealTempoVariable?
	value->setLong((long)tempo);
}

PUBLIC SyncInTempoVariableType* SyncInTempoVariable = 
new SyncInTempoVariableType();

//////////////////////////////////////////////////////////////////////
//
// syncInRawBeat
//
// The current beat count derived from the external MIDI clock.
//
//////////////////////////////////////////////////////////////////////

class SyncInRawBeatVariableType : public ScriptInternalVariable {
  public:
    SyncInRawBeatVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

SyncInRawBeatVariableType::SyncInRawBeatVariableType()
{
    setName("syncInRawBeat");
}

void SyncInRawBeatVariableType::getTrackValue(Track* t, ExValue* value)
{
	Synchronizer* s = t->getSynchronizer();
    value->setInt(s->getInRawBeat());
}

PUBLIC SyncInRawBeatVariableType* SyncInRawBeatVariable = 
new SyncInRawBeatVariableType();

//////////////////////////////////////////////////////////////////////
//
// syncInBeat
//
// The current beat count derived from the external MIDI clock,
// relative to the bar.
//
//////////////////////////////////////////////////////////////////////

class SyncInBeatVariableType : public ScriptInternalVariable {
  public:
    SyncInBeatVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

SyncInBeatVariableType::SyncInBeatVariableType()
{
    setName("syncInBeat");
}

void SyncInBeatVariableType::getTrackValue(Track* t, ExValue* value)
{
	Synchronizer* s = t->getSynchronizer();
    value->setInt(s->getInBeat());
}

PUBLIC SyncInBeatVariableType* SyncInBeatVariable = 
new SyncInBeatVariableType();

//////////////////////////////////////////////////////////////////////
//
// syncInBar
//
// The current bar count derived from the external MIDI clock.
//
//////////////////////////////////////////////////////////////////////

class SyncInBarVariableType : public ScriptInternalVariable {
  public:
    SyncInBarVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

SyncInBarVariableType::SyncInBarVariableType()
{
    setName("syncInBar");
}

void SyncInBarVariableType::getTrackValue(Track* t, ExValue* value)
{
	Synchronizer* s = t->getSynchronizer();
    value->setInt(s->getInBar());
}

PUBLIC SyncInBarVariableType* SyncInBarVariable = 
new SyncInBarVariableType();

//////////////////////////////////////////////////////////////////////
//
// syncInReceiving
//
// True if we are currently receiving MIDI clocks.
//
//////////////////////////////////////////////////////////////////////

class SyncInReceivingVariableType : public ScriptInternalVariable {
  public:
    SyncInReceivingVariableType();
	void getTrackValue(Track* t, ExValue* value);
};

SyncInReceivingVariableType::SyncInReceivingVariableType()
{
    setName("syncInReceiving");
}

void SyncInReceivingVariableType::getTrackValue(Track* t, ExValue* value)
{
	value->setBool(t->getSynchronizer()->isInReceiving());
}

PUBLIC SyncInReceivingVariableType* SyncInReceivingVariable = 
new SyncInReceivingVariableType();

//////////////////////////////////////////////////////////////////////
//
// syncInStarted
//
// True if we have received a MIDI start or continue message.
//
//////////////////////////////////////////////////////////////////////

class SyncInStartedVariableType : public ScriptInternalVariable {
  public:
    SyncInStartedVariableType();
	void getTrackValue(Track* t, ExValue* value);
};

SyncInStartedVariableType::SyncInStartedVariableType()
{
    setName("syncInStarted");
}

void SyncInStartedVariableType::getTrackValue(Track* t, ExValue* value)
{
	value->setBool(t->getSynchronizer()->isInStarted());
}

PUBLIC SyncInStartedVariableType* SyncInStartedVariable = 
new SyncInStartedVariableType();

/****************************************************************************
 *                                                                          *
 *   							  HOST SYNC                                 *
 *                                                                          *
 ****************************************************************************/

//////////////////////////////////////////////////////////////////////
//
// syncHostTempo
//
// The tempo advertised by the plugin host.
//
//////////////////////////////////////////////////////////////////////

class SyncHostTempoVariableType : public ScriptInternalVariable {
  public:
    SyncHostTempoVariableType();
	void getTrackValue(Track* t, ExValue* value);
};

SyncHostTempoVariableType::SyncHostTempoVariableType()
{
    setName("syncHostTempo");
}

void SyncHostTempoVariableType::getTrackValue(Track* t, ExValue* value)
{
	float tempo = t->getSynchronizer()->getHostTempo();

	// assume its ok to truncate this one, if you want something
	// more accurate could have a RealTempoVariable?
	value->setLong((long)tempo);
}

PUBLIC SyncHostTempoVariableType* SyncHostTempoVariable = 
new SyncHostTempoVariableType();

//////////////////////////////////////////////////////////////////////
//
// syncHostRawBeat
//
// The current beat count given by the host.
//
//////////////////////////////////////////////////////////////////////

class SyncHostRawBeatVariableType : public ScriptInternalVariable {
  public:
    SyncHostRawBeatVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

SyncHostRawBeatVariableType::SyncHostRawBeatVariableType()
{
    setName("syncHostRawBeat");
}

void SyncHostRawBeatVariableType::getTrackValue(Track* t, ExValue* value)
{
	Synchronizer* s = t->getSynchronizer();
    value->setInt(s->getHostRawBeat());
}

PUBLIC SyncHostRawBeatVariableType* SyncHostRawBeatVariable = 
new SyncHostRawBeatVariableType();

//////////////////////////////////////////////////////////////////////
//
// syncHostBeat
//
// The current beat count given by the host, relative to the bar.
//
//////////////////////////////////////////////////////////////////////

class SyncHostBeatVariableType : public ScriptInternalVariable {
  public:
    SyncHostBeatVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

SyncHostBeatVariableType::SyncHostBeatVariableType()
{
    setName("syncHostBeat");
}

void SyncHostBeatVariableType::getTrackValue(Track* t, ExValue* value)
{
	Synchronizer* s = t->getSynchronizer();
    value->setInt(s->getHostBeat());
}

PUBLIC SyncHostBeatVariableType* SyncHostBeatVariable = 
new SyncHostBeatVariableType();

//////////////////////////////////////////////////////////////////////
//
// syncHostBar
//
// The current bar count given by the host.
//
//////////////////////////////////////////////////////////////////////

class SyncHostBarVariableType : public ScriptInternalVariable {
  public:
    SyncHostBarVariableType();
    void getTrackValue(Track* t, ExValue* value);
};

SyncHostBarVariableType::SyncHostBarVariableType()
{
    setName("syncHostBar");
}

void SyncHostBarVariableType::getTrackValue(Track* t, ExValue* value)
{
	Synchronizer* s = t->getSynchronizer();
    value->setInt(s->getHostBar());
}

PUBLIC SyncHostBarVariableType* SyncHostBarVariable = 
new SyncHostBarVariableType();

//////////////////////////////////////////////////////////////////////
//
// syncHostReceiving
//
// True if we are currently receiving sync events from the host.
// Note that this means VST beat/bar events, not MIDI clocks routed
// into the plugin.
// 
// Currently this is unreliable and unused.
//
//////////////////////////////////////////////////////////////////////

class SyncHostReceivingVariableType : public ScriptInternalVariable {
  public:
    SyncHostReceivingVariableType();
	void getTrackValue(Track* t, ExValue* value);
};

SyncHostReceivingVariableType::SyncHostReceivingVariableType()
{
    setName("syncHostReceiving");
}

void SyncHostReceivingVariableType::getTrackValue(Track* t, ExValue* value)
{
	value->setBool(t->getSynchronizer()->isHostReceiving());
}

PUBLIC SyncHostReceivingVariableType* SyncHostReceivingVariable = 
new SyncHostReceivingVariableType();

/****************************************************************************
 *                                                                          *
 *                                INSTALLATION                              *
 *                                                                          *
 ****************************************************************************/

//////////////////////////////////////////////////////////////////////
//
// installationDirectory
//
// Base directory where Mobius has been installed.
// Typically c:\Program Files\Mobius on Windows and
// /Applications/Mobius on Mac.
//
//////////////////////////////////////////////////////////////////////

class InstallationDirectoryVariableType : public ScriptInternalVariable {
  public:
    InstallationDirectoryVariableType();
    void getValue(ScriptInterpreter* si, ExValue* value);
};

InstallationDirectoryVariableType::InstallationDirectoryVariableType()
{
    setName("installationDirectory");
}

void InstallationDirectoryVariableType::getValue(ScriptInterpreter* si, 
                                                 ExValue* value)
{
    Mobius* m = si->getMobius();
    MobiusContext* mc = m->getContext();
    value->setString(mc->getInstallationDirectory());
}

PUBLIC InstallationDirectoryVariableType* InstallationDirectoryVariable = 
new InstallationDirectoryVariableType();

//////////////////////////////////////////////////////////////////////
//
// configurationDirectory
//
// Base directory where Mobius has been installed.
// Typically c:\Program Files\Mobius on Windows and
// /Applications/Mobius on Mac.
//
//////////////////////////////////////////////////////////////////////

class ConfigurationDirectoryVariableType : public ScriptInternalVariable {
  public:
    ConfigurationDirectoryVariableType();
    void getValue(ScriptInterpreter* si, ExValue* value);
};

ConfigurationDirectoryVariableType::ConfigurationDirectoryVariableType()
{
    setName("configurationDirectory");
}

void ConfigurationDirectoryVariableType::getValue(ScriptInterpreter* si, 
                                                 ExValue* value)
{
    Mobius* m = si->getMobius();
    MobiusContext* mc = m->getContext();
    value->setString(mc->getConfigurationDirectory());
}

PUBLIC ConfigurationDirectoryVariableType* ConfigurationDirectoryVariable = 
new ConfigurationDirectoryVariableType();

/****************************************************************************
 *                                                                          *
 *   							 COLLECTIONS                                *
 *                                                                          *
 ****************************************************************************/

/**
 * The collection of all internal variables.
 */
PRIVATE ScriptInternalVariable* InternalVariables[] = {

	// Script state

	SustainCountVariable,
	ClickCountVariable,
	TriggerNumberVariable,
	TriggerValueVariable,
	TriggerOffsetVariable,
	MidiTypeVariable,
	MidiChannelVariable,
	MidiNumberVariable,
	MidiValueVariable,
	ReturnCodeVariable,

	// Special runtime parameters
	NoExternalAudioVariable,

	// Internal State

    BlockFramesVariable,
    SampleFramesVariable,
	
	// Loop sizes
	
	LoopCountVariable,
	LoopNumberVariable,
	LoopFramesVariable,
	LoopFrameVariable,

	CycleCountVariable,
	CycleNumberVariable,
	CycleFramesVariable,
	CycleFrameVariable,

	SubCycleCountVariable,
	SubCycleNumberVariable,
	SubCycleFramesVariable,
	SubCycleFrameVariable,

	LayerCountVariable,
	RedoCountVariable,
	EffectiveFeedbackVariable,
    HistoryFramesVariable,

	// Loop events

	NextEventVariable,
	NextEventFunctionVariable,
	NextLoopVariable,
    EventSummaryVariable,

	// Loop modes

	ModeVariable,
    IsRecordingVariable,
	InOverdubVariable,
	InHalfspeedVariable,
	InReverseVariable,
	InMuteVariable,
	InPauseVariable,
	InRealignVariable,
	InReturnVariable,
	RateVariable,
	RawSpeedVariable,
	RawPitchVariable,
	SpeedToggleVariable,
    SpeedSequenceIndexVariable,
    PitchSequenceIndexVariable,
    WindowOffsetVariable,

	// Track state

	TrackCountVariable,
	TrackVariable,
	GlobalMuteVariable,
	SoloVariable,
	TrackSyncMasterVariable,
	OutSyncMasterVariable,

	// Generic Sync

	SyncAudioFrameVariable,
    SyncBarVariable,
    SyncBeatVariable,
	SyncCorrectionsVariable,
	SyncCyclePulsesVariable,
	SyncDealignVariable,
	SyncDriftVariable,
	SyncDriftChecksVariable,
    SyncLoopFramesVariable,
	SyncPreRealignFrameVariable,
	SyncPulseVariable,
	SyncPulseFramesVariable,
    SyncPulsesVariable,
    SyncRawBeatVariable,
    SyncTempoVariable,

	// Out Sync

	SyncOutTempoVariable,
	SyncOutRawBeatVariable,
	SyncOutBeatVariable,
	SyncOutBarVariable,
	SyncOutSendingVariable,
	SyncOutStartedVariable,
	SyncOutStartsVariable,

	// MIDI Sync

	SyncInTempoVariable,
	SyncInRawBeatVariable,
	SyncInBeatVariable,
	SyncInBarVariable,
	SyncInReceivingVariable,
	SyncInStartedVariable,

	// Host sync

	SyncHostTempoVariable,
	SyncHostRawBeatVariable,
	SyncHostBeatVariable,
	SyncHostBarVariable,
	SyncHostReceivingVariable,

    // Installation

    InstallationDirectoryVariable,
    ConfigurationDirectoryVariable,

    NULL
};

/**
 * Lookup an internal variable during script parsing.
 */
PUBLIC ScriptInternalVariable* 
ScriptInternalVariable::getVariable(const char* name)
{
    ScriptInternalVariable* found = NULL;
    for (int i = 0 ; InternalVariables[i] != NULL ; i++) {
		ScriptInternalVariable* v = InternalVariables[i];
		if (v->isMatch(name)) {
			found = v;
			break;
		}
	}
    return found;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
