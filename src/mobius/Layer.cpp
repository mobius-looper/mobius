/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * A model for a segment of audio.  The Loop class  deals with Layers, 
 * Layers in turn manage the actual audio memory model.
 *
 * MEMORY MODEL
 * 
 * In the simplest case, the Layer contains an Audio object that completely
 * defines its content.  In complex cases, a Layer will also reference
 * one or more "backing" layers that are combined with any local Audio
 * at runtime.   Backing layers are referenced through Segment objects,
 * a layer may have more than one segment, and a segment may reference
 * any portion of the backing layer.  
 *
 * Originally, Segments were used to to merge the entire layer list
 * at runtime, with feedback also applied at runtime.  This can result in 
 * effecient memory allocation since we do not have to copy the entire
 * contents of one layer to another, but it increases CPU demands as the
 * layer list grows.  After about 20 layers we often would miss interrupts
 * and cause clicks.
 *
 * Applying feedback to the entire layer at runtime also meant that we
 * could not record continuous feedback changes into the layer, the
 * feedback applied would be whatever the last value was when the layer
 * was shifted.
 *
 * To avoid runtime merging of the layers, we have to incrementally
 * "flatten" them as we record a new layer, which also allows us to apply
 * continuous feedback changes.  Segments are still used as a temporary
 * model for what the backing content since it will take too long to
 * stop and copy the entire layer when it is shifted.  Copying the backing
 * content happens incrementally as the layer is recorded.  Once recording
 * of the layer has completed, the backing layer will have been copied
 * to a local Audio object and the Segments are no longer necessary.
 *
 * An interesting problem arises when you perform a retriggering operation 
 * in the middle of the new layer.  Since copying the backing layer happens
 * incrementally as we record, we will not have completed the copy before 
 * we have to switch to a different loop.  For example:
 *
 *   1) record an initial layer
 *   2) begin overdubbing into a second layer
 *   3) perform an unquantized switch to another loop
 *   4) switch back to the original loop
 *
 * In step 3 we have to leave the current layer before we have made
 * a complete record/copy pass.  When we return in step 4, we have
 * to remember where the copy ended so we know whether we can resume
 * playback from the local Audio or whether we have to use the Segments.
 * 
 * A question I have is what feedback level is applied to that portion
 * of the layer that we did not copy in realtime?  
 * 
 *    1) the feedback level in effect when we left the layer
 *    2) the current feedback level
 *    3) 100%
 *
 * It seems to make the most sense to use 1, behaving as if the remainder
 * of the layer was instantaly copied at the feedback level at that time.
 * Number 2 could produce a sharp jump in level if you didn't remember
 * to return the feedback control to the previous position when you returned.
 *
 * A similar problem happens when we return to the loop in step 4.
 * If SwitchLocation=Restore, we're supposed to pick up where we left off.
 * But the layer we were recording has now been shifted and we begin
 * recording into a new layer in the middle.  What feedback should
 * be applied to the first half of the layer that we didn't actually
 * record over?  
 *
 * Here Number 2, the current feedback level makes sense.  It's as if
 * we had instantly copied the first half of the layer before beginning
 * to record new material in the middle.  The feedback levels should be
 * the same to avoid a sharp jump.
 * 
 * So far, all we need to maintain is a pair of "watermarks" for the
 * beginning and end of the of the region that we have copied
 * into the local Audio.  When playing in this region everything
 * we need is in the local Audio and we can ignore Segments.  Outside
 * this region we still use segments.
 * 
 * An uglier problems arises if we allow recording into the layer
 * to be non-contiguous, meaning we can recorded for awhile, then 
 * jump ahead and record into another section of the layer with a gap
 * in between.  In that case we cannot use a simple watermark, we would
 * have to continually adjust the Segments so that they do not occlude
 * the local Audio, then merge the Segments with the Audio during playback
 * rather than preferring one or the other.  If we can perform a seamless
 * recording from beginning to end, all of the segments would collapse
 * to nothing and be removed.  
 *
 * While not particularly difficult to do, that problem only arises if we
 * allow a loop switch or restart to return to the partially recorded
 * layer without shifting it.  If we shift it, we'll start over with
 * a new layer that references the previous layer with a Segment, and we'll
 * begin a new contiguous recording in the middle.  
 *
 * The work involved to maintain a pair of watermarks that occlude 
 * the Segments is not that much less difficult than just continuously
 * adjusting the segments so that they don't occlude the Audio.  On playback
 * we can then simply merge the Audio and Segments if they exist.  This
 * is the most flexible solution and may be useful for other things later.
 *
 * CURSORS
 *
 * A cursor maintains cached buffer locations inside an Audio object.
 * Each layer contains several cursors:
 *
 *   Play Cursor
 *     Used when retrieving the layer content for playback.
 *     If layer flattening is off, this will be used all the time.
 *     If flattening is on, this will never be used after the layer
 *     has shifted.
 * 
 *   CopyCursor
 *     Used when copying the layer content into the next layer.
 *     Used only during flattening of the next to last layer.
 *     Once the copy has been performed, this cursor is no longer used.
 * 
 *   RecordCursor
 *     Used during the initial recording of a layer.  Most of the time
 *     The Record Cursor and the Play Cursor will not be active at the
 *     same time, but there is a small window at the end where we begin
 *     preplay of the record layer due to latency when they will both
 *     be active.  Once the layer has shifted this is no longer used.
 *
 *   FeedbackCursor
 *     When layer flattening is on, maintains the position in the
 *     current recording layer where the content from the previous layer
 *     is copied.  Will have the same ending position as RecordCursor, 
 *     but they advance at different times.
 *
 * ISOLATED OVERDUBS
 *
 * One thing that was nice about the original memory model was that
 * the local layer Audio contained only new content that was overdubbed
 * over the the backing layer.  These could be saved as individual files
 * in the project and then mixed together creatively, effectively allowing
 * you to randomze the over of the overdubs.
 *
 * In the new model local Audio contains both the new overdubs and a copy
 * of the backing layer.  To support the old behavior, we allow an option
 * to be set to also maintain a second Audio object that has just
 * the new material.  This will up to twice the amount of memory so
 * it should be used with care.
 * 
 * FEEDBACK SMOOTHING
 *
 * Track uses ContinuousController objects to smooth out changes
 * to the various levels.  But within a layer there is an additional
 * smoothing problem that occurs whenever you enter or leave a mode
 * that uses the secondary feedback level.  In the extreme case, feedback
 * is 127, secondary feedback is 0, and the result is similar to Replace
 * mode where none of the backing layer is copied.  Simply jumping from 127
 * to zero will cause a "cliff" in the waveform which will be heard
 * as a click.
 *
 * We need to perform a gradual change in feedback, similar to what
 * ContinuousController does, but more rapidly.  The result is quite similar
 * to a fade but the curve will be linear rather than exponential.  I'd
 * like to see if level changes can be integrated into the fade logic but
 * it was more complicated than I wanted to tackle.  Instead, we'll maintain
 * the last feedback level that was applied to this layer.  On the next
 * advance, we'll compare the new feedback level to the last one and if
 * different enter a loop where the backing layer is copied in short sections
 * rather than all at once, with the feedback changing slightly for each
 * section.  
 *
 * It seems like we should be able to generalize level smoothing into
 * something that the Stream manages like it does fades.  But the current
 * approach sounds ok and is relatively simple.
 *
 * It may also be nice if the duration of the feedback ramp could be 
 * set by the global FadeFrames parameter so that they sound similar.
 *
 * REVERSE HANDLING
 *
 * This is where the rubber meets the road regarding reverse handling.
 * Code above this except for a few places in Loop related to event
 * scheduling are not aware of reverse, all calculations are done "forward".
 * Layer takes the frames passed down from Loop and "reflects" them
 * so that they are in the correct reverse position, and sets the
 * reverse flag in the AudioCursor so the cursor moves backward rather
 * than forward.  There are two reflection concepts:
 *
 * Reflected Frame
 *
 * A reflected frame is what the forward frame number would be if the layer
 * had actually been playing from the end rather than the front.  It is 
 * calculated as:
 * 
 *     reflectedFrame = (layerFrames - forwardFrame - 1);
 *
 * For example in a layer that is 10 frames long, frame 2 would become
 * reflected frame 7 because 7 is the same distance from the end of
 * the layer as 2 is from the front.
 *
 *     ..2....7..
 *
 * A subtlety is that the reflection of the "loop frame" which is
 * one beyond the end of the loop is -1 which is one beyond the front
 * of the loop.  This is handled by AudioCursor and does not affect the
 * calculations here.
 *
 * Reflected Region
 *
 * Most layer operations involve "regions" which is a block of frames
 * of a specified length starting at a specified frame.  Here is a region
 * 4 frames long starting at frame 2.  The end frame of the region is 5.
 *
 *     ..>>>>....
 *
 * In reverse, the entire region must be reflected.  The start frame
 * becomes 7 and the end frame becomes 4.
 *
 *     ....<<<<..
 *
 * For some operations, it is enough just to reflect the start frame,
 * set the AudioCursor to reverse, and move backwards 4 frames without
 * calculating the end frame.  For segment operations though, it is more
 * convenient to deal with a startFrame & length defining a forward region.
 * In those cases, we swap the reflected start and end frames.  In the
 * previous example the start frame becomes 5 and the end frame is 7.
 * This is called a "reflected region".  Methods that take a frame and length
 * need to know if they are dealing with a reflected region so they 
 * do not attemt to reflect the region edges.  The methods must also
 * remember that that the content of the reflected region must be processed in 
 * reverse, even though they are given a normal forward region.
 *
 * Reverse: Play
 *
 * Reflect the startFrame, set AudioCursor to reverse and retrieve from
 * the local Audio.  When processing the segments, reflect the region
 * and pass the region startFrame to Segment.  Segment knows to process
 * the region in reverse.
 * 
 * Reverse: PlayFadeIn
 *
 * This one is funny because we don't actually reflect the fade region.
 * What we're supposed to do is fade in whatever we return from the get()
 * method.  get() will return reflected content, all we need to do is apply
 * a forward fade to that.
 *
 * Reverse: Record
 *
 * Reflect the startFrame, set AudioCursor to reverse and write into
 * the local Audio.
 *
 * Reverse: RecordFadeIn
 * 
 * REVERSE RECORD SUBTLETY
 *
 * When we do normal forward recording, we keep appending
 * frames to the Audio object, the base frame stays the same
 * and the frame counter is incremented.  When we do reverse recording,
 * we prepend frames and shift the base frame down.
 *
 * When we end a recording with Reverse, we call setLoopFrames
 * to set the layer's frame count, then start requesting frames from
 * the end of the layer.  But due to input latency, we won't actually
 * have received all of them yet so Audio's frame counter will be less
 * than the Layer's frame counter.  
 * 
 * For example with a 10000 frame layer, with an output latency of 1000, 
 * begin reverse play at frame 8999.  With an input latency of 100, 
 * Audio will only be 9900 long.  When going from forward to reverse, 
 * this is ok as long as OutputLatency is greater than InputLatency.  
 * If InputLatency is less, we'll start playing in the empty space that
 * hasn't been recorded yet, and our fade in may be finished by the time
 * we slam into the real frames, resulting in a click.  This could be 
 * solved by deferring the onset of the fade, but in practice shouldn't
 * be an issue because OL is always greater than IL.
 *
 * If we're going from reverse to forward, it's more complicated.
 * Normal recording keeps Audio's start frame constant and increments
 * the length.  Reverse recording shifts the start frame lower.  This
 * means that there will never be empty space at the beginning of the loop,
 * there is always a frame zero and it keeps shifting.  This screws up
 * the play location.  For example a 10000 frame layer with IL of 100,
 * Audio will be 99900 long and we want to begin playing at a reflected
 * frame of 1000.  Audio has a frame 1000, but this is actually frame 1100
 * of the fully recorded layer because we're missing 100 frames from the
 * front of the Audio.  We need to start play on Audio's frame 900 instead.
 *
 * A similar issue exists when we start recording in reverse, and stay
 * in reverse.  When the loop length is known, we start preplay at OL
 * which when reflected will be 8999.  But this will be located relative
 * to a base that hasn't been fully shifted yet, the accurate Audio
 * frame would be 8999 - 100 = 8899.  
 *
 * Note that in order to make the compensating calculations, when frames
 * are requested, we have to know which direction the loop is being
 * recorded in, in addition to the play direction.  The matrix is:
 *
 *    record forward, play forward : no adjustment
 *    record forward, play reverse : no adjustment
 *    record reverse, play forward : adjustment
 *    record reverse, play reverse : adjustment
 * 
 * To avoid all of this, once we know the loop frame count, we will
 * modify Audio to have this count so that the Audio and the Layer
 * will be the same size.  How this is done also depends on the record
 * direction.  For forward recording, we simply set the frame count.
 * For reverse recording, we have to shift the base frame.  This works
 * provided that the record direction can't be changed once the
 * loop frames are set.
 *
 */

#include <stdio.h>
#include <memory.h>

#include "Util.h"

#include "Audio.h"
#include "FadeWindow.h"
#include "Layer.h"
#include "Loop.h"
#include "Mobius.h"
#include "MobiusConfig.h"
#include "MobiusState.h"
#include "Mode.h"
#include "Project.h"
#include "Script.h"
#include "Segment.h"
#include "Stream.h"

// In Track.cpp
extern bool TraceFrameAdvance;

/**
 * This has been on for awhile, WTF does this do??
 */
static bool SimulateSegmentReplace = true;

/**
 * We used to trace fade events at 2 but when bend was added
 * they happen all the time so it was raised to 3 to reduce trace
 * clutter.  I temporarily needed to see them though when comparing
 * trace in different verions, so this controls it.
 */
static int FadeTraceLevel = 2;

/****************************************************************************
 *                                                                          *
 *                               CODE COVERAGE                              *
 *                                                                          *
 ****************************************************************************/
/*
 * Crude but adequate code coverage tracking for unit tests of a few
 * sensitive areas.
 */

PUBLIC bool CovFadeLeftBoth = false;
PUBLIC bool CovFadeLeftForegroundRev = false;
PUBLIC bool CovFadeLeftForeground = false;
PUBLIC bool CovFadeLeftBackgroundRev = false;
PUBLIC bool CovFadeLeftBackground = false;
PUBLIC bool CovFadeRightBoth = false;
PUBLIC bool CovFadeRightForegroundRev = false;
PUBLIC bool CovFadeRightForeground = false;
PUBLIC bool CovFadeRightBackgroundRev = false;
PUBLIC bool CovFadeRightBackground = false;
PUBLIC bool CovFadeOutCrossing = false;
PUBLIC bool CovFadeOutHeadOverlap = false;
PUBLIC bool CovFadeOutPrev = false;
PUBLIC bool CovFinalizeFadeHead = false;
PUBLIC bool CovFinalizeRaiseBackgroundHead = false;
PUBLIC bool CovFinalizeFadeBackgroundHead = false;
PUBLIC bool CovFinalizeLowerBackgroundHead = false;

PUBLIC void Layer::initCoverage()
{
    CovFadeLeftBoth = false;
    CovFadeLeftForegroundRev = false;
    CovFadeLeftForeground = false;
    CovFadeLeftBackgroundRev = false;
    CovFadeLeftBackground = false;
    CovFadeRightBoth = false;
    CovFadeRightForegroundRev = false;
    CovFadeRightForeground = false;
    CovFadeRightBackgroundRev = false;
    CovFadeRightBackground = false;
    CovFadeOutCrossing = false;
    CovFadeOutHeadOverlap = false;
    CovFadeOutPrev = false;
    CovFinalizeFadeHead = false;
    CovFinalizeRaiseBackgroundHead = false;
    CovFinalizeFadeBackgroundHead = false;
    CovFinalizeLowerBackgroundHead = false;
}

PUBLIC void Layer::showCoverage()
{
    printf("Layer coverage gaps:\n");

    if (!CovFadeLeftBoth)
      printf("  CovFadeLeftBoth\n");
    if (!CovFadeLeftForegroundRev)
      printf("  CovFadeLeftForegroundRev\n");
    if (!CovFadeLeftForeground)
      printf("  CovFadeLeftForeground\n");
    if (!CovFadeLeftBackgroundRev)
      printf("  CovFadeLeftBackgroundRev\n");
    if (!CovFadeLeftBackground)
      printf("  CovFadeLeftBackground\n");
    if (!CovFadeRightBoth)
      printf("  CovFadeRightBoth\n");
    if (!CovFadeRightForegroundRev)
      printf("  CovFadeRightForegroundRev\n");
    if (!CovFadeRightForeground)
      printf("  CovFadeRightForeground\n");
    if (!CovFadeRightBackgroundRev)
      printf("  CovFadeRightBackgroundRev\n");
    if (!CovFadeRightBackground)
      printf("  CovFadeRightBackground\n");
    if (!CovFadeOutCrossing)
      printf("  CovFadeOutCrossing\n");
    if (!CovFadeOutHeadOverlap)
      printf("  CovFadeOutHeadOverlap\n");
    if (!CovFadeOutPrev)
      printf("  CovFadeOutPrev\n");
    if (!CovFinalizeFadeHead)
      printf("  CovFinalizeFadeHead\n");
    if (!CovFinalizeRaiseBackgroundHead)
      printf("  CovFinalizeRaiseBackgroundHead\n");
    if (!CovFinalizeFadeBackgroundHead)
      printf("  CovFinalizeFadeBackgroundHead\n");
    if (!CovFinalizeLowerBackgroundHead)
      printf("  CovFinalizeLowerBackgroundHead\n");

    fflush(stdout);
}

/****************************************************************************
 *                                                                          *
 *   							LAYER CONTEXT                               *
 *                                                                          *
 ****************************************************************************/

LayerContext::LayerContext()
{
	initAudioBuffer();
    init();
}

void LayerContext::init()
{
    mReverse = false;
	mLevel = 1.0;
}
    
LayerContext::~LayerContext()
{
}

void LayerContext::setReverse(bool b)
{
	mReverse = b;
}

bool LayerContext::isReverse()
{
	return mReverse;
}

void LayerContext::setLevel(float l) 
{
	mLevel = l;
}

float LayerContext::getLevel()
{
	return mLevel;
}

/****************************************************************************
 *                                                                          *
 *                                   LAYER                                  *
 *                                                                          *
 ****************************************************************************/

Layer::Layer(LayerPool* lpool, AudioPool* apool)
{
    mLayerPool = lpool;
    mAudioPool = apool;
	mPooled = false;
	mPrev = NULL;
	mRedo = NULL;
    mNumber = 0;
    mAllocation = 0;
    mReferences = 0;
    mLoop = NULL;
    mSegments = NULL;
    mAudio = apool->newAudio();
    mOverdub = apool->newAudio();
	mFrames = 0;
	mPendingFrames = 0;
	mLastFeedbackFrame = 0;
	mCycles = 1;
	mMax = 0.0f;
    mStartingFeedback = 127;
	mFeedback = 127;
	mStarted = false;
	mRecordable = false;
	mPlayable = false;
	mPaused = false;
	mMuted = false;
	mFinalized = false;
	mAudioChanged = false;
    mStructureChanged = false;
	mFeedbackApplied = false;
	mInserting = false;
	mInsertRemaining = 0;
	mContainsDeferredFadeLeft = false;
	mContainsDeferredFadeRight = false;
	mDeferredFadeLeft = false;
	mDeferredFadeRight = false;
	mReverseRecord = false;
	mIsolatedOverdub = false;
	mNoFlattening = false;
	mFadeOverride = false;
    mHistoryOffset = 0;
    mWindowOffset = -1;
    mWindowSubcycleFrames = 0;
	mCheckpoint = CHECKPOINT_UNSPECIFIED;

	mSmoother = new Smoother();
    mHeadWindow = new FadeWindow();
    mTailWindow = new FadeWindow();

    mPlayCursor = new AudioCursor("play", mAudio);
    mCopyCursor = new AudioCursor("copy", mAudio);
    mFeedbackCursor = new AudioCursor("feedback", mAudio);

    mRecordCursor = new AudioCursor("record", mAudio);
    mRecordCursor->setAutoExtend(true);
    mOverdubCursor = new AudioCursor("new", mAudio);
    mOverdubCursor->setAutoExtend(true);

	mFade.init();
}

