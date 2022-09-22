/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * MOBIUS PUBLIC INTERFACE
 *
 * Model for MIDI event mapping, may be installed in both 
 * MidiInput and MidiOutput streams.  This is an old utility that
 * arguably should be done at a higher level if we needed it, it is
 * not currently used by Mobius.
 *
 */

#ifndef MIDI_MAP_H
#define MIDI_MAP_H


/**
 * Object used to specify a single event mapping.
 * Used by MidiMapDefinition and MidiMap.
 * channel and status may be -1 to indicate
 * that they match all channels and statuses.
 */
class MidiMapEvent {

  public:

    MidiMapEvent();
    ~MidiMapEvent();
    
    bool hasWildcard();

    int channel;
    int status;
    int value;

    int mapChannel;
    int mapStatus;
    int mapValue;

};

/**
 * Object used to define the contents of a MidiMap.
 * These are easier to build and can be "compiled"
 * into a MidiMap for runtime use.
 */
class MidiMapDefinition {

  public:

    MidiMapDefinition();
    ~MidiMapDefinition();

    void addEvent(MidiMapEvent* e);
    class List* getEvents();
    class List* stealEvents();

  private:

    class List* mEvents;
};

/**
 * Object used to define event mapping to be performed by the
 * MIDI input interrupt handler.
 * 
 * Once installed, you may modify the map at any time, though its
 * probably best to do this only when there aren't events going through, 
 * otherwise you could end up with note off's that don't match note on's etc.
 *
 * This could be more sophisticated, handling touch/bend transformations,
 * non-event specific rechannelization, etc.  
 */
class MidiMap {

  public:

	MidiMap(void);
    MidiMap(MidiMapDefinition* def);
	~MidiMap(void);

    void parseDefinition(MidiMapDefinition* def);
    void addEvent(MidiMapEvent* e);

    void map(int* channel, int* status, int* byte1, int* byte2);

	void dump();

  private:

	void init(void);
    void addEvent(MidiMapEvent* e, int channel);
    void addEvent(MidiMapEvent* e, MidiMapEvent*** channelMap, 
                  int status);

    // this is essentially a three dimensional array
    // the first array is indexed by channel
    // the second is indexed by the high order byte of the status
    // the third is indexed by the value/key
    // the result is a MidiMapEvent object

    MidiMapEvent**** mMaps;
    class List* mEvents;

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
