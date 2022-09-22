/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * There are three types of CustomButton, not all of these are
 * available in native buttons:
 *
 * normal - action fired when button goes down
 * momentary - action fires when button goes down and up
 * toggle - action fires when button goes down, push state toggles
 *
 * Toggle is new and not supported by native buttons.
 * 
 * When a button is not momentary you have a choice to fire actions
 * when the button is pressed or when it is released.  When
 * mImmediate is true the action fires on press, when false the
 * action fires on release (same as the mouseClicked event).
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
 *   						   ABSTRACT BUTTON                              *
 *                                                                          *
 ****************************************************************************/

PUBLIC AbstractButton::AbstractButton() 
{
	mText = NULL;
	mFont = NULL;
}

PUBLIC AbstractButton::AbstractButton(const char *s) 
{
	mText = CopyString(s);
}

PUBLIC AbstractButton::~AbstractButton() 
{
	delete mText;
}

PUBLIC const char* AbstractButton::getTraceName()
{
	return (mText != NULL)  ? mText : getName();
}

PUBLIC void AbstractButton::setText(const char *s) 
{
	delete mText;
	mText = CopyString(s);

	ButtonUI* ui = getButtonUI();
    ui->setText(mText);
}

PUBLIC const char* AbstractButton::getText()
{
    return mText;
}

PUBLIC void AbstractButton::setFont(Font* f)
{
	mFont = f;
}

PUBLIC Font* AbstractButton::getFont()
{
    return mFont;
}

/**
 * Programatically simulate the clicking of the button.
 */
PUBLIC void AbstractButton::click()
{
	ButtonUI* ui = getButtonUI();
    ui->click();
}

/****************************************************************************
 *                                                                          *
 *   								BUTTON                                  *
 *                                                                          *
 ****************************************************************************/

PUBLIC Button::Button() 
{
	initButton();
}

PUBLIC Button::Button(const char *s) 
{
	initButton();
	setText(s);
}

PUBLIC void Button::initButton()
{
	mClassName = "Button";
	mDefault = false;
	mImmediate = false;
	mMomentary = false;
    mToggle = false;
	mPushed = false;
    mTextColor = NULL;
	mOwnerDraw = false;
	mInvisible = false;
}

PUBLIC Button::~Button()
{
}

PUBLIC ComponentUI* Button::getUI()
{
	if (mUI == NULL)
	  mUI = UIManager::getButtonUI(this);
    return mUI;
}

PUBLIC ButtonUI* Button::getButtonUI()
{
	return (ButtonUI*)getUI();
}

bool Button::isFocusable() 
{
	return true;
}

Button* Button::isButton() 
{
	return this;
}

void Button::setDefault(bool b) 
{
	mDefault = b;
}

bool Button::isDefault() 
{
	return mDefault;
}

/**
 * If you set this, the subclass needs to overload the drawItem method.
 */
PUBLIC void Button::setOwnerDraw(bool b)
{
	mOwnerDraw = b;
}

PUBLIC bool Button::isOwnerDraw()
{
	return mOwnerDraw;
}

/**
 * When true the window will have no visible rendering, but it will
 * tell Windows that it has a size.  Simply setting the text to NULL
 * isn't enough because Windows appears to not call the event handler
 * if you programatically click on a button that has no size.
 *
 * This is used to work around an odd problem where we need to open
 * non-modal dialogs from the MobiusThread.  Opening them from the
 * thread makes the application hang, dialogs need to be opened from
 * the window event thread.  There is probably a better way to 
 * accomplish this, but this works well enough.
 */
PUBLIC void Button::setInvisible(bool b)
{
	mInvisible = b;
}

PUBLIC bool Button::isInvisible()
{
	return mInvisible;
}

PUBLIC void Button::setTextColor(Color* c)
{
    mTextColor = c;
}

PUBLIC Color* Button::getTextColor()
{
    return mTextColor;
}

PUBLIC void Button::setMomentary(bool b)
{
	mMomentary = b;
}

PUBLIC bool Button::isMomentary()
{
	return mMomentary;
}

PUBLIC void Button::setToggle(bool b)
{
	mToggle = b;
}

