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
	AUCarbonViewControl.cpp
	
	
	Revision 1.45  2005/02/25 23:06:26  bills
	don't draw
	
	Revision 1.44  2005/02/24 02:13:03  bills
	add IsCompositWindow method
	
	Revision 1.43  2005/02/24 01:22:10  bills
	remove DrawOneControl - doesn't seem to be needed
	
	Revision 1.42  2005/02/05 01:05:54  bills
	changes to property notification handling
	
	Revision 1.41  2005/02/01 19:05:04  luke
	fix gcc-4.0 warning
	
	Revision 1.40  2005/01/29 02:02:27  bills
	changes for static text values (read only params)
	
	Revision 1.39  2004/11/29 20:26:48  luke
	remove vestigal bypassEffectControl code
	
	Revision 1.38  2004/10/22 21:47:25  luke
	fix updater problem
	
	Revision 1.37  2004/10/20 22:50:10  luke
	implement parameter-setting for write-only boolean parameters
	
	Revision 1.36  2004/10/20 01:18:34  luke
	simply SizeControlToFit method
	
	Revision 1.35  2004/09/29 21:58:45  luke
	remove defined const
	
	Revision 1.34  2004/09/23 23:35:15  luke
	improve layout process
	
	Revision 1.33  2004/09/20 06:00:34  luke
	add property control height calculation
	
	Revision 1.32  2004/02/18 01:36:42  luke
	allow text input for param value names
	
	Revision 1.31  2004/02/09 19:36:19  luke
	initialize mInControlInitialization
	
	Revision 1.30  2004/01/09 23:06:20  luke
	fix build
	
	Revision 1.29  2004/01/07 22:27:33  dwyatt
	fix logic controlling when we ignore callbacks->control value updates -- should only be disabled during initialization, not based on the assumption that the first callback should be ignored
	
	Revision 1.28  2003/12/15 21:12:27  luke
	don't release string until after using it.  duh.
	
	Revision 1.27  2003/12/15 20:27:40  luke
	Fully localized
	
	Revision 1.26  2003/12/01 18:40:53  luke
	AUVParameter -> CAAUParameter
	
	Revision 1.25  2003/09/05 21:11:54  bills
	remove printf
	
	Revision 1.24  2003/08/28 21:27:41  bills
	if user preset is loaded, set preset menu to blank
	
	Revision 1.23  2003/08/27 23:03:49  mhopkins
	some fixes [bills]
	
	Revision 1.22  2003/08/27 22:41:43  mhopkins
	Radar 3396802: Changing Multiband Compressor factory preset menu doesn't change the Advanced UI menu
	
	Revision 1.21  2003/08/18 21:21:33  mhopkins
	Radar 3382301- fixed initial values of indexed parameters
	
	Revision 1.20  2003/08/06 21:20:25  mhopkins
	Radar 3364734: Cleanup of CF Properties
	
	Revision 1.19  2003/07/25 22:32:02  mhopkins
	Radar #3299718: Rounding errors in Generic UI view when the view first comes up
	
	Revision 1.18  2003/05/02 00:30:33  bills
	fix handling of param-value and control value tweaks
	
	Revision 1.17  2003/02/24 18:04:56  mhopkins
	Radar 3168765: AUControls should use 32-bit control calls not 16-bit
	
	Revision 1.16  2003/02/07 17:55:53  mhopkins
	Fixed omission in Bind() for popup menu case
	
	Revision 1.15  2003/02/06 21:50:34  mhopkins
	Fixed minor problem in HandleEvent()
	
	Revision 1.14  2003/02/04 21:19:16  mhopkins
	Fixed compilation errors with CodeWarrior and fixed graphic update problem in BindControl()
	
	Revision 1.13  2002/11/04 22:07:27  mhopkins
	Fixed Radar #307079
	
	Revision 1.12  2002/10/30 18:57:01  mhopkins
	UI tweaks to AUVPresets
	
	Revision 1.11  2002/10/29 23:32:27  mhopkins
	Changed distance between label text and popup menu from 24 -> 8 pixels as per user interface guidelines, and added ':' to label
	
	Revision 1.10  2002/10/08 22:12:47  mhopkins
	Fixed Radar# 3068245
	
	Revision 1.9  2002/10/04 19:31:32  bills
	fix use of SetProperty with preset handling
	
	Revision 1.8  2002/06/25 09:40:12  bills
	add property control class and bypass effect
	
	Revision 1.7  2002/06/25 02:33:46  bills
	when doing Presets, pass in Location (not just y)
	
	Revision 1.6  2002/06/18 01:01:36  bills
	make Update aware of whether in UI thread context
	
	Revision 1.5  2002/06/17 10:19:18  bills
	first pass at dealing with AUPresets
	
	Revision 1.4  2002/06/16 01:18:34  bills
	add handling of indexed params (inc. named ones - with popup menu)
	
	Revision 1.3  2002/05/06 14:39:11  dwyatt
	add support for event listeners
	
	Revision 1.2  2002/03/13 16:15:07  dwyatt
	API review - functions renamed
	
	Revision 1.1  2002/03/07 07:31:30  dwyatt
	moved from AUViewBase and made Carbon-specific -- a Cocoa plugin is
	going to get written in ObjC so why bother with abstracting for it
	in C++
	
	Revision 1.1  2002/02/28 04:58:04  dwyatt
	initial checkin
	
	created 11 Feb 2002, 12:39, Doug Wyatt
	Copyright (c) 2002 Apple Computer, Inc.  All Rights Reserved
	
	$NoKeywords: $
