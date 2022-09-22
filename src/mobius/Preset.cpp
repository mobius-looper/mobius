/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 *
 * Model for a collection of named parameters.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "Util.h"
#include "MidiUtil.h"
#include "XmlModel.h"
#include "XmlBuffer.h"
#include "Qwin.h"

#include "Binding.h"
#include "Function.h"
#include "Mobius.h"
#include "Parameter.h"
#include "Resampler.h"
#include "Sample.h"
#include "Script.h"

/****************************************************************************
 *                                                                          *
 *                                 CONSTANTS                                *
 *                                                                          *
 ****************************************************************************/

/**
 * XML Constants
 */
#define ATT_NAME "name"
#define ATT_NUMBER "number"
#define ATT_PROGRAM "program"
#define ATT_CHANNEL "channel"

/****************************************************************************
 *                                                                          *
 *   								PRESET                                  *
 *                                                                          *
 ****************************************************************************/

PUBLIC Preset::Preset()
{
	init();
}

PUBLIC Preset::Preset(const char* name)
{
    init();
    setName(name);
}

PUBLIC Preset::Preset(XmlElement* e)
{
	init();
	parseXml(e);
}

PUBLIC void Preset::init()
{
	mNext = NULL;
	reset();
}

PUBLIC Preset::~Preset()
{
	Preset *el, *next;

	for (el = mNext ; el != NULL ; el = next) {
		next = el->getNext();
		el->setNext(NULL);
		delete el;
	}
}

PUBLIC Target* Preset::getTarget()
{
	return TargetPreset;
}

/**
 * Initialize to default settings, but keep the name and next pointer.
 * NOTE: It is extremely important that the values here remain
 * the same, the unit tests are written depending on this initial state.
 */
PUBLIC void Preset::reset()
{
    // Limits, Misc
	mLoops   			= DEFAULT_LOOPS;
	mSubcycles   		= DEFAULT_SUBCYCLES;
	mMaxUndo			= DEFAULT_MAX_UNDO;  // 0 = infinite
	mMaxRedo			= DEFAULT_MAX_REDO;
	mNoFeedbackUndo		= false;
	mNoLayerFlattening	= false;
	mAltFeedbackEnable	= false;
    strcpy(mSustainFunctions, "");

    // Quantization
	mOverdubQuantized   = false;
	mQuantize 			= QUANTIZE_OFF;
	mBounceQuantize		= QUANTIZE_LOOP;
	mSwitchQuantize		= SWITCH_QUANT_OFF;

    // Record
   	mRecordThreshold	= 0;
    mRecordResetsFeedback = false;
	mSpeedRecord			= false;

    // Multiply, Mute
	mMultiplyMode 		= MULTIPLY_NORMAL;
	mRoundingOverdub	= true;
	mMuteMode 			= MUTE_CONTINUE;
	mMuteCancel			= MUTE_CANCEL_EDIT;

    // Slip, Shuffle, Speed, Pitch
	mSlipTime		    = 0;
	mSlipMode			= SLIP_SUBCYCLE;
	mShuffleMode		= SHUFFLE_REVERSE;
	mSpeedShiftRestart	= false;
	mPitchShiftRestart = false;
	mSpeedSequence.reset();
	mPitchSequence.reset();
    mSpeedStepRange     = DEFAULT_STEP_RANGE;
    mSpeedBendRange     = DEFAULT_BEND_RANGE;
    mPitchStepRange     = DEFAULT_STEP_RANGE;
    mPitchBendRange     = DEFAULT_BEND_RANGE;
    mTimeStretchRange   = DEFAULT_BEND_RANGE;

    // Loop Switch
	mSwitchVelocity		= false;
    mSwitchLocation     = SWITCH_RESTORE;
    mReturnLocation     = SWITCH_RESTORE;
    mSwitchDuration     = SWITCH_PERMANENT;
    mEmptyLoopAction    = EMPTY_LOOP_NONE;
	mTimeCopyMode		= COPY_PLAY;
	mSoundCopyMode		= COPY_PLAY;
    mRecordTransfer     = XFER_OFF;
	mOverdubTransfer	= XFER_FOLLOW;
	mReverseTransfer	= XFER_FOLLOW;
	mSpeedTransfer		= XFER_FOLLOW;
	mPitchTransfer		= XFER_FOLLOW;

    // AutoRecord
	mAutoRecordTempo	= DEFAULT_AUTO_RECORD_TEMPO;
	mAutoRecordBars		= DEFAULT_AUTO_RECORD_BARS;

    // Sync
    mEmptyTrackAction   = EMPTY_LOOP_NONE;
    mTrackLeaveAction   = TRACK_LEAVE_CANCEL;

    // Windowing
    mWindowSlideUnit    = WINDOW_UNIT_LOOP;
    mWindowSlideAmount  = 1;
    mWindowEdgeUnit     = WINDOW_UNIT_SUBCYCLE;
    mWindowEdgeAmount   = 1;
}

