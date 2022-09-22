/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Classes used to maintain MIDI stream state for realtime events.
 * 
 */

#ifndef MIDI_QUEUE_H
#define MIDI_QUEUE_H


/****************************************************************************
 *                                                                          *
 *   							  CONSTANTS                                 *
 *                                                                          *
 ****************************************************************************/

/**
 * We maintain an array of MidiSyncEvents, the MIDI thread fills it from
 * the head, and the audio interrupt consumes it from the tail.
 * If this array fills, we'll drop events, but that should only
 * happen if the audio interrupt is stuck in a loop.
 */
#define MAX_SYNC_EVENTS 128
 
/****************************************************************************
 *                                                                          *
 *   						   MIDI SYNC EVENT                              *
 *                                                                          *
 ****************************************************************************/

/**
 * Little structure used by MidiQueue to maintain an ordered list
 * of sync events that came in since the last interrupt.
 */
class MidiSyncEvent {
  public:

	int status;		// one of the MS_ constants (START, STOP, CLOCK, etc.)
	int songpos;	// valid if MS_SONG_POSITION
	long clock;		// millisecond timer clock
};

/****************************************************************************
 *                                                                          *
 *   							  MIDI STATE                                *
 *                                                                          *
 ****************************************************************************/

/**
 * Little state machine that watches a stream of MIDI real time events.
 */
class MidiState {

  public:

	MidiState();
	~MidiState();

	void tick(long millisecond);
	void advance(MidiSyncEvent* e);

	/**
	 * Name used for trace messages.
	 */
	const char* name;

	/**
	 * The millisecond timestamp of the last MS_CLOCK event.
	 * Used to measure the distance between clocks to see if the clock
	 * stream has started or stopped.
	 */
	long lastClockMillisecond;

	/**
	 * True if clocks are comming in often enough for us to consider
	 * that the clock stream has started.
	 */
	bool receivingClocks;

	/**
	 * Set after receiving a MS_SONGPOSITION event.
	 * We don't change position immediately, but save it for the
	 * next MS_CONTINUE event.
	 */
	int songPosition;

	/**
	 * Number of MIDI clocks within the "song".  This is set to zero
	 * after an MS_START, or derived from songPosition after an
	 * MS_CONTINUE.  It then increments without bound.
	 */
	long songClock;

	/**
	 * This starts at zero and counts up to 24, then rolls back to zero.
	 * When it reaches 24, the "beat" field is incremented.
	 * This is It is recalcualted whenever songClock changes.
	 */
	int beatClock;

	/**
	 * Incremented whenever beatClocks reaches 24.
	 * The beat counter increments without bound since the notion
	 * of a "bar" is a higher level concept that can change at any time.
	 */
	int beat;

	/**
	 * The status code of the last MIDI event that requires that we wait
	 * until the next clock to activate.  This will be either MS_START
	 * or MS_CONTINUE.   This is cleared immediately after receiving
	 * the next MS_CLOCK *after* the one that caused us to start.
	 */
	int waitingStatus;

	/**
	 * True if we've entered a start state after receiving either
	 * an MS_START or MS_CONTINUE event, and consuming the MS_CLOCK
	 * event immediately following.
	 */ 
	bool started;

};

/****************************************************************************
 *                                                                          *
 *   							  MIDI QUEUE                                *
 *                                                                          *
 ****************************************************************************/

/**
 * Maintains an ordered list of MidiSyncEvents that were accumulated
 * in between audio interrupts.  Also contains a MidiState object that will
 * be fed the MidiSyncEvents and calculates the running sync status 
 * such as started, stopped, beat and bar boundaries.
 *
 * Mobius uses two of these, one for MIDI events comming in from
 * an actual MIDI device, and another for the pseudo "loopback" device
 * that allows us to pipe sync events we send OUT back to ourselves.
 */
class MidiQueue {
  public:

  public:

	MidiQueue();
	~MidiQueue();

	/**
	 * Assign this a name for internal trace messages.
	 * The name must be a stable string constant.
	 */
	void setName(const char* name);

	/**
	 * Add an incomming MIDI event, should be called only from the
     * MidiListener thread.
	 */
	void add(class MidiEvent* e);
    void add(int status, long clock);

	/**
	 * Prepare for an audio interrupt.
	 */
	void interruptStart(long millisecond);

    /**
     * Convert the queued MidiSyncEvents into a list of Event
     * objects that can be procesed in this interrupt.
     */
    class Event* getEvents(class EventPool* pool, long interruptFrames);

	/**
	 * Get the entire running status for exposure in Variables.
	 */
	MidiState* getMidiState();

    // diagnostics
    bool hasEvents();

  private:

	// state that needs to carry over into the next interrupt
	MidiState mState;

	// number of events we couldn't process
	long mOverflows;

	// counters incremented by the MIDI thread
	int mHead;
	int mTail;
	MidiSyncEvent mEvents[MAX_SYNC_EVENTS];

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
