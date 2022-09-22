/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Parent class of all components that may contain other components.
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
 *   							  CONTAINER                                 *
 *                                                                          *
 ****************************************************************************/

PUBLIC Container::Container() 
{
    mLayoutManager = NULL;
    mComponents = NULL;
}

PUBLIC Container::~Container() 
{
    // hmm, might want to be able to reuse these since they're not
    // component specific
    delete mLayoutManager;

    delete mComponents;
}

PUBLIC Container* Container::isContainer() 
{
	return this;
}

PUBLIC LayoutManager* Container::getLayoutManager() 
{
	return mLayoutManager;
}

PUBLIC void Container::setLayout(class LayoutManager* lm) 
{
	mLayoutManager = lm;
}

PUBLIC Component* Container::getComponents() 
{
	return mComponents;
}

PUBLIC int Container::getComponentCount() 
{
    int count = 0;
    for (Component* c = getComponents() ; c != NULL ; c = c->getNext())
      count++;
    return count;
}

PUBLIC Component* Container::getComponent(int index) 
{
    Component* child = NULL;
    if (index >= 0) {
        int psn = 0;
        for (child = mComponents ; child != NULL && psn < index ; 
             child = child->getNext(), psn++);
    }
    return child;
}

/**
 * Not in swing, but handy and saves having a visitor.
 */
PUBLIC Component* Container::getComponent(const char* name)
{
	Component* found = Component::getComponent(name);
	if (found == NULL) {
        for (Component* c = mComponents ; c != NULL && found == NULL ;
			 c = c->getNext()) {
			found = c->getComponent(name);
		}
	}
	return found;
}

PUBLIC void Container::add(Component* c) 
{
	if (c != NULL) {
		// keep this ordered in case we need to do order dependnent layout
		Component *last = NULL;
		for (last = mComponents ; last != NULL && last->getNext() != NULL ; 
			 last = last->getNext());

		if (last != NULL)
		  last->setNext(c);
		else
		  mComponents = c;

		c->setParent(this);

		// if we're being added to an existing native hierarhcy,
		// flesh out the new child handles too
        if (isOpen())
          c->open();
 	}
}

/**
 * For consistency with Swing, let the layout manager region be
 * specified in the add method, though it will ultimately be 
 * stored as a field on the Component.
 */
PUBLIC void Container::add(Component* c, const char *constraints) {

    add(c);
    if (mLayoutManager != NULL)
      mLayoutManager->addLayoutComponent(c, constraints);
}

PUBLIC void Container::remove(Component *target) 
{
	Component *found = NULL;
	Component *prev = NULL;

	for (Component* c = mComponents ; c != NULL ; c = c->getNext()) {
		if (c != target)
		  prev = c;
		else {
			found = c;
			break;
		}
	}

	if (found != NULL) {
        // close it before removing from the hierarchy
		target->close();
		if (prev != NULL)
		  prev->setNext(target->getNext());
		else
		  mComponents = target->getNext();
		target->setNext(NULL);
		target->setParent(NULL);
	}
	else {
		// just ignore it?
		printf("Container::remove component not found!!\n");
	}
}

PUBLIC void Container::close()
{
	ComponentUI* ui = getUI();
	ui->close();

	// usually embeded native components will already have been closed
	// when the parent is closed (which internally calls invalidateNativeHandle)
	// but sweep over the tree looking for other close actions
	for (Component* child = mComponents ; child != NULL ; 
		 child = child->getNext())
	  child->close();
}

/**
 * Called indirectly by ComponentUI::close when it needs to clear
 * any native component handles in a hierarchy.
 */
PUBLIC void Container::invalidateNativeHandle()
{
    ComponentUI* ui = getUI();
    ui->invalidateHandle();

    for (Component* child = mComponents ; child != NULL ; 
         child = child->getNext())
      child->invalidateNativeHandle();
}

PUBLIC void Container::removeAll()
{
	for (Component* c = mComponents ; c != NULL ; c = c->getNext()) {
		c->close();
	}

	delete mComponents;
	mComponents = NULL;
}

PUBLIC void Container::setEnabled(bool b) 
{
    Component::setEnabled(b);

    if (!isNativeParent()) {
        // a lightweight container, have to forward to children
        for (Component* c = mComponents ; c != NULL ; c = c->getNext())
          c->setEnabled(b);
    }
}

PUBLIC void Container::setVisible(bool b) 
{
    Component::setVisible(b);
    if (!isNativeParent()) {
        // a lightweight container, have to forward to children
        for (Component* c = mComponents ; c != NULL ; c = c->getNext())
          c->setVisible(b);
    }
}

