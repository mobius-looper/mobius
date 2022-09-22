/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * A set of classes that implement an object pooling system with
 * coordination between an appication thread and an device interrupt handler.
 * 
 * NOTE: This is general, consider moving to util.
 *
 * The object pool will be accessed from two contexts:
 *
 *    Maintenance Thread - an application thread that runs periodically to 
 *      perform pool maintenance
 *
 *    Interrupt - an device interrupt that runs continually 
 *
 * The interrupt is expected to be able to retrieve and return 
 * objects from the pool instantly, without a critical section
 * crossing where possible.  The pool thread is expected to keep
 * the pool full of objects so the interrupt handler will never starve.
 *
 * An object pool maintains two linked lists and two ring buffers.
 * 
 *   Allocation Ring
 *     A ring buffer of pooled objects available to the interrupt.
 *     The interrupt handler will remove objects from the tail of the ring,
 *     the pool thread will add objects to the head of the ring.
 *
 *   Allocation List
 *     A list of pooled objects that the pool thread may add to the
 *     allocation ring.  The allocation ring is of fixed size and may not
 *     be large enough to hold all of the pooled objects that have ever
 *     been allocated.  When an object is freed and the ring buffer is full,
 *     it "overflows" to the allocation list.  When the pool thread
 *     needs to add something to the allocation ring, it first uses
 *     objects from the allocation list, then allocates new objects.
 *
 *   Free Ring
 *     A ring buffer of pooled objects available to be reclaimed.
 *     The interrupt handler will add objects to the head of the ring.
 *     The pool thread will remove objects from the tail of the ring.
 *     Objects removed from the tail will be placed back on the
 *     head of the allocation ring if it is not full, otherwise on
 *     the allocation list.
 *
 *   Free List
 *     A list of objects ready to be put on the free ring by the interrupt
 *     handler.  Like the allocation ring, the free ring is not necessarily
 *     large enough to hold all objects that have ever been allocated.  If the
 *     free ring is full when the interrupt handler frees an object, 
 *     it will be added to the free list.  The interrupt handler will
 *     periodically check to see if space is available on the free ring
 *     and move the free list objects to the ring.
 *
 * Only the pool thread is allowed to touch the allocation list, the head
 * of the allocation ring, and the tail of the free ring.
 *
 * Only the interrupt handler is allowed to touch the free list, the head
 * of the free ring, and the tail of the allocation ring.
 * 
 * A pool may decide not to return objects in the free list to the
 * free ring, and instead allocate directly from the free list.  This
 * requires slighthly less overhead than a ring allocation, and a freed
 * object will available immediately without waiting for the maintenance
 * thread to move it from the free ring to the allocation ring.
 * This is useful for most small objects that are allocated and freed
 * frequently.  
 *
 * The free ring is useful for very large objects such as audio buffers.
 * Since the free list is "owned" by the interrupt handler and the 
 * interrupt handler is not allowed to return objects to the heap, 
 * everything on the free list will remain allocated for the lifetime
 * of the application.  If large buffers are allowed to accumulate, 
 * the memory size of the process will steadily increase.  This is not
 * necessarily a bad thing, but may lead to increased paging.  Instead,
 * the interrupt may periodically return buffers to the free ring
 * (such as after a GlobalReset), and the maintenance thread may
 * then choose to return them to the heap rather than accumulating
 * them on the allocation list.
 *
 * Some pooled objects, notably Audio objects, contain a hierarchy
 * of other objects which may also be pooled.  The interrupt handler
 * will usually pool the root Audio object, not each of the audio buffers
 * maintained within the Audio object.  The audio buffers will be returned
 * to the pool when the pool thread reclaims the Audio object.
 *
 * This means that there are two contexts in which an object may be returned
 * to the allocation list: indirectly by the interrupt handler via
 * the free ring, or directly by the pool thread if the object is inside
 * another pooled object.
 * 
 * The Object Pool Manager is a singleton that maintains multiple
 * object pools.  The interrupt handler may retain pointers to the
 * object pools, so once they have been initialized they must not be freed
 * as long as it is possible to have interrupts.
 *
 */

#include <stdio.h>
#include <memory.h>

#include "Util.h"
#include "Thread.h"
#include "Trace.h"
#include "ObjectPool.h"

/****************************************************************************
 *                                                                          *
 *                               POOLED OBJECT                              *
 *                                                                          *
 ****************************************************************************/

PUBLIC PooledObject::PooledObject()
{
    mPool = NULL;
    mPoolChain = NULL;
    mPooled = false;
}

