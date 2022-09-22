/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * A collection of support classes used to display transient shapes
 * that track mouse movement.  These aren't components, but probably
 * could be.  These are created by an application class (typically
 * a Panel subclass) in reponse to a mouse event.
 *
 * DragBox
 *     A fixed size rectangle that tracks the mouse.  Typically
 *     used for the outline of something that is being dragged to
 *     a new location.
 *
 * DragRegion
 *     A variable size anchored rectangle with one corner tracking
 *     the mouse.  Typically used to outline an object whose size 
 *     is being changed, or to outline a selection region.
 * 
 * Here we depart completely from Swing. I haven't thought much 
 * about how you would model something similar.  The Graphics object
 * has the necessary primitives, but the restrictions on where
 * you can paint (only in the Swing event thread) don't apply here.  
 * In Windows, you can get a Device Context at any time.
 *
 * It might not be too bad, since we only draw dudring processing of 
 * mouse events which would logically be "in the Swing thread".  
 * But note that we are outside of the WM_PAINT message.
 * I tried deferring this to the WM_PAINT message, but it got ugly
 * as you have to save both the current and new location so you can
 * XOR or erase as you move.
 *
 * !! This may not work properly on Mac since it will draw outside
 * of a kEventControlDraw event, usually a mouse event.  Since
 * we're drawing relative to the window it seems to work but it would
 * be "better" if we had a HIView that could mange this.
 *
 */

#include <stdio.h>
#include <memory.h>

#include "Util.h"
#include "QwinExt.h"

QWIN_BEGIN_NAMESPACE

/****************************************************************************
 *                                                                          *
 *                                  DRAGABLE                                *
 *                                                                          *
 ****************************************************************************/
/*
 * Common interface for all dragable objects.
 */

PUBLIC Dragable::Dragable()
{
    mParent = NULL;
    mLeft = 0;
    mTop = 0;

    // since all other calculates are now done with width/height
    // can we do that here too?
    mRight = 0;
    mBottom = 0;
}

PUBLIC Dragable::~Dragable()
{
}

PRIVATE void Dragable::getBounds(Bounds* b)
{
	if (mLeft < mRight) {
		b->x = mLeft;
		b->width = mRight - mLeft + 1;
	}
	else {
		b->x = mRight;
		b->width = mLeft - mRight + 1;
	}

	if (mTop < mBottom) {
		b->y = mTop;
		b->height = mBottom - mTop + 1;
 	}
	else {
		b->y = mBottom;
		b->height = mTop - mBottom + 1;
	}
}

PRIVATE void Dragable::paintRect(Graphics* g)
{
    Bounds b;
    getBounds(&b);

	if  (b.width > 0 || b.height > 0) {
        g->setColor(Color::Red);
		g->setXORMode();
		g->drawRect(b.x, b.y, b.width, b.height);
	}
}

/****************************************************************************
 *                                                                          *
 *                                DRAG REGION                               *
 *                                                                          *
 ****************************************************************************/

PUBLIC DragRegion::DragRegion(Component* parent, int x, int y)
{
    mParent = parent;
	mLeft = x;
	mTop = y;
	mRight = x;
	mBottom = y;
}

/**
 * This must be called from the main window event thread on Mac!!
 */
PUBLIC void DragRegion::trackMouse(int x, int y) 
{
    if (mLeft != x || mTop != y) {
        Window* w = mParent->getWindow();
        Graphics* g = w->getGraphics();
        paintRect(g);
        mLeft = x;
        mTop = y;
        paintRect(g);
    }
}

/**
 * This must be called from the main window event thread on Mac!!
 */
PUBLIC void DragRegion::finish()
{
    Window* w = mParent->getWindow();
    Graphics* g = w->getGraphics();
    paintRect(g);
}

/****************************************************************************
 *                                                                          *
 *                                  DRAG BOX                                *
 *                                                                          *
 ****************************************************************************/

PUBLIC DragBox::DragBox(Component* parent, int mouseX, int mouseY, 
                        int left, int top, int right, int bottom)
{
    mParent = parent;
    mLeft = left;
    mTop = top;
    mRight = right;
    mBottom = bottom;

    // the anchor offset is relative to the current mouse position
    mAnchorX = mouseX - left;
    mAnchorY = mouseY - top;

    // this should always be positive, but handle app errors
    if (mAnchorX < 0) mAnchorX = 0;
    if (mAnchorY < 0) mAnchorY = 0;
}

/**
 * This must be called from the main window event thread on Mac!!
 */
PUBLIC void DragBox::trackMouse(int x, int y) 
{
    if (mLeft != x || mTop != y) {
        Window* w = mParent->getWindow();
        Graphics* g = w->getGraphics();
        paintRect(g);

        int width = mRight - mLeft;
        int height = mBottom - mTop;

        mLeft = x - mAnchorX;
        mTop = y - mAnchorY;

        if (mLeft < 0) {
            mLeft = 0;
            mAnchorX = x;
        }
        if (mTop < 0) {
            mTop = 0;
            mAnchorY = y;
        }
        mRight = mLeft + width;
        mBottom = mTop + height;

        paintRect(g);
    }
}

/**
 * This must be called from the main window event thread on Mac!!
 */
PUBLIC void DragBox::finish()
{
    Window* w = mParent->getWindow();
    Graphics* g = w->getGraphics();
    paintRect(g);
}

/****************************************************************************
 *                                                                          *
 *                               DRAG COMPONENT                             *
 *                                                                          *
 ****************************************************************************/

PUBLIC DragComponent::DragComponent(Component* parent, int mouseX, int mouseY,
                                    Component* c)
{
    mParent = parent;
    mLeft = c->getX();
    mTop = c->getY();
    mRight = mLeft + c->getWidth();
    mBottom = mTop + c->getHeight();

    // the anchor offset is relative to the current mouse position
    mAnchorX = mouseX - mLeft;
    mAnchorY = mouseY - mTop;

    // this should always be positive, but handle app errors
    if (mAnchorX < 0) mAnchorX = 0;
    if (mAnchorY < 0) mAnchorY = 0;

    mComponent = c;
}

/**
 * This must be called from the main window event thread on Mac!!
 */
PUBLIC void DragComponent::trackMouse(int x, int y)
{
    // similar to the calculations we do in DragBox::paint, 
    // but here we're just moving the component


    int left = x - mAnchorX;
    int top = y - mAnchorY;
        
    if (left < 0) {
        left = 0;
        mAnchorX = x;
    }
    if (top < 0) {
        mTop = 0;
        mAnchorY = y;
    }

    // Moving a lightweight component doesn't automatically set
    // it's previous area to the background color, perhaps
    // the setLocation method should be overloaded to do this?
    // This may work correctly for child window controls, not sure.

    // !! if there is no background, should have a default
    Color* c = mParent->getBackground();
    if (c != NULL) {
        Window* w = mParent->getWindow();
        Graphics* g = w->getGraphics();
        if (g != NULL) {
            Bounds b;
            mComponent->getPaintBounds(&b);
            g->setColor(c);
            g->drawRect(b.x, b.y, b.width, b.height);
        }
    }

	// !! if this is a lightweight container containing
	// heavyweight components, setLocation will not recurse
	// and move the heavyweight components
    mComponent->setLocation(left, top);

    mComponent->invalidate();
}

PUBLIC void DragComponent::finish()
{
}

QWIN_END_NAMESPACE
/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
