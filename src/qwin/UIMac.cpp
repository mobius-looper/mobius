/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Implementations of ComponentUI for the OS X
 *
 */

#include <stdio.h>
#include <memory.h>

#include "Util.h"
#include "Thread.h"
#include "MacUtil.h"

#include "Qwin.h"
#include "UIManager.h"
#include "UIMac.h"

QWIN_BEGIN_NAMESPACE

/**
 * Defined in MacWindow, used to trace component invalidation handling.
 */
extern bool TraceInvalidates;

/****************************************************************************
 *                                                                          *
 *   							 UI FACTORIES                               *
 *                                                                          *
 ****************************************************************************/

DialogUI* UIManager::getDialogUI(Dialog* d)
{
    return new MacDialogUI(new MacDialog(d));
}

SystemDialogUI* UIManager::getOpenDialogUI(OpenDialog* od)
{
	return new MacOpenDialog(od);
}

SystemDialogUI* UIManager::getColorDialogUI(ColorDialog* od)
{
	return new MacColorDialog(od);
}

SystemDialogUI* UIManager::getMessageDialogUI(MessageDialog* md)
{
	return new MacMessageDialog(md);
}

NullUI* UIManager::getNullUI()
{
	return new NullUI();
}

StaticUI* UIManager::getStaticUI(Static* s)
{
	return new MacStaticUI(new MacStatic(s));
}

PanelUI* UIManager::getPanelUI(Panel* p)
{
	return new MacPanelUI(new MacPanel(p));
}

ButtonUI* UIManager::getButtonUI(Button* b)
{
	return new MacButtonUI(new MacButton(b));
}

RadioButtonUI* UIManager::getRadioButtonUI(RadioButton* rb)
{
	return new MacRadioButtonUI(new MacRadioButton(rb));
}

RadiosUI* UIManager::getRadiosUI(Radios* r)
{
	return new MacRadiosUI(new MacRadios(r));
}

CheckboxUI* UIManager::getCheckboxUI(Checkbox* cb)
{
	return new MacCheckboxUI(new MacCheckbox(cb));
}

ComboBoxUI* UIManager::getComboBoxUI(ComboBox* cb)
{
	return new MacComboBoxUI(new MacComboBox(cb));
}

ListBoxUI* UIManager::getListBoxUI(ListBox* cb)
{
	return new MacListBoxUI(new MacListBox(cb));
}

GroupBoxUI* UIManager::getGroupBoxUI(GroupBox* gb)
{
	return new MacGroupBoxUI(new MacGroupBox(gb));
}

TextUI* UIManager::getTextUI(Text* t)
{
	return new MacTextUI(new MacText(t));
}

TextAreaUI* UIManager::getTextAreaUI(TextArea* ta)
{
	return new MacTextAreaUI(new MacTextArea(ta));
}

ToolBarUI* UIManager::getToolBarUI(ToolBar* tb)
{
	return new MacToolBarUI(new MacToolBar(tb));
}

StatusBarUI* UIManager::getStatusBarUI(StatusBar* sb)
{
    return new MacStatusBarUI(new MacStatusBar(sb));
}

TabbedPaneUI* UIManager::getTabbedPaneUI(TabbedPane* tp)
{
	return new MacTabbedPaneUI(new MacTabbedPane(tp));
}

TableUI* UIManager::getTableUI(Table* t)
{
	return new MacTableUI(new MacTable(t));
}

TreeUI* UIManager::getTreeUI(Tree* t)
{
	return new MacTreeUI(new MacTree(t));
}

ScrollBarUI* UIManager::getScrollBarUI(ScrollBar* sb)
{
	return new MacScrollBarUI(new MacScrollBar(sb));
}

WindowUI* UIManager::getWindowUI(Window* w)
{
    return new MacWindowUI(new MacWindow(w));
}

HostFrameUI* UIManager::getHostFrameUI(HostFrame* f)
{
    return new MacHostFrameUI(new MacHostFrame(f));
}

MenuUI* UIManager::getMenuUI(MenuItem* item)
{
    return new MacMenuUI(new MacMenuItem(item));
}

/****************************************************************************
 *                                                                          *
 *   						   SYSTEM UTILITIES                             *
 *                                                                          *
 ****************************************************************************/

bool UIManager::isPaintWindowRelative()
{
	return true;
}

/**
 * Return the RGB value for a system color.
 */
