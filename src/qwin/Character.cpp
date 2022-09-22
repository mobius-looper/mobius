/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Character utilities.
 * Doesn't really correspond to the Swing Character class yet, but
 * I needed someplace to hang the char/name conversion utilities.
 *
 * UPDATE: The key utiities have been mostly moved to KeyCode.cpp
 * so they can be independent of qwin.
 */

#include "Qwin.h"
#include "KeyCode.h"

QWIN_BEGIN_NAMESPACE

/**
 * Given a virtual key code, return a readable string representation.
 * This one does not handle modifiers.
 */
PUBLIC const char* Character::getString(int code)
{
    return ::GetKeyName(code);
}

/**
 * Return the printed represented of a key code, including modifiers.
 */
PUBLIC void Character::getString(int code, char* buffer)
{
    ::GetKeyString(code, buffer);
}

/**
 * Given a string possibily containing a virtual key name, 
 * convert it back into a code.
 */
PUBLIC int Character::getCode(const char* name)
{
    return ::GetKeyCode(name);
}

PUBLIC int Character::translateCode(int raw)
{
    return ::TranslateKeyCode(raw);
}

QWIN_END_NAMESPACE
/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
