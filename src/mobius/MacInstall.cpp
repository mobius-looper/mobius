/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * OSX Installation utilities
 * 
 * Originally I had this in qwin/Context, but there are some app
 * specific things (the list of configuration files) that we need
 * to refactor into a general interface.
 *
 * The "application bundle" directory normally contains only the
 * executable and the relatively static resources required to run.  This
 * normally goes in the /Applications directory, for example:
 *
 *     /Applications/Mobius.app
 *
 * Anything that is considerred to be non essential like templates, helper apps, 
 * third-party plugins are supposed to go in:
 *
 *     /Library/Application Support/Mobius
 *
 * There can be an /Applications and /Library directory in one of four "domains".
 * In practice the only interesting ones are the "local" domain which
 * is the root of the file system (/Application and /Library), the "user" domain
 * which is relative to the current user's home directory 
 * (~/Applications and ~/Library).  There is also the /Network domain 
 * which makes things accessible to other users on a network, and the /System
 * domain which is only for Apple applications.
 *
 * Because the /Applications, /Library bifurcation requires a more complex
 * installation process than just dragging a folder we support a not uncommon
 * approach where the folder you drag into /Applications contains the bundle
 * directory along with other configuration files and non-essential things
 * like samples and scripts.  A mac install would look like:
 *
 *    /Applications/Mobius       - the installation directory
 *      Mobius.app               - the application bundle directory
 *      Mobius.component		 - the AU plugin, needs to be copied
 *      Mobius.vst		         - the VSt plugin, needs to be copied
 *      README.txt               - documentation
 *      loops/                   - loops
 *      samples/                 - samples
 *      scripts/                 - example scripts
 *
 * The default configuration files and hidden files like the 
 * essage catalogs are stored under each bundle directory 
 * in Contents/Resources.
 * 
 * For Mobius this means we have potentially two directories we have
 * to look in, the "installation" directory and the "configuration" directory.
 * 
 * The installation directory is always the "Contents/Resources" directory within
 * the application bundle.  In the simple case the configuration
 * directory is the same.  But we also support a relocated configuration
 * directory either through some external force (put it in /Library/...)
 * or by searching upwards from the installation directory until we find the
 * configuration files.  [WTF?  No we don't?]
 *
 * We have less control over the plugins (I think).  VST plugins are
 * structured as a bundle with the name "foo.vst" that are placed in
 * /Library/Audio/Plug-Ins/VST.  AU plugins are strucured as a bundle
 * with the name "foo.component" and placed in /Library/Audio/Plug-Ins/Components.
 * I'm not sure we can have the extra level of directory with support
 * files like we do for /Applications/Mobius.  Even if we could I'd like
 * to share configuration between the standalone and plugin versions which
 * argues for keeping all configuration in either /Applications/Mobius
 * (which means you have to install both the application and the plugin)
 * or /Library/Application Support/Mobius (which means you only need to 
 * install one).  
 *
 * What we're going to try is a sneaky viral installation where the
 * /Library/Application Support/Mobius directory is created automatically
 * when the application or the plugin is started.  The default configuration
 * files from the Resources directory will be copied to the support
 * directory and used from then on.   Later when we start using a proper
 * installer, we can just expect it to already exist.
 *
 */

#include <stdio.h>
#include <memory.h>
#include <string.h>

#include "Util.h"
#include "Trace.h"
#include "Context.h"
#include "Qwin.h"

#include "MacInstall.h"

QWIN_USE_NAMESPACE

/**
 * Verify that a configuration file exists in the support directory,
 * copying it over from the resources directory if necessary.
 */
PRIVATE bool checkSupportFile(const char* resourceDirectory,
							  const char* supportDirectory, 
							  const char* file)
{
	bool found = false;
	char path[PATH_MAX];

	sprintf(path, "%s/%s", supportDirectory, file);
	if (IsFile(path))
	  found = true;
	else {
		char srcpath[PATH_MAX];
		sprintf(srcpath, "%s/%s", resourceDirectory, file);
		if (!IsFile(srcpath)) 
		  Trace(1, "Unable to find default config file: %s\n", srcpath);
		else if (!CopyFile(srcpath, path))
		  Trace(1, "Unable to copy config file: %s\n", srcpath);
		else
		  found = true;
	}
	return found;
}

/**
 * Boostrap the /Library/Appliation Support directory 
 * if we don't already have one.  Leave the configuration direcotry
 * path in the Context for later use.
 */
PUBLIC void MacInstall(Context* context)
{
	const char* supportDirectory = NULL;
	char path[PATH_MAX];

	// formerly these were passed in, but we need to share them in 
	// three places	
	const char* appname = "Mobius 2";
	
	const char* resources[] = {
		"mobius.xml",
		"ui.xml", 
		"host.xml", 
		"osc.xml",
		NULL
	};

	sprintf(path, "/Library/Application Support/%s", appname);
	if (IsDirectory(path)) {
		// already there
		supportDirectory = path;
	}
	else {
		if (IsFile(path)) {
			// someone stuck a file here!
			if (!MyDeleteFile(path))
			  Trace(1, "Unable to delete bogus file: %s\n", path);
		}
			
		if (!CreateDirectory(path)) {
		  Trace(1, "Unable to create support directory: %s\n", path);
		}
		else {
		  supportDirectory = path;
		}
	}

	// only use the altenrate directory if we can find or put all the resource
	// files over there

	if (supportDirectory != NULL) {
		const char* installDirectory = context->getInstallationDirectory();
		if (resources != NULL) {
			for (int i = 0 ; resources[i] != NULL ; i++) {
				if (!checkSupportFile(installDirectory, supportDirectory, 
									  resources[i])) {
					supportDirectory = NULL;
					break;
				}
			}
		}
	}

	context->setConfigurationDirectory(supportDirectory);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
