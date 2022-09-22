/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * ObjectPool extensions for Mobius objects.
 * This has never been used.
 *
 */

#include <stdio.h>
#include <string.h>

#include "Util.h"
#include "ObjectPool.h"

#include "Event.h"
#include "Mobius.h"

/****************************************************************************
 *                                                                          *
 *                                   EVENTS                                 *
 *                                                                          *
 ****************************************************************************/

class EventObjectPool : public ObjectPool {

  public:

    EventObjectPool();

    PooledObject* newObject();
    void prepareObject(PooledObject* obj);

};

PUBLIC EventObjectPool::EventObjectPool()
{
    initObjectPool("Event");
    // defaults are fine, we don't use the free ring
	prepare();
}

PUBLIC PooledObject* EventObjectPool::newObject()
{
    return NULL;
}

PUBLIC void EventObjectPool::prepareObject(PooledObject* obj)
{
    //((Event*)obj)->init();
}

/****************************************************************************
 *                                                                          *
 *   							INITIALIZATION                              *
 *                                                                          *
 ****************************************************************************/

PUBLIC void Mobius::initObjectPools()
{
	// !! Tried to use a singleton, but if we leave the pool thread
	// running after the VST has closed this causes hosts to crash.
	// Not sure why, it may be unloading the DLL without killing the thread?
	// It is safer to have Mobius manage its own private object pool.  
	if (mPools == NULL ) {
		Trace(this, 2, "Creating object pools\n");
		mPools = new ObjectPoolManager();
		mPools->add(new EventObjectPool());
	}
}

PUBLIC void Mobius::flushObjectPools()
{
	if (mPools != NULL) {
		Trace(this, 2, "Flushing object pools\n");
        // we've never used this and tracing looks confusing
		//mPools->dump();
		delete mPools;
		mPools = NULL;
	}
}

PUBLIC void Mobius::dumpObjectPools()
{
	if (mPools != NULL) {
		mPools->dump();
	}
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
