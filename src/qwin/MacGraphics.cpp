/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * This is the OSX implementation of the Graphics class.
 * The Windows implementation is in Graphics.cpp
 * 
 * Things to consider:
 *
 * FrameRect and InvertRect for hollow rectangles.
 *
 */

#include <stdio.h>
#include <memory.h>

#include "Util.h"
#include "MacUtil.h"

#include "Qwin.h"
#include "UIMac.h"

QWIN_BEGIN_NAMESPACE

/****************************************************************************
 *                                                                          *
 *   							 TEXT METRICS                               *
 *                                                                          *
 ****************************************************************************/

PUBLIC MacTextMetrics::MacTextMetrics()
{
	init();
}

PUBLIC void MacTextMetrics::init()
{
	mHeight = 0;
	mMaxWidth = 0;
	mAverageWidth = 0;
	mAscent = 0;
	mDescent = 0;
	mExternalLeading = 0;
}

PUBLIC void MacTextMetrics::init(Font* font)
{
	// not supporting everyting now, need to flesh out!
	if (font == NULL)
	  init();
	else {
		MacFont* mf = (MacFont*)font->getNativeFont();
		mHeight = mf->getHeight();
		mAscent = mf->getAscent();
		mDescent = mf->getDescent();
	}
}

PUBLIC int MacTextMetrics::getHeight()
{
	return mHeight;
}

PUBLIC int MacTextMetrics::getAscent()
{
	return mAscent;
}

PUBLIC int MacTextMetrics::getDescent()
{
	return mDescent;
}

PUBLIC int MacTextMetrics::getExternalLeading()
{
	printf("MacTextMetrics::getExternalLeading not implemented!!!\n");
	return 0;
}

PUBLIC int MacTextMetrics::getMaxWidth()
{
	printf("MacTextMetrics::getMaxWidth not implemented!!!\n");
	return 0;
}

PUBLIC int MacTextMetrics::getAverageWidth()
{
	printf("MacTextMetrics::getAverageWidth not implemented!!!\n");
	//return mHandle.tmAveCharWidth;
	return 0;
}

/****************************************************************************
 *                                                                          *
 *   							   GRAPHCIS                                 *
 *                                                                          *
 ****************************************************************************/

PUBLIC MacGraphics::MacGraphics()
{
	init();
}

PUBLIC MacGraphics::MacGraphics(MacWindow* win)
{
	init();
	setWindow(win);
}

PUBLIC void MacGraphics::setWindow(MacWindow* win)
{
    mWindow = win;
}

PUBLIC void MacGraphics::init()
{
	mWindow = NULL;
	//mDrawItem = NULL;

    // drawing attributes
    mColor = NULL;
    mFont = NULL;
    mDefaultFont = NULL;

    // !! swing doesn't have this, how is it done?
    mBackground = NULL;
}

PUBLIC MacGraphics::~MacGraphics()
{
	// release the handle?
}

/**
 * Always be able to get to a font if the components don't
 * have their own.
 */
PRIVATE Font* MacGraphics::getDefaultFont() 
{
    if (mDefaultFont == NULL)
      mDefaultFont = Font::getFont("Helvetica", 0, 18);
    return mDefaultFont;
}

PRIVATE Font* MacGraphics::getEffectiveFont()
{
    return (mFont != NULL) ? mFont : getDefaultFont();
}

/**
 * On Windows, save/restore is used to SaveDC and RestoreDC
 * so we can temporarily change colors etc.  On Mac
 * we don't need this yet, we'll push/pop a context for
 * every function.  If that's too expensive, then implement these...
 */
PUBLIC void MacGraphics::save()
{
	//if (mHandle != NULL)
	//SaveDC(mHandle);
}

PUBLIC void MacGraphics::restore()
{
	//if (mHandle != NULL)
	//RestoreDC(mHandle, -1);
}

/****************************************************************************
 *                                                                          *
 *                                 ATTRIBUTES                               *
 *                                                                          *
 ****************************************************************************/

PUBLIC void MacGraphics::setColor(Color* c)
{
    mColor = c;
}

PUBLIC Color* MacGraphics::getColor()
{
    return mColor;
}

/**
 * Windows specific, not in Swing and not used by Mobius.
 * Might correspond to the fill pattern?
 */
PUBLIC void MacGraphics::setBrush(Color* c)
{
}

/**
 * Windows specific, not in Swing and not used by Mobius.
 * Might correspond to the stroke pattern?
 */
PUBLIC void MacGraphics::setPen(Color* c)
{
}

/**
 * Have to keep the TextMetrics in sync since code may ask
 * for the metrics  before it sets the font.  
 */
PUBLIC void MacGraphics::setFont(Font* f)
{
	if (mFont != f) {
		// be sure and have a window-relative context in place when
		// we create the native font and take measurements
		CGContextRef context = beginContextBasic();

		mFont = f;
		mTextMetrics.init(getEffectiveFont());

		endContext(context);
	}
}

