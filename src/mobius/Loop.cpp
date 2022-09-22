/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Maintains the state of one loop.
 *
 * Internally each loop is made up of a list of "layers" represented
 * with Layer objects.  A layer is essentially one unique iteration
 * through the loop.  Layers are added as the loop changes, layers
 * are removed with the Undo function.
 *
 * AUTO FEEDBACK REDUCTION
 *
 * The EDP applies an automatic 5% reduction in feedback level when
 * recording new material over the previous layer (overdub, multiply, stutter).
 * When flattening is disabled this presents a problem because we cannot
 * selectively apply feedback to only the section with the overdub, it
 * must be applied to the entire layer.  If we're just playing back
 * the loop without further recording this isn't too bad, as the level
 * drop is unnoticeable and it will stay the same as we shift and reuse
 * the previous layer.  If however you are doing repeated overdubs on an
 * isolated area of the loop, you may notice the level of the entire background
 * reduce on each overdub.  
 *
 * The code is conditionalized for feedback redunction when not flattening,
 * and by default it is off.  This seems reasonable since the potential
 * overload you might have if you didn't reduce feedback normally only
 * happens after you've done a lot of overdubs, and if you're doing lots
 * of overdubs you normally want to be flattening.
 *
 * When we flatten, we can support this feature, however it presents
 * a problem for the unit tests since overdubs will record differently
 * depending on whether flattening is enabled or not.  For this we
 * provide a global paramter to enable auto feedback reduction that can be
 * turned off for testing.
 */

#include <stdio.h>
#include <memory.h>
#include <math.h>

#include "Audio.h"
#include "List.h"
#include "Thread.h"
#include "Util.h"

#include "Action.h"
#include "Event.h"
#include "EventManager.h"
#include "Function.h"
#include "Layer.h"
#include "Mobius.h"
#include "Mode.h"
#include "Project.h"
#include "Resampler.h"
#include "Script.h"
#include "Stream.h"
#include "Synchronizer.h"
#include "SyncState.h"
#include "Track.h"
#include "WatchPoint.h"

#include "Loop.h"

// in Track.cpp
extern bool TraceFrameAdvance;

/****************************************************************************
 *                                                                          *
 *                                   DEBUG                                  *
 *                                                                          *
 ****************************************************************************/

/**
 * Defined in Script.cpp and set by the Break statement.
 */
extern bool ScriptBreak;

/**
 * Experiment, selects new insert behavior.
 * This is defined in Insert.cpp and has been off for awhile.
 * Not sure what the thinking was.
 */
extern bool DeferInsertShift;

/**
 * The maximum frames of roundoff drift to allow during play
 * transitions without adding a play fade. 
 */
#define MAX_ROUNDOFF_DRIFT 3

/****************************************************************************
 *                                                                          *
 *                                STREAM STATE                              *
 *                                                                          *
 ****************************************************************************/

PUBLIC StreamState::StreamState()
{
    init();
}

PUBLIC void StreamState::init()
{
    frame = 0;
    reverse = false;
	speedToggle = 0;
    speedOctave = 0;
	speedStep = 0;
	speedBend = 0;
    pitchOctave = 0;
	pitchStep = 0;
	pitchBend = 0;
    timeStretch = 0;
}

PUBLIC StreamState::~StreamState()
{
}

PUBLIC void StreamState::capture(Track* t)
{
    InputStream* stream = t->getInputStream();

	reverse = stream->isReverse();
    speedToggle = t->getSpeedToggle();
    speedOctave = stream->getSpeedOctave();
	speedStep = stream->getSpeedStep();
    speedBend = stream->getSpeedBend();
    timeStretch = stream->getTimeStretch();
    pitchOctave = stream->getPitchOctave();
	pitchStep = stream->getPitchStep();
    pitchBend = stream->getPitchBend();
}

/****************************************************************************
 *                                                                          *
 *                                    LOOP                                  *
 *                                                                          *
 ****************************************************************************/

PUBLIC Loop::Loop(int number, Mobius* m, Track* track, 
				  InputStream* input, OutputStream* output)
{
	init(m, track, input, output);
	mNumber = number;
}

PRIVATE void Loop::init(Mobius* m, Track* track, 
						InputStream* input, OutputStream* output) 
{
    mMobius = m;
	mTrack = track;
	mPreset = track->getPreset();
	mInput = input;
	mOutput = output;
	mSynchronizer = mMobius->getSynchronizer();
    mRecord = NULL;
    mPlay = NULL;
    mPrePlay = NULL;
	mRedo = NULL;

	mNumber = 0;
    mFrame = 0;
	mPlayFrame = 0;
	mModeStartFrame = 0;
	mMode = ResetMode;

	mMute = false;
	mPause = false;
	mMuteMode = false;
	mOverdub = false;
	mRecording = false;
	mAutoFeedbackReduction = false;

    mRestoreState.init();

    mBeatLoop = false;
    mBeatCycle = false;
    mBeatSubCycle = false;
	mBreak = false;

	mState.init();

	// since we're in Reset, this has to start here
	setFrame(-(mInput->latency));
}

PUBLIC Loop::~Loop()
{
    // everything chained from here
    if (mRecord != NULL)
      mRecord->freeAll();

    // TODO: delete event and transition pools
}

/**
 * Special layer we use in some special cases to "play" without
 * actually doing anything.
 * This is a shared Layer we boostrap once.
 */
PUBLIC Layer* Loop::getMuteLayer()
{
    LayerPool* lp = mMobius->getLayerPool();
    return lp->getMuteLayer();
}

/**
 * Called by Track whenever something changes in the MobiusConfig.
 * Loop always has a reference to the Preset managed by the Track so we
 * don't have to cache any preset parameters.  Just pick up a few
 * global parameters.
 */
PUBLIC void Loop::updateConfiguration(MobiusConfig* config)
{
	mAutoFeedbackReduction = config->isAutoFeedbackReduction();

    if (mMode == ResetMode) {
        // formerly did this based on InterfaceMode=Delay which
        // we no longer have, now this should already be off
        mOverdub = false;

		// InputLatency may have changed
		setFrame(-(mInput->latency));
    }
}

/**
 * For newer functions that do their own layer processing.
 */
PUBLIC InputStream* Loop::getInputStream()
{
	return mInput;
}

PUBLIC OutputStream* Loop::getOutputStream()
{
	return mOutput;
}

PUBLIC long Loop::getInputLatency()
{
	return mInput->latency;
}

PUBLIC long Loop::getOutputLatency()
{
	return mOutput->latency;
}

/**
 * Hack for debugging.  This will be set from the
 * Break script function.
 */
PUBLIC void Loop::setBreak(bool b)
{
	mBreak = b;
}

/**
 * This is where you hang the debugger breakpoint.
 */
PUBLIC void Loop::checkBreak()
{
	if (mBreak)
	  printf("Loop breakpoint\n");
}

/**
 * We're a trace context, supply track/loop/time.
 */
PUBLIC void Loop::getTraceContext(int* context, long* time)
{
	*context = (mTrack->getDisplayNumber() * 100) + mNumber;
	*time = mFrame;
}

PUBLIC bool Loop::isInteresting()
{
    return (mPlay != NULL || mRedo != NULL);
}
      
PUBLIC void Loop::dump(TraceBuffer* b)
{
    b->add("Loop %d\n", mNumber);
    b->incIndent();

    if (mPlay != NULL) {
      for (Layer* l = mPlay ; l != NULL ; l = l->getPrev())
        l->dump(b);
    }

    if (mRedo != NULL) {
        b->add("Redo layers:\n");
        b->incIndent();
        for (Layer* r = mRedo ; r != NULL ; r = r->getRedo()) {
            // redo layer can be the head of a chain if we're using
            // checkpoints
            int count = 0;
            for (Layer* l = r ; l != NULL ; l = l->getPrev()) {
                count++;
                if (count == 2)
                  b->incIndent();
                l->dump(b);
            }
            if (count > 1)
              b->decIndent();
        }
    }

    b->decIndent();
}

/****************************************************************************
 *                                                                          *
 *   						  PROJECT SAVE/LOAD                             *
 *                                                                          *
 ****************************************************************************/

/**
 * Process a ProjectLoop object during a project load.
 *
 * Fleshing out the segment lists is difficult because they
 * reference other layers by id the layer is not necessarily
 * in this loop, or even in this track.  and there is no ensured 
 */
void Loop::loadProject(ProjectLoop* pl)
{
    // try to retain the same posiiton?
    clear();

	// layers are stored in reverse order (most recent first)
	// but they have to be prepared from oldest first
	Layer* last = NULL;
	List* layers = pl->getLayers();
	if (layers != NULL) {
        int max = layers->size();
		for (int i = max - 1 ; i >= 0 ; i--) {
            ProjectLayer* pl = (ProjectLayer*)layers->get(i);
			// layers will already have been allocated
			// reference count is already assuming that a loop owns it
            Layer* l = pl->getLayer();
			if (l != NULL) {
				l->setLoop(this);
				l->setPrev(mPlay);
				mPlay = l;
			}
		}

		if (mPlay != NULL) {
			mRecord = mPlay->copy();
			mRecord->setPrev(mPlay);
		}
	}

	// Can't be in Reset any more
	// switch processing will change this, but let this be
	// our "resume" point, could save this in the project if we
	// want to be REALLY anal
	// !! need to be able to restore the frame from the project
	setFrame(-(mInput->latency));
	mPlayFrame = mOutput->latency;

	if (!pl->isActive()) {
		setMode(PlayMode);
		mMuteMode = false;
		mMute = false;
		mPause = false;
	}
	else {
		// put the active loop in a pause mute since it is hard to predict
		// when the load will finish
		setMode(MuteMode);
		mMuteMode = true;
		mMute = true;
		mPause = true;
	}
}

/****************************************************************************
 *                                                                          *
 *   						  FRAMES AND STATUS                             *
 *                                                                          *
 ****************************************************************************/

PUBLIC int Loop::getNumber()
{
	return mNumber;
}

PUBLIC void Loop::setNumber(int i)
{
	mNumber = i;
}

PUBLIC MobiusMode* Loop::getMode()
{
	return mMode;
}

/**
 * Return the next loop number.
 * ?? If we've got a ReturnEvent, should we return that number?
 */
PUBLIC int Loop::getNextLoop()
{
	long next = 0;

    EventManager* em = mTrack->getEventManager();
    Event* switche = em->getSwitchEvent();
	if (switche != NULL) {
		Loop* l = switche->fields.loopSwitch.nextLoop;
		if (l != NULL)
		  next = l->getNumber();
	}
	return next;
}

PRIVATE void Loop::setMode(MobiusMode* m)
{
	if (mMode != m) {
		Trace(this, 2, "Loop: Set mode %s\n", m->getName());
		
		if (m == PlayMode && mPlay == NULL && 
			mMode != ResetMode && mMode != SynchronizeMode)
		  Trace(this, 1, "Loop: Entering Play mode without a layer!\n");

		mMode = m;	
	}
}

PUBLIC long Loop::getModeStartFrame()
{
    return mModeStartFrame;
}

PUBLIC void Loop::setModeStartFrame(long frame)
{
    mModeStartFrame = frame;
}

PUBLIC Track* Loop::getTrack()
{
    return mTrack;
}

PUBLIC Preset* Loop::getPreset()
{
	return mPreset;
}

PUBLIC Mobius* Loop::getMobius()
{
    return mMobius;
}

PUBLIC Synchronizer* Loop::getSynchronizer()
{
	return mSynchronizer;
}

/**
 * True if we're empty.
 */
PUBLIC bool Loop::isEmpty()
{
	// ?? need to distingiush between reset and empty
	return (getFrames() == 0);
}

/**
 * Return non-null if we're waiting on a synchronization event.
 * This is used by Track & Recorder to determine if the track
 * should be given priority.  Also used by Synchronizer to determine
 * when to generate a brother sync event.
 *
 * A Function representing the operation to be performed is returned.
 * This is used by Synchronizer to know whether to use the TrackSyncMode
 * parameter to quantize the event.  Record start/end may be quantized
 * to a subcycle or cycle boundary, realign is always to a loop boundary.
 * 
 * Returning a function is odd, but we've got two cases where sync wait
 * is determined by scheduled events, and another by a mode.  A function
 * provides a common way to return both.
 */
PUBLIC Function* Loop::isSyncWaiting()
{
	Function* waitFunction = NULL;

    if (mMode == SynchronizeMode) {
        // waiting for the start
		waitFunction = Record;
    }
    else {
        // or waiting for the end
        EventManager* em = mTrack->getEventManager();
        Event* end = em->findEvent(RecordStopEvent);
		if (end != NULL && end->pending) {
			// waiting for the end
			waitFunction = Record;
		}
		else {
			Event* realign = em->findEvent(RealignEvent);
			if (realign != NULL && realign->pending)
			  waitFunction = Realign;
		}
	}

	return waitFunction;
}

/**
 * True if we're in Reset.
 */
PUBLIC bool Loop::isReset()
{
	return mMode == ResetMode;
}

/**
 * Return true if we're in reverse mode.
 * This has to test the record context flag since the play
 * context may change before we're "fully" in reverse mode.
 */
PUBLIC bool Loop::isReverse()
{
	return mInput->isReverse();
}

PUBLIC bool Loop::isOverdub()
{
	return mOverdub;
}

PUBLIC bool Loop::isRecording()
{
	return mRecording;
}

PUBLIC bool Loop::isPlaying()
{
    return (mPlay != NULL || mPrePlay != NULL);
}

/**
 * This is the flag that says if we're actively being muted.
 * mMode is not necessarily MuteMode since other modes like ReplaceMode
 * can also cause a mute.  This is also not necessarily the same as
 * mMuteMode which tracks the "minor mode" state.
 */
PUBLIC bool Loop::isMute() 
{
	return mMute;
}

/**
 * This is true if the mute "minor mode" is enabled.  Mute minor mode is
 * similar to Overdub in that it can be toggled on and off without 
 * necessarily being in MuteMode.  The active mute state held in 
 * mMute will usually but not necessarily have the same value.
 *
 * TODO: Enumerate the cases where mMute==false && mMuteMode==true
 */
PUBLIC bool Loop::isMuteMode() 
{
	return mMuteMode;
}

PUBLIC bool Loop::isPaused()
{
	return mPause;
}

/**
 * True if the loop has content and is advancing.
 * Used by some function handlers to see if it is meaningful to schedule
 * an event.
 *
 * !! Revisit what this means in Threshold and Synchronize mode.
 * If we're rerecording over an existing loop we could let the old
 * one continue to play until the next sync point or record level.
 */
PUBLIC bool Loop::isAdvancing()
{
	return (mMode != ResetMode && 
			mMode != ThresholdMode &&
			mMode != SynchronizeMode &&
			mMode != RunMode &&
			!mPause);
}

/**
 * True if we're advancing, and not in an "extending" mode like Insert,
 * Multiply, and Stutter.  Used by Stream to see if we can make
 * consistency checks on the record and play frames locations.
 *
 * We are also not advancing normally if we're in the play jump 
 * latency period before a speed change.  Here, we have to change the playback
 * speed early, but the record speed will be changed later.  Usual rules
 * of frame distance don't apply.    We could detect this here, but
 * it seems easier to do this in stream.  If the input and output stream
 * speed are not the same, defer the check.
 *
 * Also return false if frame validation has been disabled, which
 * is sometimes an indicator that we haven't finished chancing speeds.
 */
PUBLIC bool Loop::isAdvancingNormally() 
{
    EventManager* em = mTrack->getEventManager();

	return (isAdvancing() && mMode != RunMode && !mMode->extends &&
			!em->isValidationSuppressed(NULL));
}

/**
 * Used by Synchronizer to determine if the loop is in the 
 * initial recording period.  Have to detect that odd latency
 * adjustment at the beginning.  Doesn't have to be used just for sync,
 * but naming it with "Sync" to reinforce how it is used.
 */
PUBLIC bool Loop::isSyncRecording()
{
	return ((mMode == RecordMode && mPrePlay == NULL) ||
			(mMode == PlayMode && mPlay == NULL));
}

/**
 * Used by Synchronizer to determine of the loop has finished the
 * initial recording period.  Have to detect that small latency
 * adjustment period at the end where we'll still be recording, but
 * preplaying the new loop.
 */
PUBLIC bool Loop::isSyncPlaying()
{
	return (mPlay != NULL || (mMode == RecordMode && mPrePlay != NULL));
}

/**
 * Return the current record frame. 
 */
PUBLIC long Loop::getFrame()
{
    return mFrame;
}

/**
 * Set the current record frame.
 * Also reset state related to the frame counter.  Necessary for the 
 * silly mLastSyncEventFrame since if a drift adjust rewinds to a point
 * before the last sync event, then we need to do that sync event again.
 * !! This is an ugly dependency.
 */
void Loop::setFrame(long l)
{
    mFrame = l;

    EventManager* em = mTrack->getEventManager();
    em->resetLastSyncEventFrame();
}

/**
 * Get the current playback frame, this will normally
 * differ from mFrame by the sum of InputLatency and OutputLatency,
 * wrapping around the loop frame.  But under some transition
 * conditions, multiply, insert they will go out of sync.
 */
long Loop::getPlayFrame()
{
    return mPlayFrame;
}

void Loop::setPlayFrame(long l)
{
    mPlayFrame = l;
}

/**
 * Get the number of frames in the loop.
 * It may be different than the play layer's frame count if we're
 * multiplying or inserting.
 */
PUBLIC long Loop::getFrames()
{
	return (mRecord != NULL) ? mRecord->getFrames() : 0;
}

/**
 * Return the total number of frames in all layers.
 */
PUBLIC long Loop::getHistoryFrames()
{
    long frames = 0;
    if (mPlay != NULL) {
        Layer* last = mPlay;
        // the window layer is not included in the history
        if (last->getWindowOffset() >= 0)
          last = last->getPrev();
        if (last != NULL)
          frames = last->getHistoryOffset() + last->getFrames();
    }
    return frames;
}

/**
 * Return the window offset if we are loop windowing.
 */
PUBLIC long Loop::getWindowOffset()
{
    long offset = -1;
    if (mPlay != NULL)
      offset = mPlay->getWindowOffset();
    return offset;
}

/**
 * Used by synchronizer to calculate how many cycles we should have
 * during some sync modes.
 */
PUBLIC long Loop::getRecordedFrames()
{
	return (mRecord != NULL) ? mRecord->getRecordedFrames() : 0;
}

/**
 * Return the number of cycles in the loop.
 * Get it from the record layer so we can track the effects of 
 * multiply and insert.
 */
PUBLIC long Loop::getCycles()
{
    return (mRecord != NULL) ? mRecord->getCycles() : 1;
}

/**
 * Return the number of frames in a cycle.
 * Get it from the record layer so we can track the effects of 
 * multiply and insert.
 */
PUBLIC long Loop::getCycleFrames()
{
	return (mRecord != NULL) ? mRecord->getCycleFrames() : 0;
}

/**
 * Cycle count setter for CycleCountVariableType.
 * We just obey here, there are all sorts of consequences to this
 * that will have to be handled at a higher level.
 */
PUBLIC void Loop::setCycles(int cycles)
{
    // what's a good upper bound?  should we even have one?
    if (cycles > 0 && cycles <= 1000) {
        if (mRecord != NULL)
          mRecord->setCycles(cycles);
    }
}

/**
 * Return the number of frames in a sub-cycle.
 */
PUBLIC long Loop::getSubCycleFrames()
{
	long cycleFrames = getCycleFrames();
	if (cycleFrames > 0) {
		int divisor = mPreset->getSubcycles();
		if (divisor > 0)
		  cycleFrames /= divisor;
	}
	return cycleFrames;
}

/**
 * Only for InputStream and some newer Functions that do all the
 * layer processing in the Function.
 */
PUBLIC Layer* Loop::getRecordLayer()
{
	return mRecord;
}

/**
 * Only for a few functions.
 */
PUBLIC void Loop::setRecordLayer(Layer* l)
{
	mRecord = l;
}

/**
 * Only for Project building.
 */
PUBLIC Layer* Loop::getPlayLayer()
{
	return mPlay;
}

/**
 * Only for a few functions.
 */
PUBLIC void Loop::setPlayLayer(Layer* l)
{
	mPlay = l;
}

/**
 * Only for RedoFunction.
 */
PUBLIC void Loop::setPrePlayLayer(Layer* l)
{
	mPrePlay = l;
}

/**
 * Only for LayerCountVariable and RedoFunction.
 */
PUBLIC Layer* Loop::getRedoLayer()
{
	return mRedo;
}

/**
 * Only for RedoFunction.
 */
PUBLIC void Loop::setRedoLayer(Layer* l)
{
    mRedo = l;
}

/**
 * Return a copy of the loop that is currently audible.
 * Used in the implementation of "quick save" and "save loop".
 *
 * Note that with the introduction of segments, the play layer's
 * Audio is usually, sparse, so you have to "flatten" it to get
 * an accurate representation of what is playing.
 *
 * The returned object is owned by the caller and must be freed.
 * 
 * This is normally called from the MobiusThread.
 * 
 * !! There can be race conditions here that cause AudioCursor
 * play frame overflow.  In Rehearse mode at the exact end of the loop,
 * mPlay is about to be reclaimed since we do not keep an undo list.
 * It's a very small window though and in practice getting it to happen
 * exactly on the loop boundary only happens in scripts.  Still there
 * are plenty of other things that could cause the play layer to modified.
 * Think about a way to lock it while it is being flattened.
 */
PUBLIC Audio* Loop::getPlaybackAudio()
{
	Audio* playing = NULL;
	if (mPlay != NULL) {
		playing = mPlay->flatten();
	}

	return playing;
}

/**
 * A little logic blob we need in many places.
 * Add a number of frames to a frame counter, looping if necessary.
 */
PUBLIC long Loop::addFrames(long frame, long frames)
{
	return wrapFrame(frame + frames);
}

PUBLIC long Loop::wrapFrame(long frame)
{
	return wrapFrame(frame, getFrames());
}

PUBLIC long Loop::wrapFrame(long frame, long loopFrames)
{
	// !! use modulo here idiot
	if (loopFrames > 0) {
		while (frame >= loopFrames)
		  frame -= loopFrames;

		while (frame < 0)
		  frame += loopFrames;
	}
	return frame;
}

/****************************************************************************
 *                                                                          *
 *   						   PUBLISHED STATE                              *
 *                                                                          *
 ****************************************************************************/

PUBLIC LoopState* Loop::getState()
{
	refreshState(&mState);
	return &mState;
}


PUBLIC StreamState* Loop::getRestoreState()
{
    return &mRestoreState;
}

/**
 * Return a batch of state.  
 * This is intended to be called by applications periodically to
 * gather all the interesting state they might want to display.
 * Note that we are in the UI thread, so becareful of race conditions.
 * !! Actually, there are race conditions all over here, mRecord, 
 * and mRedo are all assumed to be stable.
 */
PRIVATE void Loop::refreshState(LoopState* s)
{
	s->number = mNumber;
	s->recording = mRecording;
	s->paused = mPause;
	s->nextLoop = 0;
    s->returnLoop = 0;
	s->overdub = mOverdub;
	s->mute = mMuteMode;

	// these are set during buffer processing, and are cleared when
	// the application requests them, avoid having to have a callback
	// or asynchronous event
	s->beatLoop = mBeatLoop;
	mBeatLoop = false;
	s->beatCycle = mBeatCycle;
	mBeatCycle = false;
	s->beatSubCycle = mBeatSubCycle;
	mBeatSubCycle = false;

	// this will be zero while recording, may be interesting
	// to return the true value but there is too much sensitive
	// logic around this being zero to mean the initial record
	// !race condition, assumes mRecord stable
	s->frames = getFrames();

    s->windowOffset = -1;
    if (mPlay != NULL) {
        s->windowOffset = mPlay->getWindowOffset();
        if (s->windowOffset >= 0) {
            // this is a window layer, but if the window exactly covers
            // the last real layer, pretend like we're not windowing so the
            // UI won't draw it
            // actually no, I don't like this because there are still
            // implications about being in window mode
            /*
            Layer* prev = mPlay->getPrev();
            if (prev != NULL && 
                prev->getHistoryOffset() == s->windowOffset &&
                prev->getFrames() == mPlay->getFrames()) {
                s->windowOffset = -1;
            }
            */
        }
    }

    // don't bother calculating this unless there is a window
    if (s->windowOffset >= 0)
      s->historyFrames = getHistoryFrames();
    else
      s->historyFrames = 0;

	// !! race condition, assumes mRecord stable
	s->cycles = getCycles();

	// The frame number should be the "realtime" frame that matches what
    // is being played and heard, since mFrame lags, have to add latency
    // UPDATE: Hmm no, this doesn't really matter and at excessive shifts
    // latency can be high enough to push us into a beat which looks confusing

	//s->frame = mFrame + mInput->latency;
    s->frame = mFrame;

	// adding latency may have caused us to loop
    long loopFrames = getFrames();
	s->frame = wrapFrame(s->frame, loopFrames);

	// warp this so the GUI doesn't have to deal with reverse
	if (isReverse() && loopFrames > 0)
		s->frame = reflectFrame(s->frame);

	// During initial recording we're always at the end, so the current
	// cycle is always the maximum
	if (mMode == RecordMode) {
		// since we're always at the end, this always tracks
		// cycle count if we're using external sync
		s->cycle = s->cycles;
	}
	else {
		s->cycle = 1;
		if (s->frames > 0 && s->cycles > 0) {
			int cycleFrames = s->frames / s->cycles;
			if (cycleFrames > 0)
			  s->cycle = (s->frame / cycleFrames) + 1;
		}
	}

    s->mode = mMode;

	// calculate the number of layers, the record loop is invisible
	int added = 0;
	int lost = 0;
	if (mRecord != NULL)
	  getLayerState(mRecord->getPrev(), s->layers, MAX_INFO_LAYERS, 
					&added, &lost);

	s->layerCount = added;
	s->lostLayers = lost;

	// same for redo layers
	getLayerState(mRedo, s->redoLayers, MAX_INFO_REDO_LAYERS, &added, &lost);
	s->redoCount = added;
	s->lostRedo = lost;
}

