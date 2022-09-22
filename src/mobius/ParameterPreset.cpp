/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Static object definitions for Preset parameters.
 * These get and set the fields of a Preset object.
 * getObjectValue/setObjectValue are used when parsing or serializing XML
 * and when editing presets in the UI.
 *
 * getValue/setValue are used to process bindings.
 *
 * When we set preset parameters, we are setting them in a private
 * copy of the Preset maintained by each track, these values will 
 * be reset on a GlobalReset.
 * 
 */

#include <stdio.h>
#include <string.h>
#include <memory.h>
#include <ctype.h>

#include "Util.h"
#include "List.h"
#include "MessageCatalog.h"
#include "XmlModel.h"
#include "XmlBuffer.h"

#include "Action.h"
#include "Audio.h"
#include "Export.h"
#include "Function.h"
#include "Messages.h"
#include "Mobius.h"
#include "MobiusConfig.h"
#include "Mode.h"
#include "Project.h"
#include "Recorder.h"
#include "Resampler.h"
#include "Setup.h"
#include "Track.h"
#include "Script.h"
#include "Synchronizer.h"

#include "Parameter.h"

/****************************************************************************
 *                                                                          *
 *   						  PRESET PARAMETER                              *
 *                                                                          *
 ****************************************************************************/

class PresetParameter : public Parameter 
{
  public:

    PresetParameter(const char* name, int key) :
        Parameter(name, key) {
        scope = PARAM_SCOPE_PRESET;
    }

    /**
     * Overload the Parameter versions and cast to a Preset.
     */
	void getObjectValue(void* object, ExValue* value);
	void setObjectValue(void* object, ExValue* value);

    /**
     * Overload the Parameter versions and resolve to a Preset.
     */
	void getValue(Export* exp, ExValue* value);
	void setValue(Action* action);

    /**
     * Overload the Parameter versions and resolve to a Preset.
     */
	int getOrdinalValue(Export* exp);

    /**
     * These must always be overloaded.
     */
	virtual void getValue(Preset* p, ExValue* value) = 0;
	virtual void setValue(Preset* p, ExValue* value) = 0;

    /**
     * This must be overloaded by anything that supports ordinals.
     */
	virtual int getOrdinalValue(Preset* p);


};

void PresetParameter::setObjectValue(void* object, ExValue* value)
{
    setValue((Preset*)object, value);
}

void PresetParameter::getObjectValue(void* object, ExValue* value)
{
    getValue((Preset*)object, value);
}

void PresetParameter::getValue(Export* exp, ExValue* value)
{
    Track* track = exp->getTrack();
    if (track != NULL)
	  getValue(track->getPreset(), value);
    else {
        Trace(1, "PresetParameter:getValue track not resolved!\n");
        value->setNull();
    }
}

int PresetParameter::getOrdinalValue(Export* exp)
{
    int value = -1;

    Track* track = exp->getTrack();
    if (track != NULL)
      value = getOrdinalValue(track->getPreset());
    else 
      Trace(1, "PresetParameter:getOrdinalValue track not resolved!\n");

    return value;
}

/**
 * This one we can't have a default implementation for, it must be overloaded.
 */
int PresetParameter::getOrdinalValue(Preset* p)
{
    Trace(1, "Parameter %s: getOrdinalValue(Preset) not overloaded!\n",
          getName());
    return -1;
}

void PresetParameter::setValue(Action* action)
{
    Track* track = action->getResolvedTrack();
    if (track != NULL)
      setValue(track->getPreset(), &(action->arg));
    else
      Trace(1, "PresetParameter:setValue track not resolved!\n");
}

//////////////////////////////////////////////////////////////////////
//
// SubCycle
//
//////////////////////////////////////////////////////////////////////

