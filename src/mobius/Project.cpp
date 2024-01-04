/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * A representation of the runtime state of a Mobius instance, 
 * including audio data.  This allows Mobius state to be saved to
 * and restored from files.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "Util.h"
#include "List.h"
#include "XmlModel.h"
#include "XmlBuffer.h"
#include "XomParser.h"
#include "Expr.h"

#include "Loop.h"
#include "Mobius.h"
#include "MobiusConfig.h"
#include "Layer.h"
#include "Project.h"
#include "Setup.h"
#include "Segment.h"
#include "Track.h"
#include "UserVariable.h"

/****************************************************************************
 *                                                                          *
 *   							XML CONSTANTS                               *
 *                                                                          *
 ****************************************************************************/

#define EL_PROJECT "Project"
#define EL_TRACK "Track"
#define EL_LOOP "Loop"
#define EL_LAYER "Layer"
#define EL_SEGMENT "Segment"

#define ATT_NUMBER "number"
#define ATT_BINDINGS "bindings"
#define ATT_MIDI_CONFIG "midiConfig"
#define ATT_SETUP "setup"
#define ATT_GROUP "group"
#define ATT_LAYER "layer"
#define ATT_OFFSET "offset"
#define ATT_START_FRAME "startFrame"
#define ATT_FRAMES "frames"
#define ATT_FEEDBACK "feedback"
#define ATT_COPY_LEFT "localCopyLeft"
#define ATT_COPY_RIGHT "localCopyRight"
 
#define ATT_ID "id"
#define ATT_CYCLES "cycles"
#define ATT_BUFFERS "buffers"
#define ATT_FRAME "frame"
#define ATT_REVERSE "reverse"
#define ATT_SPEED_OCTAVE "speedOctave"
#define ATT_SPEED_STEP "speedStep"
#define ATT_SPEED_BEND "speedBend"
#define ATT_SPEED_TOGGLE "speedToggle"
#define ATT_PITCH_OCTAVE "pitchOctave"
#define ATT_PITCH_STEP "pitchStep"
#define ATT_PITCH_BEND "pitchBend"
#define ATT_TIME_STRETCH "timeStretch"
#define ATT_OVERDUB "overdub"
#define ATT_ACTIVE "active"
#define ATT_AUDIO "audio"
#define ATT_PROTECTED "protected"
#define ATT_PRESET "preset"
#define ATT_FEEDBACK "feedback"
#define ATT_ALT_FEEDBACK "altFeedback"
#define ATT_INPUT "input"
#define ATT_OUTPUT "output"
#define ATT_PAN "pan"
#define ATT_FOCUS_LOCK "focusLock"
#define ATT_DEFERRED_FADE_LEFT "deferredFadeLeft"
#define ATT_DEFERRED_FADE_RIGHT "deferredFadeRight"
#define ATT_CONTAINS_DEFERRED_FADE_LEFT "containsDeferredFadeLeft"
#define ATT_CONTAINS_DEFERRED_FADE_RIGHT "containsDeferredFadeRight"
#define ATT_REVERSE_RECORD "reverseRecord"


/****************************************************************************
 *                                                                          *
 *   						   PROJECT SEGMENT                              *
 *                                                                          *
 ****************************************************************************/

PUBLIC ProjectSegment::ProjectSegment()
{
	init();
}

PUBLIC ProjectSegment::ProjectSegment(MobiusConfig* config, Segment* src)
{
	init();

	mOffset = src->getOffset();
	mStartFrame = src->getStartFrame();
	mFrames = src->getFrames();
	mFeedback = src->getFeedback();
	mLocalCopyLeft = src->getLocalCopyLeft();
	mLocalCopyRight = src->getLocalCopyRight();

	Layer* l = src->getLayer();
	if (l != NULL) {
		// !! need a more reliable id?
		mLayer = l->getNumber();
	}
}

PUBLIC ProjectSegment::ProjectSegment(XmlElement* e)
{
	init();
	parseXml(e);
}

PUBLIC void ProjectSegment::init()
{
	mOffset = 0;
	mStartFrame = 0;
	mFrames = 0;
	mLayer = 0;
	mFeedback = 127;
	mLocalCopyLeft = false;
	mLocalCopyRight = false;
}

Segment* ProjectSegment::allocSegment(Layer* layer)
{
	Segment* s = new Segment(layer);
	s->setOffset(mOffset);
	s->setStartFrame(mStartFrame);
	s->setFrames(mFrames);
	s->setFeedback(mFeedback);
	s->setLocalCopyLeft(mLocalCopyLeft);
	s->setLocalCopyRight(mLocalCopyRight);
	return s;
}

PUBLIC ProjectSegment::~ProjectSegment()
{
}

void ProjectSegment::setOffset(long f)
{
    mOffset = f;
}

long ProjectSegment::getOffset()
{
    return mOffset;
}

void ProjectSegment::setLayer(int id)
{
	mLayer = id;
}

int ProjectSegment::getLayer()
{
    return mLayer;
}

void ProjectSegment::setStartFrame(long f)
{
    mStartFrame = f;
}

long ProjectSegment::getStartFrame()
{
    return mStartFrame;
}

void ProjectSegment::setFrames(long l)
{
    mFrames = l;
}

long ProjectSegment::getFrames()
{
    return mFrames;
}

void ProjectSegment::setFeedback(int i)
{
    mFeedback = i;
}

int ProjectSegment::getFeedback()
{
    return mFeedback;
}

void ProjectSegment::setLocalCopyLeft(long frames)
{
	mLocalCopyLeft = frames;
}

long ProjectSegment::getLocalCopyLeft()
{
	return mLocalCopyLeft;
}

void ProjectSegment::setLocalCopyRight(long frames)
{
	mLocalCopyRight = frames;
}

long ProjectSegment::getLocalCopyRight()
{
	return mLocalCopyRight;
}

void ProjectSegment::toXml(XmlBuffer* b)
{
	b->addOpenStartTag(EL_SEGMENT);
    b->addAttribute(ATT_LAYER, mLayer);
    b->addAttribute(ATT_OFFSET, mOffset);
	b->addAttribute(ATT_START_FRAME, mStartFrame);
    b->addAttribute(ATT_FRAMES, mFrames);
    b->addAttribute(ATT_FEEDBACK, mFeedback);
    b->addAttribute(ATT_COPY_LEFT, mLocalCopyLeft);
    b->addAttribute(ATT_COPY_RIGHT, mLocalCopyRight);
	b->add("/>\n");
}

