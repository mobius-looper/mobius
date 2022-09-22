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
 * A sync tracker can be in two states: "unlocked" and "locked".
 * 
 * Unlocked means that the tracker is simply counting pulses and does know
 * the length of the external sync loop.  Sync trackers will remain
 * unlocked until a Mobius track is recorded using that tracker.  
 *
 * Locked means that the size of the sync loop has been established
 * and the tracker is now able to compare the rate of advancemenet of the
 * sync pulses with the rate of advancement of the audio stream.  A
 * tracker is locked the moment a Mobius track is recorded using that trakcer.
 * Once locked, most trackers will continue using the size of the original
 * sync loop even if the Mobius loop changes size or is reset.  This
 * allows the Mobius loops to go wildly out of alignment, while still
 * tracking sync drift (see manual for a discussion on the difference
 * between "drift" and "dealign").
 *
 * A tracker will move from the locked to unlocked state when all Mobius
 * tracks that use this tracker are reset.  
 *
 * When a tracker is locked, it will begin generating it's own sync
 * events for beats and bars that exactly match the advance in the audio stream.  
 * These are the events that the Mobius tracks will follow and they may
 * not exactly match the real events being received.  This is how we can
 * keep multiple tracks in perfect sync, following a locked tracker is
 * similar to following a Track Sync master, it is immune to jitter in the
 * external pulses.  When we detect drift, all tracks following this
 * tracker are adjusted an identical amount.
 *
 * It is important that the performer think about the size of the first
 * loop recorded with this tracker because this will determine the
 * sync loop size for all tracks for the duration of the performance.
 * If it is necessary to change the size of the sync loop after it has been
 * locked, you muse use the SetSyncLoop function or set the system variables
 * hostSyncLoopPulses, midiSyncLoopPulses, and outSyncLoopPulses.
 *
 * The tracker for output MIDI clocks is unlike the others because it
 * can change size to adapt to changes in the Mobius loop size when
 * SyncAdjust=SYNC_ADJUST_TEMPO.  The tempo of the generated clocks
 * will raise or lower which may cause the number of pulses to change.
 * The length of the tracked audio loop will also change to correspond
 * to the new pulse width.  
 * 
 * There are two places where drift can be checked, defined by the
 * DriftCheckPoint global parameter.  DRIFT_CHECK_LOOP will make the
 * drift check when the Mobius loop crosses the start point within an 
 * audio interrupt.  DRIFT_CHECK_EXTERNAL will make the check when the
 * tracker pulse counter wraps back to zero which is called the
 * "external start point".  
 *
 * Note that DRIFT_CHECK_LOOP only makes sense if the number of pulses
 * in the tracker is large.  Because pulses are relatively coarse, checking
 * drift when at a specific Mobius loop location can show an error of one
 * full pulse width.  For example a loop with 100,000 frames and 4 pulses.
 * When the Mobius loop is at the start point, the ideal pulse will be 0.
 * But if the pulse is just slightly late the corresponding pulse frame
 * would be 75,000 resulting in a drift of 25,000 which looks very bad except
 * that a millisecond later when the pulse comes in the pulse frame jumps
 * to 100,000.
 *
 * This extreme example is possible when using Host sync, since 
 * beat pulses from the host can be relatively large.  It is less of a problem
 * for MIDI in/out sync since pulses are MIDI clocks which occur 24 times
 * per beat.  For a modest tempo of 60 bpm, that would be 24 clocks per
 * second.  At the usual sample rate of 44100 samples per second, the
 * pulse width would be 1837.5 which is the margin of error at any
 * given internal loop location.  The recommend drift correction threshold is
 * 4096 so this is within tolerance, still the effective tolerance is
 * almost half that due to the margin of error.
 *
 * This margin of error can also effect drift correction.  If drift
 * correction is not done exactly on a sync pulse, the true location
 * of the external loop will be somewhere between two pulses.  But since
 * we can't know that, we would have to round up or down.  In the first 
 * extreme case, if we noticed a drift of 25,000 at the loop start point
 * and corrected that by jumping to frame 75,000, on the next pulse we
 * would be lagging the external loop by up to 25,000 frames.  
 *
 * What all this means is that drift can only be calculated when pulses
 * are received.  Drift is then effectively locked until the next pulse.
 * We do not allow drift correction between pulses, but if we did
 * we would use the drift from the last pulse.  Usually this is still
 * close to the actual drift, but if the clock is changing tempo we
 * may lag a bit.  But since we don't attempt to follow external tempo 
 * changes this is not a problem.
 *
 * PULSE WIDTH JITTER
 *
 * Trackers will generate pulse events on beat/bar boundaries but not clocks
 * (for MIDI, OUT).  We try to size the initial loop so that the beat
 * width is an integer but that isn't always possible.  If the beat width
 * is not an iteer this may results in jitter in the beat sizes.  The relative
 * importance of this error depends on the loop length and the number of beats.
 * 
 * For an extreme example assume the loop length is 10 and the beat count
 * is 6.  The beat width is 1.66r
 *
 * The truncated frames for each beat origin are: 0, 1, 3, 5, 6, 8, 10
 * Which makes the beat widths 1, 2, 2, 1, 2, 2
 *
 * In real-world loops with thousands of frames the magnitude of the error 
 * becomes less but the jitter can still cause errors.  For a loop of 10000
 * frames and 6 pulses the beat origins are: 0, 1666, 3333, 5000, 6666,
 * 8333, 10000.
 *
 * The beat widths are 1666, 1667, 1667, 1666, 1667, 1667.
 *
 * The relative error is small but depending on where you start and end you
 * can end up with slave tracks that are not the same size and will drift 
 * apart over time.  We have the same problem with Track Sync on subcycles.
 *
 * When generating tracker events however we don't maintain a float pulse frame
 * counter that increments each advance.  We could, but it's extra work and
 * it isn't necessary.  Instead for each audio interrupt we start from
 * mAudioFrame and try to determine which pulse we're "in".  We do this by
 * dividing the frame number by the pulse width.  This results in a different
 * rounding error.  When accumulating from the beginning the second pulse
 * would begin on frame 1666, the third on 3333, etc.  But when working
 * back from the frame to the pulse 1666 divided by 1.66r is 0.996 which
 * rounds down to zero instead of 1.  It won't be until frame 1667 that
 * we think we've entered the second pulse.  So when calculating pulses
 * from frames the effective pulse origins are:
 *
 *     0, 1667, 3334, 5000, 6667, 8334, 10000
 *
 * with effective beat widths of:
 *
 *     1667, 1667, 1666, 1667, 1667, 1666
 *
 * The pattern is the same as before, just shifted one the left.
 *
 * Note though that is is not guaranteed that the final frame will 
 * divide cleanly into the final pulse.  With this example 10000 divided
 * by 1.66r (on my PC calculator anyway) was a nice round 6 but 
 * I'm not convinced that for some combinations on some architectures the
 * result would be lose but less, for example 5.9999999999.
 * 
 * This isn't so bad since we'll detect the pulse one frame late, but I like
 * to have the start frame be always accurate so slave tracks can follow
 * it and always get the same result.  We can remove this error by adding
 * some noise like .1 before we divide.  But this also feels dirty.  I can't 
 * convince myself that this won't result in missed beats, though the 
 * width between the beats would have to be very, very small.
 *
 * I'm going to leave the interior beats with the second kind of roundoff
 * error and add  a check to ensure that when we cross the loop start
 * point that we generate a start point beat even if the accumulated pulse
 * frame is not quite there.
 *
 * Recording while speed adjusted can throw another wrench into this.
 * I can't even think about that now..
 * 
 * WHY I HATE FLOATING POINT (more pulse detection)
 *
 * We have another complication when pulses fall on the first frame
 * of an audio buffer.
 * 
 * Assume loop length is 88200, 96 MIDI clock pulses, 24 clocks per beat, 
 * 22050 frames per beat 918.75 pulse frames.  Assume buffer stream 
 * where buffer starts at frame 21922 and is 128 frames long.
 * Note that we don't actually reach the beat boundary in this buffer, 
 * we get to frame 21049.  The final frame in the buffer is the last
 * frame in beat zero.  The first frame in the next buffer will be 22050, 
 * the start of beat one.
 *
 * At the beginning of buffer 21922 we have to determine which pulse we're
 * in so we can calculate the beginning of the next pulse.  Most of the time
 * you can do this by dividing the origin frame 21922 by pulse frames.
 * 21922 / 918.75 is 23.86 so we are near the end of beat 23.
 * To find the start of the next pulse we add 1 to get 24 and multiply by
 * pulseFrames 918.75 * 24 = 22050.  The buffer contains frames 21922 
 * through 21049 so the next beat is not within this buffer.
 *
 * Now we get to buffer 22050, dividing by 918.75 we get 24.  The problem here
 * is that we are exactly at the start of the beat, normally we would now
 * see if beat 24+1 starts in this buffer.  We can't tell the difference
 * between "beat started in a previous buffer" and "beat started at the
 * beginning of this buffer" with just with a division.  
 *
 * Now you're thinking: use modulo.  If the origin frame 22050 modulo
 * 918.75 is zero, then we knwow we're exactly at the start of the beat
 * and don't have to look for the beginning of the next one.  The problem is 
 * you can't trust modulo.  If the pulse or beat width is not an integer,
 * beats in the float world may not fall exactly on a frame, they may be
 * "between" frames.  
 *
 * For example, assume a pulse width of 919.20, something you commonly see
 * when trying to sync to a jittery MIDI clock at 120 BPM. Ideal pulse width
 * would be 918.75 but you don't always get there.  With that pulse width, 
 * pulse 56 occurs on frame 51475.5.  If you got a buffer beginning with
 * frame 51475 the modulo result would be 55.99782.. a buffer beginning
 * with frame 512476 the result would be 56.00870, neither of which are
 * zero.  So a single modulo can't be used for pulse detection.
 *
 * I tried two approaches, both using a simple divide with truncation
 * to determine the current pulse.  In the first approach, at the
 * beginning of the buffer, make two calculations.  One the pulse number
 * of the frame at the start of the buffer and another for the pulse
 * number of the frame immediately preceeding the start of the buffer.
 * If these two numbers differ you know you've crossed a pulse boundary
 * on this buffer.
 *
 * For 80,000 frames 96 pulses has a pulse frame of 833.33r.  Beats wil be at 
 * frame offsets 0, 20000, 60000, 80000.  We're in buffer 19872 which
 * is the last buffer in beat zero (pulse 23) the next one will be 20000
 * the start of pulse 24 beat 1.  19872 / 833.333 = 23.8 or pulse 23 which
 * is correct.  When we get to buffer 20000 make two calculations:
 *
 *     (20000 - 1) / 833.333r = 23.998
 *     20000 / 833.333r = 24
 *
 * Since the previous frame beat was one less than the current frame beat
 * we know we're at the start of a new beat, not in the middle of beat 24.  
 * If there is float rounding error it will be either too high or low.
 * If too high, say 24.0000000000000001 it truncates to 24 and we can still see 
 * the beat changed.  If the result was too low say 23.999999999999999 
 * we'll use 23 as a basis but detect the beat crossing within this
 * buffer.  We can be one off in either direction but we still
 * detect the beat which is the most important thing.
 * 
 * LOOP ROUNDING
 * 
 * Ideally, we should round the tracker loop length so that the smallest
 * granule we want to let slaves follow (clock, beat, bar) is an integer
 * number of frames.  This would ensure that tracks that slave to this
 * tracker would stay in sync, but the adjustment may put the tracker
 * out of ideal sync with the external clock.  This would cause everything
 * to drift away but at least they can all be drift corrected and stay
 * in perfect sync relative to each other.
 *
 * For the 10000 loop of 6 pulses, the adjusted size would be 9996.
 * The ideal length will be calculated by SyncTracker::prepare but
 * Synchronizer is not required to obey that.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <math.h>

