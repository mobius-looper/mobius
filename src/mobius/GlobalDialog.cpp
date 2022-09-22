/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Dialog for specification of global Mobius parameters.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "Util.h"
#include "MessageCatalog.h"
#include "Qwin.h"

#include "Function.h"
#include "Messages.h"
#include "MobiusConfig.h"
#include "Mode.h"
#include "Parameter.h"

#include "UIConfig.h"
#include "UITypes.h"
#include "UI.h"

// one of the few places we need to know specific events
#include "Event.h"


PUBLIC GlobalDialog::GlobalDialog(Window* parent, 
                                  MobiusInterface* mob, 
                                  MobiusConfig* c,
                                  UIConfig* uiconfig)
{
	int i;

    mMobius = mob;
	mCatalog = mob->getMessageCatalog();
	mConfig = c;
    mUIConfig = uiconfig;

	setParent(parent);
	setModal(true);
	setIcon("Mobius");
	setTitle(mCatalog->get(MSG_DLG_GLOBAL_TITLE));
	setInsets(20, 20, 20, 0);


	Panel* root = getPanel();

	TabbedPane* tabs = new TabbedPane();
	root->add(tabs);

	// Main tab
	Panel* params = new Panel("Miscellaneous");
	params->setLayout(new VerticalLayout());
	tabs->add(params);
    params->add(new Strut(0, 20));

	FormPanel* form = new FormPanel();
	// looks better without it
	//form->setAlign(FORM_LAYOUT_RIGHT);
	params->add(form);

	mQuickSave = form->addText(this, QuickSaveParameter->getDisplayName());
	mCustomMessageFile = form->addText(this, CustomMessageFileParameter->getDisplayName());
	mLongPress = addNumber(form, LongPressParameter);
	mSpreadRange = addNumber(form, SpreadRangeParameter);
    mMessageDuration = addNumber(form, MessageDurationParameter);
	mNoiseFloor = addNumber(form, NoiseFloorParameter);

	mDualPluginWindow = addCheckbox(form, DualPluginWindowParameter);
	mFileFormat = addCheckbox(form, IntegerWaveFileParameter);
 	mMonitor = addCheckbox(form, MonitorAudioParameter);
    mAutoFeedback = addCheckbox(form, AutoFeedbackReductionParameter);
	mGroupFocusLock = addCheckbox(form, GroupFocusLockParameter);
	mMidiExport = addCheckbox(form, MidiExportParameter);
	mHostMidiExport = addCheckbox(form, HostMidiExportParameter);

    Panel* limits = new Panel("Limits");
	limits->setLayout(new VerticalLayout());
	tabs->add(limits);
    limits->add(new Strut(0, 20));
    form = new FormPanel();
	limits->add(form);
 	mTracks = addNumber(form, TracksParameter);
    mTrackGroups = addNumber(form, TrackGroupsParameter);
    mMaxLoops = addNumber(form, MaxLoopsParameter);
 	mPluginPorts = addNumber(form, PluginPortsParameter);

	Panel* funcs = new Panel("Functions");
	funcs->setLayout(new VerticalLayout());
	tabs->add(funcs);

    // Focus Lock Functions

    funcs->add(new Strut(0, 10));
	funcs->add(new Label(FocusLockFunctionsParameter->getDisplayName()));
	mFocusLockFunctions = new MultiSelect(true);
	funcs->add(mFocusLockFunctions);
	mFocusLockFunctions->setColumns(20);
	mFocusLockFunctions->setRows(7);

	StringList* allowed = new StringList();
	Function** functions = mob->getFunctions();
	for (i = 0 ; functions[i] != NULL ; i++) {
		Function* f = functions[i];
		// scripts are always implicitly allowed
		if (!f->scriptOnly && !f->noFocusLock && 
			f->eventType != RunScriptEvent)
		  allowed->add(f->getDisplayName());
	}
    allowed->sort();
	mFocusLockFunctions->setAllowedValues(allowed);

	StringList* current = mConfig->getFocusLockFunctions();
	if (current != NULL) {
		// convert to display names
		StringList* names = new StringList();
		for (i = 0 ; functions[i] != NULL ; i++) {
			Function* f = functions[i];
			if (current->contains(f->getName()))
			  names->add(f->getDisplayName());
		}
		current = names;
	}
	else {
		// bootstrap an initial list of everything
		current = new StringList();
		for (i = 0 ; functions[i] != NULL ; i++) {
			Function* f = functions[i];
			// scripts are always implicitly allowed
			if (!f->scriptOnly && !f->noFocusLock && 
                f->eventType != RunScriptEvent)
			  current->add(f->getDisplayName());
		}
	}
    current->sort();
	mFocusLockFunctions->setValues(current);

    // Mute Cancel Functions

	funcs->add(new Label(MuteCancelFunctionsParameter->getDisplayName()));
	mMuteCancelFunctions = new MultiSelect(true);
	funcs->add(mMuteCancelFunctions);
	mMuteCancelFunctions->setColumns(20);
	mMuteCancelFunctions->setRows(7);

	allowed = new StringList();
	functions = mob->getFunctions();
	for (i = 0 ; functions[i] != NULL ; i++) {
		Function* f = functions[i];
		if (f->mayCancelMute)
		  allowed->add(f->getDisplayName());
	}
    allowed->sort();
	mMuteCancelFunctions->setAllowedValues(allowed);

	current = mConfig->getMuteCancelFunctions();
	if (current != NULL) {
		// convert to display names, but keep in Function order
		StringList* names = new StringList();
		for (i = 0 ; functions[i] != NULL ; i++) {
			Function* f = functions[i];
			if (current->contains(f->getName()))
			  names->add(f->getDisplayName());
		}
		current = names;
        current->sort();
	}
	mMuteCancelFunctions->setValues(current);

    // Confirmation Functions

	funcs->add(new Label(ConfirmationFunctionsParameter->getDisplayName()));
	mConfirmationFunctions = new MultiSelect(true);
	funcs->add(mConfirmationFunctions);
	mConfirmationFunctions->setColumns(20);
	mConfirmationFunctions->setRows(7);

	allowed = new StringList();
	functions = mob->getFunctions();
	for (i = 0 ; functions[i] != NULL ; i++) {
		Function* f = functions[i];
		if (f->mayConfirm)
		  allowed->add(f->getDisplayName());
	}
    allowed->sort();
	mConfirmationFunctions->setAllowedValues(allowed);

	current = mConfig->getConfirmationFunctions();
	if (current != NULL) {
		// convert to display names, but keep in Function order
		StringList* names = new StringList();
		for (i = 0 ; functions[i] != NULL ; i++) {
			Function* f = functions[i];
			if (current->contains(f->getName()))
			  names->add(f->getDisplayName());
		}
		current = names;
        current->sort();
	}
	mConfirmationFunctions->setValues(current);

	Panel* modetab = new Panel("Modes");
	modetab->setLayout(new VerticalLayout());
	tabs->add(modetab);

    modetab->add(new Strut(0, 20));
	// NOTE: This is actually a *disable* list, not an *enable* list
	// so if it is empty, alt feedback is enabled in the relevant modes
	modetab->add(new Label(AltFeedbackDisableParameter->getDisplayName()));
	mFeedbackModes = new MultiSelect(true);
	modetab->add(mFeedbackModes);
	mFeedbackModes->setColumns(20);
	mFeedbackModes->setRows(7);

	allowed = new StringList();
	MobiusMode** modes = mob->getModes();
	for (i = 0 ; modes[i] != NULL ; i++) {
		MobiusMode* m = modes[i];
		if (m->altFeedbackSensitive)
		  allowed->add(m->getDisplayName());
	}
	mFeedbackModes->setAllowedValues(allowed);

	current = mConfig->getAltFeedbackDisables();
	if (current != NULL) {
		// convert to display names, but keep in Mode order
		StringList* names = new StringList();
		for (int i = 0 ; modes[i] != NULL ; i++) {
			MobiusMode* m = modes[i];
			if (m->altFeedbackSensitive && current->contains(m->getName()))
			  names->add(m->getDisplayName());
		}
		current = names;
	}
	mFeedbackModes->setValues(current);
	
	Panel* advanced = new Panel("Advanced");
	advanced->setLayout(new VerticalLayout());
	tabs->add(advanced);
    advanced->add(new Strut(0, 20));
    advanced->add(new Label("The following parameters are either experimental or intended only for debugging.", Color::Red));
    advanced->add(new Label("Do not change these without contacting the developers.", Color::Red));
    advanced->add(new Strut(0, 20));
    
	form = new FormPanel();
	advanced->add(form);

    // this is no longer exposed because the restricted range isn't useful
	//mFadeFrames = addNumber(form, FadeFramesParameter);
	mTracePrintLevel = addNumber(form, TracePrintLevelParameter);
	mTraceDebugLevel = addNumber(form, TraceDebugLevelParameter);
	mMaxDrift = addNumber(form, MaxSyncDriftParameter);
	mSaveLayers = addCheckbox(form, SaveLayersParameter);
	mLogStatus = addCheckbox(form, LogStatusParameter);

    mOscEnable = addCheckbox(form, OscEnableParameter);
    mOscTrace = addCheckbox(form, OscTraceParameter);
    mOscInput = addNumber(form, OscInputPortParameter);
    mOscOutput = addNumber(form, OscOutputPortParameter);
	mOscHost = form->addText(this, OscOutputHostParameter->getDisplayName());

    // !! why was this removed, does it conflict with flattening?
    //mIsolate = addCheckbox(form, IsolateOverdubsParameter);
	mIsolate = NULL;

	refreshFields();
}