/**
 * Copy one preset to another.
 * Do not copy name here, only the parameters.
 * This is used to make a snapshot of operating parameters, if you
 * need a full clone use the clone() method.
 *
 * Tried to use memcpy here, but that didn't seem to work, not
 * sure what else is in "this".
 */
PUBLIC void Preset::copy(Preset* src)
{
    // do not copy mNext or mName since those are object references
    // do copy mNumber so we can correlate this back to the master preset
    // to get the name if we need it
    mNumber = src->mNumber;

    // Limits
	mLoops = src->mLoops;
	mSubcycles = src->mSubcycles;
	mMaxUndo = src->mMaxUndo;
	mMaxRedo = src->mMaxRedo;
	mNoFeedbackUndo = src->mNoFeedbackUndo;
	mNoLayerFlattening = src->mNoLayerFlattening;
	mAltFeedbackEnable = src->mAltFeedbackEnable;
    strcpy(mSustainFunctions, src->mSustainFunctions);

    // Quantization
    mOverdubQuantized = src->mOverdubQuantized;
	mQuantize = src->mQuantize;
	mBounceQuantize = src->mBounceQuantize;
	mSwitchQuantize = src->mSwitchQuantize;
    // Record
	mRecordThreshold = src->mRecordThreshold;
    mRecordResetsFeedback = src->mRecordResetsFeedback;
	mSpeedRecord = src->mSpeedRecord;
    // Multiply
	mMultiplyMode = src->mMultiplyMode;
	mRoundingOverdub = src->mRoundingOverdub;
    // Mute
	mMuteMode = src->mMuteMode;
	mMuteCancel = src->mMuteCancel;
    // Slip, Shuffle, Speed, Pitch
	mSlipTime = src->mSlipTime;
	mSlipMode = src->mSlipMode;
	mShuffleMode = src->mShuffleMode;
	mSpeedShiftRestart = src->mSpeedShiftRestart;
	mPitchShiftRestart = src->mPitchShiftRestart;
	mSpeedSequence.copy(&(src->mSpeedSequence));
	mPitchSequence.copy(&(src->mPitchSequence));
    mSpeedStepRange     = src->mSpeedStepRange;
    mSpeedBendRange     = src->mSpeedBendRange;
    mPitchStepRange     = src->mPitchStepRange;
    mPitchBendRange     = src->mPitchBendRange;
    mTimeStretchRange   = src->mTimeStretchRange;

    // Loop Switch
    mEmptyLoopAction = src->mEmptyLoopAction;
	mSwitchVelocity = src->mSwitchVelocity;
    mSwitchLocation = src->mSwitchLocation;
    mReturnLocation = src->mReturnLocation;
    mSwitchDuration = src->mSwitchDuration;
	mTimeCopyMode = src->mTimeCopyMode;
	mSoundCopyMode = src->mSoundCopyMode;
    mRecordTransfer = src->mRecordTransfer;
	mOverdubTransfer = src->mOverdubTransfer;
	mReverseTransfer = src->mReverseTransfer;
	mSpeedTransfer = src->mSpeedTransfer;
	mPitchTransfer = src->mPitchTransfer;
    // AutoRecord
	mAutoRecordTempo = src->mAutoRecordTempo;
	mAutoRecordBars = src->mAutoRecordBars;
    // Sync
	mEmptyTrackAction = src->mEmptyTrackAction;
    mTrackLeaveAction = src->mTrackLeaveAction;
    // windowing
    mWindowSlideUnit = src->mWindowSlideUnit;
    mWindowSlideAmount = src->mWindowSlideAmount;
    mWindowEdgeUnit = src->mWindowEdgeUnit;
    mWindowEdgeAmount = src->mWindowEdgeAmount;
}

