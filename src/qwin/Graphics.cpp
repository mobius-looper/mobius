/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * This is the Windows implementation of the Graphics class.
 * The OSX implementation is in MacGraphics.cpp
 * 
 * Things to consider:
 *
 * FrameRect and InvertRect for hollow rectangles.
 *
 */

#include <stdio.h>
#include <memory.h>
#include <math.h>

#include <windows.h>
#include <commctrl.h>

#include "Trace.h"
#include "Util.h"
#include "Qwin.h"
#include "UIWindows.h"

/****************************************************************************
 *                                                                          *
 *   							 TEXT METRICS                               *
 *                                                                          *
 ****************************************************************************/

PUBLIC WindowsTextMetrics::WindowsTextMetrics()
{
    memset(&mHandle, 0, sizeof(TEXTMETRIC));
}

PUBLIC void WindowsTextMetrics::init(HDC dc)
{
	GetTextMetrics(dc, &mHandle);
}

PUBLIC int WindowsTextMetrics::getHeight()
{
	return mHandle.tmHeight;
}

PUBLIC int WindowsTextMetrics::getMaxWidth()
{
	return mHandle.tmMaxCharWidth;
}

PUBLIC int WindowsTextMetrics::getAverageWidth()
{
	return mHandle.tmAveCharWidth;
}

PUBLIC int WindowsTextMetrics::getAscent()
{
	return mHandle.tmAscent;
}

PUBLIC int WindowsTextMetrics::getExternalLeading()
{
	return mHandle.tmExternalLeading;
}

/****************************************************************************
 *                                                                          *
 *   							   GRAPHCIS                                 *
 *                                                                          *
 ****************************************************************************/

PUBLIC WindowsGraphics::WindowsGraphics()
{
	init();
}

PUBLIC WindowsGraphics::WindowsGraphics(HDC dc)
{
	init();
	setDeviceContext(dc);
}

PUBLIC void WindowsGraphics::setDeviceContext(HDC dc)
{
    mHandle = dc;
	if (dc != NULL) {
		mDefaultFont = (HFONT)GetCurrentObject(dc, OBJ_FONT);
		mTextMetrics.init(dc);
	}
}

PUBLIC void WindowsGraphics::setDrawItem(LPDRAWITEMSTRUCT di)
{
    mDrawItem = di;
}

PUBLIC void WindowsGraphics::init()
{
	mHandle = NULL;
	mDrawItem = NULL;

    // drawing attributes
    mColor = NULL;
    mFont = NULL;
	mDefaultFont = NULL;

    // !! swing doesn't have this, how is it done?
    mBackground = NULL;

    // brush we use for hollow drawing
    LOGBRUSH logbrush;
    logbrush.lbStyle = BS_HOLLOW;
    mHollowBrush = CreateBrushIndirect(&logbrush);
    mSaveBrush = NULL;
}

PUBLIC WindowsGraphics::~WindowsGraphics()
{
	// release the handle?
}

PUBLIC LPDRAWITEMSTRUCT WindowsGraphics::getDrawItem()
{
    return mDrawItem;
}

/****************************************************************************
 *                                                                          *
 *                                 ATTRIBUTES                               *
 *                                                                          *
 ****************************************************************************/

PUBLIC void WindowsGraphics::setColor(Color* c)
{
    mColor = c;
    if (mColor != NULL) {

        // brush here may be a handle to a logical brush, or a color value
        // for a logical brush call: CreateHatchBrush, CreatePatternBrush,
        // CreateSolidBrush
        // you can call GetStockObject for a limited number of basic brushes
        // you can specify a system color use COLOR_WINDOW plus the color
        // value
        //HBRUSH brush = (HBRUSH)GetStockObject(WHITE_BRUSH);

        // ?? simpler to use SetDCBrushColor
        // perhaps but not supported on 95/98
        WindowsColor* wc = (WindowsColor*)mColor->getNativeColor();
        if (wc != NULL) {
            SelectObject(mHandle, wc->getBrush());
            SetTextColor(mHandle, mColor->getRgb());

            // supposedly can use SetDCPenColor on 2K/XP, but
            // I didn't see it the  includes
            //SetDCPenColor(mHandle, mColor->getRgb());

            // !! need to handle variable pen widths, this will default to 2
            SelectObject(mHandle, wc->getPen());
        }
    }
    else {
        SetTextColor(mHandle, GetSysColor(COLOR_WINDOWTEXT));
        // what about graphics colors?
    }
}

PUBLIC Color* WindowsGraphics::getColor()
{
    return mColor;
}


/**
 * Windows specific, not in Swing and not used by Mobius.
 */
PUBLIC void WindowsGraphics::setBrush(Color* c)
{
    if (c != NULL) {
        WindowsColor* wc = (WindowsColor*)c->getNativeColor();
        if (wc != NULL)
          SelectObject(mHandle, wc->getBrush());
    }
}

/** 
 * Windows specific, not in Swing and not used by Mobius.
 */
