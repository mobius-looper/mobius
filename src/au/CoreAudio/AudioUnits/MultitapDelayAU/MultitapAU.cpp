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
	MultitapAU.cpp
	
=============================================================================*/

#include <AudioUnit/AudioUnit.h>
#include "AUEffectBase.h"
#include "MultitapAU.h"
#include "AUMultitapVersion.h"

const float kDefaultValue_WetDryMix	= 50.;			// percent
const float kDefaultValue_DelayTime = 0.250;		// seconds
const float kDefaultValue_Level = 0.;				// percent


const float kMaxDelayTime = 5.0;	// seconds

// ____________________________________________________________________________
//
class MultitapAU : public AUEffectBase {
public:
	MultitapAU(AudioUnit component);

	virtual AUKernelBase *		NewKernel() { return new MultitapKernel(this); }
	
	virtual ComponentResult		GetParameterInfo(	AudioUnitScope			inScope,
													AudioUnitParameterID	inParameterID,
													AudioUnitParameterInfo	&outParameterInfo );
    
    int		GetNumCustomUIComponents () { return 1; }
        
    void	GetUIComponentDescs (ComponentDescription* inDescArray) {
        inDescArray[0].componentType = kAudioUnitCarbonViewComponentType;
        inDescArray[0].componentSubType = 'asmd';
        inDescArray[0].componentManufacturer = 'Acme';
        inDescArray[0].componentFlags = 0;
        inDescArray[0].componentFlagsMask = 0;
    }

	virtual ComponentResult	Version() { return kAUMultitapVersion; }

	virtual	bool				SupportsTail () { return true; }
    
	class MultitapKernel : public AUKernelBase		// most real work happens here
	{
	public:
		MultitapKernel(AUEffectBase *inAudioUnit )
			: AUKernelBase(inAudioUnit)
		{
			mMaxDelayFrames = (int)(GetSampleRate() * kMaxDelayTime + 10)
				/*a little extra so we can support full delay time*/;
			
			mDelayBuffer.AllocateClear(mMaxDelayFrames);
			mWriteIndex = mMaxDelayFrames - 1;
		};
		
		// processes one channel of interleaved samples
		virtual void 		Process(	const Float32 * 	inSourceP,
										Float32 * 			inDestP,
										UInt32 				inFramesToProcess,
										UInt32				inNumChannels,
										bool &				ioSilence);

        virtual void		Reset()
		{
			mDelayBuffer.Clear();
		}

	private:
		TAUBuffer<Float32>	mDelayBuffer;
		int					mWriteIndex;
		int					mMaxDelayFrames;
		
		int					NormalizeIndex(int i) {return (i + mMaxDelayFrames) % mMaxDelayFrames;};
	};
};


// ____________________________________________________________________________
//
COMPONENT_ENTRY(MultitapAU)


// ____________________________________________________________________________
//
MultitapAU::MultitapAU(AudioUnit component)
	: AUEffectBase(component)
{
	CreateElements();
	
	SetParameter(kParam_WetDryMix, 	kDefaultValue_WetDryMix);
	
	AudioUnitParameterID firstTapParam = kParam_Tap0;
	for (int tap = 0; tap < kNumTaps; ++tap) {
		SetParameter(firstTapParam + kTapParam_DelayTime, 	kDefaultValue_DelayTime * (tap + 1));
		SetParameter(firstTapParam + kTapParam_Level,	 	kDefaultValue_Level);
		firstTapParam += kParamsPerTap;
	}
}

