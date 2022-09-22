/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Loop Windowing.
 * 
 * Rebuild the play layer to contain a section within the entire loop history.
 * If the record layer has been modified, it is discarded.  Loop windowing
 * is similar to Undo, any pending changes are lost.  We could consider
 * an option to do a shift before windowing but the odd thing is that
 * if you did WindowBackward, you would hear the same loop since 
 * we effectively advanced to the end of the record layer, then moved
 * backward over it. This seems odd and compliates things.  
 */

#include <stdio.h>
#include <memory.h>
#include <string.h>

#include "Util.h"

#include "Action.h"
#include "Event.h"
#include "EventManager.h"
#include "Function.h"
#include "Layer.h"
#include "Loop.h"
#include "Mobius.h"
#include "Mode.h"
#include "Messages.h"
#include "Preset.h"
#include "Segment.h"
#include "Stream.h"
#include "Synchronizer.h"
#include "Track.h"

//////////////////////////////////////////////////////////////////////
//
// WindowMode
//
// Minor mode active when in windowing.
//
//////////////////////////////////////////////////////////////////////

class WindowModeType : public MobiusMode {
  public:
	WindowModeType();
};

WindowModeType::WindowModeType() :
    MobiusMode("window", MSG_MODE_WINDOW)
{
	minor = true;
}

MobiusMode* WindowMode = new WindowModeType();

//////////////////////////////////////////////////////////////////////
//
// WindowEvent
//
//////////////////////////////////////////////////////////////////////

class WindowEventType : public EventType {
  public:
	WindowEventType();
};

PUBLIC WindowEventType::WindowEventType()
{
	name = "Window";
}

PUBLIC EventType* WindowEvent = new WindowEventType();

//////////////////////////////////////////////////////////////////////
//
// OverflowStyle
//
//////////////////////////////////////////////////////////////////////

/**
 * Overflow handling styles.
 *
 * Overflow can be handled three ways:
 *
 *    - truncate on the overflow edge
 *    - push back from the overflow edge
 *    - ignore if there is an overflow
 *
 * For a layer with 1000 frames an offset of 800 and a length of 300.
 * Truncating the edge results in a window offset 800 and a length of 200,
 * in effect the window becomes shorter.  This feels right if you are
 * moving just that edge, but when moving the entire window it feels like you
 * should just stop when you hit the edge.
 *
 * For push back, the offset would be 700 and the length 300.
 *
 * When sliding the loop window, the third option seems right, otherwise
 * if you hit an edge and adjust the start/end of the window, sliding in
 * the other direction will not produce layers with the same content as they
 * had before.  Layer 1000 frames, window length 10 frames, offset 23.
 *
 * WindowBackward once has offset 13, window back twice has either:
 *
 *     - method 1: offset 3 length 3
 *     - method 2: offset 10 length 10
 *     - method 3: offset 13 length 10
 *
 * WindowForward then has either:
 *
 *     - method 1: offst 13 length 10
 *     - method 2: offset 20 length 10
 *     - method 3: offset 23 length 10
 *
 * For Backward and Forward, the third method seems best so the window 
 * contents are the same after you hit an edge.
 *
 * For WindowMove method 1 seems best.
 *
 * For WindowMoveStart and WindowMoveEnd method 2 is used, we just limit
 * the start to zero and the end to the maximum loop length.
 *
 * UPDATE: When testing this I didn't like IGNORE when sliding, I think the
 * expectation is that we go as far as we can so the default when sliding
 * is PUSH.
 */
typedef enum {

    OVERFLOW_TRUNCATE,
    OVERFLOW_PUSH,
    OVERFLOW_IGNORE

} OverflowStyle;

//////////////////////////////////////////////////////////////////////
//
// WindowFunction
//
//////////////////////////////////////////////////////////////////////

class WindowFunction : public Function {
  public:
	WindowFunction(bool edge, bool start, int direction);
	Event* scheduleEvent(Action* action, Loop* l);
	void doEvent(Loop* l, Event* e);

  private:

