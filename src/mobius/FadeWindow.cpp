/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Helper class for Layer to keep track of a short segment of
 * recorded audio over which a deferred fade may need to be applied.
 *
 * UPDATE: Also now used by some Plugins to implement a shutdown fade by
 * capturing a tail window, reversing it, then fading it out.  Saves having
 * to capture a fade tail from the source material.
 *
 * When you leave overdub on and record seamlessly over the loop boundary,
 * fades at the end of the last layer and the beginning of the new layer
 * are deferred to avoid a fade bump during playback.   If you later
 * undo back to the previous layer, the deferred fade at the end
 * must be applied because the content following the end will
 * no longer exist.
 *
 * Similarly, if you overdub a section into the middle of a layer then
 * turn overdub off, the end of the overdub must be faded after
 * the fact since we couldn't anticipate when the fade would be necessary.
 *
 * These two cases present a problem when "layer flattening" is enabled
 * because the new audio is being constantly combined with feedback
 * audio being copied from the previous layer.  If we simply apply
 * a fade to the current contents of the layer, we will in effect be fading
 * not only the new overdubbed content, but also the feedback content
 * being copied from the previous layer.  If the overdub happens to be
 * silent, this will produce a click because the previous layer content
 * will fade out, then abruptly resume at its normal level.
 *
 * We must have a way to accomplish a fade of only the new overdubbed
 * content without affecting the copied content.  The simplistic approach
 * is to maintain the feedback content and the overdubbed content in
 * two different Audio objects and merge them at runtime.  This is easy
 * but potentially doubles the amount of memory required.
 *
 * A more complex approach is to maintain a short "window" contaning
 * only new overdubbed content, then perform the fade as follows:
 *
 *     - subtract the contents of the window from the combined
 *       layer content
 *
 *     - apply a fade to the contents of the window
 *
 *     - add the window with the fade back into the combined layer content
 *
 * To preserve memory, this window only needs to be as wide as the
 * maximum allowed fade range. The contents of the window will be
 * constantly shifted as recording progresses so that it always
 * contains the "tail" of the recording.  When overdub is turned
 * off, we then apply the fade as described above to the tail.
 *
 * A more obscure case happens when there is a deferred fade at the
 * beginning of the layer, and a replace is performed that occludes
 * the content at the end of the previous layer.  Since there
 * will no longer be content at the end of the loop that matches the
 * beginning of the loop, this will product a click unless we apply
 * a deferred fade to the beginning of the loop.
 *
 * Applying a fade at the beginning has the same problem as applying one
 * to the tail, the new content has been merged with feedback content copied
 * from the previous layer, so we need to maintain a window containing
 * only the new content.  Unlike the previous example, this window
 * only captures the beginning of the layer and then stops, it does not
 * "follow" the record cursor as the recording progresses.
 *
 * There are therefore two types of windows:
 *
 *   Head Window
 *     Captures a range of frames and stops when the window is full.
 *
 *   Tail Window
 *     Continually captures frames, shifting old frames out of the window
 *     to make room for new frames when it becomes full.
 *
 *
 * REVERSE
 *
 * The window is always captured "forward", the fact that it will
 * be stored in the Audio in reverse does not matter initially.  A head
 * window still behaves like a head window even if the content in the 
 * window will be at the right of the layer (reversed) rather than the left.
 *
 * Where reverse matters is when the fade is applied.  We will feed
 * the window to the AudioCursor as recorded, and the AudioCursor will
 * reverse it.  The only reverse awareness we need here is to reflect
 * the start frame of the fade before using the cursor.
 *
 * OVERLAPPING FADES AND EDGE CONDITIONS
 *
 * If an overdub is begun in the middle of the Layer, an up fade
 * is automatically applied by the AudioCursor as the content is copied
 * in to the Audio.  This processing is not howver seen by the FadeWindow
 * since we get the raw audio buffer being passed to the Layer.
 *
 * If an up fade region overlaps with a FadeWindow when the window fade is 
 * applied, we will do an incorrect subtraction of the window content
 * from the layer since a portion of the window content will be at a higher
 * level than the already faded content in the layer.  
 *
 * The chances of this are slim, but boundary scenarios are:
 *
 *  1) Deferred head fade, Overdub ends within 2 times the number of 
 *     frames in the window.  Here the head window and tail window will
 *     overlap to some degree.  The tail window fade will be applied making
 *     the content in the head window not match the Audio content.  This
 *     may cause clicks when the head window is eventually applied.
 *
 *     With the usual fade range of 128 the overdub must be turned off
 *     within 256 frames from the beginning of the layer, which is about
 *     5 milliseconds.  Difficult but not impossible if you were trying
 *     to time the ending of the overdub to the end of the layers.
 *
 *     Solution : Apply down fade to head window
 *     Here the same fade that was applied to the Audio is made to the
 *     contents of the head window.  Requires coordination between the
 *     two windows.  
 *
 * 2) No deferred head fade, overdub starts within the window.
 *    A dynamic up fade will be applied to the Audio content that
 *    will not be present in the head window content.  This is not a problem
 *    provided that we never attempt to apply a head window when there
 *    was no deferred head fade.  
 *
 * 3) Deferred head fade, overdub starts within the window.
 *    This would require that the first overdub end within the window
 *    (scenario 1) and then start again almost immediately.  Very
 *    difficult to accomplish in practice.  The head window will be
 *    modified to handle scenario 1, but it will not contain the
 *    dynamic up fade for the new overdub resulting in a level jump.
 *
 *    Solution: Perform dynamic up fades in the head window
 *    We would use an AudioFade object similar to the way AudioCursor does.
 *
 * 4) Overdub starts and ends within a window.
 *    When the overdub starts a dynamic up fade is being applied that
 *    is not present in the window.  If the overdub ends within this window,
 *    will apply a down fade to the preceeding frames and the window will
 *    not match the layer.  For this to happen you would have to begin
 *    and end an overdub in 2.5 milliseconds or less.  Very unlikely, 
 *    though concievable with SUSOverdub and a bouncy switch.
 *
 *    Solution: Perform dynamic up fades in the tail window
 *    Solution2: Quantize the end of a SUS function so that it
 *    cannot end within a certain range of the start.
 *
 * 5) Overdub starts within 2x the window size from the end of the layer.
 *    Similar to scenario 4, but the "end event" is caused by the end
 *    of the layer rather than a manual event.  Since an overdub
 *    is in progress when the layer is shifted, this will result in
 *    a deferred tail fade.  When this fade is applied, it will not
 *    contain the dynamic up fade that was in progress at the end.
 *    This is more likely since you could be timing the start of the
 *    overdub to the beginning of the loop and hit it a little early.
 * 
 *    Solution: Perform dynamic up fades in the tail window
 *
 * 6) Overdub starts less than 1x the window size from the end of the layer.
 *    Similar to scenario 5 but with the added complication that the
 *    up fade is split between the trailing and leading edge of two layers.
 *    There will be a deferred fade at the begginning of the next layer so
 *    we will capture a head window.  This window will not however contain
 *    the remainder of the up fade.
 *
 *    Solution: Perform dynamic up fades in the head window
 *    Solution: Layer must transfer dynamic up fade state from one
 *    layer to the next during a shift.
 *
 * 7) Deferred head fade, overdub ends shortly after the beginning.
 *    This is the same as 1, but split out to emphasize another complication.
 *    Down fades are applied retroactively to frames preceeding the current 
 *    frame.   If a down fade must be applied within the fade range from
 *    the beginning of a layer, it must either be truncated or caused
 *    to span the layer boundary.  For example, if an overdub ends at frame 8,
 *    the fade range logically must include the last 120 frames of the previous
 *    layer and the first 8 frames of the new layer.  If you simply truncate
 *    the range to 8 frames there may not be enough time to perform 
 *    a good sounding fade.
 *
 *    Solution: Apply down fade to head window
 *    Solution: Apply down fade to the tail window of the previous layer
 *      AND the end of the previous layer's Audio.
 * 
 * The solutions to 3, 4, 5, and 6 are basically the same.  While 3 and 4
 * could be ignored, 5 and 6 are likely to happen in practice so we can
 * solve them all with the same mechanism.
 *
 * The solution to 1 solves half of 7.  The full solution to seven requires
 * the additional application of permanent fades to the previous layer.
 * The fundamental solutions are:
 *
 *   - Apply retroactive down fade to the contents of a window
 *   - Apply dynamic up fade to the contents of a window
 *   - Apply retroactive down fade to trailing edge of previous layer
 *
 * A different approach entirely would be to use the fade windows sort
 * of like a Segment, and NOT write their content into the layer Audio
 * until they are applied.  As things spill out of the tail window, they
 * are written into the Audio, at the end of the recording we will
 * have an Audio with a "gap" on both ends with the content contained
 * in the head and tail windows.  When the layer is played, we have
 * to check to see if the requested range overlaps one of the windows
 * and selectively extract content from them.  This doesn't seem any easier
 * though and complicates saving the layer in a project since the Audio
 * doesn't have everything.
 */