=============================================================================*/

#include "AUCarbonViewControl.h"
#include "AUCarbonViewBase.h"
#include "AUViewLocalizedStringKeys.h"

AUCarbonViewControl::AUCarbonViewControl(AUCarbonViewBase *ownerView, AUParameterListenerRef listener, ControlType type, const CAAUParameter &param, ControlRef control) :
	mOwnerView(ownerView),
	mListener(listener),
	mType(type),
	mParam(param),
	mControl(control),
	mInControlInitialization(0)
{
	SetControlReference(control, SInt32(this));
}

AUCarbonViewControl::~AUCarbonViewControl()
{
	AUListenerRemoveParameter(mListener, this, &mParam);
}

AUCarbonViewControl* AUCarbonViewControl::mLastControl = NULL;

void	AUCarbonViewControl::Bind()
{
	mInControlInitialization = 1;   // true
	AUListenerAddParameter(mListener, this, &mParam);
		// will cause an almost-immediate callback
	
	EventTypeSpec events[] = {
		{ kEventClassControl, kEventControlValueFieldChanged }	// N.B. OS X only
	};
	
	WantEventTypes(GetControlEventTarget(mControl), GetEventTypeCount(events), events);

	if (mType == kTypeContinuous || mType == kTypeText || mType == kTypeDiscrete) {
		EventTypeSpec events[] = {
			{ kEventClassControl, kEventControlHit },
			{ kEventClassControl, kEventControlClick }
		};
		WantEventTypes(GetControlEventTarget(mControl), GetEventTypeCount(events), events);
	} 

	if (mType == kTypeText) {
		EventTypeSpec events[] = {
			{ kEventClassControl, kEventControlSetFocusPart }
		};
		WantEventTypes(GetControlEventTarget(mControl), GetEventTypeCount(events), events); 
		ControlKeyFilterUPP proc = mParam.ValuesHaveStrings() ? StdKeyFilterCallback : NumericKeyFilterCallback;
			// this will fail for a static text field
		SetControlData(mControl, 0, kControlEditTextKeyFilterTag, sizeof(proc), &proc);
	}
	
	Update(true);
	mInControlInitialization = 0;   // false
}

