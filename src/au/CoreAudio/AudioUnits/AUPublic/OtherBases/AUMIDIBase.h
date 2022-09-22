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
	AUMIDIBase.h
	
=============================================================================*/

#ifndef __AUMIDIBase_h__
#define __AUMIDIBase_h__

#include "AUBase.h"

#if CA_AUTO_MIDI_MAP
	#include "CAAUMIDIMapManager.h"
#endif

struct MIDIPacketList;

// ________________________________________________________________________
//	MusicDeviceBase
//
	/*! @class AUMIDIBase */
class AUMIDIBase {
public:
									// this is NOT a copy constructor!
	/*! @ctor AUMIDIBase */
								AUMIDIBase(AUBase* inBase);
	/*! @dtor ~AUMIDIBase */
	virtual						~AUMIDIBase();
	
	/*! @method MIDIEvent */
	ComponentResult		MIDIEvent(		UInt32 						inStatus, 
										UInt32 						inData1, 
										UInt32 						inData2, 
										UInt32 						inOffsetSampleFrame)
	{
		UInt32 strippedStatus = inStatus & 0xf0;
		UInt32 channel = inStatus & 0x0f;
	
		return HandleMidiEvent(strippedStatus, channel, inData1, inData2, inOffsetSampleFrame);
	}
	
	/*! @method HandleMIDIPacketList */
	ComponentResult		HandleMIDIPacketList(const MIDIPacketList *pktlist);
	
	/*! @method SysEx */
	ComponentResult		SysEx(			UInt8 						*inData, 
										UInt32 						inLength);

#if TARGET_API_MAC_OSX
	/*! @method DelegateGetPropertyInfo */
	virtual ComponentResult		DelegateGetPropertyInfo(AudioUnitPropertyID			inID,
														AudioUnitScope				inScope,
														AudioUnitElement			inElement,
														UInt32 &					outDataSize,
														Boolean &					outWritable);

	/*! @method DelegateGetProperty */
	virtual ComponentResult		DelegateGetProperty(	AudioUnitPropertyID 		inID,
														AudioUnitScope 				inScope,
														AudioUnitElement		 	inElement,
														void *						outData);
														
	/*! @method DelegateSetProperty */
	virtual ComponentResult		DelegateSetProperty(	AudioUnitPropertyID 		inID,
														AudioUnitScope 				inScope,
														AudioUnitElement		 	inElement,
														const void *				inData,
														UInt32						inDataSize);
#endif

protected:
	// MIDI dispatch
	/*! @method HandleMidiEvent */
	virtual OSStatus	HandleMidiEvent(		UInt8 	inStatus,
												UInt8 	inChannel,
												UInt8 	inData1,
												UInt8 	inData2,
												long 	inStartFrame);

	/*! @method HandleNonNoteEvent */
	virtual void		HandleNonNoteEvent (	UInt8	status, 
												UInt8	channel, 
												UInt8	data1, 
												UInt8	data2, 
												UInt32	inStartFrame);

#if TARGET_API_MAC_OSX
	/*! @method GetXMLNames */
	virtual ComponentResult		GetXMLNames(CFURLRef *outNameDocument) 
	{ return kAudioUnitErr_InvalidProperty; }	// if not overridden, it's unsupported
#endif

// channel messages
	/*! @method HandleNoteOn */
	virtual void		HandleNoteOn(			int 	inChannel,
												UInt8 	inNoteNumber,
												UInt8 	inVelocity,
												long 	inStartFrame) {}
												
	/*! @method HandleNoteOff */
	virtual void		HandleNoteOff(			int 	inChannel,
												UInt8 	inNoteNumber,
												UInt8 	inVelocity,
												long 	inStartFrame) {}
												
	/*! @method HandleControlChange */
	virtual void		HandleControlChange(	int 	inChannel,
												UInt8 	inController,
												UInt8 	inValue,
												long	inStartFrame) {}
												
	/*! @method HandlePitchWheel */
	virtual void		HandlePitchWheel(		int 	inChannel,
												UInt8 	inPitch1,
												UInt8 	inPitch2,
												long	inStartFrame) {}
												
	/*! @method HandleChannelPressure */
	virtual void		HandleChannelPressure(	int 	inChannel,
												UInt8 	inValue,
												long	inStartFrame) {}

	/*! @method HandleProgramChange */
	virtual void		HandleProgramChange(	int 	inChannel,
												UInt8 	inValue) {}

	/*! @method HandlePolyPressure */
	virtual void		HandlePolyPressure(		int 	inChannel,
												UInt8 	inKey,
												UInt8	inValue,
												long	inStartFrame) {}

	/*! @method HandleResetAllControllers */
	virtual void		HandleResetAllControllers(int 	inChannel) {}
	
	/*! @method HandleAllNotesOff */
	virtual void		HandleAllNotesOff(		int 	inChannel) {}
	
	/*! @method HandleAllSoundOff */
	virtual void		HandleAllSoundOff(		int 	inChannel) {}


//System messages   
	/*! @method HandleSysEx */
	virtual void		HandleSysEx(	unsigned char 			*inData,
                                        UInt32 					inLength ) {}

#if CA_AUTO_MIDI_MAP
	/* map manager */
	CAAUMIDIMapManager			*GetMIDIMapManager() {return mMapManager;};
	
#endif

												
private:
	/*! @var mAUBaseInstance */
	AUBase						& mAUBaseInstance;
	
#if CA_AUTO_MIDI_MAP
	/* map manager */
	CAAUMIDIMapManager			* mMapManager;
#endif
	
public:
	// component dispatcher
	/*! @method ComponentEntryDispatch */
	static ComponentResult		ComponentEntryDispatch(	ComponentParameters 		*params,
														AUMIDIBase 					*This);
};

#endif // __AUMIDIBase_h__