Bindable* Preset::getNextBindable()
{
	return mNext;
}

Preset* Preset::getNext()
{
	return mNext;
}

void Preset::setNext(Preset* p)
{
	mNext = p;
}

void Preset::setSubcycles(int i) 
{
    // WTF was this for?  let them have whatever they want
	// if ((i >= 1 && i <= 96) || i == 128 || i == 256)
    if (i >= 1)
	  mSubcycles = i;
}

int Preset::getSubcycles() {
	return mSubcycles;
}

void Preset::setSustainFunctions(const char* s) 
{
    CopyString(s, mSustainFunctions, sizeof(mSustainFunctions));
}

const char* Preset::getSustainFunctions()
{
    return mSustainFunctions;
}

/**
 * Temporary upgrade helper for InsertMode and others.
 */
void Preset::addSustainFunction(const char* name)
{
    if (name != NULL && LastIndexOf(mSustainFunctions, name) < 0) {
        int len = strlen(mSustainFunctions);
        // one for the , and one for the terminator
        if ((len + strlen(name) + 2) < sizeof(mSustainFunctions)) {
            if (len > 0)
              strcat(mSustainFunctions, ",");
            strcat(mSustainFunctions, name);
        }
    }
}

/**
 * Return true if the given function is on the sustained function list.
 * Obviously not very efficient if the list can be long, but it 
 * shouldn't be.
 */
bool Preset::isSustainFunction(Function* f)
{
    return (IndexOf(mSustainFunctions, f->getName()) >= 0);
}

void Preset::setMultiplyMode(MultiplyMode i) {
	mMultiplyMode = i;
}

void Preset::setMultiplyMode(int i) {
	setMultiplyMode((MultiplyMode)i);
}

Preset::MultiplyMode Preset::getMultiplyMode() {
	return mMultiplyMode;
}

void Preset::setAltFeedbackEnable(bool b) {
    mAltFeedbackEnable = b;
}

bool Preset::isAltFeedbackEnable() {
	return mAltFeedbackEnable;
}

// 

void Preset::setEmptyLoopAction(EmptyLoopAction i) {
	mEmptyLoopAction = i;
}

void Preset::setEmptyLoopAction(int i) {
	setEmptyLoopAction((EmptyLoopAction)i);
}

Preset::EmptyLoopAction Preset::getEmptyLoopAction() {
	return mEmptyLoopAction;
}

//

void Preset::setEmptyTrackAction(EmptyLoopAction i) {
	mEmptyTrackAction = i;
}

void Preset::setEmptyTrackAction(int i) {
	setEmptyTrackAction((EmptyLoopAction)i);
}

Preset::EmptyLoopAction Preset::getEmptyTrackAction() {
	return mEmptyTrackAction;
}

// 

void Preset::setTrackLeaveAction(TrackLeaveAction i) {
	mTrackLeaveAction = i;
}

void Preset::setTrackLeaveAction(int i) {
	setTrackLeaveAction((TrackLeaveAction)i);
}

Preset::TrackLeaveAction Preset::getTrackLeaveAction() {
	return mTrackLeaveAction;
}

// 

