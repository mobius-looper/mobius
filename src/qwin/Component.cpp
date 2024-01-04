/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * The base class for all components.
 *
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>

#include "Util.h"
#include "KeyCode.h"
#include "Qwin.h"
#include "UIManager.h"

QWIN_BEGIN_NAMESPACE

/****************************************************************************
 *                                                                          *
 *   							  COMPONENT                                 *
 *                                                                          *
 ****************************************************************************/

PUBLIC Component::Component() 
{
	mNext = NULL;
	mParent = NULL;
	mBounds.x = 0;
	mBounds.y = 0;
	mBounds.width = 0;
	mBounds.height = 0;
	mPreferred = NULL;
	mMinimum = NULL;
	mMaximum = NULL;
	mForegroundColorChanged = false; //#007
    mForeground = NULL;
    mBackground = NULL;
	mActionListeners = NULL;
	mMouseListeners = NULL;
	mMouseMotionListeners = NULL;
	mKeyListeners = NULL;
    mName = NULL;
    mToolTip = NULL;
    mEnabled = true;
    mVisible = true;
    mFocusRequested = false;
    mInsets = NULL;
    mBorder = NULL;
    mUI = NULL;
}

PUBLIC Component::~Component() {

	Component *el, *next;

	//printf("Deleting component %p\n", this);
	//fflush(stdout);

	delete mPreferred;
	delete mMinimum;
	delete mMaximum;
    delete mName;
    delete mInsets;
	delete mToolTip;
	delete mActionListeners;
	delete mMouseListeners;
	delete mMouseMotionListeners;
	delete mKeyListeners;
    delete mUI;

    // mBorder is a shared object
    
    // do NOT delete mForeground and mBackground Colors, they are cached

	for (el = mNext, next = NULL ; el != NULL ; el = next) {
		next = el->getNext();
		el->setNext(NULL);
		delete el;
	}
}

PUBLIC Component* Component::getNext() 
{
	return mNext;
}

void Component::setNext(Component* c) 
{
	mNext = c;
}

/**
 * Ideally this would be a pure virtual but
 * it's a pain to keep doing this in the custom
 * classes.
 */
PUBLIC ComponentUI* Component::getUI()
{
    if (mUI == NULL)
      mUI = UIManager::getNullUI();
    return mUI;
}

PUBLIC Container* Component::isContainer() 
{
	return NULL;
}

PUBLIC Button* Component::isButton() 
{
	return NULL;
}

PUBLIC Label* Component::isLabel() 
{
	return NULL;
}

PUBLIC class MenuItem* Component::isMenuItem() 
{
	return NULL;
}

PUBLIC class Panel* Component::isPanel() 
{
	return NULL;
}

PUBLIC Container* Component::getParent() 
{
	return mParent;
}

PUBLIC void Component::setParent(Container* c) 
{
	mParent = c;
}

/**
 * This one must be overloaded so the proper ComponentUI can
 * be allocated.    
 */
PUBLIC void Component::open()
{
}

/**
 * Determine if this component is open.
 */
PUBLIC bool Component::isOpen()
{
	bool open = false;
	Component* openable = this;

	if (!isNativeParent())
	  openable = getNativeParent();

	if (openable != NULL)
      open = (openable->getNativeHandle() != NULL);

	return open;
}

/**
 * Return the native component peer.
 */
PUBLIC NativeComponent* Component::getNativeComponent()
{
	ComponentUI* ui = getUI();
    return ui->getNative();
}

/**
 * Return the native component handle.
 * This should only be used internally.
 */
PUBLIC void* Component::getNativeHandle()
{
	void* handle = NULL;
    NativeComponent* native = getNativeComponent();
    if (native != NULL)
      handle = native->getHandle();

	return handle;
}

/**
 * Returns true if the peer native component is a "parent" to 
 * the child components.  For us that means that the locations of the children
 * will be relative to the origin of this parent component.
 * (that is, they start over from 0,0).
 *
 * We may not actually have an mHandle yet.  It is also used to determine
 * when to create handles for new components that are spliced
 * into the hierarchy after the root window is opened.
 */
PUBLIC bool Component::isNativeParent() 
{
    ComponentUI* ui = getUI();
    return ui->isNativeParent();
}

PUBLIC int Component::getX()
{
    return mBounds.x;
}

