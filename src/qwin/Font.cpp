/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * The two most common stock fonts are:
 *
 *    SYSTEM_FONT
 *	  SYSTEM_FIXED_FONT
 *
 * The basic W95 fonts are: Courier New, Times New, Arial, and Symbol
 * Courier is fixed-pitch, Times is serif, Arial is a sans-serif
 * clone of Helvetica.  Symbol has a collection of random symbols.
 *
 * Size is expressed in "points" which is close to 1/72 inch.  12-point
 * text is common in books.  The size is usually the hight of the
 * characters from the top of the ascenders to the bottom of the descenders,
 * but not always.
 *
 * Use GetTextExtendPoint32 to get the width and height of a text string
 * using the font currently selected into the device context.
 * 
 */

#include <stdio.h>
#include <memory.h>
#include <math.h>
#include <string.h>

#include "Util.h"
#include "Trace.h"
#include "Qwin.h"
#include "UIManager.h"

QWIN_BEGIN_NAMESPACE

/****************************************************************************
 *                                                                          *
 *   								 FONT                                   *
 *                                                                          *
 ****************************************************************************/

Font* Font::Fonts = NULL;

PUBLIC Font::Font(const char* name, int style, int size)
{
	mNext = NULL;
	mHandle = NULL;
	mName = CopyString(name);
	mStyle = style;
	mSize = size;
}

PUBLIC Font::~Font()
{
	delete mName;
	delete mHandle;
}

PUBLIC NativeFont* Font::getNativeFont()
{
	if (mHandle == NULL)
	  mHandle = UIManager::getFont(this);
    return mHandle;
}

/**
 * Since fonts are typically reused, Components do not
 * assume they own fonts and will not delete them.
 * To ensure that the handles are releases, we maintain
 * a global list of Font objects.
 */
PUBLIC Font* Font::getFont(const char* name, int style, int size)
{
    // should have a more effecient collection, but usually aren't
    // many of these

    Font* font = NULL;

    for (Font* f = Fonts ; f != NULL ; f = f->getNext()) {
        if (!strcmp(f->getName(), name) &&
            f->getStyle() == style &&
            f->getSize() == size) {
            font = f;
            break;
        }
    }

    if (font == NULL) {
        // !! csect
        font = new Font(name, style, size);
        font->mNext = Fonts;
        Fonts = font;
    }

    return font;
}

PUBLIC void Font::dumpFonts()
{
	printf("Fonts loaded:\n");
    for (Font* f = Fonts ; f != NULL ; f = f->getNext()) {
		printf("  %s %d %d\n", f->getName(), f->getStyle(), f->getSize());
    }
}

PUBLIC void Font::exit(bool dump)
{
	if (dump)
	  dumpFonts();

    Font *f, *next;
    for (f = Fonts, next = NULL ; f != NULL ; f = next) {
        next = f->getNext();
        delete f;
    }
}

PUBLIC Font* Font::getNext()
{
    return mNext;
}

PUBLIC const char* Font::getName()
{
    return mName;
}

PUBLIC int Font::getStyle()
{
    return mStyle;
}

PUBLIC int Font::getSize()
{
    return mSize;
}

/**
 * Swing support fonts that have different baselines for
 * characters in different "writing systems".  Fonts have
 * a getBaslineFor method that takes a character argument.
 *
 * Here we'll simplify this and assume a common baseline for all 
 * characters.
 *
 * We have to have a handle to do this, which in turn required a 
 * device context.
 */
PUBLIC int Font::getAscent()
{
	getNativeFont();
	return (mHandle != NULL) ? mHandle->getAscent() : 0;
}

