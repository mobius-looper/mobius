/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Wrapper around host-specific thread implementations.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#ifdef _WIN32

#include <process.h>

// jsl - having trouble finding this in MSVC 6.0.
// Docs say that sp3 is required, 
// but the include file may not have left in the right place?
// declaring this isn't enough, it fails to link
// extern DWORD WINAPI SetCriticalSectionSpinCount(LPCRITICAL_SECTION cs, DWORD spin);

#else

// osx needs this for sleep() and something else
#include <unistd.h>
#include <sys/types.h>
#include <pthread.h>
#include <errno.h>
#include <time.h>

// mac stuff for real-time threads
extern "C" {
#include <mach/thread_policy.h>
#include <mach/thread_act.h>
#include <mach/mach_init.h>
// to get bus speed
#include <sys/sysctl.h>
}

#endif

#include "util.h"
#include "Thread.h"

#define DEFAULT_TIMEOUT 1000

//////////////////////////////////////////////////////////////////////
//
// SLEEP
// 
//////////////////////////////////////////////////////////////////////

INTERFACE void SleepSeconds(int seconds)
{
	SleepMillis(seconds * 1000);
}


INTERFACE void SleepMillis(int millis)
{
#ifdef _WIN32	
	Sleep(millis);
#else
	usleep(millis * 1000);
#endif
}

//////////////////////////////////////////////////////////////////////
//
// Critical Sections
//
//////////////////////////////////////////////////////////////////////

INTERFACE CriticalSection::CriticalSection()
{
	init();
}

INTERFACE CriticalSection::CriticalSection(const char* name)
{
	init();
	mName = CopyString(name);
}

PRIVATE void CriticalSection::init()
{
	mName = NULL;
	mCount = 0;
#ifdef _WIN32
	InitializeCriticalSection(&cs);
	// hack, always ask for a nominal spin count, 
	// our sections will either tend to be very short or very long
	setSpin(4000);
#else
	// have to ask for a recursive mutex since Mobius has
	// a few code paths that lock the same csect
	int status;
	pthread_mutexattr_t attributes;

	status = pthread_mutexattr_init(&attributes);
	checkStatus(status, "pthread_mutexattr_init");
	
	status = pthread_mutexattr_settype(&attributes, PTHREAD_MUTEX_RECURSIVE);
	checkStatus(status, "pthread_mutexattr_settype");

	status = pthread_mutex_init(&mMutex, &attributes);
	checkStatus(status, "pthread_mutex_init");

	// mutexattr_init in theory may allocate storage that has to be freed
	status = pthread_mutexattr_destroy(&attributes);
	checkStatus(status, "pthread_mutexattr_destroy");
#endif
}

INTERFACE CriticalSection::~CriticalSection()
{
	delete mName;

#ifdef _WIN32
	DeleteCriticalSection(&cs);
#else
	int status = pthread_mutex_destroy(&mMutex);
	checkStatus(status, "pthread_mutex_destroy");
#endif
}

/**
 * Sets the "spin count" for the critical section.
 * This is Windows specific and can be used when the csect is expected
 * to be short.  Setting to a small value (4000 is the example) 
 * will cause processes to spin for a short period of time rather than 
 * blocking which causes an expensive ring transition.
 *
 * !! Net comments suggest that you just not use spinloops in
 * an interrupt handler.
 */
INTERFACE void CriticalSection::setSpin(int spin)
{
	// having trouble finding this...
#if 0
	if (this)
	  SetCriticalSectionSpinCount(&cs, spin);
#endif
}

INTERFACE void CriticalSection::enter()
{
	enter(NULL);
}

INTERFACE void CriticalSection::enter(const char* reason)
{
	trace("enter", reason);
#ifdef _WIN32
	if (this)
	  EnterCriticalSection(&cs);
#else
	int status = pthread_mutex_lock(&mMutex);
	checkStatus(status, "pthread_mutex_lock");
	mCount++;
#endif
}

INTERFACE void CriticalSection::leave()
{
	leave(NULL);
}

INTERFACE void CriticalSection::leave(const char* reason)
{
	trace("leave", reason);
#ifdef _WIN32
	if (this)
	  LeaveCriticalSection(&cs);
#else
	mCount--;
	int status = pthread_mutex_unlock(&mMutex);
	checkStatus(status, "pthread_mutex_unlock");
#endif
}

