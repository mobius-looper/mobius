/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Mac implementation of the Window interface.
 *
 */

#include <stdio.h>
#include <memory.h>

#include "Util.h"
#include "Trace.h"
#include "Thread.h"
#include "MacUtil.h"

#include "KeyCode.h"
#include "Qwin.h"
#include "UIMac.h"

QWIN_BEGIN_NAMESPACE

/**
 * Flag used by UIMac.cpp:MacComponent::invalidate 
 * and WindowEventHandler below to trace the invalidation
 * of components.
 */
bool TraceInvalidates = false;

//////////////////////////////////////////////////////////////////////
//
// Application Events
//
//////////////////////////////////////////////////////////////////////

/**
 * Application event types we want to receive.
 */
PRIVATE EventTypeSpec AppEventsOfInterest[] = {

	{ kEventClassCommand, kEventCommandProcess },
	{ kEventClassApplication, kEventAppActivated },
	{ kEventClassApplication, kEventAppDeactivated },
	{ kEventClassApplication, kEventAppQuit },
	{ kEventClassApplication, kEventAppLaunchNotification },
	{ kEventClassApplication, kEventAppLaunched },
	{ kEventClassApplication, kEventAppTerminated },
	{ kEventClassApplication, kEventAppFrontSwitched },
	{ kEventClassApplication, kEventAppFocusMenuBar },
	{ kEventClassApplication, kEventAppFocusNextDocumentWindow },
	{ kEventClassApplication, kEventAppFocusNextFloatingWindow },
	{ kEventClassApplication, kEventAppFocusToolbar },
	{ kEventClassApplication, kEventAppFocusDrawer },
	{ kEventClassApplication, kEventAppGetDockTileMenu },
	{ kEventClassApplication, kEventAppIsEventInInstantMouser },
	{ kEventClassApplication, kEventAppHidden },
	{ kEventClassApplication, kEventAppShown },
	{ kEventClassApplication, kEventAppSystemUIModeChanged },
	{ kEventClassApplication, kEventAppAvailableWindowBoundsChanged },
	{ kEventClassApplication, kEventAppActiveWindowChanged },

	// custom events
	{ kEventClassCustom, kEventCustomInvalidate },
	{ kEventClassCustom, kEventCustomChange }
};

/**
 * The "data" argument is the MacWindow that installed the handler.
 *
 * The CommandProcess code doesn't do anything, I just left it behind
 * as a hard won example in case we need to handle command events out
 * here someday.  Components now have command event handlers installed
 * directly on them so we don't need them here.
 */
PRIVATE OSStatus
AppEventHandler(EventHandlerCallRef caller, EventRef event, void* data)
{
	// Return this if we don't handle the event, noErr if we do handle the event
	// It is unclear when it is appropriate to return noErr as it disables
	// calling other handlers in the chain.  It seems usually necessary to 
	// let the default handlers fire for things like the close event.
    OSStatus result = eventNotHandledErr;
    
	int cls = GetEventClass(event);
	int kind = GetEventKind(event);

	//TraceEvent("App", cls, kind);

    switch (cls) {

        case kEventClassApplication: {
            switch (kind) {
				case kEventAppActivated: { 
				}
					break;

                case kEventAppQuit: {
					// If you quit the app from the standard menu we won't
					// get kEventWindowClose so call the closing() method
					// from here.  
					MacWindow* window = (MacWindow*)data;
					window->quitEvent();
				}
					break;
			}
		}
			break;

			// These are just example stubs of command handling at the App level, 
			// the components will have already processed these with their own
			// handlers so don't fire any action events here.

        case kEventClassCommand: {
            switch (kind) {
                case kEventCommandProcess: {

					HICommandExtended cmd;
					// !! where does this come from and what does it do?
					// looks like CheckErr
					verify_noerr(GetEventParameter(event, kEventParamDirectObject, 
												   typeHICommand, NULL, 
												   sizeof(cmd), NULL, &cmd));

					UInt32 id = cmd.commandID;
					if (cmd.attributes & kHICommandFromMenu) {
						MenuRef menu = cmd.source.menu.menuRef;
						// this doesn't appear to be useful as an index
						// for GetMenuItemRefCon, items with submenus get
						// added into the index and seem hard to predict.
						// the commandID however can be used to locate the item.
						int index = cmd.source.menu.menuItemIndex;
						// there is a RefCon on the menu item since we can't
						// use the index reliably
						MacMenuItem* item = NULL;
						GetMenuItemRefCon(menu, 0, (UInt32*)&item);
						if (item != NULL) {
							// don't actually do anything, the handler
							// on the menu has already fired
							//item->fireSelectionById(id);
						}
					}
					else if (cmd.attributes & kHICommandFromControl) {
						ControlRef control = cmd.source.control;
						UInt32 refcon = GetControlReference(control);
						// should be handled by the control
					}
					else if (cmd.attributes & kHICommandFromWindow) {
						// handled by the window
					}
					else {
						// not sure what these would be
						//printf("AppEvent: Command %d from ???\n", id);
					}

                    switch (cmd.commandID) {
                        case kHICommandNew:
							// an example of what the standard New menu command 
							// would look like
							//printf("New handler!\n");
                            break;
                    }
				}
					break;
            }
        }
			break;
	
    }
    
    return result;
}

//////////////////////////////////////////////////////////////////////
//
// Window Events
//
//////////////////////////////////////////////////////////////////////

