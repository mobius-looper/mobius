/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Dialog for the specification of Mobius track setups.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "Util.h"
#include "List.h"
#include "MessageCatalog.h"

#include "Messages.h"
#include "Mobius.h"
#include "MobiusConfig.h"
#include "Parameter.h"
#include "Preset.h"
#include "Recorder.h"
#include "Setup.h"
#include "Track.h"

#include "UI.h"

// sigh...nested modal dialogs don't work on Mac
//#define USE_RENAME_DIALOG 1


static const char* PortNames[] = {
	"1", "2", "3", "4", "5", "6", "7", "8", NULL
};

PUBLIC SetupDialog::SetupDialog(Window* parent, MobiusInterface* mob, 
                                MobiusConfig* config)
{
	int i;

    mMobius = mob;
    mConfig = config;
	mCatalog = mob->getMessageCatalog();

	setParent(parent);

	// !! setting this non-modal causes crashes deep in the window proc
	// need to figure out why
	setModal(true);

	setIcon("Mobius");
	setTitle(mCatalog->get(MSG_DLG_SETUP_TITLE));
	setInsets(20, 20, 20, 0);

	// Get the currently selected setup
	mSetup = mConfig->getCurrentSetup();
    if (mSetup == NULL) {
		mSetup = new Setup();
		mConfig->addSetup(mSetup);
		mConfig->generateNames();
    }

	Panel* root = getPanel();
	VerticalLayout* vl = new VerticalLayout();
	vl->setCenterX(true);
	root->setLayout(vl);
	root->add(new Strut(0, 10));

	FormPanel* form = new FormPanel();
	form->setAlign(FORM_LAYOUT_RIGHT);
	root->add(form);

	mSelector = new ComboBox();
	mSelector->setColumns(20);
	mSelector->addActionListener(this);
	Panel* p = new Panel("Selector");
	p->setLayout(new HorizontalLayout());
	p->add(mSelector);
	p->add(new Strut(20, 0));
	mNew = new Button(mCatalog->get(MSG_DLG_NEW));
	mNew->setFont(Font::getFont("Arial", 0, 8));
	mNew->addActionListener(this);
	p->add(mNew);
	mDelete = new Button(mCatalog->get(MSG_DLG_DELETE));
	mDelete->setFont(Font::getFont("Arial", 0, 8));
	mDelete->addActionListener(this);
	p->add(mDelete);
	mRename = new Button(mCatalog->get(MSG_DLG_RENAME));
	mRename->setFont(Font::getFont("Arial", 0, 8));
	mRename->addActionListener(this);
#ifdef USE_RENAME_DIALOG
	p->add(mRename);
#endif
	form->add(mCatalog->get(MSG_DLG_SETUP_SELECTED), p);

    mName = NULL;
#ifndef USE_RENAME_DIALOG
	p = new Panel();
	p->setLayout(new HorizontalLayout(8));
    mName = new Text();
	mName->addActionListener(this);
    p->add(mName);
    p->add(mRename);
    form->add(mCatalog->get(MSG_DLG_NAME), p);
#endif

	root->add(new Strut(0, 10));
	root->add(new Divider(500));
	root->add(new Strut(0, 10));

    // tabs: Tracks, Synchronization, Options
    TabbedPane* tabs = new TabbedPane();
    root->add(tabs);

    // 
    // Tracks
    //

    Panel* trackPanel = new Panel("Tracks");
    tabs->add(trackPanel);
	trackPanel->setLayout(new VerticalLayout(10));
    // this looks funny but since the VerticalLayout pad is already 10
    // we don't need anything more
    trackPanel->add(new Strut(0, 0));

    Panel* radioPanel = new Panel();
    HorizontalLayout* hl = new HorizontalLayout(10);
    hl->setCenterY(true);
    radioPanel->setLayout(hl);
    radioPanel->add(new Label("Track"));
    
    mTrackNumber = 0;
    mTrackRadio = new Radios();
    mTrackRadio->addActionListener(this);
    for (int i = 0 ; i < mConfig->getTracks() ; i++) {
        char buf[32];
        sprintf(buf, "%d", i + 1);
        mTrackRadio->addLabel(buf);
    }
    mTrackRadio->setSelectedIndex(mTrackNumber);

    radioPanel->add(mTrackRadio);
    trackPanel->add(radioPanel);

    // formerly an array, now just one
    mTrack = new TrackComponents();

    form = new FormPanel();
    form->setAlign(FORM_LAYOUT_RIGHT);
    trackPanel->add(form);

    Text* tx = new Text();
    tx->setColumns(20);
    mTrack->name = tx;
    form->add(mCatalog->get(MSG_DLG_NAME), tx);

    mTrack->syncSource = form->addCombo(NULL, SyncSourceParameter->getDisplayName(), 
                                        SyncSourceParameter->valueLabels);

    mTrack->trackUnit = form->addCombo(NULL, TrackSyncUnitParameter->getDisplayName(), 
                                       TrackSyncUnitParameter->valueLabels);

    // box takes ownership of the name list to have to recalculate
    // it for each one
    ComboBox* cb = new ComboBox(getPresetNames());
    // !! need to be smarter about sizing, default is way too big
    cb->setColumns(20);
    mTrack->preset = cb;
    form->add(mCatalog->get(MSG_DLG_SETUP_PRESET), cb);

    cb = new ComboBox(getGroupNames());
    cb->setColumns(20);
    mTrack->group = cb;
    form->add(mCatalog->get(MSG_DLG_SETUP_GROUP), cb);

    Checkbox* chb = new Checkbox();
    mTrack->focusLock = chb;
    form->add(mCatalog->get(MSG_DLG_SETUP_FOCUS), chb);

    Slider* s = getSlider();
    mTrack->input = s;
    form->add(mCatalog->get(MSG_PARAM_INPUT_LEVEL), s);

    s = getSlider();
    mTrack->output = s;
    form->add(mCatalog->get(MSG_PARAM_OUTPUT_LEVEL), s);

    s = getSlider();
    mTrack->feedback = s;
    form->add(mCatalog->get(MSG_PARAM_FEEDBACK_LEVEL), s);

    s = getSlider();
    mTrack->altFeedback = s;
    form->add(mCatalog->get(MSG_PARAM_ALT_FEEDBACK_LEVEL), s);

    s = getSlider();
    mTrack->pan = s;
    form->add(mCatalog->get(MSG_PARAM_PAN), s);

    Checkbox* ch = new Checkbox();
    mTrack->mono = ch;
    form->add(mCatalog->get(MSG_DLG_SETUP_MONO), ch);

    Panel* extra = new Panel();
    extra->setLayout(new HorizontalLayout(30));
    trackPanel->add(extra);

    FormPanel* formLeft = new FormPanel();
    formLeft->setAlign(FORM_LAYOUT_RIGHT);
    extra->add(formLeft);

    FormPanel* formRight = new FormPanel();
    formRight->setAlign(FORM_LAYOUT_RIGHT);
    extra->add(formRight);

    // For ASIO devices, the ports are variable
    // Note that if we're a VST the AudioDevices will be NULL,
    // don't display any selectors so we don't trash the values!

    MobiusContext* mc = mMobius->getContext();
    if (!mc->isPlugin()) {
        AudioStream* stream = mMobius->getAudioStream();

        int ports = stream->getInputPorts();

        cb = new ComboBox(getPortNames(ports));
        cb->setColumns(2);
        mTrack->audioInputPort = cb;
        formLeft->add(mCatalog->get(MSG_DLG_SETUP_AUDIO_INPUTS), cb);

        ports = stream->getOutputPorts();
        cb = new ComboBox(getPortNames(ports));
        cb->setColumns(2);
        mTrack->audioOutputPort = cb;
        formRight->add(mCatalog->get(MSG_DLG_SETUP_AUDIO_OUTPUTS), cb);
    }

    // The VST could return AudioDevices with details on the
    // port counts.  Currently it doesn't so just assume 8
		
    cb = new ComboBox(PortNames);
    cb->setColumns(2);
    mTrack->pluginInputPort = cb;
    formLeft->add(mCatalog->get(MSG_DLG_SETUP_VST_INPUTS), cb);

    cb = new ComboBox(PortNames);
    cb->setColumns(2);
    mTrack->pluginOutputPort = cb;
    formRight->add(mCatalog->get(MSG_DLG_SETUP_VST_OUTPUTS), cb);

	Panel* buttons = new Panel("buttons");
	buttons->setLayout(new FlowLayout());
	trackPanel->add(new Strut(0, 10));
	trackPanel->add(buttons);

	mInit = new Button(mCatalog->get(MSG_DLG_SETUP_INIT));
	mInit->addActionListener(this);
	buttons->add(mInit);

	mCapture = new Button(mCatalog->get(MSG_DLG_SETUP_CAPTURE));
	mCapture->addActionListener(this);
	buttons->add(mCapture);

	mInitAll = new Button(mCatalog->get(MSG_DLG_SETUP_INIT_ALL));
	mInitAll->addActionListener(this);
	buttons->add(mInitAll);

	mCaptureAll = new Button(mCatalog->get(MSG_DLG_SETUP_CAPTURE_ALL));
	mCaptureAll->addActionListener(this);
	buttons->add(mCaptureAll);
    
    // 
    // Synchronization
    //

    Panel* syncPanel = new Panel("Synchronization");
    tabs->add(syncPanel);
	syncPanel->setLayout(new VerticalLayout());
	syncPanel->add(new Strut(0, 10));

	form = new FormPanel();
	form->setAlign(FORM_LAYOUT_RIGHT);
	syncPanel->add(form);

    int comboCols = 15;
    mSyncSource = addCombo(form, DefaultSyncSourceParameter, comboCols);
	mTrackUnit = addCombo(form, DefaultTrackSyncUnitParameter, comboCols);
    mSyncUnit = addCombo(form, SlaveSyncUnitParameter, comboCols);
    mBeatsPerBar = addNumber(form, BeatsPerBarParameter, 1, 128);
	mRealignTime = addCombo(form, RealignTimeParameter, comboCols);
	mRealignMode = addCombo(form, OutRealignModeParameter, comboCols);
	mMuteSync = addCombo(form, MuteSyncModeParameter, comboCols);
	mResizeSync = addCombo(form, ResizeSyncAdjustParameter, comboCols);
	mSpeedSync = addCombo(form, SpeedSyncAdjustParameter, comboCols);
	mMinTempo = addNumber(form, MinTempoParameter, 20, 500);
	mMaxTempo = addNumber(form, MaxTempoParameter, 20, 500);
	mManualStart = newCheckbox(ManualStartParameter);
    form->add("", mManualStart);

    //
	// Global Options
    //

    Panel* optionPanel = new Panel("Other");
    tabs->add(optionPanel);
	optionPanel->setLayout(new VerticalLayout());
	optionPanel->add(new Strut(0, 10));

	form = new FormPanel();
	form->setAlign(FORM_LAYOUT_RIGHT);
	optionPanel->add(form);

	// only exposing selectors for ports 1-8
	mActive = new ComboBox(PortNames);
	mActive->setColumns(4);
	form->add("Active Track", mActive);

	optionPanel->add(new Strut(0, 10));

	optionPanel->add(new Label("Restore After Reset"));
	mReset = new ListBox();
	mReset->setMultiSelect(true);
	mReset->setColumns(20);
	mReset->setRows(8);
    StringList* paramNames = new StringList();
	for (i = 0 ; Parameters[i] != NULL ; i++) {
        Parameter* p = Parameters[i];
        // !! not everything in setup scope needs to be resettable
        if (p->resettable)
          paramNames->add(p->getDisplayName());
    }
    paramNames->sort();
    mReset->setValues(paramNames);
	optionPanel->add(mReset);

    // Binding Overlay
	optionPanel->add(new Strut(0, 10));
	optionPanel->add(new Label("Binding Overlay"));
	mBindings = new ComboBox();
	mBindings->setColumns(20);
	optionPanel->add(mBindings);
    
	mBindings->addValue("[Retain]");
	mBindings->addValue("[Cancel]");

    BindingConfig* overlays = mConfig->getBindingConfigs();
    // the first one is always on, overlays start after that
    if (overlays != NULL) {
        for (overlays = overlays->getNext() ; overlays != NULL ; 
             overlays = overlays->getNext())
          mBindings->addValue(overlays->getName());
    }

	refreshSelector();
	refreshFields();
}

