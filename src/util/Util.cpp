/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 *
 * Random utility functions.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifndef _WIN32
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#else
#include <direct.h>
#include <windows.h>
#endif

#include "util.h"
#include "vbuf.h"
#include "Trace.h"
#include "List.h"

/****************************************************************************
 *                                                                          *
 *                                  SCALING                                 *
 *                                                                          *
 ****************************************************************************/

/**
 * Convert a floating point nubmer from 0.0 to 1.0 into an integer
 * within the specified range.
 *
 * This can be used to scale both OSC arguments and VST host parameter
 * values into scaled integers for Mobius parameters and controls.
 *
 * VstMobius notes
 * 
 * On the way in values are quantized to the beginning of their chunk
 * like ScaleValueOut, but when the value reaches 1.0 we'll be
 * at the beginning of the chunk beyond the last one so we have
 * to limit it.
 *
 * HACK: For parameters that have chunk sizes that are repeating
 * fractions we have to be careful about rounding down.  Example
 * trackCount=6 so selectedTrack has a chunk size of .16666667
 * Track 3 (base 0) scales out to .5 since the begging of the chunk
 * is exactly in the middle of the range.  When we try to apply
 * that value here, .5 / .16666667 results in 2.99999 which rounds
 * down to 2 instead of 3.
 *
 * There are proably several ways to handle this, could add a little
 * extra to ScaleParameterOut to be sure we'll cross the boundary
 * when scaling back.  Here we'll check to see if the beginning of
 * the chunk after the one we calculate here is equal to the
 * starting value and if so bump to the next chunk.
 * 
 */
int ScaleValueIn(float value, int min, int max)
{
    int ivalue = 0;
    int range = max - min + 1;

    if (range > 0) {
        float chunk = (1.0f / (float)range);
        ivalue = (int)(value / chunk);
        
        // check round down
        float next = (ivalue + 1) * chunk;
        if (next <= value)
          ivalue++;

        // add in min and constraint range
        ivalue += min;
        if (ivalue > max) 
          ivalue = max; // must be at 1.0
    }

    return ivalue;
}

/**
 * Scale a value within a range to a float from 0.0 to 1.0.
 * VstMobius notes
 * 
 * On the way out, the float values will be quantized
 * to the beginning of their "chunk".  This makes zero
 * align with the left edge, but makes the max value slightly
 * less than the right edge.
 */
float ScaleValueOut(int value, int min, int max)
{
    float fvalue = 0.0;

    int range = max - min + 1;
    float chunk = 1.0f / (float)range;

    int base = value - min;
    fvalue = chunk * base;

    return fvalue;
}

/**
 * Scale an integer from 0 to 127 into a smaller numeric range.
 */
int Scale128ValueIn(int value, int min, int max)
{
    int scaled = 0;
    
    if (value < 0 || value > 127) {
        Trace(1, "Invalid value at Scale128ValueIn %ld\n", (long)value);
    }
    else if (min == 0 && max == 127) {
        // don't round it
        scaled = value;
    }
    else {
        int range = max - min + 1;
        if (range > 0) {
            float chunk = 128.0f / (float)range;
            scaled = (int)((float)value / chunk);

            // check round down
            float next = (scaled + 1) * chunk;
            if (next <= value)
              scaled++;

            // add in min and constraint range
            scaled += min;
            if (scaled > max) 
              scaled = max;
        }
    }

    return scaled;
}

/**
 * Scale a value from one range to another.
 */
int ScaleValue(int value, int inmin, int inmax, int outmin, int outmax)
{
    int scaled = 0;
    
    if (value < inmin || value > inmax) {
        Trace(1, "ScaleValue out of range %ld\n", (long)value);
    }
    else if (inmin == outmin && inmax == outmax) {
        // don't round it
        scaled = value;
    }
    else {
        int inrange = inmax - inmin;
        int outrange = outmax - outmin;
        
        if (inrange == 0 || outrange == 0) {
            // shouldn't see this on outrange but
            // some Mobius states can be empty
            // avoid div by zero
        }
        else {
            float fraction = (float)value / (float)inrange;
            scaled = outmin + (int)(fraction * (float)outrange);
        }
    }

    return scaled;
}

/****************************************************************************
 *                                                                          *
 *   							RANDOM NUMBERS                              *
 *                                                                          *
 ****************************************************************************/