PUBLIC void WindowsGraphics::setPen(Color* c)
{
    if (c != NULL) {
        WindowsColor* wc = (WindowsColor*)c->getNativeColor();
        if (wc != NULL)
          SelectObject(mHandle, wc->getPen());
    }
}

PUBLIC void WindowsGraphics::setFont(Font* f)
{
    // note that you always have to call SelectObject even if mFont
    // hasn't changed since we don't know what's in the HDC

    mFont = f;

    if (mFont != NULL) {
        WindowsFont* wf = (WindowsFont*)mFont->getNativeFont();
        if (wf != NULL) {
            // !! what if our DC is different than the one the 
            // font was created with?
            HFONT fh = wf->getHandle();
            if (fh == NULL)
              printf("Unable to get handle for font %s %d\n", 
                     f->getName(), f->getSize());
            else {
                SelectObject(mHandle, fh);
                // keep this in sync
                mTextMetrics.init(mHandle);
            }

        }
    }
    else if (mDefaultFont != NULL) {
        // reselect the default font
        SelectObject(mHandle, mDefaultFont);
        mTextMetrics.init(mHandle);
    }
}

PUBLIC void WindowsGraphics::setBackgroundColor(Color* c)
{
    mBackground = c;
    if (mBackground != NULL) {
        SetBkColor(mHandle, mBackground->getRgb());
    }
    else {
        // hack since our standard background color is BTNFACE
        // so buttons look ok 
        SetBkColor(mHandle, GetSysColor(COLOR_BTNFACE));
    }
}

// SetROP2
//
// Sets the current foreground mix mode.  
// I think the Swing equivalent would be
// setComposite which is way too complicated

PUBLIC void WindowsGraphics::setXORMode(Color* c)
{
    // not sure how to handle the color, or if we even can
    setXORMode();
}

// not in Swing, but not exactly sure how these map

PUBLIC void WindowsGraphics::setXORMode()
{
    SetROP2(mHandle, R2_XORPEN);
}

/****************************************************************************
 *                                                                          *
 *                                  DRAWING                                 *
 *                                                                          *
 ****************************************************************************/

/**
 * In Java, the baseline of the leftmost character is at x,y.
 * In Windows, text alighment is variable and controled
 * by the SetTextAlign function.  Values include:
 *   TA_BASELINE, TA_BOTTOM, TA_TOP, etc.
 *
 * The default is TA_TOP | TA_LEFT, force TA_BASELINE.
 * SetTextCharacterExtra can be used to set intercharacter spacing.
 *
 * There's also a SetBkMode to "specify how the system should blend
 * the background color with the current colors".  
 */
PUBLIC void WindowsGraphics::drawString(const char* str, int x, int y)
{
    if (str != NULL) {
        // could do this just once
        SetTextAlign(mHandle, TA_BASELINE | TA_LEFT);
        TextOut(mHandle, x, y, str, strlen(str));
    }
}

//
// Rectangle
//
// From AWT docs for the "draw" functions::
// "Draws the outline of the specified rectangle, The left and
// right edges of the rectangle are x and x + width".  So if width
// is 10, the edges are 0 and 10 which actually becomes a width
// of 11 pixels.  
//
// But for the "fill" functions:
// "The left and right edges are x and x + width - 1"
//
// So we've got a silly inconsistency that I don't understand
// the reasoning behind.  I'd really rather not propagate this,
// but it makes it harder to port over fragments of Swing code.
// 
// Windows Rectangle functions exclude the bottom and right edges.
// They are outlined using the current pen and filled using the current brush.
// Don't appear to be any "hollow" graphics functions.
//
// Ok, I'm tired of this.  I'm going to depart from AWT on this
// and have all of the graphics methods consistently apply
// the rule that the right pixel is x + width - 1.
//
// Ugh, the behavior I think I'm seeing is that the "outline"
// is outside of the specified bounds of the rectangle, so you need
// to adjust coordinates to remove that.  

PUBLIC void WindowsGraphics::drawRect(int x, int y, int width, int height)
{
    startHollowShape();
    // adjust for outline
    x++;
    y++;
    width--;
    height--;
    Rectangle(mHandle, x, y, x + width, y + height);
    endHollowShape();
}

PRIVATE void WindowsGraphics::startHollowShape()
{
    // the old way, just filled with the background color, not really hollow
    /*
    if (mBackground != NULL) {
        WindowsColor* wc = (WindowsColor*)mBackground->getNativeColor();
        if (wc != NULL)
          SelectObject(mHandle, wc->getBrush());
    }
    */

    // new way, true hollow
    mSaveBrush = (HBRUSH)SelectObject(mHandle, mHollowBrush);
}

PRIVATE void WindowsGraphics::endHollowShape()
{
    /* old way
    if (mColor != NULL) {
        WindowsColor* wc = (WindowsColor*)mColor->getNativeColor();
        if (wc != NULL) 
          SelectObject(mHandle, wc->getBrush());
    }
    */
    
    SelectObject(mHandle, mSaveBrush);
}


PUBLIC void WindowsGraphics::fillRect(int x, int y, int width, int height)
{
    x++;
    y++;
    width--;
    height--;
    Rectangle(mHandle, x, y, x + width, y + height);
}