PRIVATE NumberField* SetupDialog::addNumber(FormPanel* form, Parameter* p, 
											 int min, int max)
{
	return form->addNumber(this, p->getDisplayName(), min, max);
}

PRIVATE ComboBox* SetupDialog::addCombo(FormPanel* form, Parameter* p)
{
	return form->addCombo(this, p->getDisplayName(), p->valueLabels);
}

PRIVATE ComboBox* SetupDialog::addCombo(FormPanel* form, Parameter* p,
                                         int cols)
{
	return form->addCombo(this, p->getDisplayName(), p->valueLabels, cols);
}

PRIVATE Checkbox* SetupDialog::newCheckbox(Parameter* p)
{
	Checkbox* cb = new Checkbox(p->getDisplayName());
	cb->addActionListener(this);
	return cb;
}

PUBLIC void SetupDialog::opened()
{
}

PRIVATE StringList* SetupDialog::getPortNames(int ports)
{
	StringList* list = new StringList();
	char buffer[32];
	
	// had a bug reported where this went haywire
	if (ports > 64) {
		Trace(1, "SetupDialog: Port number overflow %ld\n", (long)ports);
		ports = 64;
	}

	for (int i = 1 ; i <= ports ; i++) {
		sprintf(buffer, "%d", i);
		list->add(buffer);
	}

	return list;
}

