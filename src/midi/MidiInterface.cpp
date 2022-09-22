/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Implementation of the AbstractMidiInterface and various concrete
 * support classes for the MIDI abstraction layer.
 *
 * Originally I was going to have a MacMidiInterface and WinMidiInterface
 * but now that the MidiEnv model does the device encapsulation we
 * can share the implementation.  As such we don't really need
 * AbstractMidiInterface any more.
 */

#include <stdio.h>
#include <string.h>

#include "Port.h"
#include "Util.h"
#include "List.h"
#include "Trace.h"
#include "Thread.h"

#include "MidiEnv.h"
#include "MidiPort.h"
#include "MidiTimer.h"
#include "MidiInput.h"
#include "MidiOutput.h"
#include "MidiListener.h"

#include "MidiInterface.h"

//////////////////////////////////////////////////////////////////////
//
// AbstractMidiInterface
//
//////////////////////////////////////////////////////////////////////

PUBLIC AbstractMidiInterface::AbstractMidiInterface()
{
	mListener = NULL;
	mClockListener = NULL;

	mInputSpec = NULL;
	mOutputSpec = NULL;
	mThroughSpec = NULL;

	mInputError[0] = 0;
	mOutputError[0] = 0;
	mThroughError[0] = 0;
	mError[0] = 0;
}

PUBLIC AbstractMidiInterface::~AbstractMidiInterface()
{
	delete mInputSpec;
	delete mOutputSpec;
	delete mThroughSpec;
}


void AbstractMidiInterface::setListener(MidiEventListener* l)
{
	mListener = l;
}

void AbstractMidiInterface::setClockListener(MidiClockListener* h)
{
	mClockListener = h;
}

const char* AbstractMidiInterface::getInput()
{
	return mInputSpec;
}

const char* AbstractMidiInterface::getInputError()
{
	return (strlen(mInputError) > 0) ? mInputError : NULL;
}

const char* AbstractMidiInterface::getOutput()
{
	return mOutputSpec;
}

const char* AbstractMidiInterface::getOutputError()
{
	return (strlen(mOutputError) > 0) ? mOutputError : NULL;
}

const char* AbstractMidiInterface::getThrough()
{
	return mThroughSpec;
}

const char* AbstractMidiInterface::getThroughError()
{
	return (strlen(mThroughError) > 0) ? mThroughError : NULL;
}

const char* AbstractMidiInterface::getLastError()
{
	return mError;
}

void AbstractMidiInterface::printStatistics()
{
}

void AbstractMidiInterface::printEnvironment()
{
}

/****************************************************************************
 *                                                                          *
 *   						   INTERFACE CLASS                              *
 *                                                                          *
 ****************************************************************************/


class CommonMidiInterface : public AbstractMidiInterface, 
  public MidiInputListener, public MidiClockListener {

  public:

	CommonMidiInterface();
	~CommonMidiInterface();

    MidiPort* getInputPorts();
    MidiPort* getOutputPorts();

	bool setInput(const char* name);
	bool setOutput(const char* name);
	bool setThrough(const char* name);
	void setThroughMap(MidiMap* map);

	void startClocks(float tempo);
	void stopClocks();

	MidiEvent* newEvent(int status, int chan, int value, int vel);
	void send(MidiEvent* e);
	void send(unsigned char e);
	void echo(MidiEvent* e);

	float getInputTempo();
	int getInputSmoothTempo();
	float getOutputTempo();
	void setOutputTempo(float bpm);

	void midiStart();
	void midiStop(bool stopClocks);
	void midiContinue();

	// this is provided so we can defer the starting of the timer
	// thread until we're sure we need it, yet still do it in advance
	// of needing MIDI clocks from midiStart
	bool timerStart();

	// these are mostly just for debugging
	long getMilliseconds();
	int getMidiClocks();
	float getMillisPerClock();

	//
	// Internal methods
	//

	void midiInputEvent(MidiInput* in);
	void midiClockEvent();
	void midiStartEvent();
	void midiStopEvent();
	void midiContinueEvent();

  private:

    MidiInput* getTempoDevice();

	MidiEnv* mEnv;
	MidiTimer* mTimer;
	MidiInput* mTempoInput;
	MidiOutput* mThrough;
    bool mThroughIsOutput;

};

/****************************************************************************
 *                                                                          *
 *   						  INTERFACE FACTORY                             *
 *                                                                          *
 ****************************************************************************/

CriticalSection* MidiInterface::Csect = new CriticalSection();
MidiInterface* MidiInterface::Interface = NULL;
int MidiInterface::References = 0;
bool MidiInterface::AllocTrace = false;