void ProjectSegment::parseXml(XmlElement* e)
{
	mLayer = e->getIntAttribute(ATT_LAYER);
	mOffset = e->getIntAttribute(ATT_OFFSET);
	mStartFrame = e->getIntAttribute(ATT_START_FRAME);
	mFrames = e->getIntAttribute(ATT_FRAMES);
	mFeedback = e->getIntAttribute(ATT_FEEDBACK);
	mLocalCopyLeft = e->getIntAttribute(ATT_COPY_LEFT);
	mLocalCopyRight = e->getIntAttribute(ATT_COPY_RIGHT);
}

/****************************************************************************
 *                                                                          *
 *                               PROJECT LAYER                              *
 *                                                                          *
 ****************************************************************************/

PUBLIC ProjectLayer::ProjectLayer()
{
	init();
}

PUBLIC ProjectLayer::ProjectLayer(XmlElement* e)
{
	init();
	parseXml(e);
}

PUBLIC ProjectLayer::ProjectLayer(MobiusConfig* config, Project* p, Layer* l)
{
	init();

    // ids are only necessary if NoLayerFlattening is on and we
    // need to save LayerSegments, suppress if we're flattening to
    // avoid confusion 
    if (l->isNoFlattening())
      mId = l->getNumber();

	mCycles = l->getCycles();
	mDeferredFadeLeft = l->isDeferredFadeLeft();
	mDeferredFadeRight = l->isDeferredFadeRight();
	mContainsDeferredFadeLeft = l->isContainsDeferredFadeLeft();
	mContainsDeferredFadeRight = l->isContainsDeferredFadeRight();
	mReverseRecord = l->isReverseRecord();

    // if NoFlattening is on then we must save segments
    if (!l->isNoFlattening()) {

        // this will make a copy we own
        setAudio(l->flatten());

        // the Isolated Overdubs global parameter was experimental
        // and is no longer exposed, so this should never be true
        // and we won't have an mOverdub object or an mOverdubPath
		if (l->isIsolatedOverdub()) {
			Audio* a = l->getOverdub();
			if (a != NULL && !a->isEmpty()) {
                // have to copy this since the mExternalAudio flag
                // must apply to both mAudio and mOverdub
                AudioPool* pool = a->getPool();
                if (pool == NULL)
                  Trace(1, "ProjectLayer: no audio pool!\n");
                else {
                    Audio* ov = pool->newAudio();
                    ov->copy(a);
                    setOverdub(ov);
                    // since we're going to save this in a file, 
                    // set the correct sample rate
                    ov->setSampleRate(l->getLoop()->getMobius()->getSampleRate());
                }
            }
		}
    }
	else {
		// we don't own the Audio objects so don't delete them
		mExternalAudio = true;

		Audio* a = l->getAudio();
		if (!a->isEmpty())
		  setAudio(a);

		for (Segment* seg = l->getSegments() ; seg != NULL ; 
			 seg = seg->getNext()) {
			ProjectSegment* ps = new ProjectSegment(config, seg);
			add(ps);
		}
	}  	
}

/**
 * Used when loading individual Audios from a file.
 */
PUBLIC ProjectLayer::ProjectLayer(Audio* a)
{
	init();
	setAudio(a);
}

PUBLIC void ProjectLayer::init()
{
	mId = 0;
	mCycles = 0;
	mSegments = NULL;
    mAudio = NULL;
	mOverdub = NULL;
	mExternalAudio = false;
    mPath = NULL;
	mOverdubPath = NULL;
    mProtected = false;
	mDeferredFadeLeft = false;
	mDeferredFadeRight = false;
	mReverseRecord = false;
	mLayer = NULL;
}

PUBLIC ProjectLayer::~ProjectLayer()
{
    delete mPath;
	if (!mExternalAudio) {
		delete mAudio;
		delete mOverdub;
	}
	if (mSegments != NULL) {
		for (int i = 0 ; i < mSegments->size() ; i++) {
			ProjectSegment* s = (ProjectSegment*)mSegments->get(i);
			delete s;
		}
		delete mSegments;
	}

}

/**
 * Partially initialize a Layer object.
 * The segment list will be allocated later in resolveLayers.
 */
Layer* ProjectLayer::allocLayer(LayerPool* pool)
{
	if (mLayer == NULL) {
		mLayer = pool->newLayer(NULL);
		mLayer->setNumber(mId);

		if (mAudio != NULL) {
			mLayer->setAudio(mAudio);
			mAudio = NULL;
		}

        // this was an experimental feature that is no longer exposed
        // keep it around for awhile in case we want to resurect it
		if (mOverdub != NULL) {
			mLayer->setOverdub(mOverdub);
			mLayer->setIsolatedOverdub(true);
			mOverdub = NULL;
		}

        // when synthesizing Projects to load individual loops, not
        // all of the state may be filled out
        int cycles = mCycles;
        if (cycles <= 0) 
          cycles = 1;

		// !! need to restore the sync pulse count

		mLayer->setCycles(cycles);
		mLayer->setDeferredFadeLeft(mDeferredFadeLeft);
		mLayer->setContainsDeferredFadeLeft(mContainsDeferredFadeLeft);
		mLayer->setDeferredFadeRight(mDeferredFadeRight);
		mLayer->setContainsDeferredFadeRight(mContainsDeferredFadeRight);
		mLayer->setReverseRecord(mReverseRecord);
	}
	return mLayer;
}

void ProjectLayer::resolveLayers(Project* p)
{
	if (mLayer == NULL) 
	  Trace(1, "Calling resolveLayers before layers allocated");

	else if (mSegments != NULL) {
		for (int i = 0 ; i < mSegments->size() ; i++) {
			ProjectSegment* ps = (ProjectSegment*)mSegments->get(i);
			Layer* layer = p->findLayer(ps->getLayer());
			if (layer == NULL) {
				Trace(1, "Unable to resolve project layer id %ld\n", 
					  (long)ps->getLayer());
			}
			else {
				Segment* s = ps->allocSegment(layer);
				mLayer->addSegment(s);
			}
		}
	}
}

