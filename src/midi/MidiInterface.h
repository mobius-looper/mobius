/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * MOBIUS PUBLIC INTERFACE
 *
 * A fascade interface encapsultaing MIDI and Millisecond timer devices.
 * This is built upon the MidiEnv interface, but has been simplified
 * to meet the needs of Mobius.  There is no exposed model for managing
 * endpoints directly to  create arbitrary routes for example.  
 * Need to think about this...
 *
 * Originally the input/output/through device names were expected to be the
 * names of single devices.  We now allow the output device name to 
 * be a comma seperated list of several device names, and the expectation
 * is that sending an event goes to all of them.
 * 
 * This is also where we do the reference counting so we only
 * delete the singleton MidiEnv when we know it isn't needed by 
 * any plugin instances.  Should consider moving this down to MidiEnv.
 */

#ifndef MIDI_INTERFACE_H
#define MIDI_INTERFACE_H

/****************************************************************************
 *                                                                          *
 *   							MIDI INTERFACE                              *
 *                                                                          *
 ****************************************************************************/

class MidiInterface {

  public:

	/**
	 * Instantiate an implementation of the handler.
	 * There is normally only one of these within an application.
	 */
	static MidiInterface* getInterface(const char* who);
	static void release(MidiInterface* i);
	static void exit();

	virtual ~MidiInterface() {}
	
	virtual class MidiPort* getInputPorts() = 0;
	virtual class MidiPort* getOutputPorts() = 0;

	virtual void setListener(class MidiEventListener* h) = 0;
	virtual void setClockListener(class MidiClockListener* l) = 0;

	virtual bool setInput(const char* name) = 0;
	virtual const char* getInput() = 0;
	virtual const char* getInputError() = 0;

	virtual bool setOutput(const char* name) = 0;
	virtual const char* getOutput() = 0;
	virtual const char* getOutputError() = 0;

	virtual bool setThrough(const char* name) = 0;
	virtual const char* getThrough() = 0;
	virtual const char* getThroughError() = 0;
	virtual void setThroughMap(class MidiMap* map) = 0;

	virtual class MidiEvent* newEvent(int status, int chan, int value, int vel) = 0;
	virtual void send(class MidiEvent* e) = 0;
	virtual void send(unsigned char e) = 0;
	virtual void echo(class MidiEvent* e) = 0;

	// timer
	virtual bool timerStart() = 0;
	virtual long getMilliseconds() = 0;
	virtual int getMidiClocks() = 0;
	virtual float getMillisPerClock() = 0;

	// tempo monitor
	virtual float getInputTempo() = 0;
	virtual int getInputSmoothTempo() = 0;

	// sync out
	virtual void setOutputTempo(float bpm) = 0;
	virtual float getOutputTempo() = 0;
	virtual void midiStart() = 0;
	virtual void midiStop(bool stopClocks) = 0;
	virtual void midiContinue() = 0;
	virtual void startClocks(float tempo) = 0;
	virtual void stopClocks() = 0;

	// diagnostics

	virtual void printEnvironment() = 0;
	virtual void printStatistics() = 0;
	virtual const char* getLastError() = 0;

  protected:

	static class CriticalSection* Csect;
	static MidiInterface* Interface;
	static int References;
	static bool AllocTrace;

};

//////////////////////////////////////////////////////////////////////
//
// AbstractMidiInterface
//
//////////////////////////////////////////////////////////////////////

#define MAX_DEVICES 8
#define MAX_ERROR 1024

/**
 * Skeleton imlementation of MidiInterface with common options.
 */
class AbstractMidiInterface : public MidiInterface {

  public:

	AbstractMidiInterface();
	virtual ~AbstractMidiInterface();

	void setListener(class MidiEventListener* h);
	void setClockListener(class MidiClockListener* h);

    const char* getInput();
	const char* getInputError();
    const char* getOutput();
	const char* getOutputError();
    const char* getThrough();
	const char* getThroughError();
	const char* getLastError();

	// diagnostics

	virtual void printEnvironment();
	virtual void printStatistics();

  protected:

	class MidiEventListener* mListener;
	class MidiClockListener* mClockListener;

	char* mInputSpec;
	char* mOutputSpec;
	char* mThroughSpec;

	char mInputError[MAX_ERROR];
	char mOutputError[MAX_ERROR];
	char mThroughError[MAX_ERROR];
	char mError[MAX_ERROR];
};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
