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
	AUDispatch.cpp
	
=============================================================================*/

#include "AUBase.h"


#if PRAGMA_STRUCT_ALIGN
	#pragma options align=mac68k
#elif PRAGMA_STRUCT_PACKPUSH
	#pragma pack(push, 2)
#elif PRAGMA_STRUCT_PACK
	#pragma pack(2)
#endif

#if	TARGET_API_MAC_OS8 || TARGET_API_MAC_OSX
struct AudioUnitInitializeGluePB {
	unsigned char                  componentFlags;
	unsigned char                  componentParamSize;
	short                          componentWhat;
	AudioUnit                      ci;
};


struct AudioUnitUninitializeGluePB {
	unsigned char                  componentFlags;
	unsigned char                  componentParamSize;
	short                          componentWhat;
	AudioUnit                      ci;
};

struct AudioUnitGetPropertyInfoGluePB {
	unsigned char                  componentFlags;
	unsigned char                  componentParamSize;
	short                          componentWhat;
	Boolean*                       outWritable;
	UInt32*                        outDataSize;
	AudioUnitElement               inElement;
	AudioUnitScope                 inScope;
	AudioUnitPropertyID            inID;
	AudioUnit                      ci;
};

struct AudioUnitGetPropertyGluePB {
	unsigned char                  componentFlags;
	unsigned char                  componentParamSize;
	short                          componentWhat;
	UInt32*                        ioDataSize;
	void*                          outData;
	AudioUnitElement               inElement;
	AudioUnitScope                 inScope;
	AudioUnitPropertyID            inID;
	AudioUnit                      ci;
};

struct AudioUnitSetPropertyGluePB {
	unsigned char                  componentFlags;
	unsigned char                  componentParamSize;
	short                          componentWhat;
	UInt32                         inDataSize;
	const void*                    inData;
	AudioUnitElement               inElement;
	AudioUnitScope                 inScope;
	AudioUnitPropertyID            inID;
	AudioUnit                      ci;
};

struct AudioUnitSetRenderNotificationGluePB {
	unsigned char                  componentFlags;
	unsigned char                  componentParamSize;
	short                          componentWhat;
	void*                          inProcRefCon;
	ProcPtr                        inProc;
	AudioUnit                      ci;
};

struct AudioUnitAddPropertyListenerGluePB {
	unsigned char                  componentFlags;
	unsigned char                  componentParamSize;
	short                          componentWhat;
	void*                          inProcRefCon;
	AudioUnitPropertyListenerProc  inProc;
	AudioUnitPropertyID            inID;
	AudioUnit                      ci;
};

struct AudioUnitRemovePropertyListenerGluePB {
	unsigned char                  componentFlags;
	unsigned char                  componentParamSize;
	short                          componentWhat;
	AudioUnitPropertyListenerProc  inProc;
	AudioUnitPropertyID            inID;
	AudioUnit                      ci;
};

struct AudioUnitGetParameterGluePB {
	unsigned char                  componentFlags;
	unsigned char                  componentParamSize;
	short                          componentWhat;
	Float32*                       outValue;
	AudioUnitElement               inElement;
	AudioUnitScope                 inScope;
	AudioUnitParameterID           inID;
	AudioUnit                      ci;
};

struct AudioUnitSetParameterGluePB {
	unsigned char                  componentFlags;
	unsigned char                  componentParamSize;
	short                          componentWhat;
	UInt32                         inBufferOffsetInFrames;
	Float32                        inValue;
	AudioUnitElement               inElement;
	AudioUnitScope                 inScope;
	AudioUnitParameterID           inID;
	AudioUnit                      ci;
};

struct AudioUnitScheduleParametersGluePB {
	unsigned char                  componentFlags;
	unsigned char                  componentParamSize;
	short                          componentWhat;
	UInt32                         inNumParamEvents;
	const AudioUnitParameterEvent* inParameterEvent;
	AudioUnit                      ci;
};


