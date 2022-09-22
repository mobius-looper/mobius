/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * !! Need to stop using this since it cannot be implemented consistently
 * on all platforms, at least not easily.
 *
 * These are sort of like a combination JLabel and JPanel with
 * some border options.  It is the basis of Label.  It used
 * to be the basis of Panel but that has it's own UI now
 * because Mac has nothing like this.
 *
 * If you specify text, it is rendered like a label.  If you
 * specify one of the "rect" options you get a filled rectangle,
 * and "frame" options get you a border.  You don't seem to be able
 * to combine text and rect/frame though.  Too bad.
 *
 * From Petzold:
 * 
 * When you move or click the mouse over a static child window,
 * the child window traps the WM_NCHITTEST message and returns a value
 * of HTTRANSPARENT to Windows.  This causes Windows to send the same
 * WM_NCHITTEST message to the underlying window which is usually 
 * the parent.  The parent usually passes the message to DefWindowProc
 * where it is converted to a client-area mouse message.
 *
 * The following styles are available:
 *
 * SS_BLACKRECT
 * SS_GRAYRECT
 * SS_WHITERECT
 * SS_BLACKFRAME
 * SS_GRAYFRAME
 * SS_WHITEFRAME
 *
 * The first three render fill rectangles, the three "frame" styles
 * are rectangular outlines that are not filled.
 *
 * The three colors correspond to these system colors:
 *
 * COLOR_3DDKSHADOW
 * COLOR_BTNSHADOW
 * COLOR_BTNHIGHLIGHT
 *
 * You can combine the gray and white styles with these to create shadowed
 * frames.  I'm not seeing any difference with these in Windows XP.
 *
 * SS_ETCHEDHORZ
 * SS_ETCHEDVERT
 * SS_ETCHEDFRAME
 * 
 * You can also give these text which will be displayed
 * using the DT_WORDBREAK, DT_NOCLIP and DT_EXPANDTABS parameters
 * to DrawText.  The text will therefore be wordwrapped in the client
 * rectangle.  This makes them similar to JLabels.  When using text,
 * you can include the styles SS_LEFT, SS_RIGHT, and SS_CENTER
 * to specify text alighment.  The background of the text is normally
 * COLOR_BTNFACE and the text is COLOR_WINDOWTEXT.  You can intercept
 * WM_CTLCOLORSTATIC messages to change the text color by calling
 * SetTextColor/SetBkColor and returning a handle to the background brush.
 * 
 * SS_BITMAP
 * SS_ICON
 *
 * Allow you to display static graphics.
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
 *   								STATIC                                  *
 *                                                                          *
 ****************************************************************************/

PUBLIC Static::Static() 
{
	init();
}

PRIVATE void Static::init()
{
	mClassName = "Static";
    mText = NULL;
    mStyle = 0;
    mFont = NULL;
    mBitmap = false;
    mIcon = false;
}

PUBLIC Static::Static(const char *s) 
{
	init();
	setText(s);
}

PUBLIC Static::~Static() 
{
	delete mText;
    // mFont is NOT owned, it is normally shared
}

PUBLIC ComponentUI* Static::getUI()
{
    if (mUI == NULL)
      mUI = UIManager::getStaticUI(this);
	return mUI;
}

PUBLIC StaticUI* Static::getStaticUI()
{
	return (StaticUI*)getUI();
}

PUBLIC void Static::setText(const char *s) 
{
    mIcon = false;
    mBitmap = false;
	if (mText != s) {
		delete mText;
		mText = CopyString(s);
	}

    StaticUI* ui = getStaticUI();
    ui->setText(s);
    invalidate();
}

PUBLIC const char* Static::getText()
{
    return mText;
}

PUBLIC void Static::setBitmap(const char *name) 
{
    mBitmap = true;
    if (mText != name) {
        delete mText;
        mText = CopyString(name);
    }

    StaticUI* ui = getStaticUI();
    ui->setBitmap(name);
}

PUBLIC bool Static::isBitmap()
{
    return mBitmap;
}

PUBLIC void Static::setIcon(const char *name) 
{
    mIcon = true;
    if (mText != name) {
        delete mText;
        mText = CopyString(name);
    }

    StaticUI* ui = getStaticUI();
    ui->setIcon(name);
}

PUBLIC bool Static::isIcon()
{
    return mIcon;
}

PUBLIC void Static::setFont(Font* f)
{
    mFont = f;
}

PUBLIC Font* Static::getFont()
{
    return mFont;
}

