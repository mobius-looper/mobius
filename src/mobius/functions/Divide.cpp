/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Instant divide.
 * 
 */

#include <stdio.h>
#include <memory.h>
#include <string.h>

#include "Util.h"

#include "Action.h"
#include "Event.h"
#include "Function.h"
#include "Layer.h"
#include "Loop.h"
#include "Messages.h"
#include "Segment.h"
#include "Stream.h"
#include "Synchronizer.h"

//////////////////////////////////////////////////////////////////////
//
// DivideEvent
//
//////////////////////////////////////////////////////////////////////

class DivideEventType : public EventType {
  public:
	DivideEventType();
};

PUBLIC DivideEventType::DivideEventType()
{
	name = "Divide";
}

PUBLIC EventType* DivideEvent = new DivideEventType();

//////////////////////////////////////////////////////////////////////
//
// DivideFunction
//
//////////////////////////////////////////////////////////////////////

/**
 * Prevent runaway multiples in scripts.
 */
#define MAX_MULTIPLE 512

class DivideFunction : public Function {
  public:
	DivideFunction(int n);
	Event* scheduleEvent(Action* action, Loop* l);
	void doEvent(Loop* l, Event* e);

  private:
	long divide(LayerContext* con, Layer* layer, int multiples, long startFrame,
				int minFrames);

	int mMultiple;
};

PUBLIC Function* Divide = new DivideFunction(0);
PUBLIC Function* Divide3 = new DivideFunction(3);
PUBLIC Function* Divide4 = new DivideFunction(4);

PUBLIC DivideFunction::DivideFunction(int n)
{
	eventType = DivideEvent;
	cancelReturn = true;
	mayCancelMute = true;
	instant = true;
	mMultiple = n;

	// could do SoundCopy then instant multiply!!
	//switchStack = true;
	//switchStackMutex = true;

    if (n == 0) {
        // divide by two unless there is a binding arg
		setName("Divide");
        alias1 = "Divide2";
		setKey(MSG_FUNC_DIVIDE);
	}
	else if (n == 3) {
		setName("Divide3");
		setKey(MSG_FUNC_DIVIDE_3);
	}
	else if (n == 4) {
		setName("Divide4");
		setKey(MSG_FUNC_DIVIDE_4);
	}
}

Event* DivideFunction::scheduleEvent(Action* action, Loop* l)
{
	Event* event = NULL;

	event = Function::scheduleEvent(action, l);
	if (event != NULL) {
		// NOTE: Not scheduling a play jump here, though if we are in mute
		// and Divide is a mute cancel function, we technically
		// should so we can cancel mute in advance.  As it is we'll have
		// a little latency loss, but it isn't worth messing with.
	}

	return event;
}

void DivideFunction::doEvent(Loop* loop, Event* event)
{
	int multiple = mMultiple;

    // default to 2
    if (multiple == 0) multiple = 2;

	// Always accept an argument, for the numbered multiples, this is
	// another level of multiplication.
    Action* action = event->getAction();
    if (action != NULL) {
        int ival = action->arg.getInt();
        if (ival > 0) {
            if (mMultiple == 0)
              multiple = ival;
            else 
              multiple = mMultiple * ival;
        }
        if (multiple > MAX_MULTIPLE)
          multiple = 0;
    }

	if (multiple > 1) {

        // shift immediately so we have only one cycle to deal with
        loop->shift(false);

		// Current calculations do not support a loop that is less
		// than either of the latency values.  Pass in a minimum.
		InputStream* instream = loop->getInputStream();
		OutputStream* outstream = loop->getOutputStream();

		int min = (instream->latency > outstream->latency) ? 
			instream->latency : outstream->latency;

		Layer* record = loop->getRecordLayer();
		long frame = loop->getFrame();
		long newFrame = divide(instream, record, multiple, frame, min);

		loop->setFrame(newFrame);
		loop->recalculatePlayFrame();

		Synchronizer* sync = loop->getSynchronizer();
		sync->loopResize(loop, false);

		// and again so we can undo right away
		// !! think more here can have unnecessary layers?
        loop->shift(true);

		loop->checkMuteCancel(event);

		// do we always cancel the previous mode?
		loop->resumePlay();

		record = loop->getRecordLayer();
		Trace(loop, 2, "Loop: Divide by %ld new cycles %ld\n",
			  (long)multiple, (long)record->getCycles());

		// record and play frames do not change
		loop->validate(event);
    }
}

