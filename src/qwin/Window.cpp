/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * This file contains the Window abstract class as well as the
 * Windows implementation.  The Mac implementation is in MacWindow.cpp
 *
 */

#include <stdio.h>
#include <memory.h>

#include "Util.h"
#include "KeyCode.h"
#include "Trace.h"

#include "Qwin.h"
#include "UIManager.h"

QWIN_BEGIN_NAMESPACE

/****************************************************************************
 *                                                                          *
 *   							 WINDOW EVENT                               *
 *                                                                          *
 ****************************************************************************/

WindowEvent::WindowEvent()
{
	mWindow = NULL;
	mId = WINDOW_EVENT_ACTIVATED;
}

WindowEvent::WindowEvent(Window* window, int id)
{
	mWindow = window;
	mId = id;
}

WindowEvent::~WindowEvent()
{
}

void WindowEvent::setWindow(Window* w)
{
	mWindow = w;
}

Window* WindowEvent::getWindow()
{
	return mWindow;
}

void WindowEvent::setId(int id)
{
	mId = id;
}

int WindowEvent::getId()
{
	return mId;
}

/****************************************************************************
 *                                                                          *
 *   								WINDOW                                  *
 *                                                                          *
 ****************************************************************************/

PUBLIC Window::Window() {
	initWindow();
}

PUBLIC Window::Window(Window *parent) {
	initWindow();
	setParent(parent);
}

PUBLIC Window::Window(Window *parent, const char *title) {
	initWindow();
	setParent(parent);
	setTitle(title);
}

PUBLIC Window::~Window() 
{
	// what about parent/child relationships!
	delete mMenuBar;
	delete mPopup;
	delete mTitle;
	delete mIcon;
	delete mAccelerators;
	delete mWindowListeners;
    delete mTextMetrics;
	delete mFocusables;
}

void Window::initWindow() 
{
	mClassName = "Window";
	mClass = NULL;
	mContext = NULL;
	mMenuBar = NULL;
	mPopup = NULL;
	mTitle = NULL;
	mIcon = NULL;
	mAccelerators = NULL;
	mFocusables = NULL;
	mFocus = 0;
    mForcedFocus = false;
	mWindowListeners = NULL;
	mAutoSize = false;
	mAutoCenter = false;
	mMaximized = false;
	mMinimized = false;
	mNoClose = false;
	mRunning = false;
    mTextMetrics = NULL;
}

ComponentUI* Window::getUI()
{
    if (mUI == NULL)
      mUI = UIManager::getWindowUI(this);
    return mUI;
}

WindowUI* Window::getWindowUI()
{
    return (WindowUI*)getUI();
}

bool Window::isRunnable()
{
	return true;
}

PUBLIC void Window::setTitle(const char *title) 
{
	delete mTitle;
	mTitle = CopyString(title);
}

PUBLIC const char* Window::getTitle()
{
    return mTitle;
}

void Window::setClass(const char* name)
{
	// don't bother copying this, it will always be static
	mClass = name;
}

const char* Window::getClass()
{
	return mClass;
}

void Window::setNoClose(bool b)
{
	mNoClose = b;
}

bool Window::isNoClose()
{
	return mNoClose;
}

void Window::setAutoSize(bool b)
{
	mAutoSize = b;
}

bool Window::isAutoSize()
{
	return mAutoSize;
}

void Window::setAutoCenter(bool b)
{
	mAutoCenter = b;
}

bool Window::isAutoCenter()
{
	return mAutoCenter;
}

void Window::setRunning(bool b)
{
	mRunning = b;
}

bool Window::isRunning()
{
	return mRunning;
}

void Window::setMaximized(bool b)
{
	mMaximized = b;
}

bool Window::isMaximized()
{
	return mMaximized;
}

void Window::setMinimized(bool b)
{
	mMinimized = b;
}

bool Window::isMinimized()
{
	return mMinimized;
}

void Window::setIcon(const char *s) 
{
	delete mIcon;
	mIcon = CopyString(s);
}

const char* Window::getIcon()
{
	return mIcon;
}

void Window::setAccelerators(const char *s) 
{
	delete mAccelerators;
	mAccelerators = CopyString(s);
}

const char* Window::getAccelerators()
{
	return mAccelerators;
}

void Window::setMenuBar(MenuBar* m) {
	delete mMenuBar;
	mMenuBar = m;
	if (mMenuBar != NULL) {
		// have to set this so it can get to a Context later
		// but it isn't on our child list to avoid confusing the
		// layout manager
		mMenuBar->setParent(this);
	}
}

MenuBar* Window::getMenuBar()
{
	return mMenuBar;
}

void Window::setPopupMenu(PopupMenu* p) {
	delete mPopup;
	mPopup = p;
	if (mPopup != NULL)
	  mPopup->setParent(this);
}

PopupMenu* Window::getPopupMenu()
{
	return mPopup;
}

/**
 * Corresponds to a Swing method.
 */
PUBLIC Container* Window::getContentPane()
{
	return this;
}

PUBLIC Context* Window::getContext() {
	
	Context* c = mContext;
	if (c == NULL && mParent != NULL)
	  c = ((Window*)mParent)->getContext();
	return c;
}

PUBLIC void Window::setContext(Context* c) 
{
	mContext = c;
}

/**
 * Kludge for the KeyConfigDialog.  When this is true, we will
 * redirect all keyboard events to the Window's message handler
 * rather than the controls handler.  
 */
PUBLIC void Window::setForcedFocus(bool b)
{
    mForcedFocus = b;
}

PUBLIC bool Window::isForcedFocus()
{
    return mForcedFocus;
}

/**
 * Override the Component setBackground method and convert
 * it to a native window property.  This doesn't appear to 
 * work for controls?
 */
PUBLIC void Window::setBackground(Color* c) 
{
    if (c != NULL) {
        mBackground = c;
        WindowUI* ui = getWindowUI();
        ui->setBackground(c);
    }
}

/**
 * Obtain a Graphics context for drawing in this window.
 * This will have a cannonical HDC that was created when the
 * window was first opened.
 */
PUBLIC Graphics* Window::getGraphics()
{
    WindowUI* ui = getWindowUI();
    return ui->getGraphics();
}

/**
 * Obtain the default TextMetrics for the window.
 * This must have been initialized by the UI when it was opened.
 */
PUBLIC TextMetrics* Window::getTextMetrics()
{
    return mTextMetrics;
}

/**
 * Only for WindowsUI
 */
PUBLIC void Window::setTextMetrics(TextMetrics* tm)
{
    delete mTextMetrics;
    mTextMetrics = tm;
}

/**
 * A more useful utility for getPreferredSize methods to call.
 */
PUBLIC void Window::getTextSize(const char *text, Font* font, Dimension* d)
{
    WindowUI* ui = getWindowUI();
    Graphics* g = ui->getGraphics();
    g->getTextSize(text, font, d);
}

PUBLIC void Window::addWindowListener(WindowListener* l) 
{
    if (mWindowListeners == NULL)
	  mWindowListeners = new Listeners();
	mWindowListeners->addListener(l);
}

PUBLIC void Window::removeWindowListener(WindowListener* l) 
{
    if (mWindowListeners != NULL)
	  mWindowListeners->removeListener(l);
}

void Window::fireWindowEvent(WindowEvent* e)
{
	if (mWindowListeners != NULL)
	  mWindowListeners->fireWindowEvent(e);
}

/**
 * Open the underlying OS window.
 */