PRIVATE EventTypeSpec WindowEventsOfInterest[] = {

	{ kEventClassCommand, kEventCommandProcess },
	{ kEventClassCommand, kEventCommandUpdateStatus },

	// Action events
	{ kEventClassWindow, kEventWindowCollapse },
	{ kEventClassWindow, kEventWindowCollapseAll },
	{ kEventClassWindow, kEventWindowExpand },
	{ kEventClassWindow, kEventWindowExpandAll },
	{ kEventClassWindow, kEventWindowClose },
	{ kEventClassWindow, kEventWindowCloseAll },
	{ kEventClassWindow, kEventWindowZoom },
	{ kEventClassWindow, kEventWindowZoomAll },
	{ kEventClassWindow, kEventWindowContextualMenuSelect },
	{ kEventClassWindow, kEventWindowPathSelect },
	{ kEventClassWindow, kEventWindowGetIdealSize },
	{ kEventClassWindow, kEventWindowGetMinimumSize },
	{ kEventClassWindow, kEventWindowGetMaximumSize },
	{ kEventClassWindow, kEventWindowConstrain },
	{ kEventClassWindow, kEventWindowHandleContentClick },
	{ kEventClassWindow, kEventWindowGetDockTileMenu },
	{ kEventClassWindow, kEventWindowProxyBeginDrag },
	{ kEventClassWindow, kEventWindowProxyEndDrag },
	{ kEventClassWindow, kEventWindowToolbarSwitchMode },
    
	// Activation events
	{ kEventClassWindow, kEventWindowActivated },
	{ kEventClassWindow, kEventWindowDeactivated },
	{ kEventClassWindow, kEventWindowHandleActivate },
	{ kEventClassWindow, kEventWindowHandleDeactivate },
	{ kEventClassWindow, kEventWindowGetClickActivation },
	{ kEventClassWindow, kEventWindowGetClickModality },

	// Click events, don't really need these
	{ kEventClassWindow, kEventWindowClickDragRgn },
	{ kEventClassWindow, kEventWindowClickResizeRgn },
	{ kEventClassWindow, kEventWindowClickCollapseRgn },
	{ kEventClassWindow, kEventWindowClickCloseRgn },
	{ kEventClassWindow, kEventWindowClickZoomRgn },
	{ kEventClassWindow, kEventWindowClickContentRgn },
	{ kEventClassWindow, kEventWindowClickProxyIconRgn },
	{ kEventClassWindow, kEventWindowClickToolbarButtonRgn },
	{ kEventClassWindow, kEventWindowClickStructureRgn },

	// State change events
	{ kEventClassWindow, kEventWindowShowing },
	{ kEventClassWindow, kEventWindowHiding },
	{ kEventClassWindow, kEventWindowShown },
	{ kEventClassWindow, kEventWindowHidden },
	{ kEventClassWindow, kEventWindowCollapsing },
	{ kEventClassWindow, kEventWindowCollapsed },
	{ kEventClassWindow, kEventWindowExpanding },
	{ kEventClassWindow, kEventWindowExpanded },
	{ kEventClassWindow, kEventWindowZoomed },
	{ kEventClassWindow, kEventWindowBoundsChanging },
	{ kEventClassWindow, kEventWindowBoundsChanged },
	{ kEventClassWindow, kEventWindowResizeStarted },
	{ kEventClassWindow, kEventWindowResizeCompleted },
	{ kEventClassWindow, kEventWindowDragStarted },
	{ kEventClassWindow, kEventWindowDragCompleted },
	{ kEventClassWindow, kEventWindowClosed },
	{ kEventClassWindow, kEventWindowTransitionStarted },
	{ kEventClassWindow, kEventWindowTransitionCompleted },
    
	// Refresh events
	{ kEventClassWindow, kEventWindowUpdate },
	{ kEventClassWindow, kEventWindowDrawContent },

	// Cursor change events
	{ kEventClassWindow, kEventWindowCursorChange },

	// Focus events
	{ kEventClassWindow, kEventWindowFocusAcquired },
	{ kEventClassWindow, kEventWindowFocusRelinquish },
	{ kEventClassWindow, kEventWindowFocusContent },
	{ kEventClassWindow, kEventWindowFocusToolbar },
	{ kEventClassWindow, kEventWindowFocusDrawer },
    
	// also Sheet, Drawer, Window Definition events

	// Mouse events
	// won't actually get most of these with the standard event handler,
	// have to overload HitTest to prevent ControlManager from eating them
	{ kEventClassMouse, kEventMouseDown },
	{ kEventClassMouse, kEventMouseUp },
	{ kEventClassMouse, kEventMouseMoved },
	{ kEventClassMouse, kEventMouseDragged },
	{ kEventClassMouse, kEventMouseEntered },
	{ kEventClassMouse, kEventMouseExited },
	{ kEventClassMouse, kEventMouseWheelMoved },

	// Key events
	{ kEventClassKeyboard, kEventRawKeyDown },
	{ kEventClassKeyboard, kEventRawKeyRepeat },
	{ kEventClassKeyboard, kEventRawKeyUp },
	{ kEventClassKeyboard, kEventRawKeyModifiersChanged },
	{ kEventClassKeyboard, kEventHotKeyPressed },
	{ kEventClassKeyboard, kEventHotKeyReleased },

	// Text events
	// vstgui does this, not sure why
	{ kEventClassTextInput, kEventTextInputUnicodeForKeyEvent },

	// control events
	// these don't seem to do anything, have to put them on the root control
	{ kEventClassControl, kEventControlDraw },
	{ kEventClassControl, kEventControlHitTest },

	// custom events
	{ kEventClassCustom, kEventCustomInvalidate },
	{ kEventClassCustom, kEventCustomChange }

};

/**
 * Put an event handler on the HIView root so we can get paint messages.
 */
PRIVATE EventTypeSpec RootEventsOfInterest[] = {
	{ kEventClassControl, kEventControlDraw },
	{ kEventClassControl, kEventControlHitTest },
};

//////////////////////////////////////////////////////////////////////
//
// Event Handler Proc
//
//////////////////////////////////////////////////////////////////////

/**
 * The "data" argument is the MacWindow that installed the handler.
 *
 * !! TODO: Need to find an event to trigger the WINDOW_EVENT_ICONIFIED
 * and WINDOW_EVENT_DEICONIFIED WindowListener events.
 *
 */