PUBLIC PooledObject::~PooledObject()
{
    // we do NOT free the pool list
    if (mPool != NULL || mPooled)
      Trace(1, "Deleting pooled object!\n");
}

PUBLIC void PooledObject::setPool(class ObjectPool* p)
{
    mPool = p;
}

PUBLIC class ObjectPool* PooledObject::getPool()
{
    return mPool;
}

PUBLIC void PooledObject::setPoolChain(PooledObject* p)
{
    mPoolChain = p;
}

PUBLIC PooledObject* PooledObject::getPoolChain()
{
    return mPoolChain;
}

PUBLIC void PooledObject::setPooled(bool b)
{
    mPooled = b;
}

PUBLIC bool PooledObject::isPooled()
{
    return mPooled;
}

PUBLIC void PooledObject::free()
{
    if (mPool != NULL)
      mPool->free(this);
    else
      delete this;
}

/****************************************************************************
 *                                                                          *
 *                               POOLED BUFFER                              *
 *                                                                          *
 ****************************************************************************/

PUBLIC PooledBuffer::PooledBuffer()
{
    mBuffer = NULL;
}

PUBLIC PooledBuffer::~PooledBuffer()
{
    if (mBuffer != NULL) {
        unsigned char* block = (mBuffer - sizeof(PooledBuffer*));
        delete block;
    }
}

/**
 * Allocate the buffer.  Factored out of the constructor so we have
 * more control over sizing.
 */
PUBLIC void PooledBuffer::alloc()
{
    if (mBuffer == NULL) {
        long bytes = getByteSize();
        if (bytes > 0) {
            bytes += sizeof(PooledBuffer*);
            unsigned char* block = new unsigned char[bytes];
            memset(block, 0, bytes);
            *((PooledBuffer**)block) = this;
            mBuffer = block + sizeof(PooledBuffer*);
        }
    }
}

/**
 * Return the external buffer to the application.
 */
PUBLIC unsigned char* PooledBuffer::getBuffer()
{
    return mBuffer;
}

/**
 * Given the external buffer, extract the pointer to the 
 * owning PooledBuffer.
 */
PUBLIC PooledBuffer* PooledBuffer::getPooledBuffer(unsigned char* buffer)
{
    PooledBuffer* pb = NULL;
    if (buffer != NULL)
      pb = *((PooledBuffer**)(buffer - sizeof(PooledBuffer*)));
    return pb;
}

/****************************************************************************
 *                                                                          *
 *                                OBJECT POOL                               *
 *                                                                          *
 ****************************************************************************/

PUBLIC ObjectPool::ObjectPool()
{
}

/**
 * Called by the subclass constructor.
 */
PRIVATE void ObjectPool::initObjectPool(const char* name)
{
	mNext = NULL;
	mName = CopyString(name);
    mThread = NULL;
	mAllocList = NULL;
	mAllocRing = NULL;
	mAllocHead = 0;
	mAllocTail = 0;
	mAllocSize = 0;
    mAllocWarning = 0;
	mFreeList = NULL;
	mFreeRing = NULL;
	mFreeHead = 0;
	mFreeTail = 0;
	mFreeSize = 0;
    mUseFreeRing = false;
}

/**
 * Called by the subclass constructor after it has specified options.
 */
PRIVATE void ObjectPool::prepare()
{
    if (mName == NULL)
      mName = CopyString("unspecified");

    // rings must be at least 2 elements

	if (mAllocSize < 2)
	  mAllocSize = OBJECT_POOL_DEFAULT_RING_SIZE;

	if (mFreeSize < 2)
	  mFreeSize = OBJECT_POOL_DEFAULT_RING_SIZE;

    // when the number of objects falls below this threshold
    // signal the maintenance thread
    if (mAllocWarning <= 0)
      mAllocWarning = mAllocSize / 2;

	// the way rings work, the head must always be pointing
	// to "empty" space so the rings have to be one larger
	// than the desired number of objects
	mAllocSize++;
	mFreeSize++;

	mAllocRing = new PooledObject*[mAllocSize];
	memset(mAllocRing, 0, sizeof(PooledObject*) * mAllocSize);

	mFreeRing = new PooledObject*[mFreeSize];
	memset(mFreeRing, 0, sizeof(PooledObject*) * mFreeSize);
}

