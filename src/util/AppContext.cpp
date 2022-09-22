/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
//
// NOTE: This is not currently used, I'm trying to move the completely
// generic app environmment stuff from qwin down to util
//
// WINDOWS:
//
// We're maintinaing a menu and icon name in here so they can
// be used in the registration of the window classes.  This is a little
// odd since from a UI perspective these should be Frame operations, but
// since the Context is currently registering the classes they have
// to be passed down here.  This would't be necessary if SetWindowLong
// worked to change the icon later but it doesn't appear to.
// An alternative would be to move class registration into Frame and
// just have every Frame/Window register its own class.  
*/

#include <stdio.h>
#include <memory.h>
#include <string.h>

#include "util.h"
#include "Trace.h"
#include "qwin.h"

QWIN_BEGIN_NAMESPACE

/****************************************************************************
 *                                                                          *
 *   						   GLOBAL UTILITIES                             *
 *                                                                          *
 ****************************************************************************/
/*
 * Not really related to Context, but no better home.
 */

/**
 * Perform pre-exit cleanup and optional analysis.
 */
PUBLIC void Qwin::exit(bool dump)
{
	Font::exit(dump);
}

// 
// Global functions for OSX so we don't have to use namespaces, e.g.
// Qwin::Qwin::exit
//

PUBLIC void QwinExit(bool dump)
{
	Qwin::exit(dump);
}

PUBLIC Context* QwinGetContext(int argc, char* argv[])
{
	return Qwin::getContext(argc, argv);
}

/****************************************************************************
 *                                                                          *
 *                                  CONTEXT                                 *
 *                                                                          *
' ****************************************************************************/

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

QWIN_END_NAMESPACE

/****************************************************************************
 *                                                                          *
 *                              WINDOWS CONTEXT                             *
 *                                                                          *
 ****************************************************************************/
#ifdef _WIN32
#include "UIWindows.h"
#include <windows.h>
#include <commctrl.h>

QWIN_BEGIN_NAMESPACE

PUBLIC Context* Qwin::getContext(int argc, char* argv[]) 
{
	// can't create contexts this way on Windows, 
	// need a method that hids the WinMain args?
	return NULL; 
}

PUBLIC void WindowsContext::printContext()
{
}

/**
 * Global tracking the registration of window classes.  
 * This is a global rather than a Context member because an application
 * could in theory create more than one context?
 */
PRIVATE bool CLASSES_REGISTERED = false;

PUBLIC WindowsContext::WindowsContext(HINSTANCE instance, LPSTR commandLine,
                                      int cmdShow) 
{
    initContext(commandLine);

	mInstance = instance;
	mShowMode = cmdShow;
	mIcon = NULL;
}

PUBLIC WindowsContext::~WindowsContext() 
{
	// what about the handles?
	delete mIcon;

	// Don't unregister the classes yet, we could have more than
	// one context open in a process?  Should try to avoid that!!
}

PUBLIC HINSTANCE WindowsContext::getInstance()
{
	return mInstance;
}

PUBLIC void WindowsContext::setIcon(const char *name)
{
	delete mIcon;
	mIcon = CopyString(name);
}

/**
 * Attempt to locate the installation directory, on Windows the installer
 * puts this in a registry key.
 */
