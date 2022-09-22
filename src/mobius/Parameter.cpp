/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Static object definitions for Mobius parameters.
 *
 * There are four parameter levels:
 *
 *    Global - usually in MobiusConfig
 *    Setup  - in Setup
 *    Track  - in SetupTrack or Track
 *    Preset - in Preset
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
#include "Script.h"
#include "Synchronizer.h"

#include "Parameter.h"

/****************************************************************************
 *                                                                          *
 *   							  PARAMETER                                 *
 *                                                                          *
 ****************************************************************************/

// Shared text for boolean values

const char* BOOLEAN_VALUE_NAMES[] = {
	"off", "on", NULL
};

int BOOLEAN_VALUE_KEYS[] = {
	MSG_VALUE_BOOLEAN_FALSE, MSG_VALUE_BOOLEAN_TRUE, 0
};

const char* BOOLEAN_VALUE_LABELS[] = {
	NULL, NULL, NULL
};

PUBLIC Parameter::Parameter()
{
    init();
}

PUBLIC Parameter::Parameter(const char* name, int key) :
    SystemConstant(name, key)
{
    init();
}

PRIVATE void Parameter::init()
{
	bindable = false;
	dynamic = false;
    deprecated = false;
    transient = false;
    resettable = false;
    scheduled = false;
    takesAction = false;
    control = false;

	type = TYPE_INT;
	scope = PARAM_SCOPE_NONE;
	low = 0;
	high = 0;
    zeroCenter = false;
    mDefault = 0;

	values = NULL;
	valueKeys = NULL;
	valueLabels = NULL;
    xmlAlias = NULL;

	for (int i = 0 ; i < MAX_PARAMETER_ALIAS ; i++)
	  aliases[i] = NULL;
}

PUBLIC Parameter::~Parameter()
{
}

void Parameter::addAlias(const char* alias) 
{
    bool added = false;

	for (int i = 0 ; i < MAX_PARAMETER_ALIAS ; i++) {
        if (aliases[i] == NULL) {
            aliases[i] = alias;
            added = true;
            break;
        }
    }

    if (!added)
      Trace(1, "Alias overflow: %s\n", alias);
}

/**
 * Must be overloaded in the subclass.
 */
PUBLIC void Parameter::getObjectValue(void* object, ExValue* value)
{
    Trace(1, "Parameter %s: getObjectValue not overloaded!\n",
          getName());
}

/**
 * Must be overloaded in the subclass.
 */
PUBLIC void Parameter::setObjectValue(void* object, ExValue* value)
{
    Trace(1, "Parameter %s: setObjectValue not overloaded!\n",
          getName());
}

PUBLIC void Parameter::getValue(Export* exp, ExValue* value)
{
    Trace(1, "Parameter %s: getValue not overloaded!\n",
          getName());
	value->setString("");
}

PUBLIC int Parameter::getOrdinalValue(Export* exp)
{
    Trace(1, "Parameter %s: getOrdinalValue not overloaded! \n",
          getName());
    return -1;
}

PUBLIC void Parameter::setValue(Action* action)
{
    Trace(1, "Parameter %s: setValue not overloaded!\n",
          getName());
}

/**
 * Refresh the cached display names from the message catalog.
 * This overloads the one inherited from SystemConstant so we
 * can avoid warning about hidden and deprecated parameters.
 * Push that down to SysetmConstant?
 *
 * We also handle the localization of the values.
 */
PUBLIC void Parameter::localize(MessageCatalog* cat)
{
    int key = getKey();

	if (key == 0) {
		if (bindable)
		  Trace(1, "No catalog key for parameter %s\n", getName());
		setDisplayName(getName());
	}
	else {
		const char* msg = cat->get(key);
		if (msg != NULL)
		  setDisplayName(msg);
		else {
			Trace(1, "No localization for parameter %s\n", getName());
			setDisplayName(getName());
		}
	}

	if (valueKeys != NULL) {
		// note that these will leak if we don't have something to flush them
		if (valueLabels == NULL) {
			int count = 0;
			while (valueKeys[count] != 0) count++;
			valueLabels = allocLabelArray(count);
		}
		for (int i = 0 ; valueKeys[i] != 0 ; i++) {
			const char* msg = cat->get(valueKeys[i]);
			if (msg != NULL)
			  valueLabels[i] = msg;
			else {
				Trace(1, "No localization for parameter %s value %s\n", 
					  getName(), values[i]);
				if (valueLabels[i] == NULL)
				  valueLabels[i] = values[i];
			}
		}
	}
}

