/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * An abstract interface for MIDI input streams.
 *
 */

#ifndef MIDI_INPUT_H
#define MIDI_INPUT_H

/****************************************************************************
 *                                                                          *
 *                                 MIDI INPUT                               *
 *                                                                          *
 ****************************************************************************/

/**
 * Interface of an object that will be notified of MIDI input activity.
 * This is is lower level than MidiListener, it is expected to process
 * multiple MidiEvents and possibly MidiSysexes. 
 */
class MidiInputListener {
  public:
	virtual void midiInputEvent(class MidiInput* in) = 0;
};

/**
 * Object used to specify event filtering options for the MIDI input stream.
 * TODO: Either split this out or combine it with MidiMap so we can use it
 * through MidiInterface.
 */
class MidiFilter {

  public:

	int polyPressure;
	int control;
	int program;
	int touch;
	int bend;
	int common;
	int sysex;
	int realtime;

	MidiFilter(void);
	~MidiFilter(void);
	void init(void);
};

/**
 * Object encapsulating operations and state related to an open MIDI input port.
 */
class MidiInput {

	//
    // You obtain one these from MidiEnv factory methods
	//
	friend class MidiEnv;

  public:

	virtual ~MidiInput();

    MidiInput* getNext() {
        return mNext;
    }

	//
	// Configuration
	//

	void setPort(class MidiPort* port);
	class MidiPort* getPort();

	void setTimer(class MidiTimer *t);

	// TODO: Need to support more than one
	void setEchoDevice(class MidiOutput* out);
    void removeEchoDevice(class MidiOutput* out);

	void setListener(MidiInputListener* l) {
		mListener = l;
	}
	
	MidiInputListener* getListener() {
		return mListener;
	}

	void setInputMap(class MidiMap* map);
	void setInputMap(class MidiMapDefinition* def);

	// Echo configuration
	// TODO: This should be moved to the output device, or is it
	// a property of the "wire" between the two?
	void setEchoMap(class MidiMap* map);
	void setEchoMap(class MidiMapDefinition* def);

	// Accessors

	// want to have a safer interface here ? 
	MidiFilter *getFilters(void) {
		return &mFilters;
	}

	//
	// Interrupt handlers
	//

	void processShortMessage(int msg);

	//
	// Listener callback interface
	//

	float getPulseWidth();
	float getTempo();
	int getSmoothTempo();

	class MidiEvent *getEvents(void);
	void ignoreEvents(void);

	void incShortErrors() {
		mShortErrors++;
	}
	void incLongErrors() {
		mLongErrors++;
	}
	void incLongOverflows() {
		mLongOverflows++;
	}

	//
	// Subclass overloads
	//

	/**
	 * Establish a connection to the configured MidiPort.
	 */
	virtual int connect(void) = 0;

	/**
	 * Terminate a connection to the configured MidiPort.
	 */
	virtual void disconnect(void) = 0;

	/**
	 * Return true if a connection to the MidiPort has been established.
	 */
	virtual bool isConnected() = 0;

	/**
	 * Called when events are received.
	 * Do whatever is necessary to place the events in a place that is
	 * accessible to the listener and call the listener.
	 * May notify a monitor thread or call the listener immediately.
	 */
	virtual void notifyEventsReceived() = 0;

	/**
	 * Ignore any sysex events that have come in.
	 * This shouldn't be virtual but all sysex handling is still
	 * only in windows.
	 */
	virtual void ignoreSysex() = 0;

	/**
	 * Subclass may overload this to print platform-specific warnings.
	 */
	virtual void printWarnings();
	
  protected:

	MidiInput(class MidiEnv* env, class MidiPort* port);

	void setNext(MidiInput *n) {
		mNext = n;
	}

	void enterCriticalSection(void);
	void leaveCriticalSection(void);

	class MidiEnv*		  mEnv;
	MidiInput* 			  mNext;		// for MidiEnv's list
	class MidiPort* 	  mPort;
    class MidiTimer*	  mTimer;		// timer for timestamping events
	class TempoMonitor*   mTempo;
	bool			      mEnabled;		// set if we've called midiInStart

	class CriticalSection* mCsect;

	// input event filters
	MidiFilter	mFilters;

	// input event mapping rules
	class MidiMap* mInputMap;

	// echo
	class MidiOutput*	mEchoDevice;	// device to use for echo
    class MidiMap*      mEchoMap;       // echo mappings

	// incomming event list
	class MidiEvent* mEvents;
	class MidiEvent* mLastEvent;

	//
	// Callback state
	//

	MidiInputListener* mListener;
	int  mInInterruptHandler;
	bool mInCallback;

	//
	// Misc statistics
	//

	int mShortErrors;
	int mLongErrors;
	int mWeirdErrors;
	int mEventOverflows;
	int mInterruptOverruns;
	int mLongOverflows;

};

/****************************************************************************
 *                                                                          *
 *   							TEMPO MONITOR                               *
 *                                                                          *
 ****************************************************************************/

/*
 * The number of tempo "samples" we maintain for the running average.
 * A sample is the time in milliseconds between clocks.
 * 24 would be one "beat", works but is jittery at tempos above 200.
 * Raising this to 96 gave more stability.  The problem is that the
 * percieved tempo changes more slowly as we smooth over an entire bar.
 */
#define MIDI_TEMPO_SAMPLES 24 * 4

/**
 * The number of tempo samples that the tempo has to remain +1 or -1
 * from the last tempo before we change the tempo.
 */
#define MIDI_TEMPO_JITTER 24

/**
 * This is used internally by MidiInput to calculate a smooth tempo from
 * incomming MIDI clocks.
 */
class TempoMonitor {

  public:
 
	TempoMonitor();
	~TempoMonitor();

	void reset();
	void clock(long msec);

	float getPulseWidth();
	float getTempo();
	int getSmoothTempo();

  private:

	void initSamples();

	long mSamples[MIDI_TEMPO_SAMPLES];
	long mLastTime;
	int mSample;
	long mTotal;
	int mDivisor;

	float mPulse;
	int mSmoothTempo;		// stable tempo * 10
	int mJitter;

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
