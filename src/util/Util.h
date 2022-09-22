/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 *
 * Yet another collection of utilities.
 * 
 */

#ifndef UTIL_H
#define UTIL_H

#include "Port.h"

/****************************************************************************
 *                                                                          *
 *                                  SCALING                                 *
 *                                                                          *
 ****************************************************************************/

INTERFACE int ScaleValueIn(float value, int min, int max);
INTERFACE float ScaleValueOut(int value, int min, int max);
INTERFACE int Scale128ValueIn(int value, int min, int max);
INTERFACE int ScaleValue(int value, int inmin, int inmax, int outmin, int outmax);

/****************************************************************************
 *                                                                          *
 *   							RANDOM NUMBERS                              *
 *                                                                          *
 ****************************************************************************/

INTERFACE int Random(int low, int high);

// sigh, Random() already used on Mac in Quickdraw.h
INTERFACE float RandomFloat();

/****************************************************************************
 *                                                                          *
 *   							   STRINGS                                  *
 *                                                                          *
 ****************************************************************************/

INTERFACE char *CopyString(const char *src);
INTERFACE char* CopyString(const char* src, int len);
INTERFACE void CopyString(const char* src, char* dest, int max);
INTERFACE void AppendString(const char* src, char* dest, int max);
INTERFACE void FreeString(const char *src);
INTERFACE char* TrimString(char* src);
INTERFACE bool StringEqual(const char* s1, const char* s2);
INTERFACE bool StringEqualNoCase(const char* s1, const char* s2);
INTERFACE bool StringEqualNoCase(const char* s1, const char* s2, int max);
INTERFACE void ToLower(char* src);
INTERFACE void ToUpper(char* src);
INTERFACE bool StartsWith(const char* str, const char* prefix);
INTERFACE bool StartsWithNoCase(const char* str, const char* prefix);
INTERFACE bool EndsWith(const char* str, const char* suffix);
INTERFACE bool EndsWithNoCase(const char* str, const char* suffix);
INTERFACE bool HasExtension(const char* path);
INTERFACE int IndexOf(const char* str, const char* substr);
INTERFACE int IndexOf(const char* str, const char* substr, int start);
INTERFACE int LastIndexOf(const char* str, const char* substr);
INTERFACE bool IsInteger(const char* str);
INTERFACE int ToInt(const char* str);
INTERFACE long ToLong(const char* str);
INTERFACE void FilterString(const char* src, const char* filter, 
                            bool replaceWithSpace,
                            char* dest, int max);

#define MAX_NUMBER_TOKEN 128

INTERFACE int ParseNumberString(const char* src, int* numbers, int max);

/****************************************************************************
 *                                                                          *
 *   								FILES                                   *
 *                                                                          *
 ****************************************************************************/

INTERFACE bool IsAbsolute(const char* path);
INTERFACE bool IsFile(const char *name);
INTERFACE bool IsDirectory(const char *name);
INTERFACE int GetFileSize(const char* path);

// sigh, uSoft has one of these
INTERFACE bool MyDeleteFile(const char *name);
INTERFACE bool DeleteDirectory(const char *path);
INTERFACE bool CreateDirectory(const char *path);
INTERFACE bool CopyFile(const char *src, const char* dest);

INTERFACE char* ReadFile(const char* name);
INTERFACE int ReadFile(const char* name, unsigned char* data, int length);
INTERFACE int WriteFile(const char* name, const char* content);
INTERFACE int WriteFile(const char* name, unsigned char* data, int length);
INTERFACE void GetWorkingDirectory(char* buffer, int max);
INTERFACE bool GetFullPath(const char* relative, char* absolute, int max);
INTERFACE void MergePaths(const char* home, const char* relative, 
						  char* buffer, int max);
INTERFACE void ReplacePathFile(const char* path, const char* file,
							   char* buffer);
INTERFACE void GetDirectoryPath(const char* path, char* buffer);
INTERFACE void GetLeafName(const char* path, char* buffer, bool extension);

INTERFACE class StringList* GetDirectoryFiles(const char* path, const char* ext);

/****************************************************************************
 *                                                                          *
 *   							   REGISTRY                                 *
 *                                                                          *
 ****************************************************************************/

INTERFACE char* GetRegistryLM(const char* key, const char* name);
INTERFACE char* GetRegistryCU(const char* key, const char* name);

INTERFACE bool SetRegistryLM(const char* key, const char* name, const char* value);
INTERFACE bool SetRegistryCU(const char* key, const char* name, const char* value);

/****************************************************************************
 *                                                                          *
 *   							  EXCEPTIONS                                *
 *                                                                          *
 ****************************************************************************/

/**
 * ERR_BASE_*
 *
 * Description: 
 * 
 * Base numbers for ranges of error codes used by MUSE modules.
 * 
 */

#define ERR_BASE			20000

#define ERR_BASE_GENERAL	ERR_BASE 
#define ERR_BASE_XMLP		ERR_BASE + 100

#define ERR_MEMORY 			ERR_BASE_GENERAL + 1
#define ERR_GENERIC			ERR_BASE_GENERAL + 2

/**
 * A convenient exception class containing a message and/or error code.
 */
class AppException {

  public:
	
	INTERFACE AppException(AppException &src);

	INTERFACE AppException(const char *msg, bool nocopy = false);

	INTERFACE AppException(int c, const char *msg = 0, bool nocopy = false);

	INTERFACE ~AppException(void);

	int getCode(void) {
		return mCode;
	}

	const char *getMessage(void) {
		return mMessage;
	}

	char *stealMessage(void) {
		char *s = mMessage;
		mMessage = NULL;
		return s;
	}

	// for debugging convience, senes a message to the 
	// console and debug stream
	INTERFACE void print(void);

  private:

	int 	mCode;
	char 	*mMessage;

};

#endif