/**
 * Allocate a label array and fill it with nulls.
 */
PRIVATE const char** Parameter::allocLabelArray(int size)
{
	int fullsize = size + 1; // leave a null terminator
	const char** labels = new const char*[fullsize];
	for (int i = 0 ; i < fullsize ; i++)
	  labels[i] = NULL;

	return labels;
}

//////////////////////////////////////////////////////////////////////
//
// Default Ordinal mapping for the UI
// A few classes overload these if they don't have a fixed enumeration.
//
//////////////////////////////////////////////////////////////////////

PUBLIC int Parameter::getLow()
{
    return low;
}

PUBLIC int Parameter::getHigh(MobiusInterface* m)
{
    int max = high;

    if (type == TYPE_BOOLEAN) {
        max = 1;
    }
    else if (valueLabels != NULL) {
        for ( ; valueLabels[max] != NULL ; max++);
        max--;
    }

    return max;
}

PUBLIC int Parameter::getBindingHigh(MobiusInterface* m)
{
    int max = getHigh(m);

    // if an int doesn't have a max, give it something so we can
    // have a reasonable upper bound for CC scaling
    if (type == TYPE_INT && max == 0)
      max = 127;

    return max;
}

/**
 * Given an ordinal, map it into a display label.
 */
PUBLIC void Parameter::getOrdinalLabel(MobiusInterface* m, 
                                       int i, ExValue* value)
{
	if (valueLabels != NULL) {
		value->setString(valueLabels[i]);
	}
	else if (type == TYPE_INT) {
		value->setInt(i);
	}
    else if (type == TYPE_BOOLEAN) {
		value->setString(BOOLEAN_VALUE_LABELS[i]);
	}
    else 
	  value->setInt(i);
}

PUBLIC void Parameter::getDisplayValue(MobiusInterface* m, ExValue* value)
{
    // weird function used in just a few places by
    // things that overload getOrdinalLabel
    value->setNull();
}

//////////////////////////////////////////////////////////////////////
//
// XML
//
//////////////////////////////////////////////////////////////////////

/**
 * Emit the XML attribute for a parameter.
 */
PUBLIC void Parameter::toXml(XmlBuffer* b, void* obj)
{
	ExValue value;
	getObjectValue(obj, &value);

    if (value.getType() == EX_INT) {
        // option to filter zero?
        b->addAttribute(getName(), value.getInt());
    }
    else {
        // any filtering options?
        b->addAttribute(getName(), value.getString());
    }
}

/**
 * There are two aliases we can use here to upgrade XML.
 * If the "aliases" list is set we assume this is an upgrade to
 * both the XML name and the internal parameter name.  
 *
 * If xmlAlias is set this is an upgrade to the XML name only
 * and another parmaeter may be using this internal name.
 * This was added for inputPort and audioInputPort where we're
 * needing to upgrade inputPort to audioInputPort in the Setup
 * but we have another parameter named inputPort that has to 
 * use that name so it can't be an alias of audioInputPort.
 */
PUBLIC void Parameter::parseXml(XmlElement* e, void* obj)
{
    const char* value = e->getAttribute(getName());

    // try the xml alias
    if (value == NULL && xmlAlias != NULL)
      value = e->getAttribute(xmlAlias);

    // try regular aliases
    if (value == NULL) {
        for (int i = 0 ; i < MAX_PARAMETER_ALIAS ; i++) {
            const char* alias = aliases[i];
            if (alias == NULL)
              break;
            else {
                value = e->getAttribute(alias);
                if (value != NULL)
                  break;
            }
        }
    }

    // Only set it if we found a value in the XML, otherwise it has the
    // default from Preset::reset and more importantly may have
    // some upgraded values from older parameters like sampleStyle
    // that won't be in the XML yet.  And if deprecated setting to null
    // can have side effects we don't want.
    if (value != NULL) {
        ExValue v;
        v.setString(value);
        setObjectValue(obj, &v);
    }
}

//////////////////////////////////////////////////////////////////////
//
// coersion utilities
//
//////////////////////////////////////////////////////////////////////

/**
 * Convert a string value to an enumeration ordinal value.
 * This is the one used by most of the code, if the name doesn't match
 * it traces a warning message and returns the first value.
 */
PUBLIC int Parameter::getEnum(const char *value)
{
	int ivalue = getEnumValue(value);

    // if we couldn't find a match, pick the first one
    // !! instead we should leave it at the current value?
    if (ivalue < 0) {
        Trace(1, "ERROR: Invalid value for parameter %s: %s\n",
              getName(), value);
        ivalue = 0;
    }

	return ivalue;
}