NumberField* GlobalDialog::addNumber(FormPanel* form, Parameter* p)
{
	return form->addNumber(this, p->getDisplayName(), p->getLow(), p->getHigh(mMobius));
}

NumberField* GlobalDialog::addNumber(FormPanel* form, UIParameter* p)
{
    // don't have low and high in the model....think strongly
    // about factoring out a common interface for UI parameters
    // and Mobius parameters, maybe include Variables?

	return form->addNumber(this, p->getDisplayName(), -1, 9999999);
}

Checkbox* GlobalDialog::addCheckbox(FormPanel* form, Parameter* p)
{
	return form->addCheckbox(this, p->getDisplayName());
}

void GlobalDialog::refreshFields()
{
	mQuickSave->setValue(mConfig->getQuickSave());
	mCustomMessageFile->setValue(mConfig->getCustomMessageFile());
	mTracks->setValue(mConfig->getTracks());
	mTrackGroups->setValue(mConfig->getTrackGroups());
	mMaxLoops->setValue(mConfig->getMaxLoops());
	mPluginPorts->setValue(mConfig->getPluginPorts());
	mNoiseFloor->setValue(mConfig->getNoiseFloor());
	//mFadeFrames->setValue(mConfig->getFadeFrames());
	mLongPress->setValue(mConfig->getLongPress());
	mMaxDrift->setValue(mConfig->getMaxSyncDrift());
	mSpreadRange->setValue(mConfig->getSpreadRange());
	mTracePrintLevel->setValue(mConfig->getTracePrintLevel());
	mTraceDebugLevel->setValue(mConfig->getTraceDebugLevel());
	mAutoFeedback->setValue(mConfig->isAutoFeedbackReduction());
	mSaveLayers->setValue(mConfig->isSaveLayers());
	mLogStatus->setValue(mConfig->isLogStatus());
	mMonitor->setValue(mConfig->isMonitorAudio());
    if (mIsolate != NULL)
      mIsolate->setValue(mConfig->isIsolateOverdubs());
	mFileFormat->setValue(mConfig->isIntegerWaveFile());
	mMidiExport->setValue(mConfig->isMidiExport());
	mHostMidiExport->setValue(mConfig->isHostMidiExport());
	mGroupFocusLock->setValue(mConfig->isGroupFocusLock());
    mDualPluginWindow->setValue(mConfig->isDualPluginWindow());
    mMessageDuration->setValue(mUIConfig->getMessageDuration());
	mOscInput->setValue(mConfig->getOscInputPort());
	mOscOutput->setValue(mConfig->getOscOutputPort());
	mOscHost->setValue(mConfig->getOscOutputHost());
    mOscTrace->setValue(mConfig->isOscTrace());
    mOscEnable->setValue(mConfig->isOscEnable());
}

