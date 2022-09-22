/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Formerly part of Qwin.h but factored out so we can use it
 * with AUMobius without getting conflicts between things in qwin
 * and things in Carbon and CoreAudio.
 *
 * NOTE: I'm trying to move at least some of this over to the util
 * directory as AppContxt so we can make use of some of this without
 * depending on qwin.  A work in progress...
 * 
 */

#ifndef APP_CONTEXT_H
#define APP_CONTEXT_H

/****************************************************************************
 *                                                                          *
 *   							   CONTEXT                                  *
 *                                                                          *
 ****************************************************************************/

/**
 * Base application context, subclassed by each platform.
 */
class Context {

 public:

	// static factory method that must be conditionally
	// compiled to create the appropriate subclass
	static Context* getContext(int argc, char* argv[]);

	Context();
	Context(const char *commandLine);
    void initContext(const char* commandLine);
	virtual ~Context();

	/**
	 * Get the command line if launched from a console.
	 */
	const char *getCommandLine();

	/**
	 * Get the installation directory.
	 * For Windows this will be taken from the registry.
	 * For OSX this will be the "Resources" directory within the
	 * bundle directory of either the application or the plugin.
	 */
	virtual const char* getInstallationDirectory() = 0;
	void setInstallationDirectory(const char* path);

	/**
	 * Set an alternate configuration directory.  This is done
	 * after the context is created.  Only used on OSX to 
	 * point to the /Library/Application Support directory.
	 */
	void setConfigurationDirectory(const char* path);
	const char* getConfigurationDirectory();

	/**
	 * Print diagnostics about the OS environment.
	 */
	virtual void printContext() = 0;

  protected:

	char *mCommandLine;
	char* mInstallationDirectory;
	char* mConfigurationDirectory;

};

//////////////////////////////////////////////////////////////////////
//
// WindowsContext
//
//////////////////////////////////////////////////////////////////////

#ifdef _WIN32
#include <windows.h>
//#include <commctrl.h>

/**
 * WindowsContext
 */
class WindowsContext : public Context {
    
  public:

    WindowsContext(HINSTANCE instance, LPSTR commandLine, int cmdShow);
    ~WindowsContext();

	const char* getInstallationDirectory();

	void printContext();

	HINSTANCE getInstance();
	int getShowMode();

	//
	// Brush factory, why is this here?
	//

	HBRUSH getBrush(int b);

	//
	// Class registration
	// 

	/**
	 * Used to track registration of window classes that need
	 * to be unregistered when a DLL unloads.
	 */
	static void registerClass(const char* name);
	
	/**
	 * Unregister classes.  For applications this is normally
	 * not called, but it is necessary for VST DLL's that are
	 * being unloaded.
	 */
	static void unregisterClasses(HINSTANCE instance);

  private:

	// letting this be static in case need to make more than one context
	// is this really necessary??
	static StringList* WindowClasses;

	HINSTANCE mInstance;
	int mShowMode;

};

#endif

//////////////////////////////////////////////////////////////////////
//
// MacContext
//
//////////////////////////////////////////////////////////////////////

#ifdef OSX

/**
 * MacContext
 */
class MacContext : public Context {
    
  public:

    MacContext(int argc, char* argv[]);
    ~MacContext();

	const char* getInstallationDirectory();
	void printContext();

  private:

};
#endif

/****************************************************************************/
#endif
