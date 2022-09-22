/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * The interface of a singleton object that manages MIDI and timer devices.
 * This will be subclassed for each platform.  The primary functions
 * are:
 *
 *    - be a factory and pool for MidiEvent objects 
 * 
 *    - provide a list of MidiPort objects representing the available
 *      endpoint devices
 *
 *    - be a factory for MidiTimer, MidiInput, and MidiOutput objects
 *      which are wrappers around the platform APIs
 *
 *    - track open devices and provide auto cleanup
 * 
 *
 * The newer MidiInterface.h interface is similar but at a slightly
 * higer level, it is built upon MidiEnv.
 *
 * I considered using PortMIDI here but it looks way more hacky
 * than PortAudio.  The Mac CoreMIDI isn't that complicated and the
 * windows stuff I've had for ages so I trust it.
 * 
 */

#ifndef MIDI_ENV_H
#define MIDI_ENV_H

#include "MidiEvent.h"

//////////////////////////////////////////////////////////////////////
//
// Environment
//
//////////////////////////////////////////////////////////////////////

class MidiEnv : public MidiEventManager {

  public:

	/**
	 * This must be implemented in the platform subclass.
	 */
    static MidiEnv* getEnv();

	/**
	 * Do any platform and generic cleanup.
	 */
    static void exit();

	// event factory methods

	class MidiEvent *newMidiEvent(void);
	class MidiEvent *newMidiEvent(int status, int channel = 0, int key = 0,
                                  int velocity = 0);
    void freeMidiEvents(class MidiEvent *ev);

	// ports

    class MidiPort* getInputPorts();
    class MidiPort* getDefaultInputPort();
    class MidiPort* getInputPort(const char *name);

    class MidiPort* getOutputPorts();
    class MidiPort* getDefaultOutputPort();
    class MidiPort* getOutputPort(const char *name);

	// resources

    class MidiTimer* getTimer();

	class MidiInput* getInputs();
	class MidiInput* getInput(MidiPort* port);
	class MidiInput* openInput(MidiPort* port);
	void closeInput(class MidiInput* anInput);

	class MidiOutput* getOutputs();
	class MidiOutput* getOutput(MidiPort* port);
	class MidiOutput* openOutput(MidiPort* port);
	void closeOutput(class MidiOutput* anOutput);

    void closeInputs();
    void closeOutputs();

  protected:

	// subclass overloads

	virtual void loadDevices() = 0;
	virtual class MidiTimer* newMidiTimer() = 0;
	virtual class MidiInput* newMidiInput(class MidiPort* dev) = 0;
	virtual class MidiOutput* newMidiOutput(class MidiPort* dev) = 0;

	
	MidiEnv();
    virtual ~MidiEnv();

	void enterCriticalSection(void);
	void leaveCriticalSection(void);

    static MidiEnv* mSingleton;

	class CriticalSection *mCsect;	// critical section for pool
	class MidiEvent *mEvents;		// pool of unallocated events

    class MidiPort* mInputPorts;
    class MidiPort* mOutputPorts;

    class MidiTimer* mTimer;            // the singleton timer
    class MidiInput* mInputs;
    class MidiOutput* mOutputs;
};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