PUBLIC ObjectPool::~ObjectPool()
{
    // free the allocation list
    while (mAllocList != NULL) {
        PooledObject* obj = mAllocList;
        mAllocList = obj->getPoolChain();
        deleteObject(obj);
    }

    // free the allocation ring
    while (mAllocTail != mAllocHead) {
        PooledObject* obj = mAllocRing[mAllocTail];
        if (obj == NULL)
          Trace(1, "Corrupted allocation ring %s\n", mName);
        else
          deleteObject(obj);
        mAllocTail++;
        if (mAllocTail >= mAllocSize)
          mAllocTail = 0;
    }
    delete mAllocRing;

    // free the free ring
    while (mFreeTail != mFreeHead) {
        PooledObject* obj = mFreeRing[mFreeTail];
        if (obj == NULL)
          Trace(1, "Corrupted free ring %s\n", mName);
        else
          deleteObject(obj);
        mFreeTail++;
        if (mFreeTail >= mFreeSize)
          mFreeTail = 0;
    }
    delete mFreeRing;

    // free the free list
    while (mFreeList != NULL) {
        PooledObject* obj = mFreeList;
        mFreeList = obj->getPoolChain();
        deleteObject(obj);
    }
}

PRIVATE void ObjectPool::deleteObject(PooledObject* obj)
{
    // clear these to avoid warnings in the destructor
    obj->setPool(NULL);
    obj->setPooled(false);
    delete obj;
}

PUBLIC void ObjectPool::setNext(ObjectPool* p)
{
    mNext = p;
}

PUBLIC ObjectPool* ObjectPool::getNext()
{
    return mNext;
}

PUBLIC const char* ObjectPool::getName()
{
	return mName;
}

PUBLIC void ObjectPool::setThread(Thread* t)
{
    mThread = t;
}

/**
 * Called internally to allocate a new object from the heap.
 * The newObject method must be overloaded in the subclass.
 */
PRIVATE PooledObject* ObjectPool::allocNew()
{
    PooledObject* obj = newObject();
    obj->setPool(this);
    return obj;
}

/**
 * Called indirectly by the interrupt handler when it wants the
 * maintenance thread to run soon.
 */
PRIVATE void ObjectPool::requestMaintenance()
{
    if (mThread != NULL)
      mThread->signal();
}

/**
 * Called by the interrupt handler to allocate an object.
 */
PUBLIC PooledObject* ObjectPool::alloc()
{
	PooledObject* obj = NULL;

    if (mFreeList != NULL) {
        // first use the free list if we have one
        obj = mFreeList;
        mFreeList = obj->getPoolChain();
    }
    else if (mAllocTail == mAllocHead) {
        // the ring has been consumed, the maintenance thread 
        // must not be running
        Trace(1, "Empty allocation ring in pool %s\n", mName);
    }
    else {
        obj = mAllocRing[mAllocTail];
        if (obj == NULL)
          Trace(1, "Corrupted allocation ring in pool %s\n", mName);
        mAllocTail++;
        if (mAllocTail >= mAllocSize)
          mAllocTail = 0;
    }

    if (obj == NULL) {
        // This should never happen but on windows we can call the factory
        // and have a very good chance of success.  On Mac this will crash.
        obj = allocNew();
    }

    obj->setPooled(false);
    obj->setPoolChain(NULL);

    if (mUseFreeRing || mFreeList == NULL) {
        // capture current for threshold detection, it doesn't matter if the
        // maintenance thread advances the head after this
        int available = 0;
        int head = mAllocHead;
        if (head > mAllocTail)
          available = head - mAllocTail;
        else if (head < mAllocTail)
          available = mAllocSize - (mAllocTail - head);

        if (available < mAllocWarning)
          requestMaintenance();
    }

    return obj;
}

/**
 * Called by the interrupt handler to free an object.
 */
PUBLIC void ObjectPool::free(PooledObject* obj)
{
    if (obj->isPooled()) {
        Trace(1, "Attempt to pool object already in pool %s\n", mName);
        // let it leak
    }
    else if (obj->getPool() != this) {
        Trace(1, "Attempt to pool object %s in pool %s\n", 
              obj->getPool()->getName(), mName);
        // let it leak
    }
    else if (!mUseFreeRing) {
        
        // TODO: If the number of objects on the free list exceeds
        // a threshold, return them to the ring so they may be returned
        // to the heap.

        obj->setPoolChain(mFreeList);
        mFreeList = obj;
        obj->setPooled(true);
    }
    else {
        int nextHead = mFreeHead + 1;
        if (nextHead >= mFreeSize)
          nextHead = 0;

        else if (nextHead == mFreeTail) {
            // management thread isn't keeping up, this usually indicates
            // a problem since we don't normally free large numbers of things
            Trace(2, "Free ring overflow, spilling to free list\n");
            obj->setPoolChain(mFreeList);
            mFreeList = obj;
            obj->setPooled(true);
        }
        else {
            mFreeRing[mFreeHead] = obj;
            mFreeHead = nextHead;
            obj->setPooled(true);
        }
    }
}

