/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Mac subclass of MidiEnv.
 */

#include <stdio.h>

#include "Port.h"
#include "Util.h"
#include "Thread.h"
#include "MacUtil.h"

#include "MidiPort.h"
#include "MacMidiEnv.h"

//////////////////////////////////////////////////////////////////////
//
// MacMidiPort
//
//////////////////////////////////////////////////////////////////////

MacMidiPort::MacMidiPort()
{
	mEndpoint = NULL;
}

MacMidiPort::~MacMidiPort()
{
	// the Endpoint stays?
}

MIDIEndpointRef MacMidiPort::getEndpoint()
{
	return mEndpoint;
}

void MacMidiPort::setEndpoint(MIDIEndpointRef point)
{
	mEndpoint = point;
}

//////////////////////////////////////////////////////////////////////
//
// MacMidiEnv
//
//////////////////////////////////////////////////////////////////////

/**
 * There can be only one MIDI environment in an application.
 */
PUBLIC MidiEnv* MidiEnv::getEnv()
{
    if (mSingleton == NULL)
      mSingleton = new MacMidiEnv();
    return mSingleton;
}

/**
 * TODO: Establish the CoreMIDI client here?
 */
PUBLIC MacMidiEnv::MacMidiEnv()
{
	mClient = NULL;
	mPortsLoaded = false;
}

PUBLIC MacMidiEnv::~MacMidiEnv()
{
}

//////////////////////////////////////////////////////////////////////
//
// Ports
//
//////////////////////////////////////////////////////////////////////

/**
 * MidiEnv overload to obtain port information from the platform
 * and build the mInputPorts and mOutputPorts lists.
 * 
 * See printEnvironment() for examples of querying the device model.
 * We can walk down from Device through Entity to Endpoints or
 * just ask for the Sources and Destinations directly.  The odd
 * thing about asking for the global endpoints is that other
 * things creep in here that aren't associated with devices.
 * One is these mysterious Bidule endpoints: 
 *
 *     Bidule 1
 *     Bidule 2
 *     ...
 *
 * We might want ta filter those but I'm not sure what to look at.
 * 
 */
PUBLIC void MacMidiEnv::loadDevices()
{
	if (!mPortsLoaded) {

		MidiPort* lastInput = NULL;
		MidiPort* lastOutput = NULL;

		int count = MIDIGetNumberOfSources();
		for (int i = 0 ; i < count ; i++) {
			MIDIEndpointRef src = MIDIGetSource(i);
			MacMidiPort* dev = getPort(src);
			if (dev != NULL) {
				if (lastInput == NULL)
				  mInputPorts = dev;
				else
				  lastInput->setNext(dev);
				lastInput = dev;
			}
		}

		count = MIDIGetNumberOfDestinations();
		for (int i = 0 ; i < count ; i++) {
			MIDIEndpointRef dest = MIDIGetDestination(i);
			MacMidiPort* dev = getPort(dest);
			if (dev != NULL) {
				if (lastOutput == NULL)
				  mOutputPorts = dev;
				else
				  lastOutput->setNext(dev);
				lastOutput = dev;
			}
		}

		mPortsLoaded = true;
	}
}

/**
 * Helper for loadDevices, built a MidiPort from a MIDIEndpointRef.
 *
 * Potentially interesting properties.
 * 
 * kMIDIPropertyReceiveChannels
 * kMIDIPropertyTransmitChannels
 * kMIDIPropertyMaxTransmitChannels
 * kMIDIPropertyMacReceiveChannels
 *
 */
PRIVATE MacMidiPort* MacMidiEnv::getPort(MIDIEndpointRef point)
{
	MacMidiPort* dev = NULL;

	// do we want to include virtuals?
	bool includeVirtuals = true;
	
	MIDIEntityRef entity;
	OSStatus status = MIDIEndpointGetEntity(point, &entity);

	if (includeVirtuals || status != kMIDIObjectNotFound) {

		dev = new MacMidiPort();
		dev->setEndpoint(point);
		char* name = getString(point, kMIDIPropertyDisplayName);
		dev->setName(name);
		delete name;
	}

	return dev;
}

PRIVATE char* MacMidiEnv::getString(MIDIObjectRef obj, CFStringRef prop)
{
	CFStringRef str;
	MIDIObjectGetStringProperty(obj, prop, &str);
	char* cstr = GetCString(str);
	// !! release the CFStringRef?
	return cstr;
}

//////////////////////////////////////////////////////////////////////
//
// Factories
//
//////////////////////////////////////////////////////////////////////

/**
 * Create a millisecond timer.
 * This will cached by MidiEnv so it will only be called once.
 */
PUBLIC MidiTimer* MacMidiEnv::newMidiTimer()
{
	return new MacMidiTimer(this);
}

/**
 * Create an input stream.  
 * TODO: Should we automatically connect these? 
 */
PUBLIC MidiInput* MacMidiEnv::newMidiInput(MidiPort* port)
{
	return new MacMidiInput(this, port);
}

/**
 * Create an output stream.  
 * TODO: Should we automatically connect these? 
 */