void	AUCarbonViewControl::ParameterToControl(Float32 paramValue)
{
	++mInControlInitialization;
	switch (mType) {
	case kTypeContinuous:
		SetValueFract(AUParameterValueToLinear(paramValue, &mParam));
		break;
	case kTypeDiscrete:
		{
			long value = long(paramValue);
			
			// special case [1] -- menu parameters
			if (mParam.HasNamedParams()) {
				// if we're dealing with menus they behave differently!
				// becaue setting min and max doesn't work correctly for the control value
				// first menu item always reports a control value of 1
				ControlKind ctrlKind;
				if (GetControlKind(mControl, &ctrlKind) == noErr) {
					if ((ctrlKind.kind == kControlKindPopupArrow) 
						|| (ctrlKind.kind == kControlKindPopupButton))				
					{
						value = value - long(mParam.ParamInfo().minValue) + 1;
					}
				}
			}
			
			// special case [2] -- Write-only boolean parameters
			AudioUnitParameterInfo AUPI = mParam.ParamInfo();
			
			bool isWriteOnlyBoolParameter = (	(AUPI.unit == kAudioUnitParameterUnit_Boolean) &&
												(AUPI.flags & kAudioUnitParameterFlag_IsWritable) &&
												!(AUPI.flags & kAudioUnitParameterFlag_IsReadable)	);
			if (!isWriteOnlyBoolParameter) {
				SetValue (value);
			}
		}
		break;
	case kTypeText:
		{
			CFStringRef cfstr = mParam.GetStringFromValueCopy(&paramValue);

			if ( !(mParam.ParamInfo().flags & kAudioUnitParameterFlag_IsWritable)			//READ ONLY PARAMS
					&& (mParam.ParamInfo().flags & kAudioUnitParameterFlag_IsReadable)) 
			{
				if (mParam.GetParamTag()) {
					CFMutableStringRef str = CFStringCreateMutableCopy(NULL, 256, cfstr);
					CFRelease (cfstr);
					CFStringAppend (str, CFSTR(" "));
					CFStringAppend (str, mParam.GetParamTag());
					cfstr = str;
				}
			}
			SetTextValue(cfstr);
			CFRelease (cfstr);
		}
		break;
	}
	--mInControlInitialization;
}

void	AUCarbonViewControl::ControlToParameter()
{
	if (mInControlInitialization)
		return;

	switch (mType) {
	case kTypeContinuous:
		{
			double controlValue = GetValueFract();
			Float32 paramValue = AUParameterValueFromLinear(controlValue, &mParam);
			mParam.SetValue(mListener, this, paramValue);
		}
		break;
	case kTypeDiscrete:
		{
			long value = GetValue();
			
			// special case [1] -- Menus
			if (mParam.HasNamedParams()) {
				// if we're dealing with menus they behave differently!
				// becaue setting min and max doesn't work correctly for the control value
				// first menu item always reports a control value of 1
				ControlKind ctrlKind;
				if (GetControlKind(mControl, &ctrlKind) == noErr) {
					if ((ctrlKind.kind == kControlKindPopupArrow) 
						|| (ctrlKind.kind == kControlKindPopupButton))				
					{
						value = value + long(mParam.ParamInfo().minValue) - 1;
					}
				}
			}
			
			// special case [2] -- Write-only boolean parameters
			AudioUnitParameterInfo AUPI = mParam.ParamInfo();
			
			bool isWriteOnlyBoolParameter = (	(AUPI.unit == kAudioUnitParameterUnit_Boolean) &&
												(AUPI.flags & kAudioUnitParameterFlag_IsWritable) &&
												!(AUPI.flags & kAudioUnitParameterFlag_IsReadable)	);
			if (isWriteOnlyBoolParameter) {
				value = 1;
			}
			
			mParam.SetValue (mListener, this, value);
		}
		break;
	case kTypeText:
		{
			Float32 val = mParam.GetValueFromString (GetTextValue());
			mParam.SetValue(mListener, this, (mParam.IsIndexedParam() ? (int)val : val));
			if (mParam.ValuesHaveStrings())
				ParameterToControl(val); //make sure we display the correct text (from the AU)
		}
		break;
	}
}

