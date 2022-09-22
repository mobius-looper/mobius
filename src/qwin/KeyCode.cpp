/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Character mapping utilities.  This file is independent of Qwin
 * so it can be used in places that don't want to drag in Qwin,
 * mostly VST/AU plugin code that does native key mapping.
 *
 * Windows key codes go out to F7. Allow the shift keys to be represented
 * with bits in the second byte.
 *
 *     1 Shift
 *     2 Control
 *     4 Alt
 *     8 Windows
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Port.h"
#include "Util.h"
#include "KeyCode.h"

/**
 * Modifier key names.
 */
#define MOD_SHIFT_NAME "Shift"
#define MOD_CTRL_NAME "Ctrl"
#define MOD_CONTROL_NAME "Control"
#define MOD_ALT_NAME "Alt"
#define MOD_COMMAND_NAME "Command"

/**
 * A mapping between generic key codes (KEY_*) and readable names.
 */
PUBLIC const char* KeyNames[] = {

    NULL,
    "LButton",
    "RButton",
    "Cancel",
    "MButton",
    NULL, 
    NULL, 
    NULL,
    "Back",
    "Tab",
    NULL,
    NULL,
    "Clear",
    "Return",
	// Mac Numeric equal key, Windows doesn't have this
    "Numeric Equal",
    NULL,

    "Shift",        // 10
    "Ctrl",
    "Menu",
    "Pause",
    "Capital",
    "Kana",
    NULL,
    "Junja",
    "Final",
    "Kanji",
    NULL,
    "Escape",
    "Convert",
    "NonConvert",
    "Accept",
    "ModeChange",

    "Space",        // 20
    "Page Up",      // VK_PRIOR
    "Page Down",    // VK_NEXT
    "End",
    "Home",
    "Left",
    "Up",
    "Right",
    "Down",
    "Select",
    "Print",
    "Execute",
    "Snapshot",
    "Insert",  
    "Delete",
    "Help",

    "0",            // 30
    "1",
    "2",
    "3",
    "4",
    "5",
    "6",
    "7",
    "8",
    "9",    
    NULL,
    NULL, 
    NULL, 
    NULL, 
    NULL, 
    NULL, 

    NULL,           // 40
    "A",            
    "B",
    "C",
    "D",
    "E",
    "F",
    "G",
    "H",
    "I",
    "J",
    "K",
    "L",
    "M",
    "N",
    "O",

    "P",        // 50
    "Q",
    "R",
    "S",
    "T",
    "U",
    "V",
    "W",
    "X",
    "Y",
    "Z",        // 0x5A
    "LWindows",
    "RWindows",
    "Apps",
    NULL,
    NULL,

    "Num 0",     // 60
    "Num 1",
    "Num 2",
    "Num 3",
    "Num 4",
    "Num 5",
    "Num 6",
    "Num 7",
    "Num 8",
    "Num 9",
    "Multiply",
    "Add",
    "Separator",
    "Subtract",
    "Decimal",
    "Divide",

    "F1",        // 70
    "F2",
    "F3",
    "F4",
    "F5",
    "F6",
    "F7",
    "F8",
    "F9",
    "F10",
    "F11",
    "F12",
    "F13",
    "F14",
    "F15",
    "F16",

    "F17",      // 80
    "F18",
    "F19",
    "F20",
    "F21",
    "F22",
    "F23",
    "F24",      // 0x87
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,

    "Num Lock",  // 90
    "Scroll Lock",
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,

    "LShift",       // A0
    "RShift",
    "LCtrl",
    "RCtrl",
    "LMenu",
    "RMenu",        // 0xA5
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,

    NULL,       // B0
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    ";",
    "=",
    ",",
    "-",
    ".",
    "/",

    "`",        // C0
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,


    NULL,       // D0
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    "[",
    "\\",
    "]",
    "'",
    NULL

    // E0

    // there are a few others defined, but there is a gap until 0xF6
    // and they seem obscure

};

#define MAX_KEY_NAME 0xDF

/**
 * Given a virtual key code, return a readable string representation.
 * This one does not handle modifiers.
 */
PUBLIC const char* GetKeyName(int code)
{
    const char* name = NULL;

	// remove modifiers so we can index
	code = code & 0xFF;

    if (code >= 0 && code <= MAX_KEY_NAME)
      name = KeyNames[code];
    return name;
}

/**
 * Return the printed represented of a key code, including modifiers.
 */
