/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Common Windows initialization for standalone and plugins.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "MidiInterface.h"
#include "Qwin.h"
#include "Util.h"
#include "UIWindows.h"

#include "Mobius.h"
#include "ObjectPool.h"
#include "UI.h"


#define CurrentWorkingDirectoryMode

/**
 * The registry key for this version.
 */
#ifdef WIN64
#define REGKEY "Software\\Circular Labs\\Mobius 2 (x64)"
#else
#define REGKEY "Software\\Circular Labs\\Mobius 2"
#endif

/**
 * Called from WinMain and VstMain.
 * A WindowsContext has been initialized with the HINSTANCE and
 * the command line, here we figure out where the installation
 * directory is and repair the registry if necessary.
 */
PUBLIC void WinMobiusInit(WindowsContext* wc)
{

#ifdef CurrentWorkingDirectoryMode
  

  // Cas: 30/04/2023
  // Path -> Current MobiusVst2.dll directory [ClaudioCas]  // #008
  // https://stackoverflow.com/questions/6924195/get-dll-path-at-runtime

  char path[MAX_PATH];
  HMODULE hm = NULL;

  if (GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                        (LPCSTR)&WinMobiusInit, &hm) == 0)
  {
    int ret = GetLastError();
    fprintf(stderr, "GetModuleHandle failed, error = %d\n", ret);
    // Return or however you want to handle an error.
  }
  if (GetModuleFileName(hm, path, sizeof(path)) == 0)
  {
    int ret = GetLastError();
    fprintf(stderr, "GetModuleFileName failed, error = %d\n", ret);
    // Return or however you want to handle an error.
  }

  
  int pos = strlen((char *)path);
  while (pos > 0 && path[pos] != '\\') //Remove FileName (works with "exe" and "dll")
  {
    pos--;
  }
  path[pos] = '\0';
  
  SetRegistryCU(REGKEY, "RuntimeDirectory", path); //Debug
  wc->setInstallationDirectory(path);

#else
    // The GetRegistryCU return value is owned by the caller and
    // must be freed.
    // !! But freed how? Deleting this is causing a crash in Ableton, 
    // so make a copy and let the registry string leak, it probably
    // needs free() but unsure if there are runtime library 
    // compatability issues
    char* regstr = GetRegistryCU(REGKEY, "InstDirectory");
    if (regstr != NULL)
      wc->setInstallationDirectory(regstr);
    else {
        // try to repair it, handy for development
        // WindowsContext has code to figure out the directory
        // containing the DLL but that's typically the working
        // directory which is almost never what we want

        const char* dflt = "c:\\Program Files\\Mobius 2";
        if (IsDirectory(dflt)) {

            // try to fix the registry
            printf("Repairing registry installation direcory: %s\n", dflt);
            if (!SetRegistryCU(REGKEY, "InstDirectory", dflt))
              printf("Error repairing registry!\n");
            fflush(stdout);

            wc->setInstallationDirectory(dflt);
        }
    }
    #endif

}