/**
 * Delete the layer and any undo layers linked to it.
 * This not usually called, instead return the layer to the pool
 * with the free() method.
 */
Layer::~Layer()
{
	Layer *l, *prev;

    delete mAudio;
	delete mOverdub;
	delete mSmoother;
	delete mHeadWindow;
	delete mTailWindow;
	delete mPlayCursor;
	delete mCopyCursor;
	delete mFeedbackCursor;
	delete mRecordCursor;
	delete mOverdubCursor;

	for (l = mPrev, prev = NULL ; l != NULL ; l = prev) {
		prev = l->mPrev;
		l->mPrev = NULL;
		delete l;
	}
}

/**
 * Make the layer empty.
 * Called when bringing layers out of the pool, or when reusing
 * a squelched layer.
 */
void Layer::reset()
{
	mAudio->reset();
	mOverdub->reset();
    mHeadWindow->reset();
    mTailWindow->reset();

    resetSegments();
	mFrames = 0;
	mPendingFrames = 0;
	mLastFeedbackFrame = 0;
	mCycles = 1;
    mMax = 0.0f;    
	mStarted = false;
	mRecordable = false;
	mPlayable = false;
	mPaused = false;
	mMuted = false;
	mFinalized = false;
	mAudioChanged = false;
	mStructureChanged = false;
	mFeedbackApplied = false;
    mStartingFeedback = 127;
	mFeedback = 127;
	mSmoother->reset();
	mInserting = false;
	mInsertRemaining = 0;
	mContainsDeferredFadeLeft = false;
	mContainsDeferredFadeRight = false;
	mDeferredFadeLeft = false;
	mDeferredFadeRight = false;
	mReverseRecord = false;
    mHistoryOffset = 0;
    mWindowOffset = -1;
    mWindowSubcycleFrames = 0;
	mCheckpoint = CHECKPOINT_UNSPECIFIED;
	mRedo = NULL;
	mFade.init();
}

/**
 * We're a trace context, supply track/loop/time.
 */
PUBLIC void Layer::getTraceContext(int* context, long* time)
{
	if (mLoop != NULL)
	  mLoop->getTraceContext(context, time);
}

/**
 * Free this layer but retain any undo layers linked to it.
 */
void Layer::free()
{
    if (mLayerPool != NULL)
      mLayerPool->freeLayer(this);
    else {
        Trace(1, "Layer::free layer without pool!\n");
        //delete this;
    }
}

/**
 * Free this layer and all undo layers linked to it.
 */
void Layer::freeAll()
{
    if (mLayerPool != NULL)
      mLayerPool->freeLayerList(this);
    else {
        Trace(1, "Layer::freeAll layer without pool!\n");
    }
}

/**
 * Free the undo history of this layer.
 * This is the list of layers linked by the mPrev pointer.
 */
void Layer::freeUndo()
{
    if (mLayerPool != NULL)
      mLayerPool->freeLayerList(mPrev);
    else
      Trace(1, "Layer::freeUndo layer without pool!\n");
	mPrev = NULL;
}

/**
 * Special constructor to transfer the contents of one layer to another
 * without incrementing reference counts.  This is used in cases where
 * we need to maintain the identity of a layer, but we need to wrap
 * the current contents in another layer that can then be referenced
 * one or more times by the original layer.  
 *
 * UPDATE: This was originally used in the implementation of StartPoint
 * but is no longer used.  It works, but since the spawned layer
 * is not on the Loop's layer list, it is not saved in a project,
 * and the project model has no support for "private" layers owned
 * by another layer.  This could be solved, but it seems better
 * to simply do an immediate shift before processing the StartPoint.
 * The effect is the same, and the model is simpler.
 */
Layer* Layer::spawn()
{
	Layer* neu = mLayerPool->newLayer(mLoop);

	// call setAudio first since it resets segments
	neu->setAudio(mAudio);
    neu->setOverdub(mOverdub);
	neu->setSegments(mSegments);

    // ugly
    delete neu->mHeadWindow;
    neu->mHeadWindow = mHeadWindow;
    delete neu->mTailWindow;
    neu->mTailWindow = mTailWindow;

	mSegments = NULL;
    mHeadWindow = new FadeWindow();
    mTailWindow = new FadeWindow();
	mAudio = mAudioPool->newAudio();
    mRecordCursor->setAudio(mAudio);
    mFeedbackCursor->setAudio(mAudio);
    mPlayCursor->setAudio(mAudio);
    mCopyCursor->setAudio(mAudio);
	mOverdub = mAudioPool->newAudio();
    mOverdubCursor->setAudio(mOverdub);

	// Audio frame counter must match ours
	mAudio->setFrames(mFrames);
	mOverdub->setFrames(mFrames);

	// and this represents a fundamental change
	mStructureChanged = true;

	return neu;
}

/****************************************************************************
 *                                                                          *
 *                                 ACCESSORS                                *
 *                                                                          *
 ****************************************************************************/

int Layer::getNumber() 
{
	return mNumber;
}

void Layer::setNumber(int i) 
{
	mNumber = i;
}

int Layer::getAllocation() 
{
	return mAllocation;
}

void Layer::setAllocation(int i) 
{
	mAllocation = i;
}

int Layer::getReferences()
{
    return mReferences;
}

void Layer::incReferences()
{
    mReferences++;
}

int Layer::decReferences()
{
	if (mReferences > 0)
	  mReferences--;
	else {
		printf("Layer::decReferences: invalid reference count %d\n", 
			   mReferences);
	}
    return mReferences;
}

void Layer::setReferences(int i)
{
    mReferences = i;
}

Layer* Layer::getPrev() 
{
	return mPrev;
}

void Layer::setPrev(Layer* l) 
{
	// note that this doesn't increment the reference count, the layer
	// is still "owned" by the Loop 
	mPrev = l;
}
	
Layer* Layer::getRedo()
{
	return mRedo;
}

void Layer::setRedo(Layer* l)
{
	mRedo = l;
}

void Layer::setLoop(Loop* l)
{
	mLoop = l;
}

Loop* Layer::getLoop()
{
	return mLoop;
}

bool Layer::isIsolatedOverdub()
{
	return mIsolatedOverdub;
}

void Layer::setIsolatedOverdub(bool b)
{
	mIsolatedOverdub = b;
}

/**
 * Return true if changes were made to the Audio during recording.
 */
bool Layer::isAudioChanged() 
{
    return mAudioChanged;
}

void Layer::setDeferredFadeLeft(bool b)
{
	mDeferredFadeLeft = b;
}

bool Layer::isDeferredFadeLeft()
{
	return mDeferredFadeLeft;
}

void Layer::setContainsDeferredFadeLeft(bool b)
{
	mContainsDeferredFadeLeft = b;
}

bool Layer::isContainsDeferredFadeLeft()
{
	return mContainsDeferredFadeLeft;
}

bool Layer::hasDeferredFadeLeft()
{
	return mDeferredFadeLeft || mContainsDeferredFadeLeft;
}

void Layer::setDeferredFadeRight(bool b)
{
	mDeferredFadeRight = b;
}

bool Layer::isDeferredFadeRight()
{
	return mDeferredFadeRight;
}

void Layer::setContainsDeferredFadeRight(bool b)
{
	mContainsDeferredFadeRight = b;
}

bool Layer::isContainsDeferredFadeRight()
{
	return mContainsDeferredFadeRight;
}

bool Layer::hasDeferredFadeRight()
{
	return mDeferredFadeRight || mContainsDeferredFadeRight;
}

void Layer::setReverseRecord(bool b)
{
	mReverseRecord = b;
}

bool Layer::isReverseRecord()
{
	return mReverseRecord;
}

bool Layer::isDeferredFadeIn()
{
	return (mReverseRecord) ? mDeferredFadeRight : mDeferredFadeLeft;
}

bool Layer::isDeferredFadeOut()
{
	return (mReverseRecord) ? mDeferredFadeLeft : mDeferredFadeRight;
}

bool Layer::isContainsDeferredFadeIn()
{
	return (mReverseRecord) ? mContainsDeferredFadeRight : mContainsDeferredFadeLeft;
}

bool Layer::isContainsDeferredFadeOut()
{
	return (mReverseRecord) ? mContainsDeferredFadeLeft : mContainsDeferredFadeRight;
}

bool Layer::hasDeferredFadeIn(LayerContext* con)
{
	return ((con->isReverse()) ? hasDeferredFadeRight() : hasDeferredFadeLeft());
}

bool Layer::hasDeferredFadeOut(LayerContext* con)
{
	return ((con->isReverse()) ? hasDeferredFadeLeft() : hasDeferredFadeRight());
}

/**
 * Return true if structural changes were made to the layer
 * such as adding a cycle, or otherwise modifying the Segment list.
 *
 * Loop needs to tell the difference between a structure change
 * and an audio change in order to squelch a record layer with no content.
 * If we make a structure change but don't happen to be recording
 * any audible content, we must still keep the layer.
 */
bool Layer::isStructureChanged()
{
	return mStructureChanged;
}

/**
 * Used by Function implementations that do their own segment processing.
 */
void Layer::setStructureChanged(bool b)
{
	mStructureChanged = b;
}

bool Layer::isChanged()
{
	return (mStructureChanged || mAudioChanged);
}

/**
 * Exposed only for Project.
 * Normally this will be the same as what is in the preset
 * but presets can change and this has to be a persistent
 * part of the reorded layer.
 */
bool Layer::isNoFlattening()
{
    return mNoFlattening;
}

bool Layer::isFinalized()
{
    return mFinalized;
}

void Layer::setFinalized(bool b)
{
    mFinalized = b;
}

float Layer::getMaxSample()
{
	return mMax;
}

/**
 * When layer flattening is turned off, this will return the feedback
 * level being uniformly applied to the backing layer.  When flattening is on
 * it is the feedback first used when playing the layer, but it may
 * change later.  Used by Loop to determine if we need to shift due to
 * feedback level changes.
 */
int Layer::getStartingFeedback()
{
	return mStartingFeedback;
}

int Layer::getFeedback()
{
	return mFeedback;
}

/**
 * The number of frames in the layer.
 * This will be zero during the initial record since a non-zero
 * frame count is used to indiciate we've finished recording.  It may
 * be higher than the number returned by getRecordedFrames when we're
 * finishing up the recording, but waiting for the final InputLatency
 * frames to come in.  We need to return the correct non-zero frame count
 * even though we haven't recieved them all yet.
 *
 * !! I don't like the "zero during initial record" convention, it is
 * confusing.  Loop is in a better position to keep a flag for this.
 * Can't look for a NULL previous layer since we can start new recordings
 * after building up a few layers.
 */
long Layer::getFrames()
{
	return mFrames;
}

/**
 * Return the number of frames actualy recorded locally.
 *
 * This is used with the special Insert layer, and in some tracing
 * and consistency checks.  mFrames may not have been set yet.
 */
long Layer::getRecordedFrames()
{
	return calcFrames() - mPendingFrames;
}

/**
 * Calculate the maximum number of actual frames contained in this layer.
 */
long Layer::calcFrames()
{
	long audioFrames = mAudio->getFrames();
	long segFrames = getSegmentFrames();

	return (audioFrames > segFrames) ? audioFrames : segFrames;
}

/**
 * Calculate the segment frames in this layer. This may not
 * be the same as the Audio frames.
 */
long Layer::getSegmentFrames()
{
	long frames = 0;
    if (mSegments != NULL) {
        for (Segment* seg = mSegments ; seg != NULL ; seg = seg->getNext()) {
            long end = seg->getOffset() + seg->getFrames();
            if (end > frames)
			  frames = end;
        }
    }
	return frames;
}

/**
 * Set the number of frames in the layer.
 * See commentary at the top of Audio.cpp for a subtlety when recording
 * in reverse.
 *
 * If no LayerContext is passed, we may be initializing a new layer,
 * so just assume forward.
 *
 * This should only be called to adjust the size of the local Audios,
 * to reflect a change in segment structure, or to set the final
 * loop size once the initial recording ends, it has no effect
 * on segments.
 */
void Layer::setFrames(LayerContext* con, long frames)
{
	if (con == NULL || !con->isReverse()) {
		mAudio->setFrames(frames);
		mOverdub->setFrames(frames);
	}
	else {
		mAudio->setFramesReverse(frames);
		mOverdub->setFramesReverse(frames);
	}
	
	mFrames = frames;
}

/**
 * Resize the layer based on a predetermined frame count.
 * Typically this is done to resize a layer that has only segments
 * after reorganizing the segments, mAudio and mOverdub are empty.
 */
void Layer::resize(long frames)
{
	mAudio->setFrames(frames);
	mOverdub->setFrames(frames);
	mFrames = frames;
}

void Layer::resizeFromSegments()
{
	resize(getSegmentFrames());
}

/**
 * Set the expected number of loop frames before they've all been added.
 * Used when ending the initial record.
 *
 * This is used only for a consistency check in Loop::record, we
 * could eliminate the complexity?
 *
 * During recording mFrames will be zero, and Audio will have been
 * accumulating frames.
 */
void Layer::setPendingFrames(LayerContext* con, long frames, long pending)
{
	// factor in previous pending frames, this shouldn't happen any more
	if (mPendingFrames != 0)
	  Trace(this, 1, "Layer: Overlapping pending frames!\n");

	pending = pending - mPendingFrames;
	if (pending < 0) {
		Trace(this, 1, "Layer: Negative pending frames!\n");
		pending = 0;
	}

	// should actually already know this
	long recordedFrames = mAudio->getFrames();
	long delta = frames - recordedFrames;
	if (delta != pending)
	  Trace(this, 1, "Layer: Pending frame miscalculation!\n");

	if (pending > 0)
	  Trace(this, 2, "Layer: pending frames %ld\n", pending);

	setFrames(con, frames);

	mPendingFrames = pending;
}

/**
 * Empty the layer and set the number of frames.  
 * Used to initialize a new record layer in Rehearse mode and
 * when processing TrackCopy=Timing
 */
void Layer::zero(long frames, int cycles)
{
	resetSegments();
    mHeadWindow->reset();
    mTailWindow->reset();
	mFrames = frames;
	mAudio->reset();
	mAudio->setFrames(frames);
	mOverdub->reset();
	mOverdub->setFrames(frames);
	mCycles = cycles;
}

void Layer::zero()
{
	zero(getFrames(), getCycles());
}

int Layer::getCycles()
{
	return mCycles;
}

void Layer::setCycles(int i)
{
	if (mCycles != i) {
		mCycles = i;
		mStructureChanged = true;
	}
}

long Layer::getCycleFrames()
{
	// mCycles can be 0 in LoopCopy=Timing?
	long frames = mFrames;
	if (mCycles > 1)
	  frames /= mCycles;
	return frames;
}

Audio* Layer::getAudio() 
{
	return mAudio;
}

Audio* Layer::getOverdub() 
{
	return mOverdub;
}

/**
 * Give the layer new audio.  Used to initialize loop/layer contents
 * from project files.  Also now used when doing a bounce recording.
 *
 * This will cause memory churn if it happens a lot, consider pooling
 * Audio objects rather than layers.
 */
void Layer::setAudio(Audio* a)
{
    delete mAudio;
    mAudio = a;
    mRecordCursor->setAudio(mAudio);
    mFeedbackCursor->setAudio(mAudio);
    mPlayCursor->setAudio(mAudio);
    mCopyCursor->setAudio(mAudio);
    mOverdub->reset();
    mHeadWindow->reset();
    mTailWindow->reset();

	mFrames = 0;
	mMax = 0.0f;
	mCycles = 1;
    // always reset these too?
    resetSegments();
	mFrames = a->getFrames();
}

void Layer::setOverdub(Audio* a)
{
    delete mOverdub;
    mOverdub = a;
    mOverdubCursor->setAudio(a);
}

/**
 * Return true if there was a meaningful feedback change during the
 * recording of this layer.  Called by Loop to determine if we need
 * to shift this layer even if no audio content was modified.
 *
 * mFeedbackApplied is a transient value set true if we notice feedback
 * dip below 100% during recording of the layer.  This indiciates that 
 * even if there was no content added to the layer, it still needs to 
 * be shifted to preserve the feedback changes.
 *
 * If we're not flattening and there is only one segment, this is determined
 * by the ending feedback of the segment.  Feedabck may have been reduced
 * but as long as it was brought back up without causing an occlusion split
 * we pretend nothing happened.
 *
 * Old comments in loop also say that it should be true if the current feedback
 * is greater than the starting feedback, so when returning to 1000%
 * we don't get a level jump on preplay.  I think this was the case only
 * when we were applying feedback to the layer copy, not keeping it updated
 * as we recorded.
 */
bool Layer::isFeedbackApplied()
{
    bool applied = mFeedbackApplied;
    if (mNoFlattening && mSegments != NULL && mSegments->getNext() == NULL) {
		int segFeedback = mSegments->getFeedback();
		// segment feedback may not match current feedback, but only 
		// if there was a structure change, like a replace in the middle
		// do we need to check both?  Probably not since isStructureChanged
		// will force a shift too
        applied = (segFeedback < AUTO_FEEDBACK_LEVEL || 
				   mFeedback < AUTO_FEEDBACK_LEVEL);
    }
    return applied;
}

void Layer::setFadeOverride(bool b)
{
	mFadeOverride = b;
}

CheckpointState Layer::getCheckpoint()
{
	return mCheckpoint;
}

bool Layer::isCheckpoint()
{
	return (mCheckpoint == CHECKPOINT_ON);
}

void Layer::setCheckpoint(CheckpointState c)
{
	mCheckpoint = c;
}

long Layer::getWindowOffset() 
{
    return mWindowOffset;
}

void Layer::setWindowOffset(long offset)
{
    mWindowOffset = offset;
}

long Layer::getWindowSubcycleFrames() 
{
    return mWindowSubcycleFrames;
}

void Layer::setWindowSubcycleFrames(long offset)
{
    mWindowSubcycleFrames = offset;
}

void Layer::setHistoryOffset(long offset)
{
    mHistoryOffset = offset;
}

/**
 * This differs from most properties in that we'll calculate
 * it on the fly and cache it.  Ideally we should set this as
 * the layers are added, but that's harder to keep right.
 */
long Layer::getHistoryOffset()
{
    if (mHistoryOffset == 0) {
        if (mPrev != NULL)
          mHistoryOffset = mPrev->getHistoryOffset() + mPrev->getFrames();
    }
    return mHistoryOffset;
}

/**
 * Search backward for the previous checkpoint.
 * Normally the current layer will be a checkpoint, but this
 * methods doesn't enforce that.
 */
Layer* Layer::getPrevCheckpoint()
{
	Layer* check = mPrev;
	while (check != NULL && !check->isCheckpoint())
	  check = check->getPrev();
	return check;
}

/**
 * Search backward for layer immediately prior to the
 * previous checkpoint.  Normally the current layer will
 * be a checkpoint, but the method doesn't care.
 */