PRIVATE Slider* SetupDialog::getSlider()
{
	Slider* s = new Slider(false, true);

	s->setMinimum(0);
	s->setMaximum(127);
	s->setLabelColumns(4);

	// !! should have a smarter default
	s->setSliderLength(256);

	return s;
}

PRIVATE StringList* SetupDialog::getPresetNames()
{
	StringList* list = new StringList();
	list->add(mCatalog->get(MSG_DLG_SELECT_NONE));
	for (Preset* p = mConfig->getPresets() ; p != NULL ; p = p->getNext())
	  list->add(p->getName());
	return list;
}

/**
 * These don't have user defined names yet, just letters.
 */
PRIVATE StringList* SetupDialog::getGroupNames()
{
	StringList* list = new StringList();
	int groups = mConfig->getTrackGroups();
	
	list->add(mCatalog->get(MSG_DLG_SELECT_NONE));
	for (int i = 0 ; i < groups ; i++) {
		char gname[128];
        //sprintf(gname, "Group %c", (char)('A' + i));
        // prefer it without the Group prefix here
		sprintf(gname, "%c", (char)('A' + i));
		list->add(gname);
	}

	return list;
}

/**
 * Initialize a combo box for selecting presets.
 * Name them if they don't already have names.
 */
PRIVATE void SetupDialog::refreshSelector()
{
	mConfig->generateNames();
	mSelector->setValues(NULL);
	for (Setup* p = mConfig->getSetups() ; p != NULL ; p = p->getNext())
	  mSelector->addValue(p->getName());

	mSelector->setSelectedValue(mSetup->getName());
}