class SubCycleParameterType : public PresetParameter 
{
  public:
	SubCycleParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

SubCycleParameterType::SubCycleParameterType() :
    PresetParameter("subcycles", MSG_PARAM_SUBCYCLES)
{
    bindable = true;
	type = TYPE_INT;
	low = 1;
	// originally 1024 but I can't imagine needing it that
	// big and this doesn't map into a host parameter well
	high = 128;

    addAlias("8thsPerCycle");
}

int SubCycleParameterType::getOrdinalValue(Preset* p)
{
	return p->getSubcycles();
}

void SubCycleParameterType::getValue(Preset* p, ExValue* value)
{
	value->setInt(p->getSubcycles());
}

void SubCycleParameterType::setValue(Preset* p, ExValue* value)
{
	p->setSubcycles(value->getInt());
}

PUBLIC Parameter* SubCycleParameter = new SubCycleParameterType();

//////////////////////////////////////////////////////////////////////
//
// MultiplyMode
//
//////////////////////////////////////////////////////////////////////

class MultiplyModeParameterType : public PresetParameter
{
  public:
	MultiplyModeParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

const char* MULTIPLY_MODE_NAMES[] = {
	"normal", "simple", NULL
};

int MULTIPLY_MODE_KEYS[] = {
	MSG_VALUE_MULTIPLY_NORMAL, MSG_VALUE_MULTIPLY_SIMPLE, 
	0
};

MultiplyModeParameterType::MultiplyModeParameterType() :
    PresetParameter("multiplyMode", MSG_PARAM_MULTIPLY_MODE)
{
    bindable = true;
	type = TYPE_ENUM;
	values = MULTIPLY_MODE_NAMES;
	valueKeys = MULTIPLY_MODE_KEYS;
}

int MultiplyModeParameterType::getOrdinalValue(Preset* p)
{
    return (int)p->getMultiplyMode();
}

void MultiplyModeParameterType::getValue(Preset* p, ExValue* value)
{
	value->setString(values[p->getMultiplyMode()]);
}

/**
 * Formerly "traditional" was our old broken way and "new" was
 * the fixed way.  "normal" is now "new", "traditional" no longer
 * exists.  "simple" was formerly known as "overdub".
 */
void MultiplyModeParameterType::setValue(Preset* p, ExValue* value)
{
    // auto-upgrade, but don't trash the type if this is an ordinal!
    if (value->getType() == EX_STRING) {
        const char* str = value->getString();
        if (StringEqualNoCase(str, "traditional") || StringEqualNoCase(str, "new"))
          value->setString("normal");

        else if (StringEqualNoCase(str, "overdub"))
          value->setString("simple");
    }

	p->setMultiplyMode((Preset::MultiplyMode)getEnum(value));
}

PUBLIC Parameter* MultiplyModeParameter = new MultiplyModeParameterType();

//////////////////////////////////////////////////////////////////////
//
// ShuffleMode
//
//////////////////////////////////////////////////////////////////////

class ShuffleModeParameterType : public PresetParameter
{
  public:
	ShuffleModeParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

const char* SHUFFLE_MODE_NAMES[] = {
	"reverse", "shift", "swap", "random", NULL
};

int SHUFFLE_MODE_KEYS[] = {
	MSG_VALUE_SHUFFLE_REVERSE, 
	MSG_VALUE_SHUFFLE_SHIFT,
	MSG_VALUE_SHUFFLE_SWAP,
	MSG_VALUE_SHUFFLE_RANDOM,
	0
};

ShuffleModeParameterType::ShuffleModeParameterType() :
    PresetParameter("shuffleMode", MSG_PARAM_SHUFFLE_MODE)
{
    bindable = true;
	type = TYPE_ENUM;
	values = SHUFFLE_MODE_NAMES;
	valueKeys = SHUFFLE_MODE_KEYS;
}

int ShuffleModeParameterType::getOrdinalValue(Preset* p)
{
	return p->getShuffleMode();
}

void ShuffleModeParameterType::getValue(Preset* p, ExValue* value)
{
	value->setString(values[p->getShuffleMode()]);
}

void ShuffleModeParameterType::setValue(Preset* p, ExValue* value)
{
	p->setShuffleMode((Preset::ShuffleMode)getEnum(value));
}

PUBLIC Parameter* ShuffleModeParameter = new ShuffleModeParameterType();

//////////////////////////////////////////////////////////////////////
//
// AltFeedbackEnable
//
//////////////////////////////////////////////////////////////////////

class AltFeedbackEnableParameterType : public PresetParameter
{
  public:
	AltFeedbackEnableParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

AltFeedbackEnableParameterType::AltFeedbackEnableParameterType() :
    PresetParameter("altFeedbackEnable", MSG_PARAM_ALT_FEEDBACK_ENABLE)
{
    bindable = true;
	type = TYPE_BOOLEAN;
}

int AltFeedbackEnableParameterType::getOrdinalValue(Preset* p)
{
	return (int)p->isAltFeedbackEnable();
}

void AltFeedbackEnableParameterType::getValue(Preset* p, ExValue* value)
{
	value->setBool(p->isAltFeedbackEnable());
}

void AltFeedbackEnableParameterType::setValue(Preset* p, ExValue* value)
{
	p->setAltFeedbackEnable(value->getBool());
}

PUBLIC Parameter* AltFeedbackEnableParameter = new AltFeedbackEnableParameterType();

//////////////////////////////////////////////////////////////////////
//
// EmptyLoopAction
//
//////////////////////////////////////////////////////////////////////

class EmptyLoopActionParameterType : public PresetParameter
{
  public:
	EmptyLoopActionParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

const char* EMPTY_LOOP_NAMES[] = {
	"none", "record", "copy", "copyTime", NULL
};

int EMPTY_LOOP_KEYS[] = {
	MSG_VALUE_EMPTY_LOOP_NONE,
	MSG_VALUE_EMPTY_LOOP_RECORD,
	MSG_VALUE_EMPTY_LOOP_COPY,
	MSG_VALUE_EMPTY_LOOP_TIME,
	0
};

EmptyLoopActionParameterType::EmptyLoopActionParameterType() :
    PresetParameter("emptyLoopAction", MSG_PARAM_EMPTY_LOOP_ACTION)
{
    bindable = true;
	type = TYPE_ENUM;
	values = EMPTY_LOOP_NAMES;
	valueKeys = EMPTY_LOOP_KEYS;
}

int EmptyLoopActionParameterType::getOrdinalValue(Preset* p)
{
	return (int)p->getEmptyLoopAction();
}

void EmptyLoopActionParameterType::getValue(Preset* p, ExValue* value)
{
	value->setString(values[(int)p->getEmptyLoopAction()]);
}

void EmptyLoopActionParameterType::setValue(Preset* p, ExValue* value)
{
    // catch a common misspelling
    if (value->getType() == EX_STRING && 
        StringEqualNoCase(value->getString(), "copyTiming"))
      p->setEmptyLoopAction(Preset::EMPTY_LOOP_TIMING);

    // support for an old value
    else if (value->getType() == EX_STRING &&
             StringEqualNoCase(value->getString(), "copySound"))
      p->setEmptyLoopAction(Preset::EMPTY_LOOP_COPY);
      
    else
      p->setEmptyLoopAction((Preset::EmptyLoopAction)getEnum(value));
}

PUBLIC Parameter* EmptyLoopActionParameter = new EmptyLoopActionParameterType();

//////////////////////////////////////////////////////////////////////
//
// EmptyTrackAction
//
//////////////////////////////////////////////////////////////////////

class EmptyTrackActionParameterType : public PresetParameter
{
  public:
	EmptyTrackActionParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

EmptyTrackActionParameterType::EmptyTrackActionParameterType() :
    PresetParameter("emptyTrackAction", MSG_PARAM_EMPTY_TRACK_ACTION)
{
    bindable = true;
	type = TYPE_ENUM;
	values = EMPTY_LOOP_NAMES;
	valueKeys = EMPTY_LOOP_KEYS;
}

int EmptyTrackActionParameterType::getOrdinalValue(Preset* p)
{
	return (int)p->getEmptyTrackAction();
}

void EmptyTrackActionParameterType::getValue(Preset* p, ExValue* value)
{
	value->setString(values[(int)p->getEmptyTrackAction()]);
}

void EmptyTrackActionParameterType::setValue(Preset* p, ExValue* value)
{
	p->setEmptyTrackAction((Preset::EmptyLoopAction)getEnum(value));
}

PUBLIC Parameter* EmptyTrackActionParameter = new EmptyTrackActionParameterType();

//////////////////////////////////////////////////////////////////////
//
// TrackLeaveAction
//
//////////////////////////////////////////////////////////////////////

class TrackLeaveActionParameterType : public PresetParameter
{
  public:
	TrackLeaveActionParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

const char* TRACK_LEAVE_NAMES[] = {
	"none", "cancel", "wait", NULL
};

int TRACK_LEAVE_KEYS[] = {
	MSG_VALUE_TRACK_LEAVE_NONE,
	MSG_VALUE_TRACK_LEAVE_CANCEL,
	MSG_VALUE_TRACK_LEAVE_WAIT,
	0
};

TrackLeaveActionParameterType::TrackLeaveActionParameterType() :
    PresetParameter("trackLeaveAction", MSG_PARAM_TRACK_LEAVE_ACTION)
{
    bindable = true;
	type = TYPE_ENUM;
	values = TRACK_LEAVE_NAMES;
	valueKeys = TRACK_LEAVE_KEYS;
}

int TrackLeaveActionParameterType::getOrdinalValue(Preset* p)
{
	return (int)p->getTrackLeaveAction();
}

void TrackLeaveActionParameterType::getValue(Preset* p, ExValue* value)
{
	value->setString(values[(int)p->getTrackLeaveAction()]);
}

void TrackLeaveActionParameterType::setValue(Preset* p, ExValue* value)
{
	p->setTrackLeaveAction((Preset::TrackLeaveAction)getEnum(value));
}

PUBLIC Parameter* TrackLeaveActionParameter = new TrackLeaveActionParameterType();

//////////////////////////////////////////////////////////////////////
//
// LoopCount
//
//////////////////////////////////////////////////////////////////////

class LoopCountParameterType : public PresetParameter
{
  public:
	LoopCountParameterType();
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

LoopCountParameterType::LoopCountParameterType() :
    PresetParameter("loopCount", MSG_PARAM_LOOP_COUNT)
{
    // not bindable
	type = TYPE_INT;
    low = 1;
	high = 32;
    addAlias("moreLoops");
}

void LoopCountParameterType::getValue(Preset* p, ExValue* value)
{
	value->setInt(p->getLoops());
}

/**
 * NOTE: Setting this from a script will not have any effect since
 * Track does not watch for changes to this parameter.  We need to
 * intercept this at a higher level, probably in setValue where
 * it has the Action and inform the Track after we change the 
 * Preset.
 * 
 * Still, I'm not sure I like having the loop count changing willy nilly.
 * only allow it to be changed from the prest?
 */
void LoopCountParameterType::setValue(Preset* p, ExValue* value)
{
	// this will be constrained between 1 and 16
	p->setLoops(value->getInt());
}

PUBLIC Parameter* LoopCountParameter = new LoopCountParameterType();

//////////////////////////////////////////////////////////////////////
//
// MuteMode
//
//////////////////////////////////////////////////////////////////////

class MuteModeParameterType : public PresetParameter
{
  public:
	MuteModeParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

const char* MUTE_MODE_NAMES[] = {
	"continue", "start", "pause", NULL
};

int MUTE_MODE_KEYS[] = {
	MSG_VALUE_MUTE_CONTINUE,
	MSG_VALUE_MUTE_START,
	MSG_VALUE_MUTE_PAUSE,
	0
};

MuteModeParameterType::MuteModeParameterType() :
    PresetParameter("muteMode", MSG_PARAM_MUTE_MODE)
{
    bindable = true;
	type = TYPE_ENUM;
	values = MUTE_MODE_NAMES;
	valueKeys = MUTE_MODE_KEYS;
}

int MuteModeParameterType::getOrdinalValue(Preset* p)
{
	return (int)p->getMuteMode();
}

void MuteModeParameterType::getValue(Preset* p, ExValue* value)
{
	value->setString(values[(int)p->getMuteMode()]);
}

void MuteModeParameterType::setValue(Preset* p, ExValue* value)
{
    // auto-upgrade, but don't trash the type if this is an ordinal!
    if (value->getType() == EX_STRING && 
        StringEqualNoCase(value->getString(), "continuous"))
      value->setString("continue");

	p->setMuteMode((Preset::MuteMode)getEnum(value));
}

PUBLIC Parameter* MuteModeParameter = new MuteModeParameterType();

//////////////////////////////////////////////////////////////////////
//
// MuteCancel
//
//////////////////////////////////////////////////////////////////////

class MuteCancelParameterType : public PresetParameter
{
  public:
	MuteCancelParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

const char* MUTE_CANCEL_NAMES[] = {
	"never", "edit", "trigger", "effect", "custom", "always", NULL
};

int MUTE_CANCEL_KEYS[] = {
	MSG_VALUE_MUTE_CANCEL_NEVER,
	MSG_VALUE_MUTE_CANCEL_EDIT,
	MSG_VALUE_MUTE_CANCEL_TRIGGER,
	MSG_VALUE_MUTE_CANCEL_EFFECT,
	MSG_VALUE_MUTE_CANCEL_CUSTOM,
	MSG_VALUE_MUTE_CANCEL_ALWAYS,
	0
};

MuteCancelParameterType::MuteCancelParameterType() :
    PresetParameter("muteCancel", MSG_PARAM_MUTE_CANCEL)
{
    bindable = true;
	type = TYPE_ENUM;
	values = MUTE_CANCEL_NAMES;
	valueKeys = MUTE_CANCEL_KEYS;
}

int MuteCancelParameterType::getOrdinalValue(Preset* p)
{
	return (int)p->getMuteCancel();
}

void MuteCancelParameterType::getValue(Preset* p, ExValue* value)
{
	value->setString(values[(int)p->getMuteCancel()]);
}

void MuteCancelParameterType::setValue(Preset* p, ExValue* value)
{
    // fixed a spelling error in 2.0
    if (value->getType() == EX_STRING &&
        StringEqualNoCase(value->getString(), "allways"))
      value->setString("always");

	p->setMuteCancel((Preset::MuteCancel)getEnum(value));
}

PUBLIC Parameter* MuteCancelParameter = new MuteCancelParameterType();

//////////////////////////////////////////////////////////////////////
//
// OverdubQuantized
//
//////////////////////////////////////////////////////////////////////

class OverdubQuantizedParameterType : public PresetParameter
{
  public:
	OverdubQuantizedParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

OverdubQuantizedParameterType::OverdubQuantizedParameterType() :
    PresetParameter("overdubQuantized", MSG_PARAM_OVERDUB_QUANTIZED)
{
    bindable = true;
	type = TYPE_BOOLEAN;
	// common spelling error
    addAlias("overdubQuantize");
}

int OverdubQuantizedParameterType::getOrdinalValue(Preset* p)
{
	return (int)p->isOverdubQuantized();
}

void OverdubQuantizedParameterType::getValue(Preset* p, ExValue* value)
{
	value->setBool(p->isOverdubQuantized());
}

void OverdubQuantizedParameterType::setValue(Preset* p, ExValue* value)
{
	p->setOverdubQuantized(value->getBool());
}

PUBLIC Parameter* OverdubQuantizedParameter = new OverdubQuantizedParameterType();

//////////////////////////////////////////////////////////////////////
//
// Quantize
//
//////////////////////////////////////////////////////////////////////

class QuantizeParameterType : public PresetParameter
{
  public:
	QuantizeParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

const char* QUANTIZE_MODE_NAMES[] = {
	"off", "subCycle", "cycle", "loop", NULL
};

int QUANTIZE_MODE_KEYS[] = {
	MSG_VALUE_QUANTIZE_OFF,
	MSG_VALUE_QUANTIZE_SUBCYCLE,
	MSG_VALUE_QUANTIZE_CYCLE,
	MSG_VALUE_QUANTIZE_LOOP,
	0
};

QuantizeParameterType::QuantizeParameterType() :
    PresetParameter("quantize", MSG_PARAM_QUANTIZE_MODE)
{
    bindable = true;
	type = TYPE_ENUM;
	values = QUANTIZE_MODE_NAMES;
	valueKeys = QUANTIZE_MODE_KEYS;
}

int QuantizeParameterType::getOrdinalValue(Preset* p)
{
	return (int)p->getQuantize();
}

void QuantizeParameterType::getValue(Preset* p, ExValue* value)
{
	value->setString(values[(int)p->getQuantize()]);
}

void QuantizeParameterType::setValue(Preset* p, ExValue* value)
{
	p->setQuantize((Preset::QuantizeMode)getEnum(value));
}

PUBLIC Parameter* QuantizeParameter = new QuantizeParameterType();

//////////////////////////////////////////////////////////////////////
//
// BounceQuantize
//
//////////////////////////////////////////////////////////////////////

class BounceQuantizeParameterType : public PresetParameter
{
  public:
	BounceQuantizeParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

BounceQuantizeParameterType::BounceQuantizeParameterType() :
    PresetParameter("bounceQuantize", MSG_PARAM_BOUNCE_QUANTIZE)
{
    bindable = true;
	type = TYPE_ENUM;
	values = QUANTIZE_MODE_NAMES;
	valueKeys = QUANTIZE_MODE_KEYS;
}

int BounceQuantizeParameterType::getOrdinalValue(Preset* p)
{
	return (int)p->getBounceQuantize();
}

void BounceQuantizeParameterType::getValue(Preset* p, ExValue* value)
{
	value->setString(values[(int)p->getBounceQuantize()]);
}

void BounceQuantizeParameterType::setValue(Preset* p, ExValue* value)
{
	p->setBounceQuantize((Preset::QuantizeMode)getEnum(value));
}

PUBLIC Parameter* BounceQuantizeParameter = new BounceQuantizeParameterType();

//////////////////////////////////////////////////////////////////////
//
// RecordResetsFeedback
//
//////////////////////////////////////////////////////////////////////

class RecordResetsFeedbackParameterType : public PresetParameter
{
  public:
	RecordResetsFeedbackParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

RecordResetsFeedbackParameterType::RecordResetsFeedbackParameterType() :
    PresetParameter("recordResetsFeedback", MSG_PARAM_RECORD_FEEDBACK)
{
    bindable = true;
	type = TYPE_BOOLEAN;
}

int RecordResetsFeedbackParameterType::getOrdinalValue(Preset* p)
{
	return (int)p->isRecordResetsFeedback();
}

void RecordResetsFeedbackParameterType::getValue(Preset* p, ExValue* value)
{
	value->setBool(p->isRecordResetsFeedback());
}

void RecordResetsFeedbackParameterType::setValue(Preset* p, ExValue* value)
{
	p->setRecordResetsFeedback(value->getBool());
}

PUBLIC Parameter* RecordResetsFeedbackParameter = new RecordResetsFeedbackParameterType();

//////////////////////////////////////////////////////////////////////
//
// SpeedRecord
//
//////////////////////////////////////////////////////////////////////

class SpeedRecordParameterType : public PresetParameter
{
  public:
	SpeedRecordParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

SpeedRecordParameterType::SpeedRecordParameterType() :
    PresetParameter("speedRecord", MSG_PARAM_SPEED_RECORD)
{
    bindable = true;
	type = TYPE_BOOLEAN;
    addAlias("rateRecord");
}

int SpeedRecordParameterType::getOrdinalValue(Preset* p)
{
	return (int)p->isSpeedRecord();
}

void SpeedRecordParameterType::getValue(Preset* p, ExValue* value)
{
	value->setBool(p->isSpeedRecord());
}

void SpeedRecordParameterType::setValue(Preset* p, ExValue* value)
{
	p->setSpeedRecord(value->getBool());
}

PUBLIC Parameter* SpeedRecordParameter = new SpeedRecordParameterType();

//////////////////////////////////////////////////////////////////////
//
// RoundingOverdub
//
//////////////////////////////////////////////////////////////////////

class RoundingOverdubParameterType : public PresetParameter
{
  public:
	RoundingOverdubParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

RoundingOverdubParameterType::RoundingOverdubParameterType() :
    PresetParameter("roundingOverdub", MSG_PARAM_ROUND_MODE)
{
    bindable = true;
	type = TYPE_BOOLEAN;
    // this is what we had prior to 1.43
	addAlias("roundMode");
    // this lived briefly during 1.43
    addAlias("overdubDuringRounding");
}

int RoundingOverdubParameterType::getOrdinalValue(Preset* p)
{
	return (int)p->isRoundingOverdub();
}

void RoundingOverdubParameterType::getValue(Preset* p, ExValue* value)
{
	value->setBool(p->isRoundingOverdub());
}

void RoundingOverdubParameterType::setValue(Preset* p, ExValue* value)
{
	p->setRoundingOverdub(value->getBool());
}

PUBLIC Parameter* RoundingOverdubParameter = new RoundingOverdubParameterType();

//////////////////////////////////////////////////////////////////////
//
// SwitchLocation
//
//////////////////////////////////////////////////////////////////////

class SwitchLocationParameterType : public PresetParameter
{
  public:
	SwitchLocationParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

const char* SWITCH_LOCATION_NAMES[] = {
	"follow", "restore", "start", "random", NULL
};

int SWITCH_LOCATION_KEYS[] = {
	MSG_VALUE_SWITCH_FOLLOW,
	MSG_VALUE_SWITCH_RESTORE,
	MSG_VALUE_SWITCH_START,
	MSG_VALUE_SWITCH_RANDOM,
	0
};

SwitchLocationParameterType::SwitchLocationParameterType() :
    PresetParameter("switchLocation", MSG_PARAM_SWITCH_LOCATION)
{
    bindable = true;
	type = TYPE_ENUM;
	values = SWITCH_LOCATION_NAMES;
	valueKeys = SWITCH_LOCATION_KEYS;
}

int SwitchLocationParameterType::getOrdinalValue(Preset* p)
{
	return (int)p->getSwitchLocation();
}

void SwitchLocationParameterType::getValue(Preset* p, ExValue* value)
{
	value->setString(values[(int)p->getSwitchLocation()]);
}

void SwitchLocationParameterType::setValue(Preset* p, ExValue* value)
{
	p->setSwitchLocation(getEnum(value));
}

PUBLIC Parameter* SwitchLocationParameter = new SwitchLocationParameterType();

//////////////////////////////////////////////////////////////////////
//
// ReturnLocation
//
//////////////////////////////////////////////////////////////////////

class ReturnLocationParameterType : public PresetParameter
{
  public:
	ReturnLocationParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

ReturnLocationParameterType::ReturnLocationParameterType() :
    PresetParameter("returnLocation", MSG_PARAM_RETURN_LOCATION)
{
    bindable = true;
	type = TYPE_ENUM;
	values = SWITCH_LOCATION_NAMES;
	valueKeys = SWITCH_LOCATION_KEYS;
}

int ReturnLocationParameterType::getOrdinalValue(Preset* p)
{
	return (int)p->getReturnLocation();
}

void ReturnLocationParameterType::getValue(Preset* p, ExValue* value)
{
	value->setString(values[(int)p->getReturnLocation()]);
}

void ReturnLocationParameterType::setValue(Preset* p, ExValue* value)
{
	p->setReturnLocation(getEnum(value));
}

PUBLIC Parameter* ReturnLocationParameter = new ReturnLocationParameterType();

//////////////////////////////////////////////////////////////////////
//
// SwitchDuration
//
//////////////////////////////////////////////////////////////////////

class SwitchDurationParameterType : public PresetParameter
{
  public:
	SwitchDurationParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

const char* SWITCH_DURATION_NAMES[] = {
	"permanent", "once", "onceReturn", "sustain", "sustainReturn", NULL
};

int SWITCH_DURATION_KEYS[] = {
	MSG_VALUE_SWITCH_PERMANENT,
	MSG_VALUE_SWITCH_ONCE,
	MSG_VALUE_SWITCH_ONCE_RETURN,
	MSG_VALUE_SWITCH_SUSTAIN,
	MSG_VALUE_SWITCH_SUSTAIN_RETURN,
	0
};

SwitchDurationParameterType::SwitchDurationParameterType() :
    PresetParameter("switchDuration", MSG_PARAM_SWITCH_DURATION)
{
    bindable = true;
	type = TYPE_ENUM;
	values = SWITCH_DURATION_NAMES;
	valueKeys = SWITCH_DURATION_KEYS;
}

int SwitchDurationParameterType::getOrdinalValue(Preset* p)
{
	return (int)p->getSwitchDuration();
}

void SwitchDurationParameterType::getValue(Preset* p, ExValue* value)
{
	value->setString(values[(int)p->getSwitchDuration()]);
}

void SwitchDurationParameterType::setValue(Preset* p, ExValue* value)
{
	p->setSwitchDuration(getEnum(value));
}

PUBLIC Parameter* SwitchDurationParameter = new SwitchDurationParameterType();

//////////////////////////////////////////////////////////////////////
//
// SwitchQuantize
//
//////////////////////////////////////////////////////////////////////

class SwitchQuantizeParameterType : public PresetParameter
{
  public:
	SwitchQuantizeParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

const char* SWITCH_QUANT_NAMES[] = {
	"off", "subCycle", "cycle", "loop", 
    "confirm", "confirmSubCycle", "confirmCycle", "confirmLoop", NULL
};

int SWITCH_QUANT_KEYS[] = {
	MSG_VALUE_SWITCH_OFF,
	MSG_VALUE_SWITCH_SUBCYCLE,
	MSG_VALUE_SWITCH_CYCLE,
	MSG_VALUE_SWITCH_LOOP,
	MSG_VALUE_SWITCH_CONFIRM,
	MSG_VALUE_SWITCH_CONFIRM_SUBCYCLE,
	MSG_VALUE_SWITCH_CONFIRM_CYCLE,
	MSG_VALUE_SWITCH_CONFIRM_LOOP,
	0
};

SwitchQuantizeParameterType::SwitchQuantizeParameterType() :
    PresetParameter("switchQuantize", MSG_PARAM_SWITCH_QUANTIZE)
{
    bindable = true;
	type = TYPE_ENUM;
	values = SWITCH_QUANT_NAMES;
	valueKeys = SWITCH_QUANT_KEYS;
    // old name
    addAlias("switchQuant");
}

int SwitchQuantizeParameterType::getOrdinalValue(Preset* p)
{
	return (int)p->getSwitchQuantize();
}

void SwitchQuantizeParameterType::getValue(Preset* p, ExValue* value)
{
	value->setString(values[(int)p->getSwitchQuantize()]);
}

void SwitchQuantizeParameterType::setValue(Preset* p, ExValue* value)
{
	p->setSwitchQuantize((Preset::SwitchQuantize)getEnum(value));
}

PUBLIC Parameter* SwitchQuantizeParameter = new SwitchQuantizeParameterType();

//////////////////////////////////////////////////////////////////////
//
// TimeCopy
//
//////////////////////////////////////////////////////////////////////

class TimeCopyParameterType : public PresetParameter
{
  public:
	TimeCopyParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

const char* COPY_MODE_NAMES[] = {
	"play", "overdub", "multiply", "insert", NULL
};

int COPY_MODE_KEYS[] = {
	MSG_VALUE_COPY_MODE_PLAY,
	MSG_VALUE_COPY_MODE_OVERDUB,
	MSG_VALUE_COPY_MODE_MULTIPLY,
	MSG_VALUE_COPY_MODE_INSERT,
	0
};

TimeCopyParameterType::TimeCopyParameterType() :
    PresetParameter("timeCopyMode", MSG_PARAM_TIME_COPY_MODE)
{
    bindable = true;
	type = TYPE_ENUM;
	values = COPY_MODE_NAMES;
	valueKeys = COPY_MODE_KEYS;
}

int TimeCopyParameterType::getOrdinalValue(Preset* p)
{
	return (int)p->getTimeCopyMode();
}

void TimeCopyParameterType::getValue(Preset* p, ExValue* value)
{
	value->setString(values[(int)p->getTimeCopyMode()]);
}

void TimeCopyParameterType::setValue(Preset* p, ExValue* value)
{
	p->setTimeCopyMode((Preset::CopyMode)getEnum(value));
}

PUBLIC Parameter* TimeCopyParameter = new TimeCopyParameterType();

//////////////////////////////////////////////////////////////////////
//
// SoundCopy
//
//////////////////////////////////////////////////////////////////////

class SoundCopyParameterType : public PresetParameter
{
  public:
	SoundCopyParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

SoundCopyParameterType::SoundCopyParameterType() :
    PresetParameter("soundCopyMode", MSG_PARAM_SOUND_COPY_MODE)
{
    bindable = true;
	type = TYPE_ENUM;
	values = COPY_MODE_NAMES;
	valueKeys = COPY_MODE_KEYS;
}

int SoundCopyParameterType::getOrdinalValue(Preset* p)
{
 	return (int)p->getSoundCopyMode();
}

void SoundCopyParameterType::getValue(Preset* p, ExValue* value)
{
 	value->setString(values[(int)p->getSoundCopyMode()]);
}

void SoundCopyParameterType::setValue(Preset* p, ExValue* value)
{
	p->setSoundCopyMode((Preset::CopyMode)getEnum(value));
}

PUBLIC Parameter* SoundCopyParameter = new SoundCopyParameterType();

//////////////////////////////////////////////////////////////////////
//
// RecordThreshold
//
//////////////////////////////////////////////////////////////////////

class RecordThresholdParameterType : public PresetParameter
{
  public:
	RecordThresholdParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

RecordThresholdParameterType::RecordThresholdParameterType() :
    PresetParameter("recordThreshold", MSG_PARAM_RECORD_THRESHOLD)
{
    bindable = true;
	type = TYPE_INT;
    low = 0;
    high = 8;
	// old name
    addAlias("threshold");
}

int RecordThresholdParameterType::getOrdinalValue(Preset* p)
{
	return p->getRecordThreshold();
}

void RecordThresholdParameterType::getValue(Preset* p, ExValue* value)
{
	value->setInt(p->getRecordThreshold());
}

void RecordThresholdParameterType::setValue(Preset* p, ExValue* value)
{
	p->setRecordThreshold(value->getInt());
}

PUBLIC Parameter* RecordThresholdParameter = new RecordThresholdParameterType();

//////////////////////////////////////////////////////////////////////
//
// SwitchVelocity
//
//////////////////////////////////////////////////////////////////////

class SwitchVelocityParameterType : public PresetParameter
{
  public:
	SwitchVelocityParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

SwitchVelocityParameterType::SwitchVelocityParameterType() :
    PresetParameter("switchVelocity", MSG_PARAM_SWITCH_VELOCITY)
{
    bindable = true;
	type = TYPE_BOOLEAN;
}

int SwitchVelocityParameterType::getOrdinalValue(Preset* p)
{
	return (int)p->isSwitchVelocity();
}

void SwitchVelocityParameterType::getValue(Preset* p, ExValue* value)
{
	value->setBool(p->isSwitchVelocity());
}

void SwitchVelocityParameterType::setValue(Preset* p, ExValue* value)
{
	p->setSwitchVelocity(value->getBool());
}

PUBLIC Parameter* SwitchVelocityParameter = new SwitchVelocityParameterType();

//////////////////////////////////////////////////////////////////////
//
// MaxUndo
//
//////////////////////////////////////////////////////////////////////

class MaxUndoParameterType : public PresetParameter
{
  public:
	MaxUndoParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

MaxUndoParameterType::MaxUndoParameterType() :
    PresetParameter("maxUndo", MSG_PARAM_MAX_UNDO)
{
    // not worth bindable
	type = TYPE_INT;

}

int MaxUndoParameterType::getOrdinalValue(Preset* p)
{
	return p->getMaxUndo();
}

void MaxUndoParameterType::getValue(Preset* p, ExValue* value)
{
	value->setInt(p->getMaxUndo());
}

void MaxUndoParameterType::setValue(Preset* p, ExValue* value)
{
	p->setMaxUndo(value->getInt());
}

PUBLIC Parameter* MaxUndoParameter = new MaxUndoParameterType();

//////////////////////////////////////////////////////////////////////
//
// MaxRedo
//
//////////////////////////////////////////////////////////////////////

class MaxRedoParameterType : public PresetParameter
{
  public:
	MaxRedoParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

MaxRedoParameterType::MaxRedoParameterType() :
    PresetParameter("maxRedo", MSG_PARAM_MAX_REDO)
{
    // not worth bindable
	type = TYPE_INT;
}

int MaxRedoParameterType::getOrdinalValue(Preset* p)
{
	return p->getMaxRedo();
}

void MaxRedoParameterType::getValue(Preset* p, ExValue* value)
{
	value->setInt(p->getMaxRedo());
}

void MaxRedoParameterType::setValue(Preset* p, ExValue* value)
{
	p->setMaxRedo(value->getInt());
}

PUBLIC Parameter* MaxRedoParameter = new MaxRedoParameterType();

//////////////////////////////////////////////////////////////////////
//
// NoFeedbackUndo
//
//////////////////////////////////////////////////////////////////////

class NoFeedbackUndoParameterType : public PresetParameter
{
  public:
	NoFeedbackUndoParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

NoFeedbackUndoParameterType::NoFeedbackUndoParameterType() :
    PresetParameter("noFeedbackUndo", MSG_PARAM_NO_FEEDBACK_UNDO)
{
    // not worth bindable
	type = TYPE_BOOLEAN;
}

int NoFeedbackUndoParameterType::getOrdinalValue(Preset* p)
{
	return (int)p->isNoFeedbackUndo();
}

void NoFeedbackUndoParameterType::getValue(Preset* p, ExValue* value)
{
	value->setBool(p->isNoFeedbackUndo());
}

void NoFeedbackUndoParameterType::setValue(Preset* p, ExValue* value)
{
	p->setNoFeedbackUndo(value->getBool());
}

PUBLIC Parameter* NoFeedbackUndoParameter = new NoFeedbackUndoParameterType();

//////////////////////////////////////////////////////////////////////
//
// NoLayerFlattening
//
//////////////////////////////////////////////////////////////////////

class NoLayerFlatteningParameterType : public PresetParameter
{
  public:
	NoLayerFlatteningParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

NoLayerFlatteningParameterType::NoLayerFlatteningParameterType() :
    PresetParameter("noLayerFlattening", MSG_PARAM_NO_LAYER_FLATTENING)
{
    // not worth bindable
	type = TYPE_BOOLEAN;
}

int NoLayerFlatteningParameterType::getOrdinalValue(Preset* p)
{
	return (int)p->isNoLayerFlattening();
}

void NoLayerFlatteningParameterType::getValue(Preset* p, ExValue* value)
{
	value->setBool(p->isNoLayerFlattening());
}

void NoLayerFlatteningParameterType::setValue(Preset* p, ExValue* value)
{
	p->setNoLayerFlattening(value->getBool());
}

PUBLIC Parameter* NoLayerFlatteningParameter = new NoLayerFlatteningParameterType();

//////////////////////////////////////////////////////////////////////
//
// SpeedSequence
//
//////////////////////////////////////////////////////////////////////

class SpeedSequenceParameterType : public PresetParameter
{
  public:
	SpeedSequenceParameterType();
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

SpeedSequenceParameterType::SpeedSequenceParameterType() :
    PresetParameter("speedSequence", MSG_PARAM_SPEED_SEQUENCE)
{
    // not bindable
	type = TYPE_STRING;
    addAlias("rateSequence");
}

void SpeedSequenceParameterType::getValue(Preset* p, ExValue* value)
{
	value->setString(p->getSpeedSequence()->getSource());
}

/**
 * This can only be set as a string.
 */
void SpeedSequenceParameterType::setValue(Preset* p, ExValue* value)
{
	p->getSpeedSequence()->setSource(value->getString());
}

PUBLIC Parameter* SpeedSequenceParameter = new SpeedSequenceParameterType();

//////////////////////////////////////////////////////////////////////
//
// SpeedShiftRestart
//
//////////////////////////////////////////////////////////////////////

class SpeedShiftRestartParameterType : public PresetParameter
{
  public:
	SpeedShiftRestartParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

SpeedShiftRestartParameterType::SpeedShiftRestartParameterType() :
    PresetParameter("speedShiftRestart", MSG_PARAM_SPEED_SHIFT_RESTART)
{
    bindable = true;
	type = TYPE_BOOLEAN;
    addAlias("rateShiftRetrigger");
    addAlias("rateShiftRestart");
}

int SpeedShiftRestartParameterType::getOrdinalValue(Preset* p)
{
	return (int)p->isSpeedShiftRestart();
}

void SpeedShiftRestartParameterType::getValue(Preset* p, ExValue* value)
{
	value->setBool(p->isSpeedShiftRestart());
}

void SpeedShiftRestartParameterType::setValue(Preset* p, ExValue* value)
{
	p->setSpeedShiftRestart(value->getBool());
}

PUBLIC Parameter* SpeedShiftRestartParameter = new SpeedShiftRestartParameterType(); 

//////////////////////////////////////////////////////////////////////
//
// PitchSequence
//
//////////////////////////////////////////////////////////////////////

class PitchSequenceParameterType : public PresetParameter
{
  public:
	PitchSequenceParameterType();
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

PitchSequenceParameterType::PitchSequenceParameterType() :
    PresetParameter("pitchSequence", MSG_PARAM_PITCH_SEQUENCE)
{
    // not bindable
	type = TYPE_STRING;
}

void PitchSequenceParameterType::getValue(Preset* p, ExValue* value)
{
	value->setString(p->getPitchSequence()->getSource());
}

/**
 * This can only be set as a string.
 */
void PitchSequenceParameterType::setValue(Preset* p, ExValue* value)
{
	p->getPitchSequence()->setSource(value->getString());
}

PUBLIC Parameter* PitchSequenceParameter = new PitchSequenceParameterType();

//////////////////////////////////////////////////////////////////////
//
// PitchShiftRestart
//
//////////////////////////////////////////////////////////////////////

class PitchShiftRestartParameterType : public PresetParameter
{
  public:
	PitchShiftRestartParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

PitchShiftRestartParameterType::PitchShiftRestartParameterType() :
    PresetParameter("pitchShiftRestart", MSG_PARAM_PITCH_SHIFT_RESTART)
{
    bindable = true;
	type = TYPE_BOOLEAN;
    addAlias("pitchShiftRetrigger");
}

int PitchShiftRestartParameterType::getOrdinalValue(Preset* p)
{
	return (int)p->isPitchShiftRestart();
}

void PitchShiftRestartParameterType::getValue(Preset* p, ExValue* value)
{
	value->setBool(p->isPitchShiftRestart());
}

void PitchShiftRestartParameterType::setValue(Preset* p, ExValue* value)
{
	p->setPitchShiftRestart(value->getBool());
}

PUBLIC Parameter* PitchShiftRestartParameter = new PitchShiftRestartParameterType();

//////////////////////////////////////////////////////////////////////
//
// SpeedStepRange
//
//////////////////////////////////////////////////////////////////////

class SpeedStepRangeParameterType : public PresetParameter
{
  public:
	SpeedStepRangeParameterType();
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

SpeedStepRangeParameterType::SpeedStepRangeParameterType() :
    PresetParameter("speedStepRange", MSG_PARAM_SPEED_STEP_RANGE)
{
    // not worth bindable ?
	type = TYPE_INT;
    low = 1;
    high = MAX_RATE_STEP;
}

void SpeedStepRangeParameterType::getValue(Preset* p, ExValue* value)
{
	value->setInt(p->getSpeedStepRange());
}

void SpeedStepRangeParameterType::setValue(Preset* p, ExValue* value)
{
    p->setSpeedStepRange(value->getInt());
}

PUBLIC Parameter* SpeedStepRangeParameter = new SpeedStepRangeParameterType();

//////////////////////////////////////////////////////////////////////
//
// SpeedBendRange
//
//////////////////////////////////////////////////////////////////////

class SpeedBendRangeParameterType : public PresetParameter
{
  public:
	SpeedBendRangeParameterType();
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

SpeedBendRangeParameterType::SpeedBendRangeParameterType() :
    PresetParameter("speedBendRange", MSG_PARAM_SPEED_BEND_RANGE)
{
    // not worth bindable?
	type = TYPE_INT;
    low = 1;
    high = MAX_BEND_STEP;
}

void SpeedBendRangeParameterType::getValue(Preset* p, ExValue* value)
{
	value->setInt(p->getSpeedBendRange());
}

void SpeedBendRangeParameterType::setValue(Preset* p, ExValue* value)
{
	p->setSpeedBendRange(value->getInt());
}

PUBLIC Parameter* SpeedBendRangeParameter = new SpeedBendRangeParameterType();

//////////////////////////////////////////////////////////////////////
//
// PitchStepRange
//
//////////////////////////////////////////////////////////////////////

class PitchStepRangeParameterType : public PresetParameter
{
  public:
	PitchStepRangeParameterType();
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

PitchStepRangeParameterType::PitchStepRangeParameterType() :
    PresetParameter("pitchStepRange", MSG_PARAM_PITCH_STEP_RANGE)
{
    // not worth bindable?
	type = TYPE_INT;
    low = 1;
    high = MAX_RATE_STEP;
}

void PitchStepRangeParameterType::getValue(Preset* p, ExValue* value)
{
	value->setInt(p->getPitchStepRange());
}

/**
 * This can only be set as a string.
 */
void PitchStepRangeParameterType::setValue(Preset* p, ExValue* value)
{
	p->setPitchStepRange(value->getInt());
}

PUBLIC Parameter* PitchStepRangeParameter = new PitchStepRangeParameterType();

//////////////////////////////////////////////////////////////////////
//
// PitchBendRange
//
//////////////////////////////////////////////////////////////////////

class PitchBendRangeParameterType : public PresetParameter
{
  public:
	PitchBendRangeParameterType();
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

PitchBendRangeParameterType::PitchBendRangeParameterType() :
    PresetParameter("pitchBendRange", MSG_PARAM_PITCH_BEND_RANGE)
{
    // not worth bindable?
	type = TYPE_INT;
    low = 1;
    high = MAX_BEND_STEP;
}

void PitchBendRangeParameterType::getValue(Preset* p, ExValue* value)
{
	value->setInt(p->getPitchBendRange());
}

/**
 * This can only be set as a string.
 */
void PitchBendRangeParameterType::setValue(Preset* p, ExValue* value)
{
	p->setPitchBendRange(value->getInt());
}

PUBLIC Parameter* PitchBendRangeParameter = new PitchBendRangeParameterType();

//////////////////////////////////////////////////////////////////////
//
// TimeStretchRange
//
//////////////////////////////////////////////////////////////////////

class TimeStretchRangeParameterType : public PresetParameter
{
  public:
	TimeStretchRangeParameterType();
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

TimeStretchRangeParameterType::TimeStretchRangeParameterType() :
    PresetParameter("timeStretchRange", MSG_PARAM_TIME_STRETCH_RANGE)
{
    // not worth bindable?
	type = TYPE_INT;
    low = 1;
    high = MAX_BEND_STEP;
}

void TimeStretchRangeParameterType::getValue(Preset* p, ExValue* value)
{
	value->setInt(p->getTimeStretchRange());
}

/**
 * This can only be set as a string.
 */
void TimeStretchRangeParameterType::setValue(Preset* p, ExValue* value)
{
	p->setTimeStretchRange(value->getInt());
}

PUBLIC Parameter* TimeStretchRangeParameter = new TimeStretchRangeParameterType();

//////////////////////////////////////////////////////////////////////
//
// SlipMode
//
//////////////////////////////////////////////////////////////////////

class SlipModeParameterType : public PresetParameter
{
  public:
	SlipModeParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

const char* SLIP_MODE_NAMES[] = {
	"subCycle", "cycle", "start", "relSubCycle", "relCycle", "time", NULL
};

int SLIP_MODE_KEYS[] = {
	MSG_VALUE_SLIP_MODE_SUBCYCLE, 
	MSG_VALUE_SLIP_MODE_CYCLE, 
	MSG_VALUE_SLIP_MODE_LOOP,
	MSG_VALUE_SLIP_MODE_REL_SUBCYCLE, 
	MSG_VALUE_SLIP_MODE_REL_CYCLE, 
	MSG_VALUE_SLIP_MODE_TIME, 
	0
};

SlipModeParameterType::SlipModeParameterType() :
    PresetParameter("slipMode", MSG_PARAM_SLIP_MODE)
{
    bindable = true;
	type = TYPE_ENUM;
	values = SLIP_MODE_NAMES;
	valueKeys = SLIP_MODE_KEYS;
}

int SlipModeParameterType::getOrdinalValue(Preset* p)
{
	return (int)p->getSlipMode();
}

void SlipModeParameterType::getValue(Preset* p, ExValue* value)
{
	value->setString(values[(int)p->getSlipMode()]);
}

void SlipModeParameterType::setValue(Preset* p, ExValue* value)
{
    // upgrade a value
    if (value->getType() == EX_STRING && 
        StringEqualNoCase("loop", value->getString()))
      value->setString("start");

	p->setSlipMode((Preset::SlipMode)getEnum(value));
}

PUBLIC Parameter* SlipModeParameter = new SlipModeParameterType();

//////////////////////////////////////////////////////////////////////
//
// SlipTime
//
//////////////////////////////////////////////////////////////////////

class SlipTimeParameterType : public PresetParameter
{
  public:
	SlipTimeParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

SlipTimeParameterType::SlipTimeParameterType() :
    PresetParameter("slipTime", MSG_PARAM_SLIP_TIME)
{
    bindable = true;
	type = TYPE_INT;
    // high is theoretically unbounded, but it becomes
    // hard to predict, give it a reasonable maximum
    // for binding
    high = 128;
}

int SlipTimeParameterType::getOrdinalValue(Preset* p)
{
	return p->getSlipTime();
}

void SlipTimeParameterType::getValue(Preset* p, ExValue* value)
{
	value->setInt(p->getSlipTime());
}

void SlipTimeParameterType::setValue(Preset* p, ExValue* value)
{
	p->setSlipTime(value->getInt());
}

PUBLIC Parameter* SlipTimeParameter = new SlipTimeParameterType();

//////////////////////////////////////////////////////////////////////
//
// AutoRecordTempo
//
//////////////////////////////////////////////////////////////////////

class AutoRecordTempoParameterType : public PresetParameter
{
  public:
	AutoRecordTempoParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

AutoRecordTempoParameterType::AutoRecordTempoParameterType() :
    PresetParameter("autoRecordTempo", MSG_PARAM_AUTO_RECORD_TEMPO)
{
    bindable = true;
	type = TYPE_INT;
    high = 500;
}

int AutoRecordTempoParameterType::getOrdinalValue(Preset* p)
{
	return p->getAutoRecordTempo();
}

void AutoRecordTempoParameterType::getValue(Preset* p, ExValue* value)
{
	value->setInt(p->getAutoRecordTempo());
}

void AutoRecordTempoParameterType::setValue(Preset* p, ExValue* value)
{
	p->setAutoRecordTempo(value->getInt());
}

PUBLIC Parameter* AutoRecordTempoParameter = new AutoRecordTempoParameterType();

//////////////////////////////////////////////////////////////////////
//
// AutoRecordBars
//
//////////////////////////////////////////////////////////////////////

class AutoRecordBarsParameterType : public PresetParameter
{
  public:
	AutoRecordBarsParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

AutoRecordBarsParameterType::AutoRecordBarsParameterType() :
    PresetParameter("autoRecordBars", MSG_PARAM_AUTO_RECORD_BARS)
{
    bindable = true;
	type = TYPE_INT;
    low = 1;
    // the high is really unconstrained but when binding to a MIDI CC
    // we need to have a useful, not to touchy range
    high = 64;

    // 1.45 name
    addAlias("recordBars");
}

int AutoRecordBarsParameterType::getOrdinalValue(Preset* p)
{
	return p->getAutoRecordBars();
}

void AutoRecordBarsParameterType::getValue(Preset* p, ExValue* value)
{
	value->setInt(p->getAutoRecordBars());
}

void AutoRecordBarsParameterType::setValue(Preset* p, ExValue* value)
{
	p->setAutoRecordBars(value->getInt());
}

PUBLIC Parameter* AutoRecordBarsParameter = new AutoRecordBarsParameterType();

//////////////////////////////////////////////////////////////////////
//
// SustainFunctions
//
//////////////////////////////////////////////////////////////////////

class SustainFunctionsParameterType : public PresetParameter
{
  public:
	SustainFunctionsParameterType();
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

SustainFunctionsParameterType::SustainFunctionsParameterType() :
    PresetParameter("sustainFunctions", MSG_PARAM_SUSTAIN_FUNCTIONS)
{
    // not bindable
	type = TYPE_STRING;
}

void SustainFunctionsParameterType::getValue(Preset* p, ExValue* value)
{
	value->setString(p->getSustainFunctions());
}

/**
 * This can only be set as a string.
 */
void SustainFunctionsParameterType::setValue(Preset* p, ExValue* value)
{
	p->setSustainFunctions(value->getString());
}

PUBLIC Parameter* SustainFunctionsParameter = new SustainFunctionsParameterType();

/****************************************************************************
 *                                                                          *
 *   						PRESET TRANSFER MODES                           *
 *                                                                          *
 ****************************************************************************/
/*
 * These could all have ordinal=true but it doesn't seem useful
 * to allow these as instant parameters?
 *
 */

//////////////////////////////////////////////////////////////////////
//
// RecordTransfer
//
//////////////////////////////////////////////////////////////////////

/**
 * This is a relatively obscure option to duplicate an EDPism
 * where if you are currently in record mode and you switch to 
 * another loop, the next loop will be reset and rerecorded
 * if you have the AutoRecord option on.  Since we have
 * merged AutoRecord with LoopCopy, this requires a new
 * parameter, and it makes sense to model this with a "follow"
 * parameter like the other modes.  The weird thing about this
 * one is that "restore" is meaningless.
 */
class RecordTransferParameterType : public PresetParameter
{
  public:
	RecordTransferParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

const char* RECORD_TRANSFER_NAMES[] = {
	"off", "follow", NULL
};

int RECORD_TRANSFER_KEYS[] = {
	MSG_VALUE_TRANSFER_OFF, 
	MSG_VALUE_TRANSFER_FOLLOW, 
	0
};

RecordTransferParameterType::RecordTransferParameterType() :
    PresetParameter("recordTransfer", MSG_PARAM_RECORD_TRANSFER)
{
    bindable = true;
	type = TYPE_ENUM;
	values = RECORD_TRANSFER_NAMES;
	valueKeys = RECORD_TRANSFER_KEYS;
}

int RecordTransferParameterType::getOrdinalValue(Preset* p)
{
	return (int)p->getRecordTransfer();
}

void RecordTransferParameterType::getValue(Preset* p, ExValue* value)
{
	value->setString(values[(int)p->getRecordTransfer()]);
}

void RecordTransferParameterType::setValue(Preset* p, ExValue* value)
{
    // ignore restore mode
    Preset::TransferMode mode = (Preset::TransferMode)getEnum(value);
    if (mode != Preset::XFER_RESTORE)
      p->setRecordTransfer((Preset::TransferMode)mode);
}

PUBLIC Parameter* RecordTransferParameter = new RecordTransferParameterType();

//////////////////////////////////////////////////////////////////////
//
// OverdubTransfer
//
//////////////////////////////////////////////////////////////////////

class OverdubTransferParameterType : public PresetParameter
{
  public:
	OverdubTransferParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

const char* MODE_TRANSFER_NAMES[] = {
	"off", "follow", "restore", NULL
};

int MODE_TRANSFER_KEYS[] = {
	MSG_VALUE_TRANSFER_OFF, 
	MSG_VALUE_TRANSFER_FOLLOW, 
	MSG_VALUE_TRANSFER_RESTORE, 
	0
};

OverdubTransferParameterType::OverdubTransferParameterType() :
    PresetParameter("overdubTransfer", MSG_PARAM_OVERDUB_TRANSFER)
{
    bindable = true;
	type = TYPE_ENUM;
	values = MODE_TRANSFER_NAMES;
	valueKeys = MODE_TRANSFER_KEYS;
}

int OverdubTransferParameterType::getOrdinalValue(Preset* p)
{
	return (int)p->getOverdubTransfer();
}

void OverdubTransferParameterType::getValue(Preset* p, ExValue* value)
{
	value->setString(values[(int)p->getOverdubTransfer()]);
}

void OverdubTransferParameterType::setValue(Preset* p, ExValue* value)
{
    // changed the name in 1.43
    fixEnum(value, "remember", "restore");
	p->setOverdubTransfer((Preset::TransferMode)getEnum(value));
}

PUBLIC Parameter* OverdubTransferParameter = new OverdubTransferParameterType();

//////////////////////////////////////////////////////////////////////
//
// ReverseTransfer
//
//////////////////////////////////////////////////////////////////////

class ReverseTransferParameterType : public PresetParameter
{
  public:
	ReverseTransferParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

ReverseTransferParameterType::ReverseTransferParameterType() :
    PresetParameter("reverseTransfer", MSG_PARAM_REVERSE_TRANSFER)
{
    bindable = true;
	type = TYPE_ENUM;
	values = MODE_TRANSFER_NAMES;
	valueKeys = MODE_TRANSFER_KEYS;
}

int ReverseTransferParameterType::getOrdinalValue(Preset* p)
{
	return (int)p->getReverseTransfer();
}

void ReverseTransferParameterType::getValue(Preset* p, ExValue* value)
{
	value->setString(values[(int)p->getReverseTransfer()]);
}

void ReverseTransferParameterType::setValue(Preset* p, ExValue* value)
{
    // changed the name in 1.43
    fixEnum(value, "remember", "restore");
	p->setReverseTransfer((Preset::TransferMode)getEnum(value));
}

PUBLIC Parameter* ReverseTransferParameter = new ReverseTransferParameterType();

//////////////////////////////////////////////////////////////////////
//
// SpeedTransfer
//
//////////////////////////////////////////////////////////////////////

class SpeedTransferParameterType : public PresetParameter
{
  public:
	SpeedTransferParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

SpeedTransferParameterType::SpeedTransferParameterType() :
    PresetParameter("speedTransfer", MSG_PARAM_SPEED_TRANSFER)
{
    bindable = true;
	type = TYPE_ENUM;
	values = MODE_TRANSFER_NAMES;
	valueKeys = MODE_TRANSFER_KEYS;
    addAlias("rateTransfer");
}

int SpeedTransferParameterType::getOrdinalValue(Preset* p)
{
	return (int)p->getSpeedTransfer();
}

void SpeedTransferParameterType::getValue(Preset* p, ExValue* value)
{
	value->setString(values[(int)p->getSpeedTransfer()]);
}

void SpeedTransferParameterType::setValue(Preset* p, ExValue* value)
{
    // changed the name in 1.43
    fixEnum(value, "remember", "restore");
	p->setSpeedTransfer((Preset::TransferMode)getEnum(value));
}

PUBLIC Parameter* SpeedTransferParameter = new SpeedTransferParameterType();

//////////////////////////////////////////////////////////////////////
//
// PitchTransfer
//
//////////////////////////////////////////////////////////////////////

class PitchTransferParameterType : public PresetParameter
{
  public:
	PitchTransferParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

PitchTransferParameterType::PitchTransferParameterType() :
    PresetParameter("pitchTransfer", MSG_PARAM_PITCH_TRANSFER)
{
    bindable = true;
	type = TYPE_ENUM;
	values = MODE_TRANSFER_NAMES;
	valueKeys = MODE_TRANSFER_KEYS;
}

int PitchTransferParameterType::getOrdinalValue(Preset* p)
{
	return (int)p->getPitchTransfer();
}

void PitchTransferParameterType::getValue(Preset* p, ExValue* value)
{
	value->setString(values[(int)p->getPitchTransfer()]);
}

void PitchTransferParameterType::setValue(Preset* p, ExValue* value)
{
    // changed the name in 1.43
    fixEnum(value, "remember", "restore");
	p->setPitchTransfer((Preset::TransferMode)getEnum(value));
}

PUBLIC Parameter* PitchTransferParameter = new PitchTransferParameterType();

//////////////////////////////////////////////////////////////////////
//
// WindowSlideUnit
//
//////////////////////////////////////////////////////////////////////

class WindowSlideUnitParameterType : public PresetParameter
{
  public:
	WindowSlideUnitParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

const char* WINDOW_SLIDE_NAMES[] = {
	"loop", "cycle", "subcycle", "msec", "frame", NULL
};

int WINDOW_SLIDE_KEYS[] = {
	MSG_UNIT_LOOP,
	MSG_UNIT_CYCLE,
	MSG_UNIT_SUBCYCLE,
	MSG_UNIT_MSEC,
	MSG_UNIT_FRAME,
	0
};

WindowSlideUnitParameterType::WindowSlideUnitParameterType() :
    PresetParameter("windowSlideUnit", MSG_PARAM_WINDOW_SLIDE_UNIT)
{
    bindable = true;
	type = TYPE_ENUM;
	values = WINDOW_SLIDE_NAMES;
	valueKeys = WINDOW_SLIDE_KEYS;
}

int WindowSlideUnitParameterType::getOrdinalValue(Preset* p)
{
	return p->getWindowSlideUnit();
}

void WindowSlideUnitParameterType::getValue(Preset* p, ExValue* value)
{
	value->setString(values[p->getWindowSlideUnit()]);
}

void WindowSlideUnitParameterType::setValue(Preset* p, ExValue* value)
{
	p->setWindowSlideUnit((Preset::WindowUnit)getEnum(value));
}

PUBLIC Parameter* WindowSlideUnitParameter = new WindowSlideUnitParameterType();

//////////////////////////////////////////////////////////////////////
//
// WindowEdgeUnit
//
//////////////////////////////////////////////////////////////////////

class WindowEdgeUnitParameterType : public PresetParameter
{
  public:
	WindowEdgeUnitParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

const char* WINDOW_EDGE_NAMES[] = {
	"loop", "cycle", "subcycle", "msec", "frame", NULL
};

int WINDOW_EDGE_KEYS[] = {
	MSG_UNIT_LOOP,
	MSG_UNIT_CYCLE,
	MSG_UNIT_SUBCYCLE,
	MSG_UNIT_MSEC,
	MSG_UNIT_FRAME,
	0
};

WindowEdgeUnitParameterType::WindowEdgeUnitParameterType() :
    PresetParameter("windowEdgeUnit", MSG_PARAM_WINDOW_EDGE_UNIT)
{
    bindable = true;
	type = TYPE_ENUM;
	values = WINDOW_EDGE_NAMES;
	valueKeys = WINDOW_EDGE_KEYS;
}

int WindowEdgeUnitParameterType::getOrdinalValue(Preset* p)
{
	return p->getWindowEdgeUnit();
}

void WindowEdgeUnitParameterType::getValue(Preset* p, ExValue* value)
{
	value->setString(values[p->getWindowEdgeUnit()]);
}

void WindowEdgeUnitParameterType::setValue(Preset* p, ExValue* value)
{
	p->setWindowEdgeUnit((Preset::WindowUnit)getEnum(value));
}

PUBLIC Parameter* WindowEdgeUnitParameter = new WindowEdgeUnitParameterType();

//////////////////////////////////////////////////////////////////////
//
// WindowSlideAmount
//
//////////////////////////////////////////////////////////////////////

class WindowSlideAmountParameterType : public PresetParameter 
{
  public:
	WindowSlideAmountParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

WindowSlideAmountParameterType::WindowSlideAmountParameterType() :
    PresetParameter("windowSlideAmount", MSG_PARAM_WINDOW_SLIDE_AMOUNT)
{
    bindable = true;
	type = TYPE_INT;
	low = 1;
    // unusable if it gets to large, if you need more use scripts
    // and WindowMove
	high = 128;
}

int WindowSlideAmountParameterType::getOrdinalValue(Preset* p)
{
	return p->getWindowSlideAmount();
}

void WindowSlideAmountParameterType::getValue(Preset* p, ExValue* value)
{
	value->setInt(p->getWindowSlideAmount());
}

void WindowSlideAmountParameterType::setValue(Preset* p, ExValue* value)
{
	p->setWindowSlideAmount(value->getInt());
}

PUBLIC Parameter* WindowSlideAmountParameter = new WindowSlideAmountParameterType();

//////////////////////////////////////////////////////////////////////
//
// WindowEdgeAmount
//
//////////////////////////////////////////////////////////////////////

class WindowEdgeAmountParameterType : public PresetParameter 
{
  public:
	WindowEdgeAmountParameterType();
    int getOrdinalValue(Preset* p);
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

WindowEdgeAmountParameterType::WindowEdgeAmountParameterType() :
    PresetParameter("windowEdgeAmount", MSG_PARAM_WINDOW_EDGE_AMOUNT)
{
    bindable = true;
	type = TYPE_INT;
	low = 1;
    // unusable if it gets to large, if you need more use scripts
    // and WindowMove
	high = 128;
}

int WindowEdgeAmountParameterType::getOrdinalValue(Preset* p)
{
	return p->getWindowEdgeAmount();
}

void WindowEdgeAmountParameterType::getValue(Preset* p, ExValue* value)
{
	value->setInt(p->getWindowEdgeAmount());
}

void WindowEdgeAmountParameterType::setValue(Preset* p, ExValue* value)
{
	p->setWindowEdgeAmount(value->getInt());
}

PUBLIC Parameter* WindowEdgeAmountParameter = new WindowEdgeAmountParameterType();

/****************************************************************************/
/****************************************************************************
 *                                                                          *
 *                        DEPRECATED PRESET PARAMETERS                      *
 *                                                                          *
 ****************************************************************************/
/****************************************************************************/

// 
// Parameters in this section are retained only so we can parse them
// in old mobius.xml files and upgrade them to the new parameters.
//

//////////////////////////////////////////////////////////////////////
//
// AutoRecord
//
// DEPRECATED: Replaced by EmptyLoopAction
//
//////////////////////////////////////////////////////////////////////

class AutoRecordParameterType : public PresetParameter
{
  public:
	AutoRecordParameterType();
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

AutoRecordParameterType::AutoRecordParameterType() :
    PresetParameter("autoRecord", 0)
{
    deprecated = true;
	type = TYPE_BOOLEAN;
}

void AutoRecordParameterType::getValue(Preset* p, ExValue* value)
{
    Preset::EmptyLoopAction action = p->getEmptyLoopAction();
    bool bvalue = (action == Preset::EMPTY_LOOP_RECORD);

	value->setBool(bvalue);
}

void AutoRecordParameterType::setValue(Preset* p, ExValue* value)
{
    // since we merged two things, it is ambiguous what this should do
    if (value->getBool()) {
        // if they bothered to ask for it, it overrides LoopCopy
        p->setEmptyLoopAction(Preset::EMPTY_LOOP_RECORD);
    }
    else {
        // turn the action off only if it is already RECORD
        if (p->getEmptyLoopAction() == Preset::EMPTY_LOOP_RECORD)
          p->setEmptyLoopAction(Preset::EMPTY_LOOP_NONE);
    }
}

PUBLIC Parameter* AutoRecordParameter = new AutoRecordParameterType();

//////////////////////////////////////////////////////////////////////
//
// InsertMode
//
// DEPRECATED: InsertMode=Sustain replaced with SustainFunctions
//
//////////////////////////////////////////////////////////////////////

class InsertModeParameterType : public PresetParameter
{
  public:
	InsertModeParameterType();
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

const char* INSERT_MODE_NAMES[] = {
	"rehearse", "replace", "substitute", "halfspeed", "reverse",
	"insert", "sustain", NULL
};

InsertModeParameterType::InsertModeParameterType() :
    PresetParameter("insertMode", 0)
{
    deprecated = true;
	type = TYPE_ENUM;
	values = INSERT_MODE_NAMES;
}

void InsertModeParameterType::getValue(Preset* p, ExValue* value)
{
	value->setNull();
}

void InsertModeParameterType::setValue(Preset* p, ExValue* value)
{
    const char* s = value->getString();
    if (StringEqualNoCase(s, "sustain")) {
        p->addSustainFunction("Insert");
    }
}

PUBLIC Parameter* InsertModeParameter = new InsertModeParameterType();

//////////////////////////////////////////////////////////////////////
//
// InterfaceMode
//
// DEPRECATED: InterfaceMode=Export is now AltFeedbackEnable
// 
//////////////////////////////////////////////////////////////////////

class InterfaceModeParameterType : public PresetParameter
{
  public:
	InterfaceModeParameterType();
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

const char* INTERFACE_MODE_NAMES[] = {
	"loop", "delay", "expert", "stutter", "in", "out",
	"replace", "flip", NULL
};

InterfaceModeParameterType::InterfaceModeParameterType() :
    PresetParameter("interfaceMode", 0)
{
    deprecated = true;
	type = TYPE_ENUM;
	values = INTERFACE_MODE_NAMES;
}

void InterfaceModeParameterType::getValue(Preset* p, ExValue* value)
{
	value->setNull();
}

void InterfaceModeParameterType::setValue(Preset* p, ExValue* value)
{
    const char* s = value->getString();

    if (StringEqualNoCase(s, "expert")) {

        p->setAltFeedbackEnable(true);
    }
}

PUBLIC Parameter* InterfaceModeParameter = new InterfaceModeParameterType();

//////////////////////////////////////////////////////////////////////
//
// LoopCopy
//
// DEPRECATED: Replaced by EmptyLoopAction
//
//////////////////////////////////////////////////////////////////////

class LoopCopyParameterType : public PresetParameter
{
  public:
	LoopCopyParameterType();
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

const char* LOOP_COPY_NAMES[] = {
	"off", "timing", "sound", NULL
};

LoopCopyParameterType::LoopCopyParameterType() :
    PresetParameter("loopCopy", 0)
{
    deprecated = true;
	type = TYPE_ENUM;
	values = LOOP_COPY_NAMES;
}

void LoopCopyParameterType::getValue(Preset* p, ExValue* value)
{
    Preset::XLoopCopy lc = Preset::XLOOP_COPY_OFF;

    Preset::EmptyLoopAction action = p->getEmptyLoopAction();

    if (action == Preset::EMPTY_LOOP_TIMING)
      lc = Preset::XLOOP_COPY_TIMING;

    else if (action == Preset::EMPTY_LOOP_COPY)
      lc = Preset::XLOOP_COPY_SOUND;

	value->setString(values[(int)lc]);
}

void LoopCopyParameterType::setValue(Preset* p, ExValue* value)
{
    Preset::XLoopCopy lc = (Preset::XLoopCopy)getEnum(value);
    if (lc == Preset::XLOOP_COPY_OFF) {
        // in the old days, this could turn off while leaving AutoRecord on
        // we have no way of maintaining both states now, so there
        // is the potential for old scripts to break, but unlikely
        p->setEmptyLoopAction(Preset::EMPTY_LOOP_NONE);
    }
    else if (lc == Preset::XLOOP_COPY_TIMING) {
        p->setEmptyLoopAction(Preset::EMPTY_LOOP_TIMING);
    }
    else if (lc == Preset::XLOOP_COPY_SOUND) {
        p->setEmptyLoopAction(Preset::EMPTY_LOOP_COPY);
    }
}

PUBLIC Parameter* LoopCopyParameter = new LoopCopyParameterType();

//////////////////////////////////////////////////////////////////////
//
// OverdubMode
//
// DEPRECATED: Replaced by OverdubQuantized and SustainFunctions
//
//////////////////////////////////////////////////////////////////////

class OverdubModeParameterType : public PresetParameter
{
  public:
	OverdubModeParameterType();
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

const char *OVERDUB_MODE_NAMES[] = {
	"toggle", "sustain", "quantized", NULL
};

OverdubModeParameterType::OverdubModeParameterType() :
    PresetParameter("overdubMode", 0)
{
    deprecated = true;
	type = TYPE_ENUM;
	values = OVERDUB_MODE_NAMES;
}

void OverdubModeParameterType::getValue(Preset* p, ExValue* value)
{
	value->setNull();
}

void OverdubModeParameterType::setValue(Preset* p, ExValue* value)
{
    const char* s = value->getString();

    if (StringEqualNoCase(s, "sustain")) {
        p->addSustainFunction("Overdub");
    }
    else if (StringEqualNoCase(s, "quantized")) {
        p->setOverdubQuantized(true);
    }
}

PUBLIC Parameter* OverdubModeParameter = new OverdubModeParameterType();

//////////////////////////////////////////////////////////////////////
//
// RecordMode
//
// DEPRECATED: Replaced by SustainFunctions and RecordResetsFeedback
//
//////////////////////////////////////////////////////////////////////

class RecordModeParameterType : public PresetParameter
{
  public:
	RecordModeParameterType();
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

const char* RECORD_MODE_NAMES[] = {
	"toggle", "sustain", "safe", NULL
};

RecordModeParameterType::RecordModeParameterType() :
    PresetParameter("recordMode", 0)
{
    deprecated = true;
	type = TYPE_ENUM;
	values = RECORD_MODE_NAMES;
}

void RecordModeParameterType::getValue(Preset* p, ExValue* value)
{
	value->setString("toggle");
}

void RecordModeParameterType::setValue(Preset* p, ExValue* value)
{
    const char* s = value->getString();

    if (StringEqualNoCase(s, "sustain")) {
        p->addSustainFunction("Record");
    }
    else if (StringEqualNoCase(s, "safe")) {
        p->setRecordResetsFeedback(true);
    }
}

PUBLIC Parameter* RecordModeParameter = new RecordModeParameterType();

//////////////////////////////////////////////////////////////////////
//
// SamplerStyle
//
// DEPRECATED: This is now deprecated and replaced with SwitchLocation, 
// SwitchDuration, and ReturnLocation.  We maintain this as a hidden
// parameter for backward compatibility with scripts and to auto-upgrade
// old config files.  The parameter can be set but it cannot be read.
//
//////////////////////////////////////////////////////////////////////

class SamplerStyleParameterType : public PresetParameter
{
  public:
	SamplerStyleParameterType();
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

const char* SAMPLER_STYLE_NAMES[] = {
	"run", "start", "once", "attack", "continuous", NULL
};

SamplerStyleParameterType::SamplerStyleParameterType() :
    PresetParameter("samplerStyle", 0)
{
    deprecated = true;
	type = TYPE_ENUM;
	values = SAMPLER_STYLE_NAMES;
}

void SamplerStyleParameterType::getValue(Preset* p, ExValue* value)
{
    // value can't be returned
	value->setNull();
}

void SamplerStyleParameterType::setValue(Preset* p, ExValue* value)
{
    const char* str = value->getString();
    bool attack = false;

    if (StringEqualNoCase(str, "run")) {
        p->setSwitchLocation(Preset::SWITCH_RESTORE);
        p->setReturnLocation(Preset::SWITCH_RESTORE);
        p->setSwitchDuration(Preset::SWITCH_PERMANENT);
    }
    else if (StringEqualNoCase(str, "start")) {
        p->setSwitchLocation(Preset::SWITCH_START);
        p->setReturnLocation(Preset::SWITCH_RESTORE);
        p->setSwitchDuration(Preset::SWITCH_PERMANENT);
    }
    else if (StringEqualNoCase(str, "once")) {
        p->setSwitchLocation(Preset::SWITCH_START);
        p->setReturnLocation(Preset::SWITCH_RESTORE);
        p->setSwitchDuration(Preset::SWITCH_ONCE_RETURN);
    }
    else if (StringEqualNoCase(str, "attack")) {
        p->setSwitchLocation(Preset::SWITCH_START);
        p->setReturnLocation(Preset::SWITCH_RESTORE);
        p->setSwitchDuration(Preset::SWITCH_SUSTAIN);
        attack = true;
    }
    else if (StringEqualNoCase(str, "continuous")) {
        p->setSwitchLocation(Preset::SWITCH_FOLLOW);
        p->setReturnLocation(Preset::SWITCH_FOLLOW);
        p->setSwitchDuration(Preset::SWITCH_PERMANENT);
    }

    // switchVelocity set if mode was "attack"
	p->setSwitchVelocity(attack);

}

PUBLIC Parameter* SamplerStyleParameter = new SamplerStyleParameterType();

//////////////////////////////////////////////////////////////////////
//
// TrackCopy
//
// DEPRECATED: Replaced by EmptyTrackAction
// Needed for backward compatibility with old scripts.
//
//////////////////////////////////////////////////////////////////////

class TrackCopyParameterType : public PresetParameter
{
  public:
	TrackCopyParameterType();
	void getValue(Preset* p, ExValue* value);
	void setValue(Preset* p, ExValue* value);
};

const char* TRACK_COPY_NAMES[] = {
	"off", "timing", "sound", NULL
};

TrackCopyParameterType::TrackCopyParameterType() :
    PresetParameter("trackCopy", 0)
{
    deprecated = true;
	type = TYPE_ENUM;
	values = TRACK_COPY_NAMES;
}

void TrackCopyParameterType::getValue(Preset* p, ExValue* value)
{
    Preset::XTrackCopy tc = Preset::TRACK_COPY_OFF;
    Preset::EmptyLoopAction action = p->getEmptyTrackAction();
    switch (action) {
        case Preset::EMPTY_LOOP_RECORD:
            // not supported by the old parameter, ignore
            break;
        case Preset::EMPTY_LOOP_COPY:
            tc = Preset::TRACK_COPY_SOUND;
            break;
        case Preset::EMPTY_LOOP_TIMING:
            tc = Preset::TRACK_COPY_TIMING;
            break;
        case Preset::EMPTY_LOOP_NONE:
			break;
    }
	value->setString(values[(int)tc]);
}

void TrackCopyParameterType::setValue(Preset* p, ExValue* value)
{
    Preset::XTrackCopy tc = (Preset::XTrackCopy)getEnum(value);
    switch (tc) {
        case Preset::TRACK_COPY_OFF:
            p->setEmptyTrackAction(Preset::EMPTY_LOOP_NONE);
            break;
        case Preset::TRACK_COPY_SOUND:
            p->setEmptyTrackAction(Preset::EMPTY_LOOP_COPY);
            break;
        case Preset::TRACK_COPY_TIMING:
            p->setEmptyTrackAction(Preset::EMPTY_LOOP_TIMING);
            break;
    }
}

PUBLIC Parameter* TrackCopyParameter = new TrackCopyParameterType();

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