Layer* Layer::getCheckpointTail()
{
	Layer* tail = this;
	Layer* prev = mPrev;

	while (prev != NULL && !prev->isCheckpoint()) {
		tail = prev;
		prev = prev->getPrev();
	}

	return tail;
}

/**
 * Search backward for the oldest layer in the list.
 */
Layer* Layer::getTail()
{
	Layer* tail = this;
	while (tail->getPrev() != NULL)
	  tail = tail->getPrev();
	return tail;
}

/**
 * Helper for Loop::getState.
 * Return interesting things about this layer.
 */
void Layer::getState(LayerState* s)
{
	s->checkpoint = isCheckpoint();
}

/****************************************************************************
 *                                                                          *
 *                             SEGMENT MANAGEMENT                           *
 *                                                                          *
 ****************************************************************************/

/**
 * Remove the list of segments.
 */
void Layer::resetSegments()
{
    Segment* next = NULL;

    for (Segment* seg = mSegments ; seg != NULL ; seg = next) {
        next = seg->getNext();
        delete seg;
    }

    mSegments = NULL;
}

/**
 * Return the list of segments.
 * Do NOT modify these, only for use by the Project builder.
 */
Segment* Layer::getSegments()
{
	return mSegments;
}

/**
 * Add a layer segment.
 * Used only in the implementation of copy(Layer*)
 */
Segment* Layer::addSegment(Layer* src)
{
    Segment* seg = NULL;
    if (src != NULL) {
        seg = new Segment(src);
		addSegment(seg);
	}
	return seg;
}

/**
 * Add a segment, always append to the end since these
 * will tend to be ordered, but probably should be doing
 * an insertion sort here.  
 *
 * NOTE: If we start ordering segments, then the two trim methods
 * will have to be smart about resorting.
 *
 * Adding a segment in theory can require that the edge fades
 * for all segments be recalculated.  It is up to the caller
 * to either do a full recalc, or know that this is not necessary.
 */
void Layer::addSegment(Segment* seg)
{
    if (seg != NULL) {
        Segment* last = NULL;
        for (last = mSegments ; last != NULL && last->getNext() != NULL ; 
             last = last->getNext());

        if (last == NULL)
		  mSegments = seg;
        else 
		  last->setNext(seg);
    }
}

/**
 * Remove a segment.
 * 
 * Like adding, removing a segment in theory can require that the 
 * edge fades for all segments be recalculated.  It is up to the caller
 * to either do a full recalc, or know that this is not necessary.
 */
void Layer::removeSegment(Segment* seg)
{
    if (seg != NULL) {
        Segment* prev = NULL;
		Segment* s = NULL;
        for (s = mSegments ; s != NULL && s != seg ; s = s->getNext())
		  prev = s;

		if (s == seg) {
			if (prev == NULL)
			  mSegments = seg->getNext();
			else
			  prev->setNext(seg->getNext());
			seg->setNext(NULL);
		}
	}
}

/**
 * Set the segment list.
 * Should only be called by spawn() and should not have any
 * existing segments.
 *
 * Intended for use only when loading projects.  The segment
 * edge fades are expected to be correct and will not be reclaculated
 * (though we can do so without any trouble).
 *
 * Now also used by WindowFunction.
 */
void Layer::setSegments(Segment* list)
{
	resetSegments();
	mSegments = list;
}

/****************************************************************************
 *                                                                          *
 *                                   FADES                                  *
 *                                                                          *
 ****************************************************************************/
/*
 * There are two forms of fade during recording, an up fade performed when 
 * a new recording starts, and a down fade performed when a recording ends.
 * 
 * Up fades are performed "dynamically" by modifying samples as they
 * are received, and before they are stored in the Audio.  This happens
 * in the AudioCursor as a block of frames from the interrupt  handler
 * is put into the Audio.
 *
 * Down fades are performed "retroactively" by modifying samples
 * that have already been received and stored in the Audio.  It would
 * be possible to perform down fades dynamically in some cases, but
 * it is difficult.  Events would have to be scheduled to start the fade
 * in advance of the record ending, and during the fade the ending
 * could be canceled which would require that the fade be undone.
 * It is much much easier to wait until we're absolutely sure that the
 * fade is necessary and apply it retroactively.  The fade is performed
 * by Audio which uses an internal AudioCursor for retroactive fades.
 * 
 * There is support in the AudioFade object to "schedule" fades
 * to commence on a particular frame.  This is no longer used.
 * Dynamic up fades always start immediately with the next block
 * of frames stored in to the layer, and down fades are always performed
 * immediately on content already in the layer.
 *
 * During playback of a finished layer, there is only one kind of fade
 * at this level, a dynamic up fade used when playback resumes in the middle
 * of the layer.  This is handled by a local AudioFade object maintained
 * in each Layer.  It cannot be done in the AudioCursor as we do for
 * recording because the fade must be applied not only to the contents
 * of the local audio but to the content contributed by the segments as well.
 *
 * During recording we automatically detect when up and down fades
 * are necessary.  This assumes that recording will always proceed
 * seamlessly from beginning to end, you cannot jump around.  The need
 * for a leading edge fade is determined by looking for a trailing
 * edge fade in the previous layer.  The one case where we need
 * assistance from outside is when the layer is finalized before shifting.
 * At this level we cannot know if recording will proceed seamlessly into the
 * next layer so this must be passed down.
 */

/**
 * Perform a fade to the left edge.
 * Used to apply deferred edge fades, and also by splice().
 */
PRIVATE void Layer::fadeLeft(bool foreground, bool background, float baseLevel)
{
    int fadeFrames = AudioFade::getRange();

	if (foreground && background) {

        CovFadeLeftBoth = true;

		// the easy case, just do simple fades to the Audio objects and
		// blow off the windows
        Trace(this, 2, "Layer: Performing full fade left\n");

		mRecordCursor->setReverse(false);
		mRecordCursor->setFrame(0);
		mRecordCursor->fade(0, fadeFrames, true, baseLevel);

        // NOTE: the Isolated Overdub parameter was experimental and no longer exposed
		if (mIsolatedOverdub) {
			mOverdubCursor->setReverse(false);
			mOverdubCursor->setFrame(0);
			// baseLevel is irrelevant here?
			if (baseLevel != 1.0)
			  Trace(this, 1, "Layer: Fade question 1\n");
			mOverdubCursor->fade(0, fadeFrames, true);
		}

        // this can't be true
		if (baseLevel == 1.0) {
			mDeferredFadeLeft = false;

			// this may still be true if we have segments, but let
			// compileSegmentFades figure that out
			mContainsDeferredFadeLeft = false;
		}

		// since we've done the job, don't leave anything behind in the window
		// that may confuse things
		// ?? what if baseLevel != 1.0, the window still applies and
		// we should adjust it, luckly this only happens in finalize?
		FadeWindow* win = (mReverseRecord) ? mTailWindow : mHeadWindow;
		win->reset();
	}
	else if (foreground) {

        Trace(this, 2, "Layer: Performing local fade left\n");

		// in reverse, the left edge is the tail
		FadeWindow* win = (mReverseRecord) ? mTailWindow : mHeadWindow;

		// if the window is empty, then there was nothing recorded on the left
		if (win->getFrames() > 0) {

			// if the reverse tail window didn't make it to the left edge, 
			// then we shouldn't be asking to fade the foreground
			if (mReverseRecord && 
				win->getLastExternalFrame() != mAudio->getFrames()) {
				Trace(this, 1, "Layer: Reverse tail window does not cover left edge!\n");
			}	
			else {
                if (mReverseRecord)
                  CovFadeLeftForegroundRev = true;
                else
                  CovFadeLeftForeground = true;
				win->fadeForeground(mRecordCursor, baseLevel);
			}
		}

		// the isolated overdub has no merged audio so it's simple
        // NOTE: the Isolated Overdub parameter was experimental and no longer exposed
		if (mIsolatedOverdub) {
			mOverdubCursor->setReverse(false);
			// ?? baseLevel is irrelevant here
			if (baseLevel != 1.0)
			  Trace(this, 1, "Layer: Fade question 2\n");
			mOverdubCursor->fadeIn(mOverdub);
		}
		
		if (baseLevel == 1.0)
		  mDeferredFadeLeft = false;
	}
	else if (background) {

        Trace(this, 2, "Layer: Performing background fade left\n");

        if (mReverseRecord)
          CovFadeLeftBackgroundRev = true;
        else
          CovFadeLeftBackground = true;

        // Remove the foreground, fade, and put it back.
		// In reverse, the left edge is the tail.  Note that the window
        // may not actually cover the fade range, but we don't care as
        // long as it gets put back.

		FadeWindow* win = (mReverseRecord) ? mTailWindow : mHeadWindow;

        win->removeForeground(mRecordCursor);

        mRecordCursor->setReverse(false);
        mRecordCursor->setFrame(0);
        mRecordCursor->fade(0, fadeFrames, true, baseLevel);

        win->addForeground(mRecordCursor);

        // this may still be true if we have segments, but let
        // compileSegmentFades figure that out
		if (baseLevel == 1.0f)
		  mContainsDeferredFadeLeft = false;
	}
}

/**
 * Apply the left deferred fade.
 */
void Layer::applyDeferredFadeLeft()
{
    if (mDeferredFadeLeft) {

        if (mNoFlattening)
          fadeLeft(true, true, 1.0f);
        else
          fadeLeft(true, false, 1.0f);

        mDeferredFadeLeft = false;
    }
}

/**
 * Perform a fade to the right edge of the local audio.
 */
PRIVATE void Layer::fadeRight(bool foreground, bool background, float baseLevel)
{
	long startFrame = mAudio->getFrames();
	int fadeFrames = AudioFade::getRange();
    int fadeOffset = 0;

	startFrame -= fadeFrames;
	if (startFrame < 0) {
		// it would have to be an impossibly short loop to get here
		startFrame = 0;
		fadeFrames += startFrame;
		fadeOffset = -startFrame;
	}

	if (foreground && background) {

        CovFadeRightBoth = true;
        Trace(this, 2, "Layer: Performing full fade right\n");

		mRecordCursor->setReverse(false);
		mRecordCursor->setFrame(startFrame);
		mRecordCursor->fade(fadeOffset, fadeFrames, false, baseLevel);
        // NOTE: the Isolated Overdub parameter was experimental and no longer exposed
		if (mIsolatedOverdub) {
			// must be the same size!
			mOverdubCursor->setReverse(false);
			mOverdubCursor->setFrame(startFrame);
			// ?? baseLevel not relevant
			if (baseLevel != 1.0)
			  Trace(this, 1, "Layer: Fade question 3\n");
			mOverdubCursor->fade(fadeOffset, fadeFrames, false);
		}

		if (baseLevel == 1.0) {
			mDeferredFadeRight = false;
			// this may still be true if we have segments, but let
			// compileSegmentFades figure that out
			mContainsDeferredFadeRight = false;
		}

		// be safe and don't leave invalid fade windows behind
		// ?? same question as fadeLeft
		FadeWindow* win = (mReverseRecord) ? mHeadWindow : mTailWindow;
		win->reset();
	}
	else if (foreground) {

        Trace(this, 2, "Layer: Performing local fade right\n");

		// in reverse, the right edge is the head
		FadeWindow* win = (mReverseRecord) ? mHeadWindow : mTailWindow;

		// if it is empty, then we didn't record to this edge
		if (win->getFrames() > 0) {

			// if the tail window didn't make it to the right edge, 
			// then we should be asking to fade the foreground
			if (!mReverseRecord && 
				win->getLastExternalFrame() != mAudio->getFrames()) {
				Trace(this, 1, "Layer: Tail window does not cover right edge!\n");
			}
			else {
                if (mReverseRecord)
                  CovFadeRightForegroundRev = true;
                else
                  CovFadeRightForeground = true;
				win->fadeForeground(mRecordCursor, baseLevel);
			}
		}

        // NOTE: the Isolated Overdub parameter was experimental and no longer exposed
		if (mIsolatedOverdub) {
			mOverdubCursor->setReverse(false);
			// ?? baseLevel relevant
			if (baseLevel != 1.0)
			  Trace(this, 1, "Layer: Fade question 4\n");
			mOverdubCursor->fadeOut();
		}

		if (baseLevel == 1.0)
		  mDeferredFadeRight = false;
	}
	else if (background) {

        Trace(this, 2, "Layer: Performing background fade right\n");

        if (mReverseRecord)
          CovFadeRightBackgroundRev = true;
        else
          CovFadeRightBackground = true;

		// in reverse, the right edge is the head
		FadeWindow* win = (mReverseRecord) ? mHeadWindow : mTailWindow;

        // the window may not actually overlap the fade range but
        // it doesn't matter as long as we put it back
        win->removeForeground(mRecordCursor);

		mRecordCursor->setReverse(false);
		mRecordCursor->setFrame(startFrame);
		mRecordCursor->fade(fadeOffset, fadeFrames, false, baseLevel);

        win->addForeground(mRecordCursor);

        // this may still be true if we have segments, but let
        // compileSegmentFades figure that out
		if (baseLevel == 1.0)
		  mContainsDeferredFadeRight = false;
	}
}

/**
 * Utility to capture a portion of the local audio and save it to a file.
 */
PUBLIC void Layer::saveRegion(long startFrame, long frames, const char* name)
{
	long samples = frames * mAudio->getChannels();
	float* buffer = new float[samples];
	Audio* a = mAudioPool->newAudio();
	
	memset(buffer, 0, sizeof(float) * samples);
	mAudio->get(buffer, frames, startFrame);
	a->append(buffer, frames);
	a->write(name);

	delete buffer;
	delete a;
}

/**
 * Apply the right deferred fade.
 */
void Layer::applyDeferredFadeRight()
{
    if (mDeferredFadeRight) {

        if (mNoFlattening)
          fadeRight(true, true, 1.0f);
        else
          fadeRight(true, false, 1.0f);

        mDeferredFadeRight = false;
    }
}

/**
 * Called internally when we're about to record something into the layer.
 * If there is a gap between the current frame and the last recorded frame,
 * apply fades to the edges.  The frame must not be reflected.
 */
PRIVATE void Layer::checkRecording(LayerContext* con, long startFrame)
{
    bool firstTime = !mRecordable;

    if (firstTime) {
        // must be the first time we've recorded into this layer
        // prep both windows
        mHeadWindow->prepare(con, true);
        mTailWindow->prepare(con, false);
		mReverseRecord = con->isReverse();
    }

    if (startFrame == 0) {
        bool deferHeadFade = false;
        if (mPrev != NULL) {
			if (mPaused || mPrev->isReverseRecord() != con->isReverse()) {
				// If paused have to force the deferred fades since we can't
				// continue seamlessly.
                // If Direction changed, must have been a Reverse alternate
                // ending, the edges won't be adjacent.
                if (con->isReverse() && mPrev->isDeferredFadeRight()) {
					if (!mPaused)
					  // someone above didn't catch the direction change
					  Trace(this, 1, "Layer: Detected missing tail fade!\n");
                    mPrev->applyDeferredFadeRight();
                }
                else if (!con->isReverse() && mPrev->isDeferredFadeLeft()) {
					if (!mPaused)
					  Trace(this, 1, "Layer: Detected missing reverse tail fade!\n");
                    mPrev->applyDeferredFadeLeft();
                }
            }
            else if (con->isReverse())
              deferHeadFade = mPrev->isDeferredFadeLeft();
            else
              deferHeadFade = mPrev->isDeferredFadeRight();
        }

        // !! if we just looped, we'll be at frame zero but the last
        // tail window frame will be beyond the end of the loop
        // so it is seamless.  This wasn't being caught as far back as 
        // 1.42, was overdub ever seamless?
        /*
        if (mTailWindow->getLastExternalFrame() == mFrames)
            deferHeadFade = true;
        */

		// mFadeOverride is a special case used only for audio insertion
        // from scripts where we want to avoid a fade of already faded
        // material, but it isn't a deferred fade
        if (deferHeadFade || mFadeOverride) {
            if (mRecordCursor->isFading()) {
                // someone above thought we needed a fade, ignore it
                Trace(this, 1, "Layer: Ignoring requested head fade!\n");
                mRecordCursor->resetFade();
            }
			if (!mFadeOverride) {
				Trace(this, 2, "Layer: Seamless shift, deferring fade in\n");
				if (mReverseRecord)
				  mDeferredFadeRight = true;
				else
				  mDeferredFadeLeft = true;
			}

			mFadeOverride = false;
        }
        else {
            // InputStream and/or Loop may have already set this, but
            // don't requre that
            startRecordFade(con);
        }
    }
	else if (startFrame < 0) {
		// can't be recording in the latency lead in!
		Trace(this, 1, "Layer: Can't record during latency delay!\n");
	}
    else {
        // we're picking up in the middle
		// TODO: mFadeOverride might be meaningful here, but for now
		// it only applies to the edges
        if (firstTime && mPrev != NULL) {
            // detect incorrect deferred fades in the previous layer,
            // these should already have been performed by finalize() 
            if (mPrev->isReverseRecord()) {
                if (mPrev->isDeferredFadeLeft()) {
                    Trace(this, 1, "Layer: Detected incorrect reverse tail fade!\n");
                    mPrev->applyDeferredFadeLeft();
                }
            }
            else if (mPrev->isDeferredFadeRight()) {
                Trace(this, 1, "Layer: Detected incorrect tail fade!\n");
                mPrev->applyDeferredFadeRight();
            }
        }

		// subtlety: when we're in the limbo area after the end of an
		// unrounded insert, Loop will call us with an empty buffer
		// to signify that "silence" is being recorded. This has to 
		// force a fade of the previous recording.
		// also if we're resuming from a pause have to fade edges eve nthough
		// the last frame will be equal to the start frame
        if ((con->buffer == NULL && !mMuted) ||
			mPaused ||
			mTailWindow->getLastExternalFrame() != startFrame) {
            // a record gap
            fadeOut(con);
            startRecordFade(con);
		}

		mMuted = (con->buffer == NULL);

		if (con->isReverse() != mReverseRecord) {
			// Changed direction!  I guess we can allow this
			Trace(this, 2, "Layer: Changing recording direction, applying fades\n");

			// avoid a warning by temporarily setting the direction to 	
			// the previous value
			con->setReverse(mReverseRecord);
			fadeOut(con);
			con->setReverse(!mReverseRecord);
			startRecordFade(con);

			// !! more to do 
			// if we want to support entering and leaving the layer in 
			// different directions, will have to keep a pair of 
			// direction flags, mReverseRecord isn't enough
			// avoid the complications for now by forcing the edge fades

            if (mReverseRecord) {
				applyDeferredFadeRight();
				if (mPrev != NULL)
				  mPrev->applyDeferredFadeLeft();
			}
			else {
				applyDeferredFadeLeft();
				if (mPrev != NULL)
				  mPrev->applyDeferredFadeRight();
			}

			mReverseRecord = con->isReverse();
		}
    }

	// keep a moving window for intermediate fades
	if (startFrame >= 0) {
		mHeadWindow->add(con, startFrame);
		mTailWindow->add(con, startFrame);
		mRecordable = true;
		mStarted = true;
	}

	// exit pause only when we have frames to consume
	if (con->frames > 0)
	  mPaused = false;
}

/**
 * Called when we enable recording after at least one frame
 * of not recording.  Begin applying a permanent upward fade to 
 * our local Audio.
 */
PRIVATE void Layer::startRecordFade(LayerContext* con)
{
    // frame passed in only for this message
    Trace(this, 2, "Layer: Starting record fade in\n");

    mHeadWindow->startFadeIn();
    mTailWindow->startFadeIn();

    mRecordCursor->startFadeIn();
    // NOTE: the Isolated Overdub parameter was experimental and no longer exposed
	if (mIsolatedOverdub)
	  mOverdubCursor->startFadeIn();
}

