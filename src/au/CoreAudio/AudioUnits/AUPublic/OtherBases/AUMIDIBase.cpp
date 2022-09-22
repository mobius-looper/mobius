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
	AUMIDIBase.cpp
	
=============================================================================*/

#include "AUMIDIBase.h"
#include <CoreMIDI/CoreMIDI.h>

//temporaray location
enum
{
	kMidiMessage_NoteOff 			= 0x80,
	kMidiMessage_NoteOn 			= 0x90,
	kMidiMessage_PolyPressure 		= 0xA0,
	kMidiMessage_ControlChange 		= 0xB0,
	kMidiMessage_ProgramChange 		= 0xC0,
	kMidiMessage_ChannelPressure 	= 0xD0,
	kMidiMessage_PitchWheel 		= 0xE0,

	kMidiController_AllSoundOff			= 120,
	kMidiController_ResetAllControllers	= 121,
	kMidiController_AllNotesOff			= 123
};

AUMIDIBase::AUMIDIBase(AUBase* inBase) 
	: mAUBaseInstance (*inBase) 
{
#if CA_AUTO_MIDI_MAP
	mMapManager = new CAAUMIDIMapManager();
#endif
}

AUMIDIBase::~AUMIDIBase() 
{
#if CA_AUTO_MIDI_MAP
	if (mMapManager) 
		delete mMapManager;
#endif
}

#if TARGET_API_MAC_OSX
ComponentResult		AUMIDIBase::DelegateGetPropertyInfo(AudioUnitPropertyID				inID,
														AudioUnitScope					inScope,
														AudioUnitElement				inElement,
														UInt32 &						outDataSize,
														Boolean &						outWritable)
{
	if (inScope != kAudioUnitScope_Global) return kAudioUnitErr_InvalidScope;
	if (inElement != 0) return kAudioUnitErr_InvalidElement;

	ComponentResult result = noErr;
	
	switch (inID) {
	case kMusicDeviceProperty_MIDIXMLNames:
		if (GetXMLNames(NULL) == noErr) {
			outDataSize = sizeof(CFURLRef);
			outWritable = false;
		} else
			result = kAudioUnitErr_InvalidProperty;
		break;
		
/*
#if CA_AUTO_MIDI_MAP
		default:
			result = mMapManager ? mMapManager->GetPropertyInfo (inID, inScope, inElement, outDataSize, outWritable) : kAudioUnitErr_InvalidProperty;				
#endif
*/
#if CA_AUTO_MIDI_MAP				
	case kAudioUnitProperty_AllParameterMIDIMappings:
		outWritable = true;
		outDataSize = sizeof (AUParameterMIDIMapping)*mMapManager->NumMaps();
		result = noErr;
		break;
	
	case kAudioUnitProperty_HotMapParameterMIDIMapping:
		outWritable = true;
		outDataSize = sizeof (AUParameterMIDIMapping); 
		result = noErr;
		break;
		
	case kAudioUnitProperty_AddParameterMIDIMapping:
		outWritable = true;
		outDataSize = sizeof (AUParameterMIDIMapping);
		result = noErr;
		break;
		
	case kAudioUnitProperty_RemoveParameterMIDIMapping:
		outWritable = true;
		outDataSize = sizeof (AUParameterMIDIMapping); 
		result = noErr;
		break;
#endif

	default:
		result = kAudioUnitErr_InvalidProperty;
		break;
	}
	return result;
}

ComponentResult		AUMIDIBase::DelegateGetProperty(	AudioUnitPropertyID 			inID,
														AudioUnitScope 					inScope,
														AudioUnitElement			 	inElement,
														void *							outData)
{
	if (inScope != kAudioUnitScope_Global) return kAudioUnitErr_InvalidScope;
	if (inElement != 0) return kAudioUnitErr_InvalidElement;

	ComponentResult result;
	
	switch (inID) {
	case kMusicDeviceProperty_MIDIXMLNames:
		result = GetXMLNames((CFURLRef *)outData);
		break;
		
#if CA_AUTO_MIDI_MAP
	case kAudioUnitProperty_AllParameterMIDIMappings:{
		AUParameterMIDIMapping*  maps =  (static_cast<AUParameterMIDIMapping*>(outData));
		mMapManager->GetMaps(maps);
//		printf ("GETTING MAPS\n");
//		mMapManager->Print();
		result = noErr;
		break;
	}
		
	case kAudioUnitProperty_HotMapParameterMIDIMapping:{
		AUParameterMIDIMapping *  map =  (static_cast<AUParameterMIDIMapping*>(outData));
		mMapManager->GetHotParameterMap (*map);
		result = noErr;
		break;
	}
#endif
	
	default:
		result = kAudioUnitErr_InvalidProperty;
		break;
	}
	return result;
}

