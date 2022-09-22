/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Static object definitions for SetupTrack/Track parameters.
 *
 * Track parameters are more complicated than Preset parameters because we
 * have two locations to deal with.  The get/setObjectValue methods
 * get a SetupTrack configuration object.
 * 
 * The get/setValue objects used for bindings do not use the SetupTrack,
 * instead the Track will have copied the things defined in SetupTrack
 * over to fields on the Track and we get/set those.  The Track in effect
 * is behaving like a private copy of the SetupTrack.  
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
#include "Setup.h"
#include "Track.h"
#include "Resampler.h"
#include "Script.h"
#include "Synchronizer.h"

#include "Parameter.h"

/****************************************************************************
 *                                                                          *
 *                              TRACK PARAMETER                             *
 *                                                                          *
 ****************************************************************************/

class TrackParameter : public Parameter
{
  public:

    TrackParameter(const char* name, int key) :
        Parameter(name, key) {
        scope = PARAM_SCOPE_TRACK;
    }

    /**
     * Overload the Parameter versions and cast to a SetupTrack.
     */
	void getObjectValue(void* object, ExValue* value);
	void setObjectValue(void* object, ExValue* value);

    /**
     * Overload the Parameter versions and resolve to a Track.
     */
	void getValue(Export* exp, ExValue* value);
	void setValue(Action* action);
	int getOrdinalValue(Export* exp);

	virtual void getValue(SetupTrack* t, ExValue* value) = 0;
	virtual void setValue(SetupTrack* t, ExValue* value) = 0;

	virtual int getOrdinalValue(Track* t) = 0;

	virtual void getValue(Track* t, ExValue* value) = 0;
	virtual void setValue(Track* t, ExValue* value);

  protected:
    
    void doFunction(Action* action, Function* func);

};

void TrackParameter::getObjectValue(void* object, ExValue* value)
{
    return getValue((SetupTrack*)object, value);
}

void TrackParameter::setObjectValue(void* object, ExValue* value)
{
    return setValue((SetupTrack*)object, value);
}

/**
 * Default setter for an Action.  This does the common
 * work of extracting the resolved Track and converting
 * the value into a consistent ExValue.
 */
void TrackParameter::setValue(Action* action)
{
    Track* track = action->getResolvedTrack();
    if (track != NULL)  
      setValue(track, &action->arg);
}

/**
 * This is almost always overloaded.
 */
void TrackParameter::setValue(Track* t, ExValue* value)
{
    Trace(1, "TrackParameter: %s not overloaded!\n", getName());
}

/**
 * Default getter for an Export.  This does the common
 * work of digging out the resolved Track.
 */
void TrackParameter::getValue(Export* exp, ExValue* value)
{
    Track* track = exp->getTrack();
    if (track != NULL)
      getValue(track, value);
	else
      value->setNull();
}

int TrackParameter::getOrdinalValue(Export* exp)
{
    int value = -1;
    Track* track = exp->getTrack();
    if (track != NULL)
	  value = getOrdinalValue(track);
    return value;
}

/**
 * The Speed and Pitch parameters change latency so they must be scheduled
 * as functions rather than having an immediate effect on the track like
 * most other parameters.
 *
 * This is called to convert the parameter action into a function action
 * and invoke it.
 */
PRIVATE void TrackParameter::doFunction(Action* action, Function* func)
{
    // this flag must be on for ScriptInterpreter
    if (!scheduled)
      Trace(1, "Parameter %s is not flagged as being scheduled!\n",
            getName());

    // Convert the Action to a function
    action->setFunction(func);

    // parameter bindings don't set this, need for functions
    action->down = true;
    action->escapeQuantization = true;
    action->noTrace = true;

    Mobius* m = (Mobius*)action->mobius;
    m->doActionNow(action);
}

//////////////////////////////////////////////////////////////////////
//
// TrackName
//
//////////////////////////////////////////////////////////////////////

class TrackNameParameterType : public TrackParameter
{
  public:
	TrackNameParameterType();
	void getValue(SetupTrack* t, ExValue* value);
	void setValue(SetupTrack* t, ExValue* value);
	void getValue(Track* t, ExValue* value);
	void setValue(Track* t, ExValue* value);
    int getOrdinalValue(Track* t);
};

TrackNameParameterType::TrackNameParameterType() :
    TrackParameter("trackName", MSG_PARAM_TRACK_NAME)
{
	type = TYPE_STRING;

    // temporary, I don't like the global namespace should
    // have another value for "xmlName" or something?
    addAlias("name");
}

void TrackNameParameterType::getValue(SetupTrack* t, ExValue* value)
{
    value->setString(t->getName());
}

void TrackNameParameterType::setValue(SetupTrack* t, ExValue* value)
{
    t->setName(value->getString());
}

void TrackNameParameterType::getValue(Track* t, ExValue* value)
{
	value->setString(t->getName());
}

void TrackNameParameterType::setValue(Track* t, ExValue* value)
{
    t->setName(value->getString());
}

int TrackNameParameterType::getOrdinalValue(Track* t)
{
    return -1;
}

PUBLIC Parameter* TrackNameParameter = new TrackNameParameterType();

//////////////////////////////////////////////////////////////////////
//
// Focus
//
//////////////////////////////////////////////////////////////////////

class FocusParameterType : public TrackParameter
{
  public:
	FocusParameterType();
	void getValue(SetupTrack* t, ExValue* value);
	void setValue(SetupTrack* t, ExValue* value);
	void getValue(Track* t, ExValue* value);
	void setValue(Track* t, ExValue* value);
    int getOrdinalValue(Track* t);
};

FocusParameterType::FocusParameterType() :
    TrackParameter("focus", MSG_PARAM_FOCUS)
{
    // not bindable, use the FocusLock function
	type = TYPE_BOOLEAN;
    resettable = true;
    addAlias("focusLock");
}

void FocusParameterType::getValue(SetupTrack* t, ExValue* value)
{
    value->setBool(t->isFocusLock());
}

void FocusParameterType::setValue(SetupTrack* t, ExValue* value)
{
    t->setFocusLock(value->getBool());
}

void FocusParameterType::getValue(Track* t, ExValue* value)
{
    value->setBool(t->isFocusLock());
}

void FocusParameterType::setValue(Track* t, ExValue* value)
{
	t->setFocusLock(value->getBool());
}

int FocusParameterType::getOrdinalValue(Track* t)
{
    return (int)t->isFocusLock();
}

PUBLIC Parameter* FocusParameter = new FocusParameterType();

//////////////////////////////////////////////////////////////////////
//
// Group
//
//////////////////////////////////////////////////////////////////////

class GroupParameterType : public TrackParameter
{
  public:
	GroupParameterType();
	void getValue(SetupTrack* t, ExValue* value);
	void setValue(SetupTrack* t, ExValue* value);
    int getOrdinalValue(Track* t);
	void getValue(Track* t, ExValue* value);
	void setValue(Track* t, ExValue* value);

	int getHigh(MobiusInterface* m);
	int getBindingHigh(MobiusInterface* m);
    void getOrdinalLabel(MobiusInterface* m, int i, ExValue* value);
};

GroupParameterType::GroupParameterType() :
    TrackParameter("group", MSG_PARAM_GROUP)
{
    bindable = true;
	type = TYPE_INT;
    resettable = true;
}

void GroupParameterType::getValue(SetupTrack* t, ExValue* value)
{
    value->setInt(t->getGroup());
}

void GroupParameterType::setValue(SetupTrack* t, ExValue* value)
{
    t->setGroup(value->getInt());
}