PUBLIC int ProjectLayer::getId()
{
	return mId;
}

PUBLIC Layer* ProjectLayer::getLayer()
{
	return mLayer;
}

PUBLIC void ProjectLayer::setCycles(int i)
{
	mCycles = i;
}

PUBLIC int ProjectLayer::getCycles()
{
	return mCycles;
}

PUBLIC void ProjectLayer::setAudio(Audio* a)
{
	if (!mExternalAudio)
	  delete mAudio;
	mAudio = a;
}

PUBLIC Audio* ProjectLayer::getAudio()
{
	return mAudio;
}

PUBLIC Audio* ProjectLayer::stealAudio()
{
	Audio* a = mAudio;
	mAudio = NULL;
	mExternalAudio = false;
	return a;
}

PUBLIC void ProjectLayer::setOverdub(Audio* a)
{
	if (!mExternalAudio)
	  delete mOverdub;
	mOverdub = a;
}

PUBLIC Audio* ProjectLayer::getOverdub()
{
	return mOverdub;
}

PUBLIC Audio* ProjectLayer::stealOverdub()
{
	Audio* a = mOverdub;
	mOverdub = NULL;
	return a;
}

PUBLIC void ProjectLayer::setPath(const char* path)
{
	delete mPath;
    mPath = CopyString(path);
}

PUBLIC const char* ProjectLayer::getPath()
{
	return mPath;
}

PUBLIC void ProjectLayer::setOverdubPath(const char* path)
{
	delete mOverdubPath;
    mOverdubPath = CopyString(path);
}

PUBLIC const char* ProjectLayer::getOverdubPath()
{
	return mOverdubPath;
}

PUBLIC void ProjectLayer::setProtected(bool b)
{
	mProtected = b;
}

PUBLIC bool ProjectLayer::isProtected()
{
	return mProtected;
}

PUBLIC void ProjectLayer::setDeferredFadeLeft(bool b)
{
	mDeferredFadeLeft = b;
}

PUBLIC bool ProjectLayer::isDeferredFadeLeft()
{
	return mDeferredFadeLeft;
}

PUBLIC void ProjectLayer::setDeferredFadeRight(bool b)
{
	mDeferredFadeRight = b;
}

PUBLIC bool ProjectLayer::isDeferredFadeRight()
{
	return mDeferredFadeRight;
}

PUBLIC void ProjectLayer::setReverseRecord(bool b)
{
	mReverseRecord = b;
}

PUBLIC bool ProjectLayer::isReverseRecord()
{
	return mReverseRecord;
}

PUBLIC void ProjectLayer::add(ProjectSegment* seg)
{
	if (mSegments == NULL)
	  mSegments = new List();
	mSegments->add(seg);
}

void ProjectLayer::writeAudio(const char* baseName, int tracknum, int loopnum,
							  int layernum)
{
	char path[1024];

    if (mAudio != NULL && !mAudio->isEmpty() && !mProtected) {

        // todo: need to support inline audio in the XML
        sprintf(path, "%s-%d-%d-%d.wav", baseName, 
				tracknum, loopnum, layernum);

        // Remember the new path too, should we every try to reuse
        // the previous path?  could be out of order by now
        setPath(path);

        mAudio->write(path);
    }

	if (mOverdub != NULL && !mOverdub->isEmpty()) {
        // todo: need to support inline audio in the XML
        sprintf(path, "%s-%d-%d-%d-overdub.wav", baseName, 
				tracknum, loopnum, layernum);
		setOverdubPath(path);
		mOverdub->write(path);
	}

}


void ProjectLayer::toXml(XmlBuffer* b)
{
	b->addOpenStartTag(EL_LAYER);

    // this is required only if NoLayerFlattening is on and
    // we have to save LayerSegments, if we left it zero we
    // don't need it
    if (mId > 0)
      b->addAttribute(ATT_ID, mId);

	b->addAttribute(ATT_CYCLES, mCycles);
    b->addAttribute(ATT_AUDIO, mPath);
    b->addAttribute(ATT_OVERDUB, mOverdubPath);
    b->addAttribute(ATT_PROTECTED, mProtected);
    b->addAttribute(ATT_DEFERRED_FADE_LEFT, mDeferredFadeLeft);
    b->addAttribute(ATT_DEFERRED_FADE_RIGHT, mDeferredFadeRight);
    b->addAttribute(ATT_CONTAINS_DEFERRED_FADE_LEFT, mContainsDeferredFadeLeft);
    b->addAttribute(ATT_CONTAINS_DEFERRED_FADE_RIGHT, mContainsDeferredFadeRight);
    b->addAttribute(ATT_REVERSE_RECORD, mReverseRecord);

	if (mSegments == NULL) 
	  b->add("/>\n");
	else {
		b->add(">\n");

		if (mSegments != NULL) {
			b->incIndent();
			for (int i = 0 ; i < mSegments->size() ; i++) {
				ProjectSegment* seg = (ProjectSegment*)mSegments->get(i);
				seg->toXml(b);
			}
			b->decIndent();
		}

		b->addEndTag(EL_LAYER);
	}
}

void ProjectLayer::parseXml(XmlElement* e)
{
	mId = e->getIntAttribute(ATT_ID);	
	mCycles = e->getIntAttribute(ATT_CYCLES);
    mProtected = e->getBoolAttribute(ATT_PROTECTED);
    mDeferredFadeLeft = e->getBoolAttribute(ATT_DEFERRED_FADE_LEFT);
    mDeferredFadeRight = e->getBoolAttribute(ATT_DEFERRED_FADE_RIGHT);
    mContainsDeferredFadeLeft = e->getBoolAttribute(ATT_CONTAINS_DEFERRED_FADE_LEFT);
    mContainsDeferredFadeRight = e->getBoolAttribute(ATT_CONTAINS_DEFERRED_FADE_RIGHT);
    mReverseRecord = e->getBoolAttribute(ATT_REVERSE_RECORD);
	setPath(e->getAttribute(ATT_AUDIO));
	setOverdubPath(e->getAttribute(ATT_OVERDUB));

	for (XmlElement* child = e->getChildElement() ; child != NULL ; 
		 child = child->getNextElement()) {

		add(new ProjectSegment(child));
	}
}