#include <stdio.h>
#include <memory.h>

#include "Util.h"

// for AUDIO_MAX_CHANNELS, etc
#include "AudioInterface.h"

// for LayerContext, etc.
#include "Layer.h"

#include "FadeWindow.h"

/****************************************************************************
 *                                                                          *
 *                             COVERAGE ANALYSIS                            *
 *                                                                          *
 ****************************************************************************/
/*
 * Crude but adequate code coverage monitor for unit testing.
 */

PUBLIC bool CovFwinLocateIncompleteWindow = false;
PUBLIC bool CovFwinLocatePartialFade = false;
PUBLIC bool CovFwinFadeRight = false;
PUBLIC bool CovFwinFadeRightLevel = false;
PUBLIC bool CovFwinFadeLeft = false;
PUBLIC bool CovFwinFadeLeftLevel = false;
PUBLIC bool CovFwinFadeRightShift = false;
PUBLIC bool CovFwinFadeLeftShift = false;
PUBLIC bool CovFwinFadeLeftShiftTotal = false;
PUBLIC bool CovFwinFadeLeftShiftPartial = false;
PUBLIC bool CovFwinFadeLocalRight = false;
PUBLIC bool CovFwinFadeLocalLeft = false;

PUBLIC void FadeWindow::initCoverage()
{
    CovFwinLocateIncompleteWindow = false;
    CovFwinLocatePartialFade = false;
    CovFwinFadeRight = false;
    CovFwinFadeRightLevel = false;
    CovFwinFadeLeft = false;
    CovFwinFadeLeftLevel = false;
    CovFwinFadeRightShift = false;
    CovFwinFadeLeftShift = false;
    CovFwinFadeLeftShiftTotal = false;
    CovFwinFadeLeftShiftPartial = false;
    CovFwinFadeLocalRight = false;
    CovFwinFadeLocalLeft = false;
}

