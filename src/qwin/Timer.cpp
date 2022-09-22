/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * This is used by the Mobius UI to poll every 1/10 second.
 * Really should try to do this with the MobiusThread having
 * it push refresh events to he UI.
 *
 * The timer isn't really associated with the UI, could move this
 * over to util?
 *
 */

#include <stdio.h>
#include <memory.h>

#include "Util.h"
#include "Thread.h"
#include "Qwin.h"
#include "UIManager.h"

QWIN_BEGIN_NAMESPACE

/****************************************************************************
 *                                                                          *
 *                                   TIMER                                  *
 *                                                                          *
 ****************************************************************************/

/**
 * This was originally here just for Windows which has no way to 
 * associate user data with the timer callback.  We don't need this on Mac
 * though it could be used to ensure that all timers allocated by a plugin
 * are automatically closed.
 */
int SimpleTimer::TimerCount = 0;
SimpleTimer* SimpleTimer::Timers[MAX_TIMERS];
bool SimpleTimer::KludgeTraceTimer = false;

PUBLIC SimpleTimer::SimpleTimer(int delay)
{
    init(delay);
}

PUBLIC SimpleTimer::SimpleTimer(int delay, ActionListener* l)
{
    init(delay);
    addActionListener(l);
}

PRIVATE void SimpleTimer::init(int delay)
{
	mRunning = false;
	mDelay = delay;
    mListeners = new Listeners();
    mNativeTimer = NULL;

	// should be in a csect!
	if (TimerCount < MAX_TIMERS) {
		mNativeTimer = UIManager::getTimer(this);
		Timers[TimerCount++] = this;
	}
	else {
        // no one it turns out is going to catch this
        // so should we just silently ignore it?
		throw new AppException("Maximum timer count exceeded");
	}
}

PUBLIC SimpleTimer::~SimpleTimer()
{
	bool found = false;
	for (int i = 0 ; i < TimerCount ; i++) {
		if (found)
			Timers[i-1] = Timers[i];
		else if (Timers[i] == this)
		  found = true;
	}
	TimerCount--;

	// make sure we're not in an interrupt
	SleepMillis(100);

    delete mNativeTimer;
	delete mListeners;
}

PUBLIC NativeTimer* SimpleTimer::getNativeTimer()
{
    return mNativeTimer;
}

PUBLIC void SimpleTimer::addActionListener(ActionListener* l)
{
	mListeners->addListener(l);
}

PUBLIC void SimpleTimer::removeActionListener(ActionListener* l)
{
	mListeners->removeListener(l);
}

PUBLIC void SimpleTimer::fireActionPerformed()
{
	if (mRunning)
	  mListeners->fireActionPerformed(this);
}

PUBLIC int SimpleTimer::getDelay()
{
	return mDelay;
}

PUBLIC void SimpleTimer::setDelay(int delay)
{
	// will have to create a new timer...
}

PUBLIC void SimpleTimer::start()
{
	mRunning = true;
}

PUBLIC void SimpleTimer::stop()
{
	mRunning = false;
}

PUBLIC bool SimpleTimer::isRunning()
{
	return mRunning;
}

QWIN_END_NAMESPACE

/****************************************************************************
 *                                                                          *
 *                                  WINDOWS                                 *
 *                                                                          *
 ****************************************************************************/

#ifdef _WIN32
#include <windows.h>
#include "UIWindows.h"
QWIN_BEGIN_NAMESPACE

VOID CALLBACK TimerProc(HWND hwnd, UINT msg, UINT timerId, DWORD time)
{
	SimpleTimer* timer = WindowsTimer::getTimer(timerId);
	if (timer != NULL)
	  timer->fireActionPerformed();
}

PUBLIC WindowsTimer::WindowsTimer(SimpleTimer* t)
{
    mTimer = t;
	mId = SetTimer(NULL, 0, t->getDelay(), (TIMERPROC)TimerProc);
    if (mId == 0)
      throw new AppException("Unable to allocate timer");
}

PUBLIC WindowsTimer::~WindowsTimer()
{
	if (mId != 0) {
		if (!KillTimer(NULL, mId)) {
			// what now
			printf("Unable to kill timer!\n");
		}
	}
}

/**
 * Locate the Timer object associated with the given timer id.
 * If we could assume these were small integers could use
 * an array instead.
 *
 * HATE the control flow, think of a better way to encapsulate
 * the timer id.
 */
PUBLIC SimpleTimer* WindowsTimer::getTimer(int id)
{
	SimpleTimer* found = NULL;
	for (int i = 0 ; i < SimpleTimer::TimerCount ; i++) {
		SimpleTimer* t = SimpleTimer::Timers[i];
        WindowsTimer* native = (WindowsTimer*)t->getNativeTimer();
        if (native->mId == id) {
			found = t;
			break;	
		}
	}
	return found;
}

QWIN_END_NAMESPACE
#endif // _WIN32

//////////////////////////////////////////////////////////////////////
//
// OSX
//
//////////////////////////////////////////////////////////////////////

#ifdef OSX
#include <Carbon/Carbon.h>
#include "UIMac.h"
#include "MacUtil.h"
QWIN_BEGIN_NAMESPACE

pascal void MacTimerHandler(EventLoopTimerRef timer, void* userData)
{
	if (SimpleTimer::KludgeTraceTimer) {
		printf("Timer fired!!\n");
		fflush(stdout);
	}

	MacTimer* mt = (MacTimer*)userData;
	mt->fire();
}

PUBLIC MacTimer::MacTimer(SimpleTimer* t)
{
    mTimer = t;
	mNative = NULL;

	// interval unit is seconds, Timer.mDelay unit is milliseconds
	// values are EventTimerInterval which are EventTime which are double

	EventTimerInterval interval = (double)mTimer->getDelay() / (double)1000;

	OSStatus status = InstallEventLoopTimer(GetMainEventLoop(),
											0, // EventTimerInterfval inFireDelay
											interval,
											NewEventLoopTimerUPP(MacTimerHandler),
											this, // (void*) 
											&mNative);

	CheckStatus(status, "MacTimer:InstallEventLoopTimer");

	// !! Note that for plugins it is vital that you remove the timer handler
	// before the plugin is closed, otherwise there will be a timer event 
	// handler pointing to invalid memory
}

PUBLIC MacTimer::~MacTimer()
{
	if (mNative != NULL) {
		OSStatus status = RemoveEventLoopTimer(mNative);
		CheckStatus(status, "MacTimer:RemoveEventLoopTimer");
	}
}

PUBLIC void MacTimer::fire()
{
	mTimer->fireActionPerformed();
}

QWIN_END_NAMESPACE
#endif

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