void	AUCarbonViewControl::SetValueFract(double value)
{
	SInt32 minimum = GetControl32BitMinimum(mControl);
	SInt32 maximum = GetControl32BitMaximum(mControl);
	SInt32 cval = SInt32(value * (maximum - minimum) + minimum + 0.5);
	SetControl32BitValue(mControl, cval);
//	printf("set: value=%lf, min=%ld, max=%ld, ctl value=%ld\n", value, minimum, maximum, cval);
}

double	AUCarbonViewControl::GetValueFract()
{
	SInt32 minimum = GetControl32BitMinimum(mControl);
	SInt32 maximum = GetControl32BitMaximum(mControl);
	SInt32 cval = GetControl32BitValue(mControl);
	double result = double(cval - minimum) / double(maximum - minimum);
//	printf("get: min=%ld, max=%ld, value=%ld, result=%f\n", minimum, maximum, cval, result);
	return result;
}

void	AUCarbonViewControl::SetTextValue(CFStringRef cfstr)
{
	verify_noerr(SetControlData(mControl, 0, kControlEditTextCFStringTag, sizeof(CFStringRef), &cfstr));
}

CFStringRef	AUCarbonViewControl::GetTextValue()
{
	CFStringRef cfstr;
	verify_noerr(GetControlData(mControl, 0, kControlEditTextCFStringTag, sizeof(CFStringRef), &cfstr, NULL));
	return cfstr;
}

void	AUCarbonViewControl::SetValue(long value)
{
	SetControl32BitValue(mControl, value);
}

long	AUCarbonViewControl::GetValue()
{
	return GetControl32BitValue(mControl);
}

bool	AUCarbonViewControl::HandleEvent(EventRef event)
{
	UInt32 eclass = GetEventClass(event);
	UInt32 ekind = GetEventKind(event);
	ControlRef control;
	bool		handled = true;
	
	switch (eclass) {
		case kEventClassControl:
		{
			AudioUnitParameterInfo AUPI = mParam.ParamInfo();
			
			bool isWriteOnlyBoolParameter = (	(AUPI.unit == kAudioUnitParameterUnit_Boolean) &&
												(AUPI.flags & kAudioUnitParameterFlag_IsWritable) &&
												!(AUPI.flags & kAudioUnitParameterFlag_IsReadable)	);
			
			switch (ekind) {
				case kEventControlSetFocusPart:	// tab
					handled = !handled;		// fall through to next case
					mLastControl = this;
				case kEventControlValueFieldChanged:
					GetEventParameter(event, kEventParamDirectObject, typeControlRef, NULL, sizeof(ControlRef), NULL, &control);
					verify(control == mControl);
					ControlToParameter();
					return handled;			
				case kEventControlClick:
					if (isWriteOnlyBoolParameter) {
						GetEventParameter(event, kEventParamDirectObject, typeControlRef, NULL, sizeof(ControlRef), NULL, &control);
						verify(control == mControl);
						ControlToParameter();
					} else if (mLastControl != this) {
						if (mLastControl != NULL) {
							mLastControl->Update(false);
						}
						mLastControl = this;	
					}
					mOwnerView->TellListener(mParam, kAudioUnitCarbonViewEvent_MouseDownInControl, NULL);
					break;	// don't return true, continue normal processing
				case kEventControlHit:
					if (mLastControl != this) {
						if (mLastControl != NULL)
							mLastControl->Update(false);
						mLastControl = this;	
					} 
					mOwnerView->TellListener(mParam, kAudioUnitCarbonViewEvent_MouseUpInControl, NULL);
					break;	// don't return true, continue normal processing
			}
		}
	}
	return !handled;
}

pascal void	AUCarbonViewControl::SliderTrackProc(ControlRef theControl, ControlPartCode partCode)
{
	// this doesn't need to actually do anything
//	AUCarbonViewControl *This = (AUCarbonViewControl *)GetControlReference(theControl);
}

