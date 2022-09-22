/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * The base class for the application's primary window.
 * There are two variants here, Frame is used for normal
 * standaloen windows that we control, HostFrame is used for
 * windows created by a plugin host that we do not control.
 *
 */

#include <stdio.h>
#include <memory.h>

#include "Util.h"
#include "Trace.h"

#include "Qwin.h"
#include "UIManager.h"

QWIN_BEGIN_NAMESPACE

/****************************************************************************
 *                                                                          *
 *   								FRAME                                   *
 *                                                                          *
 ****************************************************************************/

PUBLIC Frame::Frame() 
{
	initFrame(NULL);
}

PUBLIC Frame::Frame(Context* c) 
{
	initFrame(c);
}

PUBLIC Frame::Frame(Context* c, const char *title) 
{
	initFrame(c);
	setTitle(title);
}

PUBLIC Frame::~Frame() 
{
	// Frame does NOT own the Context, this may be a shared resource
	// used by several Frames during the lifetime of the application
	mContext = NULL;
}

void Frame::initFrame(Context *c) 
{
	mClassName = "Frame";
	setContext(c);
}

void Frame::dumpLocal(int i)
{
    indent(i);
    fprintf(stdout, "Frame: %d %d %d %d\n",
            mBounds.x, mBounds.y, mBounds.width, mBounds.height);
}

/****************************************************************************
 *                                                                          *
 *   							  HOST FRAME                                *
 *                                                                          *
 ****************************************************************************/
/*
 * This is used in special circumstances where a Window needs to 
 * be created with a native parent window that we did not create.
 * The main example is a VST plugin editor.  The window will have a native
 * handle that is not in our control.
 *
 * For AudioUnits we have both a native window handle and a native
 * "pane" within the window which on OSX is a User Pane control.
 *
 * The MacHostFrame implementation installs and removes event
 * handlers on this parent window and we can open other Components
 * directly into it.
 * 
 * On Windows we don't install event handlers, instead it will embed
 * a child window which has it's own WindowsProcedure to get events.
 *
 */

PUBLIC HostFrame::HostFrame(Context* c, void* window, void* pane, Bounds* b)
{
	mClassName = "HostFrame";
	mHostWindow = window;
	mHostPane = pane;
	mNoBoundsCapture = false;
	setContext(c);
	
    // this tells Window::open we're a child window
	setClass(CHILD_WINDOW_CLASS);

	// Bounds represents what we asked the host for, just copy
	// it without getting fancy yet.  Wait till open() to try
	// to reconcile.

	if (b != NULL) {
		mBounds.x = b->x;
		mBounds.y = b->y;
		mBounds.width = b->width;
		mBounds.height = b->height;
	}
}

PUBLIC HostFrame::~HostFrame()
{
}

/**
 * Tells the WindowsWindow::messageHandler that we're not the 
 * application window and should not post QUIT.
 */
PUBLIC bool HostFrame::isHostFrame()
{
    return true;
}

/**
 * Have to overload the one inherted from Window.
 */
PUBLIC ComponentUI* HostFrame::getUI()
{
    if (mUI == NULL)
      mUI = UIManager::getHostFrameUI(this);
    return mUI;
}

/**
 * Overload this so that Window won't wait for a run() call to 
 * open the window.
 */
bool HostFrame::isRunnable()
{
	return false;
}

PUBLIC void* HostFrame::getHostWindow()
{
	return mHostWindow;
}

PUBLIC void* HostFrame::getHostPane()
{
	return mHostPane;
}

/**
 * This is a kludge to allow plugins to disable calling
 * captureNativeBounds() after we open within the host frame.
 * Mac AudioMulch resizes the window AFTER we're it opens the
 * VST editor, the initial bounds seem to be fixed at 840x420 which
 * causes the Mobius UI to render wrong.
 */
PUBLIC void HostFrame::setNoBoundsCapture(bool b)
{
	mNoBoundsCapture = b;
}