static bool RandomSeeded = false;

/**
 * Generate a random number between the two values, inclusive.
 */
INTERFACE int Random(int min, int max)
{
	// !! potential csect issues here
	if (!RandomSeeded) {
		// passing 1 "reinitializes the generator", passing any other number
		// "sets the generator to a random starting point"
		// Unclear how the seed affects the starting point, probably should
		// be based on something, maybe pass in the layer size?
		srand(2);
		RandomSeeded = true;
	}

	int range = max - min + 1;
	int value = (rand() % range) + min;

	return value;
}

INTERFACE float RandomFloat()
{
	if (!RandomSeeded) {
		srand(2);
		RandomSeeded = true;
	}

	return (float)rand() / (float)RAND_MAX;
}

/****************************************************************************
 *                                                                          *
 *   							   STRINGS                                  *
 *                                                                          *
 ****************************************************************************/

/**
 * CopyString
 *
 * Arguments:
 *	src: string to copy
 *
 * Returns: copied string (allocated with new)
 *
 * Description: 
 * 
 * The one function that should be in everyone's damn C++ run time library.
 * Copies a null terminated string.  Uses the "new" operator to allocate
 * storage.  Returns NULL if a NULL pointer is passed.
 * 
 * strdup() doesn't handle NULL input pointers, and the result is supposed
 * to be freed with free() not the delete operator.  Many compilers don't
 * make a distinction between free() and delete, but that is not guarenteed.
 */

INTERFACE char* CopyString(const char *src)
{
	char *copy = NULL;
	if (src) {
		copy = new char[strlen(src) + 1];
		if (copy)
		  strcpy(copy, src);
	}
	return copy;
}

/**
 * Copy one string to a buffer with care.
 * The max argument is assumed to be the maximum number
 * of char elements in the dest array *including* the nul terminator.
 *
 * TODO: Some of the applications of this would like us to favor
 * the end rather than the start.
 */
INTERFACE void CopyString(const char* src, char* dest, int max)
{
	if (dest != NULL && max > 0) {
		if (src == NULL) 
		  strcpy(dest, "");
		else {
			int len = strlen(src);
			int avail = max - 1;
			if (avail > len)
			  strcpy(dest, src);
			else {
				strncpy(dest, src, avail);
				dest[avail] = 0;
			}
		}
	}
}

INTERFACE void AppendString(const char* src, char* dest, int max)
{
    if (src != NULL) {
        int current = strlen(dest);
        int neu = strlen(src);
        int avail = max - 1;
        if (avail > current + neu)
          strcat(dest, src);
    }
}

/**
 * Copy a string to a point.
 */
INTERFACE char* CopyString(const char* src, int len)
{
	char* copy = NULL;
	if (src != NULL && len > 0) {
		int srclen = strlen(src);
		if (len <= srclen) {
			copy = new char[len + 1];
			if (copy != NULL) {
				strncpy(copy, src, len);
				copy[len] = 0;
			}
		}
	}
	return copy;
}

INTERFACE void FilterString(const char* src, const char* filter, 
                            bool replaceWithSpace,
                            char* dest, int max)
{
    if (dest != NULL && max > 0) {
        if (src == NULL || filter == NULL) {
            CopyString(src, dest, max);
        }
        else {
            int srclen = strlen(src);
            int filterlen = strlen(filter);
            int destlast = max - 1;
            int destpsn = 0;
            char lastchar = 0;

            for (int i = 0 ; i < srclen && destpsn < destlast ; i++) {
                char ch = src[i];
                bool contains = false;
                for (int j = 0 ; j < filterlen ; j++) {
                    if (filter[j] == ch) {
                        contains = true;
                        break;
                    }
                }

                if (!contains) {
                    dest[destpsn++] = ch;
                    lastchar = ch;
                }
                else if (replaceWithSpace) {
                    if (lastchar != ' ') {
                        dest[destpsn++] = ' ';
                        lastchar = ' ';
                    }
                }
            }
            dest[destpsn] = 0;
        }
    }
}

/**
 * FreeString
 *
 * Free a string allocated with copyString or otherwise returned
 * by the library.  This is necessary only if the application wants
 * to link with a different runtime library than the library, and only
 * if the library is packaged as a DLL.
 */
