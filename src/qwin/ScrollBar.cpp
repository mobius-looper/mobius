/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * WINDOWS NOTES
 *
 * These send VM_VSCROLL and VM_HSCROLL messages rather than VM_COMMAND.
 * You can differentiate between window scroll bars and 
 * scroll bar controls with the lParam value.  lParam will be 0
 * for window scroll bars and the window handle for controls.
 *
 * To create scroll bars that have the same dimensions as the 
 * window scroll bars, use:
 *
 *    GetSystemMetrics(SM_CYHSCROLL);  // height of horizontal bar
 *    GetSystemMetrics(SM_CXVSCROLL);  // width of vert vertical bar
 *
 * These style options can be used to give standard dimensions
 * to scroll bars, but work only in dialog boxes.
 *
 *   SBS_LEFTALIGH
 *   SBS_RIGHTALIGH
 *   SBS_TOPALIGH
 *   SBS_BOTTOMALIGH
 * 
 * There are a few other styles defined in later Windows releases.
 * You can get XP rendering with InitCommonControls, explore.
 * 
 * The system color COLOR_SCROLLBAR is not used.  The end buttons and
 * thumb are COLOR_BTNFACE, COLOR_BTNHIGHLIGHT, COLOR_BTNSHADOW,
 * COLOR_BTNTEXT (for the little arrows), COLOR_BTNSHADOW, 
 * and COLOR_BTNLIGHT.  The large area between the two
 * end buttons is a combination of COLOR_BTNFACE and COLOR_BTNHIGHLIGHT.
 *
 * You can trap WM_CTLCOLORSCROLLBAR messages and return a brush to 
 * override the color in the large area.
 *
 * More control is provided with the SCROLLINFO structure, but the
 * additional parameters seem a bit obscure.
 * 
 */
#include <stdio.h>
#include <memory.h>

#include "Util.h"
#include "Trace.h"
#include "Qwin.h"
#include "UIManager.h"

QWIN_BEGIN_NAMESPACE

/****************************************************************************
 *                                                                          *
 *                                 SCROLL BAR                               *
 *                                                                          *
 ****************************************************************************/

PUBLIC ScrollBar::ScrollBar() 
{
    initScrollBar();
}

PUBLIC ScrollBar::ScrollBar(int min, int max) 
{
    initScrollBar();
    mMinimum = min;
    mMaximum = max;
}

PRIVATE void ScrollBar::initScrollBar()
{
	mClassName = "ScrollBar";
    mVertical = false;
    mMinimum = 0;
    mMaximum = 255;
    mValue = 0;
    mPageSize = 10;
	mSlider = false;
}

PUBLIC ScrollBar::~ScrollBar() 
{
}

PUBLIC ComponentUI* ScrollBar::getUI()
{
	if (mUI == NULL)
	  mUI = UIManager::getScrollBarUI(this);
	return mUI;
}

PUBLIC ScrollBarUI* ScrollBar::getScrollBarUI()
{
	return (ScrollBarUI*)getUI();
}

/**
 * must be set before the native component is created
 */
PUBLIC void ScrollBar::setVertical(bool b) 
{
	mVertical = b;
}

PUBLIC bool ScrollBar::isVertical()
{
    return mVertical;
}

PUBLIC void ScrollBar::setSlider(bool b) 
{
	mSlider = b;
}

PUBLIC bool ScrollBar::isSlider()
{
    return mSlider;
}

PUBLIC bool ScrollBar::isFocusable() 
{
	return true;
}

PUBLIC void ScrollBar::setPageSize(int units) {
    // will want to be smarter about auto adjusting max/min...
    if (units > 0) {
        mPageSize = units;
        if (mPageSize > mMaximum)
          mMaximum = mPageSize;
        updateUI();
    }
}

PUBLIC int ScrollBar::getPageSize()
{
    return mPageSize;
}

PRIVATE void ScrollBar::updateUI() 
{
    ScrollBarUI* ui = getScrollBarUI();
    ui->update();
}

PUBLIC void ScrollBar::setMinimum(int i) {  
    if (i > 0) {
        mMinimum = i;
        updateUI();
    }
}

PUBLIC int ScrollBar::getMinimum()
{
    return mMinimum;
}

PUBLIC void ScrollBar::setMaximum(int i) 
{
    // reconcile with page size!!
    mMaximum = i;
    updateUI();
}

PUBLIC int ScrollBar::getMaximum()
{
    return mMaximum;
}

PUBLIC void ScrollBar::setRange(int min, int max) 
{
    mMinimum = min;
    mMaximum = max;
    // reconcile with page size!!
    updateUI();
}

PUBLIC void ScrollBar::setValue(int i) 
{
    mValue = i;
    updateUI();
}

/**
 * Only to be called by the UI in response to a scroll
 * bar event.
 */
