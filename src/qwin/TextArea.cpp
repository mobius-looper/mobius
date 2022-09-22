/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Multi-line text field.
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
 *                                 TEXT AREA                                *
 *                                                                          *
 ****************************************************************************/

PUBLIC TextArea::TextArea() 
{
    initTextArea();
}

PUBLIC TextArea::TextArea(const char *s) 
{
    initTextArea();
	setText(s);
}

PRIVATE void TextArea::initTextArea() 
{
	mClassName = "TextArea";
    mScrolling = false;
    mRows = 4;
}

PUBLIC TextArea::~TextArea()
{
}

PUBLIC ComponentUI* TextArea::getUI()
{
    if (mUI == NULL)
      mUI = UIManager::getTextAreaUI(this);
	return mUI;
}

PUBLIC TextAreaUI* TextArea::getTextAreaUI()
{
	return (TextAreaUI*)getUI();
}

PUBLIC void TextArea::setRows(int i) 
{
	mRows = i;
}

PUBLIC int TextArea::getRows() 
{
	return mRows;
}

PUBLIC void TextArea::setScrolling(bool b) 
{
	mScrolling = b;
}

PUBLIC bool TextArea::isScrolling() 
{
	return mScrolling;
}

PUBLIC bool TextArea::isFocusable() 
{
	return true;
}

PUBLIC void TextArea::dumpLocal(int indent)
{
    dumpType(indent, "TextArea");
}

/**
 * Overloaded from Component because we don't want TAB taking
 * away input focus.
 */
PUBLIC void TextArea::processTab()
{
}

/**
 * Overloaded from Component because we don't want RETURN 
 * in a text area to close the dialog.
 */
PUBLIC bool TextArea::processReturn()
{
	return false;
}

/**
 * Overload this from Text so we can a more specific UI.
 */
PUBLIC void TextArea::open()
{
    ComponentUI* ui = getUI();
    ui->open();
}

PUBLIC Dimension* TextArea::getPreferredSize(class Window* w)
{
	if (mPreferred == NULL) {
		mPreferred = new Dimension();
        ComponentUI* ui = getUI();
        ui->getPreferredSize(w, mPreferred);
	}
	return mPreferred;
}

QWIN_END_NAMESPACE

/****************************************************************************/
/****************************************************************************
 *                                                                          *
 *                                  WINDOWS                                 *
 *                                                                          *
 ****************************************************************************/
/****************************************************************************/

#ifdef _WIN32
#include <windows.h>
#include "UIWindows.h"
QWIN_BEGIN_NAMESPACE

PUBLIC WindowsTextArea::WindowsTextArea(TextArea* ta) :
    WindowsText(ta)
{
}

PUBLIC WindowsTextArea::~WindowsTextArea()
{
}

PUBLIC void WindowsTextArea::open()
{
	if (mHandle == NULL) {
        HWND parent = getParentHandle();
        if (parent != NULL) {
            TextArea* area = (TextArea*)mText;

            // PS_PUSHBUTTON and PS_DEFPUSHBUTTON are the same in
            // non-dialog windows except that DEF has a heavier outline

            DWORD style = getWindowStyle() | WS_GROUP | WS_TABSTOP |
                WS_BORDER | ES_LEFT | ES_MULTILINE;

            // ideally, should create this without scrollbars, trap
            // the EN_ERRSPACE message and add them dynamically

            if (area->isScrolling())
              style |= (WS_HSCROLL | WS_VSCROLL | ES_AUTOHSCROLL | ES_AUTOVSCROLL);

            // add ES_NOHIDESEL if you want the selection to remain highlighted
            // when the control doesn't have focus

            Point p;
            mText->getNativeLocation(&p);
            Bounds* b = mText->getBounds();

            mHandle = CreateWindow("edit",
                                   NULL,
                                   style,
                                   p.x, p.y, b->width, b->height,
                                   parent,
                                   0,
                                   NULL,
                                   NULL);

            if (mHandle == NULL)
              printf("Unable to create TextArea control\n");
            else {
                subclassWindowProc();
                SetWindowLong(mHandle, GWL_USERDATA, (LONG)this);
				mText->initVisibility();
				// now set the real text
				setText(mText->getInitialText());
            }
        }
    }
}

/**
 * Petzold suggests that buttons look best when their height
 * is 7/4 times the height of a SYSTEM_FONT character.  The
 * width must accomodate at least the text plus two characters.
 */
PUBLIC void WindowsTextArea::getPreferredSize(Window* w, Dimension* d)
{
    TextArea* area = (TextArea*)mText;
    TextMetrics* tm = w->getTextMetrics();

    // the scroll bars will consume some of this space,
    // need to get their metrics and factor them into the size!!

    int cols = mText->getColumns();
    if (cols == 0) cols = 1;
    d->width = cols * tm->getMaxWidth();

    int fontHeight = tm->getHeight() + tm->getExternalLeading();
    int rows = area->getRows();
    if (rows == 0) rows = 1;
    int height = rows * fontHeight;
    // extra half character when using border
    d->height = height + (fontHeight / 2);
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

PUBLIC MacTextArea::MacTextArea(TextArea* ta) :
    MacText(ta)
{
}

PUBLIC MacTextArea::~MacTextArea()
{
}

PUBLIC void MacTextArea::open()
{
	// On Window we add scroll bars, don't have that here. 
	// The only difference between 
	MacText::open();

	// MacText left this on, we need it off
	Boolean singleLine = false;
	OSErr err = SetControlData((ControlRef)mHandle, kControlEditTextPart, 
							   kControlEditTextSingleLineTag, 
							   sizeof(singleLine), &singleLine);

	CheckErr(err, "MacTextArea::kControlEditTextSingleLineTag");
}

/**
 * We override the default to calculate a size based on the 
 * desired number of columns times the maximum char width.
 * Max char width was calculated as the "em width".
 * 
 * I guess we could have done all this in open().
 */
PUBLIC void MacTextArea::getPreferredSize(Window* w, Dimension* d)
{
	// this will deal with the width
	MacText::getPreferredSize(w, d);

	int rows = ((TextArea*)mText)->getRows();
	if (rows < 1) rows = 1;
	d->height = mHeight * rows;
}

QWIN_END_NAMESPACE
#endif

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