PUBLIC bool HostFrame::isNoBoundsCapture()
{
	return mNoBoundsCapture;
}

QWIN_END_NAMESPACE

/****************************************************************************
 *                                                                          *
 *   							   WINDOWS                                  *
 *                                                                          *
 ****************************************************************************/

#ifdef _WIN32
#include <windows.h>
#include "UIWindows.h"
QWIN_BEGIN_NAMESPACE

WindowsHostFrame::WindowsHostFrame(HostFrame* f) : WindowsWindow(f) 
{
}

WindowsHostFrame::~WindowsHostFrame() 
{
    // make sure we prune the rererence to this so WindowProcedure 
    // won't send us anything, I don't think this really matters
    // since we won't have given this window our WindowProcedure
	if (mHandle != NULL)
      Trace(1, "WindowsHostFrame: lingering handle during destruction\n");

    // NO! screws up Reaper
    /*
    HWND parent = getParentWindowHandle();
    if (parent != NULL)
      (void)SetWindowLong(parent, GWL_USERDATA, (LONG)NULL);
    */
}

/**
 * Overload this so we can give WindowsWindow::open the handle
 * to the host parent window.
 */
HWND WindowsHostFrame::getParentWindowHandle()
{
    return (HWND)((HostFrame*)mWindow)->getHostWindow();
}

/**
 * WindowsHostFrame is weird because we require it to only
 * have one child component which is a ChildWindow.  The ChildWindow
 * is where we do the usual work of setting backgrounds and doing
 * the layout (the stuff in finishOpening).
 */
PUBLIC void WindowsHostFrame::open() 
{
	if (mHandle == NULL) {

		HWND parent = getParentWindowHandle();
        if (parent != NULL) {
            
            // Shouldn't need this since we aren't running our 
            // WindowsProcedure in this window
            // NO! screws up Reaper
			//(void)SetWindowLong(parent, GWL_USERDATA, (LONG)this);

            // The parent window has bounds we could try to capture
            // but the host is supposed to give us the size we asked for,
            // and anyway Ableton returns the wrong client rect
            // (wider and shorter) assume the host will resize
            // the client rect if necessary for the child window.

            // Shouldn't be necessary to set the background since the
            // child window will fill us.  
            // Note that we can't call setBackground since that expects
            // an mHandle we haven't opened yet!
            Color* c = mWindow->getBackground();
            WindowsColor* wc = (WindowsColor*)c->getNativeColor();
            HBRUSH current = (HBRUSH)
                SetClassLong(parent, GCL_HBRBACKGROUND, (long)wc->getBrush());
        }

        // this opens the embedded child window
        WindowsWindow::open();
    }

}

/**
 * Overload WindowsWindow::close which posts a WM_CLOSE message to the
 * native window.  We can't do that since we're not in control 
 * over the host window.  This will however call the close method
 * (via Container::close) on our embedded ChildWindow which does
 * do the usual WM_CLOSE on the child window.
 */
PUBLIC void WindowsHostFrame::close() 
{
	if (mHandle != NULL) {

		HWND parent = getParentWindowHandle();
        if (parent != NULL) {
            // remove our backref
            //(void)SetWindowLong(parent, GWL_USERDATA, (LONG)NULL);
        }

        WindowsWindow::close();
    }
}

QWIN_END_NAMESPACE
#endif

//////////////////////////////////////////////////////////////////////
//
// OSX
//
//////////////////////////////////////////////////////////////////////

#ifdef OSX
#include <Carbon/Carbon.h>
#include "UIMac.h"
#include "MacUtil.h"

QWIN_BEGIN_NAMESPACE

MacHostFrame::MacHostFrame(HostFrame* f) : MacWindow(f) 
{
	mControl = NULL;
	mControlSpec.u.classRef = NULL;
}