/**
 * Capture layer state.
 * Used for both normal layers and redo layers.
 * Return via pointers the number of entries in the array and the number
 * of layers that wouldn't fit.
 *
 * Layers are added in reverse order, with the most recent first.
 * The lost layer count represents old layers that would not fit in the
 * summary array.  
 *
 * The redo layers are in the order in which they will be redone.
 */
PRIVATE void Loop::getLayerState(Layer* layers, LayerState* states, int max,
								 int *retAdded, int* retLost)
{
	int added = 0;
	int lost = 0;

	// if this is the redo list, we'll have a redo pointer
	for (Layer* links = layers ; links != NULL ; links = links->getRedo()) {
		bool inCheckpoint = false;

		// with the introduction of redo links, this logic is overcomplicated
		// since if there are non-null links this must by definition
		// be a checkpoint so we don't need to scan, but trying to
		// share the same scanner for both the undo and redo lists

		for (Layer* l = links ; l != NULL ; l = l->getPrev()) {
			bool check = l->isCheckpoint();
			if (!inCheckpoint || check) {
				if (added >= max)
				  lost++;
				else {
					l->getState(&(states[added]));
					added++;
				}
				// once set, this doesn't turn off in the inner loop
				inCheckpoint = check;
			}
		}
	}

	*retAdded = added;
	*retLost = lost;
}

/**
 * Perform a simple loop size reflection of a frame.  Note that
 * events scheduled beyond the loop end will have negative reflected frames.
 * This is ok for getState the UI is expected to visualize these hanging
 * in space on the left.  
 *
 * This differs from reverseFrame() which is called during reverse processing
 * and DOES care about frames going negative.
 */
PRIVATE long Loop::reflectFrame(long frame)
{
	return (getFrames() - frame - 1);
}

/**
 * Abbreviated state returned in TrackState for all loops in the track.
 * Status for rate only indiciates if some form of rate shift or semitone shift
 * is being applied so they can be colored differently in the loop list.
 * Would be better to return the values and let the UI decide what to draw.
 */
PUBLIC void Loop::getSummary(LoopSummary* s, bool active)
{
	s->frames = getFrames();
	s->cycles = getCycles();
	s->active = active;
	s->pending = false;

	if (active) {
		// note that we return the minor mode here
		s->mute = isMuteMode();
		s->reverse = isReverse();
		s->pitch = (mOutput->getPitch() != 1.0);
		s->speed = (mOutput->getSpeed() != 1.0);
	}
	else {
		// relevant only for the active track
		s->mute = false;

		if (mPreset->getReverseTransfer() == Preset::XFER_RESTORE)
		  s->reverse = mRestoreState.reverse;
		else
		  s->reverse = false;
		
		if (mPreset->getSpeedTransfer() == Preset::XFER_RESTORE) {
            // combine spedd and octave!
			s->speed = (mRestoreState.speedStep != 0 || mRestoreState.speedBend != 0);
		}
		else {
			s->speed = false;
		}

		if (mPreset->getPitchTransfer() == Preset::XFER_RESTORE) {
			s->pitch = (mRestoreState.pitchOctave != 0 ||
                        mRestoreState.pitchStep != 0 || 
                        mRestoreState.pitchBend != 0);
		}
		else {
			s->pitch = false;
        }

	}
}

/****************************************************************************
 *                                                                          *
 *                                 UTILITIES                                *
 *                                                                          *
 ****************************************************************************/

/**
 * Recalculate one of the frame counters relative to the other.
 */
PRIVATE long Loop::recalculateFrame(bool calcplay)
{
	long loopFrames = getFrames();
	long anchorFrame = (calcplay) ? mFrame : mPlayFrame;
	long otherFrame;

	// play from record is the same as reversed record from play
	// NO, not with the introduction of virtual reverse in Audio
	//bool reverse = (isReverse()) ? calcplay : !calcplay;
	bool reverse = !calcplay;

    // Note that if the loop is very short (minimum length
    // is OutputLatency), the sum of the latency values may be greater
    // than the loop length, so you may need to "wrap" more than once
    // !! doesn't this apply to other calculations

	if (reverse) {
		otherFrame = anchorFrame - mInput->latency - mOutput->latency;
        if (loopFrames > 0) {
            while (otherFrame < 0)
              otherFrame += loopFrames;
        }
	}
	else {
		otherFrame = anchorFrame + mInput->latency + mOutput->latency;  
		otherFrame = wrapFrame(otherFrame, loopFrames);

        // don't think this can happen
        if (otherFrame < 0) {
            Trace(this, 1, "Loop: Unable to recalculate play frame!\n");
            reset(NULL);
        }
	}

	return otherFrame;
}

PRIVATE void Loop::recalculatePlayFrame()
{
    long current = mPlayFrame;
	mPlayFrame = recalculateFrame(true);
}

/**
 * Set the play frame and recalculate the record frame.
 * Used by WindowFunction.
 */
PUBLIC void Loop::movePlayFrame(long frame)
{
    mPlayFrame = frame;
    mFrame = recalculateFrame(false);
    setPrePlayLayer(NULL);
}

/****************************************************************************
 *                                                                          *
 *   							   VALIDATE                                 *
 *                                                                          *
 ****************************************************************************/

/**
 * Handler for a ValidateEvent.
 * Currently these are only scheduled during a loop switch.
 * The presence of one of these prevents the validate() method from
 * running since validation has to be deferred until other events have
 * had a chance to run.  
 *
 * When we get here, it's time to party!
 */
PUBLIC void Loop::validateEvent(Event* e)
{
	validate(e);
}

/**
 * Perform various sanity checks after we complete the event processing
 * for a function.  
 *
 * Check consistency between the record and play frames and the states
 * of the input and output streams.
 *
 * Sigh, if either stream latency isn't an even multiple of the
 * speed adjust, then we'll get roundoff errors in scheduling
 * which at this point will result in the record/play frames
 * having the wrong distance.  For simple halfspeed, the maximum
 * we should be off is 1.  Fix this, but don't introduce a fade.
 * If the adjust is much beyond 1, this may be audible and require
 * a cross fade or interpolation.
 *
 * I tried to work around this by having the latency division round up.
 * That seemed to cure some cases but not all, probably unavoidable.
 *
 * Ignore validation if there is a ValidateEvent scheduled.
 * Ignore validation if there are any other events "in progress",
 * this means that they are active (not pending) and that they
 * have a child event (normally a JumpPlay) that has already been 
 * processed.  In cases like this, we may be dealing with several
 * stacked events and have to wait for them all to finish before
 * checking consistency.
 */
PUBLIC void Loop::validate(Event* event)
{
	Layer * layer = (mPrePlay != NULL) ? mPrePlay : mPlay;

	// ignore validation under certain conditions
    EventManager* em = mTrack->getEventManager();
	bool ignore = em->isValidationSuppressed(event);

	// ignore if we haven't begun playing yet
	if (layer != NULL && !ignore) {

		// also make sure this isn't lingering
		if (layer == getMuteLayer())
			Trace(this, 1, "Loop: Still playing in mute layer!\n");

		long virtualPlayFrame = mFrame + mInput->latency + mOutput->latency;
		long loopFrames = getFrames();

		if (DeferInsertShift && mPlay != NULL) {
			// in this mode we allow the frame counts to diverge
			// but only so much as the inserts
			long inserted = mRecord->getFrames() - mPlay->getFrames();
			loopFrames = mPlay->getFrames();
			virtualPlayFrame -= inserted;
		}

		long wrappedPlayFrame = wrapFrame(virtualPlayFrame, loopFrames);
		int delta = wrappedPlayFrame - mPlayFrame;
		if (delta < 0)
		  delta = -delta;

		if (delta > 0) {

			// 2 and 3 seems to be common for rate shift rounding, though
            // I've also seen up to 5 occasionally with 
            // rapid rate shifts over a two octave range
			if (delta > MAX_ROUNDOFF_DRIFT) {
                int level = ((delta > 5) ? 1 : 2);
                Trace(this, level, "Loop: Major frame resynchronization: "
                      "mPlayFrame=%ld wrappedPlayFrame=%ld\n",
                      mPlayFrame, wrappedPlayFrame);
            }
			else {
				Trace(this, 2, "Loop: Compensating for scheduling roundoff: "
					  "mPlayFrame=%ld wrappedPlayFrame=%ld\n",
					  mPlayFrame, wrappedPlayFrame);
				
				// avoid a fade if this is the rate change roundoff
				// this is fairly common when changing rates, not sure
				// if we can avoid it 
				// NOTE: be careful if we just did an quantized play jump on
				// the current frame we still need a fade, so only prevent
				// it if we haven't already moved
				if (mOutput->getLastFrame() == mPlayFrame)
				  mOutput->setLastFrame(wrappedPlayFrame);
			}

			mPlayFrame = wrappedPlayFrame;
		}

		// TODO: Should we repair these!

		if (mOutput->isReverse() != mInput->isReverse())
		  Trace(this, 1, "Loop: Play/Record contexts have inconsistent reverse!\n");

		if (mOutput->getSpeedOctave() != mInput->getSpeedOctave())
		  Trace(this, 1, "Loop: Play/Record contexts have inconsistent speed octave!\n");
	
		if (mOutput->getSpeedStep() != mInput->getSpeedStep())
		  Trace(this, 1, "Loop: Play/Record contexts have inconsistent speed step!\n");

		if (mOutput->getSpeedBend() != mInput->getSpeedBend())
		  Trace(this, 1, "Loop: Play/Record contexts have inconsistent speed Bend!\n");
		
		if (mOutput->getPitchOctave() != mInput->getPitchOctave())
		  Trace(this, 1, "Loop: Play/Record contexts have inconsistent pitch octave!\n");
	
		if (mOutput->getPitchStep() != mInput->getPitchStep())
		  Trace(this, 1, "Loop: Play/Record contexts have inconsistent pitch step!\n");

		if (mOutput->getPitchBend() != mInput->getPitchBend())
		  Trace(this, 1, "Loop: Play/Record contexts have inconsistent pitch bend!\n");

	}
}

/****************************************************************************
 *                                                                          *
 *   								 PLAY                                   *
 *                                                                          *
 ****************************************************************************/

/**
 * External play method called by Track.
 *
 * Formerly checked for Transition objects indiciating jumps to
 * other loops.  No longer have those, but may want something similar
 * so leave the method in place for awhile.
 */
PUBLIC void Loop::play()
{
    // if we're in pause mode, do not advance
	if (mPause) {
		mOutput->captureTail();
		return;
	}

	playLocal();
}

/**
 * The primary play logic.
 */
PUBLIC void Loop::playLocal()
{
    // determine which layer we're playing
    Layer* layer = (mPrePlay != NULL) ? mPrePlay : mPlay;

	if (layer == NULL)
	  mOutput->captureTail();
	else {
        const char* tracePrefix = (mMute) ? "M" : "P";

		// Calculate the next transition frame, we used to detect
		// transitions here, now this is done by scheduling events,
		// so the only thing we have to deal with now is the loop

        long frames = mOutput->frames;
		long transitionFrame = layer->getFrames();

        // calculate the number of frames remaining before the transition frame
		long remaining = transitionFrame - mPlayFrame;

        if (remaining >= frames) {
			// bliss, no boundary conditions, play away
/*
			Trace(this, 4, "Loop: %s: layer=%ld outputFrame=%ld frames=%ld remaining=%ld\n",
				  tracePrefix, layer->getNumber(), mPlayFrame,
				  frames, remaining);
*/

            mOutput->play(layer, mPlayFrame, frames, mMute);

            notifyBeatListeners(layer, frames);
			mPlayFrame += frames;
		}
        else if (remaining >= 0) {

            // play whatever is left in this layer
			long remainder = frames - remaining;
            if (remaining > 0) {
/*
				Trace(this, 4, "Loop: %s: layer=%ld outputFrame=%ld remaining frames=%ld\n",
					  tracePrefix, layer->getNumber(), mPlayFrame, 
					  remaining);
*/

				mOutput->play(layer, mPlayFrame, remaining, mMute);

                notifyBeatListeners(layer, remaining);
				mPlayFrame += remaining;
			}

			// If we're not in a mode that extends the loop, 
			// start pre-playing the record layer.  
			
			if (mMode == InsertMode) {
				// since we're in mute, it doesn't really matter where we are,
				// but we may need to capture a tail for a canceled
				// JumpPlayEvent 
				mPlayFrame = 0;
			}
			else if (mMode == MultiplyMode) {
				if (mPlay->getCycles() == 1)
				  mPlayFrame = 0;
				else {
					// On the initial multiply, return to zero
					// on remultiply return to the mode start frame.
					// NO: This isn't how EDP works, though it may be 
					// an interesting option.  
					// mPlayFrame = mModeStartFrame;
					mPlayFrame = 0;
				}
				// unlike Insert, this should never cause a fade right?
				mOutput->setLayerShift(true);
			}
			else if (mPrePlay != NULL) {
				// This sometimes means that we processed a JumpPlayEvent
				// that moved us to a different loop, and that loop may
				// be shorter than the current loop.  Continue looping
				// in the next loop's play layer.  

				if (mPrePlay == mRecord) {
					// This can only happen in mutiply/insert when
					// we jump to the "remainder" after the new content
					// in the record layer, if the remainder is small enough,
					// we may have to loop

					if (mMode != MultiplyMode && mMode == InsertMode) {
						// this shouldn't happen unless the record layer is 
						// smaller than InputLatency
						Trace(this, 1, "Loop: Reached end of record layer preplay!\n");
					}
				}

				mOutput->setLayerShift(true);
				mPlayFrame = 0;
			}
			else {

				if (ScriptBreak) {
					int x = 0;
				}

				// begin preplay of the last record layer
				mPrePlay = mRecord;
				layer = mRecord;
				//Trace(this, 4, "Loop: Playback jumping to record layer\n");

				// once we've jumped, we can't be in this mode
				if (mMode == RehearseMode)
				  mMute = false;

                // The stream will decide whether or not a fade over
                // the layer boundary is necessary, see OutputStream::play

				mPlayFrame = 0;
			}

			//Trace(this, 4, "Loop: Playback returning to %ld\n", mPlayFrame);

			// continue filling from the head of the next layer
			// (either record layer, or play layer if multiplying)
			//Trace(this, 4, "Loop: %s: layer=%ld outputFrame=%ld remainder frames=%ld\n",
			//tracePrefix, layer->getNumber(), mPlayFrame, remainder);

			mOutput->play(layer, mPlayFrame, remainder, mMute);

			notifyBeatListeners(layer, remainder);
			mPlayFrame += remainder;
		}
        else if (remaining < 0) {
            // Remaining is negative which means mPlayFrame got beyond the
			// end of the play layer.
            Trace(this, 1, "Loop: Playback frame anomoly: mPlayFrame=%ld transitionFrame=%ld remaining=%ld\n", mPlayFrame, transitionFrame, remaining);
            reset(NULL);
        }
        else {
            // remaining is zero, this happens in cases where the loop length
            // is the same as OutputLatency, ignore
        }
	}
}

/**
 * Set flags to trigger visualization changes on interesting "beats".
 * Originally this used callbacks, now we just set flags that are 
 * eventually returned in an TrackState when the application requests them.
 * This is simpler and avoids callback and asynchronous event complications.
 *
 * There are three beats: loop start, cycle start, and sub-cycle start.
 * This is a little tricky because the visuals must align with
 * the audible output, which means that we have to trigger them
 * at mPlayFrame - OutputLatency since we will have buffered audio
 * for future playback.
 *
 * ?? Will have problems here for those cases where mPlayFrame has
 * to jump into the middle of the record buffer without starting at zero?
 */
PRIVATE void Loop::notifyBeatListeners(Layer* layer, long frames)
{
	// Don't do this in insert mode since we're never really
	// returning to the loop start?

	long loopFrames = layer->getFrames();

	if (loopFrames == 0) {
		// how can this happen?
		Trace(this, 1, "Loop: notifyBeatListeners: zero length loop\n");
	}
	else {
		int cycles = layer->getCycles();
		long firstLoopFrame = 0;

        // this is the user's percieved play frame
        long playFrame = mPlayFrame - mOutput->latency;
		if (playFrame < 0)
			playFrame += loopFrames;

        // is firstLoopFrame within this window?
        long delta = playFrame - firstLoopFrame;

        if (delta < frames) {
            mBeatLoop = true;
            LoopStartPoint->notify(mMobius, this);
        }

		long lastBufferFrame = playFrame + frames - 1;

		long cycleFrames = loopFrames;
		if (cycles > 1) {
			cycleFrames = loopFrames / cycles;
			int delta = lastBufferFrame % cycleFrames;
			// delta is now the number of frames beyond a cycle point
			// if we get a negative number subtracting the frame count,
			// then this block of frames surrounded a cycle boundary
			if (delta - frames <= 0) {
                mBeatCycle = true;
                LoopCyclePoint->notify(mMobius, this);
            }
		}

		// similar calculation as for cycles, is there a faster way?
		// !! this is the same roundoff problem that 
		// getQuantizedFrame has
		int ticks = mPreset->getSubcycles();
		// sanity check to avoid divide by zero
		if (ticks == 0) ticks = 1;
		long tickFrames = cycleFrames / ticks;
		delta = lastBufferFrame % tickFrames;
		if (delta - frames <= 0) {
            mBeatSubCycle = true;
            LoopSubcyclePoint->notify(mMobius, this);
        }

		if (mBeatLoop || mBeatCycle || mBeatSubCycle) {
			// mark the track as needing an UI update, later Mobius
			// will post an event to the Mobius thread so it can tell
			// the UI to refresh itself
			mTrack->setUISignal();
		}
	}
}

/****************************************************************************
 *                                                                          *
 *   								RECORD                                  *
 *                                                                          *
 ****************************************************************************/

/**
 * Called as we identify sections of an audio input buffer to record.
 * mInput has the region to consume.
 *
 * The logic is split between InputStream and Loop in a somewhat
 * confusing way, but I really want to keep halfspeed calculations
 * and last record state encapsulated in InputStream.  Since there's
 * not much left here, we could just move the entire record method
 * over to InputStream.
 *
 * During normal recording we call InputStream.setLastRecord to keep
 * track of the last record layer and frame.
 */
PUBLIC void Loop::record()
{
    EventManager* em = mTrack->getEventManager();
	long frames = mInput->frames;
    int feedback = getEffectiveFeedback();

	if (mPause) {
		// let the layer know if there will be a content gap
		if (mRecording && frames > 0)
		  mRecord->pause(mInput, mFrame);
	}
	else if (mMode == ThresholdMode) {

		// Would be nice if InputStream could have already done this.
		// !! Technically this should be done by stream, and the buffer
		// split at the threshold point so we start exactly on the
		// threshold frame.
		if (checkThreshold()) {

			Event* start = em->findEvent(RecordEvent);
			if (start == NULL) {
				Trace(this, 1, "Sync: Record start pulse without Record event!\n");
				// well we can at least try, this is the old code we had before
				// scheduling a pending event, might be something missing?
				setMode(RecordMode);
				if (mFrame < 0)
				  Trace(this, 1, "Loop: Negative record frame!\n");
				else
				  mRecord->record(mInput, mFrame, feedback);
			}
			else if (!start->pending) {
				// already started somehow, ignore
				Trace(this, 1, "Loop: Threshold record found active Record event!\n");
			}
			else {
				// activate the event, since these haven't been used
				// to carve up the buffer we have to start it now, can't
				// schedule it for the current frame because Track is already
				// beyind that frame by the time it calls Loop::record.
				Trace(this, 2, "Loop: Activating threshold record event\n");
				em->removeEvent(start);
				start->frame = mFrame;
				start->pending = false;
                em->processEvent(start);
				mRecord->record(mInput, mFrame, feedback);
			}
		}
	}
	else if (mRecording) {

		if (mFrame < 0) {
			Trace(this, 1, "Loop: Negative record frame!\n");
		}
		else if (mMode == InsertMode) {
			Trace(this, 4, "Loop: I recordFrame=%ld length=%ld\n", mFrame, frames);
			mRecord->insert(mInput, mFrame, feedback);
        }
        else {
            //Trace(this, 4, "Loop: R layer=%ld recordFrame=%ld frames=%ld\n",
			//mRecord->getNumber(), mFrame, frames);
            mRecord->record(mInput, mFrame, feedback);
        }
	}
    else if (mMode == InsertMode) {
        // We must be in that limbo area after we stop recording
        // an insert, but before the end.  The area must be padded
        // with slience.
		mInput->buffer = NULL;
		mRecord->insert(mInput, mFrame, feedback);
	}
	else if (mRecord != NULL) {
		// still have to tell the layer to copying the previous layer
		// ?? could tell it to fade now rather than waiting till the finalize
		mRecord->advance(mInput, mFrame, feedback);
	}

	// advance the frame counter as we record
	if (mMode != ResetMode && 
		mMode != ThresholdMode && 
		mMode != SynchronizeMode &&
		!mPause) {

		mFrame += frames;
		// sanity check
		if (mRecord != NULL) {
			long recorded = mRecord->getRecordedFrames();
			if (mFrame > recorded) {
                // if we get into this state it will tend not to fix itself
                // and will spam a huge number of messages to trace
                Trace(this, 1, "Loop: Record length anomoly, recorded %ld frame %ld\n",
                      recorded, mFrame);
            }
		}
	}
	else {
		// We allow waits to be scheduled in ResetMode and Pause mode
        em->advanceScriptWaits(frames);
	}

}

/**
 * Determine the level of feedback to apply during layer recording.
 * Made this public so we can get to it from the EffectiveFeedback variable.
 */
PUBLIC int Loop::getEffectiveFeedback()
{
    int feedback = mTrack->getFeedback();

    if (mMuteMode) {
        // always 100% in mute mode
		// !! really, seems like something we might want to relax
		// !! maybe a variant on MuteCancel to select this
        feedback = 127;
    }
    else {
		if (mPreset->isAltFeedbackEnable()) {
            // Similar to what the EDP calls InterfaceMode=Expert.
            // Primary feedback active during play, secondary during
            // overdub, multiply, substitute.  Assuming this means
            // any recording mode.  For insert this doesn't really make
			// sense but won't hurt since we're not bringing any
			// content forward.  For Replace though, we want to
			// use normal feedback or force it to zero because we will
			// have performed a play mute.
            if (mRecording) {
				if (mMode == ReplaceMode)
				  feedback = 0;
				else if (!mMode->altFeedbackDisabled)
				  feedback = mTrack->getAltFeedback();
			}
		}
		else if (mMode == ReplaceMode || mMode == SubstituteMode) {
			feedback = 0;
		}

        // apply reduction if overdubbing
		if (feedback == 127 && mAutoFeedbackReduction &&
			(mOverdub || mMode == MultiplyMode || mMode == StutterMode)) {

			feedback = AUTO_FEEDBACK_LEVEL;
		}
	}

    return feedback;
}

/**
 * Return true if the current stream buffer has samples that exceed
 * the record threshold.
 */
