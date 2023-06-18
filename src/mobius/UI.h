/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * GUI for Mobius based on Qwin.
 *
 */

#ifndef MOBIUS_UI_H
#define MOBIUS_UI_H

#include <vector>

#include "KeyCode.h"
#include "Qwin.h"
#include "QwinExt.h"
#include "FontConfig.h"

// Needed for Trigger and other Binding things
#include "Binding.h"

// needed for LoopState
#include "MobiusState.h"

// needed for MobiusListener
#include "MobiusInterface.h"

#include "BindingDialog.h"

QWIN_USE_NAMESPACE

/****************************************************************************
 *                                                                          *
 *                               BORDERED GRID                              *
 *                                                                          *
 ****************************************************************************/

class BorderedGrid : public Panel, public MouseInputAdapter {

  public:
    
    BorderedGrid(int rows, int columns);
    ~BorderedGrid();

    void add(Component *c);
    void setSelectedIndex(int index);
    int getSelectedIndex();

	void mousePressed(MouseEvent* e);


  private:

    Border* mNoBorder;
    Border* mYesBorder;

};

/****************************************************************************
 *                                                                          *
 *   							 MIDI DIALOG                                *
 *                                                                          *
 ****************************************************************************/


class MidiDialog : public SimpleDialog {

  public:

	MidiDialog(Window* parent, class MobiusInterface* mob, 
               class MobiusConfig* config);
	~MidiDialog();
	bool commit();

  private:

	void addDevices(class MessageCatalog* cat, 
					class MidiPort* devs, 
					ListBox* box, 
					const char* current, 
					bool multi);

	class MobiusConfig* mConfig;

	ListBox* mInputs;
	ListBox* mOutputs;
	ListBox* mThrus;
	ListBox* mPluginInputs;
	ListBox* mPluginOutputs;
	ListBox* mPluginThrus;
};

/****************************************************************************
 *                                                                          *
 *   							 AUDIO DIALOG                               *
 *                                                                          *
 ****************************************************************************/

class AudioDialog : public SimpleDialog {

  public:

	AudioDialog(Window* parent, class MobiusInterface* m, 
                class MobiusConfig* config);
	~AudioDialog();
	bool commit();

  private:

	void actionPerformed(void* src);

	class MobiusInterface* mMobius;
	class MobiusConfig* mConfig;
	class AudioDevice** mDevices;
	ListBox* mASIO;
	ListBox* mInputs;
	ListBox* mOutputs;
	NumberField*    mLatencyMsec;
	NumberField* 	mInputLatency;
	NumberField* 	mOutputLatency;
	Button* mCalibrate;
    ComboBox* mSampleRate;

};

/****************************************************************************
 *                                                                          *
 *   					  LATENCY CALIBRATION DIALOG                        *
 *                                                                          *
 ****************************************************************************/

class CalibrationDialog : public SimpleDialog {

  public:

	CalibrationDialog(Window* parent, class MobiusInterface* m, class MobiusConfig* config);
	~CalibrationDialog();
	const char* getOkName();
	bool commit();

	class CalibrationResult* getResult();

  private:

	void calibrate();

	class MobiusInterface* mMobius;
	class MobiusConfig* mConfig;
	class CalibrationResult* mResult;
	
};

class CalibrationResultDialog : public SimpleDialog {

  public:

	CalibrationResultDialog(Window* parent, int total, int input, int output);
	~CalibrationResultDialog();
	const char* getOkName();

	bool commit();

  private:

};

/****************************************************************************
 *                                                                          *
 *   							PRESET DIALOG                               *
 *                                                                          *
 ****************************************************************************/

class PresetDialog : public SimpleDialog  {

  public:

	PresetDialog(Window* parent, class MobiusInterface* mob,
                 class MobiusConfig* config);
	~PresetDialog();

	void actionPerformed(void* c);
	void captureFields();
	void refreshFields();
    bool commit();

  private:
	
	void refreshSelector();
	Checkbox* newCheckbox(class Parameter* p);
	ComboBox* addCombo(FormPanel* form, class Parameter* p);
	ComboBox* addCombo(FormPanel* form, class Parameter* p, int cols);
	NumberField* addNumber(FormPanel* form, class Parameter* p);
	NumberField* addNumber(FormPanel* form, class Parameter* p, int min, int max);

    class MobiusInterface* mMobius;
	class MobiusConfig* mConfig;
	class MessageCatalog* mCatalog;
	class Preset* mPreset;

	ComboBox*		mSelector;
	Button* 		mNew;
	Button*			mDelete;
    Button*         mRename;
    Text*           mName;
	NumberField* 	mSubcycles;
	Checkbox* 		mSpeedRecord;
	Checkbox* 		mRecordFeedback;
	Checkbox* 		mOverdubQuantized;
	ComboBox* 		mMultiplyMode;
	ComboBox* 		mEmptyLoopAction;
	ComboBox* 		mEmptyTrackAction;
	ComboBox* 		mTrackLeaveAction;
	NumberField* 	mLoops;
	ComboBox* 		mMuteMode;
	ComboBox* 		mMuteCancel;
	ComboBox* 		mBounceMode;
	ComboBox* 		mQuantize;
	ComboBox* 		mBounceQuantize;
	ComboBox* 		mShuffleMode;
	ComboBox* 		mRecordTransfer;
	ComboBox* 		mOverdubTransfer;
	ComboBox* 		mReverseTransfer;
	ComboBox* 		mSpeedTransfer;
	ComboBox* 		mPitchTransfer;
	Checkbox* 		mRoundingOverdub;
	ComboBox* 		mSwitchLocation;
	ComboBox* 		mSwitchDuration;
	ComboBox* 		mReturnLocation;
	ComboBox* 		mTimeCopy;
	ComboBox* 		mSoundCopy;
	ComboBox* 		mSwitchQuantize;
	ComboBox*		mSlipMode;
	NumberField*    mAutoRecordTempo;
	NumberField*    mAutoRecordBars;
	NumberField* 	mThreshold;
	NumberField*	mSlipTime;
    NumberField*    mSpeedStep;
    NumberField*    mSpeedBend;
    NumberField*    mPitchStep;
    NumberField*    mPitchBend;
    NumberField*    mTimeStretch;
	Checkbox* 		mAltFeedback;
	Checkbox* 		mVelocity;
	Checkbox*		mNoFeedbackUndo;
	Checkbox*		mNoLayerFlattening;
	NumberField*	mMaxUndo;
	NumberField*	mMaxRedo;
	Checkbox* 		mSpeedRestart;
	Checkbox* 		mPitchRestart;
	Text*			mSpeedSequence;
	Text*			mPitchSequence;
    MultiSelect*    mSustainFunctions;
    ComboBox*       mWindowSlideUnit;
    NumberField*    mWindowSlideAmount;
    ComboBox*       mWindowEdgeUnit;
    NumberField*    mWindowEdgeAmount;
};

