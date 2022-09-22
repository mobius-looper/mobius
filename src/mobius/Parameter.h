/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Static object definitions for Mobius parameters.
 *
 * This is part of the public interface.
 *
 */

#ifndef MOBIUS_PARAMETER_H
#define MOBIUS_PARAMETER_H

#include "SystemConstant.h"

/****************************************************************************
 *                                                                          *
 *                                 CONSTANTS                                *
 *                                                                          *
 ****************************************************************************/

#define MAX_PARAMETER_ALIAS 4

typedef enum {

	TYPE_INT,
	TYPE_BOOLEAN,
	TYPE_ENUM,
	TYPE_STRING

} ParameterType;

typedef enum {

    // it is really important that these initialze properly
    // don't defualt and assume it's Preset
    PARAM_SCOPE_NONE,
	PARAM_SCOPE_PRESET,
	PARAM_SCOPE_TRACK,
	PARAM_SCOPE_SETUP,
	PARAM_SCOPE_GLOBAL

} ParameterScope;

/****************************************************************************
 *                                                                          *
 *                                 PARAMETER                                *
 *                                                                          *
 ****************************************************************************/

class Parameter : public SystemConstant {

    friend class Mobius;

  public:

	Parameter();
	Parameter(const char* name, int key);
	virtual ~Parameter();
    void localize(MessageCatalog* cat);

	const char* aliases[MAX_PARAMETER_ALIAS];

    bool bindable;      // true if this bindable 
	bool dynamic;		// true if labels and max ordinal can change
    bool deprecated;    // true if this is a backward compatible parameter
    bool transient;     // memory only, not stored in config objects
    bool resettable;    // true for Setup parameters that may be reset
    bool scheduled;     // true if setting the value schedules an event
    bool takesAction;   // true if ownership of the Action may be taken
    bool control;       // true if this is displayed as control in the binding UI

    /**
     * When this is set, it is a hint to the UI to display the value
     * of this parameter as a positive and negative range with zero
     * at the center.  This has no effect on the value of the parameter
     * only the way it is displayed.
     */
    bool zeroCenter;

    /**
     * Control parameters  have a default value, usually either the 
     * upper end of the range or the center.
     */
    int mDefault;

	ParameterType type;
	ParameterScope scope;
    const char** values;
	const char** valueLabels;
	int* valueKeys;

    /**
     * Used in rare cases where we need to change the
     * name of a parameter and upgrade the xml.
     */
    const char* xmlAlias;

    //
    // Configurable Parameter property access
    // 

    int getLow();
    virtual int getHigh(class MobiusInterface* m);

    /**
     * The maximum value used for bindings.
     * This is usually the same as getHigh() except for a ew
     * integers that don't have an upper bound.  Since we have 
     * to have some bounds for scaling MIDI CCs, this will default
     * to 127 and can be overridden.
     */
    virtual int getBindingHigh(class MobiusInterface* m);

	virtual void getOrdinalLabel(class MobiusInterface* m, int i, class ExValue* value);

	//
	// Parameter value access
	//

    /**
     * Get or set the value from a configuration object.
     */
    virtual void getObjectValue(void* object, class ExValue* value);
    virtual void setObjectValue(void* object, class ExValue* value);

    //
    // Get or set the value at runtime
    //

    virtual void getValue(class Export* exp, class ExValue* value);
    virtual void setValue(class Action* action);

    // maybe this can be a quality of the Export?
    virtual int getOrdinalValue(class Export* exp);

	//
	// Coercion helpers
	//

	/**
	 * Convert a string value to an enumeration ordinal value.
     * If the value is not in the enum, an error is traced and zero is returned.
	 */
	int getEnum(const char *value);

	/**
	 * Convert a string value to an enumeration ordinal value, returning
     * -1 if the value isn't in the enum.
	 */
	int getEnumValue(const char *value);

	/**
	 * Convert an ExValue with an string or a number into an ordinal.
	 */
	int getEnum(ExValue *value);

    /**
     * Upgrade an enumeration value.
     */
    void fixEnum(ExValue* value, const char* oldValue, const char* newValue);

	/**
	 * Convert a CC number in the range of 0-127 to an enumeration ordinal.
	 * !! This isn't used any more. Scaling needs to be done at the
	 * binding trigger layer appropriate for the trigger (host, key, midi, etc.)
	 */
	int getControllerEnum(int value);

