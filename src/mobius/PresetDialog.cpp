/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Dialog for the specification of Mobius presets.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "Util.h"
#include "MessageCatalog.h"
#include "Qwin.h"

#include "Messages.h"
#include "Mobius.h"
#include "MobiusConfig.h"
#include "Parameter.h"
#include "Preset.h"
#include "Function.h"

#include "UI.h"

// sigh...nested modal dialogs don't work on Mac
//#define USE_RENAME_DIALOG 1


PUBLIC PresetDialog::PresetDialog(Window* parent, MobiusInterface* mob,
                                  MobiusConfig* config)
{
    mMobius = mob;
    mConfig = config;
	mCatalog = mob->getMessageCatalog();

	setParent(parent);

	// !! setting this non-modal causes crashes deep in the window proc
	// need to figure out why
	setModal(true);

	setIcon("Mobius");
	setTitle(mCatalog->get(MSG_DLG_PRESET_TITLE));

	// Get the currently selected track preset, not whatever was left
	// as "current" in the MobiusConfig.
	int index = mob->getTrackPreset();
	mPreset = mConfig->getPreset(index);
    if (mPreset == NULL) {
        // not supposed to happen! bootstrap something just to show
        mConfig->addPreset(new Preset());
		mConfig->generateNames();
        mPreset = mConfig->getCurrentPreset();
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
	Panel* p = new Panel();
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
	form->add(mCatalog->get(MSG_DLG_PRESET_SELECTED), p);

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
	root->add(new Divider(800));
	root->add(new Strut(0, 10));

	TabbedPane* tabs = new TabbedPane();
	root->add(tabs);

	// General tab

	Panel* tabMain = new Panel();
	tabMain->setName("General");
	tabMain->setLayout(new VerticalLayout());
	tabMain->add(new Strut(0, 10));
	tabs->add(tabMain);

	form = new FormPanel();
	form->setAlign(FORM_LAYOUT_RIGHT);
	tabMain->add(form);

	mLoops = addNumber(form, LoopCountParameter, 1, 16);
    // why the silly range restrictiosn?
	mSubcycles = addNumber(form, SubCycleParameter, 1, 96);
  	mSubcycles->addException(128);
	mSubcycles->addException(256);
	mMaxUndo = addNumber(form, MaxUndoParameter, 0, 999999);
	mMaxRedo = addNumber(form, MaxRedoParameter, 0, 999999);
    
	mNoFeedbackUndo = newCheckbox(NoFeedbackUndoParameter);
	form->add("", mNoFeedbackUndo);

	mAltFeedback = newCheckbox(AltFeedbackEnableParameter);
	form->add("", mAltFeedback);

    // keep this hidden until we can make it do somethign useful
	//mNoLayerFlattening = newCheckbox(NoLayerFlatteningParameter);
	//form->add("", mNoLayerFlattening);

    // Quantize tab

	Panel* tabQuantize = new Panel();
	tabQuantize->setName("Quantize");
	tabQuantize->setLayout(new VerticalLayout());
	tabQuantize->add(new Strut(0, 10));
	tabs->add(tabQuantize);

	form = new FormPanel();
	form->setAlign(FORM_LAYOUT_RIGHT);
	tabQuantize->add(form);

	mQuantize = addCombo(form, QuantizeParameter);
	mSwitchQuantize = addCombo(form, SwitchQuantizeParameter);
	mBounceQuantize = addCombo(form, BounceQuantizeParameter);

	mOverdubQuantized = newCheckbox(OverdubQuantizedParameter);
    form->add("", mOverdubQuantized);

    // Record tab

	Panel* tabRecord = new Panel();
	tabRecord->setName("Record");
	tabRecord->setLayout(new VerticalLayout());
	tabRecord->add(new Strut(0, 10));
	tabs->add(tabRecord);

	form = new FormPanel();
	form->setAlign(FORM_LAYOUT_RIGHT);
	tabRecord->add(form);

	mThreshold = addNumber(form, RecordThresholdParameter, 0, 8);
	mAutoRecordBars = addNumber(form, AutoRecordBarsParameter, 1, 1024);
	mAutoRecordTempo = addNumber(form, AutoRecordTempoParameter, 20, 500);

	mSpeedRecord = newCheckbox(SpeedRecordParameter);
    form->add("", mSpeedRecord);
	mRecordFeedback = newCheckbox(RecordResetsFeedbackParameter);
    form->add("", mRecordFeedback);

	// Switch tab

	Panel* tabSwitch = new Panel();
	tabSwitch->setName("Switch");
	tabSwitch->setLayout(new VerticalLayout());
	tabSwitch->add(new Strut(0, 10));
	tabs->add(tabSwitch);

    Panel* forms = new Panel();
	forms->setLayout(new HorizontalLayout(20));
	tabSwitch->add(forms);

    // left switch form
	form = new FormPanel();
	form->setAlign(FORM_LAYOUT_RIGHT);
	forms->add(form);

    mEmptyLoopAction = addCombo(form, EmptyLoopActionParameter);
    // not technically a switch parameter but it fits nicely here
	mEmptyTrackAction = addCombo(form, EmptyTrackActionParameter);
    mTrackLeaveAction = addCombo(form, TrackLeaveActionParameter);

	mTimeCopy = addCombo(form, TimeCopyParameter);
	mSoundCopy = addCombo(form, SoundCopyParameter);

	mSwitchLocation = addCombo(form, SwitchLocationParameter);
	mSwitchDuration = addCombo(form, SwitchDurationParameter);
	mReturnLocation = addCombo(form, ReturnLocationParameter);

	mVelocity = newCheckbox(SwitchVelocityParameter);
	form->add("", mVelocity);

    // right switch form
	form = new FormPanel();
	form->setAlign(FORM_LAYOUT_RIGHT);
	forms->add(form);

	mRecordTransfer = addCombo(form, RecordTransferParameter);
	mOverdubTransfer = addCombo(form, OverdubTransferParameter);
	mReverseTransfer = addCombo(form, ReverseTransferParameter);
	mSpeedTransfer = addCombo(form, SpeedTransferParameter);
	mPitchTransfer = addCombo(form, PitchTransferParameter);

    // Options

	Panel* tabOptions = new Panel();
	tabOptions->setName("Functions");
	tabOptions->setLayout(new VerticalLayout());
	tabOptions->add(new Strut(0, 10));
	tabs->add(tabOptions);

    forms = new Panel();
	forms->setLayout(new HorizontalLayout(20));
	tabOptions->add(forms);

	form = new FormPanel();
	form->setAlign(FORM_LAYOUT_RIGHT);
	forms->add(form);

	mMultiplyMode = addCombo(form, MultiplyModeParameter);
	mShuffleMode = addCombo(form, ShuffleModeParameter);
	mMuteMode = addCombo(form, MuteModeParameter);
	mMuteCancel = addCombo(form, MuteCancelParameter);
	mSlipMode = addCombo(form, SlipModeParameter);
	mSlipTime = addNumber(form, SlipTimeParameter, 0, 999999);
    mWindowSlideUnit = addCombo(form, WindowSlideUnitParameter);
    mWindowSlideAmount = addNumber(form, WindowSlideAmountParameter, 0, 999);
    mWindowEdgeUnit = addCombo(form, WindowEdgeUnitParameter);
    mWindowEdgeAmount = addNumber(form, WindowEdgeAmountParameter, 0, 999);

	form = new FormPanel();
	form->setAlign(FORM_LAYOUT_RIGHT);
	forms->add(form);

	mRoundingOverdub = newCheckbox(RoundingOverdubParameter);
    form->add("", mRoundingOverdub);

	// Effects tab

	Panel* tabEffect = new Panel();
	tabEffect->setName("Effects");
	tabEffect->setLayout(new VerticalLayout());
	tabEffect->add(new Strut(0, 10));
	tabs->add(tabEffect);

	form = new FormPanel();
	form->setAlign(FORM_LAYOUT_RIGHT);
	tabEffect->add(form);

	mSpeedSequence = form->addText(this, SpeedSequenceParameter->getDisplayName());
	mPitchSequence = form->addText(this, PitchSequenceParameter->getDisplayName());

	mSpeedRestart = newCheckbox(SpeedShiftRestartParameter);
	form->add("", mSpeedRestart);
	mPitchRestart = newCheckbox(PitchShiftRestartParameter);
	form->add("", mPitchRestart);

    mSpeedStep =  addNumber(form, SpeedStepRangeParameter);
    mSpeedBend =  addNumber(form, SpeedBendRangeParameter);
    mPitchStep =  addNumber(form, PitchStepRangeParameter);
    mPitchBend =  addNumber(form, PitchBendRangeParameter);
    mTimeStretch =  addNumber(form, TimeStretchRangeParameter);

    // Sustain tab

	Panel* tabSustain = new Panel();
	tabSustain->setName("Sustain");
	tabSustain->setLayout(new VerticalLayout());
	tabs->add(tabSustain);

    /*
	form = new FormPanel();
	form->setAlign(FORM_LAYOUT_RIGHT);
	tabSustain->add(form);
    */

	tabSustain->add(new Strut(0, 10));
	tabSustain->add(new Label("Sustain Functions"));
	mSustainFunctions = new MultiSelect(true);
	tabSustain->add(mSustainFunctions);
	mSustainFunctions->setColumns(20);
	mSustainFunctions->setRows(7);

	StringList* allowed = new StringList();
	Function** functions = mob->getFunctions();
	for (int i = 0 ; functions[i] != NULL ; i++) {
		Function* f = functions[i];
		if (f->maySustain)
		  allowed->add(f->getDisplayName());
	}
    allowed->sort();
	mSustainFunctions->setAllowedValues(allowed);

	refreshSelector();
	refreshFields();
}

PUBLIC PresetDialog::~PresetDialog()
{
    // caller owns mConfig
}

PRIVATE NumberField* PresetDialog::addNumber(FormPanel* form, Parameter* p)
{
	return form->addNumber(this, p->getDisplayName(), p->getLow(), p->getHigh(mMobius));
}

PRIVATE NumberField* PresetDialog::addNumber(FormPanel* form, Parameter* p, 
											 int min, int max)
{
	return form->addNumber(this, p->getDisplayName(), min, max);
}

PRIVATE ComboBox* PresetDialog::addCombo(FormPanel* form, Parameter* p)
{
    // default is 10 which is too short?
    // I don't know what these numbers mean but it isn't "characters"
    // there is a multiplication applied which makes it way too big
	return form->addCombo(this, p->getDisplayName(), p->valueLabels, 11);
}

PRIVATE ComboBox* PresetDialog::addCombo(FormPanel* form, Parameter* p,
                                         int cols)
{
	return form->addCombo(this, p->getDisplayName(), p->valueLabels, cols);
}

PRIVATE Checkbox* PresetDialog::newCheckbox(Parameter* p)
{
	Checkbox* cb = new Checkbox(p->getDisplayName());
	cb->addActionListener(this);
	return cb;
}

/**
 * Initialize a combo box for selecting presets.
 * Name them if they don't already have names.
 */
PRIVATE void PresetDialog::refreshSelector()
{
	mConfig->generateNames();
	mSelector->setValues(NULL);
	for (Preset* p = mConfig->getPresets() ; 
		 p != NULL ; p = p->getNext())
	  mSelector->addValue(p->getName());

	mSelector->setSelectedValue(mPreset->getName());
}

void PresetDialog::refreshFields()
{
    if (mName != NULL)
      mName->setValue(mPreset->getName());
	mSubcycles->setValue(mPreset->getSubcycles());
	mAltFeedback->setValue(mPreset->isAltFeedbackEnable());
	mSpeedRecord->setValue(mPreset->isSpeedRecord());
	mRecordFeedback->setValue(mPreset->isRecordResetsFeedback());
	mMultiplyMode->setValue((int)mPreset->getMultiplyMode());
	mShuffleMode->setValue((int)mPreset->getShuffleMode());
	mEmptyLoopAction->setValue((int)mPreset->getEmptyLoopAction());
	mEmptyTrackAction->setValue((int)mPreset->getEmptyTrackAction());
    mTrackLeaveAction->setValue((int)mPreset->getTrackLeaveAction());
	mLoops->setValue(mPreset->getLoops());
	mMuteMode->setValue((int)mPreset->getMuteMode());
	mMuteCancel->setValue((int)mPreset->getMuteCancel());
	mOverdubQuantized->setValue(mPreset->isOverdubQuantized());
	mRecordTransfer->setValue((int)mPreset->getRecordTransfer());
	mOverdubTransfer->setValue((int)mPreset->getOverdubTransfer());
	mReverseTransfer->setValue((int)mPreset->getReverseTransfer());
	mSpeedTransfer->setValue((int)mPreset->getSpeedTransfer());
	mPitchTransfer->setValue((int)mPreset->getPitchTransfer());
	mQuantize->setValue((int)mPreset->getQuantize());
	mBounceQuantize->setValue((int)mPreset->getBounceQuantize());
	mRoundingOverdub->setValue(mPreset->isRoundingOverdub());
	mSwitchLocation->setValue((int)mPreset->getSwitchLocation());
	mSwitchDuration->setValue((int)mPreset->getSwitchDuration());
	mReturnLocation->setValue((int)mPreset->getReturnLocation());
	mTimeCopy->setValue((int)mPreset->getTimeCopyMode());
	mSoundCopy->setValue((int)mPreset->getSoundCopyMode());
	mSwitchQuantize->setValue((int)mPreset->getSwitchQuantize());

	mSlipTime->setValue((int)mPreset->getSlipTime());
	mSlipMode->setValue((int)mPreset->getSlipMode());

    mWindowSlideUnit->setValue((int)mPreset->getWindowSlideUnit());
    mWindowSlideAmount->setValue((int)mPreset->getWindowSlideAmount());
    mWindowEdgeUnit->setValue((int)mPreset->getWindowEdgeUnit());
    mWindowEdgeAmount->setValue((int)mPreset->getWindowEdgeAmount());


	mAutoRecordTempo->setValue((int)mPreset->getAutoRecordTempo());
	mAutoRecordBars->setValue((int)mPreset->getAutoRecordBars());
	mThreshold->setValue(mPreset->getRecordThreshold());
	mVelocity->setValue(mPreset->isSwitchVelocity());
	mMaxUndo->setValue(mPreset->getMaxUndo());
	mMaxRedo->setValue(mPreset->getMaxRedo());
	mNoFeedbackUndo->setValue(mPreset->isNoFeedbackUndo());
	//mNoLayerFlattening->setValue(mPreset->isNoLayerFlattening());
	mSpeedRestart->setValue(mPreset->isSpeedShiftRestart());
	mPitchRestart->setValue(mPreset->isPitchShiftRestart());
	mSpeedSequence->setValue(mPreset->getSpeedSequence()->getSource());
	mPitchSequence->setValue(mPreset->getPitchSequence()->getSource());

    mSpeedStep->setValue(mPreset->getSpeedStepRange());
    mSpeedBend->setValue(mPreset->getSpeedBendRange());
    mPitchStep->setValue(mPreset->getPitchStepRange());
    mPitchBend->setValue(mPreset->getPitchBendRange());
    mTimeStretch->setValue(mPreset->getTimeStretchRange());

    const char* susfuncs = mPreset->getSustainFunctions();
    if (susfuncs != NULL) {
        StringList* suslist = new StringList(susfuncs);
		// convert to display names
        Function** functions = mMobius->getFunctions();
		StringList* names = new StringList();
		for (int i = 0 ; functions[i] != NULL ; i++) {
			Function* f = functions[i];
			if (suslist->contains(f->getName()))
			  names->add(f->getDisplayName());
		}
        names->sort();
        mSustainFunctions->setValues(names);
	}

}

/**
 * Capture the current state of the fields into the preset.
 */
void PresetDialog::captureFields()
{
	// this one requires update of the selector
    if (mName != NULL) {
        const char* oldName = mPreset->getName();
        const char* newName = mName->getValue();
        if (!StringEqual(oldName, newName)) {
            mPreset->setName(mName->getValue());
            refreshSelector();
        }
    }
	mPreset->setSubcycles(mSubcycles->getValue());
	mPreset->setAltFeedbackEnable(mAltFeedback->getValue());
	mPreset->setSpeedRecord(mSpeedRecord->getValue());
	mPreset->setRecordResetsFeedback(mRecordFeedback->getValue());
	mPreset->setMultiplyMode(mMultiplyMode->getSelectedIndex());
	mPreset->setShuffleMode(mShuffleMode->getSelectedIndex());
	mPreset->setEmptyLoopAction(mEmptyLoopAction->getSelectedIndex());
	mPreset->setEmptyTrackAction(mEmptyTrackAction->getSelectedIndex());
	mPreset->setTrackLeaveAction(mTrackLeaveAction->getSelectedIndex());
	mPreset->setLoops(mLoops->getValue());
	mPreset->setMuteMode(mMuteMode->getSelectedIndex());
	mPreset->setMuteCancel(mMuteCancel->getSelectedIndex());
	mPreset->setOverdubQuantized(mOverdubQuantized->getValue());
	mPreset->setRecordTransfer(mRecordTransfer->getSelectedIndex());
	mPreset->setOverdubTransfer(mOverdubTransfer->getSelectedIndex());
	mPreset->setReverseTransfer(mReverseTransfer->getSelectedIndex());
	mPreset->setSpeedTransfer(mSpeedTransfer->getSelectedIndex());
	mPreset->setPitchTransfer(mPitchTransfer->getSelectedIndex());
	mPreset->setQuantize(mQuantize->getSelectedIndex());
	mPreset->setBounceQuantize(mBounceQuantize->getSelectedIndex());
	mPreset->setRoundingOverdub(mRoundingOverdub->getValue());
	mPreset->setSwitchLocation(mSwitchLocation->getSelectedIndex());
	mPreset->setSwitchDuration(mSwitchDuration->getSelectedIndex());
	mPreset->setReturnLocation(mReturnLocation->getSelectedIndex());
	mPreset->setTimeCopyMode(mTimeCopy->getSelectedIndex());
	mPreset->setSoundCopyMode(mSoundCopy->getSelectedIndex());
	mPreset->setSwitchQuantize(mSwitchQuantize->getSelectedIndex());
	mPreset->setSlipMode(mSlipMode->getSelectedIndex());
	mPreset->setSlipTime(mSlipTime->getValue());
    mPreset->setWindowSlideUnit((Preset::WindowUnit)mWindowSlideUnit->getSelectedIndex());
    mPreset->setWindowSlideAmount(mWindowSlideAmount->getValue());
    mPreset->setWindowEdgeUnit((Preset::WindowUnit)mWindowEdgeUnit->getSelectedIndex());
    mPreset->setWindowEdgeAmount(mWindowEdgeAmount->getValue());
	mPreset->setAutoRecordTempo(mAutoRecordTempo->getValue());
	mPreset->setAutoRecordBars(mAutoRecordBars->getValue());
	mPreset->setRecordThreshold(mThreshold->getValue());
	mPreset->setSwitchVelocity(mVelocity->getValue());
	mPreset->setMaxUndo(mMaxUndo->getValue());
	mPreset->setMaxRedo(mMaxRedo->getValue());
	mPreset->setNoFeedbackUndo(mNoFeedbackUndo->getValue());
	//mPreset->setNoLayerFlattening(mNoLayerFlattening->getValue());
	mPreset->setSpeedShiftRestart(mSpeedRestart->getValue());
	mPreset->setSpeedSequence(mSpeedSequence->getValue());
	mPreset->setPitchSequence(mPitchSequence->getValue());
	mPreset->setPitchShiftRestart(mPitchRestart->getValue());

    mPreset->setSpeedStepRange(mSpeedStep->getValue());
    mPreset->setSpeedBendRange(mSpeedBend->getValue());
    mPreset->setPitchStepRange(mPitchStep->getValue());
    mPreset->setPitchBendRange(mPitchBend->getValue());
    mPreset->setTimeStretchRange(mTimeStretch->getValue());

	StringList* dispnames = mSustainFunctions->getValues();
	if (dispnames != NULL) {
		StringList* functions = new StringList();
		for (int i = 0 ; i < dispnames->size() ; i++) {
			Function* f = mMobius->getFunction(dispnames->getString(i));
			if (f != NULL)
			  functions->add(f->getName());
		}
		mPreset->setSustainFunctions(functions->toCsv());
	}

}

/**
 * We've got action listeners on everyting but we're no longer
 * maintaining the backing model incrementally, just wait until captureFields().
 */
void PresetDialog::actionPerformed(void *c) 
{
	if (c == mNew) {
		captureFields();
		// clone the current preset, may want an init button?
		Preset* neu = mPreset->clone();
		// null the name so we generate a new one
		neu->setName(NULL);
		mConfig->addPreset(neu);
		mConfig->generateNames();
		mConfig->setCurrentPreset(neu);
		mPreset = neu;
		refreshSelector();
		refreshFields();
	}
	else if (c == mDelete) {
		captureFields();
		// ignore if there is only one left, may want an init button?
		Preset* presets = mConfig->getPresets();
		if (presets->getNext() != NULL) {
			Preset* next = mPreset->getNext();
			if (next == NULL) {
				for (next = presets ; 
					 next != NULL && next->getNext() != mPreset ; 
					 next = next->getNext());
				if (next == NULL)
				  next = presets;
			}
			mConfig->removePreset(mPreset);
			delete mPreset;
			mConfig->setCurrentPreset(next);
			mPreset = next;
			refreshSelector();
			refreshFields();
		}
		else {
			// must have at least one preset
			MessageDialog::showError(getParentWindow(), 
									 mCatalog->get(MSG_DLG_ERROR),
									 mCatalog->get(MSG_DLG_PRESET_ONE));
		}
	}
	else if (c == mRename) {
		captureFields();
        // if we're not using a dialog the call to captureFields will
        // have updated the selector
#ifdef USE_RENAME_DIALOG
        Preset* preset = mConfig->getCurrentPreset();
        RenameDialog* d = new RenameDialog(this, "Rename", "Name", 
                                           preset->getName());
        d->show();
        if (!d->isCanceled()) {
            const char* newName = d->getValue();
            if (newName != NULL && strlen(newName) > 0) {
                // should also trim whitespace!
                if (!StringEqual(preset->getName(), newName)) {
                    preset->setName(newName);
                    refreshSelector();
                }
            }
        }
        delete d;
#endif
	}
	else if (c == mSelector) {
		// captureFields may modify the selector so capture the value first
		// subtle, have to locate the next Preset as well since the name
		// returned by the selector may become invalid
		const char* presetName = mSelector->getValue();
		Preset* p = mConfig->getPreset(presetName);

		captureFields();
		if (p != NULL) {
			mPreset = p;
			mConfig->setCurrentPreset(p);
			mSelector->setSelectedValue(p->getName());
			refreshFields();
		}
	}
	else {
		// must be one of the SimpleDialog buttons
		SimpleDialog::actionPerformed(c);
	}
}

/**
 * Called by SimpleDialog when the Ok button is pressed.
 */
PUBLIC bool PresetDialog::commit()
{
	captureFields();
    return true;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