#include "Trace.h"

#include "Event.h"
#include "Loop.h"
#include "Setup.h"
#include "Track.h"

#include "SyncTracker.h"

/**
 * An experiment to process resizes immediately rather than waiting
 * for the next MIDI clock.  This doesn't appear to help drift and dealign,
 * it seems to make it worse.  I can't explain it, need to explore this.
 * This is defined in midi/MidiTimer.cpp and is currently true.
 */
extern bool MidiTimerDeferredTempoChange;

/****************************************************************************
 *                                                                          *
 *                                SYNC TRACKER                              *
 *                                                                          *
 ****************************************************************************/

PUBLIC SyncTracker::SyncTracker(SyncSource src)
{
    mSource = src;
    mTrack = NULL;

    switch (mSource) {
        case SYNC_OUT: mName = "Out"; break;
        case SYNC_HOST: mName = "Host"; break;
        case SYNC_MIDI: mName = "Midi"; break;
        default: mName = "???"; break;
    }

    mPulseMonitor = new PulseMonitor();
    mDriftMonitor = new PulseMonitor();

    // KLUDGE: For event generation we need to know how many pulses are
    // in one beat.  This will be 24 for MIDI, 1 for host.
    // Could make Synchronizer pass this in but we can know.    
    // Ugh, the logic has to go somewhere, consider making subclasses
    // of SyncTracker for each source so we can encapsulate this better...
    if (mSource == SYNC_OUT || mSource == SYNC_MIDI)
      mPulsesPerBeat = 24;
    else
      mPulsesPerBeat = 1;

    // leave this on all the time to get beat/bar pulses
    // clock pulses will be ore selective
    mTracePulses = true;

    reset();
}

