/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 *
 * Windows implementation of MidiTimer.
 *
 */

#include <stdio.h>

#include "Port.h"
#include "Util.h"
#include "Trace.h"
#include "Thread.h"

#include "MidiTimer.h"
#include "WinMidiEnv.h"

//////////////////////////////////////////////////////////////////////
//
// TimerInterrupt
//
//////////////////////////////////////////////////////////////////////

/*
 * Arguments:
 *	     id: timer id (from timeSetEvent)
 *	    msg: not used
 *	   user: fourth arg to timeSetEvent (a MidiTimer object pointer)
 *	 param1: not used
 *	 param2: not used
 *
 * Returns: none
 *
 * This function gets called every millisecond by the system timer.
 * Here is where the work gets done.
 *
 * This of course needs to be as fast as humanly possible, though
 * if we finish up within 1ms it should be ok.
 *
 * The following functions can be called safely:
 *		PostMessage, timeGetSystemTime, timeGetTime, timeSetEvent,
 *		timeKillEvent, midiOutShortMsg, midiOutLongMsg, OutputDebugStr
 *
 * This previous list was from the Windows 3.1 docs, it might be
 * different for WIN32. In practice, I've noticed that printf is ok too.
 *
 * NOTE WELL: Despite the documentation, calling timeKillEvent inside
 * the timer interrupt handler doesn't appear to work, at least not under
 * the debugger.  The mInterruptEnable flag was added to temporarily suspend
 * timer interrupts, without actually killing the timer.
 * 
 * The user callback is expected to call setNextSignalClock or some other
 * timer control method to re-arm the timer.
 *
 */

#define MAX_TRACE 1000
#define MAX_RECOVERY 5

// this controls whether we try the performance counter
bool UsePerformanceCounter = false;

bool PerformanceCounterFrequencyChecked = false;
int TicksPerMillisecond = 0;
LARGE_INTEGER LastPerformanceCounter;
int TraceCount = 0;
int CounterDiffs[MAX_TRACE];
bool CountersDumped = false;
int MilliCounter = 0;
bool TraceCounter = true;
long InterruptCounter = 0;

void WINAPI TimerInterrupt(UINT id, UINT msg, DWORD user, DWORD param1,
						   DWORD param2)
{
    bool firstTime = false;

    if (!UsePerformanceCounter) {
        MidiTimer *t = (MidiTimer *)user;
        t->interrupt();
        return;
    }

    InterruptCounter++;

    if (!PerformanceCounterFrequencyChecked) {
        // good time to initialize these too
        LastPerformanceCounter.QuadPart = 0;
        for (int i = 0 ; i < MAX_TRACE ; i++)
          CounterDiffs[i] = 0;

        LARGE_INTEGER freq;
        if (QueryPerformanceFrequency(&freq)) {
            // this is ticks per second although this can be 64-bit
            // I realy don't think it can be that large?
            TicksPerMillisecond = (int)(freq.QuadPart / 1000);

            printf("QueryPerformanceFrequency low %ld high %ld\n",
                   (long)(freq.LowPart), (long)(freq.HighPart));

            printf("QueryPerformanceFrequency TicksPerMillisecond %d\n",
                   TicksPerMillisecond);

            fflush(stdout);
        }
        else {
            printf("QueryPerformanceFrequency failed\n");
            fflush(stdout);
        }
        PerformanceCounterFrequencyChecked = true;
        firstTime = true;
    }

    if (TicksPerMillisecond > 0) {
        LARGE_INTEGER count;
        if (!QueryPerformanceCounter(&count)) {
            printf("QueryPerformanceCounter failed\n");
            fflush(stdout);
        }
        else {
            if (!firstTime) {
                int delta = (int)(count.QuadPart - LastPerformanceCounter.QuadPart);
                int diff = TicksPerMillisecond - delta;
                //if (error != 0) {
                if (TraceCount < MAX_TRACE) {
                    CounterDiffs[TraceCount++] = diff;
                }
                else if (!CountersDumped) {
                    for (int i = 0 ; i < MAX_TRACE ; i++)
                      printf("%d\n", CounterDiffs[i]);
                    fflush(stdout);
                    CountersDumped = true;
                }
                //}

                MilliCounter += delta;
                if (MilliCounter < TicksPerMillisecond) {
                    if (TraceCounter) {
                        printf("WinMidiTimer: %ld waiting\n", InterruptCounter);
                        fflush(stdout);
                    }
                }
                else {
                    int interrupts = 0;
                    while (interrupts < MAX_RECOVERY &&
                           MilliCounter > TicksPerMillisecond) {

                        MidiTimer *t = (MidiTimer *)user;
                        t->interrupt();

                        MilliCounter -= TicksPerMillisecond;
                        interrupts++;
                    }

                    if (interrupts > 1) {
                        if (TraceCounter) {
                            printf("WinMidiTimer: %ld advanced %d\n", 
                                   InterruptCounter, interrupts);
                            fflush(stdout);
                        }
                    }
                }
            }
            LastPerformanceCounter = count;
        }
    }


}