INTERFACE void FreeString(const char *src)
{
	delete (char *)src;
}


/**
 * String comparison handling nulls.
 */
INTERFACE bool StringEqual(const char* s1, const char* s2)
{	
	bool equal = false;
	
	if (s1 == NULL) {
		if (s2 == NULL)
		  equal = true;
	}
	else if (s2 != NULL)
	  equal = !strcmp(s1, s2);

	return equal;
}

/**
 * Case insensitive string comparison.
 * Return true if the strings are equal.
 */
INTERFACE bool StringEqualNoCase(const char* s1, const char* s2)
{	
	bool equal = false;
	
	if (s1 == NULL) {
		if (s2 == NULL)
		  equal = true;
	}
	else if (s2 != NULL) {
		int len = strlen(s1);
		int len2 = strlen(s2);
		if (len == len2) {
			equal = true;
			for (int i = 0 ; i < len ; i++) {
				char ch = tolower(s1[i]);
				char ch2 = tolower(s2[i]);
				if (ch != ch2) {
					equal = false;
					break;
				}
			}
		}

	}
	return equal;
}

INTERFACE bool StringEqualNoCase(const char* s1, const char* s2, int max)
{	
	bool equal = false;
	
	if (s1 == NULL) {
		if (s2 == NULL)
		  equal = true;
	}
	else if (s2 != NULL) {
		int len = strlen(s1);
		int len2 = strlen(s2);
        if (len >= max && len2 >= max) {
			equal = true;
			for (int i = 0 ; i < max ; i++) {
				char ch = tolower(s1[i]);
				char ch2 = tolower(s2[i]);
				if (ch != ch2) {
					equal = false;
					break;
				}
			}
		}

	}
	return equal;
}

INTERFACE void ToLower(char* src)
{
	if (src != NULL) {
		int len = strlen(src);
		for (int i = 0 ; i < len ; i++) {
			if (isupper(src[i]))
			  src[i] = tolower(src[i]);
		}
	}
}

INTERFACE void ToUpper(char* src)
{
	if (src != NULL) {
		int len = strlen(src);
		for (int i = 0 ; i < len ; i++) {
			if (islower(src[i]))
			  src[i] = toupper(src[i]);
		}
	}
}

INTERFACE bool StartsWith(const char* str, const char* prefix)
{
	bool startsWith = false;
	if (str != NULL && prefix != NULL)
      startsWith = !strncmp(str, prefix, strlen(prefix));
    return startsWith;
}

INTERFACE bool StartsWithNoCase(const char* str, const char* prefix)
{
	bool startsWith = false;
	if (str != NULL && prefix != NULL)
        startsWith = StringEqualNoCase(str, prefix, strlen(prefix));
	return startsWith;
}

INTERFACE bool EndsWith(const char* str, const char* suffix)
{
	bool endsWith = false;
	if (str != NULL && suffix != NULL) {
		int len1 = strlen(str);
		int len2 = strlen(suffix);
		if (len1 > len2)
		  endsWith = !strcmp(suffix, &str[len1 - len2]);
	}
	return endsWith;
}

INTERFACE bool EndsWithNoCase(const char* str, const char* suffix)
{
	bool endsWith = false;
	if (str != NULL && suffix != NULL) {
		int len1 = strlen(str);
		int len2 = strlen(suffix);
		if (len1 > len2)
		  endsWith = StringEqualNoCase(suffix, &str[len1 - len2]);
	}
	return endsWith;
}

INTERFACE int IndexOf(const char* str, const char* substr)
{
    return IndexOf(str, substr, 0);
}

INTERFACE int IndexOf(const char* str, const char* substr, int start)
{
	int index = -1;

	// not a very smart search
	if (str != NULL && substr != NULL) {
		int len = strlen(str);
		int sublen = strlen(substr);
        int max = len - sublen;
        if (sublen > 0 && max >= 0) {
            for (int i = 0 ; i <= max ; i++) {
                if (strncmp(&str[i], substr, sublen) == 0) {
                    index = i;
                    break;
                }
            }
        }
	}
	return index;
}

INTERFACE int LastIndexOf(const char* str, const char* substr)
{
	int index = -1;

	// not a very smart search
	if (str != NULL && substr != NULL) {
		int len = strlen(str);
		int sublen = strlen(substr);
		int psn = len - sublen;
		if (psn >= 0) {
			while (psn >= 0 && strncmp(&str[psn], substr, sublen))
			  psn--;
			index = psn;
		}
	}
	return index;
}