PUBLIC SyncTracker::~SyncTracker()
{
}

PUBLIC SyncSource SyncTracker::getSyncSource()
{
    return mSource;
}

PUBLIC const char* SyncTracker::getName()
{
    return mName;
}

PUBLIC bool SyncTracker::isLocked()
{
    return (mLoopFrames > 0);
}

PUBLIC int SyncTracker::getPulse()
{
    return mPulse;
}

PUBLIC int SyncTracker::getLoopPulses()
{
    return mLoopPulses;
}

PUBLIC int SyncTracker::getFutureLoopPulses()
{
    return (mResizePulses != 0) ? mResizePulses : mLoopPulses;
}

PUBLIC long SyncTracker::getLoopFrames()
{
    return mLoopFrames;
}

PUBLIC long SyncTracker::getFutureLoopFrames()
{
    return (mResizeFrames != 0) ? mResizeFrames : mLoopFrames;
}

PUBLIC float SyncTracker::getFutureSpeed()
{
    // we let these be zero to mean "uninitailized" but caller expects 1.0
    float speed = (mResizeSpeed != 0) ? mResizeSpeed : mSpeed;
    if (speed == 0.0)
      speed = 1.0;
    return speed;
}

PUBLIC long SyncTracker::getAudioFrame()
{
    return mAudioFrame;
}

PUBLIC long SyncTracker::getDrift()
{
    return mDrift;
}

/**
 * !! decide whether this really needs to be a float and be consistent
 */
PUBLIC float SyncTracker::getAverageDrift()
{
    return mDriftMonitor->getPulseWidth();
}

/**
 * Added this for debugging, but if tempo is changing this can
 * take a LONG time to catch up, so it is highly inaccurate if the
 * user is fidding with tempos before recording.
 */
PUBLIC float SyncTracker::getAveragePulseFrames()
{
    return mPulseMonitor->getPulseWidth();
}

PUBLIC int SyncTracker::getBeatsPerBar()
{
    return mBeatsPerBar;
}

/**
 * Calculate the frames in one pulse.
 * Public so Synchronizer can use it for scheduling.
 * mLoopFrames must be adjusted so that this division will always
 * return an integral value.
 */
PUBLIC float SyncTracker::getPulseFrames()
{
    float frames = 0.0f;
    if (mLoopPulses > 0)
      frames = (float)mLoopFrames / (float)mLoopPulses;
    return frames;
}

/****************************************************************************
 *                                                                          *
 *                                  CONTROL                                 *
 *                                                                          *
 ****************************************************************************/

/**
 * Reset the state of the sync tracker.
 * The tracker will now be considerd "unlocked" and will simply count pulses
 * and calculate the average pulse width.
 *
 * Do *not* call this until you are sure no other tracks are following
 * or about to be following this tracker.
 */
PUBLIC void SyncTracker::reset()
{
    mPulse = 0;
    mLoopPulses = 0;
    mLoopFrames = 0;
    mSpeed = 0.0f;
    mBeatsPerBar = 0;
    mAudioFrame = 0;
    mDrift = 0;
    mInterruptPulse = 0;
    mPendingPulses = 0;
    mResizePulses = 0;
    mResizeFrames = 0;
    mResizeSpeed = 0.0f;
    mDriftChecks = 0;
    mDriftCorrections = 0;
    mLastPulseAudioFrame = -1;

    // Start this out true so we don't do an initial 
    // pulse increment.  
    mStopped = true;
  
    mPulseMonitor->reset();
    mDriftMonitor->reset();
}

/**
 * Called at the start of each interrupt.
 * Capture the pulse count at the beginning so we can make
 * adjustments to the final pulse number when locking
 * the tracker after a new recording.
 */
PUBLIC void SyncTracker::interruptStart()
{
    mInterruptPulse = mPulse;
}

/**
 * Temporary debugging hack to set a track for trace.
 */
PUBLIC void SyncTracker::setMasterTrack(Track* t)
{
    mTrack = t;
}

/**
 * Advance the tracker state by some number of audio frames.
 * This will usually be the length of an audio interrupt block.
 * For SYNC_HOST we'll also call this to do a partial advance after
 * locking the tracker in the middle of a buffer.
 * This must be called AFTER processing pulses that might change
 * the length of the loop (pending resize).
 *
 * This may return a list of Event objects if the advance crossed
 * one or more (almost always only one) pulse boundaries.  These events
 * will then be presented to the Mobius Loops that are slaving to the tracker.
 * See file header comments for a *long* discussion of the pulse boundary
 * calculations here.
 */