PUBLIC bool Button::isToggle()
{
	return mToggle;
}

PUBLIC void Button::setImmediate(bool b)
{
	mImmediate = b;
}

PUBLIC bool Button::isImmediate()
{
    return mImmediate;
}

/**
 * Only to be called by the UI for momentary buttons.
 */
PUBLIC void Button::setPushed(bool b)
{
    mPushed = b;
}

PUBLIC bool Button::isPushed()
{
    // since this makes sense only if it is momentary, and
    // you only call this to get the status of the button in an event
    // hander, return true for non-momentary buttons so they
    // can be handled the same
    // jsl - not sure why we did that but it screws up CustomButton
    // that needs to know push status in both cases to draw the text
    //return (mMomentary) ? mPushed : true;
    return mPushed;
}

PUBLIC Dimension* Button::getPreferredSize(Window* w)
{
	if (mPreferred == NULL) {
		mPreferred = new Dimension();

		if (mInvisible) {
			// we apparently have to have a non-zero size in order for 
			// windows to cause an event when we're programatically clicked
			// !! need to find a better way to do this
			mPreferred->width = 1;
			mPreferred->height = 1;
		}
		else {
            ComponentUI* ui = getUI();
			ui->getPreferredSize(w, mPreferred);
		}
	}
	return mPreferred;
}

PUBLIC void Button::dumpLocal(int indent)
{
    dumpType(indent, "Button");
}

PUBLIC void Button::open()
{
    getUI()->open();
}

PUBLIC void Button::paint(Graphics* g)
{
    getUI()->paint(g);
}

/****************************************************************************
 *                                                                          *
 *   						   INVISIBLE BUTTON                             *
 *                                                                          *
 ****************************************************************************/

PUBLIC InvisibleButton::InvisibleButton()
{
	initButton();
	setInvisible(true);

	// apparently this must have a non-zero size in order to make
	// windows generate an event if it is programatically clicked
	setText("Invisible");
	mClassName = "InvisibleButton";
}

bool InvisibleButton::isFocusable() 
{
	return false;
}

QWIN_END_NAMESPACE

/****************************************************************************/
/****************************************************************************
 *                                                                          *
 *   							  WINDOWS UI                                *
 *                                                                          *
 ****************************************************************************/
/****************************************************************************/

#ifdef _WIN32
#include <windows.h>
#include "UIWindows.h"
QWIN_BEGIN_NAMESPACE

WindowsButton::WindowsButton(Button * b)
{
	mButton = b;
}

WindowsButton::~WindowsButton()
{
}

void WindowsButton::setText(const char *s) 
{
	if (mHandle != NULL)
	  SetWindowText(mHandle, s);
}

/**
 * Programatically simulate the clicking of the button.
 */
void WindowsButton::click()
{
	if (mHandle != NULL)
		SendMessage(mHandle, BM_CLICK, 0, 0);
}

/**
 * Petzold suggests that buttons look best when their height
 * is 7/4 times the height of a SYSTEM_FONT character.  The
 * width must accomodate at least the text plus two characters.
 */
PUBLIC void WindowsButton::getPreferredSize(Window* w, Dimension* d)
{
    const char* text = mButton->getText();
    Font* font = mButton->getFont();

	w->getTextSize(text, font, d);

	// the "official" way to do this is to get a TEXTMETRIC
	// for the font and consume tmMaxCharWidth and tmExternalLeading
	Graphics* g = w->getGraphics();
	g->setFont(font);

	TextMetrics* tm = g->getTextMetrics();
		
	// getting strange variations in tmMaxCharWidth,
	// the default font as tmHeight=16 and tmMaxCharWidth=14,
	// but 8 pt ariel had a tmMaxCharWidth=27 !
	// use tmAveCharWidth, it seems to make more sense

	//mPreferred->width += 2 * tm->tmMaxCharWidth;
	d->width += 4 * tm->getAverageWidth();

	int fontHeight = tm->getHeight() + tm->getExternalLeading();
	d->height = 7 * fontHeight / 4;
}

