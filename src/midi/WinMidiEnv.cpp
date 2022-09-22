/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 *
 * Windows implementation of MidiEnv
 *
 */

#include <stdio.h>

#include "Port.h"
#include "Util.h"
#include "Thread.h"
#include "Trace.h"

#include "MidiPort.h"
#include "WinMidiEnv.h"

//////////////////////////////////////////////////////////////////////
//
// Constructor
//
//////////////////////////////////////////////////////////////////////

/**
 * This is a singleton factory method that creates a platform-specific 
 * subclass of MidiEnv.
 */
PUBLIC MidiEnv* MidiEnv::getEnv()
{
    if (mSingleton == NULL)
      mSingleton = new WinMidiEnv();
    return mSingleton;
}

PRIVATE WinMidiEnv::WinMidiEnv()
{
    mDevicesLoaded = false;
}

PRIVATE WinMidiEnv::~WinMidiEnv()
{
}

//////////////////////////////////////////////////////////////////////
//
// Ports
//
//////////////////////////////////////////////////////////////////////

/**
 * MidiEnv required overload to populate the input and output port lists.
 */
PUBLIC void WinMidiEnv::loadDevices()
{
    if (!mDevicesLoaded) {

        MidiPort *lastInput = NULL;

        int ndevs = midiInGetNumDevs();
        if (ndevs > 0) {
            for (int i = 0 ; i < ndevs ; i++) {
                MIDIINCAPS moc;
                int stat = midiInGetDevCaps(i, &moc, sizeof(moc));
                if (stat != MMSYSERR_NOERROR) {
                    trace("Error reading device capabilities for %d!\n", i);
                }
                else {
                    MidiPort* dev = new MidiPort(moc.szPname, i);
                    if (lastInput != NULL)
                      lastInput->setNext(dev);
                    else
                      mInputPorts = dev;
                    lastInput = dev;
                }
            }
        }

        MidiPort *lastOutput = NULL;

        ndevs = midiOutGetNumDevs();
        if (ndevs > 0) {
            for (int i = 0 ; i < ndevs ; i++) {
                MIDIOUTCAPS moc;
                int stat = midiOutGetDevCaps(i, &moc, sizeof(moc));
                if (stat != MMSYSERR_NOERROR) {
                    trace("Error reading device capabilities for %d!\n", i);
                }
                else {
                    MidiPort* dev = new MidiPort(moc.szPname, i);
                    if (lastOutput != NULL)
                      lastOutput->setNext(dev);
                    else
                      mOutputPorts = dev;
                    lastOutput = dev;
                }
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
// Factories
//
//////////////////////////////////////////////////////////////////////

/**
 * Return an instance of the singleton timer.
 */
PUBLIC MidiTimer* WinMidiEnv::newMidiTimer()
{
	return new WinMidiTimer(this);
}

/**
 * Instantiate a MidiInput subclass.
 */
PUBLIC MidiInput* WinMidiEnv::newMidiInput(MidiPort* port)
{
	MidiInput* input = new WinMidiInput(this, port);
	return input;
}

/**
 * Instantiate a MidiOutput subclass.
 */
PUBLIC MidiOutput* WinMidiEnv::newMidiOutput(MidiPort* port)
{
	MidiOutput* input = new WinMidiOutput(this, port);
	return input;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
