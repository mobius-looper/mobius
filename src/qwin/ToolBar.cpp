/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * An old attempt at tool bars.  
 * This was never used by Mobius and the Mac implementation is stubbed out.
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
 *                                  TOOL BAR                                *
 *                                                                          *
 ****************************************************************************/

PUBLIC ToolBar::ToolBar() 
{
    initToolBar();
}

PRIVATE void ToolBar::initToolBar()
{
	mClassName = "ToolBar";
    mIcons = NULL;
}

PUBLIC ToolBar::~ToolBar() 
{
    delete mIcons;
}

PUBLIC ComponentUI* ToolBar::getUI()
{
    if (mUI == NULL)
      mUI = UIManager::getToolBarUI(this);
	return mUI;
}

PUBLIC ToolBarUI* ToolBar::getToolBarUI()
{
	return (ToolBarUI*)getUI();
}

PUBLIC void ToolBar::addIcon(const char *name)
{
    if (name != NULL) {
        if (mIcons == NULL)
          mIcons = new StringList();
        mIcons->add(name);
    }
}

PUBLIC Dimension* ToolBar::getPreferredSize(Window* w)
{
	if (mPreferred == NULL) {
        TextMetrics* tm = w->getTextMetrics();
		mPreferred = new Dimension();
        
        // fake it for now
        mPreferred->width = 200;
        mPreferred->height = 40;
    }
	return mPreferred;
}

PUBLIC void ToolBar::dumpLocal(int indent)
{
    dumpType(indent, "ToolBar");
}

PUBLIC void ToolBar::open()
{
    ComponentUI* ui = getUI();
    ui->open();
}

QWIN_END_NAMESPACE

/****************************************************************************
 *                                                                          *
 *                                  WINDOWS                                 *
 *                                                                          *
 ****************************************************************************/

#ifdef _WIN32
#include <windows.h>
#include <commctrl.h>
#include "UIWindows.h"
QWIN_BEGIN_NAMESPACE

PUBLIC WindowsToolBar::WindowsToolBar(ToolBar* tb) 
{
    mToolBar = tb;
}

PUBLIC WindowsToolBar::~WindowsToolBar() 
{
}

PUBLIC void WindowsToolBar::open()
{
	if (mHandle == NULL) {
        HWND parent = getParentHandle();
        if (parent != NULL) {
            DWORD style = getWindowStyle() | WS_CLIPSIBLINGS | 
                CCS_TOP | TBSTYLE_TOOLTIPS;

            TBBUTTON tbb[] = {
                STD_FILENEW,  1, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0, 0, 0, 0,
                STD_FILEOPEN, 2, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0, 0, 0, 0,
                STD_FILESAVE, 3, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0, 0, 0, 0,
                STD_PRINT,    4, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0, 0, 0, 0,
                STD_PRINTPRE, 5, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0, 0, 0, 0,
            };

            mHandle = CreateToolbarEx(parent,
                                      style,
                                      1,        // id
                                      0,        // number of button images
                                      // module instance with the executable
                                      // file that contains the bitmap resource
                                      HINST_COMMCTRL,

                                      // bitmap resource identifier
                                      IDB_STD_SMALL_COLOR,
                                  
                                      tbb, // array of TBBUTTON structures
                                      5, // number of TBUTTON structures
                                      0, // button width
                                      0, // button height
                                      0, // button image width
                                      0, // button image height
                                      sizeof(TBBUTTON));

            if (mHandle == NULL)
              printf("Unable to create ToolBar control\n");
            else {
                subclassWindowProc();
                SetWindowLong(mHandle, GWL_USERDATA, (LONG)this);
            }
        }
    }
}

#endif

//////////////////////////////////////////////////////////////////////
//
// OSX
//
//////////////////////////////////////////////////////////////////////

#ifdef OSX
#include <Carbon/Carbon.h>
#include "UIMac.h"
QWIN_BEGIN_NAMESPACE

PUBLIC MacToolBar::MacToolBar(ToolBar* tb) 
{
    mToolBar = tb;
}

PUBLIC MacToolBar::~MacToolBar() 
{
}

PUBLIC void MacToolBar::open()
{
	if (mHandle == NULL) {
    }
}

QWIN_END_NAMESPACE
#endif

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