PRIVATE void CriticalSection::trace(const char* direction, const char* reason)
{
	bool doTrace = false;

	if (doTrace) {
		if (reason == NULL) reason = "";
		if (mName != NULL)
		  printf("Csect %s %d %s %s\n", direction, mCount, mName, reason);
		else
		  printf("Csect %s %d %p %s\n", direction, mCount, this, reason);
		fflush(stdout);
	}
}

PRIVATE void CriticalSection::checkStatus(int status, const char* function)
{
	if (status != 0) {
		printf("ERROR: %s %d!!\n", function, status);
		fflush(stdout);
	}
}

//////////////////////////////////////////////////////////////////////
//
// Threads
//
//////////////////////////////////////////////////////////////////////

#ifdef _WIN32
/**
 * Entry function for the native thread created by _beginthread.
 *
 * This is what you want for _beginthread:
 *   void WINAPIV ThreadFunction(LPVOID arg)
 * 
 * _beginthreadex uses a different calling convention.
 */
unsigned int __stdcall ThreadFunction(void* arg)
{
     Thread* thread = (Thread*) arg;
     if (thread != NULL)
       thread->runOuter();

     // must return a "thread exit code"
     return 0;
}

#else

// necessary to get C linkage rather than C++?
extern "C" void *PThreadFunction(void *);

/**
 * Entry function for the pthread.
 */
void* PThreadFunction(void* arg)
{
     Thread* thread = (Thread*) arg;
     if (thread != NULL)
       thread->runOuter();

	// must we call this or can we just return from the run function?
	// this function never returns, argument is a void* retval which 
	// must not be of local scope
	pthread_exit(NULL);

	// not sure what the return convention is but pthread_exit never
	// returns so I guess it doesn't matter
	return NULL;
}
#endif

PUBLIC Thread::Thread()
{
    init();
}

PUBLIC Thread::Thread(const char* name)
{
    init();
    mName = CopyString(name);
}

PUBLIC void Thread::init()
{
    mName = NULL;
	mTimeout = DEFAULT_TIMEOUT;
	mPriority = 0;
    mStop = false;
	mCsect = new CriticalSection();
	mTrace = false;

#ifdef _WIN32
    mHandle = 0;
	mEvent = NULL;
#else
	mThread = NULL;
#endif
}

PUBLIC Thread::~Thread()
{
	delete mCsect;
	delete mName;
}

/**
 * Return true if the thread is running.
 * Is having a non-zero handle enough?  Would we ever need
 * to keep the handle around after the thread terminates?
 */
PUBLIC bool Thread::isRunning()
{
#ifdef _WIN32	
    return (mHandle != 0);
#else
	return (mThread != NULL);
#endif
}

PUBLIC void Thread::setName(const char* s)
{
    delete mName;
    mName = CopyString(s);
}

PUBLIC const char* Thread::getName()
{
    return mName;
}

PUBLIC void Thread::setPriority(int p)
{
    mPriority = p;
}

PUBLIC int Thread::getPriority()
{
    return mPriority;
}

PUBLIC void Thread::setTimeout(int p)
{
    mTimeout = p;
}

PUBLIC int Thread::getTimeout()
{
    return mTimeout;
}

PUBLIC void Thread::start()
{
#ifdef _WIN32
    if (mHandle == 0) {
		// second arg TRUE for a manual reset
		mEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
		if (mEvent == NULL) {
			printf("ERROR: Thread::start unable to create event!\n");
			fflush(stdout);
		}

        // First is "security attribute", NULL means can't be inherited by children.  
		// Second arg is stack size.
		// Third arg is the entry function
		// Fourth arg is an argument to the entry function
        // Fifth is 0 for running or CREATE_SUSPENDED.
        // Sixth is is a location for the thread identifier.

        unsigned long status = (unsigned long)
            _beginthreadex(NULL, 0, ThreadFunction, this, 0, &mHandle);

        if (status == 0) {
            // errno will be EAGAIN if there were too many threads, 
            // EINVAL if the argument is invalid or the stack size
            // is incorrect
            printf("ERROR: Thread::start unable to launch thread!\n");
            fflush(stdout);
            mHandle = 0;
        }
	}
#else
	if (mThread == NULL) {
		// second arg is a pthread_attr_t*
		// third arg is the entry function
		// fourth arg is the argument to the entry function
		// thread attributes include options to create detached (rather than 
		// joinable threads), scheduling, scope, and stack size

		pthread_t thread;
		int status = pthread_create(&thread, NULL, PThreadFunction, this);
		
		if (status == 0) {
			mThread = thread;
			//configurePriority(false);
		}
		else {
            // status will be EAGAIN if there were too many threads, 
            // EINVAL if the value specified by attr is invalid
			fprintf(stderr, "ERROR: Thread::start unable to launch thread!\n");
			fflush(stderr);
		}
    }
#endif
}

