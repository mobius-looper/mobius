/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Static definitions for various UI objects.  These are known only
 * to the UI.  There are also static objects UIParameter and UIControl
 * but since thes are known to Mobius for binding they are defined
 * in Binding.h
 *
 */

#ifndef MOBIUS_UI_TYPES_H
#define MOBIUS_UI_TYPES_H

#include "SystemConstant.h"

/****************************************************************************
 *                                                                          *
 *                               UI PARAMETERS                              *
 *                                                                          *
 ****************************************************************************/

/**
 * The UIParameter class itself is defined in Binding.h
 */

extern class UIParameter* MessageDurationParameter;

extern class UIParameter* UIParameters[];

/****************************************************************************
 *                                                                          *
 *                                 UI CONTROL                               *
 *                                                                          *
 ****************************************************************************/

/**
 * The UIControl class itself is defined in Binding.h
 */

extern class UIControl* NextParameterControl;
extern class UIControl* PrevParameterControl;
extern class UIControl* IncParameterControl;
extern class UIControl* DecParameterControl;
extern class UIControl* SpaceDragControl;

extern class UIControl* UIControls[];

/****************************************************************************
 *                                                                          *
 *                              DISPLAY ELEMENTS                            *
 *                                                                          *
 ****************************************************************************/

class DisplayElement : public SystemConstant {

  public:

    const char* alias;

    DisplayElement(const char* name, int key);
    DisplayElement(const char* name, const char* alias, int key);

	static void localizeAll(class MessageCatalog* cat);
    static DisplayElement* get(const char* name);
    static DisplayElement* getNoAlias(const char* name);
    static DisplayElement* getWithDisplayName(DisplayElement** array, const char* name);

  private:
    void init();

};

extern DisplayElement* PresetAlertElement;
extern DisplayElement* MessagesElement;
extern DisplayElement* TrackStripElement;
extern DisplayElement* TrackStrip2Element;
extern DisplayElement* CounterElement;
extern DisplayElement* ModeElement;
extern DisplayElement* AudioMeterElement;
extern DisplayElement* LoopMeterElement;
extern DisplayElement* LoopWindowElement;
extern DisplayElement* BeatersElement;
extern DisplayElement* LoopBarsElement;
extern DisplayElement* LayerBarsElement;
extern DisplayElement* ParametersElement;
extern DisplayElement* MinorModesElement;
extern DisplayElement* SyncStatusElement;

extern DisplayElement* SpaceElements[];

extern DisplayElement* FocusLockElement;
extern DisplayElement* TrackNumberElement;
extern DisplayElement* GroupNameElement;
extern DisplayElement* InputLevelElement;
extern DisplayElement* OutputLevelElement;
extern DisplayElement* FeedbackElement;
extern DisplayElement* AltFeedbackElement;
extern DisplayElement* PanElement;
extern DisplayElement* SpeedOctaveElement;
extern DisplayElement* SpeedStepElement;
extern DisplayElement* SpeedBendElement;
extern DisplayElement* PitchOctaveElement;
extern DisplayElement* PitchStepElement;
extern DisplayElement* PitchBendElement;
extern DisplayElement* TimeStretchElement;
extern DisplayElement* SmallLoopMeterElement;
extern DisplayElement* LoopRadarElement;
extern DisplayElement* OutputMeterElement;
extern DisplayElement* LoopStatusElement;

extern DisplayElement* TrackStripElements[];

extern DisplayElement* AllDisplayElements[];

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