int GroupParameterType::getOrdinalValue(Track* t)
{
    return t->getGroup();
}

void GroupParameterType::getValue(Track* t, ExValue* value)
{
    value->setInt(t->getGroup());
}

void GroupParameterType::setValue(Track* t, ExValue* value)
{
	Mobius* m = t->getMobius();
    MobiusConfig* config = m->getConfiguration();
	int maxGroup = config->getTrackGroups();

	int g = value->getInt();
	if (g >= 0 && g <= maxGroup)
	  t->setGroup(g);
    else {
        // also allow A,B,C since that's what we display
        const char* str = value->getString();
        if (str != NULL && strlen(str) > 0) {
            char letter = toupper(str[0]);
            if (letter >= 'A' && letter <= 'Z') {
                g = (int)letter - (int)'A';
                if (g >= 0 && g <= maxGroup)
                  t->setGroup(g);
            }
        }
    }
}

/**
 * !! The max can change if the global parameters are edited.
 * Need to work out a way to convey that to ParameterEditor.
 */
PUBLIC int GroupParameterType::getHigh(MobiusInterface* m)
{
	MobiusConfig* config = m->getConfiguration();
    int max = config->getTrackGroups();
    return max;
}

/**
 * We should always have at least 1 group configured, but just
 * in case the config has zero, since we're TYPE_INT override
 * this so the default of 127 doesn't apply.
 */
PUBLIC int GroupParameterType::getBindingHigh(MobiusInterface* m)
{
    return getHigh(m);
}

/**
 * Given an ordinal, map it into a display label.
 */
PUBLIC void GroupParameterType::getOrdinalLabel(MobiusInterface* m, 
                                                int i, ExValue* value)
{
    if (i <= 0)
      value->setString("None");
    else {
        char buf[64];
		sprintf(buf, "Group %c", (char)((int)'A' + (i - 1)));
        value->setString(buf);
    }
}

PUBLIC Parameter* GroupParameter = new GroupParameterType();

//////////////////////////////////////////////////////////////////////
//
// Mono
//
//////////////////////////////////////////////////////////////////////

class MonoParameterType : public TrackParameter
{
  public:
	MonoParameterType();
	void getValue(SetupTrack* t, ExValue* value);
	void setValue(SetupTrack* t, ExValue* value);
	void getValue(Track* t, ExValue* value);
	void setValue(Track* t, ExValue* value);
    int getOrdinalValue(Track* t);
};

MonoParameterType::MonoParameterType() :
    TrackParameter("mono", MSG_PARAM_MONO)
{
    // not worth bindable?
	type = TYPE_BOOLEAN;
}

void MonoParameterType::getValue(SetupTrack* t, ExValue* value)
{
    value->setBool(t->isMono());
}

void MonoParameterType::setValue(SetupTrack* t, ExValue* value)
{
    t->setMono(value->getBool());
}

void MonoParameterType::getValue(Track* t, ExValue* value)
{
    value->setBool(t->isMono());
}

void MonoParameterType::setValue(Track* t, ExValue* value)
{
    // can we just change this on the fly?
	t->setMono(value->getBool());
}

int MonoParameterType::getOrdinalValue(Track* t)
{
    return -1;
}

PUBLIC Parameter* MonoParameter = new MonoParameterType();

//////////////////////////////////////////////////////////////////////
//
// Feedback Level
//
//////////////////////////////////////////////////////////////////////

class FeedbackLevelParameterType : public TrackParameter
{
  public:
	FeedbackLevelParameterType();

	void getValue(SetupTrack* t, ExValue* value);
	void setValue(SetupTrack* t, ExValue* value);

	void getValue(Track* t, ExValue* value);
	void setValue(Track* t, ExValue* value);
    int getOrdinalValue(Track* t);
};

FeedbackLevelParameterType::FeedbackLevelParameterType() :
    TrackParameter("feedback", MSG_PARAM_FEEDBACK_LEVEL)
{
    bindable = true;
    control = true;
	type = TYPE_INT;
	high = 127;
    resettable = true;
}

void FeedbackLevelParameterType::getValue(SetupTrack* t, ExValue* value)
{
    value->setInt(t->getFeedback());
}

void FeedbackLevelParameterType::setValue(SetupTrack* t, ExValue* value)
{
    t->setFeedback(value->getInt());
}

void FeedbackLevelParameterType::getValue(Track* t, ExValue* value)
{
    value->setInt(t->getFeedback());
}

void FeedbackLevelParameterType::setValue(Track* t, ExValue* value)
{
    int v = value->getInt();
    if (v >= low && v <= high)
      t->setFeedback(v);
}

int FeedbackLevelParameterType::getOrdinalValue(Track* t)
{
    return t->getFeedback();
}

PUBLIC Parameter* FeedbackLevelParameter = new FeedbackLevelParameterType();

//////////////////////////////////////////////////////////////////////
//
// AltFeedback Level
//
//////////////////////////////////////////////////////////////////////

class AltFeedbackLevelParameterType : public TrackParameter
{
  public:
	AltFeedbackLevelParameterType();

	void getValue(SetupTrack* t, ExValue* value);
	void setValue(SetupTrack* t, ExValue* value);

	void getValue(Track* t, ExValue* value);
	void setValue(Track* t, ExValue* value);
    int getOrdinalValue(Track* t);
};

AltFeedbackLevelParameterType::AltFeedbackLevelParameterType() :
    TrackParameter("altFeedback", MSG_PARAM_ALT_FEEDBACK_LEVEL)
{
    bindable = true;
    control = true;
	type = TYPE_INT;
	high = 127;
    resettable = true;
}

void AltFeedbackLevelParameterType::getValue(SetupTrack* t, ExValue* value)
{
    value->setInt(t->getAltFeedback());
}

void AltFeedbackLevelParameterType::setValue(SetupTrack* t, ExValue* value)
{
    t->setAltFeedback(value->getInt());
}

void AltFeedbackLevelParameterType::getValue(Track* t, ExValue* value)
{
    value->setInt(t->getAltFeedback());
}

void AltFeedbackLevelParameterType::setValue(Track* t, ExValue* value)
{
    int v = value->getInt();
    if (v >= low && v <= high)
      t->setAltFeedback(v);
}

int AltFeedbackLevelParameterType::getOrdinalValue(Track* t)
{
    return t->getAltFeedback();
}

PUBLIC Parameter* AltFeedbackLevelParameter = new AltFeedbackLevelParameterType();

//////////////////////////////////////////////////////////////////////
//
// InputLevel
//
//////////////////////////////////////////////////////////////////////

class InputLevelParameterType : public TrackParameter
{
  public:
	InputLevelParameterType();
	void getValue(SetupTrack* t, ExValue* value);
	void setValue(SetupTrack* t, ExValue* value);
	void getValue(Track* t, ExValue* value);
	void setValue(Track* t, ExValue* value);
    int getOrdinalValue(Track* t);
};

InputLevelParameterType::InputLevelParameterType() :
    TrackParameter("input", MSG_PARAM_INPUT_LEVEL)
{
    bindable = true;
    control = true;
	type = TYPE_INT;
	high = 127;
    resettable = true;
}

void InputLevelParameterType::getValue(SetupTrack* t, ExValue* value)
{
    value->setInt(t->getInputLevel());
}

void InputLevelParameterType::setValue(SetupTrack* t, ExValue* value)
{
    t->setInputLevel(value->getInt());
}

void InputLevelParameterType::getValue(Track* t, ExValue* value)
{
    value->setInt(t->getInputLevel());
}

