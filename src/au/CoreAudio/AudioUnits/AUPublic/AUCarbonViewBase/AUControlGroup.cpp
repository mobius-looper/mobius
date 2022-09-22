/*	Copyright: 	© Copyright 2005 Apple Computer, Inc. All rights reserved.

	Disclaimer:	IMPORTANT:  This Apple software is supplied to you by Apple Computer, Inc.
			("Apple") in consideration of your agreement to the following terms, and your
			use, installation, modification or redistribution of this Apple software
			constitutes acceptance of these terms.  If you do not agree with these terms,
			please do not use, install, modify or redistribute this Apple software.

			In consideration of your agreement to abide by the following terms, and subject
			to these terms, Apple grants you a personal, non-exclusive license, under AppleÕs
			copyrights in this original Apple software (the "Apple Software"), to use,
			reproduce, modify and redistribute the Apple Software, with or without
			modifications, in source and/or binary forms; provided that if you redistribute
			the Apple Software in its entirety and without modifications, you must retain
			this notice and the following text and disclaimers in all such redistributions of
			the Apple Software.  Neither the name, trademarks, service marks or logos of
			Apple Computer, Inc. may be used to endorse or promote products derived from the
			Apple Software without specific prior written permission from Apple.  Except as
			expressly stated in this notice, no other rights or licenses, express or implied,
			are granted by Apple herein, including but not limited to any patent rights that
			may be infringed by your derivative works or by other works in which the Apple
			Software may be incorporated.

			The Apple Software is provided by Apple on an "AS IS" basis.  APPLE MAKES NO
			WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED
			WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS FOR A PARTICULAR
			PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND OPERATION ALONE OR IN
			COMBINATION WITH YOUR PRODUCTS.

			IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR
			CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
			GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
			ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION, MODIFICATION AND/OR DISTRIBUTION
			OF THE APPLE SOFTWARE, HOWEVER CAUSED AND WHETHER UNDER THEORY OF CONTRACT, TORT
			(INCLUDING NEGLIGENCE), STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN
			ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
/*=============================================================================
	AUControlGroup.cpp
	
	
	Revision 1.23  2005/02/02 16:15:54  luke
	need to set control size to small BEFORE adding menus to it (rdar://item/3975252)
	
	Revision 1.22  2005/01/29 02:07:53  bills
	dont have disable menus
	
	Revision 1.21  2005/01/05 22:04:25  luke
	AU's w/o MFR name shouldn't truncate first 2 char's of AU Name [3932208]
	
	Revision 1.20  2004/11/23 19:50:19  luke
	use small popup & disable it if it's not writable
	
	Revision 1.19  2004/11/16 00:15:50  bills
	fix size of text offset
	
	Revision 1.18  2004/10/20 01:18:52  luke
	sync with changes to AUCarbonViewControl
	
	Revision 1.17  2004/09/28 16:59:14  luke
	allow popups to size-to-fit
	
	Revision 1.16  2004/09/20 16:12:12  luke
	fix [3805096]
	
	Revision 1.15  2004/08/04 19:48:27  bills
	don't use context name string
	
	Revision 1.14  2004/07/10 01:53:32  bills
	make CAAUParameter const
	
	Revision 1.13  2004/02/18 01:36:14  luke
	update to sync with changes ParamValueStrings and make text box larger for generic param values
	
	Revision 1.12  2003/12/15 20:25:45  luke
	Fully localized
	
	Revision 1.11  2003/12/01 18:40:53  luke
	AUVParameter -> CAAUParameter
	
	Revision 1.10  2003/11/19 21:05:41  luke
	uninitialized handle fix [3485369]
	
	Revision 1.9  2003/08/14 00:06:41  mhopkins
	Radar 3376853: CreateLabelledSlider() now checks to see if min an max values have value names
	
	Revision 1.8  2003/07/31 01:59:03  bills
	tweaks to view layouts (small sliders) and clumps!
	
	Revision 1.7  2003/05/02 00:30:55  bills
	32BitControl
	
	Revision 1.6  2002/11/02 00:01:55  mhopkins
	Fixes for window compositing
	
	Revision 1.5  2002/10/31 18:32:54  mhopkins
	Fixed Radar: #3082538 (Finally)
	
	Revision 1.4  2002/10/31 02:28:13  bills
	have a workaround for negative values for named params
	
	Revision 1.3  2002/10/05 00:42:42  bills
	fix vertical sliders
	
	Revision 1.2  2002/06/16 01:18:34  bills
	add handling of indexed params (inc. named ones - with popup menu)
	
	Revision 1.1  2002/05/04 22:01:31  dwyatt
	initial checkin
	
	created Thu May 02 2002, Doug Wyatt
	Copyright (c) 2002 Apple Computer, Inc.  All Rights Reserved

	$NoKeywords: $
=============================================================================*/

