/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Implementations of ComponentUI for the Windows platform.
 *
 */

#include <stdio.h>
#include <memory.h>

#include <windows.h>

#include "Util.h"
#include "Qwin.h"

#include "UIManager.h"
#include "UIWindows.h"

/****************************************************************************
 *                                                                          *
 *   							 UI FACTORIES                               *
 *                                                                          *
 ****************************************************************************/

DialogUI* UIManager::getDialogUI(Dialog* d)
{
    return new WindowsDialogUI(new WindowsDialog(d));
}

SystemDialogUI* UIManager::getOpenDialogUI(OpenDialog* od)
{
	return new WindowsOpenDialog(od);
}

SystemDialogUI* UIManager::getColorDialogUI(ColorDialog* od)
{
	return new WindowsColorDialog(od);
}

SystemDialogUI* UIManager::getMessageDialogUI(MessageDialog* md)
{
	return new WindowsMessageDialog(md);
}

NullUI* UIManager::getNullUI()
{
	return new NullUI();
}

StaticUI* UIManager::getStaticUI(Static* s)
{
	return new WindowsStaticUI(new WindowsStatic(s));
}

PanelUI* UIManager::getPanelUI(Panel* p)
{
	return new WindowsPanelUI(new WindowsPanel(p));
}

ButtonUI* UIManager::getButtonUI(Button* b)
{
	return new WindowsButtonUI(new WindowsButton(b));
}

RadioButtonUI* UIManager::getRadioButtonUI(RadioButton* rb)
{
	return new WindowsRadioButtonUI(new WindowsRadioButton(rb));
}

RadiosUI* UIManager::getRadiosUI(Radios* r)
{
	return new WindowsRadiosUI(new WindowsRadios(r));
}

CheckboxUI* UIManager::getCheckboxUI(Checkbox* cb)
{
	return new WindowsCheckboxUI(new WindowsCheckbox(cb));
}

ComboBoxUI* UIManager::getComboBoxUI(ComboBox* cb)
{
	return new WindowsComboBoxUI(new WindowsComboBox(cb));
}

ListBoxUI* UIManager::getListBoxUI(ListBox* cb)
{
	return new WindowsListBoxUI(new WindowsListBox(cb));
}

GroupBoxUI* UIManager::getGroupBoxUI(GroupBox* gb)
{
	return new WindowsGroupBoxUI(new WindowsGroupBox(gb));
}

TextUI* UIManager::getTextUI(Text* t)
{
	return new WindowsTextUI(new WindowsText(t));
}

TextAreaUI* UIManager::getTextAreaUI(TextArea* ta)
{
	return new WindowsTextAreaUI(new WindowsTextArea(ta));
}

ToolBarUI* UIManager::getToolBarUI(ToolBar* tb)
{
	return new WindowsToolBarUI(new WindowsToolBar(tb));
}

StatusBarUI* UIManager::getStatusBarUI(StatusBar* sb)
{
    return new WindowsStatusBarUI(new WindowsStatusBar(sb));
}

TabbedPaneUI* UIManager::getTabbedPaneUI(TabbedPane* tp)
{
	return new WindowsTabbedPaneUI(new WindowsTabbedPane(tp));
}

TableUI* UIManager::getTableUI(Table* t)
{
	return new WindowsTableUI(new WindowsTable(t));
}

TreeUI* UIManager::getTreeUI(Tree* t)
{
	return new WindowsTreeUI(new WindowsTree(t));
}

ScrollBarUI* UIManager::getScrollBarUI(ScrollBar* sb)
{
	return new WindowsScrollBarUI(new WindowsScrollBar(sb));
}

WindowUI* UIManager::getWindowUI(Window* w)
{
    return new WindowsWindowUI(new WindowsWindow(w));
}

HostFrameUI* UIManager::getHostFrameUI(HostFrame* f)
{
    return new WindowsHostFrameUI(new WindowsHostFrame(f));
}

MenuUI* UIManager::getMenuUI(MenuItem* item)
{
    return new WindowsMenuUI(new WindowsMenuItem(item));
}

/****************************************************************************
 *                                                                          *
 *   						   SYSTEM UTILITIES                             *
 *                                                                          *
 ****************************************************************************/