/**
 * Return true if the string looks like a signed integer.
 */
INTERFACE bool IsInteger(const char* str)
{
    bool is = false;
    if (str != NULL) {
        int max = strlen(str);
        if (max > 0) {
            is = true;
            for (int i = 0 ; i < max && is ; i++) {
                char ch = str[i];
                if (!isdigit(ch) && (i > 0 || ch != '-'))
                  is = false;
            }
        }
    }
    return is;
}

/**
 * Necessary because atoi() doesn't accept NULL arguments.
 */
INTERFACE int ToInt(const char* str)
{
	int value = 0;
	if (str != NULL)
	  value = atoi(str);
	return value;
}

/**
 * Necessary because atol() doesn't accept NULL arguments.
 */
INTERFACE long ToLong(const char* str)
{
	long value = 0;
	if (str != NULL)
	  value = atol(str);
	return value;
}

/**
 * Given a string of numbers, either whitespace or comma delimited, 
 * parse it and build an array of ints.  Return the number of ints
 * parsed.
 */
int ParseNumberString(const char* src, int* numbers, int max)
{
	char buffer[MAX_NUMBER_TOKEN + 1];
	const char* ptr = src;
	int parsed = 0;

	if (src != NULL) {
		while (*ptr != 0 && parsed < max) {
			// skip whitespace
			while (*ptr != 0 && isspace(*ptr) && !(*ptr == ',')) ptr++;

			// isolate the number
			char* psn = buffer;
			for (int i = 0 ; *ptr != 0 && i < MAX_NUMBER_TOKEN ; i++) {
				if (isspace(*ptr) || *ptr == ',') {
					ptr++;
					break;
				}
				else
				  *psn++ = *ptr++;
			}
			*psn = 0;

			if (strlen(buffer) > 0) {
				int ival = atoi(buffer);
				if (numbers != NULL)
				  numbers[parsed] = ival;
				parsed++;
			}
		}
	}

	return parsed;
}

/**
 * Return true if this looks like a file path with an extension.
 */
INTERFACE bool HasExtension(const char* path)
{
	bool extension = false;
	if (path != NULL) {
		// any dot will do
		extension = (LastIndexOf(path, ".") > 0);
	}
	return extension;
}

/**
 * Trim leading and trailing whitespace from a string buffer.
 * Note that this MUST NOT be a static string, we modify the 
 * buffer when trimming on the right.
 */
INTERFACE char* TrimString(char* src)
{
	char* start = src;

	if (start != NULL) {
		// skip preceeding whitespace
		while (*start && isspace(*start)) start++;

		// remove trailing whitespace
		int last = strlen(start) - 1;
		while (last >= 0 && isspace(start[last])) {
			start[last] = '\0';
			last--;
		}
	}

	return start;
}

/****************************************************************************
 *                                                                          *
 *   								FILES                                   *
 *                                                                          *
 ****************************************************************************/

/**
 * Return true if the path identifies a file.
 */
INTERFACE bool IsFile(const char *name)
{
	bool exists = false;
	struct stat sb;
	
	if (stat(name, &sb) == 0) {
		if (sb.st_mode & S_IFREG)
		  exists = true;
	}
	return exists;
}

/**
 * Return true if the path identifies a directory.
 */
INTERFACE bool IsDirectory(const char *name)
{
	bool directory = false;
	struct stat sb;
	
	if (stat(name, &sb) == 0) {
		if (sb.st_mode & S_IFDIR)
		  directory = true;
	}
	return directory;
}

/**
 * Return true we were able to delete a file.
 */
INTERFACE bool MyDeleteFile(const char *path)
{
	bool deleted = false;

#ifdef _WIN32
	// there is also remove() which looks identical
	int res = _unlink(path);
#else
	int res = unlink(path);
#endif

	if (res == 0)
	  deleted = true;
	else {
		// !! should use Trace?
		printf("ERROR: DeleteFile %d\n", errno);
	}
	return deleted;
}

/**
 * Return true we were able to delete a directory.
 * You should only call this with a known directory
 * path (e.g. IsDirectory) should be true.
 */