/****************************************************************************
 *                                                                          *
 *   							 PROJECT LOOP                               *
 *                                                                          *
 ****************************************************************************/

PUBLIC ProjectLoop::ProjectLoop()
{
	init();
}

PUBLIC ProjectLoop::ProjectLoop(XmlElement* e)
{
	init();
	parseXml(e);
}

PUBLIC ProjectLoop::ProjectLoop(MobiusConfig* config, Project* p, Loop* l)
{
	init();

    // hmm, capturing the current frame is bad for unit tests since 
    // MobiusThread will process the save event at a random time,
    // if it is ever useful to save this, will need a Project option
    // to prevent saving it in some cases
	//setFrame(l->getFrame());

	Layer* layer = l->getPlayLayer();
	while (layer != NULL) {
		add(new ProjectLayer(config, p, layer));
		if (config->isSaveLayers())
		  layer = layer->getPrev();
		else
		  layer = NULL;
	}
}

PUBLIC void ProjectLoop::init()
{
    mNumber = 0;
	mLayers = NULL;
	mFrame = 0;
	mActive = false;
}

PUBLIC ProjectLoop::~ProjectLoop()
{
	if (mLayers != NULL) {
		for (int i = 0 ; i < mLayers->size() ; i++) {
			ProjectLayer* l = (ProjectLayer*)mLayers->get(i);
			delete l;
		}
		delete mLayers;
	}
}

PUBLIC void ProjectLoop::add(ProjectLayer* l)
{
	if (mLayers == NULL)
	  mLayers = new List();
	mLayers->add(l);
}

PUBLIC void ProjectLoop::setNumber(int number)
{
	mNumber = number;
}

PUBLIC int ProjectLoop::getNumber()
{
	return mNumber;
}

PUBLIC List* ProjectLoop::getLayers()
{
	return mLayers;
}

PUBLIC void ProjectLoop::setFrame(long f)
{
	mFrame = f;
}

PUBLIC long ProjectLoop::getFrame()
{
	return mFrame;
}

PUBLIC void ProjectLoop::setActive(bool b)
{
	mActive = b;
}

PUBLIC bool ProjectLoop::isActive()
{
	return mActive;
}

/**
 * Helper for layer resolution at load time.
 */
Layer* ProjectLoop::findLayer(int id)
{
	Layer* found = NULL;
	if (mLayers != NULL) {
		for (int i = 0 ; i < mLayers->size() && found == NULL ; i++) {
			ProjectLayer* l = (ProjectLayer*)mLayers->get(i);
			if (l->getId() == id)
			  found = l->getLayer();
		}
	}
	return found;
}

void ProjectLoop::allocLayers(LayerPool* pool)
{
	if (mLayers != NULL) {
		for (int i = 0 ; i < mLayers->size() ; i++) {
			ProjectLayer* l = (ProjectLayer*)mLayers->get(i);
			l->allocLayer(pool);
		}
	}
}

void ProjectLoop::resolveLayers(Project* p)
{
	if (mLayers != NULL) {
		for (int i = 0 ; i < mLayers->size() ; i++) {
			ProjectLayer* l = (ProjectLayer*)mLayers->get(i);
			l->resolveLayers(p);
		}
	}
}

void ProjectLoop::writeAudio(const char* baseName, int tracknum, int loopnum)
{
	if (mLayers != NULL) {
		for (int i = 0 ; i < mLayers->size() ; i++) {
			ProjectLayer* layer = (ProjectLayer*)mLayers->get(i);
			// use the layer id, it makes more sense
			//int layernum = i + 1;
			int layernum = layer->getId();
			layer->writeAudio(baseName, tracknum, loopnum, layernum);
		}
	}
}

void ProjectLoop::toXml(XmlBuffer* b)
{
	b->addOpenStartTag(EL_LOOP);
	b->addAttribute(ATT_ACTIVE, mActive);
	if (mFrame > 0)
	  b->addAttribute(ATT_FRAME, mFrame);

	if (mLayers == NULL)
	  b->add("/>\n");
	else {
		b->add(">\n");

		if (mLayers != NULL) {
			b->incIndent();
			for (int i = 0 ; i < mLayers->size() ; i++) {
				ProjectLayer* layer = (ProjectLayer*)mLayers->get(i);
				layer->toXml(b);
			}
			b->decIndent();
		}
		b->addEndTag(EL_LOOP);
	}
}

void ProjectLoop::parseXml(XmlElement* e)
{
	mActive = e->getBoolAttribute(ATT_ACTIVE);
	mFrame = e->getIntAttribute(ATT_FRAME);
	
	for (XmlElement* child = e->getChildElement() ; child != NULL ; 
		 child = child->getNextElement()) {

		add(new ProjectLayer(child));
	}
}

/****************************************************************************
 *                                                                          *
 *   							PROJECT TRACK                               *
 *                                                                          *
 ****************************************************************************/

PUBLIC ProjectTrack::ProjectTrack()
{
	init();
}

PUBLIC ProjectTrack::ProjectTrack(XmlElement* e)
{
	init();
	parseXml(e);
}

PUBLIC ProjectTrack::ProjectTrack(MobiusConfig* config, Project* p, Track* t)
{
	int i;

	init();

    mGroup = t->getGroup();
	mFocusLock = t->isFocusLock();
	mInputLevel = t->getInputLevel();
	mOutputLevel = t->getOutputLevel();
	mFeedback = t->getFeedback();
	mAltFeedback = t->getAltFeedback();
	mPan = t->getPan();
	
	//@C #001 Fix issue about Track reverse wrong saved and load form project 08/04/2023
	mReverse =  t->getState()->reverse;  //@C 

    mSpeedOctave = t->getSpeedOctave();
    mSpeedStep = t->getSpeedStep();
    mSpeedBend = t->getSpeedBend();
    mSpeedToggle = t->getSpeedToggle();
    mPitchOctave = t->getPitchOctave();
    mPitchStep = t->getPitchStep();
    mPitchBend = t->getPitchBend();
    mTimeStretch = t->getTimeStretch();

    // include preset only if different than the setup
    Setup* setup = config->getCurrentSetup();
    SetupTrack* st = setup->getTrack(t->getRawNumber());

    Preset* pre = t->getPreset();
	if (pre != NULL) {
        const char* dflt = (st != NULL) ? st->getPreset() : NULL;
        if (dflt == NULL || !StringEqual(dflt, pre->getName()))
          setPreset(pre->getName());
    }

	// suppress emitting XML for empty loops at the end
	int last = t->getLoopCount();
	for (i = last - 1 ; i >= 0 ; i--) {
		if (t->getLoop(i)->isEmpty())
		  last--;
	}

	for (i = 0 ; i < last ; i++) {
		Loop* l = t->getLoop(i);
		ProjectLoop* pl = new ProjectLoop(config, p, l);
		if (l == t->getLoop())
		  pl->setActive(true);
		add(pl);
	}
	
}