PUBLIC void Window::open() 
{
    WindowUI* ui = getWindowUI();

	// kludge, if this is a runnable window, wait till the run
	// call to open it, not sure why this was important
	// UPDATE: I don't like this on the Mac we can't open and do
	// some test drawing before we run, see if we can live without it...
	//if (!isRunnable())
    ui->open();

	// note that we do NOT call openChildren here, window initialization
	// is a complicated multi-stage process, child opening is deferred
	// to finishOpening called indirectly by the ComponentIU::open
}

/**
 * Called by the UI after it has created the native component.
 * Here we perform some further initialization and size adjustments that
 * aren't UI specific.
 * This doesn't apply to HostFrame so maybe this should be pushed down
 * into the UI?
 */
PRIVATE void Window::finishOpening()
{
    // create child controls, they will all be at 0,0 initially
    // might want to create them hidden?
    openChildren();

    // For windows, bounds always has the "client rect" which is the area
    // within the borders, title bar, and menu bar.  

    // This is the packed size of the components in the client area
	// Note that if you use an open-ended layout like FlowLayout this
	// could be unbounded
    Dimension* ps = getPreferredSize(this);

    // if width/height weren't specified, auto-adjust
    // this is similar to "pack", do we really need this??

    if (!isMaximized() &&
        (isAutoSize() || mBounds.width <= 0 || mBounds.height <= 0)) {

		// if the window started out with a larger size, use it
		if (ps->width > mBounds.width)
		  mBounds.width = ps->width;

		if (ps->height > mBounds.height)
		  mBounds.height = ps->height;

		//printf("Window bounds adjusted to %d %d\n", 
        //mBounds.width, mBounds.height);

        updateNativeBounds();
    }

    // auto-center if requested
    // for popups, proably want to center over the parent not the screen?
	// !! on Mac the menu is always at the top so the Y coord needs to 
    // be offset by the menu height, should have been done during initial
    // sizing?
    WindowUI* ui = getWindowUI();
    if (!isMaximized() && !ui->isChild() && isAutoCenter())
      center();

    // finally run the layout managers
	//printf("Layout components\n");
    layout(this);

    // don't really need this any more, we get trace during layout
	bool traceLayout = false;
	if (traceLayout) {
		printf("Layout after opening:\n");
		initTraceLevel();
		debug();
	}

    // assign tab order
    assignTabOrder(this);

    // Have to wait for the HWND handles before we can
    // set focus.
    Component* c = findFocusedComponent(this);
    if (c != NULL) {
        c->setFocus();
        c->setFocusRequested(false);    // clear or persist?
    }   
    else {
        // always auto-select the first focusable component?
        setFocus(0);
    }

	// should we paint the lightweight components now or
	// wait until after ShowWindow on the mac?

}

PUBLIC void Window::center()
{
	int syswidth = UIManager::getScreenWidth();
	int sysheight = UIManager::getScreenHeight();
	int centerx = (syswidth - mBounds.width) / 2;
	int centery = (sysheight - mBounds.height) / 2;

	if (centerx > 0)
	  mBounds.x = centerx;
	if (centery > 0)
	  mBounds.y = centery;

	updateNativeBounds();
}

PRIVATE Component* Window::findFocusedComponent(Component* c)
{
    Component* found = NULL;

    if (c->isFocusable())
      found = c;
    else {
        Container* cnt = c->isContainer();
        if (cnt != NULL) {
            for (c = cnt->getComponents() ; c != NULL && found == NULL ;
                 c = c->getNext())
              found = findFocusedComponent(c);
        }
    }
    return found;
}

/**
 * Traverse the hiearchy of components in this window and build
 * a list of components to receive focus when the tab key is pressed.
 */
PRIVATE void Window::assignTabOrder(Component* c)
{
	if (c->isFocusRequested()) {
		if (mFocusables == NULL)
		  mFocusables = new List();
		mFocusables->add(c);
	}

	Container* cont = c->isContainer();
	if (cont != NULL) {
		for (c = cont->getComponents() ; c != NULL ; c = c->getNext())
		  assignTabOrder(c);
	}
}

/**
 * Set focus to one of the focusable components.
 */
PRIVATE void Window::setFocus(int i)
{
	if (mFocusables != NULL) {
		// wrap around both ways
		// !! shouldn't we include the root window in the list?
		int max = mFocusables->size();
		if (i < 0)
		  i = max;
		else if (i >= max)
		  i = 0;
		mFocus = i;
		Component *c = (Component*)mFocusables->get(i);
		if (c != NULL)
		  c->setFocus();
	}
	else {
		// always focus the root window for lightweight 
		// components
		Component::setFocus();
	}
}

PUBLIC void Window::incFocus(int delta)
{
	setFocus(mFocus + delta);
}

/**
 * Open the native handles and enter a message loop.
 */
PUBLIC int Window::run() 
{
	int result = 0;

    WindowUI* ui = getWindowUI();
    result = ui->run();

    return result;
}

/**
 * Called when the window is about to be closed.
 * You can't stop it now!
 */
PUBLIC void Window::closing()
{
}

/**
 * Called after the window has been fully opened.
 */
PUBLIC void Window::opened()
{
}

/**
 * Called as events come in that change the window bounds.
 * UI is expected to capture the new bounds and call the layout managers.
 * !! Does this really need to be pushed into the UI?  Windows
 * does some odd stuff with TextMetrics but Mac just calls the layout
 * managers.
 */
PUBLIC void Window::relayout()
{
    WindowUI* ui = getWindowUI();
    ui->relayout();
}

PUBLIC void Window::dumpLocal(int i)
{
    indent(i);
    fprintf(stdout, "Window: %d %d %d %d\n", 
            mBounds.x, mBounds.y, mBounds.width, mBounds.height);
}

/**
 * Return true if the native window is open.  Added so we can tell
 * whether non-modal dialogs have closed themselves.
 */
PUBLIC bool Window::isOpen()
{
    WindowUI* ui = getWindowUI();
    return ui->isOpen();
}

PUBLIC const char* Window::getTraceName()
{
	return (mTitle != NULL) ? mTitle : getName();
}

QWIN_END_NAMESPACE

/****************************************************************************/
/****************************************************************************
 *                                                                          *
 *                                 WINDOWS UI                               *
 *                                                                          *
 ****************************************************************************/
/****************************************************************************/

#ifdef _WIN32

// not sure what the right way to set this is...
// but need it for WM_MOUSEWHEEL
//#define _WIN32_WINNT 0x0400
#define WM_MOUSEWHEEL                   0x020A
#define WHEEL_DELTA                     120
#define GET_WHEEL_DELTA_WPARAM(wParam)  ((short)HIWORD(wParam))
#define GET_KEYSTATE_WPARAM(wParam)     (LOWORD(wParam))

#include <windows.h>
// for WINDOWINFO, doesn't work without APPVER=5.0
//#include <winuser.h>
#include <commctrl.h>
#include "UIWindows.h"


// until we can find way to get APPVER=5.0 without the old
// include files whining, just define what we need

/*
 * Window information snapshot
 */
typedef struct tagWINDOWINFO
{
    DWORD cbSize;
    RECT  rcWindow;
    RECT  rcClient;
    DWORD dwStyle;
    DWORD dwExStyle;
    DWORD dwOtherStuff;
    UINT  cxWindowBorders;
    UINT  cyWindowBorders;
    ATOM  atomWindowType;
    WORD  wCreatorVersion;
} WINDOWINFO, *PWINDOWINFO, *LPWINDOWINFO;

BOOL WINAPI
GetWindowInfo(
    HWND hwnd,
    PWINDOWINFO pwi
);

/**
 * Maximum number of extended "user" messages we define.
 */