bool UIManager::isPaintWindowRelative()
{
	return false;
}

/**
 * Return the RGB value for a system color.
 */
int UIManager::getSystemRgb(int code)
{
	int rgb = code;

	if (code == COLOR_BUTTON_FACE)
	  code = COLOR_BTNFACE;

	return GetSysColor(code);
}

/**
 * Return a native color object that implements a Color.
 */
NativeColor* UIManager::getColor(Color* c)
{
	return new WindowsColor(c);
}

NativeFont* UIManager::getFont(Font* f)
{
	return new WindowsFont(f);
}

NativeTimer* UIManager::getTimer(SimpleTimer* t)
{
    return new WindowsTimer(t);
}

PUBLIC void UIManager::sleep(int millis) 
{
	Sleep(millis);
}

/**
 * Key is down if the high order bit is on.
 * Key is toggled if the low order bit is on.  
 */
PUBLIC bool UIManager::isKeyDown(int code)
{
	return (GetKeyState(code) < 0);
}

PUBLIC int UIManager::getScreenWidth()
{
    return GetSystemMetrics(SM_CXSCREEN);
}

PUBLIC int UIManager::getScreenHeight()
{
	return GetSystemMetrics(SM_CYSCREEN);
}

PUBLIC int UIManager::getVertScrollBarWidth()
{
    return GetSystemMetrics(SM_CXVSCROLL);
}

PUBLIC int UIManager::getVertScrollBarHeight()
{
	return GetSystemMetrics(SM_CYVSCROLL);
}

PUBLIC int UIManager::getHorizScrollBarHeight()
{
	return GetSystemMetrics(SM_CYHSCROLL);
}

PUBLIC int UIManager::getHorizScrollBarWidth()
{
	return GetSystemMetrics(SM_CXHSCROLL);
}

/****************************************************************************
 *                                                                          *
 *                                COMPONENT UI                              *
 *                                                                          *
 ****************************************************************************/

WindowsComponent::WindowsComponent()
{
	mHandle = NULL;
	mWindowProc = NULL;
}

WindowsComponent::~WindowsComponent()
{
    detach();
}

/**
 * Subclass the built-in window proc for all controls so we
 * can do things like intercepting keystrokes when a control
 * has focus.
 */
LRESULT CALLBACK ControlProc(HWND window, UINT msg, 
							 WPARAM wparam, LPARAM lparam)
{
	LRESULT result = 0;

	// this has to be set if we end up here
	WindowsComponent* ui = (WindowsComponent*)
        GetWindowLong(window, GWL_USERDATA);

	if (ui != NULL)
      result = ui->messageHandler(msg, wparam, lparam);

	return result;
}

/**
 * Subclass the window proc for the native control.
 * Must be called by the createNativeComponent method if it wants
 * to intercept keyboard messages.
 */
void WindowsComponent::subclassWindowProc()
{
	if (mWindowProc == NULL && mHandle != NULL) {
		mWindowProc = (WNDPROC)
			SetWindowLong(mHandle, GWL_WNDPROC, (LONG)ControlProc);
	}
}

/**
 * Returns the native component handle.
 */
void* WindowsComponent::getHandle()
{
    return mHandle;
}

/**
 * Return true if the component has been opened.
 */
bool WindowsComponent::isOpen()
{
	return (mHandle != NULL);
}

/**
 * Convenience method to return the native handle of a Component.
 */
HWND WindowsComponent::getHandle(Component* c)
{
	HWND handle = NULL;

    ComponentUI* ui = c->getUI();
    if (ui != NULL) {
        WindowsComponent* wc = (WindowsComponent*)ui->getNative();
        if (wc != NULL)
          handle = (HWND)wc->getHandle();
    }
	return handle;
}

/**
 * Return the handle to the parent window for this component
 * if one is known.
 */
PUBLIC HWND WindowsComponent::getParentHandle()
{
	HWND ph = NULL;

    Component* c = getComponent();
    if (c != NULL) {
        Container* parent = c->getParent();
        while (parent != NULL) {
            if (!parent->isNativeParent())
              parent = parent->getParent();
            else {
                ph = getHandle(parent);
                break;
            }
        }
    }
	return ph;
}