/**
 * Hack to play around with simple Windows graphic styles.
 * These aren't that useful, but if you go any further with this,
 * either promote these to explicit options or define some 
 * platform independent constants.
 */
PUBLIC void Static::setStyle(int bits)
{
    mStyle = bits;
}

PUBLIC int Static::getStyle()
{
    return mStyle;
}

PUBLIC Dimension* Static::getPreferredSize(Window* w)
{
	if (mPreferred == NULL) {
		mPreferred = new Dimension();
        ComponentUI* ui = getUI();
        ui->getPreferredSize(w, mPreferred);
	}
	return mPreferred;
}

PUBLIC void Static::setBackground(Color* c) 
{
    Component::setBackground(c);
	invalidate();
}

PUBLIC void Static::dumpLocal(int indent)
{
    dumpType(indent, "Static");
}

PUBLIC void Static::open()
{
    ComponentUI* ui = getUI();
    ui->open();
}

/*
  PUBLIC void Static::updateUI()
  {
  // I'm not seeing any image if you just specify it
  // in the CreateWindow call, apparently have to set it
  // explicitly.
  StaticUI* ui = getStaticUI();
  if (mIcon)
  ui->setIcon(mText);
  else if (mBitmap)
  ui->setBitmap(mText);
  }
*/

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

PUBLIC WindowsStatic::WindowsStatic(Static* s) 
{
	mStatic = s;
	mAutoColor = false;
}

PUBLIC WindowsStatic::~WindowsStatic() 
{
}

PUBLIC void WindowsStatic::setText(const char *s) 
{
	if (mHandle != NULL) {
		SetWindowText(mHandle, s);

		// !! should update our actual size, otherwise
		// invalidate isn't going to have the right rectangle	
		// GetWinowRect doesn't work, may have to compute
		// with text metrics instead
		//RECT r;
		//GetWindowRect(mHandle, &r);
		//mBounds.width = r.right - r.left;
		//mBounds.height = r.bottom - r.top;
	}
}

PUBLIC void WindowsStatic::setBitmap(const char *name) 
{
	if (mHandle != NULL) {
        HBITMAP hbitmap = NULL;
        if (name != NULL) {
			WindowsContext* context = getWindowsContext(mStatic);
            HINSTANCE inst = context->getInstance();
            hbitmap = LoadBitmap(inst, name);
			if (hbitmap == NULL)
              printf("Unable to load bitmap %s\n", name);
        }

		// can you send NULL and have it taken away?
		SendMessage(mHandle, STM_SETIMAGE, IMAGE_BITMAP, (LONG)hbitmap);
    }
}

PUBLIC void WindowsStatic::setIcon(const char *name) 
{
    if (mHandle != NULL) {
        HICON hicon = NULL;
        if (name != NULL) {
			WindowsContext* context = getWindowsContext(mStatic);
            HINSTANCE inst = context->getInstance();
            hicon = LoadIcon(inst, name);
            if (hicon == NULL)
              printf("Unable to load icon %s\n", name);
        }

        // can you send NULL and have it taken away?
        SendMessage(mHandle, STM_SETIMAGE, IMAGE_ICON, (LONG)hicon);
    }
}