static OSStatus
WindowEventHandler( EventHandlerCallRef caller, EventRef event, void* data )
{
	// Return this if we don't handle the event, noErr if we do handle the event
	// It is unclear when it is appropriate to return noErr as it disables
	// calling other handlers in the chain.  It seems usually necessary to 
	// let the default handlers fire for things like the close event.
    OSStatus err = eventNotHandledErr;

	int cls = GetEventClass(event);
	int kind = GetEventKind(event);
	
	//TraceEvent("Window", event);

	MacWindow* window = (MacWindow*)data;
	if (window == NULL) return err;
    
    switch (cls) {

        case kEventClassCommand: {
			// this is just here as a hard won example for the future
			// there are event handlers directly on the components now
			// so we don't need to handle commands here yet

            HICommandExtended cmd;
            verify_noerr(GetEventParameter(event, kEventParamDirectObject, 
										   typeHICommand, NULL, sizeof(cmd), 
										   NULL, &cmd));
            
            switch (kind) {
                case kEventCommandProcess: {
					// handled by component
				}
					break;
				case kEventCommandUpdateStatus: {
					// appearance sample uses this to fuck with menu items
					// not sure why
					//if ( this->UpdateCommandStatus( command.commandID ) )
				}
					break;
			}
		}
			break;

        case kEventClassMouse: {
			// note the qualification since Qwin also has a Point
			if (window != NULL) { 
				if (window->mouseHandler(event)) {
					//err = noErr;
				}
			}
		}
			break;

        case kEventClassKeyboard: {
			if (window != NULL) {
				if (window->keyHandler(event)) {
					// don't pass this along if we're forcing it
					//err = noErr;
				}
			}
		}
			break;

		case kEventClassWindow: {
			switch (kind) {
				case kEventWindowDrawContent: {
					// these aren't sent in HIView compositing windows
				}
					break;

				case kEventWindowHandleActivate: {
					// Called by the standard window handler when
					// it receives a kEventWindowActivated event
					// NOT the right place to handle custom drawing

					// !! should be firing WINDOW_EVENT_ACTIVATED
					// or WINDOW_EVENT_DEACTIVATED to the WindowListeners
				}
					break;

				case kEventWindowClose: {
					window->closeEvent();
					// you MUST return NotHandled here to get the window to close
				}
					break;

				case kEventWindowBoundsChanging: {
					// the WindowRef bounds haven't actually changed yet so
					// we can't layout as we go
				}
					break;

				case kEventWindowResizeCompleted: {
					// also get kEventWindowBoundsChanged immediately before this
					// note that this will paint, it seems to be okay but should
					// wait for a Draw event on the root view?
					window->resize();
					//err = noErr;
				}
					break;

				case kEventWindowDragCompleted: {
					// capture final location so we can save it in ui.xml
					window->captureNativeBounds(false);
					//err = noErr;
				}
					break;
			}
		}
			break;

			// vstgui does this, not sure we need it?
		case kEventClassTextInput: {
			if (kind == kEventTextInputUnicodeForKeyEvent) {

				// verbatim from vstgui...
				// The "Standard Event Handler" of a window would return noErr 
				// even though no one has handled the key event.  This prevents the
				// "Standard Handler" to be called for this event, with the exception
				// of the tab key as it is used for control focus changes.
				err = eventPassToNextTargetErr;
				EventRef rawKeyEvent;
				GetEventParameter(event, kEventParamTextInputSendKeyboardEvent, 
								  typeEventRef, NULL, sizeof (EventRef), NULL, 
								  &rawKeyEvent);
				if (rawKeyEvent) {
					UInt32 keyCode = 0;
					GetEventParameter (rawKeyEvent, kEventParamKeyCode, typeUInt32, 
									   NULL, sizeof (UInt32), NULL, &keyCode);

					// vstgui write it this way, VKEY_TAB+1 is VKEY_RETURN, WTF?
					//if (keyCode == keyTable[VKEY_TAB+1])
					// keyTable entry for TAB is 0x30 for RETURN 0x24
					if (keyCode == 0x24)
					  err = eventNotHandledErr;
				}
			}
		}
			break;

		case kEventClassControl: {
			switch (kind) {
				case kEventControlDraw: {
					// HIView compositing windows supposedly call this but
					// I havent' seen it .  Probaby not in the window.
				}
					break;

				case kEventControlHitTest: {
					// Kludge necessary to get mouse events to lightweight
					// components.  Never got here, I think this can only
					// be done on a root HIView.
					//err = noErr;
				}
					break;
			}
		}
			break;

		case kEventClassCustom: {
			switch (kind) {
				case kEventCustomInvalidate: {
					MacComponent* peer = NULL;
					Component* target = NULL;
					OSStatus status;
					status = GetEventParameter(event, 
											   kEventParamCustomPeer,
											   typeQwinComponent, NULL,
											   sizeof(MacComponent*), NULL,
											   &peer);
					CheckStatus(status, "kEventCustomInvalidate:GetEventParameter:peer");
					status = GetEventParameter(event, 
											   kEventParamCustomComponent,
											   typeQwinComponent, NULL,
											   sizeof(Component*), NULL,
											   &target);
					CheckStatus(status, "kEventCustomInvalidate:GetEventParameter:target");
					
					if (TraceInvalidates) {
						printf("Handling invalidation event: component %s %p peer %p\n",
							   target->getTraceClass(), target, peer);
						fflush(stdout);
					}

					if (peer != NULL && target != NULL) {
						peer->invalidateNative(target);
					}

					// since this is ours it there is nothing the default
					// handler can do
					err = noErr;
				}
					break;

				case kEventCustomChange: {

					window->handleChangeRequest(event);
					// since this is ours it there is nothing the default
					// handler can do
					err = noErr;
				}
					break;
			}
		}
			break;

    }
    
    return err;
}

/**
 * Handler for the root view.
 * This was developed for use with the "paint list"
 * to get a collection of lightweight components redrawn.
 * It is not currently used, but leave it around since it
 * was hard won code.
 */
PRIVATE OSStatus
RootEventHandler(EventHandlerCallRef caller, EventRef event, void* data)
{
	// return this if we don't handle the event, noErr if we do handle the event
    OSStatus result = eventNotHandledErr;

	int cls = GetEventClass(event);
	int kind = GetEventKind(event);

	//TraceEvent("Root", cls, kind);

	switch (cls) {

		case kEventClassControl: {
			switch (kind) {
				case kEventControlDraw: {
					MacWindow* win = (MacWindow*)data;
					if (win != NULL) {
						//printf("!!! kEventControlDraw: on root view\n");
						win->doPaints();
					}
				}
					break;

				case kEventControlHitTest: {
					// kludge necessary to get mouse events to lightweight
					// components, didn't seem to work here either, 
					// have to use a CustomControl
					//printf("RootEventHandler!!\n");
					//fflush(stdout);
					//result = 0;
				}
					break;
			}
		}
	}

	return result;
}

/**
 * Most examples use this.  Not sure what this does but
 * seems to produce the GetWindowEventHandlerUPP() function.
 */