// ____________________________________________________________________________
//
void MultitapAU::MultitapKernel::Process(	const Float32 * 	inSourceP,
											Float32 * 			inDestP,
											UInt32 				inFramesToProcess,
											UInt32				inNumChannels,
											bool &				ioSilence )
{
	long nSampleFrames = inFramesToProcess;
	const float *sourceP = inSourceP;
	float *destP = inDestP;

	// get the parameters
	struct TapState {
		float	level;
		int		delayFrames;
		int		readIndex;
	};
	TapState tapState[kNumTaps], *tap;
	
	AudioUnitParameterID firstTapParam = kParam_Tap0;
	for (tap = &tapState[0]; tap < &tapState[kNumTaps]; ++tap) {
		tap->level = GetParameter(firstTapParam + kTapParam_Level) / 100.;	// percent to ratio

		int frames = int(GetParameter(firstTapParam + kTapParam_DelayTime) * GetSampleRate());
		if (frames < 1) frames = 1;
		else if (frames >= mMaxDelayFrames) frames = mMaxDelayFrames - 1;
		tap->delayFrames = frames;
		tap->readIndex = NormalizeIndex(mWriteIndex - frames);	// read head lags write head...

		firstTapParam += kParamsPerTap;
	}

	// wet/dry
	float wetDry = GetParameter(kParam_WetDryMix) / 100.;	// percent to ratio
	float wet = sqrt(wetDry), dry = sqrt(1.0 - wetDry);
		
	while (nSampleFrames--)
	{
		float input = *sourceP;
		sourceP += inNumChannels;	// advance to next frame
		
		// write to delay line
		mDelayBuffer[mWriteIndex] = input;
		mWriteIndex = (mWriteIndex + 1) % mMaxDelayFrames;		
		float output = dry * input;
		for (tap = &tapState[0]; tap < &tapState[kNumTaps]; ++tap) {
			// read from delay line
			if (tap->level != 0.0) {
				int readIndex = tap->readIndex;
				float delayRead = mDelayBuffer[readIndex];
				tap->readIndex = (readIndex + 1) % mMaxDelayFrames;
				output += delayRead * tap->level * wet;
			}
		}
		
		*destP = output;
		destP += inNumChannels;		// advance to next frame
	}
	ioSilence = false;
}

// ____________________________________________________________________________
//
ComponentResult		MultitapAU::GetParameterInfo(AudioUnitScope			inScope,
												AudioUnitParameterID	inParameterID,
												AudioUnitParameterInfo	&outParameterInfo )
{
	ComponentResult result = noErr;

	outParameterInfo.flags = 	kAudioUnitParameterFlag_IsWritable
						+		kAudioUnitParameterFlag_IsReadable;	
	
	if (inScope == kAudioUnitScope_Global) 
	{		
		switch(inParameterID) 
		{
		case kParam_WetDryMix:
			AUBase::FillInParameterName (outParameterInfo, CFSTR("dry/wet mix"), false);
			outParameterInfo.unit = kAudioUnitParameterUnit_EqualPowerCrossfade;
			outParameterInfo.minValue = 0.0;
			outParameterInfo.maxValue = 100.0;
			outParameterInfo.defaultValue = kDefaultValue_WetDryMix;
			break;
			
		default:
			result = kAudioUnitErr_InvalidParameter;
			if (inParameterID >= kParam_Tap0) {
				inParameterID -= kParam_Tap0;
				int tap = inParameterID / kParamsPerTap;
				int param = inParameterID % kParamsPerTap;
				if (tap >= 0 && tap < kNumTaps && param < kTapParam_Last) {
					char name[64];
					sprintf(name, "tap %d ", tap+1);
					CFStringRef str = NULL;
					switch (param) {
					case kTapParam_DelayTime:
						strcat (name, "delay time");
						str = CFStringCreateWithCString (NULL, name, kCFStringEncodingASCII);
						AUBase::FillInParameterName (outParameterInfo, str, true);
						outParameterInfo.unit = kAudioUnitParameterUnit_Seconds;
						outParameterInfo.minValue = 0.0;
						outParameterInfo.maxValue = kMaxDelayTime;
						outParameterInfo.defaultValue = kDefaultValue_DelayTime * (tap + 1);
						break;
					case kTapParam_Level:
						strcat (name, "level");
						str = CFStringCreateWithCString (NULL, name, kCFStringEncodingASCII);
						AUBase::FillInParameterName (outParameterInfo, str, true);
						outParameterInfo.unit = kAudioUnitParameterUnit_Percent;
						outParameterInfo.minValue = 0.0;
						outParameterInfo.maxValue = 100.0;
						outParameterInfo.defaultValue = kDefaultValue_Level;
						break;
					}
					result = noErr;
				}
			}
			break;
		}
	} else {
		result = kAudioUnitErr_InvalidParameter;
	}
	
	return result;
}