/**
 * Internal method to set the OSX thread scheduling priorities.
 * I started doing this from the "outside" with the thread_t returned
 * by pthread_create, but got errors calling thread_policy_set.  So I factored
 * this out so we could call it from several locations.  If "thread" is NULL
 * then call mac_thread_self like most of the examples do.
 */
PRIVATE void Thread::configurePriority(bool inside)
{
#ifndef _WIN32

	thread_act_t thread;

	// hmm, can't find this anywhere...
	if (inside)
	  thread = mach_thread_self();
	else {
		// the problem that caused the original error was assuming that a pthread
		// handle could be passed to mach thread methods, 268435459 is
		// 0x10000006 which is MACH_SEND_INVALID_DEST (bogus destination port) 
		// from mach/message.h.  You need to call pthread_mach_thread_np
		thread = pthread_mach_thread_np(mThread);
	}

	if (mPriority > 0) {

		// call mach thread methods to ask for "time constraint" scheduling
		kern_return_t err;

		bool useTimeConstraint = true;

		if (!useTimeConstraint) {
			// This was an old example from the mailing list to turn 
			// "timeshare" mode off, it may be enough.  This supposedly
			// causes threads to be "scheduled in a round robin fashion,
			// among threads ofo equal priority".
			thread_extended_policy_data_t extPolicy;
			extPolicy.timeshare = 0;
			err = thread_policy_set(thread,
									THREAD_EXTENDED_POLICY, 
									(thread_policy_t)&extPolicy,
									THREAD_EXTENDED_POLICY_COUNT);
			if (err != KERN_SUCCESS) {
				fprintf(stderr, "ERROR: Thread::start unable to set extended policy %d\n", err);
				fflush(stderr);
			}

			// ask for a precidence of 64, some posts indiciate that 63
			// is actually the most you will get which is equal to but not
			// above the window server
			thread_precedence_policy_data_t prePolicy;
			prePolicy.importance = 64;
			err = thread_policy_set(thread, 
									THREAD_PRECEDENCE_POLICY, 
									(thread_policy_t)&prePolicy,
									THREAD_PRECEDENCE_POLICY_COUNT);
			if (err != KERN_SUCCESS) {
				fprintf(stderr, "ERROR: Thread::start unable to set precedence policy %d\n", err);
				fflush(stderr);
			}

		}
		else {
			// Absolute time units differ according to the bus speed
			// of the computer, you should normally use numbers relative
			// to HZ which is a global variable containing the the
			// "number of ticks per second".  The programming guide
			// uses these examples:
			//
			//   period = HZ / 160
			//   computation = HZ / 3300
			//   constraint = HZ / 2200
			//   preemptible = 1
			//
			// Guide: Say your computer reports 133 million for the value 
			// of HZ.  If you pass the example values your thread 
			// tells the scheduler that it needs approximately 40,000
			// (HZ/3300) out of the next 833,333 (HZ/160) bus cycles.
			// The preemptible value 1 indiciates that those 40,000 bus
			// cycles need not be contiguous.  However the constraint
			// value tells hhe scheduler that there can be no more than
			// 60,000 (HZ/2200) bus cycles between the start of a 
			// computation and the end of a computation.	
			//
			// I found another reference to a "cdaudio" example that used
			// the divisors 120, 1440, 720.
			// 
			// You can also use nanoseconds_to_absolute_time to 
			// calcuulate values.
			//
			// Basically, we have to arrive at values that are low
			// enough to make sure our thread gets called often enough.
			// Since we're only using this for a millisecond timer,
			// the tolerance can probably be much higher, but we'll
			// use the example from the docs which were taken
			// from the Esound deamon (esd).

			// hmm, can't find HZ anywhere but one guy posted this..
			int bus_speed, mib [2] = { CTL_HW, HW_BUS_FREQ };
			size_t len = sizeof( bus_speed);
			int ret = sysctl (mib, 2, &bus_speed, &len, NULL, 0);
			if (ret < 0) {
				fprintf(stderr, "ERROR: Thread::start sysctl query bus speed failed, errno=%d", errno);
				fflush(stderr);
				return;
			}
			int HZ = bus_speed;

			thread_time_constraint_policy_data_t timePolicy;
					
			// period: This is the nominal amount of time between separate
			// processing arrivals, specified in absolute time units.  A
			// value of 0 indicates that there is no inherent periodicity in
			// the computation.
			timePolicy.period = HZ / 160;
					
			// computation: This is the nominal amount of computation
			// time needed during a separate processing arrival, 
			// specified in absolute time units.
			timePolicy.computation = HZ / 3300;

			// constraint: This is the maximum amount of real time that
			// may elapse from the start of a separate processing arrival
			// to the end of computation for logically correct functioning,
			// specified in absolute time units.  Must be (>= computation).
			// Note that latency = (constraint - computation).
			timePolicy.constraint = HZ / 2200;
					
			// preemptible: This indicates that the computation may be
			// interrupted, subject to the constraint specified above.
			timePolicy.preemptible = true;

			err = thread_policy_set(thread, 
									THREAD_TIME_CONSTRAINT_POLICY, 
									(thread_policy_t)&timePolicy,
									THREAD_TIME_CONSTRAINT_POLICY_COUNT);
			if (err != KERN_SUCCESS) {
				fprintf(stderr, "ERROR: Thread::start unable to set time constraint policy %d\n", err);
				fflush(stderr);
			}

		}
				
	}
#endif
}