/**
 * Convert a string value to an enumeration ordinal value if possible, 
 * return -1 if invalid.  This is like getEnum() but used in cases
 * where the enum is an optional script arg and we need to know
 * whether it really matched or not.
 */
PUBLIC int Parameter::getEnumValue(const char *value)
{
	int ivalue = -1;

	if (value != NULL) {

		for (int i = 0 ; values[i] != NULL ; i++) {
			if (StringEqualNoCase(values[i], value)) {
				ivalue = i;
				break;
			}
		}
        
        if (ivalue < 0) {
            // Try again with prefix matching, it is convenient
            // to allow common abbreviations like "quantize" rather 
            // than "quantized" or "all" rather than "always".  It
            // might be safe to do this all the time but we'd have to 
            // carefully go through all the enums to make sure
            // there are no ambiguities.
            for (int i = 0 ; values[i] != NULL ; i++) {
                if (StartsWithNoCase(values[i], value)) {
                    ivalue = i;
                    break;
                }
            }
        }
	}

	return ivalue;
}

/**
 * Check for an enumeration value that has been changed and convert
 * the old name from the XML or script into the new name.
 */
PUBLIC void Parameter::fixEnum(ExValue* value, const char* oldName, 
                               const char* newName)
{
	if (value->getType() == EX_STRING) {
        const char* current = value->getString();
        if (StringEqualNoCase(oldName, current))
          value->setString(newName);
    }
}

/**
 * Convert a Continuous Controller number in the range of 0-127
 * to an enumerated value.
 * !! this isn't used any more, if we're going to do scaling
 * it needs to be done in a way appropriate for the binding.
 */
PUBLIC int Parameter::getControllerEnum(int value)
{
	int ivalue = 0;

	if (value >= 0 && value < 128) {
		int max = 0;
		for (max = 0 ; values[max] != NULL ; max++);

		int unit = 128 / max;
		ivalue = value / unit;
	}

	return ivalue;
}

/**
 * Coerce an ExValue into an enumeration ordinal.
 * This must NOT scale, it is used in parameter setters
 * and must be symetrical with getOrdinalValue.
 */
PUBLIC int Parameter::getEnum(ExValue *value)
{
	int ivalue = 0;

	if (value->getType() == EX_STRING) {
		// map it through the value table
        ivalue = getEnum(value->getString());
	}
	else {
		// assume it is an ordinal value, but check the range
		// clamp it between 0 and max
		int i = value->getInt();
		if (i >= 0) {
			int max = 0;
			if (values != NULL)
			  for (max = 0 ; values[max] != NULL ; max++);

			if (i < max)
			  ivalue = i;
			else
			  ivalue = max;
		}
	}
	return ivalue;
}

//////////////////////////////////////////////////////////////////////
//
// Parameter Search
//
//////////////////////////////////////////////////////////////////////

PUBLIC Parameter* Parameter::getParameter(Parameter** group, 
										  const char* name)
{
	Parameter* found = NULL;
	
	for (int i = 0 ; group[i] != NULL && found == NULL ; i++) {
		Parameter* p = group[i];
		if (StringEqualNoCase(p->getName(), name))
		  found = p;
	}

	if (found == NULL) {
		// not a name match, try aliases
		for (int i = 0 ; group[i] != NULL && found == NULL ; i++) {
			Parameter* p = group[i];
			for (int j = 0 ; 
				 j < MAX_PARAMETER_ALIAS && p->aliases[j] != NULL ; 
				 j++) {
				if (StringEqualNoCase(p->aliases[j], name)) {
					found = p;
					break;
				}
			}
		}
	}

	return found;
}

PUBLIC Parameter* Parameter::getParameterWithDisplayName(Parameter** group,
														 const char* name)
{
	Parameter* found = NULL;
	
	for (int i = 0 ; group[i] != NULL ; i++) {
		Parameter* p = group[i];
		if (StringEqualNoCase(p->getDisplayName(), name)) {
			found = p;
			break;	
		}
	}
	return found;
}

PUBLIC Parameter* Parameter::getParameter(const char* name)
{
	return getParameter(Parameters, name);
}

PUBLIC Parameter* Parameter::getParameterWithDisplayName(const char* name)
{
	return getParameterWithDisplayName(Parameters, name);
}

/****************************************************************************
 *                                                                          *
 *                             SCRIPT PARAMETERS                            *
 *                                                                          *
 ****************************************************************************/

/****************************************************************************
 *                                                                          *
 *                               PARAMETER LIST                             *
 *                                                                          *
 ****************************************************************************/