#define MAX_USER_MESSAGES 32

QWIN_BEGIN_NAMESPACE

/****************************************************************************
 *                                                                          *
 *                                  WINDOWUI                                *
 *                                                                          *
 ****************************************************************************/

bool WindowsWindow::ClassesRegistered = false;

PUBLIC WindowsWindow::WindowsWindow(Window* win)
{
    mWindow = win;

	mAccel = NULL;
    mToolTip = NULL;
    mWindowEvent = new WindowEvent();
    mMouseEvent = new MouseEvent();
    mKeyEvent = new KeyEvent();
    mGraphics = NULL;
	mEventGraphics = NULL;
    mDeviceContext = NULL;
    mDragComponent = NULL;
	mChild = false;
    mFuckingClientTopOffset = 0;
    mFuckingClientLeftOffset = 0;
}

PUBLIC WindowsWindow::~WindowsWindow() 
{
    // shouldn't still have a handlemake sure 
    (void)SetWindowLong(mHandle, GWL_USERDATA, (LONG)NULL);

    delete mWindowEvent;
    delete mMouseEvent;
    delete mKeyEvent;
    delete mGraphics;
    delete mEventGraphics;
}

PUBLIC WindowsContext* WindowsWindow::getContext()
{
    return (WindowsContext*)mWindow->getContext();
}

PUBLIC Graphics* WindowsWindow::getGraphics()
{
    return mGraphics;
}

PUBLIC bool WindowsWindow::isChild() 
{
	return mChild;
}

/**
 * Capture the actual location and size of the native window,
 * used both after creation and after a resize/move.
 */
PRIVATE void WindowsWindow::captureNativeBounds(bool warn)
{
    if (mHandle != NULL) {
        
        // This should work but we can't get to it without old
        // platform SDK headers.  Hmm, not fixed after VStudio 2005 
        // upgrade either.
        // Fuck it, just remember the damn offsets when the window
        // was opened.
        /*
        RECT* client;
        WINDOWINFO info;
        info.cbSize = sizeof(WINDOWINFO);
        GetWindowInfo(mHandle, &info);
        client = &(info.rcClient);

        int cleft = client->left;
        int ctop = client->top;
        int cwidth = client->right - client->left + 1;
        int cheight = client->bottom - client->top + 1;
        */

        RECT r;
        GetWindowRect(mHandle, &r);
        int left = r.left + mFuckingClientLeftOffset;
        int top = r.top + mFuckingClientTopOffset;

        // botth/right is the "exclusive" pixel so don't +1
        GetClientRect(mHandle, &r);
        int width = r.right - r.left;
        int height = r.bottom - r.top;

        // if we're a child window, don't pay attention to the 
        // real origin since we're relative to the parent
		if (mChild) {
			left = 0;
            top = 0;
		}

        Bounds* b = mWindow->getBounds();

        if (warn) {
            // If the initial value was uninitialized (zero) don't warn
            // this is common for dialogs.  Technicaly it could have
            // been initialized to zero but that's uncommon.

            if (b->x != 0 && b->x != left)
              printf("WARNING: captureNativeBounds x %d -> %d\n", b->x, left);
            if (b->y != 0 && b->y != top)
              printf("WARNING: captureNativeBounds y %d -> %d\n", b->y, top);
            if (b->width != 0 && b->width != width)
              printf("WARNING: captureNativeBounds width %d -> %d\n",
                     b->width, width);
            if (b->height != 0 && b->height != height)
              printf("WARNING: captureNativeBounds height %d -> %d\n",
                     b->height, height);
        }

        b->x = left;
        b->y = top;
        b->width = width;
        b->height = height;
    }
}

/**
 * Override the Component setBackground method and convert
 * it to a native window property.  This doesn't appear to 
 * work for controls?
 */
PUBLIC void WindowsWindow::setBackground(Color* c) 
{
    if (c != NULL && mHandle != NULL) {
        WindowsColor* wc = (WindowsColor*)c->getNativeColor();
        HBRUSH current = (HBRUSH)
            SetClassLong(mHandle, GCL_HBRBACKGROUND, (long)wc->getBrush());

        // Petzold would call DeleteObject on the previous brush here,
        // need to be much more rigerous about the ownership of these

        mWindow->invalidate();
    }
}

PUBLIC HINSTANCE WindowsWindow::getInstance()
{
	HINSTANCE inst = NULL;

	WindowsContext *context = getContext();
	if (context != NULL)
	  inst = context->getInstance();
	return inst;
}

/**
 * Virtual method to return the native handle of the parent window.  
 * HostFrame overloads this to return the host window handle.
 * can have a non-null parent.
 * 
 * The only case where a Window can have a parent is if it's a Dialog.
 * We'll walk up one and return the native handle.
 */
PUBLIC HWND WindowsWindow::getParentWindowHandle()
{
	HWND handle = NULL;

	Component* parent = mWindow->getParent();
	if (parent != NULL)
	  handle = (HWND)parent->getNativeHandle();

	return handle;
}

/**
 * Since we started positioning the window based on the client rect
 * we need to be careful of older coordinates stored with the 
 * window rect.  This is the minimum Y coordinate necessary
 * to keep the window title bar in view for dragging.
 * This also assumes there will be a menu bar (which is 20).
 */
#define WINDOW_MIN_TOP 50

/**
 * Open the underlying OS window.
 *
 * If the parent window is NULL, then the window is created as
 * a normal "overlapped" window on the desktop.  The left/top are
 * screen coordinates.
 *
 * If the parent window is non-NULL, the left/top are relative to the
 * parent window's client area.
 *
 * Docs say that CreateWindow needs an HINSTANCE only for
 * 95/98/Me, it is ignored in NT/200/XP.
 *
 */