PUBLIC MidiInterface* MidiInterface::getInterface(const char* who)
{
	if (AllocTrace) {
		printf("MidiInterface::getInterface %s\n", who);
		fflush(stdout);
	}

	Csect->enter();
	try {
		if (Interface == NULL) {
			// here is where we need a selection method
			Interface = new CommonMidiInterface();
		}
		References++;
	}
	catch (...) {
		Trace(1, "Exception thrown during MidiInterface::getInterface\n");
	}
	Csect->leave();

	return Interface;
} 

void MidiInterface::release(MidiInterface* i)
{
	if (i != Interface) {
		Trace(1, "MidiInterface::release unknown interface!\n");
	}
	else {
		Csect->enter();
		try {
			if (References <= 0)
			  Trace(1, "MidiInterface::release overflow!\n");
			References--;
			if (References <= 0) {
				if (AllocTrace)
				  printf("MidiInterface::deleting interface\n");
				delete i;
				Interface = NULL;
				References = 0;
			}
			else {
				if (AllocTrace)
				  printf("MidiInterface::reference count not zero\n");
			}
		}
		catch (...) {
			Trace(1, "MidiInterface::release Exception!\n");
		}
		Csect->leave();

		if (AllocTrace)
		  fflush(stdout);
	}
}

void MidiInterface::exit()
{
	if (Interface != NULL) {
		delete Interface;
		Interface = NULL;
	}
}

/****************************************************************************
 *                                                                          *
 *   							IMPLEMENTATION                              *
 *                                                                          *
 ****************************************************************************/

CommonMidiInterface::CommonMidiInterface()
{
	mEnv = NULL;
	mTimer = NULL;
	mTempoInput = NULL;
	mThrough = NULL;

	// Get the singleton timer, do NOT start it yet
	// When used by VST plugins, it is common for the host to probe VSTs
	// to get information about them then delete them right away.  In these
	// cases we don't need the overhead of starting a highres timer until
	// later when the plugin is resumed.

	mEnv = MidiEnv::getEnv();
	mTimer = mEnv->getTimer();
}

CommonMidiInterface::~CommonMidiInterface()
{
	// the devices objects are owned by MidiEnv which is tracking them
	MidiEnv::exit();
}

MidiPort* CommonMidiInterface::getInputPorts()
{
	return mEnv->getInputPorts();
}

MidiPort* CommonMidiInterface::getOutputPorts()
{
	return mEnv->getOutputPorts();
}

/****************************************************************************
 *                                                                          *
 *   						   DEVICE CALLBACKS                             *
 *                                                                          *
 ****************************************************************************/

/**
 * MidiInListener interface for the internal MIDI interface.
 */
void CommonMidiInterface::midiInputEvent(MidiInput* in)
{
	MidiEvent *events, *e;
		
	// ignore any sysex that may have come in
	in->ignoreSysex();

	// get the current event(s)
	events = in->getEvents();

	if (mListener != NULL) {
		for (e = events ; e != NULL ; e = e->getNext())
		  mListener->midiEvent(e);
	}

	events->free();
}

/**
 * TimerMidiClockListener interface.
 * TODO: We don't really need to proxy these, just push our
 * clock listener into the timer.
 */
void CommonMidiInterface::midiClockEvent()
{
	if (mClockListener != NULL)
	  mClockListener->midiClockEvent();
}

void CommonMidiInterface::midiStartEvent()
{
	if (mClockListener != NULL)
	  mClockListener->midiStartEvent();
}

void CommonMidiInterface::midiStopEvent()
{
	if (mClockListener != NULL)
	  mClockListener->midiStopEvent();
}

void CommonMidiInterface::midiContinueEvent()
{
	if (mClockListener != NULL)
	  mClockListener->midiContinueEvent();
}

/****************************************************************************
 *                                                                          *
 *   								EVENTS                                  *
 *                                                                          *
 ****************************************************************************/

MidiEvent* CommonMidiInterface::newEvent(int status, int channel, 
										  int value, int velocity)
{
	return mEnv->newMidiEvent(status, channel, value, velocity);
}

void CommonMidiInterface::send(MidiEvent* e)
{
    for (MidiOutput* out = mEnv->getOutputs() ; out != NULL ; 
         out = out->getNext()) {

        if (out != mThrough || mThroughIsOutput)
          out->send(e);
    }
}

