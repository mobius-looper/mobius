/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Model for a 16 channel MIDI input or output port.
 *
 */

#include <stdio.h>
#include <string.h>

#include "Port.h"
#include "Util.h"

#include "MidiPort.h"

//////////////////////////////////////////////////////////////////////
//
// MidiPort
//
//////////////////////////////////////////////////////////////////////

PUBLIC MidiPort::MidiPort()
{
	init();
}

PUBLIC MidiPort::MidiPort(const char *name)
{
	init();
	setName(name);
}

/**
 * This is the signature Windows uses, since we don't use
 * ids on Mac we should remove it from the model?
 */
PUBLIC MidiPort::MidiPort(const char *name, int id)
{
	init();
	setName(name);
	mId = id;
}

PRIVATE void MidiPort::init()
{
	mNext = NULL;
	mName = NULL;
	mId = 0;
}

PUBLIC MidiPort::~MidiPort() 
{
	MidiPort *el, *nextel;

	delete mName;

    for (el = mNext, nextel = NULL ; el != NULL ; el = nextel) {
        nextel = el->mNext;
        el->mNext = NULL;	// prevent recursion
        delete el;
    }

}

PUBLIC MidiPort* MidiPort::getNext()
{
	return mNext;
}
	
PUBLIC void MidiPort::setNext(MidiPort *i) 
{
	mNext = i;
}

PUBLIC const char* MidiPort::getName() 
{
	return mName;
}

PUBLIC void MidiPort::setName(const char* name)
{
	delete mName;
	mName = CopyString(name);
}

PUBLIC int MidiPort::getId() 
{
	return mId;
}

PUBLIC void MidiPort::setId(int i) 
{
	mId = i;
}

/**
 * Search a MidiPort list for one with the given name.
 */
PUBLIC MidiPort* MidiPort::getPort(const char* name)
{
    MidiPort* found = NULL;
    if (name != NULL) {
        for (MidiPort* dev = this ; dev != NULL ; dev = dev->getNext()) {
            if (!strcmp(dev->getName(), name)) {
                found = dev;
                break;
            }
        }
    }
    return found;
}

/**
 * Search a MidiPort list for one with a given id.
 */
PUBLIC MidiPort* MidiPort::getPort(int id)
{
    MidiPort* found = NULL;
    if (id >= 0) {
        for (MidiPort* dev = this ; dev != NULL ; dev = dev->getNext()) {
            if (dev->getId() == id) {
                found = dev;
                break;
            }
        }
    }
    return found;
}

/**
 * Return an array of all port names in the list.
 */
PUBLIC char** MidiPort::getNames() 
{
	char **names = NULL;
	MidiPort *info;

	int count = 0;
	for (info = this ; info != NULL ; info = info->getNext())
	  count++;

	names = new char*[count + 1];
	names[count] = NULL;
	count = 0;
	for (info = this ; info != NULL ; info = info->getNext(), count++)
		names[count] = CopyString(info->getName());

	return names;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
