/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Truncate the loop on the left or right.
 * 
 * This is what I originaly thought StartPoint did, but it seemed useful
 * so I left it in and renamed it TrimStart.
 *
 * The play frame must also be adjusted so that it is relative to zero.
 * This won't produce a skip in the audio provided that we haven't
 * looped back and started buffering in the region we're about to truncate.
 *
 * Q: Does the resulting loop contain the same number of cycles, or
 * does it become a single cycle loop?  I'm assuming the same.
 *
 * The effect is similar to an unrounded multiply except that we perform
 * it immediately.
 *
 * Truncation on the right is just like an unrounded multiply from the
 * beginning except that we keep the current number of cycles.
 *
 * NOTE: For some reason this has not been scheduling JumpPlayEvents
 * if we're quantized.  Not sure why.
 */

#include <stdio.h>
#include <memory.h>
#include <string.h>

#include "Action.h"
#include "Event.h"
#include "EventManager.h"
#include "Function.h"
#include "Layer.h"
#include "Loop.h"
#include "Messages.h"
#include "Mode.h"
#include "Stream.h"
#include "Synchronizer.h"
#include "Track.h"

//////////////////////////////////////////////////////////////////////
//
// TrimEvent
//
//////////////////////////////////////////////////////////////////////

class TrimEventType : public EventType {
  public:
	TrimEventType();
};

PUBLIC TrimEventType::TrimEventType()
{
	name = "Trim";
}

PUBLIC EventType* TrimEvent = new TrimEventType();

//////////////////////////////////////////////////////////////////////
//
// TrimFunction
//
//////////////////////////////////////////////////////////////////////

/**
 * How should this handle scheduleSwitchStack?
 * Could do a LoopCopy=Sound, followed by a trim at the current position
 * in the source loop.  Once stacked, this could be changed by other 
 * Trim functions.
 *
 */
class TrimFunction : public Function {
  public:
	TrimFunction(bool start);
	void doEvent(Loop* l, Event* e);
  private:
    long calcCycleCount(Layer* layer, long newFrames);
	bool start;
};

PUBLIC Function* TrimStart = new TrimFunction(true);
PUBLIC Function* TrimEnd = new TrimFunction(false);

PUBLIC TrimFunction::TrimFunction(bool startop)
{
	eventType = TrimEvent;
	cancelReturn = true;
    start = startop;
	quantized = true;
	mayCancelMute = true;
	instant = true;

    if (start) {
		setName("TrimStart");
		setKey(MSG_FUNC_TRIM_START);
		setHelp("Remove the loop prior to the current frame");
    }
    else {
		setName("TrimEnd");
		setKey(MSG_FUNC_TRIM_END);
		setHelp("Remove the loop after the current frame");
    }
}

PUBLIC void TrimFunction::doEvent(Loop* loop, Event* event)
{
    EventManager* em = loop->getTrack()->getEventManager();
	InputStream* input = loop->getInputStream();
	OutputStream* output = loop->getOutputStream();
	long frame = loop->getFrame();

	if (event->function == TrimStart) {

		Layer* play = loop->getPlayLayer();

		if (play == NULL) {
			Trace(loop, 1, "Loop: TrimStartEvent without play layer");
		}
		else if (frame == 0) {
			// I don't think this can happen due to input latency adjust?
			Trace(loop, 2, "Loop: Ignoring TrimStart at zero\n");
		}
		else {
			long newFrames = loop->getFrames() - frame;

			// can't set the loop smaller than these yet
			if (newFrames < input->latency || newFrames < output->latency) {
				Trace(loop, 1, "Loop: Ignoring start point, loop too small!\n");
			}
			else {
				Layer* record = loop->getRecordLayer();

                // adjust cycle count for cut
                long cycles = calcCycleCount(record, newFrames);

				// splice out the section, just like unrounded multiply
				record->splice(input, frame, newFrames, cycles);

				// treat it like an unrounded multiply
				Synchronizer* sync = loop->getSynchronizer();
                sync->loopResize(loop, true);

				loop->shift(false);

				// Subtlety: shift() set the Stream's layer shift flag 
				// to prevent a fade in which is what you usually want when
				// transitioning from the recordlayer back to the play layer.
				// Here though, we've restructured the layer so we may need
				// to fade in based on layer/frame info
				output->setLayerShift(false);

				// any recording we may have been doing is meaningless
				input->resetHistory(loop);

				// have to shift events to adjust for the truncation
				// ?? unrounded multiply would perform another shift of the
				// new loop length to bring events for the next loop into this
				// loop, I don't think that applies here since haven't
				// reached the end of this loop yet
				frame = loop->getFrame();
				em->shiftEvents(frame);

				// if playback hasn't looped, we can continue from where we are
				// without a fade, if we have looped, leave the stream state
				// alone so we can do a fade
				if (loop->getPlayFrame() > frame) {
					output->adjustLastFrame(-frame);
					output->setLayerShift(true);
				}

				loop->checkMuteCancel(event);

				// I think we can only be in Play mode here, how would this
				// interact with other modes?
				loop->resumePlay();

				// warp the frame counters
				loop->setFrame(0);
				loop->recalculatePlayFrame();
			}
		}
	}
	else {
		// TrimEnd
		long newFrames = frame;

		if (newFrames < output->latency || newFrames < input->latency) {
			Trace(loop, 1, "Loop: Ignoring TrimEnd event, loop to small\n");
		}
		else {
			Layer* record = loop->getRecordLayer();
            // adjust cycle count for cut
            long cycles = calcCycleCount(record, newFrames);
			record->splice(input, 0, newFrames, cycles);

			// treat it like an unrounded multiply
			Synchronizer* sync = loop->getSynchronizer();
			sync->loopResize(loop, true);

			loop->shift(false);
			
			// Subtlety: shift() set the Stream's layer shift flag 
			// to prevent a fade in which is what you usually want when
			// transitioning from the recordlayer back to the play layer.
			// Here though, we've restructured the layer so we may need
			// to fade in based on layer/frame info
			output->setLayerShift(false);

			// recalculate frames
			loop->setFrame(0);
			loop->recalculatePlayFrame();

			// we've effectively entered the next loop, shift events
			em->shiftEvents(loop->getFrames());

			loop->checkMuteCancel(event);
			loop->resumePlay();
		}
    }

	loop->validate(event);
}

/**
 * Calculate the cycle count for the trimmed loop.
 * Prior to 2.6 we retained the original cycle count which
 * was unexpected if you cut an exact number of cycles.  Make this
 * and unrounded multiply behave the same.  Reduce the cycle
 * count if it was an exact cut, otherwise resize to one cycle.
 * I suppose we could have a parameter to retain the current count
 * but now that we can set cycleCount in scripts it's not that
 * important.
 */
PRIVATE long TrimFunction::calcCycleCount(Layer* layer, long newFrames)
{
    // resizes to 1 if not exact
    long cycles = 1;

    long cycleFrames = layer->getCycleFrames();
    if (newFrames > cycleFrames && ((newFrames % cycleFrames) == 0)) {
        cycles = newFrames / cycleFrames;
        // can't happen but be paranoid
        if (cycles == 0) cycles = 1;
    }
    return cycles;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