void CommonMidiInterface::send(unsigned char e)
{
    for (MidiOutput* out = mEnv->getOutputs() ; out != NULL ; 
         out = out->getNext()) {

        if (out != mThrough || mThroughIsOutput)
          out->send(e);
    }
}

void CommonMidiInterface::echo(MidiEvent* e)
{
	if (mThrough != NULL)
	  mThrough->send(e);
}

/****************************************************************************
 *                                                                          *
 *   								TIMER                                   *
 *                                                                          *
 ****************************************************************************/

/**
 * Return the MIDI input device that is the tempo source.
 * 
 * !! Who gets to define the tempo?
 * This could be a problem do we need to disable clock events
 * from devices other than the first one?
 * KLUDGE: assume the first one that has a non-zero tempo wins
 * which should be enough most of the time.
 */
MidiInput* CommonMidiInterface::getTempoDevice()
{
	MidiInput* dev = mTempoInput;

    // if we cached one and it stops having a temp, look for another
    if (dev != NULL) {
        float tempo = dev->getTempo();
        if (tempo == 0.0f)
          dev = NULL;
    }

    if (dev == NULL) {
        for (MidiInput* in = mEnv->getInputs() ; in != NULL ; 
             in = in->getNext()) {

			float tempo = in->getTempo();
			if (tempo > 0.0f) {
				dev = in;
				break;
			}
        }
    }

    // remember for next time
    mTempoInput = dev;

	return dev;
}

float CommonMidiInterface::getInputTempo()
{
	float tempo = 0.0;

    MidiInput* dev = getTempoDevice();
    if (dev != NULL)
      tempo = dev->getTempo();

	return tempo;
}

int CommonMidiInterface::getInputSmoothTempo()
{
	int tempo = 0;

    MidiInput* dev = getTempoDevice();
    if (dev != NULL)
      tempo = dev->getSmoothTempo();

	return tempo;
}

float CommonMidiInterface::getOutputTempo()
{
	return mTimer->getTempo();
}

bool CommonMidiInterface::timerStart()
{
	// this just arms it, it won't start until
	// midiStartClocks is called
	mTimer->setMidiSync(true);

	return mTimer->start();
}

long CommonMidiInterface::getMilliseconds()
{
	return mTimer->getMilliseconds();
}

int CommonMidiInterface::getMidiClocks()
{
	return mTimer->getMidiClocks();
}

float CommonMidiInterface::getMillisPerClock()
{
	return mTimer->getMidiMillisPerClock();
}

/**
 * This must defer chancing the tempo (aka pulse width)
 * until the next clock boundary.
 */
void CommonMidiInterface::setOutputTempo(float bpm)
{
	mTimer->setTempo(bpm);
}

/**
 * Start emitting clocks at the given tempo.
 * Typically clocks are off until we start, but with OutUserStart
 * we may decide to send them first in order for the external 
 * device to lock onto the tempo (this is rarely necessary though).
 */
void CommonMidiInterface::startClocks(float tempo)
{
	mTimer->setMidiClockListener(this);
	mTimer->setTempo(tempo);
	mTimer->setMidiSync(true);
	mTimer->midiStartClocks();
}

/**
 * Stop sending MIDI clocks controlled by the timer.
 */
void CommonMidiInterface::stopClocks()
{
	// avoid if we're already stopped for devices that you
	// have to "arm" for play (like Sonar)
	if (mTimer->isMidiSync()) {
		mTimer->setMidiClockListener(NULL);
		mTimer->midiStopClocks();
	}
}

/**
 * Send StartSong and begin sending clocks.  
 * If we've already been sending clocks, timer must make sure the internal
 * clock counters are reset so we get a full pulse between now
 * and the next clock callback from the timer.
 */
void CommonMidiInterface::midiStart()
{
	mTimer->setMidiClockListener(this);
	mTimer->midiStart();
}

/**
 * Send StopSong and optionally stop sending clocks.
 */
void CommonMidiInterface::midiStop(bool stopClocks)
{
	if (stopClocks)
	  mTimer->setMidiClockListener(NULL);
	mTimer->midiStop(stopClocks);
}

/**
 * Send Continue and resume sending clocks.
 * Do not send song position.
 */
void CommonMidiInterface::midiContinue()
{
	mTimer->setMidiClockListener(this);
	mTimer->midiContinue();
}

/****************************************************************************
 *                                                                          *
 *   								INPUT                                   *
 *                                                                          *
 ****************************************************************************/