INTERFACE bool DeleteDirectory(const char *path)
{
	bool deleted = false;

	// hmm, I can't find anything in posix to do this, DeleteFile
	// will probably fail if the directory isn't empty, but we're
	// not using this at the moment
	deleted = MyDeleteFile(path);

	return deleted;
}

/**
 * Return true we were able to create a directory.
 */
INTERFACE bool CreateDirectory(const char *path)
{
	bool created = false;
	
#ifdef _WIN32
	// second arg has "file permission bits" that will be
	// "modified by the process' file creation mask"
	int res = _mkdir(path);
#else
	// assume Posix
	// second arg has "file permission bits" that will be
	// "modified by the process' file creation mask"
	// RWX for owner, group, and other
	int res = mkdir(path, S_IRWXU | S_IRWXG | S_IRWXO);
#endif

	if (res == 0)
	  created = true;
	else {
		// !! should use Trace?
		printf("ERROR: CreateDirectory %d\n", errno);
	}

	return created;
}

/**
 * Return true we were able to copy a file.
 *
 * !! NOTE: This only supports copying text files, 
 * need to implement a true binary block copy.  Okay for
 * Mac config files but little else.
 */
INTERFACE bool CopyFile(const char *src, const char* dest)
{
	bool copied = false;
	
	char *stuff = ReadFile(src);
	if (stuff != NULL) {
		int written = WriteFile(dest, stuff);
		// what about newline translations!!
        // apparently msvc8 returns a uint here, 
        // avoid a warning
		if (written >= (int)strlen(stuff))
		  copied = true;
	}

	return copied;
}

/**
 * Return true if the path is an absolute path.
 */
INTERFACE bool IsAbsolute(const char* path)
{
    bool absolute = false;
    if (path != NULL) {
        int len = strlen(path);
        if (len > 0) {
            absolute = (path[0] == '/' || 
                        path[0] == '\\' ||
						// actually this is meaningless, it's a shell thing
                        //path[0] == '~' ||
                        (len > 2 && path[1] == ':'));
        }
    }
    return absolute;
}

INTERFACE char* ReadFile(const char* name) 
{
	char* content = NULL;
	
	// don't open in "translated mode"
	FILE* fp = fopen(name, "rb");
	if (fp != NULL) {
		Vbuf* vbuf = new Vbuf();
		char cbuf[1024];

		int read = fread(cbuf, 1, sizeof(cbuf) - 1, fp);
		while (read > 0) {
			cbuf[read] = 0;
			vbuf->add(cbuf);
			read = fread(cbuf, 1, sizeof(cbuf) - 1, fp);
		}

		content = vbuf->stealString();
		delete vbuf;
	}
	return content;
}

INTERFACE int ReadFile(const char* name, unsigned char* data, int length) 
{
	printf("ReadFile binary not implemented!!\n");
	return 0;
}

INTERFACE int WriteFile(const char* name, const char* content) 
{
	int written = 0;

	FILE* fp = fopen(name, "wb");
	if (fp != NULL) {
		if (content != NULL)
		  written = fwrite(content, 1, strlen(content), fp);
		fclose(fp);
	}
	return written;
}

INTERFACE int WriteFile(const char* name, unsigned char* data, int length)
{
	printf("WriteFile binary not implemented!!\n");
	return 0;
}

/**
 * There is also a posix getcwd, but it appears not to include
 * the drive letter?
 */
INTERFACE void GetWorkingDirectory(char* buffer, int max)
{
#ifdef _WIN32
	int drive = _getdrive();
	_getdcwd(drive, buffer, max);
#else
	getcwd(buffer, max);
#endif
}

/**
 * Return the number of bytes in the file.  Return -1 if the
 * file does not exist.
 */
INTERFACE int GetFileSize(const char* path)
{
	int size = -1;
	struct stat sb;
	
	if (stat(path, &sb) == 0) {
        size = sb.st_size;
	}
	return size;
}

INTERFACE bool GetFullPath(const char* relative, char* absolute, int max)
{
#ifdef WIN32
    return (_fullpath(absolute, relative, max) != NULL);
#else
	if (IsAbsolute(relative)) {
		CopyString(relative, absolute, max);
	}
	else {
		char cwd[1024];
		GetWorkingDirectory(cwd, sizeof(cwd));
		MergePaths(cwd, relative, absolute, max);
	}
	return true;
#endif
}

