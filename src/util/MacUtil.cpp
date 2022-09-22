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
 * 
 */

#include <stdio.h>
#include <string.h>

#include <Carbon/Carbon.h>

#include "Util.h"
#include "MacUtil.h"

//////////////////////////////////////////////////////////////////////
//
// Return value checking
//
//////////////////////////////////////////////////////////////////////

bool CheckStatus(OSStatus result, const char* prefix)
{
	bool ok = true;

	if (result != 0) {
		printf("%s %d\n", prefix, (int)result);
		ok = false;
	}

	return ok;
}

bool CheckErr(OSErr err, const char* prefix)
{
	bool ok = true;

	if (err != 0) {
		printf("%s %d\n", prefix, err);
		ok = false;
	}

	return ok;
}

//////////////////////////////////////////////////////////////////////
//
// Rectalizing
//
//////////////////////////////////////////////////////////////////////

/**
 * Convert left, top, width, height to a Rect.
 */
void SetRectLTWH(Rect* rect, int left, int top, int width, int height)
{
	// left, top, right, bottom
	// removed in Lion
	//SetRect(rect, left, top, left + width, top + height);
	rect->left = left;
	rect->top = top;
	rect->right = left + width;
	rect->bottom = top + height;
}

//////////////////////////////////////////////////////////////////////
//
// String conversions
//
//////////////////////////////////////////////////////////////////////

/**
 * I started with kCFStringEncodingMacRoman but this didn't
 * display the o-umlat on Mobius.  I tried UTF8 but that caused
 * a bunch of invalid context errors in MacGraphics, I guess because
 * 0xf6 isn't valid UTF8?   ISOLatin1 did the trick but we should try
 * to use UTF8 consistently on both Mac and Windows.
 */
//CFStringBuiltInEncodings DefaultEncoding = kCFStringEncodingMacRoman;
//CFStringBuiltInEncodings DefaultEncoding = kCFStringEncodingUTF8;
CFStringBuiltInEncodings DefaultEncoding = kCFStringEncodingISOLatin1;

/**
 * Must use CFRelease(str) to free the string.
 */
CFStringRef MakeCFStringRef(const char* src)
{
	CFStringRef cfstr = NULL;
	if (src != NULL) {
		cfstr = CFStringCreateWithCString(kCFAllocatorDefault, src, 
										  DefaultEncoding);
	}
	return cfstr;
}

void FreeCFStringRef(CFStringRef cfstr)
{
	if (cfstr != NULL)
	  CFRelease(cfstr);
}

bool GetCFStringChars(CFStringRef cfstr, char* buffer, int max)
{
	Boolean success = CFStringGetCString(cfstr, buffer, max, 
										 DefaultEncoding);
	// if (!success) what?
	return success;
}

char* GetCString(CFStringRef cfstr)
{
	char *string = NULL;
	char buffer[2048];
	if (GetCFStringChars(cfstr, buffer, sizeof(buffer) - 2))
	  string = CopyString(buffer);
	return string;
}

//////////////////////////////////////////////////////////////////////
//
// Constant rendering
//
//////////////////////////////////////////////////////////////////////

/**
 * Flag to filter out some really common events to reduce clutter.
 */
bool Verbose = false;

const char* GetEventClassName(EventClass cls) 
{
	const char* name = "unknown";
	switch (cls) {
		case kEventClassMouse: name = "mous"; break;
		case kEventClassKeyboard: name = "keyb"; break;
		case kEventClassTextInput: name = "text"; break;
		case kEventClassApplication: name = "appl"; break;
		case kEventClassAppleEvent: name = "eppc"; break;
		case kEventClassMenu: name = "menu"; break;
		case kEventClassWindow: name = "wind"; break;
		case kEventClassControl: name = "cntl"; break;
		case kEventClassCommand: name = "cmds"; break;
		case kEventClassTablet: name = "tblt"; break;
		case kEventClassVolume: name = "vol"; break;
		case kEventClassAppearance: name = "appm"; break;
		case kEventClassService: name = "serv"; break;
		case kEventClassToolbar: name = "tbar"; break;
		case kEventClassToolbarItem: name = "tbit"; break;
		case kEventClassToolbarItemView: name = "tbiv"; break;
		case kEventClassAccessibility: name = "acce"; break;
		case kEventClassSystem: name = "macs"; break;
		case kEventClassInk: name = "ink"; break;
		case kEventClassTSMDocumentAccess: name = "tdac"; break;
	}
	return name;
};