void ScrollBar::updateValue(int i) 
{
    mValue = i;
	fireActionPerformed();
}

PUBLIC int ScrollBar::getValue()
{
    // assume we've been tracking
    return mValue;
}

/**
 * Unlike Swing, we'll allow a partial setting of the sizes
 * so always check for zero rather than just the presence
 * of a non-null Dimension.  Perhaps this is more like
 * having an implicit minimum size?
 */
PUBLIC Dimension* ScrollBar::getPreferredSize(Window* w)
{
	if (mPreferred == NULL)
		mPreferred = new Dimension();

    ComponentUI* ui = getUI();
    ui->getPreferredSize(w, mPreferred);

	return mPreferred;
}

PUBLIC void ScrollBar::dumpLocal(int indent)
{
    dumpType(indent, "ScrollBar");
}

PUBLIC void ScrollBar::open()
{
    ComponentUI* ui = getUI();
    ui->open();
}

QWIN_END_NAMESPACE

/****************************************************************************
 *                                                                          *
 *   							   WINDOWS                                  *
 *                                                                          *
 ****************************************************************************/

#ifdef _WIN32
#include <windows.h>
#include "UIWindows.h"
QWIN_BEGIN_NAMESPACE

PUBLIC WindowsScrollBar::WindowsScrollBar(ScrollBar* sb) 
{
	mScrollBar = sb;
}

PUBLIC WindowsScrollBar::~WindowsScrollBar() 
{
}

PUBLIC void WindowsScrollBar::update()
{
    if (mHandle != NULL) {
        SCROLLINFO info;
        info.cbSize = sizeof(info);
        // add SIF_DISABLENOSCROLL if you want the bar disabled
        // if the new settings make it unnecessary
        info.fMask = SIF_PAGE | SIF_POS | SIF_RANGE;
        info.nMin = mScrollBar->getMinimum();
        info.nMax = mScrollBar->getMaximum();
        info.nPos = mScrollBar->getValue();
        info.nPage = mScrollBar->getPageSize();
        info.nTrackPos = 0;
        SetScrollInfo(mHandle, SB_CTL, &info, true);
    }
}

PUBLIC void WindowsScrollBar::open()
{
	if (mHandle == NULL) {
		HWND parent = getParentHandle();
        if (parent != NULL) {
            // these must be presized, though if we have text
            // could presize them as if this were a label

            DWORD style = getWindowStyle() | WS_GROUP | WS_TABSTOP;

            // need a bunch of options or just style bits?
            if (mScrollBar->isVertical())
              style |= SBS_VERT;
            else
              style |= SBS_HORZ;

            Bounds* b = mScrollBar->getBounds();
            Point p;
            mScrollBar->getNativeLocation(&p);

            mHandle = CreateWindow("scrollbar",
                                   NULL,
                                   style,
                                   p.x, p.y, b->width, b->height,
                                   parent,
                                   0,
                                   NULL,
                                   NULL);

            if (mHandle == NULL)
              printf("Unable to create ScrollBar control\n");
            else {
                subclassWindowProc();
                SetWindowLong(mHandle, GWL_USERDATA, (LONG)this);
				mScrollBar->initVisibility();
                update();
            }
        }
    }
}

PUBLIC void WindowsScrollBar::getPreferredSize(Window* w, Dimension* d)
{
	// Unlike Swing, we'll allow a partial setting of the sizes
	// so always check for zero rather than just the presence
	// of a non-null Dimension.  Perhaps this is more like
	// having an implicit minimum size?
	
	if (mScrollBar->isVertical()) {
		int minwidth = UIManager::getVertScrollBarWidth();
		if (d->width < minwidth)
		  d->width = minwidth;
		if (d->height == 0) {
			// 3x to give some space for the thumb
			d->height = UIManager::getVertScrollBarHeight() * 3;
		}
	}
	else {
		if (d->height == 0)
			d->height = UIManager::getHorizScrollBarHeight();
		if (d->width == 0)
			d->width = UIManager::getHorizScrollBarWidth();
	}

	// no insets on these
}

/**
 * Called by the default window proc when it gets a WM_HSCROLL
 * or WM_VSCROLL message.
 */
