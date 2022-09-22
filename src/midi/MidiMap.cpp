/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Model for MIDI event mapping, may be installed in both 
 * MidiInput and MidiOutput streams.  This is an old utility that
 * arguably should be done at a higher level if we needed it, it is
 * not currently used by Mobius.
 *
 */

#include <stdio.h>
#include <string.h>

#include "port.h"
#include "List.h"

#include "midibyte.h"
#include "MidiMap.h"

/****************************************************************************
 *                                                                          *
 *                               MIDI MAP EVENT                             *
 *                                                                          *
 ****************************************************************************/

PUBLIC MidiMapEvent::MidiMapEvent()
{
    channel = -1;
    status = -1;
    value = -1;
    mapChannel = -1;
    mapStatus = -1;
    mapValue = -1;
}

PUBLIC MidiMapEvent::~MidiMapEvent()
{
}

PUBLIC bool MidiMapEvent::hasWildcard()
{
    return (channel == -1 || status == -1);
}

/****************************************************************************
 *                                                                          *
 *                            MIDI MAP DEFINITION                           *
 *                                                                          *
 ****************************************************************************/

PUBLIC MidiMapDefinition::MidiMapDefinition()
{
    mEvents = new List();
}

PUBLIC MidiMapDefinition::~MidiMapDefinition()
{
    if (mEvents != NULL) {
        for (int i = 0 ; i < mEvents->size() ; i++) {
            MidiMapEvent* e = (MidiMapEvent*)mEvents->get(i);
            delete e;
        }
    }
    delete mEvents;
}

PUBLIC void MidiMapDefinition::addEvent(MidiMapEvent* e)
{
    mEvents->add(e);
}

PUBLIC List* MidiMapDefinition::getEvents()
{
    return mEvents;
}

PUBLIC List* MidiMapDefinition::stealEvents()
{
	List* events = mEvents;
	mEvents = NULL;
	return events;
}

/****************************************************************************
 *                                                                          *
 *                                  MIDI MAP                                *
 *                                                                          *
 ****************************************************************************/

PUBLIC MidiMap::MidiMap(void) 
{
	init();
}

PUBLIC MidiMap::MidiMap(MidiMapDefinition* def) 
{
	init();
	parseDefinition(def);
	delete def;
}

PUBLIC void MidiMap::init(void) 
{
	mEvents = NULL;
    mMaps = NULL;
}

PUBLIC MidiMap::~MidiMap(void) 
{
	// the maps contains references to the events but an event
	// can be in the map muliple times
	if (mMaps != NULL) {
        for (int i = 0 ; i < 16 ; i++) {
			MidiMapEvent*** channelMap = mMaps[i];
			if (channelMap != NULL) {
				for (int j = 0 ; j < 16 ; j++)
				  delete channelMap[j];
				delete channelMap;
			}
		}
		delete mMaps;
	}

	if (mEvents != NULL) {
		for (int i = 0 ; i < mEvents->size() ; i++) {
			MidiMapEvent* e = (MidiMapEvent*)mEvents->get(i);
			delete e;
		}
		delete mEvents;
	}
}

PUBLIC void MidiMap::parseDefinition(MidiMapDefinition* def)
{
    if (def != NULL) {
		// we take ownership of the MidiMapEvents
		delete mEvents;
		mEvents = def->stealEvents();
        if (mEvents != NULL) {
            int i;
            // first do events without wildcards
            for (i = 0 ; i < mEvents->size() ; i++) {
                MidiMapEvent* e = (MidiMapEvent*)mEvents->get(i);
                if (!e->hasWildcard())
				  addEvent(e);
            }
            // then ones with wildcards, in theory need
            // to be processing these in some order
            // of "specififity" in case multiple wildcards
            // are used
            for (i = 0 ; i < mEvents->size() ; i++) {
                MidiMapEvent* e = (MidiMapEvent*)mEvents->get(i);
                if (e->hasWildcard())
                  addEvent(e);
            }
        }
	}
}