const char* GetMouseEventName(int event)
{
	const char* name = "unknown";
	switch (event) {
		case kEventMouseDown: name = "MouseDown"; break;
		case kEventMouseUp: name = "MouseUp"; break;
		case kEventMouseMoved: 
			name = (Verbose) ? "MouseMoved" : NULL;
			break;
		case kEventMouseDragged: 
			name = (Verbose) ? "MouseDragged" : NULL;
			break;
		case kEventMouseEntered: name = "MouseEntered"; break;
		case kEventMouseExited: name = "MouseExited"; break;
		case kEventMouseWheelMoved: name = "MouseWheelMoved"; break;
	}
	return name;
}

const char* GetKeyboardEventName(int event)
{
	const char* name = "unknown";
	switch (event) {
		case kEventRawKeyDown: name = "RawKeyDown"; break;
		case kEventRawKeyRepeat: name = "RawKeyRepeat"; break;
		case kEventRawKeyUp: name = "RawKeyUp"; break;
		case kEventRawKeyModifiersChanged: name = "RawKeyModifiersChanged"; break;
		case kEventHotKeyPressed: name = "HotKeyPressed"; break;
		case kEventHotKeyReleased: name = "HotKeyReleased"; break;
	}
	return name;
}

const char* GetAppEventName(int event)
{
	const char* name = "unknown";
	switch (event) {
		case kEventAppActivated: name = "AppActivated"; break;
		case kEventAppDeactivated: name = "AppDeactivated"; break;
		case kEventAppQuit: name = "AppQuit"; break;
		case kEventAppLaunchNotification: name = "AppLaunchNotification"; break;
		case kEventAppLaunched: name = "AppLaunched"; break;
		case kEventAppTerminated: name = "AppTerminated"; break;
		case kEventAppFrontSwitched: name = "AppSwitched"; break;
		case kEventAppFocusMenuBar: name = "AppFocusMenuBar"; break;
		case kEventAppFocusNextDocumentWindow: name = "AppFocusNextDocument"; break;
		case kEventAppFocusNextFloatingWindow: name = "AppFocusNextFloating"; break;
		case kEventAppFocusToolbar: name = "AppFocusToolbar"; break;
		case kEventAppFocusDrawer: name = "AppFocusDrawer"; break;
		case kEventAppGetDockTileMenu: name = "AppGetDocTileMenu"; break;
		case kEventAppIsEventInInstantMouser: name = "AppIsEventInInstantMouser"; break;
		case kEventAppHidden: name = "AppHidden"; break;
		case kEventAppShown: name = "AppShown"; break;
		case kEventAppSystemUIModeChanged: name = "AppSystemUIModeChanged"; break;
		case kEventAppAvailableWindowBoundsChanged: name = "AppAvailableWindowBoundsChanged"; break;
		case kEventAppActiveWindowChanged: name = "AppActiveWindowChanged"; break;
	}
	return name;
}