/**
 * Windows Button styles include:
 * 
 *  BS_PUSHBUTTON
 *  BS_DEFPUSHBUTTON
 *  BS_CHECKBOX
 *  BS_AUTOCHECKBOX
 *  BS_3STATE
 *  BS_AUTO3STATE
 *  BS_GROUPBOX
 *  BS_AUTORADIOBUTTON
 *  BS_OWNERDRAW
 */
PUBLIC void WindowsButton::open()
{
	if (mHandle == NULL && !mButton->isOwnerDraw()) {
		HWND parent = getParentHandle();
        if (parent != NULL) {
            Bounds* b = mButton->getBounds();

            // PS_PUSHBUTTON and PS_DEFPUSHBUTTON are the same in
            // non-dialog windows except that DEF has a heavier outline
            DWORD style = getWindowStyle() | WS_GROUP | WS_TABSTOP;

            if (mButton->isOwnerDraw() || mButton->isInvisible())
              style |= BS_OWNERDRAW;
            else if (mButton->isDefault())
              style |= BS_DEFPUSHBUTTON;
            else
              style |= BS_PUSHBUTTON;

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
              printf("Unable to create Button control\n");
            else {
                //subclassWindowProc();
                SetWindowLong(mHandle, GWL_USERDATA, (LONG)this);
				mButton->initVisibility();
            }
        }
    }
}

/**
 * Called in response to a WM_CTLCOLORBTN message.
 * Doesn't appear to have any effect.
 * Not windows specific as written but the whole notion
 * of colorHook is, so keep it down here.
 */
PUBLIC Color* WindowsButton::colorHook(Graphics* g)
{
    Color* brush = NULL;

	Color* background = mButton->getBackground();
	if (background == NULL)
	  background = Color::ButtonFace;

	if (background != NULL) {
		g->setBackgroundColor(background);
		brush = background;
	}

	g->setFont(mButton->getFont());

    return brush;
}

/**
 * Interesting command codes for buttons:
 *
 * The usual code is BN_CLICKED.
 * There are 5 other codes for an obsolete button style
 * called BS_USERBUTTON but that should not be used.
 */
PUBLIC void WindowsButton::command(int code) {

	if (code != BN_CLICKED) {
		printf("Button::command unusual code %d\n", code);
	}
	else if (!mButton->isOwnerDraw() ||
			 (!mButton->isMomentary() && !mButton->isImmediate())) {

		//printf("Pressed '%s'\n", mText);
		//fflush(stdout);
        mButton->fireActionPerformed();
	}
}

/**
 * di->itemAction may have these values:
 * 
 * ODA_DRAWENTIRE		entire control needs to be drawn
 * ODA_FOCUS			lost or gained focus, see itemState
 * ODA_SELECT			selection status has changed
 *
 * di->itemState may have these values:
 *
 * ODS_CHECKED			menus only
 * ODS_COMBOBOXEDIT		draw in selection field of a combo box
 * ODS_DEFAULT			item is drawn as the default item
 * ODS_DISABLED			item is drawn disabled
 * ODS_FOCUS			item has keyboard focus
 * ODS_GRAYED			menus only, item is to be greyed
 * ODS_HOTLIGHT			item is being hot tracked
 * ODS_INACTIVE			item is inactive
 * ODS_NOACCEL			draw without keyboard accel cues
 * ODS_NOFOCUSRECCT		draw without focus indiciator cues
 * ODS_SELECTED			menus only, item is selected
 *
 * KLUDGE: Windows indiciates that owner draw buttons are drawn
 * by sending an WM_DRAWITEM event.  I didn't want to add
 * another Component method for this so instead we overload 
 * paint().  But its important that we NOT paint ourselves
 * when handling the WM_PAINT method, not sure why but it causes
 * problems.  So test for an LPDRAWITEMSTRUCT.
 *
 * !! This isn't very windows-specific so figure out away
 * to encapsulate LPDRAWITEMSTRUCT in the Graphics object.
 *
 * OwnerDraw buttons aren't used any more since Mac doesn't have them, 
 * use CustomButton instead.
 */
