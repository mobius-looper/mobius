/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * NOTE: I'm trying to move at least some of this over to the util
 * directory as AppContxt so we can make use of some of this without
 * depending on qwin.  A work in progress...
 * 
 * WINDOWS:
 * 
 * We're maintinaing a menu and icon name in here so they can
 * be used in the registration of the window classes.  This is a little
 * odd since from a UI perspective these should be Frame operations, but
 * since the Context is currently registering the classes they have
 * to be passed down here.  This would't be necessary if SetWindowLong
 * worked to change the icon later but it doesn't appear to.
 * An alternative would be to move class registration into Frame and
 * just have every Frame/Window register its own class.  
 *
 */

#include <stdio.h>
#include <string.h>

#include "Util.h"
#include "Trace.h"
#include "Context.h"

/****************************************************************************
 *                                                                          *
 *                                  CONTEXT                                 *
 *                                                                          *
 ****************************************************************************/

PUBLIC Context::Context()
{
    initContext(NULL);
}

PUBLIC Context::Context(const char* commandLine)
{
    initContext(commandLine);
}

PUBLIC Context::~Context()
{
    delete mCommandLine;
	delete mInstallationDirectory;
	delete mConfigurationDirectory;
}

PUBLIC void Context::initContext(const char* commandLine)
{
	mCommandLine = NULL;
	mInstallationDirectory = NULL;
	mConfigurationDirectory = NULL;

	// this seems to be the empty string rather than null if
	// there were no command line args
	if (commandLine != NULL && strlen(commandLine) > 0)
	  mCommandLine = CopyString(commandLine);
}

PUBLIC const char* Context::getCommandLine()
{
	return mCommandLine;
}

PUBLIC void Context::setInstallationDirectory(const char* path)
{
	delete mInstallationDirectory;
	mInstallationDirectory = CopyString(path);
}

PUBLIC void Context::setConfigurationDirectory(const char* path)
{
	delete mConfigurationDirectory;
	mConfigurationDirectory = CopyString(path);
}

/**
 * Normally the same as the installation directory.
 * It is REQUIRED that this return something, either the configuration
 * directory override or the installation directory.
 */
PUBLIC const char* Context::getConfigurationDirectory()
{
	return (mConfigurationDirectory != NULL) ? mConfigurationDirectory : 
		getInstallationDirectory();
}

/****************************************************************************
 *                                                                          *
 *                              WINDOWS CONTEXT                             *
 *                                                                          *
 ****************************************************************************/

#ifdef _WIN32
#include "List.h"
#include <windows.h>
#include <commctrl.h>

PUBLIC Context* Context::getContext(int argc, char* argv[])
{
	return new WindowsContext(NULL, NULL, 0);
}

StringList* WindowsContext::WindowClasses = NULL;

PUBLIC WindowsContext::WindowsContext(HINSTANCE instance, LPSTR commandLine,
                                      int cmdShow) 
{
    initContext(commandLine);

	mInstance = instance;
	mShowMode = cmdShow;
}

PUBLIC WindowsContext::~WindowsContext() 
{
	// Don't unregister the classes yet, we could have more than
	// one context open in a process?  Should try to avoid that!!
}

PUBLIC void WindowsContext::printContext()
{
}

PUBLIC HINSTANCE WindowsContext::getInstance()
{
	return mInstance;
}

/**
 * Attempt to locate the installation directory, on Windows the installer
 * puts this in a registry key.
 */
PUBLIC const char* WindowsContext::getInstallationDirectory()
{
    // It should have been set by now.  If not fall back to the directory
    // containing the DLL.  
	// !! is this ever what we want?

	if (mInstallationDirectory == NULL && mInstance != NULL) {
		char path[1024 * 4];
		GetModuleFileName(mInstance, path, sizeof(path));
		// this will have the module file on the end, strip it
		if (strlen(path)) {
			char directory[1024 * 4];
			ReplacePathFile(path, NULL, directory);
			mInstallationDirectory = CopyString(directory);
		}
	}

	return mInstallationDirectory;
}