void SetupDialog::refreshFields()
{
    if (mName != NULL)
      mName->setValue(mSetup->getName());

    // These are ugly because the SyncSource enumeration contains a Default 
    // item at the top, but the Parameter definition does not include it 
    // because it isn't supposed to be selectable here.  The Parameter
    // interface makes the adjustments but since we're accessing the 
    // Setup directly we have to make the adjustments.
    int index = mSetup->getSyncSource();
    if (index > 0) index--;
	mSyncSource->setValue(index);
    index = mSetup->getSyncTrackUnit();
    if (index > 0) index--;
	mTrackUnit->setValue(index);

	mActive->setValue(mSetup->getActiveTrack());
    mSyncUnit->setValue((int)mSetup->getSyncUnit());
	mMuteSync->setValue((int)mSetup->getMuteSyncMode());
	mResizeSync->setValue((int)mSetup->getResizeSyncAdjust());
	mSpeedSync->setValue((int)mSetup->getSpeedSyncAdjust());
	mRealignTime->setValue((int)mSetup->getRealignTime());
	mRealignMode->setValue((int)mSetup->getOutRealignMode());
	mMinTempo->setValue((int)mSetup->getMinTempo());
	mMaxTempo->setValue((int)mSetup->getMaxTempo());
	mBeatsPerBar->setValue((int)mSetup->getBeatsPerBar());
    mManualStart->setValue(mSetup->isManualStart());

    const char* overlay = mSetup->getBindings();
    if (overlay == NULL)
      mBindings->setSelectedIndex(0);
    else if (StringEqual(overlay, SETUP_OVERLAY_CANCEL))
      mBindings->setSelectedIndex(1);
    else
      mBindings->setSelectedValue(overlay);

	StringList* names = mSetup->getResetables();
	if (names != NULL) {
		StringList* selected = new StringList();
		for (int i = 0 ; i < names->size() ; i++) {
			const char* name = names->getString(i);
			Parameter* p = mMobius->getParameter(name);
			if (p != NULL)
			  selected->add(p->getDisplayName());
		}
		mReset->setSelectedValues(selected);
	}

    refreshTrackFields();
}