/**
 * Perform a retroactive fade out to the end of the last recorded region.
 * Called by checkRecording as we detect gaps in the recording.
 * 
 * If we are close enough to the beginning that the entire fade range
 * cannot be processed, we have to move back to the previous layer
 * to complete the fade.  A very small window but it could happen if
 * you were doing unquantized overdubs and happened to end just
 * after the layer switch.
 *
 * ?? Technically should be remembering the recording speed
 * then fade according to that speed.  Currently we'll always fade in full
 * speed, which if we were recording in half speed will result in a
 * shorter than normal fade when we return to full speed.
 */
PUBLIC void Layer::fadeOut(LayerContext* con)
{
	if (ScriptBreak) {
		int x = 0;
	}

	// tail window will be empty if we never actually recorded in this layer
	// note that since advance() calls this all the time check to see
	// if we've already faded to avoid a warning message
	if (!mTailWindow->isForegroundFaded() && 
		mTailWindow->getFrames() > 0) {

		long lastFrame = mTailWindow->getLastExternalFrame();
		Trace(this, 2, "Layer: Applying fade out before frame %ld\n", lastFrame);

		// have to detect spillage back to the previous layer
		long fadeFrames = mTailWindow->getWindowFrames();
		long fadeStartFrame = lastFrame - fadeFrames;
		long fadeOffset = 0;
		bool SaveIt = false;

		if (fadeStartFrame < 0) {
			// too close to the front
            CovFadeOutCrossing = true;
			Trace(this, 2, "Layer::fadeOut range crosses layer boundary\n");
			fadeOffset = -fadeStartFrame;
			fadeFrames -= fadeOffset;
			fadeStartFrame = 0;
		}

		// this will also detect the partial fade offset in another way
		mTailWindow->fadeForeground(mRecordCursor, 1.0f);

        // NOTE: the Isolated Overdub parameter was experimental and no longer exposed
		if (mIsolatedOverdub) {
			// reverse reflection (used only if we're not using the FadeWindow)
			long reflectedFrame = reflectFrame(con, fadeStartFrame);
			mOverdubCursor->setReverse(con->isReverse());
			mOverdubCursor->setFrame(reflectedFrame);
			mOverdubCursor->fade(0, fadeOffset, false);
		}

		// rare, but for extremely short recording blips this may
		// overlap the head window
    
		if (fadeStartFrame < mHeadWindow->getWindowFrames()) {
            CovFadeOutHeadOverlap = true;
			Trace(this, 2, "Layer: Tail fade overlaps head window\n");
			mHeadWindow->fadeWindow(fadeStartFrame, fadeOffset);
		}

		if (fadeOffset > 0) {
			if (mPrev == NULL) {
				Trace(this, 1, "Layer: Split fade with no previous layer!\n");
				// This may be ok, just a really short recording?
				// In practice we won't let it be this short though.
			}
			else if (mPrev->isDeferredFadeOut()) {
				mPrev->fadeOut(con, fadeOffset);
			}
		}
	}
}

/**
 * Called only by another layer when it needs to perform a partial
 * fade out at the end of the previous layer when the fade needs to 
 * span the layer boundary.
 */
PRIVATE void Layer::fadeOut(LayerContext* con, long frames)
{
	// This should only happen if we had a deferred fade out, which
	// means the tail window must be all the way to the end
	if (mTailWindow->getLastExternalFrame() != getFrames())
	  Trace(this, 1, "Layer: FadeWindow not positioned at the end!\n");
	else {
        CovFadeOutPrev = true;
		mTailWindow->fadeForegroundShifted(mRecordCursor, frames);

        // NOTE: the Isolated Overdub parameter was experimental and no longer exposed
		if (mIsolatedOverdub) {
			long startFrame = getFrames() - frames;
			long reflectedStartFrame = reflectFrame(con, startFrame);
			mOverdubCursor->setReverse(con->isReverse());
			mOverdubCursor->setFrame(reflectedStartFrame);
			mOverdubCursor->fade(0, frames, false);
		}
	}
}

/**
 * Cancel a play fade in that had been previously set up.
 * Called when we've setup a transition to another loop
 * and begun fading into it, then a different loop 
 * is selected before the quantized transition frame.
 * A relatively small window.
 */
void Layer::cancelPlayFade()
{
    mFade.init();
}

/**
 * Used in cases where we've begun pre-play of the record layer and
 * decide to squelch it an reuse it for the next pass.  If there was
 * a play fade in still in progress, continue it in the previous layer.
 */
void Layer::transferPlayFade(Layer* dest)
{
	dest->mFade.copy(&mFade);
}

/**
 * Do a complete analysis of the segments to determine if any changes
 * need to be made to the edge fades.  Usually they will already
 * be correct since we needed them during flattening.  But a full analysis
 * can be useful to correct improperly stored projects, or to recognize
 * complicated structures with multiple references to the same layer.
 * Such structures exist only in theory right now, but may in the future
 * when loop windowing and 'scatter' mode is introduced.
 *
 * The logic here is extremely complex and subtle, see the Layer Structure
 * section of the design spec for more details.
 * 
 * This is NOT efficient if we can have lots of segments.  It shouldn't
 * be called that often, but if it can we would have to maintain
 * dependencies between segments explicitly in the data model rather than
 * doing linear searches on the segment list.
 *
 * In theory, we should be fading adjacent segments if they have a
 * substantially different feedback level.  May be other modifications made
 * via reference?
 *
 * The redo flag is passed only so we can suppress some warning messages
 * about deferred fade changes.
 *
 * UPDATE: Don't need the redo flag any more since we can also reach
 * the same condition if you replace to the end and occlude the deferred
 * fade, so just ignore all missing trailing deferred fades.
 *
 * The checkConsistency flag is normally on because during flattening
 * we try to maintain the fades correctly as we go.  In a few special
 * cases we call this to "fix" the fades, so the flag is false to 
 * avoid warnings.
 */
void Layer::compileSegmentFades(bool checkConsistency)
{
	int fadeRange = AudioFade::getRange();
    Segment* s;

	if (ScriptBreak) {
		int x = 0;
	}

    // Calculate deferred fade containment as we go
    bool containsDeferredFadeLeft = false;
    bool containsDeferredFadeRight = false;

	// but if we replaced the head, then we can no longer have
	// indirect deferred head fades
	if (mStartingFeedback == 0) {
		if (mReverseRecord)
		  mContainsDeferredFadeRight = false;
		else
		  mContainsDeferredFadeLeft = false;
	}

    // Save the current segment fade state for later verification and
    // detect backing layer occlusions.  It's easier to turn fades off
    // than on so start by turning them all on.  If a segment is marked
    // as having adjacent local content, the fade is always off.  Note that
    // we must detect and perform a layer occlusion fades in this layer
    // before we process the segments because the presence of leading
    // deferred fade may affect the segment fades.

    bool leftOcclusion = true;
    bool rightOcclusion = true;

    for (s = mSegments ; s != NULL ; s = s->getNext()) {
        s->saveFades();
		s->setFadeLeft(true);
		s->setFadeRight(true);

        // detect layer occlusion fades
        Layer* refLayer = s->getLayer();
        long segFrames = s->getFrames();
        long localStart = s->getOffset();
        long localEnd = localStart + segFrames;
        long refStart = s->getStartFrame();
        long refEnd = refStart + segFrames;
        long refTotal = refLayer->getFrames();

		// turn off edge fade if we have adjacent copied content,
		// if we're no an edge, have to wait for adjacent segment
		// detection below
		// !! Until 1.27, we only turned off the fade if the adjacent
		// copy exceeded fadeRange, why?  if we have an adjacent copy
		// of any length, we always need to cancel the fade??

		//if (s->getLocalCopyLeft() >= fadeRange)
		if (s->getLocalCopyLeft() > 0)
		  s->setFadeLeft(false);

		//if (s->getLocalCopyRight() > fadeRange)
		if (s->getLocalCopyRight() > 0)
		  s->setFadeRight(false);

		// occlusion is a bit of a misnomer now because if we're flattening
		// we will have the adjacent content so the edge is not actually
		// occluded, but if we're not flattening it is

        if (localStart == 0 && refStart == 0)
		  leftOcclusion = false;

        if (localEnd == mFrames && refEnd == refTotal)
		  rightOcclusion = false;
    }
    
    // Apply occlusion fades if we're not flattening.
    // If we're flattening, then we'll always be trimming segments
    // and can't tell if we're occluded just by the absense of a reference,
    // have to detect this in finalize() by examining the ending feedback.
    if (mNoFlattening) {

        if (mDeferredFadeLeft && !mReverseRecord && rightOcclusion)
		  applyDeferredFadeLeft();

        if (mDeferredFadeRight && mReverseRecord && leftOcclusion)
		  applyDeferredFadeRight();
    }

    // RULE: A deferred leading edge fade must be applied whenever the
    // trailing edge of the previous layer is no longer adjacent to trailing
    // edge of the current layer.

    for (s = mSegments ; s != NULL ; s = s->getNext()) {

        Layer* refLayer = s->getLayer();
        long segFrames = s->getFrames();
        long localStart = s->getOffset();
        long localEnd = localStart + segFrames;
        long refStart = s->getStartFrame();
        long refEnd = refStart + segFrames;
        long refTotal = refLayer->getFrames();

        // before checking for adjacent segments, turn off fades
        // on the edges if the backing layer has already faded
        if (refStart == 0) {
			if (localStart == 0 && localEnd == mFrames && refEnd == refTotal) {
				// the entire layer is referenced and both layers are the
				// same size, let the referenced layer decide how to do fades
				s->setFadeLeft(false);
				s->setFadeRight(false);
			}
            else if (localStart == 0 && rightOcclusion && !mNoFlattening) {
                // assume adjacent content from the other edge has been copied
                s->setFadeLeft(false);
            }
            else if (!refLayer->hasDeferredFadeLeft()) {
				// already faded, don't add another one
                s->setFadeLeft(false);
			}
            else if (localStart == 0 && 
					 refLayer == mPrev &&
                     refLayer->isReverseRecord() && 
					 mDeferredFadeRight) {
                // Recorded seamlessly from the left edge of the previous
                // layer to the right edge of this one (in reverse).
				// See forward clause below on why the fade can only 
				// be deferred if we're replacing.
                if (refLayer->mFeedback == 0 ||
					!refLayer->mContainsDeferredFadeLeft)
                  s->setFadeLeft(false);
            }
        }

        if (refEnd == refTotal) {

            if (localEnd == mFrames && leftOcclusion && !mNoFlattening) {
                // assume adjacent content from the other edge has been copied
                s->setFadeRight(false);
            }
            else if (!refLayer->hasDeferredFadeRight()) {
				// already faded, don't add another
                s->setFadeRight(false);
			}
            else if (localEnd == mFrames &&
                     refLayer == mPrev &&
                     !refLayer->isReverseRecord() &&
					 mDeferredFadeLeft) {
                // Recorded seamlessly from the right edge of the previous
                // layer to the left edge of this one.
                // UPDATE: We can only disable the fade here if we were
                // Replacing (feedback == 0) over the loop boundary, or if
				// the previous layer does not *contain* a deferred right fade.
				// If we ended the previous layer in overdub and began this one
                // in replace, there will be a left occlusion and a break
				// in the background content of the previous layer.
				// If we did not occlude the left, it will be detected
				// with an adjacent segment below.
                if (refLayer->mFeedback == 0 ||
					!refLayer->mContainsDeferredFadeRight)
                  s->setFadeRight(false);
            }
        }

        for (Segment* s2 = mSegments ; s2 != NULL ; s2 = s2->getNext()) {
            if ((s != s2) && (s2->getLayer() == refLayer)) {

                long segFrames2 = s2->getFrames();
                long localStart2 = s2->getOffset();
                long localEnd2 = localStart2 + segFrames2;
                long refStart2 = s2->getStartFrame();
                long refEnd2 = refStart2 + segFrames2;

                // check edge adjacent segments
                if (localStart == 0 && localEnd2 == mFrames) {
                    // locally adjacent from right edge to left edge

					if (refStart == 0 && refEnd2 == refTotal) {
						// the referenced regions are also on the edges
						// of the backing layer let the layer handle its
						// own fading
                        s->setFadeLeft(false);
                        s2->setFadeRight(false);
					}
					else if (refStart == refEnd2) {
						// Not on the edges of the backing layer but
						// still adjacent.  One way this happens is 
						// after a StartPoint.  Note that this
						// also represents containment of deferred fades
						// so we can force an edge fade later if there
						// is an occlusion
                        s->setFadeLeft(false);
                        s2->setFadeRight(false);
						containsDeferredFadeLeft = true;
						containsDeferredFadeRight = true;
					}
                }

                // check for simple adjacent segments
                if (localEnd == localStart2 && refEnd == refStart2) {
					// adjacent references
					s->setFadeRight(false);
					s2->setFadeLeft(false);
                }
            }
        }
    }

    // Another pass to detect containment of deferred fades.
    // Could have done this in the previous loop too, but it gets confusing.
    // Note that we may already have detected an obscure form of
    // containment in the previous loop so don't trash the values 
    // if already set.

    for (s = mSegments ; s != NULL ; s = s->getNext()) {
        Layer* refLayer = s->getLayer();
        long segFrames = s->getFrames();
        long localStart = s->getOffset();
        long localEnd = localStart + segFrames;
        long refStart = s->getStartFrame();
        long refEnd = refStart + segFrames;
        long refTotal = refLayer->getFrames();

		// only let these turn on, not off
		if (!containsDeferredFadeLeft) {
			// if it starts at an edge, the referenced layer has a deferred
			// fade, and the segment is not already fading, we contain 
			// a deferred fade
			containsDeferredFadeLeft = 
				localStart == 0 && refStart == 0 && 
				refLayer->hasDeferredFadeLeft() && !s->isFadeLeft();
		}

        if (!containsDeferredFadeRight) {
			containsDeferredFadeRight = 
				localEnd == mFrames && refEnd == refTotal &&
				refLayer->hasDeferredFadeRight() && !s->isFadeRight();
		}
	}

	// Deferred fade containment consistency checks.
	// 
	// If we're not flattening, we can't really consistency check as there
	// are two many cases where segment adjustments made previously
	// in this method as well as during recording can affect the
	// fade semantics.  If we allowed new segments to be dropped while
	// we were flattening the same issue would exist.

    if (mContainsDeferredFadeLeft != containsDeferredFadeLeft) {
		if (mNoFlattening)
		  mContainsDeferredFadeLeft = containsDeferredFadeLeft;
		else if (containsDeferredFadeLeft) {
			// don't turn off if already on
			if (checkConsistency && 
                (!mReverseRecord || (mReverseRecord && containsDeferredFadeLeft)))
			  Trace(this, 1, "Layer: inconsistent deferred fade left\n");
			mContainsDeferredFadeLeft = containsDeferredFadeLeft;
		}
	}

    if (mContainsDeferredFadeRight != containsDeferredFadeRight) {
		if (mNoFlattening)
		  mContainsDeferredFadeRight = containsDeferredFadeRight;
		else if (containsDeferredFadeRight) {
			if (checkConsistency && 
                (mReverseRecord || (!mReverseRecord && containsDeferredFadeRight)))
			  Trace(this, 1, "Layer: inconsistent deferred fade right\n");
			mContainsDeferredFadeRight = containsDeferredFadeRight;
		}
	}

	// If we decide to turn off one of the contained deferred fade flags
	// due to occlusion, this may in turn force the application of a
	// local deferred fade because we no longer have a seamless recording.
	// WAIT! Cannot do this here even though it seems to make sense.
	// Segment fades are compiled when we begin preplay which may
	// involve capturing a fade tail.  If we apply the deferred fade now
	// we'll cause a break because we did actually play seamlessly from the
	// previous layer, even though it will be occluded on the next pass.
	// Have to leave this to finalize.
#if 0	
	if (!mReverseRecord && mDeferredFadeLeft && !mContainsDeferredFadeRight)
	  applyDeferredFadeLeft();
	
	if (mReverseRecord && mDeferredFadeRight && !mContainsDeferredFadeLeft)
	  applyDeferredFadeRight();
#endif

	// One more pass to compare the ending segment fades with what we
    // thought they should be when we started.  Not really important if we're
    // not flattening, but if we were, this may have caused a content error.
	// This is actually hard to enforce if we're not flattening,
	// since for example when we finish occluding the trailing edge,
	// occlude() isn't smart enough to search for the leading edge
	// layer and tell it it to fade.  We're catching that here so
	// don't warn.

	if (!mNoFlattening && checkConsistency) {
		for (s = mSegments ; s != NULL ; s = s->getNext()) {
			if ((s->isFadeLeft() != s->isSaveFadeLeft()) ||
				(s->isFadeRight() != s->isSaveFadeRight())) {
				Trace(this, 1, "Layer: Inconsistent segment fade detected during compilation!\n");
			}
		}
	}

	Trace(this, 3, "Layer: Compiled segment fades: %ld %ld %ld %ld\n",
		  (long)mDeferredFadeLeft, (long)mContainsDeferredFadeLeft, 
		  (long)mDeferredFadeRight, (long)mContainsDeferredFadeRight);
}

/**
 * Called when we detect that we're playing this layer for the first time.
 * This will happen shortly before the shift when we begin preplay
 * of the record layer.  It will also happen when we begin playing
 * a layer that was loaded from a project.
 */
PUBLIC void Layer::prepare(LayerContext* con)
{
	if (!mPlayable) {

		if (ScriptBreak) {
			int x = 0;
		}

		// must do this when bootstrapping layers read from a Project file
		if (mFrames == 0) {
			mFrames = calcFrames();
			setFrames(NULL, mFrames);	// resize Audios
		}

        // Note that if we're pre-playing the record layer, and we're not
        // flattening, this may detect an occlusion and apply the deferred
        // leading fade.  This will not happen during flattening.  Since
        // we're not completely done with the layer yet, may want to 
        // pass this in so we don't emit warnings that we'll correct later?
		compileSegmentFades(true);

		mPlayable = true;
	}
}

/**
 * Called by Loop when we're about to reenter this layer after an 
 * undo or redo.  
 */
PUBLIC void Layer::restore(bool undo)
{
    if (undo) {
        // always apply the trailing deferred fade
        if (mReverseRecord)
          applyDeferredFadeLeft();
        else
          applyDeferredFadeRight();

        compileSegmentFades(true);
    }
    else {
        // redo
        if (mNoFlattening) {
            // Since we must have applied the deferred trailing fade in
            // the previous layer, we have to applly the leading fade
            // if we return to this layer.  If flattening, we don't have
            // to since the trailing fade will have been copied into this
            // layer before the undo altered it.
            if (mReverseRecord)
              applyDeferredFadeRight();
            else
              applyDeferredFadeLeft();

            // Since the previous layer may have had its deferred trailing
            // fade applied, have to factor that into our segment fades.
            compileSegmentFades(true);
        }
        else {  
            // If we didn't finish copying the previous layer, it will
            // have had its trailing fade applied, and we have to then
            // apply our leading fade, just like in the previous case when
            // flattening is disabled.  

            // Have to look at the segments to detect this.  
            // Ugh, this is the same crap we do in compileSegmentFades,
            // try to move it in there?
            bool trailingEdgeReference = false;
            for (Segment* s = mSegments ; s != NULL ; s = s->getNext()) {
                Layer* refLayer = s->getLayer();
                if (refLayer == mPrev) {
                    long segFrames = s->getFrames();
                    long localStart = s->getOffset();
                    long localEnd = localStart + segFrames;
                    long refStart = s->getStartFrame();
                    long refEnd = refStart + segFrames;
                    long refTotal = refLayer->getFrames();

                    if ((mReverseRecord && 
                         (localStart == 0 && refStart == 0)) ||
                        (!mReverseRecord && 
                         (localEnd == mFrames && refEnd == refTotal))) {

                        trailingEdgeReference = true;
                        break;
                    }
                }
            }

            if (trailingEdgeReference) {
                if (mReverseRecord)
                  applyDeferredFadeRight();
                else
                  applyDeferredFadeLeft();

                compileSegmentFades(true);
            }
        }
    }
}