PUBLIC void MacGraphics::setBackgroundColor(Color* c)
{
    mBackground = c;
}

// SetROP2
//
// Sets the current foreground mix mode.  
// I think the Swing equivalent would be
// setComposite which is way too complicated

PUBLIC void MacGraphics::setXORMode(Color* c)
{
    // not sure how to handle the color, or if we even can
    setXORMode();
}

// not in Swing, but not exactly sure how these map

PUBLIC void MacGraphics::setXORMode()
{
    //SetROP2(mHandle, R2_XORPEN);
}

//////////////////////////////////////////////////////////////////////
//
// Common Operations
//
//////////////////////////////////////////////////////////////////////

PRIVATE WindowRef MacGraphics::getWindowRef()
{
	WindowRef ref = NULL;
	if (mWindow != NULL)
	  ref = (WindowRef)mWindow->getHandle();
	return ref;
}

/**
 * Get a Quartz context in which to draw, since we're not
 * using HIView composited windows, have to base this on a QuickDraw port.
 */
PRIVATE CGContextRef MacGraphics::beginContext()
{
	CGContextRef context = beginContextBasic();
	WindowRef window = getWindowRef();

	if (context != NULL && window != NULL) {
		// Set up a transform so the origin is in the upper left corner rather
		// than bottom right.  To do this we need the height of the window.

		Rect wbounds;

		// this is the size of the window relative to the screen, not what we want here
		//GetWindowBounds(window, kWindowContentRgn, &wbounds);

		// this is the size of the window (including the title bar?)
		GetWindowPortBounds(window, &wbounds);

		// this is effectively a "MoveTo" after which you can draw relative to 0,0
		CGContextTranslateCTM(context, 0, wbounds.bottom);
		CGContextScaleCTM(context, 1.0, -1.0);
	}

	return context;
}

/**
 * Do the typical context setup but don't do a coordiante translation.
 */
PRIVATE CGContextRef MacGraphics::beginContextBasic()
{
	CGContextRef context = NULL;
	WindowRef window = getWindowRef();

	if (window != NULL) {
		// older code does this, do we need this and a save/restore of
		// the QuickDraw port as well as the Quartz context?
		// if we do we'll have to save oldPort so endContext can get it
		//GrafPtr oldPort;
		//GetPort(&oldPort);

		// From Window Manager: Set the current graphics port to the window's port
		// I'm not sure why this is necessary but is in the Quartz examples
		SetPortWindowPort(window);

		// wrap a CGContext (Quartz) around a QuickDraw port from the window
		QDBeginCGContext(GetWindowPort (window), &context);
 
		// don't need to push a stack frame, QDBeginCGContext gives us a fresh one?
		// the examples don't call this
		//CGContextSaveGState(context);

		// normal scale and transformation
		CGContextSetLineWidth(context, 1.0);
		CGContextScaleCTM(context, 1.0, 1.0);

		// Quartz draws "in between" pixels rather than directly on them.
		// This can result in the "half line" problem with single pixel lines
		// that appear fuzzy, or various turds when you overwrite lines lines
		// of a different color.
		// One suggestion was to offset all coords by .5 of a "point" so it draws
		// within a pixel.  Another suggestion that doesn't require point
		// translation is to "offset the transformation matrix of user space
		// by 1/2 pixel in either coordinate direction.
		// Finally, you can use GCContextSetShouldAntiAlias
		// !! do we want this for text?
		CGContextSetShouldAntialias(context, false);
	}
	return context;
}

/**
 * Release the Quartz context returned by beginContext.
 * These are supposedly cached but since we'll tend to draw lots of
 * little things think about pushing this up to a higher level?
 */
PRIVATE void MacGraphics::endContext(CGContextRef context)
{
	WindowRef window = getWindowRef();
	
	if (context != NULL && window != NULL) {

		// pop the context stack?
		// examples don't do this, add this only for symmetry with beginContext
		//CGContextRestoreGState(context);

		QDEndCGContext(GetWindowPort(window), &context);

		// if we have to restore the QuickDraw port then do this assuming
		// we have a way to get to oldPort from beginContext()
		//SetPort(oldPort);
	}
}

PRIVATE MacColor* MacGraphics::getMacForeground() 
{
	Color* color = mColor;
	if (color == NULL) {
		// need a better fallback?
		color = Color::Black;
	}
	
	MacColor* c = (MacColor*) color->getNativeColor();
	if (c == NULL) {
		// should have been able to do something!!
		printf("ERROR: Unable to derive MacColor\n");
	}

	return c;
}