    // internal use only

    virtual void getDisplayValue(MobiusInterface* m, ExValue* value);

	//
	// XML
	//

	void toXml(class XmlBuffer* b, void* obj);
	void parseXml(class XmlElement* e, void* obj);

  protected:

    static void initParameters();
	static void localizeAll(class MessageCatalog* cat);
    static void dumpFlags();

	static Parameter* getParameter(Parameter** group, const char* name);
	static Parameter* getParameter(const char* name);

	static Parameter* getParameterWithDisplayName(Parameter** group, const char* name);
	static Parameter* getParameterWithDisplayName(const char* name);

	static void checkAmbiguousNames();
    void addAlias(const char* alias);

	const char** allocLabelArray(int size);
    int getOrdinalInternal(class ExValue* value, const char** varray);

	int low;
	int high;


  private:

    void init();

};

/****************************************************************************
 *                                                                          *
 *                            PARAMETER CONSTANTS                           *
 *                                                                          *
 ****************************************************************************/

// Preset Parameters

extern Parameter* AltFeedbackEnableParameter;
extern Parameter* AutoRecordBarsParameter;
extern Parameter* AutoRecordTempoParameter;
extern Parameter* BounceQuantizeParameter;
extern Parameter* EmptyLoopActionParameter;
extern Parameter* EmptyTrackActionParameter;
extern Parameter* LoopCountParameter;
extern Parameter* MaxRedoParameter;
extern Parameter* MaxUndoParameter;
extern Parameter* MultiplyModeParameter;
extern Parameter* MuteCancelParameter;
extern Parameter* MuteModeParameter;
extern Parameter* NoFeedbackUndoParameter;
extern Parameter* NoLayerFlatteningParameter;
extern Parameter* OverdubQuantizedParameter;
extern Parameter* OverdubTransferParameter;
extern Parameter* PitchBendRangeParameter;
extern Parameter* PitchSequenceParameter;
extern Parameter* PitchShiftRestartParameter;
extern Parameter* PitchStepRangeParameter;
extern Parameter* PitchTransferParameter;
extern Parameter* QuantizeParameter;
extern Parameter* SpeedBendRangeParameter;
extern Parameter* SpeedRecordParameter;
extern Parameter* SpeedSequenceParameter;
extern Parameter* SpeedShiftRestartParameter;
extern Parameter* SpeedStepRangeParameter;
extern Parameter* SpeedTransferParameter;
extern Parameter* TimeStretchRangeParameter;
extern Parameter* RecordResetsFeedbackParameter;
extern Parameter* RecordThresholdParameter;
extern Parameter* RecordTransferParameter;
extern Parameter* ReturnLocationParameter;
extern Parameter* ReverseTransferParameter;
extern Parameter* RoundingOverdubParameter;
extern Parameter* ShuffleModeParameter;
extern Parameter* SlipModeParameter;
extern Parameter* SlipTimeParameter;
extern Parameter* SoundCopyParameter;
extern Parameter* SubCycleParameter;
extern Parameter* SustainFunctionsParameter;
extern Parameter* SwitchDurationParameter;
extern Parameter* SwitchLocationParameter;
extern Parameter* SwitchQuantizeParameter;
extern Parameter* SwitchVelocityParameter;
extern Parameter* TimeCopyParameter;
extern Parameter* TrackLeaveActionParameter;
extern Parameter* WindowEdgeAmountParameter;
extern Parameter* WindowEdgeUnitParameter;
extern Parameter* WindowSlideAmountParameter;
extern Parameter* WindowSlideUnitParameter;

// Deprecated Parameters

extern Parameter* AutoRecordParameter;
extern Parameter* InsertModeParameter;
extern Parameter* InterfaceModeParameter;
extern Parameter* LoopCopyParameter;
extern Parameter* OverdubModeParameter;
extern Parameter* RecordModeParameter;
extern Parameter* SamplerStyleParameter;
extern Parameter* TrackCopyParameter;

// Track Parameters

