/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Definitions and macros for portability.
 *
 * This file is very old and I don't use much of it any more.
 * I used to be strict about using PUBLIC/PRIVATE/INTERFACE in front
 * of every method implementation but usage is inconsistent now.
 * I still like to use PRIVATE but if it isn't annotated you can
 * assume it is PUBLIC.  
 *
 * INTERFACE is required for functions that are exposed as Windows
 * DLL exports when you don't want to use .exp files but I don't think
 * we have any of that in Mobius.
 *
 * Avoid cluttering this with stuff that might better belong in Util.h.
 * 
 */

#ifndef PORT_UTIL_H
#define PORT_UTIL_H

/**
 * PUBLIC
 * PRIVATE
 * INTERFACE
 *
 * Macros used to add information to declaration of functions and methods.
 * These are used primarily to declare DLL exporting in a portable way, 
 * but are also informative when reading code.
 * 
 * INTERFACE is used whenever a method should be exported from the DLL.
 *
 * PUBLIC is used for methods that are in the public interface, but which
 * aren't to be exported from the DLL.  It doesn't really do anything, but
 * is nice for documentation.
 *
 * PRIVATE is used for private or protected methods.  
 * 
 */

#if !defined(PUBLIC)
#define PUBLIC
#endif

#if !defined(PRIVATE)
#define PRIVATE
#endif

#undef INTERFACE
#if defined(_WIN32)
#define INTERFACE __declspec(dllexport)
#else
#define INTERFACE
#endif

/**
 * UNUSED
 *
 * Macro to quell "unused parameter" warnings from compilers.
 * MSVC doesn't complain about these, but most Unix compilers do.
 *
 */

// Sigh, UNUSED conflicts with afx.h if you're using MFC
#define UNUSED_VARIABLE(x) if ((x) == 0) x = x

/**
 * PATH_MAX
 * 
 * The Posix constant that corresponds to the older MAXPATHLEN constant.
 * This is supposed to be in limits.h, but that doesn't appear to be
 * reliable.  Various comments indicate that gcc doesn't have it.
 * MS VC++ version of 'limits.h' only defines this macro if _POSIX_ is
 * defined, so this makes sure it is defined no matter what
 * 
 * Posix manual indicates that it may not be defined unless
 * _POSIX_SOURCE is on, and that implementations that require the
 * use of pathconf() will not define it at all.
 *
 * This works out to be a lot of crap to remember so here we just ensure
 * that its set always.
 * 
 * NOTE: The usual Windows value for MAX_PATH which they define in 
 * stdlib.y is 260, which is disturbingly small.  The Windows value for
 * PATH_MAX defined in limits.h is 512.  
 */

#include <limits.h>

#if !defined(PATH_MAX)

#if defined(_WIN32)
#define PATH_MAX 512
#else
#if defined(LINUX)
  #include <sys/param.h>
#elif defined(GCC) 
  #include <sys/param.h>
  #define PATH_MAX MAXPATHLEN
#else
  /* should have been in limits.h ! */
  #define PATH_MAX 1024
#endif
#endif

#endif

/**
 * PTRDIFF
 *
 * The usual macro for warning-free pointer differencing.
 */

#define PTRDIFF(start, end) (int)((unsigned int)end - (unsigned int) start)

/****************************************************************************
 *                                                                          *
 *                                   SIZES                                  *
 *                                                                          *
 ****************************************************************************/

/*
 * Avoid using the Windows constants when it matters.
 *
 * !! UPDATE: These are old and unreliable now that we have
 * 64-bit platforms.  Need to stop using these or define them reliably.
 * Only WaveFile should be using these now.
 *
 * New version of Xcode defines these in 
 * /Developer/SDKs/MacOSX10.5.sdk/System/Library/Frameworks/Security.framework/Headers/cssmconfig.h
 *
 * Avoid using the same names...
 */

#define myuint32 unsigned long
#define myuint16 unsigned short
#define myint16 short

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