PUBLIC void WindowsWindow::open() 
{
	if (mHandle != NULL) {
		// already open, bring it to the front?
	}
	else {
		WindowsContext *context = getContext();
		HINSTANCE instance = context->getInstance();
		const char* icon = mWindow->getIcon();
		Bounds* bounds = mWindow->getBounds();
		DWORD style = 0;
		int left = bounds->x;
		int top = bounds->y;
		int width = bounds->width;
		int height = bounds->height;
		HMENU menu = NULL;
		int showMode = context->getShowMode();

		// make sure classes are registered
		registerClasses(mWindow->getIcon());

		// load the accelerator resource for later
		const char* accel = mWindow->getAccelerators();
		if (accel != NULL) {
			mAccel = LoadAccelerators(instance, accel);
			if (mAccel == NULL)
			  printf("Unable to load accelerators '%s'\n", accel);
		}

		const char* wclass = mWindow->getClass();
		if (wclass == NULL) {
			// if no parent assume root frame, otherwise dialog
			if (mWindow->getParent() == NULL)
			  wclass = FRAME_WINDOW_CLASS;
			else
			  wclass = DIALOG_WINDOW_CLASS;
		}

		// style derives from class, may want to be more flexible 
		if (!strcmp(wclass, FRAME_WINDOW_CLASS))
		  style = WS_OVERLAPPEDWINDOW;

		else if (!strcmp(wclass, CHILD_WINDOW_CLASS)) {
			mChild = true;
			style = WS_CHILD | WS_VISIBLE;
		}
		else if (!strcmp(wclass, ALERT_WINDOW_CLASS)) {
			style = WS_POPUP;
		}
		else {
			// default dialog frame
            // Docs are unclear on exactly what WS_POPUP does but this
            // in combination with a parent handle appears to force
            // the popup to always display above the parent.
            // SYSMENU is necessary to get an X box, but you also
            // get a system menu.
            // CAPTION gets you a title bar
            // SIZEBOX makes the border a little thicker and allows the window
            // to be resized.
			style = WS_POPUP | WS_CAPTION | WS_SIZEBOX | WS_SYSMENU;
		}
		
		if (mWindow->isMaximized())
		  style |= WS_MAXIMIZE;

		// always want this?
		//style |= WS_HSCROLL | WS_VSCROLL;

        // Bounds represents the size of the client region, 
        // have to adjust this to include the surrounding componentry
        // when opening the window.
		RECT rect;
		rect.left 	= left;
		rect.top	= top;
        // note: GetWindowRect will return "exclusive" right bottom 
        // so we don't need the usual -1 adjust
		rect.right 	= left + width;
		rect.bottom	= top + height;
        // third arg are _EX_ styles which we don't use
        bool hasMenu = (mWindow->getMenuBar() != NULL);
        // not necessary if this is a child window, but it should
        // figure that out and do no adjustment
		if (!AdjustWindowRectEx(&rect, style, NULL, hasMenu))
		  printf("WindowsWindow::open: Unable to adjust window coordinates!\n"); 
        // NOTE: How fucking hard is it to get the client region origin
        // after opening?  Can't use AdjustWindowRectEx any more because
        // we don't have the style (I guess we could keep calculating it).
        // GetWindowInfo is supposed to work but I can't get to it
        // with my crappy old platform SDK headers.  Just rember the offset 
        // we calculated here and use it later.
        mFuckingClientLeftOffset = left - rect.left;
        mFuckingClientTopOffset = top - rect.top;
        
        left = rect.left;
        top = rect.top;
        // again right/bottom has the "exclusive" edge so don't 
        // need the usual 1+ adjust
		width = rect.right - rect.left;
		height = rect.bottom - rect.top;

        // AdjustWindowRectEx does not appear to factor the height
        // of the menu bar out of the client area.  If you use this size
        // then immediately call GetClientRect it will be too short.
        // This is the height of a single line bar, not sure what we'd do
        // if we had a multi-line bar
		MenuBar* mb = mWindow->getMenuBar();
        if (mb != NULL) {
            int menuHeight = GetSystemMetrics(SM_CYMENU);
            top -= menuHeight;
            height += menuHeight;
            mFuckingClientTopOffset += menuHeight;
        }

        // push the window down if necessary so the title bar is visible
        if (top < 0) top = 0;
        if (left < 0) left = 0;

		if (mb != NULL) {
            // interface to this one is a little odd because we have
            // to get the handle
            mb->open();
            // ugh, do the proxy dance
            ComponentUI* ui = mb->getUI();
            WindowsMenuItem* mui = (WindowsMenuItem*)ui->getNative();
            menu = mui->getMenuHandle();
        }

		// left, top, width, height can also be CW_USEDEFAULT
		// last arg can be a CREATESTRUCT to pass state into
		// the WM_CREATE event that will be called

		//char msg[1024 * 2];
		//sprintf(msg, "CreateWindow: class=%s title=%s parent=%p menu=%p instance=%p\n", wclass, mWindow->getTitle(), getParentWindowHandle(), menu, instance);
		//Trace(1, msg);

        // should only be non-NULL if mChild is true
		HWND parent = getParentWindowHandle();
		const char* title = mWindow->getTitle();

		mHandle = CreateWindowEx(0,
								 wclass,
								 title,
								 style,
								 left,
								 top,
								 width,
								 height,
								 parent,
								 menu,
								 NULL,		// instance, ignored on XP
								 NULL);		// creation parameters
		

		if (mHandle == 0) {
			printf("WindowsWindow::open: Unable to open window!\n");
		}  
		else {
			// Store our little extension wart in the client window
			// supposed to use SetWindowLongPtr, but can't seem to find it
			(void)SetWindowLong(mHandle, GWL_USERDATA, (LONG)this);

			// capture the actual bounds
            // ?? this should be the same as mBounds unless we asked
            // for something too big?

			captureNativeBounds(true);

			// Docs indiciate that you can call this, but it doesn't
			// always seem to work.  Works for frames but not dialogs?
			// Note that this will change the icon in the window CLASS
			// and therefore affect all future windows created with this class
			// We're doing it down in class registration instead.

			if (icon != NULL) {
				HICON hicon = LoadIcon(instance, icon);
				if (hicon) {
					SetClassLong(mHandle, GCL_HICON, (LONG)hicon);
					SetClassLong(mHandle, GCL_HICONSM, (LONG)hicon);
				}
				else
				  printf("Couldn't load icon %s\n", icon);
			}

			// now that we have a window, flesh out the child components
            // since it will be used so often, save the TEXTMETRIC and
			// a device context in case the components want them
            mDeviceContext = GetDC(mHandle);
            if (mDeviceContext == NULL)
                printf("Unable to get initial DC\n");
            else {
				// keep this around for use outside event handlers
				mGraphics = new WindowsGraphics(mDeviceContext);

				// this one is used by event handlers, the HDC will change
				mEventGraphics = new WindowsGraphics();
				
                // get a copy of the default text metrics 
                //SelectObject(dc, GetStockObject(SYSTEM_FIXED_FONT));
                //GetTextMetrics(mDeviceContext, &mTextMetric);
                WindowsTextMetrics* tm = new WindowsTextMetrics();
                tm->init(mDeviceContext);
                mWindow->setTextMetrics(tm);

                setBackground(mWindow->getBackground());

                // now that we have a handle, can call back up to Window
                // to do some additional layout and sizing adjustments
                mWindow->finishOpening();

                //mWindow->dump(0);

                // setup initial tool tips, will need to do this
                // after every layout !
                setupToolTips();
				
				// Keep this around at all times so we can draw
				// outside of the WM_PAINT message, this allows you to 
				// do things that aren't constent with Swing, but convenient
                //ReleaseDC(mHandle, mDeviceContext);
				//mDeviceContext = NULL;
            }

			// in old code, we would define the menu here

            // It was kept invisible until after we had a chance to pack it
            // now display it
			ShowWindow(mHandle, SW_SHOWNORMAL);
    
			// This call will cause the WM_PAINT message to be sent
			InvalidateRgn(mHandle, NULL, 0);
			UpdateWindow(mHandle);

			// two styles, an event and an overload,
			// don't really need both!!
			mWindow->opened();
			mWindowEvent->setId(WINDOW_EVENT_OPENED);
			mWindow->fireWindowEvent(mWindowEvent);
		}
	}
}

/**
 * Eventually called whenever the origin or size changes.
 * Since we are always dealing with the client region, 
 * have to adjust to the outer bounds.
 */
void WindowsWindow::updateNativeBounds(Bounds* neu) 
{
    if (mHandle != NULL) {
        
        // get current bounds of the entire window
		RECT wrect;
        GetWindowRect(mHandle, &wrect);

        int wleft = wrect.left;
        int wtop = wrect.top;
		int wwidth = wrect.right - wrect.left + 1;
		int wheight = wrect.bottom - wrect.top + 1;

        // and the client, Component::getBounds already has "neu"?
		RECT crect;
        GetClientRect(mHandle, &crect);
        
        // client rect origin always seems to be zero, 
        // actual origin remembered here
        int cleft = wleft + mFuckingClientLeftOffset;
        int ctop = wtop + mFuckingClientTopOffset;
		int cwidth = crect.right - crect.left + 1;
		int cheight = crect.bottom - crect.top + 1;

        int adjustLeft = cleft - wleft;
        int adjustTop = ctop - wtop;
        int adjustWidth = wwidth - cwidth;
        int adjustHeight = wheight - cheight;

        int newLeft = neu->x - adjustLeft;
        int newTop = neu->y - adjustTop;
        int newWidth = neu->width + adjustWidth;
        int newHeight = neu->height + adjustHeight;

        MoveWindow(mHandle, newLeft, newTop, newWidth, newHeight, true);
    }
}