pascal ControlKeyFilterResult	AUCarbonViewControl::StdKeyFilterCallback(ControlRef theControl, 
												SInt16 *keyCode, SInt16 *charCode, 
												EventModifiers *modifiers)
{
	SInt16 c = *charCode;
	if (c >= ' ' || c == '\b' || c == 0x7F || (c >= 0x1c && c <= 0x1f) || c == '\t')
		return kControlKeyFilterPassKey;
	if (c == '\r' || c == 3) {	// return or Enter
		AUCarbonViewControl *This = (AUCarbonViewControl *)GetControlReference(theControl);
		ControlEditTextSelectionRec sel = { 0, 32767 };
		SetControlData(This->mControl, 0, kControlEditTextSelectionTag, sizeof(sel), &sel);
		This->ControlToParameter();
	}
	return kControlKeyFilterBlockKey;
}

pascal ControlKeyFilterResult	AUCarbonViewControl::NumericKeyFilterCallback(ControlRef theControl, 
												SInt16 *keyCode, SInt16 *charCode, 
												EventModifiers *modifiers)
{
	SInt16 c = *charCode;
	if (isdigit(c) || c == '+' || c == '-' || c == '.' || c == '\b' || c == 0x7F || (c >= 0x1c && c <= 0x1f)
	|| c == '\t')
		return kControlKeyFilterPassKey;
	if (c == '\r' || c == 3) {	// return or Enter
		AUCarbonViewControl *This = (AUCarbonViewControl *)GetControlReference(theControl);
		ControlEditTextSelectionRec sel = { 0, 32767 };
		SetControlData(This->mControl, 0, kControlEditTextSelectionTag, sizeof(sel), &sel);
		This->ControlToParameter();
	}
	return kControlKeyFilterBlockKey;
}

Boolean	AUCarbonViewControl::SizeControlToFit(ControlRef inControl, SInt16 *outWidth, SInt16 *outHeight)
{
	if (inControl == 0) return false;
	
	Boolean bValue = false;
	// this only works on text controls -- returns an error for other controls, but doesn't do anything,
	// so the error is irrelevant
	SetControlData(inControl, kControlEntireControl, 'stim' /* kControlStaticTextIsMultilineTag */, sizeof(Boolean), &bValue);
	
	SInt16 baseLineOffset;
	Rect bestRect;
	OSErr err = GetBestControlRect(inControl, &bestRect, &baseLineOffset);  
	if (err != noErr) return false;
	
	int width = (bestRect.right - bestRect.left) + 1;
	int height = (bestRect.bottom - bestRect.top) + 1;
	
	Rect boundsRect;
	GetControlBounds (inControl, &boundsRect);
	
	Rect newRect;
	newRect.top = boundsRect.top;
	newRect.bottom = newRect.top + height;
	newRect.left = boundsRect.left;
	newRect.right = newRect.left + width;
	
	SetControlBounds (inControl, &newRect);
	
	if (outWidth)
		*outWidth = width;
	
	if (outHeight)
		*outHeight = height;
	
	return true;
}

#pragma mark ___AUPropertyControl
bool	AUPropertyControl::HandleEvent(EventRef event)
{	
	UInt32 eclass = GetEventClass(event);
	UInt32 ekind = GetEventKind(event);
	switch (eclass) {
	case kEventClassControl:
		switch (ekind) {
		case kEventControlValueFieldChanged:
			HandleControlChange();
			return true;	// handled
		}
	}

	return false;
}

void	AUPropertyControl::RegisterEvents ()
{
	EventTypeSpec events[] = {
		{ kEventClassControl, kEventControlValueFieldChanged }	// N.B. OS X only
	};
	
	WantEventTypes(GetControlEventTarget(mControl), GetEventTypeCount(events), events);
}

void	AUPropertyControl::EmbedControl (ControlRef theControl) 
{ 
	mView->EmbedControl (theControl); 
}

WindowRef 	AUPropertyControl::GetCarbonWindow()
{
	return mView->GetCarbonWindow();
}

#pragma mark ___AUVPreset
static CFStringRef kStringFactoryPreset = kAUViewLocalizedStringKey_FactoryPreset;
static bool sAUVPresetLocalized = false;

