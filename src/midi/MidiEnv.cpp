/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Base implementation of MidiEnv.
 * The main services we provide are management of the MidiDeviceInfoP
 * lists and the MidiEventPool.
 *
 * The class has a static singleton constructor that must be implemented
 * differently for each platform.
 */

#include <stdio.h>

#include "Port.h"
#include "Util.h"
#include "Trace.h"
#include "Thread.h"

#include "MidiPort.h"
#include "MidiInput.h"
#include "MidiOutput.h"
#include "MidiTimer.h"

#include "MidiEnv.h"

//////////////////////////////////////////////////////////////////////
//
// Constructor
//
//////////////////////////////////////////////////////////////////////

/**
 * We maintain the static singleton here but the subclass must
 * overload the getEnv() constructor to make one.
 */
MidiEnv* MidiEnv::mSingleton = NULL;

/**
 * In case the application wants MIDI capabilities to come and go,
 * you could call this to release any state we have accumulated,
 * though in practice you generally just let this live forever.
 * It can also be usedul to flush state if you're using memory
 * allocation tracking utilities.
 */
PUBLIC void MidiEnv::exit()
{
    delete mSingleton;
    mSingleton = NULL;
}

PRIVATE MidiEnv::MidiEnv()
{
	mCsect = new CriticalSection();
	mEvents = NULL;
    mInputPorts = NULL;
    mOutputPorts = NULL;
    mTimer = NULL;
    mInputs = NULL;
    mOutputs = NULL;
}

PRIVATE MidiEnv::~MidiEnv()
{
    closeInputs();
    closeOutputs();

	delete mTimer;
    delete mInputPorts;
    delete mOutputPorts;
    delete mEvents;
    delete mCsect;
}

//////////////////////////////////////////////////////////////////////
//
// Timer
//
//////////////////////////////////////////////////////////////////////

/**
 * Return the singleton timer.  There can only be one of these, older
 * versions of Windows only support one 1-millisecond timer.
 */
PUBLIC MidiTimer* MidiEnv::getTimer()
{
	if (mTimer == NULL)
	  mTimer = newMidiTimer();
	return mTimer;
}

//////////////////////////////////////////////////////////////////////
//
// Event Pool
//
//////////////////////////////////////////////////////////////////////

/**
 * Enter the critical section.
 * This must be called prior to any access of the mEvents list.
 */
PRIVATE void MidiEnv::enterCriticalSection(void)
{
	if (mCsect != NULL)
	  mCsect->enter();
}

PRIVATE void MidiEnv::leaveCriticalSection(void)
{
	if (mCsect != NULL)
	  mCsect->leave();
}

/**
 * Allocate a MidiEvent from the pool.
 * Though you can delete these with the delete operator, for the
 * pool to be effective you should return it by calling MidiEnv::freeMidiEvents
 * or MidiEvent::free methods.
 */
PUBLIC MidiEvent *MidiEnv::newMidiEvent(void)
{
	MidiEvent *ev = NULL;

	// get the next one in the pool
	// since this can be called from an interrupt handler, be careful
	// about the critical section

	enterCriticalSection();
	if (mEvents != NULL) {
		ev = mEvents;
		mEvents = ev->getNext();
	}
	leaveCriticalSection();

	if (ev != NULL) {
		// expected initialization
		ev->setNext(NULL);
		ev->setStack(NULL);
	}
	else {
		// have to allocate more
        // could consider allocating these in blocks for better
        // locality of reference
		ev = new MidiEvent();
	}

	// remember where we came from
	if (ev != NULL)
	  ev->setManager(this);
	
	return ev;
}

/**
 * Allocate and initialize an event.
 */
PUBLIC MidiEvent *MidiEnv::newMidiEvent(int status, int channel,
                                        int key, int velocity)
{
	MidiEvent *e = newMidiEvent();
	if (e != NULL) {
		e->setStatus(status);
		e->setChannel(channel);
		e->setKey(key);
		e->setVelocity(velocity);
	}
	return e;
}

/**
 * Return an event list to the pool.
 * Typically called by MidiEvent::free to put themselves back 
 * on their environment's pool.
 */
PUBLIC void MidiEnv::freeMidiEvents(MidiEvent *events)
{
    MidiEvent *e, *last;

    // locate the last item in the list, initializing and releasing
    // attached storage along the way
    last = NULL;
    for (e = events ; e != NULL ; e = e->getNext()) {
        e->reinit();
        last = e;
    }
		  
    if (last != NULL) {
        enterCriticalSection();
        last->setNext(mEvents);
        mEvents = events;
        leaveCriticalSection();
	}
}

//////////////////////////////////////////////////////////////////////
//
// Ports
//
//////////////////////////////////////////////////////////////////////

PUBLIC MidiPort* MidiEnv::getInputPorts(void)
{
	loadDevices();
	return mInputPorts;
}

/**
 * Is this always the first one?
 * Windows doesn't seem to have an SDK for this, does Mac?
 */
PUBLIC MidiPort* MidiEnv::getDefaultInputPort()
{
    return getInputPorts();
}