/**
 * Bring the window to the front and restore if minimized.
 * Hmm, if the window is minimized I'd like to restore it but if
 * maximized just bring it to the front and leave it maximized.
 * There doesn't seem to be a ShowWindow argument to do that?
 */
PRIVATE void WindowsWindow::toFront()
{
	if (mHandle != NULL) {
		ShowWindow(mHandle, SW_SHOW);
	}
}

/**
 * After layout, whip through the components registring any that
 * have tool tips.  This will need to be smarter about things
 * that are actually visible.
 */
PRIVATE void WindowsWindow::setupToolTips()
{
    if (mToolTip == NULL) {
        mToolTip = CreateWindowEx(WS_EX_TOPMOST,
                                  TOOLTIPS_CLASS,
                                  NULL,
                                  WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
                                  CW_USEDEFAULT,
                                  CW_USEDEFAULT,
                                  CW_USEDEFAULT,
                                  CW_USEDEFAULT,
                                  mHandle,
                                  NULL,
                                  getContext()->getInstance(),
                                  NULL
            );
        
        SetWindowPos(mToolTip, HWND_TOPMOST,
                     0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }

    setupToolTips(mWindow);
}

PRIVATE void WindowsWindow::setupToolTips(Component* c)
{
	HWND chandle = getHandle(c);
    const char *tip = c->getToolTip();
    if (chandle != NULL && tip != NULL) {
        TOOLINFO info;
        info.cbSize = sizeof(info);
        info.uFlags = TTF_SUBCLASS | TTF_IDISHWND;
        info.hwnd = mHandle;
        info.uId = (LONG)chandle;
        info.hinst = getContext()->getInstance();
        info.lpszText = (char*)tip;
        info.lParam = NULL;
        GetWindowRect(chandle, &info.rect);
        SendMessage(mToolTip, TTM_ADDTOOL, 0, (LONG)&info);
    }

    Container* container = c->isContainer();
    if (container != NULL) {
        for (Component* child = container->getComponents() ; child != NULL ; 
             child = child->getNext())
          setupToolTips(child);
    }
}

/**
 * Close the window by sending it a message.
 * When we close programatically have to turn off mNoClose
 * 
 * This seems to process the WM_CLOSE and WM_DESTROY messages synchronously.
 * !! It's very important that it does because we're typicallky doing
 * to be deleting the Qwin objects.
 */
PUBLIC void WindowsWindow::close()
{
	if (mHandle != NULL) {
		mWindow->setNoClose(false);
		SendMessage(mHandle, WM_CLOSE, 0, 0L);

        // assuming WM_DESTROY was called synchronously this should
        // already be NULL
        if (mHandle != NULL) {
            Trace(1, "WM_CLOSE not processed synchronously!\n");
            mHandle = NULL;
        }
    }
}

/**
 * Enter into a basic message loop.
 */
PUBLIC int WindowsWindow::run() 
{
	int result = 0;

	if (mHandle == NULL)
	  open();

	if (mHandle == NULL) {
		printf("Dialog::run Unable to open window\n");
	}
	else {
		BOOL status;
		MSG msg;

		// Set this so we know the window has been fully opened,
		// necessary for some applications like VST plugins where can
		// open and close rapidly.  Just testing the non-null handle
		// isn't enough since we may still be in the process of opening
		// our children.
		mWindow->setRunning(true);

		// may want some other extit flags here...
		// status will be 0 if the WM_QUIT is retrieved

		while ((status = GetMessage(&msg, NULL, 0, 0)) != 0) {

			if (status == -1) {
				// handle error and possibly exit
				printf("Dialog::run GetMessage error\n");
			}
			else if (mAccel == NULL ||
					 !TranslateAccelerator(mHandle, mAccel, &msg)) {

				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}

		// though GetMessage returned 0, there will still be a valid
		// WM_QUIT in MSG, return its paramater

		result = msg.wParam;

		// the handle is invalid at this point, don't try to use it again
		mHandle = NULL;
	}

	return result;
}

/**
 * Default message handler.
 * In the old days, this would call out to registered handler functions,
 * here we could use subclassing?
 */
PUBLIC long WindowsWindow::messageHandler(UINT msg, WPARAM wparam, LPARAM lparam)
{
	long result = 0;
	bool handled = false;

    //char buf[128];
    //sprintf(buf, "Msg %d\n", msg);
    //OutputDebugString(buf);

	switch (msg) {

    case WM_CREATE: {
        // wparam is unused, lparam has a CREATESTRUCT
        // among other things this includes the position and size
        // of the window (client area?) and all of the stuff
        // passed to CreateWindowEx

		// invalidate the client area so we force a WM_PAINT
        InvalidateRect(mHandle, NULL, TRUE);

		// !! when is a good time to call WindowListener.windowOpened
		// this looks too early

    }
    break;

    case WM_PAINT: {
        // Call our root refresh handler.
        // Need to be paying attention to the invalid rectangle 
        // in ps.rcPaintfor efficiency.

        PAINTSTRUCT ps;
        HDC dc = BeginPaint(mHandle, &ps);

        mEventGraphics->setDeviceContext(dc);

        mWindow->paint(mEventGraphics);

        // formerly called a class-specific refresh method here

        EndPaint(mHandle, &ps);
        handled = true;
    }
    break;

    case WM_MEASUREITEM: {
        // here for OWNERDRAW Listboxes and Comboboxes
        // assume for now the items will all be the same size
        // could add a Component method if we needed to
    }
    break;

    case WM_DRAWITEM: {
        // can be here for OwnerDraw buttons, any others?

        LPDRAWITEMSTRUCT di = (LPDRAWITEMSTRUCT)lparam;
        // handle to the control, or parent menu for menu items
        HWND win = di->hwndItem;
        WindowsComponent* ui = (WindowsComponent*)
            GetWindowLong(win, GWL_USERDATA);

        if (ui != NULL) {
            Component* c = ui->getComponent();
            mEventGraphics->setDeviceContext(di->hDC);
            mEventGraphics->setDrawItem(di);
            c->paintBorder(mEventGraphics);
            c->paint(mEventGraphics);
            mEventGraphics->setDrawItem(NULL);
            handled = true;
        }
    }
    break;

    case WM_COMMAND: {
        // A message from a menu, control, or a key that has been 
        // mapped through the accellerator table.
        // lparam has child window handle for control events.
        // id will be the child window id for controls.
        // code will be non-zero for controls.
        // If the message is from an accellerator the code is 1, if
        // its from a menu, the code is zero.

        HWND control = (HWND)lparam;
        int code = HIWORD(wparam);
        int id = LOWORD(wparam);
        WindowsComponent* ui = (WindowsComponent*)
            GetWindowLong(control, GWL_USERDATA);

        if (ui != NULL)
          ui->command(code); // need to pass lparam?
        else {
            // Addume its a menu, code has the menu item id.
            // We're not creating menus with NOTIFYBYPOS so we have
            // to search all the menus for a matching id.  Not bad,
            // but it means that all menus, bar and popups, have to 
            // have unique ids.
            //
            // OLD COMMENTS:
            // Code will be 1 if this was caused by an accelerator
            // rather than a menu item.  This should happen only
            // for menus built from resource files.

			// hmm, we seem to get a LOT of WM_COMMAND messages with id zero,
			// that aren't from menus, filter those 
			if (id > 0) {
                boolean found = false;
                MenuBar* bar = mWindow->getMenuBar();
                if (bar != NULL)
                  found = bar->fireSelectionId(id);
                if (!found) {
                    PopupMenu* popup = mWindow->getPopupMenu();
                    if (popup != NULL)
                      found = popup->fireSelectionId(id);
                }
            }
        }

        // should we prevent DefWndProc from being called?
    }
    break;

    // Why aren't we using this any more?
    // the way menus are being created now, we'll end up in WM_COMMAND
    // with an ID and have to guess that it came from a menu and search
    // for an item by id.  I guess that's okay but if you have multiple
    // popup menus you have to make sure the items all have unique ids.
#if 0
    case WM_MENUCOMMAND: {
        // A menu selection from a menu built with the MNS_NOTIFYBYPOS option
        HMENU menu = (HMENU)lparam;
        int index = wparam;

        MENUINFO minfo;
        minfo.cbSize = sizeof(minfo);
        minfo.fMask = MIM_MENUDATA;
        GetMenuInfo(menu, &minfo);
        Menu* mymenu = (Menu*)minfo.dwMenuData;
        if (mymenu == NULL) 
            printf("Unable to determine Menu object!\n");
        else {
			// don't actually have this, but could if necessary
            mymenu->fireSelection(index);
		}
    }
    break;
#endif

    case WM_NOTIFY: {
        // this is what the newer common controls send
        // in theory NMHDR can be larger than this and contain
        // extended information, if so the whole thing should
        // be passed to the Component, for now assume that
        // its just a command code
        int id = wparam;    // don't really need this
        NMHDR* info = (NMHDR*)lparam;
        HWND control = info->hwndFrom;
        WindowsComponent* ui = (WindowsComponent*)
            GetWindowLong(control, GWL_USERDATA);
        if (ui != NULL)
          ui->notify(info->code);
    }
    break;

    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    case WM_LBUTTONDBLCLK:
    case WM_MBUTTONDBLCLK: 
    case WM_RBUTTONDBLCLK: 
	case WM_MOUSEMOVE: {

		int x = LOWORD(lparam);
		int y = HIWORD(lparam);
		mouseHandler(msg, wparam, x, y);
	}
	break;

	case WM_MOUSEWHEEL: {
		//int distance = HIWORD(wparam);
		//int vkeys = LOWORD(wparam);
		int x = LOWORD(lparam);
		int y = HIWORD(lparam);

		// these macros were used in one example, but they aren't defined
		// unless _WIN32_WINNT >= 0x400, need to find the right way to 
		// assert that...
		int distance = GET_WHEEL_DELTA_WPARAM(wparam);
		int vkeys = GET_KEYSTATE_WPARAM(wparam);

		// mouse position relative to the upper-left corner of the *screen*
		// vkeys can be or'd with these
		// MK_CONTROL, MK_LBUTTON, MK_RBUTTON, MK_MBUTTON,
		// MK_SHIFT, MK_XBUTTON1, MK_XBUTTON2

		// The wheel rotation will be a multiple of WHEEL_DELTA which
		// is fixed at 120, this is the threshold for an action to be taken,
		// one action should be taken for each multiple

		printf("WM_MOUSEWHEEL: x=%d y=%d rot=%d vkeys=%d\n",
			   x, y, distance, vkeys);

	}
	break;

    case WM_KEYDOWN:
    case WM_KEYUP:
		{
			// lparam has repeat count, scan code, previous state, and others
			keyHandler(msg, (int)wparam, (long)lparam);
            // don't pass this along if we're forcing it
            if (mWindow->isForcedFocus())
			  handled = true;
		}
		break;

    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
		{
			// "SYS" keys include the alt combinations that ususally control
			// menus, use the default handler to pass those along?
            // !! don't seem to be getting these while in a dialog
            // though when the dialog closes we see them in the parent
            // window, this prevents KeyConfigDialog from binding alt keys
            // though we could probably receive them in the main window
			keyHandler(msg, (int)wparam, (long)lparam);
            if (mWindow->isForcedFocus())
			  handled = true;
		}
		break;

    case WM_CHAR:
	case WM_SYSCHAR: {
			// Windows sends us one of these sandwiched between
			// the WM_KEYDOWN and WM_KEYUP mesages, be sure not to generate
			// a handler call for both KEYDOWN and CHAR, this relies on the
			// virtual key mapping table above not having entries for things
			// that will generate char events.
			// These will always be treated as "down" transitions, there
			// will be no up transition, though I suppose we could simulate
			// one by saving some state, and wanting fo KEYUP.


		// !! TODO if char is "esc" then we should call 
		// ReleaseCapture if we're dragging something

	}
	break;

	case WM_CANCELMODE: {
		// window manager detects a change that requires an application 
		// to cancel any modal state
		// !! abort drag and call ReleaseCapture
		ReleaseCapture();
	}
	break;

	case WM_CAPTURECHANGED: {
		// capture has been cleared or some other window has taken it
		// abort the drag operation, but don't need to call ReleaseCapture
	}
	break;

    case WM_ENABLE:
        // called when the enabled state of a window is changed, 
        // not sure how this differs from activation and focus
        break;

	case WM_ACTIVATE: {
        // sent when either deactivating or activating, the
        // deactivated window is informed first
        // also will receive the WM_MOUSEACTIVATE if the mouse was
        // clicked.
		if (wparam == WA_INACTIVE)
		  mWindowEvent->setId(WINDOW_EVENT_DEACTIVATED);
		else {
			// either WA_ACTIVE or WA_CLICKACTIVE
			mWindowEvent->setId(WINDOW_EVENT_ACTIVATED);
		}
		mWindow->fireWindowEvent(mWindowEvent);

	}
	break;

    case WM_SETFOCUS: {
        // The main window has gained focus
        // Petzold usually uses this to select the first input
        // control, the mFocusable stuff should do that automatically
    }
    break;
	
    case WM_KILLFOCUS: {
        // if we had a spinner, sent it OS_EVENT_ACTIVATE
	}
	break;

	case WM_ENTERSIZEMOVE:
        // called when the window begins being moved or sized
        break;

    case WM_EXITSIZEMOVE: {
        // Called when the window has finished being moved or sized
        // you will see intermediate WM_SIZE messages while
        // the window is moved or sized.  May be cases where you
        // want to defer action till the end.
        relayout();
    }
    break;

    case WM_WINDOWPOSCHANGED:
        // a change in our "z" order
        break;

    case WM_GETMINMAXINFO:
        // sent when the window is about to be resized, can be used
        // to override the default sizes
        break;

    case WM_SIZING:
        // Sent to the window continuously as the size is in progress	
        break;

    case WM_SIZE: {
        // Sent after the window size has changed.
        // If dragging the size, this will be called repeatedly
        // like WM_SIZING

        int width = LOWORD(lparam);
        int height = HIWORD(lparam);

        switch (wparam) {
        case SIZE_MAXHIDE:
            // sent to all popup windows after some other
            // window is maximized
            break;

		case SIZE_MAXIMIZED: {
            // window has been maximized
			mWindow->setMaximized(true);
			mWindow->setMinimized(false);
            relayout();

		}
		break;

        case SIZE_MAXSHOW:
            // sent to all popup windows after some other
            // window is restored to its former size
            break;

		case SIZE_MINIMIZED: {
            // window has been minimized
			mWindow->setMaximized(false);
			mWindow->setMinimized(true);
            //relayout();

			mWindowEvent->setId(WINDOW_EVENT_ICONIFIED);
			mWindow->fireWindowEvent(mWindowEvent);
		}
		break;

        case SIZE_RESTORED:
			// This is the only message sent when the size is restored
			// after a maximize (and a minimize?).
			// Unfortunately it is also sent continuously during
			// a drag resize so it's a lot like WM_SIZING
			//if (mMaximized || mMinimized) {
				relayout();
				mWindow->setMaximized(false);
				mWindow->setMinimized(false);
				//}

				//if (mWindowListeners != NULL) {
				//mWindowEvent->setId(WINDOW_EVENT_DEICONIFIED);
				//mWindowListeners->fireWindowEvent(mWindowEvent);
				//}

            break;
        default:
            break;
        }
    }
    break;

    case WM_MOVE: {
        // called repeatedly while the window is being moved
        // the position is of the upper-left corner of the client
        // area, it is in screen coordinates for overlapped & popup 
        // windows, and in parent coordinates for child windows	
        // you may wish to defer action until WM_EXITSIZEMOVE
    }
    break;

    case WM_HSCROLL:
    case WM_VSCROLL:
        if (lparam == NULL) {
            // this is a Window scroll bar, leave it alone
        }
        else {
            // this must be a scroll bar control
            WindowsScrollBar* ui = (WindowsScrollBar*)
                GetWindowLong((HWND)lparam, GWL_USERDATA);
            if (ui != NULL)
              ui->scroll(wparam);

            // prevent default behavior?
        }
        break;

    case WM_TIMER: {
        // a timer event
    }
    break;

    case WM_PRINT:
        // a request to draw ourselves on a printer device context
        break;

        
    case WM_QUIT: {
        // do we get this?
        printf("WM_QUIT\n");
    }
    break;

    case WM_CLOSE:
        // we're about to be closed, we get this before WM_DESTROY
		if (mWindow->isNoClose()) {
			Trace(2, "Ignoring WM_CLOSE message\n");
			// keep the default window proc from calling DestroyWindow
			handled = true;
		}
		else {
			mWindow->closing();
			mWindowEvent->setId(WINDOW_EVENT_CLOSING);
			mWindow->fireWindowEvent(mWindowEvent);
		}
        break;

    case WM_DESTROY: {
        // Someone is trying to close the window, possibly through
        // a system menu item, clicking X, or by calling close()
        // in response to another event (usually a Button).
        // Here Petzold calls DeleteObject for any brushes 
        // we created along the way, not sure if this is necessary

        // this is necessary to get us out of the message loop
		// !! If this is a modeless dialog box, we seem to be processed
		// in the same thread as the parent window, so posting this
		// would also kill the parent, not sure how to control that
		// I guess you would have to give each Dialog its own thread
		// NEW: For child windows (in a VST host window) we're not a dialog
		// but we also can't post quit, check for a parent window.

		Dialog* d = mWindow->isDialog();
		if ((d == NULL && !mWindow->isHostFrame()) ||
			(d != NULL && d->isModal())) {
			PostQuitMessage(0);
			handled = true;
		}

    }
    break;

	case WM_NCDESTROY: {
		// Called after WM_DESTROY and after all child windows have
		// been destroyed.  This should be the last message we're sent
		// so init the handle

		mWindowEvent->setId(WINDOW_EVENT_CLOSED);
		mWindow->fireWindowEvent(mWindowEvent);

        // Break the link between the HWND and this object so we don't
        // receive any more spurious messages
        (void)SetWindowLong(mHandle, GWL_USERDATA, (LONG)NULL);

        // must assume this is no longer valid
		mHandle = NULL;
	}
	break;

    case MM_MOM_DONE: {
        // midi input/output message complete
    }
    break;

    case WM_CTLCOLORSCROLLBAR:
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX: 
    case WM_CTLCOLORBTN:
    case WM_CTLCOLORDLG: {
        // Various events fired before a particular control 
        // is to be painted.  Can use this as a hook to change
        // a few of the display characteristics.
        // See Petzold on why COLORBTN is useless.
        HDC dc = (HDC)wparam;
        HWND win = (HWND)lparam;
        WindowsComponent* ui = (WindowsComponent*)
            GetWindowLong(win, GWL_USERDATA);
        if (ui != NULL) {
            // may return a brush
            // !! since colorHook is a UI method,just return a brush
            // rather than dancing through a Color again?

            mEventGraphics->setDeviceContext(dc);
            Color* color = ui->colorHook(mEventGraphics);
            if (color != NULL) {
                WindowsColor* wc = (WindowsColor*)color->getNativeColor();
                result = (long)wc->getBrush();
                handled = true;
            }
        }
    }
    break;

    case WM_SYSCOLORCHANGE:
        // a system color changed, supposed to locate any color
        // caches and reset them
        break;

    case WM_INITMENU: {
        // about to display menus, you may modify them here before display
        // unfortunate can't call GetMenuInfo on all OS versions, until
        // we decide what to support look the hard way
        Menu* menu = getMenu((HMENU)wparam);
        if (menu != NULL)
          menu->opening();
    }
    break;

    case WM_MENUSELECT:
        // the mouse is roving over menu items, use this to display
        // help text in a status bar or make an annoying noise
        break;

    default:
        // We convert messages starting with WM_USER into 
        // our internally registered messages.
        // a "user" message with these range
        // 0 to WM_USER - 1	reserved for Windows
        // WM_USER to 0x7FFF integer message for private window classes
        // 0x8000 to 0xBFFF reserved for future Windows
        // 0xC000 to 0xFFFF string messages for applications
        // > 0xFFFF reserved for future Windows

        if (msg >= WM_USER && msg < WM_USER + MAX_USER_MESSAGES) {
            int index = msg - WM_USER;
				
            // formerly called through a table of function pointers
            // with the message index
        }
        break;
	}
    
	if (!handled) {
        // supposed to call DefDlgProc if this is a dialog, but
        // that causes all sorts of problems, the window doesn't 
        // move and can't be closed
        //if (mWindow->isDialog())
        //result = DefDlgProc(mHandle, msg, wparam, lparam);
        //        else
        // We're not really creating true dialogs, just floating
        // child windows.
        result = DefWindowProc(mHandle, msg, wparam, lparam);
    }

	return result;
}

/**
 * Look for a Menu object corresponding to a native menu handle.
 */
PUBLIC Menu* WindowsWindow::getMenu(HMENU handle)
{
    Menu* menu = NULL;
	MenuBar* bar = mWindow->getMenuBar();
	PopupMenu* pop = mWindow->getPopupMenu();

    // ugh, the proxy dance
    if (bar != NULL) {
        ComponentUI* ui = bar->getUI();
        if (ui != NULL) {
            WindowsMenuItem* wmi = (WindowsMenuItem*)ui->getNative();
            menu = wmi->findMenu(handle);
        }
    }

    if (menu == NULL && pop != NULL) {
        ComponentUI* ui = pop->getUI();
        if (ui != NULL) {
            WindowsMenuItem* wmi = (WindowsMenuItem*)ui->getNative();
            menu = wmi->findMenu(handle);
        }
    }

    return menu;
}

PUBLIC void WindowsWindow::mouseHandler(int msg, int keys, int x, int y) 
{
    bool dragStart = false;
    bool dragEnd = false;

	// reuse this to void heap churn
	MouseEvent* e = mMouseEvent;
	e->init(0, MOUSE_EVENT_NOBUTTON, x, y);

	int mods = 0;
	if (keys & MK_CONTROL) mods |= KEY_MOD_CONTROL;
	if (keys & MK_SHIFT) mods |= KEY_MOD_SHIFT;
	// this is how you must test the alt key
	if (GetKeyState(VK_MENU) < 0) mods |= KEY_MOD_ALT;

	// not passing the Window modifier which is just as well since
	// it tends to be captured by Windows anyway

	e->setModifiers(mods);

	switch (msg) {
		case WM_MOUSEMOVE:
			e->setType(MOUSE_EVENT_MOVED);
			break;

		case WM_LBUTTONDOWN:
			e->setType(MOUSE_EVENT_PRESSED);
			e->setButton(MOUSE_EVENT_BUTTON1);
            dragStart = true;
			break;

		case WM_LBUTTONUP:
			e->setType(MOUSE_EVENT_RELEASED);
			e->setButton(MOUSE_EVENT_BUTTON1);
            dragEnd = true;
			break;

		case WM_MBUTTONDOWN:
			e->setType(MOUSE_EVENT_PRESSED);
			e->setButton(MOUSE_EVENT_BUTTON2);
			break;

		case WM_MBUTTONUP:
			e->setType(MOUSE_EVENT_RELEASED);
			e->setButton(MOUSE_EVENT_BUTTON2);
			break;

		case WM_RBUTTONDOWN:
			e->setType(MOUSE_EVENT_PRESSED);
			e->setButton(MOUSE_EVENT_BUTTON3);
			break;

		case WM_RBUTTONUP:
			e->setType(MOUSE_EVENT_RELEASED);
			e->setButton(MOUSE_EVENT_BUTTON3);
			break;

		case WM_LBUTTONDBLCLK:
			e->setType(MOUSE_EVENT_CLICKED);
			e->setClickCount(2);
			e->setButton(MOUSE_EVENT_BUTTON1);
			break;

		case WM_MBUTTONDBLCLK:
			e->setType(MOUSE_EVENT_CLICKED);
			e->setClickCount(2);
			e->setButton(MOUSE_EVENT_BUTTON2);
			break;

		case WM_RBUTTONDBLCLK:
			e->setType(MOUSE_EVENT_CLICKED);
			e->setClickCount(2);
			e->setButton(MOUSE_EVENT_BUTTON3);
			break;
	}

    // Mouse handlers for dragging may want to draw, so
    // have this ready.  
	// This shouldn't  be necessary now that we keep a DC around all the time
    bool allocatedContext = false;
    if (mDeviceContext == NULL) {
        mDeviceContext = GetDC(mHandle);
        allocatedContext = true;
    }

    // if we have a drag component and the mouse moved, 
    // send it a drag event, could avoid this if we could
    // pass mDragComponent down through fireMouseEvent
	// though it isn't Swing, also send a MOUSE_RELEASED so
	// we can maintain reliable start/end state

    if (mDragComponent != NULL && 
		(e->getType() == MOUSE_EVENT_MOVED ||
		 e->getType() == MOUSE_EVENT_RELEASED)) {

		if (e->getType() == MOUSE_EVENT_MOVED)
		  e->setType(MOUSE_EVENT_DRAGGED);

        // have to make these relative to the component
        Bounds b;
        mDragComponent->getNativeBounds(&b);
        e->setX(e->getX() - b.x);
        e->setY(e->getY() - b.y);
        mDragComponent->fireMouseEvent(e);
    }   
    else {
        Component* handler = mWindow->fireMouseEvent(e);
        // remember the component that handled the button press
        if (dragStart) {
			SetCapture(mHandle);
			mDragComponent = handler;
		}
    }

	if (dragEnd) {
		ReleaseCapture();
		mDragComponent = NULL;
	}

    if (allocatedContext) {
        ReleaseDC(mHandle, mDeviceContext);
        mDeviceContext = NULL;
    }

    // !! how can we support this reliably, handled just says that
	// something had a handler, not that it was interested
	// in the right mouse button

    if (msg == WM_RBUTTONDOWN) {
		PopupMenu* popup = mWindow->getPopupMenu();
        if (popup != NULL)
		  popup->open(mWindow, x, y);
    }   
}

PUBLIC void WindowsWindow::keyHandler(int msg, int key, long status)
{
	// reuse this to void heap churn
	KeyEvent* e = mKeyEvent;

    // merge selected key states with the code
    // these return a short, if the high order bit is 1, the key
    // is down, if the low-order bit is 1, the key is toggled
    // (e.g. the caps lock key)

    int modifiers = 0;
    if (GetKeyState(VK_SHIFT) & 0x8000)
      modifiers |= KEY_MOD_SHIFT;
    
    if (GetKeyState(VK_CONTROL) & 0x8000)
      modifiers |= KEY_MOD_CONTROL;

    if (GetKeyState(VK_MENU) & 0x8000)
      modifiers |= KEY_MOD_ALT;
	
	e->init(modifiers, key);

	if (msg == WM_KEYUP)
	  e->setType(KEY_EVENT_UP);
	else if (msg == WM_KEYDOWN)
	  e->setType(KEY_EVENT_DOWN);

	e->setRepeatCount(status & 0xFF);

	mWindow->fireKeyEvent(e);
}

/**
 * Called as events come in that change the window bounds.
 */
PUBLIC void WindowsWindow::relayout()
{
	if (mHandle != NULL) {

		captureNativeBounds(false);

		// !! this can happen when the window has moved but not changed
		// size need to detect this and ignore.
		// Actually, I don't see where we're ever capturing the new
		// native location so this will never change anything???

		if (mDeviceContext == NULL)
		  mDeviceContext = GetDC(mHandle);
        WindowsTextMetrics* tm = (WindowsTextMetrics*)mWindow->getTextMetrics();
        tm->init(mDeviceContext);
		mWindow->layout(mWindow);

		// invalidate the entire window to get it repainted,
		// necessary for some lightweight components to get the
		// background reset
		mWindow->invalidate();

		// keep this around
		//ReleaseDC(mHandle, mDeviceContext);
		//mDeviceContext = NULL;

	}
}

//////////////////////////////////////////////////////////////////////
//
// Class Regiatration
//
//////////////////////////////////////////////////////////////////////

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
        // and possibly during shutdown of a VST host window if it
        // sends events after we've destructed the Window 
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
PUBLIC void WindowsWindow::registerClasses(const char* iconName)
{
	if (!ClassesRegistered) {

		WindowsContext* context = getContext();

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
		wc.hInstance     = context->getInstance();

		// Class icon (regular and small).
		// This must be a handle to an icon resource.
		// If null, the system supplies a default icon.
		// In the call to LoadIcon, the second arg can be the name
		// of an ICON or CURSOR resource.  If you use numbers wrap
		// the number in a MAKEINTRESOURCE() macro, e.g.
		// MAKEINTRESOURCE(125).  You can also use the string "#125" and
		// Windows will convert it.

		// wc.hIcon = LoadIcon(inst, MAKEINTRESOURCE(IDICO_ICON1));

		if (iconName == NULL) {
			// this is the default icon, looks like a white box
			wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
			wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
		}
		else {
			// you can load an alternative icon but it applies to all windows
			// supposedly you can call SetClassLong later 
			// but I couldn't get that to work
			HICON icon = LoadIcon(context->getInstance(), iconName);
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

		// let the context know so it can unregister them later if we're in a DLL
		context->registerClass(FRAME_WINDOW_CLASS);
		context->registerClass(DIALOG_WINDOW_CLASS);
		context->registerClass(ALERT_WINDOW_CLASS);
		context->registerClass(CHILD_WINDOW_CLASS);

		ClassesRegistered = true;
	}
}

QWIN_END_NAMESPACE
#endif // _WIN32

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
