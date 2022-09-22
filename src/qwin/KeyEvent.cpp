/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Model for keyboard events.
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
 *   							  KEY EVENT                                 *
 *                                                                          *
 ****************************************************************************/

PUBLIC KeyEvent::KeyEvent()
{
    init();
}

PUBLIC KeyEvent::~KeyEvent()
{
}

PUBLIC void KeyEvent::init()
{
	mComponent = NULL;
	mType = KEY_EVENT_DOWN;
    mModifiers = 0;
	mKeyCode = 0;
	mClaimed = false;
}

PUBLIC void KeyEvent::init(int modifiers, int key)
{
    init();
	mModifiers = modifiers;
	mKeyCode = key;
}

PUBLIC Component* KeyEvent::getComponent()
{
    return mComponent;
}

PUBLIC void KeyEvent::setComponent(Component* c)
{
    mComponent = c;
}

PUBLIC int KeyEvent::getType()
{
    return mType;
}

PUBLIC void KeyEvent::setType(int i) 
{
    mType = i;
}

PUBLIC int KeyEvent::getModifiers()
{
    return mModifiers;
}

PUBLIC void KeyEvent::setModifiers(int i) 
{
    mModifiers = i;
}

PUBLIC int KeyEvent::getKeyCode()
{
    return mKeyCode;
}

PUBLIC void KeyEvent::setKeyCode(int i) 
{
    mKeyCode = i;
}

PUBLIC int KeyEvent::getRepeatCount()
{
    return mRepeatCount;
}

PUBLIC void KeyEvent::setRepeatCount(int i) 
{
    mRepeatCount = i;
}

PUBLIC bool KeyEvent::isClaimed()
{
	return mClaimed;
}

PUBLIC void KeyEvent::setClaimed(bool b)
{
	mClaimed = b;
}

/**
 * Get the full key code by combining modifiers
 * and the base key code.
 */
PUBLIC int KeyEvent::getFullKeyCode()
{
	return (mModifiers | mKeyCode);
}

/**
 * Return true if this event represents a transition of
 * one of the modifier keys.
 */
PUBLIC bool KeyEvent::isModifier()
{
    return (mKeyCode == KEY_SHIFT || mKeyCode == KEY_CONTROL || 
            mKeyCode == KEY_MENU);
}

/**
 * Return true if this is one of the "toggle" keys,
 * you generally don't want to bind behavior to these.
 */
PUBLIC bool KeyEvent::isToggle()
{
    // what about VK_SCROLL "scroll lock"?
    return (mKeyCode == KEY_CAPITAL || mKeyCode == KEY_NUM_LOCK);
}

QWIN_END_NAMESPACE

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