PUBLIC void ProjectTrack::init()
{
    mNumber = 0;
	mActive = false;
    mGroup = 0;
	mFocusLock = false;
	mInputLevel = 127;
	mOutputLevel = 127;
	mFeedback = 127;
	mAltFeedback = 127;
	mPan = 64;
	mReverse = false;  //@C init mReverse to false #001
    mSpeedOctave = 0;
    mSpeedStep = 0;
    mSpeedBend = 0;
    mSpeedToggle = 0;
    mPitchOctave = 0;
    mPitchStep = 0;
    mPitchBend = 0;
    mTimeStretch = 0;
	mPreset = NULL;
	mLoops = NULL;
	mVariables = NULL;
}

PUBLIC ProjectTrack::~ProjectTrack()
{
	delete mPreset;
	delete mVariables;

	if (mLoops != NULL) {
		for (int i = 0 ; i < mLoops->size() ; i++) {
			ProjectLoop* l = (ProjectLoop*)mLoops->get(i);
			delete l;
		}
		delete mLoops;
	}
}

PUBLIC void ProjectTrack::setNumber(int number)
{
	mNumber = number;
}

PUBLIC int ProjectTrack::getNumber()
{
	return mNumber;
}

PUBLIC void ProjectTrack::setActive(bool b)
{
	mActive = b;
}

PUBLIC bool ProjectTrack::isActive()
{
	return mActive;
}

PUBLIC int ProjectTrack::getGroup()
{
    return mGroup;
}

PUBLIC void ProjectTrack::setGroup(int i)
{
    mGroup = i;
}

PUBLIC void ProjectTrack::setPreset(const char* p)
{
	delete mPreset;
	mPreset = CopyString(p);
}

PUBLIC const char* ProjectTrack::getPreset()
{
	return mPreset;
}

PUBLIC void ProjectTrack::setFeedback(int i)
{
	mFeedback = i;
}

PUBLIC int ProjectTrack::getFeedback()
{
	return mFeedback;
}

PUBLIC void ProjectTrack::setAltFeedback(int i)
{
	mAltFeedback = i;
}

PUBLIC int ProjectTrack::getAltFeedback()
{
	return mAltFeedback;
}

PUBLIC void ProjectTrack::setOutputLevel(int i)
{
	mOutputLevel = i;
}

PUBLIC int ProjectTrack::getOutputLevel()
{
	return mOutputLevel;
}

PUBLIC void ProjectTrack::setInputLevel(int i)
{
	mInputLevel = i;
}

PUBLIC int ProjectTrack::getInputLevel()
{
	return mInputLevel;
}

PUBLIC void ProjectTrack::setPan(int i)
{
	mPan = i;
}

PUBLIC int ProjectTrack::getPan()
{
	return mPan;
}

PUBLIC void ProjectTrack::setReverse(bool b)
{
	mReverse = b;
}

PUBLIC bool ProjectTrack::isReverse()
{
	return mReverse;
}

PUBLIC void ProjectTrack::setSpeedOctave(int i)
{
	mSpeedOctave = i;
}

PUBLIC int ProjectTrack::getSpeedOctave()
{
	return mSpeedOctave;
}

PUBLIC void ProjectTrack::setSpeedStep(int i)
{
	mSpeedStep = i;
}

PUBLIC int ProjectTrack::getSpeedStep()
{
	return mSpeedStep;
}

PUBLIC void ProjectTrack::setSpeedBend(int i)
{
	mSpeedBend = i;
}

PUBLIC int ProjectTrack::getSpeedBend()
{
	return mSpeedBend;
}

PUBLIC void ProjectTrack::setSpeedToggle(int i)
{
	mSpeedToggle = i;
}

PUBLIC int ProjectTrack::getSpeedToggle()
{
	return mSpeedToggle;
}

PUBLIC void ProjectTrack::setPitchOctave(int i)
{
	mPitchOctave = i;
}

PUBLIC int ProjectTrack::getPitchOctave()
{
	return mPitchOctave;
}

PUBLIC void ProjectTrack::setPitchStep(int i)
{
	mPitchStep = i;
}

PUBLIC int ProjectTrack::getPitchStep()
{
	return mPitchStep;
}

PUBLIC void ProjectTrack::setPitchBend(int i)
{
	mPitchBend = i;
}

PUBLIC int ProjectTrack::getPitchBend()
{
	return mPitchBend;
}

PUBLIC void ProjectTrack::setTimeStretch(int i)
{
	mTimeStretch = i;
}

PUBLIC int ProjectTrack::getTimeStretch()
{
	return mTimeStretch;
}

PUBLIC void ProjectTrack::setFocusLock(bool b)
{
	mFocusLock = b;
}

PUBLIC bool ProjectTrack::isFocusLock()
{
	return mFocusLock;
}

PUBLIC void ProjectTrack::add(ProjectLoop* l)
{
	if (mLoops == NULL)
	  mLoops = new List();
	mLoops->add(l);
}

PUBLIC List* ProjectTrack::getLoops()
{
	return mLoops;
}

PUBLIC void ProjectTrack::setVariable(const char* name, ExValue* value)
{
	if (name != NULL) {
		if (mVariables == NULL)
		  mVariables = new UserVariables();
		mVariables->set(name, value);
	}
}

PUBLIC void ProjectTrack::getVariable(const char* name, ExValue* value)
{
	value->setNull();
	if (mVariables != NULL)
	  mVariables->get(name, value);
}

