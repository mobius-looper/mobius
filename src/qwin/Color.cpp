/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * A model to represent colors with a few constant objects.
 * 
 */

#include <stdio.h>
#include <memory.h>

#include "util.h"
#include "qwin.h"
#include "UIManager.h"

QWIN_BEGIN_NAMESPACE

/****************************************************************************
 *                                                                          *
 *                                   COLOR                                  *
 *                                                                          *
 ****************************************************************************/

Color* Color::Black = new Color(0, 0, 0);
Color* Color::White = new Color(255, 255, 255);
Color* Color::Gray = new Color(128, 128, 128);
Color* Color::Red = new Color(255, 0, 0);
Color* Color::Green = new Color(0, 255, 0);
Color* Color::Blue = new Color(0, 0, 255);
Color* Color::ButtonFace = new Color(COLOR_BUTTON_FACE, true);

PUBLIC void Color::init()
{
	mHandle = NULL;
    mRgb = 0;
	mSystemCode = 0;
}

PUBLIC Color::Color(int r, int g, int b)
{
    init();

    // macro not working for some reason..
    mRgb = RGB_ENCODE(r, g, b);
}

PUBLIC Color::Color(int rgb)
{
    init();
    mRgb = rgb;
}

/**
 * Create a color for one of the "window element" constants.
 * Sigh, have to add a flag to make this different than the
 * rgb signature, but these are used less often.
 * 
 * In Windows, can use a system brush which are cached
 * rather than allocating a new one.  These must never
 * be destroyed.  
 */
PUBLIC Color::Color(int code, bool system)
{
    init();
    if (!system)
	  mRgb = code;
	else { 	
		mSystemCode = code;
		mRgb = UIManager::getSystemRgb(code);
    }
}

PUBLIC NativeColor* Color::getNativeColor()
{
    if (mHandle == NULL)
      mHandle = UIManager::getColor(this);
    return mHandle;
}

PUBLIC int Color::getRgb() 
{
    return mRgb;
}

PUBLIC int Color::getSystemCode()
{
    return mSystemCode;
}

/**
 * Change an rgb value after construction.
 * This should only be used for Color objects managed by
 * an application, such as within a Palette.  You should
 * not modify the static system colors.
 */
PUBLIC void Color::setRgb(int rgb)
{
	if (rgb != (int)mRgb) {
		mRgb = rgb;
		if (mHandle != NULL)
		  mHandle->setRgb(mRgb);
	}
}

/**
 * Various docs indiciate that its a good idea to free brushes
 * you create with CreateSolidBrush, but I'm not sure what the penalty
 * is for not doing that.  Surely these can't leek outside
 * the process boundary??
 * Be careful not to delete these if they have been selected
 * into a device context.
 */
PUBLIC Color::~Color()
{
	delete mHandle;
}

QWIN_END_NAMESPACE

/****************************************************************************
 *                                                                          *
 *   							WINDOWS COLOR                               *
 *                                                                          *
 ****************************************************************************/

#ifdef _WIN32
#include <windows.h>
#include "UIWindows.h"
QWIN_BEGIN_NAMESPACE

PUBLIC WindowsColor::WindowsColor(Color* c)
{
	// assuming that Color's RGB value is compatible with Windows
	mColor = c;
    mBrush = NULL;
	for (int i = 0 ; i < MAX_PEN_WIDTH ; i++)
	  mPens[i] = NULL;
}

/**
 * Various docs indiciate that its a good idea to free brushes
 * you create with CreateSolidBrush, but I'm not sure what the penalty
 * is for not doing that.  Surely these can't leek outside
 * the process boundary??
 * Be careful not to delete these if they have been selected
 * into a device context.
 */
PUBLIC WindowsColor::~WindowsColor()
{
    if (mBrush != NULL && mColor->getSystemCode() == 0)
	  DeleteObject(mBrush);
}

/**
 * Change an rgb value after construction.
 * This should only be used for Color objects managed by
 * an application, such as within a Palette.  You should
 * not modify the static system colors.
 */
