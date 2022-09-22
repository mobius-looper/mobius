/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Platform independent representation of a MIDI sequence.
 * These aren't used by Mobius yet, but give it time...
 */

#ifndef MIDI_SEQUENCE_H
#define MIDI_SEQUENCE_H

/****************************************************************************
 *                                                                          *
 *   							  SEQUENCES                                 *
 *                                                                          *
 ****************************************************************************/

/**
 * Object representing a sequence of events, plus some extra state
 * that applies to all events.
 */
class MidiSequence { 

  public:
	
    //
    // Constructors
    //

    MidiSequence();
    MidiSequence(class XmlElement* e);
    ~MidiSequence();

	void setNext(MidiSequence *s);
	void setName(const char *newname);
	void setEvents(class MidiEvent *e);
	void setChannel(int c);
	void setTempo(float t);
	void setDivision(int t);
	void clear(void);

    // the sequence can also be an event factory
	class MidiEvent *newMidiEvent(void);
	class MidiEvent *newMidiEvent(int status, int chan = 0, int key = 0, int vel = 0);

	//
	// accessors
	//

	class MidiEvent *stealEvents(void);

	MidiSequence *getNext(void) {
		return mNext;
	}

	const char *getName(void) {
		return mName;
	}
	
	class MidiEvent *getEvents(void) {
		return mEvents;
	}

	int getChannel(void) {
		return mChannel;
	}

 	int getDivision(void) {
		return mDivision;
	}

	float getTempo(void) {
		return mTempo;
	}

    //
    // Simple event operations
    //
    
	bool isEmpty(void);
	void insert(class MidiEvent *e);
	void replace(class MidiEvent *e);
	void remove(class MidiEvent *e);

    class MidiEvent* getLastEvent();
    class MidiEvent* getNextEvent(MidiEvent* current);
    class MidiEvent* getPrevEvent(MidiEvent* current);

	//
	// XML serialization
	//

	void parseXml(class XmlElement* e);
	void toXml(class XmlBuffer* b);
	char* toXml();

	void readXml(const char* file);
	void writeXml(const char* file);

  private:

	void init();

	MidiSequence 	*mNext;			// next sequence
	char 			*mName;			// optional name
	class MidiEvent	*mEvents;		// event list

    /**
     * Output channel override.
     */
	int 			mChannel;		// output channel for all events

    //
    // these can come in from standard MIDI files
    //

	float 			mTempo;
	int 			mDivision;
	int 	        mTimeSigNum;
	int				mTimeSigDenom;

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