PUBLIC const char* WindowsContext::getInstallationDirectory()
{
	if (mInstallationDirectory == NULL) {
		// the return value is owned by the caller and must be freed
		// !! but freed how, delete or free?
		mInstallationDirectory = GetRegistryCU("Software\\Mobius", "InstDirectory");
	}

	// fall back to the directory containing the DLL
	// !! is this necessarily what we want?

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
 * Default window message handler.
 * Can we actually get here?
 */
PRIVATE LRESULT defaultHandler(HWND win, UINT msg, 
							   WPARAM wparam, LPARAM lparam)
{
	LRESULT res = 0;
	bool handled = false;

	switch (msg) {

		case WM_PAINT: {
			// this is exactly what the default handler does
			// wouldn't need to do this?
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(win, &ps);
			EndPaint(win, &ps);
			handled = true;
		}
		break;

		case WM_DESTROY: {
			// this will insert a WM_QUIT message in the queue
			PostQuitMessage(0);
			handled = true;
		}
	}

	if (!handled)
	  res = DefWindowProc(win, msg, wparam, lparam);

	return res;
}

/**
 * The global "Window Procedure" registered with our window classes.
 * This will be called by the system to process messages.
 *
 * Docs have this as CALLBACK rather than FAR PASCAL.
 */
PUBLIC LRESULT CALLBACK WindowProcedure(HWND window, UINT msg, 
										 WPARAM wparam, LPARAM lparam)
{
	LRESULT res = 0;

	// retrieve our extension
	// supposed to use GetWindowLongPtr, but can't seem to find it
	WindowsWindow* ui = (WindowsWindow *)GetWindowLong(window, GWL_USERDATA);

	if (ui == NULL) {
		// can see this during initialization before we set our extension
	    res = defaultHandler(window, msg, wparam, lparam);
	}
	else {
		HWND current = (HWND)ui->getHandle();
		if (window != current) {
			if (current != NULL) 
			  Trace(1, "WindowProcedure: Window handle changed!!\n");
			else {
				Trace(1, "WindowProcedure: NULL handle for message %d\n", msg);
			}
		}

		res = ui->messageHandler(msg, wparam, lparam);
	}

	return res;
}

/**
 * The global "Window Procedure" registered with our dialog classes.
 * This is the same as WindowProcedure but calls DefDlgProc instead
 * of DefWindowProc if the default handler has to be used.
 */
PUBLIC LRESULT CALLBACK DialogProcedure(HWND window, UINT msg, 
                                        WPARAM wparam, LPARAM lparam)
{
	LRESULT res = 0;

	// retrieve our extension
	// supposed to use GetWindowLongPtr, but can't seem to find it
	WindowsWindow* ui = (WindowsWindow *)GetWindowLong(window, GWL_USERDATA);

	if (ui == NULL) {
        LRESULT res = 0;
        bool handled = false;

        switch (msg) {

            case WM_PAINT: {
                // this is exactly what the default handler does
                // wouldn't need to do this?
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(window, &ps);
                EndPaint(window, &ps);
                handled = true;
            }
            break;

            case WM_DESTROY: {
                // this will insert a WM_QUIT message in the queue
                PostQuitMessage(0);
                handled = true;
            }
        }

        if (!handled) {
            res = DefDlgProc(window, msg, wparam, lparam);
            //res = DefWindowProc(window, msg, wparam, lparam);
        }
    }
	else {
		HWND current = (HWND)ui->getHandle();
		if (window != current && current != NULL)
		  Trace(1, "DialogProcedure: Window handle changed!!\n");

		res = ui->messageHandler(msg, wparam, lparam);
	}

	return res;
}

void traceLastError()
{
	DWORD e = GetLastError();
	char msg[1024];
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, e, 0, 
				  msg, sizeof(msg) - 4, NULL);
	Trace(1, "Last error: %s (%ld)\n", msg, e);
}

/**
 * Register Windows classes we will be using.
 * Should only be called once.
 */