AUVPresets::AUVPresets (AUCarbonViewBase* 		inParentView, 
						CFArrayRef& 			inPresets,
						Point 					inLocation, 
						int 					nameWidth, 
						int 					controlWidth, 
						ControlFontStyleRec & 	inFontStyle)
	: AUPropertyControl (inParentView),
	  mPresets (inPresets),
	  mView (inParentView)
{
	Rect r;
	
	// ok we now have an array of factory presets
	// get their strings and display them

	r.top = inLocation.v;		r.bottom = r.top;
	r.left = inLocation.h;		r.right = r.left;
	
    // localize as necessary
    if (!sAUVPresetLocalized) {
        CFBundleRef mainBundle = CFBundleGetBundleWithIdentifier(kLocalizedStringBundle_AUView);
        if (mainBundle) {
            kStringFactoryPreset =	CFCopyLocalizedStringFromTableInBundle(
                                        kAUViewLocalizedStringKey_FactoryPreset, kLocalizedStringTable_AUView,
                                        mainBundle, CFSTR("FactoryPreset title string"));
            sAUVPresetLocalized = true;
        }
    }
    
    // create localized title string
    CFMutableStringRef factoryPresetsTitle = CFStringCreateMutable(NULL, 0);
    CFStringAppend(factoryPresetsTitle, kStringFactoryPreset);
    CFStringAppend(factoryPresetsTitle, kAUViewUnlocalizedString_TitleSeparator);
    
	ControlRef theControl;
    verify_noerr(CreateStaticTextControl(mView->GetCarbonWindow(), &r, factoryPresetsTitle, &inFontStyle, &theControl));
	SInt16 width = 0;
	AUCarbonViewControl::SizeControlToFit(theControl, &width, &mHeight);
    CFRelease(factoryPresetsTitle);
	EmbedControl(theControl);
	
	r.top -= 2;
	r.left += width + 10;
	r.right = r.left;
	r.bottom = r.top;
	
	verify_noerr(CreatePopupButtonControl (	mView->GetCarbonWindow(), &r, NULL, 
											-12345,	// DON'T GET MENU FROM RESOURCE mMenuID,!!!
											FALSE,	// variableWidth, 
											0,		// titleWidth, 
											0,		// titleJustification, 
											0,		// titleStyle, 
											&mControl));
	
	MenuRef menuRef;
	verify_noerr(CreateNewMenu(1, 0, &menuRef));
	
	int numPresets = CFArrayGetCount(mPresets);
	
	for (int i = 0; i < numPresets; ++i)
	{
		AUPreset* preset = (AUPreset*) CFArrayGetValueAtIndex (mPresets, i);
		verify_noerr(AppendMenuItemTextWithCFString (menuRef, preset->presetName, 0, 0, 0));
	}
	
	verify_noerr(SetControlData(mControl, 0, kControlPopupButtonMenuRefTag, sizeof(menuRef), &menuRef));
	verify_noerr (SetControlFontStyle (mControl, &inFontStyle));
	
	SetControl32BitMaximum (mControl, numPresets);
	
	// size popup
	SInt16 height = 0;
	
	AUCarbonViewControl::SizeControlToFit(mControl, &width, &height);
	
	if (height > mHeight) mHeight = height;
	if (mHeight < 0) mHeight = 0;
	
	// find which menu item is the Default preset
	UInt32 propertySize = sizeof(AUPreset);
	AUPreset defaultPreset;
	ComponentResult result = AudioUnitGetProperty (mView->GetEditAudioUnit(), 
									kAudioUnitProperty_PresentPreset,
									kAudioUnitScope_Global, 
									0, 
									&defaultPreset, 
									&propertySize);
	
	mPropertyID = kAudioUnitProperty_PresentPreset;
	
	if (result != noErr) {	// if the PresentPreset property is not implemented, fall back to the CurrentPreset property
		ComponentResult result = AudioUnitGetProperty (mView->GetEditAudioUnit(), 
									kAudioUnitProperty_CurrentPreset,
									kAudioUnitScope_Global, 
									0, 
									&defaultPreset, 
									&propertySize);
		mPropertyID = kAudioUnitProperty_CurrentPreset;
		if (result == noErr)
			CFRetain (defaultPreset.presetName);
	} 
		
	EmbedControl (mControl);
	
	HandlePropertyChange(defaultPreset);
	
	RegisterEvents();
}