void SetupDialog::refreshTrackFields()
{
    SetupTrack* st = mSetup->getTrack(mTrackNumber);
    if (st != NULL) {

        // Preset is a little complicated.  They start off without
        // a Preset but the behavior is to fall back to the first one
        // so we have to make that obvious.
        // NO...we need to be able to select [none] to leave the preset alone
        const char* p = st->getPreset();
        /*
        if (p == NULL) {
            MobiusConfig* config = mMobius->getConfiguration();
            Preset* presets = config->getPresets();
            if (presets == NULL) {
                // not supposed to happen
                Trace(1, "SetupDialog: No presets defined!\n");
            }
            else {
                p = presets->getName();
            }
        }
        */

        if (p != NULL)
          mTrack->preset->setSelectedValue(p);
        else
          mTrack->preset->setSelectedIndex(0);

		mTrack->name->setText(st->getName());
		mTrack->group->setSelectedIndex(st->getGroup());
		mTrack->focusLock->setSelected(st->isFocusLock());

		if (mTrack->audioInputPort != NULL)
		  mTrack->audioInputPort->setSelectedIndex(st->getAudioInputPort());
		if (mTrack->audioOutputPort != NULL)
		  mTrack->audioOutputPort->setSelectedIndex(st->getAudioOutputPort());

		mTrack->pluginInputPort->setSelectedIndex(st->getPluginInputPort());
		mTrack->pluginOutputPort->setSelectedIndex(st->getPluginOutputPort());
		mTrack->input->setValue(st->getInputLevel());
		mTrack->output->setValue(st->getOutputLevel());
		mTrack->feedback->setValue(st->getFeedback());
		mTrack->altFeedback->setValue(st->getAltFeedback());
		mTrack->pan->setValue(st->getPan());
		mTrack->mono->setSelected(st->isMono());

        mTrack->syncSource->setSelectedIndex(st->getSyncSource());
        mTrack->trackUnit->setSelectedIndex(st->getSyncTrackUnit());
	}
}

/**
 * Called as we switch between the different setups to copy
 * any pending changes in the UI components back to the current
 * setup before displaying the next one.
 */