PUBLIC void WindowsContext::registerClasses()
{
	if (!CLASSES_REGISTERED) {

        // call this in case we want to use "newer" common controls
        INITCOMMONCONTROLSEX init;
        init.dwSize = sizeof(init);
        init.dwICC = ICC_WIN95_CLASSES;
        InitCommonControlsEx(&init);

		WNDCLASSEX wc;

		wc.cbSize = sizeof(wc);
		
		// DBLCLCKS enables sending double click messages.
		// DROPSHADOW can be used in XP, typically for menus.
		// GLOBALCLASS says this can be used by any code in the process,
		// need this if we package this in a DLL!
		// HREDRAW redraws the entire window if movement or size adjustment
		// changes the width.  Necessary if we render our own widgets?
		// VREDRAW like HREDRAW only vertical.
		// NOCLOSE disalbes close on the window menu.
		// OWNDC allocates a private device context for the window
		// rather than using one from the global pool.  Takes memory
		// but supposedly more effecient.

		//wc.style         = CS_OWNDC | CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
		wc.style         = CS_GLOBALCLASS | CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;

		// pointer to the Window Procedure, this will be called 
		// by the system to process window events
		wc.lpfnWndProc   = WindowProcedure;
		
		// number of extra bytes to allocate after the window class structure
		// will be initialized to zero
		wc.cbClsExtra    = 0;

		// number of extra bytes to allocate after the window instance
		// this is where we hide our Window pointer
		// formerly added sizeof(void*) here, but we don't really need that
		// since GWLP_USERDATA is builtin
		// wc.cbWndExtra    = sizeof(Window*);
		wc.cbWndExtra = 0;

		// handle to the instance that contains the window procedure 
		// for the class
		wc.hInstance     = mInstance;

		// Class icon (regular and small).
		// This must be a handle to an icon resource.
		// If null, the system supplies a default icon.
		// In the call to LoadIcon, the second arg can be the name
		// of an ICON or CURSOR resource.  If you use numbers wrap
		// the number in a MAKEINTRESOURCE() macro, e.g.
		// MAKEINTRESOURCE(125).  You can also use the string "#125" and
		// Windows will convert it.

		// wc.hIcon = LoadIcon(inst, MAKEINTRESOURCE(IDICO_ICON1));

		if (mIcon == NULL) {
			// this is the default icon, looks like a white box
			wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
			wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
		}
		else {
			// you can load an alternative icon but it applies to all windows
			// supposedly you can call SetClassLong later 
			// but I couldn't get that to work
			HICON icon = LoadIcon(mInstance, mIcon);
			if (icon) {
				wc.hIcon = icon;
				wc.hIconSm = icon;
			}
			else {
				Trace(1, "Couldn't load icon!\n");
				traceLastError();
			}
		}

		// Handle to a cursor, if this is NULL, the application must
		// explicitly set the cursor shape whenever the mouse moves
		// into the application window.
		//wc.hCursor = LoadCursor(inst, MAKEINTRESOURCE(IDPTR_NOTE));
		wc.hCursor = LoadCursor(NULL, IDC_ARROW);

		// Handle to the class background brush.
		// May be a physical brush handle or a color value.  
		// Docs say:  A color value must be one of the standard set of
		// system colors and that "the value 1 must be added to the chosen
		// color" whatever that means.  Also if a color value is given, 	
		// you must convert it to an HBRUSH type, which appear
		// to be the COLOR_ constants.
		// If this is NULL, the application must paint its own background.
		//wc.hbrBackground = COLOR_BACKGROUND;  -- doesn't work

		// VSTGUI does this
		//wc.hbrBackground = GetSysColorBrush(COLOR_BTNFACE); 

		// Sigh, you can set the background color, but button controls
		// will always use system colors designed for dialog boxes.
		// So in practice you have to set the background to be the
		// standard dialog background color to make the buttons look right.
		// See Petzold for more, buttons suck.  If you want over
		// button color you pretty much have to use OWNERDRAW and roll
		// your own.
		//wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
		wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
		
		// if NULL there will be no default menu, set this later
		// in the CreateWindow call
		wc.lpszMenuName = NULL;
			
		// string or "atom" created by previous call to RegisterClass
		// Docs suggest that this name must have been previously
		// registered, but since we're trying to register it I don'
		// see how that happens.  
		// Old comments say this is normally the name of the application,
		// if so could dig that out of the context.
		wc.lpszClassName = FRAME_WINDOW_CLASS;

		// Once registered, you can pass the ATOM to CreateWindow but
		// you can also just use the name
		ATOM atom = RegisterClassEx(&wc);
		if (atom == 0) {
			Trace(1, "Failed to register frame window class!\n");
			traceLastError();
		}

		// Another class for dialogs and popup windows
		// this turns out to be identical to the frame class, but
		// duplicate it so it can be easily adjusted later
		// leave hIcon, hIconSm, and hCursor the same

		wc.style         = CS_GLOBALCLASS | CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
        // supposed to use DefDlgProc, but this seems to cause problems
		//wc.lpfnWndProc   = DialogProcedure;
		wc.lpfnWndProc   = WindowProcedure;
		wc.cbClsExtra    = 0;
		wc.cbWndExtra	 = 0;
		wc.hbrBackground = GetSysColorBrush (COLOR_BTNFACE);
		wc.lpszMenuName  = NULL;
		wc.lpszClassName = DIALOG_WINDOW_CLASS;

		atom = RegisterClassEx(&wc);
		if (atom == 0) {
			Trace(1, "Failed to register dialog window class!\n");
			traceLastError();
		}
			
		// Class for self-closing borderless alert dialogs

		wc.style         = CS_GLOBALCLASS | CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
        // supposed to use DefDlgProc, but this seems to cause problems
		//wc.lpfnWndProc   = DialogProcedure;
		wc.lpfnWndProc   = WindowProcedure;
		wc.cbClsExtra    = 0;
		wc.cbWndExtra	 = 0;
		wc.hbrBackground = 0;  //GetSysColorBrush (COLOR_BTNFACE);
		wc.lpszMenuName  = NULL;
		wc.lpszClassName = ALERT_WINDOW_CLASS;

		atom = RegisterClassEx(&wc);
		if (atom == 0) {
			Trace(1, "Failed to register alert window class!\n");
			traceLastError();
		}
			
		// Class for VST editor windows
		// these are child windows within a window created by the host

		// VSTGUI only sets GLOBALCLASS
		wc.style         = CS_GLOBALCLASS | CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
		wc.lpfnWndProc   = WindowProcedure;
		wc.cbClsExtra    = 0;
		wc.cbWndExtra	 = 0;
		wc.hIcon		 = NULL;
		wc.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
		wc.lpszMenuName  = NULL;
		wc.lpszClassName = CHILD_WINDOW_CLASS;

		atom = RegisterClassEx(&wc);
		if (atom == 0) {
			Trace(1, "Failed to register child window class!\n");
			traceLastError();
		}

		CLASSES_REGISTERED = true;
	}
}

