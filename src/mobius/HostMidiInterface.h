/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * MOBIUS PUBLIC INTERFACE
 *
 * This is a kludge to allow the VST/AU wrappers to give
 * Mobius an object that can receive MIDI events with the expectation
 * that they be sent through the plugin intereface rather than
 * a MIDI device.
 *
 * Unfortunately necessary because MidiInterface isn't flexible enough
 * to model the plugin host input and output "ports".  HostInterface
 * and PluginInterface handle incomming MIDI events but have nothing
 * for outbound events.  
 *
 * Some options to consider:
 *
 *   Have VstMobius or MobiusPlugin create a MidiInterface that wraps
 *   the other one to add a VST pseudo-device.
 *
 *   Give Mobius a handle to HostInterface and extend it to have a
 *   midi send method.  
 *
 * Neither is pretty....think
 */

#ifndef HOST_MIDI_INTERFACE
#define HOST_MIDI_INTERFACE

class HostMidiInterface {

  public:

	virtual void send(class MidiEvent* e) = 0;

};

#endif
/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