int UIManager::getSystemRgb(int code)
{
	int rgb = code;
	/*
	if (code == COLOR_BUTTON_FACE)
	  code = COLOR_BTNFACE;
	  rgb = GetSysColor(code);
	*/
	return rgb;
}

/**
 * Return a native color object that implements a Color.
 */
NativeColor* UIManager::getColor(Color* c)
{
	return new MacColor(c);
}

NativeFont* UIManager::getFont(Font* f)
{
	return new MacFont(f);
}

NativeTimer* UIManager::getTimer(SimpleTimer* t)
{
    return new MacTimer(t);
}

PUBLIC void UIManager::sleep(int millis) 
{
	SleepMillis(millis);
}

/**
 * Key is down if the high order bit is on.
 * Key is toggled if the low order bit is on.  
 */
PUBLIC bool UIManager::isKeyDown(int code)
{
	//return (GetKeyState(code) < 0);
	return false;
}

PUBLIC int UIManager::getScreenWidth()
{
    //return GetSystemMetrics(SM_CXSCREEN);
	return 0;
}

PUBLIC int UIManager::getScreenHeight()
{
	//return GetSystemMetrics(SM_CYSCREEN);
	return 0;
}

PUBLIC int UIManager::getVertScrollBarWidth()
{
	// measured with ARTIS at 16, leave a little extra
	return 20;
}

PUBLIC int UIManager::getVertScrollBarHeight()
{
	// measured with ARTIS at 16, leave a little extra
	return 20;
}

PUBLIC int UIManager::getHorizScrollBarHeight()
{
	//return GetSystemMetrics(SM_CYHSCROLL);
	return 20;
}

PUBLIC int UIManager::getHorizScrollBarWidth()
{
	//return GetSystemMetrics(SM_CXHSCROLL);
	return 20;
}

/****************************************************************************
 *                                                                          *
 *                                COMPONENT UI                              *
 *                                                                          *
 ****************************************************************************/

MacComponent::MacComponent()
{
	mHandle = NULL;
}

MacComponent::~MacComponent()
{
    detach();
}


/**
 * Return true if the component has been opened.
 */
bool MacComponent::isOpen()
{
	return (mHandle != NULL);
}

/**
 * Detach any special state we may have placed in the native
 * object.
 *
 * Messages can sometimes come in after we're deleted ourselves
 * if this native component has focus, be sure to clip this reference.
 */
void MacComponent::detach()
{
	if (mHandle != NULL) {
		//SetWindowLong(mHandle, GWL_USERDATA, (LONG)NULL);
	}
}

/**
 * Invalidate the native handle if any.
 * Called whenever a parent component is closed, which on Mac automatically
 * closes all the children.
 */
void MacComponent::invalidateHandle()
{
    mHandle = NULL;
}

/**
 * All of the Component location and sizing methods eventually call here 
 * (through the UI proxy) to make the corresponding adjustment in the native
 * component if we have one.
 *
 * The Default implementation assumes we always have a ControlRef handle.
 */
PUBLIC void MacComponent::updateBounds() 
{
    if (mHandle != NULL) {
        Component* c = getComponent();
        if (c != NULL) {
			// this gets the bounds relatively to the nearest "native parent"
			// which for Mac should always be a WindowRef
            Bounds b;
            c->getNativeBounds(&b);
			updateNativeBounds(&b);
        }
    }
}

void MacComponent::convertBounds(Bounds* in, Rect* out)
{
	int left = in->x;
	int top = in->y;
		
	out->left = left;
	out->top = top;
	out->right = left + in->width;
	out->bottom = top + in->height;
}

/**
 * Inner bounds setter for updateBounds().	
 */