PUBLIC void GetKeyString(int code, char* buffer)
{
    const char* baseName = GetKeyName(code);

	strcpy(buffer, "");

	// add modifiers
	if (code & KEY_MOD_SHIFT)
	  strcat(buffer, MOD_SHIFT_NAME);

	if (code & KEY_MOD_CONTROL) {
		if (buffer[0] != 0)
		  strcat(buffer, "+");
		strcat(buffer, MOD_CTRL_NAME);
	}

	if (code & KEY_MOD_ALT) {
		if (buffer[0] != 0)
		  strcat(buffer, "+");
		strcat(buffer, MOD_ALT_NAME);
	}

	if (code & KEY_MOD_COMMAND) {
		if (buffer[0] != 0)
		  strcat(buffer, "+");
		strcat(buffer, MOD_COMMAND_NAME);
	}

	if (baseName != NULL) {
		if (buffer[0] != 0)
		  strcat(buffer, "+");
		strcat(buffer, baseName);
	}
}

/**
 * Given a string possibily containing a virtual key name, 
 * convert it back into a code.
 */
PUBLIC int GetKeyCode(const char* name)
{
    int code = 0;

    if (name != NULL) {

		// this is grossly inefficient

		if (strstr(name, MOD_SHIFT_NAME))
		  code |= KEY_MOD_SHIFT;

		if (strstr(name, MOD_CTRL_NAME) || strstr(name, MOD_CONTROL_NAME))
		  code |= KEY_MOD_CONTROL;

		if (strstr(name, MOD_ALT_NAME))
		  code |= KEY_MOD_ALT;

		if (strstr(name, MOD_COMMAND_NAME))
		  code |= KEY_MOD_COMMAND;

		// isolate the base key name
		int plus = LastIndexOf(name, "+");
		if (plus > 0)
		  name = &name[plus+1];

		int baseCode = 0;
        char ch = name[0];

        if (strlen(name) == 1 &&
			((ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'Z')))
          baseCode = (int)ch;
        else {
            // brute force lookup
            for (int i = 0 ; i <= MAX_KEY_NAME ; i++) {
                const char* mapname = KeyNames[i];
                if (mapname != NULL && !strcmp(mapname, name)) {
                    baseCode = i;
                    break;
                }
            }
        }

		// if unable to find a mapping, assume it is a number
        if (baseCode == 0)
		  baseCode = atoi(name);

		// finally, mix it with the modifiers
		code |= baseCode;
    }

    return code;
}

//////////////////////////////////////////////////////////////////////
//
// Windows
//
//////////////////////////////////////////////////////////////////////

#ifdef _WIN32

/**
 * No translation necessary.
 */
PUBLIC int TranslateKeyCode(int raw)
{
	return raw;
}

#endif

//////////////////////////////////////////////////////////////////////
//
// Mac
//
//////////////////////////////////////////////////////////////////////

#ifdef OSX

/**
 * Mac raw key code to KEY_ code map.
 */
int MacKeyMap[] = {

	// 0x00
    KEY_A,
    KEY_S,
    KEY_D,
    KEY_F,
    KEY_H,
    KEY_G,
    KEY_Z,
    KEY_X,
    KEY_C,
    KEY_V,
	0,	// 0xA
    KEY_B,
    KEY_Q,
    KEY_W,
    KEY_E,
    KEY_R,

	// 0x10
    KEY_Y,
    KEY_T,
    KEY_1,
    KEY_2,
    KEY_3,
    KEY_4,
    KEY_6,
    KEY_5,
    KEY_EQUAL,
    KEY_9,
    KEY_7,
    KEY_HYPHEN,
    KEY_8,
    KEY_0,
    KEY_RBRACKET,
    KEY_O,

	// 0x20
    KEY_U,
    KEY_LBRACKET,
    KEY_I,
    KEY_P,
    KEY_RETURN,
    KEY_L,
    KEY_J,
    KEY_APOS,
    KEY_K,
    KEY_SEMI,
    KEY_BACKSLASH,
    KEY_COMMA,
    KEY_SLASH,
    KEY_N,
    KEY_M,
    KEY_DOT,

	// 0x30
    KEY_TAB,
    KEY_SPACE,
    KEY_BACKQUOTE,
    KEY_BACK,
    0,
    KEY_ESCAPE,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,

	// 0x40
    0,
    KEY_DECIMAL,
    0,
    KEY_MULTIPLY,
    0,
    KEY_ADD,
    0,
    KEY_CLEAR,	// Windows has a code for this but I don't see it?
    0,
    0,
    0,
    KEY_DIVIDE,
    KEY_NUMENTER,
    0,
    KEY_SUBTRACT,
    0,

	// 0x50
    0,
    KEY_NUMEQUAL,	// mac only
    KEY_NUMPAD0,
    KEY_NUMPAD1,
    KEY_NUMPAD2,
    KEY_NUMPAD3,
    KEY_NUMPAD4,
    KEY_NUMPAD5,
    KEY_NUMPAD6,
    KEY_NUMPAD7,
    0, // 0x5a
    KEY_NUMPAD8,
    KEY_NUMPAD9,
    0,
    0,
    0,

	// 0x60
    KEY_F5,
    KEY_F6,
    KEY_F7,
    KEY_F3,
    KEY_F8,
    KEY_F9,
    0, // 0x66
    KEY_F11,
    0,
    KEY_F13,
    KEY_F16,
    0, // 0x6b
    0,
    KEY_F10,
    0,
    KEY_F12,

	// 0x70
    0,
    0,
    KEY_HELP,
    KEY_HOME,
    KEY_PAGEUP,
    KEY_DELETE,
    KEY_F4,
    KEY_END,
    KEY_F2,
	KEY_PAGEDOWN,
    KEY_F1, 
    KEY_LEFT,
    KEY_RIGHT,
    KEY_DOWN,
    KEY_UP,
    0,

	// 0x80
};