PUBLIC MidiPort* MidiEnv::getInputPort(const char *name)
{
    MidiPort* ports = getInputPorts();
    return (ports != NULL) ? ports->getPort(name) : NULL;
}


PUBLIC MidiPort* MidiEnv::getOutputPorts(void)
{
	loadDevices();
	return mOutputPorts;
}

PUBLIC MidiPort* MidiEnv::getDefaultOutputPort()
{
    return getOutputPorts();
}

PUBLIC MidiPort* MidiEnv::getOutputPort(const char *name)
{
    MidiPort* devs = getOutputPorts();
    return (devs != NULL) ? devs->getPort(name) : NULL;
}

//////////////////////////////////////////////////////////////////////
//
// Inputs
//
//////////////////////////////////////////////////////////////////////

/**
 * Return the list of currently open input devices.
 */
PUBLIC MidiInput* MidiEnv::getInputs()
{
	return mInputs;
}

/**
 * Return an input for a port if one is open.
 */
PUBLIC MidiInput* MidiEnv::getInput(MidiPort* port)
{
	MidiInput* found = NULL;
    for (MidiInput* in = mInputs ; in != NULL ; in = in->getNext()) {
        if (in->getPort() == port) {
            found = in;
            break;
        }
    }
	return found;
}

/**
 * Open an input port if one is not already open.
 */
PUBLIC MidiInput* MidiEnv::openInput(MidiPort* port)
{
	MidiInput* in = getInput(port);
	if (in == NULL) {
		in = newMidiInput(port);
		if (in != NULL) {
			in->setNext(mInputs);
			mInputs = in;
		}
	}
    return in;
}

/**
 * Disconnect an input and remove it from the list.
 * You can also call MidiInput::disconnect and leave it on the list
 * but I'm not sure that's the best interface.
 */
PUBLIC void MidiEnv::closeInput(MidiInput* anInput)
{
	MidiInput* prev = NULL;
	MidiInput* found = NULL;

    for (MidiInput* in = mInputs ; in != NULL ; in = in->getNext()) {
		if (in != anInput)
		  prev = in;
		else {
            found = in;
            break;
        }
    }

    if (found == NULL) {
        trace("MidiEnv::closeInput untracked input!\n");
    }
    else {
        MidiInput* next = anInput->getNext();
        if (prev == NULL)
          mInputs = next;
        else
          prev->setNext(next);
	}

    // I guess do this whether we have it tracked or not
    MidiPort* port = anInput->getPort();
    if (port != NULL)
      printf("Closing MIDI input %s...\n", port->getName());

    anInput->disconnect();

    delete anInput;
}

/**
 * Close all currently open inputs.
 */
PUBLIC void MidiEnv::closeInputs()
{
    while (mInputs != NULL)
      closeInput(mInputs);
}

//////////////////////////////////////////////////////////////////////
//
// Outputs
//
//////////////////////////////////////////////////////////////////////

/**
 * Return the list of currently open output devices.
 */
PUBLIC MidiOutput* MidiEnv::getOutputs()
{
	return mOutputs;
}

/**
 * Return an output for a port if one is open.
 */
PUBLIC MidiOutput* MidiEnv::getOutput(MidiPort* port)
{
	MidiOutput* found = NULL;
    for (MidiOutput* out = mOutputs ; out != NULL ; out = out->getNext()) {
        if (out->getPort() == port) {
            found = out;
            break;
        }
    }
	return found;
}

/**
 * Open an output port if one is not already open.
 */
PUBLIC MidiOutput* MidiEnv::openOutput(MidiPort* port)
{
	MidiOutput* out = getOutput(port);
	if (out == NULL) {
		out = newMidiOutput(port);
		if (out != NULL) {
			out->setNext(mOutputs);
			mOutputs = out;
		}
	}
    return out;
}

/**
 * Close an output and remove it from the list.
 */
PUBLIC void MidiEnv::closeOutput(MidiOutput* anOutput)
{
	MidiOutput* prev = NULL;
	MidiOutput* found = NULL;

    for (MidiOutput* out = mOutputs ; out != NULL ; out = out->getNext()) {
		if (out != anOutput) 
		  prev = out;
		else {
            found = out;
            break;
        }
    }

    if (found == NULL) {
        trace("MidiEnv::closeOutput untracked output!\n");
    }
    else {
        MidiOutput* next = anOutput->getNext();
        if (prev == NULL)
          mOutputs = next;
        else
          prev->setNext(next);
	}

    // I guess we should disconnect whether it was tracked or not

    // timer can ref it
    mTimer->removeMidiOutput(anOutput);

    // as can inputs
    for (MidiInput* in = mInputs ; in != NULL ; in = in->getNext()) {
        in->removeEchoDevice(anOutput);
    }

    MidiPort* port = anOutput->getPort();
    if (port != NULL)
      printf("Closing MIDI output %s...\n", port->getName());

    anOutput->disconnect();

    delete anOutput;
}

/**
 * Close all currently open outputs.
 */
PUBLIC void MidiEnv::closeOutputs()
{
    // lose these first
    if (mTimer != NULL)
      mTimer->resetMidiOutputs();

    while (mOutputs != NULL)
      closeOutput(mOutputs);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
