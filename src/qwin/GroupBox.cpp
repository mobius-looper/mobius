/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * The closest correspondence in Swing would be a Border assigned
 * to a panel, but let's model this as its own container for now
 * until we flesh out the concept of borders.
 *
 */

#include <stdio.h>
#include <string.h>
#include <memory.h>

#include "Util.h"
#include "Qwin.h"
#include "UIManager.h"

QWIN_BEGIN_NAMESPACE

/****************************************************************************
 *                                                                          *
 *   							  GROUP BOX                                 *
 *                                                                          *
 ****************************************************************************/

PUBLIC GroupBox::GroupBox() 
{
	GroupBox(NULL);
}

PUBLIC GroupBox::GroupBox(const char *s) 
{
	mClassName = "GroupBox";
	mText = CopyString(s);
}

PUBLIC GroupBox::~GroupBox() 
{
	delete mText;
}

PUBLIC ComponentUI* GroupBox::getUI()
{
    if (mUI == NULL)
      mUI = UIManager::getGroupBoxUI(this);
	return mUI;
}

PUBLIC GroupBoxUI* GroupBox::getGroupBoxUI()
{
	return (GroupBoxUI*)getUI();
}

PUBLIC void GroupBox::setText(const char *s) 
{
	delete mText;
	mText = CopyString(s);
    GroupBoxUI* ui = getGroupBoxUI();
    ui->setText(s);
}

PUBLIC const char* GroupBox::getText()
{
    return mText;
}

/**
 * Petzold doesn't say much about size calculation on these.
 * !! This needs to be pushed into the GroupBoxUI
 *
 * These probably need to be handled like TabbedPanel, first
 * compute the size of the children, then add some girth for the
 * surrounding graphics.
 */
PUBLIC Dimension* GroupBox::getPreferredSize(Window* w)
{
	// these are almost always presized
	if (mPreferred == NULL) {
        TextMetrics* tm = w->getTextMetrics();
		mPreferred = new Dimension();

		if (tm != NULL) {
			// it will be rendered with about a char of padding on the left
			// adjust for two on either side
			int chars = 4 + ((mText != NULL) ? strlen(mText) : 0);
			mPreferred->width = chars * tm->getMaxWidth();

			int fontHeight = tm->getHeight() + tm->getExternalLeading();
			mPreferred->height = fontHeight * 2;
		}
		else {
			// must be mac, just guess since we don't use these yet
			int chars = 4 + ((mText != NULL) ? strlen(mText) : 0);

			mPreferred->width = chars * 16;
			int fontHeight = 20;
			mPreferred->height = fontHeight * 2;
		}
	}
	return mPreferred; 
}

PUBLIC void GroupBox::open()
{
    GroupBoxUI* ui = getGroupBoxUI();
    ui->open();

	// recurse on children
	Container::open();
}

PUBLIC void GroupBox::dumpLocal(int indent)
{
    dumpType(indent, "GroupBox");
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

PUBLIC WindowsGroupBox::WindowsGroupBox(GroupBox* gb) 
{
	mGroupBox = gb;
}

PUBLIC WindowsGroupBox::~WindowsGroupBox() 
{
}

PUBLIC void WindowsGroupBox::setText(const char *s) 
{
	if (mHandle != NULL)
	  SetWindowText(mHandle, s);
}

PUBLIC void WindowsGroupBox::open()
{
	if (mHandle == NULL) {
        HWND parent = getParentHandle();
        if (parent != NULL) {
            // PS_PUSHBUTTON and PS_DEFPUSHBUTTON are the same in
            // non-dialog windows except that DEF has a heavier outline

            // do these need WS_GROUP or WS_TABSTOP?
            DWORD style = getWindowStyle() | BS_GROUPBOX;

            Bounds* b = mGroupBox->getBounds();
            Point p;
            mGroupBox->getNativeLocation(&p);

            mHandle = CreateWindow("button",
                                   mGroupBox->getText(),
                                   style,
                                   p.x, p.y, b->width, b->height,
                                   parent,
                                   0,
                                   NULL,
                                   NULL);

            if (mHandle == NULL)
              printf("Unable to create GroupBox control\n");
            else {
                // don't really need this since we can't generate events
                // but be consistent
                subclassWindowProc();
                SetWindowLong(mHandle, GWL_USERDATA, (LONG)this);
				mGroupBox->initVisibility();
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


PUBLIC MacGroupBox::MacGroupBox(GroupBox* gb) 
{
	mGroupBox = gb;
}

PUBLIC MacGroupBox::~MacGroupBox() 
{
}

PUBLIC void MacGroupBox::setText(const char *s) 
{
	if (mHandle != NULL) {
		//SetWindowText(mHandle, s);
	}
}

PUBLIC void MacGroupBox::open()
{
	// give it something to say
	if (mGroupBox->getComponents() == NULL) {
		Label* l = new Label("GroupBox not implemented");
		mGroupBox->add(l);
		mGroupBox->setBorder(Border::BlackLine);
	}
}

QWIN_END_NAMESPACE
#endif

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