PUBLIC void SyncTracker::advance(long frames, EventPool* pool, EventList* events)
{
    // originally did this only if the tracker was locked but that
    // makes it harder to measure the average pulse width before locking
    //if (mLoopFrames > 0) {

    long startFrame = mAudioFrame;

    // NOTE: This was commended out in some uncommitted from May 2013, 
    // I don't remember why and I think it needs to be here!!
    mAudioFrame = advanceInternal(frames);

    if (mLoopFrames > 0) {
        float pulseFrames = getPulseFrames();

        // always guard against divide by zero
        if (pulseFrames == 0.0) {
            // really shouldn't see this, mLoopFrames must be way too short
            Trace(1, "SyncTracker %s: pulseFrames zero!\n", mName);
        }
        else {
            // This is harder than it needs to be in practice, but
            // in theory the buffer can contain several pulses, and
            // the start point may occur on any one of them.
            // This would be much easier if we just looked for beats
            // instead of pulses, there can never be more than one in
            // an interrupt and prepare() will have ensured that it was
            // integral.  But there's no glory in that, we might have to
            // return clock pulses someday.

            int prevPulse;
            int originPulse;
            int nextPulse;
            float nextPulseFrame;
            if (startFrame == 0) {
                // wrapped exactly to zero, no need to go back
                prevPulse = 0;
                originPulse = 0;
                nextPulse = 0;
                nextPulseFrame = 0.0f;
            }
            else {
                prevPulse = (int)((float)(startFrame - 1) / pulseFrames);
                originPulse = (int)((float)startFrame / pulseFrames);
                if (prevPulse != originPulse) {
                    // crossing at a buffer boundary
                    nextPulse = originPulse;
                    nextPulseFrame = (float)startFrame;

                }
                else {
                    nextPulse = prevPulse + 1;
                    // the frame (without wrapping) of the next pulse
                    nextPulseFrame = (float)nextPulse * pulseFrames;
                }
            }

            // adjustment for final pulse to make sure we don't miss it
            // due to float roundoff
            if (nextPulse == mLoopPulses) {
                if ((long)nextPulseFrame != mLoopFrames) {
                    // not an error but I want to know when this happens
                    Trace(1, "SyncTracker %s: Correcting final pulse width\n", 
                          mName);
                    nextPulseFrame = (float)mLoopFrames;
                }
            }
            // and where it would be in this buffer
            long nextPulseOffset = (long)(nextPulseFrame - (float)startFrame);

            if (nextPulseOffset < 0) {
                // can't happen unless pulseFrames is wrong
                Trace(1, "SyncTracker %s: Bad pulse offset!\n", mName);
            }
            else if (events == NULL) {
                // Not expecting events, this is normal for SYNC_OUT
                // but for others we do this immediately after locking and
                // are not expecting events.  Well...actually this will
                // detect the start point pulse which might be interesting
                // to inject back into the event list but Synchronizer
                // doesn't have a way to do that yet.
                if (mSource != SYNC_OUT && 
                    nextPulseOffset < frames && 
                    // allow the start pulse
                    nextPulse != 0)
                  Trace(1, "SyncTracker %s: Partial advance ignored sync events!\n", mName);
            }
            else {
                int ecount = 0;

                while (nextPulseOffset < frames) {

                    // let's assume that trackers don't need to generate
                    // clock events, only beats and bars
                    if ((nextPulse % mPulsesPerBeat) == 0) {

                        Event* e = pool->newEvent();
                        e->type = SyncEvent;
                        e->fields.sync.source = mSource;
                        e->fields.sync.syncTrackerEvent = true;
                        e->fields.sync.eventType = SYNC_EVENT_PULSE;
                        e->frame = nextPulseOffset;

                        // this is needed for Realign, it must be wrapped
                        e->fields.sync.pulseFrame = wrap((long)nextPulseFrame);

                        int beat = nextPulse / mPulsesPerBeat;
                        int remainder = beat % mBeatsPerBar;
                        if (remainder == 0)
                          e->fields.sync.pulseType = SYNC_PULSE_BAR;
                        else
                          e->fields.sync.pulseType = SYNC_PULSE_BEAT;

                        // nextPulse isn't wrapped to zero
                        if ((nextPulse % mLoopPulses) == 0)
                          e->fields.sync.syncStartPoint = true;

                        // actually we should be able to use zeroness
                        // of the pulseFrame to detect start point now
                        // that we always snap it to the edge
                        if (e->fields.sync.pulseFrame == 0 && 
                            !e->fields.sync.syncStartPoint)
                          Trace(1, "SyncTracker %s: Inconsistent start point detection\n", mName);

                        // sigh, not enough string args
                        const char* format;
                        if (e->fields.sync.syncStartPoint)
                          format = "SyncTracker %s: %s Start offset %ld drift %ld\n";
                        else
                          format = "SyncTracker %s: %s offset %ld drift %ld\n";

                        Trace(2, format, mName,
                              GetSyncPulseTypeName(e->fields.sync.pulseType),
                              e->frame, (long)mDrift);

                        events->insert(e);
                    }

                    // use the same calculation as above for consistency
                    nextPulse++;
                    nextPulseFrame += pulseFrames;
                    if (nextPulse == mLoopPulses) {
                        if ((long)nextPulseFrame != mLoopFrames) {
                            Trace(1, "SyncTracker %s: Correcting final pulse width\n", 
                                  mName);
                            nextPulseFrame = (float)mLoopFrames;
                        }
                    }
                    nextPulseOffset = (long)(nextPulseFrame - (float)startFrame);
                    ecount++;
                }
            }
        }
    }
}

/**
 * Advance the audio frame with wrapping.
 * Factored out of advance() because it used to calculate
 * partial advances for events occuring in the middle of the interrupt
 * block.  This only happens for host pulses.
 */
PRIVATE long SyncTracker::advanceInternal(long frames)
{
    long advanced = frames;

    if (frames < 0) {
        // can never happen since we're dealing with interrupt blocks
        Trace(1, "SyncTracker %s: advance negative frames!\n", mName);
    }
    else {
        advanced = mAudioFrame + frames;

        // mLoopFrames won't be set if we're unlocked, just let it freewheel
        if (mLoopFrames > 0 && advanced >= mLoopFrames) {
            advanced -= mLoopFrames;
            if (advanced >= mLoopFrames) {
                // this shouldn't happen
                Trace(1, "SyncTracker %s: advance severe wrap!\n", mName);
                advanced = advanced % mLoopFrames;
            }
        }
    }

    return advanced;
}

/**
 * Process a SyncEvent at the start of an audio interrupt.
 * These are the actual events received from the timer, host, or MIDI
 * they are not the derived events we generate in advance() above.
 *
 * Originally START and STOP events were not considered pulses
 * but this is important for host sync in Ableton when there
 * is a loop in the timeline.  Ableton will rewind the transport
 * causing us to raise a START (if rewind to zero) or CONTINUE
 * (if rewind anywhere else).  These have to be treated as an initial
 * pulse.
 */
PUBLIC void SyncTracker::event(Event* e)
{
    SyncEventType type = e->fields.sync.eventType;

    // toggle between two algorithms for testing
    bool startContinueSimple = true;

    switch (type) {

        case SYNC_EVENT_PULSE: {
            pulse(e);
        }
        break;

        case SYNC_EVENT_STOP: {
            // nothing really to do, we could suppress advancing
            // mAudioFrame since we know we're going to drift wildly but
            // as long as Synchronzier knows this and doesn't use drift
            // we can just let it go...still would be cleaner to lock 
            // them both?
            Trace(2, "SyncTracker %s: SYNC_EVENT_STOP\n", mName);
            mStopped = true;
        }
        break;

        case SYNC_EVENT_START: {
            Trace(2, "SyncTracker %s: SYNC_EVENT_START\n", mName);
            if (mTrack != NULL)
              Trace(2, "SyncTracker %s: Restarting at Loop frame %ld\n",
                    mName, mTrack->getLoop()->getFrame());

            // NOTE: For OUT sync we have a pretty accurate amount
            // of dealign because of the partial advance between the time
            // lock() was called till the next interrupt when we'll see
            // the START event.  Should just leave mAudioFrame alone, it
            // would be more accurate?
            if (mAudioFrame > 0)
              Trace(2, "SyncTracker %s: Restarting with initial advance %ld\n",
                    mName, mAudioFrame);

            if (!mStopped && startContinueSimple) {
                // igore the type and just treat it like a pulse
                pulse(e);
            }
            else {
                // If we receive one it means the sequencer has restarted.
                // If we send one it means we've asked the sequencer to restart.
                // In both cases the pulse counter resets.
                mPulse = 0;
                jumpPulse(e);
            }
        }
        break;

        case SYNC_EVENT_CONTINUE: {
            
            // Similar to START, but we don't necessarily start at zero.
            long newPulse = e->fields.sync.continuePulse;
            // the sequencer track may be longer than ours, so wrap
            if (mLoopPulses > 0 && newPulse >= mLoopPulses)
              newPulse %= mLoopPulses;

            if (!mStopped && startContinueSimple) {
                Trace(2, "SyncTracker %s: SYNC_EVENT_CONTINUE, ignoring pulse %ld\n", 
                      mName, (long)newPulse);
                pulse(e);
            }
            else {
                Trace(2, "SyncTracker %s: SYNC_EVENT_CONTINUE pulse %ld\n", 
                      mName, (long)newPulse);

                if (mTrack != NULL)
                  Trace(2, "SyncTracker %s: Continuing at Loop frame %ld\n",
                        mName, mTrack->getLoop()->getFrame());

                mPulse = newPulse;
                jumpPulse(e);
            }
        }
        break;
    }
}