void ProjectTrack::writeAudio(const char* baseName, int tracknum)
{
	if (mLoops != NULL) {
		for (int i = 0 ; i < mLoops->size() ; i++) {
			ProjectLoop* loop = (ProjectLoop*)mLoops->get(i);
			loop->writeAudio(baseName, tracknum, i + 1);
		}
	}
}

Layer* ProjectTrack::findLayer(int id)
{
	Layer* found = NULL;
	if (mLoops != NULL) {
		for (int i = 0 ; i < mLoops->size() && found == NULL ; i++) {
			ProjectLoop* l = (ProjectLoop*)mLoops->get(i);
			found = l->findLayer(id);
		}
	}
	return found;
}

void ProjectTrack::allocLayers(LayerPool* pool)
{
	if (mLoops != NULL) {
		for (int i = 0 ; i < mLoops->size() ; i++) {
			ProjectLoop* l = (ProjectLoop*)mLoops->get(i);
			l->allocLayers(pool);
		}
	}
}

void ProjectTrack::resolveLayers(Project* p)
{
	if (mLoops != NULL) {
		for (int i = 0 ; i < mLoops->size() ; i++) {
			ProjectLoop* l = (ProjectLoop*)mLoops->get(i);
			l->resolveLayers(p);
		}
	}
}

void ProjectTrack::toXml(XmlBuffer* b)
{
	toXml(b, false);
}

void ProjectTrack::toXml(XmlBuffer* b, bool isTemplate)
{
	b->addOpenStartTag(EL_TRACK);

    b->addAttribute(ATT_ACTIVE, mActive);
	b->addAttribute(ATT_PRESET, mPreset);

    if (mGroup > 0)
      b->addAttribute(ATT_GROUP, mGroup);
    b->addAttribute(ATT_FOCUS_LOCK, mFocusLock);

    b->addAttribute(ATT_INPUT, mInputLevel);
    b->addAttribute(ATT_OUTPUT, mOutputLevel);
    b->addAttribute(ATT_FEEDBACK, mFeedback);
    b->addAttribute(ATT_ALT_FEEDBACK, mAltFeedback);
    b->addAttribute(ATT_PAN, mPan);

	//@C debug #001
	Trace(3, "[CasDebug]:toXML* ATT_REVERSE! is : %s", mReverse ? "true" : "false");
    b->addAttribute(ATT_REVERSE, mReverse);
    
	b->addAttribute(ATT_SPEED_OCTAVE, mSpeedOctave);
    b->addAttribute(ATT_SPEED_STEP, mSpeedStep);
    b->addAttribute(ATT_SPEED_BEND, mSpeedBend);
    b->addAttribute(ATT_SPEED_TOGGLE, mSpeedToggle);
    b->addAttribute(ATT_PITCH_OCTAVE, mPitchOctave);
    b->addAttribute(ATT_PITCH_STEP, mPitchStep);
    b->addAttribute(ATT_PITCH_BEND, mPitchBend);
    b->addAttribute(ATT_TIME_STRETCH, mTimeStretch);


	if (mLoops == NULL && mVariables == NULL)
	  b->add("/>\n");
	else {
		b->add(">\n");
		b->incIndent();

		if (!isTemplate) {
			if (mLoops != NULL) {
				for (int i = 0 ; i < mLoops->size() ; i++) {
					ProjectLoop* loop = (ProjectLoop*)mLoops->get(i);
					loop->toXml(b);
				}
			}
		}

		if (mVariables != NULL)
		  mVariables->toXml(b);

		b->decIndent();
		b->addEndTag(EL_TRACK);
	}
}

void ProjectTrack::parseXml(XmlElement* e)
{
	setActive(e->getBoolAttribute(ATT_ACTIVE));
	setPreset(e->getAttribute(ATT_PRESET));
    setGroup(e->getIntAttribute(ATT_GROUP));
	setFocusLock(e->getBoolAttribute(ATT_FOCUS_LOCK));
	setInputLevel(e->getIntAttribute(ATT_INPUT));
	setOutputLevel(e->getIntAttribute(ATT_OUTPUT));
	setFeedback(e->getIntAttribute(ATT_FEEDBACK));
	setAltFeedback(e->getIntAttribute(ATT_ALT_FEEDBACK));
	setPan(e->getIntAttribute(ATT_PAN));

	//@C #001 cas : here miss some set? or the next rows of code load "reverse"?
	Trace(3, "[CasDebug]:parseXML* ATT_REVERSE! is : %s", e->getBoolAttribute(ATT_REVERSE) ? "true" : "false");
	setReverse(e->getBoolAttribute(ATT_REVERSE));  //08/04/2023 //@C <-- now load correctly but still save issue -> toXml mReverse is true...
	


	//@C read all child in xml, they can be UserVariables or Loop
	for (XmlElement* child = e->getChildElement() ; child != NULL ; 
		 child = child->getNextElement()) {

		if (child->isName(EL_VARIABLES)) {
			delete mVariables;
			mVariables = new UserVariables(child);
		}
		else
		{
			// cas: if is not a Variable Name is a loop, so load loop?
		  add(new ProjectLoop(child));
		}
	}
}

/****************************************************************************
 *                                                                          *
 *   							   PROJECT                                  *
 *                                                                          *
 ****************************************************************************/

PUBLIC Project::Project()
{
	init();
}

PUBLIC Project::Project(XmlElement* e)
{
	init();
	parseXml(e);
}

PUBLIC Project::Project(const char* file)
{
	init();
    setPath(file);
}

/**
 * Convenience method that builds the project hierarchy around
 * a single loop layer.  Used when you want to load .wav files
 * one at a time.
 *
 * Track and loop number are both relative to zero.
 */
PUBLIC Project::Project(Audio* a, int trackNumber, int loopNumber)
{
	init();

	ProjectTrack* track = new ProjectTrack();
	ProjectLoop* loop = new ProjectLoop();
    ProjectLayer* layer = new ProjectLayer(a);
    
    track->setNumber(trackNumber);
    loop->setNumber(loopNumber);

	loop->add(layer);
	track->add(loop);
	add(track);

    // this must be on
    mIncremental = true;
}