/**
 * Called by Stream when it jumps from the play layer to the record layer.
 * We are about to begin playing the current layer and need to know
 * the level of feedback that will be applied to the layer we are just
 * leaving.  If the level is less than 127, Stream must capture an adjusted
 * fade tail from the beginning of the previous layer and merge it
 * with the beginning of this layer so the transition is smooth.  This
 * is a playback only adjustment, it is not recorded into the layer.
 *
 * When flattening is enabled, this is simply the feedback level when we
 * started recording this layer.
 *
 * When not flattening it is more complicated.  We have to locate the segment
 * that contains the start of the previous layer and return the feedback
 * level that was last active when we passed over that segment.  If there
 * were no structural changes, there will still be only one segment and
 * we're not done passing over it yet.  In that case, we have to make sure
 * that the feedback will not change for the remainder of the recording
 * into this layer.  We do not actually need to lock it here, we can assume
 * that if the layer is prepared for playing that feedback changes stop,
 * this is handled in advanceInternal.
 *
 * !! TODO: If we decide to put back support for AUTO_FEEDBACK_LEVEL,
 * this is where it should go when not flattening.
 * Check MobiusConfig::isAutoFeedbackReduction
 *
 */
PUBLIC int Layer::lockStartingFeedback()
{
    int level = mStartingFeedback;
    if (mNoFlattening) {
        if (mPlayable) {
			// hmm, we've already started playing, shouldn't be locking now?
            Trace(this, 1, "Layer: Redundant feedback lock\n");
        }

		// Locate the segment covering the start of the previous layer,
		// in a general segment model there could be several of these but
		// we'll assume only one for now.  Note that if the layer was recorded
		// in reverse, we look for the segment covering the end of the layer.

		for (Segment* s = mSegments ; s != NULL ; s = s->getNext()) {
			if ((!mReverseRecord && s->isAtStart(this)) ||
				(mReverseRecord && s->isAtEnd(this))) {
				level = s->getFeedback();
				break;
			}
		}
		mStartingFeedback = level;
	}
    return level;
}

/****************************************************************************
 *                                                                          *
 *   								 COPY                                   *
 *                                                                          *
 ****************************************************************************/

/**
 * Called when loading projects, or processing a LoopCopy=Sound.
 */
Layer* Layer::copy()
{
	Layer* neu = mLayerPool->newLayer(mLoop);
    neu->copy(this);
	return neu;
}

/**
 * Called to create a new record layer after shifting, undo, auto-undo,
 * or cancelling rehearse.
 */
void Layer::copy(Layer* src)
{
	reset();
	if (src != NULL) {
		Segment* seg = addSegment(src);
		mCycles = src->getCycles();
		mFrames = calcFrames();
		// resize the local Audio
		setFrames(NULL, mFrames);

        // roll these forward
        mContainsDeferredFadeLeft = src->hasDeferredFadeLeft();
        mContainsDeferredFadeRight = src->hasDeferredFadeRight();
	}

	// a copy initializes change status
	mStructureChanged = false;
	mAudioChanged = false;

	// seeing occasional copy frame mismatches, looked like
	// it might be a power of two boundary in Audio?
	if (src->getFrames() != getFrames()) {
		Trace(this, 1, "Layer: Frame count mismatch after copy, %ld expecting %ld\n",
			  getFrames(), src->getFrames());
	}
}

/**
 * Debugging utility to save the layer contents to a file.
 * This is not flattened.
 */
void Layer::save(const char* file) 
{
	if (mAudio != NULL)
	  mAudio->write(file);
}

/****************************************************************************
 *                                                                          *
 *   								 PLAY                                   *
 *                                                                          *
 ****************************************************************************/

/**
 * Warp the frame if we're in virtual reverse
 * Subtlety: if we're empty, then the reflection of 0 will be -1.
 * Sounds illogical but it's symetrical and temporary.  
 * This can happen in AudioCursor but shouldn't here since we only
 * reflect when playing.
 *
 * NOTE WELL: Use the Audios frame counter rather than the local 
 * frame counter.  During the inital recording our frame counter
 * will stay at zero, and Loop uses that to tell if we're in the
 * initial recording.  I don't like this convention, but it will
 * be difficult to change.
 */
PRIVATE long Layer::reflectFrame(LayerContext* con, long frame) 
{
	if (con->isReverse())
	  frame = mAudio->getFrames() - frame - 1;
	return frame;
}

/**
 * Variant of reflectFrame that calculates the start of the
 * reflected regin.
 */
PRIVATE long Layer::reflectRegion(LayerContext* con, long frame, long frames) 
{
	if (con->isReverse()) {
		// reflect to get to the end of the region
		frame = mAudio->getFrames() - frame - 1;
		// then back up to the start
		frame = frame - frames + 1;
	}
	return frame;
}

/**
 * Retrieve a block of frames.
 * This is the public method called by Loop.  Calls another
 * method to do the work passing in a flag indicating that we're
 * playing rather than copying.
 */
PUBLIC void Layer::play(LayerContext* con, long startFrame, bool fadeIn)
{
	prepare(con);
	if (fadeIn) {
        // If you're watching the trace frame on this, it will usuallky look 
        // highewr than you expect because the trace frame is taken from
        // the loop's record frame which has already been advanced for this
        // block. We're now in the process of advancing the play frame
        // by the same amount.
        // UPDATE: These started happening all the time after
        // SpeedBend so lower the level to 3
		Trace(this, FadeTraceLevel, "Layer: Starting play fade in at %ld\n", startFrame);
		// should have prevented this with cancelPlayFadeIn
		if (mFade.enabled)
		  Trace(this, 1, "Layer: fade already active\n");
		mFade.activate(true);
		mFade.startFrame = startFrame;
	}

    get(con, startFrame, true);
}

/**
 * Retrieve a block of frames.
 *
 * The root flag is set only if we're being called from current layer,
 * as we descend into Segments, this will be false.
 *
 * The play flag is on if we're retrieving frames for playback.
 * If the flag is false, we're copying frames from our backing
 * layers to our local Audio object.
 *
 * If we're in reverse, the region must be reflected before passing
 * it to the Segments.
 * 
 * There may be a transient play fade in that we apply after we've
 * flattened the content.  Note that we process this without reflection.
 * The content of the return buffer has already been properly reversed,
 * what we're supposed to do is fade in whatever we end up with.  startFrame
 * is used only to tell us how many frames we've already faded.
 * 
 */
void Layer::get(LayerContext* con, long startFrame, bool play)
{
	// reflect the region
    long reflectedStart = reflectRegion(con, startFrame, con->frames);

	// root flag is true only for the topmost layer
	// once we descend into Segments, they will call getNoReflect
	getNoReflect(con, reflectedStart, NULL, true, play);

	// After flattening the content, process the transient play fade.
	// Play fades are ONLY done if we're using the play cursor, there
	// are no fades when we copy.

	if (mFade.enabled && play) {
		// if this is the first time, remember the startFrame so we
		// can tell how far we are into the fade
		mFade.fade(con, startFrame);
	}
}

/**
 * Inner implementation to retrieve frames from a reflected region.
 *
 * Normally we merge the local Audio with the segments.
 * The one exception is when we're copying in the topmost layer.
 * There we only want to traverse segments not include local Audio because
 * we're trying to copy INTO the local Audio.
 *
 * KLUDGE: Normally we use local cursors, but when saving a layer
 * from the UI thread we also have to flatten the layer without
 * distrupting playback.  In that case, a cursor may be pased in.
 *
 */
void Layer::getNoReflect(LayerContext* con, long startFrame, 
						 AudioCursor* cursor, bool root, bool play)
{
    // We may need to adjust the buffer pointer and length for
    // each segment, but want to keep all the other options.
    // Remember and restore the original values.
    float* buffer = con->buffer;
    long frames = con->frames;

	// include local audio unless we're copying into the root layer
	if (!root || play) {
		// Since we have a reflected region, we have to calculate
		// the end frame since AudioCursor iterates in reverse.
		long audioFrame = startFrame;
		if (con->isReverse())
		  audioFrame = startFrame + con->frames - 1;

		// if cursor supplied use it, otherwise pick a local one
		// do not trash the argument so we don't send a local
		// cursor down into the segments
		AudioCursor* localCursor = cursor;
		if (localCursor == NULL)
		  localCursor = ((play) ? mPlayCursor : mCopyCursor);

		localCursor->setReverse(con->isReverse());
		localCursor->get(con, mAudio, audioFrame, con->getLevel());
	}

    if (mSegments != NULL) {
        long endFrame = startFrame + frames - 1;
        for (Segment* seg = mSegments ; seg != NULL ; seg = seg->getNext()) {
            long segFrames = seg->getFrames();
            long relFirst = seg->getOffset();
            long relLast = relFirst + segFrames - 1;

			if (relFirst <= endFrame && relLast >= startFrame) {

                // at least some portion is within range

                long segStart = 0;
				long destOffset = 0;

                if (relFirst < startFrame ) {
                    // truncate on the left 
                    segStart = startFrame - relFirst;
                    segFrames -= segStart;
                }
                else {  
                    // segment is at or after startFrame, shift
                    // the output buffer destination
					destOffset = relFirst - startFrame;
                }

				// truncate on the right
				long destEnd = destOffset + segFrames;
				if (destEnd > frames) {
					segFrames = frames - destOffset;
					destEnd = frames;
				}

				// If we're in reverse, Segment will handle filling
				// the frames in reverse order, but we need to reflect
				// the output buffer destination region.  The distance
				// of the segment's last frame from the end of the output
				// buffer becomes the distance of the segment's first
				// frame from the start of the output buffer.
				// The first shall be last and the last shall be first.

				if (con->isReverse())
				  destOffset = frames - destEnd;

				float* segDest = (buffer + (destOffset * con->channels));
                con->buffer = segDest;
                con->frames = segFrames;

                seg->get(con, segStart, cursor, play);
            }
        }
    }

    // restore the original values
    con->buffer = buffer;
    con->frames = frames;
}

/**
 * Create a new Audio object by flattening all of the segments in a layer.
 * Used in the implementation of save loop.
 * 
 * This could be expensive so it should not be called within the interrupt.
 * Normally called only from the UI thread.
 *
 * Assume we don't need to be affected by reverse, we're returning the true
 * content.
 *
 * Be careful not to use the playback cursor because we could
 * be playing right now.  Have to make our own private cursor.
 *
 * Note that the size of the buffer must be the same as that used
 * by the audio interrupt, the fade code in Segment::get depends
 * on this so it can allocate a stack buffer.
 *
 * This is usually being run from the MobiusThread, in theory
 * there can be concurrency issues with the interrupt handler but the
 * play layer shouldn't be modified.  
 * !! Hmm, we really can't assume that, several functions could
 * cause the play layer to be modified including Reset.  You have
 * to be careful to wait a bit after using the Save Loop function
 * before resetting the loop!
 * 
 */
PUBLIC Audio* Layer::flatten()
{
	Audio* flat = mAudioPool->newAudio();
	AudioCursor* cursor = new AudioCursor("flatten", NULL);
	float buffer[AUDIO_MAX_FRAMES_PER_BUFFER * AUDIO_MAX_CHANNELS];

    // in case we decide to save this in a project, set the
    // right sample rate
    flat->setSampleRate(mLoop->getMobius()->getSampleRate());

	LayerContext con;
	con.buffer = buffer;
	con.frames = AUDIO_MAX_FRAMES_PER_BUFFER;

	long frame = 0;
	long remaining = getFrames();
	long chunk = con.frames;

	while (remaining > 0) {
		if (remaining < chunk) {
			chunk = remaining;
			con.frames = remaining;
		}
		
		memset(buffer, 0, sizeof(buffer));
		getNoReflect(&con, frame, cursor, true, true);
		flat->put(&con, frame);

		frame += chunk;
		remaining -= chunk;
	}

	delete cursor;
	return flat;
}

/**
 * Capture a fade tail from a specified location.
 * The supplied buffer will be at least as long as 
 * AudioFade::Range * the maximum number of channels.
 *
 * adjust is normally 1.0, but may be less if we're capturing
 * a tail to level out a feedback reduction when moving from the play
 * layer to the record layer.  Since the tail will be taken from
 * the beginning of the play layer it will be combined with a copy of that
 * same content at the beginning of the record layer.  We have
 * to adjust the level of the tail to factor in the level that already
 * exists in the copy.  The result is that the background head will be
 * raised to match the level of the previous layer tail.
 */
PUBLIC long Layer::captureTail(LayerContext* con, long playFrame, 
                               float adjust)
{
    long tailFrames = AudioFade::getRange();
	long remainder = mFrames - playFrame;

    if (remainder < 0) {
        // something isn't right
        Trace(this, 1, "Layer: captureTail: negative remainder\n");
		tailFrames = 0;
    }
	else if (remainder == 0 && 
			 !((con->isReverse() && hasDeferredFadeLeft()) ||
			   (!con->isReverse() && hasDeferredFadeRight()))) {
		// we're at the edge and have already faded to zero
	}
    else {
        float* tailStart = con->buffer;
        long remainderFrames = tailFrames;
		long overflowFrames = 0;
		bool fade = true;

        if (remainder < tailFrames) {
            remainderFrames = remainder;
			if ((con->isReverse() && !hasDeferredFadeLeft()) ||
				(!con->isReverse() && !hasDeferredFadeRight())) {
				// already faded, capture as much as we can
				fade = false;
			}
			else
			  overflowFrames = tailFrames - remainder;
		}

        // these started happening a lot after continuous speed shift
        // so drop the level to 3
		if (overflowFrames == 0) {
			Trace(this, FadeTraceLevel, "Layer: Capture fade tail, %ld frames at %ld\n",
				  remainderFrames, playFrame);
			con->frames = remainderFrames;
			play(con, playFrame, false);
		}
		else {
			Trace(this, FadeTraceLevel, "Layer: Capture fade tail, %ld frames at %ld then wrap %ld\n",
				  remainder, playFrame, overflowFrames);
			if (remainderFrames > 0) {
				con->frames = remainderFrames;
				play(con, playFrame, false);
				con->buffer += (remainderFrames * con->channels);
			}
			con->frames = overflowFrames;
			play(con, 0, false);
		}

		if (fade)
		  AudioFade::fade(tailStart, con->channels, 0, tailFrames, 
						  0, false, adjust);
	}

	return tailFrames;
}

/****************************************************************************
 *                                                                          *
 *   								RECORD                                  *
 *                                                                          *
 ****************************************************************************/
/*
 * When we say a layer is being "recorded" one of three methods will
 * be called by Loop.
 *
 *    record - during overdub
 *    insert - during insert
 *    advance - when not recording
 *
 * One of these three MUST be called for each audio interrupt to make sure 
 * that the backing layer is copied to the local Audio, even if we don't
 * happen to be recording any new audio.
 */

/**
 * While recording, keep track of the maximum sample we encounter.
 * Used to determine if we really need to keep an overdub loop for undo.
 */
PRIVATE void Layer::watchMax(LayerContext* con)
{
	float* src = con->buffer;
	long frames = con->frames;
	if (src != NULL) {
		int samples = frames * 2;
		for (int i = 0 ; i < samples ; i++) {
			float sample = src[i];
			if (sample < 0)
			  sample = -sample;
			if (sample > mMax)
			  mMax = sample;
		}
	}
}

/**
 * Called during normal or Replace mode recording.
 * The Audio and AudioCursor objects handle most of the work.
 *
 * If we're replacing, it's more complicated.  The layer must be divided
 * into two segments, the first from the beginning to the replace point, 
 * and the other from the replace point to the end.  Then, as we
 * replace contiguous frames, the beginning of the segment segment
 * is incremented to "hide" the frames being recorded locally.
 * This process can occur several times.  
 * 
 * TODO: In Substitute mode when InterfaceMode=Stutter, we apply
 * secondary feedback to the underlying segment rather than replacing it.
 * Feels like a generally useful thing to have outside of Stutter mode.
 * UPDATE: We no longer have InterfaceMode, but this still sounds
 * like a useful thing.
 *
 */
void Layer::record(LayerContext* con, long startFrame, int feedback)
{
	// if we dropped feedback suddenly, and aren't already sliding feedback,
	// and it looks like current feedback is "full", simulate segment occlusion
	if (feedback == 0 && !mSmoother->isActive() &&
		mFeedback >= AUTO_FEEDBACK_LEVEL &&
		!mNoFlattening && SimulateSegmentReplace) {

		fadeBackground(con, startFrame);
		forceFeedback(0);
	}

    checkRecording(con, startFrame);

	// copy from the backing layer, this method does its own reflection
	advanceInternal(con, startFrame, feedback);

	// Occlude the backing layer if this is a replace, 
	// if flattening is enabled, advance will already have done this.
	if (mNoFlattening && feedback == 0) {
		// occlusion start must be a reflected *region* not just a frame
		long occludeStart = reflectRegion(con, startFrame, con->frames);
		occlude(occludeStart, con->frames, false);
		mStructureChanged = true;
	}

	// now reflect the frame for the Audio puts
	startFrame = reflectFrame(con, startFrame);

	// finally save the new audio, AudioCursor will handle copying in reverse
	// do NOT increment mFrames, Loop depends on this remaining zero
	// to know that we haven't finished the initial record

	mRecordCursor->setReverse(con->isReverse());
	mRecordCursor->put(con, OpAdd, startFrame);
    // NOTE: the Isolated Overdub parameter was experimental and no longer exposed
	if (mIsolatedOverdub) {
		mOverdubCursor->setReverse(con->isReverse());
		mOverdubCursor->put(con, OpAdd, startFrame);
	}

	if (mPendingFrames > 0) {
		mPendingFrames -= con->frames;
		if (mPendingFrames < 0) {
			Trace(this, 1, "Layer: pending frame miscalculation\n");
			mPendingFrames = 0;
		}
	}

	watchMax(con);
	mAudioChanged = true;

}

/**
 * Force feedback to a value without a gradual shift.
 */
void Layer::forceFeedback(int level)
{
	mFeedback = level;
	mSmoother->setValue(AudioFade::getRampValue(level));
}

/**
 * Called by Loop when we need to advance the coping of the previous
 * layer into the new layer, but without recording any new content.
 */
void Layer::advance(LayerContext* con, long startFrame, int feedback)
{
	// If we're not recording when we enter the loop, still prepare
	// the windows and detect the initial direction.
    if (!mStarted) {
		mReverseRecord = con->isReverse();
		mStarted = true;
	}

	if (con->frames > 0) {
		// there is by definition a recording gap so fade now
		fadeOut(con);
	}

	advanceInternal(con, startFrame, feedback);
}

/**
 * Called internally by record() and advance() to perform the copy
 * before adding the new material.
 *
 * A copy is performed by "playing" ourselves using the copy cursor
 * rather than the play cursor.  This will traverse the Segment hierarchy
 * and leave the result in the local Audio object.
 *
 * In reverse, we need to be copying from the end of the backing layer,
 * but we don't need to be reversing the content.  We simply reflect
 * the region, and copy the unreversed region over to the local audio.
 * We could also just do an internal get/put normally but it
 * would be more work, get would return reversed frames, and put would
 * reverse them again.  The result is the same as if we didn't reverse at all.
 *
 * It will however result in more churn in the feedback cursor since
 * we're going to be jumping backwards on each call.  Shouldn't be
 * that significant, and still cheaper than going through content reflection.
 */