PUBLIC void WindowsStatic::open()
{
	if (mHandle == NULL) {
		HWND parent = getParentHandle();
        if (parent != NULL) {
            const char* text = mStatic->getText();

			// these must be presized, though if we have text
			// could presize them as if this were a label

            DWORD style = getWindowStyle();

            // If we're not displaying text and the background is one
            // of three builtin styles, use those.  Not sure if this
            // is a significant optimization.  It doesn't appear that
            // you can mix text with these styles so if you set text later
            // later it won't work.

            if (text == NULL) {
                DWORD colorStyle = 0;
                Color* c = mStatic->getBackground();
                if (c == Color::Gray)
                  colorStyle = SS_GRAYRECT;
                else if (c == Color::Black)
                  colorStyle = SS_BLACKRECT;
                else if (c == Color::White)
                  colorStyle = SS_WHITERECT;

                if (colorStyle != 0) {
                    // set this so colorHook knows not to process mBackground
                    mAutoColor = true;
                    style |= colorStyle;
                }
            }
            else if (mStatic->isBitmap()) {
                style |= SS_BITMAP;
            }
            else if (mStatic->isIcon()) {
                style |= SS_ICON;
            }
            else {
                // so we don't have to expose this in qwin, default
                // to left justified
                style |= SS_LEFT;
            }

            // allow user define style bits, should convert this
            // to a bunch of explicit options, if a style was specified
			// assume this means we're also auto-color
			int userStyle = mStatic->getStyle();
			if (userStyle > 0) {
                if (userStyle & SS_BLACK)
                  style |= SS_BLACKRECT;
                if (userStyle & SS_GRAY)
                  style |= SS_GRAYRECT;
                if (userStyle & SS_WHITE)
                  style |= SS_WHITERECT;

				mAutoColor = true;
			}

            Bounds* b = mStatic->getBounds();
            Point p;
            mStatic->getNativeLocation(&p);

            //printf("Opening a static!!: %s\n", mStatic->getTraceName());
            //fflush(stdout);

            mHandle = CreateWindow("static",
                                   text,
                                   style,
                                   p.x, p.y, b->width, b->height,
                                   parent,
                                   0,
                                   NULL,
                                   NULL);

            if (mHandle == NULL)
              printf("Unable to create Static control\n");
            else {
                subclassWindowProc();
                SetWindowLong(mHandle, GWL_USERDATA, (LONG)this);

				// native components may be created invisible in tabs
				mStatic->initVisibility();

                // I'm not seeing any image if you just specify it
                // in the CreateWindow call, apparently have to set it
                // explicitly.
                if (mStatic->isIcon())
                  setIcon(text);
                else if (mStatic->isBitmap())
                  setBitmap(text);
            }
        }
    }
}

PUBLIC Color* WindowsStatic::colorHook(Graphics* g)
{
    Color* brush = NULL;

    if (mStatic->isBitmap() || mStatic->isIcon()) {
		Color* back = mStatic->getBackground();
        if (back != NULL && !mAutoColor)
          brush = back;
    }
    else {
		const char* text = mStatic->getText();
		Color* fore = mStatic->getForeground();

        if (text != NULL && fore != NULL)
          g->setColor(fore);

        // strange: setting the foreground color doesn't
        // seem to do anything unless you also 
        // return a background brush, why the hell is that?
        Color* background = mStatic->getBackground();
        if (background == NULL)
		  background = Color::ButtonFace;

        if (background != NULL) {
            if (text != NULL)
              g->setBackgroundColor(background);
		
            // can avoid this if we're using a style option, 
            // not sure this optimization really matters
            if (!mAutoColor)
              brush = background;
        }

        // can use this opportunity to set fonts,
        // I don't like this since we can't save/restore
        // the DC in this style, may be best to just
        // use OWNERDRAW?
        // It doesn't seem to matter as the font doesn't
        // stick, I guess the default window handler
        // for static will put it back to the default
        g->setFont(mStatic->getFont());
    }
    return brush;
}

/**
 * May be called to return the size of the icon or bitmap after loading.
 */
PUBLIC void WindowsStatic::getPreferredSize(Window * w, Dimension* d)
{
	if (mStatic->isBitmap() || mStatic->isIcon()) {
		RECT r;
		GetWindowRect(mHandle, &r);
		d->width = r.right - r.left;
		d->height = r.bottom - r.top;
	}
	else {
		w->getTextSize(mStatic->getText(), mStatic->getFont(), d);
	}
}


QWIN_END_NAMESPACE
#endif // __WIN32

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

PUBLIC MacStatic::MacStatic(Static* s) 
{
	mStatic = s;
	mAutoColor = false;
}

PUBLIC MacStatic::~MacStatic() 
{
}

PUBLIC void MacStatic::setText(const char *text) 
{
	if (mHandle != NULL) {
		ControlRef control = (ControlRef)mHandle;

		CFStringRef cfstring = MakeCFStringRef(text);
		OSErr err = SetControlData(control, 0, 
								   kControlStaticTextCFStringTag,
								   sizeof(CFStringRef),
								   &cfstring);

		CheckErr(err, "MacStatic::setText");

		// !! TODO: free the CFStringRef?

		// optional redraw?
		// mStatic->invalidate();
	}
}

PUBLIC void MacStatic::setBitmap(const char *name) 
{
}

PUBLIC void MacStatic::setIcon(const char *name) 
{
}

/**
 * May be called to return the size of the icon or bitmap after loading.
 * !! TODO: Not currently supporting bitmaps with Static components, need
 * to refactor this.  This won't be called for static text either,
 * Static::getPreferrredSize just gets the text metrics from the Graphics.
 * We could do something similar down here but the result is the same.
 */