void MacComponent::updateNativeBounds(Bounds* b) 
{
	// the default implementation assumes we have a ControlRef handle
    if (mHandle != NULL) {
		ControlRef control = (ControlRef)mHandle;

		Rect macBounds = { 0, 0, 0, 0 };
		convertBounds(b, &macBounds);
		
		// Sigh, buttons need some extra padding around them for some
		// kind of drop shadow or some such shit.  Text too.
		adjustControlBounds(&macBounds);

		int width = macBounds.right - macBounds.left;
		int height = macBounds.bottom - macBounds.top;

		/*
		Component* c = getComponent();
		printf("%s %s setting bounds %d %d %d %d\n",
			   c->getTraceClass(), c->getTraceName(),
			   macBounds.left, macBounds.top, width, height);
		*/

		// On Windows everything is a window and uses MoveWindow,
		// on Mac we have to use different functions for controls and windows.
		// I'd rather this be factored into the UIComponent interface 
		// but it's disruptive...

		Component* c = getComponent();
		if (c == NULL || !c->isWindow())
		  SetControlBounds(control, &macBounds);
		else {
            // for Windows bounds are always of the content region
			// second arg may be kWindowStructureRgn and
			// kWindowContentRgn
			// !! WindowsWindow handles this by overloading the method
			// would be cleaner.  We have less control over negative origins
			// here without checking to see what the structure region
			// would be...
			SetWindowBounds((WindowRef)control, kWindowContentRgn, &macBounds);
		}


		// this is the HiView way, tried this while debugging
		// TabbedPanel embedding, old way works as well
		// HIRect is a CGRect containing a CGPoint and CGSize
		/*
		HIRect hirect;
		hirect.origin.x = macBounds.left;
		hirect.origin.y = macBounds.top;
		hirect.size.width = width;
		hirect.size.height = height;
		HIViewSetFrame(control, &hirect);
		*/
	}
}

/**
 * This may be overloaded in a subclass in case it needs to make bounds adjustments.
 * Used by Button and Text to reduce the actual component size by a small amount
 * so the borders don't extend beyond Component space.  
 * Preferred size will have been caculated for Component space.
 * You can assume that the Rect is transient and may be modified.
 */
void MacComponent::adjustControlBounds(Rect* rect)
{
}

/**
 * Dump native bounds.	
 */
void MacComponent::debug()
{
#if 0
    if (mHandle != NULL) {
		Component* peer = getComponent();
		HIRect rect;
		HIViewGetBounds((ControlRef)mHandle, &rect);
		peer->trace(" Native bounds %f %f %f %f", rect.origin.x, rect.origin.y, rect.size.width, rect.size.height);

		HIViewRef sv = HIViewGetSuperview((ControlRef)mHandle);
		HIViewConvertRect(&rect, (ControlRef)mHandle, sv);
		peer->trace(" Native converted %f %f %f %f", rect.origin.x, rect.origin.y, rect.size.width, rect.size.height);

		HIViewGetFrame((ControlRef)mHandle, &rect);
		peer->trace(" Native frame %f %f %f %f", rect.origin.x, rect.origin.y, rect.size.width, rect.size.height);

	}
#endif
}

/**
 * Return true if we're using compositing windows.
 */
bool MacComponent::isCompositing()
{
	bool compositing = false;
	MacWindow* window = getMacWindow();
	if (window != NULL)
	  compositing = window->isCompositing();
	return compositing;
}

/**
 * After opening a component that represents a native parent other
 * than the root window (in practce only a Panel backed by a UserPane)
 * find all heavyweight components beneath us and change the embedding
 * from the default root window to the panel.
 *
 * This has to be recursive because there may be a layer of lightweight
 * containers between us and the heavyweights.
 */
void MacComponent::embedChildren(ControlRef parent)
{
	embedChildren(parent, isCompositing());
}

/**
 * Inner embedder that doesn't have to keep looking for
 * the compositing flag.
 */
void MacComponent::embedChildren(ControlRef parent, bool compositing)
{
	Component* comp = getComponent();
	Container* cont = comp->isContainer();
	if (cont != NULL) {
		Component* children = cont->getComponents();
		for (Component* c = children ; c != NULL ; c = c->getNext()) {
			ComponentUI* ui = c->getUI();
			if (ui != NULL) {
				MacComponent* mc = (MacComponent*)(ui->getNative());
				if (mc != NULL) {
					ControlRef control = (ControlRef)mc->getHandle();
					if (control != NULL) {
						if (compositing) {
							OSStatus status = HIViewAddSubview(parent, control);
							CheckStatus(status, "MacComponent::embedChildren");
						}
						else {
							EmbedControl(control, parent);
						}
					}
					else {
						// may be a lightweight container
						mc->embedChildren(parent, compositing);
					}
				}
			}
		}
	}
}

/**
 * Invalidate a component rectangle so it will be repainted.
 * On Windows we can draw lightweight components here but on Mac drawing
 * can only be done from the main window event loop thread.  So we have
 * to post a custom event whose handler will end up calling invalidateNative below.
 *
 * Because this must be pushed into the ComponentUI the thing we're
 * trying to draw may be a lightweight component that is a child to this one.
 * To avoid excessive redraws we therefore have to pass to arguments
 * into the event, this MacComponent and the thing we need to repaint.
 *
 */
