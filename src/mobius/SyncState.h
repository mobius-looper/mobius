/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Various state related to synchronization maintained on each Track.
 * These could just go on Track, but there are a lot of them and I
 * like making the relationship clearer.
 * 
 * The fields that define how sync will be performed are taken
 * from the Setup, the SetupTrack, and in a few cases the Preset.  
 * These are all gathered into one place so Synchronzer doesn't have
 * to hunt for them.  Once the first loop in a track is recorded,
 * the SyncState is "locked" and will no longer track changes to the 
 * sync configuration.  When all loops in the track are reset, the
 * SyncState is unlocked and we'll refresh the parameters from the Setup.
 *
 */

#ifndef SYNC_STATE_H
#define SYNC_STATE_H

// for SyncSource
#include "Setup.h"

/****************************************************************************
 *                                                                          *
 *                                 SYNC STATE                               *
 *                                                                          *
 ****************************************************************************/

/**
 * A class used to maintain synchronization state for each track.
 * Each track will have one of these.  This should only be modified
 * by Synchronizer.
 */
class SyncState {
  public:
    
    SyncState(Track* t);
    ~SyncState();

    // Sync options

    SyncSource getDefinedSyncSource();
    SyncSource getEffectiveSyncSource();
    SyncUnit getSyncUnit();
    SyncTrackUnit getSyncTrackUnit();
    bool isManualStart();

    // Lifecycle

    void lock();
    void unlock();
    void startRecording(int originPulse, int cyclePulses, int beatsPerBar, bool trackerLocked);
    void pulse();
    void addPulses(int extra);
    void scheduleStop(int trackerPulses, long trackerFrames);
    void stopRecording();

    void setBoundaryEvent(EventType* t);
    EventType* getBoundaryEvent();

    // Recording state

    bool isRecording();
    bool isRounding();
    bool wasTrackerLocked();
    int getOriginPulse();
    int getCyclePulses();
    int getRecordPulses();
    int getTrackerPulses();
    long getTrackerFrames();
    int getTrackerBeatsPerBar();

    // tests

    void setPreRealignFrame(long frame);
    long getPreRealignFrame();

  private:

    void initRecordState();

    /**
     * Track we're associated with.
     */
    class Track* mTrack;

    /**
     * True if we're locked for recording.
     */
    bool mLocked;

    //
    // Sync Options
    // 

    /**
     * The effective sync source for this track.
     */
    SyncSource mSyncSource;

    /**
     * The source units.
     */
    SyncUnit mSyncUnit;

    /**
     * The track sync unit for this tracks.
     */
    SyncTrackUnit mSyncTrackUnit;

    //
    // Recording State
    //

    /**
     * True if we've begin recording a loop in this track.
     */
    bool mRecording;

    /**
     * True if we've scheduled the end of the recording and are waiting
     * for a specific frame rather than waiting for a pulse.
     */
    bool mRounding;

    /**
     * True if the SyncTracker was locked at the time reording started.
     * This is important if you're recording more than one track at the same
     * time before the tracker is locked.  One of the tracks will finish
     * before the others and lock the tracker.  From that point forward
     * we normally start following tracker pulses rather than the external
     * pulses.  But if we've already begun following raw pulses we don't
     * want to switch over to tracker pulses mid-stream.  It might be okay
     * but the size still won't be guaranteed and I'm worried about missing
     * the final pulse since the first pulse after locking is not sent
     * by SyncTracker::advance.
     */
    bool mTrackerLocked;

    /**
     * The pulse count from the sync tracker we are following at the
     * moment the recording started.  When recording stops we use
     * this to determine whether to wrap the tracker pulse count when
     * there can be more than one pulse per interrupt.
     */
    int mOriginPulse;

    /**
     * The number of pulses received during recording that are in one cycle.
     * Used to bump the Loop's cycle count during recording.
     */
    int mCyclePulses;

    /**
     * The number of pulses received so far during recording.
     * If mRounding becomes true, then we'll stop counting pulses.
     */
    int mRecordPulses;

    /**
     * When mRounding is true we defer locking the SyncTracker until
     * the loop has finished recording.  This is the calculated number of
     * pulses that should be in the SyncTracker loop if it is not 
     * already locked.
     */
    int mTrackerPulses;

    /**
     * When mRounding is true we defer locking the SyncTracker until
     * the loop has finished recording.  This is the calculated number of
     * frame3s that should be in the SyncTracker loop if it is not 
     * already closed.
     */
    long mTrackerFrames;

    /**
     * When mRounding is true we defer locking the SyncTracker until
     * the loop has finished recording.  This is the calculated number of
     * beats in one bar that should be in the SyncTracker loop if it is not 
     * already locked.
     *
     * This one is not as crucial as mTrackerPulses and mTrackerFrames to
     * save since it is likely not going to change during the rounding
     * period.  Still, we made rounding choices based on it to let's be
     * save and save it.
     */
    int mTrackerBeatsPerBar;

    //
    // Misc State
    //

    /**
     * Set whenever one of the generated events for a sybcycle, cycle,
     * or loop boundary is seen during an interrupt.  This is used
     * by checkDrift to determine whether we've crossed a drift check point.
     */
    EventType* mBoundaryEvent;

    //
    // Unit Test Statistics
    //
    
    /**
     * The loop frame immediately before a Realign or a drift correction.
     */
    long mPreRealignFrame;

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
