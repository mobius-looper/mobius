/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * An abstract interface for a high resolution timer.
 * The timer doesn't have to be used with MIDI applications and the
 * platform implementation may not use MIDI APIs.  But it is closely
 * related to the other Midi interfaces so we'll give it the Midi prefix.
 *
 * The timer is built upon a millisecond resolution system clock.
 * There will only be one of these within an application since the underlying
 * high-res timer may be a scarce resource.
 * Call MidiEnv::getTimer to allocate one.
 * 
 * Upon this we maintain state for two virtual clocks, the "MIDI clock"
 * and the "user clock".  The MIDI clock ticks 24 times per beat as
 * defined by the MIDI standard.  It is typically used when sending clock
 * pulses to drum machines.  The user clock has an arbitrary resolution
 * determined by the resolution (clocks-per-beat) and tempo parameters.
 * The user clock is typically configured by MIDI sequencing application
 * according to the user's desired tempo and clock resolution.
 *
 * Maintaining both virtual clocks within the Timer frees the
 * application from having to bother with sending MIDI clock pulses.
 * The application just sets the tempo and MidiOutput device and the
 * Timer emits the clock events.
 *
 * The application receives notification of timer events by registering
 * a callback function and setting the user clock time at which it wants
 * to be called.  This is called the "signal clock".
 *
 * Note that the application is not automatically notified on every user
 * clock tick.  A typical sequencing application will examine all of the
 * MIDI events waiting to be sent and determine the nearest event time.  
 * This time is then registered with the Timer as the next signal clock, 
 * and the sequencer waits to be called.  After processing the 
 * events, the callback function then determines the next nearest time
 * when something interesting happens and sets a new signal clock.
 * This approach allows us to notify the application only when necessary
 * rather than on every user clock tick which can reduce overhead.
 * If desired, an application can however be notified on every tick by
 * continually setting the signal clock to one clock greater than the
 * current clock in the callback.
 *
 * The timer also maintains a logical "measure" counter if you
 * set the beats-per-measure parameter.  This doesn't affect the speed
 * or resolution of the clocks, it is just a convenient counter if
 * application wants to organize the display of events using 
 * measure markers.
 *
 * TODO: Support SMPTE-oriented configuration.
 */

#ifndef MIDI_TIMER_H
#define MIDI_TIMER_H

//////////////////////////////////////////////////////////////////////
//
// Constants
//
//////////////////////////////////////////////////////////////////////

/**
 * Default tempo for the Timer, expressed in beats per minute.
 */
#define TIMER_DEFAULT_TEMPO 90.0f

/**
 * Default Timer clocks per beat.  This determines the time quantiziation
 * of the MIDI events, low CPB will require less overhead but 
 * cause more quantization.  96 was a common value in the late eighties,
 * but modern sequencers have better resolution.  
 * Find out what Sonar uses...
 */
#define TIMER_DEFAULT_CPB 96

/**
 * TIMER_MSEC_PER_CLOCK
 *
 * A macro to calculate the number of milliseconds per clock
 * given a tempo expressed as beats-per-minute and clock resolution
 * expressed as clocks-per-beat.
 */
#define TIMER_MSEC_PER_CLOCK(bpm, cpb) \
(1000.0f / (((float)bpm * (float)cpb) / 60.0f))

/**
 * The maximum number of timer clock registers.
 * These may be used to capture the current clock for later analysis.
 */
#define TIMER_MAX_REGISTERS 8

/**
 * The maximun number of MidiOutput devices that the timer can
 * send clock events to.
 */
#define TIMER_MAX_OUTPUTS 8

/**
 * A callback function that may be registered with the timer.
 * It will be called whenever the timer reaches a predefined "signal time".
 * Typically this is the clock of the next interesting event for
 * a sequencer, such as a MIDI note event.  The callback is expected
 * to set the next signal time.
 *
 * This is not used by Mobius, and should really be a "listener" interface.
 * Gag, it's not the 80's any more.
 */
typedef void (*TIMER_CALLBACK)(class MidiTimer *timer, void *args);

//////////////////////////////////////////////////////////////////////
//
// MidiTimer
//
//////////////////////////////////////////////////////////////////////

/**
 * The abstract interface of the MIDI timer.
 */
class MidiTimer {

	friend class MidiEnv;

  public:

	//
	// Configuration
	//

	void setResolution(int cpb);
	void setTempo(float bpm);
	void setBeatsPerMeasure(int beats);
	void setNextSignalDelay(int clocks);
	void setNextSignalClock(int clock);
	void setCallback(TIMER_CALLBACK cb, void *args);
	void setMidiClockListener(class MidiClockListener* l);
	void setInterruptEnabled(bool b);