DEFINE_ONE_SHOT_HANDLER_GETTER( WindowEventHandler )

//////////////////////////////////////////////////////////////////////
//
// Window Events
//
//////////////////////////////////////////////////////////////////////

/**
 * This is a big hammer, you should try to use invalidate()
 * on individual components instead.
 */
PRIVATE void MacWindow::repaint()
{
	Graphics* g = mWindow->getGraphics();
	mWindow->paint(g);
}

PRIVATE void MacWindow::resize()
{
	relayout();
}

/**
 * Terminate the application event loop when the window is closed.
 * This is overridden by MacDialog to call QuitAppModalLoopForWindow
 * if it was a modal dialog.  
 */
PRIVATE void MacWindow::closeEvent()
{
	if (mClosed) {
		// not expecting this
		printf("WARNING: MacWindow::closeEvent called more than once!\n");
		fflush(stdout);
	}
	else {
		// Windows has this, I forget what it was for
		if (mWindow->isNoClose()) {
			printf("WARNING: Ignoring the noClose option!!!\n");
			fflush(stdout);
		}

		removeEventHandlers();
		// good place for this too?
		if (mHandle != NULL)
		  SetWRefCon((WindowRef)mHandle, (UInt32)0);

		// overloaded by Dialog but then we wouldn't be here
		// since it also overloads CloseEvent
		mWindow->closing();
		mWindowEvent->setId(WINDOW_EVENT_CLOSING);
		mWindow->fireWindowEvent(mWindowEvent);

		QuitApplicationEventLoop();
		mClosed = true;

		// Java has  two of these, not sure why
		mWindowEvent->setId(WINDOW_EVENT_CLOSED);
		mWindow->fireWindowEvent(mWindowEvent);
	}
}

/**
 * Terminate the application event loop when the window is closed.
 * This is overridden by MacDialog to call QuitAppModalLoopForWindow
 * if it was a modal dialog.
 *
 * For the root Frame this may be called twice, once for kEventWindowClose
 * and again for kEventAppQuit.  If you exit the app from the standard menu 
 * we will get an AppQuit but not a WindowClose.  If you close the window you
 * get both.   In order to do things like save the last Mobius UI config, we
 * have to make sure that Window::closing is called in both cases.  To prevent
 * closing() from being called twice we keep a flag.
 */
PRIVATE void MacWindow::quitEvent()
{
	if (mClosed) {
		// this must be the extra call from kEventAppQuit, we've
		// already processed kEventWindowClose so we don't have to 
		// do it again
	}
	else {
		mWindow->closing();
		mWindowEvent->setId(WINDOW_EVENT_CLOSING);
		mWindow->fireWindowEvent(mWindowEvent);

		// we presumably don't have to call QuitApplicationEventLoop 
		// in this case since we're already out of the loop?

		mClosed = true;

		// Java has  two of these, not sure why
		mWindowEvent->setId(WINDOW_EVENT_CLOSED);
		mWindow->fireWindowEvent(mWindowEvent);
	}
}

//////////////////////////////////////////////////////////////////////
//
// Mouse Handler
//
//////////////////////////////////////////////////////////////////////

/**
 * Given an event, determine the coordinates of the mouse.
 * 
 * kEventParamMouseLocation returns global cooordinates
 * kEventParamWindowMouseLocation is what you want
 *
 * Note that mouse event coords are relative to the full window whereas
 * drawing and embedding coords are relative to the content region.  Have
 * to factor out the title bar height before passing along.
 * !! what about the left/right border width?
 */
PRIVATE void MacWindow::getMouseLocation(EventRef event, int* retx, int* rety)
{
	// note the qualification since Qwin namespace also has a Point
	::Point point;

	OSStatus stat = GetEventParameter(event, kEventParamWindowMouseLocation,
									  typeQDPoint, NULL, 
									  sizeof(::Point), NULL,
									  &point);
	CheckStatus(stat, "kEventParamWindowMouseLocation");

	// QT point is vertical and horizontal
	int x = point.h;
	int y = point.v;

	int theight  = getTitleBarHeight();
	y -= theight;

	//printf("Mouse %d %d\n", x, y);

	*retx = x;
	*rety = y;
}

/**
 * Given an event, determine the mouse button that was moved.
 */
PRIVATE int MacWindow::getMouseButton(EventRef event)
{
	int button = 0;

	// this is a UInt16
	EventMouseButton macButton;
	OSStatus stat = GetEventParameter(event, kEventParamMouseButton,
									  typeMouseButton, NULL, 
									  sizeof(macButton), NULL,
									  &macButton);
	if (stat) {
		printf("Not supposed to be here!\n");
		fflush(stdout);
	}

	CheckStatus(stat, "kEventParamMouseButton");

	// usually the left
	if (macButton & kEventMouseButtonPrimary)
	  button = MOUSE_EVENT_BUTTON1;

	// usually the right
	else if (macButton & kEventMouseButtonSecondary)
	  button = MOUSE_EVENT_BUTTON3;

	// usually the middle
	else if (macButton & kEventMouseButtonTertiary)
	  button = MOUSE_EVENT_BUTTON2;

	return button;
}

/**
 * Given an event, determine the mouse button click count.
 */
PRIVATE int MacWindow::getClickCount(EventRef event)
{
	UInt32 clicks;
	OSStatus stat = GetEventParameter(event, kEventParamClickCount,
									  typeUInt32, NULL, 
									  sizeof(clicks), NULL, 
									  &clicks);
	CheckStatus(stat, "kEventParamClickCount");
	return (int)clicks;
}

/**
 * Given an event, get the key modifiers.
 *
 * Mac modifiers we don't support:
 *   alphaLock has state of Caps Lock
 *   kEventKeyModifierNumLockMask
 *   kEventKeyModifierFnMask
 */
PRIVATE int MacWindow::getKeyModifiers(EventRef event)
{
	int modifiers = 0;

	UInt32 macModifiers;
	OSStatus stat = GetEventParameter(event, kEventParamKeyModifiers,
									  typeUInt32, NULL, 
									  sizeof(macModifiers), NULL, 
									  &macModifiers);
	CheckStatus(stat, "kEventParamKeyModifiers");

	if (macModifiers & shiftKey) modifiers |= KEY_MOD_SHIFT;
	if (macModifiers & controlKey) modifiers |= KEY_MOD_CONTROL;
	if (macModifiers & optionKey) modifiers |= KEY_MOD_ALT;

	// We might want to make this one look like ALT in case we're key remapping?
	if (macModifiers & cmdKey) modifiers |= KEY_MOD_COMMAND;

	return modifiers;
}

