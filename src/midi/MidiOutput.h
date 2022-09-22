/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * An abstract interface for MIDI output streams.
 *
 */

#ifndef MIDI_OUTPUT_H
#define MIDI_OUTPUT_H

/****************************************************************************
 *                                                                          *
 *                                  MIDI OUT                                *
 *                                                                          *
 ****************************************************************************/

/**
 * Object encapsulating operations and state related to a MIDI output port.
 *
 */
class MidiOutput {

	//
    // You obtain one these from MidiEnv factory methods
	//
    friend class MidiEnv;

  public:

	virtual ~MidiOutput();

    MidiOutput* getNext() {
        return NULL;
    }

	//
	// Configuration
	//

	void setPort(class MidiPort* port);
	class MidiPort* getPort();

	// TODO: Should allow a map on output too?
	//void setMap(class MidiMap* map);

	//
	// MIDI Messages
	//

    void send(class MidiEvent* e, int channel = -1);
    void sendProgram(int channel, int program);
    void sendControl(int channel, int type, int value);
    void sendNoteOn(int channel, int key, int velocity);
    void sendNoteOff(int channel, int key);
    void sendStart(void);
    void sendStop(void);
    void sendContinue(void);
    void sendClock(void);
    void sendSongPosition(int psn);
    void sendSongSelect(int song);
    void sendLocal(int channel, int onoff);
    void sendAllNotesOff(int channel);
    void panic(void);

	//
	// Subclass overloads
	//

	/**
	 * Establish a connection to the configured MidiPort.
	 */
	virtual int connect(void) = 0;

	/**
	 * Terminate a connection to the configured MidiPort.
	 */
	virtual void disconnect(void) = 0;

	/**
	 * Return true if a connection to the MidiPort has been established.
	 */
	virtual bool isConnected() = 0;

	/**
	 * Send a message in packed format.	
	 */
    virtual void send(int msg) = 0;

    /**
     * Send a packed sysex message.
     */
    virtual int sendSysex(const unsigned char *buffer, int length) = 0;

	//
	// Diagnostics
	//

    virtual void printWarnings();


  protected:

	MidiOutput(class MidiEnv* env, class MidiPort* port);

	void setNext(MidiOutput *n) {
		mNext = n;
	}

    MidiEnv*        mEnv;
    MidiOutput*     mNext;              // main have more than one of these
	MidiPort* 		mPort;

	// error statistics

	int mWeirdErrors;
	int mSysexTimeouts;

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