/*
 * Can't use a simple static initializer for the Parameters array any more
 * now that they've been broken up into several files.  Have to build
 * the array at runtime.
 */

#define MAX_STATIC_PARAMETERS 256

PUBLIC Parameter* Parameters[MAX_STATIC_PARAMETERS];
PRIVATE int ParameterIndex = 0;

PRIVATE void add(Parameter* p)
{
	if (ParameterIndex >= MAX_STATIC_PARAMETERS - 1) {
		printf("Parameter array overflow!\n");
		fflush(stdout);
	}
	else {
		Parameters[ParameterIndex++] = p;
		// keep it NULL terminated
		Parameters[ParameterIndex] = NULL;
	}
}

/**
 * Called early during Mobius initializations to initialize the 
 * static parameter arrays.  Had to start doing this after splitting
 * the parameters out into several files, they are no longer accessible
 * with static initializers.
 */
PUBLIC void Parameter::initParameters()
{
    // ignore if already initialized
    if (ParameterIndex == 0) {

        // Preset
        add(AltFeedbackEnableParameter);
        add(AutoRecordBarsParameter);
        add(AutoRecordTempoParameter);
        add(BounceQuantizeParameter);
        add(EmptyLoopActionParameter);
        add(EmptyTrackActionParameter);
        add(LoopCountParameter);
        add(MaxRedoParameter);
        add(MaxUndoParameter);
        add(MultiplyModeParameter);
        add(MuteCancelParameter);
        add(MuteModeParameter);
        add(NoFeedbackUndoParameter);
        add(NoLayerFlatteningParameter);
        add(OverdubQuantizedParameter);
        add(OverdubTransferParameter);
        add(PitchBendRangeParameter);
        add(PitchSequenceParameter);
        add(PitchShiftRestartParameter);
        add(PitchStepRangeParameter);
        add(PitchTransferParameter);
        add(QuantizeParameter);
        add(SpeedBendRangeParameter);
        add(SpeedRecordParameter);
        add(SpeedSequenceParameter);
        add(SpeedShiftRestartParameter);
        add(SpeedStepRangeParameter);
        add(SpeedTransferParameter);
        add(TimeStretchRangeParameter);
        add(RecordResetsFeedbackParameter);
        add(RecordThresholdParameter);
        add(RecordTransferParameter);
        add(ReturnLocationParameter);
        add(ReverseTransferParameter);
        add(RoundingOverdubParameter);
        add(ShuffleModeParameter);
        add(SlipModeParameter);
        add(SlipTimeParameter);
        add(SoundCopyParameter);
        add(SubCycleParameter);
        add(SustainFunctionsParameter);
        add(SwitchDurationParameter);
        add(SwitchLocationParameter);
        add(SwitchQuantizeParameter);
        add(SwitchVelocityParameter);
        add(TimeCopyParameter);
        add(TrackLeaveActionParameter);
        add(WindowEdgeAmountParameter);
        add(WindowEdgeUnitParameter);
        add(WindowSlideAmountParameter);
        add(WindowSlideUnitParameter);

        // Deprecated

        add(AutoRecordParameter);
        add(InsertModeParameter);
        add(InterfaceModeParameter);
        add(LoopCopyParameter);
        add(OverdubModeParameter);
        add(RecordModeParameter);
        add(SamplerStyleParameter);
        add(TrackCopyParameter);

        // Track

        add(AltFeedbackLevelParameter);
        add(AudioInputPortParameter);
        add(AudioOutputPortParameter);
        add(FeedbackLevelParameter);
        add(FocusParameter);
        add(GroupParameter);
        add(InputLevelParameter);
        add(InputPortParameter);
        add(MonoParameter);
        add(OutputLevelParameter);
        add(OutputPortParameter);
        add(PanParameter);
        add(PluginInputPortParameter);
        add(PluginOutputPortParameter);

        add(SpeedOctaveParameter);
        add(SpeedBendParameter);
        add(SpeedStepParameter);

        add(PitchOctaveParameter);
        add(PitchBendParameter);
        add(PitchStepParameter);

        add(TimeStretchParameter);

        add(TrackNameParameter);
        add(TrackPresetParameter);
        add(TrackPresetNumberParameter);
        add(TrackSyncUnitParameter);
        add(SyncSourceParameter);

        // Setup

        add(BeatsPerBarParameter);
        add(DefaultSyncSourceParameter);
        add(DefaultTrackSyncUnitParameter);
        add(ManualStartParameter);
        add(MaxTempoParameter);
        add(MinTempoParameter);
        add(MuteSyncModeParameter);
        add(OutRealignModeParameter);
        add(RealignTimeParameter);
        add(ResizeSyncAdjustParameter);
        add(SlaveSyncUnitParameter);
        add(SpeedSyncAdjustParameter);

        // Global

        add(AltFeedbackDisableParameter);
        add(AudioInputParameter);
        add(AudioOutputParameter);
        add(AutoFeedbackReductionParameter);
        add(BindingsParameter);
        add(ConfirmationFunctionsParameter);
        add(CustomMessageFileParameter);
        add(CustomModeParameter);
        add(DriftCheckPointParameter);
        add(DualPluginWindowParameter);
        add(FadeFramesParameter);
        add(FocusLockFunctionsParameter);
        add(GroupFocusLockParameter);
        add(HostMidiExportParameter);
        add(InputLatencyParameter);
        add(IntegerWaveFileParameter);
        add(IsolateOverdubsParameter);
        add(LogStatusParameter);
        add(LongPressParameter);
        add(MaxLoopsParameter);
        add(MaxSyncDriftParameter);
        add(MidiExportParameter);
        add(MidiInputParameter);
        add(MidiOutputParameter);
        add(MidiRecordModeParameter);
        add(MidiThroughParameter);
        add(MonitorAudioParameter);
        add(MuteCancelFunctionsParameter);
        add(NoiseFloorParameter);
        add(OutputLatencyParameter);
        add(OscInputPortParameter);
        add(OscOutputPortParameter);
        add(OscOutputHostParameter);
        add(OscTraceParameter);
        add(OscEnableParameter);
        add(PluginMidiInputParameter);
        add(PluginMidiOutputParameter);
        add(PluginMidiThroughParameter);
        add(PluginPortsParameter);
        add(QuickSaveParameter);
        add(SampleRateParameter);
        add(SaveLayersParameter);
        add(SetupNameParameter);
        add(SetupNumberParameter);
        add(SpreadRangeParameter);
        add(TraceDebugLevelParameter);
        add(TracePrintLevelParameter);
        add(TrackGroupsParameter);
        add(TrackParameter);
        add(TracksParameter);
        add(UnitTestsParameter);

        // sanity check on scopes since they're critical
        for (int i = 0 ; Parameters[i] != NULL ; i++) {
            if (Parameters[i]->scope == PARAM_SCOPE_NONE) {
                Trace(1, "Parameter %s has no scope!\n", 
                      Parameters[i]->getName());
            }
        }
    }
}