/**
 * Combine an absolute home directory path with a relative path.
 * If the relative path looks absolute, leave it alone.
 */
INTERFACE void MergePaths(const char* home, const char* relative, 
						  char* buffer, int max)
{
	if (relative == NULL) {
		if (home != NULL)
		  strcpy(buffer, home);
	}
	else if (home == NULL) {
		if (relative != NULL)
		  strcpy(buffer, relative);
	}
	else if (IsAbsolute(relative)) {
		// lools absolute
		strcpy(buffer, relative);
	}
	else {
		int hlen = strlen(home);
		if (hlen == 0)
		  strcpy(buffer, relative);
		else {
			strcpy(buffer, home);
			char last = home[hlen - 1];
			bool hslash = (last == '/' || last == '\\');
			bool rslash = (relative[0] == '/' || relative[0] == '\\');
			if (!hslash && !rslash)
			  strcat(buffer, "/");
			else if (hslash && rslash)
			  relative++;
			strcat(buffer, relative);
		}
	}
}

/**
 * Given the full path to a file, derive a new file path within
 * the same directory.
 */
INTERFACE void ReplacePathFile(const char* path, const char* file,
							   char* buffer)
{
	int psn = strlen(path) - 1;
	while (psn > 0 && path[psn] != '/' && path[psn] != '\\')
	  psn--;

	if (psn < 0) {
		// looked like a simple file name, no change
		strcpy(buffer, file);
	}
	else {
		strncpy(buffer, path, psn + 1);
		buffer[psn + 1] = 0;
		if (file != NULL)
		  strcat(buffer, file);
	}
}

/**
 * Given a full path name to a file, return the directory path.
 */
INTERFACE void GetDirectoryPath(const char* path, char* buffer)
{
	int psn = strlen(path) - 1;
	while (psn > 0 && path[psn] != '/' && path[psn] != '\\')
	  psn--;

	if (psn < 0) {
		// looked like a simple file name, no change
		strcpy(buffer, "");
	}
	else {
		strncpy(buffer, path, psn + 1);
		buffer[psn + 1] = 0;
	}
}

/**
 * Given a file path, return the leaf file name.
 */
INTERFACE void GetLeafName(const char* path, char* buffer, bool extension)
{
	int last = strlen(path) - 1;
	int dot = -1;
	int psn = last;

	while (psn > 0 && path[psn] != '/' && path[psn] != '\\') {
		if (path[psn] == '.')
		  dot = psn;
		psn--;
	}

	if (psn < 0) {
		// looked like a simple file name, no change
		psn = 0;
	}
	else
	  psn++;

	if (!extension && dot > 0)
	  last = dot - 1;

	int len = last - psn + 1;

	strncpy(buffer, &path[psn], len);
	buffer[len] = 0;
}

/**
 * Return the names of the files in a directory, optionally with a 
 * specific extension.
 * Think about more general directory browsing utilities.
 */
INTERFACE StringList* GetDirectoryFiles(const char* path, const char* ext)
{
	StringList* files = NULL;

	if (IsDirectory(path)) {
#ifdef WIN32
        WIN32_FIND_DATA fd;
        char wild[MAX_PATH];
        CopyString(path, wild, MAX_PATH);
        AppendString("\\*", wild, MAX_PATH);

        HANDLE h = FindFirstFile(wild, &fd);
        if (h == INVALID_HANDLE_VALUE) {
            printf("FindFirstFile: invalid handle returned\n");
        }
        else {
            do {
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    // it's another directory, don't descend
                }
                else {
                    // fd.cFileName does not have the diretcory prefix
                    char* name = fd.cFileName;
                    if (ext == NULL || EndsWithNoCase(name, ext)) {
                        char buffer[MAX_PATH];
                        MergePaths(path, name, buffer, sizeof(buffer));
                        if (IsFile(buffer)) {
                            if (files == NULL)
                              files = new StringList();
                            files->add(buffer);
                        }
                    }
                }
            }
            while (FindNextFile(h, &fd) != 0);

            FindClose(h);
        }
#else		
		DIR* dir = opendir(path);
		if (dir == NULL) {
			// various reasons including EACCESS, ENOENT, ENOTDIR...
			printf("ERROR: GetDirectoryFiles:opendir %d\n", errno);
		}
		else {
			// this may return entries for . and ..
			struct dirent* ent;
			while ((ent = readdir(dir)) != NULL) {
				char* name = ent->d_name;
				if (ext == NULL || EndsWithNoCase(name, ext)) {
					// have to add the directory
					char buffer[PATH_MAX];
					MergePaths(path, name, buffer, sizeof(buffer));
					if (IsFile(buffer)) {
						if (files == NULL)
						  files = new StringList();
						files->add(buffer);
					}
				}
			}

			// in theory this can return EBADF and EINTR
			closedir(dir);
		}
#endif
	}

	return files;
}