void InputLevelParameterType::setValue(Track* t, ExValue* value)
{
    int v = value->getInt();
    if (v >= low && v <= high)
      t->setInputLevel(v);
}

int InputLevelParameterType::getOrdinalValue(Track* t)
{
    return t->getInputLevel();
}

PUBLIC Parameter* InputLevelParameter = new InputLevelParameterType();

//////////////////////////////////////////////////////////////////////
//
// OutputLevel
//
//////////////////////////////////////////////////////////////////////

class OutputLevelParameterType : public TrackParameter
{
  public:
	OutputLevelParameterType();
	void getValue(SetupTrack* t, ExValue* value);
	void setValue(SetupTrack* t, ExValue* value);
	void getValue(Track* t, ExValue* value);
	void setValue(Track* t, ExValue* value);
    int getOrdinalValue(Track* t);
};

OutputLevelParameterType::OutputLevelParameterType() :
    TrackParameter("output", MSG_PARAM_OUTPUT_LEVEL)
{
    bindable = true;
    control = true;
	type = TYPE_INT;
	high = 127;
    resettable = true;
}

void OutputLevelParameterType::getValue(SetupTrack* t, ExValue* value)
{
    value->setInt(t->getOutputLevel());
}

void OutputLevelParameterType::setValue(SetupTrack* t, ExValue* value)
{
    t->setOutputLevel(value->getInt());
}

void OutputLevelParameterType::getValue(Track* t, ExValue* value)
{
    value->setInt(t->getOutputLevel());
}

void OutputLevelParameterType::setValue(Track* t, ExValue* value)
{
    int v = value->getInt();
    if (v >= low && v <= high)
      t->setOutputLevel(v);
}

int OutputLevelParameterType::getOrdinalValue(Track* t)
{
    return t->getOutputLevel();
}

PUBLIC Parameter* OutputLevelParameter = new OutputLevelParameterType();

//////////////////////////////////////////////////////////////////////
//
// Pan
//
//////////////////////////////////////////////////////////////////////

class PanParameterType : public TrackParameter
{
  public:
	PanParameterType();
	void getValue(SetupTrack* t, ExValue* value);
	void setValue(SetupTrack* t, ExValue* value);
	void getValue(Track* t, ExValue* value);
	void setValue(Track* t, ExValue* value);
    int getOrdinalValue(Track* t);
};

PanParameterType::PanParameterType() :
    TrackParameter("pan", MSG_PARAM_PAN)
{
    bindable = true;
    control = true;
    // now that we have zero center parameters with positive 
    // and negative values it would make sense to do that for pan
    // but we've had this zero based and 64 center for so long
    // I think it would too painful to change
	type = TYPE_INT;
	high = 127;
    resettable = true;
}

void PanParameterType::getValue(SetupTrack* t, ExValue* value)
{
    value->setInt(t->getPan());
}

void PanParameterType::setValue(SetupTrack* t, ExValue* value)
{
    t->setPan(value->getInt());
}

void PanParameterType::getValue(Track* t, ExValue* value)
{
    value->setInt(t->getPan());
}

void PanParameterType::setValue(Track* t, ExValue* value)
{
    int v = value->getInt();
    if (v >= low && v <= high)
      t->setPan(v);
}

int PanParameterType::getOrdinalValue(Track* t)
{
    return t->getPan();
}

PUBLIC Parameter* PanParameter = new PanParameterType();

//////////////////////////////////////////////////////////////////////
//
// SpeedOctave
//
//////////////////////////////////////////////////////////////////////

/**
 * Not currently exposed.
 */
class SpeedOctaveParameterType : public TrackParameter
{
  public:
	SpeedOctaveParameterType();
	void getValue(SetupTrack* t, ExValue* value);
	void setValue(SetupTrack* t, ExValue* value);
	void getValue(Track* t, ExValue* value);
    void setValue(Action* action);
    int getOrdinalValue(Track* t);
};

SpeedOctaveParameterType::SpeedOctaveParameterType() :
    TrackParameter("speedOctave", MSG_PARAM_SPEED_OCTAVE)
{
    bindable = true;
    control = true;
	type = TYPE_INT;
    // the range is 4, might want to halve this?
    high = MAX_RATE_OCTAVE;
    low = -MAX_RATE_OCTAVE;
    zeroCenter = true;
    resettable = true;
    // we convert to a function!
    scheduled = true;
}

/**
 * Not in the setup yet.
 */
void SpeedOctaveParameterType::getValue(SetupTrack* t, ExValue* value)
{
    //value->setInt(t->getSpeedOctave());
}

void SpeedOctaveParameterType::setValue(SetupTrack* t, ExValue* value)
{
    //t->setSpeedOctave(value->getInt());
}

void SpeedOctaveParameterType::getValue(Track* t, ExValue* value)
{
    value->setInt(t->getSpeedOctave());
}

void SpeedOctaveParameterType::setValue(Action* action)
{
    doFunction(action, SpeedOctave);
}

int SpeedOctaveParameterType::getOrdinalValue(Track* t)
{
    return t->getSpeedOctave();
}

PUBLIC Parameter* SpeedOctaveParameter = new SpeedOctaveParameterType();

//////////////////////////////////////////////////////////////////////
//
// SpeedStep
//
//////////////////////////////////////////////////////////////////////

class SpeedStepParameterType : public TrackParameter
{
  public:
	SpeedStepParameterType();
	void getValue(SetupTrack* t, ExValue* value);
	void setValue(SetupTrack* t, ExValue* value);
	void getValue(Track* t, ExValue* value);
    void setValue(Action* action);
    int getOrdinalValue(Track* t);
};

/**
 * The range is configurable for the SpeedShift spread function
 * but mostly so that we don't claim notes that we could use
 * for something else.  The parameter doesn't have that problem
 * as it is bound to a single CC.  We could assume a full CC 
 * range of 64 down and 63 up, but we've been defaulting
 * to a 48 step up and down for so long, let's keep that so
 * if someone binds a CC to this parameter or to the SpeedShift
 * function they behave the same.  I don't think we need
 * to configure a range here but it would make a pedal less
 * twitchy and easier to control.
 */
SpeedStepParameterType::SpeedStepParameterType() :
    TrackParameter("speedStep", MSG_PARAM_SPEED_STEP)
{
    bindable = true;
    control = true;
	type = TYPE_INT;
    low = -MAX_RATE_STEP;
	high = MAX_RATE_STEP;
    zeroCenter = true;
    resettable = true;
    // we convert to a function!
    scheduled = true;
}

/**
 * Not in the setup yet.
 */
void SpeedStepParameterType::getValue(SetupTrack* t, ExValue* value)
{
    //value->setInt(t->getSpeedStep());
}

void SpeedStepParameterType::setValue(SetupTrack* t, ExValue* value)
{
    //t->setSpeedStep(value->getInt());
}

void SpeedStepParameterType::getValue(Track* t, ExValue* value)
{
    value->setInt(t->getSpeedStep());
}

void SpeedStepParameterType::setValue(Action* action)
{
    doFunction(action, SpeedStep);
}

int SpeedStepParameterType::getOrdinalValue(Track* t)
{
    return t->getSpeedStep();
}

PUBLIC Parameter* SpeedStepParameter = new SpeedStepParameterType();

//////////////////////////////////////////////////////////////////////
//
// SpeedBend
//
//////////////////////////////////////////////////////////////////////