ComponentResult		AUMIDIBase::DelegateSetProperty(	AudioUnitPropertyID 			inID,
														AudioUnitScope 					inScope,
														AudioUnitElement			 	inElement,
														const void *					inData,
														UInt32							inDataSize)
{	
	if (inScope != kAudioUnitScope_Global) return kAudioUnitErr_InvalidScope;
	if (inElement != 0) return kAudioUnitErr_InvalidElement;

	OSStatus result;
	
	switch (inID) {
#if CA_AUTO_MIDI_MAP
		case kAudioUnitProperty_AddParameterMIDIMapping:{
			AUParameterMIDIMapping * maps = (AUParameterMIDIMapping*)inData;
			mMapManager->SortedInsertToParamaterMaps (maps, (inDataSize / sizeof(AUParameterMIDIMapping)), mAUBaseInstance);
			mAUBaseInstance.PropertyChanged (kAudioUnitProperty_AllParameterMIDIMappings, kAudioUnitScope_Global, 0);	 
			result = noErr;
			break;
		}
			
		case kAudioUnitProperty_RemoveParameterMIDIMapping:{
			AUParameterMIDIMapping * maps = (AUParameterMIDIMapping*)inData;
			bool didChange;
			mMapManager->SortedRemoveFromParameterMaps(maps, (inDataSize / sizeof(AUParameterMIDIMapping)), didChange);
			if (didChange)
				mAUBaseInstance.PropertyChanged (kAudioUnitProperty_AllParameterMIDIMappings, kAudioUnitScope_Global, 0);	 
			result = noErr;
			break;
		}
					
		case kAudioUnitProperty_HotMapParameterMIDIMapping:{
			AUParameterMIDIMapping & map = *((AUParameterMIDIMapping*)inData);
			mMapManager->SetHotMapping (map);			
			result = noErr;
			break;
		}
		case kAudioUnitProperty_AllParameterMIDIMappings:{
			AUParameterMIDIMapping * mappings = (AUParameterMIDIMapping*)inData;
			mMapManager->ReplaceAllMaps (mappings, (inDataSize / sizeof(AUParameterMIDIMapping)), mAUBaseInstance);
			result = noErr;
			break;
		}
#endif
		
	default:
		result = kAudioUnitErr_InvalidProperty;
		break;
	}
	return result;
}



#endif


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#pragma mark ____MidiDispatch