/****************************************************************************
 *                                                                          *
 *   							 SETUP DIALOG                               *
 *                                                                          *
 ****************************************************************************/

#define MAX_UI_TRACKS 8

/**
 * Class used to coordinate the widgets for one track.
 * We used to keep several of these under a tab component, now there
 * is just one that shows the current track selected with a radio button.
 */
class TrackComponents {

  public:

	TrackComponents() {
		//track = NULL;
        name = NULL;
		preset = NULL;
		audioInputPort = NULL;
		audioOutputPort = NULL;
		pluginInputPort = NULL;
		pluginOutputPort = NULL;
		focusLock = NULL;
		group = NULL;
		input = NULL;
		output = NULL;
		feedback = NULL;
		altFeedback = NULL;
		pan = NULL;
		mono = NULL;
        syncSource = NULL;
        trackUnit = NULL;
	}

	//class Track* track;
    Text* name;
	ComboBox* preset;
	ComboBox* audioInputPort;
	ComboBox* audioOutputPort;
	ComboBox* pluginInputPort;
	ComboBox* pluginOutputPort;
	Checkbox* focusLock;
	ComboBox* group;
	ComboBox* syncSource;
	ComboBox* trackUnit;
	Slider* input;
	Slider* output;
	Slider* feedback;
	Slider* altFeedback;
	Slider* pan;
	Checkbox* mono;
};

class SetupDialog : public SimpleDialog  {

  public:

	SetupDialog(Window* parent, class MobiusInterface* mob,
                class MobiusConfig* config);
	~SetupDialog();

	void actionPerformed(void* c);
	void refreshFields();
    bool commit();
	void opened();

  private:
	
	class StringList* getPresetNames();
	class StringList* getGroupNames();
	class StringList* getPortNames(int ports);
	class Slider* getSlider();
	void refreshSelector();
    void refreshTrackFields();
	void captureFields();
    void captureTrackFields();
	void checkLabels(const char* when);
	void checkLabels(class Component* c);

    NumberField* addNumber(FormPanel* form, Parameter* p, int min, int max);
    ComboBox* addCombo(FormPanel* form, Parameter* p);
    ComboBox* addCombo(FormPanel* form, Parameter* p, int cols);
    Checkbox* newCheckbox(Parameter* p);

	class MobiusInterface* mMobius;
	class MobiusConfig* mConfig;
	class MessageCatalog* mCatalog;
	class Setup* mSetup;


	ComboBox*		mSelector;
	Button* 		mNew;
	Button*			mDelete;
    Button*         mRename;
    Text*           mName;
    Radios*         mTrackRadio;
    int             mTrackNumber;
	Button* 		mInit;
	Button* 		mCapture;
	Button* 		mInitAll;
    Button* 		mCaptureAll;
	ListBox* 		mReset;
    ComboBox*       mBindings;

	ComboBox*		mActive;
	ComboBox*		mSyncSource;
	ComboBox*		mSyncUnit;
	ComboBox*		mTrackUnit;
	ComboBox*		mMuteSync;
	ComboBox*		mResizeSync;
	ComboBox*		mSpeedSync;
	ComboBox*		mRealignTime;
	ComboBox*		mRealignMode;
	NumberField* 	mMinTempo;
	NumberField* 	mMaxTempo;
    NumberField* 	mBeatsPerBar;
    Checkbox*       mManualStart;

    // originally had an array of these, now just one
    TrackComponents* mTrack;


};

/****************************************************************************
 *                                                                          *
 *   							GLOBAL DIALOG                               *
 *                                                                          *
 ****************************************************************************/

class GlobalDialog : public SimpleDialog  {

  public:

	GlobalDialog(Window* parent, class MobiusInterface* mob, 
                 class MobiusConfig* config, class UIConfig* uiconfig);
	~GlobalDialog();

	void refreshFields();
    bool commit();

  private:
	
	NumberField* addNumber(FormPanel* form, class Parameter* p);
    NumberField* addNumber(FormPanel* form, class UIParameter* p);
	Checkbox* addCheckbox(FormPanel* form, class Parameter* p);

    class MobiusInterface* mMobius;
	class MobiusConfig* mConfig;
    class UIConfig* mUIConfig;
	class MessageCatalog* mCatalog;

