/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Generic key constants and key mapping utilities.
 * Broken out of qwin.h so we can use it in code that doesn't use qwin
 * (like VstVirtualKey key mapping).
 * 
 * On Windows the KEY_ constants should be the same as the similarly named  VK_ 
 * constants but I don't want to be dependent on WinUser.h.
 *
 * Mac key codes don't correspond to Windows VK_ codes so we need
 * a mapping table.
 * 
 * Number and letter codes are assumed to be ASCII, you do not need
 * to use the constants:
 * 
 * KEY_0 thru KEY_9 are the same as ASCII '0' thru '9' (0x30 - 0x39)
 * KEY_A thru KEY_Z are the same as ASCII 'A' thru 'Z' (0x41 - 0x5A)
 * 
 * All KEY_ constants need to match entries in the KeyNames array.
 *
 * The KEY_MOD_ constants are used when combining key codes with 
 * modifier keys to produce a single integer code.  The are designed
 * as a bit mask so they may be combined.
 */

#ifndef KEYCODE_H
#define KEYCODE_H

/**
 * Given a by KEY_ constant return a displayable name.
 * If the code contains modifier bits they are removed.
 */
extern const char* GetKeyName(int code);

/**
 * Given a key code which may or may not include modifier bits,
 * render a readable name into the buffer.
 */
extern void GetKeyString(int code, char* buffer);

/**
 * Given a string possibily containing a virtual key name, 
 * convert it back into a code.
 */
extern int GetKeyCode(const char* name);

/**
 * Convert an OS specific key code into the generic code.
 */
extern int TranslateKeyCode(int raw);

/**
 * Convert the three parts of a VstKeyCode into our compound
 * key code with modifiers.
 */
extern int TranslateVstKeyCode(int raw, int virt, int modifier);


/**
 * Key modifier bit constants.
 * These may combined with a KEY_ code.
 */
#define KEY_MOD_SHIFT 0x100
#define KEY_MOD_CONTROL 0x200
#define KEY_MOD_ALT 0x400
#define KEY_MOD_COMMAND 0x800

/**
 * This will be the largest key code combined with modifiers.
 * Useful when allocating arrays to track key state.
 * Lookup tables must therefore be 4096 long.
 */
#define KEY_MAX_CODE 0xFFF

/**
 * Key modifier key codes.
 * Do not confuse these with the KEY_MOD_ constants.
 * These are the key codes that will come from the OS in a key event,
 * they are not ORable like the KEY_MODE_ constants are.
 * Windows sends these in key events not sure about Mac.
 * 
 */
#define KEY_SHIFT 0x10
#define KEY_CONTROL 0x11
#define KEY_MENU 0x12
#define KEY_CAPITAL 0x14

/**
 * Regular (non-modifier) key codes.
 */
#define KEY_BACK 0x8
#define KEY_TAB 0x9
// Windows defines this but I don't know what key generates it
// Mac has it on the number pad
#define KEY_CLEAR 0xC
#define KEY_RETURN 0xD
// Mac only
#define KEY_NUMEQUAL 0xE
#define KEY_PAUSE 0x13
#define KEY_ESCAPE 0x1B

#define KEY_SPACE 0x20
// WinUser.h calls the next two VK_PRIOR and VK_NEXT
#define KEY_PAGEUP 0x21
#define KEY_PAGEDOWN 0x22
#define KEY_END 0x23
#define KEY_HOME 0x24
#define KEY_LEFT 0x25
#define KEY_UP 0x26
#define KEY_RIGHT 0x27
#define KEY_DOWN 0x28
#define KEY_SELECT 0x29
#define KEY_PRINT 0x2A
// KEY_EXECUTE defined in winnt.h, not sure what it's for
#define KEY_EXEC 0x2B
#define KEY_SNAPSHOT 0x2C
#define KEY_INSERT 0x2D
#define KEY_DELETE 0x2E
#define KEY_HELP 0x2F

#define KEY_0 0x30
#define KEY_1 0x31
#define KEY_2 0x32
#define KEY_3 0x33
#define KEY_4 0x34
#define KEY_5 0x35
#define KEY_6 0x36
#define KEY_7 0x37
#define KEY_8 0x38
#define KEY_9 0x39

#define KEY_A 0x41
#define KEY_B 0x42
#define KEY_C 0x43
#define KEY_D 0x44
#define KEY_E 0x45
#define KEY_F 0x46
#define KEY_G 0x47
#define KEY_H 0x48
#define KEY_I 0x49
#define KEY_J 0x4A
#define KEY_K 0x4B
#define KEY_L 0x4C
#define KEY_M 0x4D
#define KEY_N 0x4E
#define KEY_O 0x4F

#define KEY_P 0x50
#define KEY_Q 0x51
#define KEY_R 0x52
#define KEY_S 0x53
#define KEY_T 0x54
#define KEY_U 0x55
#define KEY_V 0x56
#define KEY_W 0x57
#define KEY_X 0x58
#define KEY_Y 0x59
#define KEY_Z 0x5A
// could map these to the option or command keys
// but Windows has too much control over them
#define KEY_LWINDOWS 0x5B
#define KEY_RWINDOWS 0x5C

#define KEY_NUMPAD0 0x60
#define KEY_NUMPAD1 0x61
#define KEY_NUMPAD2 0x62
#define KEY_NUMPAD3 0x63
#define KEY_NUMPAD4 0x64
#define KEY_NUMPAD5 0x65
#define KEY_NUMPAD6 0x66
#define KEY_NUMPAD7 0x67
#define KEY_NUMPAD8 0x68
#define KEY_NUMPAD9 0x69
#define KEY_MULTIPLY 0x6A
#define KEY_ADD 0x6B
#define KEY_SEPARATOR 0x6C
// on Mac this is the numeric Enter key, on Windows numeric Enter is the
// same as VK_RETURN
#define KEY_NUMENTER 0x6C
#define KEY_SUBTRACT 0x6D
#define KEY_DECIMAL 0x6E
#define KEY_DIVIDE 0x6F

#define KEY_F1 0x70
#define KEY_F2 0x71
#define KEY_F3 0x72
#define KEY_F4 0x73
#define KEY_F5 0x74
#define KEY_F6 0x75
#define KEY_F7 0x76
#define KEY_F8 0x77
#define KEY_F9 0x78
#define KEY_F10 0x79
#define KEY_F11 0x7A
#define KEY_F12 0x7B
#define KEY_F13 0x7C
#define KEY_F14 0x7D
#define KEY_F15 0x7E
#define KEY_F16 0x7F

#define KEY_NUM_LOCK 0x90
#define KEY_SCROLL_LOCK 0x91

#define KEY_LSHIFT 0xA0
#define KEY_RSHIFT 0xA1
#define KEY_LCTRL 0xA2
#define KEY_RCTRL 0xA3
#define KEY_LMENU 0xA4
#define KEY_RMENU 0xA5

#define KEY_SEMI 0xBA
#define KEY_EQUAL 0xBB
#define KEY_COMMA 0xBC
#define KEY_HYPHEN 0xBD
#define KEY_DOT 0xBE
#define KEY_SLASH 0xBF

#define KEY_BACKQUOTE 0xC0

#define KEY_LBRACKET 0xDB
#define KEY_BACKSLASH 0xDC
#define KEY_RBRACKET 0xDD
#define KEY_APOS 0xDE

#endif