/**
 * All of the dimension methods must call here to make the
 * corresponding adjustment int he proxy if we have one.
 */
PUBLIC void Component::updateNativeBounds() 
{
    ComponentUI* ui = getUI();
    ui->updateBounds();
}

PUBLIC void Component::setX(int i) {
	if (mBounds.x != i) {
		mBounds.x = i;
		updateNativeBounds();
	}
}

PUBLIC int Component::getY() {
    return mBounds.y;
}

PUBLIC void Component::setY(int i) {
	if (mBounds.y != i) {
		mBounds.y = i;
		updateNativeBounds();
	}
}

PUBLIC int Component::getWidth() {
    return mBounds.width;
}

PUBLIC void Component::setWidth(int i) {
	if (mBounds.width != i) {
		mBounds.width = i;
		updateNativeBounds();
	}
}

PUBLIC int Component::getHeight() {
    return mBounds.height;
}

PUBLIC void Component::setHeight(int i) {
	if (mBounds.height != i) {
		mBounds.height = i;
		updateNativeBounds();
	}
}

PUBLIC void Component::setLocation(int x, int y) {
	mBounds.x = x;
	mBounds.y = y;
	updateNativeBounds();
}

PUBLIC void Component::setSize(int width, int height) {
	mBounds.width = width;
	mBounds.height = height;
	updateNativeBounds();
}

PUBLIC Dimension* Component::getSize()
{
	return &mBounds;
}

/**
 * Note that unlike the others, we do not take ownership of the dimension.
 * This is ordinarilly called only by the layout manager passing
 * in the preferred size.
 */
PUBLIC void Component::setSize(Dimension *d) {
	if (d != NULL)
	  setSize(d->width, d->height);
}


PUBLIC void Component::setBounds(int x, int y, int width, int height) {

	trace("Component::setBounds %d %d %d %d", x, y, width, height);

	mBounds.x = x;
	mBounds.y = y;
	mBounds.width = width;
	mBounds.height = height;
	updateNativeBounds();
}

PUBLIC void Component::setBounds(Bounds* b) {
	mBounds.x = b->x;
	mBounds.y = b->y;
	mBounds.width = b->width;
	mBounds.height = b->height;
	delete b;
	updateNativeBounds();
}

PUBLIC Bounds* Component::getBounds()
{
	return &mBounds;
}

PUBLIC void Component::setName(const char *s) {
    delete mName;
    mName = CopyString(s);
}

PUBLIC const char *Component::getName()
{
    return mName;
}

PUBLIC void Component::setForeground(Color* c) 
{
	//trace("Component::setForeground");
	mForegroundColorChanged = mForeground != NULL && !(mForeground == c);  // #007
	mForeground = c;
}

PUBLIC Color* Component::getForeground() 
{
	return mForeground;
}

PUBLIC void Component::setBackground(Color* c) 
{
	mBackground = c;
}

PUBLIC Color* Component::getBackground() 
{
	return mBackground;
}

PUBLIC void Component::setToolTip(const char *s) 
{
    delete mToolTip;
    mToolTip = CopyString(s);
}

PUBLIC const char *Component::getToolTip()
{
    return mToolTip;
}

PUBLIC Dimension* Component::getCurrentPreferredSize()
{
	return mPreferred;
}

/**
 * Calculate the preferred size for the component.
 * This is ordinarilly overloaded.
 */
PUBLIC Dimension* Component::getPreferredSize(Window* w)
{
	if (mPreferred == NULL) {
		// should have been overloaded, caller expects something
		mPreferred = new Dimension();
	}
	return mPreferred;
}

PUBLIC void Component::setPreferredSize(Dimension *d)
{
	delete mPreferred;
	mPreferred = d;
}

PUBLIC void Component::setPreferredSize(int width, int height)
{
	setPreferredSize(new Dimension(width, height));
}

PUBLIC Dimension* Component::getMinimumSize()
{
	return mMinimum;
}

PUBLIC void Component::setMinimumSize(Dimension *d)
{
	delete mMinimum;
	mMinimum = d;
}

PUBLIC Dimension* Component::getMaximumSize()
{
	return mMaximum;
}

PUBLIC void Component::setMaximumSize(Dimension *d)
{
	delete mMaximum;
	mMaximum = d;
}

/**
 * Locate the nearest Container that has a native peer.
 */