PUBLIC void MidiMap::addEvent(MidiMapEvent* e)
{
    int ch = e->channel;
    if (ch >= 0)
      addEvent(e, ch);
    else {
        // wildcard for all channels
        for (int i = 0 ; i < 16 ; i++)
          addEvent(e, i);
    }
}

PRIVATE void MidiMap::addEvent(MidiMapEvent* e, int channel)
{
    if (mMaps == NULL) {
        mMaps = new MidiMapEvent***[16];
        for (int i = 0 ; i < 16 ; i++)
          mMaps[i] = NULL;
    }
    
    MidiMapEvent*** channelMap = mMaps[channel];
    if (channelMap == NULL) {
        channelMap = new MidiMapEvent**[16];
        for (int i = 0 ; i < 16 ; i++)
          channelMap[i] = NULL;
        mMaps[channel] = channelMap;
    }
    
    int status = e->status;
    if (status >= 0)
      addEvent(e, channelMap, status);
    else {
        for (int i = 0 ; i < 16 ; i++)
          addEvent(e, channelMap, i);
    }

}

PRIVATE void MidiMap::addEvent(MidiMapEvent* e, MidiMapEvent*** channelMap, 
                               int status)
{
    int index = status >> 4;
    MidiMapEvent** statusMap = channelMap[index];
    if (statusMap == NULL) {
        statusMap = new MidiMapEvent*[128];
        for (int i = 0 ; i < 128 ; i++)
          statusMap[i] = NULL;
        channelMap[index] = statusMap;
    }
    if (e->value >= 0) {
		if (statusMap[e->value] == NULL)
		  statusMap[e->value] = e;
	}
    else {
        for (int i = 0 ; i < 128 ; i++) {
			if (statusMap[i] == NULL)
			  statusMap[i] = e;
		}
	}

    // If status is MS_NOTEON, add the same thing
    // for MS_NOTEOFF.  We'll use the same MidiMapEvent
    // for both on/off
    if (status == MS_NOTEON)
      addEvent(e, channelMap, MS_NOTEOFF);
}

PUBLIC void MidiMap::map(int* channel, int* status, 
                         int* byte1, int* byte2)
{
    if (mMaps != NULL) {
        MidiMapEvent*** channelMap = mMaps[*channel];
        if (channelMap != NULL) {
            int oldStatus = *status;
            int index = oldStatus >> 4;
            MidiMapEvent** statusMap = channelMap[index];
            if (statusMap != NULL) {
                MidiMapEvent* e = statusMap[*byte1];
                if (e != NULL) {

                    if (e->mapChannel >= 0)
                      *channel = e->mapChannel;

                    // be smart about some type changes,
                    // there are others, but this should be enough for now
                    int newStatus = e->mapStatus;
                    int newByte2 = *byte2;
                    if (newStatus >= 0) {
                        // ignore release velocity
                        if (newStatus == MS_CONTROL) {
                            if (oldStatus == MS_NOTEOFF)
                              newByte2 = 0;
                        }
                        *status = newStatus;
                    }

                    if (e->mapValue >= 0)
                      *byte1 = e->mapValue;
                    
                    *byte2 = newByte2;
                }
            }
        }
    }
}

PUBLIC void MidiMap::dump()
{
	if (mMaps == NULL)
	  printf("MidiMap empty\n");
	else {
		for (int i = 0 ; i < 16 ; i++) {
			MidiMapEvent*** channelMap = mMaps[i];
			if (channelMap != NULL) {
				for (int j = 0 ; j < 16 ; j++) {
					MidiMapEvent** statusMap = channelMap[j];
					if (statusMap != NULL) {
						for (int k = 0 ; k < 128 ; k++) {
							MidiMapEvent* e = statusMap[k];
							if (e != NULL) {
								
								printf("Channel %d Status %d Key %d mapChanel %d mapStatus %d mapKey %d\n",
									   i, j, k, 
									   e->mapChannel, e->mapStatus, e->mapValue);

							}
						}
					}
				}
			}
		}
	}
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