PRIVATE void Project::init()
{
	mNumber = 0;
	mPath = NULL;
	mTracks = NULL;
	mVariables = NULL;
	mBindings = NULL;
	mSetup = NULL;

	mLayerIds = 0;
	mError = false;
	strcpy(mMessage, "");
	mIncremental = false;
    mIncludeAudio = true;

	mFile = NULL;
	strcpy(mBuffer, "");
	mTokens = NULL;
	mNumTokens = 0;

	mFinished = false;
}

PUBLIC Project::~Project()
{
	clear();
}

PUBLIC void Project::clear()
{
	if (mTracks != NULL) {
		for (int i = 0 ; i < mTracks->size() ; i++) {
			ProjectTrack* l = (ProjectTrack*)mTracks->get(i);
			delete l;
		}
		delete mTracks;
		mTracks = NULL;
	}
	delete mVariables;
	mVariables = NULL;
	delete mBindings;
	mBindings = NULL;
	delete mSetup;
	mSetup = NULL;
}

PUBLIC void Project::setNumber(int i)
{
	mNumber = i;
}

PUBLIC int Project::getNumber()
{
	return mNumber;
}

PUBLIC int Project::getNextLayerId()
{
	return mLayerIds++;
}

Layer* Project::findLayer(int id)
{
	Layer* found = NULL;
	if (mTracks != NULL) {
		for (int i = 0 ; i < mTracks->size() && found == NULL ; i++) {
			ProjectTrack* t = (ProjectTrack*)mTracks->get(i);
			found = t->findLayer(id);
		}
	}
	return found;
}

PUBLIC void Project::setBindings(const char* name)
{
	delete mBindings;
	mBindings = CopyString(name);
}

PUBLIC const char* Project::getBindings()
{
	return mBindings;
}

PUBLIC void Project::setSetup(const char* name)
{
	delete mSetup;
	mSetup = CopyString(name);
}

PUBLIC const char* Project::getSetup()
{
	return mSetup;
}

PUBLIC void Project::setVariable(const char* name, ExValue* value)
{
	if (name != NULL) {
		if (mVariables == NULL)
		  mVariables = new UserVariables();
		mVariables->set(name, value);
	}
}

PUBLIC void Project::getVariable(const char* name, ExValue* value)
{
	value->setNull();
	if (mVariables != NULL)
	  mVariables->get(name, value);
}

PUBLIC void Project::setTracks(Mobius* m)
{
	int i;

	// todo: Project needs to support a lot of save options
	MobiusConfig* config = m->getConfiguration();


	int last = m->getTrackCount();

	// suppress empty tracks at the end (unless they're using
	// a different preset
	// NO, these can different preset and other settings that 
	// are useful to preserve
	//for (i = last - 1 ; i >= 0 ; i--) {
	//if (m->getTrack(i)->isEmpty())
	//last--;
	//}

	for (i = 0 ; i < last ; i++) {
		Track* t = m->getTrack(i);
		ProjectTrack* pt = new ProjectTrack(config, this, t);
		if (t == m->getTrack())
		  pt->setActive(true);
		add(pt);
	}
}

PUBLIC void Project::setPath(const char* path)
{
	delete mPath;
	mPath = CopyString(path);
}

PUBLIC const char* Project::getPath()
{
	return mPath;
}

PUBLIC bool Project::isError()
{
	return mError;
}

PUBLIC const char* Project::getErrorMessage()
{
	return mMessage;
}

PUBLIC void Project::setErrorMessage(const char* msg)
{
	if (msg == NULL)
	  strcpy(mMessage, "");
	else
	  strcpy(mMessage, msg);
	mError = true;
}

PUBLIC void Project::add(ProjectTrack* t)
{
	if (mTracks == NULL)
	  mTracks = new List();
	mTracks->add(t);
}

PUBLIC List* Project::getTracks()
{
	return mTracks;
}

PUBLIC void Project::setIncremental(bool b)
{
	mIncremental = b;
}

PUBLIC bool Project::isIncremental()
{
	return mIncremental;
}

PUBLIC void Project::setFinished(bool b)
{
	mFinished = b;
}

PUBLIC bool Project::isFinished()
{
	return mFinished;
}

/**
 * Delete all of the external layer files associated with 
 * this project.  This is called prior to saving a project so 
 * we make sure to clean out old layer files that are no longer
 * relevant to the project.
 *
 * In case the project was hand written and included references
 * to files outside the project directory, ignore those.
 *
 * !! Don't see the logic to protected external files
 */