    void moveWindow(Event* event);
    void resizeWindow(Event* event);
    Preset::WindowUnit getScriptUnit(ExValue* arg);
    long getUnitFrames(Preset::WindowUnit unit);
    long getMsecFrames(int msecs);
    void buildWindow();
    void constrainWindow();
    Segment* buildSegments();
    Layer* getNextLayer(Layer* src);
    void installSegments(Segment* segs);
    void calculateNewFrame();

    bool mEdge;
    bool mStart;
    int mDirection;

    // transient execution state

    Loop* mLoop;
    Layer* mLayer;
    Layer* mLastLayer;
    long mOffset;
    long mFrames;
    OverflowStyle mStyle;
    long mNewFrame;
    bool mContinuity;
    bool mIgnore;
};

PUBLIC Function* WindowBackward = new WindowFunction(false, false, -1);
PUBLIC Function* WindowForward = new WindowFunction(false, false, 1);
PUBLIC Function* WindowMove = new WindowFunction(false, false, 0);

PUBLIC Function* WindowStartBackward = new WindowFunction(true, true, -1);
PUBLIC Function* WindowStartForward = new WindowFunction(true, true, 1);
PUBLIC Function* WindowEndBackward = new WindowFunction(true, false, -1);
PUBLIC Function* WindowEndForward = new WindowFunction(true, false, 1);

PUBLIC Function* WindowResize = new WindowFunction(true, false, 0);

PUBLIC WindowFunction::WindowFunction(bool edge, bool start, int direction)
{
	eventType = WindowEvent;
	cancelReturn = true;
	mayCancelMute = true;
	instant = true;

    mEdge = edge;
    mStart = start;
    mDirection = direction;

    if (mEdge) {
        if (mDirection == 0) {
            setName("WindowResize");
            scriptOnly = true;
            variableArgs = true;
        }
        else if (mStart) {
            if (mDirection < 0) {
                setName("WindowStartBackward");
                setKey(MSG_FUNC_WINDOW_START_BACKWARD);
            }
            else {
                setName("WindowStartForward");
                setKey(MSG_FUNC_WINDOW_START_FORWARD);
            }
        }
        else {
            if (mDirection < 0) {
                setName("WindowEndBackward");
                setKey(MSG_FUNC_WINDOW_END_BACKWARD);
            }
            else {
                setName("WindowEndForward");
                setKey(MSG_FUNC_WINDOW_END_FORWARD);
            }
        }
    }
    else {
        if (mDirection == 0) {
            setName("WindowMove");
            scriptOnly = true;
            variableArgs = true;
        }
        else if (mDirection < 0) {
            setName("WindowBackward");
            setKey(MSG_FUNC_WINDOW_BACKWARD);
        }
        else {
            setName("WindowForward");
            setKey(MSG_FUNC_WINDOW_FORWARD);
        }
    }
}

Event* WindowFunction::scheduleEvent(Action* action, Loop* l)
{
	return Function::scheduleEvent(action, l);
}

/**
 * This one is relatively unusual because we modify the Play layer to
 * play the moving window, this does not represent an editable change that
 * will shift a new layer.
 */
void WindowFunction::doEvent(Loop* loop, Event* event)
{
    // We're going to assume that the current record layer is lost,
    // we only window within layers that have been finalized.  Could
    // do a shift here but we still have to base our relative moves
    // on the play layer since that's what we're hearing now.
    Layer* layer = loop->getPlayLayer();

    if (layer != NULL) {

        // We need to pass a bunch of state around, use transient
        // fields since we're never reentrant.
        mLoop = loop;
        mLayer = layer;
        mLastLayer = layer;
        mOffset = layer->getWindowOffset();
        mFrames = layer->getFrames();
        mStyle = OVERFLOW_PUSH;
        mNewFrame = 0;
        mContinuity = false;
        mIgnore = false;

        if (mOffset < 0) {
            // haven't been windowing yet
            mOffset = layer->getHistoryOffset();
        }

        if (!mEdge) {
            // default is PUSH, don't have an option to override that
            moveWindow(event);
        }
        else {
            mStyle = OVERFLOW_TRUNCATE;
            resizeWindow(event);
        }

        buildWindow();

        if (!mIgnore) {
            loop->checkMuteCancel(event);
            // trace...
        }
    }
}