PUBLIC void WindowsScrollBar::scroll(int code) 
{
    bool setValue = true;
    bool setPosition = true;
    int request = LOWORD(code);
    int value = HIWORD(code);
	int current = mScrollBar->getValue();

    // docs also indicate there is SB_LEFT and SB_RIGHT
    // for "scroll to upper left", etc.  Not sure what those are used for

    switch (request) {

        case SB_LINELEFT:   // same as SB_LINEUP
            // decrement by one unit
            value = current - 1;
            break;

        case SB_LINERIGHT: // same as SB_LINEDOWN
            // increment by one unit
            value = current + 1;
            break;

        case SB_PAGELEFT: // same as SB_PAGEUP
            // decrement by number of units in window
			value = current - mScrollBar->getPageSize();
            break;

        case SB_PAGERIGHT: // same as SB_PAGEDOWN
            // increment by number of units in window
			value = current + mScrollBar->getPageSize();
            break;

        case SB_THUMBTRACK:
            // user is dragging the thumb, see HIWORD for new value
            setPosition = false;
            break;

        case SB_THUMBPOSITION:
            // user is finished dragging the thumb, see HIWORD for new value
            // since we fired events as THUMBTRACK was happening,
            // don't need one here, probably want to conditionalize this
            setValue = false;
            break;

        case SB_ENDSCROLL:
            // end scrolling
            setValue = false;
            setPosition = false;
            break;
    }

	int min = mScrollBar->getMinimum();
	int max = mScrollBar->getMaximum();

	if (value < min)
	  value = min;
	else if (value > max)
	  value = max;

    if (setPosition) 
      SetScrollPos(mHandle, SB_CTL, value, true);
    
    if (setValue) 
	  mScrollBar->updateValue(value);

}

PUBLIC Color* WindowsScrollBar::colorHook(Graphics* g)
{
    return mScrollBar->getBackground();
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

PUBLIC MacScrollBar::MacScrollBar(ScrollBar* sb) 
{
	mScrollBar = sb;
}

PUBLIC MacScrollBar::~MacScrollBar() 
{
}

PUBLIC void MacScrollBar::update()
{
    if (mHandle != NULL) {
		int value = mScrollBar->getValue();
		SetControl32BitValue((ControlRef)mHandle, value);
		// !! TODO: Not tracking changes to the min/max value, 
		// this is okay for Mobius but would be necessary if we ever
		// wanted to use these as true scroll bars.
		// SetControl32BitMinimum/Maximum may work...
    }
}

/**
 * Registered action handler for the scroll bar control.
 */
PUBLIC void ScrollBarAction(ControlRef control, ControlPartCode code)
{
	MacScrollBar* msb = (MacScrollBar*)GetControlReference(control);
	if (msb == NULL)
	  Trace(1, "ScrollBarAction: unresolved MacScrollBar\n");
	else
	  msb->moved();
}

PUBLIC void MacScrollBar::moved()
{
	if (mHandle != NULL) {
		int value = GetControl32BitValue((ControlRef)mHandle);
		// this will cache the value and fire action perfomred
		mScrollBar->updateValue(value);
	}
}

PUBLIC void MacScrollBar::open()
{
	OSStatus status;
	OSErr err;

	WindowRef window = getWindowRef();

	if (mHandle == NULL && window != NULL) {

		ControlRef control;
		Rect bounds = { 0, 0, 0, 0 };
		
		// until we can figure out how to control orientation
		// of scroll bars, always use a slider
		//bool slider = mScrollBar->isSlider();
		bool slider = true;

		if (slider) {
			// can use kControlSliderPointsDownOrRight
			// kControlSliderPointsUpOrLeft
			status = CreateSliderControl(window, &bounds, 
										 mScrollBar->getValue(),
										 mScrollBar->getMinimum(),
										 mScrollBar->getMaximum(),
										 kControlSliderDoesNotPoint,
										 0, // numTickMarks
										 true, // liveTracking
										 NewControlActionUPP(ScrollBarAction),
										 &control);
		}
		else {
			// if zero creates a non-proportional scroll bar thumb
			// ?? how do you set vert vs horizontal
			int viewSize = 0;
			status = CreateScrollBarControl(window, &bounds, 
											mScrollBar->getValue(),
											mScrollBar->getMinimum(),
											mScrollBar->getMaximum(),
											viewSize,
											true, // liveTracking
											NewControlActionUPP(ScrollBarAction),
											&control);
		}

		if (CheckStatus(status, "MacListBox::open")) {
			mHandle = control;

			SetControlReference(control, (SInt32)this);

			SetControlVisibility(control, true, true);
		}
	}
}

/**
 * This is implemented like Windows with some magic constants
 * for bar width and height.
 *
 * Note that the Dimension may already be initialized so 
 * only make it bigger.
 */
PUBLIC void MacScrollBar::getPreferredSize(Window* w, Dimension* d)
{
	if (mScrollBar->isVertical()) {
		int minwidth = UIManager::getVertScrollBarWidth();
		if (d->width < minwidth)
		  d->width = minwidth;
		if (d->height == 0) {
			// 3x to give some space for the thumb
			d->height = UIManager::getVertScrollBarHeight() * 3;
		}
	}
	else {
		if (d->height == 0)
			d->height = UIManager::getHorizScrollBarHeight();
		if (d->width == 0)
			d->width = UIManager::getHorizScrollBarWidth();
	}

	// no insets on these
}

QWIN_END_NAMESPACE
#endif

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