PRIVATE MacColor* MacGraphics::getMacBackground() 
{
	Color* color = mBackground;
	if (color == NULL) {
		// need a better fallback?
		color = Color::White;
	}
	
	MacColor* c = (MacColor*) color->getNativeColor();
	if (c == NULL) {
		// should have been able to do something!!
		printf("ERROR: Unable to derive MacColor\n");
	}

	return c;
}

PRIVATE void MacGraphics::setFillColor(CGContextRef context)
{
	MacColor* color = getMacForeground();
	CGContextSetRGBFillColor(context, color->getRed(), color->getGreen(), color->getBlue(), 
							 color->getAlpha());
}

PRIVATE void MacGraphics::setStrokeColor(CGContextRef context)
{
	MacColor* color = getMacForeground();
	CGContextSetRGBStrokeColor(context, color->getRed(), color->getGreen(), color->getBlue(), 
							   color->getAlpha());
}

/****************************************************************************
 *                                                                          *
 *                                  DRAWING                                 *
 *                                                                          *
 ****************************************************************************/

PUBLIC void MacGraphics::drawLine(int x1, int y1, int x2, int y2)
{
	// get a Quartz context in which to draw
	CGContextRef context = beginContext();
	if (context != NULL) {
		// for lines and rectangles
		CGContextSetLineWidth(context, 1.0);
	
		setStrokeColor(context);

		CGContextBeginPath(context);
		CGContextMoveToPoint(context, x1, y1);
		CGContextAddLineToPoint(context, x2, y2);

		// current path is cleared as a side effect
		CGContextStrokePath(context);

		endContext(context);
	}
}

/**
 * We depart from AWT on this and have all of the graphics methods 
 * consistently apply the rule that the right pixel is x + width - 1.
 * See WindowsGraphics::drawRect for more.
 */
PUBLIC void MacGraphics::drawRect(int x, int y, int width, int height)
{
	// get a Quartz context in which to draw
	CGContextRef context = beginContext();
	if (context != NULL) {
		// in case we want to draw a bounding rectangle
		CGContextSetLineWidth(context, 1.0);
	
		setStrokeColor(context);

		// CGRect is a CGPoint origin and CGSize size
		// CGPoint is float x and y
		// GCZize is float width and height
		// GCRectMake returns a CGRect with args x, y, width, height
		// !! who frees this the CGRectMake value, examples don't show?

		CGRect rect;
		rect.origin.x = x;
		rect.origin.y = y;
		rect.size.width = width;
		rect.size.height = height;

		// can also use CGContextStrokRectWithWidth with a float width
		// The line straddles the path with half of the total width on either side
		CGContextStrokeRect(context, rect);

		endContext(context);
	}
}

PUBLIC void MacGraphics::fillRect(int x, int y, int width, int height)
{
	// get a Quartz context in which to draw
	CGContextRef context = beginContext();
	if (context != NULL) {
		// in case we want to draw a bounding rectangle
		CGContextSetLineWidth(context, 1.0);
	
		setFillColor(context);

		// CGRect is a CGPoint origin and CGSize size
		// CGPoint is float x and y
		// GCZize is float width and height
		// GCRectMake returns a CGRect with args x, y, width, height
		// !! who frees this the CGRectMake value, examples don't show?

		CGRect rect;
		rect.origin.x = x;
		rect.origin.y = y;
		rect.size.width = width;
		rect.size.height = height;

		CGContextFillRect(context, rect);

		endContext(context);
	}
}

PUBLIC void MacGraphics::drawOval(int x, int y, int width, int height)
{
	// get a Quartz context in which to draw
	CGContextRef context = beginContext();
	if (context != NULL) {

		setStrokeColor(context);

		// single pixel ovals look really anemic on Mac, 2 is a bit too pudgy
		// we've got the usualy "in between the line" problem but hopefully
		// turning off antialiasing will fixed this too
		// hmm, adding any fraction bumps it up quite a bit to the point
		// where the bounding box does changes and slightly screws up
		// labeled knobs in Mobius...but it looks better...think about this...
		// SHIT: Thickening the line screws up the bounding box
		// compensating makes some of the Mobius components that expect to 
		// know the center (like level knobs) look funny...revisit this...
		bool thickBorder = false;

		if (thickBorder) {
			CGContextSetLineWidth(context, 1.25);
			// knock twice the line width off the width/height to compensate 
			x += 2;
			y += 2;
			width -= 2;
			height -= 2;
		}

		// CGRect is a CGPoint origin and CGSize size
		// CGPoint is float x and y
		// GCZize is float width and height
		// GCRectMake returns a CGRect with args x, y, width, height
		// !! who frees this the CGRectMake value, examples don't show?

		CGRect rect;
		rect.origin.x = x;
		rect.origin.y = y;
		rect.size.width = width;
		rect.size.height = height;

		CGContextStrokeEllipseInRect(context, rect);

		endContext(context);
	}
}