	Text* 			mQuickSave;
	Text* 			mCustomMessageFile;
    Text*           mOscHost;
	NumberField* 	mTracks;
	NumberField* 	mTrackGroups;
	NumberField* 	mMaxLoops;
	NumberField* 	mPluginPorts;
	NumberField* 	mNoiseFloor;
	NumberField* 	mFadeFrames;
	NumberField* 	mLongPress;
	NumberField* 	mMaxDrift;
	NumberField* 	mSpreadRange;
	NumberField* 	mTracePrintLevel;
	NumberField* 	mTraceDebugLevel;
	NumberField* 	mMessageDuration;
    NumberField*    mOscInput;
    NumberField*    mOscOutput;
	Checkbox*		mAutoFeedback;
	Checkbox*		mSaveLayers;
	Checkbox*		mLogStatus;
	Checkbox*		mMonitor;
	Checkbox*		mIsolate;
	Checkbox*		mDualPluginWindow;
	Checkbox* 		mFileFormat;
	Checkbox* 		mMidiExport;
	Checkbox* 		mHostMidiExport;
	Checkbox* 		mGroupFocusLock;
	Checkbox*		mOscTrace;
	Checkbox*		mOscEnable;
	MultiSelect*	mFocusLockFunctions;
	MultiSelect*	mMuteCancelFunctions;
	MultiSelect*	mConfirmationFunctions;
	MultiSelect*	mFeedbackModes;

};

/****************************************************************************
 *                                                                          *
 *   							 PORT DIALOG                                *
 *                                                                          *
 ****************************************************************************/

class PortDialog : public SimpleDialog {

  public:

	PortDialog(Window* parent, class UI* ui, class MobiusInterface* m);
	~PortDialog();

    bool commit();

  private:

	class MobiusInterface* mMobius;
	ComboBox** mInputs;
	ComboBox** mOutputs;

};

/****************************************************************************
 *                                                                          *
 *                               DISPLAY DIALOG                             *
 *                                                                          *
 ****************************************************************************/

class DisplayDialog : public SimpleDialog {

  public:

	DisplayDialog(Window* parent, class MobiusInterface* mob, 
                  class UIConfig* config);
	~DisplayDialog();

    void actionPerformed(void *c);
    bool commit();

  private:

    class DisplayElement* getDisplayElement(const char *dname);
    void buildControlSelector(MultiSelect* ms, StringList* current);
    StringList* convertControls(StringList* selected);

    class MobiusInterface* mMobius;
	class UIConfig* mConfig;
    MultiSelect* mSelector;
    MultiSelect* mParameters;
    MultiSelect* mFloatingStrip;
    MultiSelect* mFloatingStrip2;
    MultiSelect* mDockedStrip;

};

/****************************************************************************
 *                                                                          *
 *   							BUTTON DIALOG                               *
 *                                                                          *
 ****************************************************************************/

class ButtonConfigDialog : public SimpleDialog {

  public:

	ButtonConfigDialog(Window* parent, class MobiusInterface* mob, 
                       class UIConfig* config);
	~ButtonConfigDialog();

    void actionPerformed(void *c);
    bool commit();

  private:

	class UIConfig* mConfig;
    MultiSelect* mSelector;

};

/****************************************************************************
 *                                                                          *
 *   							SCRIPT DIALOG                               *
 *                                                                          *
 ****************************************************************************/

class ScriptDialog : public SimpleDialog {

  public:

	ScriptDialog(Window* parent, class MobiusInterface* mob, 
                 class MobiusConfig* config);
	~ScriptDialog();

    void actionPerformed(void *c);
    bool commit();

  private:

	class MessageCatalog* mCatalog;
	class MobiusConfig* mConfig;
    ListBox* mSelector;
	Button* mAdd;
	Button* mAddDir;
	Button* mDelete;

};

/****************************************************************************
 *                                                                          *
 *   							SAMPLE DIALOG                               *
 *                                                                          *
 ****************************************************************************/

class SampleDialog : public SimpleDialog {

  public:

	SampleDialog(Window* parent, class MobiusInterface* mob, 
                 class MobiusConfig* config);
	~SampleDialog();

    void actionPerformed(void *c);
    bool commit();

  private:

	class MessageCatalog* mCatalog;
	class MobiusConfig* mConfig;
    ListBox* mSelector;
	Button* mAdd;
	Button* mDelete;
	Button* mUp;
	Button* mDown;
};

/****************************************************************************
 *                                                                          *
 *   						   SAVE/LOAD DIALOG                             *
 *                                                                          *
 ****************************************************************************/

class SaveDialog : public SimpleDialog  {

  public:

	SaveDialog(Window* parent, class MobiusInterface* m);
	~SaveDialog();

	void actionPerformed(void* src);
    bool commit();

  private:
	
	void addLoopChecks(Panel* grid, const char* label);
	Checkbox* newCheckbox();

	class MobiusInterface* mMobius;
	Panel* mGrid;

};

/****************************************************************************
 *                                                                          *
 *   							 ALERT DIALOG                               *
 *                                                                          *
 ****************************************************************************/

class PromptDialog : public SimpleDialog  {

  public:

	PromptDialog(Window* parent, UI* ui, class Prompt* p);
	~PromptDialog();

	void setNext(PromptDialog* d);
	PromptDialog* getNext();
	class Prompt* getPrompt();

	const char* getCancelName();
	void closing();


  private:

	PromptDialog* mNext;
	UI* mUI;
	class Prompt* mPrompt;

};

/****************************************************************************
 *                                                                          *
 *                               RENAME DIALOG                              *
 *                                                                          *
 ****************************************************************************/

class RenameDialog : public SimpleDialog  {

  public:

	RenameDialog(Window* parent, const char* title, const char* prompt,
                 const char* current);
	~RenameDialog();

    const char* getValue();

    // SimpleDialog overloads
	const char* getCancelName();
    void closing();

  private:

	UI* mUI;
    Text* mText;
    char* mValue;
};

/****************************************************************************
 *                                                                          *
 *                                HELP DIALOGS                              *
 *                                                                          *
 ****************************************************************************/

class MidiHelpDialog : public SimpleDialog  {

  public:

	MidiHelpDialog(Window* parent, class MobiusInterface* mob);
	~MidiHelpDialog();

	const char* getCancelName();

  private:

    void addBindings(class MobiusInterface* mob, BindingConfig* config);

    // transient build state
    class FormPanel* mForm;
    int mRow;
    int mColumn;

};