const char* GetWindowEventName(int event)
{
	const char* name = "unknown";
	switch (event) {
    
		// window refresh events
    
		case kEventWindowUpdate: name = "Update"; break;
		case kEventWindowDrawContent: name = "DrawContent"; break;
    
		// window activation events
    
		case kEventWindowActivated: name = "Activated"; break;
		case kEventWindowDeactivated: name = "Deactivated"; break;
		case kEventWindowHandleActivate: name = "HandleActivate"; break;
		case kEventWindowHandleDeactivate: name = "HandleDeactivate"; break;
		case kEventWindowGetClickActivation: name = "GetClickActivation"; break;
		case kEventWindowGetClickModality: name = "GetClickModality"; break;
    
		// window state change events
		
		case kEventWindowShowing: name = "Showing"; break;
		case kEventWindowHiding: name = "Hiding"; break;
		case kEventWindowShown: name = "Shown"; break;
		case kEventWindowHidden: name = "Hidden"; break;
		case kEventWindowCollapsing: name = "Collapsing"; break;
		case kEventWindowCollapsed: name = "Collapsed"; break;
		case kEventWindowExpanding: name = "Expandind"; break;
		case kEventWindowExpanded: name = "Expanded"; break;
		case kEventWindowZoomed: name = "Zoomed"; break;
		case kEventWindowBoundsChanging: name = "BoundsChanging"; break;
		case kEventWindowBoundsChanged: name = "BoundsChanged"; break;
		case kEventWindowResizeStarted: name = "ResizeStarted"; break;
		case kEventWindowResizeCompleted: name = "ResizeComplete"; break;
		case kEventWindowDragStarted: name = "DragStarted"; break;
		case kEventWindowDragCompleted: name = "DragCompleted"; break;
		case kEventWindowClosed: name = "Closed"; break;
		case kEventWindowTransitionStarted: name = "TransitionStarted"; break;
		case kEventWindowTransitionCompleted: name = "TransitionComplete"; break;
    
		// window click events
    
		case kEventWindowClickDragRgn: name = "ClickDragRgn"; break;
		case kEventWindowClickResizeRgn: name = "ClickResizeRgn"; break;
		case kEventWindowClickCollapseRgn: name = "ClickCollapseRgn"; break;
		case kEventWindowClickCloseRgn: name = "ClickCloseRgn"; break;
		case kEventWindowClickZoomRgn: name = "ClickZoomRgn"; break;
		case kEventWindowClickContentRgn: name = "ClickContentRgn"; break;
		case kEventWindowClickProxyIconRgn: name = "ClickProxyIconRgn"; break;
		case kEventWindowClickToolbarButtonRgn: name = "ClickToolbarButtonRgn"; break;
		case kEventWindowClickStructureRgn: name = "ClickStrucctureRgn"; break;

		// window cursor change events

		case kEventWindowCursorChange:
			name = (Verbose) ? "CursorChange" : NULL;
			break;

		// window action events
    
		case kEventWindowCollapse: name = "Collapse"; break;
		case kEventWindowCollapseAll: name = "CollapseAll"; break;
		case kEventWindowExpand: name = "Expand"; break;
		case kEventWindowExpandAll: name = "ExpandAll"; break;
		case kEventWindowClose: name = "Close"; break;
		case kEventWindowCloseAll: name = "CloseAll"; break;
		case kEventWindowZoom: name = "Zoom"; break;
		case kEventWindowZoomAll: name = "ZoomAll"; break;
		case kEventWindowContextualMenuSelect: name = "ContextualMenuSelect"; break;
		case kEventWindowPathSelect: name = "PathSelect"; break;
		case kEventWindowGetIdealSize: name = "GetIdealSize"; break;
		case kEventWindowGetMinimumSize: name = "GetMinimumSize"; break;
		case kEventWindowGetMaximumSize: name = "GetMaximumSize"; break;
		case kEventWindowConstrain: name = "Constrain"; break;
		case kEventWindowHandleContentClick: name = "HandleContentClick"; break;
		case kEventWindowGetDockTileMenu: name = "GetDocTileMenu"; break;
		case kEventWindowProxyBeginDrag: name = "ProxyBeginDrag"; break;
		case kEventWindowProxyEndDrag: name = "ProxyEndDrag"; break;
		case kEventWindowToolbarSwitchMode: name = "ToolbarSwitchMode"; break;
    
		// window focus events
    
		case kEventWindowFocusAcquired: name = "FocusAcquired"; break;
		case kEventWindowFocusRelinquish: name = "FocusRelinquish"; break;
		case kEventWindowFocusContent: name = "FocusContent"; break;
		case kEventWindowFocusToolbar: name = "FocusToolbar"; break;
		case kEventWindowFocusDrawer: name = "FocusDrawer"; break;
    
		// sheet events
    
		case kEventWindowSheetOpening: name = "SheetOpening"; break;
		case kEventWindowSheetOpened: name = "SheetOpened"; break;
		case kEventWindowSheetClosing: name = "SheetClosing"; break;
		case kEventWindowSheetClosed: name = "SheetClosed"; break;
    
		// Drawer events
    
		case kEventWindowDrawerOpening: name = "DrawerOpening"; break;
		case kEventWindowDrawerOpened: name = "DrawerOpened"; break;
		case kEventWindowDrawerClosing: name = "DrawerClosing"; break;
		case kEventWindowDrawerClosed: name = "DrawerClosed"; break;
    
		// window definition events
    
		case kEventWindowDrawFrame: name = "DrawFrame"; break;
		case kEventWindowDrawPart: name = "DrawPart"; break;
		case kEventWindowGetRegion: name = "GetRegion"; break;
		case kEventWindowHitTest: name = "HitTest"; break;
		case kEventWindowInit: name = "Init"; break;
		case kEventWindowDispose: name = "Dispose"; break;
		case kEventWindowDragHilite: name = "DragHilite"; break;
		case kEventWindowModified: name = "Modified"; break;
		case kEventWindowSetupProxyDragImage: name = "SetupProxyDragImage"; break;
		case kEventWindowStateChanged: name = "StateChanged"; break;
		case kEventWindowMeasureTitle: name = "MeasureTitle"; break;
		case kEventWindowDrawGrowBox: name = "DrawGrowBox"; break;
		case kEventWindowGetGrowImageRegion: name = "GetGrowImageRegion"; break;
		case kEventWindowPaint: name = "Paint"; break;
		case kEventWindowAttributesChanged: name = "AttributesChanged"; break;
		case kEventWindowTitleChanged: name = "TitleChanged"; break;
	}
	return name;
}