PUBLIC void MacGraphics::fillOval(int x, int y, int width, int height)
{
	// get a Quartz context in which to draw
	CGContextRef context = beginContext();
	if (context != NULL) {
		// in case we want to draw a bounding rectangle
		CGContextSetLineWidth(context, 1.0);
	
		setFillColor(context);

		// CGRect is a CGPoint origin and CGSize size
		// CGPoint is float x and y
		// GCZize is float width and height
		// GCRectMake returns a CGRect with args x, y, width, height
		// !! who frees this the CGRectMake value, examples don't show?

		CGRect rect;
		rect.origin.x = x;
		rect.origin.y = y;
		rect.size.width = width;
		rect.size.height = height;

		CGContextFillEllipseInRect(context, rect);

		endContext(context);
	}
}

/**
 * PI and friends.
 */
#define PI 3.141592654f

#define RADS(degrees) ((PI / 180.0f) * (degrees))

#define RAD_0 RADS(0)
#define RAD_45 RADS(45)
#define RAD_90 RADS(90)
#define RAD_180 RADS(180)
#define RAD_270 RADS(270)

/**
 * Swing and Windows support both an arcWidth and argHeight which
 * lets you effectively draw an oval arc.  Quartz doesn't have this
 * with AddArc, though we could get something similar with AddCurve
 * or AddQuadCurve.  Since Mobius does not currently have different
 * width and height values we'll just ignore height.
 *
 * 1 rad = 180/pi = 57.2958f;
 * 
 * 1 degree = pi/180 = 0.0175 rad
 *
 * 0 degrees = 0 radians
 * 90 degrees = pi/2 radians = 1.570796327
 * 180 degrees = pi radians = 3.141592654
 * 270 degrees = 3pi/2 radians = 4.712388980
 * 360 degrees = 2pi radians  = 6.283185307
 * 
 * Rads are normally visualized with 0 extending from the center to the east,
 * 90 to the north, 180 west, and 270 south.
 *
 * The transformation we do on the context to move the origin to upper/left
 * seems to screw this up.  Here 0 is east, 90 south, 180 west and 270 north.
 * But also the draw direction seems to be reversed.  Asking for clockwise draws
 * counterclockwise.
 *
 * The end result of all this is to get the upper-left rounded edge of a rectangle
 * instead of drawing from 180 to 90 clockwise you have to draw 180 to 270 
 * counter clockwise. 
 *
 * UPDATE: After having done all this, it looks like CGContextAddArcToPoint
 * would have made this easier, in fact the Quartz programming guide
 * says this is ideal for rounded rectangles.
 *   CGContextAddArcToPoint(context, x1, y1, x2, y2, radius)
 */	
PRIVATE CGContextRef MacGraphics::pathRoundRect(int x, int y, int width, int height,
												int arcWidth, int arcHeight)
{
	// get a Quartz context in which to draw
	CGContextRef context = beginContext();
	if (context != NULL) {
		// in case we want to draw a bounding rectangle
		CGContextSetLineWidth(context, 1.0);
	
		setFillColor(context);

		// can't support different width/height right now
		int radius = arcWidth;
	
		// upper left arc
		CGFloat centerX = x + radius;
		CGFloat centerY = y + radius;
		// x,y,radius,startAngle,endAngle,clockwise
		CGContextAddArc(context, centerX, centerY, radius, RAD_180, RAD_270, 0);

		// top side
		int right = x + width - 1;
		CGContextAddLineToPoint(context, right - radius, y);

		// upper right arc
		centerX = right - radius;
		centerY = y + radius;
		// x,y,radius,startAngle,endAngle,clockwise
		CGContextAddArc(context, centerX, centerY, radius, RAD_270, RAD_0, 0);

		// right side
		int bottom = y + height - 1;
		CGContextAddLineToPoint(context, right, bottom - radius);

		// lower right arc
		centerX = right - radius;
		centerY = bottom - radius;
		// x,y,radius,startAngle,endAngle,clockwise
		CGContextAddArc(context, centerX, centerY, radius, RAD_0, RAD_90, 0);

		// bottom side
		CGContextAddLineToPoint(context, x + radius, bottom);

		// lower left arc
		centerX = x + radius;
		centerY = bottom - radius;
		// x,y,radius,startAngle,endAngle,clockwise
		CGContextAddArc(context, centerX, centerY, radius, RAD_90, RAD_180, 0);

		// left side
		CGContextAddLineToPoint(context, x, y + radius);
	}

	return context;
}

PUBLIC void MacGraphics::drawRoundRect(int x, int y, int width, int height,
									   int arcWidth, int arcHeight)
{
	CGContextRef context = pathRoundRect(x, y, width, height, arcWidth, arcHeight);
	if (context != NULL) {
		// current path is cleared as a side effect
		CGContextStrokePath(context);

		endContext(context);
	}
}