/**
 * Calculate the logical pulse frame.
 */
PRIVATE float SyncTracker::getPulseFrame()
{
    return (float)mPulse * getPulseFrames();
}

/**
 * Adjust state in response to a START or CONTINUE event.
 * mPulse has already been set.
 * 
 * We treat these like pulses for synchronzing recording and
 * we need to detect when the host is playing a loop which 
 * will cause it to rewind periodically.
 *
 * This needs to accomplish some of the same things as
 * pulse(Event*) but we don't increment the pulse counter
 * and we don't bother adding this to the pulse width
 * and drift averages.
 *
 * Formerly we would cancel drift here, old notes:
 *
 *     The Mobius Loop frame will usually be dealigned slightly
 *     from mAudioFrame.  With MIDI input latency and
 *     MidiQueue quantizing events to the next buffer, it is very 
 *     difficult to keep mAudioFrame and the Loop frame aligned, but
 *     it doesn't really matter.  All we need to do here is detect
 *     changes in pulse width, the alignment of the loop is as close
 *     as we can get it without a lot more work for practicaly
 *     no audible gain.
 *
 * Now that we support start and continue events as pulses we have
 * to retain drift since we're logically continuing to play seamlessly,
 * the host transport is just jumping back.  We could try to be smart
 * and cancel drift only if we're sure we jumped back to an expected
 * pulse but it doesn't really matter.  If they're jacking with the
 * transport randomly drift and dealign lose meaning.
 */
PRIVATE void SyncTracker::jumpPulse(Event* e)
{
    // we really shouldn't have this, right?
    if (mPendingPulses > 0) {
        mPendingPulses--;
        Trace(1, "SyncTracker %s: Pending pulses after a START/CONTINUE event!\n",
              mName);
        mLastPulseAudioFrame = -1;
    }
    else {
        mAudioFrame = (long)getPulseFrame();

        e->fields.sync.pulseNumber = mPulse;
        // remember this for Realign when OutRealign=Restart
        // UPDATE: Now that Realign follows the SyncTrakcer pulses we
        // won't actually use this
        e->fields.sync.pulseFrame = mAudioFrame;

        // on start/continue we will never reach the end pulse
        // only need to watch a rewind to pulse zero
        if (mPulse == 0)
          e->fields.sync.syncStartPoint = true;

        if (mStopped) {
            // we don't know where we are so reset drift
            mDrift = 0;
            mLastPulseAudioFrame = -1;
            mPulseMonitor->reset();
            mDriftMonitor->reset();
        }
        else {
            // retain drift, but don't measure this pulse
            // just in case to avoid poluting the average
            mLastPulseAudioFrame = -1;
            mAudioFrame = addDrift(mAudioFrame, mLoopFrames, mDrift);
        }
    }

    // start/continue always reset this
    mStopped = false;
}

/**
 * Process a pulse SyncEvent at the start of an audio interrupt.
 * If the pulse advances us past the external start point, set a flag
 * in the event.
 *
 * NOTE: Some of what this does was designed before we started
 * following the events generated by SyncTracker::advance rather than
 * directly following the external events.  For example setting
 * SyncStartPoint and PulseFrmae in the Event.  Try to weed these 
 * someday so we don't confuse things.
 */
PRIVATE void SyncTracker::pulse(Event* e)
{
    // If we have pending pulses, ignore them since they were logically
    // included when the tracker was locked.  There should normally be
    // only one of these.
    if (mPendingPulses > 0) {
        mPendingPulses--;
        if (mTracePulses)
          Trace(2, "SyncTracker %s: Ignoring pending pulse\n",
                mName);
        // don't start monitoring pulse width until we're pass this
        mLastPulseAudioFrame = -1;
        return;        
    }

    mPulse++;

    // Remember the pulse number, this is only necessary for an obscure
    // edge condition when recording new loops with an unlocked tracker.
    // It is important that we know the origin pulse of the recording 
    // so we can properly wrap the final pulse count in lock() when there
    // can be more than one pulse per interrupt.
    e->fields.sync.pulseNumber = mPulse;

    // Pulses can be offset into the buffer, but we don't advance
    // mAudioFrame until after processing all the pulses.  So calculate the
    // effective frame of this pulse.  In practice this is only relevant
    // for Host pulses since they are not quantized to the start of the
    // next buffer.
    long effectiveAudioFrame = advanceInternal(e->frame);

    // Remember the advance since the last pulse and monitor average
    // pulse width.  If the last frame is -1 it means we moved mAudioFrame
    // and should skip the next pulse width monitor
    long advance = 0;
    if (mLastPulseAudioFrame >=  0) {
        if (effectiveAudioFrame > mLastPulseAudioFrame)
          advance = effectiveAudioFrame - mLastPulseAudioFrame;
        else {
            // wrapped on the last advance
            advance = (mLoopFrames - mLastPulseAudioFrame) + effectiveAudioFrame;
        }
        mPulseMonitor->pulse(advance);
    }

    // Apply a pending resize, have to do this before we compare
    // against mLoopPulses for wrapping
    doResize();

    // resize can change mAudioFrame to recalculate
    // actually this doesn't matter, resize only happens for OutSync and pulse
    // offets only happen for HostSync
    effectiveAudioFrame = advanceInternal(e->frame);

    if (mLoopFrames > 0) {
        // loop has been locked

        if (mPulse == mLoopPulses) {
            mPulse = 0;
            e->fields.sync.syncStartPoint = true;
        }

        // logical pulse frame
        float pulseFrame = getPulseFrame();

        mDrift = calcDrift((long)pulseFrame, effectiveAudioFrame, mLoopFrames);

        // remember this for Realign when OutRealign=Restart
        // UPDATE: Now that Realign follows the SyncTrakcer pulses we
        // won't actually use this
        e->fields.sync.pulseFrame = (long)pulseFrame;

        // calculate average drift
        mDriftMonitor->pulse((int)mDrift);
    }

    // Don't trace once the tracker is locked and we no longer
    // follow raw pulses.  We'll trace in advance()
    if (mTracePulses && !isLocked()) {
        SyncPulseType type = e->fields.sync.pulseType;
        const char* traceType = GetSyncPulseTypeName(type);

        if (mSource == SYNC_HOST) {
            Trace(2, "SyncTracker %s: %s pulse %ld advance %ld drift %ld\n",
                  mName, traceType, (long)mPulse, (long)advance, (long)mDrift);
        }
        else {
            // for the two MIDI sources, the advance is the last clock
            // but it is more interesting to show the average and only
            // trace beats and bars
            bool traceClocks = false;
            if (traceClocks || type != SYNC_PULSE_CLOCK)
              Trace(2, "SyncTracker %s: %s pulse %ld average advance %ld drift %ld\n",
                    mName, traceType, (long)mPulse, 
                    (long)mPulseMonitor->getPulseWidth(),
                    (long)mDrift);
        }
    }

    // do this every time?  if the Mobius Loop is multi-cycle should
    // only do this in the first cycle
    if (mPulse == 0)
      traceDealign();

    mLastPulseAudioFrame = effectiveAudioFrame;

}