void Preset::setLoops(int i) {
	if (i >= 1 && i <= 16)
	  mLoops = i;
}

int Preset::getLoops() {
	return mLoops;
}

void Preset::setMuteMode(MuteMode i) {
	mMuteMode = i;
}

void Preset::setMuteMode(int i) {
	setMuteMode((MuteMode)i);
}

Preset::MuteMode Preset::getMuteMode() {
	return mMuteMode;
}

void Preset::setMuteCancel(MuteCancel i) {
	mMuteCancel = i;
}

void Preset::setMuteCancel(int i) {
	setMuteCancel((MuteCancel)i);
}

Preset::MuteCancel Preset::getMuteCancel() {
	return mMuteCancel;
}

void Preset::setOverdubQuantized(bool b) {
	mOverdubQuantized = b;
}

bool Preset::isOverdubQuantized() {
	return mOverdubQuantized;
}

void Preset::setRecordTransfer(TransferMode i) {
	mRecordTransfer = i;
}

void Preset::setRecordTransfer(int i) {
	setRecordTransfer((TransferMode)i);
}

Preset::TransferMode Preset::getRecordTransfer() {
	return mRecordTransfer;
}

void Preset::setOverdubTransfer(TransferMode i) {
	mOverdubTransfer = i;
}

void Preset::setOverdubTransfer(int i) {
	setOverdubTransfer((TransferMode)i);
}

Preset::TransferMode Preset::getOverdubTransfer() {
	return mOverdubTransfer;
}

void Preset::setReverseTransfer(TransferMode i) {
	mReverseTransfer = i;
}

void Preset::setReverseTransfer(int i) {
	setReverseTransfer((TransferMode)i);
}

Preset::TransferMode Preset::getReverseTransfer() {
	return mReverseTransfer;
}

void Preset::setSpeedTransfer(TransferMode i) {
	mSpeedTransfer = i;
}

void Preset::setSpeedTransfer(int i) {
	setSpeedTransfer((TransferMode)i);
}

Preset::TransferMode Preset::getSpeedTransfer() {
	return mSpeedTransfer;
}

void Preset::setPitchTransfer(TransferMode i) {
	mPitchTransfer = i;
}

void Preset::setPitchTransfer(int i) {
	setPitchTransfer((TransferMode)i);
}

Preset::TransferMode Preset::getPitchTransfer() {
	return mPitchTransfer;
}

void Preset::setQuantize(QuantizeMode i) {
	mQuantize = i;
}

void Preset::setQuantize(int i) {
	setQuantize((QuantizeMode)i);
}

Preset::QuantizeMode Preset::getQuantize() {
	return mQuantize;
}

void Preset::setBounceQuantize(QuantizeMode i) {
	mBounceQuantize = i;
}

void Preset::setBounceQuantize(int i) {
	setBounceQuantize((QuantizeMode)i);
}

Preset::QuantizeMode Preset::getBounceQuantize() {
	return mBounceQuantize;
}

void Preset::setSpeedRecord(bool b) {
	mSpeedRecord = b;
}

bool Preset::isSpeedRecord() {
	return mSpeedRecord;
}

void Preset::setRecordResetsFeedback(bool b) {
	mRecordResetsFeedback = b;
}

bool Preset::isRecordResetsFeedback() {
	return mRecordResetsFeedback;
}

void Preset::setRoundingOverdub(bool b) {
	mRoundingOverdub = b;
}

bool Preset::isRoundingOverdub() {
	return mRoundingOverdub;
}

void Preset::setSwitchLocation(SwitchLocation i) {
	mSwitchLocation = i;
}

void Preset::setSwitchLocation(int i) {
	setSwitchLocation((SwitchLocation)i);
}

Preset::SwitchLocation Preset::getSwitchLocation() {
	return mSwitchLocation;
}

void Preset::setReturnLocation(SwitchLocation i) {
	mReturnLocation = i;
}

void Preset::setReturnLocation(int i) {
	setReturnLocation((SwitchLocation)i);
}