/**
 * Called by the application thread to perform pending operations.
 */
PUBLIC void ObjectPool::maintain()
{
    // consume the free ring
	int freed = 0;
    while (mFreeTail != mFreeHead) {
        PooledObject* obj = mFreeRing[mFreeTail];
        if (obj == NULL)
          Trace(1, "Corrupted free ring %s\n", mName);
        else {
            obj->setPoolChain(mAllocList);
            mAllocList = obj;
        }
        mFreeTail++;
		freed++;
        if (mFreeTail >= mFreeSize)
          mFreeTail = 0;
    }

	if (freed > 0)
	  Trace(2, "ObjectPool: consumed %ld objects from the free ring\n", (long)freed);

    // TODO: If the number of objects on the free list exceeds a threshold
    // return them to the heap

    // fill the allocation ring

    int nextHead = mAllocHead + 1;
    if (nextHead >= mAllocSize) nextHead = 0;
	int added = 0;

    while (nextHead != mAllocTail) {
        PooledObject* obj;
        if (mAllocList == NULL)
          obj = allocNew();
        else {
            obj = mAllocList;
            mAllocList = obj->getPoolChain();
            obj->setPoolChain(NULL);
        }
        mAllocRing[mAllocHead] = obj;
        mAllocHead = nextHead;
        nextHead = mAllocHead + 1;
		added++;
        if (nextHead >= mAllocSize) nextHead = 0;
    }

	if (added > 0)
	  Trace(2, "ObjectPool: added %ld objects to the allocation ring\n", 
			(long)added);
}

PUBLIC void ObjectPool::dump()
{
	PooledObject* o;

	printf("%s\n", mName);
	
	int allocListCount = 0;
	int allocRingCount = 0;
	int freeListCount = 0;
	int freeRingCount = 0;

	for (o = mAllocList ; o != NULL ; o = o->getPoolChain())
	  allocListCount++;

	for (o = mFreeList ; o != NULL ; o = o->getPoolChain())
	  freeListCount++;

	if (mAllocHead > mAllocTail)
	  allocRingCount = mAllocHead - mAllocTail;
	else if (mAllocHead < mAllocTail)
	  allocRingCount = mAllocSize - (mAllocTail - mAllocHead);

	if (mFreeHead > mFreeTail)
	  freeRingCount = mFreeHead - mFreeTail;
	else if (mFreeHead < mFreeTail)
	  allocRingCount = mFreeSize - (mFreeTail - mFreeHead);

	// remember that the internal sizes are 1+ the requested 
	// size because the head must always point to "free" space
	char msg[1024];

	// formerly used Trace(1 here, why?
	sprintf(msg, "  %d objects on the allocation list, allocation ring has %d of %d\n", 
			allocListCount, allocRingCount, mAllocSize - 1);
	printf("%s", msg);

	sprintf(msg, "  %d objects on the free list, free ring has %d of %d\n", 
			freeListCount, freeRingCount, mFreeSize - 1);
	printf("%s", msg);
}

/****************************************************************************
 *                                                                          *
 *                                POOL THREAD                               *
 *                                                                          *
 ****************************************************************************/

/**
 * Simple thread that waits for a signal from one of the object pools,
 * then performs pool maintenance.
 */
class PoolThread : public Thread {

  public:

    PoolThread(ObjectPoolManager* pm);
	~PoolThread();

    void processEvent();
    void eventTimeout();
    
  private:

    class ObjectPoolManager* mPools;

};

PUBLIC PoolThread::PoolThread(ObjectPoolManager* pm)
{
    mPools = pm;
	setName("PoolThread");
}

PUBLIC PoolThread::~PoolThread()
{
}

/**
 * This is called by Thread during the periodic wait timeout.
 * By default this will happen at 1 second intervals.
 */
void PoolThread::eventTimeout()
{
    // take the opportunity to do proactive maintenance
    mPools->maintain();
}

/**
 * Called when one of the ObjectPools signals the thread.
 */
void PoolThread::processEvent()
{
    // take the opportunity to do proactive maintenance
    mPools->maintain();
}

/****************************************************************************
 *                                                                          *
 *                                POOL MANAGER                              *
 *                                                                          *
 ****************************************************************************/

ObjectPoolManager* ObjectPoolManager::Singleton = NULL;

ObjectPoolManager::ObjectPoolManager()
{
    mPools = NULL;
	mThread = NULL;
	mExternalThread = false;
}

/**
 * Call this if you want the pool to be updated by a thread managed
 * by the application.
 */
