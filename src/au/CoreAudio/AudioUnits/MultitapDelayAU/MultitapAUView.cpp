/*	Copyright: 	© Copyright 2005 Apple Computer, Inc. All rights reserved.

	Disclaimer:	IMPORTANT:  This Apple software is supplied to you by Apple Computer, Inc.
			("Apple") in consideration of your agreement to the following terms, and your
			use, installation, modification or redistribution of this Apple software
			constitutes acceptance of these terms.  If you do not agree with these terms,
			please do not use, install, modify or redistribute this Apple software.

			In consideration of your agreement to abide by the following terms, and subject
			to these terms, Apple grants you a personal, non-exclusive license, under Apple’s
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
	MultitapAUView.cpp
	
=============================================================================*/

#include "MultitapAU.h"
#include "AUCarbonViewBase.h"
#include "AUControlGroup.h"

// ____________________________________________________________________________
//
class MultitapAUView : public AUCarbonViewBase {
public:
	MultitapAUView(AudioUnitCarbonView auv) : AUCarbonViewBase(auv) { }
	
	virtual OSStatus			CreateUI(Float32 xoffset, Float32 yoffset);
};

// ____________________________________________________________________________
//
COMPONENT_ENTRY(MultitapAUView)


// ____________________________________________________________________________
//
OSStatus	MultitapAUView::CreateUI(Float32 xoffset, Float32 yoffset)
{
    // need offsets as int's:
    int xoff = (int)xoffset;
    int yoff = (int)yoffset;
    
    // for each parameter, create controls
	// inside mCarbonWindow, embedded in mCarbonPane
	
#define kLabelWidth 80
#define kLabelHeight 16
#define kEditTextWidth 40
#define kMinMaxWidth 32
	
	ControlRef newControl;
	ControlFontStyleRec fontStyle;
	fontStyle.flags = kControlUseFontMask | kControlUseJustMask;
	fontStyle.font = kControlFontSmallSystemFont;
	fontStyle.just = teFlushRight;
	
	Rect r;
	Point labelSize, textSize;
	labelSize.v = textSize.v = kLabelHeight;
	labelSize.h = kMinMaxWidth;
	textSize.h = kEditTextWidth;
	
	// wet/dry mix at top
	{
		CAAUParameter auvp(mEditAudioUnit, kParam_WetDryMix, kAudioUnitScope_Global, 0);
		
		// text label
		r.top = 4 + yoff;
        r.bottom = r.top + kLabelHeight;
		r.left = 4 +xoff;
        r.right = r.left + kLabelWidth;
		verify_noerr(CreateStaticTextControl(mCarbonWindow, &r, auvp.GetName(), &fontStyle, &newControl));
		verify_noerr(EmbedControl(newControl));

		r.left = r.right + 4;
		r.right = r.left + 240;
		AUControlGroup::CreateLabelledSliderAndEditText(this, auvp, r, labelSize, textSize, fontStyle);
	}
	
#define kDelayTimeTop 32
#define kDelayTimeHeight 200
#define kFirstColumnX kLabelWidth
#define kColumnWidth 50
#define kLevelTop 240
#define kLevelHeight 200
    
	AudioUnitParameterID param = kParam_Tap0;
	for (int tap = 0; tap < kNumTaps; ++tap) {
		r.left = kFirstColumnX + tap * kColumnWidth + xoff;
		r.right = r.left + kColumnWidth;
		{
			CAAUParameter auvp(mEditAudioUnit, param + kTapParam_DelayTime, kAudioUnitScope_Global, 0);
			
			r.top = kDelayTimeTop + yoff;
			r.bottom = r.top + kDelayTimeHeight;
			
			AUControlGroup::CreateLabelledSliderAndEditText(this, auvp, r, labelSize, textSize, fontStyle);
		}
		{
			CAAUParameter auvp(mEditAudioUnit, param + kTapParam_Level, kAudioUnitScope_Global, 0);
			
			r.top = kLevelTop + yoff;
			r.bottom = r.top + kLevelHeight;
			
			AUControlGroup::CreateLabelledSliderAndEditText(this, auvp, r, labelSize, textSize, fontStyle);
		}
		param += kParamsPerTap;
	}
	
	// set size of overall pane
	SizeControl(mCarbonPane, mBottomRight.h + 8, mBottomRight.v + 8);
	return noErr;
}