PUBLIC Container* Component::getNativeParent()
{
	return getNativeParent(this);
}

PUBLIC Container* Component::getNativeParent(Component* c)
{
	Container* parent = NULL;
	if (c != NULL) {
		parent = c->getParent();
		while (parent != NULL) {
			if (!parent->isNativeParent())
			  parent = parent->getParent();
			else
			  break;
		}
	}
	return parent;
}

/**
 * Calculate the actual x/y position of the native component
 * factoring in containment by lightweight containers that have no 
 * native handle.
 *
 * NOTE: This must be used only when creating and moving
 * components with a native peer.  For custom components that
 * draw themselves with the paint() method, you must use 
 * getPaintBounds().  This is a kludge for Mac, we do not properly
 * implement drawing into user panes which is what we create in the
 * PanelUI.  Now that we invalidate HIViews and receive paint events,
 * we should be able to fix it so the Graphics has the CGContext
 * of the UserPane and return to "normal".
 * 
 * I HATE THIS NAME
 *
 * getNativeBounds and updateNativeBounds are not symetrical.
 * updateNativeBounds moves the native component to a specific location
 * (with hooks for adjustment) what we're calculating here is the
 * offset of the Component (native or not) within the nearest native parent.
 * A better name would be getLocationInNativeParent
 * and getNativeBounds would be getBoundsInNativeParent().
 */
PUBLIC void Component::getNativeLocation(Point* p)
{
    p->x = 0;
    p->y = 0;
    getNativeLocation2(p);
}

/**

 * Inner recursive method, assumes nx/ny have been initialized.
 */
PRIVATE void Component::getNativeLocation2(Point* p)
{
    p->x += mBounds.x;
    p->y += mBounds.y;
    
    if (mParent != NULL) {
        if (!mParent->isNativeParent()) {
            // a lightweight container, or a component with a handle
            // that isn't our parent window 
            // recurse
            mParent->getNativeLocation2(p);
        }
    }
}

/**
 * Initialize a Bounds object with the bounds of this component
 * relative to its native parent window.
 *
 * NOTE: This must be used only when creating and moving
 * components with a native peer.  For custom components that
 * draw themselves with the paint() method, you must use 
 * getPaintBounds().  This is a kludge for Mac, we do not properly
 * implement drawing into user panes which is what we create in the
 * PanelUI.
 * 
 * !! Seems like we should be able to avoid this if we encapsulated
 * the native offsets in the Graphics as we traversed?
 */
PUBLIC void Component::getNativeBounds(Bounds* b)
{
    getNativeLocation(b);
    b->width = mBounds.width;
    b->height = mBounds.height;
}

/**
 * Kludge for Mac.
 * Initialize a Bounds object with the bounds of a custom component
 * that paints itself into a Graphics.
 */
PUBLIC void Component::getPaintBounds(Bounds* b)
{
	// so we don't have to add an isPaintParent to every UI
	// have a global flag in the UIManager
	if (!UIManager::isPaintWindowRelative()) {
		getNativeBounds(b);
	}
	else {
		b->x = 0;
		b->y = 0;
		b->width = mBounds.width;
		b->height = mBounds.height;
		getWindowLocation(b);
	}
}

/**
 * Kludge for Mac.
 * Inner recursive method to find the location of a component relative
 * to the window.
 */
PRIVATE void Component::getWindowLocation(Point* p)
{
	if (isWindow() == NULL) {
		p->x += mBounds.x;
		p->y += mBounds.y;
		if (mParent != NULL)
		  mParent->getWindowLocation(p);
	}
}

/**
 * Invalidate our rectangle so it will be repainted.
 * Usually this ends up generating an event that will be handled
 * in the window event loop.   For lightweight components that
 * draw themselves it will eventually end up calling the paint() method.
 * You must never call paint from outside the window event management
 * thread.  You can on Wndows but not on Mac so it must be handled 
 * consistently.
 * 
 * Note that lightweight comonents will have a NullUI which will
 * end walking up the parent stack until we find a native component.
 */
PUBLIC void Component::invalidate()
{
	ComponentUI* ui = getUI();
	ui->invalidate(this);
}