void SetupDialog::captureFields()
{
	// this one requires update of the selector?
    if (mName != NULL) {
        const char* oldName = mSetup->getName();
        const char* newName = mName->getValue();
        if (!StringEqual(oldName, newName)) {
            mSetup->setName(mName->getValue());
            refreshSelector();
        }
    }

    // enumeration has an extra "Default" item at the top that we don't
    // display, have to adjust the indexes
    SyncSource src = (SyncSource)(mSyncSource->getSelectedIndex() + 1);
	mSetup->setSyncSource(src);
    SyncTrackUnit tunit = (SyncTrackUnit)(mTrackUnit->getSelectedIndex() + 1);
	mSetup->setSyncTrackUnit(tunit);

	mSetup->setActiveTrack(mActive->getSelectedIndex());
	mSetup->setSyncUnit((SyncUnit)mSyncUnit->getSelectedIndex());
	mSetup->setMuteSyncMode((MuteSyncMode)mMuteSync->getSelectedIndex());
	mSetup->setResizeSyncAdjust((SyncAdjust)mResizeSync->getSelectedIndex());
	mSetup->setSpeedSyncAdjust(mSpeedSync->getSelectedIndex());
	mSetup->setRealignTime(mRealignTime->getSelectedIndex());
	mSetup->setOutRealignMode(mRealignMode->getSelectedIndex());
	mSetup->setMinTempo(mMinTempo->getValue());
	mSetup->setMaxTempo(mMaxTempo->getValue());
    mSetup->setBeatsPerBar(mBeatsPerBar->getValue());
    mSetup->setManualStart(mManualStart->getValue());

    const char* bindings = mBindings->getSelectedValue();
    int index = mBindings->getSelectedIndex();
    if (index <= 0)
      bindings = NULL;  // retain
    else if (index == 1)
      bindings = SETUP_OVERLAY_CANCEL;
    mSetup->setBindings(bindings);

	StringList* selected = mReset->getSelectedValues();
	if (selected == NULL)
	  mSetup->setResetables(NULL);
	else {
		StringList* names = new StringList();
		for (int i = 0 ; i < selected->size() ; i++) {
			const char* displayName = selected->getString(i);
			Parameter* p = mMobius->getParameterWithDisplayName(displayName);
			if (p != NULL)
			  names->add(p->getName());
		}
		mSetup->setResetables(names);
	}

    captureTrackFields();
}

void SetupDialog::captureTrackFields()
{
    SetupTrack* st = mSetup->getTrack(mTrackNumber);
    if (st != NULL) {
			
        st->setName(mTrack->name->getValue());

        if (mTrack->preset->getSelectedIndex() == 0)
          st->setPreset(NULL);
        else
          st->setPreset(mTrack->preset->getSelectedValue());

		st->setGroup(mTrack->group->getSelectedIndex());
		st->setFocusLock(mTrack->focusLock->isSelected());

		if (mTrack->audioInputPort != NULL)
		  st->setAudioInputPort(mTrack->audioInputPort->getSelectedIndex());
		if (mTrack->audioOutputPort != NULL)
		  st->setAudioOutputPort(mTrack->audioOutputPort->getSelectedIndex());

		st->setPluginInputPort(mTrack->pluginInputPort->getSelectedIndex());
		st->setPluginOutputPort(mTrack->pluginOutputPort->getSelectedIndex());
		st->setInputLevel(mTrack->input->getValue());
		st->setOutputLevel(mTrack->output->getValue());
		st->setFeedback(mTrack->feedback->getValue());
		st->setAltFeedback(mTrack->altFeedback->getValue());
		st->setPan(mTrack->pan->getValue());
		st->setMono(mTrack->mono->isSelected());

        st->setSyncSource((SyncSource)mTrack->syncSource->getSelectedIndex());
        st->setSyncTrackUnit((SyncTrackUnit)mTrack->trackUnit->getSelectedIndex());
	}
}

