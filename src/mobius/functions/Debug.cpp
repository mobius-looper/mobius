/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Debug - Unit test function to run some random undefined test.
 *
 * Breakpoint - Unit test function to hit a debugger breakpoint.
 *
 * Status - Dump some runtime statistics to the console.
 */


#include <stdio.h>
#include <memory.h>

#include "AudioInterface.h"
#include "MidiInterface.h"

#include "Action.h"
#include "Event.h"
#include "Function.h"
#include "Layer.h"
#include "Loop.h"
#include "Messages.h"
#include "Mobius.h"

//////////////////////////////////////////////////////////////////////
//
// DebugFunction
//
//////////////////////////////////////////////////////////////////////

class DebugFunction : public Function {
  public:
	DebugFunction();
	void invoke(Action* action, Mobius* m);
  private:
};

PUBLIC Function* Debug = new DebugFunction();

PUBLIC DebugFunction::DebugFunction() :
    Function("Debug", 0)
{
    global = true;
	scriptOnly = true;
}

PUBLIC void DebugFunction::invoke(Action* action, Mobius* m)
{
  if (action->down) {
		trace(action, m);

        MobiusContext* con = m->getContext();
        MidiInterface* midi = con->getMidiInterface();
        AudioStream* stream = m->getAudioStream();

        long milli = midi->getMilliseconds();
        double st = stream->getStreamTime();

        Trace(2, "DebugFunction: current millisecond %ld stream time (x1000) %ld\n",
              milli, (long)(st * 1000));

        Trace(2, "DebugFunction: trigger millisecond %ld stream time (x1000) %ld\n",
              action->millisecond, (long)(action->streamTime * 1000));

		// test loop detection
		//while (true);
	}
}

//////////////////////////////////////////////////////////////////////
//
// BreakpointFunction
//
//////////////////////////////////////////////////////////////////////

/**
 * Have to extern these in the referending file if you want to use
 * them for something.
 */
bool Breakpoint1 = false;
bool Breakpoint2 = false;
bool Breakpoint3 = false;

class BreakpointFunction : public Function {
  public:
	BreakpointFunction();
	void invoke(Action* action, Mobius* m);
  private:
};

PUBLIC Function* Breakpoint = new BreakpointFunction();

PUBLIC BreakpointFunction::BreakpointFunction() :
    Function("Breakpoint", 0)
{
    global = true;
	scriptOnly = true;
}

/**
 * I forget what the int arg stuff is supposed to do.
 */
PUBLIC void BreakpointFunction::invoke(Action* action, Mobius* m)
{
	if (action->down) {
		trace(action, m);

		int number = action->arg.getInt();
		switch (number) {
			case 0:
			case 1:
				Breakpoint1 = !Breakpoint1;
				break;
			case 2:
				Breakpoint2 = !Breakpoint2;
				break;
			case 3:
				Breakpoint3 = !Breakpoint3;
				break;
		}
	}
}

//////////////////////////////////////////////////////////////////////
//
// DebugStatusFunction
//
//////////////////////////////////////////////////////////////////////

class DebugStatusFunction : public Function {
  public:
	DebugStatusFunction();
	void invoke(Action* action, Mobius* m);
  private:
};

PUBLIC Function* DebugStatus = new DebugStatusFunction();

PUBLIC DebugStatusFunction::DebugStatusFunction() :
    Function("Status", 0)
{
    global = true;

    // no, if we're going to dump loop/layer/segment structure
    // it needs to be stable
    //outsideInterrupt = true;

	// this keeps localize from complaining about a missing key
	externalName = true;
}

PUBLIC void DebugStatusFunction::invoke(Action* action, Mobius* m)
{
	if (action->down) {
		trace(action, m);

        m->logStatus();
	}
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