class SpeedBendParameterType : public TrackParameter
{
  public:
	SpeedBendParameterType();
	void getValue(SetupTrack* t, ExValue* value);
	void setValue(SetupTrack* t, ExValue* value);
	void getValue(Track* t, ExValue* value);
	void setValue(Action* action);
    int getOrdinalValue(Track* t);
};

SpeedBendParameterType::SpeedBendParameterType() :
    TrackParameter("speedBend", MSG_PARAM_SPEED_BEND)
{
    bindable = true;
    control = true;
	type = TYPE_INT;
    low = MIN_RATE_BEND;
    high = MAX_RATE_BEND;
    zeroCenter = true;
    resettable = true;
    scheduled = true;
}

/**
 * Not in the setup yet.
 */
void SpeedBendParameterType::getValue(SetupTrack* t, ExValue* value)
{
    //value->setInt(t->getSpeedBend());
}

void SpeedBendParameterType::setValue(SetupTrack* t, ExValue* value)
{
    //t->setSpeedBend(value->getInt());
}

void SpeedBendParameterType::getValue(Track* t, ExValue* value)
{
    value->setInt(t->getSpeedBend());
}

void SpeedBendParameterType::setValue(Action* action)
{
    doFunction(action, SpeedBend);
}

int SpeedBendParameterType::getOrdinalValue(Track* t)
{
    return t->getSpeedBend();
}

PUBLIC Parameter* SpeedBendParameter = new SpeedBendParameterType();

//////////////////////////////////////////////////////////////////////
//
// PitchOctave
//
//////////////////////////////////////////////////////////////////////

/**
 * Not currently exposed.
 */
class PitchOctaveParameterType : public TrackParameter
{
  public:
	PitchOctaveParameterType();
	void getValue(SetupTrack* t, ExValue* value);
	void setValue(SetupTrack* t, ExValue* value);
	void getValue(Track* t, ExValue* value);
	void setValue(Action* action);
    int getOrdinalValue(Track* t);
};

PitchOctaveParameterType::PitchOctaveParameterType() :
    TrackParameter("pitchOctave", MSG_PARAM_PITCH_OCTAVE)
{
    bindable = true;
    control = true;
	type = TYPE_INT;
    // this doesn't have the same buffer issues as speed shift
    // (actually it may inside the pitch plugin?) but make them
    // the same for consistency
    high = MAX_RATE_OCTAVE;
    low = -MAX_RATE_OCTAVE;
    zeroCenter = true;
    resettable = true;
    scheduled = true;
}

/**
 * Not in the setup yet.
 */
void PitchOctaveParameterType::getValue(SetupTrack* t, ExValue* value)
{
    //value->setInt(t->getPitchOctave());
}

void PitchOctaveParameterType::setValue(SetupTrack* t, ExValue* value)
{
    //t->setPitchOctave(value->getInt());
}

void PitchOctaveParameterType::getValue(Track* t, ExValue* value)
{
    value->setInt(t->getPitchOctave());
}

void PitchOctaveParameterType::setValue(Action* action)
{
    doFunction(action, PitchOctave);
}

int PitchOctaveParameterType::getOrdinalValue(Track* t)
{
    return t->getPitchOctave();
}

PUBLIC Parameter* PitchOctaveParameter = new PitchOctaveParameterType();

//////////////////////////////////////////////////////////////////////
//
// PitchStep
//
//////////////////////////////////////////////////////////////////////

class PitchStepParameterType : public TrackParameter
{
  public:
	PitchStepParameterType();
	void getValue(SetupTrack* t, ExValue* value);
	void setValue(SetupTrack* t, ExValue* value);
	void getValue(Track* t, ExValue* value);
	void setValue(Action* action);
    int getOrdinalValue(Track* t);
};

/**
 * The range is configurable for the PitchShift spread function
 * but mostly so that we don't claim notes that we could use
 * for something else.  The parameter doesn't have that problem
 * as it is bound to a single CC.  We could assume a full CC 
 * range of 64 down and 63 up, but we've been defaulting
 * to a 48 step up and down for so long, let's keep that so
 * if someone binds a CC to this parameter or to the PitchShift
 * function they behave the same.  I don't think we need
 * to configure a range here but it would make a pedal less
 * twitchy and easier to control.
 */
PitchStepParameterType::PitchStepParameterType() :
    TrackParameter("pitchStep", MSG_PARAM_PITCH_STEP)
{
    bindable = true;
    control = true;
	type = TYPE_INT;
    low = -MAX_RATE_STEP;
	high = MAX_RATE_STEP;
    zeroCenter = true;
    resettable = true;
    scheduled = true;
}

/**
 * Not in the setup yet.
 */
void PitchStepParameterType::getValue(SetupTrack* t, ExValue* value)
{
    //value->setInt(t->getPitchStep());
}

void PitchStepParameterType::setValue(SetupTrack* t, ExValue* value)
{
    //t->setPitchStep(value->getInt());
}

void PitchStepParameterType::getValue(Track* t, ExValue* value)
{
    value->setInt(t->getPitchStep());
}

void PitchStepParameterType::setValue(Action* action)
{
    doFunction(action, PitchStep);
}

int PitchStepParameterType::getOrdinalValue(Track* t)
{
    return t->getPitchStep();
}

PUBLIC Parameter* PitchStepParameter = new PitchStepParameterType();

//////////////////////////////////////////////////////////////////////
//
// PitchBend
//
//////////////////////////////////////////////////////////////////////

class PitchBendParameterType : public TrackParameter
{
  public:
	PitchBendParameterType();
	void getValue(SetupTrack* t, ExValue* value);
	void setValue(SetupTrack* t, ExValue* value);
	void getValue(Track* t, ExValue* value);
	void setValue(Action* action);
    int getOrdinalValue(Track* t);
};

PitchBendParameterType::PitchBendParameterType() :
    TrackParameter("pitchBend", MSG_PARAM_PITCH_BEND)
{
    bindable = true;
    control = true;
	type = TYPE_INT;
    low = MIN_RATE_BEND;
    high = MAX_RATE_BEND;
    zeroCenter = true;
    resettable = true;
    scheduled = true;
}

/**
 * Not in the setup yet.
 */
void PitchBendParameterType::getValue(SetupTrack* t, ExValue* value)
{
    //value->setInt(t->getPitchBend());
}

void PitchBendParameterType::setValue(SetupTrack* t, ExValue* value)
{
    //t->setPitchBend(value->getInt());
}

void PitchBendParameterType::getValue(Track* t, ExValue* value)
{
    value->setInt(t->getPitchBend());
}

void PitchBendParameterType::setValue(Action* action)
{
    doFunction(action, PitchBend);
}

int PitchBendParameterType::getOrdinalValue(Track* t)
{
    return t->getPitchBend();
}

PUBLIC Parameter* PitchBendParameter = new PitchBendParameterType();

//////////////////////////////////////////////////////////////////////
//
// TimeStretch
//
//////////////////////////////////////////////////////////////////////

class TimeStretchParameterType : public TrackParameter
{
  public:
	TimeStretchParameterType();
	void getValue(SetupTrack* t, ExValue* value);
	void setValue(SetupTrack* t, ExValue* value);
	void getValue(Track* t, ExValue* value);
	void setValue(Action* action);
    int getOrdinalValue(Track* t);
};

TimeStretchParameterType::TimeStretchParameterType() :
    TrackParameter("timeStretch", MSG_PARAM_TIME_STRETCH)
{
    bindable = true;
    control = true;
	type = TYPE_INT;
    low = MIN_RATE_BEND;
    high = MAX_RATE_BEND;
    zeroCenter = true;
    resettable = true;
    scheduled = true;
}