PUBLIC void Project::deleteAudioFiles()
{
    if (mTracks != NULL) {
        for (int i = 0 ; i < mTracks->size() ; i++) {
            ProjectTrack* track = (ProjectTrack*)mTracks->get(i);
            List* loops = track->getLoops();
            if (loops != NULL) {
                for (int j = 0 ; j < loops->size() ; j++) {
                    ProjectLoop* loop = (ProjectLoop*)loops->get(j);
                    List* layers = loop->getLayers();
                    if (layers != NULL) {
                        for (int k = 0 ; k < layers->size() ; k++) {
                            ProjectLayer* layer = (ProjectLayer*)layers->get(k);
                            const char* path = layer->getPath();
                            if (path != NULL && !layer->isProtected()) {
                                FILE* fp = fopen(path, "r");
                                if (fp != NULL) {
                                    fclose(fp);
                                    remove(path);
                                }
                            }
                            path = layer->getOverdubPath();
                            if (path != NULL) {
                                FILE* fp = fopen(path, "r");
                                if (fp != NULL) {
                                    fclose(fp);
                                    remove(path);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

/**
 * Traverse the hierarchy to instantiate Layer and Segment objects and
 * resolve references between them.
 */
void Project::resolveLayers(LayerPool* pool)
{
	int i;
	if (mTracks != NULL) {
		for (i = 0 ; i < mTracks->size() ; i++) {
			ProjectTrack* t = (ProjectTrack*)mTracks->get(i);
			t->allocLayers(pool);
		}
		for (i = 0 ; i < mTracks->size() ; i++) {
			ProjectTrack* t = (ProjectTrack*)mTracks->get(i);
			t->resolveLayers(this);
		}
	}
}

void Project::toXml(XmlBuffer* b)
{
	toXml(b, false);
}

void Project::toXml(XmlBuffer* b, bool isTemplate)
{
	b->addOpenStartTag(EL_PROJECT);
	b->addAttribute(ATT_NUMBER, mNumber);
	b->addAttribute(ATT_BINDINGS, mBindings);
	b->addAttribute(ATT_SETUP, mSetup);
	b->addAttribute(ATT_AUDIO, mPath);

	if (mTracks == NULL && mVariables == NULL)
	  b->add("/>\n");
	else {
		b->add(">\n");
		b->incIndent();

		if (mTracks != NULL) {
			for (int i = 0 ; i < mTracks->size() ; i++) {
				ProjectTrack* track = (ProjectTrack*)mTracks->get(i);
				track->toXml(b, isTemplate);
			}
		}

		if (mVariables != NULL)
		  mVariables->toXml(b);

		b->decIndent();
		b->addEndTag(EL_PROJECT);
	}
}

void Project::parseXml(XmlElement* e)
{
	setNumber(e->getIntAttribute(ATT_NUMBER));
	setPath(e->getAttribute(ATT_AUDIO));

    // recognize the old MidiConfig name, the MidiConfigs will
    // have been upgraded to BindingConfigs by now
    const char* bindings = e->getAttribute(ATT_BINDINGS);
    if (bindings == NULL) 
      bindings = e->getAttribute(ATT_MIDI_CONFIG);
	setBindings(bindings);

	//@C #002 | set Track Setup parsed from project file  14/04/2023
	Trace(3, "[CasDebug]Project::parseXml ,setSetup(%s);", e->getAttribute(ATT_SETUP));
	setSetup(e->getAttribute(ATT_SETUP));
	
	//C BUG: setup is correctly set, but not in mobiusConfig and in Window!! 18/06/2023
	

	for (XmlElement* child = e->getChildElement() ; child != NULL ; 
		 child = child->getNextElement()) {

		if (child->isName(EL_VARIABLES)) {
			delete mVariables;
			mVariables = new UserVariables(child);
		}
		else
		  add(new ProjectTrack(child));
	}
}

/****************************************************************************
 *                                                                          *
 *   							   FILE IO                                  *
 *                                                                          *
 ****************************************************************************/

/**
 * Read the project structure but no audio files.
 */
PUBLIC void Project::read()
{
    if (mPath != NULL)
      read(NULL, mPath);
}

PUBLIC void Project::read(AudioPool* pool)
{
    if (mPath != NULL)
      read(pool, mPath);
}

PRIVATE void Project::read(AudioPool* pool, const char* file)
{
	char path[1024];

	mError = false;
	strcpy(mMessage, "");

	if (strchr(file, '.') != NULL)
	  strcpy(path, file);
	else {
		// auto extend
		sprintf(path, "%s.mob", file);
	}

	FILE* fp = fopen(path, "r");
	if (fp == NULL) {
		sprintf(mMessage, "Unable to open file %s\n", path);
		mError = true;
	}
	else {
		fclose(fp);
        
        XomParser* p = new XomParser();
        XmlDocument* d = p->parseFile(path);
        if (d != NULL) {
            XmlElement* e = d->getChildElement();
            if (e != NULL) {
                clear();
                parseXml(e);
            }
            delete d;
        }
        else {
            // there was a syntax error in the file
            sprintf(mMessage, "Unable to read file %s: %s\n", 
                    path, p->getError());
            mError = true;
        }
        delete p;

        readAudio(pool);
    }
}

/**
 * After reading the Project structure from XML, traverse the hierarhcy
 * and load any referenced Audio files.
 */
PRIVATE void Project::readAudio(AudioPool* pool)
{
    if (pool != NULL && mTracks != NULL) {
        for (int i = 0 ; i < mTracks->size() ; i++) {
            ProjectTrack* track = (ProjectTrack*)mTracks->get(i);
            List* loops = track->getLoops();
            if (loops != NULL) {
                for (int j = 0 ; j < loops->size() ; j++) {
                    ProjectLoop* loop = (ProjectLoop*)loops->get(j);
                    List* layers = loop->getLayers();
                    if (layers != NULL) {
                        for (int k = 0 ; k < layers->size() ; k++) {
                            ProjectLayer* layer = (ProjectLayer*)layers->get(k);
                            const char* path = layer->getPath();
                            if (path != NULL)
							  layer->setAudio(pool->newAudio(path));
                            path = layer->getOverdubPath();
                            if (path != NULL)
							  layer->setOverdub(pool->newAudio(path));
                        }
                    }
                }
            }
        }
    }
}

PUBLIC void Project::write()
{
    if (mPath != NULL)
      write(mPath, false);
}

PUBLIC void Project::write(const char* file, bool isTemplate)
{
	char path[1024];
	char baseName[1024];

	mError = false;
	strcpy(mMessage, "");

	if (EndsWithNoCase(file, ".mob"))
	  strcpy(path, file);
	else
	  sprintf(path, "%s.mob", file);

	// calculate the base file name to be used for Audio files
	strcpy(baseName, path);
	int psn = LastIndexOf(baseName, ".");
	if (psn > 0)
	  baseName[psn] = 0;

	// clean up existing Audio files
    FILE* fp = fopen(path, "r");
    if (fp != NULL) {
        fclose(fp);
        Project* existing = new Project(path);
        existing->deleteAudioFiles();
        delete existing;
    }

	// write the new project and Audio files
	fp = fopen(path, "w");
	if (fp == NULL) {
		sprintf(mMessage, "Unable to open output file: %s\n", path);
		mError = true;
	}
	else {
		fclose(fp);

		// first write Audio files and assign Layer paths
        if (!isTemplate)
          writeAudio(baseName);
			
        // then write the XML directory
        XmlBuffer* b = new XmlBuffer();
        toXml(b, isTemplate);
        WriteFile(path, b->getString());
        delete b;
	}
}

void Project::writeAudio(const char* baseName)
{
	if (mTracks != NULL) {
		for (int i = 0 ; i < mTracks->size() ; i++) {
			ProjectTrack* track = (ProjectTrack*)mTracks->get(i);
			track->writeAudio(baseName, i + 1);
		}
	}
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