class KeyHelpDialog : public SimpleDialog  {

  public:

	KeyHelpDialog(Window* parent, class MobiusInterface* mob);
	~KeyHelpDialog();

	const char* getCancelName();

  private:

};

/****************************************************************************
 *                                                                          *
 *   							   TRACKER                                  *
 *                                                                          *
 ****************************************************************************/
/*
 * The Tracker classes were originally designed to be independent.
 * But "space" components have evolved to the point where they really need
 * to be integrated more tightly.
 */

class TrackerSource {

  public:

	virtual const char* getTrackedString(class Tracker* t) = 0;
	virtual long getTrackedInt(class Tracker* t) = 0;

};

#define MAX_TRACKED_LENGTH 128

#define TRACKER_STRING 0
#define TRACKER_INT 1

class Tracker : public Component, public ActionListener {

  public:

	Tracker();
	Tracker(int type);
	Tracker(TrackerSource* src, SimpleTimer* t, int type);
	~Tracker();

	void initTracker(TrackerSource* src, SimpleTimer* t, int type);

    void setSource(TrackerSource* src);
    void setTimer(SimpleTimer* t);
	void setType(int type);
	void setFont(Font* f);
	void setColor(Color* c);
	void setMaxChars(int i);
	void setValue(const char* s);
	void setValues(class StringList* values);
	void setValue(int i);
	void setDivisor(int i);

	virtual void actionPerformed(void* src);
	void update();
	void update(const char* s);
	void paint(Graphics* g);
	Dimension* getPreferredSize(Window* w);

  protected:

	TrackerSource* mSource;
    SimpleTimer* mTimer;
	Font* mFont;
	int mType;
	int mDivisor;
	class StringList* mValues;
	int mMaxChars;
	char mValue[MAX_TRACKED_LENGTH];
};

/****************************************************************************
 *                                                                          *
 *                                   BEATER                                 *
 *                                                                          *
 ****************************************************************************/

/**
 * Default Number of milliseonds to display the beat graphic.
 * 150 looks ok but starts to smear with fast tempos.  
 * 50 and 100 seem to miss a lot of beats.  
 */
#define BEAT_DECAY 150

class Beater : public Tracker {

  public:

	Beater();
	Beater(const char* label);
	Beater(const char* label, SimpleTimer* t);
	Beater(SimpleTimer* t);
	~Beater();

    void setLabel(const char* label);
	void setDecay(int i);
	void setBeat(int i);

    void setDiameter(int i); //#012

	void beat();
	void beat(int ticks);
    void beatOn();
    void beatOff();

	void actionPerformed(void* src);
	Dimension* getPreferredSize(Window* w);
	void paint(Graphics* g);
    void init();
	void dumpLocal(int indent);

  private:

	void draw(bool includeLabel);

    Color* mBeatColor;
    char* mLabel;
    int mDiameter;
	int mDecay;
	int mDecayCounter;

	int mBeat;
	int mBeatCounter;

};

/****************************************************************************
 *                                                                          *
 *                                THERMOMETER                               *
 *                                                                          *
 ****************************************************************************/

class Thermometer : public Tracker {

  public:

	Thermometer();
	Thermometer(TrackerSource* src, SimpleTimer* t);
	~Thermometer();
    void init();

    void setValue(int i);
	int getValue();
	void setRange(int i);
	int getRange();
    void setMeterColor(Color* c);
	void actionPerformed(void* src);

	Dimension* getPreferredSize(Window* w);
	void paint(Graphics* g);
	void dumpLocal(int indent);

  private:

    Color* mMeterColor;
	int mRange;
	int mValue;

};

/****************************************************************************
 *                                                                          *
 *   							  COMPONENTS                                *
 *                                                                          *
 ****************************************************************************/

class Knob : public Component, public MouseInputAdapter {

  public:

    Knob();
    Knob(const char *label);
    Knob(const char *label, int max);
    ~Knob();

	int getValue();

	void setLabel(const char* label);
	void setDiameter(int r);
    void setValue(int i);
	void setNoDisplayValue(bool b);
    void setMinValue(int i);
    void setMaxValue(int i);
	void setFont(Font* f);
	void setClickIncrement(bool b);
	void update(int i);

    Dimension* getPreferredSize(Window* w);
    void paint(Graphics* g);
    
    void mousePressed(MouseEvent* e);
    void mouseDragged(MouseEvent* e);
    void mouseReleased(MouseEvent* e);

	bool debugging;

  private:

    void init();

	char* mLabel;
	int mDiameter;
    Font* mFont;
	bool mClickIncrement;

    // probably need mMinValue eventually
    int mValue;
    int mMinValue;
    int mMaxValue;
	bool mNoDisplayValue;

    bool mDragging;
    int mDragStartValue;
    int mDragOriginX;
    int mDragOriginY;
	int mDragChanges;


};

/****************************************************************************
 *                                                                          *
 *   								COLORS                                  *
 *                                                                          *
 ****************************************************************************/

/**
 * Names of the global palette colors used by the Space components.
 */
#define COLOR_SPACE_BACKGROUND "background"
#define COLOR_BUTTON "button"
#define COLOR_BUTTON_TEXT "buttonText"
#define COLOR_BAR "bar"
#define COLOR_ACTIVE_BAR "activeBar"
#define COLOR_CHECKPOINT_BAR "checkpointBar"
#define COLOR_METER "meter"
#define COLOR_SLOW_METER "slowMeter"
#define COLOR_RECORDING_METER "recordingMeter"
#define COLOR_MUTE_METER "muteMeter"
#define COLOR_EVENT "event"
#define COLOR_ALERT_BACKGROUND "alertBackground"
#define COLOR_ALERT_TEXT "alertText"
#define COLOR_BLINK "blink"
#define COLOR_PARAM_NAME "paramName"
#define COLOR_PARAM_VALUE "paramValue"
#define COLOR_GROUP1 "group1"
#define COLOR_GROUP2 "group2"
#define COLOR_GROUP3 "group3"
#define COLOR_GROUP4 "group4"
#define COLOR_TICK_CYCLE "tickCycle"
#define COLOR_TICK_SUBCYCLE "tickSubcycle"
#define COLOR_TICK_CUE "tickCue"
#define COLOR_LOOP_WINDOW "window"