// Ellipse
//
// It looks like this DOES include the right and bottom edges.
//
 
PUBLIC void WindowsGraphics::drawOval(int x, int y, int width, int height)
{
    startHollowShape();
	Ellipse(mHandle, x, y, x + width - 1, y + height - 1);
    endHollowShape();
}

PUBLIC void WindowsGraphics::fillOval(int x, int y, int width, int height)
{
    // it looks like this one DOES include the right & bottom edge
	Ellipse(mHandle, x, y, x + width - 1, y + height - 1);
}

// RoundRect
//
// Pen and Brush handling like Rectangle
// Docs don't say if it excludes the right and bottom edges,
// assume it works like Rectangle and excludes

PUBLIC void WindowsGraphics::drawRoundRect(int x, int y, int width, int height,
                                    int arcWidth, int arcHeight)
{
    startHollowShape();
    x++;
    y++;
    width--;
	height--;
	RoundRect(mHandle, x, y, x + width, y + height, arcWidth, arcHeight);
    endHollowShape();
}

PUBLIC void WindowsGraphics::fillRoundRect(int x, int y, int width, int height,
                                    int arcWidth, int arcHeight)
{
    x++;
    y++;
    width--;
    height--;
	RoundRect(mHandle, x, y, x + width, y + height, arcWidth, arcHeight);
}

// Lines

PUBLIC void WindowsGraphics::drawLine(int x1, int y1, int x2, int y2)
{
	MoveToEx(mHandle, x1, y1, NULL);
	LineTo(mHandle, x2, y2);
}

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
 */
PUBLIC void WindowsGraphics::fillArc(int x, int y, int width, int height,
									 int startAngle, int arcAngle)
{
    // convert to the two radial coordinates necessary for Pie()
    // getteing a turd at the top of the range so pack everything in by one
    x++;
    y++;
    width--;
    height--;

	int radius = width / 2;
	int centerx = x + radius;
	int centery = y + radius;

    int endAngle;
    if (arcAngle >= 0) {
        SetArcDirection(mHandle, AD_COUNTERCLOCKWISE);
        endAngle = startAngle - arcAngle;
        if (endAngle < 0)
            endAngle += 360;
    }
    else {
        SetArcDirection(mHandle, AD_CLOCKWISE);
        endAngle = startAngle - arcAngle;
        if (endAngle > 360)
          endAngle -= 360;
    }

	int radial1x, radial1y;
	getRadial(centerx, centery, radius, startAngle, &radial1x, &radial1y);

	int radial2x, radial2y;
	getRadial(centerx, centery, radius, endAngle, &radial2x, &radial2y);
	
    // don't have anything equivalent to this, I think swign does
    // this with a negative arcAngle

	Pie(mHandle, x, y, x + width - 1, y + height - 1,
		radial1x, radial1y,	radial2x, radial2y);
}

/**
 * Convert an angle to a radial point (point along a the circle).
 * There are two basic methods.
 * 
 * Second-degree polynomial:
 * 
 *   P = (x, sqrt( r^2 - x^2 ))
 *
 * Trigonometric:
 *
 *   P = (r * cos(theta), r * sin(theta))
 *   where theta is the angle in radians
 *
 * Trig is more computationally expensive.  Breshenham's algorithm is 
 * preferred for actually drawing a circle, but its more complicated
 * and we only need to determine one point here.
 */
PRIVATE void WindowsGraphics::getRadial(int centerx, int centery, 
										int radius, int angle,
										int* radialx, int* radialy)
{
	// radians = degrees * (pi / 180)
	double radians = angle / 57.2957;
	*radialx = centerx + (int)(radius * ::cos(radians));
	*radialy = centery + (int)(radius * ::sin(radians));
}

/****************************************************************************
 *                                                                          *
 *   							  EXTENSIONS                                *
 *                                                                          *
 ****************************************************************************/

// Things that don't exactly correspond to Swing methods.
// Some of these could be approxomated, others are simplifications
// of some typical Swing overengineering

PUBLIC TextMetrics* WindowsGraphics::getTextMetrics()
{
	return &mTextMetrics;
}

PUBLIC void WindowsGraphics::save()
{
	if (mHandle != NULL)
	  SaveDC(mHandle);
}

PUBLIC void WindowsGraphics::restore()
{
	if (mHandle != NULL)
	  RestoreDC(mHandle, -1);
}

PUBLIC void WindowsGraphics::getTextSize(const char *text, Dimension* d)
{
	getTextSize(text, mFont, d);
}

PUBLIC void WindowsGraphics::getTextSize(const char *text, Font* font, 
										 Dimension* d)
{
    // font may be NULL here to use the default font
	if (text != NULL) {
		SIZE size;

		save();
		setFont(font);
		GetTextExtentPoint32(mHandle, text, strlen(text), &size);
		d->width = size.cx;
		d->height = size.cy;
		
		// in some cases this comes back zero, why?
		if (d->height == 0)
		  d->height = mTextMetrics.getHeight() + 
			  mTextMetrics.getExternalLeading();
			  
		restore();
	}
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