PUBLIC void MacComponent::invalidate(Component* c)
{
	// have ot have a WindowRef to receive the event
	// will be null if we haven't opened yet
	MacWindow* window = getMacWindow();
	if (window != NULL) {

		WindowRef win = (WindowRef)window->getHandle();
		if (win != NULL) {

			if (TraceInvalidates) {
				printf("MacComponent::invalidate %s %p\n", c->getTraceClass(), c);
				fflush(stdout);
			}

			EventRef event;
			OSStatus status = CreateEvent(NULL, // CFAllocatorRef
										  kEventClassCustom,
										  kEventCustomInvalidate,
										  GetCurrentEventTime(), // EventTime, pass zero for current time
										  // can also be kEventAttributeUserEvent
										  kEventAttributeNone, 
										  &event);
			CheckStatus(status, "MacComponent::invalidate CreateEvent");
		
			// send this object
			MacComponent* peer = this;
			status = SetEventParameter(event, 
									   kEventParamCustomPeer,
									   typeQwinComponent,
									   sizeof(MacComponent*), 
									   &peer);
			CheckStatus(status, "MacComponent::invalidate kEventParamCustomPeer");
			status = SetEventParameter(event, 
									   kEventParamCustomComponent, 
									   typeQwinComponent,
									   sizeof(Component*), 
									   &c);
			CheckStatus(status, "MacComponent::invalidate kEventParamCustomComponent");

			// note that we have to use PostEventToQueue,
			// SendEventToEventTarget may run synchronously which won't
			// accomplish the thread swapping we need
			//status = SendEventToEventTarget( event, GetWindowEventTarget( win ) );

			// if we don't specify a target it will go to the App
			// event handler
			EventTargetRef target = GetWindowEventTarget(win);
			status = SetEventParameter(event, 
									   kEventParamPostTarget,
									   typeEventTargetRef,
									   sizeof(target), 
									   &target);
			CheckStatus(status, "MacComponent::invalidate SetEventParameter");

			EventQueueRef queue = GetMainEventQueue();
			// also have low and high priority
			status = PostEventToQueue(queue, event, kEventPriorityStandard);
			CheckStatus(status, "MacComponent::invalidate PostEventToQueue");
		
			ReleaseEvent( event );
		}
	}
}

/**
 * This should eventually be called by the event handler for the
 * custom event we send in the invalidate() method.
 * 
 * On Mac, there are serveral HIView functions we can call:
 *    HIViewSetNeedsDisplay
 *    HIViewSetNeedsDisplayInRegion
 *    HIViewSetNeedsDisplayInRect
 *    HIViewSetNeedsDisplayInShape
 * 
 * which all fire Carbon events and this
 *
 *    HIViewRender which renders immediately without an event.  This is
 *    not recommended.
 *
 * Docs say "You should never pass false for the inNeedsDisplay parameter".
 *
 * HIViewSetNeedsDisplay marks the entire view.  Since we're not using
 * HIView "correctly" the only views we have are the root window and any
 * UserPanes created for heavyweight Panels.  We'll start by invaliding the
 * window and trying to specify the component rect.  
 * 
 */
PUBLIC void MacComponent::invalidateNative(Component* c)
{
	Qwin::csectEnter();
	if (c->isWindow()) {
		// the full Monty
		WindowRef win = (WindowRef)c->getNativeHandle();
		if (win != NULL) {
			//printf("MacComponent::invalidateNative: Invalidating root view\n");
			//fflush(stdout);
			HIViewRef root = HIViewGetRoot(win);
			HIViewSetNeedsDisplay(root, true);
		}
		// also paint all the lightweights
		c->paint();
	}
	else {
		// we assume that invalidate() has already located the
		// suitable component with a view handle
		// !! for some odd reason if we invalidate a UserPane that is nested
		// in another (Panel within Panel) we get Draw events sent to both
		// the parent and this one.  To prevent this you have to use
		// lightweight panels which avoid creating UserPanes.  This is because
        // HIView walks the hierarchy setting NeedsDisplay which causes
        // paints, and we also paint the hierarchy.  Should no longer an issue
        // since we stopped using heavyweight Panels.

		ControlRef view = (ControlRef)c->getNativeHandle();
		if (view != NULL) {
			//printf("MacComponent::invalidateNative: Invalidating view %s\n",
			//c->getTraceName());
			//fflush(stdout);
			OSStatus status = HIViewSetNeedsDisplay(view, true);
			CheckStatus(status, "HIViewSetNeedsDisplay");
		}

		// also paint the lightweights, this was formerly conditional
		// if view != null, why?  Some panels get forced heavyweight but
		// they still contain lightweights, Space does not sure why...
		c->paint();
	}
	Qwin::csectLeave();
}

