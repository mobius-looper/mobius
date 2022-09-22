/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 *
 * Misc MIDI and/or music oriented utilities of common interest.
 *
 */

#ifndef MIDIUTIL_H
#define MIDIUTIL_H

INTERFACE void	MidiNoteName(int note, char *buf);
INTERFACE int	MidiNoteNumber(const char *str);

INTERFACE int	MidiChecksum(unsigned char *buffer, int len);

INTERFACE void	MidiGetName(unsigned char *src, int size, char *dest);
INTERFACE void	MidiSetName(unsigned char *dest, int size, char *src);

#endif