Preset::SwitchLocation Preset::getReturnLocation() {
	return mReturnLocation;
}

void Preset::setSwitchDuration(SwitchDuration i) {
	mSwitchDuration = i;
}

void Preset::setSwitchDuration(int i) {
	setSwitchDuration((SwitchDuration)i);
}

Preset::SwitchDuration Preset::getSwitchDuration() {
	return mSwitchDuration;
}

void Preset::setSwitchQuantize(SwitchQuantize i) {
	mSwitchQuantize = i;
}

void Preset::setSwitchQuantize(int i) {
	setSwitchQuantize((SwitchQuantize)i);
}

Preset::SwitchQuantize Preset::getSwitchQuantize() {
	return mSwitchQuantize;
}

void Preset::setTimeCopyMode(CopyMode i) {
	mTimeCopyMode = i;
}

void Preset::setTimeCopyMode(int i) {
	setTimeCopyMode((CopyMode)i);
}

Preset::CopyMode Preset::getTimeCopyMode() {
	return mTimeCopyMode;
}

void Preset::setSoundCopyMode(CopyMode i) {
	mSoundCopyMode = i;
}

void Preset::setSoundCopyMode(int i) {
	setSoundCopyMode((CopyMode)i);
}

Preset::CopyMode Preset::getSoundCopyMode() {
	return mSoundCopyMode;
}

void Preset::setRecordThreshold(int i) {
	if (i >= 0 && i <= 8)
	  mRecordThreshold = i;
}

int Preset::getRecordThreshold() {
	return mRecordThreshold;
}

void Preset::setSwitchVelocity(bool b) {
	mSwitchVelocity = b;
}

bool Preset::isSwitchVelocity() {
	return mSwitchVelocity;
}

bool Preset::isNoFeedbackUndo()
{
	return mNoFeedbackUndo;
}

void Preset::setNoFeedbackUndo(bool b)
{
	mNoFeedbackUndo = b;
}

int Preset::getMaxUndo()
{
	return mMaxUndo;
}

void Preset::setMaxUndo(int i)
{
	mMaxUndo = i;
}

int Preset::getMaxRedo()
{
	return mMaxRedo;
}

void Preset::setMaxRedo(int i)
{
	mMaxRedo = i;
}

int Preset::getAutoRecordTempo()
{
	return mAutoRecordTempo;
}

void Preset::setAutoRecordTempo(int i)
{
	mAutoRecordTempo = i;
}

int Preset::getAutoRecordBars()
{
	return mAutoRecordBars;
}

void Preset::setAutoRecordBars(int i)
{
	// this can't go below 1
	if (i > 0)
	  mAutoRecordBars = i;
	else
	  mAutoRecordBars = 1;
}

void Preset::setNoLayerFlattening(bool b)
{
	mNoLayerFlattening = b;
}

bool Preset::isNoLayerFlattening()
{
	return mNoLayerFlattening;
}

void Preset::setSpeedSequence(const char* seq)
{
	mSpeedSequence.setSource(seq);
}

StepSequence* Preset::getSpeedSequence()
{
	return &mSpeedSequence;
}

void Preset::setSpeedShiftRestart(bool b)
{
	mSpeedShiftRestart = b;
}

bool Preset::isSpeedShiftRestart()
{
	return mSpeedShiftRestart;
}

void Preset::setPitchSequence(const char* seq)
{
	mPitchSequence.setSource(seq);
}

StepSequence* Preset::getPitchSequence()
{
	return &mPitchSequence;
}

void Preset::setPitchShiftRestart(bool b)
{
	mPitchShiftRestart = b;
}

bool Preset::isPitchShiftRestart()
{
	return mPitchShiftRestart;
}

void Preset::setSpeedStepRange(int range)
{
    if (range <= 0) 
      range = DEFAULT_STEP_RANGE;
    else if (range > MAX_RATE_STEP)
      range = MAX_RATE_STEP;

    mSpeedStepRange = range;
}