/**
 * Calculate the alignment between the virtual sync loop and a real
 * Mobius loop.  The loop is expected to be in a track that follows
 * this sync tracker.
 * 
 * This is intended for use with unit tests where the length of the audio
 * loop is the same as the sync loop.  Once the audio loop changes size it
 * will go wildly out of alignment.
 *
 * Note that speed shift is applied to the sync loop as a resize, for
 * example starting with a 44100 frame loop then entering 1/2 speed resizes
 * the sync loop to 88200 frames.
 *
 * The audio loop however is not resized, the shift is applied dynamicallyu
 * as the stream advances.  To get a proper dealignment we therefore
 * need to apply the last known speed adjustment.
 */
PUBLIC long SyncTracker::getDealign(Track* t)
{
    long dealign = 0;

    if (t != NULL) {
        float pulseFrame = getPulseFrame();
        long loopFrame = t->getLoop()->getFrame();
        if (mSpeed != 0.0)
          loopFrame = (long)((float)loopFrame / mSpeed);

        dealign = calcDrift((long)pulseFrame, loopFrame, mLoopFrames);
    }
    return dealign;
}

/**
 * Trace alignment between the virtual sync loop and the real Mobius loop
 * if we have a track follower.
 * Public so we can call it from Synchronizer::checkDrift when mTracePulses
 * is off.
 *
 * For OUT sync this is ordinarlly the same as the initial advance
 * we traced in the START event handler above.
 */
PUBLIC void SyncTracker::traceDealign()
{
    if (mTrack != NULL) {
        long dealign = getDealign(mTrack);
        long loopFrame = mTrack->getLoop()->getFrame();
        Trace(2, "SyncTracker %s: pulse %ld loopFrame %ld dealign %ld\n",
              mName, (long)mPulse, loopFrame, dealign);
    }
}


/****************************************************************************
 *                                                                          *
 *                                  LOCKING                                 *
 *                                                                          *
 ****************************************************************************/

/**
 * Calculate the ideal frame length for this loop given the
 * number of pulses.  Warn if the passed frame length is not ideal.
 * Synchronizer should call this before locking if possible.
 *
 * It is best if Synchronizer round the loop length so the loop
 * is an exact multiple of the beat width and that the beat width 
 * is integral, without that there will be roundoff errors
 * for any followers that are aligning to beat boundaries
 *
 * Actually, it should be unnecessary to call this if Synchronizer
 * calculates an integral beat size when scheduling the record
 * end events.  So we use this as a warning at the time of locking.
 */
PUBLIC long SyncTracker::prepare(TraceContext* tc, int pulses, long frames,
                                 bool warn)
{
    long ideal = frames;
    float pulseFrames = (float)frames / (float)pulses;
    float beatFrames = pulseFrames * (float)mPulsesPerBeat;
    float intpart;
    float frac = modff(beatFrames, &intpart);

    if (warn && (frac != 0))
      Trace(tc, 2, "SyncTracker %s: WARNING: Fractional beat width %ld (x100)\n",
            mName, (long)(beatFrames * 100));

    long ibeatFrames = (long)intpart;

    int remainder = frames % ibeatFrames;
    if (remainder != 0) {
        int beatsInLoop = frames / ibeatFrames;
        if (beatsInLoop == 0) {
            // not even a full beat, must have been recording on clock
            // boundaries, something is wrong...
            Trace(tc, 1, "SyncTracker %s: %ld frames with %ld pulses is not a full beat!\n",
                  mName, frames, (long)pulses);
        }
        else {
            long proposed = beatsInLoop * ibeatFrames;

            long delta = frames - proposed;
            // don't think this can happen, we always round down
            if (delta < 0)
              delta = -delta;

            // and here out of my ass, comes a number
            if (delta < 200) {
                ideal = proposed;
                if (warn)
                  Trace(tc, 2, "SyncTracker %s: WARNING: For %ld beats, %ld frames requested, %ld frames ideal\n",
                        mName, (long)beatsInLoop, frames, ideal);
            }
            else {
                // we're way off must not even be close to a beat multiple
                Trace(tc, 1, "SyncTracker %s: Unable to adjust %ld frames with %ld beats, delta %ld\n",
                      mName, frames, (long)beatsInLoop, delta);
            }
        }
    }

    return ideal;
}