#include <Carbon/Carbon.h>
#include "AUCarbonViewBase.h"
#include "AUCarbonViewControl.h"
#include "AUControlGroup.h"
#include "AUViewLocalizedStringKeys.h"

#define kSliderThinDimension 10
#define kLabelAndSliderSpacing	4

static CFStringRef kStringManufacturer = kAUViewLocalizedStringKey_Manufacturer;
static bool sLocalized = false;

void	AUControlGroup::CreateLabelledSlider(
										AUCarbonViewBase *			auView, 
										const CAAUParameter &		auvp, 
										const Rect &				area, 
										Point 						labelSize, 
										const ControlFontStyleRec &	inFontStyle)
{
	ControlFontStyleRec fontStyle = inFontStyle;
	Rect minValRect, maxValRect, sliderRect;
	ControlRef newControl;
	int width = area.right - area.left, height = area.bottom - area.top;
	CFStringRef cfstr;
	int sliderValueMax, sliderValueMin, sliderValueDefault;
	AUCarbonViewControl::ControlType sliderType;
	
	bool horizontal = (width > height);

	if (horizontal) {
		maxValRect.top = minValRect.top = area.top + (height - labelSize.v) / 2;
		minValRect.left = area.left;
		maxValRect.left = area.right - labelSize.h;
		
		minValRect.bottom = minValRect.top + labelSize.v;
		minValRect.right = minValRect.left + labelSize.h;
		maxValRect.bottom = maxValRect.top + labelSize.v;
		maxValRect.right = maxValRect.left + labelSize.h;

		sliderRect.left = minValRect.right + kLabelAndSliderSpacing;
		sliderRect.right = maxValRect.left - kLabelAndSliderSpacing;
		sliderRect.top = area.top + (height - kSliderThinDimension) / 2;
		sliderRect.bottom = sliderRect.top + kSliderThinDimension + 4;

		if (auvp.IsIndexedParam ()) {
			sliderValueMin = sliderValueDefault = int(auvp.ParamInfo().minValue);
			sliderValueMax = int(auvp.ParamInfo().maxValue);
			sliderType = AUCarbonViewControl::kTypeDiscrete;
		} else {
			sliderValueMin = sliderValueDefault = 0;
			sliderValueMax = sliderRect.right - sliderRect.left;
			sliderType = AUCarbonViewControl::kTypeContinuous;
		}
	} else {
		maxValRect.left = minValRect.left = area.left + (width - labelSize.h) / 2;
		maxValRect.top = area.top;
		minValRect.top = area.bottom - labelSize.v;
		
		minValRect.bottom = minValRect.top + labelSize.v;
		minValRect.right = minValRect.left + labelSize.h;
		maxValRect.bottom = maxValRect.top + labelSize.v;
		maxValRect.right = maxValRect.left + labelSize.h;
	
		sliderRect.left = area.left + (width - kSliderThinDimension) / 2;
		sliderRect.right = sliderRect.left + kSliderThinDimension + 4;
		sliderRect.top = maxValRect.bottom + kLabelAndSliderSpacing;
		sliderRect.bottom = minValRect.top - kLabelAndSliderSpacing;

		if (auvp.IsIndexedParam ()) {
			sliderValueMin = sliderValueDefault = int(auvp.ParamInfo().minValue);
			sliderValueMax = int(auvp.ParamInfo().maxValue);			
			sliderType = AUCarbonViewControl::kTypeDiscrete;
		} else {
			sliderValueMin = sliderValueDefault = 0;
			sliderValueMax = sliderRect.bottom - sliderRect.top;
			sliderType = AUCarbonViewControl::kTypeContinuous;
		}
	}

	// minimum value label
	if (labelSize.v > 0 && labelSize.h > 0) {
		// check to see if the minimum value has a label
		cfstr = auvp.GetStringFromValueCopy(&auvp.ParamInfo().minValue);
		fontStyle.just = horizontal ? teFlushRight : teCenter;
		verify_noerr(CreateStaticTextControl(auView->GetCarbonWindow(), &minValRect, cfstr, &fontStyle, &newControl));
		CFRelease(cfstr);
		verify_noerr(auView->EmbedControl(newControl));
	
		// maximum value label
		cfstr = auvp.GetStringFromValueCopy(&auvp.ParamInfo().maxValue);
		fontStyle.just = horizontal ? teFlushLeft : teCenter;
		verify_noerr(CreateStaticTextControl(auView->GetCarbonWindow(), &maxValRect, cfstr, &fontStyle, &newControl));
		CFRelease(cfstr);
		verify_noerr(auView->EmbedControl(newControl));
	}
	
	// slider
	verify_noerr(CreateSliderControl(auView->GetCarbonWindow(), &sliderRect, sliderValueDefault, sliderValueMin, sliderValueMax, kControlSliderDoesNotPoint, 0, true, AUCarbonViewControl::SliderTrackProc, &newControl));


	ControlSize small = kControlSizeSmall;
	SetControlData(newControl, kControlEntireControl, kControlSizeTag, sizeof(ControlSize), &small);
	auView->AddCarbonControl(sliderType, auvp, newControl);
}