PUBLIC void FadeWindow::showCoverage()
{
    printf("FadeWindow coverage gaps:\n");

    if (!CovFwinLocateIncompleteWindow)
      printf("  CovFwinLocateIncompleteWindow\n");
    if (!CovFwinLocatePartialFade)
      printf("  CovFwinLocatePartialFade\n");
    if (!CovFwinFadeRight)
      printf("  CovFwinFadeRight\n");
    if (!CovFwinFadeRightLevel)
      printf("  CovFwinFadeRightLevel\n");
    if (!CovFwinFadeLeft)
      printf("  CovFwinFadeLeft\n");
    if (!CovFwinFadeLeftLevel)
      printf("  CovFwinFadeLeftLevel\n");
    if (!CovFwinFadeRightShift)
      printf("  CovFwinFadeRightShift\n");
    if (!CovFwinFadeLeftShift)
      printf("  CovFwinFadeLeftShift\n");
    if (!CovFwinFadeLeftShiftTotal)
      printf("  CovFwinFadeLeftShiftTotal\n");
    if (!CovFwinFadeLeftShiftPartial)
      printf("  CovFwinFadeLeftShiftPartial\n");
    if (!CovFwinFadeLocalRight)
      printf("  CovFwinFadeLocalRight\n");
    if (!CovFwinFadeLocalLeft)
      printf("  CovFwinFadeLocalLeft\n");

    fflush(stdout);
}

/****************************************************************************
 *                                                                          *
 *   							 FADE WINDOW                                *
 *                                                                          *
 ****************************************************************************/

/**
 * If AUDIO_MAX_CHANNELS is greater than 2, we may actually end up with 
 * a window that is much longer than it needs to be, but we'll always
 * allocate the maximum possible length so these can be pooled and used
 * in any layer context.
 */
PUBLIC FadeWindow::FadeWindow()
{
	mBufferSize = AUDIO_MAX_FADE_FRAMES * AUDIO_MAX_CHANNELS;
	mBuffer = new float[mBufferSize];
    reset();
}