/**
 * Process a kEventClassMouse event.
 * Ignoring these:
 *   kEventMouseEntered
 *   kEventMouseExited
 *   kEventMouseWheelMoved
 *
 * TODO: Think of something interesting to bind to WheelMoved
 * 
 * We appear to get a kEventClassMouseMoved only when a button is down
 * and a kEventClassMouseDragged when a button is down.  We don't actually
 * care about non-drag events so we could filter those but something
 * may want them someday.
 */
PUBLIC bool MacWindow::mouseHandler(EventRef event)
{
	int kind = GetEventKind(event);

	if (kind == kEventMouseDown || kind == kEventMouseUp || 
		kind == kEventMouseMoved || kind == kEventMouseDragged) {

		bool dragStart = false;
		bool dragEnd = false;

		int x,y;
		getMouseLocation(event, &x, &y);

		// reuse this to void heap churn
		MouseEvent* e = mMouseEvent;
		e->init(0, MOUSE_EVENT_NOBUTTON, x, y);

		e->setModifiers(getKeyModifiers(event));

		// kEventMouseMoved won't have button info
		if (kind != kEventMouseMoved)
		  e->setButton(getMouseButton(event));

		// NOTE: Only for mac we'll treat ctrl-left as right, this
		// is a universal transformation.
		if (e->getButton() == MOUSE_EVENT_BUTTON1 &&
			e->getModifiers() & KEY_MOD_CONTROL) {
			e->setButton(MOUSE_EVENT_BUTTON3);
		}

		int kind = GetEventKind(event);

		switch (kind) {
			case kEventMouseDown:
				e->setType(MOUSE_EVENT_PRESSED);

				// would this apply here?
				// Windows has MOUSE_EVENT_CLICKED with a click count
				e->setClickCount(getClickCount(event));

				if (e->getButton() == MOUSE_EVENT_BUTTON1)
				  dragStart = true;
				break;

			case kEventMouseUp:
				e->setType(MOUSE_EVENT_RELEASED);
				e->setClickCount(getClickCount(event));
				if (e->getButton() == MOUSE_EVENT_BUTTON1)
				  dragEnd = true;
				break;

			case kEventMouseMoved:
			case kEventMouseDragged:
				e->setType(MOUSE_EVENT_MOVED);
				break;
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

			// Have to make these relative to the component.
			// Note that we use getWindowLocation here rather than
			// getNativeLocation like windows since we dont't get events
			// relative to container panels.  Could cache this.
			Point p;
			mDragComponent->getWindowLocation(&p);
			e->setX(e->getX() - p.x);
			e->setY(e->getY() - p.y);
			mDragComponent->fireMouseEvent(e);
		}   
		else if (mDownButton != NULL && 
				 e->getType() == MOUSE_EVENT_RELEASED) {
			mDownButton->fireMouseReleased();
			mDownButton = NULL;
		}
		else {
			Component* handler = mWindow->fireMouseEvent(e);
			// remember the component that handled the button press
			if (dragStart) {

				// this is a Windows call that forces mouse events to this window
				// find the equivalent!!
				//SetCapture(mHandle);

				mDragComponent = handler;
			}
		}

		if (dragEnd) {
			// Windows call
			//ReleaseCapture();
			mDragComponent = NULL;
		}

		// !! how can we support this reliably, handled just says that
		// something had a handler, not that it was interested
		// in the right mouse button

		if (e->getButton() == MOUSE_EVENT_BUTTON3) {
			PopupMenu* popup = mWindow->getPopupMenu();
			if (popup != NULL)
			  popup->open(mWindow, x, y);
		}
	}

	// we don't have any propagation prevention, 
	// it's sad really
	return false;
}

PUBLIC void MacWindow::setDownButton(MacButton* b)
{
	//printf("Setting DownButton %p\n", b);
	mDownButton = b;
}

//////////////////////////////////////////////////////////////////////
//
// Key Handling
//
//////////////////////////////////////////////////////////////////////

/**
 * Given a key event, return the key code.
 */
PRIVATE int MacWindow::getKeyCode(EventRef event)
{
	UInt32 code;
	OSStatus stat = GetEventParameter(event, kEventParamKeyCode,
									  typeUInt32, NULL, 
									  sizeof(code), NULL, 
									  &code);
	CheckStatus(stat, "kEventParamKeyCode");

	return (int)code;
}

/**
 * Given a key event, return the "mac char".
 * This isn't interesting for qwin but I was curious.
 */
PRIVATE int MacWindow::getMacChar(EventRef event)
{
	char macChar;
	OSStatus stat = GetEventParameter(event, kEventParamKeyMacCharCodes,
									  typeChar, NULL, 
									  sizeof(macChar), NULL, 
									  &macChar);
	CheckStatus(stat, "kEventParamMacCharCodes");

	return (int)macChar;
}

/**
 * Process a kEventClassKeyboard event.
 *
 * kEventClassTextInput is recommended for Unicode text processing apps,
 * specifically kEventTextInputUnicodeForKeyEvent.
 *
 * Ignoring kEventHotKeyPressed/Released, I'm not sure what they do.
 * Ignoring kEventRawKeyModifiersChanged, we should get up/down events
 * for these as well?
 *
 * kEventRawKeyRepeat 
 */
PUBLIC bool MacWindow::keyHandler(EventRef event)
{
	int kind = GetEventKind(event);

	if (kind == kEventRawKeyDown || kind == kEventRawKeyUp) {

		// reuse this to void heap churn
		KeyEvent* e = mKeyEvent;

		if (kind == kEventRawKeyDown)
		  e->setType(KEY_EVENT_DOWN);
		else
		  e->setType(KEY_EVENT_UP);

		e->setModifiers(getKeyModifiers(event));

		int code = getKeyCode(event);
		int ch = getMacChar(event);
		int trancode = Character::translateCode(code);
		const char* transtr = Character::getString(trancode);

		//printf("Key %x %c %x %s\n", 
		//code, (char)ch, trancode, transtr);

		e->setKeyCode(Character::translateCode(code));
			   
		// don't have anything like this but I suppose
		// we could count kEventRawKeyRepeats
		//e->setRepeatCount(status & 0xFF);

		mWindow->fireKeyEvent(e);
	}

	// don't propagate if forced focused
	bool handled = (mWindow->isForcedFocus());

	return handled;
}