/**
 * Close the native component.
 * Normally called in preparation of removing this Component from
 * its parent.  
 * Not in swing, though we could try to do this as a side effect
 * of removing the parent?
 * 
 * Windows Note: DestroyWindow will automatically traverse and destroy
 * child windows so we don't have to do that here, but Containers DO need to 
 * call invalidateNativeHandle on the child components.
 */
PUBLIC void Component::close()
{
    ComponentUI* ui = getUI();
    ui->close();
}

/**
 * Invalidate the native handle.
 * This must be called in Windows for any child compontnts
 * that will be closed automatically when the native parent component
 * is closed.
 */
PUBLIC void Component::invalidateNativeHandle()
{
    ComponentUI* ui = getUI();
    ui->invalidateHandle();
}

/**
 * Should this be a general Component method?
 */
PUBLIC void Component::setEnabled(bool b) 
{
    mEnabled = b;
    ComponentUI* ui = getUI();
    ui->setEnabled(b);
}

PUBLIC bool Component::isSetEnabled()
{
	return mEnabled;
}

PUBLIC bool Component::isEnabled()
{
    ComponentUI* ui = getUI();
    if (ui->isOpen())
      mEnabled = ui->isEnabled();
	return mEnabled;
}

PUBLIC void Component::setVisible(bool b) 
{
    mVisible = b;
    ComponentUI* ui = getUI();
    ui->setVisible(b);
}

PUBLIC bool Component::isVisible()
{
    ComponentUI* ui = getUI();
    if (ui->isOpen())
      mVisible = ui->isVisible();
	return mVisible;
}

/**
 * Only for UIComponent to check initial visibility without
 * asking the component.  Used by getWindowClass to determine
 * if we should set WS_VISIBLE.
 */
PUBLIC bool Component::isSetVisible()
{
	return mVisible;
}

/**
 * Only for UIComponent to set initial invisibility after
 * the native handle has been created.  Can't use isVisible
 * because that asks the UI handle, here we want the initial
 * value set by the user.
 *
 * !! Hmm, this was added while flailing around with TabPannel,
 * on further inspection it looks like getWindowClass calling
 * isSetVisible should do the trick?
 */
PUBLIC void Component::initVisibility()
{
	// assume native components are visible by default, don't
	// disrupt the natural order of things unless we have to
	if (!mVisible)
	  setVisible(false);
}

/**
 * Swing calls this requestFocus.
 */
PUBLIC void Component::setFocusRequested(bool b) 
{
    mFocusRequested = b;
}

PUBLIC bool Component::isFocusRequested()
{
	return mFocusRequested;
}

/**
 * Return true if this component should be included in the tab focus
 * sequence for a window.  Normally overloaded in the subclass, but
 * if focus has been requested, assume it's focusable too.
 */
PUBLIC bool Component::isFocusable() 
{
	return mFocusRequested;
}

/**
 * Determine if a point is within range of this component.
 * Not in Swing.
 */
PUBLIC bool Component::isCovered(Point* p)
{
	Bounds* b = getBounds();
	int right = b->x + b->width;
	int bottom = b->y + b->height;

	bool covered = (p->x >= b->x && p->x < right &&
					p->y >= b->y && p->y < bottom);

	return covered;
}

/**
 * Ask for keyboard focus.  I never have understood how Swing handles this
 * but it seems relatively obvious for Windows.
 * Swing calls this requestFocus.
 *
 * Older code would assume tht if we had a null ComponentUI that
 * we were lightweight and propagated the call to the parent Window.
 * Not sure why.
 */
PUBLIC void Component::setFocus()
{
    ComponentUI* ui = getUI();
    ui->setFocus();
}

/**
 * Not in Swing, but handy to traverse hierarchies of named components.
 */
PUBLIC Component* Component::getComponent(const char* name)
{
	Component* found = NULL;
	if (name != NULL && mName != NULL && !strcmp(name, mName))
	  found = this;
	return found;
}

PUBLIC Listeners* Component::getActionListeners()
{
    return mActionListeners;
}

PUBLIC void Component::addActionListener(ActionListener* l) 
{
    if (mActionListeners == NULL)
	  mActionListeners = new Listeners();
	mActionListeners->addListener(l);
}

PUBLIC void Component::removeActionListener(ActionListener* l) {
    if (mActionListeners != NULL)
	  mActionListeners->removeListener(l);
}

PUBLIC void Component::fireActionPerformed(void* o) 
{
	if (mActionListeners != NULL)
      mActionListeners->fireActionPerformed(o);
}

