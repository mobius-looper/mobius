/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * MOBIUS PUBLIC INTERFACE
 *
 * Platform-independent representation of a MIDI event.
 *
 */

#ifndef MIDI_EVENT_H
#define MIDI_EVENT_H

// need this for some inline methods
#include "midibyte.h"

/**
 * Interface of something that owns the event, used when freeing events.
 */
class MidiEventManager {

  public:

	virtual class MidiEvent* newMidiEvent() = 0;
	virtual void freeMidiEvents(class MidiEvent* list) = 0;

};

/**
 * MS_CMD_
 *
 * Command event types.
 * These are special events that are encoded in the MidiEvent structure
 * but which don't correspond to real events to be sent to devices.
 * The CMD_ codes are stored in the status field of the event, without
 * the high bit set to identify them as non-standard MIDI status bytes.
 * Some of these events like LOOP, are stored as permenent parts of
 * the sequence.  Others like ERASE, are transient and only used during
 * recording of a sequence.
 *
 * NOTE: not sure how much these are used any more?
 *
 * The meaning of event fields is dependent on the type of event.
 *
 * MS_CMD_LOOP
 *   Used to loop between two points.  Loop time is determined by
 *   adding the loop start clock with the duration.
 *     clock	  time at which the loop region starts
 *     duration   width of the loop region
 *     value      loop counter
 * 
 * MS_CMD_CALL
 *   Used to jump into a nested sequence.
 *     clock 	time at which to call 
 *     data     (SEQUENCE *)to call
 *
 * MS_CMD_ERASE
 *   clock	time at which to begin the erasure
 *   duration   time at which to stop the erasure
 *   
 */
#define MS_CMD_LOOP 1
#define MS_CMD_CALL 2
#define MS_CMD_ERASE 10

/**
 * Maximum length of the special name event.
 */
#define SEQ_MAX_EVENT_NAME 80

/**
 * The highest possible clock value.
 * Use this so we don't depend too much on signed vs. unsigned storage.
 * Currently clocks are signed 32 bit integers.
 * 
 * Do not use -1 here, we do too much signed comparisons of this.
 */
#define MIDI_MAX_CLOCK 0x7FFFFFFF

/**
 * Class used for the memory representation of MIDI events.
 * These are normally created by the MidiEnv::newEvent method and 
 * maintained in a pool.
 */
class MidiEvent { 

  public:

    //
    // Constructors
    //

	MidiEvent();
	MidiEvent(class XmlElement* e);
	~MidiEvent();
    MidiEvent* copy();
	void reinit();
    void free();

	void setManager(MidiEventManager* man) {
		mManager = man;
	}

	void setNext(MidiEvent *n) {
		mNext = n;
	}

	void setStack(MidiEvent *s) {
		mStack = s;
	}

	void setClock(int c) {
		mClock = c;
	}

	void setStatus(int s) {
		mStatus = s;
	}
	
	void setChannel(int c) {
		mChannel = c;
	}

	void setKey(int k) {
		mKey = k;
	}

	void setVelocity(int v) {
		mVelocity = v;
	}

	void setDuration(int d) {
		mDuration = d;
	}

	void setExtra(int v) {
		mExtra = v;
	}

	void setData(void *d) {
		mData = d;
	}

	//
	// accessors
	//

    void dump(bool simple);
	void printType(char* buffer);

	MidiEvent *getNext(void) {
		return mNext;
	}

	MidiEvent *getStack(void) {
		return mStack;
	}

	int getStatus(void) {
		return mStatus;
	}

	int getClock(void) {
		return mClock;
	}

	int getChannel(void) {
		return (int)mChannel;
	}

	int getKey(void) {
		return (int)mKey;
	}

	int getProgram(void) {
		return (int)mKey;
	}

	int getController() {
		return (int)mKey;
	}

	int getValue(void) {
		return (int)mVelocity;
	}

	int getVelocity(void) {
		return (int)mVelocity;
	}

	int getDuration(void) {
		return mDuration;
	}

	int getExtra(void) {
		return mExtra;
	}
	
	const void *getData(void) {
		return mData;
	}

	int getSongPosition(void) {
		return (mKey | mVelocity << 7);
	}

    int getPitchBend(void) {
		return (mKey | mVelocity << 7);
    }

	//
	// convenient type predicates
	//
	
	bool isNote(void) {
		return mStatus == MS_NOTEON;
	}

	// what about name events ? 
	bool isProgram(void) {
		return mStatus == MS_PROGRAM;
	}

	bool isController(void) {
		return MIDI_IS_CONTROLLER_STATUS(mStatus);
	}

	//
	// XML serialization for diagnistics
	//
	
	void parseXml(class XmlElement* e);
	void toXml(class XmlBuffer* b);

	// 
	// List operations
    // I think I'd rather these be MidiSequence methods
    //

    MidiEvent *getLast(int status);
	MidiEvent *getNextEvent(void);
	MidiEvent *insert(MidiEvent *neu);
	MidiEvent *replace(MidiEvent *neu);
	MidiEvent *remove(MidiEvent *e);

  private:

	void init();

	MidiEventManager* mManager;

	MidiEvent* 	mNext;		// list link
	MidiEvent*	mStack;	// for the sequencer only

	// most of these can be "byte"

	int 		mClock;		// absolute time of the event
	int 		mStatus;	// midi status byte (without channel)
	int			mChannel; 	// specific channel (FF if not known)
	int 		mKey;		// key, controller, program, command
	int			mVelocity;	// velocity, controller value
	int			mDuration;	// duration, pixel, command duration
	int			mExtra;		// command parameter, loop counter
	const void 	*mData;		// name, commands, loop state

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
