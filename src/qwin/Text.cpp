/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Corresponds to the JTextField component, but I didn't feel
 * like dragging over the "Field" appendage.
 *
 */

#include <stdio.h>
#include <memory.h>
#include <string.h>

#include "Util.h"
#include "Trace.h"
#include "Qwin.h"
#include "UIManager.h"

QWIN_BEGIN_NAMESPACE

/****************************************************************************
 *                                                                          *
 *   								 TEXT                                   *
 *                                                                          *
 ****************************************************************************/

PUBLIC Text::Text() 
{
    initText();
}

PUBLIC Text::Text(const char *s) 
{
    initText();
	setText(s);
}

PRIVATE void Text::initText()
{
	mClassName = "Text";
	mText = NULL;
    mColumns = 20;
    mEditable = true;
}

PUBLIC Text::~Text() 
{
    delete mText;
}

PUBLIC ComponentUI* Text::getUI()
{
    if (mUI == NULL)
      mUI = UIManager::getTextUI(this);
	return mUI;
}

PUBLIC TextUI* Text::getTextUI()
{
	return (TextUI*)getUI();
}

PUBLIC void Text::setEditable(bool b)
{
    mEditable = b;
    TextUI* ui = getTextUI();
    ui->setEditable(b);
}

PUBLIC bool Text::isEditable()
{
    return mEditable;
}

PUBLIC void Text::setColumns(int i) 
{
    mColumns = i;
}

PUBLIC int Text::getColumns() 
{
    return mColumns;
}

/**
 * This is intended only for the native peer
 * to get the initial text to display.
 */
PUBLIC const char* Text::getInitialText()
{
	return mText;
}

PUBLIC const char* Text::getText() 
{
    TextUI* ui = getTextUI();
    if (ui->isOpen()) {
        delete mText;
        mText = ui->getText();
    }
	return mText;
}

/**
 * For consistency with most other components.
 */
PUBLIC const char* Text::getValue()
{
	return getText();
}

/**
 * For consistency with most other components.
 */
PUBLIC void Text::setValue(const char* s)
{
	setText(s);
}

PUBLIC void Text::setText(const char *s) 
{
	if (mText != s) {
		delete mText;
		mText = CopyString(s);
	}
    TextUI* ui = getTextUI();
    ui->setText(s);
}

PUBLIC Dimension* Text::getPreferredSize(Window* w)
{
	if (mPreferred == NULL) {
		mPreferred = new Dimension();
        ComponentUI* ui = getUI();
        ui->getPreferredSize(w, mPreferred);
	}
	return mPreferred;
}

PUBLIC void Text::dumpLocal(int indent)
{
    dumpType(indent, "Text");
}