/**
 * Recalculate the window offset. 
 * 
 * The WindowSlideUnit preset parameter determines the amount of slide.
 * The WindowSlideAmount preset parameter has the number of units.  If not
 * set the amount is 1.
 */
PRIVATE void WindowFunction::moveWindow(Event* event)
{
    int amount = -1;

    // optional argument can specify units to shift
    Action* action = event->getAction();
    if (action != NULL) {
        if (action->arg.getType() == EX_INT) {
            int ival = action->arg.getInt();
            // !! need a configurable sanity check on the upper range here?
            amount = ival;
        }
    }

    Preset* p = mLoop->getPreset();
    Preset::WindowUnit unit = p->getWindowSlideUnit();
    long unitFrames = 0;

    if (amount <= 0) {
        amount = p->getWindowSlideAmount();
        if (amount <= 0)
          amount = 1;
    }

    if (mDirection == 0) {
        // WindowMove, unit and amount specified with arguments
        ExValueList* args = action->scriptArgs;
        if (args == NULL || args->size() == 0)
          Trace(mLoop, 1, "WindowMove called without arguments\n");
        else {
            ExValue* arg = args->getValue(0);
            Preset::WindowUnit argUnit = getScriptUnit(arg);
            if (argUnit == Preset::WINDOW_UNIT_INVALID) {
                // should be an int
                amount = arg->getInt();
                if (args->size() > 1) 
                  Trace(mLoop, 1, "WindowMove called with and extra args\n");
            }                
            else {
                unit = argUnit;
                // amount defaults to 1
                if (args->size() > 1) {
                    arg = args->getValue(1);
                    amount = arg->getInt();
                }
            }
        }
    }

    if (unit == Preset::WINDOW_UNIT_START) {
        mOffset = 0;
    }
    else if (unit == Preset::WINDOW_UNIT_END) {
        mOffset = mLoop->getHistoryFrames() - mLoop->getFrames();
    }
    else if (unit == Preset::WINDOW_UNIT_LAYER) {
        // ignore this, doesn't seem that useful
        Trace(mLoop, 1, "WindowMove layer not implemented\n");
    }
    else {
        long unitFrames = getUnitFrames(unit);
        long slideFrames = amount * unitFrames;
        if (mDirection >= 0)
          mOffset += slideFrames;
        else
          mOffset -= slideFrames;
    }
}

/**
 * Adjust an edge which may change both the offset and size.
 */
PRIVATE void WindowFunction::resizeWindow(Event* event)
{
    int amount = 0;

    Action* action = event->getAction();
    if (action != NULL) {
        if (action->arg.getType() == EX_INT) {
            int ival = action->arg.getInt();
            amount = ival;
        }
    }

    Preset* p = mLoop->getPreset();
    Preset::WindowUnit unit = p->getWindowEdgeUnit();
    long unitFrames = 0;

    if (amount <= 0) {
        amount = p->getWindowEdgeAmount();
        if (amount <= 0)
          amount = 1;
    }

    bool start = mStart;
    if (mDirection == 0) {
        // WindowResize: edge, unit and amount specified with args
        ExValueList* args = action->scriptArgs;
        if (args == NULL || args->size() == 0)
          Trace(mLoop, 1, "WindowResize with no arguments\n");
        else {
            ExValue* arg = args->getValue(0);
            const char* str = arg->getString();
            if (StringEqualNoCase(str, "start"))
              start = true;
            else if (!StringEqualNoCase(str, "end")) {
                Trace(mLoop, 1, "WindowResize with invalid direction\n");
                amount = 0;
            }

            if (amount > 0 && args->size() > 1) {
                arg = args->getValue(1);
                Preset::WindowUnit argUnit = getScriptUnit(arg);
                if (argUnit == Preset::WINDOW_UNIT_INVALID) {
                    // should be an int
                    amount = arg->getInt();
                    if (args->size() > 2) 
                      Trace(mLoop, 1, "WindowResize with extra args\n");
                }                
                else if (argUnit == Preset::WINDOW_UNIT_LAYER ||
                         argUnit == Preset::WINDOW_UNIT_START ||
                         argUnit == Preset::WINDOW_UNIT_END) {
                    // these aren't supported for resize
                    Trace(mLoop, 1, "WindowResize with invalid unit\n");
                    amount = 0;
                }
                else {
                    unit = argUnit;
                    // amount defaults to 1
                    if (args->size() > 2) {
                        arg = args->getValue(2);
                        amount = arg->getInt();
                    }
                }
            }
        }
    }

    if (amount != 0) {

        // for WindowMove the polarity of the amount defines the direction
        bool forward = (mDirection > 0);
        if (mDirection == 0) {
            if (amount > 0)
              forward = true;
            else 
              amount = 0 - amount;
        }

        long unitFrames = getUnitFrames(unit);
        long resizeFrames = amount * unitFrames;

        if (start) {
            if (forward) {
                mOffset += resizeFrames;
                mFrames -= resizeFrames;
            }
            else {
                mOffset -= resizeFrames;
                mFrames += resizeFrames;
            }
        }
        else {
            if (forward)
              mFrames += resizeFrames;
            else
              mFrames -= resizeFrames;
        }
    }


    int a = 0;
    int b = a -1;
    Trace(mLoop, 1, "Experiment %ld\n", (long)b);
    
}