PRIVATE bool Loop::checkThreshold()
{
    bool ok = false;

	// determine the absolute maximum sample, remember to convert
	// frames to samples
	float max = 0.0f;
	int slength = mInput->frames * mInput->channels;
	for (int i = 0 ; i < slength ; i++) {
		float s = mInput->buffer[i];
		if (s < 0.0f) s = -s;
		if (s > max) max = s;
	}
	int imax = SampleFloatToInt16(max);

	// doc says: Each successive number represents a 6db increase
	// in the volume necessary to trigger recording, so 1 is very 
	// sensitive and 8 requires Pete-Townshend like moves

	// ?? Being DSP challenged, I don't know how to map a "db" 
	// into a sample value.  Started by carving up the range
	// evenly (divide by 9), but it required too much force even at 1.
	// I think this needs to be a logarithmic scale.  Dividing
	// by 32 makes level 1 and 2 usable.
	int required = mPreset->getRecordThreshold() * (32768 / 32);

	if (imax >= required) {
		// technically, we should begin recording at or near
		// the frame that contained the max sample
		// but since interrupt buffer sizes are relatively small,
		// just assume we can use the whole thing

		ok = true;
	}

    return ok;
}

/****************************************************************************
 *                                                                          *
 *                                   SHIFT                                  *
 *                                                                          *
 ****************************************************************************/

/**
 * Shifting is the process of making the current record layer become
 * the new play layer, and creating a new record layer by copying
 * the previous record layer.
 *
 * ?? On page 5-5, when in Overdub or Multiply and Feedback=100%,
 * the EDP automatically drops this to around 95% to prevent
 * overload.  This could have been handled while the overdub was
 * being performed but we'd have to pre-attenuate the record layer
 * which we wouldn't want if there turned out not to be any overdubs.
 * We could also defer the merger of the old/new audio until
 * the shift.  The simplest thing is to attenuate at the end, but
 * note that we'll only detect this if OVERDUB hanpens to still
 * be on.  MULTIPLY won't be caught since we don't shift during multiply.
 * TODO: A more robust mechanism would be to keep a flag in Layer
 * that is set whenever an overdub/multiply happens, this is different
 * than the changed flag which also happens in inserts.
 *
 */
PUBLIC void Loop::shift(bool checkAutoUndo)
{
	if (ScriptBreak) {
		int x = 0;
	}

	if (mRecord == NULL) {
		Trace(this, 1, "Loop: shift: no record layer\n");
	}
    else if (mMode == RehearseMode) {

        // Move the record layer to the play layer and zero
        // the record layer rather than copy the play layer
        // ?? I think the EDP will lose the previous play layer
        // on each phase, but we could keep stacking them for undo?
        bool undoableRehearse = false;
        if (undoableRehearse)
		  addUndo(mRecord);
		else
          mRecord->freeUndo();
        
		mPlay = mRecord;
        mPrePlay = NULL;

        LayerPool* lp = mMobius->getLayerPool();
        mRecord = lp->newLayer(this);

        mRecord->zero(mPlay->getFrames(), 1);
        mRecord->setPrev(mPlay);

        Trace(this, 3, "Loop: shift: playing %ld, new rehearse layer %ld\n",
			  mPlay->getNumber(), mRecord->getNumber());
    }
	else {
		// If we're preplaying in a different Loop, do the record/play
		// shift, but leave mPrePlay alone
		bool switching = (mPrePlay != NULL) && (mPrePlay->getLoop() != this);
        bool audioChanged = isLayerChanged(mRecord, checkAutoUndo);
        bool feedbackChanged = mRecord->isFeedbackApplied();

		if (!audioChanged && !feedbackChanged && mPlay != NULL) {
			// squelch the record layer

			// Have to be careful with Mute fades placed in the 
			// mRecord layer when it was mPrePlay.  If we decide not to 
			// shift, have to transition the fade back to the mPlay layer.
			mRecord->transferPlayFade(mPlay);
			
			if (!switching) {
				// OutputStream thinks the last layer was mRecord, 
				// if there was a Replace operation on frame zero, we will 
				// immediately begin modifying mRecord.  When OutputStream
				// notices that we are muted, it will try to capture a fade
				// tail, but we've already modified the layer at that point.
				// To avoid this, make OutputStream think the last layer
				// was mPlay which will be accurate since we're squelching.
				// This only applies though if the last layer was the
				// current record layer, so pass that in to check.
				mOutput->squelchLastLayer(mRecord, mPlay, mPlayFrame);
				mPrePlay = NULL;
			}
			
			// transfer checkpoint state if explicitly changed
			// NOTE: this should no longer be used, we always shift
			// before setting a checkpoint in the play layer
			CheckpointState check = mRecord->getCheckpoint();
			if (check != CHECKPOINT_UNSPECIFIED)
			  mPlay->setCheckpoint(check);

            if (mRecord->isAudioChanged()) {
				Trace(this, 3, "Loop: shift: reusing squelched record layer %ld\n",
					  mRecord->getNumber());
				// treat this like an undo so we apply a deferred tail
                // fade which may have some low level noise
				mPlay->restore(true);
            }
            else {
                Trace(this, 3, "Loop: shift: reusing unchanged record layer %ld\n",
					  mRecord->getNumber());
			}
            mRecord->copy(mPlay);
		}
		else {
			// a normal shift
			// TODO: if !audioChanged and the NoFeedbackUndo
			// option is on, then release the current play layer.
			// If we're not flattening, this will require some surgery
			// so the segments reference the the layer before the play layer
			// with a higher feedback level.  When flattening, it is easier
			// if there are no segments, but if there are residual segments
			// due to a retrigger, then we may not be able to free the layer.
			
            addUndo(mRecord);

            mRecord = mPlay->copy();
			mRecord->setPrev(mPlay);
			if (!switching)
			  mPrePlay = NULL;

			Trace(this, 3, "Loop: shift: playing %ld, new record layer %ld\n",
				  mPlay->getNumber(), mRecord->getNumber());
		}
	}
}

/**
 * Determine if any significant recording was made to the a layer.
 * Used by shift() to see if it can squelch an empty record layer.
 * Used by undoEvent() to determine whether to undo record ayer
 * or the play layer.
 *
 * Structure changes (multiply, insert, etc.) always cause a shift
 * but if there was only an overdub with no audio above the noise
 * floor, can ignore it. EDP calls this "auto undo"
 */
PRIVATE bool Loop::isLayerChanged(Layer* layer, bool checkAutoUndo)
{
	bool changed = mRecord->isStructureChanged();
	if (!changed) {
		changed = mRecord->isAudioChanged();
		if (changed && checkAutoUndo &&  layer->getPrev() != NULL) {
			short max = SampleFloatToInt16(layer->getMaxSample());
			if (max < 0)
			  max = -max;
			MobiusConfig* c = mMobius->getInterruptConfiguration();
			changed = (max > c->getNoiseFloor());
		}
	}
	return changed;
}

/**
 * Add a layer to the undo list.
 * Formerly checked the MaxUndo parameter here and pruned the list
 * but that has to be deferred until Layer::finalize because we may still
 * need the previous layer and when MaxUndo=1 we can't take it away here.
 */
