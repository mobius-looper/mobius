/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * A file encapsulating the "main" goo necessary to get a VST plugin running.
 * As far as I can remember there wasn't a standard file for this in SDK 2.2, 
 * but 2.4 has public.sdk/source/vst2.x/vstpluginmain.cpp.
 *
 * Rather than trying to use vstpluginmain.cpp, I've duplicated
 * the relevant bits since we're using different versions on
 * windows and mac.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "Trace.h"
#include "Util.h"
#include "Context.h"

//////////////////////////////////////////////////////////////////////
//
// Windows
//
//////////////////////////////////////////////////////////////////////

// !!! This is assuming SDK < 2.4

#ifdef _WIN32

#include <windows.h>
#include "UIwindows.h"
// have been using 2.3
#include "audioeffectx.h"

//#include "Context.h"
//#include "qwin.h"
#include "VstMobius.h"

// in WinInit
extern void WinMobiusInit(WindowsContext* wc);

QWIN_USE_NAMESPACE

static HINSTANCE DllMainInstance = NULL;

BOOL WINAPI DllMain (HINSTANCE hInst, DWORD dwReason, LPVOID lpvReserved)
{
	switch (dwReason) {
		case 0: {
			trace("VstMobius::DllMain DLL_PROCESS_DETACH\n");
			// here is the only safe place to unregister classes
			WindowsContext::unregisterClasses(hInst);
		}
    	break;
		case 1: 
			trace("VstMobius::DllMain DLL_PROCESS_ATTACH\n");
			break;
		case 2: 
			trace("VstMobius::DllMain DLL_THREAD_ATTACH\n");
			break;
		case 3:
			trace("VstMobius::DllMain DLL_THREAD_DETACH\n");
			break;
        default:
			trace("VstMobius::DllMain dwReason %d\n", (int)dwReason);
			break;
	}

	DllMainInstance = hInst;
	return 1;
}

// define with dllexport to avoid having to have a .def file
// VC8 started whining about main returning a pointer, pray
// that an int and a pointer are the same size!

//extern "C" __declspec(dllexport) AEffect *main (audioMasterCallback audioMaster);

extern "C" __declspec(dllexport) int main (audioMasterCallback audioMaster);

//AEffect *main (audioMasterCallback audioMaster)
int main (audioMasterCallback audioMaster)
{
	AEffect* effect = NULL;

	// check VST Version
	if (audioMaster(0, audioMasterVersion, 0, 0, 0, 0)) {

        // Qwin handles the basic context setup
		WindowsContext* wc = new WindowsContext(DllMainInstance, NULL, 0);

        // This adds Mobius specific stuff
        WinMobiusInit(wc);

		VstMobius* mobius = new VstMobius(wc, audioMaster);

		effect = mobius->getAeffect();
	}

	return (int)effect;
}

#endif

//////////////////////////////////////////////////////////////////////
//
// Mac
//
//////////////////////////////////////////////////////////////////////

#ifdef OSX

// requires 2.4
#include "audioeffect.h"

#include "util.h"
#include "MacUtil.h"
#include "MacInstall.h"

#include "VstMobius.h"

//QWIN_USE_NAMESPACE


/**
 * This is the CFBundleIdentifier from Info.plist.
 * They must match.
 */
#define BUNDLE_ID "circularlabs.mobiusvst.2.5"


extern "C" {

#if defined (__GNUC__) && ((__GNUC__ >= 4) || ((__GNUC__ == 3) && (__GNUC_MINOR__ >= 1)))
	#define VST_EXPORT	__attribute__ ((visibility ("default")))
#else
	#define VST_EXPORT
#endif

VST_EXPORT AEffect* VSTPluginMain (audioMasterCallback audioMaster)
{
	AEffect* effect = NULL;

	// check VST version
	if (audioMaster (0, audioMasterVersion, 0, 0, 0, 0)) {

		MacContext* mc = new MacContext(0, NULL);

		// The default getInstallationDirectory in MacContext
		// will use CFBundleGetMainBundle which will be the bundle
		// of the host, not Mobius.vst.  We're allowed to override this,
		// but the control flow feels messy.  I kind of like not having 
		// this kind of stuff buried in qwin, refactor someday when you're bored.

		// this is CFBundleIdentifier from Info.plist
		CFStringRef cfbundleId = MakeCFStringRef(BUNDLE_ID);
		CFBundleRef bundle = CFBundleGetBundleWithIdentifier(cfbundleId);
		if (bundle != NULL) {
			static char path[PATH_MAX];
			strcpy(path, "");
			CFURLRef url = CFBundleCopyResourcesDirectoryURL(bundle);
			if (!CFURLGetFileSystemRepresentation(url, TRUE, (UInt8*)path, PATH_MAX)) {
				Trace(1, "Unable to get bundle Resources path!\n");
			}
			CFRelease(url);
			if (strlen(path) > 0)
			  mc->setInstallationDirectory(CopyString(path));
		}
		else {
			// hmm, really shouldn't happen
			Trace(1, "Unable to locate bundle %s!\n", BUNDLE_ID);
		}

		// Setup Application Support
		MacInstall(mc);

		VstMobius* mobius = new VstMobius(mc, audioMaster);

		effect = mobius->getAeffect();
	}

	return effect;
}

// support for old hosts not looking for VSTPluginMain
VST_EXPORT AEffect* main_macho (audioMasterCallback audioMaster) { return VSTPluginMain (audioMaster); }

} // extern "C"

#endif


/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
