/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Definitions for various UI static objects.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "Util.h"
#include "Trace.h"
#include "MessageCatalog.h"

#include "Messages.h"
#include "Binding.h"

#include "UITypes.h"


/****************************************************************************
 *                                                                          *
 *                               UI PARAMETERS                              *
 *                                                                          *
 ****************************************************************************/

// If this has to be known for binding, then the arrays need to be 
// passed into Mobius like UIControls.

PUBLIC UIParameter* MessageDurationParameter = 
    new UIParameter("messageDuration", MSG_UI_PARAM_MESSAGE_DURATION);

/**
 * Since we allocate it here we can initialize it this way.
 * Still...should try to do this like all the other sys constants?
 */
PUBLIC UIParameter* UIParameters[] = {
	MessageDurationParameter,
    NULL
};

/****************************************************************************
 *                                                                          *
 *                                UI CONTROLS                               *
 *                                                                          *
 ****************************************************************************/
/*
 * UIControl comes from Binding.h
 * We could subclass UIControl and put the handlign in the UIControl
 * subclass itself, but since we don't have many of these just switch
 * on them in the MobiusListener method.
 */

UIControl* NextParameterControl = 
    new UIControl("nextParameter", MSG_UI_CMD_NEXT_PARAM);

UIControl* PrevParameterControl = 
    new UIControl("prevParameter", MSG_UI_CMD_PREV_PARAM);

UIControl* IncParameterControl = 
    new UIControl("incParameter", MSG_UI_CMD_INC_PARAM);

UIControl* DecParameterControl = 
    new UIControl("decParameter", MSG_UI_CMD_DEC_PARAM);

UIControl* SpaceDragControl = 
    new UIControl("spaceDrag", MSG_UI_CMD_SPACE_DRAG);

UIControl* UIControls[] = {
	NextParameterControl,
	PrevParameterControl,
	IncParameterControl,
	DecParameterControl,
    SpaceDragControl,
    NULL
};

/****************************************************************************
 *                                                                          *
 *                              DISPLAY ELEMENTS                            *
 *                                                                          *
 ****************************************************************************/

PUBLIC DisplayElement::DisplayElement(const char* iname, int ikey) :
    SystemConstant(iname, ikey)
{
    init();
}

/**
 * Constructor for elements that has a name change.
 */
PUBLIC DisplayElement::DisplayElement(const char* iname, const char* ialias, 
                                      int ikey) :
    SystemConstant(iname, ikey)
{
    init();
    alias = ialias;
}

PUBLIC void DisplayElement::init()
{
    alias = NULL;
}

PUBLIC DisplayElement* DisplayElement::get(const char* name)
{
    DisplayElement* found = NULL;

    if (name != NULL) {
        for (int i = 0 ; AllDisplayElements[i] != NULL ; i++) {
            DisplayElement* el = AllDisplayElements[i];
            // !! not searching on DisplayName like we do for other
            // constants.  Maybe SystemConstant could handle aliases too?
            if (!strcmp(name, el->getName()) ||
                (el->alias != NULL && !strcmp(name, el->alias))) {
                found = el;
                break;
            }
        }
    }
    return found;
}

PUBLIC DisplayElement* DisplayElement::getNoAlias(const char* name)
{
    DisplayElement* found = NULL;

    if (name != NULL) {
        for (int i = 0 ; AllDisplayElements[i] != NULL ; i++) {
            DisplayElement* el = AllDisplayElements[i];
            if (!strcmp(name, el->getName())) {
                found = el;
                break;
            }
        }
    }
    return found;
}

PUBLIC DisplayElement* DisplayElement::getWithDisplayName(DisplayElement** array, 
                                                          const char* name)
{
    DisplayElement* found = NULL;

    if (array != NULL) {
        for (int i = 0 ; array[i] != NULL ; i++) {
            DisplayElement* el = array[i];
            if (!strcmp(name, el->getDisplayName())) {
                found = el;
                break;
            }
        }
    }
    return found;
}

PUBLIC void DisplayElement::localizeAll(MessageCatalog* cat)
{
	for (int i = 0 ; AllDisplayElements[i] != NULL ; i++) {
		DisplayElement* el = AllDisplayElements[i];
        el->localize(cat);
	}
}

DisplayElement* PresetAlertElement =
    new DisplayElement("PresetAlert", MSG_UI_EL_PRESET_ALERT);

DisplayElement* MessagesElement =
    new DisplayElement("Messages", MSG_UI_EL_MESSAGES);
    
DisplayElement* TrackStripElement =
    new DisplayElement("TrackStrip", "TrackControls", MSG_UI_EL_TRACK_STRIP);

DisplayElement* TrackStrip2Element =
    new DisplayElement("TrackStrip2", MSG_UI_EL_TRACK_STRIP_2);

DisplayElement* CounterElement =
    new DisplayElement("Counter", MSG_UI_EL_COUNTER);

DisplayElement* ModeElement = 
    new DisplayElement("Mode", "Status", MSG_UI_EL_MODE);

DisplayElement* AudioMeterElement =
    new DisplayElement("AudioMeter", MSG_UI_EL_AUDIO_METER);

DisplayElement* LoopWindowElement =
    new DisplayElement("LoopWindow", MSG_UI_EL_LOOP_WINDOW);

DisplayElement* LoopMeterElement = 
    new DisplayElement("LoopMeter", MSG_UI_EL_LOOP_METER);

DisplayElement* BeatersElement = 
    new DisplayElement("Beaters", "Blinkers", MSG_UI_EL_BEATERS);

DisplayElement* LoopBarsElement = 
    new DisplayElement("LoopBars", "LoopList", MSG_UI_EL_LOOP_BARS);