/**
 * Definitions for each color, used to make sure that existing Palette
 * objects from ui.xml are upgraded when new colors are added.
 */
class ColorDefinition {
  public:

	const char* name;
	int key;

	ColorDefinition(const char* aname) {
		name = aname;
	}

	ColorDefinition(const char* aname, int akey) {
		name = aname;
		key = akey;
	}
};

extern ColorDefinition* ColorDefinitions[];

/****************************************************************************
 *                                                                          *
 *                                   SPACE                                  *
 *                                                                          *
 ****************************************************************************/


class Space : public Panel, public MouseInputAdapter, public KeyListener {

  public:

    Space();
    ~Space();

    Component* findComponent(int x, int y);
    void mousePressed(MouseEvent* e);
    void mouseReleased(MouseEvent *e);
    void mouseDragged(MouseEvent* e);
	void keyPressed(KeyEvent* e);
	void keyReleased(KeyEvent* e);

	// Kludge to display a temporary border around the space components
	// while dragging. Static interface so we don't have to make all SpaceComponents
	// aware of their parent Space.  Refactor someday...

	void setDragging(bool b);
    static bool isDragging();

  private:

	static bool Dragging;
	Dragable* mDragable;

};

class SpaceComponent : public Container {

  public:

    SpaceComponent();
    virtual ~SpaceComponent();

    const char* getDragName();

    virtual void setEnabled(bool b);
    void erase();
	void erase(Graphics* g);
	void erase(Graphics* g, Bounds* b);
	void drawMoveBorder(Graphics* g);

  protected:

    void setType(DisplayElement* type);

    /**
     * Set by the subclass to one of the display element type definitions.
     */
    class DisplayElement* mElementType;

    // name shown in the drag box, costant and non-localized for now
    //const char* mDragName;

};

/****************************************************************************
 *                                                                          *
 *                                   ALERT                                  *
 *                                                                          *
 ****************************************************************************/

/**
 * Originally had this be a non-modal dialog, but it doesn't really need to be
 * and it causes destructor complexity since the window wants to delete itself.
 */
class PopupAlert : public Dialog {

  public:

    PopupAlert(Window* parent);
    PopupAlert(Window* parent, const char* msg);
    ~PopupAlert();

	void setDuration(int i);
	void setFont(Font* font);
	void setMessage(const char* msg);

    // should let this manage it's own timer
    bool tick();

	Dimension* getPreferredSize(Window* w);

  private:

    void initAlert(Window* w);

	int mDuration;
	int mCounter;
    Label* mLabel;
};

/****************************************************************************
 *                                                                          *
 *   						   SPACE COMPONENTS                             *
 *                                                                          *
 ****************************************************************************/

class ModeDisplay : public SpaceComponent {

  public:

	ModeDisplay();
	~ModeDisplay();

	void setValue(const char* value);
	void paint(Graphics* g);
	void update(class MobiusState* s);

  private:

	Tracker* mMode;
};

class AudioMeter : public SpaceComponent {

  public:

	AudioMeter();
	~AudioMeter();

	void setRange(int i);
	void setValue(int i);
	void setRequiredSize(Dimension* d);
	Dimension* getPreferredSize(Window* w);
	void paint(Graphics* g);
	void dumpLocal(int indent);
	void stop();
	void update(int level);

  private:

	Dimension* mRequiredSize;
    Color* mMeterColor;
	int mRange;
	int mValue;
	int mLevel;

	//Peak
	int mPeakLevel; // #015
	int mPeakWidth;

};

class LoopWindow : public SpaceComponent {

  public:

	LoopWindow();
	~LoopWindow();

	Dimension* getPreferredSize(Window* w);
	void paint(Graphics* g);

	void update(class MobiusState* s);
    void dumpLocal(int indent);

  private:

    Color* mWindowColor;
    long mWindowOffset;
    long mWindowFrames;
    long mHistoryFrames;
};

class Beaters : public SpaceComponent {

  public:

    Beaters(SimpleTimer* t);
    ~Beaters();

    void reset();
	void update(class MobiusState* s);
	void paint(Graphics* g);
	void setBeaterDiameter(int i); //#012

  private:

    Beater* mLoop;
    Beater* mCycle;
    Beater* mSubCycle;

};

class BarGraph : public SpaceComponent {

  public:
	
	BarGraph();
	~BarGraph();
	
	void setValue(int i);
	void setMaxValue(int i);
	void setVertical(bool b);
	void setBarWidth(int i);
	void setBarHeight(int i);
	void setBarGap(int i);
	void setInitialBars(int i);

	virtual bool isSpecial(int index);

    void update(int newMaxValue, int newValue, bool force);

    Dimension* getPreferredSize(Window* w);
    void paint(Graphics* g);

  protected:

	int getRequiredSize(int max);
	void paintOne(Graphics* g, Bounds* b, int offset);
	void paintOne(Graphics* g, Bounds* b, Color* c, Color* border,
				  int offset, int length);

	int mInitialBars;
	int mValue;
	int mMaxValue;
	bool mIncrementalUpdate;
	int mNewValue;
	int mNewMaxValue;
	bool mVertical;
	int mBarWidth;
	int mBarHeight;
	int mBarGap;
	Color* mBarColor;
	Color* mActiveBarColor;
	Color* mSpecialColor;

};

class LoopList : public BarGraph {

  public:
	
	LoopList();
	~LoopList();
	
    void update(class MobiusState* s);

  private:

};

class LayerList : public BarGraph {

  public:
	
	LayerList();
	~LayerList();
	