PUBLIC void Text::open()
{
    ComponentUI* ui = getUI();
    ui->open();
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

PUBLIC WindowsText::WindowsText(Text* t)
{
    mText = t;
}

PUBLIC WindowsText::~WindowsText() 
{
}

PUBLIC void WindowsText::setEditable(bool b)
{
    if (mHandle != NULL)
      SendMessage(mHandle, EM_SETREADONLY, ((b) ? FALSE : TRUE), 0);
}

PUBLIC char *WindowsText::getText() 
{
    char* text = NULL;
	if (mHandle != NULL) {
		int chars = GetWindowTextLength(mHandle);
		if (chars > 0) {
			text = new char[chars + 1];
			// third arg is size INCLUDING the null character
			GetWindowText(mHandle, text, chars + 1);
			text[chars] = '\0';
		}
	}
	return text;
}

PUBLIC void WindowsText::setText(const char *s) 
{
	if (mHandle != NULL) {
		// same as sending th WM_SETTEXT message
		// not sure if this handles NULL
		if (s == NULL)
		  SetWindowText(mHandle, "");
		else
		  SetWindowText(mHandle, s);
	}
}

/**
 * Petzold suggests that buttons look best when their height
 * is 7/4 times the height of a SYSTEM_FONT character.  The
 * width must accomodate at least the text plus two characters.
 */
PUBLIC void WindowsText::getPreferredSize(Window* w, Dimension* d)
{
	int cols = mText->getColumns();
	const char* text = mText->getInitialText();
	TextMetrics* tm = w->getTextMetrics();

	if (cols == 0 && text != NULL)
	  cols = strlen(text);
	d->width = cols * tm->getMaxWidth();

	// 1 1/2 times char height if using border
	int height = tm->getHeight() + tm->getExternalLeading();
	d->height = height + (height / 2);
}

PUBLIC void WindowsText::open()
{
	if (mHandle == NULL) {
        HWND parent = getParentHandle();
        if (parent != NULL) {

            // PS_PUSHBUTTON and PS_DEFPUSHBUTTON are the same in
            // non-dialog windows except that DEF has a heavier outline

            DWORD style = getWindowStyle() | WS_GROUP | WS_TABSTOP |
                WS_BORDER | ES_LEFT;

            if (!mText->isEditable())
              style |= ES_READONLY;

            Point p;
            mText->getNativeLocation(&p);
            Bounds* b = mText->getBounds();

			// these seem to come out a little larger than we asked for
			int height = b->height - 2;

            mHandle = CreateWindow("edit",
                                   NULL,
                                   style,
                                   p.x, p.y, b->width, height,
                                   parent,
                                   0,
                                   NULL,
                                   NULL);

            if (mHandle == NULL)
              printf("Unable to create Text control\n");
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
 * Called by the WindowsWindowUI event loop.
 *
 * EN_ERRSPACE is supposed to be sent if the control can't allocate
 * enough memory, apparently this happens if you try to go beyond 32K of text.
 * 
 * EN_MAXTEXT is supposed to be sent if you go beyond the specififed
 * maximum number of characters or lines, I'm not seeing this.
 *
 * EN_SETFOCUS, EN_KILLFOCUS - focus changed
 * EN_HSCROLL, EN_VSCROLL - scroll bar clicked
 * EN_CHANGE - contents will change
 * EN_UPDATE - contents have changed
 * 
 */
PUBLIC void WindowsText::command(int code) 
{
	switch (code) {

		case EN_KILLFOCUS: {
			// treat loss of focus as an implicit change
			mText->fireActionPerformed();
		}
		break;

		case EN_ERRSPACE:
		case EN_MAXTEXT:
		{
            HWND window = getWindowHandle(mText);
			MessageBox(window, "Edit control out of space",
					   "Warning", MB_OK | MB_ICONSTOP);
		}
		break;

		case EN_CHANGE: {
		}

		case EN_UPDATE: {
			// note that this happens for every key, simplify by
			// notifying the listeners only when the return key is pressed,
			// hmm, this also closes the dialog so may not want that
		}
		break;
	}
}

/**
 * Overload WindowsComponentUI's messageHandler to handle some
 * extra events relevant only for text fields.
 */
PUBLIC long WindowsText::messageHandler(UINT msg, WPARAM wparam, 
                                          LPARAM lparam)
{
	if (msg == WM_KEYDOWN) {
		if (wparam == 'A') {
			if (GetKeyState(VK_CONTROL) < 0) {
				// ctrl-a, select all text
				int len = GetWindowTextLength(mHandle);
				SendMessage(mHandle, EM_SETSEL, 0, len);
			}
		}
		else if (wparam == VK_RETURN)
		  mText->fireActionPerformed();
	}
	return WindowsComponent::messageHandler(msg, wparam, lparam);
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

PUBLIC MacText::MacText(Text* t)
{
    mText = t;
	mHeight = 0;
	mEmWidth = 0;
}

PUBLIC MacText::~MacText() 
{
}

PUBLIC void MacText::setEditable(bool b)
{
}

PUBLIC char *MacText::getText() 
{
	OSErr err;
	char* result = NULL;

	if (mHandle != NULL) {
		CFStringRef cfstring = NULL;
		err = GetControlData((ControlRef)mHandle, 0, kControlEditTextCFStringTag, 
							 sizeof(cfstring), &cfstring, NULL);
		CheckErr(err, "MacText::getText GetControlData\n");

		if (cfstring != NULL) {
			// this supposedly works sometimes but I've never seen it
			const char* cstr = CFStringGetCStringPtr(cfstring, kCFStringEncodingUTF8);
			if (cstr != NULL)
			  result = CopyString(cstr);
			else { 
				// have to fall back to this
				// Trying to allocate the correct size is inaccurate, since
				// we don't know how many bytes a given UTF-16 will blow into
				// when renderd as UTF-8.  Assume double, but that may not always
				// be enough for Asian langs.
				int len = CFStringGetLength(cfstring);
				if (len == 0) {
					// empty string, leave result null
				}
				else {
					len *= 2;
					result = new char[len + 1];
					Boolean success = CFStringGetCString(cfstring, result, len,
														 kCFStringEncodingUTF8);
					if (!success)
					  Trace(1, "MacText::getText string truncation!\n");
				}
			}

			// from carbondev Wiki...
			// must release after GetControlData w/kControlEditTextCFStringTag
			// this is exception to CF naming rule for Get & Copy functions.
			CFRelease(cfstring);  
		}
	}
	return result;
}

/**
 * NB: We're using a custom message to change the text to make
 * sure that it gets done in the UI thread.  This was necessary
 * for the Mobius MIDI Control window that wants to change text 
 * fields when "capture" is on.  These changes come in 
 * on the MIDI handler thread and can cause a crash if the UI
 * thread is still processing an invalidate() request from the
 * last MIDI message.
 *
 * So we don't have to mess with copying the string again we assume
 * that the desired text will always be the current value in the
 * Text peer.  The passed string is ignored but we have to do that
 * to meet the UIText interface.
 */
PUBLIC void MacText::setText(const char *s) 
{
	if (mHandle != NULL) {
		bool doAsync = true;
		if (doAsync) {
			// avoid sending the string in the message so we 
			// con't have to copy it, just let the message handler
			// get whatever is in the Text peer which is always
			// what "s" will be.
			sendChangeRequest(0, NULL);
		}
		else {
			setTextNow();
			mText->invalidate();
		}
	}
}

PUBLIC void MacText::handleChangeRequest(int type, void* value)
{
	setTextNow();

	// need to invalidate to see changes
	// don't use mText->invalidate() which will send another message,
	// since we know we're in the UI thread call invalidateNative
	// directly
	invalidateNative(mText);
}

PUBLIC void MacText::setTextNow() 
{
	// use whatever was left here
	const char* s = mText->getInitialText();

	if (s == NULL) {
		SetControlData((ControlRef)mHandle,
					   kControlEntireControl,
					   kControlEditTextTextTag,
					   0, 
					   NULL);
	}
	else {
		CFStringRef cfstring = MakeCFStringRef(s);
		SetControlData((ControlRef)mHandle,
					   kControlEntireControl,
					   //kControlEditTextTextTag,
					   kControlStaticTextCFStringTag,
					   sizeof(cfstring), 
					   &cfstring);
		// !! I think we need to release the string
	}
}

/**
 * We get a Click when the mouse button goes down and a Hit when it goes up.
 * These aren't usable as action events since we haven't done anything yet.
 * Activate/Deactivate happen when the entire window loses focus.
 * 
 * Tab will remove focus and we get SetFocusPart messages but I'm not sure
 * how to dig out the gained/lost status.
 * 
 */
PRIVATE EventTypeSpec TextEventsOfInterest[] = {
	{ kEventClassControl, kEventControlSetFocusPart },
	//{ kEventClassControl, kEventControlHit },
	//{ kEventClassControl, kEventControlClick },
	//{ kEventClassTextInput, kEventTextInputUpdateActiveInputArea },
	//{ kEventClassTextInput, kEventTextInputUnicodeForKeyEvent },
};

PRIVATE OSStatus
TextEventHandler(EventHandlerCallRef caller, EventRef event, void* data)
{
    OSStatus result = eventNotHandledErr;

	int cls = GetEventClass(event);
	int kind = GetEventKind(event);

	//TraceEvent("Text", cls, kind);

	if (cls == kEventClassControl) {
		if (kind == kEventControlSetFocusPart) {
			// TODO: dig out the gained/lost state and fire on lost!!
		}
	}

	return result;
}

PUBLIC void MacText::fireActionPerformed()
{
	mText->fireActionPerformed();
}

PUBLIC void MacText::open()
{
	OSStatus status;
	OSErr err;

	WindowRef window = getWindowRef();

	if (mHandle == NULL && window != NULL) {

		ControlRef control;
		Rect bounds = { 0, 0, 0, 0 };

		// Note that we create the component with a single M to 
		// get the "em width" for later sizing by column count.  There
		// might be a way to do that using font manager but I couldn't find one.
		CFStringRef cftext = MakeCFStringRef("M");

		status = CreateEditUnicodeTextControl(window, &bounds, cftext, 
											  false, // password
											  NULL, // ControlFontStyleRec
											  &control);

		if (CheckStatus(status, "Mactext::open")) {
			mHandle = control;

			status = InstallControlEventHandler(control,
												NewEventHandlerUPP(TextEventHandler),
												GetEventTypeCount(TextEventsOfInterest),
												TextEventsOfInterest,
												this, NULL);

			CheckStatus(status, "MacText::InstallEventHandler");
			SetControlVisibility(control, true, false);

			// possible control data
			//kControlEditTextLockedTag - make it read-only


			Boolean singleLine = true;
			err = SetControlData(control, kControlEditTextPart, 
								 kControlEditTextSingleLineTag, 
								 sizeof(singleLine), &singleLine);

			CheckErr(err, "MacText::kControlEditTextSingleLineTag");

			// Ask for default BestControlRect size
			Window* w = mText->getWindow();
			Dimension d;
			MacComponent::getPreferredSize(w, &d);
			mHeight = d.height;
			mEmWidth = d.width;

			// now set the real text
			setText(mText->getInitialText());
		}
    }
}

/**
 * We override the default to calculate a size based on the 
 * desired number of columns times the maximum char width.
 */
PUBLIC void MacText::getPreferredSize(Window* w, Dimension* d)
{
	int cols = mText->getColumns();
	const char* text = mText->getInitialText();

	if (cols == 0 && text != NULL)
	  cols = strlen(text);

	d->width = cols * mEmWidth;

	// this doesn't seem to be quite enough, add some border padding?
	d->width += 2;

	// We saved the default height in open() calculated by GetBestControlRect.
	// This isn't high enough, it seems to not include enough space for the
	// bottom border.  Highlighted border was measured at 5 pixels if you include
	// the full gradient, 4 would be enough.  Note that this has to be done
	// as a hidden inset, just making it taller isn't enough.

	d->height = mHeight + 8;
}

/**
 * Have to overload this because there is some kind of strange shadow shit
 * that occludes the bottom of the button.  Simply making it taller
 * doesn't help, have to add some extra invisible padding around the button.
 */
PUBLIC void MacText::adjustControlBounds(Rect* bounds)
{
	bounds->top += 4;
	bounds->bottom -= 4;
}

QWIN_END_NAMESPACE
#endif

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
