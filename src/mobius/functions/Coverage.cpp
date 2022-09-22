/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Unit test functions to initialize and display coverage statistics.
 *
 */

#include <stdio.h>
#include <memory.h>

#include "Action.h"
#include "Event.h"
#include "FadeWindow.h"
#include "Function.h"
#include "Layer.h"
#include "Loop.h"
#include "Messages.h"

//////////////////////////////////////////////////////////////////////
//
// CoverageFunction
//
//////////////////////////////////////////////////////////////////////

class CoverageFunction : public Function {
  public:
	CoverageFunction();
	void invoke(Action* action, Mobius* m);
  private:
};

PUBLIC Function* Coverage = new CoverageFunction();

PUBLIC CoverageFunction::CoverageFunction() :
    Function("Coverage", 0)
{
    global = true;
    scriptOnly = true;
}

PUBLIC void CoverageFunction::invoke(Action* action, Mobius* m)
{
	if (action->down) {
		trace(action, m);

		FadeWindow::showCoverage();
		Layer::showCoverage();
	}
}

//////////////////////////////////////////////////////////////////////
//
// InitCoverageFunction
//
//////////////////////////////////////////////////////////////////////

class InitCoverageFunction : public Function {
  public:
	InitCoverageFunction();
	void invoke(Action* action, Mobius* m);
  private:
};

PUBLIC Function* InitCoverage = new InitCoverageFunction();

PUBLIC InitCoverageFunction::InitCoverageFunction() :
    Function("InitCoverage", 0)
{
    global = true;
    scriptOnly = true;
}

PUBLIC void InitCoverageFunction::invoke(Action* action, Mobius* m)
{
	if (action->down) {
		trace(action, m);

		FadeWindow::initCoverage();
		Layer::initCoverage();
	}
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