/**
 * This is a static called when the DLL is unloaded.
 * This violates encapsulation of the Context but it is difficult
 * to unwind this carefully if we allow more than one VstMobius plugin
 * to create more than one WindowsContext concurrently.
 */
PUBLIC void WindowsContext::unregisterClasses(HINSTANCE inst)
{
	if (CLASSES_REGISTERED) {

		Trace(2, "Unregistering window classes\n");

		UnregisterClass(FRAME_WINDOW_CLASS, inst);
		UnregisterClass(DIALOG_WINDOW_CLASS, inst);
		UnregisterClass(ALERT_WINDOW_CLASS, inst);
		UnregisterClass(CHILD_WINDOW_CLASS, inst);

		CLASSES_REGISTERED = false;
	}
}

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

PUBLIC HBRUSH WindowsContext::getBrush(int b)
{
	HBRUSH brush;

	// try to cache these if we can call this many times

	brush = CreateSolidBrush(pen_Rgb[b]);
	// brush = CreateHatchBrush(def->style, pen_Rgb[b]);

	return brush;
}

QWIN_END_NAMESPACE
#endif // _WIN32

//////////////////////////////////////////////////////////////////////
//
// MAC Context
//
//////////////////////////////////////////////////////////////////////

#ifdef OSX
#include <Carbon/Carbon.h>
#include "UIMac.h"
#include "MacUtil.h"

QWIN_BEGIN_NAMESPACE

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

/**
 * Provide an opaque object factory so the application doesn't have
 * to drag in Carbon files and deal with namespaces.
 */
PUBLIC Context* Qwin::getContext(int argc, char* argv[]) 
{
	return new MacContext(argc, argv);
}

QWIN_END_NAMESPACE
#endif // OSX

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