PUBLIC void MacGraphics::fillRoundRect(int x, int y, int width, int height,
                                    int arcWidth, int arcHeight)
{
	CGContextRef context = pathRoundRect(x, y, width, height, arcWidth, arcHeight);
	if (context != NULL) {
		// current path is cleared as a side effect
		CGContextFillPath(context);

		endContext(context);
	}
}

static inline double radians (double degrees) { return degrees * M_PI / 180; }

/**
 * From java.awt.Graphics
 * 
 * Fill a circular or ellipitcal arc covering the specifed rectangle.
 * 
 * The resulting arc begins at startAngle and extends for arcAngle
 * degrees. Angles are interpreted such that 0 degrees is at the 3
 * o'clock position. A positive value indicates a counter-clockwise
 * rotation while a negative value indicates a clockwise rotation.
 * 
 * The center of the arc is the center of the rectangle whose origin is
 * (x, y) and whose size is specified by the width and height arguments.
 * 
 * The resulting arc covers an area width + 1 pixels wide by height + 1
 * pixels tall.
 * 
 * The angles are specified relative to the non-square extents of the
 * bounding rectangle such that 45 degrees always falls on the line from
 * the center of the ellipse to the upper right corner of the bounding
 * rectangle. As a result, if the bounding rectangle is noticeably longer
 * in one axis than the other, the angles to the start and end of the arc
 * segment will be skewed farther along the longer axis of the bounds.
 * 
 * Parameters:
 * x - the x coordinate of the upper-left corner of the arc to be filled.
 * y - the y coordinate of the upper-left corner of the arc to be filled.
 * width - the width of the arc to be filled.
 * height - the height of the arc to be filled.
 * startAngle - the beginning angle.
 * arcAngle - the angular extent of the arc, relative to the start angle.
 * 
 * NOTE: If arcAngle is 360 CGContextAddArc ends up being called 
 * with the same radian angle for the start and end which apparently
 * collapses to nothing but we want this to mean a filled circle.  
 * I'm not sure if there is a way to control that but it's easy enough
 * to catch it early and just convert this to a filled circle.  
 */
PUBLIC void MacGraphics::fillArc(int x, int y, int width, int height,
								 int startAngle, int arcAngle)
{
	if (arcAngle >= 360 || arcAngle <= -360) {
		fillOval(x, y, width, height);
	}
	else {
		// get a Quartz context in which to draw
		CGContextRef context = beginContext();
		if (context != NULL) {

			CGContextSetLineWidth(context, 1.0);
			setFillColor(context);
			setStrokeColor(context);

			// just be square, ignore height
			int radius = width / 2;
			int centerx = x + radius;
			int centery = y + radius;

			CGContextBeginPath(context);
			CGContextMoveToPoint(context, centerx, centery);

			// according to online docs, in iPhone OS, 0 means clockwise but
			// in Mac OS X, 1 means clockwise, but that's not what I see,
			// this probably has to do with the origin transformation, 
			// zero renders clockwise
			int direction;
			int endAngle;
			if (arcAngle >= 0) {
				direction = 1;
				endAngle = startAngle - arcAngle;
				if (endAngle < 0)
				  endAngle += 360;
			}
			else {
				direction = 0;
				endAngle = startAngle - arcAngle;
				if (endAngle > 360)
				  endAngle -= 360;
			}

			CGFloat cgStart = radians(startAngle);
			CGFloat cgEnd = radians(endAngle);
			CGContextAddArc(context, centerx, centery, radius, 
							cgStart, cgEnd, direction);

			// return point to center
			CGContextAddLineToPoint(context, centerx, centery);

			CGContextFillPath(context);

			endContext(context);
		}
	}
}

//////////////////////////////////////////////////////////////////////
//
// Text
//
// You would not believe how fucking hard it is to draw a string in Carbon.
//
//////////////////////////////////////////////////////////////////////

PUBLIC int GetStyleAttribute(ATSUStyle style, ATSUAttributeTag attribute)
{
	ATSUTextMeasurement value = Long2Fix(0L);

	OSStatus status = ATSUGetAttribute(style, attribute, sizeof(value), 
									   &value, NULL);
	// this seems to be common, but it still returns a good looking value
	if (status != kATSUNotSetErr)
	  CheckStatus(status, "ATSUGetAttribute");

	float fvalue = FixedToFloat(value);
	int ivalue = ceil(fvalue);

	return ivalue;
}

/**
 * Set the font for an ATSUStyle
 */
PUBLIC void SetStyleFont(ATSUStyle style, ATSUFontID font)
{
	//printf("SetStyleFont %d\n", font);

	ATSUAttributeTag  tags[1];
	ByteCount sizes[1];
	ATSUAttributeValuePtr values[1];

	tags[0] = kATSUFontTag;
	sizes[0] = sizeof(ATSUFontID);
	values[0] = &font;
	OSErr err = ATSUSetAttributes(style, 1, tags, sizes, values);
	CheckErr(err, "SetStyleFont");
}

