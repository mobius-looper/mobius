/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Generic container.  Normally this has no native component,
 * you use it just to organize other components with a layout manager.
 *
 * In a few special cases you may set the "heavyweight" flag to 
 * make this open a platform-specific native container.  On Windows
 * this is a "static" window, on Mac this is a UserPane.
 *
 * Trying to phase this out, or at least factor to another class.
 * Use this as little as possible and document why!!
 * 
 */

#include <stdio.h>
#include <memory.h>

#include "Util.h"
#include "Qwin.h"
#include "UIManager.h"

QWIN_BEGIN_NAMESPACE

/****************************************************************************
 *                                                                          *
 *                                   PANEL                                  *
 *                                                                          *
 ****************************************************************************/

PUBLIC Panel::Panel()
{
	init();
}

PUBLIC Panel::Panel(const char* name)
{
	init();
    setName(name);
}

PUBLIC void Panel::init()
{
	mClassName = "Panel";
    mHeavyweight = false;
}

PUBLIC Panel::~Panel()
{
}

PUBLIC ComponentUI* Panel::getUI()
{
	if (mUI == NULL)
	  mUI = UIManager::getPanelUI(this);
    return mUI;
}

PUBLIC PanelUI* Panel::getPanelUI()
{
	return (PanelUI*)getUI();
}

PUBLIC void Panel::setHeavyweight(bool b) 
{
    mHeavyweight = b;
}

PUBLIC bool Panel::isHeavyweight()
{
    return mHeavyweight;
}

/**
 * Kludge for mac, return true if we have either click or motion
 * listeners for the panel.  This forces it to be heavyweight on Mac.
 */
PUBLIC bool Panel::isMouseTracking()
{
    return (mMouseListeners != NULL && mMouseListeners->size() > 0) ||
		(mMouseMotionListeners != NULL && mMouseMotionListeners->size() > 0);
}

PUBLIC bool Panel::isNativeParent()
{
    ComponentUI* ui = getUI();
	return ui->isNativeParent();
}

PUBLIC void Panel::dumpLocal(int indent)
{
    dumpType(indent, "Panel");
}

PUBLIC void Panel::open()
{
    ComponentUI* ui = getUI();
	ui->open();
	
	// recurse on children
	Container::open();

	// This is the only component that has a postOpen method for 
    // Mac  user panes.	 Since this is in the ComponentUI postOpen should 
    // arguably be done at a higher level in Container, but since it is
    // rare we'll hide it down here.
	ui->postOpen();
}

PUBLIC void Panel::paint(Graphics* g)
{
	if (!mHeavyweight) {
		tracePaint();
        if (mBackground != NULL) {
            Bounds b;
            getPaintBounds(&b);
            g->setColor(mBackground);
            g->fillRect(b.x, b.y, b.width, b.height);
        }
    }
    Container::paint(g);
}