PUBLIC int WindowsContext::getShowMode() 
{
	return mShowMode;
}

/**
 * Register a window class in our static registry.
 */
void WindowsContext::registerClass(const char* name)
{
	if (name != NULL) {
		if (WindowClasses == NULL)
		  WindowClasses = new StringList();
		WindowClasses->add(name);
	}
}

/**
 * Unregister classes we may have registered.
 * This is intended only for use from the DllMain
 * procedure when we're notified that a DLL is being unloaded.
 */
void WindowsContext::unregisterClasses(HINSTANCE instance)
{
	if (WindowClasses != NULL) {
		for (int i = 0 ; i < WindowClasses->size() ; i++) {
			const char* name = WindowClasses->getString(i);
			UnregisterClass(name, instance);
		}
		delete WindowClasses;
		WindowClasses = NULL;
	}
}

/**
 * Pens for the brush factory.
 */
PUBLIC int pen_Rgb[] = {
	
	// PEN_BACK, need find out what RGB value LTGREY_BRUSH is
	RGB(128, 128, 128),

	RGB(255, 255, 255),			// PEN_FRONT, white
	RGB(64, 64, 64),			// PEN_SHADOW, dark gray
	RGB(255, 0, 0),				// PEN_HIGHLIGHT, red

	RGB(255, 255, 255),			// PEN_WHITE
	RGB(0, 0, 0),				// PEN_BLACK
	RGB(128, 128, 128),			// PEN_PALEGRAY
	RGB(64, 64, 64),			// PEN_DARKGRAY
	RGB(255, 0, 0),				// PEN_RED
	RGB(0, 255, 0),				// PEN_GREEN
	RGB(0, 0, 255),				// PEN_BLUE
	RGB(255, 255, 0),			// PEN_YELLOW
	RGB(0, 255, 255),			// PEN_CYAN
	RGB(255, 0, 255),			// PEN_PURPLE
};

/**
 * Brush factory.  Don't remember why this is here, maybe
 * we're supposed to delete them on shutdown?
 */
PUBLIC HBRUSH WindowsContext::getBrush(int b)
{
	HBRUSH brush;

	// try to cache these if we can call this many times

	brush = CreateSolidBrush(pen_Rgb[b]);
	// brush = CreateHatchBrush(def->style, pen_Rgb[b]);

	return brush;
}

#endif // _WIN32

//////////////////////////////////////////////////////////////////////
//
// MAC Context
//
//////////////////////////////////////////////////////////////////////

#ifdef OSX
#include <Carbon/Carbon.h>
#include <ApplicationServices/ApplicationServices.h>
#include "MacUtil.h"

/**
 * Global factory method.
 */
PUBLIC Context* Context::getContext(int argc, char* argv[])
{
	return new MacContext(argc, argv);
}

PUBLIC MacContext::MacContext(int argc, char* argv[])
{
	// todo convert argc/argv to a flat command line or give
	// Context the option to represent it that way?

    initContext(NULL);
}

PUBLIC MacContext::~MacContext() 
{
}

PUBLIC void MacContext::printContext()
{
	PrintBundle();

	const char* path = getInstallationDirectory();
	//if (path != NULL)
	//printf("Resources path is: %s\n", path);
}

PUBLIC const char* MacContext::getInstallationDirectory()
{
	if (mInstallationDirectory == NULL) {
		static char path[PATH_MAX];
		strcpy(path, "");

		CFBundleRef mainBundle = CFBundleGetMainBundle();
		CFURLRef url = CFBundleCopyResourcesDirectoryURL(mainBundle);
		if (!CFURLGetFileSystemRepresentation(url, TRUE, (UInt8*)path, PATH_MAX)) {
			Trace(1, "Unable to get bundle Resources path!\n");
		}
		CFRelease(url);

		mInstallationDirectory = CopyString(path);
	}
	return mInstallationDirectory;
}

#endif // OSX

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