const char* GetControlEventName(int event)
{
	const char* name = "unknown";
	switch (event) {
		case kEventControlInitialize: name="Initialize"; break;
		case kEventControlDispose: name="Dispose"; break;
		case kEventControlGetOptimalBounds: name="GetOptimalBounds"; break;
		case kEventControlHit: name="Hit"; break;
		case kEventControlSimulateHit: name="SimulateHit"; break;
		case kEventControlHitTest: name="HitTest"; break;
		case kEventControlDraw: name="Draw"; break;
		case kEventControlApplyBackground: name="ApplyBackground"; break;
		case kEventControlApplyTextColor: name="ApplyTextColor"; break;
		case kEventControlSetFocusPart: name="SetFocusPart"; break;
		case kEventControlGetFocusPart: name="GetFocusPart"; break;
		case kEventControlActivate: name="Activate"; break;
		case kEventControlDeactivate: name="Deactivate"; break;
		case kEventControlSetCursor: name="SetCursor"; break;
		case kEventControlContextualMenuClick: name="ContextualMenuClick"; break;
		case kEventControlClick: name="Click"; break;
		case kEventControlGetNextFocusCandidate: name="GetNextFocusCandidate"; break;
		case kEventControlGetAutoToggleValue: name="GetAutoToggleValue"; break;
		case kEventControlInterceptSubviewClick: name="InterceptSubviewClick"; break;
		case kEventControlGetClickActivation: name="GetClickActivation"; break;
		case kEventControlDragEnter: name="DragEnter"; break;
		case kEventControlDragWithin: name="DragWithin"; break;
		case kEventControlDragLeave: name="DragLeave"; break;
		case kEventControlDragReceive: name="DragReceive"; break;
		case kEventControlTrack: name="Track"; break;
		case kEventControlGetScrollToHereStartPoint: name="GetScrollToHereStartPoint"; break;
		case kEventControlGetIndicatorDragConstraint: name="GetIndicatorDragConstraint"; break;
		case kEventControlIndicatorMoved: name="IndicatorMoved"; break;
		case kEventControlGhostingFinished: name="GhostingFinished"; break;
		case kEventControlGetActionProcPart: name="GetActionProcPart"; break;
		case kEventControlGetPartRegion: name="GetPartRegion"; break;
		case kEventControlGetPartBounds: name="GetPartBounds"; break;
		case kEventControlSetData: name="SetData"; break;
		case kEventControlGetData: name="GetData"; break;
		case kEventControlGetSizeConstraints: name="GetSizeConstraints"; break;
		case kEventControlValueFieldChanged: name="ValueFieldChanged"; break;
		case kEventControlAddedSubControl: name="AddedSubControl"; break;
		case kEventControlRemovingSubControl: name="RemovingSubControl"; break;
		case kEventControlBoundsChanged: name="BoundsChanged"; break;
		case kEventControlTitleChanged: name="TitleChanged"; break;
		case kEventControlOwningWindowChanged: name="OwningWindowChanged"; break;
		case kEventControlHiliteChanged: name="HiliteChanged"; break;
		case kEventControlEnabledStateChanged: name="EnabledStateChanged"; break;
		case kEventControlArbitraryMessage: name="ArbitraryMessage"; break;

			// these add too much noise



	}

	return name;
}

const char* GetCommandEventName(int event)
{
	const char* name = "unknown";
	switch (event) {
		case kEventCommandProcess: name = "CommandProcess"; break;
		case kEventCommandUpdateStatus: name = "CommandUpdateStatus"; break;
	}
	return name;
}