PUBLIC int TranslateKeyCode(int raw)
{
	int trans = 0;

	if (raw >= 0 && raw < 0x80)
	  trans = MacKeyMap[raw];

	return trans;
}

#endif

//////////////////////////////////////////////////////////////////////
//
// VST Key Mapping
//
//////////////////////////////////////////////////////////////////////

/**
 * Mapping between a VstVirtualKey and a KEY_ constant.
 * The enumeration is defined starting from 1.
 */
int VstVirtualKeyMap[] = {

    0,      // undefined
	KEY_BACK,
	KEY_TAB, 
	KEY_CLEAR,
	KEY_RETURN,
	KEY_PAUSE, 
	KEY_ESCAPE,
	KEY_SPACE, 
	0,          // VKEY_NEXT - what's this?
    KEY_END,
    KEY_HOME,

    // 11
    KEY_LEFT,
    KEY_UP,
    KEY_RIGHT,
    KEY_DOWN,
    KEY_PAGEUP,
    KEY_PAGEDOWN,

    // 17
    KEY_SELECT,
    KEY_PRINT,
    KEY_NUMENTER,      // VKEY_ENTER - Mac only
    KEY_SNAPSHOT,
    KEY_INSERT,
    KEY_DELETE,
    KEY_HELP,
    KEY_NUMPAD0,
    KEY_NUMPAD1,
    KEY_NUMPAD2,
    KEY_NUMPAD3,
    KEY_NUMPAD4,
    KEY_NUMPAD5,
    KEY_NUMPAD6,
    KEY_NUMPAD7,
    KEY_NUMPAD8,
    KEY_NUMPAD9,
    KEY_MULTIPLY,
    KEY_ADD,
    KEY_SEPARATOR,
    KEY_SUBTRACT,
    KEY_DECIMAL,
    KEY_DIVIDE,

    // 40
    KEY_F1,
    KEY_F2,
    KEY_F3,
    KEY_F4,
    KEY_F5,
    KEY_F6,
    KEY_F7,
    KEY_F8,
    KEY_F9,
    KEY_F10,
    KEY_F11,
    KEY_F12,

    // 52
    KEY_NUM_LOCK,     // only on Windows, shouldn't be comming in as a key event
    0,              // VKEY_SCROLL - scroll lock? only on Windows
    KEY_SHIFT,       // a modifier, don't usually get key code for this
    KEY_CONTROL,     // a modifier, don't usually get key code 
    0,              // VKEY_ALT - not defined on Windows, shouldn't see anyway

    KEY_NUMEQUAL    // VKEY_EQUALS - only on Mac keyboards
};

/**
 * Duplication of VstModifierKey without the dependency on the VST SDK.
 * I don't really like having these here but it's simple enough.
 */
#define VST_MODIFIER_SHIFT 1
#define VST_MODIFIER_ALTERNATE 2
#define VST_MODIFIER_COMMAND 4
#define VST_MODIFIER_CONTROL 8

/**
 * Convert the three parts of a VstKeyCode into a generic key code
 * with modifier bits.
 * 
 * VstKeyCode has three fields:
 *    long character;
 *    unsigned char virt;   // defined by VstVirtualKey
 *    unsidned char modifier; // defined by VstModifierKey
 *
 * Hosts normally do their own translation from native key codes
 * into VstVirtualKey and VstModifierKey.  Some hosts pass other
 * key codes in "character" but this seems to be unreliable.
 *
 */
int TranslateVstKeyCode(int raw, int virt, int modifier)
{
    int mykey = 0;

    if (raw > 0) {
        // this is unreliable, should just ignore these...
        mykey = TranslateKeyCode(raw);
    }
    else {
        mykey = VstVirtualKeyMap[virt];
    }

    if (modifier & VST_MODIFIER_SHIFT)
      mykey |= KEY_MOD_SHIFT;
 
    if (modifier & VST_MODIFIER_ALTERNATE) 
      mykey |= KEY_MOD_ALT;

    if (modifier & VST_MODIFIER_CONTROL) 
      mykey |= KEY_MOD_CONTROL;

    if (modifier & VST_MODIFIER_COMMAND) 
      mykey |= KEY_MOD_COMMAND;

    return mykey;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