/**
 * Convert the unit from a string script argument to an enumeration value.
 * There are two parmaeters we could use, WindowSlideUnitParameter and
 * WindowEdgeUnitParameter but they both have the same values.
 */
PRIVATE Preset::WindowUnit WindowFunction::getScriptUnit(ExValue* arg)
{
    Preset::WindowUnit unit = Preset::WINDOW_UNIT_INVALID;
    const char* str = arg->getString();
    int ordinal = WindowSlideUnitParameter->getEnumValue(str);

    if (ordinal >= 0)
      unit = (Preset::WindowUnit)ordinal;
    else {
        // These aren't included in the parameter definitions since they
        // are not visible so we have to check for them oursleves.
        if (StringEqualNoCase(str, "start"))
          unit = Preset::WINDOW_UNIT_START;
        else if (StringEqualNoCase(str, "end"))
          unit = Preset::WINDOW_UNIT_END;
        else if (StringEqualNoCase(str, "layer"))
          unit = Preset::WINDOW_UNIT_LAYER;
    }

    return unit;
}
        
/**
 * Calculate the number of frames in one unit.
 */
PRIVATE long WindowFunction::getUnitFrames(Preset::WindowUnit unit)
{
    long frames = 0;

    if (unit == Preset::WINDOW_UNIT_LOOP)
      frames = mLoop->getFrames();

    else if (unit == Preset::WINDOW_UNIT_CYCLE)
      frames = mLoop->getCycleFrames();

    else if (unit == Preset::WINDOW_UNIT_SUBCYCLE) {
        if (mLayer->getWindowOffset() < 0)
          frames = mLoop->getSubCycleFrames();
        else {
            // use the saved original subcycle size
            frames = mLayer->getWindowSubcycleFrames();
        }
    }
    else if (unit == Preset::WINDOW_UNIT_MSEC) {
        frames = getMsecFrames(1);
    }
    else if (unit == Preset::WINDOW_UNIT_FRAME) {
        frames = 1;
    }
    else {
        // LAYER, START, END used only in WindowMove, location is calculated
    }

    return frames;
}

/**
 * Calculate the number of frames corresponding to a number of milliseconds.
 * This is adjusted relative to the playback speed since you want to 
 * hear the change the same way regardless of the speed.
 */
PRIVATE long WindowFunction::getMsecFrames(int msecs)
{
    // milliseconds are relative to the playback rate
    // !! do they need to be?
    float rate = mLoop->getTrack()->getEffectiveSpeed();

    // should we ceil()?
   long frames = (long)(MSEC_TO_FRAMES(msecs) * rate);

   return frames;
}