MacHostFrame::~MacHostFrame() 
{
	if (mControl != NULL)
	  DisposeControl (mControl);

	if (mControlSpec.u.classRef) {
		// !! DEPRECATED
		OSStatus status = UnregisterToolboxObjectClass((ToolboxObjectClassRef)mControlSpec.u.classRef);
		CheckStatus(status, "MacHostFrame::UnregisterToolboxObjectClass");
	}
}

/**
 * Overload MacWindow::open deal with a previously opened window.
 * 
 * For VST editors we will have a WindowRef which usually seems to be 
 * a borderless child window surrounded by the host's standard VST window.
 * I tool examples of a custom "root control" from VSTGUI but didn't use it.
 * Child qwin components will be embedded directly in the VST window.
 * 
 * For AU editors we have both a WindowRef and a ControlRef to a UserPane,
 * child qwin components must be embedded in the UserPane. As long as we use
 * the AUCarbonViewBase framework, we don't seem to need to resize the window, 
 * we set the size of the user pane and the framework adapts the window.
 * This does however mean that we can't capture the native bounds here. 
 * 
 */
PUBLIC void MacHostFrame::open() 
{
	if (mHandle == NULL) {

		// this gets copied down so we can behave like other MacComponents
		mHandle = ((HostFrame*)mWindow)->getHostWindow();

		if (mHandle != NULL) {
			WindowRef theWindow = (WindowRef)mHandle;
			ControlRef pane = (ControlRef)((HostFrame*)mWindow)->getHostPane();

			// this is not necessarily compositing, AULab's isn't
			WindowAttributes attributes;
			GetWindowAttributes(theWindow, &attributes);
			mCompositing = (attributes & kWindowCompositingAttribute);

			// ****** start diagnostics ******
			/*
			printf("-----Host Frame Characteristics-----\n");
			WindowGroupRef group = GetWindowGroup(theWindow);
			if (group != NULL) {
				printf("HostFrame window group\n");
				DebugPrintWindowGroup(group);
			}
			else 
			  printf("HostFrame not in window group\n");

			if (attributes & kWindowNoActivatesAttribute)
			  printf("HostFrame does not receive activation events!!\n");

			if (!(attributes & kWindowCompositingAttribute)) {
			  printf("HostFrame is not a compositing window!!\n");
			
			WindowClass wclass;
			GetWindowClass(theWindow, &wclass);
			printf("HostFrame window class %d\n", (int)wclass);

			fflush(stdout);
			*/
			// ****** end diagnostics ******

			// Store our little extension wart in the window
			// arg is supposed to be a SRefCon which is the same as uint32
			SetWRefCon(theWindow, (UInt32)this);

			installEventHandlers(theWindow);

			// In Windows we would try to set the icon here, I don't
			// think you can do that on Mac, the icon has to come
			// from the bundle.
			mGraphics = new MacGraphics(this);

			// In Window we would now calculate the defalt text
			// metrics for native components like list boxes and
			// save it in Window::mTextMetrics.  This is Windows
			// specific, the same metrics do not necessarily work 
			// everywhere so we'll defer this to the peers.
			setBackground(mWindow->getBackground());
			
			// move it where we want it
			// kludge, if origin is 0,0 assume it wasn't set and we'll
			// live by the whim of the host, this can't happen anyway
			// since 0,0 of the client region would put the title bar
			// out of reach
			// hmm, this seems to be ignored, the host (Bidule at least)
			// seems to be free to move this after opening.
#if 0
			Bounds* b = mWindow->getBounds();
			if (b->x > 0 || b->y > 0) {

				Rect r;
				GetWindowBounds(theWindow, kWindowContentRgn, &r);

				int width = r.right - r.left;
				int height = r.bottom - r.top;

				r.left = b->x;
				r.top = b->y;
				r.right = r.left + width;
				r.bottom = r.top + height;

				SetWindowBounds(theWindow, kWindowContentRgn, &r);
			}
#endif

			if (pane != NULL) {
				// we're an AU, resize the root pane according to the 
				// bounds left on the HostFrame
				Bounds* b = mWindow->getBounds();
				SizeControl(pane, b->width, b->height);

				Rect paneBounds;
				GetControlBounds(pane, &paneBounds);

				// The AU pane works similarly to the VST root control
				// we're not using any more, reuse the mControl for embedding
				mControl = pane;
			}
			else {
				// I thought this would fix the button background
				// problem but it doesn't, interesting example though
				//setupRootControl();

				// vstgui does this for the root custom control it creates
				//SetControlDragTrackingEnabled (controlRef, true);
				//SetAutomaticControlDragTrackingEnabledForWindow ((WindowRef)systemWin, true);

				// we're a VST capture final bounds
				// this should be the same as the current mBounds
				// if the host obeyed our wishes
				// this works but isn't very useful since we have to 
				// ignore a row of host-specific components at the top
				// UPDATE: Mac AudioMulch resizes the window after
				// opening so this must be disabled.  I guess
				// we can just do this unconditionally?

				if (!((HostFrame*)mWindow)->isNoBoundsCapture())
				  captureNativeBounds(true);
			}

			// MacWindow would do this stuff in finishOpening
			mWindow->openChildren();

			// embed the immediate heavyweight compoents
			postOpen();

			// run the layout managers
			mWindow->layout(mWindow);
			bool traceLayout = false;
			if (traceLayout) {
				printf("Layout after opening:\n");
				mWindow->initTraceLevel();
				mWindow->debug();
			}

			// don't have to ShowWindow or ActivateWindow
			// but need to ask for focus
			// NOTE: This was something I did while flailing with VST 
			// window focus not sure if it works
			AdvanceKeyboardFocus(theWindow);
			SetUserFocusWindow(theWindow);

			// draw lightweight components
			// !! should we do this here or invalidate?
			// it doesn't seem to hurt as long as we're 
			// in the UI thread
			Graphics* g = mWindow->getGraphics();
			mWindow->paint(g);

			mWindow->opened();
			mWindowEvent->setId(WINDOW_EVENT_OPENED);
			mWindow->fireWindowEvent(mWindowEvent);
		}
	}
}