void	AUControlGroup::CreateLabelledSliderAndEditText(
										AUCarbonViewBase *			auView, 
										const CAAUParameter &		auvp, 
										const Rect &				area, 
										Point 						labelSize, 
										Point						editTextSize,
										const ControlFontStyleRec &	inFontStyle)
{
	ControlFontStyleRec fontStyle = inFontStyle;
	Rect sliderArea, textArea;
	ControlRef newControl;
	int width = area.right - area.left, height = area.bottom - area.top;
	
	bool horizontal = (width > height);

	sliderArea = area;
	textArea = area;
	if (horizontal) {
		textArea.left = area.right - editTextSize.h;
			// provide a large text box if param is generic and its values have strings...
		if (auvp.ValuesHaveStrings() && (auvp.ParamInfo().unit == kAudioUnitParameterUnit_Generic))
		{
			textArea.right += 30;
		}
		sliderArea.right = textArea.left - kLabelAndSliderSpacing;
		textArea.top = area.top + (height - editTextSize.v) / 2;
		textArea.bottom = textArea.top + editTextSize.v;
	} else {
		textArea.top = area.bottom - editTextSize.v;
		sliderArea.bottom = textArea.top - kLabelAndSliderSpacing;
		textArea.left = area.left + (width - editTextSize.h) / 2;
		textArea.right = textArea.left + editTextSize.h;
	}
	CreateLabelledSlider(auView, auvp, sliderArea, labelSize, fontStyle);
	
	verify_noerr(CreateEditUnicodeTextControl(auView->GetCarbonWindow(), &textArea, CFSTR(""), false, 
			&fontStyle, &newControl));
	auView->AddCarbonControl(AUCarbonViewControl::kTypeText, auvp, newControl);
}

void	AUControlGroup::CreatePopupMenu (AUCarbonViewBase *			auView, 
										const CAAUParameter &		auvp, 
										const Rect &				area, 
										const ControlFontStyleRec &	inFontStyle,
										const bool					inSizeToFit)
{
	ControlRef thePopUp;
			
	verify_noerr(CreatePopupButtonControl (auView->GetCarbonWindow(), &area, NULL, 
												-12345,	// DON'T GET MENU FROM RESOURCE mMenuID
												FALSE,	// variableWidth, 
												0,		// titleWidth, 
												0,		// titleJustification, 
												0,		// titleStyle, 
												&thePopUp));
	
	ControlSize small = kControlSizeSmall;
	SetControlData(thePopUp, kControlEntireControl, kControlSizeTag, sizeof(ControlSize), &small);
	
	MenuRef menuRef;
	verify_noerr(CreateNewMenu( 1, 0, &menuRef));
	
	for (int i = 0; i < auvp.GetNumIndexedParams(); ++i) {
		verify_noerr(AppendMenuItemTextWithCFString (menuRef, auvp.GetParamName(i), kMenuItemAttrIgnoreMeta, 0, 0));
	}
	
	verify_noerr(SetControlData(thePopUp, 0, kControlPopupButtonMenuRefTag, sizeof(menuRef), &menuRef));
	SetControl32BitMaximum(thePopUp, auvp.GetNumIndexedParams());

	verify_noerr (SetControlFontStyle (thePopUp, &inFontStyle));
	
	if (inSizeToFit) {
		AUCarbonViewControl::SizeControlToFit(thePopUp);
	}
	
	auView->AddCarbonControl(AUCarbonViewControl::kTypeDiscrete, auvp, thePopUp);
}