/**
 * Called immediately after we get into the thread entry function.
 */
PUBLIC void Thread::runOuter()
{
	bool catchAllExceptions = true;

	if (mTrace) {
		printf("Thread: Starting thread %s\n", mName);
		fflush(stdout);
	}

	configurePriority(true);

	if (catchAllExceptions) {
		try {
			run();
		}
		catch (AppException& e) {
			printf("ERROR: Thread::run exception %s\n", e.getMessage());
			fflush(stdout);
		}
		catch (AppException* e) {
            // had to start doing this for some reason...
			printf("ERROR: Thread::run exception %s\n", e->getMessage());
			fflush(stdout);
		}
		catch (...) {
			printf("ERROR: Thread::run caught exception\n");
			fflush(stdout);
		}
	}
	else {
		try {
			run();
		}
		catch (AppException& e) {
			printf("ERROR: Thread::run exception %s\n", e.getMessage());
			fflush(stdout);
		}
		catch (AppException* e) {
			printf("ERROR: Thread::run exception %s\n", e->getMessage());
			fflush(stdout);
		}
	}

	if (mTrace) {
		printf("Thread: Ending thread %s\n", mName);
		fflush(stdout);
	}

	// let the subclass know in case it has resources to release
	threadEnding();

    // this will cease to be relevant as soon as the thread function retrns
	// isRunning test this
	// !! hmm, may be a slight delay between now and when the thread actually
	// finishes, issues?
#ifdef _WIN32
    mHandle = 0;
#else
	mThread = NULL;
#endif

	if (mTrace) {
		printf("Thread: Ended thread %s\n", mName);
		fflush(stdout);
	}

}

PUBLIC void Thread::threadEnding()
{
}


/**
 * May be overloaded by the subclass, but often you can overload
 * processEvent instead and make use of the default wait loop.
 */