PUBLIC int Font::getHeight()
{
	getNativeFont();
	return (mHandle != NULL) ? mHandle->getHeight() : 0;
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

PUBLIC WindowsFont::WindowsFont(Font* f)
{
	mFont = f;
	mHandle = NULL;

    // initialize this so we don't fly off into the ozone if
    // you call it before the handle is available
    memset(&mTextMetric, 0, sizeof(mTextMetric));

	// go ahead and allocate the system font now, so we can get text metrics
	// before the window is available
	getHandle();
}

PUBLIC WindowsFont::~WindowsFont()
{
    if (mHandle != NULL) {
        // Supposed to call this, not sure what the consequences are
        // if you don't. Not supposed to call this if it is currently
        // selected into a DC, how can we ensure that?
        DeleteObject(mHandle);
    }
}

PUBLIC int WindowsFont::getAscent()
{
    return mTextMetric.tmAscent;
}

PUBLIC int WindowsFont::getHeight()
{
    return mTextMetric.tmHeight;
}

PUBLIC HFONT WindowsFont::getHandle()
{
	if (mHandle == NULL) {
		// use the screen DC so we can get font metrics before we
		// start opening windows
		HDC dc = GetDC(NULL);

		mHandle = getHandle(dc);

		ReleaseDC(NULL, dc);
	}
	return mHandle;
}

/**
 * Adapted from Petzold's "ezfont" example in Programming Windows 95.
 * Use just normal point size rather than "decipoints".
 */
PRIVATE HFONT WindowsFont::getHandle(HDC dc)
{
	HFONT font;
	FLOAT dpix, dpiy;
	LOGFONT lf;
	POINT p;

	//printf("getHandle %s %d %d\n", mFont->getName(), 
	//mFont->getStyle(), mFont->getSize());

	SaveDC(dc);
		
	// Petzold allows this to be passed in to select "logical resolution"
	// based on the device capabilities.  
	bool logicalResolution = false;

	SetGraphicsMode(dc, GM_ADVANCED);
	ModifyWorldTransform(dc, NULL, MWT_IDENTITY);
	SetViewportOrgEx(dc, 0, 0, NULL);
	SetWindowOrgEx(dc, 0, 0, NULL);

		
	if (logicalResolution) {
		int logx = GetDeviceCaps(dc, LOGPIXELSX);
		int logy = GetDeviceCaps(dc, LOGPIXELSY);
		dpix = (FLOAT)logx;
		dpiy = (FLOAT)logy;
	}
	else {
		int hres = GetDeviceCaps(dc, HORZRES);
		int hsize = GetDeviceCaps(dc, HORZSIZE);
		int vres = GetDeviceCaps(dc, VERTRES);
		int vsize = GetDeviceCaps(dc, VERTSIZE);
		dpix = (FLOAT)(25.4 * hres / hsize);
		dpiy = (FLOAT)(25.4 * vres / vsize);
	}

	// these weren't specified in "decipoints" so need to adjust
	p.x = (int) ((mFont->getSize() * 10) * dpix / 72);
	p.y = (int) ((mFont->getSize() * 10) * dpiy / 72);

		// convert device coords to logical coords
	DPtoLP(dc, &p, 1);

    int style = mFont->getStyle();
	memset(&lf, 0, sizeof(lf));
	lf.lfHeight = - (int)(fabs((float)p.y) / 10.0 + 0.5);
	lf.lfWeight = (style & FONT_BOLD) ? 700 : 0;
	lf.lfItalic = (style & FONT_ITALIC) ? 1 : 0;
	lf.lfUnderline = (style & FONT_UNDERLINE) ? 1 : 0;
	lf.lfStrikeOut = (style & FONT_STRIKEOUT) ? 1 : 0;
	strcpy(lf.lfFaceName, mFont->getName());

	font = CreateFontIndirect(&lf);

		// if a decipoint width were specified, you then do this
	int deciWidth = 0;

	if (deciWidth != 0) {
		TEXTMETRIC tm;
		font = (HFONT)SelectObject(dc, font);
		GetTextMetrics(dc, &tm);
		DeleteObject(SelectObject(dc, font));
		lf.lfWidth = (int)(tm.tmAveCharWidth *
						   fabs((float)p.x) / fabs((float)p.y) + 0.5);
		font = CreateFontIndirect(&lf);
	}

	// remember some things in a TEXTMETRIC
	// technically we would need to refresh these every time the font
	// is selected in a Graphics, or put the methods on the Graphics
	// object, but this seems safe since we're not doing anything
	// fancy with transforms or radically different device contexts
	SelectObject(dc, font);
	GetTextMetrics(dc, &mTextMetric);

	RestoreDC(dc, -1);

	return font;
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

PUBLIC MacFont::MacFont(Font* f)
{
	mFont = f;
	mHandle = 0;
	mStyle = NULL;
	mAscent = 0;
	mDescent = 0;
	mLeading = 0;
}

PUBLIC MacFont::~MacFont()
{
	if (mHandle) {
		// !! release it??
	}

	if (mStyle != NULL) {
		// !! destroy it?
	}
}

PUBLIC int MacFont::getAscent()
{
    return mAscent;
}

PUBLIC int MacFont::getDescent()
{
    return mDescent;
}

PUBLIC int MacFont::getHeight()
{
    return mAscent + mDescent;
}

PUBLIC ATSFontRef MacFont::getATSFontRef()
{
    if (!mHandle) {

		const char* name = mFont->getName();
		int size = mFont->getSize();

		// PLAIN, BOLD, ITALIC, UNDERLINE, STRIKEOUT
		// not supporting these yet and I'm not sure we really need to
		// we're trying to do this currently by setting ATSUStyle options
		// during rendering
		int style = mFont->getStyle();

		CFStringRef cfname = MakeCFStringRef(name);
		mHandle = ATSFontFindFromName(cfname, kATSOptionFlagsDefault);
		if (mHandle == 0) {
			Trace(1, "Unable to find font %s\n", name);
			// The official Mac way would be to use "font fallbacks", here
			// we'll just ask for a known system font
			mHandle = ATSFontFindFromName(CFSTR("Helvetica"), kATSOptionFlagsDefault);
			if (mHandle == 0) {
				// really serious...
				Trace(1, "Unable to find fallback font!\n");
			}
		}

		FreeCFStringRef(cfname);

		if (mHandle) {
			OSStatus status;

			// capture some metrics
			// unfortunately these are relative floats
			/*
			ATSFontMetrics metrics;

			status = ATSFontGetHorizontalMetrics(mHandle, 0, &metrics);
			CheckStatus(status, "ATSFontGetHorizontalMetrics");
			dumpMetrics("Horizontal", &metrics);
				
			status = ATSFontGetVerticalMetrics(mHandle, 0, &metrics);
			CheckStatus(status, "ATSFontGetVerticalMetrics");
			dumpMetrics("Vertical", &metrics);
			*/

			// This seems to apply the metrics to a point size, but 
			// I'm not sure how accurate this is for screen pixels.
			// !! Rework this to pass in the Graphics so we can use ATSUI
			// calls to make text measurements.  This is okay for now but
			// doesn't let us do packed "image" retangles and full "typographic"
			// rectangles as an option.

			ATSUStyle style;
			status = ATSUCreateStyle(&style);
			SetStyleFont(style, mHandle);
			SetStyleFontSize(style, mFont->getSize());

			// These seem to be relatively accurate as long as a window's
			// Quartz context is in scope
			mAscent = GetStyleAttribute(style, kATSUAscentTag);
			mDescent = GetStyleAttribute(style, kATSUDescentTag);
			mLeading = GetStyleAttribute(style, kATSULeadingTag);
			//printf("Ascent %d Descent %d Leading %d\n", 
			//mAscent, mDescent, mLeading);
		}
	}

	return mHandle;
}

PRIVATE void MacFont::dumpMetrics(const char* type, ATSFontMetrics* metrics)
{
	printf("%s Font Metrics: %s %d %d\n", type, mFont->getName(),
		   mFont->getSize(), mFont->getStyle());
	printf("  ascent %f\n", metrics->ascent);
	printf("  descent %f\n", metrics->descent);
	printf("  leading %f\n", metrics->leading);
	printf("  avgAdvanceWidth %f\n", metrics->avgAdvanceWidth);
	printf("  minLeftSideBearing %f\n", metrics->minLeftSideBearing);
	printf("  minRightSideBearing %f\n", metrics->minRightSideBearing);
	printf("  stemWidth %f\n", metrics->stemWidth);
	printf("  stemHeight %f\n", metrics->stemHeight);
	printf("  capHeight %f\n", metrics->capHeight);
	printf("  xHeight %f\n", metrics->xHeight);
	printf("  italicAngle %f\n", metrics->italicAngle);
	printf("  underlinePosition %f\n", metrics->underlinePosition);
	printf("  underlineThickness %f\n", metrics->underlineThickness);
	fflush(stdout);
};

/**
 * This is ultimately what we want in MacGraphics.
 * ATSUI programmers guide suggests we should reuse these rather than
 * make one every time.
 * 
 * MacGraphics will change the colors.
 * 
 * An ATSUStyle is quite complex, besides font you can ask for vertical text, 
 * rotation, justification, bold, italic, underline.
 *
 */
PUBLIC ATSUStyle MacFont::getStyle()
{
    if (mStyle == NULL) {
		ATSFontRef macfont = getATSFontRef();
		if (macfont) {
			OSStatus status = ATSUCreateStyle(&mStyle);
			CheckStatus(status, "ATSUCreateStyle");

			SetStyleFont(mStyle, macfont);
			SetStyleFontSize(mStyle, mFont->getSize());

			// not supporting underline & strikeout
			// Mac has kATSUQDUnderlineTag but nothing for strikeout I could see
			if (mFont->getStyle() & FONT_BOLD)
			  SetStyleBold(mStyle, true);

			if (mFont->getStyle() & FONT_ITALIC)
			  SetStyleItalic(mStyle, true);
		}
	}

	return mStyle;
}

QWIN_END_NAMESPACE
#endif

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
