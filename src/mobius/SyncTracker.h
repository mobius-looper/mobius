/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * An object that tracks synchronization pulses comming from
 * an external sync source, compares those to the rate of advancement
 * in the audio stream, and calculates the amount of drift.
 * 
 * One of these is maintained by Synchronizer for each of the sync
 * sources: Host Beats, MIDI Clocks, and the Timer used to generate
 * output MIDI clocks.
 *
 * See SyncTracker.cpp for design details.
 *
 */
#ifndef SYNC_TRACKER_H
#define SYNC_TRACKER_H

/****************************************************************************
 *                                                                          *
 *                                  TRACKER                                 *
 *                                                                          *
 ****************************************************************************/

/**
 * An object that tracks synchronization pulses comming from
 * an external sync source, compares those to the rate of advancement
 * in the audio stream, and makes adjustments when they drift.
 */
class SyncTracker {
  public:
    
    SyncTracker(SyncSource src);
    ~SyncTracker();

    // Control

    void setMasterTrack(Track* t);
    void reset();
    void interruptStart();
    void advance(long frames, class EventPool* pool, class EventList* events);
    void event(Event* e);
    long prepare(TraceContext* tc, int pulses, long frames, bool warn);
    void lock(TraceContext* tc, int originPulses, int pulses, 
              long frames, float rate, int beatsPerBar);
    void resize(int pulses, long frames, float rate);
    void correct();

    // Status

    SyncSource getSyncSource();
    const char* getName();
    bool isLocked();
    int getPulse();
    int getLoopPulses();
    int getFutureLoopPulses();
    long getLoopFrames();
    long getFutureLoopFrames();
    float getFutureSpeed();
    long getAudioFrame();
    float getPulseFrames();
    long getDrift();
    float getAverageDrift();
    float getAveragePulseFrames();

    int getBeatsPerBar();
    long getDealign(Track* t);

    // Unit Tests

    void drift(int delta);
    int getDriftChecks();
    void incDriftChecks();
    int getDriftCorrections();
    void incDriftCorrections();
    void setDriftCorrections(int i);
    void traceDealign();

  private:

    float getPulseFrame();
    void jumpPulse(Event* e);
    void pulse(Event* e);
    long advanceInternal(long frames);
    void doResize();
    long calcDrift(long pulseFrame, long audioFrame, long loopFrames);
    long addDrift(long audioFrame, long loopFrames, long drift);
    long wrap(long frame);
    long wrap(long frame, long max);

    /**
     * Name for trace.
     */
    const char* mName;

    /**
     * Source code for matching events.
     */
    SyncSource mSource;

	/**
	 * Current pulse within the sync loop, increments each time
     * SYNC_EVENT_PULSE is received. Until the sync loop is locked this increments
     * without bound.  When the loop is locked it increments up to 
     * mLoopPulses then wraps to zero.
     * 
     * This is the closest we can get to knowing the location of the
     * external loop. It will be inaccurate due to pulse jitter.
	 *
     */
	int mPulse;

	/**
	 * Length of the sync loop in pulses.  The pulse currently will 
     * represent either MIDI clocks or host beats.  Generally it will
     * be the smallest unit available, concepts such as "bars" are handled
     * at a higher level.
     */
	int mLoopPulses;

    /**
     * The final number of audio frames in the sync loop after it is locked.
     * This will be adjusted so that mLoopFrames / mLoopPulses will 
     * always result in an integral number since pulse width needs
     * to be multiplyable without roundoff errors.
     */
    long mLoopFrames;

    /**
     * The playback rate that was in effect when the tracker was last locked
     * or resized.  This is used to determine if a later rate shift
     * should force a resize.
     */
    float mSpeed;

    /**
     * The number of pulses considered to be in one "beat".
     * This will be 24 for MIDI in and out, 1 for host.
     */
    int mPulsesPerBeat;

    /**
     * The number of beats considered to be in one "bar" at the time the
     * tracker was locked.
     */
    int mBeatsPerBar;

    /**
     * The current location within the sync loop that advances by
     * the number of frames processed during each audio interrupt.
     * The difference between this and mPulseFrame is the amount of drift.
     */
    long mAudioFrame;