PUBLIC GlobalDialog::~GlobalDialog()
{
}

bool GlobalDialog::commit()
{
	mConfig->setQuickSave(mQuickSave->getValue());
	mConfig->setCustomMessageFile(mCustomMessageFile->getValue());
    mConfig->setTracks(mTracks->getValue());
    mConfig->setTrackGroups(mTrackGroups->getValue());
    mConfig->setMaxLoops(mMaxLoops->getValue());
    mConfig->setPluginPorts(mPluginPorts->getValue());
    mConfig->setNoiseFloor(mNoiseFloor->getValue());
    //mConfig->setFadeFrames(mFadeFrames->getValue());
    mConfig->setLongPress(mLongPress->getValue());
    mConfig->setMaxSyncDrift(mMaxDrift->getValue());
    mConfig->setSpreadRange(mSpreadRange->getValue());
	mConfig->setTracePrintLevel(mTracePrintLevel->getValue());
	mConfig->setTraceDebugLevel(mTraceDebugLevel->getValue());
	mConfig->setAutoFeedbackReduction(mAutoFeedback->getValue());
	mConfig->setSaveLayers(mSaveLayers->getValue());
	mConfig->setLogStatus(mLogStatus->getValue());
	mConfig->setMonitorAudio(mMonitor->getValue());
    if (mIsolate != NULL)
      mConfig->setIsolateOverdubs(mIsolate->getValue());
	mConfig->setIntegerWaveFile(mFileFormat->getValue());
	mConfig->setMidiExport(mMidiExport->getValue());
	mConfig->setHostMidiExport(mHostMidiExport->getValue());
	mConfig->setGroupFocusLock(mGroupFocusLock->getValue());
	mConfig->setDualPluginWindow(mDualPluginWindow->getValue());
    mConfig->setOscInputPort(mOscInput->getValue());
    mConfig->setOscOutputPort(mOscOutput->getValue());
    mConfig->setOscOutputHost(mOscHost->getValue());
    mConfig->setOscTrace(mOscTrace->getValue());
    mConfig->setOscEnable(mOscEnable->getValue());

    mUIConfig->setMessageDuration(mMessageDuration->getValue());


	StringList* dispnames = mFocusLockFunctions->getValues();
	if (dispnames != NULL) {
		StringList* functions = new StringList();
		for (int i = 0 ; i < dispnames->size() ; i++) {
			Function* f = mMobius->getFunction(dispnames->getString(i));
			if (f != NULL)
			  functions->add(f->getName());
		}
		mConfig->setFocusLockFunctions(functions);
	}

	dispnames = mMuteCancelFunctions->getValues();
	if (dispnames != NULL) {
		StringList* functions = new StringList();
		for (int i = 0 ; i < dispnames->size() ; i++) {
			Function* f = mMobius->getFunction(dispnames->getString(i));
			if (f != NULL)
			  functions->add(f->getName());
		}
		mConfig->setMuteCancelFunctions(functions);
	}

	dispnames = mConfirmationFunctions->getValues();
	if (dispnames != NULL) {
		StringList* functions = new StringList();
		for (int i = 0 ; i < dispnames->size() ; i++) {
			Function* f = mMobius->getFunction(dispnames->getString(i));
			if (f != NULL)
			  functions->add(f->getName());
		}
		mConfig->setConfirmationFunctions(functions);
	}

	dispnames = mFeedbackModes->getValues();
	if (dispnames != NULL) {
		StringList* modes = new StringList();
		for (int i = 0 ; i < dispnames->size() ; i++) {
			MobiusMode* m = mMobius->getMode(dispnames->getString(i));
			if (m != NULL)
			  modes->add(m->getName());
		}
		mConfig->setAltFeedbackDisables(modes);
	}
	return true;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