/**
 * Not in the setup yet.
 */
void TimeStretchParameterType::getValue(SetupTrack* t, ExValue* value)
{
    //value->setInt(t->getTimeStretch());
}

void TimeStretchParameterType::setValue(SetupTrack* t, ExValue* value)
{
    //t->setTimeStretch(value->getInt());
}

void TimeStretchParameterType::getValue(Track* t, ExValue* value)
{
    value->setInt(t->getTimeStretch());
}

/**
 * Time stretch alters speed which alters latency so it has to be scheduled.
 * Events are designed around functions so we have to pass this
 * off to TimeStretchFunction even though we don't expose that in the UI.
 */
void TimeStretchParameterType::setValue(Action* action)
{
    doFunction(action, TimeStretch);
}

int TimeStretchParameterType::getOrdinalValue(Track* t)
{
    return t->getTimeStretch();
}

PUBLIC Parameter* TimeStretchParameter = new TimeStretchParameterType();

//////////////////////////////////////////////////////////////////////
//
// TrackPreset
//
//////////////////////////////////////////////////////////////////////

class TrackPresetParameterType : public TrackParameter
{
  public:
	TrackPresetParameterType();
	void getValue(SetupTrack* t, ExValue* value);
	void setValue(SetupTrack* t, ExValue* value);
    int getOrdinalValue(Track* t);
	void getValue(Track* t, ExValue* value);
	void setValue(Action* action);

	int getHigh(MobiusInterface* m);
	void getOrdinalLabel(MobiusInterface* m, int i, ExValue* value);

};

PUBLIC Parameter* TrackPresetParameter = new TrackPresetParameterType();

TrackPresetParameterType::TrackPresetParameterType() :
    // this must match the TargetPreset name
    TrackParameter("preset", MSG_PARAM_TRACK_PRESET)
{
    bindable = true;
	type = TYPE_STRING;
    resettable = true;
	dynamic = true;
}

void TrackPresetParameterType::getValue(SetupTrack* t, ExValue* value)
{
    value->setString(t->getPreset());
}

void TrackPresetParameterType::setValue(SetupTrack* t, ExValue* value)
{
    // since we intend this for parsing and editing should always
    // have a string, harder to support ordinals here because
    // we don't have a handle to Mobius
    t->setPreset(value->getString());
}

int TrackPresetParameterType::getOrdinalValue(Track* t)
{
    Preset* p = t->getPreset();
    return p->getNumber();
}

void TrackPresetParameterType::getValue(Track* t, ExValue* value)
{
    const char* name = NULL;

    // You usually want the string for display.
    // Unfortunately the private track preset did not copy
    // the name to avoid memory allocation so we have to go back
    // to the MobiusConfig.  Note also that Track::mPreset came 
    // from the interrupt config but for the outside view of the
    // parameter we need to use the master config.  This can result
    // in a small window of inconsistency if we're in the middle of 
    // shifting a new configuration down.  Since this is only used for the UI
    // it should correct itself quickly.

    MobiusConfig* iconfig = t->getMobius()->getConfiguration();
    Preset* p = t->getPreset();
    p = iconfig->getPreset(p->getNumber());
    if (p != NULL) 
      name = p->getName();
    else {
        // should only happen if we're shifting down a new config object
        // and one or more of the presets were deleted
        Trace(1, "ERROR: TrackPresetParameter: Unable to determine preset name\n");
    }

	value->setString(name);
}

/**
 * This is one of the unusual ones that overloads the Action signature
 * so we can get information about the trigger.
 */
void TrackPresetParameterType::setValue(Action* action)
{
	// accept either a name or index
	Mobius* m = action->mobius;
	MobiusConfig* config = m->getConfiguration();

	// value may be string or int, ints are used in the
	// ParameterDisplay component 
	Preset* preset = NULL;
	if (action->arg.getType() == EX_INT)
	  preset = config->getPreset(action->arg.getInt());
	else 
	  preset = config->getPreset(action->arg.getString());

	if (preset != NULL) {
        Track* t = action->getResolvedTrack();

        if (action->trigger != TriggerScript) {
            // !! assume this has to be pending for safety, though
            // we'll always be in a script?
            // We should be doing this with Actions now rather than
            // yet another type of pending
            t->setPendingPreset(preset->getNumber());
        }
        else {
            // do it immediately so the reset of the script sees it
            // !! should be getting this from the interrupt config?
            t->setPreset(preset->getNumber());
        }
	}
}

/**
 * !! The max can change as presets are added/removed.
 * Need to work out a way to convey that to ParameterEditor.
 */
PUBLIC int TrackPresetParameterType::getHigh(MobiusInterface* m)
{
	MobiusConfig* config = m->getConfiguration();
    int max = config->getPresetCount();
    // this is the number of presets, the max ordinal is zero based
    max--;
    return max;
}

/**
 * Given an ordinal, map it into a display label.
 */
PUBLIC void TrackPresetParameterType::getOrdinalLabel(MobiusInterface* m,
													  int i, ExValue* value)
{
	MobiusConfig* config = m->getConfiguration();
	Preset* preset = config->getPreset(i);
	if (preset != NULL)
	  value->setString(preset->getName());
	else
	  value->setString("???");
}

//////////////////////////////////////////////////////////////////////
//
// TrackPresetNumber
//
//////////////////////////////////////////////////////////////////////

/**
 * Provided so scripts can deal with presets as numbers if necessary
 * though I would expect usually they will be referenced using names.
 *
 * !! NOTE: We have historically returned the zero based preset
 * ordinal number here.  This is unlike the way wa number tracks and
 * loops from 1.  I don't like the inconsistency, would be better to 
 * follow a consistent indexing scheme but I'm afraid of breaking something.
 *
 */
class TrackPresetNumberParameterType : public TrackParameter
{
  public:
	TrackPresetNumberParameterType();
	void getValue(SetupTrack* t, ExValue* value);
	void setValue(SetupTrack* t, ExValue* value);
	void getValue(Track* t, ExValue* value);
	void setValue(Action* action);
    int getOrdinalValue(Track* t);
};

PUBLIC Parameter* TrackPresetNumberParameter = 
new TrackPresetNumberParameterType();

TrackPresetNumberParameterType::TrackPresetNumberParameterType() :
    TrackParameter("presetNumber", MSG_PARAM_TRACK_PRESET_NUMBER)
{
    // not bindable
	type = TYPE_INT;
    // not in the XML
    transient = true;
    // dynmic means it can change after the UI is initialized
	dynamic = true;
}

void TrackPresetNumberParameterType::getValue(SetupTrack* t, ExValue* value)
{
    // should not be calling this
    Trace(1, "TrackPresetNumberParameterType::getValue!\n");
}

void TrackPresetNumberParameterType::setValue(SetupTrack* t, ExValue* value)
{
    // should not be calling this
    Trace(1, "TrackPresetNumberParameterType::getValue!\n");
}

void TrackPresetNumberParameterType::getValue(Track* t, ExValue* value)
{
	value->setInt(t->getPreset()->getNumber());
}

void TrackPresetNumberParameterType::setValue(Action* action)
{
	Mobius* m = action->mobius;
	MobiusConfig* config = m->getConfiguration();
    int index = action->arg.getInt();
	Preset* preset = config->getPreset(index);

	if (preset != NULL) {
        Track* t = action->getResolvedTrack();
        if (action->trigger != TriggerScript) {
            // !! assume this has to be pending for safety, though
            // we'll always be in a script?
            // Should be doing this with deferred Actions now
            t->setPendingPreset(index);
        }
        else {
            t->setPreset(index);
        }
	}
}

