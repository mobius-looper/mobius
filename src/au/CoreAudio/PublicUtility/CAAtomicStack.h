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
	TStack.h
	
=============================================================================*/

#ifndef __TStack_h__
#define __TStack_h__

#if __LP64__
	#include <libkern/OSAtomic.h>
#elif !defined(__COREAUDIO_USE_FLAT_INCLUDES__)
	#include <CoreServices/CoreServices.h>
#else
	#include <DriverSynchronization.h>
#endif

//  linked list FIFO stack, elements are pushed and popped atomically
//  class T must implement set_next() and get_next()
template <class T>
class TAtomicStack {
public:
	TAtomicStack() : mHead(NULL) { }
	
	// non-atomic routines, for use when initializing/deinitializing, operate NON-atomically
	void	push_NA(T *item)
	{
		item->set_next(mHead);
		mHead = item;
	}
	
	T *		pop_NA()
	{
		T *result = mHead;
		if (result)
			mHead = result->get_next();
		return result;
	}
	
	bool	empty() { return mHead == NULL; }
	
	// atomic routines
	void	push_atomic(T *item)
	{
		T *head;
		do {
			head = mHead;
			item->set_next(head);
		} while (!compare_and_swap(head, item, &mHead));
	}
	
	T *		pop_atomic()
	{
		T *result;
		do {
			if ((result = mHead) == NULL)
				break;
		} while (!compare_and_swap(result, result->get_next(), &mHead));
		return result;
	}
	
	T *		pop_all()
	{
		T *result;
		do {
			if ((result = mHead) == NULL)
				break;
		} while (!compare_and_swap(result, NULL, &mHead));
		return result;
	}
	
	bool	compare_and_swap(T *oldvalue, T *newvalue, T **pvalue)
	{
#if __LP64__
		return ::OSAtomicCompareAndSwap64Barrier(int64_t(oldvalue), int64_t(newvalue), (int64_t *)pvalue);
#else
		return ::CompareAndSwap(UInt32(oldvalue), UInt32(newvalue), (UInt32 *)pvalue);
#endif
	}
	
protected:
	T *		mHead;
};

#endif // __TStack_h__