/**
 * Trim off a multiple of the loop, return the logical location
 * of the startFrame after the trim.
 *
 * The loop is first divided into sections according to the "multiples"
 * number.  Then the section we are currently in is preserved and
 * the rest are lopped off.  We try to retain the cycle size if we can,
 * if not the layer is restructured to have one cycle.
 *
 * After the division, there is the possibility for roundoff if the
 * layer length isn't an even multiple of the divisor.  Typically only
 * a few frames are lost, example:
 *
 *   frames=10000, divisor=3, segFrames=3333, inverted=9999, loss=1
 *
 * If the divisor is near the frame count, the possible roundoff error
 * is greater:
 *
 *   frames=10, divisor=6, segFrames=1, inverted=6, loss=4
 * 
 * After the trim, the current frame may fall into this "lost area".
 * If that happens we must shift the segment forward to make sure
 * that it covers the current frame.
 */
long DivideFunction::divide(LayerContext* con, Layer* layer, 
							int multiples, long startFrame,
							int minFrames)
{
	Segment* segments = layer->getSegments();
	if (segments == NULL) {
		Trace(layer, 1, "DivideFunction: no backing layer!\n");
	}
	else if (segments->getNext() != NULL) {
		Trace(layer, 1, "DivideFunction: more than one segment!\n");
	}
	else if (multiples < 1) {
		// this shouldn't happen
		Trace(layer, 1, "DivideFunction: invalid multiple %ld\n", (long)multiples);
	}
	else if (multiples == 1) {
		// this is legal but effectively a noop
		Trace(layer, 2, "DivideFunction: ignoring divide with multiple 1\n");
	}
	else {
		long frames = segments->getFrames();
		if (frames <= multiples) {
			// the loop is very short or the divisor is very large,
			// this shouldn't happen in practice, treat it as a noop 
			Trace(layer, 1, "DivideFunction: divisor %ld larger than layer size %ld\n",
				  (long)multiples, frames);
		}
		else if (startFrame >= frames) {
			Trace(layer, 1, "DivideFunction: invalid startFrame %ld within %ld\n",
				  startFrame, frames);
		}
		else {
			// determine the divided segment size, this may round down 
			long segFrames = frames / multiples;

			if (segFrames < minFrames) {
				// Loop calculations do not allow a loop to go below
				// the maximum latency, this would be an odd case
				// but possible of you were going wild with divide.
				Trace(layer, 1, "DivideFunction: ignoring divide, resulting loop too small %ld\n",
					  segFrames);
			}
			else {

				// locate the base offset for the segment
				int segNumber = startFrame / segFrames;
				long segOffset = segNumber * segFrames;
				long segMax = segOffset + segFrames;
				
				if (segMax > frames) {
					// we're in the "lost zone" at the end
					// work backward from the current frame
					segOffset = (startFrame - segFrames) + 1;
					if (segOffset < 0) {
						Trace(layer, 1, "DivideFunction: roundoff calculation error!\n");
						segOffset = 0;
					}
				}
				else if (segMax <= startFrame) {
					// pretty sure this can't happen
					Trace(layer, 1, "DivideFunction: your math sucks!\n");
					segOffset = 0;
				}

				Trace(layer, 2, "DivideFunction: segment offset %ld size %ld\n",
					  segOffset, segFrames);

				// calculate the new cycle count
				long curCycles = layer->getCycles();
				long newCycles = curCycles / multiples;
				long invCycles = newCycles * multiples;

				if (invCycles != curCycles) {
					// did not divide evenly
					Trace(layer, 2, "DivideFunction: restructing layer from %ld cycles to 1\n",
						  curCycles);
					newCycles = 1;
				}
				
				// this does the heavy lifting, shared with unrounded
				// multiply and remultiply
				layer->splice(con, segOffset, segFrames, newCycles);
				
				startFrame -= segOffset;
				if (startFrame < 0 || startFrame >= segFrames) {
					// again this can't happen based on previous
					// calculations but be very careful
					Trace(layer, 1, "DivideFunction: your math sucks again!\n");
					startFrame = 0;
				}
			}
		}
	}

	return startFrame;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