void	AUVPresets::AddInterest (AUEventListenerRef		inListener,
											void *		inObject)
{
	AudioUnitEvent e;
	e.mEventType = kAudioUnitEvent_PropertyChange;
	e.mArgument.mProperty.mAudioUnit = mView->GetEditAudioUnit();
	e.mArgument.mProperty.mPropertyID = mPropertyID;
	e.mArgument.mProperty.mScope = kAudioUnitScope_Global;
	e.mArgument.mProperty.mElement = 0;
	
	AUEventListenerAddEventType(inListener, inObject, &e);
}

void	AUVPresets::RemoveInterest (AUEventListenerRef	inListener,
											void *		inObject)
{
	AudioUnitEvent e;
	e.mEventType = kAudioUnitEvent_PropertyChange;
	e.mArgument.mProperty.mAudioUnit = mView->GetEditAudioUnit();
	e.mArgument.mProperty.mPropertyID = mPropertyID;
	e.mArgument.mProperty.mScope = kAudioUnitScope_Global;
	e.mArgument.mProperty.mElement = 0;

	AUEventListenerRemoveEventType(inListener, inObject, &e);
}

void	AUVPresets::HandleControlChange ()
{
	SInt32 i = GetControl32BitValue(mControl);
	if (i > 0)
	{
		AUPreset* preset = (AUPreset*) CFArrayGetValueAtIndex (mPresets, i-1);
	
		verify_noerr(AudioUnitSetProperty (mView->GetEditAudioUnit(), 
									mPropertyID,	// either currentPreset or PresentPreset depending on which is supported
									kAudioUnitScope_Global, 
									0, 
									preset, 
									sizeof(AUPreset)));
									
		// when we change a preset we can't expect the AU to update its state
		// as it isn't meant to know that its being viewed!
		// so we broadcast a notification to all listeners that all parameters on this AU have changed
		AudioUnitParameter changedUnit;
		changedUnit.mAudioUnit = mView->GetEditAudioUnit();
		changedUnit.mParameterID = kAUParameterListener_AnyParameter;
		verify_noerr (AUParameterListenerNotify (NULL, NULL, &changedUnit) );
	}
}

void	AUVPresets::HandlePropertyChange(AUPreset &preset) 
{
	// check to see if the preset is in our menu
	int numPresets = CFArrayGetCount(mPresets);
	if (preset.presetNumber < 0) {	
		SetControl32BitValue (mControl, 0); //controls are one-based
	} else {
		for (SInt32 i = 0; i < numPresets; ++i) {
			AUPreset* currPreset = (AUPreset*) CFArrayGetValueAtIndex (mPresets, i);
			if (preset.presetNumber == currPreset->presetNumber) {
				SetControl32BitValue (mControl, ++i); //controls are one-based
				break;
			}
		}
	}
	
	if (preset.presetName)
		CFRelease (preset.presetName);
}

bool	AUVPresets::HandlePropertyChange (const AudioUnitProperty &inProp)
{
	if (inProp.mPropertyID == mPropertyID) 
	{
		UInt32 theSize = sizeof(AUPreset);
		AUPreset currentPreset;
		
		ComponentResult result = AudioUnitGetProperty(inProp.mAudioUnit, 
												inProp.mPropertyID, 
												inProp.mScope, 
												inProp.mElement, &currentPreset, &theSize);
		
		if (result == noErr) {
			if (inProp.mPropertyID == kAudioUnitProperty_CurrentPreset && currentPreset.presetName)
				CFRetain (currentPreset.presetName);
			HandlePropertyChange(currentPreset);
			return true;
		}
	}
	return false;
}