    /**
     * The amount of sync drift calculated on the last pulse.
     */
    long mDrift;

    /**
     * The value of mPulse at the beginning of the last interrupt.
     * Used for an obscure edge case when locking the tracker and there
     * were more than one pulses in an interrupt.
     * UPDATE: This was part of an attempt to round the final pulse count
     * when there can be more than one pulse per interrupt.  We're now doing
     * that by rembering the origin pulse in SyncState so we don't need this,
     * delete unless you can find a use for it.
     */
    int mInterruptPulse;

    /**
     * Used only when closing the tracker and the loop length 
     * is "rounded" and may not lock exactly on a pulse boundary.
     * If we lock early, tracker state will have been calculated
     * using the ideal number of pulses, we then have to ignore 
     * this number of pulses as they come in since they have already
     * be factored into the tracker state.  See comments above
     * SyncTracker::lock for more.
     */
    int mPendingPulses;

    /**
     * Pending pulse count set by resize().
     * Since MidiInterface won't change the clock speed until the next pulse
     * we wait until we receive the next pulse before applying these changes.
     */
    int mResizePulses;

    /**
     * Pending frame count set by resize().
     * Since MidiInterface won't change the clock speed until the next pulse
     * we wait until we receive the next pulse before applying these changes.
     */
    int mResizeFrames;
    
    /**
     * Pending rate set by resize().
     */
    float mResizeSpeed;
    
    /**
     * True if we've received a STOP event.
     */
    bool mStopped;

    //
    // Unit Test Statistics
    //
    
    /**
     * A count of the number of drift checks we've done.
     */
    int mDriftChecks;

    /**
     * A count of the number of drift corrections we've done.
     */
    int mDriftCorrections;

    /**
     * For OutSync debugging, the master track.
     */
    Track* mTrack;

    /**
     * The value of mAudioFrame on the last pulse.  Used to calculate
     * true frames between pulses.  This is interesting only for debugging,
     * we don't really need it if things are working properly.
     * This will be -1 whenever mAudioFrame is set directly rather than
     * allowed to advance incrementally like after MS_START, MS_CONTINUE,
     * or a drift correction.  This is a signal that the next advance
     * should not be included in the PulseMonitor until we can reorient
     * on the new location.
     */
    long mLastPulseAudioFrame;

    // 
    // pulse analysis
    //

    /**
	 * The average number of audio frames between each pulse.
     * This is calculatd as we receive pulses while the tracker is open.
     * It is eventually used in the calculation of the final mLoopFrames
     * when the tracker is locked.
     * 
     * Note that the final mPulseFrames may differ from this value due
     * to rounding, jitter compensation, and other considerations.  But
     * normally they should be close.
     *
     * We will continue to maintain this after the tracker is locked
     * and when the difference between this and mPluseFrames becomes
     * too great we can take action, either to disable tracking since
     * the sync source is changing tempo or attempt to follow the
     * tempo changes.
     */
    class PulseMonitor *mPulseMonitor;

    /**
     * Averages the amount of drift, used for debugging.
     */
    class PulseMonitor *mDriftMonitor;

    /**
     * Flag to enable pulse trace.
     * Relatively harmless for host sync, MIDI/Out sync gets noisy.
     */
    bool mTracePulses;

};

/**
 * The number of pulse "samples" we maintain for the running average.
 * Most SYNC_OUT loops start out with 96 pulses.
 */
#define PULSE_MONITOR_SAMPLES 96

/**
 * Used internally by SyncTracker to calculate average pulse width
 * and average drift to help see better when trends are occuring.
 * The algorithm is simlar to the one used in midi/TempoMonitor
 * but it doesn't care what the pulses mean.
 */
class PulseMonitor {

  public:
 
	PulseMonitor();
	~PulseMonitor();

	void reset();
	void pulse(int width);

	float getPulseWidth();

  private:

	int mSamples[PULSE_MONITOR_SAMPLES];
	int mSample;
	int mTotal;
	int mDivisor;
	float mPulse;
};



/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