//////////////////////////////////////////////////////////////////////
//
// Paint List
//
//////////////////////////////////////////////////////////////////////

// UPDATE: This was developed while flailing around trying to 
// get HIViewSetNeedsDisplay to work from a secondary thread
// (it does't you have to post a custom event and call it from
// the main UI thread).  We don't actually use the paint list, but
// let's keep it around in case we want to use it to reduce the
// number of custom events we post do draw a set of lightweight
// components.

/**
 * Since most of the Mobius "space" components are lightweight we're
 * not going to bother with making them all custom HIViews though that
 * would be the technically correct Mac design.  Instread, they are
 * drawn by invalidating the HIView root view for a window but only 
 * calling the comonent draw methods for the components on the
 * "paint list" of the window.
 *
 * PROBLEMS:  This works reasonable well for lightweight components
 * but if the window also has a lot of heavyweight components we will
 * draw them redundantly.  Need to restrict this to panels (UserPanes)
 * rather than the entire root view!!
 * 
 * It appears that we can draw into the window from any event handler, but
 * some drawing has to occur from other threads so we have to consistently
 * use the paint list even if it may not be necessary in the current context.
 * 
 * To avoid memory allocation overhead the list of components to paint is
 * managed in a ring buffer.  The tail of the buffer is advanced by an
 * event handling thread for a timer or input device, the head is advanced
 * only during the kEventControlPaint event method.  If the buffer overflows
 * we set a flag that will cause the next paint event handeler to just
 * repaint everything.  
 *
 * I'm not sure how significant it is that we avoid heap churn here, but 
 * it makes me feel better.
 *
 * For extra safety we're assuming we need a Csect around access to the 
 * head and tail pointer values, since they are 4 byte integers and not 
 * necessarily going to be atomically updated. If we can determine
 * this is not a problem for Mac remove the csects.
 */
PUBLIC void MacWindow::addPaint(Component* c) 
{
	//printf("Adding paint %s\n", c->getTraceClass());
	//fflush(stdout);
	mCsect->enter();
	int next = mPaintTail + 1;
	if (next >= MAX_PAINT_LIST)
	  next = 0;
	if (next == mPaintHead)
	  mPaintOverflow = true;
	else {
		mPaintComponents[mPaintTail] = c;
		mPaintTail = next;
	}
	mCsect->leave();
}

PUBLIC void MacWindow::doPaints() 
{
	// capture the head and tail pointers
	bool overflow;
	int head, tail;
	mCsect->enter();
	overflow = mPaintOverflow;
	head = mPaintHead;
	tail = mPaintTail;
	mCsect->leave();

	if (overflow) {
		// not really serious but I want to know if it happens
		//printf("MacWindow paint overflow!!!\n");
		repaint();
		// hmm this still feels wrong, can't we miss something?
		mCsect->enter();
		mPaintHead = 0;
		mPaintTail = 0;
		mPaintOverflow = false;
		mCsect->leave();
	}
	else if (head != tail) {
		//printf("Painting head %d tail %d\n", head, tail);
		//fflush(stdout);
		int psn = head;
		while (psn != tail) {
			Component* c = mPaintComponents[psn++];
			if (c != NULL)
			  c->paint();
			if (psn >= MAX_PAINT_LIST)
			  psn = 0;
		}
		
		// tail may have advanced by now but we should have an
		// event in the queue?
		int newTail;
		mCsect->enter();
		mPaintHead = psn;
		newTail = mPaintTail;
		mCsect->leave();

		if (tail != newTail) {
			// TODO: ensure that we have a paint event
			// in the queue or process it proactively?
		}
	}
}

//////////////////////////////////////////////////////////////////////
//
// MacWindow
//
//////////////////////////////////////////////////////////////////////

PUBLIC MacWindow::MacWindow(Window* win)
{
    mWindow = win;
	mCompositing = false;
	mAccel = NULL;
    mToolTip = NULL;
	mWindowHandler = NULL;
	mRootHandler = NULL;
	
    mGraphics = NULL;
    mWindowEvent = new WindowEvent();
    mMouseEvent = new MouseEvent();
    mKeyEvent = new KeyEvent();
    mDragComponent = NULL;
	mDownButton = NULL;
	mChild = false;
	mTitleBarHeight = -1;
	mClosed = false;

	mCsect = new CriticalSection();
	mPaintHead = 0;
	mPaintTail = 0;
	mPaintOverflow = false;
}

PUBLIC MacWindow::~MacWindow() 
{
    delete mWindowEvent;
    delete mMouseEvent;
    delete mKeyEvent;
    delete mGraphics;
}

PUBLIC MacContext* MacWindow::getContext()
{
    return (MacContext*)mWindow->getContext();
}

PUBLIC bool MacWindow::isCompositing()
{
	return mCompositing;
}

PUBLIC Graphics* MacWindow::getGraphics()
{
	if (mGraphics == NULL)
	  mGraphics = new MacGraphics(this);
    return mGraphics;
}

PUBLIC bool MacWindow::isChild() 
{
	return mChild;
}

/**
 * Capture the actual location and size of the native window,
 * used both after creation and after a resize/move.
 * We're only interested in the content region
 */
PUBLIC void MacWindow::captureNativeBounds(bool warn)
{
    if (mHandle != NULL) {
		Rect macBounds;
		GetWindowBounds((WindowRef)mHandle, kWindowContentRgn, &macBounds);

        int left = macBounds.left;
        int top = macBounds.top;
        int width = macBounds.right - macBounds.left;
        int height = macBounds.bottom - macBounds.top;

        // if we're a child window, don't pay attention to the 
        // real origin since we're relative to the parent
		if (mChild) {
			left = 0;
            top = 0;
		}

        Bounds* b = mWindow->getBounds();

        if (warn) {
            if (b->x != left) {
				printf("WARNING: captureNativeBounds x %d -> %d\n", b->x, left);
				fflush(stdout);
			}
            if (b->y != top) {
				printf("WARNING: captureNativeBounds y %d -> %d\n", b->y, top);
				fflush(stdout);
			}
            if (b->width != width) {
				printf("WARNING: captureNativeBounds width %d -> %d\n",
					   b->width, width);
				fflush(stdout);
			}
            if (b->height != height) {
				printf("WARNING: captureNativeBounds height %d -> %d\n",
					   b->height, height);
				fflush(stdout);
			}
		}

        b->x = left;
        b->y = top;
        b->width = width;
        b->height = height;
    }
}