/**
 * Called by MacWindow when it intercepts a kEventWindowClose,
 * normally when you've just clicked the window close icon.
 * Since we don't control the event loop, only fire events.
 */
void MacHostFrame::closeEvent()
{
	if (mClosed) {
		// not expecting this
		printf("WARNING: MacHostFrame::closeEvent called more than once!\n");
		fflush(stdout);
	}
	else {
		// Windows has this, I forget what it was for
		if (mWindow->isNoClose()) {
			printf("WARNING: Ignoring the noClose option!!!\n");
			fflush(stdout);
		}

		// it is VERY important in Live to remove the event handlers
		removeEventHandlers();

		// good place for this too?
		if (mHandle != NULL)
		  SetWRefCon((WindowRef)mHandle, (UInt32)0);

		// usual notifications
		mWindow->closing();
		mWindowEvent->setId(WINDOW_EVENT_CLOSING);
		mWindow->fireWindowEvent(mWindowEvent);

		mClosed = true;

		// Java has  two of these, not sure why
		mWindowEvent->setId(WINDOW_EVENT_CLOSED);
		mWindow->fireWindowEvent(mWindowEvent);
	}
}

/**
 * Should be called by the application when it regains control
 * after the window is closed.
 * 
 * At the very least this needs to prune the references
 * between the native and Component models.  RefCons need to be removed
 * since we're typically going to delete the Component hierarchy
 * before the host deletes the window, which can result in events being
 * propagated to the deleted objects.
 *
 * We're doing most of the important work in closeEvent, do we really need this?
 */
PUBLIC void MacHostFrame::close()
{
	// AU window doesn't seem to be calling our handler for kEventWindowClose
	// so closeEvent never gets called.
	if (!mClosed)
	  closeEvent();

	if (mHandle != NULL) {
		WindowRef theWindow = (WindowRef)mHandle;
		SetWRefCon(theWindow, (UInt32)0);
		mHandle = NULL;

		// Container will be traversing the child hierarchy
		// asking them to close
	}
}