bool CommonMidiInterface::setInput(const char* spec)
{
	bool success = true;

	if (!StringEqual(mInputSpec, spec)) {
		int i;

		delete mInputSpec;
		mInputSpec = CopyString(spec);
		strcpy(mInputError, "");

		// parse the spec string into string list
		StringList* names = new StringList(spec);

		// We could try to be smarter and only close the ones that we
		// won't be reopening, but it's complicated and not really necessary.
        mEnv->closeInputs();
        mTempoInput = NULL;

		for (i = 0 ; i < names->size() ; i++) {
			MidiPort* port = mEnv->getInputPort(names->getString(i));
			if (port != NULL) {
				MidiInput* in = mEnv->openInput(port);
				in->setListener(this);
				in->setEchoDevice(mThrough);
				in->setTimer(mTimer);

                // open doesn't connect...still not happy with the
                // interface here, the notion that the in/out lists
                // can have disconnected things on it feels funny
				in->connect();
			}
			else {
				if (strlen(mInputError) == 0)
				  strcpy(mInputError, "Unable to open MIDI input ports: ");
				else
				  strcat(mInputError, ", ");
				strcat(mInputError, names->getString(i));
				success = false;
			}
		}

		delete names;
	}

	return success;
}

/****************************************************************************
 *                                                                          *
 *   							   THROUGH                                  *
 *                                                                          *
 ****************************************************************************/

bool CommonMidiInterface::setThrough(const char* spec)
{
	bool success = true;

	if (!StringEqual(mThroughSpec, spec)) {

		delete mThroughSpec;
		mThroughSpec = CopyString(spec);
		strcpy(mThroughError, "");

		// take it away first in case we disconnect it
        for (MidiInput* in = mEnv->getInputs() ; in != NULL ; 
             in = in->getNext()) {
            in->setEchoDevice(NULL);
        }

		if (mThrough != NULL) {
			// disconnect this ONLY if it is not also an output device
			if (!mThroughIsOutput)
              mEnv->closeOutput(mThrough);
			mThrough = NULL;
		}
        mThroughIsOutput = false;

		if (spec != NULL) {
			MidiPort* port = mEnv->getOutputPort(spec);
			if (port != NULL) {
				mThrough = mEnv->openOutput(port);
				mThrough->connect();

                // remember this so we can tell if this is behaving
                // only as a through and not an output since MidiEnv
                // only has one list
                if (LastIndexOf(mOutputSpec, mThroughSpec) >= 0)
                  mThroughIsOutput = true;

                for (MidiInput* in = mEnv->getInputs() ; in != NULL ; 
                     in = in->getNext()) {
                    in->setEchoDevice(mThrough);
                }
			}
			else {
				sprintf(mThroughError, "Unable to open MIDI through port: %s", 
						spec);
				success = false;
			}
		}
	}

	return success;
}

void CommonMidiInterface::setThroughMap(MidiMap* map)
{
    for (MidiInput* in = mEnv->getInputs() ; in != NULL ; in = in->getNext()) {

        in->setEchoMap(map);
    }
}

/****************************************************************************
 *                                                                          *
 *   								OUTPUT                                  *
 *                                                                          *
 ****************************************************************************/

bool CommonMidiInterface::setOutput(const char* spec)
{
	bool success = true;

	if (!StringEqual(mOutputSpec, spec)) {
		int i;

		delete mOutputSpec;
		mOutputSpec = CopyString(spec);
		strcpy(mOutputError, "");

		// parse the spec string into string list
		StringList* names = new StringList(spec);

		// Since we may have more than one here we could try to 
		// be smarter and only close the ones that we won't be reopening,
		// but it's complicated and not really necessary.
        mEnv->closeOutputs();

		for (i = 0 ; i < names->size() ; i++) {
			MidiPort* port = mEnv->getOutputPort(names->getString(i));
			if (port != NULL) {
				MidiOutput* out = mEnv->openOutput(port);
				out->connect();
				mTimer->addMidiOutput(out);
			}
			else {
				if (strlen(mOutputError) == 0)
				  strcpy(mOutputError, "Unable to open MIDI output ports: ");
				else
				  strcat(mOutputError, ", ");
				strcat(mOutputError, names->getString(i));
				success = false;
			}
		}

        // The through was on the MidiEnv's output list and will
        // be gone now too.  Reopen it.
        char* throughSpec = mThroughSpec;
        if (throughSpec != NULL) {
            // init things so we can flow through the usual set logic
            mThroughSpec = NULL;
            mThrough = NULL;
            mThroughIsOutput = false;
            setThrough(throughSpec);
            delete throughSpec;
        }

		delete names;
	}

	return success;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