PUBLIC void FadeWindow::reset()
{
    mHeadWindow = false;
    mWindowFrames = AudioFade::getRange();
    mChannels = 2;
    mReverse = false;
    mFrames = 0;
	mCursor = 0;
	mLastExternalFrame = 0;
	mFull = false;
	mLeftFrames = 0;
	mLeftOffset = 0;
	mRightFrames = 0;
	mRightOffset = 0;
	mLeftBuffer = NULL;
	mRightBuffer = NULL;
	mForegroundFaded = false;
	mBackgroundFaded = false;

	mFade.init();
}

PUBLIC FadeWindow::~FadeWindow()
{
	delete mBuffer;
}

PUBLIC bool FadeWindow::isForegroundFaded()
{
	return mForegroundFaded;
}

/**
 * We only allow dynamic up fades, down fades are always done
 * retroactively.
 */
PUBLIC void FadeWindow::startFadeIn()
{
    if (!mFull)
	  mFade.activate(true);
}

/**
 * This must be called by Layer before it begins adding content
 * to the window.
 */
PUBLIC void FadeWindow::prepare(LayerContext* con, bool head)
{
    reset();
    mChannels = con->channels;
    mReverse = con->isReverse();
    mHeadWindow = head;
    mWindowFrames = AudioFade::getRange();
}

PUBLIC long FadeWindow::getLastExternalFrame()
{
	return mLastExternalFrame;
}

PUBLIC void FadeWindow::setLastExternalFrame(long frame)
{
	mLastExternalFrame = frame;
}

PUBLIC int FadeWindow::getWindowFrames()
{
    return mWindowFrames;
}

PUBLIC int FadeWindow::getFrames()
{
	return mFrames;
}

PUBLIC void FadeWindow::setFrames(long frames)
{
	mFrames = frames;
}

/**
 * The head points to the next "free" location, which will start
 * overwriting frames once the window is full.  Note that we do not
 * capture the window in reverse, it will be reversed later when the
 * fade is applied.
 *
 * The startFrame argument is just for consistency checking, 
 * it must be unreflected if the context is in reverse.
 */
PUBLIC void FadeWindow::add(LayerContext* con, long startFrame)
{
	// stop processing the head window once we've filled it, or
	// moved beyond its range
	if (mHeadWindow && !mFull)
	  mFull = (mFrames >= mWindowFrames || startFrame >= mWindowFrames);

    // if we're a full head window, nothing more to do 
    if (!mFull) {

        long frames = con->frames;
        float* src = con->buffer;

		if (mHeadWindow) {
			long avail = mWindowFrames - mFrames;
			if (avail < frames)
			  frames = avail;
		}
		else {
			// convenient breakpoint
			int x = 0;
		}

        if (mFrames > 0 && mLastExternalFrame != startFrame) {
			// we jumped,  this is ok for the tail window provided we've 
			// applied it, it should not happen for the head window?
			if (mHeadWindow || (!mForegroundFaded && !mBackgroundFaded))
			  Trace(1, "Fade window gap!\n");
            prepare(con, mHeadWindow);
        }
        mLastExternalFrame = startFrame + frames;

        if (mReverse != con->isReverse()) {
            // changed direction midstream, not allowed
            Trace(1, "Fade window changed direction!\n");
            prepare(con, mHeadWindow);
        }

        if (mChannels != con->channels) {
            // this really shouldn't happen
            Trace(1, "Fade window changed channel count!\n");
            prepare(con, mHeadWindow);
        }
	   
		add(src, frames);
    }

	// Now that we've moved the window, can clear these.
	// Technically, we shouldn't until we've advanced more than
	// the fade range, or keep seperate pointers so we know exactly what's
	// been faded.  But this is really just a safety check, we shouldn't
	// have overlapping fades within the fade range.
	if (con->frames > 0) {
		mForegroundFaded = false;
		mBackgroundFaded = false;
	}
}

/**
 * Inner window appender.  Called directly by the Plugins that don't
 * need the LayerContext consistency checking.
 */
PUBLIC void FadeWindow::add(float* src, long frames)
{
	long end = (mWindowFrames * mChannels);
	for (int i = 0 ; i < frames ; i++) {
		for (int j = 0 ; j < mChannels ; j++) {
			// note that src can be NULL during insert mode
			float sample = 0.0;
			if (src != NULL)
			  sample = *src++;
			sample = mFade.fade(sample);
			mBuffer[mCursor++] = sample;
		}
		if (mCursor >= end)
		  mCursor = 0;
		mFrames++;
		mFade.inc(0, false);
	}
}