void Layer::advanceInternal(LayerContext* con, long startFrame, int feedback)
{
	if (ScriptBreak) {
		int x = 0;
	}

	// if we're not going to advance, don't trip the feedback tracking logic
	if (con->frames == 0) return;

    // Remember the initial feedback level so we can adjust edge
    // fades when the layer is finalized
    if (startFrame == 0) {
		mStartingFeedback = feedback;
		// and can jump directly there?
		forceFeedback(feedback);

		// set the starting feedback on all segments, this may change
		// as we progress
		if (mNoFlattening) {
			for (Segment* s = mSegments ; s != NULL ; s = s->getNext())
			  s->setFeedback(mFeedback);
		}
	}

	// keep track of the last frame we copied, assumes we can't jump backwards
	mLastFeedbackFrame = startFrame + con->frames;

	if (mNoFlattening) {
		// remember running feedback level for finalize
		if (!mPlayable) {
			forceFeedback(feedback);
		}

		long occludeStart = reflectRegion(con, startFrame, con->frames);

		// !! should we be using feedback or mFeedback here, need
		// to be consistent with the next clause
		if (feedback == 0) {
			// start truncating segments, leaving the existing feedback
			occlude(occludeStart, con->frames, false);
			mStructureChanged = true;
		}
		else {
			// for each segment we are passing over, adjust the feedback
			long occludeLast = occludeStart + con->frames - 1;
			for (Segment* s = mSegments ; s != NULL ; s = s->getNext()) {
				long segFirst = s->getOffset();
				long segFrames = s->getFrames();
				long segLast = segFirst + segFrames - 1;

				if (segFirst <= occludeLast && segLast >= occludeStart) {
					// we are "over" this segment
					s->setFeedback(mFeedback);
				}
			}
		}
	}
	else if (mSegments == NULL) {
		// nothing to flatten, just keep track of the feedback for finalize
		forceFeedback(feedback);
	}
	else if (con->frames > AUDIO_MAX_FRAMES_PER_BUFFER) {
		// Could handle this by making several passes using the CopyCursor
		// but it complicates things and shouldn't happen.  Segmenting
		// the interrupt buffer should be handled by Recorder or Track,
		// not at this level.
		Trace(this, 1, "Layer: Unable to flatten layers, buffer to large");
	}
	else {
		// Make it back off significantly before forcing a shift since
		// we may be using automatic feedback reduction during overdub
		if (feedback < AUTO_FEEDBACK_LEVEL)
		  mFeedbackApplied = true;

		// reflect the region in reverse
		long regionStart = reflectRegion(con, startFrame, con->frames);
		long regionFrames = con->frames;
		long copyStart = regionStart;
		long copyFrames = regionFrames;

		// first copy into a temporary buffer applying feedback adjustments
		LayerContext* cc = mLayerPool->getCopyContext();
		float* copyBuffer = cc->buffer;
		memset(copyBuffer, 0, sizeof(float) * (regionFrames * con->channels));
		cc->setLevel(mSmoother->getValue());

		mSmoother->setTarget(feedback);
		if (mSmoother->isActive()) {

			// Copy one frame at a time until the feedback adjusts.
			cc->frames = 1;

			// In reverse, the "fade" is applied to the end of the reflected
			// region.
			if (con->isReverse()) {
				long feedFrame = regionStart + regionFrames - 1;
				cc->buffer = &copyBuffer[(regionFrames - 1) * con->channels];
				while (copyFrames > 0 && mSmoother->isActive()) {
					get(cc, feedFrame, false);
					feedFrame--;
					copyFrames--;
					mSmoother->advance();
					cc->setLevel(mSmoother->getValue());
					cc->buffer -= con->channels;
				}
				cc->buffer = copyBuffer;
			}
			else {
				while (copyFrames > 0 && mSmoother->isActive()) {
					get(cc, copyStart, false);
					copyStart++;
					copyFrames--;
					mSmoother->advance();
					cc->setLevel(mSmoother->getValue());
					cc->buffer += con->channels;
				}
			}
		}

		// !! can we go there yet, what if the smoother hasn't
		// finished due to block size
		mFeedback = feedback;

		// copy the remainder after feedback ramping
		if (copyFrames > 0) {
			cc->frames = copyFrames;
			get(cc, copyStart, false);
		}

		// restore the beginning of the buffer and add it to this layer
		cc->buffer = copyBuffer;
		cc->frames = regionFrames;
		mFeedbackCursor->put(cc, OpAdd, mAudio, regionStart);

		// Now adjust the segments so that the portion we just copied
		// is no longer included, set the noFade flags since the
		// surrounding content is seamless
		occlude(regionStart, regionFrames, true);
	}
}

/**
 * Helper for Replace mode (feedback == 0) and incremental flattening.
 * Restructure the segment list to occlude a region of continguous
 * frames.  startFrame & frames must already be a reflected *region* if
 * we're in reverse.
 *
 * The "seamless" flag will be true if content from the
 * segments being occluded has been copied into our local Audio.  When
 * that happens we record the amount in the segment so we know that
 * we don't need a fade on that edge since the local audio will
 * have the correct adjacent content.
 * 
 * I considered allowing the occlusion level to be variable, by setting
 * feedback levels on the Segments, but this gets really complicated and
 * results in unwanted fades between adjacent segments that were created
 * only to have different feedback levels.  It will result in
 * memory churn since we'll be constantly carving out the front of a segment
 * on each audio interrupt to change its level, if we're not also careful
 * to congeal adjacent segments with the same level.
 * 
 * This is really only necessry to occlude a block of frames in Replace
 * mode, or for a block of frames that have now been copied into the local
 * Audio object.  Variable feedback if it were necessary was applied during
 * the copy, we don't need it here.
 *
 * Segment Fades:
 *
 * If we're flattening, then trimming the segments will increment their
 * copy counts which will then disable fades.
 *
 * If we're not flattening, trimming will enable fades which is usually
 * what we want.  In theory, we could go back in later and move the
 * startFrame back, or add a segment that referenced the adjacent content,
 * this will be handled in compileSegmentFades.
 *
 * NOTE: If we're flattening and feedback is lowered, there can still
 * be significant level differences if we retrigger and leave segments
 * behind before the copy is complete.  This is partially addressed in
 * finalize() which will set the ending feedback level on any remaining
 * segments, but if we wanted to support jumping around at random, it
 * would be more complicated.
 * 
 * NOTE: If a segment collapses to zero, and there is an adjacent segment
 * on either side, the local copy count from the segment we're collapsing
 * needs to be copied to the adjacent segment so we can properly compile fades.
 * In practice this only happens in stutter mode, but could also happen
 * if we were randomizing.  An alternative is to automaticaly coalesce
 * adjacent segments as they are added under the assumption that they may
 * get shorter, but won't get longer.  That's easier so we'll do that for now.
 * See coalesce.
 */
void Layer::occlude(long startFrame, long frames, bool seamless)
{
	Segment* next = NULL;
	long lastFrame = startFrame + frames - 1;
	for (Segment* s = mSegments ; s != NULL ; s = next) {
		next = s->getNext();
		long segFirst = s->getOffset();
		long segFrames = s->getFrames();
		long segLast = segFirst + segFrames - 1;

		if (segFirst >= startFrame && segFirst <= lastFrame) {
			// truncate on the left
			long replaced = lastFrame - segFirst + 1;
			if (replaced < segFrames) {
				s->trimLeft(replaced, seamless);
			}
			else {
				// the segment is entirely occluded
				// fades.  
				// 
				removeSegment(s);
				delete s;
			}
		}
		else if (segLast >= startFrame && segLast <= lastFrame) {
			// truncate on the right
			long replaced = segLast - startFrame + 1;
			if (replaced < segFrames) {
				s->trimRight(replaced, seamless);
			}
			else {
				// the segment is entirely occluded
				removeSegment(s);
				delete s;
			}
		}
		else if (segFirst <= lastFrame && segLast >= startFrame) {
			// split in two
			// note that we can't clone local segment Audio yet
			Segment* clone = new Segment(s);
			addSegment(clone);
				
			// replace everything after the startFrame
			s->trimRight(segLast - startFrame + 1, seamless);

			// replace everything before startFrame
			clone->trimLeft(lastFrame - segFirst + 1, seamless);
		}
	}

	// If we're occluding from the left then we can no longer
	// contain a deferred left fade.   This is just for a consistency
	// check later in compileSegmentFades, though in theory we could
	// drop in another segment later that contains another deferred fade.
	// This applies only if the seamless flag is off, meaning that
	// we're doing a true occlusion not just trimming segments after 
	// flattening.
	if (!seamless) {
		if (startFrame == 0)
		  mContainsDeferredFadeLeft = false;
		else if ((startFrame + frames) == mFrames)
		  mContainsDeferredFadeRight = false;
	}
}

/**
 * Make a pass over the segments looking for those that are logicaly
 * adjacent and merging them.  In practice this is necessary
 * after a stutter, but may eventually be useful for other special functions.
 * 
 * This avoids a fade compilation problem when we finish copying 
 * a segment, remove it, but now we think the adjacent segment needs a
 * fade because it has a zero local copy count.  We could also be handling
 * this in occlude(), transfering the copy counts as segments are collapsed, 
 * but it's easier to coalesce.
 *
 * Because the segments can be in any order, have to make multiple passes
 * until we can coalesce no more.
 *
 * NOTE: If the feedback of the adjacent segments differ, then arguably
 * we should not be coalescing.
 */
void Layer::coalesce()
{
	Segment* next;
	int coalesced;

	do {
		coalesced = 0;
		for (Segment* s = mSegments ; s != NULL ; s = s->getNext()) {

			Layer* refLayer = s->getLayer();
			long segFrames = s->getFrames();
			long localStart = s->getOffset();
			long localEnd = localStart + segFrames;
			long refStart = s->getStartFrame();
			long refEnd = refStart + segFrames;

			for (Segment* s2 = mSegments ; s2 != NULL ; s2 = next) {
				next = s2->getNext();
				if (s != s2 && refLayer == s2->getLayer()) {

					long segFrames2 = s2->getFrames();
					long localStart2 = s2->getOffset();
					long localEnd2 = localStart2 + segFrames2;
					long refStart2 = s2->getStartFrame();

					if (localEnd == localStart2 && refEnd == refStart2) {
						// adjacent on the right

						// some sanity checks
						if (s->getLocalCopyRight() > 0)
						  Trace(this, 1, "Layer: Unusual adjacent segments 1\n");
						if (s2->getLocalCopyLeft() > 0)
						  Trace(this, 1, "Layer: Unusual adjacent segments 2\n");

						s->setFrames(segFrames + segFrames2);
						s->setLocalCopyRight(s2->getLocalCopyRight());
						s->setFadeRight(s2->isFadeRight());
						removeSegment(s2);
						delete s2;
						coalesced++;
						// all the local info extracted above is now wrong,
						// could adjust it, but since we're making multiple
						// passes it's easier just to restart
						break;
					}
				}
			}
		}

	} while (coalesced > 0);

}

/**
 * Called by Loop when we enter a "paused" state in the record layer.
 * Even though we may resume recording at the last known frame, it won't 
 * be seamless audio since we will have ignored some portion of the live
 * stream while paused.  Need to treat this as a non-contiguous stream
 * and fade the edges.
 *
 * Formerly tried to do the fades here, but since we'll be called many
 * times during while paused, just set a flag and let checkRecording
 * deal with it when we wake up.
 */
PUBLIC void Layer::pause(LayerContext* con, long startFrame)
{
	mPaused = true;
}

/****************************************************************************
 *                                                                          *
 *   							   MULTIPLY                                 *
 *                                                                          *
 ****************************************************************************/

/**
 * Add a cycle during Multiply.  Called by Loop as it processes a LoopEvent.
 *
 * This simply adds another segment reference to the play layer.
 * It assumes that if you had made any modifications to the content
 * preceeding the multiply, that Loop will have done a layer shift, so
 * we'll now be referencing the modified content.
 * 
 * startFrame has frame in this layer where the multiply started.  This
 * is used to locate the cycle from the source layer to add since there
 * may be several cycles in the source layer.  
 *
 * I first implemented this by adding cycles beginning at the mode start
 * frame rather than frame zero.  This seemed reasonable since you
 * were starting a new life in the middle of the layer and if you wanted
 * to extend further you would return to the where you started.  This does
 * however conflict with a Multiply alternate ending to Insert which is
 * expected to include the newly inserted content in the remultiply.  
 * 
 * Might want an option for this?
 *
 * An usual reflection happens here.  The calculations are mostly
 * to determine which cycle in the source layer to reference.  If we are
 * in reverse, it is easiest to do a forward cycle calculation, then reflect
 * the cycle number rather than reflect the modeStartFrame and work backwards.
 * Reflecting a cycle is the same as reflecting a frame except the "length"
 * is the number of cycles rather than the number of frames:
 *
 *     reflectedCycle = totalCycles - cycle - 1
 * 
 */
void Layer::multiplyCycle(LayerContext* con, Layer* src, long modeStartFrame)
{
	Segment* cycle = new Segment(src);
	long cycleFrames = getCycleFrames();

	// the base of the first cycle in the source layer
	// see commentary above on why this has to be zero
	//int baseCycle = modeStartFrame / cycleFrames;
	int baseCycle = 0;

	// number of source cycles we have to work with
	int availCycles = src->getCycles() - baseCycle;

	// relative number of the new cycle
	// assumes we're always larger than the backing layer
	int relCycle = mCycles - src->getCycles();

	// source cycle corresponding to the new cycle
	int srcCycle = baseCycle + (relCycle % availCycles);

	// normally goes at the end
	long offset = mFrames;

	// open a cycle at the end (or front if reversing)
	// this will do its own reflection
	insertCycle(con, offset);

	if (con->isReverse()) {
		// new segment starts at the beginning
		offset = 0;
		// reflect the cycle, note that we have to use the cycle
		// count of the source layer, not this layer
 		srcCycle = src->getCycles() - srcCycle - 1;
	}

	cycle->setOffset(offset);
	cycle->setStartFrame(srcCycle * cycleFrames);
	cycle->setFrames(cycleFrames);
    
    adjustSegmentFades(cycle);

	addSegment(cycle);
}

/**
 * Set edge fades for a new segment added during a multiply or stutter.
 * Fades are required if the segment edge is not adjacent to a layer edge.
 * They are are also required if adjacent to a layer edge, but the
 * layer edge has a deferred fade.
 * 
 * Could be conservative and just always fade here, an extra level
 * of fading is usually not noticeable when flattening, though it could
 * be bad if there are multiple levels of them when not flattening.
 */
PRIVATE void Layer::adjustSegmentFades(Segment* s)
{
    Layer* layer = s->getLayer();

    if (layer->hasDeferredFadeLeft() || s->getStartFrame() > 0)
        s->setFadeLeft(true);

    if (layer->hasDeferredFadeRight() ||
        ((s->getStartFrame() + s->getFrames()) < layer->getFrames()))
        s->setFadeRight(true);
}

/**
 * Slice out a section of the layer between two points and redefine
 * the cycle length.  Used in the implementation of unrounded
 * multiply and remultiply.
 * 
 * Apply fades to the edges of the local Audio if necessary.
 */
PUBLIC void Layer::splice(LayerContext* con, long startFrame, long frames, 
						  int cycles)
{
	// Loop will already have emitted trace mesages
	int fadeRange = AudioFade::getRange();

	// startFrame & frames define a region, do a region reflection
	if (con->isReverse())
	  startFrame = reflectRegion(con, startFrame, frames);

	long endFrame = startFrame + frames;
	if (endFrame > mFrames) {
		Trace(this, 1, "Layer: splice length overflow!\n");
		endFrame = mFrames - startFrame;
	}

	// restructure the segments
	if (mSegments != NULL) {
		// this is the actual last frame number within the region
        long lastFrame = endFrame - 1;
        for (Segment* seg = mSegments ; seg != NULL ; seg = seg->getNext()) {
            long segFrames = seg->getFrames();
            long relFirst = seg->getOffset();
            long relLast = relFirst + segFrames - 1;

			if (relFirst <= lastFrame && relLast >= startFrame) {

				// at least some portion is in range of the new cycle
				seg->setUnused(false);

                if (relFirst < startFrame ) {
                    // truncate on the left 
					long delta = startFrame - relFirst;
					seg->setStartFrame(seg->getStartFrame() + delta);
					seg->setFrames(segFrames - delta);
					seg->setOffset(0);
                    seg->setLocalCopyLeft(0);
					if (seg->getStartFrame() > 0)
					  seg->setFadeLeft(true);
                }
                else {  
					// shift back
					long offset = relFirst - startFrame;
					seg->setOffset(offset);

					long copyLeft = seg->getLocalCopyLeft();
					if (copyLeft > offset) {
						copyLeft = offset;
						seg->setLocalCopyLeft(copyLeft);
					}

					if (copyLeft < fadeRange && seg->getStartFrame() > 0)
					  seg->setFadeLeft(true);
				}

                // finally truncate on the right
				bool refRight = 
					((seg->getStartFrame() + seg->getFrames()) >= 	
					 seg->getLayer()->getFrames());
					
				if (relLast > lastFrame) {
					int delta = relLast - lastFrame;
					// note that we may already have adjusted this during
					// left truncation, so be sure to get the current value
					seg->setFrames(seg->getFrames() - delta);
					seg->setLocalCopyRight(0);
					if (!refRight)
					  seg->setFadeRight(true);
				}
				else {
					// may have less copied on the right
					long maxRight = lastFrame - relLast;
					long copyRight = seg->getLocalCopyRight();
					if (copyRight > maxRight) {
						copyRight = maxRight;
						seg->setLocalCopyRight(copyRight);
					}
					if (copyRight < fadeRange && !refRight)
					  seg->setFadeRight(true);
				}

                // Finally adjust edge fades which will usually turn on.
                // In theory it is more complex if there can be other
                // adjacent segments, but in practice there won't be and
                // compileSegmentFades will detect them later.
                // ?? Is this really necessary now that we'll do a full
                // fade compilation shortly during finalize()?
                adjustSegmentFades(seg);
            }
			else {
				// segment not in range of the new cycle, can delete it
				seg->setUnused(true);
			}
        }

		// could have done this in the same loop but I didn't want to 
		// make it more complicated
		pruneSegments();
    }

	// Splice out the region of local audio
	mAudio->splice(startFrame, frames);
    // NOTE: the Isolated Overdub parameter was experimental and no longer exposed
    if (mIsolatedOverdub)
	  mOverdub->splice(startFrame, frames);

	// adjust the fade windows to reflect the truncations
	long tailShift = 0;
	if (mReverseRecord)
	  tailShift = mFrames - endFrame;
	else
	  tailShift = startFrame;

	if (tailShift < 0) {
		// one of the edges must be bogus
		Trace(this, 1, "Layer: Invalid splice end frame!\n");
		mTailWindow->reset();
	}
	else if (tailShift > 0) {
		// we're going to do a full fade so the head window is not
		// longer required, oh it may overlap a bit but since
		// we're doing both foreground and background we don't need it
		mHeadWindow->reset();

		if (mTailWindow->getFrames() > 0) {
			// this one effectively moved down after the trim
			long last = mTailWindow->getLastExternalFrame();
			last -= tailShift;
			if (last > 0)
			  mTailWindow->setLastExternalFrame(last);
			else {
				// may be harmless?
				Trace(this, 1, "Layer: Splice starts after tail window!\n");
				mTailWindow->reset();
			}
		}
	}

	// it would be unusual, but a portion of the tail window
	// may now be after the splice
	if (mTailWindow->getFrames() > 0) {
		long newFrames = mAudio->getFrames();
		long last = mTailWindow->getLastExternalFrame();
		if (last > newFrames) {
			// can this happen?
			Trace(this, 1, "Layer: Splice ends before tail window!\n");
			long windowFrames = mTailWindow->getFrames();
			windowFrames -= (last - newFrames);
			if (windowFrames <= 0)
			  mTailWindow->reset();
			else
			  mTailWindow->setFrames(windowFrames);
		}
	}

    // apply fades
	// !! the way this is implemented, we can't have a seamless recording
	// after the splice if the current record location is at the end, we
	// need to defer the local right fade (left in reverse) and adjust
	// the corresponding fade window so if we don't happen to be 
	// continuing recording into the next layer we can to a retoactive fade.
	// Actually, I think we're closer now that we did the tail
	// window adjustments above, if the tail is eactly adjacent to the
	// edge we can defer.

	if (startFrame > 0) {
		if (mReverseRecord) {
			// we'll be leaving on the left don't fade it yet
			// !! sigh, can't do this without adjusting the window
			//fadeLeft(false, true);
			//mDeferredFadeLeft = true;
			fadeLeft(true, true, 1.0f);
			mDeferredFadeLeft = false;
		}
		else {
			fadeLeft(true, true, 1.0f);
			mDeferredFadeLeft = false;
		}
		// true in both cases
		mContainsDeferredFadeLeft = false;

		// also an occlusion for a deferred fade on the right
		if (endFrame == mFrames) {
			if (mReverseRecord) {
				// fade flags also serve as foreground/background selectors
				fadeRight(mDeferredFadeRight, mContainsDeferredFadeRight, 1.0f);
				mDeferredFadeRight = false;
			}
			else {
				// we may still be continuing
				// !! can't do this yet
				//fadeRight(false, true);
				//mDeferredFadeRight = true;
				fadeRight(true, mContainsDeferredFadeRight, 1.0f);
				mDeferredFadeRight = false;
			}
			mContainsDeferredFadeRight = false;
		}
	}

	if (endFrame < mFrames) {
		// truncation on the right
		if (mReverseRecord) {
			fadeRight(true, true, 1.0f);
			mDeferredFadeRight = false;
		}
		else {
			// we'll be leaving on the right so don't fade yet
			// !! can't do this yet, see above
			//fadeRight(false, true);
			//mDeferredFadeRight = true;
			fadeRight(true, true, 1.0f);
			mDeferredFadeRight = false;
		}
		// true in both cases
		mContainsDeferredFadeRight = false;

		// also an occlusion for a deferred fade on the left
		if (startFrame == 0) {
			if (mReverseRecord) {
				// we may be continuing
				// !! can't do this yet
				//fadeLeft(false, true);
				//mDeferredFadeLeft = true;
				fadeLeft(true, mContainsDeferredFadeLeft, 1.0f);
				mDeferredFadeLeft = false;
			}
			else {
				fadeLeft(mDeferredFadeLeft, mContainsDeferredFadeLeft, 1.0f);
				mDeferredFadeLeft = false;
			}
			mContainsDeferredFadeLeft = false;
		}
	}

	mFrames = frames;
	mCycles = cycles;
	mMax = 0.0f;			// why this?
	mStructureChanged = true;
}

