/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * A very simple component that renders nothing but adds padding 
 * in both dimensions.
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
 *                                   STRUT                                  *
 *                                                                          *
 ****************************************************************************/

/**
 * Note that it is important that we maintain the dimension
 * in some place other than mPreferred.  The layout managers
 * expect to be able to null out mPreferred and have it recalculated.
 */
PUBLIC Strut::Strut()
{
	mClassName = "Strut";
    mDimension = new Dimension();
}

PUBLIC Strut::Strut(int width, int height)
{
	mClassName = "Strut";
    mDimension = new Dimension(width, height);
}

PUBLIC Strut::~Strut()
{
	delete mDimension;
}

PUBLIC ComponentUI* Strut::getUI() {
    if (mUI == NULL)
      mUI = UIManager::getNullUI();
    return mUI;
}

PUBLIC void Strut::setWidth(int i)
{
    if (mDimension == NULL)
      mDimension = new Dimension();
    mDimension->width = i;
}

PUBLIC void Strut::setHeight(int i)
{
    if (mDimension == NULL)
      mDimension = new Dimension();
    mDimension->height = i;
}

PUBLIC Dimension* Strut::getPreferredSize(Window* w)
{
	if (mPreferred == NULL) {
		mPreferred = new Dimension();
		if (mDimension != NULL) {
			mPreferred->width = mDimension->width;
			mPreferred->height = mDimension->height;
		}
	}
	return mPreferred;
}

QWIN_END_NAMESPACE

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