/**
 * Supposedly HIView hierarchies clean themselves up when you delete the root
 * so all we need to do here is null out the handle.  One exception is 
 * MacMenuItem which needs to overload this so it can remove items from the
 * parent menu.
 * 
 * NO! It's worse, if you have a "refcon" set on the native control
 * we can still hve events comming in for them after the MacCoponent is
 * deleted.  Not sure the exact circumstances but I see it with Panels.
 * Components with refcons should overload this and remove the reference.
 * 
 * !! What if we're just taking something out of the hierarchy without
 * closing the parent, should still be closing it???
 * We do this for Button and it is handling that with a close() overload.
 * Really all subclasses should handle this down here, but at the moment
 * it's only an issue for heavyweight buttons (which we don't use any
 * more in Mobius).  Not only do they need to remove references to the native
 * handle but they also need to remove the "ControlRef" from the native
 * control to us.  If we ever embed heavyweight components in a VST window
 * this will cause crashes, but at the moment this only happens for Panel
 * which has a close() overload.
 *
 * Like windows, closing a HIView will automatically close everything
 * below it so we have to null out any native handles in our children.
 */
PUBLIC void MacComponent::close()
{
	if (mHandle != NULL) {
		mHandle = NULL;
	}

	Component* c = getComponent();
	Container* con = c->isContainer();
	if (con != NULL) {
		// isn't this the same as checking to see if we had a non-null mHandle?
		if (con->isNativeParent())
			con->invalidateNativeHandle();
	}
}

PUBLIC void MacComponent::setEnabled(bool b) 
{
	if (mHandle != NULL) {
		//EnableWindow(mHandle, ((b) ? TRUE : FALSE));
	}
}

PUBLIC bool MacComponent::isEnabled()
{
	bool enabled = true;
	if (mHandle != NULL) {
		//enabled = (IsWindowEnabled(mHandle) != 0);
	}
	return enabled;
}

PUBLIC void MacComponent::setVisible(bool b) 
{
	if (mHandle != NULL) {
		//ShowWindow(mHandle, ((b) ? SW_SHOW : SW_HIDE));
	}
}

PUBLIC bool MacComponent::isVisible()
{
	bool visible = true;
	if (mHandle != NULL) {
		//visible = (IsWindowVisible(mHandle) != 0);
	}
	return visible;
}

/**
 * Ask for keyboard focus.
 */
PUBLIC void MacComponent::setFocus()
{
	if (mHandle != NULL) {
		//SetFocus(mHandle); 
	}
}

/**
 * Handle a window event for a focused control.
 * 
 * Hmm, pass all key messages on to the root window.  Originally
 * did this only if the isForcedFocus flag was on, but that prevented
 * keyboard shortcut commands from reaching the root frame if a component
 * had the focus.  
 *
 * For Lightweight components this is always what you want.  For
 * heavyweight components that understand keys (Text, ScrollBar)
 * it may not.  Might need a Component flag for this?  Assume for now
 * that we can call both handlers.
 *
 * KLUDGE: If this is a component in a Dialog and the processReturn
 * or processEscape methods end up closing the dialog, we can't
 * do any further processing since both "this" object and the native
 * handle will be invalid.  The same issue exists between
 * the calls to CallWindowProc and root->messageHandler.  In theory
 * CallWindowProc could close the window, which could render
 * the root window and this component invalid.
 *
 * Not sure the best way to approach this, may need a deferred
 * delete list?
 */
long MacComponent::messageHandler(int msg, int wparam, int lparam)
{
    long status = 0;

    return status;
}

void MacComponent::paint(Graphics* g)
{
}

void MacComponent::command(int code)
{
}

void MacComponent::notify(int code)
{
}

Color* MacComponent::colorHook(Graphics* g)
{
    return NULL;
}

/**
 * The default preferred size calculator for most controls.
 */
