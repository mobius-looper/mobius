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
	AUInputElement.h
	
=============================================================================*/

#ifndef __AUInput_h__
#define __AUInput_h__

#include "AUScopeElement.h"
#include "AUBuffer.h"

/*! @class AUInputElement */
class AUInputElement : public AUIOElement {
public:
	
	/*! @ctor AUInputElement */
						AUInputElement(AUBase *audioUnit);
	/*! @dtor ~AUInputElement */
	virtual				~AUInputElement() { }

	// AUElement override
	/*! @method SetStreamFormat */
	virtual OSStatus	SetStreamFormat(const CAStreamBasicDescription &desc);
	/*! @method NeedsBufferSpace */
	virtual bool		NeedsBufferSpace() const { return IsCallback(); }

	/*! @method SetConnection */
	void				SetConnection(const AudioUnitConnection &conn);
	/*! @method SetInputCallback */
	void				SetInputCallback(ProcPtr proc, void *refCon);

	/*! @method IsActive */
	bool				IsActive() const { return mInputType != kNoInput; }
	/*! @method IsCallback */
	bool				IsCallback() const { return mInputType == kFromCallback; }
	/*! @method HasConnection */
	bool				HasConnection() const { return mInputType == kFromConnection; }

	/*! @method PullInput */
	ComponentResult		PullInput(	AudioUnitRenderActionFlags &  	ioActionFlags,
									const AudioTimeStamp &			inTimeStamp,
									AudioUnitElement				inElement,
									UInt32							inNumberFrames);

#if 0
	// prepare an input element's I/O buffer for PullInput -- uses its mBufferPtr
	AudioBufferList &	PrepareNullInputBuffer(UInt32 nFrames) {
							return mIOBuffer.PrepareNullInputBuffer(nFrames);
						}

	AudioBufferList &	PrepareNonNullInputBuffer(UInt32 nFrames) {
							return mIOBuffer.PrepareNonNullInputBuffer(nFrames);
						}

	// valid after PullInput
	const AudioBufferList &	GetInputBuffer() { return mIOBuffer.GetBuffer(); }
	
#endif

protected:
	/*! @method Disconnect */
	void				Disconnect();

	enum EInputType { kNoInput, kFromConnection, kFromCallback };

	/*! @var mInputType */
	EInputType					mInputType;
	/*! @var mCritical */
	bool						mCritical;

	// if from callback:
	/*! @var mInputProc */
	ProcPtr						mInputProc;
	/*! @var mInputProcRefCon */
	void *						mInputProcRefCon;
	
	// if from connection:
	/*! @var mConnection */
	AudioUnitConnection			mConnection;
	/*! @var mConnRenderProc */
	ProcPtr						mConnRenderProc;
	/*! @var mConnInstanceStorage */
	void *						mConnInstanceStorage;		// for the input component
};

#endif // __AUInput_h__