/**
 * Get the height of the title bar.
 * This is used to adjust mouse coordinates that come in relative
 * to the upper left of the entire window rather than the content region.
 * We draw relative to the content region, I think due to the way
 * we create the CGContext bounds and embedding components appears to be
 * relative to the content region as well.  AFAIK the only coord that
 * is relative to the entire window is mouse events.
 */
PUBLIC int MacWindow::getTitleBarHeight()
{
    if (mTitleBarHeight == -1 && mHandle != NULL) {
		Rect macBounds;
		// this can error if the window doesn't have a title bar?
		// started seeing this with the Rax AU host
		OSStatus status = 
			GetWindowBounds((WindowRef)mHandle, kWindowTitleBarRgn, &macBounds);
		if (status == noErr) {
			mTitleBarHeight = macBounds.bottom - macBounds.top;
		}
		else {
			// Ignore status codes returned by hosts that don't make
			// windows with title bars, saw this first with Rax
			if (status != errWindowRegionCodeInvalid) {
				printf("WARNING: getTitleBarHeight status %d\n", (int)status);
				fflush(stdout);
			}
			mTitleBarHeight = 0;
		}
	}
	return mTitleBarHeight;
}

/**
 * Override the Component setBackground method and convert
 * it to a native window property.  This doesn't appear to 
 * work for controls?
 */
PUBLIC void MacWindow::setBackground(Color* c) 
{
    if (c != NULL && mHandle != NULL) {
        MacColor* mc = (MacColor*)c->getNativeColor();
		if (mc != NULL) {
			SetWindowContentColor((WindowRef)mHandle, mc->getRGBColor());
		}
	}
}

/**
 * The minimum top of the structure region of a new window.
 * This has to be far enough below the Mac menu bar so we can
 * grab it.
 */
#define MIN_WINDOW_TOP 22

/**
 * Open the underlying OS window.
 * 
 * SIZE NOTES
 * 
 * Most of this is irrelevant now that we always manage bounds
 * in terms of the content region rather than the structure region,
 * but this is potentially valuable information...
 *
 * Title bar measures with Artis loupe at 24 pixels.
 * Calling GetWindowBounds with kWindowTitleBarRgn gives 22.
 * Letting the window be opened and comaring the kWindowContentRgn 
 * with the total size also gives 22.  Not sure where the 2 pixels
 * go, this may due to be Mac's "line between the pixel" coorrdinate system.
 * 
 * It looks like there are no borders though a drop shadow is added
 *  around the permiter.
 * 
 * The sizing box in the lower right is 15x15
 * The three close/minimize/maximize buttons are 64 wide.
 */
PUBLIC void MacWindow::open() 
{
	OSStatus status = 0;

	if (mHandle != NULL) {
		// already open, bring it to the front?
	}
	else {

		WindowRef         theWindow;
		WindowClass		  windowClass = kDocumentWindowClass;
		WindowAttributes  windowAttrs;
		Rect              contentRect;
		CFStringRef       titleKey;
		OSStatus          result;

		// create the standard menu, this should only be done
		// for the root window
		if (mWindow->isFrame() != NULL) {
			MenuRef menu;
			CreateStandardWindowMenu(0 ,&menu);

			// Install our handler for common commands on the application target
			// This must only be done for the root frame
			InstallApplicationEventHandler(NewEventHandlerUPP(AppEventHandler),
										   GetEventTypeCount(AppEventsOfInterest),
										   AppEventsOfInterest,
										   this, NULL );
			CheckStatus(status, "MacPanel::InstallEventHandler");


		}

		// NOTE: need to ask for a "compositing" window to use HIView
		windowAttrs = 
			// compositing for HIView
			kWindowCompositingAttribute |

			// makes it dragable with minimum fuss?
			kWindowAsyncDragAttribute | 

			// supposedly does stuff, not sure
			kWindowStandardHandlerAttribute;


		if (mWindow->isDialog() == NULL) {
			windowAttrs |= 
				// CloseBox, FullZoom, CollapseBox, Resizeable
				kWindowStandardDocumentAttributes | 

				// not sure
				kWindowInWindowMenuAttribute;
		}
		else {
			windowAttrs |= 
				kWindowCloseBoxAttribute |
				kWindowResizableAttribute;

			// also kFloatingWindowClass for non-modals?
			// ouch! this doesn't work at all for VST HostFrames
			//windowClass = kMovableModalWindowClass;
		}

        // size of the content region
        // note that if the origin is too close to the upper left
        // the title bar will be obscured underneath the mac menu bar
        
		Bounds* bounds = mWindow->getBounds();
        if (bounds->y < MIN_WINDOW_TOP)
          bounds->y = MIN_WINDOW_TOP;

		SetRectLTWH(&contentRect, bounds->x, bounds->y, bounds->width, bounds->height);

		status = CreateNewWindow(windowClass, windowAttrs, &contentRect, &theWindow);
		if (CheckStatus(status, "MacWindow::open")) {
			// set this early so we can call things that need it
			mHandle = theWindow;

			// when we control the window, it will always be compositing
			// not so for HostFrame
			mCompositing = true;

			// Store our little extension wart in the window
			// arg is supposed to be a SRefCon which is the same as uint32
			SetWRefCon(theWindow, (UInt32)this);

			// set the title
			const char* title = mWindow->getTitle();
			if (title != NULL) {
				CFStringRef cftitle = MakeCFStringRef(title);
				SetWindowTitleWithCFString(theWindow, cftitle);
			}

			// capture the actual bounds
            // ?? this should be the same as mBounds unless we asked   
            // for something too big?
			captureNativeBounds(true);

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

			if (mWindow->isFrame() != NULL) {
				// add the menus
				MenuBar* mb = mWindow->getMenuBar();
				if (mb != NULL)
				  mb->open();
			}

			// now that we have a handle, can call back up to Window
			// to do some additional layout and sizing adjustments
			mWindow->finishOpening();

			//mWindow->dump(0);

			// setup initial tool tips, will need to do this
			// after every layout !
			// Windows specific?
			if (mWindow->isFrame() != NULL)
			  setupToolTips();

			// Auto position dialogs.  These usually start out with zero length
			// and get resized during layout so be sure to do this AFTER finishOpening
			// kWindowCenterOnMainScreen     = 1,
			// kWindowCenterOnParentWindow   = 2,
			// kWindowCascadeOnMainScreen
			if (mWindow->isDialog() != NULL)
			  RepositionWindow (theWindow, NULL, kWindowCenterOnMainScreen);

			// capture the final origin
			captureNativeBounds(false);

			// Show the window
			ShowWindow (theWindow); 
			// And make sure it is at the front, this is necessary for dialogs
			// opened by HostFrame, not sure why this is different than dialogs
			// opened under normal frames.
			SelectWindow(theWindow);

			// Appearance sample does this
			AdvanceKeyboardFocus(theWindow);
			SetUserFocusWindow(theWindow);

			// don't need this as long as SelectWindow is called
			//ActivateWindow(theWindow, true);

			// draw lightweight components
			// !! should we do this here or invalidate?
			// it doesn't seem to hurt as long as we're 
			// in the UI thread
			Graphics* g = mWindow->getGraphics();
			mWindow->paint(g);

			mWindow->opened();
			mWindowEvent->setId(WINDOW_EVENT_OPENED);
			mWindow->fireWindowEvent(mWindowEvent);
			
			//printf("---Window Groups------------------------------\n");
			//DebugPrintAllWindowGroups();

		}
	}
}