int TrackPresetNumberParameterType::getOrdinalValue(Track* t)
{
    ExValue v;
    getValue(t, &v);
    return v.getInt();
}

//////////////////////////////////////////////////////////////////////
//
// SyncSource
//
//////////////////////////////////////////////////////////////////////

class SyncSourceParameterType : public TrackParameter
{
  public:
	SyncSourceParameterType();
	void getValue(SetupTrack* s, ExValue* value);
	void setValue(SetupTrack* s, ExValue* value);
    int getOrdinalValue(Track* t);
	void getOrdinalLabel(MobiusInterface* m, int i, ExValue* value);
    void getValue(Track* t, ExValue* value);
    void setValue(Track* t, ExValue* value);
};

const char* SYNC_SOURCE_NAMES[] = {
	"default", "none", "track", "out", "host", "midi", NULL
};

int SYNC_SOURCE_KEYS[] = {
	MSG_VALUE_SYNC_SOURCE_DEFAULT,
	MSG_VALUE_SYNC_SOURCE_NONE,
	MSG_VALUE_SYNC_SOURCE_TRACK,
	MSG_VALUE_SYNC_SOURCE_OUT,
	MSG_VALUE_SYNC_SOURCE_HOST,
	MSG_VALUE_SYNC_SOURCE_MIDI,
	0
};

SyncSourceParameterType::SyncSourceParameterType() :
    TrackParameter("syncSource", MSG_PARAM_SYNC_SOURCE)
{
    bindable = true;
	type = TYPE_ENUM;
	values = SYNC_SOURCE_NAMES;
	valueKeys = SYNC_SOURCE_KEYS;
}

void SyncSourceParameterType::getValue(SetupTrack* s, ExValue* value)
{
    value->setString(values[(int)s->getSyncSource()]);
}

void SyncSourceParameterType::setValue(SetupTrack* s, ExValue* value)
{
	s->setSyncSource((SyncSource)getEnum(value));
}

/**
 * Direct accessors actually just forward it to the SetupTrack.
 * SyncState will go back to the SetupTrack until it is locked after
 * which it won't change.
 *
 * Note that you can't get the effective sync source from here,
 * if we need that then it should be a variable.
 */
int SyncSourceParameterType::getOrdinalValue(Track* t)
{
    int value = 0;
    SetupTrack* st = t->getSetup();
    if (st != NULL)
      value = (int)st->getSyncSource();
    return value;
}

/**
 * Direct accessors actually just forward it to the SetupTrack.
 * SyncState will go back to the SetupTrack until it is locked after
 * which it won't change.
 *
 * Note that you can't get the effective sync source from here,
 * if we need that then it should be a variable.
 */
void SyncSourceParameterType::getValue(Track* t, ExValue* value)
{
    SetupTrack* st = t->getSetup();
    if (st != NULL)
      getValue(st, value);
    else
      value->setString("default");
}

void SyncSourceParameterType::setValue(Track* t, ExValue* value)
{
    SetupTrack* st = t->getSetup();
    if (st != NULL)
      setValue(st, value);
}

/**
 * Given an ordinal, map it into a display label.
 * If the value is "default", we qualify it to show what the
 * default mode is.
 */
PUBLIC void SyncSourceParameterType::getOrdinalLabel(MobiusInterface* m,
                                                     int i, ExValue* value)
{
    // should always have these
	if (valueLabels == NULL)
      value->setInt(i);
    else {
		value->setString(valueLabels[i]);
        if (i == 0) {
            // add a qualifier
            // Actually the qualifer makes this rather long so don't
            // bother showing "Default", just wrap it
            value->setString("(");
            ExValue v2;
            DefaultSyncSourceParameter->getDisplayValue(m, &v2);
            value->addString(v2.getString());
            value->addString(")");
        }
	}
}


PUBLIC Parameter* SyncSourceParameter = 
new SyncSourceParameterType();

//////////////////////////////////////////////////////////////////////
//
// TrackSyncUnit
//
//////////////////////////////////////////////////////////////////////

class TrackSyncUnitParameterType : public TrackParameter
{
  public:
	TrackSyncUnitParameterType();
	void getValue(SetupTrack* s, ExValue* value);
	void setValue(SetupTrack* s, ExValue* value);
    int getOrdinalValue(Track* t);
	void getOrdinalLabel(MobiusInterface* m, int i, ExValue* value);
    void getValue(Track* t, ExValue* value);
    void setValue(Track* t, ExValue* value);
};

const char* TRACK_SYNC_UNIT_NAMES[] = {
	"default", "subcycle", "cycle", "loop", NULL
};

int TRACK_SYNC_UNIT_KEYS[] = {
	MSG_VALUE_TRACK_UNIT_DEFAULT,
	MSG_VALUE_TRACK_UNIT_SUBCYCLE,
	MSG_VALUE_TRACK_UNIT_CYCLE,
	MSG_VALUE_TRACK_UNIT_LOOP,
	0
};

TrackSyncUnitParameterType::TrackSyncUnitParameterType() :
    TrackParameter("trackSyncUnit", MSG_PARAM_TRACK_SYNC_UNIT)
{
    bindable = true;
	type = TYPE_ENUM;
	values = TRACK_SYNC_UNIT_NAMES;
	valueKeys = TRACK_SYNC_UNIT_KEYS;
}

void TrackSyncUnitParameterType::getValue(SetupTrack* s, ExValue* value)
{
	value->setString(values[(int)s->getSyncTrackUnit()]);
}

void TrackSyncUnitParameterType::setValue(SetupTrack* s, ExValue* value)
{
	s->setSyncTrackUnit((SyncTrackUnit)getEnum(value));
}

/**
 * Direct accessors actually just forward it to the SetupTrack.
 * SyncState will go back to the SetupTrack until it is locked after
 * which it won't change.
 */
int TrackSyncUnitParameterType::getOrdinalValue(Track* t)
{
    int value = 0;
    SetupTrack* st = t->getSetup();
    if (st != NULL)
      value = (int)st->getSyncTrackUnit();
    return value;
}

/**
 * Direct accessors actually just forward it to the SetupTrack.
 * SyncState will go back to the SetupTrack until it is locked after
 * which it won't change.
 */
void TrackSyncUnitParameterType::getValue(Track* t, ExValue* value)
{
    SetupTrack* st = t->getSetup();
    if (st != NULL)
      getValue(st, value);
    else
      value->setString("default");
}

void TrackSyncUnitParameterType::setValue(Track* t, ExValue* value)
{
    SetupTrack* st = t->getSetup();
    if (st != NULL)
      setValue(st, value);
}

/**
 * Given an ordinal, map it into a display label.
 * If the value is "default", we qualify it to show what the
 * default mode is.
 */
PUBLIC void TrackSyncUnitParameterType::getOrdinalLabel(MobiusInterface* m,
                                                        int i, ExValue* value)
{
    // should always have these
	if (valueLabels == NULL)
      value->setInt(i);
    else {
		value->setString(valueLabels[i]);
        if (i == 0) {
            // add a qualifier
            // Actually the qualifer makes this rather long so don't
            // bother showing "Default", just wrap it
            value->setString("(");
            ExValue v2;
            DefaultTrackSyncUnitParameter->getDisplayValue(m, &v2);
            value->addString(v2.getString());
            value->addString(")");
        }
	}
}

PUBLIC Parameter* TrackSyncUnitParameter = 
new TrackSyncUnitParameterType();

