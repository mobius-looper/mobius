/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Instant multiply.
 * 
 */

#include <stdio.h>
#include <memory.h>
#include <string.h>

#include "Util.h"

#include "Action.h"
#include "Event.h"
#include "Function.h"
#include "Stream.h"
#include "Layer.h"
#include "Loop.h"
#include "Messages.h"
#include "Segment.h"
#include "Synchronizer.h"

//////////////////////////////////////////////////////////////////////
//
// InstantMultiplyEvent
//
//////////////////////////////////////////////////////////////////////

class InstantMultiplyEventType : public EventType {
  public:
	InstantMultiplyEventType();
};

PUBLIC InstantMultiplyEventType::InstantMultiplyEventType()
{
	name = "InstantMultiply";
}

PUBLIC EventType* InstantMultiplyEvent = new InstantMultiplyEventType();

//////////////////////////////////////////////////////////////////////
//
// InstantMultiplyFunction
//
//////////////////////////////////////////////////////////////////////

/**
 * Prevent runaway multiples in scripts.
 */
#define MAX_MULTIPLE 512

class InstantMultiplyFunction : public Function {
  public:
	InstantMultiplyFunction(int n);
	Event* scheduleEvent(Action* action, Loop* l);
	void doEvent(Loop* loop, Event* event);

  private:
	void multiply(LayerContext* con, Layer* layer, int multiples);

	int mMultiple;
};

PUBLIC Function* InstantMultiply = new InstantMultiplyFunction(0);
PUBLIC Function* InstantMultiply3 = new InstantMultiplyFunction(3);
PUBLIC Function* InstantMultiply4 = new InstantMultiplyFunction(4);

PUBLIC InstantMultiplyFunction::InstantMultiplyFunction(int n)
{
	eventType = InstantMultiplyEvent;
	cancelReturn = true;
	instant = true;
	mMultiple = n;

	// could do SoundCopy then instant multiply!!
	//switchStack = true;
	//switchStackMutex = true;

	if (n == 0) {
		setName("InstantMultiply");
        alias1 = "InstantMultiply2";
		setKey(MSG_FUNC_INSTANT_MULTIPLY);
	}
	else if (n == 3) {
		setName("InstantMultiply3");
		setKey(MSG_FUNC_INSTANT_MULTIPLY_3);
	}
	else if (n == 4) {
		setName("InstantMultiply4");
		setKey(MSG_FUNC_INSTANT_MULTIPLY_4);
	}
}

Event* InstantMultiplyFunction::scheduleEvent(Action* action, Loop* l)
{
	Event* event = NULL;

	event = Function::scheduleEvent(action, l);
	if (event != NULL) {
		// NOTE: Not scheduling a play jump here, though if we are in mute
		// and InstantMultiply is a mute cancel function, we technically
		// should so we can cancel mute in advance.  As it is we'll have
		// a little latency loss, but it isn't worth messing with.
	}

	return event;
}

void InstantMultiplyFunction::doEvent(Loop* loop, Event* event)
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

		InputStream* instream = loop->getInputStream();
		Layer* record = loop->getRecordLayer();
		multiply(instream, record, multiple);
			
		// if we're near the end of the loop, may have already
		// wrapped the play frame, have to unrap it
		long playFrame = loop->getPlayFrame();
		long frame = loop->getFrame();
		if (playFrame < frame) {
			long save = playFrame;

			loop->recalculatePlayFrame();

			playFrame = loop->getPlayFrame();
			Trace(loop, 2, "Loop: Unwrapping play frame from %ld to %ld\n",
				  save, playFrame);

			// we don't want to cause a fade so pretend we were
			// here all the time
			OutputStream* outstream = loop->getOutputStream();
			outstream->setLastFrame(playFrame);
		}

        // let synchronizer know if were the out sync master
		Synchronizer* sync = loop->getSynchronizer();
		sync->loopResize(loop, false);

		// and again so we can undo right away
		// !! think more here can have unnecessary layers?
        loop->shift(true);

		loop->checkMuteCancel(event);

		// do we always cancel the previous mode?
		loop->resumePlay();

		record = loop->getRecordLayer();
		Trace(loop, 2, "Loop: Instant multiply by %ld new cycles %ld\n",
			  (long)multiple, (long)record->getCycles());

		// record and play frames do not change
		loop->validate(event);
    }
}

/**
 * Perform an immediate multiplication of the layer.
 * It is assumed that we have just shifted, there will be a single
 * segment, and we can simply replicate it.
 */
void InstantMultiplyFunction::multiply(LayerContext* con, Layer* layer, 
									   int multiples)
{
	Segment* segments = layer->getSegments();
	if (segments == NULL) {
		Trace(layer, 1, "InstantMultiply: no backing layer!\n");
	}
	else if (segments->getNext() != NULL) {
		Trace(layer, 1, "InstantMultiply: more than one segment!\n");
	}
	else {
		long startFrames = segments->getFrames();
		long offset = startFrames;

		long cycles = layer->getCycles();
		cycles *= multiples;
		layer->setCycles(cycles);

		// we already have 1 so adjust down
		multiples--;

		for (int i = 0 ; i < multiples ; i++) {
			Segment* seg = new Segment(segments);
			seg->setOffset(offset);
			offset += startFrames;
			layer->addSegment(seg);
		}

		// recalculate cycles and frames
		long newFrames = layer->calcFrames();
		// resize Audios, reverse doesn't matter
		layer->setFrames(NULL, newFrames);
	}

	layer->setStructureChanged(true);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