/**
 * An overload of MacWindow mouse handler to keep asking for focus.
 * For unknown reasons the window given to us by Bidule is not initially
 * given focus and loses it as soon as the Bidule window is clicked.
 * We don't get any events on the transfer so the handler we install
 * doesn't seem to be called in the same places as a window under our control.
 */
PUBLIC bool MacHostFrame::mouseHandler(EventRef event)
{
	int kind = GetEventKind(event);

	if (kind == kEventMouseDown) 
	  SetUserFocusWindow((WindowRef)mHandle);

	return MacWindow::mouseHandler(event);
}

static OSStatus
HostFrameEventHandler( EventHandlerCallRef caller, EventRef event, void* data )
{
	// Return this if we don't handle the event, noErr if we do handle the event
	// It is unclear when it is appropriate to return noErr as it disables
	// calling other handlers in the chain.  It seems usually necessary to 
	// let the default handlers fire for things like the close event.
    OSStatus err = eventNotHandledErr;

	int cls = GetEventClass(event);
	int kind = GetEventKind(event);
	
	TraceEvent("HostFrame", event);

	MacHostFrame* frame = (MacHostFrame*)data;
    
    switch (cls) {

        case kEventClassControl: {
		}
	}

	return err;
}

/**
 * Setup a root custom control like vstgui.
 * This is what VSTGUI does, I tried this to fix a problem
 * with button backgrounds in compositing windows but it didn't work.
 */
PRIVATE void MacHostFrame::setupRootControl()
{
	// apparently these need unique ids vstgui includes
	// the pointer to the frame class 
	CFStringRef defControlString = 
		CFStringCreateWithFormat(NULL, NULL, CFSTR("zonemobius.hostframe.%d"), (int)this);

	mControlSpec.defType = kControlDefObjectClass;
	mControlSpec.u.classRef = NULL;

	EventTypeSpec eventTypes[] = {	
		{kEventClassControl, kEventControlDraw},
		{kEventClassControl, kEventControlHitTest},
		{kEventClassControl, kEventControlClick},
		{kEventClassControl, kEventControlTrack},
		{kEventClassControl, kEventControlContextualMenuClick},
		{kEventClassKeyboard, kEventRawKeyDown},
		{kEventClassKeyboard, kEventRawKeyRepeat},
		{kEventClassMouse, kEventMouseWheelMoved},
		{kEventClassControl, kEventControlDragEnter},
		{kEventClassControl, kEventControlDragWithin},
		{kEventClassControl, kEventControlDragLeave},
		{kEventClassControl, kEventControlDragReceive},
		{kEventClassControl, kEventControlInitialize},
		{kEventClassControl, kEventControlGetClickActivation},
		{kEventClassControl, kEventControlGetOptimalBounds},
		{kEventClassScrollable, kEventScrollableGetInfo},
		{kEventClassScrollable, kEventScrollableScrollTo},
		{kEventClassControl, kEventControlSetFocusPart},
		{kEventClassControl, kEventControlGetFocusPart},
	};

	ToolboxObjectClassRef controlClass = NULL;

	// !! DEPRECATED
	OSStatus status = RegisterToolboxObjectClass(defControlString,
 												 NULL,
												 GetEventTypeCount(eventTypes),
												 eventTypes,
												 NewEventHandlerUPP(HostFrameEventHandler),
												 this,
												 &controlClass);
	CFRelease(defControlString);

	if (CheckStatus(status, "MacHostFrame::RegisterToolboxObjectClass")) {

		mControlSpec.u.classRef = controlClass;

		WindowRef theWindow = (WindowRef)mHandle;

		// this seems to be a borderless window?
		Bounds* b = mWindow->getBounds();

		// top, left, bottom, right
		Rect r = {(short)0, (short)0, (short)b->height, (short)b->width};
		// !! DEPRECATED
		OSStatus status = CreateCustomControl (NULL, &r, &mControlSpec, NULL, &mControl);
		if (CheckStatus(status, "MacHostFrame::CreateCustomControl")) {
	
			SetControlDragTrackingEnabled(mControl, true);
			SetAutomaticControlDragTrackingEnabledForWindow(theWindow, true);

			if (mCompositing) {
				HIViewRef contentView;
				HIViewRef rootView = HIViewGetRoot(theWindow);
				if (HIViewFindByID(rootView, kHIViewWindowContentID, &contentView) != noErr)
				  contentView = rootView;
				HIViewAddSubview(contentView, mControl);
			}
			else {
				ControlRef rootControl;
				GetRootControl(theWindow, &rootControl);
				if (rootControl == NULL)
				  CreateRootControl(theWindow, &rootControl);
				EmbedControl(mControl, rootControl);	
			}
		}
	}
}

