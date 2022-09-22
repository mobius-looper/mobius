/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Listener "interfaces" for MIDI and timer events.
 * While these are needed by "level 2" classes like MidiTimer, they have
 * been factored out so they can be used in "level 3" classes
 * like MidiInterface.
 *
 */

#ifndef MIDI_LISTENER_H
#define MIDI_LISTENER_H

/**
 * The interface of an object that may be registered MidiInterface
 * to recieve individual MIDI events.
 * This is only used by MidiInterface so it cold be moved there.
 */
class MidiEventListener {

  public:

	virtual void midiEvent(class MidiEvent* e) = 0;

};

/**
 * The interface of an object that may be registered with a Timer or MidiInterface
 * to receive MIDI clock callbacks.  These aren't events, you will
 * be called in the timer interrupt whenever a MIDI clock is advanced,
 * and optionally when a MIDI clock is sent to the MIDI output device.
 */
class MidiClockListener {

  public:

	virtual void midiClockEvent() = 0;
	virtual void midiStartEvent() = 0;
	virtual void midiStopEvent() = 0;
	virtual void midiContinueEvent() = 0;

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
