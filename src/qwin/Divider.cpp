/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * An extended component that renders as a colored dividing line.
 * 
 */

#include <stdio.h>
#include <memory.h>

#include "Util.h"
#include "Qwin.h"
#include "QwinExt.h"

QWIN_BEGIN_NAMESPACE

PUBLIC Divider::Divider()
{
	initDivider();
}

PUBLIC Divider::Divider(int width)
{
	initDivider();
	setWidth(width);
}

PUBLIC Divider::Divider(int width, int height)
{
	initDivider();
	setWidth(width);
	setHeight(height);
}

PUBLIC void Divider::initDivider()
{
	mClassName = "Divider";
	setForeground(Color::Black);
	setBackground(Color::Black);
	setWidth(500);
	setHeight(2);
}

PUBLIC Divider::~Divider()
{
}

PUBLIC Dimension* Divider::getPreferredSize(Window* w)
{
	if (mPreferred == NULL) {
		mPreferred = new Dimension();
		mPreferred->width = getWidth();
		mPreferred->height = getHeight();
	}
	return mPreferred;
}

QWIN_END_NAMESPACE
