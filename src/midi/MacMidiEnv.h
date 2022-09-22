/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Mac implementation of the MidiEnv.
 *
 */

#ifndef MAC_MIDI_ENV_H
#define MAC_MIDI_ENV_H

#include <CoreMIDI/MIDIServices.h>

#include "MidiEnv.h"
#include "MidiPort.h"
#include "MidiTimer.h"
#include "MidiInput.h"
#include "MidiOutput.h"

class MacMidiEnv : public MidiEnv {

	friend class MidiEnv;

  public:

	// the required virtual overloads

	void loadDevices();
	class MidiTimer* newMidiTimer();
	class MidiInput* newMidiInput(class MidiPort* port);
	class MidiOutput* newMidiOutput(class MidiPort* port);

	// extended operations

	MIDIClientRef getClient();
	void printEnvironment();

  protected:

	MacMidiEnv();
	~MacMidiEnv();

  private:

	class MacMidiPort* getPort(MIDIEndpointRef point);
	char* getString(MIDIObjectRef obj, CFStringRef prop);

	void dumpDevice(MIDIDeviceRef dev);
	void dumpEntity(MIDIEntityRef ent);
	void dumpEndpoint(const char* type, MIDIEndpointRef point);

	MIDIClientRef mClient;
	bool mPortsLoaded;

};

/**
 * Create extensions of MidiPort so we can remember the endpoint.
 */
class MacMidiPort : public MidiPort {

  public:

	MacMidiPort();
	~MacMidiPort();

	MIDIEndpointRef getEndpoint();
	void setEndpoint(MIDIEndpointRef point);

  private:
	
	MIDIEndpointRef mEndpoint;
};

class MacMidiTimer : public MidiTimer
{
  public:
	
	MacMidiTimer(MacMidiEnv* env);
	~MacMidiTimer();

	bool start();
	void stop();
	bool isRunning();

  private:

	class MacMidiTimerThread* mThread;

};

class MacMidiInput : public MidiInput 
{
  public:
	
	MacMidiInput(MacMidiEnv* env, MidiPort* port);
	~MacMidiInput();

	int connect(void);
	void disconnect();
	bool isConnected();
	void notifyEventsReceived();
	void ignoreSysex();

	// the interrupt handler
	void processPackets(const MIDIPacketList* packets, MacMidiPort* port);

  private:

	MIDIClientRef getClient();

	MIDIPortRef mInputPort;
	MIDIEndpointRef mSource;
};

class MacMidiOutput : public MidiOutput 
{
  public:
	
	MacMidiOutput(MacMidiEnv* env, MidiPort* port);
	~MacMidiOutput();

	int connect(void);
	void disconnect();
	bool isConnected();
	void send(int msg);
    int sendSysex(const unsigned char *buffer, int length);

  private:

	MIDIClientRef getClient();

	MIDIPortRef mOutputPort;
	MIDIEndpointRef mDestination;

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
