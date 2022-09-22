/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Component that renders as a checkbox.
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
 *   							   CHECKBOX                                 *
 *                                                                          *
 ****************************************************************************/

PUBLIC Checkbox::Checkbox() 
{
    init();
}

PUBLIC Checkbox::Checkbox(const char *s) 
{
    init();
	setText(s);
}

PUBLIC void Checkbox::init()
{
	mClassName = "Checkbox";
	mTriState = false;
}

PUBLIC Checkbox::~Checkbox()
{
}

PUBLIC void Checkbox::setTriState(bool b) 
{
	mTriState = b;
}

PUBLIC bool Checkbox::isTriState() 
{
	return mTriState;
}

PUBLIC ComponentUI* Checkbox::getUI()
{
    if (mUI == NULL)
      mUI = UIManager::getCheckboxUI(this);
    return mUI;
}

PUBLIC CheckboxUI* Checkbox::getCheckboxUI()
{
    return (CheckboxUI*)getUI();
}

PUBLIC void Checkbox::dumpLocal(int indent)
{
    dumpType(indent, "Checkbox");
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

PUBLIC WindowsCheckbox::WindowsCheckbox(Checkbox* cb)
{
    mCheckbox = cb;
}

PUBLIC WindowsCheckbox::~WindowsCheckbox()
{
}

PUBLIC void WindowsCheckbox::setSelected(bool b) 
{
    if (mHandle != NULL)
      SendMessage(mHandle, BM_SETCHECK, (int)b, 0);
}

PUBLIC bool WindowsCheckbox::isSelected() 
{
	bool selected = false;
    if (mHandle != NULL)
      selected = (SendMessage(mHandle, BM_GETCHECK, 0, 0) != 0);
	return selected;
}

/**
 * Petzold suggests that buttons look best when their height
 * is 7/4 times the height of a SYSTEM_FONT character.  
 * Not sure this applies to Checkbox/Radio buttons.
 *
 * The width must accomodate at least the text plus two characters.
 * 
 * Petzold says that the minimum height of the checkbox
 * is one character height and the minimum width is
 * the number of characters in the text plus two.  
 * It doesn't sound like the "box" will be scaled to fit the font
 * however, there must be absolute minimum sizes defined somewhere.
 * 
 * Just adding two doesn't seem to be enough to account for
 * both the check and the pad in front of the text.  Maybe
 * true string sizes would fix this?
 */
PUBLIC void WindowsCheckbox::getPreferredSize(Window* w, Dimension* d)
{
    const char* text = mCheckbox->getText();
	TextMetrics* tm = w->getTextMetrics();
	w->getTextSize(text, NULL, d);

	d->width += tm->getMaxWidth();
	if (text != NULL)
	  d->width += (2 * tm->getMaxWidth()); // padding

	int fontHeight = tm->getHeight() + tm->getExternalLeading();
	d->height = 7 * fontHeight / 4;
}

PUBLIC void WindowsCheckbox::open()
{
	if (mHandle == NULL) {
		// have to do this before we're open
		bool initialValue = mCheckbox->isSelected();

        HWND parent = getParentHandle();
        if (parent != NULL) {
            // If you use BS_CHECKBOX rather than BS_AUTOCHECKBOX
            // you have to set the check state explicitly
            // using SendMessage with BM_SETCHECK

            DWORD style = getWindowStyle() | WS_GROUP | WS_TABSTOP;
            if (mCheckbox->isTriState())
              style |= BS_AUTO3STATE;
            else
              style |= BS_AUTOCHECKBOX;

            Bounds* b = mCheckbox->getBounds();
            Point p;
            mCheckbox->getNativeLocation(&p);

            // !! what does mText do?
            mHandle = CreateWindow("button",
                                   mCheckbox->getText(),
                                   style,
                                   p.x, p.y, b->width, b->height,
                                   parent,
                                   0,
                                   NULL,
                                   NULL);

            if (mHandle == NULL)
              printf("Unable to create Checkbox control\n");
            else {
                subclassWindowProc();
                SetWindowLong(mHandle, GWL_USERDATA, (LONG)this);
				mCheckbox->initVisibility();
				setSelected(initialValue);
            }
        }
    }
}

/**
 * Interesting command codes for buttons:
 *
 * The usual code is BN_CLICKED.
 * There are 5 other codes for an obsolete button style
 * called BS_USERBUTTON but that should not be used.
 */
PUBLIC void WindowsCheckbox::command(int code) {

	if (code != BN_CLICKED) {
		printf("Checkbox::command unusual code %d\n", code);
	}
	else {
		mCheckbox->fireActionPerformed();
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
#include "MacUtil.h"

QWIN_BEGIN_NAMESPACE

PUBLIC MacCheckbox::MacCheckbox(Checkbox* cb)
{
    mCheckbox = cb;
}

PUBLIC MacCheckbox::~MacCheckbox()
{
}

PUBLIC void MacCheckbox::setSelected(bool b) 
{
	if (mHandle != NULL) {
		int ival = (b) ? 1 : 0;
		SetControl32BitValue((ControlRef)mHandle, ival);
		mCheckbox->invalidate();
	}
}

PUBLIC bool MacCheckbox::isSelected() 
{
	bool selected = false;
	if (mHandle != NULL) {
		int ival = GetControl32BitValue((ControlRef)mHandle);
		selected = (ival != 0);
	}
	return selected;
}

/**
 * We get a Click when the mouse button goes down and a Hit when it goes up.
 * We also get a Command/Process but it happens before the value changes.
 * Don't seem to get any Command events though the window does.
 */
PRIVATE EventTypeSpec CheckboxEventsOfInterest[] = {
	{ kEventClassControl, kEventControlHit },
};

PRIVATE OSStatus
CheckboxEventHandler(EventHandlerCallRef caller, EventRef event, void* data)
{
    OSStatus result = eventNotHandledErr;

	int cls = GetEventClass(event);
	int kind = GetEventKind(event);

	//TraceEvent("Checkbox", cls, kind);

	if (cls == kEventClassControl) {
		// for buttons, need to fire action handlers on Hit rather than Click
		// because the selection state doesn't change until Hit
		if (kind == kEventControlHit) {
			MacCheckbox* cb = (MacCheckbox*)data;
			if (cb != NULL)
			  cb->fireActionPerformed();
		}
	}

	return result;
}

PUBLIC void MacCheckbox::fireActionPerformed()
{
	mCheckbox->fireActionPerformed();
}

PUBLIC void MacCheckbox::open()
{
	OSStatus status;
	OSErr err;

	WindowRef window = getWindowRef();

	if (mHandle == NULL && window != NULL) {

		// have to do this before we're open
		bool initialValue = mCheckbox->isSelected();

		ControlRef control;
		Rect bounds = { 0, 0, 0, 0 };
		const char* text = mCheckbox->getText();
		CFStringRef cftext = MakeCFStringRef(text);

		status = CreateCheckBoxControl(window, &bounds, cftext, 
									   0, // initialValue
									   true, // autoToggle
									   &control);

		if (CheckStatus(status, "MacCheckbox::open")) {
			mHandle = control;

			status = InstallControlEventHandler(control,
												NewEventHandlerUPP(CheckboxEventHandler),
												GetEventTypeCount(CheckboxEventsOfInterest),
												CheckboxEventsOfInterest,
												this, NULL);

			CheckStatus(status, "MacCheckbox::InstallEventHandler");

			SetControlVisibility(control, true, false);

			setSelected(initialValue);
		}

    }
}


QWIN_END_NAMESPACE
#endif

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