int Preset::getSpeedStepRange()
{
    return mSpeedStepRange;
}

void Preset::setSpeedBendRange(int range)
{
    if (range <= 0) 
      range = DEFAULT_BEND_RANGE;
    else if (range > MAX_BEND_STEP)
      range = MAX_BEND_STEP;

    mSpeedBendRange = range;
}

int Preset::getSpeedBendRange()
{
    return mSpeedBendRange;
}

void Preset::setPitchStepRange(int range)
{
    if (range <= 0) 
      range = DEFAULT_STEP_RANGE;
    else if (range > MAX_RATE_STEP)
      range = MAX_RATE_STEP;

    mPitchStepRange = range;
}

int Preset::getPitchStepRange()
{
    return mPitchStepRange;
}

void Preset::setPitchBendRange(int range)
{
    if (range <= 0) 
      range = DEFAULT_BEND_RANGE;
    else if (range > MAX_BEND_STEP)
      range = MAX_BEND_STEP;

    mPitchBendRange = range;
}

int Preset::getPitchBendRange()
{
    return mPitchBendRange;
}

void Preset::setTimeStretchRange(int range)
{
    if (range <= 0) 
      range = DEFAULT_BEND_RANGE;
    else if (range > MAX_BEND_STEP)
      range = MAX_BEND_STEP;

    mTimeStretchRange = range;
}

int Preset::getTimeStretchRange()
{
    return mTimeStretchRange;
}

void Preset::setSlipMode(SlipMode sm) {
	mSlipMode = sm;
}

void Preset::setSlipMode(int i) {
	setSlipMode((SlipMode)i);
}

Preset::SlipMode Preset::getSlipMode() {
	return mSlipMode;
}

void Preset::setSlipTime(int msec)
{
	mSlipTime = msec;
}

int Preset::getSlipTime()
{
	return mSlipTime;
}

void Preset::setShuffleMode(ShuffleMode sm) {
	mShuffleMode = sm;
}

void Preset::setShuffleMode(int i) {
	setShuffleMode((ShuffleMode)i);
}

Preset::ShuffleMode Preset::getShuffleMode() {
	return mShuffleMode;
}

//

void Preset::setWindowSlideUnit(WindowUnit u) {
	mWindowSlideUnit = u;
}

Preset::WindowUnit Preset::getWindowSlideUnit() {
	return mWindowSlideUnit;
}

//

void Preset::setWindowSlideAmount(int amount) {
	mWindowSlideAmount = amount;
}

int Preset::getWindowSlideAmount() {
	return mWindowSlideAmount;
}

//

void Preset::setWindowEdgeUnit(WindowUnit u) {
	mWindowEdgeUnit = u;
}

Preset::WindowUnit Preset::getWindowEdgeUnit() {
	return mWindowEdgeUnit;
}

//

void Preset::setWindowEdgeAmount(int amount) {
	mWindowEdgeAmount = amount;
}

int Preset::getWindowEdgeAmount() {
	return mWindowEdgeAmount;
}

/****************************************************************************
 *                                                                          *
 *   								 XML                                    *
 *                                                                          *
 ****************************************************************************/

PUBLIC char* Preset::toXml()
{
	char* xml = NULL;
	XmlBuffer* b = new XmlBuffer();
	toXml(b);

	xml = b->stealString();
	delete b;
	return xml;
}

PUBLIC void Preset::toXml(XmlBuffer* b)
{
	b->addOpenStartTag(EL_PRESET);
	// name, number
	toXmlCommon(b);
	b->setAttributeNewline(true);

	for (int i = 0 ; Parameters[i] != NULL ; i++)  {
        Parameter* p = Parameters[i];
        // don't write the ones marked deprecated, only read and convert
        if (p->scope == PARAM_SCOPE_PRESET && !p->deprecated)
          p->toXml(b, this);
    }

	b->add("/>\n");
	b->setAttributeNewline(false);
}

