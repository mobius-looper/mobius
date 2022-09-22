/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Misc independent MIDI utility functions.
 * 
 */

#include <stdio.h>
#include <ctype.h>

#include "Port.h"
#include "MidiUtil.h"

/****************************************************************************
 * MidiNoteName
 *
 * Arguments:
 *	note: note number
 *	 buf: output buffer
 *
 * Returns: none
 *
 * Description: 
 * 
 * Renders a printed representation of the note number into a buffer.
 ****************************************************************************/

PRIVATE const char *notenames[] = {"C ", "C#", "D ", "D#", "E ", "F ", 
								   "F#", "G ", "G#", "A ", "A#", "B "};

INTERFACE void MidiNoteName(int note, char *buf)
{
	int octave;

	octave = note / 12;
	note   = note - (octave * 12);
	sprintf(buf, "%s%d", notenames[note], octave - 2);
}

/****************************************************************************
 * MidiNoteNumber
 *
 * Arguments:
 *	str: symbolic note name
 *
 * Returns: note number
 *
 * Description: 
 *
 * Maps a symbolic key name into a MIDI key number.
 * The format recognized is:
 *
 *	note := <key-base>[<modifier>][-][<octave>]
 *
 *	key-base := C | D | E | F | G | A | B
 *
 *	modifier := b | # | bb | x
 *
 *	octave := 0 | 1 | 2 | 3 | 4
 *
 * The lowest MIDI note (zero) is "C-2", middle C is "C1".
 *
 ****************************************************************************/

INTERFACE int MidiNoteNumber(const char *str)
{
	const char *key_base_chars = "CDEFGAB";			// base names
	int key_bases[] = {0, 2, 4, 5, 7, 9, 11};		// semitone offsets
	int ch, i, base, modifier, octmod, octave, key;
	const char *ptr;

	key = -1;

	// match the first character of the note name with a base char
	base = -1;
	ptr = str;
	ch = toupper(*ptr);
	for (i = 0 ; i < 8 && base == -1 ; i++) {
		if (key_base_chars[i] == ch)
		  base = i;
	}

	if (base != -1) {
    
		// advance the pointer and parse the modifier
		ptr = str + 1;
		modifier = 0;
		if (*ptr == 'b') {
			modifier = -1;
			ptr++;
			if (*ptr == 'b') {
				/* double flat */
				modifier = -2;
				ptr++;
			}
		}
		else if (*ptr == '#') {
			modifier = 1;
			ptr++;
		}
		else if (*ptr == 'x') {
			modifier = 2;
			ptr++;
		}

		/* parse the octave modifier */
		octmod = 1;
		if (*ptr == '-') {
			octmod = -1;
			ptr++;
		}
    
		// Parse the octave number, if there is none, ignore the modifier
		// and select the octave beginning with middle C.
		// Calculate the MIDI note number of the desired octave root.

		if (!isdigit(*ptr))
		  octave = 3 * 12;
		else {
			octave = (*ptr - '0');	/* could use atol */
			/* factor in the modifier */
			octave *= octmod;
			if (octave >= -2 && octave <= 4) 
			  octave = (octave + 2) * 12;
			else
			  octave = 3 * 12;
		}

		// finally calculate the note number by combining the octave,
		// key base, and modifiers.

		key = octave + key_bases[base] + modifier;
	}

	return key;
}

/****************************************************************************
 * MidiChecksum
 *
 * Arguments:
 *	buffer: buffer to examine
 *	   len: length of buffer
 *
 * Returns: checksum number
 *
 * Description: 
 * 
 * Yamaha style sysex buffer checksum calculator.
 ****************************************************************************/

INTERFACE int MidiChecksum(unsigned char *buffer, int len)
{
	unsigned char check;
	unsigned int sum;
	int i;

	sum = 0;
	for (i = 0 ; i < len ; i++) 
	  sum += (unsigned int) buffer[i];

	check = (0 - sum) & 0x7F;

	return check;
}

/****************************************************************************
 * MidiGetName
 *
 * Arguments:
 *	 src: buffer to examine
 *	size: size of name
 *	dest: output buffer
 *
 * Returns: none
 *
 * Description: 
 * 
 * Extracts a name from a typical MIDI bulk dump.  The name
 * is within a fixed field and not necessarily zero terminated.
 ****************************************************************************/

INTERFACE void MidiGetName(unsigned char *src, int size, char *dest)
{
	int i;

	for (i = 0 ; i < size ; i++)
	  dest[i] = src[i];
	dest[i] = '\0';
}

/****************************************************************************
 * MidiSetName
 *
 * Arguments:
 *	dest: output buffer
 *	size: max field size
 *	 src: name we want to assign
 *
 * Returns: none
 *
 * Description: 
 * 
 * Places a zero terminated ASCII string into a MIDI bulk name field.
 * The name is blank padded if necessary.
 ****************************************************************************/

INTERFACE void MidiSetName(unsigned char *dest, int size, char *src)
{
	int i;

	for (i = 0 ; i < size && src[i] != '\0' ; i++)
	  dest[i] = src[i];

	for ( ; i < size ; i++)
	  dest[i] = ' ';
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
