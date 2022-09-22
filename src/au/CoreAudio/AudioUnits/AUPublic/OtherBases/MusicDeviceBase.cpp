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
	MusicDeviceBase.cpp
	
=============================================================================*/

#include "MusicDeviceBase.h"

// compatibility with older OS SDK releases
typedef ComponentResult
(*TEMP_MusicDeviceMIDIEventProc)(	void *				inComponentStorage,
								UInt32					inStatus,
								UInt32					inData1,
								UInt32					inData2,
								UInt32					inOffsetSampleFrame);


static ComponentResult	MusicDeviceBaseMIDIEvent(void *			inComponentStorage,
					UInt32				inStatus,
					UInt32				inData1,
					UInt32				inData2,
					UInt32				inOffsetSampleFrame);
									
MusicDeviceBase::MusicDeviceBase(		ComponentInstance				inInstance, 
						UInt32						numInputs,
						UInt32						numOutputs,
						UInt32						numGroups,
						UInt32						numParts) 
	: AUBase(inInstance, numInputs, numOutputs, numGroups, numParts),
	  AUMIDIBase(this)
{
}

ComponentResult		MusicDeviceBase::GetPropertyInfo(AudioUnitPropertyID	inID,
							AudioUnitScope				inScope,
							AudioUnitElement			inElement,
							UInt32 &				outDataSize,
							Boolean &				outWritable)
{
	ComponentResult result;
	
	switch (inID) 
	{
		case kMusicDeviceProperty_InstrumentCount:
			if (inScope != kAudioUnitScope_Global) return kAudioUnitErr_InvalidScope;
			outDataSize = sizeof(UInt32);
			outWritable = false;
			result = noErr;
			break;

		default:
			result = AUBase::GetPropertyInfo (inID, inScope, inElement, outDataSize, outWritable);
			
			if (result == kAudioUnitErr_InvalidProperty)
				result = AUMIDIBase::DelegateGetPropertyInfo (inID, inScope, inElement, outDataSize, outWritable);
			break;
	}
	return result;
}

ComponentResult		MusicDeviceBase::GetProperty(	AudioUnitPropertyID			inID,
							AudioUnitScope 				inScope,
							AudioUnitElement		 	inElement,
							void *					outData)
{
	ComponentResult result;

	switch (inID) 
	{
		case kAudioUnitProperty_FastDispatch:
			if (inElement == kMusicDeviceMIDIEventSelect) {
				*(TEMP_MusicDeviceMIDIEventProc *)outData = MusicDeviceBaseMIDIEvent;
				return noErr;
			}
			return kAudioUnitErr_InvalidElement;

		case kMusicDeviceProperty_InstrumentCount:
			if (inScope != kAudioUnitScope_Global) return kAudioUnitErr_InvalidScope;
			return GetInstrumentCount (*(UInt32*)outData);

		default:
			result = AUBase::GetProperty (inID, inScope, inElement, outData);
			
			if (result == kAudioUnitErr_InvalidProperty)
				result = AUMIDIBase::DelegateGetProperty (inID, inScope, inElement, outData);
	}
	
	return result;
}


ComponentResult		MusicDeviceBase::SetProperty(	AudioUnitPropertyID 			inID,
							AudioUnitScope 				inScope,
							AudioUnitElement 			inElement,
							const void *				inData,
							UInt32 					inDataSize)

{

	ComponentResult result = AUBase::SetProperty (inID, inScope, inElement, inData, inDataSize);
		
	if (result == kAudioUnitErr_InvalidProperty)
		result = AUMIDIBase::DelegateSetProperty (inID, inScope, inElement, inData, inDataSize);
		
	return result;
}

// For a MusicDevice that doesn't support separate instruments (ie. is mono-timbral)
// then this call should return an instrument count of zero and noErr
OSStatus			MusicDeviceBase::GetInstrumentCount (UInt32 &outInstCount) const
{
	outInstCount = 0;
	return noErr;
}

void		MusicDeviceBase::HandleNoteOn (int 	inChannel,
										UInt8 	inNoteNumber,
										UInt8 	inVelocity,
										long 	inStartFrame)
{
	NoteInstanceID id;
	MusicDeviceNoteParams params;
	
	params.argCount = 2;
	params.mPitch = (Float32)inNoteNumber;
	params.mVelocity = (Float32)inVelocity;
	
	StartNote( kMusicNoteEvent_UseGroupInstrument, inChannel, id, inStartFrame, params);
}

												
void		MusicDeviceBase::HandleNoteOff (int 	inChannel,
											UInt8 	inNoteNumber,
											UInt8 	inVelocity,
											long 	inStartFrame)
{
	// do something with velocity..?
	StopNote (inChannel, inNoteNumber, inStartFrame);
}