    void update(class MobiusState* s);
	bool isSpecial(int index);
	void paint(Graphics* g);

	//void setBarWidth(int i);	//#013  Expose BarList BarWidth || Già esposte per gerarchia!  LayerList deriva da BarGraph
	//void setBarHeight(int i);	//#013 	Expose BarList BarHeight || Già esposte per gerarchia!

  private:

	LoopState mState;
	Font* mFont;
};


#define EDP_DISPLAY_UNITS 11

class EDPDisplay : public SpaceComponent {

  public:

	EDPDisplay(int sampleRate);
	~EDPDisplay();

    void update(class MobiusState* s);

	Dimension* getPreferredSize(Window* w);
	void paint(Graphics* g);

  private:

	Font* mFont;
	Font* mFont2;

	// xx yy.y zz/cc
	int mLeft[EDP_DISPLAY_UNITS];
	int mTop[EDP_DISPLAY_UNITS];
	int mValues[EDP_DISPLAY_UNITS];

    int mSampleRate;
	int mLoop;
	int mFrame;
	int mCycle;
	int mCycles;
	int mNextLoop;
	int mFontOffset;
};

class ActionButton : public CustomButton, public ActionListener {

  public:

	ActionButton(class MobiusInterface* mob, class Action* b);
    ~ActionButton();

    void actionPerformed(void *o);

  private:

    void init();

    class MobiusInterface* mMobius;
    class Action* mAction;

};

class SpaceKnob : public SpaceComponent, public ActionListener {

  public:

    SpaceKnob();
    virtual ~SpaceKnob();

    void setDiameter(int i);
	void setLabel(const char* label);
    void setValue(int i);
	void setNoDisplayValue(bool b);
    void setMinValue(int i);
    void setMaxValue(int i);
    void setBackground(Color* c);
    void setForeground(Color* c);
    void actionPerformed(void* o);
	void update(int i);

	int getValue();

  private:

	Knob* mKnob;

};

class ActionKnob : public SpaceKnob {

  public:
    
    ActionKnob(class MobiusInterface* m, const char* name, int track);
    ~ActionKnob();

    void actionPerformed(void* src);
    void update();

  private:

    void resolve(const char* name, int track);

    class MobiusInterface* mMobius;
    class Action* mAction;
    class Export* mExport;

};

		
class LoopMeter : public SpaceComponent {

  public:

    LoopMeter();
    LoopMeter(bool ticks, bool markers);
    ~LoopMeter();

    void setEnabled(bool b);
	void update(class MobiusState* state);
	void paint(Graphics* g);

	void setPreferredSize(int width, int height); //#011 Custom Size LoopMeter (override SpaceComponent definition)

  private:

	void init(bool ticks, bool markers);
	void paintEventName(Graphics* g, Function* f, 
						const char* name, int argument, 
                        Bounds* b, int left, int top,
                        Bounds* nameBounds, int eventIndex);
	
	Font* mFont;
	Thermometer* mMeter;
	bool mTicks;
	bool mMarkers;
	LoopState mState;
	int mSubcycles;

    Color* mColor;
    Color* mSlowColor;
    Color* mRecordingColor;
    Color* mMuteColor;
    Color* mEventColor;
    Color* mTickCycleColor;
	Color* mTickSubcycleColor;
	Color* mTickCueColor;

	//Override setPreferredSize

};

class LoopGrid : public SpaceComponent {

  public:

    LoopGrid();
    ~LoopGrid();

	void update(class MobiusState* state);
	void paint(Graphics* g);

  private:



};

/**
 * Structure used by LoopStack to maintain the currently displayed loop state.
 */
typedef struct {

	bool active;
	bool pending;
	bool mute;
	bool speed;
	int cycles;

} LoopStackState;

/**
 * Maximum number of loops we will display in teh LoopStack.
 */
#define LOOP_STACK_MAX_LOOPS 8

class LoopStack : public SpaceComponent, public MouseInputAdapter
{
  public:

    LoopStack(MobiusInterface* m, int track);
    ~LoopStack();

	Dimension* getPreferredSize(Window* w);
	void update(class MobiusState* state);
	void paint(Graphics* g);
	void mousePressed(MouseEvent* e);

  private:

    MobiusInterface* mMobius;
    Action* mAction;

	Font* mFont;
    Color* mColor;
    Color* mActiveColor;
    Color* mPendingColor;
    Color* mSlowColor;
    Color* mMuteColor;

	LoopStackState mLoops[LOOP_STACK_MAX_LOOPS];
    int mMaxLoops;
	int mLoopCount;

};

#define MAX_ALERT 1024

class SpaceAlert : public SpaceComponent {

  public:

    SpaceAlert();
    ~SpaceAlert();

	Dimension* getPreferredSize(Window* w);

    void openPopup(const char* msg);
    void closePopup();

	void update(const char* msg);
	void update();

  private:

    PopupAlert* mPopup;
};

#define MAX_NAME 1024

class PresetAlert : public SpaceAlert {

  public:

    PresetAlert();
    ~PresetAlert();

	void update(class MobiusState* s);

  private:

	int mPreset;
};

class Radar : public SpaceComponent {

  public:

	Radar();
    ~Radar();

	void setRange(int i);
	virtual void update(int value);
	
	void setDiameter(int i); //#004
	int getDiameter(); //#004

	Dimension* getPreferredSize(Window* w);
   	void paint(Graphics* g);

  protected:

    void init();

  private:

	int mDiameter;
	int mRange;
    int mDegree;
    int mLastDegree;
	int mLastRange;
	bool mPhase;

};

class LoopRadar : public Radar {

  public:

    LoopRadar();

	void update(class MobiusState* state);

  private:

    Color* mColor;
    Color* mSlowColor;
    Color* mRecordingColor;
    Color* mMuteColor;
};