void	AUControlGroup::AddAUInfo (		AUCarbonViewBase *			auView, 
										const Point &				inLocation, 
										const SInt16 				inRightOffset,
										const SInt16				inTotalWidth)
{
    // get component info
	ComponentDescription desc;
    Handle h1 = NewHandleClear(4);
	OSStatus err = GetComponentInfo ((Component)auView->GetEditAudioUnit(), &desc, h1, 0, 0);
    
    if (err == noErr) {        
        // Get the manufacturer's name... look for the ':' character convention
        HLock(h1);
        char* ptr1 = *h1;
        int len = *ptr1++;
        char* displayStr = 0;
        
        for (int i = 0; i < len; ++i) {
            if (ptr1[i] == ':') { // found the name
                ptr1[i++] = 0;
                displayStr = ptr1;
                break;
            }
        }
        
        // localize as necessary:
        if (!sLocalized) {
            CFBundleRef mainBundle = CFBundleGetBundleWithIdentifier(kLocalizedStringBundle_AUView);
            if (mainBundle) {
                kStringManufacturer = CFCopyLocalizedStringFromTableInBundle(
                                            kAUViewLocalizedStringKey_Manufacturer, kLocalizedStringTable_AUView,
                                            mainBundle, CFSTR("Manufacturer title string"));
                sLocalized = true;
            }
        }
        
        // display strings
        ControlRef newControl;
        Rect r;
        r.top = SInt16(inLocation.v);		r.bottom = SInt16(inLocation.v) + 16;
        ControlFontStyleRec fontStyle;
        fontStyle.flags = kControlUseFontMask | kControlUseJustMask;
        fontStyle.font = kControlFontSmallBoldSystemFont;
        
        // display manufacturer string
        if (displayStr) {
            CFMutableStringRef mfrstring = CFStringCreateMutable(NULL, 0);
            CFStringAppend(mfrstring, kStringManufacturer);		// "Manufacturer"
            CFStringAppend(mfrstring, kAUViewUnlocalizedString_TitleSeparator);
                                                                // "Manufacturer: "
            CFStringRef mfrname = CFStringCreateWithCString(NULL, displayStr, kCFStringEncodingUTF8);
            if (mfrname) {
                CFStringAppend(mfrstring, mfrname);	// "Manufacturer: MFRName"
                CFRelease (mfrname);
            }
            
            r.left = inLocation.h + inRightOffset;	
			r.right = inLocation.h + inTotalWidth - 28;
			fontStyle.just = teFlushRight;
            
            verify_noerr(CreateStaticTextControl(auView->GetCarbonWindow(), &r, mfrstring, &fontStyle, &newControl));
            verify_noerr(auView->EmbedControl(newControl));
            CFRelease (mfrstring);
                                        
            //move displayStr ptr past the manu, to the name
            // we move the characters down an index, because the handle doesn't have any room
            // at the end for the \0
			int i = strlen(displayStr), j = 0;
			while (displayStr[++i] == ' ' && i < len)
				;
			while (i < len)
				displayStr[j++] = displayStr[i++];
			displayStr[j] = 0;
        } else {
			displayStr = ptr1;
			int i = 0, j = 0;
			do {
				displayStr[j] = displayStr[i];
				++j; ++i;
			} while (i < len);
			
			displayStr[j] = 0;
        }
        
        // display AudioUnit string
        r.left = inLocation.h;	r.right = r.left + inRightOffset;
        fontStyle.just = 0;
        
        CFMutableStringRef cfstr = CFStringCreateMutable(NULL, 0);
        CFStringAppend(cfstr, kAUViewLocalizedStringKey_AudioUnit);		// "Audio Unit"
        CFStringAppend(cfstr, kAUViewUnlocalizedString_TitleSeparator);
                                                        // "Audio Unit: "
        
		CFStringRef auname = CFStringCreateWithCString(NULL, displayStr, kCFStringEncodingUTF8);
		CFStringAppend(cfstr, auname);				// "Audio Unit: AUName"
		CFRelease (auname);
        
        verify_noerr(CreateStaticTextControl(auView->GetCarbonWindow(), &r, cfstr, &fontStyle, &newControl));
		
		// size text control correctly
		Boolean bValue = false;
		SetControlData(newControl, kControlEntireControl, 'stim' /* kControlStaticTextIsMultilineTag */, sizeof(Boolean), &bValue);
		SInt16 baseLineOffset;
		Rect bestRect;
		err = GetBestControlRect(newControl, &bestRect, &baseLineOffset);  
		if (err == noErr)
		{
			int width = (bestRect.right - bestRect.left) + 1;
			int height = (bestRect.bottom - bestRect.top) + 1;
			SizeControl (newControl, width, height);
		}
		
        verify_noerr(auView->EmbedControl(newControl));
        CFRelease (cfstr);
    }
    
	DisposeHandle (h1);
}


