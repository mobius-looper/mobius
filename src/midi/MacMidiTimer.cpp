/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Mac subclass of MidiTimer.
 *
 * See coreaudio-api mailing list thread "Timestamp from midipacket"
 * for an example of using AudioConvertHostTimeToNanos.
 * 
 * AudioGetCurrentHostTime just seems to wrap mach_absolute_time
 */

#include <stdio.h>

// for usleep
#include <unistd.h>

#include "port.h"
#include "util.h"
#include "Trace.h"
#include "Thread.h"

#include "MidiTimer.h"
#include "MacMidiEnv.h"

#include <CoreAudio/HostTime.h>
#include <CoreServices/CoreServices.h>
#include <mach/mach_time.h>

//////////////////////////////////////////////////////////////////////
//
// MacMidiTimerThread
//
//////////////////////////////////////////////////////////////////////

/**
 * TODO: Also look at using a HAL IO Proc as a timing source.
 */
class MacMidiTimerThread : public Thread {

  public:

	MacMidiTimerThread(MacMidiTimer* timer);	

	void run();

  private:

	MacMidiTimer* mTimer;

};

PUBLIC MacMidiTimerThread::MacMidiTimerThread(MacMidiTimer* timer)
{
	mTimer = timer;

	// there are only two priorities, 1 makes this
	// as close to a realtime thread as we can
	setPriority(1);

	setName("MacMidiTimerThread");
}

#define MAX_DELTAS 100
static int misses[MAX_DELTAS];
static int deltas[MAX_DELTAS];
static int drifts[MAX_DELTAS];
static int deltaCount = 0;


/**
 * pthread_cond_timedwait is supposed too jittery but maybe
 * not if we're in a time constraint thread?  Various posts
 * suggest simple wait functions are enough and much simpler.
 * 
 * Supposedly AudioGetCurrentHostTime is just a wrapper around
 * mach_asbolute_time, the deltas appear to be the same.
 *
 * Another example of absolute to nanosecond conversion doesn't
 * require CoreServices:
 *
 * 
 *     // If this is the first time we've run, get the timebase.
 *     // We can use denom == 0 to indicate that sTimebaseInfo is
 *     // uninitialised because it makes no sense to have a zero
 *     // denominator is a fraction.
 * 
 *     static mach_timebase_info_data_t    sTimebaseInfo;
 *     if ( sTimebaseInfo.denom == 0 ) {
 *         (void) mach_timebase_info(&sTimebaseInfo);
 *     }
 * 
 *    // Do the maths.  We hope that the multiplication doesn't
 *    // overflow; the price you pay for working in fixed point.
 *  
 *    elapsedNano = elapsed * sTimebaseInfo.numer / sTimebaseInfo.denom;
 *
 */
PUBLIC void MacMidiTimerThread::run()
{
	UInt64 lastTime;
	UInt64 lastTimerTime;
	UInt64 startTime;
	UInt64 endTime;
	UInt64 nextTime;

	mach_timebase_info_data_t tbi;
	mach_timebase_info(&tbi);
	double invRatio = ((double)tbi.denom) / ((double)tbi.numer);
	int absoluteWait = (int)(1000000.0 * invRatio);

	//printf("%d absolute units per cycle\n", absoluteWait);

	lastTime = mach_absolute_time();
	lastTimerTime = lastTime;
	nextTime = lastTime + absoluteWait;

    while (!mStop) {

		mach_wait_until(nextTime);
		startTime = mach_absolute_time();

		if (!mStop) {

			int miss = startTime - nextTime;
			lastTime = nextTime;
			nextTime += absoluteWait;

			int delta = startTime - lastTimerTime;
			int drift = delta - absoluteWait;
			
			lastTimerTime = startTime;

			// defined on CoreServices
			// bizarre casting used in the examples
			//Nanoseconds nanos = AbsoluteToNanoseconds(*(AbsoluteTime*)&delta);
			//delta = *(int*)&nanos;

			if (deltaCount < MAX_DELTAS) {
				deltas[deltaCount] = delta;
				misses[deltaCount] = miss;
				drifts[deltaCount] = drift;
			}


			// this may take awhile so have to check for slice overflow
			mTimer->interrupt();

			endTime = mach_absolute_time();
			if (nextTime <= endTime) {
				// must have had a really long interrupt
				trace("MacMidiTimer interrupt overflow!\n");
				nextTime = endTime + 100;
			}

			/*
			if (deltaCount < MAX_DELTAS) {
				deltaCount++;
				if (deltaCount == MAX_DELTAS) {
					printf("Millisecond deltas:\n");
					for (int i = 0 ; i < MAX_DELTAS ; i++) {
						printf("%d %d %d %d\n", deltas[i], misses[i], drifts[i]);
					}
					fflush(stdout);
				}
			}
			*/
		}
	}
}

//////////////////////////////////////////////////////////////////////
//
// MacMidiTimer
//
//////////////////////////////////////////////////////////////////////

PUBLIC MacMidiTimer::MacMidiTimer(MacMidiEnv* env) : MidiTimer(env)
{
	mThread = NULL;
}

PUBLIC MacMidiTimer::~MacMidiTimer()
{
	stop();
}

/**
 * MidiTimer overload to get the timer started.
 */
PUBLIC bool MacMidiTimer::start()
{
    if (mThread == NULL) {
		mThread = new MacMidiTimerThread(this);
		mThread->start();
    }

	return (mThread != NULL);
}

PUBLIC void MacMidiTimer::stop(void)
{
    if (mThread != NULL) {
		// always wait for it???
		mThread->stopAndWait();
		mThread = NULL;
	}
}

/**
 * Return true if the timer is running.  
 */
PUBLIC bool MacMidiTimer::isRunning(void) 
{
    return (mThread != NULL);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