/**
 * Remove any segments that are marked as being unused.
 * Used to cleanup after splice.
 */
PRIVATE void Layer::pruneSegments()
{
	if (mSegments != NULL) {

		Segment* prev = NULL;
		Segment* next = NULL;

		for (Segment* s = mSegments ; s != NULL ; s = next) {
			next = s->getNext();
			if (!s->isUnused()) 
			  prev = s;
			else {
				if (prev == NULL)
				  mSegments = next;
				else
				  prev->setNext(next);
				s->setNext(NULL);
				delete s;
			}
		}
	}
}

/****************************************************************************
 *                                                                          *
 *   								INSERT                                  *
 *                                                                          *
 ****************************************************************************/
/*
 * First implementation:
 * 
 * Synthesize three new segments, one containing the contents of this
 * layer up to the insert point, one containing a new insert layer, and
 * another containing the contents of this layer after the insert point.  
 * Insert fades can be performed transiently by Segment.  
 * 
 * If all we had to deal with was a single backing segment, this would
 * be easy.  But in theory there could be more than one segment, 
 * and we may also have local Audio.  The easiest thing is to create a new
 * dummy Layer holding our current segments/audio.  Then reference this
 * layer in our side segments.  Then create a new Audio for any remaining
 * overdubs in this layer.  
 *
 * This works ok, but results in a lot of segment hierarchy if there
 * are a bunch of little SUS inserts in the same layer.
 *
 * Second implementation:
 * 
 * The local Audio is by definition either empty, or we're inserting
 * after any previous recording, we can't be inserting in front of
 * previously recorded frames because we would have shifted the loop.
 * So, all we need to do for local audio is make it bigger and append
 * like a normal recording.  We do however have to detect when we've
 * inserted one cycles worth of material, and grow the layer by another cycle.
 *
 * When the first cycle is inserted, we have to find any segments that
 * span the startFrame and split them (normally there will be only one).
 * Segments that begin on or after the startFrame have their offsets
 * increased by one cycle.  As we insert new cycles, any segments
 * after the startFrame again have their offsets increased.
 *
 * This results in no additional segment hierarchy and a cleaner 
 * implementation.  This is also exactly what we need for Stutter.
 */

/**
 * Initialize a layer insertion.
 * 
 * Immediately insert an empty cycle so the thermometer looks different
 * even though we may not end up filling the entire cycle. 
 * Remember the frame we began the insertion and keep a counter so
 * we know when we've inserted a full cycle and need to insert another.
 *
 * NOTE WELL: If there are no segments and we're at frame zero do NOT add
 * a cycle yet.  This is how LoopCopy=Timing conveys the cycle size before
 * the insert event is processed. I hate this subtlety but in practice a 
 * non timing copy insert will always be performed into a layer 
 * with a backing segment, or after frame zero.
 *
 */
PUBLIC void Layer::startInsert(LayerContext* con, long startFrame)
{
	if (mInserting) {
		// this won't happen if endInsert is called properly
		Trace(this, 1, "Layer: Multiple inserts into the same layer!\n");
	}

	// special case for LoopCopy=Timing
	if (mSegments != NULL || startFrame > 0)
	  insertCycle(con, startFrame);

	// If we're flattening, then we need to fade the background we just copied
	// Avoid this if we're exactly on a cycle boundary and the previous layer
	// does not contain a deferred fade out.  It sounds fine if we do a 
	// redundant fade, but it makes the result different from when we use 
	// segments which makes the unit tests fail.
	if (!mNoFlattening) {
		if ((startFrame % getCycleFrames() != 0) ||
			(mPrev != NULL && 
			 ((mReverseRecord && mPrev->hasDeferredFadeLeft()) ||
			  (!mReverseRecord && mPrev->hasDeferredFadeRight())))) {

			fadeBackground(con, startFrame);
		}
	}

	mInserting = true;
	mInsertRemaining = getCycleFrames();
}

/**
 * Apply a fade to the background contents of an Audio object.
 * Special case to make replace mode look the same with and without segments
 * by applying a retroactive fade to just the background content, making
 * it look like there was a segment fade rather than a gradual feedback
 * reduction over the segment boundary.
 */
PUBLIC void Layer::fadeBackground(LayerContext* con, long startFrame)
{
    long fadeFrames = AudioFade::getRange();
    long fadeStartFrame = startFrame - fadeFrames;
    long fadeOffset = 0;

    if (fadeStartFrame < 0) {
        fadeOffset = -startFrame;
        fadeFrames -= fadeOffset;
        fadeStartFrame = 0;
    }

    // the window may not actually be over this region
    mTailWindow->removeForeground(mRecordCursor);

    long reflected = reflectFrame(con, fadeStartFrame);
    mRecordCursor->setReverse(con->isReverse());
    mRecordCursor->setFrame(reflected);
    mRecordCursor->fade(fadeOffset, fadeFrames, 0);

    mTailWindow->addForeground(mRecordCursor);
}

/**
 * Internal helper to insert an empty cycle.  
 * startFrame must not have been reflected yet.
 *
 * Subtlety: going forward, any segment exactly on the startFrame
 * is pushed because insertSegmentGap compares offset >= startFrame.
 * When reflecting, we're now pointing at the last frame of a cycle
 * but we want the insertion to happen after the end of the cycle, 
 * so we have to add 1, otherwise offset >= startFrame will cause us
 * to push a segment that overlaps the final frame of the cycle rather
 * than splitting it.  In practice this doesn't often happen.
 */
PRIVATE void Layer::insertCycle(LayerContext* con, long startFrame)
{
	Trace(this, 2, "Layer: Adding cycle\n");

	// open up a gap in the segments
	long reflectedFrame = reflectFrame(con, startFrame);
	long cycleFrames = getCycleFrames();
	
	if (con->isReverse())
		reflectedFrame++;

	insertSegmentGap(reflectedFrame, cycleFrames);

	// extend local audio, either at the end or the front if in reverse
	setFrames(con, mAudio->getFrames() + cycleFrames);

	mCycles++;
	mStructureChanged = true;
}

/**
 * Helper for both initInsert and stutter.
 * Insert a empty cycle into the segment list.  Segments that come
 * after the insert point are moved, segments that span the insert point
 * are split.  The frame must already be reflected.
 *
 */
PRIVATE void Layer::insertSegmentGap(long startFrame, long frames)
{
	// we need to iterate over the current segments while inserting new
	// ones into the list, so be careful not to process the new ones
	Segment* segments = mSegments;
	Segment* next = NULL;
	int fadeRange = AudioFade::getRange();

	mSegments = NULL;

	for (Segment* seg = segments ; seg != NULL ; seg = next) {
		next = seg->getNext();
		seg->setNext(NULL);

		Layer* refLayer = seg->getLayer();
		long offset = seg->getOffset();

		if (offset >= startFrame) {
			// entirely after the start frame, it gets pushed
			seg->setOffset(offset + frames);

			long copyLeft = seg->getLocalCopyLeft();
			long leftFrame = offset - copyLeft;
			if (leftFrame < startFrame) {
				int loss = startFrame - leftFrame;
				copyLeft -= loss;
				seg->setLocalCopyLeft(copyLeft);
			}

            // and must be faded if we lost adjacent content
			// note that we have to factor in LCL here to determine if
			// we "include" the left edge
			long refStart = seg->getStartFrame() - copyLeft;

			if (copyLeft < fadeRange &&
				(refStart > 0 || refLayer->hasDeferredFadeLeft()))
			  seg->setFadeLeft(true);

			addSegment(seg);
		}
		else {
			long last = offset + seg->getFrames() - 1;
			if (last >= startFrame) {
				// it gets split
				Segment* right = new Segment(seg);
				long leftlen = startFrame - offset;
				long rightlen = seg->getFrames() - leftlen;
				seg->setFrames(leftlen);
                seg->setLocalCopyRight(0);
                seg->setFadeRight(true);
				addSegment(seg);
				right->setStartFrame(right->getStartFrame() + leftlen);
				right->setFrames(rightlen);
				right->setOffset(startFrame + frames);
                right->setLocalCopyLeft(0);
                right->setFadeLeft(true);
				addSegment(right);
			}
			else {
				// entirely before the insert start frame
				long copyRight = seg->getLocalCopyRight();
				long rightFrame = last + copyRight;
				if (rightFrame >= startFrame) {
					// note that we're dealing with the actual last frame, 
					// not 1+ last frame like we usually do
					int loss = (rightFrame - startFrame) + 1;
					copyRight -= loss;
					seg->setLocalCopyRight(copyRight);
				}

				// note that to determine if we need a fade, have to factor
				// in the LCR to determine if we "include" the edge
				long lastFrame = seg->getStartFrame() + seg->getFrames() +
					copyRight;
				if (copyRight < fadeRange &&
					(lastFrame <  refLayer->getFrames() ||
					 refLayer->hasDeferredFadeRight()))
				  seg->setFadeRight(true);

				addSegment(seg);
			}
		}
	}
}

/**
 * Called by Loop as it records during insert mode.  initInsert
 * must have been called first.
 * 
 * The work of restructuring the segments to open a gap for the insertion
 * is done in initInsert and insertCycle before we start recording into the
 * gap.  So all we have to do here is add things to the local Audio.
 *
 * Insert by definition contains new content so we don't have to deal with
 * copying content from the previous layer (hmm, this might be interesting?).
 * But feedback is passed in so we can track changes.
 */
PUBLIC void Layer::insert(LayerContext* con, long startFrame, int feedback)
{
	if (!mInserting) {
		Trace(this, 1, "Layer: Uninitialized layer insert!\n");
		record(con, startFrame, feedback);
	}
	else {
		checkRecording(con, startFrame);

		// don't have to smooth, just go there
		forceFeedback(feedback);

		// If we're crossing an insert cycle boundary insert another cycle
		// Do this before recording so we get the Audios resized 
		mInsertRemaining -= con->frames;

		if (mInsertRemaining < 0) {
			// crossed a cycle boundary, add another
			// new cycle begins here (before reflection)
			long newCycle = startFrame + con->frames + mInsertRemaining;
			insertCycle(con, newCycle);
			mInsertRemaining += getCycleFrames();
		}

		// Record the insertion
		long reflectedFrame = reflectFrame(con, startFrame);
		mRecordCursor->setReverse(con->isReverse());
		mRecordCursor->put(con, OpAdd, reflectedFrame);
        // NOTE: the Isolated Overdub parameter was experimental and no longer exposed
		if (mIsolatedOverdub) {
			mOverdubCursor->setReverse(con->isReverse());
			mOverdubCursor->put(con, OpAdd, reflectedFrame);
		}
		watchMax(con);
		mAudioChanged = true;
	}
}

/**
 * Called by Loop when the insert cycle boundary is exactly on the loop
 * boundary, we're processing the LoopEvent and have to make the
 * layer larger now or else we'll never move beyond the loop end.
 */
PUBLIC void Layer::continueInsert(LayerContext* con, long startFrame)
{
	if (!mInserting) {
		Trace(this, 1, "Layer: Uninitialized layer insert!\n");
	}
	else if (mInsertRemaining != 0) {
		// if we still have a remainder then the layer didn't think
		// we were on a cycle boundary and we should have already
		// extended in insert()
		Trace(this, 1, "Layer: Inserting cycle with remainder from last cycle!\n");
	}
	else {
		insertCycle(con, startFrame);
		mInsertRemaining = getCycleFrames();
	}
}

/**
 * Called by Loop when we're finished with an insertion.
 * If this was a rounded insert, then the remainder should have
 * counted down to zero.  If it is unrounded, then we have to 
 * remove a portion of the last cycle we inserted.
 *
 * Since we've been inserting full cycles, on an unrounded insert we
 * have to remove the part of the cycle we decided not to fill.
 */
PUBLIC void Layer::endInsert(LayerContext* con, long endFrame, bool unrounded)
{
	if (!mInserting)
	  Trace(this, 1, "Layer: Meaningless insert ending!\n");

	else if (unrounded) {
		
		// you've gone too far!

		// This can be zero if we just happened to insert exactly a
		// cycle length, but it should never be negative
		if (mInsertRemaining < 0)
		  Trace(this, 1, "Layer: Negative insertion remainder frames!\n");

		else if (mInsertRemaining > 0) {

			// last inserted cycle ended here
			long insertCycleEnd = endFrame + mInsertRemaining;
			insertCycleEnd = reflectFrame(con, insertCycleEnd);

			// Pull back segments that were pushed out during the insert.
            // In theory, if the insert length was zero, we'll now
            // be putting split segments back next to each other again
            // and we no longer need edge fades on them.  In practice,
            // this can't happen, but it will be corrected in 
            // compileSegmentFades if we're not flattening.
			for (Segment* s = mSegments ; s != NULL ; s = s->getNext()) {
				long offset = s->getOffset();
				if (offset >= insertCycleEnd)
				  s->setOffset(offset - mInsertRemaining);
			}

			// a cycle was added to the local audio too, have to round down
			setFrames(con, mFrames - mInsertRemaining);
		}

		mCycles = 1;
	}
	else if (mInsertRemaining != 0) {
		// must be an error in Loop's rounding calculations
		Trace(this, 1, "Layer: Expecting more insert content!\n");
	}

	mInserting = false;
	mInsertRemaining = 0;
}

/****************************************************************************
 *                                                                          *
 *                                  STUTTER                                 *
 *                                                                          *
 ****************************************************************************/

/**
 * Add a cycle during Stutter mode.  Called by Loop for both CycleEvent
 * and LoopEvent.
 * 
 * Similar to Insert in that we have insert a new cycle into the
 * middle of the layer.  Segments that follow the stutter point are pushed,
 * segments that span are split.
 *
 * The srcFrame argument has the location in the src layer of the cycle
 * to be stuttered.  The destFrame is expected to be at the start of a 
 * cycle, or one past the end of the loop.  
 *
 * Both srcFrame and destFrame must be reflected.  Note that srcFrame
 * must be reflected by the length of the src layer not the current layer,
 * since this layer will grow.
 *
 * Reverse subetlty: after reflection we're pointing at the last
 * frame in the cycle before the new inserted cycle.  The segment
 * offset must be 1+ this to be "in" the new cycle.  Similar issue
 * happens in insertCycle().
 *
 * Fade subtlety: If we're stuttering in the middle, we will create a 
 * segment
 */
void Layer::stutterCycle(LayerContext* con, Layer* src, long srcFrame,
                         long destFrame)
{
	long cycleFrames = getCycleFrames();

	// reflect before the insertion, but remember that insertCycle
	// does its own reflection so don't trash destFrame
	long reflectedDest = reflectFrame(con, destFrame);
	if (con->isReverse())
	  reflectedDest++;

	long previousFrames = getFrames();
	insertCycle(con, destFrame);

	// If we're flattening, then we need to fade the background we just copied
	// because we're creating a discontinuity.  Can avoid if we're
	// stuttering the last cycle, and the previous layer does not have
	// a deferred fade.
	if (!mNoFlattening) {
		if ((destFrame !=  previousFrames) ||
			(mPrev != NULL && 
			 ((mReverseRecord && mPrev->hasDeferredFadeLeft()) ||
			  (!mReverseRecord && mPrev->hasDeferredFadeRight())))) {

			fadeBackground(con, destFrame);
		}
	}


	// remember to reflect relative to the size of the src layer
	srcFrame = src->reflectRegion(con, srcFrame, cycleFrames);

	Segment* cycle = new Segment(src);
	cycle->setOffset(reflectedDest);
    cycle->setStartFrame(srcFrame);
	cycle->setFrames(cycleFrames);

    // Fade the edges that aren't exactly on the layer edge
    adjustSegmentFades(cycle);
	addSegment(cycle);

	// Fade subtlety: if we're stuttering in the middle, we will have
	// created a segment for the cycle(s) after the one we just stuttered
	// this normally has the leftFade flag set by insertCycle.  But here
	// we know that the content from the stuttered cycle flows seamlessly 
	// into the one we just pushed so we can turn off the fade.  We could
	// try to be smart and turn off the fades on the adjacent edges, but
	// it's easier just to coalesce.
	coalesce();

	mStructureChanged = true;
}