/**
 * Rebuild the window layer.  Window state is passed in transient
 * fields in the fucntion object.
 */
PRIVATE void WindowFunction::buildWindow()
{
    constrainWindow();
    if (!mIgnore) {
        Segment* segments = buildSegments();
        if (!mIgnore)
          installSegments(segments);
    }
}

/**
 * Constrain the edges of the new window.
 * Sets mLastLayer, mOffset, mFrames.
 */
PRIVATE void WindowFunction::constrainWindow()
{
    mLastLayer = mLayer;
    long historyFrames = 0;

    if (mLayer->getWindowOffset() >= 0) {
        // this is a windowing layer, it is not part of the history
        mLastLayer = mLayer->getPrev();
    }

    if (mLastLayer == NULL) {
        // can't happen
        Trace(mLoop, 1, "Window: Missing layer history!\n");
        mIgnore = true;
    }

    if (!mIgnore) {
        historyFrames = mLastLayer->getHistoryOffset() + mLastLayer->getFrames();

        Trace(mLoop, 2, "Window: Constraining window offset %ld frames %ld history %ld\n",
              (long)mOffset, (long)mFrames, (long)historyFrames);
    }

    // constrain the left edge
    if (mOffset < 0) {
        if (mStyle == OVERFLOW_IGNORE) {
            Trace(mLoop, 2, "Window: Igoring window with negative offset\n");
            mIgnore = true;
        }
        else if (mStyle == OVERFLOW_TRUNCATE) {
            Trace(mLoop, 2, "Window: Truncating window with negative offset\n");
            mFrames += mOffset;
            mOffset = 0;
        }
        else {
            Trace(mLoop, 2, "Window: Pushing window with negative offset\n");
            mOffset = 0;
        }
    }

    // constrain the right edge
    if (!mIgnore) {
        long maxFrame = historyFrames - 1;
        long endFrame = (mOffset + mFrames) - 1;
        if (endFrame < mOffset) {
            // how did frames go negative? calculation error somewhere
            Trace(mLoop, 1, "Window: Ignoring window with negative length!\n");
            mIgnore = true;
        }
        else if (endFrame > maxFrame) {
            if (mStyle == OVERFLOW_IGNORE) {
                Trace(mLoop, 2, "Window: Igoring window with overflow\n");
                mIgnore = true;
            }
            else if (mStyle == OVERFLOW_TRUNCATE) {
                Trace(mLoop, 2, "Window: Truncating window with overflow\n");
                mFrames -= (endFrame - maxFrame);
            }
            else {
                mOffset -= (endFrame - maxFrame);
                if (mOffset >= 0)
                  Trace(mLoop, 2, "Window: Pushing window with overflow\n");
                else {
                    // Window is larger than the size of the entire history,
                    // this should only happen from a bad script.
                    Trace(mLoop, 2, "Window: Constraining push, window too large\n");
                    mOffset = 0;
                }
            }
        }
    }

    // check size
    if (!mIgnore && mFrames < mLoop->getMinimumFrames()) {
        // should only happen when truncating, or from a script
        // I don't think we need to get fancy here, just ignore it
        Trace(mLoop, 2, "Window: Ignoring window less than minimum size\n");
        mIgnore = true;
    }
    
    // check for noops
    if (!mIgnore && 
        mFrames == mLayer->getFrames() &&
        ((mLayer->getWindowOffset() >= 0 && mOffset == mLayer->getWindowOffset()) ||
         (mLayer->getWindowOffset() < 0 && mOffset == mLayer->getHistoryOffset()))) {
        Trace(mLoop, 2, "Window: Ignoring noop window change\n");
        mIgnore = true;
    }

    if (!mIgnore) {
        Trace(mLoop, 2, "Window: Constrained window offset %ld frames %ld\n",
              (long)mOffset, (long)mFrames);
    }
}

/**
 * Build the segment list.
 */
