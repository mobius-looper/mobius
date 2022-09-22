/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * I'm not really sure what this does, the Mac implementation has
 * been stubbed out and it was never used with Mobius.
 * I think on windows it is something that typically goes into the bottom
 * border.
 *
 */

#include <stdio.h>
#include <memory.h>

#include "Util.h"
#include "Qwin.h"
#include "UIManager.h"

QWIN_BEGIN_NAMESPACE

/****************************************************************************
 *                                                                          *
 *                                 STATUS BAR                               *
 *                                                                          *
 ****************************************************************************/

PUBLIC StatusBar::StatusBar() 
{
    initStatusBar();
}

PRIVATE void StatusBar::initStatusBar()
{
	mClassName = "StatusBar";
}

PUBLIC StatusBar::~StatusBar() 
{
}

PUBLIC ComponentUI* StatusBar::getUI()
{
	if (mUI == NULL)
	  mUI = UIManager::getStatusBarUI(this);
	return mUI;
}

PUBLIC StatusBarUI* StatusBar::getStatusBarUI()
{
	return (StatusBarUI*)getUI();
}

PUBLIC Dimension* StatusBar::getPreferredSize(Window* w)
{
	if (mPreferred == NULL) {
		TextMetrics* tm = w->getTextMetrics();
		mPreferred = new Dimension();
        
        // hmm, these size themselves though it must be based on the
        // default font.  Emperical tests show height to be 10 using
        // the default font?  No that seems too low and doesn't 
		// include the border.
        mPreferred->height = tm->getHeight() + tm->getExternalLeading() + 10;

        // width is arbitrary, since these are always placed in 
        // a BorderLayout it doesn't really matter
        mPreferred->width = tm->getMaxWidth() * 2;
    }
	return mPreferred;
}

PUBLIC void StatusBar::open()
{
    ComponentUI* ui = getUI();
    ui->open();
}

PUBLIC void StatusBar::dumpLocal(int indent)
{
    dumpType(indent, "StatusBar");
}

QWIN_END_NAMESPACE

/****************************************************************************
 *                                                                          *
 *   							   WINDOWS                                  *
 *                                                                          *
 ****************************************************************************/

#ifdef _WIN32
#include <windows.h>
#include <commctrl.h>
#include "UIWindows.h"
QWIN_BEGIN_NAMESPACE

PUBLIC WindowsStatusBar::WindowsStatusBar(StatusBar* sb) 
{
	mStatusBar = sb;
}

PUBLIC WindowsStatusBar::~WindowsStatusBar() 
{
}

PUBLIC void WindowsStatusBar::open()
{
	if (mHandle == NULL) {
		HWND parent = getParentHandle();
        if (parent != NULL) {
            // don't really need CCS_BOTTOM with LayoutManagers
            DWORD style = getWindowStyle() | WS_CLIPSIBLINGS | 
                CCS_BOTTOM | SBARS_SIZEGRIP;

            Point p;
            mStatusBar->getNativeLocation(&p);

            mHandle = CreateStatusWindow(style,
                                         "Ready",
                                         parent,
                                         1 // id
                );

            if (mHandle == NULL)
              printf("Unable to create StatusBar control\n");
            else {
                subclassWindowProc();
                SetWindowLong(mHandle, GWL_USERDATA, (LONG)this);
            }
        }
    }
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
QWIN_BEGIN_NAMESPACE

PUBLIC MacStatusBar::MacStatusBar(StatusBar* sb) 
{
	mStatusBar = sb;
}

PUBLIC MacStatusBar::~MacStatusBar() 
{
}

PUBLIC void MacStatusBar::open()
{
	if (mHandle == NULL) {
    }
}

QWIN_END_NAMESPACE
#endif

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