/****************************************************************************
 *                                                                          *
 *   							 TRACK STRIP                                *
 *                                                                          *
 ****************************************************************************/

/**
 * This is used directly for the little round focus lock button.
 * It is also subclassed by TrackNumber for the more common numeric
 * focus lock button.
 */
class FocusButton : public SpaceComponent, public MouseInputAdapter {

  public:

	FocusButton(class MobiusInterface* m, int trackIndex);

	void setPushed(bool b);
	bool isPushed();

	void mousePressed(MouseEvent* e);
	virtual Dimension* getPreferredSize(Window* w);
	virtual void paint(Graphics* g);

  public:

    class MobiusInterface* mMobius;
    int mTrack;

	bool mPushed;
	int mDiameter;
	Color* mPushColor;

};

class TrackNumber : public FocusButton {

  public:

	TrackNumber(class MobiusInterface* m, int trackIndex);

	void setNumber(int n);
	int getNumber();

    void setName(const char* name);

	Dimension* getPreferredSize(Window* w);
	void paint(Graphics* g);

  private:

    char mName[128];
	Font* mNumberFont;
	Font* mNameFont;

};

#define MAX_GROUP_NAME 128

/**
 * Note that TrackGroup is a Function constant so we have
 * to use a less convenient name.
 */
class TrackGroupButton : public SpaceComponent, public MouseInputAdapter {

  public:
	
    TrackGroupButton(class MobiusInterface* m, int trackIndex);
    ~TrackGroupButton();

	Dimension* getPreferredSize(Window* w);
	void update(int group);
	void paint(Graphics* g);
	void mousePressed(MouseEvent* e);

  private:
    
    class MobiusInterface* mMobius;
    int mTrack;

	Font* mFont;
	char mLabel[MAX_GROUP_NAME];
	int mGroup;

};

#define MAX_MESSAGE 1024

class MessageArea : public SpaceComponent {
  public:

	MessageArea();
	~MessageArea();

    void setDuration(int seconds);
	void add(const char* msg);

	Dimension* getPreferredSize(Window* w);
	void update();
	void paint(Graphics* g);

  private:

	Font* mFont;
	char mMessage[MAX_MESSAGE];
    bool mRefresh;
    int mDuration;
	int mTicks;
};

class TrackStrip : public SpaceComponent {

 public:

	TrackStrip(class MobiusInterface* m, int trackIndex);
	virtual ~TrackStrip();

    
	//void updateConfiguration(StringList* controls);
	void updateConfiguration(StringList* controls, UIConfig* UIConfig);  // #004
	
	void update(class MobiusState* state);
	void paint(Graphics* g);

 private:

    void initComponents();

	class MobiusInterface* mMobius;
    int mTrack;

	FocusButton* mLock;
	TrackNumber* mNumber;
	TrackGroupButton* mGroup;
	ActionKnob* mInput;
	ActionKnob* mOutput;
   	ActionKnob* mFeedback;
   	ActionKnob* mAltFeedback;
	ActionKnob* mPan;
	ActionKnob* mSpeedOctave;
	ActionKnob* mSpeedStep;
	ActionKnob* mSpeedBend;
	ActionKnob* mPitchOctave;
	ActionKnob* mPitchStep;
	ActionKnob* mPitchBend;
	ActionKnob* mTimeStretch;
	Thermometer* mMeter;
	LoopRadar* mRadar;
	AudioMeter* mLevel;
	LoopStack* mLoops;

    Color* mColor;
    Color* mSlowColor;
    Color* mRecordingColor;
    Color* mMuteColor;

};

class TrackStrip2 : public TrackStrip {

 public:
	TrackStrip2(class MobiusInterface* m, int trackIndex);
};

/****************************************************************************
 *                                                                          *
 *   						  PARAMETER DISPLAY                             *
 *                                                                          *
 ****************************************************************************/

class ParameterEditor : public Component, public MouseInputAdapter {

 public:

	ParameterEditor(class MobiusInterface* m, Action* action, Export* exp);
	~ParameterEditor();

	void setFont(Font* f);
    void setValue(int i);
    void setSelected(bool b);
    bool isSelected();
	Dimension* getPreferredSize(Window* w);
	void paint(Graphics* g);
	void update();
	void commit();
    void increment();
    void decrement();

    void mousePressed(MouseEvent* e);
    void mouseDragged(MouseEvent* e);
    void mouseReleased(MouseEvent* e);

 private:

	class MobiusInterface* mMobius;
    Action* mAction;
	Export* mExport;
	Font* mFont;
    Border* mNoBorder;
    Border* mYesBorder;
	char mValue[256];
	int mInt;
    int mMaxValue;
    bool mSelected;

    bool mDragging;
    int mDragStartValue;
    int mDragOriginX;
    int mDragOriginY;
	int mDragChanges;

};

class ParameterDisplay : public SpaceComponent, public ActionListener {

    friend class ParameterEditor;

 public:

	ParameterDisplay(class MobiusInterface* mob);
	~ParameterDisplay();

	void setEnabled(bool b);
	void refresh();
	void actionPerformed(void* src);
	void update(class MobiusState* state);
	void update(StringList* names);
	void paint(Graphics* g);

    int getSelectedIndex();
    void setSelectedIndex(int i);
    void nextParameter();
    void prevParameter();
    void incParameter();
    void decParameter();

	void layout(Window* w);

  protected:

    void setSelectedParameter(ParameterEditor* p);
    ParameterEditor* getSelectedEditor();

 private:

	void updateEnabled();

	class MobiusInterface* mMobius;
	StringList* mNames;
    List* mEditors;

};

/****************************************************************************
 *                                                                          *
 *   							 MODE MARKERS                               *
 *                                                                          *
 ****************************************************************************/

class ModeMarkers : public SpaceComponent {

  public:

	ModeMarkers();
	~ModeMarkers();

	Dimension* getPreferredSize(Window* w);
	void update(class MobiusState* state);
	void paint(Graphics* g);