PRIVATE void Loop::addUndo(Layer* l)
{
    l->setPrev(mPlay);
    mPlay = l;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************
 *                                                                          *
 *   						 FUNCTION PROCESSING                            *
 *                                                                          *
 ****************************************************************************/
/****************************************************************************/
/****************************************************************************/

void Loop::setPause(bool b)
{
	mPause = b;
}

void Loop::setMute(bool b)
{
	mMute = b;
}

/**
 * To be used only by MuteFunction when in reset.
 * Also called by PlayFunction.
 */
 void Loop::setMuteMode(bool b)
{
    mMuteMode = b;
}

/**
 * To be called *only* by OverdubFunction when we're in reset.
 * Also now called by PlayFunction.
 */
void Loop::setOverdub(bool b) {
	mOverdub = b;
}

/**
 * Only for use by function event handlers.
 */
void Loop::setRecording(bool b) 
{
	mRecording = b;
}

/****************************************************************************
 *                                                                          *
 *   								RESET                                   *
 *                                                                          *
 ****************************************************************************/

/**
 * Local Reset.  May be part of a TrackReset being processed by Track.
 * Reset must leave intact any tempos set using TempoSelect.
 */
PUBLIC void Loop::reset(Action* action)
{
	clear();

	mMode = ResetMode;
	mRecording = false;
	mOverdub = false;
	mPause = false;
	mMute = false;
	mMuteMode = false;
	mModeStartFrame = 0;
    mRestoreState.init();

	// returning to Reset cancels Reverse, though you can arm it again
	mInput->reset();
	mOutput->reset();

	// reset pseudo-event state
	mBeatLoop = false;
	mBeatCycle = false;
	mBeatSubCycle = false;

    // no more events
    // !! this should be done up in Track
    EventManager* em = mTrack->getEventManager();
	em->reset();

	// do this after Stream::loopReset so we get the right latency
	setFrame(-mInput->latency);
	mPlayFrame = mOutput->latency;

	mSynchronizer->loopReset(this);
}

/**
 * Release all of the audio in this loop.  Used for Reset,
 * EmptyLoopMode, and RecordFollow.
 *
 * Do *not* trash mFrame here, EmptyLoopMode needs to preserve it.
 */
PRIVATE void Loop::clear()
{
	// if the IO streams are pointing at this loop, reset history
	// and capture fade state as necessary

    mOutput->resetHistory(this);
	mInput->resetHistory(this);

	if (mRecord != NULL) {
		mRecord->freeAll();
		mRecord = NULL;
	}

	// this is always from the mRecord chain
	mPlay = NULL;
	mPrePlay = NULL;

    // remember these are linked with the Redo pointer and
    // each element can be a list linked by Prev
    Layer* nextRedo = NULL;
    for (Layer* redo = mRedo ; redo != NULL ; redo = nextRedo) {
        nextRedo = redo->getRedo();
        redo->freeAll();
    }
    mRedo = NULL;
}

/****************************************************************************
 *                                                                          *
 *                                    LOOP                                  *
 *                                                                          *
 ****************************************************************************/

/**
 * Handler for a pseudo-event we generate when we reach the loop frame.
 */
PUBLIC void Loop::loopEvent(Event* e)
{
	// If we're in Multiply mode but have no play layer, then we must have
	// ended the initial record with a Multiply.  Don't add a cycle yet,
	// just a normal shift.  Same goes for Insert.

	// !! is this true?  why are we entering the mode before shifting
	// if not, simplify this
	if (mPlay == NULL && mMode->rounding)
	  Trace(this, 1, "Loop: Missing play layer in insert/multiply mode\n");

	if (mMode == MultiplyMode && mPlay != NULL) {

		mRecord->multiplyCycle(mInput, mPlay, mModeStartFrame);

		Trace(this, 2, "Loop: New cycles %ld loop frames %ld\n", 
			  (long)mRecord->getCycles(), mRecord->getFrames());

		// unlike below we do not reset mFrame here, run free!
	}
	else if (mMode == InsertMode && mPlay != NULL) {

		// Should be here only if the insert cycle boundary is
		// exactly on the loop boundary.  Otherwise, we'll extend
		// the record layer in Layer::insert as we go and will
		// never get to the LoopEvent.

		// !! hmm, since we have to handle this case, maybe it makes
		// sense to maintain the insert cycle state out here rather
		// than in Layer and synthesize pseudo InsertCycleEvents like
		// we used to.  Also try to make Insert and Multiply the same.

		mRecord->continueInsert(mInput, mFrame);
	}
	else if (mMode == StutterMode) {

		stutterCycle();
	}
    else if (mMode == RehearseMode) {

        if (mRecording) {
            Trace(this, 2, "Loop: Entering rehearse mode play phase\n");
            shift(false);
            setFrame(0);
            mRecording = false;
			mMute = false;
        }
        else {
            // we've just played the rehearse cycle once, 
            // record another one but do not lose the cycle time
            // the record layer will already be zeroed
			// !! if we're in host sync mode, we may drift during
			// rehearse, could try to detect this and retrigger
			// the rehearse loop from an adjusted location

            Trace(this, 2, "Loop: Entering rehearse mode record phase\n");

            mPrePlay = NULL;
            setFrame(0);
			mMute = true;
			mRecording = true;
        }
		
        EventManager* em = mTrack->getEventManager();
        em->shiftEvents(getFrames());
    }
	else {
		// Take the loop, do not check auto-undo if we're
		// in a mode that can change the loop size.
		// Can't check mMode here because that may be PlayMode after
        // the mode that did the extension finishes.  Just check size.
                
		Layer* prev = mRecord->getPrev();
		bool checkAutoUndo = 
			(prev != NULL && prev->getFrames() == mRecord->getFrames());

        // !! if we're going into Rehearse mode, there's no need
        // to copy the play buffer into the new record buffer, it just
        // needs to be zeroed.  shift has logic to do that but mMode
        // has to be RehearseMode and we won't know that till we
        // process the event, could peek into the event list?
		shift(checkAutoUndo);

		// TODO: If Subcycles changed, we're supposed to wait
		// for the LoopStart frame (now) to put it into effect, currently
		// it will take effect immediately.  
        // Hmm, not sure I dislike this.

		setFrame(0);

        EventManager* em = mTrack->getEventManager();
        em->shiftEvents(getFrames());

		mSynchronizer->loopLocalStartPoint(this);
	}
}

/****************************************************************************
 *                                                                          *
 *   								RECORD                                  *
 *                                                                          *
 ****************************************************************************/


/**
 * Stop playback immediately.
 * Currently called only by Synchronizer before it schedules
 * a RecordEvent event.  Since we know we're going to stop
 * playback in InputLatency, we may as well stop it now.
 * Though it wouldn't really hurt to defer this to RecordFunction::doEvent 
 * and would be simpler.
 */
PUBLIC void Loop::stopPlayback()
{
    EventManager* em = mTrack->getEventManager();
	em->cancelSwitch();
    
    // leave the layers in place, we may still need to perform
    // shifting before the record event can be processed!
    mMute = true;
}

/****************************************************************************
 *                                                                          *
 *                                RECORD STOP                               *
 *                                                                          *
 ****************************************************************************/
//
// Trying to move as much of this as possible to RecordFunction but
// still have some lingering dependencies...
//

/**
 * Return the minimum size allowed for this loop.
 * This is a function of the latency values so it is actually
 * a global setting, not loop specific, but it's convenient to have it here.
 * 
 * Various calculations do not support a loop that is less than either
 * of the latency values.  With a slow driver latency of 9216 frames, 
 * this can happen with short SUS records.
 *
 * UPDATE: Odd behavior when loop is just OutputLatency, I think it needs
 * to be the sum of both,  ugh, but then we get scads of
 * "play frame overflow messages"
 *
 * !! revisit this, at low latencies we probably want a fixed minimum
 * that is even higher just to prevent excessive trace among other things.
 * What if overrides are both zero?
 *
 */
PUBLIC long Loop::getMinimumFrames()
{
	long min = (mInput->latency > mOutput->latency) ? 
		mInput->latency : mOutput->latency;

    // just in case overrides are zero
    if (min < 32) min = 32;

    return min;
}

/**
 * When the initial recording is about to end, prepare the loop by 
 * calculating the ending frame count and begin pre-play of the record layer.
 * The inputLatency flag indiciates whether or not we will be delaying
 * the actual ending to compensate for latency.  It will be true if the 
 * prepare could be done far enough in advance and we have time to do 
 * a proper pre-play.  False if the prepare and end have to be done at
 * the same time with a skip in pre-play.
 *
 * This is used both by RecordFunction::doEvent when it processes
 * RecordStopEvent and by Synchronizer when it activates a pending
 * RecordStop event and in some cases when it schedules one.
 * !! FIgure out why we need all these...
 */
PUBLIC void Loop::prepareLoop(bool inputLatency, int extra)
{
    // !! not implemented yet, think this through
    bool doExtra = false;

	int remaining = (inputLatency) ? mInput->latency : 0;
    if (doExtra)
      remaining += extra;

	long loopFrames = mFrame + remaining;
	long min = getMinimumFrames();

	if (loopFrames < min) {
        // this will certainly screw up synchronization, and pre-play
        // alignment but we really shouldn't have loops this small
		Trace(this, 1, "Loop: Loop too small, adjusting from %ld to %ld\n",
			  loopFrames, min);
		loopFrames = min;
	}

	Trace(this, 2, "Loop: prepareLoop: loop frames %ld remaining %ld\n", 
		  loopFrames, (long)remaining);

	// set this now so we can begin using it in calculations before
	// all of the frames are actually recorded
	mRecord->setPendingFrames(mInput, loopFrames, remaining);

	// immediately begin playing the record loop
	mPrePlay = mRecord;

	// don't trust these to be in sync
    // Pre 2.2 we wouldn't set output stream state until now, in 2.2
    // we're doing it for rate and pitch but not reverse yet
	mOutput->setReverse(mInput->isReverse());
	mOutput->setSpeedOctave(mInput->getSpeedOctave());
	mOutput->setSpeedStep(mInput->getSpeedStep());
	mOutput->setSpeedBend(mInput->getSpeedBend());
    mOutput->setTimeStretch(mInput->getTimeStretch());
	mOutput->setPitchOctave(mInput->getPitchOctave());
	mOutput->setPitchStep(mInput->getPitchStep());
	mOutput->setPitchBend(mInput->getPitchBend());

    // Kludge: In 2.2 we started setting the output stream speed
    // immediately if the speed was changed before recording started.
    // This can mean that at the point we're ready to actually start
    // playing, there can be 1 frame of speed shift "remainder" queued
    // up which will take away from the next advance.  This results
    // in a slight difference in the test files.  We should really
    // not do this and regen the files but I'm trying to just get 2.2
    // to work as it did before for basic half speed.
    mOutput->resetResampler();

	// note that mPlayFrame has to be set after setting speed since
    // this adjusts the latency
	mPlayFrame = mOutput->latency;

	// if we're not compensating for input latency, then have to jump
	// ahead by that amount too
	if (!inputLatency)
	  mPlayFrame += mInput->latency;

    // if we added extra frames
    // !! what, it feels like we need to subtract the extra
    // from mPlayFrame because we gave it more breathing room beyond
    // inputLatency but what if that goes zero (output latency
    // would have to be really small?  Figure this out...
    if (doExtra) {
        mPlayFrame -= extra;
        if (mPlayFrame < 0) {
            Trace(this, 1, "Loop: prepareLoop extra adjust underflow!\n");
            mPlayFrame = 0;
        }
    }

	// MUTE MODE!!: Formerly did an unconditional unmute here, but it
	// is interesting to let the loop new loop stay muted
	//mMute = false;

	// now that we know the loop frame, also can now schedule
	// the return event
    EventManager* em = mTrack->getEventManager();
    em->finishReturnEvent(this);
}

/**
 * Called by event handlers to end the recording started by
 * the previous mode.  Drop back into play mode, though the 
 * event handler that called us will usually change this.  
 * 
 * We used to have RecordStopEvent handling for Record mode here but
 * that has been moved to RecordFunction.  
 * 
 * Hmm, don't like having the mode specific stuff in here, but
 * can't think of an easy way to factor it out since there are
 * too many dependencies.
 */
PUBLIC void Loop::finishRecording(Event* e)
{
    if (mRecording) {

        // formerly handled RecordMode here but that should
        // now be done by RecordFunction::doEvent
        if (mMode == RecordMode)
          Trace(this, 1, "Loop::finishRecording called during Record mode\n");

        // turn this off now so alternate endings may turn it back on
        mRecording = false;

		// Script Kludge: avoid a fade out on the right edge for
		// some tests, is this a Record mode only thing?
        mRecord->setFadeOverride(e->fadeOverride);
        
        // Normally will drop into play mode, but some modes
        // (Multiply, Insert) need to stop recording but stay in that mode.    
        MobiusMode* newMode = PlayMode;
        if (mMode->rounding)
          newMode = mMode;

        // mPlayFrame should be preplaying from the beginning
		// !! what about switching?, will be prepalying the next layer
        long loopFrames = getFrames();
        if (mPlayFrame >= loopFrames) {
            Trace(this, 1, "Loop: Unexpected play frame warping playFrame=%ld loopFrames=%ld\n", mPlayFrame, loopFrames);
			mPlayFrame = wrapFrame(mPlayFrame, loopFrames);
        }

        // if we looped back to the start frame, shift any future events
        setFrame(mFrame);
        if (mFrame == 0) {
            EventManager* em = mTrack->getEventManager();
            em->shiftEvents(getFrames());
        }

		// if we're switching remember the new mode, but don't display it
		if (newMode != PlayMode)
		  setMode(newMode);
        else {
            // drop out of recording mode, resume overdub if left on
            resumePlay();
		}

		// do NOT call validate() here, the mode may still be active
    }
}

/****************************************************************************
 *                                                                          *
 *   								 PLAY                                   *
 *                                                                          *
 ****************************************************************************/

/**
 * Called when we're dropping out of various modes and can resume playback.
 * If overdub is still on, we resume overdub instead.
 */
PUBLIC void Loop::resumePlay()
{
    // normally off by now, but make sure
    mRecording = false;

	if (mMode != ResetMode) {
		if (!mOverdub) {
			if (mMuteMode)
			  setMode(MuteMode);
			else
			  setMode(PlayMode);
		}
		else if (mFrame >= 0) {
			setMode(OverdubMode);
			mRecording = true;
		}
        else {
            // should only be here if we're comming out of Mute
            // with overdub left on
            // ?? really how can the frame be negative
            Trace(this, 1, "Loop: Negative resumePlay frame, can this happen?\n");
            EventManager* em = mTrack->getEventManager();
            Event* e = em->newEvent(Overdub, 0);
			em->addEvent(e);
        }

		// in all cases, this turns off
		mPause = false;

		// this should already be set, but make sure
		// but be careful because it may be on for other reasons
		if (mMuteMode)
		  mMute = true;
	}
}

/**
 * Called by various event handlers when the mode corresponding
 * to the event is entered or exited.  Depending on the MuteCancel parameter,
 * we either cancel or preserve the current mute state.
 *
 * Sometimes a prior jumpPlayEvent will have changed mMute given
 * logic in jumpPlayEvent.  Some modes that don't ordinarily
 * require play jumps do not (Overdub, Multiply).
 */
PRIVATE bool Loop::checkMuteCancel(Event* e)
{
	bool canceled = false;

	if (e == NULL)
	  Trace(this, 1, "Loop: checkMuteCancel called with NULL event!\n");
	else {
		Function* func = e->function;
		if (func == NULL)
		  Trace(this, 1, "Loop: checkMuteCancel called with NULL function!\n");

		else if (mMuteMode && func->isMuteCancel(mPreset)) {
		
			mMuteMode = false;
			mMute = false;
            mPause = false;
			canceled = true;

			if (mMode == MuteMode) {
				// not in a mode previously, drop back into Play or Overdub	
				resumePlay();
			}
		}
	}
	return canceled;
}

/****************************************************************************
 *                                                                          *
 *                                    MUTE                                  *
 *                                                                          *
 ****************************************************************************/

/**
 * Direct mute control without events.
 * This was added to support Bounce recording which needs to 
 * mute tracks without scheduling MuteEvents.
 *
 * It is also used by Solo.
 *
 * It was easiest just to fake up a MuteEvent, though we may not want
 * all of the same parameter handling?
 *
 * !! What about synchronization, if we're going to mute the sync 
 * master, we should have transfered control before now?
 *
 * !! I hate this interface, rework this to schedule normal events if we can!!
 *
 * Called by Track::setMute(Function,bool)
 * Indirectly called by MuteFunction::globalMute, SoloFunction::invoke, 
 * and Mobius::toggleBounceRecording.
 * 
 */
PUBLIC void Loop::setMuteKludge(Function* f, bool mute)
{
	// ignore if we're not in a mode that indicates content
	// need a MobiusMode flag for this!!
	if  (isAdvancing()) {

		if ((mute && (!mMuteMode && !mPause)) || (!mute && (mMuteMode || mPause))) {
            EventManager* em = mTrack->getEventManager();

            // The function needs to be in the mute family so we
            // get to the event handler.  invokingFunction carries
            // around the extra context
            Function* muteFunction = (mute) ? MuteOn : MuteOff;
			Event* e = em->newEvent(muteFunction, MuteEvent, mFrame);
            e->setInvokingFunction(f);
			e->savePreset(mPreset);
		
			// this was added for things like Bounce where the event may 
			// not be happening at the "right" latency adusted time, so
			// set the "insane" flag to disable sanity checking after the
			// mode transition
			e->insane = true;

            e->invoke(this);
		}
	}
}

/****************************************************************************
 *                                                                          *
 *   						   MULTIPLY/INSERT                              *
 *                                                                          *
 ****************************************************************************/

/**
 * Common Multiply/Insert end scheduling.
 * 
 * We are in Multiply or Insert mode and another function was used.
 * The event passed is the primary event for the trigger function.
 * We will schedule either MultiplyEndEvent or InsertEndEvent along
 * with a JumpPlayEvent as it's child.
 *
 * The trigger event will be ignored if it is the normal end event
 * for this mode (Multiply or Insert) or if it is one of the alternate
 * endings such as Record for Unrounded Multiply.
 *
 * If we decide to keep the trigger event, the end event will be set
 * as it's child.  
 *
 * NOTE: In retrospect, we really don't need a set of "end" event types,
 * we could just reuse the main event and check to see if we're already
 * in the mode, like we do for most other things.
 *
 * Trigger event is also ignored if we've already ended the mode and the 
 * function isn't one that increases the length of the mode or forces
 * a unrounded ending.  
 * Q: Could in theory just schedule them after the ending?
 * 
 *
 * The Action and the Event will be linked together.  If we decide
 * to ignore the event the Action must be transfered to the mode end event.
 *
 * TODO: If the multiply originated with either the
 * SUSRoundMultiply or SUSUnroundedMultiply functions, then we
 * may need to ignore Preset::isRoundOverdub here?
 * 
 * !! Need to consistently detect events that come in during a 
 * SUS operation and either ignore them, or cancel the SUS.
 * This makes it possible to have "dangling" SUS up transitions
 * come in after the SUS mode was canceled.
 */
PUBLIC Event* Loop::scheduleRoundingModeEnd(Action* action, Event* event)
{
    Event* endEvent = NULL;
	bool ignoreTrigger = false;
	Function* function = event->function;
	Function* primaryFunction;
	EventType* endType;

	// !! needs to be encapsulated in the MobiusMode, or else the
	// MobiudMode needs to reference a Function which can provide these
	if (mMode == InsertMode) {
		primaryFunction = Insert;
		endType = InsertEndEvent;
	}
	else {
		primaryFunction = Multiply;
		endType = MultiplyEndEvent;
	}

    EventManager* em = mTrack->getEventManager();
	Event* prev = em->findEvent(endType);

    if (prev != NULL) {
		if (isUnroundedEnding(function)) {
			if (prev->quantized) {
				// escape quantization and perform an unrounded operation
				Trace(this, 2, "Loop: Escaping quantization after %s during %s\n",
					  function->getName(), mMode->getDisplayName());

				// note that the primary function stays the same
                // !! woah, hate this
				prev->setInvokingFunction(function);
				int latency = (action->noLatency) ? 0 : mInput->latency;
				long newFrame = mFrame + latency;

				if (newFrame < prev->frame)
				  moveModeEnd(prev, newFrame);
				else {
					// we're about ready to process the future event anyway,
					// leave it alone
				}
			}
			else {
				Trace(this, 2, "Loop: Ignoring %s during %s quantize period\n",
					  function->getName(), mMode->getDisplayName());
			}
        }
        else if (function == primaryFunction) {
			// this is "multi increase"
			// how does MULTIPLY_OVERDUB effect this?

            // start accumulating the count in the event for display
            if (prev->number == 0)
              prev->number = 2;           // first time
            else
              prev->number++;

			Trace(this, 2, "Loop: Increase %s to %ld\n", 
				  mMode->getDisplayName(), (long)prev->number);

			long newFrame = prev->frame + getCycleFrames();
			moveModeEnd(prev, newFrame);
		}
		else {
			// if we've got a NextLoop ending event, this could
			// be another one to increment the target loop, that 	
			// will be handled above this
			Trace(this, 2, "Loop: Ignoring %s during %s quantize period\n",
				  function->getName(), mMode->getDisplayName());
		}

		// in all cases, the trigger event should not be scheduled
		// !! crap, we don't want to unschedule a NextLoop so we can 
		// continue to press it to incrremnt the loop, need another
		// flag on functions like "incrementable" to generalize this
		if (event != em->getSwitchEvent())
		  ignoreTrigger = true;
    }
	else if (isUnroundedEnding(function)) {

        // unrounded multiply/insert
		// ?? If we've just ended a Record with a SUSInsert, when
		// the SUS goes up do we process it like another Insert and keep
		// the cycles or process it like an unrounded insert?
		// (currently unrounded)

		// !! TODO: Do quantization of ending functions like Record
		// that are not normally quantized.

        endEvent = em->newEvent(primaryFunction, endType, event->frame);
		endEvent->setInvokingFunction(function);
		endEvent->savePreset(mPreset);
		endEvent->quantized = event->quantized;
        em->addEvent(endEvent);

		// resume playback after the multiply/insert
		Event* jump = scheduleModeEndPlayJump(endEvent, true);

		// the trigger event is never relevant
		ignoreTrigger = true;

		Trace(this, 2, "Loop: Unrounded ending to %s\n", mMode->getDisplayName());
	}
	else {
		// rounded multiply/insert
		// Don't confuse "rounded" with "round mode".
		// A rounded insert always rounds up to a cycle boundary.
		// (should support other quantizations someday?)

		Event *recordStop = NULL;
        long endFrame = 0;
		// !! don't really need to schedule this if event is quantized
		if (!mPreset->isRoundingOverdub()) {
			// have to stop recording early
			endFrame = getUnroundedRecordStopFrame(event);
            // I HATE how you have to know the "family function" to create
            // a stop pevent, need to encapsulate this better.
			recordStop = em->newEvent(Record, RecordStopEvent, endFrame);
            // We have two possible invokers here, action->function
            // and primaryFunction.  Before the RecordFunction reorg
            // recordStop->function had primaryFunction and 
            // recordStop->invokingFunction had event->function.  Now
            // that recordStop->function has to be Record to get the
            // right event handler called, what do we put here?  Since
            // the original event handler (Loop::finishRecording) didn't
            // look at invokingFunction use primaryFunction.  I don't think
            // it matters anyway because alternate endings only apply
            // if we're in Record mode.
			recordStop->setInvokingFunction(primaryFunction);;
			recordStop->savePreset(mPreset);
			em->addEvent(recordStop);
		}

		// calculate the end of the Multiply, make sure this
		// goes in front of the alternate ending event
		endFrame = getModeEndFrame(event);

		if (recordStop != NULL && endFrame < recordStop->frame) {
			// must be a calculation error
			Trace(this, 1, "Loop: %s end frame less than record stop frame: %ld %ld\n",
				  mMode->getDisplayName(), endFrame, recordStop->frame);
			recordStop->frame = endFrame;
		}

		// For MultipyMode=Simple, we'll try to stop immediately, but 
		// not before the quantized record stop frame.	
		// Can this happen in other modes?  

		if (mMode == MultiplyMode && 
			recordStop != NULL && endFrame < recordStop->frame) {
			if (mPreset->getMultiplyMode() == Preset::MULTIPLY_SIMPLE) {
				// quantize the end of the multiply
				endFrame = recordStop->frame;
			}
			else {	
				// proabaly a calculation error?
				Trace(this, 1, "Loop: Multiply end frame less than record end frame: %ld %ld\n",
					  endFrame, recordStop->frame);
				recordStop->frame = endFrame;
			}
		}

		endEvent = em->newEvent(primaryFunction, endType, endFrame);
		endEvent->setInvokingFunction(function);
		endEvent->savePreset(mPreset);
		endEvent->addChild(recordStop);
		endEvent->quantized = true;
		em->addEvent(endEvent);

		if (recordStop != NULL)
		  Trace(this, 2, "Loop: Scheduled %s record stop at %ld\n",
				mMode->getDisplayName(), recordStop->frame);

		Trace(this, 2, "Loop: Scheduled %s mode end at %ld\n", 
			  mMode->getDisplayName(), endFrame);

		// jump playback to the beginning of what will eventually
		// be the new loop (remultiply), or the remainder after
		// the mode end (multiply, insert)
		Event* jump = scheduleModeEndPlayJump(endEvent, false);
		
		// If the function that ended the mode was the same as the one that
        // started it, don't reschedule it after the mode ending.
        // Note that function may be SUSInsert while primaryFunction
        // will be just Insert.  These are considered the same here.
        if ((mMode == InsertMode && function->eventType == InsertEvent) ||
            (mMode == MultiplyMode && function->eventType == MultiplyEvent)) {

            ignoreTrigger = true;
        }
		else {
			// must be an alternate ending, MultipyEndEvent may have
			// been pushed to an even cycle multiple
			if (endEvent->frame > event->frame) {
				Trace(this, 2, "Loop: Adjusting mode end trigger event (%s) from %ld to %ld\n",
					  event->getName(), event->frame, endEvent->frame);
				em->moveEventHierarchy(this, event, endEvent->frame);
			}
		}
	}

	// in a few odd cases, we'll be moving a previously scheduled hierarchy
	// (like a SUSReturn)
	bool triggerScheduled = em->isEventScheduled(event);
	if (ignoreTrigger) {
        if (endEvent != NULL) {
            // undoScheduledEvent will also reclaim the action so have
            // to reattach it first
            action->changeEvent(endEvent);
        }
        // remove, cancel side effects, and free
        em->removeEvent(event);
	}
	else if (triggerScheduled) {
		event->addChild(endEvent);
		em->reorderEvent(event);
	}
	else {
		event->addChild(endEvent);
		em->addEvent(event);
	}

    return endEvent;
}

/**
 * Return true if the function being used to end the multiply
 * will result in an unrounded multiply.
 */
PRIVATE bool Loop::isUnroundedEnding(Function* f)
{
	bool unrounded = false;

	if (mMode == InsertMode) {
		unrounded = (f == Record ||
					 f == AutoRecord ||
					 f == SUSUnroundedInsert);
	}
	else {
		unrounded = (f == Record || 
					 f == AutoRecord ||
					 f == SUSUnroundedMultiply);
	}

	return unrounded;
}

/**
 * When RoundingOverdub=Off, calculate the frame at which to stop recording.
 * Used when ending both Multiply and Insert modes.
 *
 * ?? It is unclear how the end is subject to quantization.
 * When ending with a quantized function, the record stop will
 * also be quantized.  But if the Record alternate ending
 * was used, it will not be quantized.  Text on 5-11 says that
 * when Quantize is on, it always quantizes to a cycle boundary, 
 * but this is probably a typo since Quantize=SubCycle also makes sense.
 * Quantize=Loop doesn't make sense.
 *
 * Assume that we quantize if the ending event is not already quantized.
 */
PRIVATE long Loop::getUnroundedRecordStopFrame(Event* e)
{
	long stopFrame = e->frame;

	if (!e->quantized) {
		Preset::QuantizeMode q = mPreset->getQuantize();
		if (q == Preset::QUANTIZE_SUBCYCLE) {
            EventManager* em = mTrack->getEventManager();
            stopFrame = em->getQuantizedFrame(this, stopFrame, q, false);
        }
    }

	return stopFrame;
}

/**
 * Calculate the ending frame of a Multiply/Insert.
 * If RoundingOverdub=Off, recording may end sooner than we end the mode.  
 *
 * The original behavior was based on a diagram in the manual that suggested
 * that Multiply/Insert always end on a cycle boundary RELATIVE TO THE
 * START OF THE MODE.  This resulted in behavior most people (including me)
 * hated which was that you would get an extra cycle added if you
 * stopped the multiply AFTER the original start point.  The original
 * start point is hard to remember so this was a common problem.
 *
 * Now, we always end the Multiply/Insert in the current cycle.
 *
 * Because I didn't like the original behavior I added 
 * MultiplyMode=Overdub which sort of works the desired way but
 * you lose the "remultiply" behavior.  Now that normal multiply
 * works better think about whether we need Overdub mode.
 * 
 * Unrounded multiply works as before, cut between the quantized or
 * unquantized record ending and the start of the mode.
 *
 * Remultiply is funny. We're supposed to maintain the cycle length
 * but if we don't extend relative to the start point the way people
 * hate there is no way to do this.
 *
 *   - loop with 2 cycles, cycle length 1000
 *   - start remultiply in cycle 2 frame 500
 *   - end remultiply in cycle 2 frame 800
 *
 * If we follow the "always end in the current cycle" rule then we have
 * to move the start of the remultiply backward to the beginning of the
 * cycle to retain the cycle length.
 * 
 * TODO: If the stop frame is within 100ms after a cycle boundary, the
 * EDP will round it down.  This helps prevent adding an unwanted
 * extra cycle if Quantize=Off and you're manually pressing buttons
 * near the end.
 * 
 * OLD NOTES
 *
 * Rather than ending on a cycle boundary relative to the start
 * of the multiply, we can simply end on the next cycle boundary.
 * I actually like this better because you have more control over
 * how many cycles you will end up with and don't have to worry
 * about where you started the multiply.
 * Actually, why not just end the multiply immediately?
 * It would end up behaving like Overdub with the ability
 * to add new cycles.  This to me seems most natural.
 * Added Preset::MULTIPLY_OVERDUB for this.
 *
 * UPDATE: In scripts it is common to call an ending function
 * on the same frame as the Insert or Multiply, the expectation
 * being that we insert/multiply exactly one cycle.  Originally
 * however, if endFrame and mModeStartFrame were the same we
 * would immediately end the mode.  I can see advantages for both
 * behaviors, an immediate end allows us to cancel an Insert after
 * a loop switch without actually inserting but there are other ways
 * to do that.
 *
 */
PRIVATE long Loop::getModeEndFrame(Event* event)
{
	// mModeStartFrame has the realtime starting frame (latency adjusted)
	long endFrame = event->frame;

	Preset::MultiplyMode mmode = mPreset->getMultiplyMode();

	if (mMode == MultiplyMode && mmode == Preset::MULTIPLY_SIMPLE) {
		// TODO: a mode that selects immediate end or quantize
		// to the next cycle?  Just end now
		//endFrame = getQuantizedFrame(endFrame, Preset::QUANTIZE_CYCLE, false);
	}
	else if (mMode == MultiplyMode) {
        // must be MULTIPLY_NORMAL
		// !! should this also apply to insert mode?

		// the first part of this is the same as for TRADITIONAL

		long multiplyLength = endFrame - mModeStartFrame;
		long cycleFrames = getCycleFrames();
		int newCycles = 0;

		// shouldn't happen, but be careful to avoid divide by zero
		if (cycleFrames > 0)
		  newCycles = multiplyLength / cycleFrames;

		if ((newCycles * cycleFrames) != multiplyLength) {
			// not exactly on it, but are we close?
			int spill = multiplyLength % cycleFrames;
			
			// TODO: This calculation isn't right, needs thought
			// if (spill > DEFAULT_EVENT_GRAVITY_FRAMES)

			if (spill > 0)
			  newCycles++;
		}
		int quantizedLength = newCycles * cycleFrames;
		endFrame = mModeStartFrame + quantizedLength;

		// in NEW mode, never add another cycle if we spilled over the
		// cycle boundary relative to mModeStartFrame
		int max = getFrames();
		if (endFrame > max) {
			long delta = endFrame - max;
			endFrame = max;

			// If we're re-multiplying back up the start frame so we get
			// an even cycle multiple.  Otherwise MultiplyEndEvent gets 
			// confused.  This is NOT how the EDP does it.  They will
			// continoue "rounding" to fill out a cycle relative
			// to mModeStartFrame then glue the ends together.  
			// Need to try that but we've got to get away from the
			// "insert full cycle at the loop boundary" approach.
			long newStart = mModeStartFrame - delta;
			if (newStart < 0) {
				Trace(this, 1, "Remultiply start adjustment error!\n");
				newStart = 0;
			}
			mModeStartFrame = newStart;
		}

		Trace(this, 2, "Loop: %s start=%ld, length=%ld, quantizedLength=%ld, newCycles=%ld\n",
			  ((mMode == InsertMode) ? "Insert" : "Multiply"),
			  mModeStartFrame, (long)multiplyLength, 
			  (long)quantizedLength, (long)newCycles);
	}
	else {
        // InsertMode
        // This was also the old "traditional" multiply behavior which might
        // also be wrong for Insert mode too?

		// If we're exactly on, or within a few hundred milliseconds after
		// the boundary frame, quantize down so we don't insert an 
		// unintended cycle.
		// TODO: This will only work if we're within InputLatency of the
		// boundary, if we actually go past the boundary we will process
		// an LoopEvent event and add another cycle. Technically, we have
		// to detect this and remove the cycle.

		long multiplyLength = endFrame - mModeStartFrame;
		long cycleFrames = getCycleFrames();
		int newCycles = 0;

		// if ending on the start frame, add one cycle, see notes above
		if (multiplyLength == 0)
		  multiplyLength = cycleFrames;

		// shouldn't happen, but be careful to avoid divide by zero
		if (cycleFrames > 0)
		  newCycles = multiplyLength / cycleFrames;

		if ((newCycles * cycleFrames) != multiplyLength) {
			// not exactly on it, but are we close?
			int spill = multiplyLength % cycleFrames;
			
			// TODO: This calculation isn't right, needs thought
			// if (spill > DEFAULT_EVENT_GRAVITY_FRAMES)

			if (spill > 0)
			  newCycles++;
		}
		int quantizedLength = newCycles * cycleFrames;
		
		endFrame = mModeStartFrame + quantizedLength;

		Trace(this, 2, "Loop: %s start=%ld, length=%ld, quantizedLength=%ld, newCycles=%ld\n",
			  ((mMode == InsertMode) ? "Insert" : "Multiply"),
			  mModeStartFrame, (long)multiplyLength, 
			  (long)quantizedLength, (long)newCycles);
	}

	return endFrame;
}

/**
 * After a multiply or insert ending has been scheduled, another
 * function can trigger an immediate unrounded ending.  Besides
 * modifying the frame of the mode end event, we also need to adjust
 * all the related alternate ending events.
 *
 * This can also be used with "multi increase" to move the end forward.
 *
 * NOTE: Technically we may need to process the undo's in a ceratin
 * order.  For example, ending Multiply with Mute will schedule both
 * a MutePlay and a JumpPlay event.  JumpPlay will restore mute status
 * so if done after the MutePlay undo may cancel it.  In practice this
 * shouldn't be an issue since you can't be in Mute when the JumpPlay
 * is scheduled, though there may be other sensitive combinations.
 *
 * UPDATE: Previous comment is obsolte, we no longer have MutePlay.
 * Delete when ready.
 */
PRIVATE void Loop::moveModeEnd(Event* endEvent, long newFrame)
{
    EventManager* em = mTrack->getEventManager();
	long delta = newFrame - endEvent->frame;

	// assume a shift of zero is still an unrounded operation
	if (delta <= 0)
	  Trace(this, 2, "Loop: Forcing unrounded %s at %ld\n", endEvent->getName(), newFrame);
	else 
	  Trace(this, 2, "Loop: Delaying %s till %ld\n", endEvent->getName(), newFrame);

	// if we have an end event, move the hierarchy from there
	Event* parent = endEvent->getParent();
	if (parent == NULL)
	  em->moveEventHierarchy(this, endEvent, newFrame);
	else {
		// this should always be on the same frame, but handle it
		long parentFrame = (parent->frame - parent->latencyLoss) + delta;
		em->moveEventHierarchy(this, parent, parentFrame);
	}
}

/****************************************************************************
 *                                                                          *
 *   						   PLAY TRANSITIONS                             *
 *                                                                          *
 ****************************************************************************/

/**
 * For Multiply/Insert, schedule a JumpPlayEvent to resume
 * playback buffering.
 *
 * Originally jumped into the record layer at an offset that would
 * place us after the multiply/insert to finish playing any remainder
 * content from the original layer.  This works, but requires that
 * we shift immediately after the mode ends.  If we performed
 * another operation in the layer it could confuse playback which 
 * would now be cursoring in front of the record cursor.
 *
 * For Insert we can just return to the play layer at the current
 * frame minus the total number of inserts that have been performed
 * in this layer.  If we reach the end we preplay the record layer as usual.
 *
 * For simple multiply we can do the same.
 *
 * For remultiply and unrounded multiply we have to jump
 * to the record layer at the mode start frame since the front of the
 * layer may be truncated.  Remultiply and unrounded multiply therefore
 * REQUIRE that a shift be performed when the mode ends.  
 *
 * Simple multiply does not necessarily need to do an immediate shift,
 * but that may be best since if you did another multiply in the layer
 * it would become a remultiply and you would lose the abillity to undo
 * to just the original multiply.  
 * 
 */
PUBLIC Event* Loop::scheduleModeEndPlayJump(Event* endEvent, bool unrounded)
{
    EventManager* em = mTrack->getEventManager();
	Event* jump = em->schedulePlayJump(this, endEvent);

	if (mMode == MultiplyMode) {
		// jump to the record layer since we'll always do a shift
		jump->fields.jump.nextLayer = mRecord;

		if (unrounded || 
			(mPreset->getMultiplyMode() == Preset::MULTIPLY_NORMAL &&
			 mPlay != NULL && mPlay->getCycles() > 1)) {
			// For both unrounded multiply and remultiply (more than one cycle)
			// we will trim off the content before and after the mode edges.
			// Return to the mode start frame.  If we're in Simple mode
			// mode we won't trim.
			jump->fields.jump.nextFrame = mModeStartFrame;
		}
		else {
			// Since we're playing the same initial cycle over and over,
			// we're resuming playback at logically the same place so
			// we can try to avoid a fade.  The record layer may however
			// have an overdub that isn't in the play layer so if there
			// was any latency loss, we'll be jumping to a place without
			// a nice fade in and have to do a runtime fade.
			jump->fields.jump.nextFrame = endEvent->frame;
			if (jump->latencyLoss == 0)
			  jump->fields.jump.nextShift = true;
		}
	}
	else if (!DeferInsertShift) {
		// handled like multiply, immediately jump to the record layer
		jump->fields.jump.nextLayer = mRecord;
		jump->fields.jump.nextFrame = endEvent->frame;
		if (unrounded) {
			// For unrounded insert, we won't prune the last 
			// inserted cycle until we reach the InsertEndEvent.  
			// So, play can't start at what will eventually be the mode
			// end frame, it has to be quantized to a cycle boundary.
			long inserted = getModeInsertedFrames(endEvent);
			jump->fields.jump.nextFrame = mModeStartFrame + inserted;
		}
	}
	else {
		// fresh new way, keep going in the play layer and allow
		// multiple inserts before the shift
		long inserted = mRecord->getFrames() - mPlay->getFrames();
		if (unrounded) {
			// we won't be inserting a full cycle
			long cycleFrames = mRecord->getCycleFrames();
			long lastInsert = endEvent->frame - mModeStartFrame;
			// get just the amount inserted in the last partial cycle
			lastInsert = lastInsert % cycleFrames;
			inserted -= (cycleFrames - lastInsert);
		}
		long nextFrame = endEvent->frame - inserted;
		if (nextFrame >= 0)
		  jump->fields.jump.nextFrame = nextFrame;
		else {
			// calculation error, shouldn't happen but if it does
			// handle like the non-deferred shift case
			Trace(this, 1, "Loop: Play jump after insert miscalculation!\n");
			jump->fields.jump.nextLayer = mRecord;
			jump->fields.jump.nextFrame = endEvent->frame;
			if (unrounded) {
				long inserted = getModeInsertedFrames(endEvent);
				jump->fields.jump.nextFrame = mModeStartFrame + inserted;
			}
		}
	}

	return jump;
}

/**
 * Given an event that defines the end of an insert/multiply operation,
 * determine how many cycle frames will have been inserted in the
 * record layer when we reach the end event.  This is not subject
 * to unrounded endings.
 */
PRIVATE long Loop::getModeInsertedFrames(Event* endEvent)
{
	long rawLength = endEvent->frame - mModeStartFrame;
	long cycleFrames = getCycleFrames();
	int newCycles = 0;

	if (cycleFrames > 0) {
		newCycles = rawLength / cycleFrames;
		if ((rawLength % cycleFrames) > 0)
		  newCycles++;
	}

	return newCycles * cycleFrames;
}

/**
 * JumpPlayEvent handler.
 * Scheduled for lots of functions to change the playback position
 * in advance of the record position so that when finally reach
 * the function event, the playback position will be correct and adjusted
 * for latency.
 *
 * This is also where the logic resides to handle the pitch, rate, 
 * and direction transfer modes when switching loops.
 * 
 * NOTE: The latency loss correction we perform here is necessary
 * if an event is scheduled on the same frame as a JumpPlayEvent that
 * is about to change the rate.  The next event will be scheduled
 * using the current latencies, not knowing that the JumpPlayEvent
 * will soon change them.  In practice, this only happens in scripts and
 * can be avoided if you add a "Wait last" after the rate function.
 * Ideally we could look for any pending jump events on the current
 * frame when scheduling every event and factor in the latency changes.
 * But this is complicated and error prone and only relevant for obscure
 * cases in scripts.  Leave the warning in because it sometimes catches
 * real scheduling errors.
 *
 * FUTURE: 
 *
 * This is a severe violation of Function's encapsulation but I wanted
 * to start with the play transition logic in one place to better see
 * understand it.  Now that things have settled down, this needs to be 
 * carefully factored back into the Function interface
 *
 * UPDATE:
 * We now allow play jumps during recording for Speed shifts.
 * I tried supressing the events but unfortunately they also adjust
 * mOutput's latency.  Rather than trying to duplicate the latency
 * adjustment else where, we'll schedule a jump event that goes nowhere.
 * 
 * 
 */
PUBLIC void Loop::jumpPlayEvent(Event* e)
{
	Layer* currentLayer = ((mPrePlay != NULL) ? mPrePlay : mPlay);
	Event* parent = e->getParent();
	Function* func = e->function;

	// calculate the amount of latency loss
	long latencyLoss = 0;
	if (parent == NULL) {
		// Stutter may schedule events with a pre determined
		// latency loss, would be nice if we could avoid the extra field
		// in the event!
		latencyLoss = e->latencyLoss;
	}
	else {
		long latency = mInput->latency + mOutput->latency;
		long idealFrame = parent->frame - latency;
		latencyLoss = e->frame - idealFrame;

		// consistency check for older code that tries to pre-calculate loss
		if (e->latencyLoss != 0 && e->latencyLoss != latencyLoss)
		  Trace(1, "Loop: JumpPlayEvent latencyLoss mismatch %ld %ld\n",
				e->latencyLoss, latencyLoss);

		if (func == NULL)
		  func = parent->function;
		else {
			// Not supposed to have functions assigned to a JumpPlayEvent
			// unless it has no parent
			Trace(this, 1, "Loop: JumpPlayEvent with non-null function!\n");
		}
	}

	// save previous state for undo
	// FUTURE: Every time we add a function that has an effect on the 
	// stream parameters we need to save/restore it for undo.  This kind of
	// sucks because it is hard to do this in an extensible way, it requires
	// hard coded fields in the Event and JumpChanges classes.  Example
	// future functions could be changing the level controls
	// adjusted for latency.  We could simplify this somewhat by adding the
	// notion of a "committed" event.  Once we reach the JumpPlay event
	// the parent is committed and cannot be undone.

	e->fields.jump.undoLayer = currentLayer;
	e->fields.jump.undoFrame = mPlayFrame;
	e->fields.jump.undoMute = mMute;
	e->fields.jump.undoReverse = mOutput->isReverse();
	e->fields.jump.undoSpeedToggle = mTrack->getSpeedToggle();
	e->fields.jump.undoSpeedOctave = mOutput->getSpeedOctave();
	e->fields.jump.undoSpeedStep = mOutput->getSpeedStep();
	e->fields.jump.undoSpeedBend = mOutput->getSpeedBend();
    e->fields.jump.undoTimeStretch = mOutput->getTimeStretch();
	e->fields.jump.undoPitchOctave = mOutput->getPitchOctave();
	e->fields.jump.undoPitchStep = mOutput->getPitchStep();
	e->fields.jump.undoPitchBend = mOutput->getPitchBend();

	// Initialize structure that will hold the next play state
	JumpContext next;
	memset(&next, 0, sizeof(next));
	next.layer = e->fields.jump.nextLayer;
	next.frame = e->fields.jump.nextFrame;
	next.latencyLossOverride = false;
	next.mute = false;
	next.unmute = false;
	next.muteForced = false;
	next.reverse = mOutput->isReverse();
    next.speedToggle = mTrack->getSpeedToggle();
    // always get speed state consistently from the input stream?  
    // hmm, they're supposed to match...
    next.speedOctave = mOutput->getSpeedOctave();
	next.speedStep = mOutput->getSpeedStep();
	next.speedBend = mOutput->getSpeedBend();
    next.timeStretch = mOutput->getTimeStretch();
    next.speedRestore = false;
    next.pitchOctave = mOutput->getPitchOctave();
	next.pitchStep = mOutput->getPitchStep();
	next.pitchBend = mOutput->getPitchBend();
    next.pitchRestore = false;
	next.inputLatency = mInput->latency;
	next.outputLatency = mOutput->latency;

	// Next layer is sometimes in the event, but for jumps within
	// same loop it may be NULL meaning to stay in the current layer.
	// If this is a switch jump, the next layer will usually be changed
	// when we get around to adjusting the jump.
	// currentLayer may be null for events processing during recording.
	
	if (next.layer == NULL) 
	  next.layer = currentLayer;

	// master function may force us out of mute
	// do this before event processing so we can stack a Mute
	// event that will keep us in mute
	// (only relevant for loop triggering?)
	// !! There sre some residual cases where the function here will
	// be the alternate ending, not the "family" function.
	// Insert/Record, Multiply/Record, others?
	// Might be a problem if Insert is not on the custom list??
	if (mMuteMode && func->isMuteCancel(mPreset))
	  next.unmute = true;

	// Determine the new playback parameters	
	// Have to do this in two passes, the first to determine the next playback
	// rate which affects a few latency calculations used for the others,
	// I think only Unmute.
	// !! rethink why we need this

	next.speedOnly = true;
	adjustJump(e, &next);

	// now get the eventual latencies
	next.inputLatency = mInput->getAdjustedLatency(next.speedOctave,
                                                   next.speedStep,
                                                   next.speedBend,
                                                   next.timeStretch);

	next.outputLatency = mOutput->getAdjustedLatency(next.speedOctave,
                                                     next.speedStep,
                                                     next.speedBend,
                                                     next.timeStretch);

	// do it all again with the correct latencies
	next.speedOnly = false;
	adjustJump(e, &next);

	// If we're jumping to what is logically the same content as we 
	// just played, avoid a fade.  This must be done before we do
	// fade tail computations below which may clear this flag.  
	mOutput->setLayerShift(e->fields.jump.nextShift);

	// Before updating the stream, need to capture a fade tail
	// in case there will be a break in the playback.
	// Currently only doing this if we change direction, but 
	// in theory rate and pitch changes should do cross fades too!!
	// !! think about always doing this and letting Stream figure
	// out if it is necessary.  

	// reversePlayEvent has some complex logic to try to avoid
	// the fade if there was no latency loss and we're not on
	// the loop boundary, not sure if that's really necessary
	if (mOutput->isReverse() != next.reverse) {
		//if (loss > 0 || (playFrame > 0 && playFrame < loopFrames - 1))
		mOutput->captureTail();
	}

	// update the stream
	mOutput->setSpeed(next.speedOctave, next.speedStep, next.speedBend);
	mOutput->setPitch(next.pitchOctave, next.pitchStep, next.pitchBend);
    mOutput->setTimeStretch(next.timeStretch);
	mOutput->setReverse(next.reverse);
    // no, wait for the input stream event
    //mTrack->setSpeedToggle(next.speedToggle);

	// From here on we need a a layer
	if (next.layer == NULL) {
		// this is allowed during recoding, otherwise it's an error
		if (getFrames() > 0)
		  Trace(this, 1, "Loop: Ignoring jumpPlayEvent with no layer!\n");
		return;
	}

	// !! when do we inform the Synchronizer about rate changes, 
	// now or when we reach the parent event?
	// we're waiting for the parent...

	// Adjust next play frame based on rate changes and latency loss.
	// We've already calculated latencyLoss, it is adjusted by the rate
	// to determine the adjustment to the current play frame.
	long layerFrames = next.layer->getFrames();

	long nextFrame = next.frame;
	if (nextFrame < 0) {
		// this is an indication to leave the play frame where it is
		// in this case latencyLoss is not factored in, just continue
		nextFrame = mPlayFrame;
	}
	else if (latencyLoss != 0) {
		// NOTE: This assumes that the input and output streams change
		// rate at the same time.  If the rates do not need to 
		// be symetrical, there's more work to do...

		// first descale loss from the current rate then adjust by new rate
        float oldSpeed = Resampler::getSpeed(e->fields.jump.undoSpeedOctave,
                                             e->fields.jump.undoSpeedStep,
                                             e->fields.jump.undoSpeedBend,
                                             e->fields.jump.undoTimeStretch);

		// sanity check to avoid divide by zero
		if (oldSpeed == 0.0) oldSpeed = 1.0;
		float loss = latencyLoss / oldSpeed;
		loss *= mOutput->getSpeed();
		latencyLoss = (long)ceil(loss);

		// jump adjustments may have calculated the new frame factoring
		// in latency loss so we don't need to add it again, but go ahead
		// and do the scaling for the trace messages
		if (!next.latencyLossOverride)
		  nextFrame += latencyLoss;
		
		// hmm, I tried to do a sanity check here comparing nextFrame
		// relative to parent->frame + latencies but that doesn't work
		// if mFrame is going to change after the event too, no way 
		// to know where mFrame will be here?
	}
	
	if (layerFrames == 0) {
		// next layer is empty, doesn't matter where we though we should go
		// this is probably a scheduling errror
		if (nextFrame != 0)
		  Trace(this, 1, "Loop: Attempted jump into an empty layer %ld\n", 
                (long)nextFrame);
		nextFrame = 0;
	}
	else if (nextFrame < 0) {
		// In rare cases it may be too small, but this is probably an error
		// with scheduling when there have been rate changes?
		Trace(this, 1, "Loop: Negative jump frame after latency compensation %ld\n",
			  nextFrame);
		while (nextFrame < 0)
		  nextFrame += layerFrames;
	}
	else {
		// Wrap if too large, this can happen when jumping from a longer 
		// loop to a shorter one.  Also if we're jumping into the MuteLayer
		// after EmptyLoopAction=copyTiming.
		// In Multiply mode, we can have a play jump near the
		// end with the mode end event after the loop point.
		// Normally, we don't extend the loop until we see the
		// end and generate a LoopEvent.  But here, the jump needs
		// to go into the extension.  We have to do the extension 
		// now rather than waiting for LoopEvent to avoid wrapping
		// back to the beginning of the loop.  This only happens
		// when the jump and main event straddle the loop boundary.
		// Could try to extend early so we don't hit this condition
		// but I don't want to mess up the logic too much right now.

		if (nextFrame >= layerFrames) {

			if (parent != NULL && 
				(parent->frame > layerFrames ||
				 (parent->frame == layerFrames && parent->afterLoop))) {

				// jump and parent straddle the loop boundary
				// FUTURE: Try to encapsulate this in the Function

				if (mMode == MultiplyMode) {
					Trace(this, 2, "Loop: Adding cycle for play jump near end\n");
					mRecord->multiplyCycle(mInput, mPlay, mModeStartFrame);
					layerFrames = mRecord->getFrames();
					Trace(this, 2, "Loop: New cycles %ld loop frames %ld\n", 
						  (long)mRecord->getCycles(), layerFrames);
				}
				else if (mMode == InsertMode) {
					// possibly similar issues here, but if the insert is
					// continuing, we'll either be staying muted or unrounded
					// processing will have extended the loop by now?
					Trace(this, 1, "Loop: Possible jump error near loop boundary in Insert mode!\n");
				}
				else if (mMode == StutterMode) {
					stutterCycle();
					layerFrames = mRecord->getFrames();
				}
			}
			
			// now wrap if we didn't extend
			while (nextFrame >= layerFrames)
			  nextFrame -= layerFrames;
		}
	}

	// it is important that we remember what we actually did here so 
	// we can calculate the advance for undo
	e->fields.jump.nextLayer = next.layer;
	e->fields.jump.nextFrame = nextFrame;

	// set target frame and layer
	long oldPlayFrame = mPlayFrame;
	mPlayFrame = nextFrame;

	// If we're skipping a small number of frames in the
	// same layer, it is probably rate shift rouding error and we
	// should defer the fade by setting either LayerShift or 
	// LastPlayFrame in the output stream.

	long lastPlayFrame = mOutput->getLastFrame();
	if (mOutput->getLastLayer() == next.layer && lastPlayFrame != mPlayFrame) {
		long delta = mPlayFrame - lastPlayFrame;
		if (delta < 0) delta = -delta;
		if (delta <= MAX_ROUNDOFF_DRIFT) {
			// close enough, don't cause a fade
			mOutput->setLastFrame(mPlayFrame);
		}
	}

	// Use preplay so we maintain the mRecord/mPlay relationshp.  But if
	// we're jumping to a different location within mPlay do not set mPrePlay
	// or else we won't fall into mRecord when we hit the loop end.
	if (next.layer != mPlay)
	  mPrePlay = next.layer;
	else
	  mPrePlay = NULL;

	// update mute
	if (next.mute) {
		// something is explicitily putting us there
		Trace(this, 2, "Loop: Jump forcing mute on\n");
		mMute = true;
	}
	else if (next.unmute) {
		// something is explicitly taking us out of mute
		Trace(this, 2, "Loop: Jump forcing mute off\n");
		mMute = false;
	}

	// update pause
	// do jumps always take us out of pause?  It seems that by definition
	// if we're advancing and processing events we must not be in pause
	if (mPause) {
		Trace(this, 1, "Loop: JumpPlayEvent during Pause mode!\n");
		mPause = false;
	}

	Layer* traceLayer = ((mPrePlay != NULL) ? mPrePlay : mPlay);

    if (!e->silent) {
        if (traceLayer == getMuteLayer())
          Trace(this, 2, "Loop: Playback jumping from %ld to frame %ld of MuteLayer latency loss %ld\n", 
                oldPlayFrame, mPlayFrame, (long)latencyLoss);
        else
          Trace(this, 2, "Loop: Playback jumping from %ld to frame %ld of layer %ld latency loss %ld\n", 
                oldPlayFrame, mPlayFrame, (long)traceLayer->getNumber(), (long)latencyLoss);
    }
}

/**
 * Change play parameters according to the event that owns
 * the JumpPlay.   When called for the first time, event will be a
 * JumpPlayEvent.
 * 
 * If the jump is owned by a SwitchEvent, then we will recursively call
 * this for each event stacked on the SwitchEvent.  In this case the
 * eventType will not be JumpPlayEvent, and we use it directly.
 *
 * NOTE: This obviously cries out for a polymorphic method on Function,
 * but it is being evolved from several directions, and I first wanted
 * to just get all the rules in one place.  When things settle down, think
 * about a way to move all the "family" specific stuff into the Function.
 */
PRIVATE void Loop::adjustJump(Event* event, JumpContext* next)
{
	Event* primary = event;
	Event* parent = event->getParent();
	bool switchStack = false;

	// Find the primary event
	if (event->type == JumpPlayEvent) {
		if (parent != NULL)
		  primary = parent;
	}
	else {
		// we must be stacked on a switch
		// !! TODO: should generalize stacking, it seems reasonable to 
		// allow this anywhere, not just on a switch.  Quantize could
		// be handled this way, right now we're scheduling multiple jumps.
		switchStack = true;
		if (parent == NULL || parent->type != SwitchEvent)
		  Trace(this, 1, "Loop: Odd jump event parentage!\n");
	}
	
	Function* function = primary->function;
	if (function == NULL) {
		Trace(this, 1, "Loop: Event with no function!\n");
		return;
	}

 	// To make selection easier, get the cannonical event for this 
	// function family.  
	// !! It would less confusing if we didn't have the EventType
	// in two places, the Event and the Function.  See sanity check below
	// for the special cases.

	EventType* family = primary->type;

	if (family == JumpPlayEvent) {
		// A standalone jump, used only in special cases like Stutter,
		// the Function on the jump event defines the family.
		family = function->eventType;
	}
	else if (family == InvokeEvent) {
		// a deferred invoke event, look to the function 
		family = function->eventType;
	}

	// Event/Function types are usually the same except for these older
	// alternate endings.  
	// !! Fix this so we don't have to store the EventType in the Event?
	// Will need the introduction of some pseudo-functions like Jump
	// InsertEnd, MultiplyEnd, Return, or else just a flag to ingnore them?
	if (family != function->eventType &&
		family != InsertEndEvent && 
		family != MultiplyEndEvent &&
		family != ReturnEvent) {

		Trace(this, 1, "Loop: Inconsistent function/event family %s!\n",
			  family->name);
	}

	// now we begin

	if (family == SwitchEvent) {
		// this is more complicated since we may have severl stacked events
		// this may call back to this method several times
		// TODO: To generalize this we'd need a way for the EventType
		// or Function  to say if it supports stacked events.
		if (switchStack) {
			// can't have recursive switches??
			Trace(1, "Loop: Stacked switch event!\n");
		}
		else
		  adjustSwitchJump(event, next);
	}
	else if (next->speedOnly) {
        if (family == SpeedEvent) {
            function->prepareJump(this, event, next);
        }
	}
	else if (family == SpeedEvent) {
		// this is the second pass where speedOnly=false, ignore
		// them here, but need this clause so we don't emit
		// a scary trace message below
	}
	else if (family == ReturnEvent) {
		// Sort of like a greatly simplified loop switch
        // since this requires type specfic field setup, make sure
        // we're here only for the JumpPlayEvent  this should not
        // happen for the events stacked under a SwitchEvent.
        if (event->type == JumpPlayEvent) {
            next->layer = event->fields.jump.nextLayer;
            next->frame = event->fields.jump.nextFrame;
        }
        else {
            Trace(this, 1, "Loop: Found ReturnEvent under a Switch!!\n");
        }
	}
	else {
		// defer to the Function
		function->prepareJump(this, event, next);
	}

}

/**
 * When we reach the JumpPlayEvent for a SwitchEvent we have a lot of work.
 * Because several function events may be "stacked" for evaluation immediately
 * after the switch, we have to look at all of them to determine what
 * to play.  Transfer modes have already been processed when the SwitchEvent
 * was scheduled and converted into stacked events so we do not need
 * to look at them here.
 *
 * If the speedOnly flag is on, then we're only interested in rate changes
 * this pass, but it's easier just to go through all the work twice.
 */
PRIVATE void Loop::adjustSwitchJump(Event* jump, JumpContext* next)
{
	Event* switche = jump->getParent();
	Loop* nextLoop = switche->fields.loopSwitch.nextLoop;

	SwitchContext actions;
	memset(&actions, 0, sizeof(actions));

	if (nextLoop == NULL) {
		Trace(1, "Loop: Invalid switch play jump!\n");
		return;
	}

	// !! technically, we can have a zero length loop that is not 
	// in Reset?  If so is it more accurate to check for ResetMode?
	bool nextEmpty = (nextLoop->getFrames() == 0);
	bool srcEmpty = (this->getFrames() == 0);

	// Prepare copy default modes based on the preset
	if (nextEmpty) {
        Preset::EmptyLoopAction action = mPreset->getEmptyLoopAction();
		if (srcEmpty) {
            // if the source loop is empty, then the copy modes don't
            // make sense?
            // ?? I guess we could let all of them do a Record?
			actions.record = (action == Preset::EMPTY_LOOP_RECORD);
		}
		else {
			if (action == Preset::EMPTY_LOOP_COPY)
			  actions.loopCopy = true;
			else if (action == Preset::EMPTY_LOOP_TIMING)
			  actions.timeCopy = true;
			else if (action == Preset::EMPTY_LOOP_RECORD)
			  actions.record = true;
		}
	}

 	// First check transfer modes if we're not restarting
 	if (nextLoop != this) {
 		Preset::TransferMode tm = mPreset->getSpeedTransfer();
 		if (tm == Preset::XFER_OFF) {
            next->speedToggle = 0;
            next->speedOctave = 0;
 			next->speedStep = 0;
            next->speedBend = 0;
            next->timeStretch = 0;
 		}
 		else if (tm == Preset::XFER_RESTORE) {
            next->speedToggle = nextLoop->mRestoreState.speedToggle;
            next->speedOctave = nextLoop->mRestoreState.speedOctave;
            next->speedStep = nextLoop->mRestoreState.speedStep;
            next->speedBend = nextLoop->mRestoreState.speedBend;
            next->timeStretch = nextLoop->mRestoreState.timeStretch;
            next->speedRestore = true;
 		}

 		tm = mPreset->getPitchTransfer();
 		if (tm == Preset::XFER_OFF) {
            next->pitchOctave = 0;
            next->pitchStep = 0;
            next->pitchBend = 0;
 		}
 		else if (tm == Preset::XFER_RESTORE) {
            next->pitchOctave = nextLoop->mRestoreState.pitchOctave;
            next->pitchStep = nextLoop->mRestoreState.pitchStep;
            next->pitchBend = nextLoop->mRestoreState.pitchBend;
            next->pitchRestore = true;
 		}

 		tm = mPreset->getReverseTransfer();
 		if (tm == Preset::XFER_OFF)
 		  next->reverse = false;
 		else if (tm == Preset::XFER_RESTORE)
 		  next->reverse = nextLoop->mRestoreState.reverse;
 	}

	// Now check stacked events
	// The "minor mode" events can adjust record/play settings like pitch,
	// rate and reverse, there may be several of those.  There should
	// only be one major mode event that determines what we will do
	// to the next loop before playing it.
	// It shouldn't happen if we're mutexing the events properly, but be 	
	// careful here about mutexing the copy/record flags just in case.
	// !! We need some encapsulation of the switch logic in the Function.

	for (Event* te = switche->getChildren() ; te != NULL ; te = te->getSibling()) {
		EventType* type = te->type;

		if (te == jump) {
			// the play jump is also a child ignore it
		}
		else if (type == InvokeEvent) {
			// this is the new way I'd like to start handling all switch
			// adjustments
			Function* f = te->function;
			if (f == NULL)
			  Trace(this, 1, "Loop: stack switch event with no function!\n");
			else 
			  f->prepareSwitch(this, te, &actions, next);
		}
		else if (type == JumpPlayEvent) {
			// hmm, not expecting any other jumps besides "jump"
			Trace(this, 1, "Loop: Unexpected stacked jump play event!\n");
		}
		else if (type == RecordEvent) {
			actions.loopCopy = false;
			actions.timeCopy = false;
			actions.record = true;
			actions.mute = false;
		}
		else if (type == MultiplyEvent) {
			// Multiply forces loop copy
			actions.loopCopy = true;
			actions.timeCopy = false;
			actions.record = false;
			actions.mute = false;
		}
		else if (type == StutterEvent) {
			// single cycle copy
			actions.loopCopy = true;
			actions.timeCopy = false;

			// !! needs more work
			//actions.singleCycle = true;

			actions.record = false;
			actions.mute = false;
		}
		else if (type == InsertEvent) {
			// time copy
			actions.loopCopy = false;
			actions.timeCopy = true;
			actions.record = false;
			actions.mute = false;
		}
		else if (type == OverdubEvent) {
			// Overdub does loopCopy if the loop is empty (simple copy)
			// !! how should this mutex with Multiply and Insert?
			if (nextEmpty) {
				actions.loopCopy = true;
				actions.timeCopy = false;
				actions.record = false;
                actions.mute = false;
			}
		}
		else if (type == ReplaceEvent) {
			// immediately enter replace, treat this like a record
			// had been doing this, does the EDP define it?
			if (nextEmpty) {
				actions.loopCopy = false;
				actions.timeCopy = false;
				actions.record = true;
                actions.mute = false;
			}
		}
		else if (type == MuteEvent) {
			// ignore if the next loop isn't empty
			actions.loopCopy = false;
			actions.timeCopy = false;
			actions.record = false;

			// Complicated due to MuteCancel
			// if we're not currently muted, mute
			// if we're muted, unmute if the trigger does not already cancel the mute
			// if we're muted, mute stay if the trigger canceled the mute

			actions.mute = true;
		}
		else { 
			// assume this is one of the minor modes events and accumulate
			// their changes
			adjustJump(te, next);
		}
	}

    // EDPISM: Ending a Record with a Trigger when AutoRecord=On
	// always resets the next loop and begins recording.
    // We control this with the RecordTransfer parameter
	// Normally we can't have stacked events here because the record end
	// isn't quantized, but could if we're synchronizing?  If we have
	// stacked events, ignore this.

	if (!actions.loopCopy && !actions.timeCopy && !actions.mute && 
		mMode == RecordMode && 
        mPreset->getRecordTransfer() == Preset::XFER_FOLLOW) {

		actions.record = true;
	}

	// Determine the target layer, this will have been initialized to the
	// current play layer if left null, so make sure it starts null here

	next->layer = NULL;
	next->mute = actions.mute;

	if (actions.record || actions.timeCopy) {
		// have to be silent, but we're not technically in mute mode
		next->layer = getMuteLayer();
	}
	else if (actions.loopCopy)
	  next->layer = mRecord;
	else
	  next->layer = nextLoop->getPlayLayer();

	// if next loop is empty, still need a non-null layer to stay in mute
	if (next->layer == NULL)
	  next->layer = getMuteLayer();

	// determine the target frame
	next->frame = 0;

	if (nextLoop == this) {
		// assume this always means restart?
	}
	else {
		long nextFrames = nextLoop->getFrames();

		// most things will start the loop over from zero
		long nextFrame = 0;

		Preset::SwitchLocation location = mPreset->getSwitchLocation();

		if (switche->function == RestartOnce) {
			// doesn't matter what SwitchStyle is, always start
			// from zero
		}
		else if (!actions.record) {
			// if re-recording always start from zero, otherwise
            // obey switchLocation

            if (actions.loopCopy || actions.timeCopy) {

                Preset::CopyMode cmode;
                if (actions.loopCopy)
                  cmode = mPreset->getSoundCopyMode();
                else
                  cmode = mPreset->getTimeCopyMode();

                if (cmode == Preset::COPY_INSERT || 
                    cmode == Preset::COPY_MULTIPLY) {

                    // !! if we're doing a SoundCopy or TimeCopy into Insert or
                    // Multiply mode, ignore switchLocation and start at zero
                    // or else Multiply/Insert will get screwed up and add an
                    // extra cycle.  This is because mModeStartFrame will be
                    // at the location of the switch but in these cases it
                    // proabbly should be set at zero as if we had been doing
                    // the Multiply/Insert from the beginning.  I REALLY
                    // don't like this for Mutliply mode!
                    location = Preset::SWITCH_START;
                }
                else if (location == Preset::SWITCH_RESTORE) {
                    // since the loop is being replaced, restore doesn't
                    // really make sense, prior to 1.43 this was converted
                    // to SAMPER_STYLE_RUN which is now SWITCH_FOLLOW.
                    // I suppose if the next loop isn't empty we could
                    // try to restore the old location but I've never
                    // liked SAMPLER_STYLE_RUN anyway.
                    location = Preset::SWITCH_FOLLOW;
                }
            }

            if (location == Preset::SWITCH_START) {
                // leave zero
            }
            else if (location == Preset::SWITCH_RANDOM) {
                // RANDOM_SUBCYCLE would be more useful?
                if (nextFrames > 0)
                  nextFrame = Random(0, nextFrames - 1);
            }
            else if (location == Preset::SWITCH_RESTORE) {
				// restore previous position
				if (nextFrames > 0) {
					// We're restoring the playback position relative to the
					// last record position.  When we reach our SwitchEvent,
					// mPlayFrame needs to be ahead of the saved record frame,
					// so we have to start it ahead of the restored frame.
					// This means that playback won't return exactly to the 
					// same location, it will skip back a little.

					// UPDATE: !! formerly had a different calculation here
					// that required we compute the latencies that would be 
					// in effect after the switch, since we no longer need
					// these here, can we get rid of them?

					nextFrame = nextLoop->mRestoreState.frame;
				}
			}
			else if (location == Preset::SWITCH_FOLLOW) {
				// Maintain the same relative position but may need to wrap
				// if the target is smaller than we are.

				if (isEmpty()) {
					// TODO: If we have been in an empty loop, but are 
					// using Sync, we could calculate the amount of "advance" 
					// from the  external loop pulses!!
					// This would feel more natural?  
					// For now, treat this like SWITCH_RESTORE
                    // !! it feels better to leave this zero?
                    if (nextFrames > 0)
                      nextFrame = nextLoop->mRestoreState.frame;
				}
				else {
					long maxFrames = nextFrames;

					if (actions.timeCopy || actions.loopCopy) {
						if (actions.singleCycle)
						  maxFrames = getCycleFrames();
						else
						  maxFrames = getFrames();
					}

					if (maxFrames != 0) {
						nextFrame = switche->frame;
						while (nextFrame >= maxFrames)
						  nextFrame -= maxFrames;
					}
				}
			}
		}

		next->frame = nextFrame;
	}

	// Note that we also must remember this in the parent SwitchEvent so
	// that we can pick up recording at the right spot during
	// switchEvent() processing
	switche->fields.loopSwitch.nextFrame = next->frame;

	// Switch jumps are odd since we don't calculate these two until
	// the very end.  Shouldn't be necessary to leave the result in the
	// event but keep it consistent.
	jump->fields.jump.nextFrame = next->frame;
	jump->fields.jump.nextLayer = next->layer;
}

/**
 * Undo the effect of a previous play jump.
 */
PUBLIC void Loop::jumpPlayEventUndo(Event* e)
{
	// If we were preplaying the record layer return to that, otherwise
	// just stay in play
	Layer* undoLayer = e->fields.jump.undoLayer;
	mPrePlay = undoLayer;
	if (mPrePlay == mPlay)
	  mPrePlay = NULL;

	// restore playback options, but not rate yet
	mMute = e->fields.jump.undoMute;
	mOutput->setReverse(e->fields.jump.undoReverse);

	mOutput->setPitch(e->fields.jump.undoPitchOctave, 
                      e->fields.jump.undoPitchStep,
                      e->fields.jump.undoPitchBend);

	// restoring the frame is complicated because we've been advancing,
	// and the advance rate may not be the same as the current rate
	Layer* prevLayer = e->fields.jump.nextLayer;
	float prevSpeed = mOutput->getSpeed();
	long undoFrame = e->fields.jump.undoFrame;
	long advance = 0;

	if (prevLayer == NULL)
	  Trace(this, 1, "Loop: Attempt to undo a jump without a layer!\n");
	else {
		// if we jumped into MuteLayer or a smaller layer (can that happen?)
		// we may have had to wrap the playframe, redo that calculation
		long maxFrames = prevLayer->getFrames();
		long startFrame = e->fields.jump.nextFrame;
		while (startFrame >= maxFrames)
		  startFrame -= maxFrames;

		if (mPlayFrame > startFrame)
		  advance = mPlayFrame - startFrame;
		else {
			// must have wrapped, in theory it could have wrapped several
			// times and we don't know how many, but only if loops can
			// be sorter than the combined latencies
			// !! this could actually be an issue for rate adjusted latencies
			// we only restrict loop length based on normal latencies
			advance = mPlayFrame + (maxFrames - startFrame);
		}
	}

	// now restore the rate parameters
	mOutput->setSpeed(e->fields.jump.undoSpeedOctave,
                      e->fields.jump.undoSpeedStep,
                      e->fields.jump.undoSpeedBend);

    mOutput->setTimeStretch(e->fields.jump.undoTimeStretch);

	if (prevSpeed != mOutput->getSpeed()) {
		// the advance needs to be adjusted for the rate change

		// sanity check to avoid divide by zero
		if (prevSpeed == 0.0) prevSpeed = 1.0;
		
		// adjust relative to the normal rate
		advance = (long)(advance / prevSpeed);

		// then for the restored rate
		advance = (long)(advance * mOutput->getSpeed());
	}

	// undoLayer will be NULL if comming from a Reset loop
	if (undoLayer == NULL)
	  mPlayFrame = 0;
	else {
		mPlayFrame = e->fields.jump.undoFrame + advance;

		// if we were multiplying, the new relative frame may be larger
		// than the restored layer, have to round down
		mPlayFrame = wrapFrame(mPlayFrame, undoLayer->getFrames());
	}

	Trace(this, 2, "Loop: Undo %s base %ld advance %ld mPlayFrame %ld\n",
		  e->type->name, e->fields.jump.undoFrame, advance, mPlayFrame);
}

/****************************************************************************
 *                                                                          *
 *                                  MULTIPLY                                *
 *                                                                          *
 ****************************************************************************/

/**
 * For operations like Insert and Multiply, we may have prematurely
 * started playback of the record loop, but now we need to return
 * to the original playback loop.
 */
PUBLIC void Loop::cancelPrePlay()
{
	if (mPrePlay != NULL) {
		// must also set this to avoid a fade
        // !! figure out how to push this into Stream?
		if (mOutput->getLastLayer() == mPrePlay)
		  mOutput->setLayerShift(true);
		mPrePlay = NULL;
	}
}


/****************************************************************************
 *                                                                          *
 *   								INSERT                                  *
 *                                                                          *
 ****************************************************************************/

/**
 * InsertEvent event handler.
 * This is called from the InsertEvent and RecordEvent handlers.
 * !! Need to move this to InsertFunction.  Why does Record need this?
 *
 * If there is no audio in this loop, then we must be here
 * via EmptyLoopAction=copyTiming.  In that case start the insert layer cycle
 * counter at zero since we start by logically filling the empty first cycle.
 * Otherwise start it at 1 to represent the insertion of a new cycle.
 */
PRIVATE void Loop::insertEvent(Event* e)
{
	if (mMode == RehearseMode) {
		// just cancel rehearse and stay in the last loop
		// !! I don't think this is exactly what the EDP does
		cancelRehearse(e);
	}
	else {
		if (mRecording)
          finishRecording(e);

        cancelPrePlay();
		checkMuteCancel(e);

        mModeStartFrame = mFrame;
		mRecord->startInsert(mInput, mFrame);

        mRecording = true;
		mMute = true;
        setMode(InsertMode);
		
		// Subtlety:  If the insert happens near the beginning of the loop,
		// we may have already preplayed some of it.  The audio that was
		// being preplayed as now been shifted right one cycle, so when
		// OutputStream gets around to capturing the fade tail it won't
		// be there.  We could force a tail capture now, but its easier
		// to doctor the location of the tail.
		if (mOutput->getLastLayer() == mRecord && 
			mOutput->getLastFrame() >= mFrame) 
		  mOutput->adjustLastFrame(getCycleFrames());
	}
}

/**
 * Called by event handlers to cancel Rehearse mode.
 * If we're recording, stop.  If we're playing the record layer
 * won't have a copy of the play layer so we have to do the 
 * copy now.
 * 
 * !! Wouldn't it have been better to let the record layer have
 * a copy of the play layer like we do in a normal shift(), then 
 * clear it if we decide to stay in rehearse?
 */
PUBLIC void Loop::cancelRehearse(Event* event)
{
	if (mMode == RehearseMode) {

        if (mRecording) {
            // just end the recording, and begin playing
            Trace(this, 3, "Loop: Exiting rehearse after finishing record layer\n");
			mMute = false;
            finishRecording(event);
        }
        else {
            Trace(this, 3, "Loop: Exiting rehearse mode with current play layer\n");

            // the record layer was empty, now that we're back to normal,
            // need to make it have a copy of the play layer
            mRecord->copy(mPlay);

            // immediately enter play mode to resume output buffering,  
            // overdub may come back on
            resumePlay();
        }
    }
}


/****************************************************************************
 *                                                                          *
 *                                  STUTTER                                 *
 *                                                                          *
 ****************************************************************************/

/**
 * Called in three places:
 *  1) when we enter Stutter mode
 *  2) when crossing a cycle boundary in Stutter mode
 *  3) when scheduling the end of Stutter mode
 *
 * In the first 2 cases we jump back to the beginning of the
 * stuttered cycle.  In the third case we jump to the original cycle
 * that followed the stuttered cycle.
 *
 * This is an unusual case of a JumpPlayEvent that has no parent.
 * Since this is important, should make it visible as a reminder where
 * the stutter point will be?
 */
PRIVATE Event* Loop::scheduleStutterTransition(bool ending)
{
	Event* trans = NULL;
	int inputLatency, outputLatency;

    // the jump is processed near the end of the current record cycle
    long cycleFrames = mPlay->getCycleFrames();

	// this really can't happen but avoid divide by zero at all costs
	if (cycleFrames == 0) return NULL;

    long recCycleStart = (mFrame / cycleFrames) * cycleFrames;
    long recCycleEnd = recCycleStart + cycleFrames;

	// If we're ending and we are exactly on a cycle boundary now, the
	// the stutter will end and the transition has to happen now
	// rather than on the next cycle
	if (ending && ((mFrame % cycleFrames) == 0))
	  recCycleEnd = mFrame;

    EventManager* em = mTrack->getEventManager();
	em->getEffectiveLatencies(this, NULL, recCycleEnd, 
                              &inputLatency, &outputLatency);

	long transitionFrame = recCycleEnd - inputLatency - outputLatency;
	long latencyLoss = 0;

	if (transitionFrame < mFrame) {
        // this should only happen if we're initiating the stutter
        // close to the end of the cycle, during stutter it could
        // only happen if the cycle size is too small
		latencyLoss = mFrame - transitionFrame;
		transitionFrame = mFrame;
	}

	// Since we have no parent, this is the only case where we
	// must save the function and latencyLoss in the Event.
	trans = em->newEvent(JumpPlayEvent, transitionFrame);
    trans->function = Stutter;
	trans->latencyLoss = latencyLoss;
	trans->fields.jump.nextLayer = mPlay;

	long jumpFrame = mModeStartFrame;
	if (ending) {
		jumpFrame += cycleFrames;
		// note that we have to warp within the play layer, 
		// so can't use warpFrame
		long frames = mPlay->getFrames();
		while (jumpFrame >= frames)
		  jumpFrame -= frames;
	}
	trans->fields.jump.nextFrame = jumpFrame;

	em->addEvent(trans);

	return trans;
}

/**
 * Called by cycleEvent when we cross a cycle boundary in Stutter mode.
 *
 * This needs to be a MobiusMode and Function entry point!!
 */
PUBLIC void Loop::stutterCycle()
{
    // insert a cycle into the record layer
    mRecord->stutterCycle(mInput, mPlay, mModeStartFrame, mFrame);

    // schedule another play jump back to the start of the stuttered cycle
    scheduleStutterTransition(false);
}

/****************************************************************************
 *                                                                          *
 *                                    UNDO                                  *
 *                                                                          *
 ****************************************************************************/

/**
 * UndoEvent event handler.
 * Be careful when undoing from a multiplied or inserted loop
 * back to a shorter one.
 */
PUBLIC void Loop::undoEvent(Event* e)
{
	Layer* restore = NULL;
	Layer* undo = NULL;
	bool initialRecording = false;

	// If we're auto-recording and have multipled, remove multiples
    // TODO: Need to encapsulate this in MobiusMode so we don't
    // have Function specific calls like this...
	if (Record->undoModeStop(this))
	  return;

	// If we're in a loop entered with SwitchStyle=OnceReturn
	// and there is a return Transtion to the previous loop, Undo
	// cancels the transition, can we stop now?
    EventManager* em = mTrack->getEventManager();
    if (em->cancelReturn())
      return;

    // next try to undo an event, it doesn't matter what it does
    // this method returns true if we undid something
    if (em->undoLastEvent())
      return;

	// Doing this here means that we cancel mute when undoing layers,
	// but not when undoing events, ok?
	checkMuteCancel(e);

	if (mPlay == NULL) {
		// must be an initial recording
		restore = mRecord->getPrev();
		initialRecording = true;
	}
	else {
		// must capture a fade tail now because the current layer may
		// be pooled and reset before the stream can detect the need for a fade
		mOutput->captureTail();

		// If we have been modifying the last layer, throw away the
		// changes but stay on that layer.  The original logic was this
		// if (mRecording && mMode != OverdubMode) {
		// which gets us out of Multiply/Insert/Stutter/Replace/Rehearse
		// but not overdubs or replaces that were finished before the shift.
		// This method will get us strcuture and recording changes, but
		// not feedabck changes.
		// If overdub is left on, tis may not be what we want, we'll get
		// stuck in the current layer if the overdub exceeds the noise floor!
		if (isLayerChanged(mRecord, true)) {
			// Toss what we just did and resume playing the previous layer
			restore = mRecord->getPrev();
		}
		else {
			// delete what we're curently playing
			undo = mPlay;
			Layer* undoTail = undo;
			if (undo->isCheckpoint())
			  undoTail = undo->getCheckpointTail();
			
			restore = undoTail->getPrev();
			undoTail->setPrev(NULL);
		}
	}

	// If we end up undoing a layer, can flush any remaining  events.
	// This is important if we happen to get the undo in before the
	// InputLatency period of a non-quantized  event
	em->flushEventsExceptScripts();
	resumePlay();

	if (restore != NULL) {

		if (undo != NULL)
		  Trace(this, 3, "Loop: Restoring play layer %ld, freeing layer %ld, resetting record layer %ld\n",
				(long)restore->getNumber(), (long)undo->getNumber(), (long)mRecord->getNumber());
		else
		  Trace(this, 3, "Loop: Restoring play layer %ld, resetting record layer %ld\n",
				(long)restore->getNumber(), (long)mRecord->getNumber());

		mPlay = restore;
        mPrePlay = NULL;

		// may have deferred the fade if there was a recording that
		// crossed the loop boundary, this method will fix it
		mPlay->restore(true);
        
        // if this had been a windowing layer, make sure that's off
        mPlay->setWindowOffset(-1);

		// if we've just been playing or squelching, don't need to 
		// reinitialize the record loop
		if (mRecord->isChanged() || 
			mRecord->getPrev() != mPlay ||
			mRecord->getFrames() != mPlay->getFrames()) {

			mRecord->copy(mPlay);
			mRecord->setPrev(mPlay);
		}

		long loopFrames = mRecord->getFrames();
		if (loopFrames == 0) {
			// can this happen?
			Trace(this, 1, "Loop: Undo anomoly 32!\n");
			setFrame(-mInput->latency);
		}
		else if (initialRecording || mFrame >= loopFrames) {
			// Returning to a loop that may be of a different size,
			// warp the frame to a sensible location
			// !! if initialRecording, ambiguous what's supposed to happen, 
			// should we have saved our previous play frame?
			warpFrame();
		}

		recalculatePlayFrame();

        // this state is no longer relevant, clear it to avoid trying
        // to fade something that isn't there any more
		mInput->resetHistory(this);

		// treat like a resize for out sync
        mSynchronizer->loopResize(this, false);

		if (undo != NULL)
		  addRedo(e, undo);

		Trace(this, 2, "Loop: Undo resuming at frame %ld play frame %ld\n", 
			  mFrame, mPlayFrame);
	}
	else {
		// Formerly reset here, but several people didn't like that since
		// it is easy to hit it by accident and lose everything.
		// Just ignore it.
		// NOTE: If we do decide to reset here, then need to decide whether
		// to return to the Setup settings
		//reset(NULL); 
	}
}

/**
 * Add a layer we just "undid" to the redo list.
 * There is a maximum size on the redo list, prune as we go.
 * If checkpoints are being used, the given layer may be the
 * head of a list. and maximum size refers to the number of checkpoints,
 * not the actual number of layers.
 *
 * The redo list is a fifo, but if checkpoints are used there
 * is a twist to the ordering.  For this orignal list:
 *
 *   1*<-2<-3<-4<-5*<-6<-7<-8<-9*
 *
 * with checkpoints on layers 1, 5, and 9.  Pressing undo twice
 * will result in this redo list:
 *
 *   6<-7<-8<-9*<-2<-3<-4<-5*
 *
 * UPDATE: We can't store checkpoint chains on the redo list
 * using the mPrev pointer.  If there were a normal layer on the
 * redo list, then you add a checkpoint chain, the end of the checkpoint
 * chain connects to the normal layer and we can't tell where the
 * boundary was.  All non-checkpoint layers get sucked into the
 * checkpoint chain.  The redo list is now maintained using the mRedo
 * pointer on the layer.
 */
void Loop::addRedo(Event* e, Layer* undone)
{
	Preset* p = e->getPreset();
	int max = p->getMaxRedo();

	if (max == 0)
	  undone->freeAll();
	else {
		// push it on the redo list
		undone->setRedo(mRedo);
		mRedo = undone;
	}

	// locate the last allowed redo layer, free the rest
	Layer* lastRedo = mRedo;
	for (int i = 0 ; i < max - 1 && lastRedo != NULL ; i++)
	  lastRedo = lastRedo->getRedo();

	if (lastRedo != NULL) {
		// we're keeping this one, but free the rest
		Layer* extras = lastRedo->getRedo();
		lastRedo->setRedo(NULL);
		while (extras != NULL) {
			Layer* next = extras->getRedo();
			extras->freeAll();
			extras = next;
		}
	}
}

/**
 * After switching to a layer that may be of a different size than
 * the last one, warp the frame counter to be relative to the
 * new loop size.
 *
 * NOTE: Try to make it look like we had been playing the previous
 * layer all along so we'll be at the right relative position when
 * synchronizing with a drum machine.  This means we can't just
 * lop off cycles.
 */
PRIVATE void Loop::warpFrame()
{
	long loopFrames = getFrames();
	if (loopFrames > 0 && mFrame >= loopFrames) {
        if (mMode != StutterMode)
          setFrame(mFrame - ((mFrame / loopFrames) * loopFrames));
        else {
            // In Stutter mode, we're supposed to return to the cycle we
            // were stuttering in.  mModeStartFrame has the beginning
            // of that cycle.
            long cycleFrames = getCycleFrames();
            long cycleOffset = mFrame - ((mFrame / cycleFrames) * cycleFrames);
            setFrame(mModeStartFrame + cycleOffset);
        }
	}
}

/****************************************************************************
 *                                                                          *
 *   							   REVERSE                                  *
 *                                                                          *
 ****************************************************************************/

/**
 * ReversePlayEvent event handler.
 * We now begin playback in reverse, prior to the ReverseEvent
 * event that finishes the transition for the record stream.
 * 
 * Reverse is subtle as usual.  You have to factor in latency loss
 * AFTER reflecting the play frame.  If you do it before, the starting
 * frame is too large and the reflected frame becomes earlier than it
 * should be.  We need to LOSE frames after the reflection not gain them.
 * Still not sure I understand this.
 * 
 * Also note that the first frame to play in reverse is not the transition
 * frame but the frame immediately before it.  In other words, the
 * reverse does not include the transition frame.  This is necessary
 * for proper symetry.  ReverseEvent is like LoopEvent
 * in that they're "outside" the region being processed.
 * 
 * Technically the only time we need to capture a tail is if there
 * was latency loss, but sometimes purely symetrical frames result
 * in abrupt transitions that sound like a bump.  Always capture
 * a tail (and fade in) unless we're exactly on a loop boundary.
 *
 * !! Try to merge this with JumpPlayEvent.
 */
PUBLIC void Loop::reversePlayEvent(Event* e) 
{
	// save previous state for undo
	e->fields.jump.undoLayer = (mPrePlay != NULL) ? mPrePlay : mPlay;
	e->fields.jump.undoFrame = mPlayFrame;
	e->fields.jump.undoMute = mMute;
	e->fields.jump.undoReverse = isReverse();

	// Before we make any changes check to see if we're already going
	// in the right direction.  We'll schedule potentially redundant events
	// since we can't always know reliably what the future direction will be
	// at the time the function was scheduled.

	if ((e->function == Forward && !isReverse()) ||
		(e->function == Backward && isReverse())) {

		Trace(this, 2, "Loop: Ignoring redundant reverse play event\n");
	}
	else {
		long loopFrames = getFrames();
		long loss = e->latencyLoss;

		// calculate the frame where the reverse is supposed to happen,
		// could also get this from our parent event, but try to be independent
		long transitionFrame = e->frame + mInput->latency + 
			mOutput->latency - loss;

		// if we were near the end, this may have wrapped
		if (transitionFrame >= getFrames())
		  transitionFrame -= getFrames();

		long playFrame = transitionFrame - 1;
		if (playFrame < 0)
		  playFrame = loopFrames - 1;

		// loop boundary rule, see above
		// have to capture the tail BEFORE we change direction
		// !! try to fix this by saving the previous direction
		if (loss > 0 || (playFrame > 0 && playFrame < loopFrames - 1))
		  mOutput->captureTail();

		// first reflect the new frame
		playFrame = reverseFrame(playFrame);

		// then factor in latency loss
		playFrame = addFrames(playFrame, loss);
	
		mPlayFrame = playFrame;
	
		// In case we did a smooth transition, adjust the last play frame
		// to avoid a fade since it will be one beyond where it should be.
		// If we captured a tail, this will be ignored.
		mOutput->setLastFrame(playFrame);

		mOutput->setReverse(!mOutput->isReverse());

		Trace(this, 2, "Loop: Starting reverse play at %ld\n", mPlayFrame);
	}
}

PUBLIC void Loop::reversePlayEventUndo(Event* e)
{
    // most of the work is in here
    jumpPlayEventUndo(e);

    // then put the flag back
	mOutput->setReverse(e->fields.jump.undoReverse);
} 

/**
 * Called by reverseEvent to officially reverse direction.
 * Also called by Function when in ResetMode to toggle reverse
 * prior to the initial recording.
 */
PUBLIC void Loop::setReverse(bool b)
{
	mInput->setReverse(b);
}

/**
 * Perform a "loop size" reflection of a frame, warning in some conditions.
 */
PUBLIC long Loop::reverseFrame(long frame)
{
    long loopFrames = getFrames();

    if (frame > loopFrames) {
        // Reflecting this will result in a negative frame counter 
        // which we will never reach until you reverse again and reflect
        // them back into this dimension.  I actually kind of like that,
        // but it probably shouldn't happen if Reverse cancels the
        // current mode.
        // This shouldn't happen for non-event frames
        Trace(this, 1, "Loop: Attempting to reflect frame greater than loop size!\n");
    }
    else if (frame == loopFrames) {
        // Must be quantized to a loop boundary, assume
        // we can "loop" this back to zero.  Shouldn't see this
        // if afterLoop is on in the ReverseEvent event,  but see it a lot
		// handling quantized transition frames.
        frame = 0;
    }

	return (loopFrames - frame - 1);
}

/****************************************************************************
 *                                                                          *
 *   								BOUNCE                                  *
 *                                                                          *
 ****************************************************************************/

/**
 * Called by Mobius via Track after we've stopped a bounce recording.
 * Note that though this will have been initated by the bounceEvent
 * handler above, we won't end up in the same Track or Loop.  This
 * is the *target* track.
 */
PUBLIC void Loop::setBounceRecording(Audio* a, int cycles) 
{
	// supposed to already be reset but make sure
	// !! should we reset the controls here?
    // !! What about Synchronizer?  The bounce track should be able
    // to become the OutSyncMaster

	reset(NULL);

    LayerPool* lp = mMobius->getLayerPool();
	mPlay = lp->newLayer(this);

	mPlay->setAudio(a);
	mPlay->setCycles(cycles);

	mRecord = mPlay->copy();
	mRecord->setPrev(mPlay);
	
	// Start playing immediately?
	// !! do we need a way to specify a start frame?
	setFrame(0);
	recalculatePlayFrame();
	setMode(PlayMode);
	
	// !! what are the potential Synchronizer issues?
	// I suppose this track could become the OutSyncMaster...
	// Same with TrackCopyTiming and TrackCopySound
}

/****************************************************************************
 *                                                                          *
 *   								SWITCH                                  *
 *                                                                          *
 ****************************************************************************/


/**
 * SwitchEvent event handler.
 * 
 * This is where we actually perform the switch.  We normally will
 * have already processed a JumpPlayEvent and have begun preplay of the
 * next loop.
 *
 * This one is complicated, pay attention.
 *
 * First examine the child events attached to the SwitchEvent.
 * Some of these are considered "control" events that will cause
 * operations such as TimeCopy and SoundCopy into the next loop.
 * Others are just placeholders for events that need to be processed
 * immediately after the switch.  They are similar to pending events
 * though they are not on the master event list.
 * 
 * Anything that still remains on the master event list is transfered
 * to the next loop, possibly with frame adjustments.  This shouldn't
 * happen often, ScriptEvent is one of the rare exceptions.
 *
 * If we're restarting the same loop, then leave the events alone.
 * Hmm, this may conflict with switch behavior if LoopCount just happens
 * to be 1?
 * 
 * Originally we tried to allow normally scheduled events, including
 * those stacked on successive quantization boudaries and marked
 * for rescheduling to be passed into the next loop. We no longer support
 * this, only control events performed immediately after the switch
 * and a few special cases like ScriptEvent are allowed once the
 * switch has begun.  We could probably support that if necessary
 * but it's complicated and obscure.  But since the code was fleshed out
 * at one time, it is still there.
 * 
 * OLD COMMENTS
 *
 * Any events that remain are transfered to the next loop.
 * The first one is normally not marked for rescheduling, and we just
 * pass it over after adjusing the frame.  Remaining events should all 
 * be marked for rescheduling.  Because we didn't know what the quantization
 * boundaries would be when the reschedule events were added, they will
 * all have the switch frame.  We could be smart and reschedule them now
 * to determine their proper frame, but it's enough just to stick them 
 * all at the end of the loop and reschedule them incrementally.
 * 
 * If the next loop is empty, we won't propagate any events that need
 * rescheduling.  But if there is an event that is ready to be activated
 * it has to pass over.  This is important for SpeedEvent and ReverseEvent
 * which need to complete state transitions even if the new loop is reset.
 * END OLD COMMENTS
 */
PUBLIC void Loop::switchEvent(Event* event)
{
	Loop* next = event->fields.loopSwitch.nextLoop;
	bool restarting = (next == this);

    // like other event handlers, cancel rehearse mode if we were in it
    // (may finish out a recording) or finish recording if we're comming
    // out of a record mode, may want to do these sooner?
    // UPDATE: To support RecordTransfer=follow, remember if we started
    // in record mode, !! does Rehearse count?
    // Sigh, we schedule and process a RecordStopEvent before the SwitchEvent
    // so we can't look at mMode here.  Currently leaving a kludgey flag
    // in the SwitchEvent when it was scheduled.
    bool wasRecording = event->fields.loopSwitch.recordCanceled;

    if (mMode == RehearseMode)
      cancelRehearse(event);
    else if (mRecording)    
      finishRecording(event);

	// If the loop was modified, need to shift before leaving so we don't
	// return and start recording from a different spot, Layer does not
	// support multiple recording passes, and if this is a Restart we 
	// are expected to be playing what was just recorded anyway
	if (mFrame > 0)
	  shift(true);

	// Save ending state.  Must get it from the input stream since the
    // output stream is normally pre-playing the next loop with possibly
    // different minor modes.  
    mRestoreState.capture(mTrack);
    mRestoreState.frame = mFrame;

	// Transfer mute state, this is always "follow"
	checkMuteCancel(event);
	next->mMuteMode = mMuteMode;
	next->mMute = mMute;
    next->mPause = mPause;

	// Transfer overdub state
	// The EDP remembers the exit overdub state and restores it
	// when you return, but most people prefer to have it "follow"
	// the current state.
	// !! it would be nice to handle this with a generated event like
	// we do for the other transfer modes, even though this way is simpler?
	Preset::TransferMode ot = mPreset->getOverdubTransfer();
	bool overdub = false;
	if (ot == Preset::XFER_FOLLOW)
	  overdub = mOverdub;
	else if (ot == Preset::XFER_RESTORE)
	  overdub = next->mOverdub;
	next->mOverdub = overdub;

	// !! technically, we can have a zero length loop that is not 
	// in Reset?  If so is it more accurate to check for ResetMode?
	bool empty = next->getFrames() == 0;

	// If it isn't in Reset, it may have been left in SwitchQuantize,
	// so be sure to return to Play/Mute/Overdub mode
	// this may be immediately changed by a stacked event
	if (!empty)
	  next->resumePlay();

	// set the frame so we can begin scheduling events
	// will also clear mLastSyncEvent
	next->setFrame(event->fields.loopSwitch.nextFrame);

	// We may have just processed a sync event in this interrupt, after
	// the switch there may be another sync event within the same interrupt
	// which we do not currently have the machinery to support.  Assume
	// that if the cycle lengths are the same we can carry over 
	// mLastSyncEvent to avoid another redundant event after the switch.
	// We may still have one though if we're moving to a different frame.
	// hmm, no, let Track deal with this since we really should
	// support this
    // !! how does this fit with EventManager now, should we be 
    // resetting the last sync event frame?
	//next->mLastSyncEvent = mLastSyncEvent;

	// set if we perform an operation on the next loop that will result
	// in us resuming at identical content though the layers will change
	bool seamless = false;

	// if we are in reset, don't attept a copy, note that the mode
	// will be RunMode in that case
	bool somethingToCopy = (getFrames() > 0);

	// set if we start rerecording
	bool recording = false;

	// We need to start scheduling events in the next loop, but if
	// we're restarting we'll be schedulng into *this* loop.
	// We have to initialize the event list before rescheduling, so 
	// capture a copy of the current event list for processing.
    EventManager* em = mTrack->getEventManager();
	EventList* current = em->stealEvents();

    // Process events stacked under the SwitchEvent that behave differently
    // than they do when not under a SwitchEvent.  These are the EDP
    // "lame duck period" events like Multiply that causes sound copy
    // rather than enter Multiply mode.  These will be removed from
    // the switch stack, what remains will be scheduled at the
    // beginning of the next loop and evaluated normally.
	
    // !! TODO: try to implement SoundCopy and TimeCopy as functions
    // with normal events.  They might be useful on their own and it
    // would make event handling during switch more consistent.
    // When Multiply is scheduled during the switch quant period 
    // for example we would stack a CopySound event rather than Multiply
    // which looks confusing anyway.  Note that if there is an AutoRecord
    // stacked or a script that does a Record then we may do a lot
    // of work for nothing.  Could recognize AutoRecord.  Make sure this
    // doesn't screw up the scheduler!

	// maintain a list of events we decide to process early so we
	// can inform the script interpreter in case it is waiting on them
	Event* toFree = NULL;

	Event* e = NULL;
	Event* nexte = NULL;

	for (e = event->getChildren() ; e != NULL ; e = nexte) {
		nexte = e->getSibling();

		Event* reschedule = NULL;
		bool remove = false;

		if (e->type == RecordEvent) {
			// Unconditional record
			// Note that threshold recording is disabled on loop switch.
			// Not sure what the EDP does but threshold seems less useful
			// here since we've already got a rhythm going.
            switchRecord(next, event, e);
			recording = true;
			remove = true;
		}
		else if (e->type == OverdubEvent) {
			// if the next loop is empty, perform a SimpleCopy and
			// remain in Overdub mode
			if (restarting) {
				// if we're restarting, treat it like a normal
				// overdub event.   
                // !! This was done here for awhile, but since we have
                // logic to transfer stacked events to the new loop
                // below this can be handled there??
				//Event* neu = Event::newEvent(Overdub, next->mFrame);
				//next->addEvent(neu);
			}
			else if (empty && somethingToCopy) {
				// execute a "simple copy"
				// the third "false" argument will cause us to ignore
				// SoundCopyMode from the preset, and always leave us in Overdub
				reschedule = next->copySound(this, OverdubOn, false, next->getFrame());
				remove = true;
				seamless = true;
			}
		}
		else if (e->type == MultiplyEvent) {
			if (somethingToCopy && !restarting) {
				reschedule = next->copySound(this, Multiply, true, next->getFrame());
				remove = true;
				seamless = true;
			}
		}
		else if (e->type == StutterEvent) {
			if (somethingToCopy && !restarting) {
				reschedule = next->copySound(this, Stutter, true, next->getFrame());
				remove = true;
				seamless = true;
			}
		}
		else if (e->type == InsertEvent) {
			// execute a TimeCopy
			// ?? ignore when restarting?  This could be another way
			// to clear the loop but leave the length.
			if (somethingToCopy && !restarting) {
				reschedule = next->copyTiming(this, next->getFrame());
				remove = true;
			}
		}
        else if (e->processed) {
            // This was scheduled on the main event list and processed
            // it is not a "stacked" or "command" event.
            // !! I HATE having the command/stack event list on the same
            // list as the child event list, consider separating these
            // but it makes some calculations harder.
            // Usually these are JumpPlayEvents but can also be 
            // InsertEndEvent and MultiplyEndEvent if we had to end a mode.
            // Don't transition these over to the next loop or you can
            // get warnings about not being in the mode it tries to cancel
            if (e->type != JumpPlayEvent && 
                e->type != InsertEndEvent && 
                e->type != MultiplyEndEvent)
              Trace(this, 1, "ERROR?: Unexpected processed event during switch: %s\n",
                    e->type->name);
            remove = true;
        }
		else {
			// everything else slides over to the next loop
		}

		// If we promoted the control event to an ordinary event
		// in the next loop, have to tell the script interpreter
		// in case we're waiting on the the control event.
		if (reschedule != NULL)
		  e->rescheduleScriptWait(reschedule);

		if (remove) {
			event->removeChild(e);
			e->processed = true;
			e->setNext(toFree);
			toFree = e;
		}
	}

	// recalculate empty 
	empty = next->getFrames() == 0;

    // if we didn't already force recording, carry it over unless we
    // did one of the loop copy things
    if (!recording && wasRecording && 
        mPreset->getRecordTransfer() == Preset::XFER_FOLLOW) {

        switchRecord(next, event, NULL);
        recording = true;
    }

    // perform empty loop processing if we haven't already
    // !! TODO: Try to handle all of these as normal events.
    // If we have an AutoRecord stacked for example and autoRecord is on
    // we'll force a RecordEvent to be processed then try to do an AutoRecord.

	if (empty && !recording) {

		Preset::EmptyLoopAction action = mPreset->getEmptyLoopAction();
		switch (action) {
			case Preset::EMPTY_LOOP_NONE: {
                // leave it in Reset
                Trace(this, 3, "Loop: Entering empty loop with no action: %ld\n",
                      (long)next->getNumber());
			}
			break;

			case Preset::EMPTY_LOOP_RECORD: {
                // immediately enter record mode
                Trace(this, 3, "Loop: Automatic recording of empty loop %ld\n",
                      (long)next->getNumber());
                // note that threshold mode is not supported here
                switchRecord(next, event, NULL);
                recording = true;

                // TODO: If SwitchDuration=OnceReturn and we AutoRecord,
                // jump back to the original loop when the record has 
                // finished.  If you use a record alternate ending, stay
                // in the next loop.  If you jump through severeal
                // loops with AutoRecord, you only jump back one loop
                // at the end, not all of them.  (Makes sense since
                // NextLoop would be an alternate ending to the
                // Record??)
            }
			break;

			case Preset::EMPTY_LOOP_TIMING: {
				Trace(this, 3, "Loop: Copy timing: loop=%ld frames=%ld\n", 
					  (long)next->getNumber(), getFrames());

				// TODO: If SamplerStyle=Once it is sutble, see manual
				// set the length of the new loop, but do not
				// copy the audio, immediately enter Insert mode
				if (somethingToCopy) {
					next->copyTiming(this, next->getFrame());
				}
			}
			break;

			case Preset::EMPTY_LOOP_COPY: {
				Trace(this, 3, "Loop: Copy sound: loop=%ld frames=%ld\n", 
					  (long)next->getNumber(), getFrames());

				// TODO: If SwitchStyle=Once it is sutble, see manual
				// Copy the previous loop and immeidately enter		
				// multiply mode.  Need to figure out a way
				// to do this incrementally so we don't swamp
				// the interrupt handler
				if (somethingToCopy) {
					next->copySound(this, Multiply, true, next->getFrame());
					seamless = true;
				}
			}
			break;
		}
	}
	
	// check empty yet again
	empty = next->getFrames() == 0;

	// We've been using OUR play state to preplay the next loop, have
	// to transfer that so it knows what we did.
	next->mPlayFrame = mPlayFrame;

	// If we switch toward the end, we may have been preplaying
	// the record layer, return to the play layer
	next->mPrePlay = NULL;

	if (!recording) {
		if (empty) {
			// if we started out empty, restore the original reset frames
			next->setFrame(-mInput->latency);
		}
		else {
			// even though the layer/frame may have changed, if this
			// was a LoopCopySound, it's a seamless shift
			mOutput->setLayerShift(seamless);
		}
	}

	// if we've been preplaying the MuteLayer, this must have been
	// LoopCopyTiming, the play frame may be way off since MuteLayer
	// isn't the same size
	if (mPrePlay == getMuteLayer())
	  next->recalculatePlayFrame();

	Trace(this, 3, "Loop: %ld return frame %ld\n", (long)mNumber, mFrame);
	Trace(this, 3, "Loop: %ld start frame %ld\n", (long)next->getNumber(), 
		  next->getFrame());

	// Any stacked events that remain other than the JumpPlayEvent
	// are promoted to normal events that will run immediately after 
	// the switch
    // !! if the next loop is in reset, should we take anything over?
    // if these can result in changes to the loop then some
    // of the empty loop optimizations we make below might be wrong
	nexte = NULL;
	for (e = event->getChildren() ; e != NULL ; e = nexte) {
		nexte = e->getSibling();

		if (e->type != JumpPlayEvent) {
			// promote it
			event->removeChild(e);
			e->pending = false;
			e->frame = next->mFrame;
            em->addEvent(e);
		}
        else {
            // should have found these on the loop above?
            // was this not marked processed?
            Trace(this, 1, "Dangling unprocessed JumpPlanEvent during switch");
        }
	}

	// Generate events to complete the transfer of stream state
 	if (next != this) {
        SpeedStep->scheduleTransfer(next);
        PitchStep->scheduleTransfer(next);
        Reverse->scheduleTransfer(next);
 	}

    // UPDATE: This comment looks old, I'm sure we need to defer
    // validation, but for different reasons....
	// Because the events we just slid over shared the same JumpPlayEvent
	// have to defer validation until *all* of them have been processed.
	// Scheduling a ValidateEvent will defer the validation until it 
	// is evaluated.  Note that this needs to be before any of the
	// top-level events are transfered since the others have their own
    // jump events and validation can work normally.
    // This also works nicely as a place to move the "Wait last" on the
	// SwitchEvent so the script won't be resumed until after all of the
	// stacked events have finished.  This is especially important
	// for the automatic events scheduled for transfer modes
	Event* v = em->newEvent(ValidateEvent, next->mFrame);
	em->addEvent(v);
	event->rescheduleScriptWait(v);

	// Any remaining top-level events slide over to the next loop.
	// Originally this is how we transfered "stacked" events, but now
	// we should only find pending events and script waits.
	
	for (e = current->getEvents() ; e != NULL ; e = nexte) {
		nexte = e->getNext();

		bool transfer = false;

		if (e == event || e->getParent() == event) {
			// The SwitchEvent or one of its children, ignore
            // at this point the only remaining child should be
            // the JumpPlayEvent
		}
		else if (e->pending) {
			// event remains pending in the next loop
			transfer = true;
		}
		else if (e->reschedule) {
			// shouldn't happen any more, it should have been stacked
            // under the switch event
			Trace(this, 1, "Loop: Ignoring reschedulable event during switch!\n");
		}
		else if (e->type == ScriptEvent) {
			// !! if this was an "until" wait, may need to adjust the frame
			// but we've lost the context, just leave it where it is
			//e->frame = (e->frame - event->frame) + next->mFrame;
			transfer = true;
		}
        /* don't have these any more
		else if (e->type == MidiOutEvent) {
            // !! need a more general mechanism for registering transportable
            // events
			transfer = true;
		}
        */
		else {
			// a normal event, adjust frame and transfer
			// this isn't supposed to happen any more
            // !! revisit this, need to have some logical behavior
            // of doing a loop switch after stacking events
			Trace(this, 1, "Loop: Ignoring transfer of active event during switch!\n");
		}
		
		if (transfer) {
			current->remove(e);
            em->addEvent(e);
		}
	}
    
	// Schedule a Mute at the end for RestartOnce, SwitchDuration=Once
    // Restart ignores SwitchDuration, which feels like what you want
    // since this is often used to bring things back into alignment but
    // keep going.
    Preset::SwitchDuration duration = mPreset->getSwitchDuration();
	if (event->function == RestartOnce || 
        (event->function != Restart && duration == Preset::SWITCH_ONCE)) {
        if (empty) {
            // TODO: If the next loop is empty we can either return immediately
            // or just stay there.  Returning immediately feels funny,
            // it would look like nothign happened.  If we do decide
            // to allow that as an option then it needs to be handled
            // at invocation time so we don't bother with events.
            Trace(this, 2, "Loop: Ignoring SWITCH_ONCE in empty loop\n");
        }
        else {
            Event* mute = em->newEvent(MuteOn, next->getFrames());
            mute->savePreset(mPreset);
            mute->quantized = true;	// to make it visible and undoable
            em->addEvent(mute);
            em->schedulePlayJump(next, mute);
        }
    }
        
	// Subtlety: if we're running a script and scheduled a wait before
    // the switch, the wait frame needs to be recalculated relative to the
    // new loop
	em->loopSwitchScriptWaits(this, event->fields.loopSwitch.nextFrame);

	// If we're Run mode, we started in Reset but had to run so the
	// event scheduling logic fired appropriately.  Return to Reset.
	// Do this after anything that needs the ending mFrame.
	if (mMode == RunMode) {
		// hmm, avoid full blown reset() since there is state
		// we may want to keep since we technically never left reset?
        // YES: reset() calls Synchronizer::loopRest which we don't want here
		mMode = ResetMode;
		setFrame(-mInput->latency);
	}

	// cancel switch mode
    em->setSwitchEvent(NULL);

	// We were using our own mPlayFrame and mOutput stream to pre-play
	// the next loop.  We will have saved the necessary state above
	// for return to this loop, be safe and reset the play position.
	if (next != this) {
		mPlayFrame = 0;
		mPrePlay = NULL;
	}

    // Schedule a Return event or a pending SUSReturn
    bool isRestart = (event->function == Restart || 
                        event->function == RestartOnce);

    // not all triggers are sustainable
    bool sustainable = false;
    Action* action = event->getAction();
    if (action != NULL)
      sustainable = action->isSustainable();

    if (event->function->sustain ||
        (!isRestart && duration == Preset::SWITCH_SUSTAIN_RETURN)) {

        // We're using a SUS variant that may return.  

        if (!sustainable) {
            // the trigger can't sustain so ignore it
            Trace(this, 2, "Ignoring schedule of Return event with non-sustainable trigger\n");
        }
        else if (empty && !recording) {
            // If the next loop is empty then do not schedule a SUSReturn,
            // otherwise we can never select empty loops to record something
            // into.  If Record was auto enabled, schedule the return,
            // this is supposed to make Record behave like SUSRecord, 
            // terminating the recording and returning at the same time,
            // this is almost certaily broken...
            Trace(this, 2, "Ignoring schedule of Return event in empty loop\n");
        }
        else if (event->fields.loopSwitch.upTransition) {
            // trigger went up during the switch quantize period
            // schedule an actual Return event
            // !! TODO if recording, this will need to be more complicated
            // also need to end the recording
            if (!empty)
              em->scheduleReturnEvent(next, event, this, true);
            else
              Trace(this, 1, "Unable to schedule Return event after recording\n");
        }
        else {
            // schedule a pending SUSReturn so we can see that we need
            // to release the trigger before something happens
            // TODO: Use invokingFunction here??
            // !! Need the full FunctionContext so we can know whether this
            // is a sustainable trigger
            // UPDATE: We should have that now with event->action
            if (!recording) {
                Event* sus = em->newEvent(event->function, SUSReturnEvent, 0);
                sus->savePreset(mPreset);
                sus->fields.loopSwitch.nextLoop = this;
                sus->pending = true;
                em->addEvent(sus);
            }
            else {
              Trace(this, 1, "Unable to schedule SUSReturn event after recording\n");
            }
        }
    }
	else if (!isRestart && duration == Preset::SWITCH_ONCE_RETURN) {
		// schedule a simple ReturnEvent 
		// do this after the jumpPlayhEventUndo above so we return
		// to the right frame
        // !! what if we're recording, could schedule some sort of pending
        // event 
        if (!empty)
          em->scheduleReturnEvent(next, event, this, false);
        else if (recording)
          Trace(this, 1, "Unable to schedule Return event after recording\n");
        else
          Trace(this, 2, "Ignoring schedule of Return event in empty loop\n");
	}
    else if (!isRestart && duration == Preset::SWITCH_SUSTAIN) {
        // KLUDGE: We'll schedule a SUSReturn for SWITCH_SUSTAIN which
        // won't actually do a retrun, it will mute, but we don't have
        // a nice event and function for SUSUnmute or SUSWait or something
        // !! How about a generic SUSWait that we can put a display name in?

        if (!sustainable) {
            // the trigger can't sustain so ignore it
            Trace(this, 2, "Ignoring schedule of SUSReturn event with non-sustainable trigger\n");
        }
        else if (event->fields.loopSwitch.upTransition) {
            // trigger went up during switch quantization
            // should do an immediate mute
            if (!empty) {
                Event* mute = em->newEvent(MuteOn, next->getFrame());
                mute->savePreset(mPreset);
                em->addEvent(mute);
                em->schedulePlayJump(next, mute);
            }
            else if (recording) {
                Trace(this, 1, "Unable to schedule Mute event after recording");
            }
            else {
                Trace(this, 2, "Ignoring schedule of Mute event in empty loop\n");
            }
        }
        else if (!empty) {
            // TODO: Use invokingFunction here??
            Event* sus = em->newEvent(event->function, SUSReturnEvent, 0);
            sus->savePreset(mPreset);
            sus->fields.loopSwitch.nextLoop = this;
            sus->pending = true;
            em->addEvent(sus);
        }
        else if (recording) {
            Trace(this, 1, "Unable to schedule SUSReturn event after recording");
        }
        else {
            Trace(this, 2, "Ignoring schedule of SUSReturn event in empty loop\n");
        }
    }

	// we're done with the next loop setup, sanity check on frame
	next->validate(event);

	Trace(this, 2, "Loop: Switching to loop %ld\n", (long)next->getNumber());
    mTrack->setLoop(next);

	// Let the Synchronizer know.  The new loop size and possible restart.
    // The rules for EDP SamplerStyle and SamplePlay are strange and I
	// don't like them.  Before 1.43 the logic was:
	// 	
	//   syncRestart = (event->function == Restart ||
	//                    samplerStyle == SAMPLERONCE ||
	//                    (event->function != SamplePlay &&
	//					   (samplerStyle == SAMPLER_START ||
	//                      samplerStyle == SAMPLER_ATTACK))
	//
	// Not sure where I got this, it may have been a misread of the EDP
	// manual.  Now, Restart and RestartOnce will always send START as
	// will switchLocation=Start

	Preset* p = event->getPreset();
    Preset::SwitchLocation location = p->getSwitchLocation();
	bool syncRestart = (event->function == Restart || 
                          event->function == RestartOnce ||
						  location == Preset::SWITCH_START);

    mSynchronizer->loopSwitch(this, syncRestart);

	// Since we're processing events in an unusual way have to 
	// inform the ScriptInterpreter about any control events we consumed
	// in case the script is waiting on them.  Also note that
	// the SwitchEvent handler is unusual in that it will inform the 
	// ScriptInterpreter of its own completion rather than 
    // EventManager::processEvent because we have to make sure it is
    // finished *before* the control events.

	event->finishScriptWait();

	// release the control events we processed
	nexte = NULL;
	for (e = toFree ; e != NULL ; e = nexte) {
		nexte = e->getNext();
		e->setNext(NULL);

        // can there be script waits on these?
        e->finishScriptWait();

        em->freeEvent(e);
	}

	// it shouldn't happen, but if we had waits on any of the
	// residual events, finish them too
	for (e = current->getEvents() ; e != NULL ; e = e->getNext())
	  e->finishScriptWait();

	// this will return the contained events to the free list
	delete current;
}

/**
 * Helper for switchEvent.
 * Force recording to start in the next loop.
 * We do this by making a transient RecordEvent and passing
 * it through the function handler.  Since Record is so fundamental could
 * consider just having Loop functions for this?
 */
PRIVATE void Loop::switchRecord(Loop* next, Event* switchEvent,
                                Event* stackedEvent)
{
    // TODO: What about ending with AutoRecord?
    EventManager* em = mTrack->getEventManager();
    Event* re = em->newEvent(Record, 0);

    // This is used in some test scripts, not sure if it needs to
    // be conveyed through the switch event though.  If anything it 
    // would probably be set on the stacked RecordEvent event?
    re->fadeOverride = switchEvent->fadeOverride;

    // could put this here if significant?
    //re->invokingFunction = switchEvent->function;

    re->invoke(next);
    re->free();
}

/**
 * Reset the current loop and give it a copy of the play layer
 * in the source loop.  Used in the implementation of EmptyLoopActioon=copy.
 *
 * Can also now be called in the implementation of the TrackCopy function
 * which is similar to EmptyLoopAction=copy except that the source loop
 * isn't one of ours.
 *
 * If an event type is passed, schedule it for immedate evaluation.
 * Return the event we scheduled so we can update the script interpreter
 * if we're waiting on the trigger event.
 *
 * !! This doesn't support the ability to copy a deferred fade.
 *
 * If a recording is ended with a switch, EmptyLoopAction=copy, and overdub will
 * be on in the next loop, we should be able to handle this like ending
 * a record with overdub and continue overdubbing seamlessly.  
 */
PRIVATE Event* Loop::copySound(Loop* src, Function* initial,
							   bool checkCopyMode, long modeFrame)
{
	Event* event = NULL;

	// release layers and Audio but leave location intact
	clear();

	// !! new: if initial == Stutter we're supposed
	// to copy only the current cycle, not the entire loop

	Layer* play = src->mPlay;
    if (play == NULL) {
        // the source is either empty or still in its initial recording
        Trace(2, "Loop::copySound source loop is empty\n");
        reset(NULL);

        // ignore the initial event, that is just for SoundCopyMode
        // and since we didn't copy it doesn't apply
    }
    else {
		mPlay = play->copy();
		mPlay->setLoop(this);

		mRecord = play->copy();
		mRecord->setLoop(this);

		mRecord->setPrev(mPlay);

        setMode(PlayMode);

        Trace(this, 2, "Loop: Copy sound from loop %ld to %ld\n", 
              (long)src->getNumber(), (long)getNumber());

        if (checkCopyMode) {
            Preset::CopyMode copyMode = mPreset->getSoundCopyMode();
            switch (copyMode) {
                case Preset::COPY_PLAY:
                    initial = NULL;
                    break;
                case Preset::COPY_OVERDUB:
                    initial = OverdubOn;
                    break;
                case Preset::COPY_MULTIPLY:
                    initial = Multiply;
                    break;	
                case Preset::COPY_INSERT:
                    initial = Insert;
                    break;
            }
        }

        if (initial != NULL) {
            // !! save preset?
            EventManager* em = mTrack->getEventManager();
            event = em->newEvent(initial, modeFrame);
            em->addEvent(event);
        }
    }

	return event;
}

/**
 * Used in the implementation of EmptyLoopAction=copyTiming
 * It is ambiguous what "timing" means exactly, it could be:
 *
 *   - the source loop length with the same number of cycles
 *   - the source loop length with one cycle
 *   - one source cycle
 *
 * Since we normally fall into Insert, the third seems the most
 * useful.  
 * 
 * The EDP says it copies the timing of the loop then enters Insert
 * mode.  But you can't create an silent loop of the correct size
 * and start inserting because you'll end up with an extra cycle
 * of silence after the insert.  Instead, Layer needs to detect when
 * we're beginnign an insert in an "empty" layer.  It does this
 * by checking for a NULL segment list.
 */
PRIVATE Event* Loop::copyTiming(Loop* src, long modeFrame)
{
	Event* event = NULL;
    EventManager* em = mTrack->getEventManager();
	Preset::CopyMode copyMode = mPreset->getTimeCopyMode();

	// clear layers and Audio, but leave positions intact
	// the record frame has already been set, do *not* trash it
	clear();

    Layer* srcPlay = src->getPlayLayer();
    if (srcPlay == NULL) {
        // it is empty or still recording, ignore
        Trace(2, "Loop::copyTiming Empty source loop\n");
        reset(NULL);
    }
    else {

        // see above on what this could be, I like one cycle
        // if this ever changes then you must also change other calculations
        // like getTrackCopyFrames
        long cycleFrames = src->getCycleFrames();
        int cycles = 1;

        // start with a play layer of the initial size
        // not absolutely necessary but nice to be able to undo
        // back to a layer with just the original cycle
        // ?? an option
        LayerPool* lp = mMobius->getLayerPool();
        mPlay = lp->newLayer(this);
        mPlay->zero(cycleFrames, cycles);

        // note that unlike the usual mRecord/mPlay relationship,
        // mRecord doesn't reference mPlay, but it is presized 
        // startInsert recognizes and starts the insert in this initial cycle
        // without adding a second one
        mRecord = lp->newLayer(this);
        mRecord->zero(cycleFrames, cycles);
        mRecord->setPrev(mPlay);

        setMode(PlayMode);

        if (copyMode == Preset::COPY_PLAY) {
            // already in play
        }
        else if (copyMode == Preset::COPY_INSERT) {
            event = em->newEvent(Insert, modeFrame);
            em->addEvent(event);
        }
        else if (copyMode == Preset::COPY_OVERDUB) {
            // overdub needs to be forced on, since the event will
            // toggle, need to make sure it starts off
            mOverdub = false;
            event = em->newEvent(Overdub, modeFrame);
            em->addEvent(event);
        }
        else if (copyMode == Preset::COPY_MULTIPLY) {
            event = em->newEvent(Multiply, modeFrame);
            em->addEvent(event);
        }

        Trace(this, 2, "Loop: Copy timing from loop %ld to %ld\n", 
              (long)src->getNumber(), (long)getNumber());
    }

	return event;
}

/****************************************************************************
 *                                                                          *
 *   								RETURN                                  *
 *                                                                          *
 ****************************************************************************/

/**
 * ReturnEvent event handler.
 * 
 * These are scheduled to return to the the previous loop after the
 * current was triggered with SwitchStyle=Once and RestartOnce.
 * This should be at the end of the loop. Conceptually similar to
 * SwitchEvent, but simpler.
 *
 * Like SwitchEvent, we've already begun playing the next loop
 * after processing a JumpPlayEvent.
 */
PUBLIC void Loop::returnEvent(Event* event)
{
	Loop* next = event->fields.loopSwitch.nextLoop;
	bool empty = next->getFrames() == 0;

	if (empty) {
		// if we started out empty, restore the original reset frames
		next->setFrame(-mInput->latency);
		next->setPlayFrame(0);

		Trace(this, 2, "Loop: Returning from loop %ld to empty loop %ld\n", 
			  (long)mNumber, (long)next->getNumber());
	}
	else {
		// should already be in play but make sure?
		next->setMode(PlayMode);
		next->setFrame(event->fields.loopSwitch.nextFrame);

		Trace(this, 2, "Loop: Returning from loop %ld to %ld frame %ld\n", 
			  (long)mNumber, (long)next->getNumber(), next->getFrame());

		// We've been using OUR play state to preplay the next loop, have
		// to transfer that so it knows what we did.
		next->mPlayFrame = mPlayFrame;

		// If we switch toward the end, we may have been preplaying
		// the record layer, return to the play layer
		next->mPrePlay = NULL;

		// if we've been preplaying the MuteLayer, this must have been
		// EmptyLoopAction=copyTiming, the play frame may be way off since MuteLayer
		// isn't the same size
		// ?? can this happen, 
		if (mPrePlay == getMuteLayer())
		  next->recalculatePlayFrame();

		// should have been preplaying this, now we're caught up
		next->validate(event);
	}

	mPrePlay = NULL;

	// transfer pending script events for the unit tests,
	// any other interesting events?
    EventManager* em = mTrack->getEventManager();
    em->cleanReturnEvents();
	em->loopSwitchScriptWaits(this, event->fields.loopSwitch.nextFrame);

	// Activate any "Wait return" event
	Event* wait = em->findEvent(ScriptEvent);
	if (wait != NULL && 
        wait->pending && 
		wait->fields.script.waitType == WAIT_RETURN) {
		wait->pending = false;
		// note that we use the special immediate option just to make sure
		wait->immediate = true;
		wait->frame = next->getFrame();
	}

    mTrack->setLoop(next);

    mSynchronizer->loopSwitch(this, false);
}

/****************************************************************************
 *                                                                          *
 *                                TRACK SELECT                              *
 *                                                                          *
 ****************************************************************************/
/*
 * Similar to Switch, but we move between Tracks which can be
 * playing simultaneously.  This is implemented down here so we can
 * process TrackCopy operations, and provide a quantized select event.
 * Unfortunately requires awareness of what a Track is and how they're
 * organized, but I don't want to contort the control flow for better
 * encapsulation.
 */

/**
 * TrackEvent event handler.
 *
 * Perform TrackCopy operations and switch control to another track.
 * If we do a TrackCopy we're only affecting the current loop.
 * Might be interesting to do a TrackReset first?
 */
PUBLIC void Loop::trackEvent(Event* e)
{
    Track* next = e->fields.trackSwitch.nextTrack;
	if (next != NULL) {
		Preset::EmptyLoopAction action = mPreset->getEmptyTrackAction();
		Loop* dest = next->getLoop();

		// ignore EmptyTrackAction if the loop has content or if we have none
		if (!dest->isReset() || isReset())
		  action = Preset::EMPTY_LOOP_NONE;
	
        if (action == Preset::EMPTY_LOOP_RECORD) {
            // Depending on leave action we may already have already 
            // done a latency delay, if not then we have to wait
            // for input latency as usual.  Record is so damn complicated
            // I'm passing it through RecordFunction which will make
            // it subject to synchronization which we may or may not
            // want.  This is different than EmptyLoopAction.
            Action* a = mMobius->newAction();
            a->inInterrupt = true;
            a->setFunction(Record);
            a->setResolvedTrack(next);
            a->trigger = TriggerEvent;
            a->triggerMode = TriggerModeOnce;
            a->noLatency = e->fields.trackSwitch.latencyDelay;

            // might want control over this?
            // actually this doesn't work, we can prevent synchronzation
            // of the record start but Synchronizer gets confused later
            // expecting it to follow sync when it ends
            // allowing sync seems more useful anyway
            //a->noSynchronization = true;

            mMobius->doActionNow(a);
            // if we're doing a Wait last on the track switch,
            // should this convert to a Wait last on the Record if
            // it is synchronized or latency delayed??
        }
        else {
            // !! if this is before the current track, we will have already
            // processed it during this interrupt and we may further advance
            // our frame in this interrupt.  That will cause the tracks
            // to become slightly out of sync, to a max of the number of frames
            // in the interrupt buffer.  Need to be syncing the frame
            // counters at the END of the interrupt.  Not sure how
            // to convey this to Mobius/Track

            if (action == Preset::EMPTY_LOOP_COPY) {
                trackCopySound(this, dest);
            }
            else if (action == Preset::EMPTY_LOOP_TIMING) {
                trackCopyTiming(this, dest);
            }
        }

        mMobius->setTrack(next->getRawNumber());
        // copied track should be able to become a sync master?
    }
}
/**
 * Used in the implementation of TrackCopy and EmptyTrackAction=Copy.
 * Copy the audio from one loop to another, position the frames and
 * set up the new mode.
 *
 * "this" loop is the Loop the event is being processed in.  It
 * will be the same as "src" for EmptyTrackAction=copy and
 * the same as "dest" for TrackCopy.
 */
PRIVATE void Loop::trackCopySound(Loop* src, Loop* dest)
{
    // this is complicated
    long startFrame = 0;
    long modeFrame = 0;

    getTrackCopyFrame(src, dest, &startFrame, &modeFrame);

    dest->setFrame(startFrame);

    // Third arg was originaly false to ignore SoundCopyMode,
    // but that's expected here too right?
	dest->copySound(src, NULL, true, modeFrame);

    // have to do this after the size is known
	dest->recalculatePlayFrame();
}

/**
 * Used in the implementation of TrackCopyTiming and EmptyTrackAction=CopyTime.
 * Copy the timing from one loop to another, position the frames and
 * set up the new mode.
 *
 * "this" loop is the Loop the event is being processed in.  It
 * will be the same as "src" for EmptyTrackAction=copy and
 * the same as "dest" for TrackCopy.
 * 
 * ?? What if the source loop is empty, go into reset or
 * leave it alone?
 */
PRIVATE void Loop::trackCopyTiming(Loop* src, Loop* dest)
{
    // this is complicated
    long startFrame = 0;
    long modeFrame = 0;

    getTrackCopyFrame(src, dest, &startFrame, &modeFrame);

	dest->setFrame(startFrame);
	dest->copyTiming(src, modeFrame);

    // have to do this after the size is known
	dest->recalculatePlayFrame();
}

/**
 * Helper for all forms of track copy.
 * We want the destination track to have the same frame as the source
 * track but since the event we're processing can be in the middle
 * of an interrupt block, they can be out of sync at the end
 * of the current interrupt.
 *
 * "this" loop is the Loop the event is being processed in.  It
 * will be the same as "src" for EmptyTrackAction=copy and
 * the same as "dest" for TrackCopy.
 *
 * Example 1: EmptyTrackAction, source before dest
 *
 *     Track 1: source
 *       currently at frame 100,000, offset 142 into a buffer of 256
 *
 *     Track 2: destination
 *  
 * In this example, we're a little over halfway processing our interrupt
 * buffer with 114 frames left to go.  Track 2 has not advanced at all
 * yet so it has 256 frames to go.  If we copy the source frame
 * to the destination track without adjustment, track 1 will end the
 * interrupt on frame 100,114 and track 2 will end on frame 100,256.
 * To make them align, the destination frame is calculated as:
 *
 *      destFrame = sourceFrame - sourceAdvance
 *
 * In this example 99858.  When the destination track advances 256
 * it also ends up at 99858 + 256 = 100114.
 *
 * Example 2: EmptyTrackAction, dest before source
 *
 *   Track 1: destination
 *   Track 2: source, frame 100,000, offset 142, buffer 256
 * 
 * The destination track has already fully advanced in this interrupt.
 * The source track has 114 frames remaining.  Without adjustment
 * at the end of the interrupt track 1 would be at frame 100,000
 * and track 2 would be at 100,114.  The adjustment is:
 *
 *       destFrame = sourceFrame + sourceRemaining
 *
 * In this example 100,000 + 114.
 *
 * Example 3: TrackCopy, source before dest
 *
 * With the TrackCopy functions are are "in" the destination track.
 *
 *   Track 1: source, frame 100,000
 *   Track 2: dest, frame 50,000, offset 142, buffer 256
 *
 * The source track has finished advancing, the destination track
 * has 114 frames remaining.  The adjustment is:
 *
 *    destFrame = sourceFrame - destRemaining
 *
 * Example 3: TrackCopy, dest after source
 * 
 *   Track 1: dest, frame 50,000, offset 142, buffer 256
 *   Track 2: source, frame 100,000
 *
 * At the end of the interrupt track 2 will be at 100,256.
 * The adjustment is:
 *
 *     destFrame = sourceFrame + destAdvance
 *
 * In this examle, 100,142 plus the final advance of 114 brings 
 * track 1 to 100,256.
 *
 * As usual rate shift complicates this.
 *
 * If we can assume that the rate shift is the same in both
 * tracks, then the remainder or advance adjustment 
 * is simply multipled by the rate of the source track.
 * In example 4 if both tracks were in 1/2 speed, the
 * effective buffer size would be 128, the ending frame
 * for track 2 will be 100,128 and destAdvance would be 71
 * so 100,000 + 71 (destAdvance) + 57 (destRemaining) = 100,128.
 * 
 * If the rates are different it is more complicated.
 * If the source is in 1/2 speed and the destination is in 
 * full speed, we could handle this several ways.
 *
 *    - copy the entire unadjusted source track, it will sound
 *      twice as long as the source track
 *
 *    - copy the rate adjusted source track, it will sound
 *      the same size until the source track unshifts
 *
 *
 * UPDATE:
 *
 * Well that's all well and good, but there is another complication
 * that can be seen if you switch during recording.  The switch happens
 * immediately after the recording of the current track is closed
 * which leave the source frame at zero.  In this case for example 1 the
 * frame will be negative.  We wrap it back to a little before the end
 * of the loop.  Now if TimeCopyMode is Multiply or Insert we have a problem
 * because we immediately see a crossing of the loop boundary which
 * adds another cycle.  
 *
 * What we really need to calculate here is two frames.  The first the
 * starting frame the loop would have if it had always been the size
 * of the copied loop, and the second the frame that the TimeCopyMode
 * change should occur, this will be exactly the same as the offset
 * into the original buffer that the track select event happened on.
 * So for example 1:
 *
 *    startFrame = sourceFrame - sourceAdvance
 *    modeSwitchFrame = sourceFrame
 *
 * In any of the other examples where the dest frame is after the sourceFrame
 * we have a problem.  Ideally we would have started the mode exactly on 
 * the sourceFrame but since that buffer has been fully advanced now, we can't
 * easilly go back in time and insert a bit of recording into the previous
 * tracks.  In these cases the Overdub/Multiply/Insert will start a little
 * late but since the loops are least the same size it won't be that bad. 
 * They still be aligned, we must might shave a very small bit off the front.
 *
 */
PRIVATE void Loop::getTrackCopyFrame(Loop* src, Loop* dest, 
                                     long* retStartFrame, long* retModeFrame)
{
    long startFrame = src->getFrame();
    long modeFrame = startFrame;

    if (this == src) {
        // We're pushing content from this loop to another,
        // TrackSelect with EmptyTrackSction=Copy or EmptyTrackAction=CopyTime
        Track* strack = src->getTrack();
        float rate = strack->getEffectiveSpeed();
        if (mTrack->isPriority() || 
            (mTrack->getRawNumber() < dest->getTrack()->getRawNumber())) {
            // Example 1: destFrame = sourceFrame - sourceAdvance
            long advance = strack->getProcessedFrames();
            startFrame = src->getFrame() - (long)((float)advance * rate);
        }
        else {
            // Example 2: destFrame = sourceFrame + sourceRemaining
            long remaining = strack->getRemainingFrames();
            startFrame = src->getFrame() + (long)((float)remaining * rate);
            modeFrame = startFrame;
        }
    }
    else {
        // We're pulling content from another loop into this one,
        // TrackCopy or TrackCopyTime.
        Track* strack = src->getTrack();
        Track* dtrack = dest->getTrack();
        // Who shold own the rate?
        float rate = dtrack->getEffectiveSpeed();

        if (strack->isPriority() || 
            (src->getTrack()->getRawNumber() < mTrack->getRawNumber())) {
            // Example 3: destFrame = sourceFrame - destRemaining
            long remaining = dtrack->getRemainingFrames();
            startFrame = src->getFrame() - (long)((float)remaining * rate);
        }
        else {
            // Example 4: destFrame = sourceFrame + destAdvance
            long advance = dtrack->getProcessedFrames();
            startFrame = src->getFrames() + (long)((float)advance * rate);
            modeFrame = startFrame;
        }
    }

    // However we got here wrap relative to the source cycle size
    // because we only copied one cycle and the source frame may be
    // in a higher cycle.
    // Only expecting this to be negative if we just closed 
    // the recording of the source loop.  
    // If we ever decide to copy more than one cycle, then this
    // and copyTiming() both need to change!
    long newFrames = src->getCycleFrames();
    startFrame = src->wrapFrame(startFrame, newFrames);
    modeFrame = src->wrapFrame(modeFrame, newFrames);

    *retStartFrame = startFrame;
    *retModeFrame = modeFrame;
}

/**
 * Helper for TrackCopyFunction.  This isn't an event we just process
 * them immediately, though it might be interesting to let them
 * be quantized? 
 *
 * !! Should we be transfering the play state too (reverse, speed, etc.)
 */
PUBLIC void Loop::trackCopySound(Track* src)
{
    if (src != NULL) {
		// ignore tail capture?
        // !! if we're currently the track sync master, this will reassign
        // it, it is hard to tell the difference between internal reset
        // and user initiated reset
        reset(NULL);

        trackCopySound(src->getLoop(), this);

		// also copy the track controls
		// it may actually be more interesting to designate other play modes
		// e.g. reverse so we can immediately play a counterpoint
        // why not do this for trackCopyTiming?
        // !! do we really want this, what if they preset the controls
        // for some effect?
		mTrack->setInputLevel(src->getInputLevel());
		mTrack->setOutputLevel(src->getOutputLevel());
		mTrack->setFeedback(src->getFeedback());
		mTrack->setAltFeedback(src->getAltFeedback());
		mTrack->setPan(src->getPan());

		// what about group and focus lock?
		// what about pitch, rate, and direction?
        // !! should this be a special Synchronizer callback like loopSwitch?
		mSynchronizer->loopResize(this, false);
    }
}

/**
 * Helper for TrackCopyFunction.
 * 
 * !! Should we be transfering the play state too (reverse, speed, etc.)
 * !! what about track controls, output level and pan seem reasonable what
 * about input level and feedback?
 *
 * Here it seems like we should leave the track controls as they are.
 */
PUBLIC void Loop::trackCopyTiming(Track* src)
{
    if (src != NULL) {
        reset(NULL);

        trackCopyTiming(src->getLoop(), this);

        // !! should this be a special Synchronizer callback like loopSwitch?
		mSynchronizer->loopResize(this, false);
    }
}

/****************************************************************************
 *                                                                          *
 *   								 SYNC                                   *
 *                                                                          *
 ****************************************************************************/

/**
 * Called by Synchronizer when we've begun recording another cycle
 */
PUBLIC void Loop::setRecordCycles(long cycles)
{
	if (mRecord != NULL)
	  mRecord->setCycles(cycles);
}

/**
 * Helper for MidiStartEvent and RealignEvent.
 * For the special functions that enter Mute mode waiting for
 * a sync pulse, we have the possibility of the pulse comming in
 * before we get to the scheduled mute event.  This happens often
 * when RealignTime=Immediate, but can happen in any of the other
 * modes if we schedule very near the next clock pulse.
 *
 * Have to look for an upcomming MuteEvent and remove it.
 * 
 * Tried to avoid this by doing an immediate mute without schedulign an event,
 * but MuteEvent triggers all the other return/rehearse/record unwinding 
 * logic that we still need.
 * !! Revisit this, the MuteRealign scheduler could stop record modes too.
 *
 */
PRIVATE void Loop::cancelSyncMute(Event* e)
{
    EventManager* em = mTrack->getEventManager();
	Event* mute = em->findEvent(MuteEvent);
	if (mute != NULL) {
		Trace(this, 2, "Loop: Removing obsolete mute event after %s\n",
			  e->type->name);
        em->undoEvent(mute);
	}

	mMute = false;
    mPause = false;
	mMuteMode = false;
	resumePlay();
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