PUBLIC void SetStyleFontSize(ATSUStyle style, int size)
{
	//printf("SetStyleFontSize %d\n", size);

	ATSUAttributeTag tags[1];
	ByteCount sizes[1];
	ATSUAttributeValuePtr values[1];
	
	// has to be passed as a Fixed
	Fixed fix = Long2Fix((long)size);

	tags[0] = kATSUSizeTag;
	sizes[0] = sizeof(Fixed);
	values[0] = &fix;
	OSErr err = ATSUSetAttributes(style, 1, tags, sizes, values);
	CheckErr(err, "SetStyleFontSize");
}

PUBLIC void SetStyleBold(ATSUStyle style, bool bold)
{
	ATSUAttributeTag tags[1];
	ByteCount sizes[1];
	ATSUAttributeValuePtr values[1];
	
	Boolean isBold = (bold) ? TRUE : FALSE;

	tags[0] = kATSUQDBoldfaceTag;
	sizes[0] = sizeof(Boolean);
	values[0] = &isBold;
	OSErr err = ATSUSetAttributes(style, 1, tags, sizes, values);
	CheckErr(err, "SetStyleBold");
}

PUBLIC void SetStyleItalic(ATSUStyle style, bool italic)
{
	ATSUAttributeTag tags[1];
	ByteCount sizes[1];
	ATSUAttributeValuePtr values[1];
	
	Boolean isItalic = (italic) ? TRUE : FALSE;

	tags[0] = kATSUQDItalicTag;
	sizes[0] = sizeof(Boolean);
	values[0] = &isItalic;
	OSErr err = ATSUSetAttributes(style, 1, tags, sizes, values);
	CheckErr(err, "SetStyleBold");
}

/**
 * kATSUColor - must be an RGBColor
 * There is also kAATSURGBAlphaColorTag which is in the programming guide
 * and lets you set transluency.
 */
PUBLIC void SetStyleColor(ATSUStyle style, RGBColor* color)
{
	ATSUAttributeTag tags[1];
	ByteCount sizes[1];
	ATSUAttributeValuePtr values[1];
	
	tags[0] = kATSUColorTag;
	sizes[0] = sizeof(RGBColor);
	values[0] = color;
	OSErr err = ATSUSetAttributes(style, 1, tags, sizes, values);
	CheckErr(err, "SetStyleColor");
}

PUBLIC void SetLayoutLineWidth(ATSUTextLayout layout, int width)
{
	ATSUAttributeTag tags[1];
	ByteCount sizes[1];
	ATSUAttributeValuePtr values[1];
	
	Fixed fixedWidth = X2Fix(width);
	tags[0] = kATSULineWidthTag;
	sizes[0] = sizeof(Fixed);
	values[0] = &fixedWidth;
	OSErr err = ATSUSetLayoutControls(layout, 1, tags, sizes, values);
	CheckErr(err, "SetLayoutLineWidth");
}

PUBLIC void SetLayoutAlignLeft(ATSUTextLayout layout)
{
	ATSUAttributeTag tags[1];
	ByteCount sizes[1];
	ATSUAttributeValuePtr values[1];
	
	// horizontal text left to right from the left margin
	Fract horzAlign = kATSUStartAlignment; 

	tags[0] = kATSULineFlushFactorTag;
	sizes[0] = sizeof(Fract);
	values[0] = &horzAlign;

	OSErr err = ATSUSetLayoutControls(layout, 1, tags, sizes, values);
	CheckErr(err, "SetLayoutAlignLeft");
}

PUBLIC void SetLayoutContext(ATSUTextLayout layout, CGContextRef context)
{
	ATSUAttributeTag tags[1];
	ByteCount sizes[1];
	ATSUAttributeValuePtr values[1];
	
	tags[0] = kATSUCGContextTag;
	sizes[0] = sizeof(CGContextRef);
	values[0] = &context;

	OSErr err = ATSUSetLayoutControls(layout, 1, tags, sizes, values);
	CheckErr(err, "SetLayoutContext");
}

/**
 * In Java, the baseline of the leftmost character is at x,y.
 * In ATSUI this is called the "origin".
 */
