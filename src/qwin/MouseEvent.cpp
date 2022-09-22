/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Model for mouse events.
 *
 */

#include <stdio.h>
#include <memory.h>

#include "Util.h"
#include "KeyCode.h"
#include "Qwin.h"

QWIN_BEGIN_NAMESPACE

/****************************************************************************
 *                                                                          *
 *                                   MOUSE                                  *
 *                                                                          *
 ****************************************************************************/

PUBLIC MouseEvent::MouseEvent()
{
    init();
}

PUBLIC MouseEvent::~MouseEvent()
{
}

PUBLIC void MouseEvent::init()
{
    mType = 0;
    mButton = 0;
    mModifiers = 0;
    mClickCount = 0;
    mX = 0;
    mY = 0;
    mClaimed = false;
}

PUBLIC void MouseEvent::init(int type, int button, int x, int y)
{
    init();
    mType = type;
    mButton = button;
    mX = x;
    mY = y;
}

PUBLIC int MouseEvent::getType()
{
    return mType;
}

PUBLIC void MouseEvent::setType(int i) 
{
    mType = i;
}

PUBLIC int MouseEvent::getButton()
{
    return mButton;
}

PUBLIC void MouseEvent::setButton(int i) 
{
    mButton = i;
}

PUBLIC int MouseEvent::getModifiers()
{
    return mModifiers;
}

PUBLIC void MouseEvent::setModifiers(int i) 
{
    mModifiers = i;
}

PUBLIC bool MouseEvent::isShiftDown()
{
	return (mModifiers & KEY_MOD_SHIFT) > 0;
}

PUBLIC bool MouseEvent::isControlDown()
{
	return (mModifiers & KEY_MOD_CONTROL) > 0;
}

PUBLIC bool MouseEvent::isAltDown()
{
	return (mModifiers & KEY_MOD_ALT) > 0;
}

PUBLIC int MouseEvent::getClickCount()
{
    return mClickCount;
}

PUBLIC void MouseEvent::setClickCount(int i)
{
    mClickCount = i;
}

PUBLIC int MouseEvent::getX()
{
    return mX;
}

PUBLIC void MouseEvent::setX(int i)
{
    mX = i;
}

PUBLIC int MouseEvent::getY()
{
    return mY;
}

PUBLIC void MouseEvent::setY(int i)
{
    mY = i;
}

PUBLIC bool MouseEvent::isLeftButton()
{
    return (mButton == MOUSE_EVENT_BUTTON1);
}

PUBLIC bool MouseEvent::isRightButton()
{
    return (mButton == MOUSE_EVENT_BUTTON3);
}

PUBLIC void MouseEvent::setClaimed(bool b)
{
    mClaimed = b;
}

PUBLIC bool MouseEvent::isClaimed()
{
    return mClaimed;
}

QWIN_END_NAMESPACE

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