  protected:

	Font* mFont;
	bool mOverdub;
	bool mMute;
	bool mReverse;
	bool mSpeed;
	bool mRecording;
	bool mTrackSyncMaster;
	bool mOutSyncMaster;
    bool mSolo;
    bool mGlobalMute;
    bool mGlobalPause;
    bool mWindow;
    int mSpeedToggle;
    int mSpeedOctave;
	int mSpeedStep;
    int mSpeedBend;
    int mPitchOctave;
	int mPitchStep;
    int mPitchBend;
    int mTimeStretch;
};

/****************************************************************************
 *                                                                          *
 *   							 SYNC MARKERS                               *
 *                                                                          *
 ****************************************************************************/

class SyncMarkers : public SpaceComponent {

  public:

	SyncMarkers();
	~SyncMarkers();

	Dimension* getPreferredSize(Window* w);
	void update(class MobiusState* state);
	void paint(Graphics* g);

  protected:

	Font* mFont;
	float mTempo;
	bool mDoBeat;
	bool mDoBar;
	int mBeat;
	int mBar;

};

/****************************************************************************
 *                                                                          *
 *   								FRAME                                   *
 *                                                                          *
 ****************************************************************************/

class Instance {

  public:
	
	LoopMeter* meter;

};

class UIFrame : public Frame {

  public:

    UIFrame(Context* con, class MobiusInterface* mobius);
    ~UIFrame();

	void opened();
	void closing();
    void prepareToDelete();

  private:

	class UI* mUI;
	bool mFullScreen;

};

class UI : public ActionListener, 
		   public MenuListener, 
		   public KeyListener,
		   public MobiusListener {
    
  public:

    UI(class MobiusInterface* mobius);
    void prepareToDelete();
    ~UI();

	void open(Window* win, bool plugin);
	class UIConfig* getUIConfig();

	void opened();
	void saveLocations();
    void actionPerformed(void *c);
    void menuSelected(class Menu *src);

	// MobiusListener
	void MobiusAlert(const char *msg);
	void MobiusMessage(const char *msg);
	bool MobiusMidiEvent(MidiEvent* e);
	void MobiusRefresh();
	void MobiusTimeBoundary();
	void MobiusPrompt(Prompt* p);
    void MobiusConfigChanged();
    void MobiusGlobalReset();
    void MobiusAction(Action* a);
    void MobiusRedraw();

	void updateUI();

    // KeyListener
	void keyPressed(KeyEvent* e);
	void keyReleased(KeyEvent* e);

	// MidiEvent listener for the dialogs
	UIMidiEventListener* setMidiEventListener(UIMidiEventListener* l);

	// prompt dialog callback
	void finishPrompt(PromptDialog* d, Prompt* p);

	void closing();

	class MobiusInterface* getMobius();

  private:
	
    void redraw();
	void localize(class MessageCatalog* cat);
	void loadConfiguration();
    void convertKeyConfig();
    void convertButtonConfig();
    UIControl* getUIControl(const char* name);
	void writeConfig(class UIConfig* config);

 	void upgradePalette(class Palette* p);
    void buildMenus(bool plugin);
	Menu* buildFileMenu(bool plugin);
    bool isShowNewStuff();
	Menu* buildConfigMenu();
	Menu* buildHelpMenu();

	void buildDockedTrackStrips(class MobiusConfig* config);
    void refreshPresetMenu();
    void refreshPresetMenu(Menu* menu);
    void refreshSetupMenu();
    void refreshSetupMenu(Menu* menu);
	Button* addButton(Function* f);
	void checkDevices();
    void updateDisplayConfig();
	void updateButtons();
	void saveLocations(Component* c);
    void updateGlobalConfig();
	bool isPushed(void* o);
	void alert(const char* msg);
	void doInvisible();
	void gcPrompts(bool force);
    void showDialog(class Dialog* d);
    void showSystemDialog(class SystemDialog* d);
    void cancelDialogs();

	Window* 		mWindow;
	class MobiusInterface* mMobius;
	char*			mUIConfigFile;
	class UIConfig*	mUIConfig;
	Panel*	   		mButtons;
    Menu*           mPresets;
    Menu*           mPopupPresets;
    Menu*           mSetups;
    Menu*           mPopupSetups;

	MenuBar*		mMenuBar;
	PopupMenu* 		mPopup;
    List*           mDialogs;
	KeyHelpDialog* 	mKeyHelpDialog;
	MidiHelpDialog* mMidiHelpDialog;
	SimpleTimer* 	mTimer;
	UIMidiEventListener* mMidiEventListener;
	
	Space* 			mSpace;
	TrackStrip*	    mFloatingStrip;
	TrackStrip*	    mFloatingStrip2;
	AudioMeter* 	mMeter;
	ModeDisplay*	mStatus;
	LoopMeter*		mLoopMeter;
	EDPDisplay*		mCounter;
    LoopWindow*     mLoopWindow;
    Beaters*        mBeaters;
	LoopList*		mLoopList;
	LayerList*		mLayerList;
    BorderedGrid*   mTrackGrid;

    //Instance mInstances[MAX_TRACKS];
    TrackStrip**	mTracks;
    int             mTrackCount;
    ParameterDisplay* mParameters;
	ModeMarkers* 	mModes;
	SyncMarkers* 	mSync;
	StatusBar* 		mStatusBar;
	PresetAlert*	mAlert;
	MessageArea* 	mMessages;

	PromptDialog* 	mPrompts;
	PromptDialog* 	mPromptsTodo;
	Button* 		mInvisible;

	char			mKeyState[KEY_MAX_CODE];

	// track state we maintain in order to generate status messages
	int 			mLastPreset;

	class CriticalSection* mCsect;

};

class AboutDialog : public SimpleDialog  {

  public:

	AboutDialog(Window* parent);
	~AboutDialog();

	const char* getCancelName();

  private:

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
