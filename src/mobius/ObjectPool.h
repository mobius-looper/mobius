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
 */

#ifndef OBJECT_POOL_H
#define OBJECT_POOL_H

/****************************************************************************
 *                                                                          *
 *                               POOLED OBJECT                              *
 *                                                                          *
 ****************************************************************************/

/**
 * All classes that may be pooled must extend this interface.
 */
class PooledObject {

  public:

    PooledObject();
    virtual ~PooledObject();

    void setPool(class ObjectPool* p);
    class ObjectPool* getPool();

    void setPoolChain(PooledObject* p);
    PooledObject* getPoolChain();

    void setPooled(bool b);
    bool isPooled();

    void free();

  private:

    /**
     * The pool this came from.
     */
    class ObjectPool* mPool;

    /**
     * Chain pointer for the free list.
     */
    PooledObject* mPoolChain;

    /**
     * True if the object is in the pool.  Can't use mPoolChain because
     * it will be null for the last object in the list.
     */
    bool mPooled;

};

/****************************************************************************
 *                                                                          *
 *                               POOLED BUFFER                              *
 *                                                                          *
 ****************************************************************************/

/**
 * Simple extension to PooledObject for representing unstructured
 * memory blocks, such as arrays of float sample data.
 *
 */
class PooledBuffer : public PooledObject {

  public:

    PooledBuffer();
    virtual ~PooledBuffer();

    static PooledBuffer* getPooledBuffer(unsigned char* buffer);

    virtual long getByteSize() = 0;

    void alloc();

    unsigned char* getBuffer();

  private:

    unsigned char* mBuffer;

};

/****************************************************************************
 *                                                                          *
 *                                OBJECT POOL                               *
 *                                                                          *
 ****************************************************************************/

#define OBJECT_POOL_DEFAULT_RING_SIZE 64

/**
 * A pool for one type of object.  This must be subclassed to add
 * implementations for the newObject and prepareObject abstract methods.
 */
class ObjectPool {

  public:

	ObjectPool();
    virtual ~ObjectPool();

    // overloaded by the subclass
    
    virtual PooledObject* newObject() = 0;
    virtual void prepareObject(PooledObject* o) = 0;

    // called by ObjectPoolManager

    const char* getName();
    ObjectPool* getNext();
    void setNext(ObjectPool* p);
    void setThread(class Thread* t);
	void dump();

    // called by the maintenance thread
    
    void maintain();

    // called by the interrupt handler
    
    PooledObject* alloc();
    void free(PooledObject* o);

  protected:

    void initObjectPool(const char* name);
	void prepare();

    PooledObject* allocNew();
	void deleteObject(PooledObject* obj);
    void requestMaintenance();

    ObjectPool* mNext;          // ObjectPoolManager chain
    Thread* mThread;
    char* mName;

    PooledObject* mAllocList;
    PooledObject** mAllocRing;
    int mAllocHead;
    int mAllocTail;
    int mAllocSize;
    int mAllocWarning;

    PooledObject* mFreeList;
    PooledObject** mFreeRing;
    int mFreeHead;
    int mFreeTail;
    int mFreeSize;
    bool mUseFreeRing;

};

/****************************************************************************
 *                                                                          *
 *                                POOL MANAGER                              *
 *                                                                          *
 ****************************************************************************/

class ObjectPoolManager {

  public:

    static ObjectPoolManager* instance();
    static void exit(bool dump);
	static void sdump();

    ObjectPoolManager();
    ~ObjectPoolManager();

	void setThread(Thread* t);
    void startThread();

    void add(ObjectPool* pool);
    ObjectPool* get(const char* name);

    void maintain();
    void dump();

  private:

    static ObjectPoolManager* Singleton;

    ObjectPool* mPools;
    class Thread* mThread;
	bool mExternalThread;

};

/****************************************************************************
 *                                                                          *
 *                             SAMPLE BUFFER POOL                           *
 *                                                                          *
 ****************************************************************************/

/**
 * Concrete implementation of a frequently used buffer type.
 */
class SampleBuffer : public PooledBuffer {

  public:

    SampleBuffer(long samples);
    ~SampleBuffer();

    long getByteSize();

    float* getSamples();

  private:

    long mSamples;

};

/**
 * A pool implementation for commonly used sample buffers of a fixed size.
 */
class SampleBufferPool : public ObjectPool {

  public:

	SampleBufferPool(long samples);
    ~SampleBufferPool();
    
    // ObjectPool implementations
    PooledObject* newObject();
    void prepareObject(PooledObject* o);

    float* allocSamples();
    void freeSamples(float* b);

  protected:

    /**
     * The number of samples in the buffers returned by this pool.
     */
    long mSamples;

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
