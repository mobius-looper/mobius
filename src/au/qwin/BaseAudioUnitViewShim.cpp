/*
 * This is a level of indirection to the AU view entry
 * point that loads the view from a dynamic library.
 * This suggests that it is not necessary to have the
 * view in an different bundle, not sure what the
 * consequences would be.
 *
 * If you want to use a shim, specify the entry
 * point BaseAudioUnitViewEntryShim in the 
 * BaseAudioUnit.r definition for the view.
 *
 * The view dynlib is expected to be BaseAudioUnitView
 * !! need a way to configure this.
 */

#include <CoreServices/CoreServices.h>
#include <mach-o/dyld.h>

#include "Trace.h"

//////////////////////////////////////////////////////////////////////
//
// Parameters
//
//////////////////////////////////////////////////////////////////////

/**
 * Identifier of the bundle.
 * !! Has to match what's in Info.plist
 */
#define BUNDLE_ID "zonemobius.BaseAudioUnit"

/**
 * Name of the library in Contents/MacOS
 */
#define DYNLIB_NAME "Contents/MacOS/BaseAudioUnitView"

/**
 * Name of the entry point in the library.
 */
#define ENTRY_POINT "_BaseAudioUnitViewEntry"

//////////////////////////////////////////////////////////////////////
//
// Symbol Resolution
//
//////////////////////////////////////////////////////////////////////

static bool ShimTrace = true;

/**
 * Global variable holding the "start of the loaded image"
 * of the view dynamic library, returned by NSAddImage.
 * We cache this and use it every time LookupSymbol is called.
 */
static const struct mach_header *gViewImage = NULL;

/**
 * Cached entry point from the library.
 */ 
static ComponentRoutineProcPtr gViewEntry = NULL;

/**
 * Return a reference to a symbol in a dynamic library image.
 */
static void	*LookupSymbol(char *symbolName)
{
	void* value = NULL;

	if (ShimTrace)
	  trace("LookupSymbol %s\n", symbolName);

	if (gViewImage == NULL) {
		CFBundleRef bundle = CFBundleGetBundleWithIdentifier(CFSTR(BUNDLE_ID));
		if (bundle == NULL)
		  trace("BaseAudioUnitViewShim::LookupSymbol unable to find bundle!\n");
		else{
			CFURLRef loc = CFBundleCopyBundleURL (bundle);
			if (loc == NULL)
			  trace("BaseAudioUnitViewShim::LookupSymbol unable to copy URL!\n");
			else {
				CFURLRef fullPath = CFURLCreateCopyAppendingPathComponent(NULL, loc, CFSTR(DYNLIB_NAME), false);
				if (fullPath == NULL)
				  trace("BaseAudioUnitViewShim::LookupSymbol unable to create full path!\n");
				else {
					CFStringRef str = CFURLCopyFileSystemPath (fullPath, kCFURLPOSIXPathStyle);
					if (str == NULL)
					  trace("BaseAudioUnitViewShim::LookupSymbol unable to copy path!\n");
					else {
						char* path = (char*)malloc(CFStringGetLength(str) + 32);
						CFStringGetCString(str, path, CFStringGetLength(str) + 32, kCFStringEncodingASCII);
				
						gViewImage = NSAddImage(path, NSADDIMAGE_OPTION_RETURN_ON_ERROR);
						free(path);
						CFRelease (str);
					}
					CFRelease (fullPath);
				}
				CFRelease (loc);
			}
		}
	}

	if (gViewImage != NULL) {
		NSSymbol symbol = NSLookupSymbolInImage(gViewImage, symbolName, 
												NSLOOKUPSYMBOLINIMAGE_OPTION_RETURN_ON_ERROR);
		if (symbol != NULL)
		  value = NSAddressOfSymbol(symbol);
	}
	else
	  trace("LookupSymbol: unable to find image!!\n");

	return value;
}

/**
 * Entry point registered with the resource.
 */
extern "C" ComponentResult BaseAudioUnitViewEntryShim(ComponentParameters *params, Handle componentStorage)
{
	ComponentResult result = unresolvedComponentDLLErr;

	if (ShimTrace)
	  trace("BaseAudioUnitViewEntryShim\n");

	if (gViewEntry == NULL)
	  gViewEntry = (ComponentRoutineProcPtr)LookupSymbol(ENTRY_POINT);

	if (gViewEntry != NULL)
	  result = (*gViewEntry)(params, componentStorage);
	else
	  trace("BaseAudioUnitViewEntryShim: no result!\n");

	return result;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
