/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * A root singleton object to manage a critical section and provide
 * a place to hang an exit cleanup method.
 *
 */


#include <stdio.h>
#include <memory.h>
#include <string.h>

#include "util.h"
#include "Trace.h"
#include "Thread.h"
#include "Qwin.h"

QWIN_BEGIN_NAMESPACE

/****************************************************************************
 *                                                                          *
 *   						   GLOBAL UTILITIES                             *
 *                                                                          *
 ****************************************************************************/

/**
 * A global critical section object used on Mac to ensure that we don't
 * try to modify a component's state while the UI thread is rendering it
 * as a side effect of invalidate().  This happens in the MidiControlDialog
 * if MIDI events come in too fast.
 *
 * NOTE: I tried putting this in Qwin but that caused problems compiling
 * with Mobius, it conflicted with other "class CriticalSection"s in 
 * Mobius header files.  Not sure why the class qualifier didn't work,
 * maybe because it was static?   Just make it a good old-fashioned
 * global variable.
 */

static CriticalSection* QwinCsect = new CriticalSection();

PUBLIC void Qwin::csectEnter()
{
	if (QwinCsect != NULL)
	  QwinCsect->enter();
}

PUBLIC void Qwin::csectLeave()
{
	if (QwinCsect != NULL)
	  QwinCsect->leave();
}

/**
 * Perform pre-exit cleanup and optional analysis.
 */
PUBLIC void Qwin::exit(bool dump)
{
	Font::exit(dump);
	// I guess clean up this too?
	delete QwinCsect;
	QwinCsect = NULL;
}

/**
 * Global functions for OSX so we don't have to use namespaces, e.g.
 * Qwin::Qwin::exit
 */
PUBLIC void QwinExit(bool dump)
{
	Qwin::exit(dump);
}

QWIN_END_NAMESPACE