PUBLIC void MacStatic::getPreferredSize(Window* w, Dimension* d)
{
	if (mHandle == NULL) {
	}
	else if (mStatic->isBitmap() || mStatic->isIcon()) {
		// no story here...
	}
	else {
		// Note that on Mac the text width calculated by Graphics
		// uses ATSUI and does NOT render the same as a static text component.
		// The sizes look about the same but the static text component has more
		// space between characters and space at the top and bottom.
		//w->getTextSize(mStatic->getText(), mStatic->getFont(), d);
		//printf("MacStatic::Graphics text width %d height %d\n", 
		//d->width, d->height);

		// in order for this to be accurate you MUST turn off
		// the "is multi line" option when the component is created
		// doesn't seem to be accurate, compare with this
		Rect bounds = { 0, 0, 0, 0 };
		SInt16 baseLine;
		GetBestControlRect((ControlRef)mHandle, &bounds, &baseLine);

		int bestWidth = bounds.right - bounds.left;
		int bestHeight = bounds.bottom - bounds.top;

		//printf("MacStatic::BestControlRect width %d height %d baseline %d\n", 
		//bestWidth, bestHeight, baseLine);

		d->width = bestWidth;
		// !! how does baseline factor in here?
		d->height = bestHeight;
	}
}

void MacStatic::updateNativeBounds(Bounds* b) 
{
	MacComponent::updateNativeBounds(b);
}

PUBLIC void MacStatic::open()
{
	OSStatus status;
	OSErr err;

	if (mHandle == NULL) {
		WindowRef window;
		ControlRef parent;
		getEmbeddingParent(&window, &parent);

		if (window != NULL || parent != NULL) {
			ControlRef control;
			Rect bounds = { 0, 0, 0, 0 };
			const char* text = mStatic->getText();
			CFStringRef cftext = MakeCFStringRef(text);
			ControlFontStyleRec style;

			style.flags = 0;

			Font* font = mStatic->getFont();
			if (font != NULL) {
				MacFont* mf = (MacFont*)font->getNativeFont();
				ATSFontRef atsfont = mf->getATSFontRef();
				int fontSize = font->getSize();

				style.flags |= kControlUseFontMask | kControlUseSizeMask;
				style.font = atsfont;
				style.size = fontSize;
				style.style = 0;

				if (font->getStyle() & FONT_BOLD)
				  style.style |= 1;

				if (font->getStyle() & FONT_ITALIC)
				  style.style |= 2;

				if (font->getStyle() & FONT_UNDERLINE)
				  style.style |= 4;

				// Mac doesn't have strikeout, but it does have 
				// "outline" at bitmask 8 and "shadow" at bitmask 16
			}

			Color* color = mStatic->getForeground();
			if (color != NULL) {
				MacColor* c = (MacColor*) color->getNativeColor();
				style.flags |= kControlUseForeColorMask;
				c->getRGBColor(&style.foreColor);
			}

			// Background color doesn't work with static controls, 
			// not sure why but forums seem to indiciate that if you're
			// not using "compositing windows" that the dialog/window
			// gets to control the backgrond?  It might be related to the
			// mysterious "mode" but I couldn't find any interesting
			// documentation or comments on that.  For now, if you
			// need text with a background use a lightweight component.

			color = mStatic->getBackground();
			if (color != NULL) {
				MacColor* c = (MacColor*) color->getNativeColor();
				style.flags |= kControlUseBackColorMask;
				c->getRGBColor(&style.backColor);
			}

			status = CreateStaticTextControl(window, &bounds, cftext, 
											 &style, &control);

			if (CheckStatus(status, "MacStatic::open")) {
				mHandle = control;
				
				if (isCompositing()) {
					status = HIViewAddSubview(parent, control);
					if (status == controlHandleInvalidErr) {
						// see this on occasion, MacErrors.h says
						// "You passed an invalid ControlRef to a Control Manager API."
						printf("Static::controlHandleInvalidErr\n");
						fflush(stdout);
					}
					else
					  CheckStatus(status, "MacStatic::HIViewAddSubview");
				}
				else {
					EmbedControl(control, parent);
				}

				// you MUST turn this off or else GetBestControlRect
				// doesn't return a valid width and text will wrap
				Boolean b = false;
				err = SetControlData(control, 
									 kControlEntireControl, // inPart
									 kControlStaticTextIsMultilineTag,
									 (Size)sizeof(Boolean),
									 &b);

				SetControlVisibility(control, true, false);
			}
		}
	}
}

QWIN_END_NAMESPACE
#endif

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