/****************************************************************************
 *                                                                          *
 *   							  EXCEPTIONS                                *
 *                                                                          *
 ****************************************************************************/

INTERFACE AppException::AppException(const char *msg, bool nocopy)
{
	AppException(ERR_GENERIC, msg, nocopy);
}

INTERFACE AppException::AppException(int c, const char *msg, bool nocopy)
{
	mCode = c;
	if (nocopy)
	  mMessage = (char *)msg;
	else if (msg == NULL) 
	  mMessage = NULL;
	else {
		// if this throws, then I guess its more important 
		mMessage = new char[strlen(msg) + 1];
		strcpy(mMessage, msg);
	}	
}

/** 
 * copy constructor, important for this exceptions since
 * we want to pass ownership of the string
 */
INTERFACE AppException::AppException(AppException &src) 
{
	mCode = src.mCode;
	mMessage = src.mMessage;
	src.mMessage = NULL;
}

INTERFACE AppException::~AppException(void)
{
	delete mMessage;
}

INTERFACE void AppException::print(void)
{
	if (mMessage)
	  printf("ERROR %d : %s\n", mCode, mMessage);
	else
	  printf("ERROR %d\n", mCode);

}

/****************************************************************************
 *                                                                          *
 *   							   REGISTRY                                 *
 *                                                                          *
 ****************************************************************************/

#ifdef _WIN32

PRIVATE char* GetRegistry(HKEY root, const char* key, const char* name)
{
	char* value = NULL;
    unsigned char buffer[1024];
    HKEY hkey;
    LONG status;
 
	status = RegOpenKeyEx(root, key, 0, KEY_QUERY_VALUE, &hkey);
	if (status == ERROR_SUCCESS) {

		DWORD max = sizeof(buffer);
		status = RegQueryValueEx(hkey, name, NULL, NULL, buffer, &max);
		if (status == ERROR_SUCCESS)
		  value = CopyString((char*)buffer);

		RegCloseKey(hkey);
	}

	return value;
}

INTERFACE char* GetRegistryLM(const char* key, const char* name)
{
	return GetRegistry(HKEY_LOCAL_MACHINE, key, name);
}

INTERFACE char* GetRegistryCU(const char* key, const char* name)
{
	return GetRegistry(HKEY_CURRENT_USER, key, name);
}

PRIVATE bool SetRegistry(HKEY root, const char* key, const char* name,
                         const char* value)
{
    HKEY hkey;
    LONG status;
    DWORD disposition;
    bool set = false;

    // this only opens, RegCreateKeyEx will create or optn
	//status = RegOpenKeyEx(root, key, 0, KEY_SET_VALUE, &hkey);

    status = RegCreateKeyEx(root, key, 0, NULL, 0, KEY_SET_VALUE, NULL, 
                            &hkey, &disposition);
    
	if (status == ERROR_SUCCESS) {

        // if it matters, disposition will have REG_CREATED_NEW_KEY or
        // REG_OPENED_EXISTING_KEY

        status = RegSetValueEx(hkey, name, 0, REG_SZ, (const BYTE*)value,
                               strlen(value) + 1);

        if (status == ERROR_SUCCESS)
          set = true;
        else {
            printf("ERROR: RegSetValueEx %ld\n", status);
            fflush(stdout);
        }

		RegCloseKey(hkey);
	}

	return set;
}

INTERFACE bool SetRegistryLM(const char* key, const char* name, const char* value)
{
	return SetRegistry(HKEY_LOCAL_MACHINE, key, name, value);
}

INTERFACE bool SetRegistryCU(const char* key, const char* name, const char* value)
{
	return SetRegistry(HKEY_CURRENT_USER, key, name, value);
}

#endif

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