struct AudioUnitRenderGluePB {
	unsigned char                  componentFlags;
	unsigned char                  componentParamSize;
	short                          componentWhat;
	AudioBufferList*               ioData;
	UInt32                         inNumberFrames;
	UInt32                         inOutputBusNumber;
	const AudioTimeStamp*          inTimeStamp;
	AudioUnitRenderActionFlags*    ioActionFlags;
	AudioUnit                      ci;
};

struct AudioUnitResetGluePB {
	unsigned char                  componentFlags;
	unsigned char                  componentParamSize;
	short                          componentWhat;
	AudioUnitElement               inElement;
	AudioUnitScope                 inScope;
	AudioUnit                      ci;
};
#elif	TARGET_OS_WIN32
struct AudioUnitInitializeGluePB {
	unsigned char                  componentFlags;
	unsigned char                  componentParamSize;
	short                          componentWhat;
};


struct AudioUnitUninitializeGluePB {
	unsigned char                  componentFlags;
	unsigned char                  componentParamSize;
	short                          componentWhat;
};

struct AudioUnitGetPropertyInfoGluePB {
	unsigned char                  componentFlags;
	unsigned char                  componentParamSize;
	short                          componentWhat;
	long						   inID;
	long                 		   inScope;
	long                           inElement;
	long                           outDataSize;
	long	                       outWritable;
};

struct AudioUnitGetPropertyGluePB {
	unsigned char                  componentFlags;
	unsigned char                  componentParamSize;
	short                          componentWhat;
	long                           inID;
	long                           inScope;
	long                           inElement;
	long                           outData;
	long                           ioDataSize;
};

struct AudioUnitSetPropertyGluePB {
	unsigned char                  componentFlags;
	unsigned char                  componentParamSize;
	short                          componentWhat;
	long                           inID;
	long                           inScope;
	long                           inElement;
	long                           inData;
	long                           inDataSize;
};

struct AudioUnitSetRenderNotificationGluePB {
	unsigned char                  componentFlags;
	unsigned char                  componentParamSize;
	short                          componentWhat;
	long                           inProc;
	long                           inProcRefCon;
};

struct AudioUnitAddPropertyListenerGluePB {
	unsigned char                  componentFlags;
	unsigned char                  componentParamSize;
	short                          componentWhat;
	long                           inID;
	long                           inProc;
	long                           inProcRefCon;
};

struct AudioUnitRemovePropertyListenerGluePB {
	unsigned char                  componentFlags;
	unsigned char                  componentParamSize;
	short                          componentWhat;
	long                           inID;
	long                           inProc;
};

struct AudioUnitGetParameterGluePB {
	unsigned char                  componentFlags;
	unsigned char                  componentParamSize;
	short                          componentWhat;
	long                           inID;
	long                           inScope;
	long                           inElement;
	long                           outValue;
};

struct AudioUnitSetParameterGluePB {
	unsigned char                  componentFlags;
	unsigned char                  componentParamSize;
	short                          componentWhat;
	long                           inID;
	long                           inScope;
	long                           inElement;
	long                           inValue;
	long                           inBufferOffsetInFrames;
};

struct AudioUnitScheduleParametersGluePB {
	unsigned char                  componentFlags;
	unsigned char                  componentParamSize;
	short                          componentWhat;
	long                           inParameterEvent;
	long                           inNumParamEvents;
};

struct AudioUnitRenderSliceGluePB {
	unsigned char                  componentFlags;
	unsigned char                  componentParamSize;
	short                          componentWhat;
	long                           inActionFlags;
	long                           inTimeStamp;
	long                           inOutputBusNumber;
	long                           ioData;
};

struct AudioUnitRenderGluePB {
	unsigned char                  componentFlags;
	unsigned char                  componentParamSize;
	short                          componentWhat;
	long                           ioActionFlags;
	long                           inTimeStamp;
	long                           inOutputBusNumber;
	long						   inNumberFrames;
	long                           ioData;
};