ComponentResult		MusicDeviceBase::HandleStartNoteMessage (MusicDeviceInstrumentID 	inInstrument, 
															MusicDeviceGroupID 			inGroupID, 
															NoteInstanceID 				*outNoteInstanceID, 
															UInt32 						inOffsetSampleFrame, 
															const MusicDeviceNoteParams	*inParams)
{
	if (inParams == NULL)
		return paramErr;
	
	if (!IsInitialized()) return kAudioUnitErr_Uninitialized;
	
	NoteInstanceID noteID;
	
	// ok - if velocity is zero we dispatch to StopNote
	ComponentResult result;
	if (fnonzero(inParams->mVelocity)) {
		result = StartNote (inInstrument, inGroupID, noteID, inOffsetSampleFrame, *inParams);
		if (result == noErr && outNoteInstanceID)
			*outNoteInstanceID = noteID;
	} else {
		noteID = UInt32(inParams->mPitch);
		result = StopNote (inGroupID, noteID, inOffsetSampleFrame);
	}
	return result;
}

#if PRAGMA_STRUCT_ALIGN
	#pragma options align=mac68k
#elif PRAGMA_STRUCT_PACKPUSH
	#pragma pack(push, 2)
#elif PRAGMA_STRUCT_PACK
	#pragma pack(2)
#endif
#if	TARGET_API_MAC_OS8 || TARGET_API_MAC_OSX
struct MusicDevicePrepareInstrumentGluePB {
	unsigned char                  componentFlags;
	unsigned char                  componentParamSize;
	short                          componentWhat;
	MusicDeviceInstrumentID        inInstrument;
	MusicDeviceComponent           ci;
};
struct MusicDeviceReleaseInstrumentGluePB {
	unsigned char                  componentFlags;
	unsigned char                  componentParamSize;
	short                          componentWhat;
	MusicDeviceInstrumentID        inInstrument;
	MusicDeviceComponent           ci;
};
struct MusicDeviceStartNoteGluePB {
	unsigned char                  componentFlags;
	unsigned char                  componentParamSize;
	short                          componentWhat;
	const MusicDeviceNoteParams*   inParams;
	UInt32                         inOffsetSampleFrame;
	NoteInstanceID*                outNoteInstanceID;
	MusicDeviceGroupID             inGroupID;
	MusicDeviceInstrumentID        inInstrument;
	MusicDeviceComponent           ci;
};
struct MusicDeviceStopNoteGluePB {
	unsigned char                  componentFlags;
	unsigned char                  componentParamSize;
	short                          componentWhat;
	UInt32                         inOffsetSampleFrame;
	NoteInstanceID                 inNoteInstanceID;
	MusicDeviceGroupID             inGroupID;
	MusicDeviceComponent           ci;
};
#elif	TARGET_OS_WIN32
struct MusicDevicePrepareInstrumentGluePB {
	unsigned char                  componentFlags;
	unsigned char                  componentParamSize;
	short                          componentWhat;
	long                           inInstrument;
};
struct MusicDeviceReleaseInstrumentGluePB {
	unsigned char                  componentFlags;
	unsigned char                  componentParamSize;
	short                          componentWhat;
	long                           inInstrument;
};
struct MusicDeviceStartNoteGluePB {
	unsigned char                  componentFlags;
	unsigned char                  componentParamSize;
	short                          componentWhat;
	long                           inInstrument;
	long                           inGroupID;
	long                           outNoteInstanceID;
	long                           inOffsetSampleFrame;
	long                           inParams;
};
struct MusicDeviceStopNoteGluePB {
	unsigned char                  componentFlags;
	unsigned char                  componentParamSize;
	short                          componentWhat;
	long                           inGroupID;
	long                           inNoteInstanceID;
	long                           inOffsetSampleFrame;
};
#else
	#error	Platform not supported
#endif
#if PRAGMA_STRUCT_ALIGN
	#pragma options align=reset
#elif PRAGMA_STRUCT_PACKPUSH
	#pragma pack(pop)
#elif PRAGMA_STRUCT_PACK
	#pragma pack()
#endif