PUBLIC void MacComponent::getPreferredSize(Window* w, Dimension* d)
{
	if (mHandle != NULL) {
		Rect bounds = { 0, 0, 0, 0 };
		SInt16 baseLine;
		GetBestControlRect((ControlRef)mHandle, &bounds, &baseLine);

		int bestWidth = bounds.right - bounds.left;
		// !! how does baseline factor in here?
		int bestHeight = bounds.bottom - bounds.top;

		d->width = bestWidth;
		d->height = bestHeight;
	}
}

//////////////////////////////////////////////////////////////////////
//
// Change Request Events
//
//////////////////////////////////////////////////////////////////////

/**
 * Sends a kEventCustomChange message to the window containing information
 * about the new value for the component.  This should be called
 * by any methods that may be called from a thread outside the 
 * main event thread (specifically the MIDI event handling thread).
 */
PUBLIC void MacComponent::sendChangeRequest(int type, void* value)
{
	// have ot have a WindowRef to receive the event
	// will be null if we haven't opened yet
	MacWindow* window = getMacWindow();
	if (window != NULL) {

		//printf("MacComponent::invalidate %s %p\n", c->getTraceClass(), c);
		//fflush(stdout);

		WindowRef win = (WindowRef)window->getHandle();
		if (win != NULL) {

			EventRef event;
			OSStatus status = CreateEvent(NULL, // CFAllocatorRef
										  kEventClassCustom,
										  kEventCustomChange,
										  GetCurrentEventTime(), // EventTime, pass zero for current time
										  // can also be kEventAttributeUserEvent
										  kEventAttributeNone, 
										  &event);
			CheckStatus(status, "MacComponent::invalidate CreateEvent");
		
			// send this object
			MacComponent* peer = this;
			status = SetEventParameter(event, 
									   kEventParamCustomPeer,
									   typeQwinComponent,
									   sizeof(MacComponent*), 
									   &peer);
			CheckStatus(status, "MacComponent::invalidate kEventParamCustomPeer");
			// the type
			UInt32 utype = type;
			status = SetEventParameter(event, 
									   kEventParamCustomType,
									   typeUInt32,
									   sizeof(utype), 
									   &utype);
			CheckStatus(status, "MacComponent::invalidate kEventParamCustomType");
			// the value
			status = SetEventParameter(event, 
									   kEventParamCustomValue,
									   typeQwinComponent,
									   sizeof(void*), 
									   &value);
			CheckStatus(status, "MacComponent::invalidate kEventParamCustomValue");
			// note that we have to use PostEventToQueue,
			// SendEventToEventTarget may run synchronously which won't
			// accomplish the thread swapping we need
			//status = SendEventToEventTarget( event, GetWindowEventTarget( win ) );

			// if we don't specify a target it will go to the App
			// event handler
			EventTargetRef target = GetWindowEventTarget(win);
			status = SetEventParameter(event, 
									   kEventParamPostTarget,
									   typeEventTargetRef,
									   sizeof(target), 
									   &target);
			CheckStatus(status, "MacComponent::invalidate SetEventParameter");

			EventQueueRef queue = GetMainEventQueue();
			// also have low and high priority
			status = PostEventToQueue(queue, event, kEventPriorityStandard);
			CheckStatus(status, "MacComponent::invalidate PostEventToQueue");
		
			ReleaseEvent( event );
		}
	}
}

/**
 * Called by the MacWindow event handler to process a kEventCustomChange event.
 * Even though "this" will always be a MacWindow it is defined here so 
 * it can be next to sendChangeRequest to make it easier to keep them in sync.
 */
PUBLIC void MacComponent::handleChangeRequest(EventRef event)
{

	MacComponent* peer = NULL;
	UInt32 type;
	void* value;
	OSStatus status;

	status = GetEventParameter(event, 
							   kEventParamCustomPeer,
							   typeQwinComponent, NULL,
							   sizeof(MacComponent*), NULL,
							   &peer);
	CheckStatus(status, "kEventCustomInvalidate:GetEventParameter:peer");

	status = GetEventParameter(event, 
							   kEventParamCustomType,
							   typeUInt32, NULL,
							   sizeof(UInt32), NULL,
							   &type);
	CheckStatus(status, "kEventCustomInvalidate:GetEventParameter:type");

	status = GetEventParameter(event, 
							   kEventParamCustomValue,
							   typeQwinComponent, NULL,
							   sizeof(void*), NULL,
							   &value);
	CheckStatus(status, "kEventCustomInvalidate:GetEventParameter:value");
					
	if (peer != NULL) {
		peer->handleChangeRequest(type, value);
	}
}