void TraceEvent(const char* prefix, int cls, int kind)
{
	const char* classname = GetEventClassName(cls);
	const char* kindname = NULL;

	switch (cls) {
		case kEventClassMouse: 
			kindname = GetMouseEventName(kind);
			break;
		case kEventClassKeyboard: 
			kindname = GetKeyboardEventName(kind);
			break;
		case kEventClassApplication: 
			kindname = GetAppEventName(kind);
			break;
		case kEventClassWindow: 
			kindname = GetWindowEventName(kind);
			break;
		case kEventClassCommand:
			kindname = GetCommandEventName(kind);
			break;
		case kEventClassControl:
			kindname = GetControlEventName(kind);
			break;
	}

 	if (prefix == NULL) prefix = "";
	if (classname == NULL) classname = "";

	if (kindname != NULL) {
		printf("%s %s %s\n", prefix, classname, kindname);
	}
	else {
		// let a null kindname indiciate that the trace is suppresse to 
		// control verbosity
		//printf("%s Class %s kind %d\n", prefix, classname, kind);
	}
}

void TraceEvent(const char* prefix, EventRef event)
{
	int cls = GetEventClass(event);
	int kind = GetEventKind(event);
	TraceEvent(prefix, cls, kind);
}

//////////////////////////////////////////////////////////////////////
//
// Fonts
//
//////////////////////////////////////////////////////////////////////

void PrintFontMetrics(const char* name, ATSFontRef font) 
{
	ATSFontMetrics horiz;
	ATSFontMetrics vert;

    ATSFontGetHorizontalMetrics (font, 0, &horiz);
	ATSFontGetVerticalMetrics(font, 0, &vert);
	
	printf("*** Font %s ***\n", name);
	printf("Horizontal:\n");
	PrintFontMetrics(&horiz);
	printf("Vertical:\n");
	PrintFontMetrics(&vert);
}

void PrintFontMetrics(ATSFontMetrics* metrics)
{
	printf("  version=%d\n", (int)metrics->version);
	printf("  ascent=%f\n", metrics->ascent);
	printf("  descent=%f\n", metrics->descent);
	printf("  leading=%f\n", metrics->leading);
	printf("  avgAdvanceWidth=%f\n", metrics->avgAdvanceWidth);
	printf("  maxAdvanceWidth=%f\n", metrics->maxAdvanceWidth);
	printf("  minLeftSideBearing=%f\n", metrics->minLeftSideBearing);
	printf("  minRightSideBearing=%f\n", metrics->minRightSideBearing);
	printf("  stemWidth=%f\n", metrics->stemWidth);
	printf("  stemHeight=%f\n", metrics->stemHeight);
	printf("  capHeight=%f\n", metrics->capHeight);
	printf("  xHeight=%f\n", metrics->xHeight);
	printf("  italicAngle=%f\n", metrics->italicAngle);
	printf("  underlinePosition=%f\n", metrics->underlinePosition);
	printf("  underlineThickness=%f\n", metrics->underlineThickness);
};


void ListFonts() 
{
	ATSFontIterator iterator;
	OSStatus status = ATSFontIteratorCreate(kATSFontContextGlobal, NULL, NULL, 
											kATSOptionFlagsUnRestrictedScope,
											&iterator);

	while (status == 0) {
		ATSFontRef font;
		status = ATSFontIteratorNext(iterator, &font);
		if (status == 0) {
			CFStringRef name;
			ATSFontGetName(font, kATSOptionFlagsDefault, &name);

			CFStringRef psname;
			ATSFontGetPostScriptName(font, kATSOptionFlagsDefault, &psname);

			ATSFontMetrics vmetrics;
			ATSFontGetVerticalMetrics(font, kATSOptionFlagsDefault, &vmetrics);

			ATSFontMetrics hmetrics;
			ATSFontGetHorizontalMetrics(font, kATSOptionFlagsDefault, &hmetrics);

			// this one works with MacRoman
			const char* cname = CFStringGetCStringPtr(name, kCFStringEncodingMacRoman);
			// this one doesn't
			//const char* cpsname = CFStringGetCStringPtr(psname, kCFStringEncodingMacRoman);
			char cpsname[1024];
			CFStringGetCString(psname, cpsname, sizeof(cpsname), kCFStringEncodingMacRoman);
			
			// supposed to use ATSFontGetFileReference which returns
			// an FSRef but that isn't defined in my headers for 10.4.11

			// !! getting deprecation warnings here, since we're not supporting
			// 10.4 any more need to rework this...

			FSSpec filespec;
			ATSFontGetFileSpecification(font, &filespec);

			// convert the old nasty FSSpec to a new just as nasty FSRef
			FSRef file;
			FSpMakeFSRef(&filespec, &file);
			
			// there is this...
			//HFSUniStr255 filename;
			//FSGetCatalogInfo(&file, kFSCatInfoNone, NULL, &filename, NULL, NULL);

			// but this is easier
			unsigned char filename[1024];
			FSRefMakePath(&file, filename, sizeof(filename));

			// postscript name seems to be binary, blow it off
			printf("%s, %s\n", cname, filename);
		}
		else if (status != kATSIterationCompleted) {
			printf("Font iterator returned %d\n", (int)status);
		}
	}
}