PUBLIC void Container::paint(Graphics* g)
{
    // If the parent is disabled, then you don't paint the children
    // !! what about "visible"?
    if (isEnabled()) {
        incTraceLevel();
        for (Component* c = mComponents ; c != NULL ; c = c->getNext()) {
            c->paint(g);
            c->paintBorder(g);
        }
        decTraceLevel();
    }
}

/**
 * Called during layout.  Depending on the layout manager
 * used by OUR container, we may get a size that is different than this.
 */
PUBLIC Dimension* Container::getPreferredSize(Window* w)
{
	if (mPreferred == NULL) {
		if (mLayoutManager != NULL)
		  mPreferred = mLayoutManager->preferredLayoutSize(this, w);
		else
		  mPreferred = NullLayout::nullPreferredLayoutSize(this, w);
	}
	return mPreferred;
}

PUBLIC void Container::layout(Window* w)
{
	// containers reset their preferred size during layout,
	// leaf components get to keep theirs
    // Swing does this with a "valid" flag, but the effect is the same
	setPreferredSize(NULL);

    if (mLayoutManager != NULL)
      mLayoutManager->layoutContainer(this, w);
	else
	  NullLayout::nullLayoutContainer(this, w);

	// did this originally, but this is what the LayoutManagers
	// are supposed to do
	//for (Component* c = mComponents ; c != NULL ; c = c->getNext())
	//c->layout(w);

	Dimension* d = getCurrentPreferredSize();
	if (d == NULL)
	  trace("No preferred size calculated!");
	else
	  trace("Preferred size %d %d", d->width, d->height);
}

/**
 * We can assume that the event is within range of this component
 * and the coordinates relative to our origin.  We're responsible
 * for checking the ranges of our child components.
 *
 * Return the depeest Component that was interested in the event.
 * This is used to implement mouse dragging events since we need
 * to know which component has mouse "focus".
 *
 * If any container/component sets the isClaimed flag in the event.
 * The event will not be propagated further.
 */
PUBLIC Component* Container::fireMouseEvent(MouseEvent* e) 
{
	Component* handler = NULL;

    // first we run
    handler = Component::fireMouseEvent(e);

	if (mComponents != NULL && handler == NULL) {
        int mousex = e->getX();
        int mousey = e->getY();

		for (Component* c = mComponents ; c != NULL && handler == NULL ; 
			 c = c->getNext()) {
			Bounds* b = c->getBounds();
			int right = b->x + b->width;
			int bottom = b->y + b->height;

			if (mousex >= b->x && mousex < right &&
				mousey >= b->y && mousey < bottom) {

                // adjust coordinates relative to component
                e->setX(mousex - b->x);
                e->setY(mousey - b->y);


                // stop on the first child that claims the event
                handler = c->fireMouseEvent(e);

                // restore event coordinates
                e->setX(mousex);
                e->setY(mousey);

			}
		}
	}

	return handler;
}

PUBLIC Component* Container::fireKeyEvent(KeyEvent* e) 
{
	Component* handler = NULL;

    // first we run
    handler = Component::fireKeyEvent(e);

	if (mComponents != NULL && handler == NULL) {

		for (Component* c = mComponents ; c != NULL && handler == NULL ; 
			 c = c->getNext()) {

			handler = c->fireKeyEvent(e);
		}
	}

	return handler;
}

/**
 * For lightweight containers, simply recurse on children.
 */
PUBLIC void Container::open()
{
	openChildren();
}

/**
 * Used by some ComponentUI's to open the children immediately after 
 * the native handle is open, or for Containers that want to overload
 * open() to have more control over order.
 */
PUBLIC void Container::openChildren()
{
 	for (Component* c = mComponents ; c != NULL ; c = c->getNext())
 	  c->open();
}

//////////////////////////////////////////////////////////////////////
//
// Trace
//
//////////////////////////////////////////////////////////////////////

PUBLIC void Container::debug()
{
	Component::debug();
	incTraceLevel();
 	for (Component* c = mComponents ; c != NULL ; c = c->getNext())
 	  c->debug();
	decTraceLevel();
}

PUBLIC void Container::dump(int indent)
{
    Component::dump(indent);
    indent += 2;
    for (Component *c = mComponents ; c != NULL ; c = c->getNext())
      c->dump(indent);
}

QWIN_END_NAMESPACE
/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