DisplayElement* LayerBarsElement =
    new DisplayElement("LayerBars", "LayerList", MSG_UI_EL_LAYER_BARS);

DisplayElement* ParametersElement = 
    new DisplayElement("Parameters", MSG_UI_EL_PARAMETERS);

DisplayElement* MinorModesElement = 
    new DisplayElement("MinorModes", "Modes", MSG_UI_EL_MINOR_MODES);

DisplayElement* SyncStatusElement = 
    new DisplayElement("SyncStatus", "Sync", MSG_UI_EL_SYNC_STATUS);


/****************************************************************************
 *                                                                          *
 *                            TRACK STRIP ELEMENTS                          *
 *                                                                          *
 ****************************************************************************/

PUBLIC DisplayElement* FocusLockElement =
    new DisplayElement("lock", MSG_UI_TRACK_FOCUS_LOCK);

PUBLIC DisplayElement* TrackNumberElement =
    new DisplayElement("number", MSG_UI_TRACK_NUMBER);

PUBLIC DisplayElement* GroupNameElement =
    new DisplayElement("group", MSG_UI_TRACK_GROUP);

PUBLIC DisplayElement* InputLevelElement =
    new DisplayElement("input", MSG_PARAM_INPUT_LEVEL);

PUBLIC DisplayElement* OutputLevelElement =
    new DisplayElement("output", MSG_PARAM_OUTPUT_LEVEL);

PUBLIC DisplayElement* FeedbackElement =
    new DisplayElement("feedback", MSG_PARAM_FEEDBACK_LEVEL);

PUBLIC DisplayElement* AltFeedbackElement =
    new DisplayElement("altFeedback", MSG_PARAM_ALT_FEEDBACK_LEVEL);

PUBLIC DisplayElement* PanElement =
    new DisplayElement("pan", MSG_PARAM_PAN);

PUBLIC DisplayElement* SpeedOctaveElement =
    new DisplayElement("speedOctave", MSG_PARAM_SPEED_OCTAVE);

PUBLIC DisplayElement* SpeedStepElement =
    new DisplayElement("speedStep", MSG_PARAM_SPEED_STEP);

PUBLIC DisplayElement* SpeedBendElement =
    new DisplayElement("speedBend", MSG_PARAM_SPEED_BEND);

PUBLIC DisplayElement* PitchOctaveElement =
    new DisplayElement("pitchOctave", MSG_PARAM_PITCH_OCTAVE);

PUBLIC DisplayElement* PitchStepElement =
    new DisplayElement("pitchStep", MSG_PARAM_PITCH_STEP);

PUBLIC DisplayElement* PitchBendElement =
    new DisplayElement("pitchBend", MSG_PARAM_PITCH_BEND);

PUBLIC DisplayElement* TimeStretchElement =
    new DisplayElement("timeStretch", MSG_PARAM_TIME_STRETCH);

PUBLIC DisplayElement* SmallLoopMeterElement =
    new DisplayElement("loopMeter", MSG_UI_TRACK_LOOP_METER);

PUBLIC DisplayElement* LoopRadarElement =
    new DisplayElement("loopRadar", MSG_UI_TRACK_LOOP_RADAR);

PUBLIC DisplayElement* OutputMeterElement =
    new DisplayElement("outputMeter", MSG_UI_TRACK_OUTPUT_METER);

PUBLIC DisplayElement* LoopStatusElement =
    new DisplayElement("loopStatus", MSG_UI_TRACK_LOOP_STATUS);


/****************************************************************************
 *                                                                          *
 *                                ALL ELEMENTS                              *
 *                                                                          *
 ****************************************************************************/

DisplayElement* SpaceElements[] = {

	// the heavy-weight window caused problems, now using DisplayMessages
    //&PresetAlertElement,

    MessagesElement,
    TrackStripElement,
    TrackStrip2Element,
    CounterElement,
    ModeElement,
    AudioMeterElement,
    LoopMeterElement,
    LoopWindowElement,
    BeatersElement,
    LoopBarsElement,
    LayerBarsElement,
    ParametersElement,
    MinorModesElement,
    SyncStatusElement,
    NULL
};

PUBLIC DisplayElement* TrackStripElements[] = {
    FocusLockElement,
    TrackNumberElement,
	GroupNameElement,
    InputLevelElement,
    OutputLevelElement,
    FeedbackElement,
    AltFeedbackElement,
    PanElement,
    SpeedOctaveElement,
    SpeedStepElement,
    SpeedBendElement,
    PitchOctaveElement,
    PitchStepElement,
    PitchBendElement,
    TimeStretchElement,
    SmallLoopMeterElement,
    LoopRadarElement,
    OutputMeterElement,
	LoopStatusElement,
    NULL
};

PUBLIC DisplayElement* AllDisplayElements[] = {
    MessagesElement,
    TrackStripElement,
    TrackStrip2Element,
    CounterElement,
    ModeElement,
    AudioMeterElement,
    LoopMeterElement,
    LoopWindowElement,
    BeatersElement,
    LoopBarsElement,
    LayerBarsElement,
    ParametersElement,
    MinorModesElement,
    SyncStatusElement,

    FocusLockElement,
    TrackNumberElement,
	GroupNameElement,
    InputLevelElement,
    OutputLevelElement,
    FeedbackElement,
    AltFeedbackElement,
    PanElement,
    SpeedOctaveElement,
    SpeedStepElement,
    SpeedBendElement,
    PitchOctaveElement,
    PitchStepElement,
    PitchBendElement,
    TimeStretchElement,
    SmallLoopMeterElement,
    LoopRadarElement,
    OutputMeterElement,
	LoopStatusElement,
    NULL
};


/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

