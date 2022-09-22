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
	ViewComponentShim.cpp
	
=============================================================================*/

#include <CoreServices/CoreServices.h>
#include <mach-o/dyld.h>

static const struct mach_header *gViewComponentsImage = NULL;

static void	*LookupSymbol(char *symbolName)
{
if (gViewComponentsImage == NULL) {
		CFBundleRef bundle = CFBundleGetBundleWithIdentifier(CFSTR("com.Acme.AUMultitap"));
		if (!bundle) return NULL;

		CFURLRef loc = CFBundleCopyBundleURL (bundle);
		if (!loc) { return NULL; }

		CFURLRef fullPath = CFURLCreateCopyAppendingPathComponent (NULL, loc, CFSTR("Contents/MacOS/MultitapAUView"), false);
		if (!fullPath) { CFRelease (loc); return NULL; }
		
		CFStringRef str = CFURLCopyFileSystemPath (fullPath, kCFURLPOSIXPathStyle);
		if (!str) { CFRelease (fullPath); CFRelease (loc); return NULL; } 

		char* path = (char*)malloc (CFStringGetLength(str) + 32);
		CFStringGetCString (str, path, CFStringGetLength(str) + 32, kCFStringEncodingASCII);
				
		gViewComponentsImage = NSAddImage(path, NSADDIMAGE_OPTION_RETURN_ON_ERROR);

		CFRelease (loc);
		CFRelease (fullPath);
		CFRelease (str);
		free(path);
		if (gViewComponentsImage == NULL)
            return NULL;
	}
	NSSymbol symbol = NSLookupSymbolInImage(gViewComponentsImage, symbolName, 
		NSLOOKUPSYMBOLINIMAGE_OPTION_RETURN_ON_ERROR);
	if (symbol == NULL)
		return NULL;
	return NSAddressOfSymbol(symbol);
}

static ComponentRoutineProcPtr MultitapAUViewEntry = NULL;

extern "C" ComponentResult MultitapAUViewEntryShim(ComponentParameters *params, Handle componentStorage)
{
	if (MultitapAUViewEntry == NULL) {
		MultitapAUViewEntry = (ComponentRoutineProcPtr)LookupSymbol("_MultitapAUViewEntry");
		if (MultitapAUViewEntry == NULL)
			return unresolvedComponentDLLErr;
	}
	return (*MultitapAUViewEntry)(params, componentStorage);
}