PUBLIC void WindowsColor::setRgb(int rgb)
{
	// delete brush if not a system color
	if (mBrush != NULL && mColor->getSystemCode() == 0)
	  DeleteObject(mBrush);
	mBrush = NULL;

	for (int i = 0 ; i < MAX_PEN_WIDTH ; i++) {
		if (mPens[i] != NULL)
		  DeleteObject(mPens[i]);
		mPens[i] = NULL;
	}
}

/**
 * Return a handle to a brush.
 * We create these during construction.
 *
 */
HBRUSH WindowsColor::getBrush() 
{
    if (mBrush == NULL) {
		int code = mColor->getSystemCode();
		// can optimize on shared brushes for system colors
		if (code == COLOR_BUTTON_FACE)
		  mBrush = GetSysColorBrush(COLOR_BTNFACE);
		else
		  mBrush = CreateSolidBrush(mColor->getRgb());
	}
    return mBrush;
}

/**
 * Return a handle to a pen.
 */
PUBLIC HPEN WindowsColor::getPen()
{
	return getPen(2);
}

PUBLIC HPEN WindowsColor::getPen(int width)
{
	HPEN pen = NULL;

	if (width <= MAX_PEN_WIDTH) {
		pen = mPens[width];
		if (pen == NULL) {
			pen = CreatePen(PS_SOLID, width, mColor->getRgb());
			mPens[width] = pen;
		}
	}
	else {
		// rare, will leak!
		pen = CreatePen(PS_SOLID, width, mColor->getRgb());
	}

    return pen;
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
QWIN_BEGIN_NAMESPACE

/**
 * Color values have ranged from 0 to 255 so they can be stored
 * as a single int.  Here we have to scale up to RGBColor's 
 * range from 0 to 65535.  I don't like the loss of resolution, 
 * rework Color and Palette to handle greater depth!
 */
PUBLIC MacColor::MacColor(Color* c)
{
	mColor = c;
	setRgb(c->getRgb());
}

PUBLIC MacColor::MacColor()
{
	setRgb(0);
}

PUBLIC RGBColor* MacColor::getRGBColor() 
{
	return &mRGBColor;
}

PUBLIC void MacColor::getRGBColor(RGBColor* color) 
{
	color->red = mRGBColor.red;
	color->green = mRGBColor.green;
	color->blue = mRGBColor.blue;
}

PUBLIC CGFloat MacColor::getRed()
{
	return mRed;
}

PUBLIC CGFloat MacColor::getGreen()
{
	return mGreen;
}

PUBLIC CGFloat MacColor::getBlue()
{
	return mBlue;
}

PUBLIC CGFloat MacColor::getAlpha()
{
	return mAlpha;
}

/**
 * Various docs indiciate that its a good idea to free brushes
 * you create with CreateSolidBrush, but I'm not sure what the penalty
 * is for not doing that.  Surely these can't leek outside
 * the process boundary??
 * Be careful not to delete these if they have been selected
 * into a device context.
 */
PUBLIC MacColor::~MacColor()
{
}

/**
 * Change an rgb value after construction.
 * This should only be used for Color objects managed by
 * an application, such as within a Palette.  You should
 * not modify the static system colors.
 * 
 * Rgb is in the windows format of 3 4 bit nibbles from 0-255.
 * Not sure what the order is.
 */
PUBLIC void MacColor::setRgb(int rgb)
{

	// color values range from 0 to 65535, have to scale up from 0-255
	int red = RGB_GET_RED(rgb);
	int green = RGB_GET_GREEN(rgb);
	int blue = RGB_GET_BLUE(rgb);

	mRGBColor.red = RGB_WIN_TO_MAC(red);
	mRGBColor.green = RGB_WIN_TO_MAC(green);
	mRGBColor.blue = RGB_WIN_TO_MAC(blue);

	// the CG values range from 0.0 to 1.0
	mRed = RGB_WIN_TO_FLOAT(red);
	mGreen = RGB_WIN_TO_FLOAT(green);
	mBlue = RGB_WIN_TO_FLOAT(blue);

	// no way to specify this yet
	mAlpha = 1.0f;
}


QWIN_END_NAMESPACE
#endif // OSX

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