PUBLIC void Preset::parseXml(XmlElement* e)
{
	parseXmlCommon(e);

	for (int i = 0 ; Parameters[i] != NULL ; i++) {
		Parameter* p = Parameters[i];
        if (p->scope == PARAM_SCOPE_PRESET) {
            //Trace(2, "Parameter %s\n", p->name);
            p->parseXml(e, this);
        }
    }

    // auto upgrades

    // InterfaceMode=Expert was the original way to enable
    // secondary feedback, now we have a boolean
    const char* str = e->getAttribute("interfaceMode");
    if (StringEqualNoCase(str, "expert"))
      mAltFeedbackEnable = true;

    // RecordMode=Safe was the original way to set
    // RecordResetsFeedback
    str = e->getAttribute("recordMode");
    if (StringEqualNoCase(str, "safe"))
      mRecordResetsFeedback = true;
    else if (StringEqualNoCase(str, "sustain"))
      addSustainFunction("Record");

    // OverdubMode=Quantized was the original way to set
    // OverdubQuantized
    str = e->getAttribute("overdubMode");
    if (StringEqualNoCase(str, "quantized"))
      mOverdubQuantized = true;
    else if (StringEqualNoCase(str, "sustain"))
      addSustainFunction("Overdub");

    str = e->getAttribute("insertMode");
    if (StringEqualNoCase(str, "sustain"))
      addSustainFunction("Insert");
}

PUBLIC Preset* Preset::clone()
{
	Preset* clone = new Preset();
	clone->copy(this);
	
	// these aren't cloned
	clone->setName(getName());
	clone->setNumber(getNumber());

	return clone;
}

/****************************************************************************
 *                                                                          *
 *   							STEP SEQUENCE                               *
 *                                                                          *
 ****************************************************************************/

PUBLIC StepSequence::StepSequence()
{
	memset(mSource, 0, sizeof(mSource));
	mStepCount = 0;
}

PUBLIC StepSequence::StepSequence(const char* src)
{
	mStepCount = 0;
	setSource(src);
}

PUBLIC StepSequence::~StepSequence()
{
}

PUBLIC void StepSequence::reset()
{
	setSource(NULL);
}

PUBLIC void StepSequence::setSource(const char* src) 
{
	if (src == NULL)
	  memset(mSource, 0, sizeof(mSource));
	else {
		// bounds check!
		strcpy(mSource, src);
	}

	for (int i = 0 ; i < MAX_SEQUENCE_STEPS ; i++) 
	  mSteps[i] = 0;

	mStepCount = ParseNumberString(src, mSteps, MAX_SEQUENCE_STEPS);

	// TODO: could normalize this way?
#if 0
	char normalized[1024];
	strcpy(normalized, "");
	for (int i = 0 ; i < mStepCount ; i++) {
		char num[32];
		sprintf(num, "%d", mSteps[i]);
		if (i > 0)
		  strcat(normalized, " ");
		strcat(normalized, num);
	}
	strcpy(mSource, normalized);
#endif
}

const char* StepSequence::getSource()
{
	return mSource;
}

int* StepSequence::getSteps()
{
	return mSteps;
}

int StepSequence::getStepCount()
{
	return mStepCount;
}

int StepSequence::advance(int current, bool next, int dflt,
						  int* retval)
{
	int value = dflt;
	int index = current;

	if (mStepCount > 0) {
		if (next) {
			index++;
			if (index >= mStepCount)
			  index = 0;
		}
		else {
			index--;
			if (index < 0)
			  index = mStepCount  - 1;
		}
		value = mSteps[index];
	}

	if (retval)
	  *retval = value;

	return index;
}

/**
 * Necessary to copy the "real" Preset sequence into the one that
 * Track and Loop own.
 */
void StepSequence::copy(StepSequence* src) 
{
	strcpy(mSource, src->mSource);
	mStepCount = src->mStepCount;
	for (int i = 0 ; i < mStepCount ; i++) 
	  mSteps[i] = src->mSteps[i];
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