inline const Byte *	NextMIDIEvent(const Byte *event, const Byte *end)
{
	Byte c = *event;
	switch (c >> 4) {
	default:	// data byte -- assume in sysex
		while ((*++event & 0x80) == 0 && event < end)
			;
		break;
	case 0x8:
	case 0x9:
	case 0xA:
	case 0xB:
	case 0xE:
		event += 3;
		break;
	case 0xC:
	case 0xD:
		event += 2;
		break;
	case 0xF:
		switch (c) {
		case 0xF0:
			while ((*++event & 0x80) == 0 && event < end)
				;
			break;
		case 0xF1:
		case 0xF3:
			event += 2;
			break;
		case 0xF2:
			event += 3;
			break;
		default:
			++event;
			break;
		}
	}
	return (event >= end) ? end : event;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	AUMIDIBase::HandleMidiEvent
//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ComponentResult		AUMIDIBase::HandleMIDIPacketList(const MIDIPacketList *pktlist)
{
	if (!mAUBaseInstance.IsInitialized()) return kAudioUnitErr_Uninitialized;
	
	int nPackets = pktlist->numPackets;
	const MIDIPacket *pkt = pktlist->packet;
	
	while (nPackets-- > 0) {
		const Byte *event = pkt->data, *packetEnd = event + pkt->length;
		long startFrame = (long)pkt->timeStamp;
		while (event < packetEnd) {
			Byte status = event[0];
			if (status & 0x80) {
				// really a status byte (not sysex continuation)
				HandleMidiEvent(status & 0xF0, status & 0x0F, event[1], event[2], startFrame);
					// note that we're generating a bogus channel number for system messages (0xF0-FF)
			}
			event = NextMIDIEvent(event, packetEnd);
		}
		pkt = reinterpret_cast<const MIDIPacket *>(packetEnd);
	}
	return noErr;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	AUMIDIBase::HandleMidiEvent
//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus 	AUMIDIBase::HandleMidiEvent(UInt8 status, UInt8 channel, UInt8 data1, UInt8 data2, long inStartFrame)
{
	if (!mAUBaseInstance.IsInitialized()) return kAudioUnitErr_Uninitialized;
		
#if CA_AUTO_MIDI_MAP	
// you potentially have a choice to make here - if a param mapping matches, do you still want to process the 
// MIDI event or not. The default behaviour is to continue on with the MIDI event.
	if (mMapManager->HandleHotMapping (status, channel, data1, mAUBaseInstance)) {
		mAUBaseInstance.PropertyChanged (kAudioUnitProperty_HotMapParameterMIDIMapping, kAudioUnitScope_Global, 0);
	}
	else {
		mMapManager->FindParameterMapEventMatch(status, channel, data1, data2, inStartFrame, mAUBaseInstance);
	}	
#endif	
	
	switch(status)
	{
		case kMidiMessage_NoteOn:
			if(data2)
			{
				HandleNoteOn(channel, data1, data2, inStartFrame);
			}
			else
			{
				// zero velocity translates to note off
				HandleNoteOff(channel, data1, data2, inStartFrame);
			}
			break;
			
		case kMidiMessage_NoteOff:
			HandleNoteOff(channel, data1, data2, inStartFrame);
			break;
		
		default:
			HandleNonNoteEvent (status, channel, data1, data2, inStartFrame);
			break;
	}
	
	return noErr;
}

void	AUMIDIBase::HandleNonNoteEvent (UInt8 status, UInt8 channel, UInt8 data1, UInt8 data2, UInt32 inStartFrame)
{
	switch (status)
	{
		case kMidiMessage_PitchWheel:
			HandlePitchWheel(channel, data1, data2, inStartFrame);
			break;
			
		case kMidiMessage_ProgramChange:
			HandleProgramChange(channel, data1);
			break;
			
		case kMidiMessage_ChannelPressure:
			HandleChannelPressure(channel, data1, inStartFrame);
			break;
			
		case kMidiMessage_ControlChange:
		{
			switch (data1) {
				case kMidiController_AllNotesOff:
					HandleAllNotesOff(channel);
					break;

				case kMidiController_ResetAllControllers:
					HandleResetAllControllers(channel);
					break;

				case kMidiController_AllSoundOff:
					HandleAllSoundOff(channel);
					break;
								
				default:
					HandleControlChange(channel, data1, data2, inStartFrame);
					break;
			}
			break;
		}
		case kMidiMessage_PolyPressure:
			HandlePolyPressure (channel, data1, data2, inStartFrame);
			break;
	}
}

ComponentResult 	AUMIDIBase::SysEx(	UInt8 						*inData, 
										UInt32 						inLength)
{
	if (!mAUBaseInstance.IsInitialized()) return kAudioUnitErr_Uninitialized;

	HandleSysEx(inData, inLength );
	
	return noErr;
}


#if PRAGMA_STRUCT_ALIGN
	#pragma options align=mac68k
#elif PRAGMA_STRUCT_PACKPUSH
	#pragma pack(push, 2)
#elif PRAGMA_STRUCT_PACK
	#pragma pack(2)
#endif
#if	TARGET_API_MAC_OS8 || TARGET_API_MAC_OSX
struct MusicDeviceMIDIEventGluePB {
	unsigned char                  componentFlags;
	unsigned char                  componentParamSize;
	short                          componentWhat;
	UInt32                         inOffsetSampleFrame;
	UInt32                         inData2;
	UInt32                         inData1;
	UInt32                         inStatus;
	MusicDeviceComponent           ci;
};
struct MusicDeviceSysExGluePB {
	unsigned char                  componentFlags;
	unsigned char                  componentParamSize;
	short                          componentWhat;
	UInt32                         inLength;
	UInt8*                         inData;
	MusicDeviceComponent           ci;
};
#elif	TARGET_OS_WIN32
struct MusicDeviceMIDIEventGluePB {
	unsigned char                  componentFlags;
	unsigned char                  componentParamSize;
	short                          componentWhat;
	long                           inStatus;
	long                           inData1;
	long                           inData2;
	long                           inOffsetSampleFrame;
};
struct MusicDeviceSysExGluePB {
	unsigned char                  componentFlags;
	unsigned char                  componentParamSize;
	short                          componentWhat;
	long                           inData;
	long                           inLength;
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


ComponentResult		AUMIDIBase::ComponentEntryDispatch(	ComponentParameters *			params,
															AUMIDIBase *				This)
{
	if (This == NULL) return paramErr;

	ComponentResult result;
	
	switch (params->what) {
	case kMusicDeviceMIDIEventSelect:
		{
			MusicDeviceMIDIEventGluePB *pb = (MusicDeviceMIDIEventGluePB *)params;
			#if	!TARGET_OS_WIN32
				UInt32					pbinStatus = pb->inStatus;
				UInt32					pbinData1 = pb->inData1;
				UInt32					pbinData2 = pb->inData2;
				UInt32					pbinOffsetSampleFrame = pb->inOffsetSampleFrame;
			#else
				UInt32					pbinStatus = (*((UInt32*)&pb->inStatus));
				UInt32					pbinData1 = (*((UInt32*)&pb->inData1));
				UInt32					pbinData2 = (*((UInt32*)&pb->inData2));
				UInt32					pbinOffsetSampleFrame = (*((UInt32*)&pb->inOffsetSampleFrame));
			#endif
			
			result = This->MIDIEvent(pbinStatus, pbinData1, pbinData2, pbinOffsetSampleFrame);
		}
		break;
	case kMusicDeviceSysExSelect:
		{
			MusicDeviceSysExGluePB *pb = (MusicDeviceSysExGluePB *)params;
			#if	!TARGET_OS_WIN32
				UInt32					pbinLength = pb->inLength;
				UInt8*					pbinData = pb->inData;
			#else
				UInt32					pbinLength = (*((UInt32*)&pb->inLength));
				UInt8*					pbinData = (*((UInt8**)&pb->inData));
			#endif
			
			result = This->SysEx(pbinData, pbinLength);
		}
		break;

	default:
		result = badComponentSelector;
		break;
	}
	
	return result;
}