PUBLIC MidiOutput* MacMidiEnv::newMidiOutput(MidiPort* port)
{
	return new MacMidiOutput(this, port);
}

//////////////////////////////////////////////////////////////////////
//
// Client
//
//////////////////////////////////////////////////////////////////////

/**
 * Not sure if we only need one of these or one for every input.
 */
MIDIClientRef MacMidiEnv::getClient()
{
	if (mClient == NULL) {
		// each client has a name, not sure why or if it has to be unique
		CFStringRef name = MakeCFStringRef("MacMidiInterface");

		// This is a callback function for "changes to the system"
		// In think this means when devices and endpoints come and go.
		MIDINotifyProc proc = NULL;

		// argument to the MIDINotifyProc
		void* arg = NULL;
		
		OSStatus status = MIDIClientCreate(name, proc, arg, &mClient);
		CheckStatus(status, "MIDIClientCreate");
	}

	return mClient;
}

//////////////////////////////////////////////////////////////////////
//
// Diagnostics
//
//////////////////////////////////////////////////////////////////////

PUBLIC void MacMidiEnv::printEnvironment()
{
	ItemCount count = 0;

	printf("Devices:\n");

	count = MIDIGetNumberOfDevices();
	if (count == 0)
	  printf("  No devices\n");
	else {
		for (int i = 0 ; i < count ; i++) {
			MIDIDeviceRef dev = MIDIGetDevice(i);
			dumpDevice(dev);
		}
	}

	printf("External Devices:\n");

	count = MIDIGetNumberOfExternalDevices();
	if (count == 0) 
	  printf("  No devices\n");
	else {
		for (int i = 0 ; i < count ; i++) {
			MIDIDeviceRef dev = MIDIGetDevice(i);
			dumpDevice(dev);
		}
	}

	printf("Sources:\n");

	count = MIDIGetNumberOfSources();
	if (count == 0) 
	  printf("  No sources\n");
	else {
		for (int i = 0 ; i < count ; i++) {
			MIDIEndpointRef src = MIDIGetSource(i);
			dumpEndpoint("Source", src);
		}
	}

	printf("Destinations:\n");

	count = MIDIGetNumberOfDestinations();
	if (count == 0) 
	  printf("  No destinations\n");
	else {
		for (int i = 0 ; i < count ; i++) {
			MIDIEndpointRef src = MIDIGetDestination(i);
			dumpEndpoint("Destination", src);
		}
	}

	// can also create and dispose endpoints
	// MIDIDestinationCreate
	// MIDISourceCreate
	// MIDIEndpointDispose

}

PRIVATE void MacMidiEnv::dumpDevice(MIDIDeviceRef dev)
{
	char* cstr = getString(dev, kMIDIPropertyName);
	printf("  Device: %s\n", cstr);
	delete cstr;

	ItemCount nents = MIDIDeviceGetNumberOfEntities(dev);
	if (nents == 0)
	  printf("    No entities\n");
	else {
		for (int i = 0 ; i < nents ; i++) {
			MIDIEntityRef ent = MIDIDeviceGetEntity(dev, i);
			dumpEntity(ent);
		}
	}
}

PRIVATE void MacMidiEnv::dumpEntity(MIDIEntityRef ent)
{
	// MIDIEntityGetDevice returns the parent MIDIDeviceRef

	char* cstr = getString(ent, kMIDIPropertyName);
	printf("    Entity: %s\n", cstr);
	delete cstr;

	ItemCount npoints = MIDIEntityGetNumberOfSources(ent);
	if (npoints == 0)
	  printf("      No sources\n");
	else {
		for (int i = 0 ; i < npoints ; i++) {
			MIDIEndpointRef point = MIDIEntityGetSource(ent, i);
			dumpEndpoint("Source", point);
		}
	}

	npoints = MIDIEntityGetNumberOfDestinations(ent);
	if (npoints == 0)
	  printf("      No destinations\n");
	else {
		for (int i = 0 ; i < npoints ; i++) {
			MIDIEndpointRef point = MIDIEntityGetDestination(ent, i);
			dumpEndpoint("Destination", point);
		}
	}
}

/**
 * Potentially interesting properties:
 *
 * kMIDIPropertyReceiveChannels
 * kMIDIPropertyTransmitChannels
 * kMIDIPropertyMaxTransmitChannels
 * kMIDIPropertyMacReceiveChannels
 */
PRIVATE void MacMidiEnv::dumpEndpoint(const char* type, MIDIEndpointRef point)
{
	char* cstr;

	cstr = getString(point, kMIDIPropertyName);
	printf("        %s: %s\n", type, cstr);
	delete cstr;

	cstr = getString(point, kMIDIPropertyDisplayName);
	printf("          Display name: %s\n", cstr);
	delete cstr;

	SInt32 ival;
	MIDIObjectGetIntegerProperty(point, kMIDIPropertyPrivate, &ival);
	printf("          Private: %d\n", (int)ival);

	MIDIEntityRef entity;
	OSStatus status = MIDIEndpointGetEntity(point, &entity);
	if (status == kMIDIObjectNotFound) {
        printf("          Virtual\n");
	}

}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