ComponentResult		MusicDeviceBase::ComponentEntryDispatch(	ComponentParameters *			params,
																MusicDeviceBase *				This)
{
	if (This == NULL) return paramErr;

	ComponentResult result;
	
	switch (params->what) {
	case kMusicDeviceMIDIEventSelect:
	case kMusicDeviceSysExSelect:
		{
			result = AUMIDIBase::ComponentEntryDispatch (params, This);
		}
		break;
	case kMusicDevicePrepareInstrumentSelect:
		{
			MusicDevicePrepareInstrumentGluePB *pb = (MusicDevicePrepareInstrumentGluePB *)params;
			#if	!TARGET_OS_WIN32
				MusicDeviceInstrumentID	pbinInstrument = pb->inInstrument;
			#else
				MusicDeviceInstrumentID	pbinInstrument = (*((MusicDeviceInstrumentID*)&pb->inInstrument));
			#endif
			
			result = This->PrepareInstrument(pbinInstrument);
		}
		break;
	case kMusicDeviceReleaseInstrumentSelect:
		{
			MusicDeviceReleaseInstrumentGluePB *pb = (MusicDeviceReleaseInstrumentGluePB *)params;
			#if	!TARGET_OS_WIN32
				MusicDeviceInstrumentID	pbinInstrument = pb->inInstrument;
			#else
				MusicDeviceInstrumentID	pbinInstrument = (*((MusicDeviceInstrumentID*)&pb->inInstrument));
			#endif
			
			result = This->ReleaseInstrument(pbinInstrument);
		}
		break;
	case kMusicDeviceStartNoteSelect:
		{
			MusicDeviceStartNoteGluePB *pb = (MusicDeviceStartNoteGluePB *)params;
			#if	!TARGET_OS_WIN32
				MusicDeviceInstrumentID			pbinInstrument = pb->inInstrument;
				MusicDeviceGroupID				pbinGroupID = pb->inGroupID;
				NoteInstanceID*					pboutNoteInstanceID = pb->outNoteInstanceID;
				UInt32							pbinOffsetSampleFrame = pb->inOffsetSampleFrame;
				const MusicDeviceNoteParams*	pbinParams = pb->inParams;
			#else
				MusicDeviceInstrumentID			pbinInstrument = (*((MusicDeviceInstrumentID*)&pb->inInstrument));
				MusicDeviceGroupID				pbinGroupID = (*((MusicDeviceGroupID*)&pb->inGroupID));
				NoteInstanceID*					pboutNoteInstanceID = (*((NoteInstanceID**)&pb->outNoteInstanceID));
				UInt32							pbinOffsetSampleFrame = (*((UInt32*)&pb->inOffsetSampleFrame));
				const MusicDeviceNoteParams*	pbinParams = (*((const MusicDeviceNoteParams**)&pb->inParams));
			#endif
			
			result = This->HandleStartNoteMessage(pbinInstrument, pbinGroupID, pboutNoteInstanceID, pbinOffsetSampleFrame, pbinParams);
		}
		break;
	case kMusicDeviceStopNoteSelect:
		{
			MusicDeviceStopNoteGluePB *pb = (MusicDeviceStopNoteGluePB *)params;
			#if	!TARGET_OS_WIN32
				MusicDeviceGroupID				pbinGroupID = pb->inGroupID;
				NoteInstanceID					pbinNoteInstanceID = pb->inNoteInstanceID;
				UInt32							pbinOffsetSampleFrame = pb->inOffsetSampleFrame;
			#else
				MusicDeviceGroupID				pbinGroupID = (*((MusicDeviceGroupID*)&pb->inGroupID));
				NoteInstanceID					pbinNoteInstanceID = (*((NoteInstanceID*)&pb->inNoteInstanceID));
				UInt32							pbinOffsetSampleFrame = (*((UInt32*)&pb->inOffsetSampleFrame));
			#endif
			
			result = This->StopNote(pbinGroupID, pbinNoteInstanceID, pbinOffsetSampleFrame);
		}
		break;

	default:
		result = AUBase::ComponentEntryDispatch(params, This);
		break;
	}
	
	return result;
}



// fast dispatch
static ComponentResult	MusicDeviceBaseMIDIEvent(void *					inComponentStorage,
						UInt32					inStatus,
						UInt32					inData1,
						UInt32					inData2,
						UInt32					inOffsetSampleFrame)
{
	ComponentResult result = noErr;
	try {
		MusicDeviceBase *This = static_cast<MusicDeviceBase *>(inComponentStorage);
		if (This == NULL) return paramErr;
		result = This->MIDIEvent(inStatus, inData1, inData2, inOffsetSampleFrame);
	}
	COMPONENT_CATCH
	return result;
}