PUBLIC void WindowsButton::paint(Graphics* g)
{
	if (mButton->isOwnerDraw()) {
		mButton->tracePaint();
        WindowsGraphics* wg = (WindowsGraphics*)g;
		LPDRAWITEMSTRUCT di = wg->getDrawItem();

		int left = 0;
		int top = 0;
        if (di == NULL) {
            // when handling ownerdraw messages the origin is zero
            // for some reason, I guess because we're using the 
            // LPDRAWITEMSTRUCT'S HDC in Graphics
            left = mButton->getX();
            top = mButton->getY();
        }

		int width = mButton->getWidth();
		int height = mButton->getHeight();
						 
        // formerly required an LPDRAWITEMSTRUCT but now
        // we're trying to be a "pure" lightweight button

		if (mButton->isEnabled()) {

            bool selected = false;
            if (di != NULL) {
                // supposedly for menus only?
                selected = di->itemState & ODS_SELECTED;
            }
            else {
                // !! find a way
            }

			// clear the background
			g->setColor(mButton->getBackground());
			g->fillRect(left, top, width, height);

			// don't understand the algorithm, but this looks good
			int arcWidth = 20;
			int arcHeight = 20;

			g->setColor(mButton->getForeground());
			g->fillRoundRect(left, top, width, height, arcWidth, arcHeight);

			// note that the text background is the button foreground 
			g->setBackgroundColor(mButton->getForeground());
			if (selected)
			  g->setColor(mButton->getTextColor());
			else
			  g->setColor(mButton->getBackground());
			g->setFont(mButton->getFont());

			// use getTextSize instead!!
			int sleft = left + 14;
            TextMetrics* tm = g->getTextMetrics();
			int stop = top + (height / 2) + (tm->getAscent() / 2);
			// this is just a little too low, same thing happened
			// with knob, need to resolve this!!
			stop -= 2;

			if (mButton->getText() != NULL)
			  g->drawString(mButton->getText(), sleft, stop);

			// handle momentary buttons
			// I don't like doing this in the paint() method, but we have
			// no other way for the Window to send us events, and this
			// works well enough

			if (mButton->isMomentary()) {
				if (selected != mButton->isPushed()) {
					// hack to behave like a momentary button, need
					// to push this back down into a common
					// MomentaryButton class or something
					mButton->setPushed(selected);
					mButton->fireActionPerformed();
				}
			}
			else if (mButton->isImmediate()) {
				// fire events when the button goes down rather than
				// waiting for BN_CLICKED
				// !! must be an easier way
				if (selected)
				  mButton->fireActionPerformed();
			}

		}
		else {
			g->setColor(mButton->getBackground());
			g->fillRect(left, top, width, height);
		}

	}
	else if (mButton->isInvisible()) {
		// no visible rendering
	}
}