/**
 * Called to end the recording of the first loop to use this sync tracker.
 * The size of the audio loop is passed, normally Mobius should round
 * this up to be an even number.  The number of expected sync pulses in
 * the loop is also passed.
 * 
 * The Mobius loop that caused this is expected to be exactly at
 * it's start/end point.  The tracker is also initiailzied to be at
 * it's start point.  
 *
 * TODO: With a little more work we should be able to close the tracker
 * early and let it start tracking before we actually finish the initial
 * recording.  That might be interesting for example if you wanted
 * to use AutoRecrord to record several bars, but lock the tracker as
 * soon as the first bar was received.  I can't think of a really 
 * compelling reason for that though.  This would avoid the
 * mPendingPulses complication but would make it harder to extend
 * the tracker if they change the length of the auto or sync'd recording.
 *
 * For Out sync, the MidiTimer wasn't running so we were not accumulating
 * pulses.  mPulse is zero, Just take what we are given and lock the tracker.
 *
 * For Host sync, this should be immeidately after a pulse was received
 * and there can only be one pulse per block since pulses are beats
 * which are large.  mPulse should equal the new pulse count. 
 * Take what we are given and lock the tracker.
 *
 * MIDI tracking is more complicated.  
 * 
 * First the SyncTracker consumes all pulses in an interrupt up front,
 * then Synchronizer sends them to the Tracks one at a time.  If two
 * pulses come in it is possible that recording could stop on
 * pulse 1 and the second pulse is considered a "play" pulse.  In this
 * case the passed number of pulses will be one less than mPulse.
 * This is rare since only MIDI clocks at a very high tempo can generate
 * more than one pulse per interrupt block.
 * 
 * A more common situation for MIDI sync is that the tracker is not 
 * locked exactly on a pulse.   If the midiRecordPulsed parameter is
 * set, then we will be exactly on a clock pulse but this is no longer
 * used.  Instead we adjust for clock jitter by calculating the ideal
 * loop length from the smoothed clock tempo and scheduling an event
 * for that length.  The tracker will then be locked either slightly
 * before or slightly after the final pulse.
 * 
 * If the lock event is before the final pulse, the "pulses" argument
 * will usually be one greater than mPulse since we haven't received
 * the final pulse yet.  The one exception is if there is more than one
 * pulse in the interrupt so mPulse advanced twice which is rare.
 * When mPulse is less than "pulses" we have to ignore the next
 * pulse event because we're logically including it when the tracker
 * is locked.  This is indicated by setting mPendingPulses.
 *
 * If the lock event is after the final pulse, usually "pulses" and
 * mPulse will be the same unless pulses are very close together and
 * we received another while rounding the loop length.  
 * When mPulse is greater than "pulses" the extra pulses must 
 * carrry over and become play pulses after the tracker is locked.
 */
PUBLIC void SyncTracker::lock(TraceContext* tc, int originPulse, int pulses, 
                              long frames, float speed, int beatsPerBar)
{
     if (mLoopFrames > 0) {
        Trace(tc, 1, "SyncTracker %s: tracker is already locked\n", mName);
    }
    else if (frames <= 0) {
        // actually should have a much higher minimum size!
        Trace(tc, 1, "SyncTracker %s: invalid loop frames\n", mName);
    }
    else if (pulses <= 0) {
        Trace(tc, 1, "SyncTracker %s: invalid pulse count\n", mName);
    }
    else {
        // Synchronizer should have called prepare() to calculate an
        // integral beat width, if not calculate the ideal and warn
        // we won't round though, this may cause advance() to return
        // start point pulses that are early
        prepare(tc, pulses, frames, true);

        mLoopPulses = pulses;
        mLoopFrames = frames;
        mBeatsPerBar = beatsPerBar;

        Trace(tc, 2, "SyncTracker %s: loop locked with %ld pulses %ld frames\n",
              mName, (long)mLoopPulses, mLoopFrames);

        // these always starts over from zero
        mAudioFrame = 0;
        mDrift = 0;
        mLastPulseAudioFrame = -1;

        // reset the pulse monitor too?  it's kind of late now since we've
        // locked to it already
        mDriftMonitor->reset();

        // this ususally start from zero but can be adjusted below
        mPendingPulses = 0;

        if (mPulse == 0) {
            // must be OutSync, it will start counting now
        }
        else {
            // Calculate the final state for mPulse.  Usually this will
            // be zero but if we had more than one pulse in a buffer, or 
            // are rounding we have to make adjustments.

            int finalPulse = originPulse + mLoopPulses;
            if (mPulse > finalPulse) {
                // we either received multiple pulses in this interrupt, 
                // or we're rounding and got an extra one during the rounding 
                // period,  extra pulses carry forward
                mPulse = mPulse - finalPulse;
                Trace(tc, 2, "SyncTracker %s: carrying over %ld pulses after closing\n",
                      mName, (long)mPulse);
            }
            else {
                // we're either early or exactly on it
                mPendingPulses = finalPulse - mPulse;
                mPulse = 0;

                if (mPendingPulses > 0) {
                    // should never have more than one of these, either the
                    // rounding was WAY off or pulses are extremely short
                    int level = ((mPendingPulses > 1) ? 1 : 2);
                    Trace(tc, level, "SyncTracker %s: closing with %ld pending pulses\n",
                          mName, (long)mPendingPulses);

                    // since we're ignoring the next pulse, we also
                    // don't advance mPulseFrames
                    // !! how do we control that?
                }
            }
        }
    }
}


/**
 * Should be called ONLY for the OutSyncTracker to adjust for
 * changes in tempo.  Since the MidiTimer doesn't adjust the clock tempo
 * until after the next pulse we save the new size and process it later
 * in pulse().
 */
PUBLIC void SyncTracker::resize(int pulses, long frames, float speed)
{
    if (isLocked()) {
        // have to defer
        mResizePulses = pulses;
        mResizeFrames = frames;
        mResizeSpeed = speed;
        if (!MidiTimerDeferredTempoChange)
          doResize();
    }
    else {
        Trace(1, "SyncTracker %s: resize while tracker was locked!\n", mName);
    }
}

/**
 * Process a resize when we receive the next pulse.
 */
PRIVATE void SyncTracker::doResize()
{
    if (mResizePulses > 0 && mResizeFrames > 0) {

        Trace(2, "SyncTracker %s: resizing to %ld pulses %ld frames\n",
              mName, (long)mResizePulses, (long)mResizeFrames);

        mLoopPulses = mResizePulses;
        mLoopFrames = mResizeFrames;
        mSpeed = mResizeSpeed;

        // if we make them smaller have to wrap
        if (mPulse > mLoopPulses) {
            int oldPulse = mPulse;
            mPulse = mPulse % mLoopPulses;
            Trace(2, "SyncTracker %s: wrapping pulse counter from %ld to %ld\n",
                  mName, (long)oldPulse, (long)mPulse);
        }
        
        // NOTE WELL: mAudioFrame doesn't just wrap, it is rescaled
        // to have the same relative location within the new loop
        // if drift is positive the audio frame is ahead
        long newFrame = wrap((long)(getPulseFrame() + mDrift));

        if (mAudioFrame != newFrame)
          Trace(2, "SyncTracker %s: warping frame counter from %ld to %ld\n",
                mName, mAudioFrame, newFrame);

        mAudioFrame = newFrame;
        mLastPulseAudioFrame = -1;
        
        // drift stays the same...

        mResizePulses = 0;
        mResizeFrames = 0;
        mResizeSpeed = 0;

        // reset the pulse monitors too so the last set of samples
        // don't make the new average lag
        mPulseMonitor->reset();
        mDriftMonitor->reset();
    }
}

/****************************************************************************
 *                                                                          *
 *                                   DRIFT                                  *
 *                                                                          *
 ****************************************************************************/