PUBLIC void Component::fireActionPerformed() 
{
	fireActionPerformed(this);
}

PUBLIC void Component::addMouseListener(MouseListener* l) 
{
    if (mMouseListeners == NULL)
	  mMouseListeners = new Listeners();
	mMouseListeners->addListener(l);
}

PUBLIC void Component::addMouseMotionListener(MouseMotionListener* l) 
{
    if (mMouseMotionListeners == NULL)
	  mMouseMotionListeners = new Listeners();
	mMouseMotionListeners->addListener(l);
}

PUBLIC void Component::addKeyListener(KeyListener* l) 
{
    if (mKeyListeners == NULL)
	  mKeyListeners = new Listeners();
	mKeyListeners->addListener(l);
}

PUBLIC Component* Component::fireMouseEvent(MouseEvent* e) 
{
	Component* handler = NULL;

	if (e->getType() == MOUSE_EVENT_MOVED ||
        e->getType() == MOUSE_EVENT_DRAGGED) {
		if (mMouseMotionListeners != NULL) {
			mMouseMotionListeners->fireMouseMotionEvent(e);
            if (e->isClaimed())
              handler = this;
		}
	}
	else if (mMouseListeners != NULL) {
		mMouseListeners->fireMouseEvent(e);
        if (e->isClaimed())
          handler = this;
	}

	return handler;
}

/**
 * !! Not handling focus properly.  This will just blast events
 * to anything with a key listener.
 */
PUBLIC Component* Component::fireKeyEvent(KeyEvent* e) 
{
	Component* handler = NULL;

	if (mKeyListeners != NULL) {
		mKeyListeners->fireKeyEvent(e);
        if (e->isClaimed())
          handler = this;
	}
	return handler;
}

/**
 * Handy utility.
 */
PUBLIC void Component::sleep(int millis) 
{
	UIManager::sleep(millis);
}

PUBLIC Border* Component::getBorder()
{
    return mBorder;
}

PUBLIC void Component::setBorder(Border* b)
{
    mBorder = b;
    if (b != NULL) {
        // In Swing, Border takes precedence over Insets
        // Here, we could combine them?
        // To avoid allocating a new Insets every time, assume
        // the border object won't change and cache the insets.
        // note that this style won't let you setInsets, setBorder
        // remove the border and restore the previous insets
        if (mInsets == NULL)
            mInsets = new Insets();
        b->getInsets(this, mInsets);
    }
}

PUBLIC Insets* Component::getInsets()
{
    return mInsets;
}

PUBLIC void Component::setInsets(Insets* i)
{
    delete mInsets;
    mInsets = i;
}

PUBLIC void Component::setInsets(int left, int top, int right, int bottom)
{
	setInsets(new Insets(left, top, right, bottom));
}

PUBLIC void Component::addInsets(Dimension* d)
{
    if (mInsets != NULL) {
        d->width += mInsets->left + mInsets->right;
        d->height += mInsets->top + mInsets->bottom;
    }
}

/**
 * Redraw a lightweight component.
 * This must only be called from the main window event handling thread on Mac.
 * 
 * Not exactly swing, but it's how we get the border painted
 * before calling the overloaded paint(Graphics) method.
 */
PUBLIC void Component::paint()
{
    Window* w = getWindow();
    if (w != NULL) {
        Graphics* g = w->getGraphics();
        if (g != NULL) {
            paint(g);
			// let the border overwrite leakage from the main paint
            paintBorder(g);
        }
    }
}

/**
 * This must only be called from the main window event handling thread on Mac.
 * Ugh, I don't like having to remember to call paintBorder
 * in everyone's paint method.  Currently, Container::paint
 * will call it, as will the no-arg signature of Component::Paint.
 * This should be enough, but there are probably holes.
 */
PUBLIC void Component::paintBorder(Graphics* g)
{
    if (mBorder != NULL) {
        Bounds b;
        getPaintBounds(&b);
        mBorder->paintBorder(this, g, b.x, b.y, b.width, b.height);
    }
}

/**
 * Redraw a lightweight component.
 * This must only be called from the main window event handling thread on Mac.
 */
PUBLIC void Component::paint(Graphics* g) 
{
}

PUBLIC void Component::layout(Window* w)
{
}

/**
 * Walk backwards up the parent chain till we find a Window.
 */