PUBLIC void WindowsButton::updateBounds() 
{
	// buttons seems to come out higher than what you ask for, 
	// is it aligning based on the text?
	// no, there is somethign screwed up in layout...
	Bounds b;
	mButton->getNativeBounds(&b);
	b.y += 2;
	updateNativeBounds(&b);
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

MacButton::MacButton(Button * b)
{
	mButton = b;
	mDown = false;
	mHilites = 0;
}

MacButton::~MacButton()
{
}

/**
 * Overload this so we can remove the button from the parent view.
 * This is necessary for Mobius that likes to add and remove buttons
 * at any time.  Really ALL components should work this way but in practice
 * we only do this for Mobius function buttons.
 */
void MacButton::close()
{
	if (mHandle != NULL) {
		DisposeControl((ControlRef)mHandle);
		mHandle = NULL;
	}
}

void MacButton::setText(const char *s) 
{
	if (mHandle != NULL) {
		// do we really need to change the text on these on the fly?
		// it will screw up layout
		printf("WARNING: MacButton::setText not implemented\n");
	}
}

/**
 * Programatically simulate the clicking of the button.
 * On Windows this sends a message to the button.
 * If we're using a CustomButton, what would we send to, just
 * fire the action handers?
 */
void MacButton::click()
{
	printf("WARNING: MacButton::click not implemented\n");
}

/**
 * We get a Click when the mouse button goes down.
 * To get the button to change appearance (going from grey to blue)
 * when clicked you MUST return eventNotHandledError from the Click
 * event handler.
 *
 * IFF you return eventNotHandledError from the Click handler then we
 * will also get a Hit when the button goes up.  BUT you only get a Hit
 * if the mouse stays within the button.  As soon as the mouse
 * strays outside the button, focus is lost, the button turns from
 * blue to grey and you won't get a Hit event and you also will not
 * get a mouse up event.
 *
 * This makes it hard for momentary buttons that need events on 
 * both press and release.  Returning eventNotHandledError and letting
 * the default handler change the button color and fire the Hit event
 * is convenient but you MUST stay inside the button.  
 *
 * If you return noErr from the click event, the button does not
 * automatically change color, but we do get a Mouse UP event sent
 * to the window and we can use the setDownButton hack to redirect
 * the mouse up event to the button.  This works well but you don't
 * get button color changes which feels strange.
 *
 * We do get a kEventControlHiliteChanged when the pressed mouse
 * leaves the button and could use that to trigger an up event.  But 
 * we have to be careful because this is also sent immediately after the
 * initial Click event.  If you move the mouse back into the button it will
 * rehighlight and produce a Hit event if you finally released the mouse
 * within the button.  This will cause an orhpan up event that might cause
 * problems.  
 *
 * Between the two it seems worse to lose the Hit and let the function
 * turn into a long press than it does to fire a stray up event. We'll see...
 */
PRIVATE EventTypeSpec ButtonEventsOfInterest[] = {
	{ kEventClassCommand, kEventCommandProcess },
	{ kEventClassControl, kEventControlHit },
	{ kEventClassControl, kEventControlClick },
	{ kEventClassControl, kEventControlHiliteChanged },
};

/**
 * Used to test the two kinds of button event handling and upclick processing.
 * When this is true we let the window event handler notify us when
 * it receives a mouse up event.  When this is false we do our
 * own local mouse tracking using HiliteChanged events.  The later
 * is simpler and seems to work well enough so remove the
 * "down button" stuff in MacWindow eventually.
 */
PRIVATE bool MacButtonTrackMouseFromWindow = false;

/**
 * Event handler for native buttons.
 */
PRIVATE OSStatus
ButtonEventHandler(EventHandlerCallRef caller, EventRef event, void* data)
{
    OSStatus result = eventNotHandledErr;

	int cls = GetEventClass(event);
	int kind = GetEventKind(event);

	//TraceEvent("Button", cls, kind);

	if (cls == kEventClassControl) {
		if (kind == kEventControlClick) {
			MacButton* b = (MacButton*)data;
			if (b != NULL)
			  b->fireActionPerformed(false);
			
			// this prevents the normal mouse tracking from happening
			if (MacButtonTrackMouseFromWindow)
			  result = noErr;
		}
		else if (kind == kEventControlHit) {
			// Should only be here if we're doing our own tracking
			// (MacButtonTrackMouseFromWindow is false).  Pay attention
			// only if mDown is still on.
			MacButton* b = (MacButton*)data;
			if (b != NULL)
			  b->fireActionPerformed(true);
		}
		else if (kind == kEventControlHiliteChanged) {
			// Should only be here if we're doing our own tracking
			MacButton* b = (MacButton*)data;
			if (b != NULL)
			  b->hiliteChanged();
		}
	}

	return result;
}

/**
 * This is called by the event handler, "hit" will be true for
 * kEventControlHit events false for kEventControlClick events.
 */
PUBLIC void MacButton::fireActionPerformed(bool hit)
{
	if (!mButton->isMomentary()) {
		// only care about click events
		if (!hit) {
			mButton->setPushed(true);
			mButton->fireActionPerformed();
		}
	}
	else if (!hit || mDown) {
		// hits are relevant only if we're doing our own tracking
		// and the down flag is still on

		mButton->setPushed(!hit);
		mButton->fireActionPerformed();
		
		if (MacButtonTrackMouseFromWindow) {
			// can only be here on a click
			MacWindow* window = getMacWindow(mButton);
			window->setDownButton(this);
		}
		else {
			// our local mouse tracker that looks for hilite changes
			if (hit) 
			  mDown = false;
			else {
				mDown = true;
				mHilites = 0;
			}
		}
	}
}

PUBLIC void MacButton::hiliteChanged()
{
	if (mButton->isMomentary()) {
		if (mDown) {
			mHilites++;
			// the first is expected the second means we strayed
			if (mHilites > 1)
			  fireActionPerformed(true);
		}
		else {
			// must be reentering the button after straying out? ignore
		}
	}
}

/**
 * This is called by MacWindow when the mouse release event is received
 * after having set the "down button". 
 * Should only happen if MacButtonTrackMouse is true.
 */
PUBLIC void MacButton::fireMouseReleased()
{
	mButton->setPushed(false);
	mButton->fireActionPerformed();
}

PUBLIC void MacButton::open()
{
	OSStatus status;
	OSErr err;

	WindowRef window = getWindowRef();

	if (mButton->isOwnerDraw()) {
		// Could try to do something like Windows but it's harder on Mac
		// because we have to have a UserPane to get mouse click events.
		// CustomButton does this outside the Button hierarchy so it's
		// a cleaner basis for extension.
		printf("ERROR: OwnerDraw buttons not supported on Mac!\n");
	}
	else if (mHandle == NULL && window != NULL) {

		ControlRef control;
		Rect bounds = { 0, 0, 0, 0 };
		const char* text = mButton->getText();
		CFStringRef cftext = MakeCFStringRef(text);

		status = CreatePushButtonControl(window, &bounds, cftext, &control);

		if (CheckStatus(status, "MacButton::open")) {
			mHandle = control;

			status = InstallControlEventHandler(control,
												NewEventHandlerUPP(ButtonEventHandler),
												GetEventTypeCount(ButtonEventsOfInterest),
												ButtonEventsOfInterest,
												this, NULL);

			CheckStatus(status, "MacButton::InstallEventHandler");
				
			// TODO: set kControlPushButtonDefaultTag and
			// kControlPushButtonCancelTag for the closure buttons
			// on a dialog
			/*
			Boolean b = true;
			err = SetControlData(control, 
								 kControlEntireControl, // inPart
								 kControlPushButtonDefaultTag,
								 (Size)sizeof(Boolean),
								 &b);
			*/

			// this asks Carbon to look up the embedding hierarchy when
			// drawing the control background
			SetUpControlBackground(control, 32, true);

			SetControlVisibility(control, true, false);
		}

    }
}

/**
 * Have to overload this because there is some kind of strange shadow shit
 * that occludes the bottom of the button.  Simply making it taller
 * doesn't help, have to add some extra invisible padding around the button.
 */
PUBLIC void MacButton::adjustControlBounds(Rect* bounds)
{
	bounds->bottom -= 1;
}

/**
 * Have to overload this because there is some kind of strange shadow shit
 * that occludes the bottom of the button.  The extra padding we put in here
 * will be used by the layout manager, but MacComponent::updateNativeBounds
 * will subtract it when sizing the component.
 */
PUBLIC void MacButton::getPreferredSize(Window* w, Dimension* d)
{
	if (!mButton->isOwnerDraw()) {
		MacComponent::getPreferredSize(w, d);
		// this will be subtracted later by adjustControlBounds
		d->height += 1;
	}
	else {
		// this algorithm is taken directly from Windows, 
		// need to tune it for Mac!!

		const char* text = mButton->getText();
		Font* font = mButton->getFont();

		w->getTextSize(text, font, d);

		// the "official" way to do this is to get a TEXTMETRIC
		// for the font and consume tmMaxCharWidth and tmExternalLeading
		Graphics* g = w->getGraphics();
		g->setFont(font);

		TextMetrics* tm = g->getTextMetrics();
		
		// getting strange variations in tmMaxCharWidth,
		// the default font as tmHeight=16 and tmMaxCharWidth=14,
		// but 8 pt ariel had a tmMaxCharWidth=27 !
		// use tmAveCharWidth, it seems to make more sense

		//mPreferred->width += 2 * tm->tmMaxCharWidth;
		d->width += 4 * tm->getAverageWidth();

		int fontHeight = tm->getHeight() + tm->getExternalLeading();
		d->height = 7 * fontHeight / 4;
	}
}

QWIN_END_NAMESPACE
#endif

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
