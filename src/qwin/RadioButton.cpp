/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
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
 *   							 RADIO BUTTON                               *
 *                                                                          *
 ****************************************************************************/

PUBLIC RadioButton::RadioButton() 
{
	init();
}

PUBLIC void RadioButton::init()
{
	mClassName = "RadioButton";
	mLeftText = false;
	mSelected = false;
	
	// this is a windows kludge, need to factor this down
	// into the UI model
	mGroup = false;
}

PUBLIC RadioButton::RadioButton(const char *s) 
{
	init();
	setText(s);
}

PUBLIC RadioButton::~RadioButton()
{
}

PUBLIC ComponentUI* RadioButton::getUI()
{
	if (mUI == NULL)
	  mUI = UIManager::getRadioButtonUI(this);
    return mUI;
}

/**
 * Sigh, have to implement this for AbstractButton.
 * Think more about inheritance...
 */
PUBLIC ButtonUI* RadioButton::getButtonUI()
{
	return (ButtonUI*)getUI();
}

PUBLIC RadioButtonUI* RadioButton::getRadioButtonUI()
{
	return (RadioButtonUI*)getUI();
}

PUBLIC void RadioButton::setGroup(bool b)
{
	mGroup = b;
}

PUBLIC bool RadioButton::isGroup()
{
    return mGroup;
}

PUBLIC void RadioButton::setLeftText(bool b) 
{
	mLeftText = b;
}

PUBLIC void RadioButton::setSelected(bool b) 
{
	mSelected = b;
    RadioButtonUI* ui = getRadioButtonUI();
    ui->setSelected(b);
}

PUBLIC void RadioButton::setValue(bool b)
{
	setSelected(b);
}

PUBLIC bool RadioButton::isSelected() 
{
    RadioButtonUI* ui = getRadioButtonUI();
	if (ui->isOpen())
	  mSelected = ui->isSelected();
	return mSelected;
}

PUBLIC bool RadioButton::getValue()
{	
	return isSelected();
}

PUBLIC Dimension* RadioButton::getPreferredSize(Window* w)
{
	if (mPreferred == NULL) {
		mPreferred = new Dimension();
        ComponentUI* ui = getUI();
        ui->getPreferredSize(w, mPreferred);
	}
	return mPreferred;
}

PUBLIC void RadioButton::open()
{
    ComponentUI* ui = getUI();
    ui->open();
}