/**
 * Common utility method to locate the range of valid content in the window.
 * Have to save the result in transient instance variables, but better
 * than duplicating the logic.  The result is only valid until the next time
 * content is added to the window.
 */
PRIVATE void FadeWindow::locateEdges(int fadeFrames)
{
	// since we always move forward, frames to the right of
	// the cursor came before the frames on the left

	int cursorFrame = mCursor / mChannels;

	mLeftFrames = cursorFrame;
	mLeftOffset = 0;
	mRightFrames = mWindowFrames - cursorFrame;
	mRightOffset = 0;

	if (mFrames < mWindowFrames) {
		// the window is not completely full
		// probably can't happen, but not that hard to allow
        CovFwinLocateIncompleteWindow = true;
		if (mFrames <= mLeftFrames) {
			mLeftOffset = mLeftFrames - mFrames;
			mRightFrames = 0;
		}
		else {
			mRightFrames = mFrames - mLeftFrames;
			mRightOffset = mWindowFrames - mRightFrames;
		}
	}

	// an adjustment used only when making a partial fade to the 
    // contents of the window
	if (fadeFrames > 0) {
        CovFwinLocatePartialFade = true;
		if (fadeFrames <= cursorFrame) {
			// fade range entirely on the left
			long notCovered = cursorFrame - fadeFrames;
			mRightFrames = 0;
			mLeftOffset += notCovered;
			mLeftFrames -= notCovered;
		}
		else {
			// split between two sides
			long notCovered = mRightFrames - (fadeFrames - cursorFrame);
			mRightOffset += notCovered;
			mRightFrames -= notCovered;
		}
	}

	mLeftBuffer = &mBuffer[mLeftOffset * mChannels];
	mRightBuffer = &mBuffer[(cursorFrame + mRightOffset) * mChannels];
}

/**
 * Add or remove the contents of the window to or from an Audio object.
 * When op is OpRemove it will have the effect of removing the foreground
 * content from the Audio, OpAdd puts it back.
 */
PUBLIC void FadeWindow::applyWindow(AudioCursor* cursor, AudioOp op)
{
    AudioBuffer ab;
    long startFrame;

	ab.channels = mChannels;

    // we always record forward, but it may have been placed into the
    // Audio in reverse, so must also add in reverse
    cursor->setReverse(mReverse);

    if (mHeadWindow)
      startFrame = 0;
    else {
        // note that mLastExternalFrame is actually 1+ the last frame
        // in this window
		if (mFrames < mWindowFrames)
		  startFrame = mLastExternalFrame - mFrames;
		else
		  startFrame = mLastExternalFrame - mWindowFrames;
		  
    }
    startFrame = cursor->reflectFrame(startFrame);

    locateEdges(0);

    if (mRightFrames > 0) {
        ab.buffer = mRightBuffer;
        ab.frames = mRightFrames;
        cursor->put(&ab, op, startFrame);
    }
    if (mLeftFrames > 0) {
        ab.buffer = mLeftBuffer;
        ab.frames = mLeftFrames;
        // note that we have to "reflect" the increment
        long destFrame;
        if (mReverse)
          destFrame = startFrame - mRightFrames;
        else
          destFrame = startFrame + mRightFrames;
        cursor->put(&ab, op, destFrame);
    }
}

PUBLIC void FadeWindow::removeForeground(AudioCursor* cursor)
{
    applyWindow(cursor, OpRemove);
}

PUBLIC void FadeWindow::addForeground(AudioCursor* cursor)
{
    applyWindow(cursor, OpAdd);
}

/**
 * Perform a foreground fade.  The contents of the window is removed
 * from the Audio, faded, then put back.
 */
