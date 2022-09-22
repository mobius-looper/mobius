/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Wrapper around host-specific thread implementations, 
 *  loosely based on Java Threads.
 *
 */

#ifndef THREAD_H
#define THREAD_H

#include <stdarg.h>

#ifdef _WIN32
// for CRITICAL_SECTION and HANDLE
#include <windows.h>
#else
// for pthread_mutex_t
#include <pthread.h>
#endif

#include "port.h"

//////////////////////////////////////////////////////////////////////
//
// Sleep
//
//////////////////////////////////////////////////////////////////////

INTERFACE void SleepSeconds(int seconds);
INTERFACE void SleepMillis(int millis);

//////////////////////////////////////////////////////////////////////
//
// Critical Section
//
//////////////////////////////////////////////////////////////////////

class CriticalSection {

  public:
	
	INTERFACE CriticalSection();
	INTERFACE CriticalSection(const char* name);
	INTERFACE ~CriticalSection();

	// windows specific
	INTERFACE void setSpin(int spin);

	INTERFACE void enter();
	INTERFACE void enter(const char* reason);
	INTERFACE void leave();
	INTERFACE void leave(const char* reason);

  private:

	void init();
	void trace(const char* direction, const char* reason);
	void checkStatus(int status, const char* function);

	char* mName;
	int mCount;

#ifdef _WIN32
	CRITICAL_SECTION cs;
#else
	pthread_mutex_t mMutex;
#endif

};

//////////////////////////////////////////////////////////////////////
//
// Thread
//
//////////////////////////////////////////////////////////////////////


class Thread {

  public:
    
    Thread();
    Thread(const char* name);
    virtual ~Thread();

    void setName(const char* name);
    const char* getName();

    void setTimeout(int p);
    int getTimeout();

    void setPriority(int p);
    int getPriority();

    /**
     * Start the thread.
     * After creating the native thread, the run() method is called.
     */
    void start();

    /**
     * The entry point of the thread, called by the system after
     * a native thread is created by the start() method.
	 * The default implementation provides a periodic timer thread, override
	 * if you want to wait on signals or something.
     */
    virtual void run();

    /**
     * Request that the thread stop.
     * The thread may ignore this.
     */
    virtual void stop();

    /**
     * Returns true if the thread is running.
     */
    bool isRunning();

	/**
	 * Ask the thread to stop and wait till it does.
	 * This will time out after two seconds if the thread is
	 * unresponsive, and return false.
	 */
	bool stopAndWait();

    /**
     * Returns true if the thread has been asked to stop.
     */
    bool isStopping();

    /**
     * Only for use by our thread entry point function.
     */
    void runOuter();

    static void sleep(int millis);

	void signal();

	virtual void processEvent();
	virtual void eventTimeout();
	virtual void threadEnding();

	void enterCriticalSection();
	void leaveCriticalSection();

  protected:

    void init();
	void configurePriority(bool inside);

    /**
     * Optional thread name for debugging.
     */
    char* mName;

	/**
	 * Wait timeout interval in milliseconds.
	 * Default is 1000.	
	 */
	int mTimeout;

    /**
     * Desired priority (currently ignored)
     */
    int mPriority;

#ifdef _WIN32
    /**
     * Handle to the native thread.
     */
    unsigned int mHandle;

	/**
	 * the synchronization event.
	 */
	HANDLE mEvent;
#else
	pthread_t mThread;
	pthread_mutex_t mConditionMutex;
	pthread_cond_t mCondition;
#endif

    /**
     * When set will cause the thread to stop (assuming the
     * run loop is still responsive).
     */
    bool mStop;

	/**
	 * General purpose csect, may be used by the subclasses for synchronization.
	 * Note that this is NOT the condition mutex for pthread waits.
	 */	
	class CriticalSection* mCsect;

	bool mTrace;
};

#endif
