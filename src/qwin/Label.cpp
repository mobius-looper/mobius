/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Simple static text.
 *
 */

#include <stdio.h>
#include <memory.h>

#include "Util.h"
#include "Qwin.h"

QWIN_BEGIN_NAMESPACE

/****************************************************************************
 *                                                                          *
 *   								LABEL                                   *
 *                                                                          *
 ****************************************************************************/

PUBLIC Label::Label() 
{
	init(NULL);
}

PUBLIC Label::Label(const char *s) 
{
	init(s);
}

PUBLIC Label::Label(const char *s, Color* fore) 
{
	init(s);
    setForeground(fore);
}

PRIVATE void Label::init(const char* s)
{
	mClassName = "Label";
	mColumns = 0;
    setText(s);

	// !! lightweights draw ugly on Mac, force them to static text controls
	// there are issues besides just anti-aliasing, the size calculations
	// also aren't quite right, need to fix this ...
#ifdef _WIN32	
    mHeavyweight = false;
#else
    mHeavyweight = true;
#endif
}

PUBLIC Label::~Label()
{
}

PUBLIC void Label::setColumns(int c)
{
	mColumns = c;
}

PUBLIC void Label::setHeavyweight(bool b) 
{
	mHeavyweight = b;
}

PUBLIC bool Label::isHeavyweight() 
{
	return mHeavyweight;
}

/**
 * This should never be necessary for labels, even if they
 * are heavyweight!!  Unfortunately it inherits from Static
 * which claims to be a Container.
 */
PUBLIC bool Label::isNativeParent() 
{
	bool isParent = false;
	if (mHeavyweight) {
		// defer to the 	
		isParent = Component::isNativeParent();
	}
	return isParent;
}

/**
 * Originally implemented these with just a paint method,
 * but it is more convenient for resizing to let these
 * be static components. Keep the old implementation around
 * for awhile just in case it turns out to be useful.
 */
PUBLIC void Label::paint(Graphics* g)
{
	if (!mHeavyweight && mText != NULL) {
		tracePaint();

        // formerly calling this, Graphics should support it?
        //SaveDC(dc);

        g->setColor(mForeground);
        g->setBackgroundColor(mBackground);
        g->setFont(mFont);
		
		Bounds b;
        getPaintBounds(&b);

        TextMetrics* tm = g->getTextMetrics();
		g->drawString(mText, b.x, b.y + tm->getAscent());

        // RestoreDC(dc, -1);
	}

	// used when debugging text sizes
	//Bounds b;
	//getPaintBounds(&b);
	//g->drawRect(b.x, b.y, mBounds.width, mBounds.height);
}

PUBLIC Dimension* Label::getPreferredSize(Window* w)
{
	if (mPreferred == NULL) {
		if (!mHeavyweight) {
			mPreferred = new Dimension();
			w->getTextSize(mText, mFont, mPreferred);
			//char buf[128];
			//sprintf(buf, "Label preferred %d %d\n", 
			//mPreferred->width, mPreferred->height);
			//OutputDebugString(buf);
		}
		else {
			// call the inherited method for side effect
			(void)Static::getPreferredSize(w);
		}
		
		if (mColumns > 0) {
			// a minmum column width specified for variable labels
			Dimension em;
			w->getTextSize("M", mFont, &em);
			int min = em.width * mColumns;
			if (mPreferred->width < min)
			  mPreferred->width = min;
		}

	}
	return mPreferred;
}

PUBLIC void Label::open()
{
    if (mHeavyweight)
	  Static::open();
}

PUBLIC void Label::dumpLocal(int indent)
{
    dumpType(indent, "Label");
}

QWIN_END_NAMESPACE

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