/**
 * Return the native handle of the root window.
 * This has become especially awkward with the introduction
 * of the UI proxy layer.
 */
PUBLIC HWND WindowsComponent::getWindowHandle(Component* c)
{
    HWND handle = NULL;
    Window* w = c->getWindow();
    if (w != NULL)  
      handle = getHandle(w);
    return handle;
}

/**
 * Detach any special state we may have placed in the native
 * object.
 *
 * Messages can sometimes come in after we're deleted ourselves
 * if this native component has focus, be sure to clip this reference.
 */
void WindowsComponent::detach()
{
	if (mHandle != NULL)
	  SetWindowLong(mHandle, GWL_USERDATA, (LONG)NULL);
}

/**
 * Invalidate the native handle if any.
 * Called whenever a parent component is closed, which on Windows automatically
 * closes all the children.
 */
void WindowsComponent::invalidateHandle()
{
    mHandle = NULL;
}

/**
 * Locate the WindowsContext.
 */
PUBLIC WindowsContext* WindowsComponent::getWindowsContext(Component* c)
{
	WindowsContext* context = NULL;
	Window* w = c->getWindow();

	if (w != NULL) {
		ComponentUI* ui = w->getUI();
        WindowsWindow* native = (WindowsWindow*)ui->getNative();
		context = native->getContext();
	}

	return context;
}

/**
 * All of the dimension methods must call here to make the
 * corresponding adjustment int he proxy if we have one.
 */
PUBLIC void WindowsComponent::updateBounds() 
{
    // assume we're in the layout manager and don't need to 
    // send a WM_PAINT event (last arg false), might want to do this though
    if (mHandle != NULL) {
        Component* c = getComponent();
        if (c != NULL) {
            Bounds b;
            c->getNativeBounds(&b);
			updateNativeBounds(&b);
        }
    }
}

/**
 * Inner bounds setter, called by Components that want to size
 * themselves differently than what mBounds is.
 * NOTE: This will be overloaded by WindowsWindow which needs to 
 * make adjustments for the border components.
 */
void WindowsComponent::updateNativeBounds(Bounds* b) 
{
    // assume we're in the layout manager and don't need to 
    // send a WM_PAINT event (last arg false), might want to do this though
    if (mHandle != NULL) {
		MoveWindow(mHandle, b->x, b->y, b->width, b->height, true);
	}
}

/**
 * Invalidate a component rectangle so it will be repainted.
 *
 * Originally this would walk up the tree until we found a native parent
 * This should end up sending a WM_PAINT message to the parent window.
 * This works but can end up painting more than we need.
 *
 * Now we will assume that if the object does not have a native handle
 * that it is lightweight and can be painted directly rather than invalidated.
 *
 * NOOO!!!  If you paint it now it will use the Graphics from the root
 * window.  If this component happens to be inside a native panel
 * a "static" window, the relative coordinates will be all wrong.
 * Hmm, something is way fucked up here but mixing lightweights
 * and native components has always been a kludge.  Need to make
 * a clean break.
 * 
 */
PUBLIC void WindowsComponent::invalidate(Component* c)
{
	bool oldWay = false;
	
   	HWND handle = (HWND)c->getNativeHandle();

	if (oldWay || handle != NULL) {

		// walk up till we find the native parent component
		Component* npc = c;
		if (!c->isNativeParent())
		  npc = c->getNativeParent();

		if (npc != NULL) {
			handle = (HWND)npc->getNativeHandle();
			if (handle != NULL) {
				if (c == npc) {
					// invalidating self, shortcut
					InvalidateRect(handle, NULL, TRUE);
				}
				else {
					Point p;
					Bounds* b = c->getBounds();
					c->getNativeLocation(&p);
					RECT r;
					SetRect(&r, p.x, p.y, p.x + b->width, p.y + b->height);
					InvalidateRect(handle, &r, FALSE);
				}
			}
		}
	}
	else {
		// must be a lightweight, paint
		c->paint();
	}
}

/**
 * DestroyWindow will automatically traverse and destroy
 * child windows so we don't have to do that here, but Containers DO need to 
 * call invalidateNativeHandle on the child components.
 */