/**
 * This is the default implementation of the kEventCustomChange 
 * handler called after unpacking the event parameters.
 * This must be overloaded in subclasses to do the right thing.
 */
PUBLIC void MacComponent::handleChangeRequest(int type, void* value)
{
}

//////////////////////////////////////////////////////////////////////
//
// Model Flipping
//
//////////////////////////////////////////////////////////////////////

/**
 * Locate the MacWindow for a generic Component..
 */
PUBLIC MacWindow* MacComponent::getMacWindow(Component* c)
{
	MacWindow* window = NULL;
	if (c != NULL) {
		Window* w = c->getWindow();
		if (w != NULL) {
			ComponentUI* ui = w->getUI();
			window = (MacWindow*)ui->getNative();
		}
	}

	return window;
}

PUBLIC MacWindow* MacComponent::getMacWindow()
{
	return getMacWindow(getComponent());
}

/**
 * Locate the MacContext for a generic Component..
 */
PUBLIC MacContext* MacComponent::getMacContext(Component* c)
{
	MacContext* context = NULL;
	MacWindow* window = getMacWindow(c);
	if (window != NULL) {
		context = window->getContext();
	}
	return context;
}

/**
 * Locate the MacContext for this native component.
 */
PUBLIC MacContext* MacComponent::getMacContext()
{
	// this is virtual
	Component* c = getComponent();
	return getMacContext(c);
}

/**
 * Returns the native component handle.
 */
void* MacComponent::getHandle()
{
    return mHandle;
}

/**
 * Static convenience method to return the native handle of a Component.
 * !! this is the same as Component::getNativeHandle, don't need both
 */
void* MacComponent::getHandle(Component* c)
{
	void* handle = NULL;

    ComponentUI* ui = c->getUI();
    if (ui != NULL) {
        MacComponent* mc = (MacComponent*)ui->getNative();
        if (mc != NULL)
          handle = (void*)mc->getHandle();
    }
	return handle;
}

/**
 * Return the MacComonent that is a parent to this one.
 * The layer of UI classes really sucks.
 */
PUBLIC MacComponent* MacComponent::getParent()
{
	MacComponent* mParent = NULL;
	Component* c = getComponent();
	if (c != NULL) {
        Container* cParent = c->getParent();
        while (mParent == NULL && cParent != NULL) {
			ComponentUI* ui = cParent->getUI();
			if (ui != NULL)
			  mParent = (MacComponent*)(ui->getNative());
				
			if (mParent == NULL)
              cParent = cParent->getParent();
        }
    }
	return mParent;
}

/**
 * Return the native handle of native parent comonent.
 */
void* MacComponent::getParentHandle()
{
	void* handle = NULL;
	MacComponent* parent = getParent();
	if (parent != NULL)
	  handle = parent->getHandle();
	return handle;
}

/**
 * Find the containing native WindowRef for this component.
 */
PUBLIC WindowRef MacComponent::getWindowRef()
{
	WindowRef ref = NULL;
	Component* c = getComponent();
	if (c != NULL) {
		Window* w = c->getWindow();
		if (w != NULL) {
			ref = (WindowRef)getHandle(w);
		}
	}
    return ref;
}

/**
 * Find the parent component in which to embed a child component.
 * This will return either a WindowRef if we're adding something to the
 * root window, or a ControlRef if we have a UserPane in between.
 */
PUBLIC void MacComponent::getEmbeddingParent(WindowRef* retwindow,
											 ControlRef* retcontrol)
{
	WindowRef window = NULL;
	ControlRef control = NULL;

	Component* c = getComponent();
	Container* parent = c->getParent();
	while (parent != NULL) {
		if (parent->isWindow()) {
			window = (WindowRef)parent->getNativeHandle();
			break;
		}
		else if (parent->isNativeParent()) {
			control = (ControlRef)parent->getNativeHandle();
			break;
		}
		else
		  parent = parent->getParent();
	}
			
	if (retwindow != NULL) *retwindow = window;
	if (retcontrol != NULL) *retcontrol = control;
}

PUBLIC MacGraphics* MacComponent::getMacGraphics()
{
	MacGraphics* g = NULL;
	Component* c = getComponent();
	if (c != NULL) {
		Window* w = c->getWindow();
		if (w != NULL) {
			g = (MacGraphics*)(w->getGraphics());
		}
	}
    return g;
}

QWIN_END_NAMESPACE
/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