PUBLIC void Thread::run()
{
#ifdef _WIN32	
	HANDLE events[1];
	events[0] = mEvent;
	int nEvents = 1;

    while (!mStop) {

		// fourth arg is timeout interval in milliseconds
        DWORD waitStatus = WaitForMultipleObjects(nEvents, events, FALSE, mTimeout);

		if (!mStop) {
			if (waitStatus == WAIT_FAILED) {
				// failed for some reason
			}
			else if (waitStatus == WAIT_TIMEOUT) {
				// subclass may overload this
				eventTimeout();
			}
			else if (waitStatus >= WAIT_OBJECT_0 && 
					 waitStatus < WAIT_OBJECT_0 + nEvents) {
				// More general than it needs to be, but plan for
				// multiple events, we could just iterate over the event
				// array too...
				int index = waitStatus - WAIT_OBJECT_0;
			
				// only one for now
				processEvent();
			}
		}
	}
#else
	pthread_mutex_init(&mConditionMutex, NULL);
	pthread_cond_init(&mCondition, NULL);

	// sigh, OSX doesn't have clock_gettime, have to use gettimeofday and convert
	timeval  tval;
	timespec tspec;

	// convert the timeout into a sec/usec pair for timespec math
	int timeoutSeconds = (mTimeout / 1000);
	int timeoutUsecs = (mTimeout % 1000) * 1000;

    while (!mStop) {
		
		// OSX for some odd reason doesn't have clock_gettime
		//clock_gettime(CLOCK_REALTIME, &tspec);
		// not sure how fast gettimeofday is, do it outside the mutex
		gettimeofday(&tval, NULL);
		tspec.tv_sec = tval.tv_sec + timeoutSeconds;
		// have to convert microseconds to nanoseconds
		int usecs = tval.tv_usec + timeoutUsecs;
		// not sure if pthread is smart about field overflow
		if (usecs > 1000000) {
			tspec.tv_sec++;
			usecs -= 1000000;
		}
		// convert from microseconds to nanoseconds
		tspec.tv_nsec = usecs * 1000;

		// must have a mutex around the wait
		pthread_mutex_lock(&mConditionMutex);
		int status = pthread_cond_timedwait(&mCondition, &mConditionMutex, &tspec);
		pthread_mutex_unlock(&mConditionMutex);

		switch (status) {
			case 0: {
				processEvent();
			}
			break;
			case ETIMEDOUT: {
				eventTimeout();
			}
			break;
			default: {
				// a random error
				printf("ERROR: Thread: pthread_cond_timedwait returned %d!\n", status);
				fflush(stdout);
				mStop = true;
			}
		}
	}

	// these are apparently not necessary
	//pthread_cond_destroy(&mCondition);
	//pthread_mutex_destroy(&mConditionMutex);

#endif
}

/**
 * Called by the run loop when an event is triggered.
 * Expected to be overloaded by the subclass.
 */
PUBLIC void Thread::processEvent()
{
}

/**
 * Called by the run loop when an event wait times out.
 * Expected to be overloaded by the subclass.
 */
PUBLIC void Thread::eventTimeout()
{
}

/**
 * Ask that the thread be stopped.  This may be ignored if the
 * subclass overrides the run() method.
 */
PUBLIC void Thread::stop()
{
    mStop = true;
	signal();
}

/**
 * Wait for the thread to stop.
 */
PUBLIC bool Thread::stopAndWait()
{
	stop();
#ifdef _WIN32
	// wait for the run method to delete the frame
	// at most 2 seconds
	for (int i = 0 ; i < 20 && isRunning() ; i++) 
	  SleepMillis(100);
#else
	// this is technically correct, but it could hang
	// !! there is a race using pthread_join
	// When runOuter terminates it will null mThread, if that happens before
	// we call pthread_join a null is passed and we get a EXC_BAD_ACCESS
	// We could leave mThread set and set another flag or just do it like
	// we do on Windows
	//pthread_join(mThread, NULL);
	for (int i = 0 ; i < 20 && isRunning() ; i++) 
	  SleepMillis(100);
#endif
	return !isRunning();
}

/**
 * Raise the event that the default run() loop is waiting on so that it
 * will call the processEvent method.
 *
 * Java has interrupt() but I don't think that's the same thing.
 */
PUBLIC void Thread::signal()
{
#ifdef _WIN32
	if (mEvent) {
		if (!SetEvent(mEvent)) {
			printf("ERROR: Thread::signal unable to set event\n");
			fflush(stdout);
		}
	}
#else
	pthread_mutex_lock(&mConditionMutex);
	pthread_cond_signal(&mCondition);
	pthread_mutex_unlock(&mConditionMutex);
#endif
}

PUBLIC bool Thread::isStopping()
{
    return mStop;
}

PUBLIC void Thread::sleep(int millis)
{
    SleepMillis(millis);
}

PUBLIC void Thread::enterCriticalSection()
{
    if (mCsect != NULL)
      mCsect->enter();
}

PUBLIC void Thread::leaveCriticalSection()
{
    if (mCsect != NULL)
      mCsect->leave();
}