PUBLIC Window* Component::getWindow()
{
	Window* window = isWindow();
	if (window == NULL && mParent != NULL)
	  window = mParent->getWindow();
	return window;
}

/** 
 * Overload if you don't want the Tab key to transfer focus
 * (such as in a TextArea)
 */
PUBLIC void Component::processTab()
{
    // will want to allow this in text areas!!
    // assume for now it always switches focus
    Window* root = getWindow();
    if (root != NULL) {
        // high order bit set when key is down
		bool shiftDown = UIManager::isKeyDown(KEY_SHIFT);
        int delta = ((shiftDown) ? -1 : 1);
        root->incFocus(delta);
        // should we call the default proc or just eat the event?
    }
}

/**
 * Overload if you don't want the Return key to close a dialog.
 */
PUBLIC bool Component::processReturn()
{
	bool handled = false;

    Window* root = getWindow();
    if (root != NULL) {
        Dialog* dialog = root->isDialog();
        if (dialog != NULL) {
			dialog->processReturn(this);
			handled = true;
		}
    }

	return handled;
}

/**
 * Overload if you don't want the Escape key to close a dialog (rare).
 */
PUBLIC bool Component::processEscape()
{
	bool handled = false;
    Window* root = getWindow();
    if (root != NULL) {
        Dialog* dialog = root->isDialog();
        if (dialog != NULL) {
			dialog->processEscape(this);
			handled = true;
		}
    }
	return handled;
}

//////////////////////////////////////////////////////////////////////
//
// Trace	
//
//////////////////////////////////////////////////////////////////////

bool Component::TraceEnabled = false;
bool Component::PaintTraceEnabled = false;
int Component::TraceLevel = 0;

PUBLIC const char* Component::getTraceClass()
{
	return mClassName;
}

PUBLIC const char* Component::getTraceName()
{
	return getName();
}

PUBLIC void Component::initTraceLevel()
{
	TraceLevel = 0;
}

PUBLIC void Component::incTraceLevel()
{
	TraceLevel += 2;
}

PUBLIC void Component::decTraceLevel()
{
	TraceLevel -= 2;
	if (TraceLevel < 0) TraceLevel = 0;
}

PUBLIC void Component::trace(const char *string, ...)
{
    va_list args;
    va_start(args, string);
	vtrace(string, args);
    va_end(args);
}

PUBLIC void Component::vtrace(const char *string, va_list args)
{
	if (TraceEnabled) {
		char buf[2048];
		vsprintf(buf, string, args);

		for (int i = 0 ; i < TraceLevel ; i++)
		  printf(" ");

		const char* cls = getTraceClass();
		const char* name = getTraceName();

		if (name == NULL)
		  printf("%s: %p - %s\n", cls, this, buf);
		else
		  printf("%s: %s - %s\n", cls, name, buf);

		fflush(stdout);
	}
}

PUBLIC void Component::tracePaint()
{
	if (PaintTraceEnabled) {
		// sigh override this so we don't have to set multiple flags
		bool saveTraceEnabled = TraceEnabled;
		TraceEnabled = true;
		trace("paint");
		TraceEnabled = saveTraceEnabled;
	}
}

PUBLIC void Component::debug()
{
	trace(" Component %d %d %d %d", getX(), getY(), getWidth(), getHeight());

	ComponentUI* ui = getUI();
	if (ui != NULL)
	  ui->debug();
}

//
// Older stuff
//

PUBLIC void Component::dump()
{
    dump(0);
}

PUBLIC void Component::dump(int indent)
{
    dumpLocal(indent);
}

PUBLIC void Component::dumpLocal(int indent)
{
    dumpType(indent, "Anonymous");
}

PUBLIC void Component::indent(int indent)
{
    for (int i = 0 ; i < indent ; i++)
      putc(' ', stdout);
}
    
PUBLIC void Component::dumpType(int i, const char *type)
{
    indent(i);

    fprintf(stdout, "%s: %d %d %d %d", type, 
			mBounds.x, mBounds.y, mBounds.width, mBounds.height);
	if (mPreferred != NULL)
	  fprintf(stdout, " preferred %d %d", 
			  mPreferred->width, mPreferred->height);
	fprintf(stdout, "\n");
	fflush(stdout);
}

QWIN_END_NAMESPACE

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