/****************************************************************************
 *                                                                          *
 *                                  FINALIZE                                *
 *                                                                          *
 ****************************************************************************/

/**
 * Called by InputStream when we have finished recording this layer
 * and now know if we will be continuing a seamless recording into the
 * next layer.  
 *
 * I wanted to do this in Loop::shift, but there's too much stuff that
 * can happen between the shift and the InputStream that can change
 * the recording target.  Instead InputStream remembers the last
 * layer recorded, and detects when we start recording in a different layer.
 * 
 * If a retrigger or loop switch was performed, we may not have recorded
 * all the way to the end.  This means that feedback flattening won't
 * be complete and we'll still have a segment reference to the previous layer.
 * This reference does however have to have feedback "applied" so that it
 * matches the feedback in effect when the retrigger happened.
 *
 * If the ending feedback is not 100% we need to apply the leading
 * deferred fade if any because the preceeding content will have been
 * copied at a lower level.  It is an occlusion fade.
 *
 * If the initial feedback in this layer was not 100% we need to apply
 * trailing deferred fades from the background layer since the following
 * content will have been copied at a lower level.
 *
 * This is also where we check the Max Undo parameter.  This has to be
 * deferred because this layer may need parts of the previous layer
 * for fading, once a layer is finalized it will have no dependencies
 * on the previous layer, other than in Segments which will keep
 * the reference count up.
 */
PUBLIC void Layer::finalize(LayerContext* con, Layer* next)
{
	if (mFinalized) {
		Trace(this, 1, "Layer: already finalized!\n");
		return;
	}

	if (ScriptBreak) {
		int x = 1;
	}

    // If we haven't completed flattening and we're within FadeRange
    // of the end, finish now so we can be immune to a deferred trailing
    // edge fade on the previous layer being applied out from under us,
    // making our local copy inconsistent with what is really there.
    // To get into this state, you would have to have a retrigger
    // within milliseconds of the end of the loop, followed by an undo.
        
    if (mLastFeedbackFrame < mFrames && 
        (mLastFeedbackFrame >= (mFrames - AudioFade::getRange()))) {

		Trace(this, 2, "Layer: Completing feedback copy to end of loop\n");
        // not sure what state the provided context is in, don't trash it
        LayerContext fc;
        fc.channels = con->channels;
        fc.frames = mFrames - mLastFeedbackFrame;
        advanceInternal(&fc, mLastFeedbackFrame, mFeedback);
    }

    // If we haven't finished flattening, save the final feedback
    // level on the remaining segments.  This shouldn't happen often
	// now that advanceInternal tries to keep feedback set, but I think
	// it can happen with retriggering?
	if (!mNoFlattening) {
		bool needed = false;
		for (Segment* s = mSegments ; s != NULL ; s = s->getNext()) {
			if (s->getOffset() >= mLastFeedbackFrame) {
				// ?? if it already has feedback should these accumulate
				// hmm, if we haven't finished a smoothing ramp we won't
				// be at the desired feedback level, could perform
				// a short advance() to get there?
				s->setFeedback(mFeedback);
				needed = true;
			}
            else {
				// This should only happen if we had to do an early
				// shift after an Insert or Multiply and started
				// recording/flattening this layer in the middle.  There
				// will be a segment at the front that should be at
				// feedback 127.
				if (s->getFeedback() != 127)
				  Trace(this, 1, "Layer: Odd segment encountered at finalize!\n");
            }
		}
		if (needed) {
            Trace(this, 2, "Layer: Set layer remainder feedback %ld at %ld\n",
                  (long)mFeedback, mLastFeedbackFrame);
        }
    }

	// Check for a seamless recording into the next layer and defer fades.
	// Subtlety: If we're going into Multiply mode, it looks like a 
	// seamless record but it isn't really because the leading edge
	// in the new layer won't meet the trailing edge of the previous layer
	// unless the multiply is ended with only one cycle.   It's hard to 
	// wait until the multiply ends to detect this and since a single cycle
	// multiply is rare, go ahead and fade.
	// Obscure: If we just completed an insert at the end, and immediately
	// begin another insert, we will be recording in the middle of the next
	// layer rather than from the start.  In this case we also need to 
	// force a fade to the previous layer.  But to detect that here, 
	// we have to know the Loop's record frame.  Eventuall checkRecording
	// will catch this, but it issues a warning if we didn't get it here.

	if (mFadeOverride) {
		// script kludge, assume the audio is already properly faded
		mFadeOverride = false;
	}
	else if (mTailWindow->getLastExternalFrame() != getFrames() ||
             next == NULL || 
             next->getPrev() != this || 
			 mLoop->getFrame() != 0 ||
             mLoop->isPaused() ||
             !mLoop->isRecording() ||
             mLoop->getMode() == MultiplyMode) {

		// not seamless into the next layer
		fadeOut(con);
	}
	else {
		// mark a deferred fade out and remember the final record direction
		// the reverse inconsistency should now be caught by checkRecording
		Trace(this, 2, "Layer: Seamless shift, deferring fade out\n");
		if (con->isReverse()) {
			if (!mReverseRecord && mDeferredFadeLeft)
			  Trace(this, 1, "Layer: Changed direction after deferring fade in!\n");
			mDeferredFadeLeft = true;
			mReverseRecord = true;
		}
		else {
			if (mReverseRecord) 
			  Trace(this, 1, "Layer: Changed direction after deferring fade in!\n");
			mDeferredFadeRight = true;
			mReverseRecord = false;
		}
	}

	// If the ending feedback is less than 127 and there is a deferred fade
	// into this layer, then we must lower the foreground head to match the
	// ending feedback.  This applies to both segments and flattening.
	// With segments, 

	if (mNoFlattening) {
		// When not flattening, ending feedback is supposed to have
		// been set by lockStartingFeedback called when OutputStream
		// starts preplaying this layer.  Make sure.
		for (Segment* s = mSegments ; s != NULL ; s = s->getNext()) {
			if ((mReverseRecord && s->isAtStart(this)) ||
				(!mReverseRecord && s->isAtEnd(this))) {
				// this segment covers the tail of the previous layer
				if (s->getFeedback() != mFeedback) {
					Trace(this, 1, "Layer: Adjusting ending feedback\n");
					mFeedback = s->getFeedback();
					break;
				}
			}
		}
	}

	// If we have a seamless record into this layer and we lowered
	// feedback at the end, have to perform a fade adjustment to the
	// foreground head.
	// !! if we were smoothing, edge feedback values may not have
	// actually reached the targets, should be basing the adjustment
	// on the float feedback value that was actually in use at the time.

	if (mFeedback < 127 && isDeferredFadeIn()) {
        CovFinalizeFadeHead = true;
		float* ramp = AudioFade::getRamp128();
		float baseLevel = ramp[mFeedback];

		// !!! What if the head window only has a portion of the
		// fade range, it won't actually go to zero?
		// If we did a fadeOut() above, then we're applying a redundant fade

		mHeadWindow->fadeForeground(mRecordCursor, baseLevel);

		// if we ended up doing a full fade, turn off the flags now
		// so compileSegments doesn't get confused
		if (baseLevel == 1.0) {
			if (mReverseRecord)
			  mDeferredFadeRight = false;
			else 
			  mDeferredFadeLeft = false;
		}
	}

	// Process edges fades triggered by feedback changes
	// Normally if mContainsDeferredFadeLeft is on, so will FadeRight,
	// but compileSegmentFades and occlude may turn off one of them so we
	// can't rely on them being consistent.  Assume if either one is on
	// we had at one time contained fades on both edges.

	if (!mNoFlattening) {

		// special case: if the ending feedback of the previous layer and 
		// the starting feedback of this layer were both zero, it is
		// effectively a replace over the edge and we don't have to 
		// do any fading

		if ((mContainsDeferredFadeLeft || mContainsDeferredFadeRight) &&
			mStartingFeedback != mFeedback &&
			!(mStartingFeedback == 0 &&
			  mPrev != NULL && mPrev->mFeedback == 0)) {

			if (mStartingFeedback < mFeedback) {

				// The background tail is louder than the background head
				// We should either have both contained fade flags on or off,
				// it doesn't really matter what direction we're going.

				if (mStartingFeedback > 0 || isDeferredFadeIn()) {
					// Raise the background head by capturing a fade
					// tail from the beginning of the previous layer, 
					// and add it to the beginning of this layer.
					raiseBackgroundHead(con);
				}
				else {
					// We replaced the head and there is no seamless
					// record into this layer,  can just do a simple
					// background tail fade.  We could handle this
					// by raising the head, but segments work this way
					// so it simplifies testing.
					fadeBackgroundTail(con);
				}
			}
			else {
				// The background head is louder than the background tail.
				// See notes as to why it is better to lower the head than
				// raise the tail.
				lowerBackgroundHead(con);
			}
		}

		// if feedback when to zero on an edge, can no longer have
		// contained fades, should have caught these by now but make sure

        if (mStartingFeedback == 0) {
            if (mReverseRecord)
              mContainsDeferredFadeRight = false;
            else
              mContainsDeferredFadeLeft = false;
        }
        if (mFeedback == 0) {
            if (mReverseRecord)
              mContainsDeferredFadeLeft = false;
            else
              mContainsDeferredFadeRight = false;
        }

	}

	// Do a final compilation of segment fades.
	compileSegmentFades(true);

	// If we're not flattening, segment compilation can force an edge
	// fade and turn off one of the contained flags.  If we still have
	// a deferred local fade on the other edge, it must be applied now.

	if (!mReverseRecord && mDeferredFadeLeft && !mContainsDeferredFadeRight)
	  applyDeferredFadeLeft();

	if (mReverseRecord && mDeferredFadeRight && !mContainsDeferredFadeLeft)
	  applyDeferredFadeRight();

	// Fade compilation may have changed the deferred fade flags
	// which were copied into the next layer by copy(), update them
	if (next != NULL && next->getPrev() == this) {
		next->mContainsDeferredFadeLeft = hasDeferredFadeLeft();
		next->mContainsDeferredFadeRight = hasDeferredFadeRight();
	}

	ScriptBreak = false;

	mPaused = false;
	mMuted = false;
	mFinalized = true;

    checkMaxUndo();
}

/**
 * Helper for finalize().  
 * Raise the background head to the same level as the background tail.
 * Thsi is done by capturing a fade tail from the beginning of the previous
 * layer, and adding it to the beginning of this layer.
 */
PRIVATE void Layer::raiseBackgroundHead(LayerContext* con)
{
    CovFinalizeRaiseBackgroundHead = true;

	// feedback must use this ramp
	float* ramp = AudioFade::getRamp128();
	float tailFactor = ramp[mFeedback];
	float headFactor = ramp[mStartingFeedback];
	float adjust = tailFactor - headFactor;
	float tail[AUDIO_MAX_FADE_FRAMES * AUDIO_MAX_CHANNELS];
	LayerContext fc;
	fc.buffer = tail;
	fc.frames = AudioFade::getRange();
	fc.setReverse(con->isReverse());
	long samples = AudioFade::getRange() * con->channels;
	memset(tail, 0, sizeof(float) * samples);

    if (mPrev != NULL)
      mPrev->captureTail(&fc, 0, adjust);
    else {
        // saw this before we deferred checking MaxUndo until finalize
        Trace(this, 1, "Layer::raiseBackgroundHead mPrev is NULL!\n");
    }

	long startFrame = reflectFrame(con, 0);
	mRecordCursor->setReverse(con->isReverse());
	mRecordCursor->put(&fc, OpAdd, startFrame);

	if (mStartingFeedback == 0) {
		if (mReverseRecord)
		  mContainsDeferredFadeRight = false;
		else
		  mContainsDeferredFadeLeft = false;
	}
}

/**
 * Helper for finalize()
 * Fade the background tail to zero.
 */
PRIVATE void Layer::fadeBackgroundTail(LayerContext* con)
{
    CovFinalizeFadeBackgroundHead = true;
	if (mReverseRecord)
	  fadeLeft(false, true, 1.0f);
	else
	  fadeRight(false, true, 1.0f);
}

/**
 * Helper for finalize()
 * Lower the background head to match the level of the tail.
 */
PRIVATE void Layer::lowerBackgroundHead(LayerContext* con)
{
    CovFinalizeLowerBackgroundHead = true;
	float* ramp = AudioFade::getRamp128();
	float tailFactor = ramp[mFeedback];
	float headFactor = ramp[mStartingFeedback];
	float baseLevel = headFactor - tailFactor;

	// geez I hate this thing, need to clean it up!
	mRecordCursor->setReverse(con->isReverse());

	if (mReverseRecord)
	  fadeRight(false, true, baseLevel);
	else
	  fadeLeft(false, true, baseLevel);

	// if the matching feedabck level was zero, then we've
	// done a full fade and can turn off the flags
	if (mFeedback == 0) {
		if (mReverseRecord) 
		  mContainsDeferredFadeRight = false;
		else
		  mContainsDeferredFadeLeft = false;
	}
}

/**
 * After a layer has been finalized, check the undo limit.
 * At this point, we are the play layer at the head of the undo list.
 */
PRIVATE void Layer::checkMaxUndo()
{
    Layer* oldest = NULL;
    Preset* p = mLoop->getPreset();
	int max = p->getMaxUndo();

    if (max > 0) {
        oldest = this;
        for (int i = 0 ; i < max - 1 && oldest != NULL ; i++)
          oldest = oldest->getPrev();
    }

    if (oldest != NULL) {
        Layer* extras = oldest->getPrev();
        if (extras != NULL) {
            oldest->setPrev(NULL);
            
            // should be only one, but there could be more if
            // the parameter changed after building a list
            int count = 0;
            for (Layer* l = extras ; l != NULL ; l = l->getPrev())
              count++;
            Trace(this, 2, "Freeing %ld excess layers\n", (long)count);

            extras->freeAll();
        }
    }
}

/****************************************************************************
 *                                                                          *
 *                                 LAYER POOL                               *
 *                                                                          *
 ****************************************************************************/

/**
 * Layers were originally pooled so we could reuse their large Audio
 * objects.  Now that we pool Audio buffers this is less necessary but an
 * allocation interface like this is still necessary to manage the
 * reference count.
 */
PUBLIC LayerPool::LayerPool(AudioPool* aupool)
{
    mAudioPool = aupool;
    mLayers = NULL;
    mCounter = 0;
    mAllocated = 0;
    mMuteLayer = NULL;
    mCopyContext = NULL;
}

/**
 * This can only be called during shutdown when we know we won't
 * be in an interrupt trying to allocate layers.
 */
PUBLIC LayerPool::~LayerPool()
{
    delete mCopyContext;

    // return to the pool first for statistics
    if (mMuteLayer != NULL) 
      freeLayer(mMuteLayer);

    // this will delete the prev pointer chain
    delete mLayers;
}

/**
 * Get the shared LayerContext used for layer flattening.
 * Since we can only ever process one layer at a time in an interrupt,
 * we can share a single context.
 */
LayerContext* LayerPool::getCopyContext()
{
	if (mCopyContext == NULL) {
		float* buffer = new float[AUDIO_MAX_FRAMES_PER_BUFFER * AUDIO_MAX_CHANNELS];
		mCopyContext = new LayerContext();
		mCopyContext->setBuffer(buffer, AUDIO_MAX_FRAMES_PER_BUFFER);
	}
	return mCopyContext;
}

/**
 * Boostrap a spsecial empty layer used to "play" a muted area.
 * This is a static in Loop so we allocate only one for the 
 * Mobius instance.
 */
PUBLIC Layer* LayerPool::getMuteLayer()
{
    if (mMuteLayer == NULL) {

        mMuteLayer = newLayer(NULL);
            
        // kludge: make MuteLayer look like it has some content
        // so jumpPlayEventUndo calculations work, this has to be 
        // at least as large as the combined IO latencies, 1 second
        // should be enough.  Note that though we use CD_SAMPLE_RATE
        // the buffer size isn't that important.  It just needs to be suitably
        // large.
        mMuteLayer->setFrames(NULL, CD_SAMPLE_RATE);
    }
    return mMuteLayer;
}

/**
 * Allocate a new layer, use the pool if available.
 * Loop may be NULL here for special layer constants like MuteLayer.
 */
Layer* LayerPool::newLayer(Loop* loop)
{
	Layer* layer = mLayers;

	if (layer == NULL) {
        layer = new Layer(this, mAudioPool);
        layer->setAllocation(mAllocated++);
    }
	else {
        // pool is chained by the prev pointer...confusing!
		mLayers = layer->getPrev();
		if (!layer->mPooled)
		  Trace(1, "Layer:  Layer in pool not marked as pooled\n");
		layer->mPooled = false;
		layer->reset();
		layer->setPrev(NULL);
	}

    // tag with a unique number for debugging, unlike
    // mAllocated this one can be reset
    layer->setNumber(mCounter++);

	layer->setReferences(1);

	// cache some global options now, might want to move this
	// into the Preset?
	if (loop != NULL) {
        layer->setLoop(loop);

		Mobius* m = loop->getMobius();
		MobiusConfig* c = m->getInterruptConfiguration();
        // NOTE: the Isolated Overdub parameter was experimental and no
        // longer exposed
		layer->mIsolatedOverdub = c->isIsolateOverdubs();
        // originally in MobiusConfig, but this is a useful performance
        // option so moved to Preset
        Preset* p = loop->getPreset();
		layer->mNoFlattening = p->isNoLayerFlattening();
	}

	return layer;
}

/**
 * Return a layer to the pool.
 */
void LayerPool::freeLayer(Layer* layer)
{
	if (layer != NULL) {
		if (layer->mPooled)
		  Trace(1, "Layer: Attempt to free layer already in the pool!\n");
		else {
			int refs = layer->decReferences();
			if (refs <= 0) {
				layer->reset();
				layer->setPrev(mLayers);
				layer->mPooled = true;
				
				bool checkpool = true;
				if (!checkpool)
				  mLayers = layer;
				else {
					Layer* found = NULL;
					for (found = mLayers ; found != NULL ; found = found->getPrev()) {
						if (found == layer)
						  break;
					}
					if (found != NULL) 
					  Trace(1, "Layer: Attempt to free layer already in the pool!\n");
					else
					  mLayers = layer;
				}
			}
			else {
				// do NOT null the prev pointer, it may still be on a list
				//printf("freeLayer %d still referenced\n", l->getNumber());
			}
		}
	}
}

/**
 * Return a list of layers to the pool.
 * Note that the layer list is linked by the mPrev pointer rather than
 * the usual next pointer.  
 */
void LayerPool::freeLayerList(Layer* list)
{
	if (list != NULL) {
        Layer* next = NULL;
        for (Layer* l = list ; l != NULL ; l = next) {
            next = l->getPrev();
            freeLayer(l);
        }
    }
}

void LayerPool::resetCounter()
{
    mCounter = 0;
}

PUBLIC void LayerPool::dump()
{
    int count = 0;

    for (Layer* l = mLayers ; l != NULL ; l = l->getPrev())
      count++;

    printf("LayerPool: %d allocated, %d in the pool, %d in use\n", 
           mAllocated, count, mAllocated - count);
}

/****************************************************************************
 *                                                                          *
 *   								DEBUG                                   *
 *                                                                          *
 ****************************************************************************/

void Layer::dump(TraceBuffer* b)
{
    // started using mNumber but I think mAllocated is more useful
	b->add("Layer %d: frames %ld cycles %d references %d\n",
		   mAllocation, mFrames, mCycles, mReferences);

	b->incIndent();

	if (mAudio != NULL)
	  mAudio->dump(b);

	for (Segment* s = mSegments ; s != NULL ; s = s->getNext())
	  s->dump(b);

	b->decIndent();
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
