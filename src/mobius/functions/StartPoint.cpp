/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Set the loop start point.
 * This is a simple layer segment reorganization.
 *
 */

#include <stdio.h>
#include <string.h>
#include <memory.h>

#include "Action.h"
#include "Event.h"
#include "EventManager.h"
#include "Function.h"
#include "Layer.h"
#include "Loop.h"
#include "Messages.h"
#include "Mobius.h"
#include "Segment.h"
#include "Stream.h"
#include "Synchronizer.h"
#include "Track.h"

//////////////////////////////////////////////////////////////////////
//
// StartPointEvent
//
//////////////////////////////////////////////////////////////////////

class StartPointEventType : public EventType {
  public:
	StartPointEventType();
};

PUBLIC StartPointEventType::StartPointEventType()
{
	name = "StartPoint";
}

PUBLIC EventType* StartPointEvent = new StartPointEventType();

//////////////////////////////////////////////////////////////////////
//
// StartPointFunction
//
//////////////////////////////////////////////////////////////////////

/**
 * How should this handle scheduleSwitchStack?
 * Could do a LoopCopy=Sound, followed by a StartPoint at the current position
 * in the source loop.  Once stacked, this could be changed by other 
 * StartPoint functions.  For SyncStartPoint, it could do LoopCopy=Sound,
 * then simply stack and wait for the global MIDI start point.
 *
 */
class StartPointFunction : public Function {

  public:
	StartPointFunction(bool midiop);
    void invokeLong(Action* action, Loop* l);
	Event* scheduleEvent(Action* action, Loop* l);
	void doEvent(Loop* loop, Event* event);
	void undoEvent(Loop* loop, Event* event);

  private:
	void startPoint(LayerContext* con, Layer* layer, long startFrame);

    bool mMidi;
};

PUBLIC Function* StartPoint = new StartPointFunction(false);
PUBLIC Function* SyncStartPoint = new StartPointFunction(true);

PUBLIC StartPointFunction::StartPointFunction(bool midiop)
{
	eventType = StartPointEvent;
	mayCancelMute = true;
	instant = true;
	cancelReturn = true;
	quantized = true;

	mMidi = midiop;

	if (mMidi) {
		setName("SyncStartPoint");
		setKey(MSG_FUNC_SYNC_START_POINT);
		setHelp("Set the loop start point at next external sync start point");
	}
	else {
		setName("StartPoint");
		setKey(MSG_FUNC_START_POINT);
		setHelp("Set the start point to current position in the loop");
	}
}

PUBLIC Event* StartPointFunction::scheduleEvent(Action* action, Loop* l)
{
	Event* event = NULL;
    EventManager* em = l->getTrack()->getEventManager();

	if (mMidi) {
		// since we aren't a mode, have to catch redundant invocations
		// by checking for an existing event
		event = em->findEvent(StartPointEvent);
	}

    if (event != NULL) {
        // we're ignoring it so don't return it
        event = NULL;
    }
    else {
		// this will come back pending if we're ending multiply/insert
		event = Function::scheduleEvent(action, l);
		if (event != NULL && !event->reschedule) {
			if (mMidi) {
				// we scheduled it normally, but make it pending so we can
				// defer triggering it until the external start point happens
				event->pending = true;
			}
		}
	}

	return event;
}

PUBLIC void StartPointFunction::invokeLong(Action* action, Loop* l)
{
    EventManager* em = l->getTrack()->getEventManager();

    if (!mMidi) {
		// Performs SyncStartPoint
		Event* event = em->findEvent(StartPointEvent);
		if (event != NULL) {
			// we haven't processed the simple StartPoint yet
			event->pending = true;
			event->function = SyncStartPoint;
		}
		else {
			// must have already processed it, make another one
            Mobius* m = l->getMobius();
            Action* a = m->newAction();
            // hmm, may want a new trigger type like TriggerLong?
            // use TriggerEvent since this is indirect
            a->trigger = TriggerEvent;
            a->inInterrupt = true;
            a->down = true;
            a->setFunction(SyncStartPoint);
            a->setResolvedTrack(l->getTrack());
            
            m->doAction(a);
		}
	}
}

/**
 * Undo handler.
 * Nothing to do but need to define in order to avoid a trace warning.
 */
void StartPointFunction::undoEvent(Loop* loop, Event* event)
{
}

/**
 * Event handler.
 * We can get here from either the StartPoint or SyncStartPoint functions.
 *
 * In SyncStartPoint we move the start point to the next external 
 * loop start point.   This will be performed by scheduling a pending
 * StartPoint and activating it when we see the external start point.
 * We could try to calculate where the external start point will be and
 * do an immediate StartPoint, but it will be more accurate if we just
 * wait for it.   Since the audible loop won't change, it doesn't really
 * matter and deferring allows you to undo it.
 *
 * Note that unlike most functions, we do an immediate shift BEFORE
 * modifying the Layer.  See commentary in Layer::startPoint for the reasons.
 */
