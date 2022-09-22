/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Components that render as line borders around the container.
 */

#include <stdio.h>
#include <memory.h>

#include "Util.h"
#include "Qwin.h"

QWIN_BEGIN_NAMESPACE

/****************************************************************************
 *                                                                          *
 *                                 CONSTANTS                                *
 *                                                                          *
 ****************************************************************************/

PUBLIC Border* Border::BlackLine = new LineBorder(Color::Black, 1);
PUBLIC Border* Border::BlackLine2 = new LineBorder(Color::Black, 2);
PUBLIC Border* Border::WhiteLine = new LineBorder(Color::White, 1);
PUBLIC Border* Border::WhiteLine2 = new LineBorder(Color::White, 2);
PUBLIC Border* Border::RedLine = new LineBorder(Color::Red, 1);
PUBLIC Border* Border::RedLine2 = new LineBorder(Color::Red, 2);

/****************************************************************************
 *                                                                          *
 *                                   BORDER                                 *
 *                                                                          *
 ****************************************************************************/

PUBLIC Border::Border()
{
	mInsets.left = 0;
	mInsets.top = 0;
	mInsets.right = 0;
	mInsets.bottom = 0;
	mThickness = 0;
}

PUBLIC Border::~Border()
{
}

PUBLIC void Border::setThickness(int i)
{
    mThickness = i;
}

PUBLIC void Border::setInsets(int left, int top, int right, int bottom)
{
	mInsets.left = left;
	mInsets.top = top;
	mInsets.right = right;
	mInsets.bottom = bottom;
}

/**
 * This allocate a new Insets object every time, so it is practically
 * useless for C++, use the other signature.
 */
PUBLIC Insets* Border::getInsets(Component* c)
{
    Insets* i = new Insets();
    getInsets(c, i);
    return i;
}

PUBLIC void Border::getInsets(Component* c, Insets* i)
{
    i->left = mInsets.left + mThickness;
    i->top = mInsets.top + mThickness;
    i->right = mInsets.right + mThickness;
    i->bottom = mInsets.bottom + mThickness;
}

PUBLIC bool Border::isBorderOpaque()
{
    return false;
}

/****************************************************************************
 *                                                                          *
 *                                LINE BORDER                               *
 *                                                                          *
 ****************************************************************************/

PUBLIC LineBorder::LineBorder()
{
    init();
}

PUBLIC LineBorder::LineBorder(Color* c)
{
    init();
    setColor(c);
}

PUBLIC LineBorder::LineBorder(Color* c, int thickness)
{
    init();
    setColor(c);
    setThickness(thickness);
}

PUBLIC LineBorder::LineBorder(Color* c, int thickness, bool rounded)
{
    init();
    setColor(c);
    setThickness(thickness);
    setRoundedCorners(rounded);
}

PUBLIC void LineBorder::init()
{
    mColor = NULL;
    mRounded = false;

	// add some air between the border and the content
	setInsets(2, 2, 2, 2);
}

PUBLIC LineBorder::~LineBorder()
{
}

PUBLIC void LineBorder::setColor(Color* c)
{
    mColor = c;
}

PUBLIC void LineBorder::setRoundedCorners(bool b)
{
    mRounded = b;
}

PUBLIC void LineBorder::paintBorder(Component* c, Graphics* g, 
                                    int x, int y, int width, int height)
{
    Color* oldColor = g->getColor();
    int i;

    // here's how swing does it, assuming a single pixel pen
    // Windows allows you to set the pen width which would
    // be more effecient

    if (mColor != NULL)
        g->setColor(mColor);

    for (i = 0; i < mThickness; i++)  {
        // note that in AWT we would need an extra 1+ in the adjustment
        // due to AWT's silly inconsistency in draw method coordinate handling
        int adjust = (i * 2);
        int rx = x + i;
        int ry = y + i;
        int rw = width - adjust;
        int rh = height - adjust;

	    if (!mRounded)
          g->drawRect(rx, ry, rw, rh);
	    else
          g->drawRoundRect(rx, ry, rw, rh, mThickness, mThickness);
    }

    g->setColor(oldColor);
}

QWIN_END_NAMESPACE

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
