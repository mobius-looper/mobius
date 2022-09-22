/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Swing has a typically over-engineered ColorChooser hiearchy.
 * Here we just have one class that encapsulates a native dialog.
 * 
 */

#include <stdio.h>
#include <memory.h>

#include "Util.h"
#include "QwinExt.h"
#include "UIManager.h"

QWIN_BEGIN_NAMESPACE

/****************************************************************************
 *                                                                          *
 *                                COLOR DIALOG                              *
 *                                                                          *
 ****************************************************************************/

PUBLIC ColorDialog::ColorDialog(Window* parent) :
    SystemDialog(parent)
{
    mRgb = 0;
}

PUBLIC ColorDialog::~ColorDialog()
{
}

PUBLIC void ColorDialog::setRgb(int rgb)
{
    mRgb = rgb;
}

PUBLIC int ColorDialog::getRgb()
{
    return mRgb;
}

/**
 * Show (rather than open) is what is usually used to open a 
 * synchronous dialog.
 */
PUBLIC bool ColorDialog::show()
{
	mCanceled = false;
    
    SystemDialogUI* ui = UIManager::getColorDialogUI(this);

    // this will turn around and call our setRgb and setCanceled methods,
    // don't really like that but it isn't worth duplicating
    ui->show();

    delete ui;

	return !mCanceled;
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

PUBLIC WindowsColorDialog::WindowsColorDialog(ColorDialog* cd)
{
    mDialog = cd;
}

PUBLIC WindowsColorDialog::~WindowsColorDialog()
{
}

PUBLIC void WindowsColorDialog::show()
{
	CHOOSECOLOR cc;
	static COLORREF acrCustClr[16]; // array of custom colors 

    HWND parent = NULL;
    Window* pw = mDialog->getParent();
    if (pw != NULL)
      parent = WindowsComponent::getHandle(pw);

	ZeroMemory(&cc, sizeof(cc));
	cc.lStructSize = sizeof(cc);
	cc.hwndOwner = parent;
	cc.lpCustColors = (LPDWORD) acrCustClr;
	cc.rgbResult = mDialog->getRgb();
	cc.Flags = CC_FULLOPEN | CC_RGBINIT;
 
	if (ChooseColor(&cc) != TRUE)
      mDialog->setCanceled(true);
    else
      mDialog->setRgb(cc.rgbResult);
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

PUBLIC MacColorDialog::MacColorDialog(ColorDialog* cd)
{
    mDialog = cd;
}

PUBLIC MacColorDialog::~MacColorDialog()
{
}

PUBLIC void MacColorDialog::show()
{
	NColorPickerInfo info;

	// carbondev example does this which seems to indicate 
	// that zero is your friend
	memset(&info, 0, sizeof(NColorPickerInfo));

	// reference says "Make sure that you set theColor.profile
	// field ...to the color space that you want the color returned in.
	// For the older PickColor setting this to NULL causes it to use
	// "the default system profile".    CMProfileRefs can be had from
	// the "ColorSync Manager"
	info.theColor.profile = NULL;
	
	// you can set this if you want to initialize the color
	// of course this is a union of 15 things and it isn't at all
	// clear how you say which union field to use, it may be part of
	// the "color space" defined by the "profile"?
	// The carbondev example just uses rgb so I guess this is the system default.

	int rgb = mDialog->getRgb();
	int red = RGB_GET_RED(rgb);
	int green = RGB_GET_GREEN(rgb);
	int blue = RGB_GET_BLUE(rgb);

	// scale up from 0-255 to 0-65535
	red = RGB_WIN_TO_MAC(red);
	green = RGB_WIN_TO_MAC(green);
	blue = RGB_WIN_TO_MAC(blue);

	info.theColor.color.rgb.red = red;
	info.theColor.color.rgb.green = green;
	info.theColor.color.rgb.blue = blue;

	// "The destination profile"
	// "A handle to a ColorSync 1.0 profile for the final output device
	//  to use the default system profile set this to NULL"
	info.dstProfile = NULL;

	info.flags = 
		kColorPickerDialogIsMoveable | 
		kColorPickerDialogIsModal;

	// kAtSpecifiedOrigin with info.dialogOrigin
	// kDeepestColorScreen
	info.placeWhere = kCenterOnMainScreen;

	// The component subtype of the initial color picker. 
	// If your application sets this field to 0, the default color picker is used
	// (that is, the last color picker chosen by the user). 
	// When PickColor returns, this field contains the component subtype of
	// the color picker that was chosen when the user closed the color picker dialog box.
	info.pickerType = 0;
	
	// don't think we need these if we use a modal?
	info.eventProc = NULL;
	info.colorProc = NULL;
	info.colorProcData = (URefCon)NULL;
	
	// this is a Str255 prompt 
	// the default is "Colors" which is good enough
	//info.prompt = ...some fucking char* to Str255 conversion
	
	// this is a PickerMenuItemInfo "that allows the application
	// to specify an Edit menu for use when a color picker
	// dialog box is displayed"
	//info.mInfo...
	
	OSErr err = NPickColor(&info);
	if (CheckErr(err, "MacColorDialog::NPickColor")) {
		
		if (!info.newColorChosen) {
			mDialog->setCanceled(true);
		}
		else {
			// the range is 0 to 65535
			int red = RGB_MAC_TO_WIN(info.theColor.color.rgb.red);
			int green = RGB_MAC_TO_WIN(info.theColor.color.rgb.green);
			int blue = RGB_MAC_TO_WIN(info.theColor.color.rgb.blue);

			//printf("ColorDialog: red %d green %d blue %d\n", red, green, blue);

			mDialog->setRgb(RGB_ENCODE(red, green, blue));
		}
	}
}

QWIN_END_NAMESPACE
#endif

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
