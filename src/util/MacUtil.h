/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Misc debugging utilities for the Mac.
 * Most of this is relevant only for qwin, but keep it together
 * for awhile.
 */

#ifndef MAC_UTIL_H
#define MAC_UTIL_H

// caller must have this
#include <Carbon/Carbon.h>

bool CheckStatus(OSStatus result, const char* prefix);
bool CheckErr(OSErr err, const char* prefix);

CFStringRef MakeCFStringRef(const char* src);
void FreeCFStringRef(CFStringRef cfstr);
bool GetCFStringChars(CFStringRef cfstr, char* buffer, int max);
char* GetCString(CFStringRef cfstr);

const char* GetEventClassName(EventClass cls);
const char* GetMouseEventName(int event);
const char* GetKeyboardEventName(int event);
const char* GetAppEventName(int event);
const char* GetWindowEventName(int event);
const char* GetCommandEventName(int event);

void TraceEvent(const char* prefix, int cls, int kind);
void TraceEvent(const char* prefix, EventRef event);

void SetRectLTWH(Rect* rect, int left, int top, int width, int height);

//
// Fonts & Text
//

void PrintFontMetrics(const char* name, ATSFontRef font);
void PrintFontMetrics(ATSFontMetrics* metrics);
void ListFonts();

void QuickDrawString(WindowRef window, int left, int top, const char* text);
void MLTEDrawString(WindowRef window, int left, int top, CFStringRef text);
void ATSUDrawString(WindowRef window, int left, int top, ATSFontRef font, CFStringRef text);

void PrintBundle();

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
