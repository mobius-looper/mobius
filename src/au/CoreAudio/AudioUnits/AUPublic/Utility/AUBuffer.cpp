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
	AUBuffer.cpp

=============================================================================*/

#include "AUBuffer.h"
#include <stdlib.h>

AUBufferList::~AUBufferList()
{
	Deallocate();
	if (mPtrs)
		free(mPtrs);
}

void				AUBufferList::Allocate(const CAStreamBasicDescription &format, UInt32 nFrames)
{
	UInt32 nStreams;
	UInt32 channelsPerStream;
	if (format.IsInterleaved()) {
		nStreams = 1;
		channelsPerStream = format.mChannelsPerFrame;
	} else {
		nStreams = format.mChannelsPerFrame;
		channelsPerStream = 1;
	}

	// careful -- the I/O thread could be running!
	if (nStreams > mAllocatedStreams) {
		mPtrs = (AudioBufferList *)realloc(mPtrs, offsetof(AudioBufferList, mBuffers) + nStreams * sizeof(AudioBuffer));
		mAllocatedStreams = nStreams;
	}
	UInt32 bytesPerStream = (nFrames * format.mBytesPerFrame + 0xF) & ~0xF;
	UInt32 nBytes = nStreams * bytesPerStream;	
	if (nBytes > mAllocatedBytes) {
		if (mExternalMemory) {
			mExternalMemory = false;
			mMemory = NULL;
		}
		mMemory = (Byte *)realloc(mMemory, nBytes);
		mAllocatedBytes = nBytes;
	}
	mAllocatedFrames = nFrames;
	mPtrState = kPtrsInvalid;
}

void				AUBufferList::Deallocate()
{
	mAllocatedStreams = 0;
	mAllocatedFrames = 0;
	mAllocatedBytes = 0;
// this causes a world of hurt if someone upstream disconnects during I/O (SysSoundGraph)
/*	if (mPtrs) {
		printf("deallocating bufferlist %08X\n", int(mPtrs));
		free(mPtrs);
		mPtrs = NULL;
	} */
	if (mMemory) {
		if (mExternalMemory)
			mExternalMemory = false;
		else
			free(mMemory);
		mMemory = NULL;
	}
	mPtrState = kPtrsInvalid;
}

AudioBufferList &	AUBufferList::PrepareBuffer(const CAStreamBasicDescription &format, UInt32 nFrames)
{
	UInt32 nStreams;
	UInt32 channelsPerStream;
	if (format.IsInterleaved()) {
		nStreams = 1;
		channelsPerStream = format.mChannelsPerFrame;
	} else {
		nStreams = format.mChannelsPerFrame;
		channelsPerStream = 1;
		if (nStreams > mAllocatedStreams)
			COMPONENT_THROW(kAudioUnitErr_FormatNotSupported);
	}
	
	AudioBufferList *abl = mPtrs;
	abl->mNumberBuffers = nStreams;
	AudioBuffer *buf = abl->mBuffers;
	Byte *mem = mMemory;
	UInt32 streamInterval = (mAllocatedFrames * format.mBytesPerFrame + 0xF) & ~0xF;
	UInt32 bytesPerBuffer = nFrames * format.mBytesPerFrame;
	for ( ; nStreams--; ++buf) {
		buf->mNumberChannels = channelsPerStream;
		buf->mData = mem;
		buf->mDataByteSize = bytesPerBuffer;
		mem += streamInterval;
	}
	if (UInt32(mem - mMemory) > mAllocatedBytes)
		COMPONENT_THROW(kAudioUnitErr_TooManyFramesToProcess);
	mPtrState = kPtrsToMyMemory;
	return *mPtrs;
}

AudioBufferList &	AUBufferList::PrepareNullBuffer(const CAStreamBasicDescription &format, UInt32 nFrames)
{
	UInt32 nStreams;
	UInt32 channelsPerStream;
	if (format.IsInterleaved()) {
		nStreams = 1;
		channelsPerStream = format.mChannelsPerFrame;
	} else {
		nStreams = format.mChannelsPerFrame;
		channelsPerStream = 1;
		if (nStreams > mAllocatedStreams)
			COMPONENT_THROW(kAudioUnitErr_FormatNotSupported);
	}
	AudioBufferList *abl = mPtrs;
	abl->mNumberBuffers = nStreams;
	AudioBuffer *buf = abl->mBuffers;
	UInt32 bytesPerBuffer = nFrames * format.mBytesPerFrame;
	for ( ; nStreams--; ++buf) {
		buf->mNumberChannels = channelsPerStream;
		buf->mData = NULL;
		buf->mDataByteSize = bytesPerBuffer;
	}
	mPtrState = kPtrsToExternalMemory;
	return *mPtrs;
}

// this should NOT be called while I/O is in process
void		AUBufferList::UseExternalBuffer(const CAStreamBasicDescription &format, const AudioUnitExternalBuffer &buf)
{
	UInt32 alignedSize = buf.size & ~0xF;
	if (mMemory != NULL && alignedSize >= mAllocatedBytes) {
		// don't accept the buffer if we already have one and it's big enough
		// if we don't already have one, we don't need one
		Byte *oldMemory = mMemory;
		mMemory = buf.buffer;
		mAllocatedBytes = alignedSize;
		// from Allocate(): nBytes = nStreams * nFrames * format.mBytesPerFrame;	
		// thus: nFrames = nBytes / (nStreams * format.mBytesPerFrame)
		mAllocatedFrames = mAllocatedBytes / (format.NumberChannelStreams() * format.mBytesPerFrame);
		mExternalMemory = true;
		free(oldMemory);
	}
}

#if DEBUG
void	AUBufferList::PrintBuffer(const char *label, int subscript, const AudioBufferList &abl, UInt32 nFrames, bool asFloats)
{
	printf("  %s [%d] 0x%08lX:\n", label, subscript, long(&abl));
	const AudioBuffer *buf = abl.mBuffers;
	for (UInt32 i = 0; i < abl.mNumberBuffers; ++buf, ++i) {
		printf("      [%2ld] %5ldbytes %ldch @ 0x%08lX: ", i, buf->mDataByteSize, buf->mNumberChannels, long(buf->mData));
		if (buf->mData != NULL) {
			UInt32 nSamples = nFrames * buf->mNumberChannels;
			for (UInt32 j = 0; j < nSamples; ++j) {
				if (nSamples > 16 && (j % 16) == 0)
					printf("\n\t");
				if (asFloats)
					printf(" %6.3f", ((float *)buf->mData)[j]);
				else
					printf(" %08lX", ((UInt32 *)buf->mData)[j]);
			}
		}
		printf("\n");
	}
}
#endif