PUBLIC void RadioButton::dumpLocal(int indent)
{
    dumpType(indent, "Radiobutton");
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

PUBLIC WindowsRadioButton::WindowsRadioButton() 
{
	mButton = NULL;
}

PUBLIC WindowsRadioButton::WindowsRadioButton(RadioButton* rb) 
{
	mButton = rb;
}

PUBLIC WindowsRadioButton::~WindowsRadioButton()
{
}

PUBLIC void WindowsRadioButton::setSelected(bool b) 
{
    if (mHandle != NULL)
      SendMessage(mHandle, BM_SETCHECK, (int)b, 0);
}

PUBLIC bool WindowsRadioButton::isSelected() 
{
	bool selected = false;
    if (mHandle != NULL)
      selected = (SendMessage(mHandle, BM_GETCHECK, 0, 0) != 0);
	return selected;
}

PUBLIC void WindowsRadioButton::open()
{
	if (mHandle == NULL) {
		// have to do this before we're open
		bool initialValue = mButton->isSelected();

		HWND parent = getParentHandle();
        if (parent != NULL) {
            // If you use BS_CHECKBOX rather than BS_AUTOCHECKBOX
            // you have to set the check state explicitly
            // using SendMessage with BM_SETCHECK

            DWORD style = getWindowStyle() | BS_AUTORADIOBUTTON;
		
            // need WS_TABSTOP?
            if (!mButton->isGroup())
              style |= WS_GROUP;

            Bounds* b = mButton->getBounds();
            Point p;
            mButton->getNativeLocation(&p);

            mHandle = CreateWindow("button",
                                   mButton->getText(),
                                   style,
                                   p.x, p.y, b->width, b->height,
                                   parent,
                                   0,
                                   NULL,
                                   NULL);

            if (mHandle == NULL)
              printf("Unable to create RadioButton control\n");
            else {
                subclassWindowProc();
                SetWindowLong(mHandle, GWL_USERDATA, (LONG)this);
				mButton->initVisibility();
				setSelected(initialValue);
            }
        }
    }
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
PUBLIC void WindowsRadioButton::getPreferredSize(Window* w, Dimension* d)
{
    const char* text = mButton->getText();
	TextMetrics* tm = w->getTextMetrics();
	w->getTextSize(text, NULL, d);

	d->width += tm->getMaxWidth();
	if (text != NULL)
	  d->width += (2 * tm->getMaxWidth()); // padding

	int fontHeight = tm->getHeight() + tm->getExternalLeading();
	d->height = 7 * fontHeight / 4;
}

/**
 * Interesting command codes for buttons:
 *
 * The usual code is BN_CLICKED.
 * There are 5 other codes for an obsolete button style
 * called BS_USERBUTTON but that should not be used.
 */
PUBLIC void WindowsRadioButton::command(int code) {

	if (code != BN_CLICKED) {
		printf("RadioButton::command unusual code %d\n", code);
	}
	else {
		mButton->fireActionPerformed();
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

PUBLIC MacRadioButton::MacRadioButton() 
{
	mButton = NULL;
}

PUBLIC MacRadioButton::MacRadioButton(RadioButton* rb) 
{
	mButton = rb;
}

PUBLIC MacRadioButton::~MacRadioButton()
{
}

PUBLIC void MacRadioButton::setSelected(bool b) 
{
	if (mHandle != NULL) {
		int ival = (b) ? 1 : 0;
		SetControl32BitValue((ControlRef)mHandle, ival);
	}
}

PUBLIC bool MacRadioButton::isSelected() 
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
 * Don't seem to get any Command events though the window does.
 */
PRIVATE EventTypeSpec RadioButtonEventsOfInterest[] = {
	{ kEventClassCommand, kEventCommandProcess },
	{ kEventClassControl, kEventControlHit },
	{ kEventClassControl, kEventControlHit },
	{ kEventClassControl, kEventControlClick },
};

PRIVATE OSStatus
RadioButtonEventHandler(EventHandlerCallRef caller, EventRef event, void* data)
{
    OSStatus result = eventNotHandledErr;

	int cls = GetEventClass(event);
	int kind = GetEventKind(event);

	//TraceEvent("RadioButton", cls, kind);

	if (cls == kEventClassControl) {
		// for buttons, need to fire action handlers on Hit rather than Click
		// because the selection state doesn't change until Hit
		if (kind == kEventControlHit) {
			MacRadioButton* rb = (MacRadioButton*)data;
			if (rb != NULL)
			  rb->fireActionPerformed();
		}
	}

	return result;
}

PUBLIC void MacRadioButton::fireActionPerformed()
{
	mButton->fireActionPerformed();
}

PUBLIC void MacRadioButton::open()
{
	OSStatus status;
	OSErr err;

	WindowRef window = getWindowRef();

	if (mHandle == NULL && window != NULL) {

		// have to do this before we're open
		bool initialValue = mButton->isSelected();

		ControlRef control;
		Rect bounds = { 0, 0, 0, 0 };
		const char* text = mButton->getText();
		CFStringRef cftext = MakeCFStringRef(text);

		// auto-toggle should be false if we're in a radio button group
		// which is the normal case, if not we behave just like checkboxes
		bool autoToggle = false;

		status = CreateRadioButtonControl(window, &bounds, cftext, 
										  0, // initialValue
										  autoToggle,
										  &control);

		if (CheckStatus(status, "MacRadioButton::open")) {
			mHandle = control;

			status = InstallControlEventHandler(control,
												NewEventHandlerUPP(RadioButtonEventHandler),
												GetEventTypeCount(RadioButtonEventsOfInterest),
												RadioButtonEventsOfInterest,
												this, NULL);

			CheckStatus(status, "MacRadioButton::InstallEventHandler");

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