/**
 * Refresh the cached display names from the message catalog.
 */
PUBLIC void Parameter::localizeAll(MessageCatalog* cat)
{
	int i;

	for (i = 0 ; Parameters[i] != NULL ; i++)
	  Parameters[i]->localize(cat);

	// these are shared by all
	for (i = 0 ; BOOLEAN_VALUE_NAMES[i] != NULL; i++) {
		const char* msg = cat->get(BOOLEAN_VALUE_KEYS[i]);
		if (msg == NULL)
		  msg = BOOLEAN_VALUE_NAMES[i];
		BOOLEAN_VALUE_LABELS[i] = msg;
	}

    // a good point to run diagnostics
    checkAmbiguousNames();

    // debugging hack
    //    dumpFlags();
}

PRIVATE void Parameter::checkAmbiguousNames()
{
	int i;

	for (i = 0 ; Parameters[i] != NULL ; i++) {
        Parameter* p = Parameters[i];
        const char** values = p->values;
        if (values != NULL) {
            for (int j = 0 ; values[j] != NULL ; j++) {
                Parameter* other = getParameter(values[j]);
                if (other != NULL) {
                    printf("WARNING: Ambiguous parameter name/value %s\n", values[j]);
					fflush(stdout);
                }
            }
        }
    }
}

PRIVATE void Parameter::dumpFlags()
{
    int i;

    printf("*** Bindable ***\n");
	for (i = 0 ; Parameters[i] != NULL ; i++) {
        Parameter* p = Parameters[i];
        if (p->bindable)
          printf("%s\n", p->getName());
    }

    printf("*** Hidden ***\n");
	for (i = 0 ; Parameters[i] != NULL ; i++) {
        Parameter* p = Parameters[i];
        if (!p->bindable)
          printf("%s\n", p->getName());
    }

    printf("*** Deprecated ***\n");
	for (i = 0 ; Parameters[i] != NULL ; i++) {
        Parameter* p = Parameters[i];
        if (p->deprecated)
          printf("%s\n", p->getName());
    }

}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