/**
 * Install the event handlers.  
 * This was factored out of open() so we can call it from
 * HostFrame::open.
 */
void MacWindow::installEventHandlers(WindowRef theWindow)
{
	// Install a command handler on the window
	// the WindowRef is passed as th "userData" so it will be
	// passed as the last arg to the handler function
	InstallWindowEventHandler(theWindow, 
							  GetWindowEventHandlerUPP(),
							  GetEventTypeCount(WindowEventsOfInterest),
							  WindowEventsOfInterest,
							  this, &mWindowHandler );

	// also install a handler on the root view so we can get paint messages?
	HIViewRef root = HIViewGetRoot(theWindow);
	InstallControlEventHandler(root,
							   NewEventHandlerUPP(RootEventHandler),
							   GetEventTypeCount(RootEventsOfInterest),
							   RootEventsOfInterest,
							   this, &mRootHandler);

}

void MacWindow::removeEventHandlers()
{
	OSStatus status;

	if (mWindowHandler != NULL) {
		status = RemoveEventHandler(mWindowHandler);
		CheckStatus(status, "MacWindow::RemoveEventhandler window");
		mWindowHandler = NULL;
	}

	if (mRootHandler != NULL) {
		status = RemoveEventHandler(mRootHandler);
		CheckStatus(status, "MacWindow::RemoveEventhandler root");
		mRootHandler = NULL;
	}
}


/**
 * Bring the window to the front and restore if minimized.
 * Hmm, if the window is minimized I'd like to restore it but if
 * maximized just bring it to the front and leave it maximized.
 * There doesn't seem to be a ShowWindow argument to do that?
 */
PRIVATE void MacWindow::toFront()
{
}

/**
 * After layout, whip through the components registring any that
 * have tool tips.  This will need to be smarter about things
 * that are actually visible.
 */
PRIVATE void MacWindow::setupToolTips()
{
}

/**
 * Close the window by sending it a message.
 * This overloads the MacComponent method but it works differently.
 * The close is deferred to the handler for this event so we can't
 * invalidate the child handles yet?  I guess we could.
 */
PUBLIC void MacWindow::close()
{
	if (mHandle != NULL) {
		// this is how appearance sample does it, but in response
		// to a close command event
		EventRef event;
		OSStatus status = CreateEvent(NULL, // CFAllocatorRef
									  kEventClassWindow, 
									  kEventWindowClose,
									  GetCurrentEventTime(), // EventTime, pass zero for current time
									  // can also be kEventAttributeUserEvent
									  kEventAttributeNone, 
									  &event);
		CheckStatus(status, "MacWindow::close CreateEvent");
		
		WindowRef window = (WindowRef)mHandle;
		SetEventParameter( event, kEventParamDirectObject, typeWindowRef,
						   sizeof( WindowRef ), &window );
		SendEventToEventTarget( event, GetWindowEventTarget( window ) );
		ReleaseEvent( event );

		// saw this in an example
		// this works but it acts weird, we don't get close events
		/*
		HICommand closeCommand;
		closeCommand.attributes = 0;  // not from a menu, control, or window
		closeCommand.commandID = kHICommandClose;
		ProcessHICommand(&closeCommand);
		*/
	}


}

/**
 * Enter into a basic message loop.
 */
PUBLIC int MacWindow::run() 
{
	int result = 0;

	if (mHandle == NULL)
	  open();

	if (mHandle == NULL) {
		printf("Window::run Unable to open window\n");
		fflush(stdout);
	}
	else {

		// the standard event loop
		RunApplicationEventLoop();

		// in case you we need our own

		// the handle is invalid at this point, don't try to use it again
		mHandle = NULL;
	}

	return result;
}

PRIVATE void MacWindow::customEventLoop()
{
	EventRef theEvent;
	EventTargetRef theTarget;
 
	theTarget = GetEventDispatcherTarget();
 
    while  (ReceiveNextEvent(0, NULL,kEventDurationForever,true, &theEvent) == noErr) {
 
		SendEventToEventTarget (theEvent, theTarget);
		ReleaseEvent(theEvent);
	}
}

/**
 * Called as events come in that change the window bounds.
 */
PUBLIC void MacWindow::relayout()
{
	if (mHandle != NULL) {

		captureNativeBounds(false);

		mWindow->layout(mWindow);

		// invalidate the entire window to get it repainted,
		// necessary for some lightweight components to get the
		// background reset
		mWindow->invalidate();
	}
}

QWIN_END_NAMESPACE
/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

