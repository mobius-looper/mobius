/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * A class managing coordination between a MIDI output device
 * and the millisecond timer to provide a higher level "transport"
 * abstraction for generating MIDI realtime events.
 *
 * Designed for use with the Synchronizer, it could be used elsewhere
 * except it has a dependency on Event.
 *
 * See MidiTransport.cpp for design details.
 *
 */

#ifndef MIDI_TRANSPORT
#define MIDI_TRANSPORT

#include "MidiListener.h"
#include "MidiQueue.h"

/****************************************************************************
 *                                                                          *
 *                               MIDI TRANSPORT                             *
 *                                                                          *
 ****************************************************************************/

class MidiTransport : public MidiClockListener {

  public:

    MidiTransport(class MidiInterface* midi, int sampleRate);
	~MidiTransport();

    // 
    // Timer callbacks
    //

	void midiClockEvent();
	void midiStartEvent();
	void midiStopEvent();
	void midiContinueEvent();

    //
    // Transport commands
    //

    void setTempo(TraceContext* context, float tempo);
    void startClocks(TraceContext* c);
    void start(TraceContext* c);
    void stop(TraceContext* c, bool sendStop, bool stopClocks);
    void fullStop(TraceContext* c, const char* msg);
    void midiContinue(TraceContext* c);
    void incStarts();

    //
    // Status
    //

    float getTempo();
    int getRawBeat();
    int getBeat(int beatsPerBar);
    int getBar(int beatsPerBar);
    bool isSending();
    bool isStarted();
    int getStarts();
    int getSongClock();

    //
    // Audio Interrupt
    //

    void interruptStart(long millisecond);
    Event* getEvents(class EventPool* pool, long interruptFrames);

    // Diagnostics

    bool hasEvents();

  private:

    /**
     * Given to the constructor, we register ourselves as the MidiClockListener.
     * This will be sent commands to start, stop, and change tempo as the
     * trasnsport is used. 
     */
    class MidiInterface* mMidi;

    /**
     * Audio sample rate.
     */
    int mSampleRate;

    /**
     * Queue for clock events from the timer and our own transport events.
     */
	MidiQueue mQueue;

	/**
     * The tempo we sent to the MidiInterface.
     */
	float mTempo;

	/**
     * True if we're sending out MIDI clocks.
     */
	bool mSending;

    /**
     * Increments each time we send MS_START, 
     * cleared after MS_STOP.
     */
	int mStarts;

    /**
     * Set to ignore MIDI clock pulses from the internal timer until
     * it finishes processing the StartSong event.
     */
	bool mIgnoreClocks;
    
    /**
     * Set at the start of each interrupt, used for timing adjusments.  
     */
	long mInterruptMsec;

    /**
     * An old hack that has been enabled for quite awhile 
     * Used to be called !UseInternalTransport.
     *
     * When this is true we won't wait for the Timer callbacks to 
     * stuff transport events like MS_START, MS_STOP, etc. into the
     * MidiQueue, instead we'll put them in immediately when start()
     * and stop() are called, and then ignore them in the Timer callbacks.
     * I forget why this was an interesting idea, it probably made some
     * drift calculations better.  Need to experiment with the old way.
     */
    bool mImmediateTransportQueue;

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