PUBLIC void MacGraphics::drawString(const char* text, int x, int y)
{
	OSErr err;
	OSStatus status;

	// ATSU measurement functions return errors if you give them
	// empty strings so catch that early
	if (text == NULL || strlen(text) == 0) return;

	// must have a font by now
    Font* font = getEffectiveFont();
	if (font == NULL) return;

    CGContextRef context = beginContextBasic();
	if (context == NULL) return;

	// make a layout
	// it doesn't appear that this uses a CGContxt or GrafPtr
	ATSUTextLayout layout = getLayout(context, text, font);

	// measure it
	Dimension d;
	measureText(layout, &d);

	// set final layout width
	SetLayoutLineWidth(layout, d.width);

	// ATSUDrawString takes an x/y coordinate as the "origin".
	// This is apparently the baseline in the Y dimension 
	// (bounding box bottom - descent).

	int textX = x;
	int textY = y;

	// beginContext will do a transform that will cause the text to appear
	// inverted so we can't use it.  Instead do a similar coordinate flip 
	// but without the transform.

	// this is the size of the window, including the title bar
	Rect wbounds;
	WindowRef window = getWindowRef();
	//GetWindowBounds(window, kWindowContentRgn, &wbounds);
	GetWindowPortBounds(window, &wbounds);

	// note that this does not do the usual +1 height adjustment
	int windowRange = wbounds.bottom - wbounds.top;
	int reflectedBaseline = windowRange - textY;

	// if a background is set, use it, otherwise default?
	Color* back = mBackground;
	// hack for location testing
	//back = Color::White;
	if (back != NULL) {
		MacColor* c = (MacColor*) back->getNativeColor();
		CGContextSetRGBFillColor(context, c->getRed(), c->getGreen(), c->getBlue(), 
								 c->getAlpha());

		// fucking Quartz coordinate shenanigans 

		// textY is the baseline, have to remove the ascent
		int top = textY - font->getAscent();

		// The "origin" in a CGRect is the lower left corner of the rectangle
		top += d.height;

		// finally reflect into Quartz space
		int reflectedTop = windowRange - top;

		CGRect rect;
		rect.origin.x = x;
		rect.origin.y = reflectedTop;
		rect.size.width = d.width;
		rect.size.height = d.height;
		CGContextFillRect(context, rect);
	}

	// and draw!
	err = ATSUDrawText(layout, kATSUFromTextBeginning, kATSUToTextEnd, 
					   X2Fix(textX), X2Fix(reflectedBaseline));
	CheckErr(err, "ATSUDrawString:ATSUDrawText");

	// whew, I need a shower

    ATSUDisposeTextLayout(layout);
	endContext(context);
}

/**
 * Convert a C string to a UniChar* string.
 * 
 * So we don't have to allocate memory every damn time we draw a string,
 * we'll maintain a private buffer.  This will put a limit on the
 * maximum string we can draw but since these are single line strings
 * the monitor imposes a practical limit on what we can draw anyway.
 */
PRIVATE void MacGraphics::setUniChars(const char* str)
{
	// KLUDGE: This only works if the source string is 7-bit ASCII
	// it will not work for UTF-8.  In theory we're supposed
	// to use the CFString conversion functions I can't find
	// any that don't allocate memory.  If we can't find something
	// better, consider building these general converters:

	mUniCharsLength = 0;
	if (str != NULL) {
		int len = strlen(str);
		int max = MAX_UNICHAR_BUFFER - 1;
		int i;
		for (i = 0 ; i < len && i < max ; i++) {
			mUniChars[i] = str[i];
		}
		mUniChars[i] = 0;
		mUniCharsLength = i;
	}
}

PUBLIC void MacGraphics::getTextSize(const char *text, Dimension* d)
{
    Font* font = getEffectiveFont();
	getTextSize(text, font, d);
}

/**
 * Setup an ATSUI style and layout for the given text with the
 * current font and style settings.
 */
PRIVATE ATSUTextLayout MacGraphics::getLayout(CGContextRef context,
											  const char* text, 
											  Font* font)
{
	// make a layout
	// it doesn't appear that this uses a CGContxt or GrafPtr
	ATSUTextLayout layout;
	ATSUCreateTextLayout(&layout);
	SetLayoutContext(layout, context);
	SetLayoutAlignLeft(layout);

	// convert the C string to UniChar
	setUniChars(text);

	// attach the UTF-16 text to the layout
	OSErr err = ATSUSetTextPointerLocation(layout, mUniChars, 
										   kATSUFromTextBeginning, kATSUToTextEnd,
										   mUniCharsLength);
	CheckErr(err, "ATSUDrawString:ATSUSetTextPointerLocation");

	// get a style from the font
	MacFont* macfont = (MacFont*)font->getNativeFont();
	ATSUStyle style = macfont->getStyle();

	// what about background!?
	MacColor* macForeground = getMacForeground();
	SetStyleColor(style, macForeground->getRGBColor());

	// put the style in the layout
	err = ATSUSetRunStyle(layout, style, kATSUFromTextBeginning, kATSUToTextEnd);
	CheckErr(err, "ATSUDrawString:ATSUSetRunStyle");

	return layout;
}

/**
 * Measure the text in a layout.
 * It is important that we get the "typographic bounds" and not the
 * "image bounds" as is returned by ATSUMeasureTextImage.
 */
