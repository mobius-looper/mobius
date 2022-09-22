/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * MOBIUS PUBLIC INTERFACE
 *
 * Model for a 16 channel MIDI input or output port.
 *
 */

#ifndef MIDI_PORT_H
#define MIDI_PORT_H

/**
 * These represent 16-channel input or output ports.
 * A given physical device may have several ports.  We don't model the
 * distinction between an device and its ports. We just flatten the port list.
 * If you display them in order they will be grouped by device.
 *
 * The primary identifier for a port is the name.
 * We have also historicalky supported a numeric id but this
 * is not used on the Mac and needs to be removed.
 */
class MidiPort {

  public:

	MidiPort();
	MidiPort(const char *name);
	MidiPort(const char *name, int id);
	~MidiPort();

	MidiPort *getNext();
	void setNext(MidiPort *i);

	const char *getName();
	void setName(const char* name);

	int getId();
	void setId(int i);

    MidiPort* getPort(const char* name);
    MidiPort* getPort(int id);

	/**
	 * A convenience method to convert the list into an array of strings.
	 */
	char** getNames();

  private:

	void init();

	MidiPort *mNext;
	char *mName;
	int mId;

	// subclass may add platform-specific things

};

#endif