PUBLIC void FadeWindow::fadeForeground(AudioCursor* cursor, 
                                       float baseLevel)
{
	bool up = mHeadWindow;
    long fadeOffset = 0;

	// ignore if empty
	if (mFrames == 0) return;

	if (mForegroundFaded) {
		Trace(1, "Fade window already applied to foreground!\n");
		return;
	}

	// window merging must be done in the dirction it was captured
	bool saveReverse = cursor->isReverse();
	cursor->setReverse(mReverse);

    if (mFrames < mWindowFrames) {
        // we didn't get an entire window, if fading up just do what
        // we can, if fading down adjust offset so we reach zero
        if (!up)
          fadeOffset = mWindowFrames - mFrames;
    }

    // first remove the window from the Audio, this also calls locateEdges
    removeForeground(cursor);

	// fade the right side of the window
	if (mRightFrames > 0) {
        CovFwinFadeRight = true;
        if (baseLevel == 1.0) {
            AudioFade::fade(mRightBuffer, mChannels, 0, mRightFrames,
                            fadeOffset, up);
        }
        else {
            CovFwinFadeRightLevel = true;
            AudioFade::fadePartial(mRightBuffer, mChannels, 0, mRightFrames,
                                   fadeOffset, up, baseLevel);
        }
        fadeOffset += mRightFrames;
    }

	// fade the left side of the window
	if (mLeftFrames > 0) {
        CovFwinFadeLeft = true;
        if (baseLevel == 1.0) {
            AudioFade::fade(mLeftBuffer, mChannels, 0, mLeftFrames,
                            fadeOffset, up);
        }
        else {
            CovFwinFadeLeftLevel = true;
            AudioFade::fadePartial(mLeftBuffer, mChannels, 0, mLeftFrames,
                                   fadeOffset, up, baseLevel);
        }
    }

    // put the window back
    addForeground(cursor);

    // restore the cursor
	cursor->setReverse(saveReverse);

	// only set the faded flags if we did a full fade
	if (baseLevel == 1.0)
      mForegroundFaded = true;
}

/**
 * Special foreground fade that shifts the fade range to the right.
 * Used when we need to apply fade out to an overdub whose tail
 * crosses a layer boundary.
 *
 * The "fadeFrames" argument has the number of frames that should be
 * in the fade.
 */
PUBLIC void FadeWindow::fadeForegroundShifted(AudioCursor* cursor, 
                                              int fadeFrames)
{
	bool up = mHeadWindow;
    long fadeOffset = 0;

	// ignore if empty
	if (mFrames == 0) return;

	if (mForegroundFaded) {
		Trace(1, "Fade window already applied to foreground!\n");
		return;
	}

    // this makes sense only for the tail window
    if (mHeadWindow)
      Trace(1, "fadeForegroundShifted called with head window!\n");

	// window merging must be done in the dirction it was captured
	bool saveReverse = cursor->isReverse();
	cursor->setReverse(mReverse);

    long shift = 0;
	long windowFrames = (mFrames < mWindowFrames) ? mFrames : mWindowFrames;
    if (windowFrames >= fadeFrames) {
        // ignore this much of the window
        shift = windowFrames - fadeFrames;
    }
    else {
        // We don't have enough in this window to meet the shifted request
        // this would have to be an extremely short overdub over the 
        // layer boundary.  In practice we can only be here when fading
        // down so adjust the offset so we still reach the expected level.
        if (!up)
          fadeOffset = fadeFrames - windowFrames;
    }

    // remove the window from the Audio, this also calls locateEdges
    removeForeground(cursor);

	float* shiftedRightBuffer = mRightBuffer;
	long shiftedRightFrames = mRightFrames;
	if (shift > 0) {
		shiftedRightBuffer += (shift * mChannels);
		shiftedRightFrames -= shift;
	}

	// fade the right side of the window
	if (shiftedRightFrames > 0) {
        CovFwinFadeRightShift = true;
        AudioFade::fade(shiftedRightBuffer, mChannels, 0, shiftedRightFrames,
                        fadeOffset, up);
        fadeOffset += shiftedRightFrames;
    }

	// fade the left side of the window
	float* shiftedLeftBuffer = mLeftBuffer;
	long shiftedLeftFrames = mLeftFrames;
	if (shift > 0) {
		if (mRightFrames <= shift) { 
			// the shift spills over into the left window
			CovFwinFadeLeftShiftTotal = true;
			long leftShift = shift - mRightFrames;
			shiftedLeftBuffer += (leftShift * mChannels);
			shiftedLeftFrames -= leftShift;	
		}
		else
		  CovFwinFadeLeftShiftPartial = true;
	}

	// fade the left side of the window
	if (shiftedLeftFrames > 0) {
        CovFwinFadeLeftShift = true;
        AudioFade::fade(shiftedLeftBuffer, mChannels, 0, shiftedLeftFrames,
                        fadeOffset, up);
    }

    // put the window back
    addForeground(cursor);

    // restore the cursor
	cursor->setReverse(saveReverse);

    // This is only considered a full fade if the entire fadeRange
    // is included, which shouldn't happen if we're calling this method
    if (fadeFrames >= windowFrames)
      mForegroundFaded = true;
}