//////////////////////////////////////////////////////////////////////
//
// AudioInputPort
//
//////////////////////////////////////////////////////////////////////

/**
 * Note that this is not bindable, for bindings and export
 * you must use InputPort which merges AudioInputPort
 * and PluginInputPort.
 *
 * When used from a script, it behaves the same as InputPort.
 */
class AudioInputPortParameterType : public TrackParameter
{
  public:
	AudioInputPortParameterType();
	int getHigh(MobiusInterface* m);
	void getValue(SetupTrack* t, ExValue* value);
	void setValue(SetupTrack* t, ExValue* value);
    int getOrdinalValue(Track* t);
    void getOrdinalLabel(MobiusInterface* m, int i, ExValue* value);
	void getValue(Track* t, ExValue* value);
	void setValue(Track* t, ExValue* value);
};

AudioInputPortParameterType::AudioInputPortParameterType() :
    TrackParameter("audioInputPort", MSG_PARAM_AUDIO_INPUT_PORT)
{
    // not bindable
	type = TYPE_INT;
    low = 1;
    high = 64;

    // rare case of an xmlAlias since we have a new parameter 
    // with the old name
    xmlAlias = "inputPort";
}

int AudioInputPortParameterType::getHigh(MobiusInterface* m)
{
    AudioStream* stream = m->getAudioStream();
    return stream->getInputPorts();
}

void AudioInputPortParameterType::getValue(SetupTrack* t, ExValue* value)
{
    value->setInt(t->getAudioInputPort());
}

void AudioInputPortParameterType::setValue(SetupTrack* t, ExValue* value)
{
    t->setAudioInputPort(value->getInt());
}

int AudioInputPortParameterType::getOrdinalValue(Track* t)
{
    return t->getInputPort();
}

/**
 * These are zero based but we want to display them 1 based.
 */
PUBLIC void AudioInputPortParameterType::getOrdinalLabel(MobiusInterface* m,
                                                    int i, ExValue* value)
{
    value->setInt(i + 1);
}

void AudioInputPortParameterType::getValue(Track* t, ExValue* value)
{
    value->setInt(t->getInputPort());
}

void AudioInputPortParameterType::setValue(Track* t, ExValue* value)
{
    // can you just set these like this?
    // Track will need to do some cross fading
    t->setInputPort(value->getInt());
}

PUBLIC Parameter* AudioInputPortParameter = new AudioInputPortParameterType();

//////////////////////////////////////////////////////////////////////
//
// AudioOutputPort
//
//////////////////////////////////////////////////////////////////////

/**
 * Note that this is not bindable, for bindings and export
 * you must use OutputPort which merges AudioOutputPort
 * and PluginOutputPort.
 *
 * When used from a script, it behaves the same as OutputPort.
 */
class AudioOutputPortParameterType : public TrackParameter
{
  public:
	AudioOutputPortParameterType();
	int getHigh(MobiusInterface* m);
	void getValue(SetupTrack* t, ExValue* value);
	void setValue(SetupTrack* t, ExValue* value);
    int getOrdinalValue(Track* t);
    void getOrdinalLabel(MobiusInterface* m, int i, ExValue* value);
	void getValue(Track* t, ExValue* value);
	void setValue(Track* t, ExValue* value);
};

AudioOutputPortParameterType::AudioOutputPortParameterType() :
    TrackParameter("audioOutputPort", MSG_PARAM_AUDIO_OUTPUT_PORT)
{
    // not bindable
	type = TYPE_INT;
    low = 1;
    high = 64;

    // rare case of an xmlAlias since we have a new parameter 
    // with the old name
    xmlAlias = "outputPort";
}

int AudioOutputPortParameterType::getHigh(MobiusInterface* m)
{
    AudioStream* stream = m->getAudioStream();
    return stream->getOutputPorts();
}

void AudioOutputPortParameterType::getValue(SetupTrack* t, ExValue* value)
{
    value->setInt(t->getAudioOutputPort());
}

void AudioOutputPortParameterType::setValue(SetupTrack* t, ExValue* value)
{
    t->setAudioOutputPort(value->getInt());
}

int AudioOutputPortParameterType::getOrdinalValue(Track* t)
{
    return t->getOutputPort();
}

/**
 * These are zero based but we want to display them 1 based.
 */
PUBLIC void AudioOutputPortParameterType::getOrdinalLabel(MobiusInterface* m,
                                                     int i, ExValue* value)
{
    value->setInt(i + 1);
}

void AudioOutputPortParameterType::getValue(Track* t, ExValue* value)
{
    value->setInt(t->getOutputPort());
}

void AudioOutputPortParameterType::setValue(Track* t, ExValue* value)
{
    // can you just set these like this?
    // Track will need to do some cross fading
    t->setOutputPort(value->getInt());
}

PUBLIC Parameter* AudioOutputPortParameter = new AudioOutputPortParameterType();

//////////////////////////////////////////////////////////////////////
//
// PluginInputPort
//
//////////////////////////////////////////////////////////////////////

/**
 * This is only used when editing the setup, it is not bindable
 * or usable from a script.  From scripts it behaves the same
 * as InputPort and TrackInputPort.
 */
class PluginInputPortParameterType : public TrackParameter
{
  public:
	PluginInputPortParameterType();
	int getHigh(MobiusInterface* m);
	void getValue(SetupTrack* t, ExValue* value);
	void setValue(SetupTrack* t, ExValue* value);
    int getOrdinalValue(Track* t);
    void getOrdinalLabel(MobiusInterface* m, int i, ExValue* value);
	void getValue(Track* t, ExValue* value);
	void setValue(Track* t, ExValue* value);
};

PluginInputPortParameterType::PluginInputPortParameterType() :
    TrackParameter("pluginInputPort", MSG_PARAM_PLUGIN_INPUT_PORT)
{
    // not bindable
	type = TYPE_INT;
    low = 1;
    high = 64;
    addAlias("vstInputPort");
}

int PluginInputPortParameterType::getHigh(MobiusInterface* m)
{
    MobiusConfig* config = m->getConfiguration();
    return config->getPluginPorts();
}

void PluginInputPortParameterType::getValue(SetupTrack* t, ExValue* value)
{
    value->setInt(t->getPluginInputPort());
}

void PluginInputPortParameterType::setValue(SetupTrack* t, ExValue* value)
{
    t->setPluginInputPort(value->getInt());
}

// When running this is the same as InputPortParameterType

int PluginInputPortParameterType::getOrdinalValue(Track* t)
{
    return t->getInputPort();
}

/**
 * These are zero based but we want to display them 1 based.
 */
PUBLIC void PluginInputPortParameterType::getOrdinalLabel(MobiusInterface* m,
                                                          int i, ExValue* value)
{
    value->setInt(i + 1);
}

void PluginInputPortParameterType::getValue(Track* t, ExValue* value)
{
    value->setInt(t->getInputPort());
}

void PluginInputPortParameterType::setValue(Track* t, ExValue* value)
{
    // can you just set these like this?
    // Track will need to do some cross fading
    t->setInputPort(value->getInt());
}

PUBLIC Parameter* PluginInputPortParameter = new PluginInputPortParameterType();

//////////////////////////////////////////////////////////////////////
//
// PluginOutputPort
//
//////////////////////////////////////////////////////////////////////

/**
 * This is used only for setup editing, it is not bindable.
 * If used from a script it behave the same as OutputPort
 * and TrackOutputPort.
 */