PUBLIC void WindowsComponent::close()
{
	if (mHandle != NULL) {
		SetWindowLong(mHandle, GWL_USERDATA, (LONG)NULL);
        // hmm, formerly when deleting Component hierarchies, we wouldn't
        // call this except for the root component, is this safe?
		DestroyWindow(mHandle);
		mHandle = NULL;
	}

	// don't leave stale handles in the child components
	// it looks odd but since we don't have the hierarchy in the
	// ComponentUI model, we have to let Component/Container do the
	// traversal and call back through the ComponentUI to null the handles
	Component* c = getComponent();
	Container* con = c->isContainer();
	if (con != NULL) {
		// isn't this the same as checking to see if we had a non-null mHandle?
		if (con->isNativeParent())
			con->invalidateNativeHandle();
	}
}

PUBLIC void WindowsComponent::setEnabled(bool b) 
{
	if (mHandle != NULL)
	  EnableWindow(mHandle, ((b) ? TRUE : FALSE));
}

PUBLIC bool WindowsComponent::isEnabled()
{
	bool enabled = true;
	if (mHandle != NULL)
	  enabled = (IsWindowEnabled(mHandle) != 0);
	return enabled;
}

PUBLIC void WindowsComponent::setVisible(bool b) 
{
	if (mHandle != NULL)
	  ShowWindow(mHandle, ((b) ? SW_SHOW : SW_HIDE));
}

PUBLIC bool WindowsComponent::isVisible()
{
	bool visible = true;
	if (mHandle != NULL)
	  visible = (IsWindowVisible(mHandle) != 0);
	return visible;
}

/**
 * Ask for keyboard focus.
 */
PUBLIC void WindowsComponent::setFocus()
{
	if (mHandle != NULL)
	  SetFocus(mHandle);
}

/**
 * Internal use only
 */
int WindowsComponent::getWindowStyle()
{
    DWORD style = WS_CHILD; 
    Component* c = getComponent();
    if (c != NULL) {
		// note that we use isSetVisible rather than isVisible to make
		// sure we only look at the mVisible flag and don't ask the UI
        if (c->isSetVisible())
          style |= WS_VISIBLE;

        if (!c->isSetEnabled())
          style |= WS_DISABLED;
    }
    return style;
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
long WindowsComponent::messageHandler(UINT msg, WPARAM wparam, LPARAM lparam)
{
    long status = 0;
	bool handled = false;
    Component* c = getComponent();

    if (c != NULL) {
        switch (msg) {
            case WM_KEYDOWN: {
                if (wparam == VK_TAB)
                  c->processTab();
                else if (wparam == VK_RETURN)
                  handled = c->processReturn();
                else if (wparam == VK_ESCAPE)
                  handled = c->processEscape();
            }
            break;

            case WM_DRAWITEM: {
                // can be here for OwnerDraw buttons in static panels?
                // doesn't seem to be working
                printf("WM_DRAWITEM!\n");
            }
            break;
        }

        // ugh, don't like having to do this
        if (!handled) {
            if (msg == WM_KEYDOWN || msg == WM_KEYUP) {
                Window* root = c->getWindow();
                ComponentUI* ui = root->getUI();
                if (ui != NULL) {
                    WindowsComponent* rootui = (WindowsComponent*)ui->getNative();

                    if (root->isForcedFocus()) {
                      status = rootui->messageHandler(msg, wparam, lparam);
                    }
                    else {
                        status = CallWindowProc(mWindowProc, mHandle, msg, wparam, lparam);
                        // update, always do this too
                        status = rootui->messageHandler(msg, wparam, lparam);   
                    }
                    handled = true;
                }
            }
        }
    }

    if (!handled)
      status = CallWindowProc(mWindowProc, mHandle, msg, wparam, lparam);

    return status;
}

void WindowsComponent::paint(Graphics* g)
{
}

void WindowsComponent::command(int code)
{
}

void WindowsComponent::notify(int code)
{
}

Color* WindowsComponent::colorHook(Graphics* g)
{
    return NULL;
}

void WindowsComponent::debug()
{
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