void StartPointFunction::doEvent(Loop* loop, Event* event)
{
	// shift first, then rearrange layer segments
	loop->shift(false);

	InputStream* input = loop->getInputStream();
	Layer* layer = loop->getRecordLayer();
	startPoint(input, layer, loop->getFrame());

	// synchronizer will adjust our dealign and maybe send MIDI Stop
	Synchronizer* sync = loop->getSynchronizer();
	sync->loopSetStartPoint(loop, event);

	// crap! have to shift again so we start playing from the new
	// frame zero.  I had hoped to avoid the extra layer, but we can't
	// without some way to restructure the start point of an Audio.  This
	// is similar to the original Layer::spawn() approach but at least the
	// Layer is on the layer list and will be saved with the project.
	loop->shift(false);

	loop->setFrame(0);
	loop->recalculatePlayFrame();

	loop->checkMuteCancel(event);
	// always reset the current mode?
	loop->resumePlay();

	// not an audible shift	
	OutputStream* output = loop->getOutputStream();
	output->setLayerShift(true);
}

/**
 * Redefine the layer start point.
 *
 * Originally did this using spawn() to push the current layer content
 * into an invisible layer, then referencing it with two segments.  This
 * works, but since the new layer isn't on the Loop's layer list, it
 * won't be saved in the project.
 *
 * It would be possible to restructure the segment list to handle
 * the backing layer, but there isn't a way to quickly restructure
 * the local Audios.  We could create an AudioWindow object that does something
 * similar to a Segment, or just allow Segment to wrap an Audio rather
 * than a Layer.  But you could not record into the layer, only read from it.
 *
 * The easiest approach is to simply do an immediate shift of the
 * record layer BEFORE applying the start point.  That means that
 * here we can simply split the one segment referencing the previous
 * layer, there will be no local audio.
 * 
 */
void StartPointFunction::startPoint(LayerContext* con, Layer* layer, long startFrame)
{
	Segment* segments = layer->getSegments();

	if (startFrame == 0) {
		// must be quantized, ignore
	}
	else if (segments == NULL) {
		Trace(layer, 1, "Layer: startPoint with no backing layer!\n");
	}
	else if (segments->getNext() != NULL) {
		Trace(layer, 1, "Layer: startPoint with more than one segment!\n");
	}
	else {
		Layer* refLayer = segments->getLayer();

		// sanity checks
		if (segments->isFadeLeft() || 
			segments->isFadeRight() ||
			segments->getLocalCopyLeft() > 0 ||
			segments->getLocalCopyRight() > 0) {
			Trace(layer, 1, "Layer: Unusual segment state for StartPoint\n");
		}

		// could also check for content in the Audio?

		// Subtlety: In reverse do calculations in involving mFrames
		// with an unreflected startFrame, this is because mFrames
		// represents the frame one AFTER the last, in reverse the
		// symetrical frame would be -1.  
		long remainder = layer->getFrames() - startFrame;

		Segment* seg1 = segments;
		Segment* seg2 = new Segment(seg1);
		layer->addSegment(seg2);

		if (con->isReverse()) {
			// Reflect the start frame if in reverse, in effect StartPoint
			// in reverse behaves like an EndPoint funcion.
			seg1->setOffset(0);
			seg1->setStartFrame(remainder);
			seg1->setFrames(startFrame);

			seg2->setOffset(startFrame);
			seg2->setStartFrame(0);
			seg2->setFrames(remainder);
		}
		else {
			seg1->setOffset(0);
			seg1->setStartFrame(startFrame);
			seg1->setFrames(remainder);

			seg2->setOffset(remainder);
			seg2->setStartFrame(0);
			seg2->setFrames(startFrame);
		}

		// should be no fades
		seg1->setFadeLeft(false);
		seg1->setFadeRight(false);
		seg2->setFadeLeft(false);
		seg2->setFadeRight(false);

		// be we now contain deferred fades on both sides
		layer->setContainsDeferredFadeLeft(true);
		layer->setContainsDeferredFadeRight(true);

        // assuming we're dealing with a freshly shifted layer, we don't
        // have to worry about segment edge fades, they will all be off

		layer->setStructureChanged(true);
    }
}

/**
 * Original implementation that used spawn.
 * Keep around for awhile as a spawn example.
 * 
 * In reverse, we reflect the frame and split into two segments, but in 
 * a surprising bit of symetry we don't have to change the order of the
 * segments.  The remainder segment is moved to the front in both cases.
 * In effect reverse start point sets the end point of the layer.
 */
#if 0
void Layer::startPointSpawn(LayerContext* con, long startFrame)
{
	// transfer our contents to an inner layer
	Layer* inner = spawn();

	// TODO: need to cause there to be deferred edge fades 
	// on both sides after this, where is this stored that won't
	// get lost when segment fades are recompiled?

	// reflect the start frame if in reverse
	startFrame = reflectFrame(con, startFrame);

	// second half moves to the front
	Segment* seg = new Segment(inner);
	long remainder = mFrames - startFrame;
	seg->setStartFrame(startFrame);
	seg->setOffset(0);
	seg->setFrames(remainder);
	addSegment(seg);

	// first half is pushed
	seg = new Segment(inner);
	seg->setStartFrame(0);
	seg->setOffset(remainder);
	seg->setFrames(startFrame);
	addSegment(seg);

	// The layer reference count is now 3 (1 on allocation and 2 segs)
	// since it is not directly referenced by Loop, decrease by 1 so
	// it will be freed when the two segments are freed
	inner->decReferences();
}
#endif

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