class PluginOutputPortParameterType : public TrackParameter
{
  public:
	PluginOutputPortParameterType();
	int getHigh(MobiusInterface* m);
	void getValue(SetupTrack* t, ExValue* value);
	void setValue(SetupTrack* t, ExValue* value);
    int getOrdinalValue(Track* t);
    void getOrdinalLabel(MobiusInterface* m, int i, ExValue* value);
	void getValue(Track* t, ExValue* value);
	void setValue(Track* t, ExValue* value);
};

PluginOutputPortParameterType::PluginOutputPortParameterType() :
    TrackParameter("pluginOutputPort", MSG_PARAM_PLUGIN_OUTPUT_PORT)
{
    // not bindable
	type = TYPE_INT;
    low = 1;
    high = 64;
    addAlias("vstOutputPort");
}

int PluginOutputPortParameterType::getHigh(MobiusInterface* m)
{
    MobiusConfig* config = m->getConfiguration();
    return config->getPluginPorts();
}

void PluginOutputPortParameterType::getValue(SetupTrack* t, ExValue* value)
{
    value->setInt(t->getPluginOutputPort());
}

void PluginOutputPortParameterType::setValue(SetupTrack* t, ExValue* value)
{
    t->setPluginOutputPort(value->getInt());
}

// When running, this is the same as OutputPortParameterType

int PluginOutputPortParameterType::getOrdinalValue(Track* t)
{
    return t->getOutputPort();
}

/**
 * These are zero based but we want to display them 1 based.
 */
PUBLIC void PluginOutputPortParameterType::getOrdinalLabel(MobiusInterface* m,
                                                           int i, ExValue* value)
{
    value->setInt(i + 1);
}

void PluginOutputPortParameterType::getValue(Track* t, ExValue* value)
{
    value->setInt(t->getOutputPort());
}

void PluginOutputPortParameterType::setValue(Track* t, ExValue* value)
{
    // can you just set these like this?
    // Track will need to do some cross fading
    t->setOutputPort(value->getInt());
}

PUBLIC Parameter* PluginOutputPortParameter = 
new PluginOutputPortParameterType();

//////////////////////////////////////////////////////////////////////
//
// InputPort
//
//////////////////////////////////////////////////////////////////////

/**
 * This is the bindable parameter that displays and sets the 
 * port being used by this track, which may either be an audio
 * device port or a plugin port.
 *
 * At runtime it behaves the same as AudioInputPort and PluginInputPort
 * the difference is that getHigh can return two different
 * values depending in how we are being run.
 */
class InputPortParameterType : public TrackParameter
{
  public:
	InputPortParameterType();
	int getHigh(MobiusInterface* m);
	void getValue(SetupTrack* t, ExValue* value);
	void setValue(SetupTrack* t, ExValue* value);
    int getOrdinalValue(Track* t);
    void getOrdinalLabel(MobiusInterface* m, int i, ExValue* value);
	void getValue(Track* t, ExValue* value);
	void setValue(Track* t, ExValue* value);
};

/**
 * Note we use the same display name as InputPort.
 */
InputPortParameterType::InputPortParameterType() :
    TrackParameter("inputPort", MSG_PARAM_INPUT_PORT)
{
    bindable = true;
	type = TYPE_INT;
    low = 1;
    high = 64;
    // not in the XML
    transient = true;
}

/**
 * This is the reason we have this combo parameter.
 * It has a different upper bound depending on how we're running.
 */
int InputPortParameterType::getHigh(MobiusInterface* m)
{
    int ports = 0;

    MobiusContext* con = m->getContext();
    if (con->isPlugin()) {
        MobiusConfig* config = m->getConfiguration();
        ports = config->getPluginPorts();
    }
    else {
        AudioStream* stream = m->getAudioStream();
        ports = stream->getInputPorts();
    }
    return ports;
}

void InputPortParameterType::getValue(SetupTrack* t, ExValue* value)
{
    // not supposed to be called
    Trace(1, "InputPort::getValue\n");
}

void InputPortParameterType::setValue(SetupTrack* t, ExValue* value)
{
    // not supposed to be called
    Trace(1, "InputPort::setValue\n");
}

// When running this is the same as InputPortParameterType

int InputPortParameterType::getOrdinalValue(Track* t)
{
    return t->getInputPort();
}

/**
 * These are zero based but we want to display them 1 based.
 */
PUBLIC void InputPortParameterType::getOrdinalLabel(MobiusInterface* m,
                                                          int i, ExValue* value)
{
    value->setInt(i + 1);
}

void InputPortParameterType::getValue(Track* t, ExValue* value)
{
    value->setInt(t->getInputPort());
}

void InputPortParameterType::setValue(Track* t, ExValue* value)
{
    // can you just set these like this?
    // Track will need to do some cross fading
    t->setInputPort(value->getInt());
}

PUBLIC Parameter* InputPortParameter = new InputPortParameterType();

//////////////////////////////////////////////////////////////////////
//
// OutputPort
//
//////////////////////////////////////////////////////////////////////

/**
 * This is the bindable parameter that displays and sets the 
 * port being used by this track, which may either be an audio
 * device port or a plugin port.
 *
 * At runtime it behaves the same as AudioOutputPort and PluginOutputPort
 * the difference is that getHigh can return two different
 * values depending in how we are being run.
 */
class OutputPortParameterType : public TrackParameter
{
  public:
	OutputPortParameterType();
	int getHigh(MobiusInterface* m);
	void getValue(SetupTrack* t, ExValue* value);
	void setValue(SetupTrack* t, ExValue* value);
    int getOrdinalValue(Track* t);
    void getOrdinalLabel(MobiusInterface* m, int i, ExValue* value);
	void getValue(Track* t, ExValue* value);
	void setValue(Track* t, ExValue* value);
};

OutputPortParameterType::OutputPortParameterType() :
    TrackParameter("outputPort", MSG_PARAM_OUTPUT_PORT)
{
    bindable = true;
	type = TYPE_INT;
    low = 1;
    high = 64;
    // not in the XML
    transient = true;
}

/**
 * This is the reason we have this combo parameter.
 * It has a different upper bound depending on how we're running.
 */
int OutputPortParameterType::getHigh(MobiusInterface* m)
{
    int ports = 0;

    MobiusContext* con = m->getContext();
    if (con->isPlugin()) {
        MobiusConfig* config = m->getConfiguration();
        ports = config->getPluginPorts();
    }
    else {
        AudioStream* stream = m->getAudioStream();
        ports = stream->getOutputPorts();
    }
    return ports;
}

void OutputPortParameterType::getValue(SetupTrack* t, ExValue* value)
{
    // not supposed to be called
    Trace(1, "OutputPort::getValue\n");
}

void OutputPortParameterType::setValue(SetupTrack* t, ExValue* value)
{
    // not supposed to be called
    Trace(1, "OutputPort::setValue\n");
}

int OutputPortParameterType::getOrdinalValue(Track* t)
{
    return t->getOutputPort();
}

/**
 * These are zero based but we want to display them 1 based.
 */
PUBLIC void OutputPortParameterType::getOrdinalLabel(MobiusInterface* m,
                                                           int i, ExValue* value)
{
    value->setInt(i + 1);
}

void OutputPortParameterType::getValue(Track* t, ExValue* value)
{
    value->setInt(t->getOutputPort());
}

void OutputPortParameterType::setValue(Track* t, ExValue* value)
{
    // can you just set these like this?
    // Track will need to do some cross fading
    t->setOutputPort(value->getInt());
}

PUBLIC Parameter* OutputPortParameter = new OutputPortParameterType();

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