void ObjectPoolManager::setThread(Thread* t)
{
	if (mThread != NULL) {
		// !!could be smarter and shut down properly...
		Trace(1, "ObjectPoolManager: Replacing thread!\n");
	}
	mThread = t;
	mExternalThread = (mThread != NULL);
}

/**
 * Only call this if you want the pool to manage its own update thread.
 */
void ObjectPoolManager::startThread()
{
	if (mThread == NULL || mExternalThread) {
		mExternalThread = false;
		mThread = new PoolThread(this);
		mThread->start();
	}
}

ObjectPoolManager::~ObjectPoolManager()
{
	if (mThread != NULL && !mExternalThread) {
		if (mThread->stopAndWait()) {
			ObjectPool* next = NULL;
			for (ObjectPool* p = mPools ; p != NULL ; p = next) {
				next = p->getNext();
				delete p;
			}

			delete mThread;
		}
		else {
			// thread not responding, or really busy
			// may crash if we delete the pools now, better to leak
			Trace(1, "Unable to halt pool thread!\n");
		}
	}
	mThread = NULL;
	mExternalThread = false;
}

PUBLIC ObjectPoolManager* ObjectPoolManager::instance()
{
    if (Singleton == NULL) {
		Trace(2, "ObjectPoolManager: creating global pool!\n");
		Singleton = new ObjectPoolManager();
	}
    return Singleton;
}

PUBLIC void ObjectPoolManager::exit(bool dumpit)
{
	if (Singleton != NULL) {
		Trace(2, "ObjectPoolManager: deleting global pool!\n");
		if (dumpit)
		  Singleton->dump();
		delete Singleton;
		Singleton = NULL;
	}
}

PUBLIC void ObjectPoolManager::dump()
{
	printf("*** Object Pools ***\n");
    for (ObjectPool* p = mPools ; p != NULL ; p = p->getNext())
	  p->dump();
}

PUBLIC void ObjectPoolManager::sdump()
{
	if (Singleton != NULL)
	  Singleton->dump();
}

PUBLIC void ObjectPoolManager::add(ObjectPool* p)
{
    p->setNext(mPools);
    p->setThread(mThread);
    mPools = p;
}

PUBLIC ObjectPool* ObjectPoolManager::get(const char* name)
{
    ObjectPool* found = NULL;
    for (ObjectPool* p = mPools ; p != NULL ; p = p->getNext()) {
        if (p->getName() != NULL && !strcmp(p->getName(), name)) {
            found = p;
            break;
        }
    }
    return found;
}

PUBLIC void ObjectPoolManager::maintain()
{
    // TODO: if we have a lot of these, could add a flag that marks
    // only those that requested maintenance

    for (ObjectPool* p = mPools ; p != NULL ; p = p->getNext())
      p->maintain();
}

/****************************************************************************
 *                                                                          *
 *                                SAMPLE POOL                               *
 *                                                                          *
 ****************************************************************************/
/*
 * Implementation for a buffer of floating point numbers, something
 * extremely common in audio applications.
 */

PUBLIC SampleBuffer::SampleBuffer(long samples)
{
    mSamples = samples;
    alloc();
}

PUBLIC SampleBuffer::~SampleBuffer()
{
}

PUBLIC long SampleBuffer::getByteSize()
{
    return mSamples * sizeof(float);
}

PUBLIC float* SampleBuffer::getSamples()
{
    return (float*)getBuffer();
}

//
// Pool
//

PUBLIC SampleBufferPool::SampleBufferPool(long samples)
{
    mSamples = samples;
}

PUBLIC SampleBufferPool::~SampleBufferPool()
{
}

/**
 * Required overload from ObjectPool.
 */
PUBLIC PooledObject* SampleBufferPool::newObject()
{
    return new SampleBuffer(mSamples);
}

/**
 * Required overload from ObjectPool.
 */
PUBLIC void SampleBufferPool::prepareObject(PooledObject* o)
{
    SampleBuffer* sb = (SampleBuffer*)o;
    float* samples = sb->getSamples();
    memset(samples, 0, sizeof(float) * mSamples);
}

PUBLIC float* SampleBufferPool::allocSamples()
{
    // machinery inherited from ObjectPool, downcast the result
    SampleBuffer* sb = (SampleBuffer*)alloc();
    return sb->getSamples();
}

PUBLIC void SampleBufferPool::freeSamples(float* buffer)
{
    // extract the PooledBuffer pointer from the block prefix
    PooledBuffer* pb = PooledBuffer::getPooledBuffer((unsigned char*)buffer);
    free(pb);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