/**
 * After opening the children, have to embed the views in the 
 * window. Auto embedding doesn't seem to work reliably with
 * VST host windows.
 */
PUBLIC void MacHostFrame::postOpen()
{
	//printf("MacHostFrame::postOpen\n");
	
	if (mHandle != NULL) {
		WindowRef window = (WindowRef)mHandle;

		// the dual contentView/rootControl was for compositing/nonCompositing
		// now that we have the AU pane to deal with this could be simplified 
		// to just one ControlRef with checks for mCompositing in embedChildren?
		HIViewRef contentView = NULL;
		ControlRef rootControl = NULL;
		WindowAttributes attributes;

		if (mControl != NULL) {
			// we opened an intermediate custom control, embed there
			rootControl = mControl;
		}
		else {
			if (mCompositing) {
				if (mControl != NULL) {
					// supposed to embed here
					contentView = mControl;
				}
				else {
					HIViewRef rootView = HIViewGetRoot(window);
					OSErr err = HIViewFindByID(rootView, kHIViewWindowContentID, &contentView);
					if (!CheckErr(err, "MacHostFrame::HIViewFindByID")) {
						// vstgui didn't handle the error case so it probably
						// "can't happen" in HIView, leave contentView NULL and ignore
					}
				}
			}
			else {
				// non-compositing
				if (mControl != NULL)
				  rootControl = mControl;
				else {
					GetRootControl(window, &rootControl);
					if (rootControl == NULL)
					  CreateRootControl(window, &rootControl);
				}
			}
		}

		embedChildren(contentView, rootControl, mWindow);
	}
}

void MacHostFrame::embedChildren(HIViewRef contentView, ControlRef rootControl,
								 Container* parent) 
{
	Component* children = parent->getComponents();
	for (Component* c = children ; c != NULL ; c = c->getNext()) {
		ComponentUI* ui = c->getUI();
		bool lightweight = true;
		if (ui != NULL) {
			MacComponent* mc = (MacComponent*)(ui->getNative());
			if (mc != NULL) {
				ControlRef control = (ControlRef)mc->getHandle();
				if (control != NULL) {
					lightweight = false;
					if (contentView != NULL) {
						// we're always compositing if we have a contentView
						OSStatus status = HIViewAddSubview(contentView, control);
						CheckStatus(status, "MacHostFrame::HIViewAddSubview");
					}
					else if (rootControl != NULL) {
						if (mCompositing) {
							OSStatus status = HIViewAddSubview(rootControl, control);
							CheckStatus(status, "MacHostFrame::HIViewAddSubview");
						}
						else {
							EmbedControl(control, rootControl);	
						}
					}
				}
			}
		}

		if (lightweight) {
			Container* cont = c->isContainer();
			if (cont != NULL)
			  embedChildren(contentView, rootControl, cont);
		}
	}
}

QWIN_END_NAMESPACE
#endif

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