PRIVATE Segment* WindowFunction::buildSegments()
{
    // find the layer containing the offset
    Layer* startLayer = mLastLayer;
    while (startLayer != NULL) {
        if (startLayer->getHistoryOffset() > mOffset)
          startLayer = startLayer->getPrev();
        else
          break;
    }

    if (startLayer == NULL) {
        // ran off the end on the left, some calculation above was wrong
        Trace(mLoop, 1, "Window: Unable to find layer with offset %ld\n",
              (long)mOffset);
        mIgnore = true;
    }

    // build segments
    Segment* segments = NULL;
    if (!mIgnore) {
        Segment* lastSegment = NULL;
        Layer* curLayer = startLayer;
        long refOffset = mOffset - startLayer->getHistoryOffset();
        long need = mFrames;
        long layerFrame = 0;

        while (need > 0 && curLayer != NULL) {

            long avail = curLayer->getFrames() - refOffset;
            long take = (avail > need) ? need : avail;

            if (take <= 0) {
                // either the layer is empty or the offset is too high
                // must be a calculation error somewhere
                Trace(mLoop, 1, "Window: Invalid layer take %ld\n",
                      (long)take);
                curLayer = NULL;
            }
            else {
                Trace(mLoop, 2, "Window: Segment for layer %ld ref offset %ld start frame %ld frames %ld\n",
                      curLayer->getNumber(), refOffset, layerFrame, take);

                Segment* seg = new Segment(curLayer);
                // keep them ordered first to last
                if (lastSegment == NULL)
                  segments = seg;
                else
                  lastSegment->setNext(seg);
                lastSegment = seg;

                // location within the parent layer
                seg->setOffset(layerFrame);
                // offset into the referenced layer
                seg->setStartFrame(refOffset);
                seg->setFrames(take);
                layerFrame += take;
                need -= take;
                if (need > 0) {
                    // sigh, this isn't doubly linked so we have
                    // to go back to the head of the list and work backward
                    curLayer = getNextLayer(curLayer);
                }
            }

            // offset only applies to the layer we started in
            refOffset = 0;
        }
        
        if (need > 0) {
            // ran off the end, calculation error somewhere
            Trace(mLoop, 1, "Window: Unable to fill segments!\n");
            while (segments != NULL) {
                Segment* next = segments->getNext();
                delete segments;
                segments = next;
            }
            segments = NULL;
            mIgnore = true;
        }
    }

    return segments;
}

/**
 * Get the layer later on the timeline than the given layer.
 * The layer model only has a "prev" pointer to the one before it,
 * to get the next one we have to search the Loop's layer list.
 * Would be nice if this could be doubly linked, is there anything
 * else that needs this?
 *
 */
PRIVATE Layer* WindowFunction::getNextLayer(Layer* src) 
{
    Layer* found = NULL;

    Layer* layer = mLoop->getPlayLayer();

    while (layer != NULL) {
        Layer* prev = layer->getPrev();
        if (prev != src)
          layer = prev;
        else {
            found = layer;
            break;
        }
    }

    return found;
}

/**
 * Install the new window segments.
 */