extern Parameter* AltFeedbackLevelParameter;
extern Parameter* AudioInputPortParameter;
extern Parameter* AudioOutputPortParameter;
extern Parameter* FeedbackLevelParameter;
extern Parameter* FocusParameter;
extern Parameter* GroupParameter;
extern Parameter* InputLevelParameter;
extern Parameter* InputPortParameter;
extern Parameter* MonoParameter;
extern Parameter* OutputLevelParameter;
extern Parameter* OutputPortParameter;
extern Parameter* PanParameter;
extern Parameter* PluginInputPortParameter;
extern Parameter* PluginOutputPortParameter;
extern Parameter* SpeedBendParameter;
extern Parameter* SpeedOctaveParameter;
extern Parameter* SpeedStepParameter;
extern Parameter* TrackNameParameter;
extern Parameter* PitchBendParameter;
extern Parameter* PitchOctaveParameter;
extern Parameter* PitchStepParameter;
extern Parameter* TimeStretchParameter;
extern Parameter* TrackPresetParameter;
extern Parameter* TrackPresetNumberParameter;
extern Parameter* TrackSyncUnitParameter;
extern Parameter* SyncSourceParameter;

// Setup Parameters

extern Parameter* BeatsPerBarParameter;
extern Parameter* DefaultSyncSourceParameter;
extern Parameter* DefaultTrackSyncUnitParameter;
extern Parameter* ManualStartParameter;
extern Parameter* MaxTempoParameter;
extern Parameter* MinTempoParameter;
extern Parameter* MuteSyncModeParameter;
extern Parameter* OutRealignModeParameter;
extern Parameter* RealignTimeParameter;
extern Parameter* ResizeSyncAdjustParameter;
extern Parameter* SlaveSyncUnitParameter;
extern Parameter* SpeedSyncAdjustParameter;

// Global Parameters

extern Parameter* AltFeedbackDisableParameter;
extern Parameter* AudioInputParameter;
extern Parameter* AudioOutputParameter;
extern Parameter* AutoFeedbackReductionParameter;
extern Parameter* BindingsParameter;
extern Parameter* ConfirmationFunctionsParameter;
extern Parameter* CustomMessageFileParameter;
extern Parameter* CustomModeParameter;
extern Parameter* DriftCheckPointParameter;
extern Parameter* DualPluginWindowParameter;
extern Parameter* FadeFramesParameter;
extern Parameter* FocusLockFunctionsParameter;
extern Parameter* GroupFocusLockParameter;
extern Parameter* HostMidiExportParameter;
extern Parameter* InputLatencyParameter;
extern Parameter* IntegerWaveFileParameter;
extern Parameter* IsolateOverdubsParameter;
extern Parameter* LogStatusParameter;
extern Parameter* LongPressParameter;
extern Parameter* MaxLoopsParameter;
extern Parameter* MaxSyncDriftParameter;
extern Parameter* MidiExportParameter;
extern Parameter* MidiInputParameter;
extern Parameter* MidiOutputParameter;
extern Parameter* MidiRecordModeParameter;
extern Parameter* MidiThroughParameter;
extern Parameter* MonitorAudioParameter;
extern Parameter* MuteCancelFunctionsParameter;
extern Parameter* NoiseFloorParameter;
extern Parameter* OutputLatencyParameter;
extern Parameter* OscInputPortParameter;
extern Parameter* OscOutputPortParameter;
extern Parameter* OscOutputHostParameter;
extern Parameter* OscTraceParameter;
extern Parameter* OscEnableParameter;
extern Parameter* PluginMidiInputParameter;
extern Parameter* PluginMidiOutputParameter;
extern Parameter* PluginMidiThroughParameter;
extern Parameter* PluginPortsParameter;
extern Parameter* QuickSaveParameter;
extern Parameter* SampleRateParameter;
extern Parameter* SaveLayersParameter;
extern Parameter* SetupNameParameter;
extern Parameter* SetupNumberParameter;
extern Parameter* SpreadRangeParameter;
extern Parameter* TraceDebugLevelParameter;
extern Parameter* TracePrintLevelParameter;
extern Parameter* TrackGroupsParameter;
extern Parameter* TrackParameter;
extern Parameter* TracksParameter;
extern Parameter* UnitTestsParameter;
extern Parameter* TrackInputPortParameter;
extern Parameter* TrackOutputPortParameter;

// Parameter Groups

extern Parameter* Parameters[];

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
