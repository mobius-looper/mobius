/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Erase the contents of a loop but leave the timing intact.
 *
 */

#include <stdio.h>
#include <memory.h>
#include <string.h>

#include "Action.h"
#include "Event.h"
#include "Function.h"
#include "Layer.h"
#include "Loop.h"
#include "Messages.h"
#include "Mobius.h"
#include "Mode.h"
#include "Stream.h"

//////////////////////////////////////////////////////////////////////
//
// ClearEvent
//
//////////////////////////////////////////////////////////////////////

class ClearEventType : public EventType {
  public:
	ClearEventType();
};

PUBLIC ClearEventType::ClearEventType()
{
	name = "Clear";
}

PUBLIC EventType* ClearEvent = new ClearEventType();

//////////////////////////////////////////////////////////////////////
//
// ClearFunction
//
//////////////////////////////////////////////////////////////////////

class ClearFunction : public Function {
  public:
	ClearFunction();
	void doEvent(Loop* l, Event* e);

  private:
	bool start;
};

PUBLIC Function* Clear = new ClearFunction();

PUBLIC ClearFunction::ClearFunction() :
    Function("Clear", MSG_FUNC_CLEAR)
{
	eventType = ClearEvent;
	cancelReturn = true;
	mayCancelMute = true;
	instant = true;
}

/**
 * Several ways we could do this.  Could avoid the extra
 * shift by entering a minor mute mode but that seems
 * unreliable?
 */
PUBLIC void ClearFunction::doEvent(Loop* loop, Event* event)
{
	OutputStream* output = loop->getOutputStream();

	// have to capture a fade tail now
	output->captureTail();

	// do a shift in case we recorded something
	loop->shift(true);

	// and another we can erase
	// sigh, shift should have an argument to force a layer? so we
	// don't have to do this out here!

	Layer* record = loop->getRecordLayer();
	record->zero();

    LayerPool* lp = loop->getMobius()->getLayerPool();
	Layer* newRecord = lp->newLayer(loop);

	newRecord->setPrev(record);
	newRecord->copy(record);

	loop->setPlayLayer(record);
	loop->setRecordLayer(newRecord);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