struct AudioUnitResetGluePB {
	unsigned char                  componentFlags;
	unsigned char                  componentParamSize;
	short                          componentWhat;
	long                           inScope;
	long                           inElement;
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






#if AU_DEBUG_DISPATCHER
	#include "AUDebugDispatcher.h"

	#define INIT_DEBUG_DISPATCHER(This)		\
		UInt64 nowTime = 0;					\
		if (This->mDebugDispatcher != NULL) \
			nowTime = CAHostTimeBase::GetTheCurrentTime();
#endif

ComponentResult	AUBase::ComponentEntryDispatch(ComponentParameters *params, AUBase *This)
{
	if (This == NULL) return paramErr;

#if AU_DEBUG_DISPATCHER
	INIT_DEBUG_DISPATCHER(This)
#endif
	
	ComponentResult result = noErr;


	switch (params->what) {
	case kComponentCanDoSelect:
		switch (params->params[0]) {
	// any selectors
			case kAudioUnitInitializeSelect:
			case kAudioUnitUninitializeSelect:
			case kAudioUnitGetPropertyInfoSelect:
			case kAudioUnitGetPropertySelect:
			case kAudioUnitSetPropertySelect:
			case kAudioUnitAddPropertyListenerSelect:
			case kAudioUnitRemovePropertyListenerSelect:
			case kAudioUnitGetParameterSelect:
			case kAudioUnitSetParameterSelect:
			case kAudioUnitResetSelect:
				result = 1;
				break;
	// v1 selectors

	// v2 selectors
			case kAudioUnitAddRenderNotifySelect:
			case kAudioUnitRemoveRenderNotifySelect:
			case kAudioUnitScheduleParametersSelect:
			case kAudioUnitRenderSelect:
				result = (This->AudioUnitAPIVersion() > 1);
				break;
				
			default:
				return ComponentBase::ComponentEntryDispatch(params, This);
		}
		break;
		
	case kAudioUnitInitializeSelect:
	{
		result = This->DoInitialize();

				#if AU_DEBUG_DISPATCHER
					if (This->mDebugDispatcher)
						This->mDebugDispatcher->Initialize (nowTime, result);
				#endif
	}
		break;
		
	case kAudioUnitUninitializeSelect:
	{
		This->DoCleanup();
		result = noErr;

				#if AU_DEBUG_DISPATCHER
					if (This->mDebugDispatcher)
						This->mDebugDispatcher->Uninitialize (nowTime, result);
				#endif
	}
		break;

	case kAudioUnitGetPropertyInfoSelect:
		{
			AudioUnitGetPropertyInfoGluePB *p = (AudioUnitGetPropertyInfoGluePB *)params;
			#if	!TARGET_OS_WIN32
				AudioUnitPropertyID	pinID = p->inID;
				AudioUnitScope		pinScope = p->inScope;
				AudioUnitElement	pinElement = p->inElement;
				UInt32*				poutDataSize = p->outDataSize;
				Boolean*			poutWritable = p->outWritable;
			#else
				AudioUnitPropertyID	pinID = (*((AudioUnitPropertyID*)&p->inID));
				AudioUnitScope		pinScope = (*((AudioUnitScope*)&p->inScope));
				AudioUnitElement	pinElement = (*((AudioUnitElement*)&p->inElement));
				UInt32*				poutDataSize = (*((UInt32**)&p->outDataSize));
				Boolean*			poutWritable = (*((Boolean**)&p->outWritable));
			#endif

			// pass our own copies so that we assume responsibility for testing
			// the caller's pointers against null and our C++ classes can
			// always assume they're non-null
			UInt32 dataSize;
			Boolean writable;
			
			result = This->DispatchGetPropertyInfo(pinID, pinScope, pinElement, dataSize, writable);
			if (poutDataSize != NULL)
				*poutDataSize = dataSize;
			if (poutWritable != NULL)
				*poutWritable = writable;

			#if AU_DEBUG_DISPATCHER
				if (This->mDebugDispatcher)
					This->mDebugDispatcher->GetPropertyInfo (nowTime, result, pinID, pinScope, pinElement, 
																poutDataSize, poutWritable);
			#endif
		
			
		}
		break;

	case kAudioUnitGetPropertySelect:
		{
			AudioUnitGetPropertyGluePB *p = (AudioUnitGetPropertyGluePB *)params;
			#if	!TARGET_OS_WIN32
				AudioUnitPropertyID	pinID = p->inID;
				AudioUnitScope		pinScope = p->inScope;
				AudioUnitElement	pinElement = p->inElement;
				void*				poutData = p->outData;
				UInt32*				pioDataSize = p->ioDataSize;
			#else
				AudioUnitPropertyID	pinID = (*((AudioUnitPropertyID*)&p->inID));
				AudioUnitScope		pinScope = (*((AudioUnitScope*)&p->inScope));
				AudioUnitElement	pinElement = (*((AudioUnitElement*)&p->inElement));
				void*				poutData = (*((void**)&p->outData));
				UInt32*				pioDataSize = (*((UInt32**)&p->ioDataSize));
			#endif

			UInt32 actualPropertySize, clientBufferSize;
			Boolean writable;
			char *tempBuffer;
			void *destBuffer;
			
			if (pioDataSize == NULL) {
				debug_string("AudioUnitGetProperty: null size pointer");
				result = paramErr;
				goto finishGetProperty;
			}
			if (poutData == NULL) {
				UInt32 dataSize;
				Boolean writable;
				
				result = This->DispatchGetPropertyInfo(pinID, pinScope, pinElement, dataSize, writable);
				*pioDataSize = dataSize;
				goto finishGetProperty;
			}
			
			clientBufferSize = *pioDataSize;
			if (clientBufferSize == 0) {
				debug_string("AudioUnitGetProperty: *ioDataSize == 0 on entry");
				// $$$ or should we allow this as a shortcut for finding the size?
				result = paramErr;
				goto finishGetProperty;
			}
			
			result = This->DispatchGetPropertyInfo(pinID, pinScope, pinElement, 
													actualPropertySize, writable);
			if (result) 
				goto finishGetProperty;
			
			if (clientBufferSize < actualPropertySize) {
				tempBuffer = new char[actualPropertySize];
				destBuffer = tempBuffer;
			} else {
				tempBuffer = NULL;
				destBuffer = poutData;
			}
			
			result = This->DispatchGetProperty(pinID, pinScope, pinElement, destBuffer);
			
			if (result == noErr) {
				if (clientBufferSize < actualPropertySize) {
					memcpy(poutData, tempBuffer, clientBufferSize);
					delete[] tempBuffer;
					// pioDataSize remains correct, the number of bytes we wrote
				} else
					*pioDataSize = actualPropertySize;
			} else
				*pioDataSize = 0;

			finishGetProperty:
				
				#if AU_DEBUG_DISPATCHER
					if (This->mDebugDispatcher)
						This->mDebugDispatcher->GetProperty (nowTime, result, pinID, pinScope, pinElement, 
																pioDataSize, poutData);
				#else
					;
				#endif

		}
		break;
		
	case kAudioUnitSetPropertySelect:
		{
			AudioUnitSetPropertyGluePB *p = (AudioUnitSetPropertyGluePB *)params;
			#if	!TARGET_OS_WIN32
				AudioUnitPropertyID	pinID = p->inID;
				AudioUnitScope		pinScope = p->inScope;
				AudioUnitElement	pinElement = p->inElement;
				const void*			pinData = p->inData;
				UInt32				pinDataSize = p->inDataSize;
			#else
				AudioUnitPropertyID	pinID = (*((AudioUnitPropertyID*)&p->inID));
				AudioUnitScope		pinScope = (*((AudioUnitScope*)&p->inScope));
				AudioUnitElement	pinElement = (*((AudioUnitElement*)&p->inElement));
				const void*			pinData = (*((void**)&p->inData));
				UInt32				pinDataSize = (*((UInt32*)&p->inDataSize));
			#endif
			
			if (pinData && pinDataSize)
				result = This->DispatchSetProperty(pinID, pinScope, pinElement, pinData, pinDataSize);
			else {
				if (pinData == NULL && pinDataSize == 0) {
					result = This->DispatchRemovePropertyValue (pinID, pinScope, pinElement);
				} else {
					if (pinData == NULL) {
						debug_string("AudioUnitSetProperty: inData == NULL");
						result = paramErr;
						goto finishSetProperty;
					}

					if (pinDataSize == 0) {
						debug_string("AudioUnitSetProperty: inDataSize == 0");
						result = paramErr;
						goto finishSetProperty;
					}
				}
			}
			finishSetProperty:
				
				#if AU_DEBUG_DISPATCHER
					if (This->mDebugDispatcher)
						This->mDebugDispatcher->SetProperty (nowTime, result, pinID, pinScope, pinElement, 
														pinData, pinDataSize);
				#else
					;
				#endif

		}
		break;
		
	case kAudioUnitAddPropertyListenerSelect:
		{
			AudioUnitAddPropertyListenerGluePB *p = (AudioUnitAddPropertyListenerGluePB *)params;
			#if	!TARGET_OS_WIN32
				AudioUnitPropertyID				pinID = p->inID;
				AudioUnitPropertyListenerProc	pinProc = p->inProc;
				void*							pinProcRefCon = p->inProcRefCon;
			#else
				AudioUnitPropertyID				pinID = (*((AudioUnitPropertyID*)&p->inID));
				AudioUnitPropertyListenerProc	pinProc = (*((AudioUnitPropertyListenerProc*)&p->inProc));
				void*							pinProcRefCon = (*((void**)&p->inProcRefCon));
			#endif
			
			result = This->AddPropertyListener(pinID, pinProc, pinProcRefCon);
		}
		break;

	case kAudioUnitRemovePropertyListenerSelect:
		{
			AudioUnitRemovePropertyListenerGluePB *p = (AudioUnitRemovePropertyListenerGluePB *)params;
			#if	!TARGET_OS_WIN32
				AudioUnitPropertyID				pinID = p->inID;
				AudioUnitPropertyListenerProc	pinProc = p->inProc;
			#else
				AudioUnitPropertyID				pinID = (*((AudioUnitPropertyID*)&p->inID));
				AudioUnitPropertyListenerProc	pinProc = (*((AudioUnitPropertyListenerProc*)&p->inProc));
			#endif
			
			result = This->RemovePropertyListener(pinID, pinProc);
		}
		break;

		
	case kAudioUnitAddRenderNotifySelect:
		{
			AudioUnitSetRenderNotificationGluePB *p = (AudioUnitSetRenderNotificationGluePB *)params;
			#if	!TARGET_OS_WIN32
				ProcPtr							pinProc = p->inProc;
				void*							pinProcRefCon = p->inProcRefCon;
			#else
				ProcPtr							pinProc = (*((ProcPtr*)&p->inProc));
				void*							pinProcRefCon = (*((void**)&p->inProcRefCon));
			#endif
			
			result = This->SetRenderNotification (pinProc, pinProcRefCon);
		}
		break;


	case kAudioUnitRemoveRenderNotifySelect:
		{
			AudioUnitSetRenderNotificationGluePB *p = (AudioUnitSetRenderNotificationGluePB *)params;
			#if	!TARGET_OS_WIN32
				ProcPtr							pinProc = p->inProc;
				void*							pinProcRefCon = p->inProcRefCon;
			#else
				ProcPtr							pinProc = (*((ProcPtr*)&p->inProc));
				void*							pinProcRefCon = (*((void**)&p->inProcRefCon));
			#endif
			
			result = This->RemoveRenderNotification (pinProc, pinProcRefCon);
		}
		break;

	case kAudioUnitGetParameterSelect:
		{
			AudioUnitGetParameterGluePB *p = (AudioUnitGetParameterGluePB *)params;
			#if	!TARGET_OS_WIN32
				AudioUnitPropertyID				pinID = p->inID;
				AudioUnitScope					pinScope = p->inScope;
				AudioUnitElement				pinElement = p->inElement;
				Float32*						poutValue = p->outValue;
			#else
				AudioUnitPropertyID				pinID = (*((AudioUnitPropertyID*)&p->inID));
				AudioUnitScope					pinScope = (*((AudioUnitScope*)&p->inScope));
				AudioUnitElement				pinElement = (*((AudioUnitElement*)&p->inElement));
				Float32*						poutValue = (*((Float32**)&p->outValue));
			#endif
			
			result = (poutValue == NULL ? paramErr : This->GetParameter(pinID, pinScope, pinElement, *poutValue));
		}
		break;

	case kAudioUnitSetParameterSelect:
		{
			AudioUnitSetParameterGluePB *p = (AudioUnitSetParameterGluePB *)params;
			#if	!TARGET_OS_WIN32
				AudioUnitPropertyID				pinID = p->inID;
				AudioUnitScope					pinScope = p->inScope;
				AudioUnitElement				pinElement = p->inElement;
				Float32							pinValue = p->inValue;
				UInt32							pinBufferOffsetInFrames = p->inBufferOffsetInFrames;
			#else
				AudioUnitPropertyID				pinID = (*((AudioUnitPropertyID*)&p->inID));
				AudioUnitScope					pinScope = (*((AudioUnitScope*)&p->inScope));
				AudioUnitElement				pinElement = (*((AudioUnitElement*)&p->inElement));
				Float32							pinValue = (*((Float32*)&p->inValue));
				UInt32							pinBufferOffsetInFrames = (*((UInt32*)&p->inBufferOffsetInFrames));
			#endif
						
			result = This->SetParameter(pinID, pinScope, pinElement, pinValue, pinBufferOffsetInFrames);
		}
		break;

	case kAudioUnitScheduleParametersSelect:
		{
			if (This->AudioUnitAPIVersion() > 1)
			{
				AudioUnitScheduleParametersGluePB *p = (AudioUnitScheduleParametersGluePB *)params;
				#if	!TARGET_OS_WIN32
					const AudioUnitParameterEvent*	pinParameterEvent = p->inParameterEvent;
					UInt32							pinNumParamEvents = p->inNumParamEvents;
				#else
					const AudioUnitParameterEvent*	pinParameterEvent = (*((const AudioUnitParameterEvent**)&p->inParameterEvent));
					UInt32							pinNumParamEvents = (*((UInt32*)&p->inNumParamEvents));
				#endif
							
				result = This->ScheduleParameter (pinParameterEvent, pinNumParamEvents);
			} else
				result = badComponentSelector;
		}
		break;


	case kAudioUnitRenderSelect:
		{
				AudioUnitRenderGluePB *p = (AudioUnitRenderGluePB *)params;
				#if	!TARGET_OS_WIN32
					AudioUnitRenderActionFlags*				pinActionFlags = p->ioActionFlags;
					const AudioTimeStamp*					pinTimeStamp = p->inTimeStamp;
					UInt32									pinOutputBusNumber = p->inOutputBusNumber;
					UInt32									pinNumberFrames = p->inNumberFrames;
					AudioBufferList*						pioData = p->ioData;
				#else
					AudioUnitRenderActionFlags*				pinActionFlags = (*((AudioUnitRenderActionFlags**)&p->ioActionFlags));
					const AudioTimeStamp*					pinTimeStamp = (*((const AudioTimeStamp**)&p->inTimeStamp));
					UInt32									pinOutputBusNumber = (*((UInt32*)&p->inOutputBusNumber));
					UInt32									pinNumberFrames = p->inNumberFrames;
					AudioBufferList*						pioData = *(AudioBufferList **)&p->ioData;
				#endif
				AudioUnitRenderActionFlags tempFlags;
				
				if (pinTimeStamp == NULL || pioData == NULL)
					result = paramErr;
				else {
					if (pinActionFlags == NULL) {
						tempFlags = 0;
						pinActionFlags = &tempFlags;
					}
					result = This->DoRender(*pinActionFlags, *pinTimeStamp, pinOutputBusNumber, pinNumberFrames, *pioData);
				}
				
				#if AU_DEBUG_DISPATCHER
					if (This->mDebugDispatcher)
						This->mDebugDispatcher->Render (nowTime, result, pinActionFlags, pinTimeStamp, 
														pinOutputBusNumber, pinNumberFrames, pioData);
				#endif
			
		}
		break;

	case kAudioUnitResetSelect:
		{
			AudioUnitResetGluePB *p = (AudioUnitResetGluePB *)params;
			#if	!TARGET_OS_WIN32
				AudioUnitScope					pinScope = p->inScope;
				AudioUnitElement				pinElement = p->inElement;
			#else
				AudioUnitScope					pinScope = (*((AudioUnitScope*)&p->inScope));
				AudioUnitElement				pinElement = (*((AudioUnitElement*)&p->inElement));
			#endif

			This->mLastRenderedSampleTime = -1;
			result = This->Reset(pinScope, pinElement);
		}
		break;

	default:
		result = ComponentBase::ComponentEntryDispatch(params, This);
		break;
	}

	return result;
}

// Fast dispatch entry points -- these need to replicate all error-checking logic from above

#if TARGET_OS_WIN32
	#undef pascal
	#define pascal
#endif

pascal ComponentResult AudioUnitBaseGetParameter(	AUBase *				This,
													AudioUnitParameterID	inID,
													AudioUnitScope			inScope,
													AudioUnitElement		inElement,
													float					*outValue)
{
	ComponentResult result = noErr;
	
	try {
		if (This == NULL || outValue == NULL) return paramErr;
		result = This->GetParameter(inID, inScope, inElement, *outValue);
	}
	COMPONENT_CATCH
	
	return result;
}

pascal ComponentResult AudioUnitBaseSetParameter(	AUBase * 				This,
													AudioUnitParameterID	inID,
													AudioUnitScope			inScope,
													AudioUnitElement		inElement,
													float					inValue,
													UInt32					inBufferOffset)
{
	ComponentResult result = noErr;
	
	try {
		if (This == NULL) return paramErr;
		result = This->SetParameter(inID, inScope, inElement, inValue, inBufferOffset);
	}
	COMPONENT_CATCH
	
	return result;
}


pascal ComponentResult AudioUnitBaseRender(			AUBase *				This,
													AudioUnitRenderActionFlags *ioActionFlags,
													const AudioTimeStamp *	inTimeStamp,
													UInt32					inBusNumber,
													UInt32					inNumberFrames,
													AudioBufferList *		ioData)
{
	if (inTimeStamp == NULL || ioData == NULL) return paramErr;

#if AU_DEBUG_DISPATCHER
	INIT_DEBUG_DISPATCHER(This)
#endif
	
	ComponentResult result = noErr;
	AudioUnitRenderActionFlags tempFlags;
	
	try {
		if (ioActionFlags == NULL) {
			tempFlags = 0;
			ioActionFlags = &tempFlags;
		}
		result = This->DoRender(*ioActionFlags, *inTimeStamp, inBusNumber, inNumberFrames, *ioData);
	}
	COMPONENT_CATCH

	#if AU_DEBUG_DISPATCHER
		if (This->mDebugDispatcher)
			This->mDebugDispatcher->Render (nowTime, result, ioActionFlags, inTimeStamp, inBusNumber, inNumberFrames, ioData);
	#endif
	
	return result;
}