//////////////////////////////////////////////////////////////////////
//
// WinMidiTimer
//
//////////////////////////////////////////////////////////////////////

PUBLIC WinMidiTimer::WinMidiTimer(WinMidiEnv* env) : MidiTimer(env)
{

	mTimer = 0;
	mActive = false;
}

PUBLIC WinMidiTimer::~WinMidiTimer()
{
	deactivate();
}

/**
 * Activate the timer.
 * Activation means the timer object has obtained access to the internal
 * OS timer.  It will not recieve interrupts until it has been started.
 *
 * UPDATE: I forget why I decided to distinguish between "active"
 * and "running".  It seems better to not activate until we really 
 * need it?
 */
PRIVATE bool WinMidiTimer::activate(void) 
{
	if (!mActive) {

        // This requests timer services at a resolution of 1ms
        // I think this can fail if another process has already 
        // locked this timer, though the docs are vague.  
        // TIMER_NOCANDO is only supposed to be returned if the period is out
        // of range.

		//Trace(2, "WinMidiTimer: activating millisecond timer\n");
		int rc = timeBeginPeriod(1);
        if (rc != TIMERR_NOCANDO)
          mActive = true;
        else {
            // need a better reporting mechanism
			printf("ERROR: Unable to allocate high-resolution timer!\n");
		}
	}

	return mActive;
}

/**
 * Deactivate the timer.
 */
PRIVATE void WinMidiTimer::deactivate(void)
{
	if (mActive) {
        stop();
		printWarnings();
        timeEndPeriod(1);
        mActive = false;
	}
}

/**
 * MidiTimer overload to get the timer started.
 * Internally we will also activate the timer.
 */
PUBLIC bool WinMidiTimer::start()
{
    if (mTimer == 0 && activate()) {

        // start receiving interrupts
        mTimer = timeSetEvent(1, 1, (LPTIMECALLBACK)TimerInterrupt,
                              (DWORD)this, TIME_PERIODIC);

        if (mTimer == 0) 
          printf("ERROR: Unable to start timer!\n");
    }

	return (mTimer != 0);
}

PUBLIC void WinMidiTimer::stop(void)
{
    if (mTimer != 0) {
		
        // hmm, not sure how to recover from inability to stop timer...
		Trace(2, "MidiTimer: deactivating millisecond timer\n");

        int rc = timeKillEvent(mTimer);
        if (rc == TIMERR_NOCANDO)
          printf("ERROR: Unable to stop the timer!\n");

        mTimer = 0;
    }
}

/**
 * Return true if the timer is running.  
 * We're considered to be running if we've called timeSetEvent
 * and are receiving interrupts.  We are not necessarily sending
 * MIDI clocks or in the "started" state, which means that we've
 * sent StartSong.
 */
PUBLIC bool WinMidiTimer::isRunning(void) 
{
    return (mTimer != 0);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
