/*	Copyright: 	� Copyright 2005 Apple Computer, Inc. All rights reserved.

	Disclaimer:	IMPORTANT:  This Apple software is supplied to you by Apple Computer, Inc.
			("Apple") in consideration of your agreement to the following terms, and your
			use, installation, modification or redistribution of this Apple software
			constitutes acceptance of these terms.  If you do not agree with these terms,
			please do not use, install, modify or redistribute this Apple software.

			In consideration of your agreement to abide by the following terms, and subject
			to these terms, Apple grants you a personal, non-exclusive license, under Apple�s
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
	AUMIDIEffectBase.cpp
	
=============================================================================*/

#include "AUMIDIEffectBase.h"

// compatibility with older OS SDK releases
typedef ComponentResult
(*TEMP_MusicDeviceMIDIEventProc)(	void *				inComponentStorage,
								UInt32					inStatus,
								UInt32					inData1,
								UInt32					inData2,
								UInt32					inOffsetSampleFrame);

static ComponentResult	AUMIDIEffectBaseMIDIEvent(void *				inComponentStorage,
						UInt32					inStatus,
						UInt32					inData1,
						UInt32					inData2,
						UInt32					inOffsetSampleFrame);

AUMIDIEffectBase::AUMIDIEffectBase(		ComponentInstance				inInstance,
						bool 						inProcessesInPlace ) 
	: AUEffectBase(inInstance, inProcessesInPlace),
	  AUMIDIBase(this)
{
}

ComponentResult		AUMIDIEffectBase::GetPropertyInfo(AudioUnitPropertyID			inID,
							AudioUnitScope				inScope,
							AudioUnitElement			inElement,
							UInt32 &				outDataSize,
							Boolean &				outWritable)
{
	ComponentResult result;
	
	result = AUEffectBase::GetPropertyInfo (inID, inScope, inElement, outDataSize, outWritable);
	
	if (result == kAudioUnitErr_InvalidProperty)
		result = AUMIDIBase::DelegateGetPropertyInfo (inID, inScope, inElement, outDataSize, outWritable);
	
	return result;
}

ComponentResult		AUMIDIEffectBase::GetProperty(	AudioUnitPropertyID		inID,
													AudioUnitScope 			inScope,
													AudioUnitElement		inElement,
													void *					outData)
{
	ComponentResult result;

	if (inID == kAudioUnitProperty_FastDispatch) {
		if (inElement == kMusicDeviceMIDIEventSelect) {
			*(TEMP_MusicDeviceMIDIEventProc *)outData = AUMIDIEffectBaseMIDIEvent;
			return noErr;
		}
		return kAudioUnitErr_InvalidElement;
	}
	
	result = AUEffectBase::GetProperty (inID, inScope, inElement, outData);
	
	if (result == kAudioUnitErr_InvalidProperty)
		result = AUMIDIBase::DelegateGetProperty (inID, inScope, inElement, outData);
	
	return result;
}

ComponentResult		AUMIDIEffectBase::SetProperty(	AudioUnitPropertyID			inID,
							AudioUnitScope 				inScope,
							AudioUnitElement		 	inElement,
							const void *				inData,
							UInt32					inDataSize)
{

	ComponentResult result = AUEffectBase::SetProperty (inID, inScope, inElement, inData, inDataSize);
		
	if (result == kAudioUnitErr_InvalidProperty)
		result = AUMIDIBase::DelegateSetProperty (inID, inScope, inElement, inData, inDataSize);
		
	return result;
}


ComponentResult		AUMIDIEffectBase::ComponentEntryDispatch(ComponentParameters *			params,
								AUMIDIEffectBase *			This)
{
	if (This == NULL) return paramErr;

	ComponentResult result;
	
	switch (params->what) {
	case kMusicDeviceMIDIEventSelect:
	case kMusicDeviceSysExSelect:
		result = AUMIDIBase::ComponentEntryDispatch (params, This);
		break;
	default:
		result = AUEffectBase::ComponentEntryDispatch(params, This);
		break;
	}
	
	return result;
}

// fast dispatch
static ComponentResult	AUMIDIEffectBaseMIDIEvent(void *				inComponentStorage,
						UInt32					inStatus,
						UInt32					inData1,
						UInt32					inData2,
						UInt32					inOffsetSampleFrame)
{
	ComponentResult result = noErr;
	try {
		AUMIDIEffectBase *This = static_cast<AUMIDIEffectBase *>(inComponentStorage);
		if (This == NULL) return paramErr;
		result = This->MIDIEvent(inStatus, inData1, inData2, inOffsetSampleFrame);
	}
	COMPONENT_CATCH
	return result;
}