PRIVATE void MacGraphics::measureText(ATSUTextLayout layout, Dimension* d)
{
	OSStatus status;
	int width = 0;
	int height = 0;

	// to calculate line height, this is given as a "modern" example
	ATSUTextMeasurement ascent, descent;
	ByteCount actualSize;
	UniCharArrayOffset offset;

	// apparently -8801 will happen if you did not explicitly set it
	status = ATSUGetLineControl(layout, 0, kATSULineAscentTag, 
								sizeof(ascent), &ascent, &actualSize);

	if (status != kATSUNotSetErr)
	  CheckStatus(status, "ATSUGetLineControl:kATSULineAscentTag");

	status = ATSUGetLineControl(layout, 0, kATSULineDescentTag, 
								sizeof(descent), &descent, &actualSize);
	if (status != kATSUNotSetErr)
	  CheckStatus(status, "ATSUGetLineControl:kATSULineDescentTag");

	float fascent = FixedToFloat(ascent);
	float fdescent = FixedToFloat(descent);

	//printf("ATSUGetLineControl ascent %f descent %f\n", fascent, fdescent);

	height = ceil(fascent) + ceil(fdescent);

	// this is recommended for older code, but also gives the typographic width
	// docs say to use kATSUseFractionalOrigins but it seems to make
	// more sense to use kATSUseDeviceOrigins
	ATSTrapezoid glyphBounds;
	ItemCount numBounds;
	status = ATSUGetGlyphBounds(layout, 0, 0, kATSUFromTextBeginning, kATSUToTextEnd,
								kATSUseDeviceOrigins,
								1, &glyphBounds, &numBounds);
	CheckStatus(status, "ATSUGetGlyphBounds");
	if (numBounds <= 0) {
		printf("ERROR: ATSUGetGlyphBounds did not return bounds!!\n");
	}
	else {
		/*
		printf("Glyph %d: upperLeft %f,%f upperRight %f,%f lowerRight %f,%f lowerLeft %f,%f\n",
			   1,
			   FixedToFloat(glyphBounds.upperLeft.x),
			   FixedToFloat(glyphBounds.upperLeft.y),
			   FixedToFloat(glyphBounds.upperRight.x),
			   FixedToFloat(glyphBounds.upperRight.y),
			   FixedToFloat(glyphBounds.lowerRight.x),
			   FixedToFloat(glyphBounds.lowerRight.y),
			   FixedToFloat(glyphBounds.lowerLeft.x),
			   FixedToFloat(glyphBounds.lowerLeft.y));
		*/

		ascent = glyphBounds.upperLeft.y;
		descent = glyphBounds.lowerLeft.y;

		float fascent = FixedToFloat(ascent);
		float fdescent = FixedToFloat(descent);

		// here the ascent is normally negative (relative to 0,0)
		if (fascent < 0)
		  fascent = -fascent;

		//printf("ATSUGetGlyphBounds ascent %f descent %f\n", 
		//fascent, fdescent);

		// ascent is normally negative, relative to 0,0
		int height2 = ceil(fascent) + ceil(fdescent);

		// I want to know if this happens
		if (height != height2) {
			printf("WARNING: Different text height measurement: %d %d\n",
				   height, height2);
			height = height2;
		}

		int left = ceil(FixedToFloat(glyphBounds.upperLeft.x));
		int right = ceil(FixedToFloat(glyphBounds.upperRight.x));

		// always zero?
		if (left != 0)
		  printf("WARNING: Text measured with non-zero left edge!!\n");
		
		width = (right - left) + 1;
	}
	
	if (d != NULL) {
		d->width = width;
		d->height = height;
	}
}

/**
 * Not  technically in Swing but simplies some typical Swing over engineering.
 */
PUBLIC void MacGraphics::getTextSize(const char *text, Font* font, Dimension* d)
{
	d->width = 0;
	d->height = 0;

	if (text == NULL || strlen(text) == 0) return;
    if (font == NULL) font = getDefaultFont();

    CGContextRef context = beginContext();
	if (context == NULL) return;

	// build a layout
	ATSUTextLayout layout = getLayout(context, text, font);

	// and measure it
	measureText(layout, d);

    ATSUDisposeTextLayout(layout);
	endContext(context);
}

/**
 * Not in Swing.
 * This is used on Windows to get metrics for the system font used
 * to draw things like list boxes and combo boxes.  This needs
 * to be pushed into the comonents since they might not all use
 * the same font!
 */
PUBLIC TextMetrics* MacGraphics::getTextMetrics()
{
	// Not sure if it's necessary but make sure we have a window context
	// in place before we start asking ATSUI sizing questions.  I don't
	// see how else it could know about the destination device...
    CGContextRef context = beginContextBasic();

	mTextMetrics.init(getEffectiveFont());

	endContext(context);

	return &mTextMetrics;
}

QWIN_END_NAMESPACE

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