/**
 * Calculate the number of frames of drift, forward or backward, between
 * the audio stream frame and the pulse frame.  
 *
 * If positive it indicates that the audio frame is ahead, if negative behind.
 * 
 * Original Synchronizer first calculated where we expect the pulse frame
 * to be from the loop frame.  It started with the current loop frame
 * and added the input latency if we were MIDI slave syncing, then
 * wrapped this around the loop length.  Latency compensation was
 * necessary since pulses needed to be delayed like other
 * MIDI events.  Here this isn't important since we're maintaining
 * our own pseudo loop that starts and ends exactly on a pulse whenever
 * it is received.  This may make our loop slightly ahead of the
 * actual loop, but that doesn't affect drift calculations.
 */
PUBLIC long SyncTracker::calcDrift(long pulseFrame, long audioFrame,
                                   long loopFrames)
{
    long drift = 0;

    if (audioFrame != pulseFrame) {
        long limit = loopFrames;
		long ahead;
		long behind;

        // find the "nearest" pulse
		if (audioFrame > pulseFrame) {
			ahead = audioFrame - pulseFrame;
			behind = (limit - audioFrame) + pulseFrame;
		}
		else {
			behind = pulseFrame - audioFrame;
			ahead = (limit - pulseFrame) + audioFrame;
		}

		if (ahead <= behind)
		  drift = ahead;
		else
		  drift = -behind;
    }

	return drift;
}

/**
 * Called after drift correction to remove drift.
 * Note that this is more complicated than just setting mAudioFrame
 * to getPulseFrame() and setting drift to zero which only works if the
 * drift is being corrected exactly on a pulse.  
 *
 * When DriftCheckPoint=Loop, we can be in the middle of a pulse near
 * the Loop start point of the first following track.  In this case we
 * have to keep the partial advance of mAudioFrames.  
 * Ugh, this is the kind of mess got us into trouble before, perhaps
 * we should just require that drift correction be performed on a pulse
 * and call it a day?
 */
PUBLIC void SyncTracker::correct()
{
    // only works if you're exactly on a pulse
    //mAudioFrame = (long)getPulseFrame();
    //mDrift = 0;

    if (mDrift != 0) {

        // if drift is positive the audio frame is ahead
        long newFrame = wrap(mAudioFrame - mDrift);

        Trace(2, "SyncTracker %s: Drift correction of tracker from %ld to %ld\n",  
              mName, mAudioFrame, newFrame);

        mAudioFrame = newFrame;
        mDrift = 0;
        mDriftMonitor->reset();

        // pulse width isn't changing so we don't have to reset mPulseMonitor,  
        // but we just moved mAudioFrame will will screw up the 
        // next calculation of advance so make it start over
        mLastPulseAudioFrame = -1;
    }
}

/**
 * Calculate a drifted frame.  
 * This is used in cases where we need to change mAudioFrame but
 * we want to perserve the amount of drift.  This happens when
 * we receive START or CONTINUE events.
 *
 * Hmm, rather than bothering with this it might be simpler just
 * to ignore START/CONTINUE and treat it just like a pulse that
 * doesn't change mAudioFrame.
 */
PUBLIC long SyncTracker::addDrift(long audioFrame, long loopFrames,
                                  long drift)
{
    long drifted = audioFrame;

    // only makes sense if we have a size
    if (loopFrames > 0) {

        drifted += drift;

        if (drifted >= loopFrames)
          drifted = drifted % loopFrames;

        else if (drifted < 0)
          drifted = loopFrames - ((-drifted) % loopFrames);
    
    }
    return drifted;
}

/**
 * The usual wrap calculation.
 * Could make advance use this too...
 */
PRIVATE long SyncTracker::wrap(long frame)
{
    return wrap(frame, mLoopFrames);
}
           
PRIVATE long SyncTracker::wrap(long frame, long max)
{
    if (max > 0) {
        if (frame > 0)
          frame = frame % max;
        else {
            while (frame < 0)
              frame += max;
        }
    }
    return frame;
}

/****************************************************************************
 *                                                                          *
 *                                 UNIT TESTS                               *
 *                                                                          *
 ****************************************************************************/

PUBLIC int SyncTracker::getDriftChecks()
{
    return mDriftChecks;
}

PUBLIC void SyncTracker::incDriftChecks()
{
    mDriftChecks += 1;
}

PUBLIC int SyncTracker::getDriftCorrections()
{
    return mDriftCorrections;
}

PUBLIC void SyncTracker::incDriftCorrections()
{
    mDriftCorrections += 1;
}

/**
 * Only for the unit tests that set this through a ScriptVariable.
 */ 
PUBLIC void SyncTracker::setDriftCorrections(int i)
{
    mDriftCorrections = i;
}

/**
 * Force a drift.  This is intended for unit tests to set up
 * drift conditions then check to see that correction was applied.
 */
PUBLIC void SyncTracker::drift(int delta)
{
    long startFrame = mAudioFrame;
    mAudioFrame = wrap(mAudioFrame + delta);

    Trace(2, "SyncTracker %s: Drifting audio frame %ld by %ld to %ld\n",
          mName, startFrame, (long)delta, mAudioFrame);

    long startDrift = mDrift;

    bool driftAbsolute = false;
    if (driftAbsolute) {
        // The new drift calculated relative to getPulseFrame() may 
        // be quite large because we're in the middle of the pulse and
        // the partial advance of mAudioFrame now looks like drift.
        // This will correct itself on the next pulse, but it looks
        // scay at the moment.  
        mDrift = calcDrift((long)getPulseFrame(), mAudioFrame, mLoopFrames);
    }
    else {
        // This method is closer to reality because it makes drift look like
        // what it would have been on the last pulse.  
        mDrift = mDrift + delta;
    }

    Trace(2, "SyncTracker %s: Starting drift %ld new drift %ld\n",
          mName, startDrift, mDrift);

}

/****************************************************************************
 *                                                                          *
 *                               PULSE MONITOR                              *
 *                                                                          *
 ****************************************************************************/

PUBLIC PulseMonitor::PulseMonitor()
{
	reset();
}

PUBLIC void PulseMonitor::reset()
{
	mSample = 0;
	mTotal = 0;
	mDivisor = 0;
	mPulse = 0.0f;

	for (int i = 0 ; i < PULSE_MONITOR_SAMPLES ; i++)
	  mSamples[i] = 0;
}

PUBLIC PulseMonitor::~PulseMonitor()
{
}

PUBLIC float PulseMonitor::getPulseWidth()
{
	return mPulse;
}

PUBLIC void PulseMonitor::pulse(int width)
{
    // remove the oldest sample (the one we're about to replace)
    mTotal -= mSamples[mSample];

    // add the new sample
    mTotal += width;
    mSamples[mSample] = width;
        
    mSample++;
    if (mSample >= PULSE_MONITOR_SAMPLES)
      mSample = 0;

    if (mDivisor < PULSE_MONITOR_SAMPLES)
      mDivisor++;

    // maintain the average pulse width
    mPulse = (float)mTotal / (float)mDivisor;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