ATSFontRef FindFont(const char* name)
{
	CFStringRef cfname = MakeCFStringRef(name);

	ATSFontRef font = ATSFontFindFromName(cfname, kATSOptionFlagsDefault);

	CFRelease(cfname);

	//if (font == 0)
	//printf("Unable to find font!\n");

	return font;
}

//////////////////////////////////////////////////////////////////////
//
// Text Measurement
//
//////////////////////////////////////////////////////////////////////

void DebugRect(const char* prefix, Rect* bounds)
{
	printf("%s top %d left %d bottom %d right %d\n",
		   prefix, bounds->top, bounds->left, bounds->bottom, bounds->right);
}


//////////////////////////////////////////////////////////////////////
//
// Bundle Info
//
//////////////////////////////////////////////////////////////////////

void PrintBundle()
{
	CFBundleRef mainB = CFBundleGetMainBundle();
	fprintf(stderr, "MainBundle:\n");
	CFShow(mainB);
	
	CFURLRef url1 = CFBundleCopyBuiltInPlugInsURL(mainB);
	fprintf(stderr, "BuiltInPlugInsURL:\n");
	CFShow(url1);
	
	CFURLRef url2 = CFBundleCopyExecutableURL(mainB);
	fprintf(stderr, "ExecutableURL:\n");
	CFShow(url2);
	
	CFURLRef url3 = CFBundleCopyPrivateFrameworksURL(mainB);
	fprintf(stderr, "PrivateFrameworksURL:\n");
	CFShow(url3);
	
	CFURLRef url4 = CFBundleCopyResourcesDirectoryURL(mainB);
	fprintf(stderr, "ResourcesDirectoryURL:\n");
	CFShow(url4);
	
	CFURLRef url5 = CFBundleCopySharedFrameworksURL(mainB);
	fprintf(stderr, "SharedFrameworksURL:\n");
	CFShow(url5);
	
	CFURLRef url6 = CFBundleCopySharedSupportURL(mainB);
	fprintf(stderr, "SharedSupportURL:\n");
	CFShow(url6);
	
	CFURLRef url7 = CFBundleCopySupportFilesDirectoryURL(mainB);
	fprintf(stderr, "SupportFilesDirectoryURL:\n");
	CFShow(url7);
		
	CFURLRef url11 = CFBundleCopyBundleURL(mainB);
	fprintf(stderr, "BundleURL:\n");
	CFShow(url11);
	
	CFStringRef region1 = CFBundleGetDevelopmentRegion(mainB);
	fprintf(stderr, "DevelopmentRegion:\n");
	CFShow(region1);
	
	CFStringRef ident = CFBundleGetIdentifier(mainB);
	fprintf(stderr, "Identifier:\n");
	CFShow(ident);
	
	CFDictionaryRef dict3 = CFBundleGetInfoDictionary(mainB);
	fprintf(stderr, "InfoDictionary:\n");
	CFShow(dict3);
	
	CFDictionaryRef dict4 = CFBundleGetLocalInfoDictionary(mainB);
	fprintf(stderr, "LocalInfoDictionary:\n");
	CFShow(dict4);
	
	UInt32 tid = CFBundleGetTypeID();
	fprintf(stderr, "TypeID: %ld\n", (long)tid);

	UInt32 version = CFBundleGetVersionNumber(mainB);
	fprintf(stderr, "VersionNumber: %ld\n", (long)version);

	CFStringRef bundleName2 = 
		reinterpret_cast<CFStringRef>
		( CFBundleGetValueForInfoDictionaryKey(mainB, CFSTR("CFBundleName") )  );

	//CFBundleGetValueForInfoDictionary just returns a CFTypeRef - 
	// just cast that into what you know the key is

	fprintf(stderr, "BundleName:\n");
	CFShow(bundleName2);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