void SetupDialog::actionPerformed(void *src) 
{
	if (src == mNew) {
		captureFields();
		// clone the current setup, may want an init button?
		Setup* neu = mSetup->clone();
		// null the name so we generate a new one
		neu->setName(NULL);
		mConfig->addSetup(neu);
		mConfig->generateNames();
		// note that we can't call this until we've generated names
		mConfig->setCurrentSetup(neu);
		mSetup = neu;
		refreshSelector();
		refreshFields();
	}
	else if (src == mDelete) {
		captureFields();
		// ignore if there is only one left, may want an init button?
		Setup* setups = mConfig->getSetups();
		if (setups->getNext() != NULL) {
			Setup* next = mSetup->getNext();
			if (next == NULL) {
				for (next = setups ; 
					 next != NULL && next->getNext() != mSetup ; 
					 next = next->getNext());
				if (next == NULL)
				  next = setups;
			}
			mConfig->removeSetup(mSetup);
			delete mSetup;
			mConfig->setCurrentSetup(next);
			mSetup = next;
			refreshSelector();
			refreshFields();
		}
		else {
			// must have at least one setup
			MessageDialog::showError(getParentWindow(), 
									 mCatalog->get(MSG_DLG_ERROR),
									 mCatalog->get(MSG_DLG_SETUP_ONE));
		}
	}
	else if (src == mRename) {
		captureFields();
#ifdef USE_RENAME_DIALOG
        Setup* setup = mConfig->getCurrentSetup();
        RenameDialog* d = new RenameDialog(this, "Rename", "Name", 
                                           setup->getName());
        d->show();
        if (!d->isCanceled()) {
            const char* newName = d->getValue();
            if (newName != NULL && strlen(newName) > 0) {
                // should also trim whitespace!
                if (!StringEqual(setup->getName(), newName)) {
                    setup->setName(newName);
                    refreshSelector();
                }
            }
        }
        delete d;
#else
        // captureFields will have done the work
#endif
	}
	else if (src == mSelector) {
		// captureFields may modify the selector so capture the value first
		// subtle, have to locate the next Preset as well since the name
		// returned by the selector may become invalid
		const char* setupName = mSelector->getValue();
		Setup* s = mConfig->getSetup(setupName);

		captureFields();
		if (s != NULL) {
			mSetup = s;
			mConfig->setCurrentSetup(s);
			refreshFields();
		}
	}
    else if (src == mTrackRadio) {
        captureTrackFields();
        mTrackNumber = mTrackRadio->getSelectedIndex();
        refreshFields();
    }
	else if (src == mInit) {
        SetupTrack* st = mSetup->getTrack(mTrackNumber);
        if (st != NULL) {
			st->reset();
			refreshFields();
		}
	}
	else if (src == mCapture) {
        SetupTrack* st = mSetup->getTrack(mTrackNumber);
        if (st != NULL) {
            MobiusState* state = mMobius->getState(mTrackNumber);
            if (state != NULL) {
                st->capture(state);
            }
            else {
                // not supposed to happen
                st->reset();
            }
            refreshFields();
        }
    }
	else if (src == mInitAll) {
		for (int i = 0 ; i < MAX_UI_TRACKS ; i++) {
			SetupTrack* st = mSetup->getTrack(i);
            if (st != NULL)
              st->reset();
		}
		refreshFields();
	}
	else if (src == mCaptureAll) {
		for (int i = 0 ; i < MAX_UI_TRACKS ; i++) {
            SetupTrack* st = mSetup->getTrack(i);
            if (st != NULL) {
                MobiusState* state = mMobius->getState(i);
                if (state != NULL)
                  st->capture(state);
                else {
                    // not supposed to happen
                    st->reset();
                }
			}
		}
		refreshFields();
	}
	else {
		// must be one of the SimpleDialog buttons
		SimpleDialog::actionPerformed(src);
	}
}

PUBLIC SetupDialog::~SetupDialog()
{
    // caller owns mConfig
    delete mTrack;
}

/**
 * Called by SimpleDialog when the Ok button is pressed.
 */
PUBLIC bool SetupDialog::commit()
{
	// copy any remaining component changes back to the current Setup
	captureFields();
    return true;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