/**
 * Called by layer when a downward tail fade was applied that overlapped
 * a portion of the head window.  We have to make a corresponding fade
 * to the contents of the window so that it matches what happened
 * to the underlying Audio.
 *
 * The startFrame argument has the location of the start of the fade
 * within the entire layer.  The fadeOffset argument has the location
 * of the start frame within the fade range.
 */
PUBLIC void FadeWindow::fadeWindow(long startFrame, long fadeOffset)
{
	if (mFrames > 0) {
		long baseFrame = 0;
		if (!mHeadWindow)
		  baseFrame = mLastExternalFrame - mFrames;
    
		// relative start frame within this window
		long localStartFrame = startFrame - baseFrame;

		// number of frames to fade in this window
		long fadeFrames = mFrames - localStartFrame;

		if (fadeFrames > 0) {

			// locate the affected content, work backwards from the cursor
			locateEdges(fadeFrames);

			if (mRightFrames > 0) {
                CovFwinFadeLocalRight = true;
                AudioFade::fade(mRightBuffer, mChannels, 0, mRightFrames, 
                                fadeOffset, false);
            }

			if (mLeftFrames > 0) {
                CovFwinFadeLocalLeft = true;
                AudioFade::fade(mLeftBuffer, mChannels, 0, mLeftFrames,
                                fadeOffset + mRightFrames, false);
            }
		}
	}
}

/**
 * Special fade function for plugins.
 * Given what is assumed to be a tail window, extract the end of the window in
 * reverse and apply a down fade.
 *
 * The buffer is at least as large as the maximum fade range.
 * Return the number of frames actually put into the buffer.  This is usually
 * the same as the fade range but may be less if the window isn't full.
 */
PRIVATE long FadeWindow::reverseFade(float* buffer)
{
	long frames = 0;

	if (mFrames > 0) {

		long range = AudioFade::getRange();
		float* dest = buffer;
		int i;

		// locate the affected content, work backwards from the cursor
		locateEdges(0);

		// copy the window backwards from the end into the buffer
		// since we always move forward, frames to the right of
		// the cursor came before the frames on the left

		float* src = mLeftBuffer + ((mLeftFrames - 1) * mChannels);
		for (i = 0 ; i < mLeftFrames && frames < range ; i++) {
			float* frame = src;
			for (int j = 0 ; j < mChannels ; j++)
			  *dest++ = *frame++;
			frames++;
			src -= mChannels;
		}

		src = mRightBuffer + ((mRightFrames - 1) * mChannels);
		for (i = 0 ; i < mRightFrames && frames < range ; i++) {
			float* frame = src;
			for (int j = 0 ; j < mChannels ; j++)
			  *dest++ = *frame++;
			frames++;
			src -= mChannels;
		}

		if (frames < range) {
			// A partial window, this would be rare, in the case of a plugin it 
			// means that we disconnected it almost immediately assume in this
			// case that we were performing a startup fade in, and the reverse
			// tail we just captured will already end in a zero crossing.
			// Could make sure by doing a fade over a reduced range but 
			// AudioFade doesn't have anything handy.
			Trace(2, "FadeWindow::reverseFade window too small, assuming edge fade\n");
		}
		else {
			// perform a downward fade on the reversed tail
			AudioFade::fade(buffer, mChannels, 0, range, 0, false);
		}
	}

	return frames;
}

PRIVATE void FadeWindow::dump(const char* name)
{
	char filename[128];

	// first the raw window
	sprintf(filename, "%s.wav", name);
	Audio* a = new Audio();
	a->append(mBuffer, mWindowFrames);
	a->write(filename);
	
	// then properly arranged
	printf("FadeWindow cursor %d\n", mCursor);
	sprintf(filename, "%s2.wav", name);
	a->reset();
	locateEdges(0);
	a->append(mRightBuffer, mRightFrames);
	a->append(mLeftBuffer, mLeftFrames);
	a->write(filename);

    delete a;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