	void resetMidiOutputs();
	void addMidiOutput(class MidiOutput *odev);
    void removeMidiOutput(class MidiOutput* odev);

	//
	// registers
	//

	void setRegister(int reg, int clk);
	void captureRegister(int reg);
	void restoreRegister(int reg);
	void clearRegisters(void);

	//
	// MIDI realtime generation
	//

	void setMidiSync(bool b);
	void midiStartClocks();
	void midiStopClocks();
	void midiStart();
	void midiStop();
	void midiStop(bool stopClocks);
	void midiContinue();
	void midiContinue(bool sendSongPosition);
	
	bool isMidiStarted();
	bool isSendingClocks();

	//
	// Sequencer transport control
	//

	void setClock(int clock);
	void setSongPosition(int psn);
	bool transStart(int initialDelay = 0);
	void transStop();
	void transContinue();

	// called evey millisecond by the platform-specific timer interrupt
	void interrupt();

	//
	// accessors
	//

	long getMilliseconds(void) {
		return mMillis;
	}

	int getClock(void) {
		return mClock;
	}

	int getResolution(void) {
		return mClocksPerBeat;
	}

	int getBeatsPerMeasure(void) {
		return mBeatsPerMeasure;
	}

	int getSongPosition(void) {
		return mSongPosition;
	}

	bool isMidiSync(void) {
		return mMidiSync;
	}

	int getMidiClocks();
	float getMidiMillisPerClock();
	float getTempo(void);
	
	void printWarnings();

	// subclass overloads 

	virtual bool start() = 0;
	virtual void stop() = 0;
	virtual	bool isRunning() = 0;

  protected:

    MidiTimer(class MidiEnv* env);
    virtual ~MidiTimer();

  private:

    void updateClock();
	void setPendingTempo();
	void setTempoInternal(float tempo);

	void sendClock();
	void sendStart();
	void sendStop();
	void sendContinue();
	void sendSongPosition(int psn);

	class MidiEnv* mEnv;

    //
    // Configuration parameters
    //

    float mBeatsPerMinute;         // current tempo
	float mNewBeatsPerMinute;	   // requsted tempo	
    int mClocksPerBeat;            // user clock resolution
    int mBeatsPerMeasure;          // length of a logical measure
    float mMillisPerClock;         // derived from BPM and CPB
    float mMidiMillisPerClock;     // derived from BPM and 24

    /**
     * The output devices to receive MIDI clock pulses.
     */
    class MidiOutput* mMidiOutputs[TIMER_MAX_OUTPUTS];
	int mMidiOutputCount;

    /**
     * Set to true if you want to send MIDI start/stop/clock events
	 * as you do things to the timer.  You can set the output device once,
	 * and then turn sync on and off with this flag.
     */
	bool mMidiSync;

    //
    // Application callback state
    //

	TIMER_CALLBACK  mCallback;
	void 			*mCallbackArgs;
	int 			mSignalClock;           // next callback clock

	// Improived interface for monitoring MIDI clocks
	class MidiClockListener* mMidiClockListener;

    //
    // Registers
    //

	int mRegisters[TIMER_MAX_REGISTERS];

    //
    // Transient runtime state
    //

	long	mMillis;				// number of milliseconds
    int     mClock;                 // current time in user clock ticks
    int     mBeat;                  // current beat
    int     mMeasure;               // current logical measure
    int     mSongPosition;          // current time expressed as MIDI song psn
	int     mBeatCounter;           // clocks since last beat

	float   mTick;					// milli accumulator for the user clock
	float   mMidiTick;				// milli accumulator for the midi clock
	int     mMidiClocks;			// midi clocks sent out since last start

	bool    mMidiStarted;			// true after StartSong has been sent
	bool    mSendingClocks;			// true when sending MIDI clocks
	bool    mPendingStart;			// send Start at next interrupt
	bool 	mPendingStop;			// send Stop at next interrupt
	bool 	mPendingContinue;		// send Continue at next interupt
	bool 	mPendingSongPosition;	// send SongPsn with Continue
	bool    mRestartTicks;			// signal to interrupt to zero tick basis
	float 	mPendingTempo;

    //
    // Interrupt handler stats
    // These should all remain zero if things are working properly
    //

    bool    mEnabled;               // true to disable the interrupt 
    bool    mEntered;               // true when in interrupt handler
	int     mReentries;             // number of interrupt reentries
    int     mOverflows;             // number of missed callbacks

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