PUBLIC void Panel::setBackground(Color* c) {

    Component::setBackground(c);
	invalidate();
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

PUBLIC WindowsPanel::WindowsPanel(Panel* p)
{
	mPanel = p;
}

PUBLIC WindowsPanel::~WindowsPanel()
{
}

/**
 * NOTE: On Windows if you try to embed a Button in a heavyweight
 * Panel the button events don't come through.  I never found out why that is
 * but we're trying to stop using heavyweight panels in Windows.
 * We need them on Mac in a few places.
 */
PUBLIC bool WindowsPanel::isNativeParent()
{
    return mPanel->isHeavyweight();
}

PUBLIC void WindowsPanel::open()
{
	if (mHandle == NULL && isNativeParent()) {
		HWND parent = getParentHandle();
        if (parent != NULL) {
            DWORD style = WS_CHILD | WS_VISIBLE;

            // Since this will be drawn natively have to factor in
            // insets for borders or padding.  Need to be doing this
            // with all native components!!

            Bounds b;

			// !! formerly had a confusing getNativeBoundsInset that would
			// subtract the system generated inset, I think this was to
			// adjust for the border automatically added to static components?
			// if so, then we need a new concept like "native insets" and/or
			// have this component override getNativeBounds
            mPanel->getNativeBounds(&b);

            printf("Opening a static panel!!:  %s\n", mPanel->getTraceName());
            fflush(stdout);

            mHandle = CreateWindow("static",
                                   NULL,
                                   style,
                                   b.x, b.y, b.width, b.height,
                                   parent,
                                   0,
                                   NULL,
                                   NULL);

            if (mHandle == NULL)
              printf("Unable to create Panel control\n");
            else {
                subclassWindowProc();
				SetWindowLong(mHandle, GWL_USERDATA, (LONG)this);
				mPanel->initVisibility();
			}
        }
    }
}

/**
 * Nothing to do on Windows.
 */
PUBLIC void WindowsPanel::postOpen()
{
}

PUBLIC Color* WindowsPanel::colorHook(Graphics* g)
{
    Color* brush = NULL;
	Color* back = mPanel->getBackground();

    if (mPanel->isHeavyweight() && back != NULL)
        brush = back;

    return brush;
}

QWIN_END_NAMESPACE
#endif // _WIN32

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

PUBLIC MacPanel::MacPanel(Panel* p)
{
	mPanel = p;
}

PUBLIC MacPanel::~MacPanel()
{
	// NOTE: This is not the place to be disposing components, it is especially
	// important not to call close() here now that we expect the Component
	// model to still be valid.  But when destructing we can't be sure of that.
}

/**
 * Seem to get occasional events sent to panels after the MacPanel
 * is deleted, make sure the reference is pruned.
 * As one of the few embeddable native components we also need to 
 * take ourselves out of the HIView hierarchy.
 * 
 * HIViewRemoveFromSuperview will take it out of the hierarchy but
 * does not delete it.
 *
 * DisposeControl removes it from the hierarchy and also deletes it if
 * the reference count goes to zero.  Docs say this alwo removes/deletes
 * all embedded controls too.
 *
 * Like Windows, since DisposeControl potentially invalidates all the
 * native handles below us, the caller (Container) has to use 
 * invalidateNativeHandle when we return.    This is handled indirectly
 * by closing all child components first.  It should be okay
 * to DisposeControl then call invalidateNativeHandle like 
 * MacComponent does.
 */
PUBLIC void MacPanel::close()
{
	if (mHandle != NULL) {
		SetControlReference((ControlRef)mHandle, (SInt32)NULL);
		
		// dispose alone didn't work for me, so we'll explicitly close
		// the children first
		for (Component* c = mPanel->getComponents() ; c != NULL ; 
			 c = c->getNext())
		  c->close();

		// this removes the view from the parent but does not release it
		//OSStatus status = HIViewRemoveFromSuperview((HIViewRef)mHandle);
		//CheckStatus(status, "HIViewRemoveFromSperview");

		DisposeControl((ControlRef)mHandle);
		mHandle = NULL;
	}
}

PUBLIC bool MacPanel::isNativeParent()
{
	// to get mouse events we always have to be a heavyweight panel
	return mPanel->isHeavyweight() || mPanel->isMouseTracking();
}

/**
 * We get a Click when the mouse button goes down and a Hit when it goes up.
 * Don't seem to get any Command events though the window does.
 */
PRIVATE EventTypeSpec PanelEventsOfInterest[] = {
	{ kEventClassControl, kEventControlHitTest },
	{ kEventClassControl, kEventControlClick },
	{ kEventClassControl, kEventControlTrack },
	{ kEventClassControl, kEventControlDraw },
};

/**
 * kEventControlHitTest
 * Sent when someone wants to find out what part of your control is at
 * a given point in local coordinates. You should set the 
 * kEventParamControlPart parameter to the appropriate part.
 *
 */
PRIVATE OSStatus
PanelEventHandler(EventHandlerCallRef caller, EventRef event, void* data)
{
    OSStatus result = eventNotHandledErr;

	int cls = GetEventClass(event);
	int kind = GetEventKind(event);

	//TraceEvent("Panel", cls, kind);

	if (cls == kEventClassControl) {
		if (kind == kEventControlDraw) {
			MacPanel* p = (MacPanel*)data;
			if (p != NULL) {
				p->draw();
			}
		}
		else if (kind == kEventControlHitTest) {
			MacPanel* p = (MacPanel*)data;
			if (p != NULL) {
				if (p->hitTest(event))
				  result = noErr;
			}
		}
	}

	return result;
}

/**
 * On Mac, the normal behavior for compositing windows using the
 * standard event handler is to NOT receive MouseDragged and MouseUp
 * events.  Instead these are eaten by the control manager.  The suggested
 * way to prevent this is to override HitTest in a view and have it return
 * a part code of zero.  This apparently prevents control manager from taking
 * over.  Probably you can only do this in a view that does not contain
 * native components like Buttons.
 */
PUBLIC bool MacPanel::hitTest(EventRef event)
{
	bool result = false;

	// Note that being heavyweight does not necessarily mean we're mouse
	// tracking.  Since this will take events away from the Control Manager
	// you can't do this if the panel contains any heavyweight controls.

	if (mPanel->isMouseTracking()) {

		// Extract the mouse location to see if there is a native
		// component beneath us
		HIPoint where;
		OSErr err = GetEventParameter(event, 
									  kEventParamMouseLocation, 
									  typeHIPoint, NULL, 
									  sizeof(HIPoint), NULL, 
									  &where );

		CheckErr(err, "GetEventParameter::kEventParamMouseLocation");

		// TODO: check to see if we're over a button and return 1
		// instead of zero?

		//printf("Setting control part to zero\n");
		ControlPartCode part = kControlNoPart;
		OSStatus status = SetEventParameter(event, 
											kEventParamControlPart,
											typeControlPartCode,
											sizeof(part),
											&part);
		CheckStatus(status, "SetEventParameter:kEventParamControlPart");
			
		result = true;
	}

	return result;
}


PUBLIC void MacPanel::open()
{
	OSStatus status;
	OSErr err;

	WindowRef window = getWindowRef();

	if (isNativeParent() && mHandle == NULL && window != NULL) {

		ControlRef control;
		Rect bounds = { 0, 0, 0, 0 };
		
		// lots of other options for tracking and focus
		int features = kControlSupportsEmbedding;

		status = CreateUserPaneControl(window, &bounds, features, &control);

		if (CheckStatus(status, "MacPanel::open")) {
			mHandle = control;
			SetControlReference(control, (SInt32)this);

			status = InstallControlEventHandler(control,
												NewEventHandlerUPP(PanelEventHandler),
												GetEventTypeCount(PanelEventsOfInterest),
												PanelEventsOfInterest,
												this, NULL);
			CheckStatus(status, "MacPanel::InstallEventHandler");
			SetControlVisibility(control, true, false);
		}
	}
}

/**
 * Handler for kEventControlDraw.
 * 
 * Note that if you have heavyweight panels in a hierarchy, we will
 * receive a draw event for each one.  On Windows when we receive a draw
 * event we recursively paint the components below this one, but that will
 * result in redundant paints as we receive another event for each
 * child panel.  
 *
 * We could prevent the cascade by returning noErr from our event handler
 * but I'm not sure what that would do to other heavyweight subcomponents.
 *
 * I don't see a good way to do this other than to stop using 
 * heavy weight Panels by default the only place we really need them
 * is the root for each TabbedPane pane.
 * 
 * The HIView way would seem to be that we only draw lightweight subcomponents,
 * as soon as we hit another heavyweight Panel as we descend we stop and
 * wait for its event.  Not such a bad thing.  
 */
PUBLIC void MacPanel::draw()
{
	//printf("MacPanel::repaint %s\n", mPanel->getTraceName());
	//fflush(stdout);

	MacGraphics* g = getMacGraphics();

	//printf("Panel repaint Panel %p graphics %p\n", mPanel, g);

	mPanel->paint(g);
}

/**
 * After opening the children, have to embed them in the user pane.
 * Doing it this way by "pulling" the children is easher than making
 * every child know how to "push" itself into us.
 *
 * !! This isn't going to work for Lightweight labels and 
 * lightweight containers.  Unlike Windows, whether we treat this as a
 * native parent for the purpose of positioning will depend on the child.
 * Real controls will be embedded but lightweights are still drawn relative
 * to this control's parent.  Hmm, this seems wrong, can you draw into
 * a UserPane?
 */
PUBLIC void MacPanel::postOpen()
{
	if (mHandle != NULL) {
		embedChildren((ControlRef)mHandle);
	}
}

QWIN_END_NAMESPACE
#endif

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