PRIVATE void WindowFunction::installSegments(Segment* segments)
{
    // sets mNewFrame and mContinuity;
    calculateNewFrame();

    // fade if we're going to have a discontinuity  
    if (!mContinuity) 
      mLoop->getOutputStream()->captureTail();
    else {
        // suppress a fade bump since we won't actually
        // change anything even though the frame may be different
        mLoop->getOutputStream()->setLayerShift(true);
    }

    // Like redo, flush all remaining events
    EventManager* em = mLoop->getTrack()->getEventManager();
    em->flushEventsExceptScripts();

    // splice in a windowing layer if we don't already have one
    if (mLayer->getWindowOffset() < 0) {
        Trace(mLoop, 2, "Window: Inserting window layer\n");

        Mobius* mobius = mLoop->getMobius();
        LayerPool* lp = mobius->getLayerPool();
        Layer* window = lp->newLayer(mLoop);

        // in the first window layer only, save the starting subcycle size
        window->setWindowSubcycleFrames(mLoop->getSubCycleFrames());

        Layer* rec = mLoop->getRecordLayer();
        window->setPrev(mLayer);
        mLoop->setPlayLayer(window);
        rec->setPrev(window);

        mLayer = window;
    }

    // reset segments and old compilation state, remember the subcycle frames
    long saveSubcycleFrames = mLayer->getWindowSubcycleFrames();
    mLayer->reset();
    mLayer->setWindowSubcycleFrames(saveSubcycleFrames);

    // this is what indicates we're windowing
    mLayer->setWindowOffset(mOffset);

    // set the Layer and Audio frame size
    Trace(mLoop, 2, "Window: Resizing window layer to %ld\n", mFrames);
    mLayer->zero(mFrames, 1);

    // and then set the new segments
    mLayer->setSegments(segments);

    // fade the new segments, checkConsistency is false because we built
    // them from scratch
    mLayer->compileSegmentFades(false);

    // this is also finalized since we never recorded it incrementally
    mLayer->setFinalized(true);

    // reset the record layer
    Layer* rec = mLoop->getRecordLayer();
    rec->copy(mLayer);
    
    // should have already wrapped this but make sure
    mLoop->setFrame(mNewFrame);
    mLoop->recalculatePlayFrame();

    // this state is no longer relevant, clear it to avoid trying
    // to fade something that isn't there any more
    mLoop->getInputStream()->resetHistory(mLoop);

    // Redo calls checkMuteCancel here...

    // don't leave it in a recording mode since we threw
    // away the last record layer
    mLoop->resumePlay();

    // handle this like undo, possible resize
    Synchronizer* sync = mLoop->getSynchronizer();
    sync->loopResize(mLoop, false);
}

/**
 * Calculate the new frame after the window has moved.
 * There are a few options to consider.  If the new window
 * does not contain what is currently playing, we can start
 * over from the beginning or try to maintain the same relative
 * position in the new window.  I think most people would expect
 * to start over, but the EDP probably tries to maintain the position
 * since it is based on Undo.
 *
 * If you change the edges of the window, but the window still
 * contains what is currently playing, you can keep the same 
 * relative location, it just plays to the new end, or starts
 * over with a different start.  
 *
 * Since scheduled events are always oriented around the record frame
 * setting the play frame may result in latency loss.  We're not messing
 * with a JumpPlayEvent for this, windowing is inheritly glitchy so 
 * smooth play transitions are less of an issue.
 * 
 */
PRIVATE void WindowFunction::calculateNewFrame()
{
    // assume restart
    mNewFrame = 0;

    long currentFrame = mLoop->getFrame();
    long historyOffset;

    // offset in history to the start of the current layer
    if (mLayer->getWindowOffset() >= 0)
      historyOffset = mLayer->getWindowOffset();
    else
      historyOffset = mLayer->getHistoryOffset();

    // current play frame in history
    long historyFrame = historyOffset + currentFrame;

    if (mEdge) {
        // don't restart unless we have to
        if (currentFrame < mFrames && historyFrame >= mOffset) {

            long leftDelta = historyOffset - mOffset;
            if (leftDelta < 0)
              Trace(mLoop, 2, "Window: Window reduced on the left %ld\n",  leftDelta);

            else if (leftDelta > 0)
              Trace(mLoop, 2, "Window: Window extended on the left %ld\n", leftDelta);

            long rightDelta = mFrames - mLayer->getFrames();
            if (rightDelta < 0)
              Trace(mLoop, 2, "Window: Window reduced on the right %ld\n", rightDelta);

            else if (rightDelta > 0)
              Trace(mLoop, 2, "Window: Window extended on the right %ld\n", rightDelta);


            long newFrame = currentFrame + leftDelta;
            if (currentFrame != newFrame)
              Trace(mLoop, 2, "Window: Adjusting loop frame from %ld to %ld\n",
                    currentFrame, newFrame);

            mNewFrame = newFrame;
            mContinuity = true;
        }
        else {
            // TODO: consider wrapping, or backing up slightly from the end?
            Trace(mLoop, 2, "Window: Restarting from zero after resize\n");
        }
    }
    else {
        // let move always start over
        // if (preset->isWindowRestart())...
        Trace(mLoop, 2, "Window: Restarting from zero after slide\n");
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
