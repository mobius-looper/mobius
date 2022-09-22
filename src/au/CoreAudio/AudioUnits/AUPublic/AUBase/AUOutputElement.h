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
	AUOutputElement.h
	
=============================================================================*/

#ifndef __AUOutput_h__
#define __AUOutput_h__

#include "AUScopeElement.h"
#include "AUBuffer.h"

	/*! @class AUOutputElement */
class AUOutputElement : public AUIOElement {
public:
	/*! @ctor AUOutputElement */
						AUOutputElement(AUBase *audioUnit);

	// AUElement override
	/*! @method SetStreamFormat */
	virtual OSStatus	SetStreamFormat(const CAStreamBasicDescription &desc);
	/*! @method NeedsBufferSpace */
	virtual bool		NeedsBufferSpace() const { return true; }
						
#if 0
	// Return the output's cache buffer.  Must render into this, in order to support fan-out
	// and to protect against the possibility of the client having passed us NULL buffer pointers.
	AudioBufferList &	GetOutputBuffer() {
							return *mIOBuffer.GetOutputBuffer();
						}

	AudioBufferList &	PrepareOutputBuffer(UInt32 nFrames) {
							return mIOBuffer.PrepareOutputBuffer(nFrames);
						}

	AudioBufferList &	PrepareZeroedOutputBuffer(UInt32 nFrames) {
							return mIOBuffer.PrepareZeroedOutputBuffer(nFrames);
						}

	// This has to be called at the end of Render().
	// Copy the cached rendered output to a Render() ioBuffer.
	// In the optimized case of the client having passed NULL buffer pointers to us,
	// we simply replace them with pointers to our cached rendered output.
	void				CopyOutputBuffer(AudioBufferList &destbuflist)
	{
		// return rendered (possibly cached) output
		const AudioBufferList &srcbuflist = mIOBuffer.GetBuffer();
		const AudioBuffer *srcbuf = srcbuflist.mBuffers;
		AudioBuffer *destbuf = destbuflist.mBuffers;
		for (UInt32 i = srcbuflist.mNumberBuffers; i--; ++srcbuf, ++destbuf) {
			if (destbuf->mData != NULL)
				// copy output into client's buffer
				memcpy(destbuf->mData, srcbuf->mData, srcbuf->mDataByteSize);
			else
				// client passed null, so we give him a pointer into our cached output
				destbuf->mData = srcbuf->mData;
			destbuf->mDataByteSize = srcbuf->mDataByteSize;
			destbuf->mNumberChannels = srcbuf->mNumberChannels;
		}
	}
#endif
};

#endif // __AUOutput_h__
