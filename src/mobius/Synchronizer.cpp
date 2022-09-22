/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * A class encapsulating most of the logic related to external and
 * internal synchronization.
 * 
 * This one is a little funny in that it contains things that are called
 * from several levels of the system.  But the synchronization logic is
 * so closely related, I wanted to keep it all in one place rather than 
 * distributing it over Mobius, Track, InputStream, and Loop.  It's a bit
 * like a Mediator pattern.
 *
 * At the top, Synchronizer is called by Mobius during each audio Interrupt
 * to handle sync events that came in since the last interrupt.  MIDI events
 * are maintained in a MidiQueue, the queue maintains a MidiState that
 * is updated as events are removed from the queue.  MidiState keeps
 * track of start/stop, clock counts, and beat counts.
 * 
 * Synchronizer manages a MidiTransport object which coordinates the
 * control over a millisecond timer and a MIDI device used to generate
 * midi clocks in "out" sync mode.  As we send out MIDI clocks we also
 * route them into another MidiQueue managed by MidiTransport so that we
 * can track drift between the timer and the audio stream.  In this way, 
 * tracking internal clock drift works much the same as tracking an
 * external MIDI clock.
 * 
 * During an audio interrupt, we will be passed an AudioStream
 * containing information about VST/AU host sync events such as beats or 
 * bars that will happen within the next audio buffer.  The VST/AU
 * sync events, and events from the internal and external MIDI queues
 * are converted into a list of Event objects with appropriate offsets
 * into the current audio buffer.  These events will be injected
 * into the Event list of the active Loop in each Track so that each
 * track can take action on them differently.
 * 
 * Sync events for inter-track sync are a bit more complicated because
 * the master track must be advanced first before we know if it
 * crosses any interesting sync boundaries.  This is handled by 
 * giving the TrackSyncMaster track a higher priority than the others,
 * Recorder will process it first in each interrupt.
 *
 * So, the Synchronizer is the funnel into which three types of sync
 * events go to be converted into the appropriate Event objects for
 * each track.  These sync Events are then merged with the events scheduled
 * on the Loop.  
 *
 * When the loop is eventually ready to process a sync Event, it just
 * turns around and calls back to Synchronizer::syncEvent for processing.
 * Sync event processing happens during three loop modes:
 *
 *  SynchronizeMode
 *     - a recording has been requested and we're waiting for the
 *       appropriate time to start
 *
 *  RecordMode
 *     - recording has begun, we're waiting for a function to stop the
 *       recording, waiting for the quantized end to be reached, or waiting
 *       for the end of an AutoRecord
 *
 *  PlayMode (MuteMode, ConfirmMode)
 *     - the loop has finished recording, we maintain a SyncTrakcer to 
 *       compare the receipt rate of sync pulses to the advance in the
 *       audio stream, the loop is adjusted if it drifts too far
 *       out of sync with the pulses
 *    
 * Loop will also call back to synchronizer when various events with
 * possible synchronization consequences occur.  These include ending
 * the initial record, multiplying or inserting new cycles, 
 * redefining the cycle length with unrounded multiply/insert, speed shift,
 * or "transport" operations like mute, restart, and pause.
 *
 * TRACKER EVENTS
 *
 * Once a SyncTracker has been locked it will generate its own events
 * for beats and bars according to the frame advance during each interrupt.
 * These will start close to the beat/bar events being received from the
 * external source but may drift over time.  
 *
 * It is important to understand that once a SyncTracker has been locked
 * all other tracks that follow the same source will beging following
 * tracker events *not* source events.  For example, say all tracks are
 * configured to follow MIDI clocks.
 *
 * The first track to be recorded watches pulses from the MIDI device,
 * calculates a tempo and rounds off the recording so we have a nice
 * integral beat length.  The tracker is now locked.  If another MIDI
 * follower begins recording it waits for pulses from the tracker rather
 * than the MIDI device.  The effect is similar to track sync, once the
 * tracker is locked it becomes the "master" track that everyone else follows.
 * This ensures that all tracks that follow the same source will end up
 * with compatible lengths which we can't ensure if we follow jittery
 * clocks like MIDI.  This also solves a number of other problems related
 * to realign and drift.  Realign is always done to the tracker not the
 * actual source so that all tracks realign consistently.  Drift correction
 * when it happens is detectted once by the tracker and applied to all
 * followers.  
 *
 * This applies to SYNC_MIDI and SYMC_HOST.  SYNC_OUT has historically fallen
 * back to normal Track Sync with the master track, but now that we have
 * Tracker Sync we could provide an alternate to follow OutSyncTracker 
 * which has a stable beat (subcycle) length.
 */

#include <stdlib.h>
#include <memory.h>
#include <math.h>

#include "Thread.h"

#include "MidiByte.h"
#include "MidiEvent.h"
#include "MidiInterface.h"

#include "Action.h"
#include "Event.h"
#include "EventManager.h"
#include "Function.h"
#include "Layer.h"
#include "Loop.h"
#include "MidiQueue.h"
#include "MidiTransport.h"
#include "Mode.h"
#include "Mobius.h"
#include "MobiusConfig.h"
#include "Script.h"
#include "Stream.h"
#include "SyncState.h"
#include "SyncTracker.h"
#include "Track.h"

#include "Synchronizer.h"

/****************************************************************************
 *                                                                          *
 *   							  CONSTANTS                                 *
 *                                                                          *
 ****************************************************************************/

/****************************************************************************
 *                                                                          *
 *   							 SYNCHRONIZER                               *
 *                                                                          *
 ****************************************************************************/

PUBLIC Synchronizer::Synchronizer(Mobius* mob, MidiInterface* midi)
{
	mMobius = mob;
	mMidi = midi;
    mTransport = new MidiTransport(midi, mob->getSampleRate());
    // MidiQueue self initializes
    mHostTracker = new SyncTracker(SYNC_HOST);
    mMidiTracker = new SyncTracker(SYNC_MIDI);
    mOutTracker = new SyncTracker(SYNC_OUT);
	mOutSyncMaster = NULL;
	mTrackSyncMaster = NULL;

	mMaxSyncDrift = DEFAULT_MAX_SYNC_DRIFT;
	mDriftCheckPoint = DRIFT_CHECK_LOOP;
	mMidiRecordMode = MIDI_TEMPO_AVERAGE;
    mNoSyncBeatRounding = false;

    EventPool* epool = mMobius->getEventPool();

    mInterruptEvents = new EventList();
    mReturnEvent = epool->newEvent();
    mReturnEvent->setOwned(true);
	mHostTempo = 0.0f;
	mHostBeat = 0;
    mHostBeatsPerBar = 0;
    mHostTransport = false;
    mHostTransportPending = false;
	mLastInterruptMsec = 0;
	mInterruptMsec = 0;
	mInterruptFrames = 0;

    mForceDriftCorrect = false;
	// kludge for special conditional breakpoints
	mKludgeBreakpoint = false;

	// assign trace names
	mMidiQueue.setName("external");
}

PUBLIC Synchronizer::~Synchronizer()
{
    delete mTransport;
    delete mHostTracker;
    delete mMidiTracker;
    delete mOutTracker;

    flushEvents();
    delete mInterruptEvents;
    
    mReturnEvent->setOwned(false);
    mReturnEvent->free();
}

/**
 * Flush the interrupt event list.
 */
PRIVATE void Synchronizer::flushEvents()
{
    // have to mark them not owned so they can be freed
    for (Event* event = mInterruptEvents->getEvents() ; event != NULL ; 
         event = event->getNext())
      event->setOwned(false);

    mInterruptEvents->flush(true, false);
}

/**
 * Pull out configuration parameters we need frequently.
 */
PUBLIC void Synchronizer::updateConfiguration(MobiusConfig* config)
{
	mMaxSyncDrift = config->getMaxSyncDrift();
	mDriftCheckPoint = config->getDriftCheckPoint();
	mMidiRecordMode = config->getMidiRecordMode();
    mNoSyncBeatRounding = config->isNoSyncBeatRounding();
}

/**
 * Set a flag to force drift correction on the next interrupt.
 */
PUBLIC void Synchronizer::forceDriftCorrect()
{
    Trace(2, "Sync: Scheduling forced drift correction\n");
    mForceDriftCorrect = true;
}

/**
 * Called by Mobius after a global reset.
 * We can't clear the queues because incomming sync state is relevent.
 * This only serves to emit some diagnostic messages.
 */
PUBLIC void Synchronizer::globalReset()
{
    if (mMidiQueue.hasEvents())
      Trace(1, "WARNING: External MIDI events queued after global reset\n");

    // !! why is this important, we're going to be shutting down the clock
    // so it doesn't matter if transport state loses sync
    if (mTransport->hasEvents())
      Trace(1, "WARNING: Internal MIDI events queued after global reset\n");
}

/****************************************************************************
 *                                                                          *
 *                               MIDI INTERRUPT                             *
 *                                                                          *
 ****************************************************************************/

/**
 * Called in the MIDI thread as events come in.
 * Return true if this was a realtime event that should not
 * be treated as a function trigger.
 *
 * Most realtime events are added to a MidiQueue for processing
 * on the next audio interrupt.
 */
PUBLIC bool Synchronizer::event(MidiEvent* e)
{
	bool realtime = true;
	int status = e->getStatus();

	switch (status) {
		case MS_QTRFRAME: {
			// not sure what this is, ignore
		}
		break;
		case MS_SONGPOSITION: {
			// this is only considered actionable if a MS_CONTINUE is received
			mMidiQueue.add(e);
		}
		break;
		case MS_SONGSELECT: {
			// nothing meaningful for Mobius?
			// could use it to select loops?
		}
		break;
		case MS_CLOCK: {
			mMidiQueue.add(e);
		}
		break;
		case MS_START: {
			mMidiQueue.add(e);
		}
		break;
		case MS_STOP: {
			mMidiQueue.add(e);
		}
		break;
		case MS_CONTINUE: {
			mMidiQueue.add(e);
		}
		break;
		case MS_SENSE: {
			// not realtime, but always ignore them
		}
		break;
		default: {
			realtime = false;
		}
		break;
	}

	return realtime;
}

/****************************************************************************
 *                                                                          *
 *                               BEATS PER BAR                              *
 *                                                                          *
 ****************************************************************************/
/*
 * This is too complicated for my taste, think more about ways to simplify this!
 *
 * The number of beats in a bar is ideally defined in the Setup, but we
 * have historically fallen back to subCycles parameter from the Preset
 * if Setup beatsPerBar isn't set.  The problem is that this makes BPB
 * track specific since each track can have a different preset.
 *
 * The exception to this rule is SYNC_HOST where we always let the 
 * host determine the time signature.  
 *
 * In practice beatsPerBar is almost never changed, but I'm not sure how
 * advanced users have been using it so we'll continue to fall back to the
 * Preset.
 *
 * BPB is used for several things:
 *
 *    - quantizing the start/end of a recording
 *    - Realign when RealignTime=Bar
 *    - calcultaing the number of cycle pulses during recording
 *    - length of an AutoRecord bar
 *
 * Since this is a fundamental part of the SyncTracker calculations, we
 * capture beatsPerBar in the SyncTracker when it is locked.  Thereafter
 * this value will be used in all calcultaions even if the Preset or
 * Setup changes.  The idea here is that you recorded something against
 * an external loop with a certain time signature, and once that recording
 * ends you no longer control the time signature of the external loop.
 * The Preset can change BPB for other purposes like polyrhythms, but for
 * the purposes of Realign the BPB in effect when the SyncTracker was locked
 * remains constant.
 *
 * So...determining the effective beats per bar is defined as:
 *
 *    If the Loop/Track follows a locked SyncTracker, get it from the tracker.
 *    Else use the Setup
 *    Else use the Preset active in the Track
 *
 * It's even a little more complicated than that because when recording
 * starts we'll save the BPB in the SyncState temporarily until the
 * recording ends.  This because BPB was used in the quantization of the
 * record start and we should be consistent about quantizing the ending.
 *
 * THINK: It would make life easier if we only used the Setup and we let
 * it change during recording.   This might have strange results but 
 * maybe no stranger than explaining to users why changing Presets has
 * an effect on sync tempo.
 */

/**
 * Get the tracker for a sync source.
 */
PRIVATE SyncTracker* Synchronizer::getSyncTracker(SyncSource src)
{
    SyncTracker* tracker = NULL;

    if (src == SYNC_OUT)
      tracker = mOutTracker;

    else if (src == SYNC_HOST)
      tracker = mHostTracker;

    else if (src == SYNC_MIDI)
      tracker = mMidiTracker;

    return tracker;
}

/**
 * Derive the number of beats in one bar.
 *
 * This expected to be defined globally for all tracks and sync sources
 * in the Setup.  If not set there, we will fall back to the subCycles
 * parameter from the current track which is an older convention I don't like.
 *
 * The number of beats defines the lengt of a "bar" which is important for:
 * Once a SyncTracker is locked, the BeatsPerBar active at that time 
 * is also locked because this defines where the bars were when the 
 * tracker was used for recording.
 */
PRIVATE int Synchronizer::getBeatsPerBar(SyncSource src, Loop* l)
{
    int beatsPerBar = 0;

    SyncTracker* tracker = getSyncTracker(src);

    if (tracker != NULL && tracker->isLocked())
      beatsPerBar = tracker->getBeatsPerBar();

    else {
        // host is special, we let it be determined externally
        // if not set we fall back to the setup
        if (src == SYNC_HOST)
          beatsPerBar = mHostBeatsPerBar;

        if (beatsPerBar <= 0) {

            Setup* setup = mMobius->getInterruptSetup();
            beatsPerBar = setup->getBeatsPerBar();

            if (beatsPerBar <= 0) {
                // now it gets spooky, pick the current track preset
                Track* t;
                if (l != NULL)
                  t = l->getTrack();
                else
                  t = mMobius->getTrack(mMobius->getActiveTrack());
                Preset* p = t->getPreset();
                beatsPerBar = p->getSubcycles();
            }
        }
    }

    return beatsPerBar;
}

/****************************************************************************
 *                                                                          *
 *                          MIDI OUT SYNC VARIABLES                         *
 *                                                                          *
 ****************************************************************************/

/**
 * Get the effective beatsPerBar for OUT sync.
 */
PUBLIC int Synchronizer::getOutBeatsPerBar()
{
    return getBeatsPerBar(SYNC_OUT, NULL);
}

/**
 * Exposed as variable syncOutTempo.
 * 
 * The tempo of the internal clock used for out sync.
 * This is the same value returned by "tempo" but only if the
 * current track is in Sync=Out or Sync=OutManual.
 * Note that unlike "tempo" this one is not sensitive to 
 * mTransport->isSending().
 */
PUBLIC float Synchronizer::getOutTempo()
{
	return mTransport->getTempo();
}

/**
 * Exposed as the variable syncOutRawBeat.
 * 
 * The current raw beat count maintained by the internal clock.
 * This will be zero if the internal clock is not running.
 */
PUBLIC int Synchronizer::getOutRawBeat()
{
    return mTransport->getRawBeat();
}

/**
 * Exposed as the variable syncOutBeat.
 * The current beat count maintained by the internal clock relative 
 * to the bar.
 */
PUBLIC int Synchronizer::getOutBeat()
{
    int bpb = getOutBeatsPerBar();

    return mTransport->getBeat(bpb);
}

/**
 * Exposed as the variable syncOutBar.
 * The current bar count maintained by the internal clock.
 * This is calculated from the raw beat count, modified by the
 * effective beatsPerBar.
 */
PUBLIC int Synchronizer::getOutBar()
{
    int bpb = getOutBeatsPerBar();

    return mTransport->getBar(bpb);
}

/**
 * Exposed as variable syncOutSending.
 * Return true if we're sending clocks.
 */
PUBLIC bool Synchronizer::isSending()
{
	return mTransport->isSending();
}

/**
 * Exposed as variable syncOutStarted.
 * Return true if we've sent the MIDI Start event and are sending clocks.
 */
PUBLIC bool Synchronizer::isStarted()
{
	return mTransport->isStarted();
}

/**
 * Exposed as variable syncOutStarts.
 * Return the number of MIDI Start messages sent since the last stop.
 * Used by unit tests to verify that we're sending start messages.
 */
PUBLIC int Synchronizer::getStarts()
{
	return mTransport->getStarts();
}

/****************************************************************************
 *                                                                          *
 *                           MIDI IN SYNC VARIABLES                         *
 *                                                                          *
 ****************************************************************************/

/**
 * Get the effective beats per bar for MIDI sync.
 */
PUBLIC int Synchronizer::getInBeatsPerBar()
{
    return getBeatsPerBar(SYNC_MIDI, NULL);
}

/**
 * Exposed as variable syncInTempo.
 * The tempo of the external MIDI clock being received.
 * This is the same value returned by "tempo" but only if the
 * current track is in SyncMode In, MIDIBeat, or MIDIBar.
 * Note that this is the full precision tempo, not the "smooth" tempo.
 */
PUBLIC float Synchronizer::getInTempo()
{
	return mMidi->getInputTempo();
}

/**
 * Exposed as syncInRawBeat
 * The current beat count derived from the external MIDI clock.
 */
PUBLIC int Synchronizer::getInRawBeat()
{
	MidiState* s = mMidiQueue.getMidiState();
	return s->beat;
}

/**
 * Exposed as syncInBeat.
 * 
 * The current beat count derived from the external MIDI clock,
 * relative to the bar.
 */
PUBLIC int Synchronizer::getInBeat()
{
	MidiState* s = mMidiQueue.getMidiState();
	int beat = s->beat;
    int beatsPerBar = getInBeatsPerBar();

	if (beatsPerBar > 0)
	  beat = beat % beatsPerBar;

	return beat;
}

/**
 * Exposed as syncInBar.
 * The current bar count derived from the external MIDI clock.
 */
PUBLIC int Synchronizer::getInBar()
{
	MidiState* s = mMidiQueue.getMidiState();
    int beatsPerBar = getInBeatsPerBar();
    int bar = 1;

	if (beatsPerBar > 0)
	  bar = s->beat / beatsPerBar;

	return bar;
}

/**
 * Exposed as syncInReceiving.
 * True if we are currently receiving MIDI clocks.
 */
PUBLIC bool Synchronizer::isInReceiving()
{
	MidiState* state = mMidiQueue.getMidiState();
	return state->receivingClocks;
}

/**
 * Exposed as syncInStarted.
 * True if we have received a MIDI start or continue message.
 */
PUBLIC bool Synchronizer::isInStarted()
{
	MidiState* state = mMidiQueue.getMidiState();
	return state->started;
}

/****************************************************************************
 *                                                                          *
 *                            HOST SYNC VARIABLES                           *
 *                                                                          *
 ****************************************************************************/

/**
 * Get the effective beats per bar for HOST sync.
 */
PUBLIC int Synchronizer::getHostBeatsPerBar()
{
    return getBeatsPerBar(SYNC_HOST, NULL);
}

/**
 * Exposed as syncHostTempo.
 * The tempo advertised by the plugin host.
 */
PUBLIC float Synchronizer::getHostTempo()
{
	return mHostTempo;
}

/**
 * Exposed as syncHostRawBeat
 * The current beat count given by the host, not relative to the bar.
 */
PUBLIC int Synchronizer::getHostRawBeat()
{
    return mHostBeat;
}

/**
 * Exposed as syncHostBeat
 * The current beat count given by the host, relative to the bar.
 */
PUBLIC int Synchronizer::getHostBeat()
{
    int beat = mHostBeat;

    int bpb = getHostBeatsPerBar();
    if (bpb > 0)
      beat = (mHostBeat % bpb);

	return beat;
}

/**
 * Exposed as syncHostBar.
 * The current bar count given by the host.
 */
PUBLIC int Synchronizer::getHostBar()
{
    int bar = 0;

    int bpb = getHostBeatsPerBar();
    if (bpb > 0)
      bar = (mHostBeat / bpb);

	return bar;
}

/**
 * Exposed as syncHostReceiving.
 * True if we are currently receiving VST pulse events from the host.
 *
 * TODO: Need to determine what this means.
 * If the transport is playing it makes sense for this to be one, but 
 * you could also use this for bypass state.
 */
PUBLIC bool Synchronizer::isHostReceiving()
{
	return mHostTransport;
}

/****************************************************************************
 *                                                                          *
 *                           GENERIC SYNC VARIABLES                         *
 *                                                                          *
 ****************************************************************************/

/**
 * Exposed as variable "syncTempo".
 * 
 * For OUT this is the tempo we calculated.
 * For MIDI this is the tempo we're smoothing from the external source.
 * For HOST this is the tempo reported by the host.
 *
 * Also called by track to return in the TrackState for the UI to display.
 * This is called outside the interrupt handler so anything we touch
 * has to be stable.
 * 
 */
PUBLIC float Synchronizer::getTempo(Track* t)
{
	float tempo = 0.0f;
    SyncState* state = t->getSyncState();

	switch (state->getDefinedSyncSource()) {
		case SYNC_OUT:
			// only return a value while we're sending clocks, 
			// currently used so we don't display a tempo when we're
			// not running
			if (mTransport->isSending())
			  tempo = mTransport->getTempo();
            break;

		case SYNC_MIDI:
			tempo = mMidi->getInputTempo();
            break;

		case SYNC_HOST:
			// NOTE: mHostTime is usually valid, but technically it could
			// be in a state of change during an interrupt, so we need to 
			// capture it to a local field.
			tempo = mHostTempo;
            break;
			
		// have to have these or Xcode 5 bitches
		case SYNC_DEFAULT:
		case SYNC_NONE:
		case SYNC_TRACK:
			break;

	}

	return tempo;
}

/**
 * Exposed as syncRawBeat
 * The current absolute beat count.
 * This will be the same as syncOutRawBeat, syncInRawBeat, 
 * or syncHostRawBeat depending on the SyncMode of the current track.
 */
PUBLIC int Synchronizer::getRawBeat(Track* t)
{
	int beat = 0;
    SyncState* state = t->getSyncState();

	switch (state->getDefinedSyncSource()) {
		case SYNC_OUT:
			beat = getOutRawBeat();
            break;

		case SYNC_MIDI:
			beat = getInRawBeat();
            break;

		case SYNC_HOST:
			beat = getHostRawBeat();
            break;

		// have to have these or Xcode 5 bitches
		case SYNC_DEFAULT:
		case SYNC_NONE:
		case SYNC_TRACK:
			break;
	}

	return beat;
}

/**
 * Exposd as syncBeat
 * The current bar relative beat count.
 * This will be the same as syncOutBeat, syncInBeat, or syncHostBeat
 * depending on the SyncMode of the current track.
 */
PUBLIC int Synchronizer::getBeat(Track* t)
{
	int beat = 0;
    SyncState* state = t->getSyncState();
    
	switch (state->getDefinedSyncSource()) {
		case SYNC_OUT:
			beat = getOutBeat();
            break;

		case SYNC_MIDI:
			beat = getInBeat();
            break;

		case SYNC_HOST:
			beat = getHostBeat();
            break;

		// have to have these or Xcode 5 bitches
		case SYNC_DEFAULT:
		case SYNC_NONE:
		case SYNC_TRACK:
			break;
	}

	return beat;
}

/**
 * Exposed as syncBar
 * The current bar count.
 * This will be the same as syncOutBar, syncInBar, or syncHostBar
 * depending on the SyncMode of the current track.
 */
PUBLIC int Synchronizer::getBar(Track* t)
{
	int bar = 0;
    SyncState* state = t->getSyncState();

	switch (state->getDefinedSyncSource()) {
		case SYNC_OUT:
			bar = getOutBar();
            break;

		case SYNC_MIDI:
			bar = getInBar();
            break;

		case SYNC_HOST:
			bar = getHostBar();
            break;

		// have to have these or Xcode 5 bitches
		case SYNC_DEFAULT:
		case SYNC_NONE:
		case SYNC_TRACK:
			break;

	}

	return bar;
}

/****************************************************************************
 *                                                                          *
 *   							 SYNC STATUS                                *
 *                                                                          *
 ****************************************************************************/

/**
 * Used for the Variables that expose sync loop status.
 * TODO: Should we just put this on the SyncState?
 */
PUBLIC SyncTracker* Synchronizer::getSyncTracker(Track* t)
{
    SyncState* state = t->getSyncState();
    SyncSource src = state->getEffectiveSyncSource();

    return getSyncTracker(src);
}

PRIVATE SyncTracker* Synchronizer::getSyncTracker(Loop* l)
{
    return getSyncTracker(l->getTrack());
}

/**
 * Return the current MIDI clock for use in trace messages.
 * Be sure to return the ITERATOR clock, not the global one that hasn't
 * been incremented yet.
 */
PUBLIC long Synchronizer::getMidiSongClock(SyncSource src)
{
	int clock = 0;

	switch (src) {
		case SYNC_OUT:
			clock = mTransport->getSongClock();
            break;

		case SYNC_MIDI:
			clock = mMidiQueue.getMidiState()->songClock;
            break;

		case SYNC_HOST:
			// hmm, probably could capture this if necessary
            break;

		// have to have these or Xcode 5 bitches
		case SYNC_DEFAULT:
		case SYNC_NONE:
		case SYNC_TRACK:
			break;
	}

	return clock;
}

/**
 * Called by Track to fill in the relevant sync state for a track.
 *
 * The tempo value will be zero if we are not currently sending or receiving
 * clocks.  It is normally always non-zero for host sync.
 *
 * When the beat and bar values are zero, they do not have meaningful
 * values and should not be displayed.  The UI may want to capture the last
 * known valid values and continue displaying those until the next
 * start/continue.  
 *
 * When beat/bar are non-zero, we are receiving or sending clocks and are
 * in a "started" state.  The first beat and bar are numbered 1.
 *
 * !! This is no longer really track specific.  Sync on/off can be set
 * but you can't have one track with Sync=Host and another with Sync=Midi,
 * some of the state variables could be moved up?
 */
PUBLIC void Synchronizer::getState(TrackState* state, Track* t)
{
	SyncState* syncState = t->getSyncState();
    SyncSource source = syncState->getEffectiveSyncSource();
	
    state->syncSource = source;
    state->syncUnit = syncState->getSyncUnit();
	state->outSyncMaster = (t == mOutSyncMaster);
	state->trackSyncMaster = (t == mTrackSyncMaster);
	state->tempo = 0;
	state->beat = 0;
	state->bar = 0;

	switch (source) {

		case SYNC_OUT: {
			// if we're not sending, don't display tempo
			// ?? what about beat/bar, could display those?
			if (mTransport->isSending()) {
				state->tempo = getOutTempo();
				// Note that we adjust the zero based beat/bar count
				// for display.
				state->beat = getOutBeat() + 1;
				state->bar = getOutBar() + 1;
			}
		}
		break;

		case SYNC_MIDI: {
			// for display purposes we use the "smooth" tempo
			// this is a 10x integer
			int smoothTempo = mMidi->getInputSmoothTempo();
			state->tempo = (float)smoothTempo / 10.0f;

			// only display advance beats when started,
			// TODO: should we save the last known beat/bar values
			// so we can keep displaying them till the next start/continue?
			if (isInStarted()) {
				state->beat = getInBeat() + 1;
				state->bar = getInBar() + 1;
			}
		}
		break;

		case SYNC_HOST: {
			state->tempo = getHostTempo();

			// only display advance beats when started,
			// TODO: should we save the last known beat/bar values
			// so we can keep displaying them till the next start/continue?
			if (isHostReceiving()) {
                state->beat = getHostBeat() + 1;
                state->bar = getHostBar() + 1;
            }
		}
		break;

		// have to have these or Xcode 5 bitches
		case SYNC_DEFAULT:
		case SYNC_NONE:
		case SYNC_TRACK:
			break;
	}

}

/****************************************************************************
 *                                                                          *
 *                          RECORD START SCHEDULING                         *
 *                                                                          *
 ****************************************************************************/

/**
 * Schedule a recording event.
 * This must be called only by RecordFunction and the Action's function
 * must be in the Record family.
 * 
 * If were already in Record mode should have called scheduleModStop first.
 * See file header comments about nuances.
 */
PUBLIC Event* Synchronizer::scheduleRecordStart(Action* action,
                                                Function* function,
                                                Loop* l)
{
	Event* event = NULL;
    EventManager* em = l->getTrack()->getEventManager();
	MobiusMode* mode = l->getMode();

    // When we moved this over from RecordFunction we may have lost
    // the original function, make sure.  I don't think this hurts 
    // anything but we need to be clearer
    Function* f = action->getFunction();
    if (f != function)
      Trace(1, "Sync: Mismatched function in scheduleRecordStart\n");

	if (mode == SynchronizeMode ||
        mode == ThresholdMode ||
        mode == RecordMode) {

		// These cases are almost identical: schedule a RecordStop
		// event to end the recording after the number of auto-record bars.
		// If there is already a RecordStop event, extend it by one bar.

        event = em->findEvent(RecordStopEvent);
        if (event != NULL) {
            // Function::invoke will always call scheduleModeStop 
            // before calling the Function specific scheduleEvent.  For
            // the second press of Record this means we'll end up here
            // with the stop event already scheduled, but this is NOT
            // an extension case.  Catch it before calling extendRecordStop
            // to avoid a trace error.
            if (action->down && action->getFunction() != Record) {

                // another trigger, increase the length of the recording
                // but ignore the up transition of SUSRecord
                extendRecordStop(action, l, event);
            }
        }
        else if (action->down || function->sustain) {

            // schedule an auto-stop
            if (function->sustain) {
                // should have had one from the up transition of the
                // last SUS trigger
                Trace(l, 1, "Sync: Missing RecordStopEvent for SUSRecord!\n");
            }

            event = scheduleRecordStop(action, l);
        }
    }
	else if (!action->noSynchronization && isRecordStartSynchronized(l)) {

        // Putting the loop in Threshold or Synchronize mode is treated
        // as "not advancing" and screws up playing.  Need to rethink this
        // so we could continue playing the last play layer while waiting.
        // !! Issues here.  We could consider this to be resetting the loop
        // and stopping sync clocks if we're the master but that won't happen
        // until the Record event activates.  If we just mute now and don't
        // advance, the loop thermometer will freeze in place.  But it
        // is sort of like a pause with possible undo back so maybe that's okay.
		l->stopPlayback();

		event = schedulePendingRecord(action, l, SynchronizeMode);
	}
	else if (!action->noSynchronization && isThresholdRecording(l)) {
        // see comments above for SynchronizeMode
        // should noSynchronization control threshold too?
		l->stopPlayback();
		event = schedulePendingRecord(action, l, ThresholdMode);
	}
	else {
        // Begin recording now
		// don't need to wait for the event, stop playback now
		l->stopPlayback();

        // If this is AudoRecord we'll be scheudlign both a start   
        // and an end event.  The one that owns the action will be
        // the "primary" event that scripts will wait on.  It feels like
        // this should be the stop event.
        
        Action* startAction = action;
        if (f == AutoRecord)
          startAction = mMobius->cloneAction(action);

        Function* f = action->getFunction();
		event = f->scheduleEventDefault(startAction, l);

        // should never be complete but follow the pattern
        if (startAction != action)
          mMobius->completeAction(startAction);
			
		// Ugly: when recording from a script, we often have latency
		// disabled and want to start right away.  mFrame will
		// currently be -InputLatency but we'll set it to zero as
		// soon as the event is processed.  Unfortunately if we setup
		// a script Wait, it will be done relative to -InputLatency.
		// Try to detect this and preemtively set the frame to zero.
		// !! does the source matter, do this always?
		if (action->trigger == TriggerScript) {
			long frame = l->getFrame();
			if (frame == event->frame) {
				l->setFrame(0);
				l->setPlayFrame(0);
				event->frame = 0;
			}
		}

        // if trigger was AutoRecord schedule a stop event
        if (f == AutoRecord) {
            // we'll do this below for the primary event, but for AutoRecord
            // need it on both
            if (action->arg.getType() == EX_STRING &&
                StringEqualNoCase(action->arg.getString(), "noFade"))
              event->fadeOverride = true;

            event = scheduleRecordStop(action, l);
        }

		// If we're in Reset, we have to pretend we're in Play
		// in order to get the frame counter started.  Otherwise
		// leave the current mode in place until RecordEvent.  Note that
		// this MUST be done after scheduleStop because decisions
		// are made based on whether we're in Reset mode 
		// (see Synchronizer::d;getSyncMode)

		if (mode == ResetMode)
		  l->setMode(PlayMode);
    }

	// Script Kludge: If we're in a script context with this
	// special flag set, set yet another kludgey flag on the event
	// that will set a third kludgey option in the Layer to suppress
	// the next fade.  
	if (event != NULL && 
        action->arg.getType() == EX_STRING &&
        StringEqualNoCase(action->arg.getString(), "noFade"))
	  event->fadeOverride = true;

	return event;
}

/**
 * Called by RecordFunction::scheduleEvent to see if the start of a recording
 * needs to be synchronized.  When true it usually means that the start of
 * the recording needs to wait for a synchronization pulse and the
 * end may either need to wait for a pulse or be scheduled for an exact time.
 *
 * !! Need to support an option where we start recording immediately then
 * round off at the end.
 *
 * !! Should just always call Synchronizer to start the recording and 
 * let it have the logic.
 */
PUBLIC bool Synchronizer::isRecordStartSynchronized(Loop* l)
{
	bool sync = false;
    
    Track* track = l->getTrack();
    SyncState* state = track->getSyncState();

    // note that we use getEffectiveSyncSource to factor
    // in the master tracks
    SyncSource src = state->getEffectiveSyncSource();

    sync = (src == SYNC_MIDI || src == SYNC_HOST || src == SYNC_TRACK);
        
	return sync;
}

/**
 * Return true if we need to enter threshold detection mode
 * before recording.  Threshold recording is disabled if there
 * is any form of slave sync enabled.
 *
 * !! I can see where it would be useful to have
 * a threshold on the very first loop record, but then disable it
 * for things like AutoRecord=On since we'll already
 * have momentum going.
 */
PRIVATE bool Synchronizer::isThresholdRecording(Loop* l)
{
	bool threshold = false;

	Preset* p = l->getPreset();
	if (p->getRecordThreshold() > 0) {
		Synchronizer* sync = l->getSynchronizer();
		threshold = !sync->isRecordStartSynchronized(l);
	}

	return threshold;
}

/**
 * Helper for Synchronize and Threshold modes.
 * Schedule a pending Record event and optionally a RecordStop event
 * if this is an AutoRecord.
 */
PRIVATE Event* Synchronizer::schedulePendingRecord(Action* action,
                                                   Loop* l,
                                                   MobiusMode* mode)
{
    Event* event = NULL;
    EventManager* em = l->getTrack()->getEventManager();
	Preset* p = l->getPreset();
    Function* f = action->getFunction();

	l->setMode(mode);

	event = em->newEvent(f, RecordEvent, 0);
	event->pending = true;
	event->savePreset(p);
	em->addEvent(event);

    // For AutoRecord we could want on the start or the stop.
    // Seems reasonable to wait for the stop, this must be in sync
    // with what scheduleRecordStart does...

    if (f != AutoRecord) {
        action->setEvent(event);
    }
    else {
		// Note that this will be scheduled for the end frame,
		// but the loop isn't actually recording yet.  That's ok, 
		// it is where we want it when we eventually do start recording.\
        // Have to clone the action since it is already owned by RecordEvent.
        Mobius* m = l->getMobius();
        Action* startAction = m->cloneAction(action);
        startAction->setEvent(event);

        // scheduleRecordStop will take ownership of the action
        // !! this may return null in which we should have allowed 
        // the original Action to own the start event
        event = scheduleRecordStop(action, l);
	}

	return event;
}

/****************************************************************************
 *                                                                          *
 *                           RECORD STOP SCHEDULING                         *
 *                                                                          *
 ****************************************************************************/

/**
 * Return true if a recording will be stopped by the Synchronizer 
 * after a sync pulse is received.  Returns false if the recording will
 * be stopped on a specific frame calculated from the sync tempo, or if this
 * is an unsynchronized recording that will stop normally.
 *
 * Note that this does not have to return the same value as
 * isRecordStartSynchronzied.
 */
PRIVATE bool Synchronizer::isRecordStopPulsed(Loop* l)
{
    bool pulsed = false;

    Track* t = l->getTrack();
    SyncState* state = t->getSyncState();
    SyncSource src = state->getEffectiveSyncSource();

    if (src == SYNC_TRACK) {
        // always pulsed
        pulsed = true;
    }
    else if (src == SYNC_HOST || src == SYNC_MIDI) {
        // we pulse if the tracker is locked, otherwise schedule
        SyncTracker* tracker = getSyncTracker(src);
        pulsed = tracker->isLocked();

        // !! Not supporting this old option any more, weed this out
        if (!pulsed && src == SYNC_MIDI && 
            (mMidiRecordMode == MIDI_RECORD_PULSED))
          pulsed = true;
    }

	return pulsed;
}

/**
 * Decide how to end Record mode.
 * Normally thigns like this would stay in the Function subclass but
 * recording is so tightly related to synchronization that we keep 
 * things over here.
 *
 * Called by RecordFunction from its scheduleModeStop method.
 * Indirectly called by Function::invoke whenever we're in Record mode
 * and a function si recieved that wants to change modes. This will be called
 * from a function handler, not an event handler.
 *
 * Called by LoopTriggerFunction::scheduleTrigger, 
 * RunScriptFunction::invoke, and TrackSelectFunction:;invoke, via
 * RecordFunction::scheduleModeStop.
 * 
 * In the simple case, we schedule a RecordStopEvent delayed by
 * InputLatency and begin playing.  The function that called this is
 * then free to schedule another event, usually immediately after
 * the RecordStopEvent.  
 *
 * If we're synchronizing, the end of the recording is delayed
 * to a beat or bar boundary defined by the synchronization mode.
 * There are two ways to determine when where this boundary is:
 *
 *   - waiting until we receive a number of sync pulses
 *   - calculating the end frame based on the sync tempo
 *
 * Waiting for sync pulses is used in sync modes where the pulses
 * are immune to jitter (track sync, tracker sync, host sync).  
 * Calculating a specific end frame is used when the pulses are not
 * stable (MIDI sync).
 *
 * If we use the pulse waiting approach, the RecordStopEvent is marked
 * pending and Synchronizer will activate it when the required number
 * of pulses are received.
 *
 * If we calculate a specific end frame, the event will not be pending.
 *
 * If we're using one of the bar sync modes, or we're using AutoRecord,
 * the stop event could be scheduled quite far into the future.   While
 * we're waiting for the stop event, further presses of Record and Undo 
 * can be used to increase or decrease the length of the recording.
 *
 * NOTE: If we decide to schedule the event far enough in the future, there
 * is opportunity to schedule a JumpPlayEvent to begin playback without
 * an output latency jump.
 *
 */
PUBLIC Event* Synchronizer::scheduleRecordStop(Action* action, Loop* loop)
{
    Event* event = NULL;
    EventManager* em = loop->getTrack()->getEventManager();
    Event* prev = em->findEvent(RecordStopEvent);
    MobiusMode* mode = loop->getMode();
    Function* function = action->getFunction();

    if (prev != NULL) {
        // Since the mode doesn't change until the event is processed, we
        // can get here several times as functions are stacked for
        // evaluation after the stop.  This is common for AutoRecord.
        Trace(loop, 2, "Sync: RecordStopEvent already scheduled\n");
        event = prev;
    }
    else if (mode != ResetMode &&
             mode != SynchronizeMode &&
             mode != RecordMode &&
             mode != PlayMode) {

        // For most function handlers we must be in Record mode.
        // For the Record function, we expect to be in Record, 
        // Reset or Synchronize modes.  For AutoRecord we may be 
        // in Play mode.

        Trace(loop, 1, "Sync: Attempt to schedule RecordStop in mode %s!\n",
              mode->getName());
    }
    else {
        // Pressing Record during Synchronize mode is handled the same as
        // an AutoRecord, except that the bar length is limited to 1 rather
        // than using the RecordBars parameter.

        bool scheduleEnd = true;

        if (function == AutoRecord || 
            (function == Record && mode == SynchronizeMode)) {

            // calculate the desired length, the second true argument says	
            // extend to a full bar if we're using a beat sync mode
            float barFrames;
            int bars;
            getAutoRecordUnits(loop, &barFrames, &bars);

            // Only one bar if not using AutoRecord
            if (function != AutoRecord)
              bars = 1;

            if (isRecordStopPulsed(loop)) {
                // Schedule a pending event and wait for a pulse.
                // Ignore the bar frames but remember the bar count
                // so we knows how long to wait.
                // Use the actual invoking function so we know
                // Record vs AutoRecord.
                event = em->newEvent(function, RecordStopEvent, 0);
                event->pending = true;
                event->number = bars;

                Trace(loop, 2, "Sync: Added pulsed Auto RecordStop after %ld bars\n",
                      (long)bars);
            }
            else if (barFrames <= 0.0) {
                // if there isn't a valid bar length in the preset, just
                // ignore it and behave like an ordinary Record
                Trace(loop, 2, "Sync: No bar length defined for AutoRecord\n");

                if (mode == SynchronizeMode) {
                    // Hmm, not sure what to do here, could cancel the recording
                    // or just ignore it?
                    Trace(loop, 2, "Sync: Ignoring Record during Synchronize mode\n");
                    scheduleEnd = false;
                }
                else if (mode == PlayMode) {
                    // We must be in that brief latency delay period
                    // before the recording starts?  Old logic prevents
                    // scheduling in this mode, not exactly sure why.
                    Trace(loop, 2, "Sync: Ignoring Record during Play mode\n");
                    scheduleEnd = false;
                }
            }
            else {
                // we know how long to wait, schedule the event
                event = em->newEvent(function, RecordStopEvent, 0);
                event->quantized = true;	// just so it is visible

                // calculate the stop frame from the barFrames and bars
                setAutoStopEvent(action, loop, event, barFrames, bars);

                Trace(loop, 2, "Sync: Scheduled auto stop event at frame %ld\n",
                      event->frame);
            }
        }

        // If we didn't schedule an AutoRecord event, and we didn't detect
        // an AutoRecord scheduling error, procees with normal scheduling
        if (event == NULL && scheduleEnd) {

            // if the start was synchronized, so too the end
            if (isRecordStartSynchronized(loop)) {

                event = scheduleSyncRecordStop(action, loop);
            }
            else {
                // !! legacy comment from stopInitialRecording, not sure
                // if we really need this?
                // with scripts, its possible to have a Record stop before
                // we've actually made it to recordEvent and create the
                // record layer
                Layer* layer = loop->getRecordLayer();
                if (layer == NULL) {
                    LayerPool* pool = mMobius->getLayerPool();
                    loop->setRecordLayer(pool->newLayer(loop));
                    loop->setFrame(0);
                    loop->setPlayFrame(0);
                }

                // Nothing to wait for except input latency
                long stopFrame = loop->getFrame();
                bool doInputLatency = !action->noLatency;
                if (doInputLatency)
                  stopFrame += loop->getInputLatency();

                // Must use Record function since the invoking function
                // can be anything that ends Record mode.
                event = em->newEvent(Record, RecordStopEvent, stopFrame);
                // prepare the loop early so we can beging playing

                loop->prepareLoop(doInputLatency, 0);

                Trace(loop, 2, "Sync: Scheduled RecordStop at %ld\n",
                      event->frame);
            }
        }

        if (event != NULL) {
            // take ownership of the Action
            action->setEvent(event);
            event->savePreset(loop->getPreset());
            em->addEvent(event);
        }
    }

    return event;
}

/**
 * Called whenever the Record or AutoRecord function
 * is pressed again after we have already scheduled a RecordStopEvent.
 *
 * For AutoRecord we push the stop event out by the number of bars
 * set in the RecordBars parameter.
 *
 * For Record during synchronize mode we push it out by one bar.
 *
 * For Record during Record mode (we're waiting for the final pulse)
 * we push it out by one "unit".  Unit may be either a bar or a beat.
 * 
 */
PUBLIC void Synchronizer::extendRecordStop(Action* action, Loop* loop, 
                                           Event* stop)
{
	// Pressing Record during Synchronize mode is handled the same as
	// an AutoRecord, except that the bar length is limited to 1 rather
	// than using the RecordBars parameter.
    Function* function = action->getFunction();

    if (function == AutoRecord || 
        (function == Record && loop->getMode() == SynchronizeMode)) {

		// calculate the desired length
		float barFrames;
		int bars;
		getAutoRecordUnits(loop, &barFrames, &bars);

		// Only one bar if not using AutoRecord
		if (function != AutoRecord)
		  bars = 1;

		int newBars = stop->number + bars;

		if (isRecordStopPulsed(loop)) {
			// ignore the frames, but remember bars, 
			stop->number = newBars;
		}
		else if (barFrames <= 0.0) {
			// If there isn't a valid bar length in the preset, just
			// ignore it and behave like an ordinary Record.  
			// Since we've already scheduled a RecordStopEvent, just
			// ignore the extra Record.

			Trace(loop, 2, "Sync: Ignoring Record during Synchronize mode\n");
		}
		else {
			setAutoStopEvent(action, loop, stop, barFrames, newBars);
		}

        // !! Action should take this so a script can wait on it
    }
    else {
        // normal recording, these can't be extended
		Trace(loop, 2, "Sync: Ignoring attempt to extend recording\n");
	}
}

/**
 * Called from RecordFunction::undoModeStop.
 *
 * Check if we are in an AutoRecord that has been extended 
 * beyond one "unit" by pressing AutoRecord again during the 
 * recording period.  If so, remove units if we haven't begun recording
 * them yet.  
 *
 * If we can't remove any units, then let the undo remove
 * the RecordStopEvent which will effectively cancel the auto record
 * and you have to end it manually.  
 *
 * Q: An interesting artifact will
 * be that the number of cycles in the loop will be left at
 * the AutoRecord bar count which may not be what we want.
 */
PUBLIC bool Synchronizer::undoRecordStop(Loop* loop)
{
	bool undone = false;
    EventManager* em = loop->getTrack()->getEventManager();
	Event* stop = em->findEvent(RecordStopEvent);

	if (stop != NULL &&
		((stop->function == AutoRecord) ||
		 (stop->function == Record && isRecordStartSynchronized(loop)))) {
		
		// calculate the unit length
		float barFrames;
		int bars;
		getAutoRecordUnits(loop, &barFrames, &bars);

		// Only one bar if not using AutoRecord
		// this must match what we do in extendRecordStop
		if (stop->function != AutoRecord)
		  bars = 1;

		int newBars = stop->number - bars;
		long newFrames = (long)(barFrames * (float)newBars);

		if (newFrames < loop->getFrame()) {
			// we're already past this point let the entire event
			// be undone
		}
		else {
			undone = true;
			stop->number = newBars;

			if (!isRecordStopPulsed(loop)) {
				stop->frame = newFrames;

				// When you schedule stop events on specific frames, we have
				// to set the loop cycle count since Synchronizer is no
				// longer watching.
				loop->setRecordCycles(newBars);
			}
		}
	}

	return undone;
}

/**
 * For an AutoRecord, return the number of frames in one bar and the number
 * of bars to record.   This is used both for scheduling the initial
 * record ending, as well as extending or decreasing an existing ending.
 *
 * If pulsing the recording ending then the frames calculated here 
 * will be ignored.
 * 
 * For auto record, we always want to record a multiple of a bar,
 * even when Sync=MIDIBeat or Sync=HostBeat.  If you want to autorecord
 * a single beat you have to turn down RecordBeats to 1.
 * !! REALLY?  It seems better to let the Sync mode determine this?
 *
 * !! This is an ugly interface, look at callers and see if they can either
 * just to bar counts or frames by calling getRecordUnit directly.
 */
PRIVATE void Synchronizer::getAutoRecordUnits(Loop* loop,
                                              float* retFrames,
                                              int* retBars)
{
    Preset* preset = loop->getPreset();
    int bars = preset->getAutoRecordBars();
    if (bars <= 0) bars = 1;

    SyncUnitInfo unit;
    getRecordUnit(loop, &unit);

	if (retFrames != NULL) *retFrames = unit.adjustedFrames;
	if (retBars != NULL) *retBars = bars;
}

/**
 * Helper for scheduleRecordStop and extendRecordStop.
 * Given the length of a bar in frames and a number of bars to record,
 * calculate the total number of frames and put it in the event.
 * This is only used for AutoRecord.
 */
PRIVATE void Synchronizer::setAutoStopEvent(Action* action, Loop* loop, 
                                            Event* stop, 
                                            float barFrames, int bars)
{
	// multiply by bars and round down
	long totalFrames = (long)(barFrames * (float)bars);

    MobiusMode* mode = loop->getMode();
	if (mode == RecordMode) {
		// we're scheduling after we started
		long currentFrame = loop->getFrame();
		if (currentFrame > totalFrames) {
			// We're beyond the point where would
			// have normally stopped, act as if the
			// auto-record were extended.

			int moreBars;
			if (action->getFunction() == AutoRecord) {
                Preset* p = loop->getPreset();
				moreBars = p->getAutoRecordBars();
				if (moreBars <= 0) moreBars = 1;
			}
			else {
				// must be Record during Synchronize, advance by one bar
				moreBars = 1;
			}

			while (currentFrame > totalFrames) {
				bars += moreBars;
				totalFrames = (long)(barFrames * (float)bars);
			}
		}
	}

	stop->number = bars;
	stop->frame = (long)totalFrames;

	// When you schedule stop events on specific frames, we have to set
	// the loop cycle count since Synchronizer is no longer watching.
	loop->setRecordCycles(bars);
}

/**
 * Called by scheduleRecordStop when a RecordStop event needs to be 
 * synchronized to a pulse or pre-scheduled based on tempo.
 *
 * Returns the RecordStop event or NULL if it was not scheduled for some 
 * reason.
 *         
 * Action ownership is handled by the caller
 */
PRIVATE Event* Synchronizer::scheduleSyncRecordStop(Action* action, Loop* l)
{
    Event* stop = NULL;
    Function* function = action->getFunction();
    EventManager* em = l->getTrack()->getEventManager();

    if (isRecordStopPulsed(l)) {
        // schedule a pending RecordStop and wait for the pulse
        // syncPulseRecording will figure out which pulse to stop on
        // must force this to use Record since the action function
        // can be anyhting
        stop = em->newEvent(Record, RecordStopEvent, 0);
        stop->pending = true;

        Trace(l, 2, "Sync: Added pulsed RecordStop\n");
    }
    else {
        // Should only be here for SYNC_MIDI but the logic is more general
        // than it needs to be in case we want to do this for other modes.
        // Things like this will be necessary if we want to support immediate
        // recording with rounding.

        // Calculate the base unit size, this will represent either a beat
        // or bar depending on sync mode.
        SyncUnitInfo unit;
        getRecordUnit(l, &unit);

        float unitFrames = unit.adjustedFrames;
        long loopFrames = l->getFrame();

        if (unitFrames == 0.0f) {
            // should never happen, do something so we can end the loop
            Trace(l, 1, "Sync: unitFrames zero!\n");
            unitFrames = (float)loopFrames;
        }

        long units = (long)((float)loopFrames / unitFrames);

        if (loopFrames == 0) {
            // should never happen, isn't this more severe should
            // we even be scheduling a StopEvent??
            Trace(l, 1, "Sync: Scheduling record end with empty loop!\n");
            units = 1;
        }
        else {
            // now we need to round up to the next granule
            // !! will float rounding screw us here? what if remainder
            // is .00000000001, may be best to truncate this 
            float remainder = fmodf((float)loopFrames, unitFrames);
            if (remainder > 0.0) {
                // we're beyond the last boundary, add another
                units++;
            }
        }

        long stopFrame = (long)((float)units * unitFrames);

        Trace(l, 2, "Sync: Scheduled RecordStop currentFrames %ld unitFrames %ld units %ld stopFrame %ld\n",
              loopFrames, (long)unitFrames, (long)units, stopFrame);
        

        // sanity check
        if (stopFrame < loopFrames) {
            Trace(l, 1, "Sync: Record end scheduling underflow %ld to %ld\n",
                  stopFrame, loopFrames);
            stopFrame = loopFrames;
        }
        
        // !! think about scheduling a PrepareRecordStop event
        // so we close off the loop and begin preplay like we do
        // when the end isn't being synchronized
        stop = em->newEvent(Record, RecordStopEvent, stopFrame);
        // so we see it
        stop->quantized = true;

        // remember the unadjusted tracker frames and pulses
        long trackerFrames = (long)((float)units * unit.frames);
        int trackerPulses = unit.pulses * units;

        Track* t = l->getTrack();
        SyncState* state = t->getSyncState();
        state->scheduleStop(trackerPulses, trackerFrames);

        // Once the RecordStop event is not pending, syncPulseRecording
        // will stop trying to calculate the number of cycles, we have
        // to set the final cycle count.
        // !! does this need to be speed adjusted?
        int cycles = (int)(unit.cycles * (float)units);
        if (cycles == 0) {
            Trace(l, 1, "Sync: something hootered with cycles!\n");
            cycles = 1;
        }
        l->setRecordCycles(cycles);

        Trace(l, 2, "Sync: scheduleRecorStop trackerPulses %ld trackerFrames %ld cycles %ld\n",
              (long)trackerPulses, (long)trackerFrames, (long)cycles);
    }

    return stop;
}

/**
 * Helper for scheduleRecordStop and others, calculate the properties of
 * one synchronization "unit".  A synchronized loop will normally have a 
 * length that is a multiple of this unit.
 * 
 * For SYNC_OUT this is irrelevant because we only calculate this when 
 * slaving and once the out sync master is set all others use SYNC_TRACK.
 * 
 * For SYNC_TRACK a unit is the master track subcycle, cycle, or loop.
 * Pulses are the number of subcycles in the returned unit but that isn't
 * actually used.
 *
 * For SYNC_HOST a unit will be the width of one beat or bar calculated
 * from the host tempo.  In theory the tracker is also monitoring the average
 * pulse width and we could work from there, but I think its better to
 * use what the host says the ideal tempo will be.  Since we're pulsing both
 * the start and end this isn't especially important but it will be if we
 * allow unquatized edges and have to calculate the length.
 * 
 * For SYNC_MIDI we drive the unit from the smoothed tempo calculated by
 * MidiInput.  SyncTracker also has an average pulse width but working from
 * the tempo is more accurate.  Should compare someday...
 *
 * If the HOST or MIDI SyncTrackers are locked, we let those decide the
 * width of the unit so that we always match them exactly.  This is less
 * important now since once the trackers are locked we always pulse the
 * record end with a tracker pulse and don't use the frame size calculated
 * here.  But once we allow unquantized record starts and can't pulse
 * the end we'll need an accurate tracker unit returned here.
 *
 */
PRIVATE void Synchronizer::getRecordUnit(Loop* l, SyncUnitInfo* unit)
{
    Track* t = l->getTrack();
    SyncState* state = t->getSyncState();
    SyncTracker* tracker = NULL;

    // note that this must be the *effective* source
    SyncSource src = state->getEffectiveSyncSource();

	unit->frames = 0.0f;
    unit->pulses = 1;
    unit->cycles = 1.0f;
    unit->adjustedFrames = 0.0f;

    if (src == SYNC_TRACK) {
        Loop* masterLoop = mTrackSyncMaster->getLoop();
        Preset* p = masterLoop->getPreset();
        int subCycles = p->getSubcycles();
        SyncTrackUnit tsunit = state->getSyncTrackUnit();

        switch (tsunit) {
            case TRACK_UNIT_LOOP: {
                int cycles = masterLoop->getCycles();
				unit->frames = (float)masterLoop->getFrames();
                unit->pulses = cycles * subCycles;
                unit->cycles = (float)cycles;
            }
            break;
            case TRACK_UNIT_CYCLE: {
				unit->frames = (float)masterLoop->getCycleFrames();
                unit->pulses = subCycles;
            }
			break;
			case TRACK_UNIT_SUBCYCLE: {
				// NOTE: This could result in a fractional value if the
				// number of subcyles is odd.  The issues here
                // are similar to those in SyncTracker when determining
                // beat boundaries.
				long cycleFrames = masterLoop->getCycleFrames();
				unit->frames = (float)cycleFrames / (float)subCycles;
                unit->cycles = 1.0f / (float)subCycles;
                
                long iframes = (long)unit->frames;
                if ((float)iframes != unit->frames)
                  Trace(2, "Sync: WARNING Fractional track sync subcycle %ld (x100)\n",
                        (long)(unit->cycles * 100));
            }
            break;
			case TRACK_UNIT_DEFAULT:
				// xcode 5 whines if this isn't here
				break;
        }
    }
    else if (src == SYNC_HOST) {
        if (mHostTracker->isLocked()) {
            // we've already locked the beat length, normally this
            // will have been rounded before locking so we won't have a fraction
            unit->frames = mHostTracker->getPulseFrames();
        }
        else {
            // NOTE: Should we use what the host tells us or what we measured
            // in tye SyncTracker?  Assuming we should follow the host.
            traceTempo(l, "Host", mHostTempo);
            unit->frames = getFramesPerBeat(mHostTempo);
        }

        adjustBarUnit(l, state, src, unit);
    }
    else if (src == SYNC_MIDI) {
        if (mMidiTracker->isLocked()) {
            // We've already locked the pulse length, this may have a fraction
            // but normally we will round it up so that when multiplied by 24
            // the resulting beat width is integral
            float pulseFrames = mMidiTracker->getPulseFrames();
            unit->frames = pulseFrames * (float)24;
        }
        else {
            // Two tempos to choose from, the average tempo and
            // a smoothed tempo rounded down to a 1/10th.  
            // We have an internal parameter to select the mode, figure
            // out the best one and stick with it!

            float tempo = mMidi->getInputTempo();
            traceTempo(l, "MIDI", tempo);

            int smooth = mMidi->getInputSmoothTempo();
            float fsmooth = (float)smooth / 10.0f;
            traceTempo(l, "MIDI smooth", fsmooth);

            float frames = getFramesPerBeat(tempo);
            float sframes = getFramesPerBeat(fsmooth);

            Trace(l, 2, "Sync: getRecordUnit average frames %ld smooth frames %ld\n",
                  (long)frames, (long)sframes);
        
            if (mMidiRecordMode == MIDI_TEMPO_AVERAGE)
              unit->frames = frames;
            else
              unit->frames = sframes;
        }

        adjustBarUnit(l, state, src, unit);

        // NOTE: sync pulses are actually clocks so multiply by 24
        unit->pulses *= 24;
    }
    else {
        // NONE, OUT
        // only here for AutoRecord, we control the tempo
        // the unit size is one bar
        Preset* p = t->getPreset();
        float tempo = (float)p->getAutoRecordTempo();
        traceTempo(l, "Auto", tempo);
        unit->frames = getFramesPerBeat(tempo);

        // !! do we care about the OUT tracker for SYNC_NONE?
        // formerly got BeatsPerBar from a preset parameter, now it 
        // comes from the setup so all sync modes can use it consistently
        //int bpb = p->getAutoRecordBeats();
        int bpb = getBeatsPerBar(src, l);

        if (bpb <= 0)
          Trace(l, 1, "ERROR: Sync: BeatsPerBar not set, assuming 1\n");
        else {
            unit->pulses = bpb;
            unit->frames *= (float)bpb;
        }
    }

    Trace(l, 2, "Sync: getRecordUnit %s frames %ld pulses %ld cycles %ld\n",    
          GetSyncSourceName(src), (long)unit->frames, (long)unit->pulses, 
          (long)unit->cycles);

    // NOTE: This could result in a fractional value if the
    // number of subcyles is odd, we won't always handle this well.
    // This can also happen with fractional MIDI tempos and proabbly
    // host tempos.  We may need to round down here...
    float intpart;
    float frac = modff(unit->frames, &intpart);
    if (frac != 0) {
        // supported but it will cause problems...
        Trace(l, 2, "WARNING: Sync: getRecordUnit non-integral unit frames %ld fraction %ld\n",
              (long)unit->frames, (long)frac);
    }

    // factor in the speed
    float speed = getSpeed(l);
    if (speed == 1.0)
      unit->adjustedFrames = unit->frames;
    else {
        // !! won't this have the same issues with tracker rounding?
        unit->adjustedFrames = unit->frames * speed;
        Trace(l, 2, "Sync: getRecordUnit speed %ld (x100) adjusted frames %ld (x100)\n",
              (long)(speed * 100), (long)(unit->adjustedFrames * 100));
    }

}

PRIVATE float Synchronizer::getSpeed(Loop* l)
{
    InputStream* is = l->getInputStream();
    return is->getSpeed();
}

PRIVATE void Synchronizer::traceTempo(Loop* l, const char* type, float tempo)
{
    long ltempo = (long)tempo;
    long frac = (long)((tempo - (float)ltempo) * 100);
    Trace(l, 2, "Sync: getRecordUnit %s tempo %ld.%ld\n", type, ltempo, frac);
}

/**
 * Helper for getRecordUnit.  Convert a tempo in beats per minute
 * into framesPerBeat.
 * 
 * Optionally truncate fractions so we can always deal with integer
 * beat lengths which is best for inter-track sync although it
 * may produce more drift relative to the host.
 */
PRIVATE float Synchronizer::getFramesPerBeat(float tempo)
{
    float beatsPerSecond = tempo / 60.0f;

    float framesPerSecond = (float)mMobius->getSampleRate();

    float fpb = framesPerSecond / beatsPerSecond;

    if (!mNoSyncBeatRounding) {
        int ifpb = (int)fpb;
        if ((float)ifpb != fpb) 
          Trace(2, "Sync: Rouding framesPerBeat for tempo %ld (x100) from %ld (x100) to %ld\n", 
                (long)(tempo * 100), (long)(fpb * 100), (long)ifpb);
        fpb = (float)ifpb;
    }
    
    return fpb;
}

/**
 * Helper for getRecordUnit.
 * After calculating the beat frames, check for bar sync and multiply
 * the unit by beats per bar.
 *
 * !! Sumething looks funny about this.  getBeatsPerBar() goes out
 * and gets the SyncTracker but state also captured it.  Follow this
 * mess and make sure if the tracker isn't locked we get it from the state.
 */
PRIVATE void Synchronizer::adjustBarUnit(Loop* l, SyncState* state, 
                                         SyncSource src,
                                         SyncUnitInfo* unit)
{
    int bpb = getBeatsPerBar(src, l);
    if (state->getSyncUnit() == SYNC_UNIT_BAR) {
        if (bpb <= 0)
          Trace(l, 1, "ERROR: Sync: BeastPerBar not set, assuming 1\n");
        else {
            unit->pulses = bpb;
            unit->frames *= (float)bpb;
        }
    }
    else {
        // one bar is one cycle, but if the unit is beat should
        // we still use BeatsPerBar to calculate cycles?
        if (bpb > 0)
          unit->cycles = 1.0f / (float)bpb;
    }
}

/****************************************************************************
 *                                                                          *
 *                              AUDIO INTERRUPT                             *
 *                                                                          *
 ****************************************************************************/

/**
 * Called by Mobius at the beginning of a new audio interrupt.
 * Convert raw events recieved since the last interrupt into a list
 * of Event object we can feed into each track's event list.
 *
 * Host events may have an offset within the current buffer.  MIDI and Timer
 * events are always processed at the beinning of the buffer since they
 * have already happened and we need to catch up ASAP.  
 *
 * TODO: Eventually try to be smarter about buffer quantization.
 * The events are always queued and being processed late so we must
 * handle them at the beginning of the interrupt.  But the delay could
 * factor in to some calculations like input latency delay.  
 *
 *    effectiveInputLatency = inputLatency - triggerLatency
 *
 * Where triggerLatency is defined by the physical trigger latency 
 * (around 1ms for MIDI) plus buffering latency, which will be up to
 * the interrupt block size.  See looptime.txt for a more thorough explanation.
 *
 */
PUBLIC void Synchronizer::interruptStart(AudioStream* stream)
{
    Event* events = NULL;
    Event* event = NULL;
    Event* next = NULL;

    // capture some statistics
	mLastInterruptMsec = mInterruptMsec;
	mInterruptMsec = mMidi->getMilliseconds();
	mInterruptFrames = stream->getInterruptFrames();

    // should be empty but make sure
    flushEvents();
    mNextAvailableEvent = NULL;

    // tell the trackers to prepare for an interrupt
    mMidiTracker->interruptStart();
    mHostTracker->interruptStart();
    mOutTracker->interruptStart();

    // external MIDI events
    // note we'll get UNIT_BEAT events here, to detect UNIT_BAR we have
    // to apply BeatsPerBar from the Setup
    // NOTE: in theory BPB can be track specific if we fall back to the Preset
    // that would mean we have to recalculate the pulses for every Track,
    // I really dont' think that's worth it
    EventPool* pool = mMobius->getEventPool();
    int bpb = getInBeatsPerBar();
    events = mMidiQueue.getEvents(pool, mInterruptFrames);
    next = NULL;
    for (event = events ; event != NULL ; event = next) {
        next = event->getNext();
        event->setNext(NULL);

        event->fields.sync.source = SYNC_MIDI;

        if (event->fields.sync.eventType == SYNC_EVENT_PULSE && 
            event->fields.sync.pulseType == SYNC_PULSE_BEAT &&
            bpb > 0) {

            if ((event->fields.sync.beat % bpb) == 0)
              event->fields.sync.pulseType = SYNC_PULSE_BAR;
        }

        // else if SYNC_EVENT_START can assume BAR later,
        // SYNC_EVENT_CONTINUE will have sync.pulseType set

        // Pass through the SyncTracker to for annotations
        mMidiTracker->event(event);

        mInterruptEvents->insert(event);
    }

    // internal MIDI events
    bpb = getOutBeatsPerBar();
    events = mTransport->getEvents(pool, mInterruptFrames);
    next = NULL;
    for (event = events ; event != NULL ; event = next) {
        next = event->getNext();
        event->setNext(NULL);

        event->fields.sync.source = SYNC_OUT;

        if (event->fields.sync.eventType == SYNC_EVENT_PULSE && 
            event->fields.sync.pulseType == SYNC_PULSE_BEAT &&
            bpb > 0) {

            if ((event->fields.sync.beat % bpb) == 0)
              event->fields.sync.pulseType = SYNC_PULSE_BAR;
        }

        mOutTracker->event(event);

        mInterruptEvents->insert(event);
    }

    // Host events
    // Unlike MIDI events which are quantized by the MidiQueue, these
    // will have been created in the *same* interrupt and will have frame
    // values that are offsets into the current interrupt.  These must
    // be maintained in order and interleaved with the loop events.

    // refresh host sync state for the status display in the UI thread
	AudioTime* hostTime = stream->getTime();
    if (hostTime == NULL) {
        // can this happen, reset everyting or leave it where it was?
        /*
        mHostTempo = 0.0f;
        mHostBeat = 0;
        mHostBeatsPerBar = 0;
        mHostTransport = false;
        mHostTransportPending = false;
        */
    }
    else {
        // similar jump detection in VstMobius, could we push  
        // that into AudioTimer?
        int lastBeat = mHostBeat;

		mHostTempo = (float)hostTime->tempo;
		mHostBeat = hostTime->beat;
        mHostBeatsPerBar = hostTime->beatsPerBar;
        
        // stop is always non-pulsed
        if (mHostTransport && !hostTime->playing) {
            event = pool->newEvent();
            event->type = SyncEvent;
            event->fields.sync.source = SYNC_HOST;
            event->fields.sync.eventType = SYNC_EVENT_STOP;
            // no boundary offset on these
            mHostTracker->event(event);
            // do these need propagation?
            mInterruptEvents->insert(event);
            mHostTransport = false;
        }
        else if (hostTime->playing && !mHostTransport)
          mHostTransportPending = true;

        // should this be an else with handling transport stop above ?
        // what about CONTINUE, will we always be on a boundary?
        if (hostTime->beatBoundary || hostTime->barBoundary) {

            event = pool->newEvent();
            event->type = SyncEvent;
            event->fields.sync.source = SYNC_HOST;
            event->frame = hostTime->boundaryOffset;

            // If the transport state changed or if we did not
            // advance the beat by one, assume we can do a START/CONTINUE.
            // This isn't critical but it's nice with host sync so we can 
            // reset the avererage pulse width calculator which may be
            // way off if we're jumping the host transport.  

            if (mHostTransportPending || ((lastBeat + 1) != mHostBeat)) {
                if (mHostBeat == 0) {
                    event->fields.sync.eventType = SYNC_EVENT_START;
                    event->fields.sync.pulseType = SYNC_PULSE_BAR;
                }
                else {
                    event->fields.sync.eventType = SYNC_EVENT_CONTINUE;
                    // continue pulse is the raw pulse not rounded for bars
                    event->fields.sync.continuePulse = mHostBeat;
                    if (hostTime->barBoundary)
                      event->fields.sync.pulseType = SYNC_PULSE_BAR;
                    else
                      event->fields.sync.pulseType = SYNC_PULSE_BEAT;
                }
                mHostTransport = true;
                mHostTransportPending = false;
            }
            else {
                event->fields.sync.eventType = SYNC_EVENT_PULSE;
                if (hostTime->barBoundary)
                  event->fields.sync.pulseType = SYNC_PULSE_BAR;
                else
                  event->fields.sync.pulseType = SYNC_PULSE_BEAT;
            }

            mHostTracker->event(event);
            mInterruptEvents->insert(event);
        }
    }

    // advance the audio frames of the trackers, these may also generate events
    // we don't care about OUT events since we always fall back to track sync

    mOutTracker->advance(mInterruptFrames, NULL, NULL);
    mHostTracker->advance(mInterruptFrames, pool, mInterruptEvents);
    mMidiTracker->advance(mInterruptFrames, pool, mInterruptEvents);

    // mark all of these as "owned" so the usually event processing
    // logic in Loop won't free them
    // actually now that we always return mReturnEvent we don't need
    // to own these
    for (event = mInterruptEvents->getEvents() ; event != NULL ; 
         event = event->getNext()) {
        event->setOwned(true);

        // sanity check, these must be processed in the current interrupt
        if (event->frame >= mInterruptFrames)
          Trace(1, "Sync: Sync event beyond edge of interrupt!\n");
    }

}


/**
 * Called as each Track is about to be processed.
 * Reset the sync event iterator.
 */
PUBLIC void Synchronizer::prepare(Track* t)
{
    mNextAvailableEvent = mInterruptEvents->getEvents();

    // this will be set by trackSyncEvent if we see boundary
    // events during this interrupt
    SyncState* state = t->getSyncState();
    state->setBoundaryEvent(NULL);
}

/**
 * Called after each track has finished processing.
 * We should have consumed every sync event that is relevant for this track.
 * If not, there could be float rounding issues in InputStream.
 *
 * Unfotunately we can't just test for mNextAvailableEvent != NULL because
 * getNextEvent doesn't advance it if it doesn't find any relevant events.
 * In theory this is so we can Loop events between two sync events that change
 * the sync source and therefore make events that might have been irrelevant
 * at the start of the interrupt relevant later.  I'm not sure this can
 * happen in practice. Think...
 */
PUBLIC void Synchronizer::finish(Track* t)
{
    if (mNextAvailableEvent != NULL) {
        
        SyncState* state = t->getSyncState();
        SyncSource src = state->getEffectiveSyncSource();

        int unused = 0;
        for (Event* e = mNextAvailableEvent ; e != NULL ; e = e->getNext()) {
            if (e->fields.sync.source == src) 
              unused++;
        }

        if (unused > 0)
          Trace(t, 1, "Sync: Finishing interrupt for track %ld with %ld unused sync events\n",
                (long)t->getDisplayNumber(), (long)unused);
    }
}

/**
 * Called when we're done with one audio interrupt.
 */
PUBLIC void Synchronizer::interruptEnd()
{
    // do drift correction at the end of each interrupt
    checkDrift();

    flushEvents();
    mNextAvailableEvent = NULL;
}

/**
 * As Tracks are processed and reach interesting sync boundaries,
 * Track will call back here so we can record them.  Currently
 * we're only interested in events from the one track
 * designated as the TrackSyncMaster.
 */
PUBLIC void Synchronizer::trackSyncEvent(Track* t, EventType* type, int offset)
{
    if (t == mTrackSyncMaster) {

        EventPool* pool = mMobius->getEventPool();
        Event* e = pool->newEvent();
        e->type = SyncEvent;
        e->fields.sync.source = SYNC_TRACK;
        e->fields.sync.eventType = SYNC_EVENT_PULSE;
        
        // the "frame" is the offset into the interrupt buffer,
        // loop will adjust this for its own relative position
        e->frame = offset;

        // convert event type to pulse type
        SyncPulseType pulse;
        if (type == LoopEvent) 
          pulse = SYNC_PULSE_LOOP;
        else if (type == CycleEvent) 
          pulse = SYNC_PULSE_CYCLE;
        else if (type == SubCycleEvent) 
          pulse = SYNC_PULSE_SUBCYCLE;

        else {
            // what the hell is this?
            Trace(t, 1, "Sync: Invalid master track sync event!\n");
            pulse = SYNC_PULSE_CYCLE;
        }
        
        e->fields.sync.pulseType = pulse;

        // So "Wait external" has defined behavior, consider the
        // external start point to be the master track start point
        e->fields.sync.syncStartPoint = (pulse == SYNC_PULSE_LOOP);

        // Remember this for Realign pulses
        Loop* masterLoop = mTrackSyncMaster->getLoop();
        e->fields.sync.pulseFrame = masterLoop->getFrame();

        // all events in mInterruptEvents must have this set!
        e->setOwned(true);

        mInterruptEvents->insert(e);
    }

    // In all cases store the event type in the SyncState so we know
    // we reached an interesting boundary during this interrupt.  
    // This is how we detect boundary crossings for checkDrift.
    SyncState* state = t->getSyncState();
    state->setBoundaryEvent(type);

}

/**
 * Return the next ordered sync event relevant for the given loop.
 * The caller may decide not to use this, in which case we keep
 * searching from this position on every call.  If the loop decides
 * to use it it will call useEvent() and we can begin searching
 * from the next event on the list.
 *
 * Relevance means the event sync source matches the effective sync
 * source of the track.  Note that we will usually be getting pairs
 * of pulse events from the same source once a tracker is locked, one
 * a "raw" event from the external clock and one internal event generated
 * by the tracker.  We could filter those here, but I'd rather defer
 * them to syncEvent() so we can think about them at their appropiate
 * location within the loop.
 * 
 */
PUBLIC Event* Synchronizer::getNextEvent(Loop* loop)
{
    Event* next = NULL;
    Event* relevant = NULL;

    if (mNextAvailableEvent != NULL) {
        Track* track = loop->getTrack();
        SyncState* state = track->getSyncState();
        SyncSource src = state->getEffectiveSyncSource();

        // move up the list until we find one of the type we're interested in
        relevant = mNextAvailableEvent;
        while (relevant != NULL && 
               relevant->fields.sync.source != src)
          relevant = relevant->getNext();
    }

    // Sigh, Stream wants to change the sync event frame to fit within
    // the loop being advanced.  But since we use the same events
    // for all tracks we don't want to lose the original buffer offset
    // that is stored in the frame.  We could burn another arg on the Event
    // for this, but it's safest just to return a copy that the caller can do
    // anything it wants to.  
    // !! ugh, I hate this,we have to remember to copy every sync related
    // field one at a time 
    if (relevant != NULL) {

        // do NOT call init() here, it clears owned among other things
        next = mReturnEvent;
        next->setNext(relevant->getNext());
        next->type = relevant->type;
        next->frame = relevant->frame;
        next->processed = false;
        next->fields.sync.source = relevant->fields.sync.source;
        next->fields.sync.eventType = relevant->fields.sync.eventType;
        next->fields.sync.pulseType = relevant->fields.sync.pulseType;
        next->fields.sync.pulseFrame = relevant->fields.sync.pulseFrame;
        next->fields.sync.beat = relevant->fields.sync.beat;
        next->fields.sync.continuePulse = relevant->fields.sync.continuePulse;
        next->fields.sync.millisecond = relevant->fields.sync.millisecond;
        next->fields.sync.syncStartPoint = relevant->fields.sync.syncStartPoint;
        next->fields.sync.syncTrackerEvent = relevant->fields.sync.syncTrackerEvent;
        next->fields.sync.pulseNumber = relevant->fields.sync.pulseNumber;
    }

    return next;
}

/**
 * Move the next available event pointer to the last one we returned
 * from getEvent().
 */
PUBLIC void Synchronizer::useEvent(Event* e)
{
    if (e != NULL) {
        if (e != mReturnEvent)
          Trace(1, "Sync:useEvent called with the wrong event!\n");
        mNextAvailableEvent = e->getNext();
    }
}

/**
 * NOTE: THis is not used and I never did get it working, but it
 * represents some thought in this direction and I want to keep it
 * around for awhile.
 * 
 * Given a MIDI sync event, calculate the offset into the interrupt
 * buffer near where this event occured.
 *
 * MidiEvents are timestamped with the millisecond timer before they
 * are sent, this is captured in the "clock"  field of the MidiSyncEvent
 * when it is moved to the MidiQueue, and then copied from MidiSyncEvent
 * to the "millisecond" field of the Event.
 *
 * We saved the millisecond counter at the beginning of the last interrupt
 * in mLastInterruptMsec.  The distance between these represents the 
 * location of the MIDI event within the last buffer.  We convert that 
 * distance in milliseconds to frames and leave that as the interrupt buffer
 * offset for the event in the current interrupt.
 *
 * A consesquence of this is that MIDI events are always processed 1 interrupt
 * later than they happened.  Resulting in a latency of around 5ms with
 * a 256 frame buffer.  If we slid all the events to the front of the buffer
 * rather than trying to offset them the response time would be better
 * on average, though more jittery and lead to worse inaccuracies in 
 * the recorded frame count, which results in more frequent drift adjustments.
 *
 * UPDATE: This is flakey.  There is a common overflow of 264 (8 frames) 
 * and less common underflows ranging from -1 to -30.  The underflows seem
 * to coincide with system load, such as dragging a window around.  Both
 * are disturbing and are probably due to PortAudio not calling the interupts
 * in close to "real" time.  May need to be using the "stream" time instead?
 *
 * The 264 overflow is probably just roudning since a buffer is not an even
 * multiple of msecs.
 */
PRIVATE void Synchronizer::adjustEventFrame(Loop* l, Event* e)
{
	long delta = e->fields.sync.millisecond - mLastInterruptMsec;
	long offset = 0;

	if (delta < 0) {
		// In theory this could happen if the msec timer rolled immediately
		// after creating the MidiEvent but before it made its way to 
		// our MidiQueue.  It should never be more than one though.
		// Well, it often is, see comments above.
		if (delta < -31 && l->getTrack()->getDisplayNumber() == 1)
		  Trace(l, 2, "Sync: Sync event offset underflow %ld!\n", delta);
	}
	else {
		// convert millisecond delta to frame offset
		float framesPerMsec = mMobius->getSampleRate() / 1000.0f;
		offset = (long)(framesPerMsec * (float)delta);

		if (offset >= mInterruptFrames) {
			// We don't have enough frames in this interrupt to hold 
			// the full offset.  This can happen if we're processing 
			// buffers more rapidly than in real time, which seems to happen
			// sometimes as PortAudio tries to make up for a previous
			// interrupt that took too long.
			
			if (offset > 264 && l->getTrack()->getDisplayNumber() == 1)
			  Trace(l, 2, "Sync: Sync event offset overflow %ld!\n", offset);
			offset = mInterruptFrames;
		}
	}

	e->frame = offset;
}

/****************************************************************************
 *                                                                          *
 *                               EVENT HANDLING                             *
 *                                                                          *
 ****************************************************************************/

/**
 * Called by Loop when it gets around to processing one of the
 * sync pseudo-events we insert into the event stream.
 *
 * Usually here for pulse events.  Call one of the three mode handlers.
 *
 * For pulse events we can get here from two places, first the "raw"
 * event that comes from the external source (host, midi, timer)
 * and one that can come from the SyncTracker after it has been locked
 * (currently only HOST and MIDI).
 *
 * Only one of these will be relevant to pass down to the lower levels
 * of pulse handling but we can allow any of them to be waited on in scripts.
 * 
 */
PUBLIC void Synchronizer::syncEvent(Loop* l, Event* e)
{
	SyncEventType type = e->fields.sync.eventType;
	Track* track = l->getTrack();

    // becomes true if the event represent a pulse we can take action on
    bool pass = false;

	if (type == SYNC_EVENT_STOP) {

		if (track->getDisplayNumber() == 1)
			Trace(l, 2, "Sync: Stop Event\n");
        
        // TODO: event script
        // I've had requests to let this become a Pause, but 
        // it seems more useful to keep going and realign on continue
	}
	else {
        // START, CONTINUE, or PULSE

        // trace in just the first track
        // start/continue would be a good place for an event script
        // actually don't trace, SyncTracker will already said enough
        if (type == SYNC_EVENT_START) {
            //if (track->getDisplayNumber() == 1)
            //Trace(l, 2, "Sync: Start Event\n");
            // TODO: event script
        }
        else if (type == SYNC_EVENT_CONTINUE) {
            //if (track->getDisplayNumber() == 1)
            //Trace(l, 2, "Sync: Continue Event\n");
            // TODO: event script
        }

        // sanity check, should have filtered events that the track
        // doesn't want
        SyncSource src = e->fields.sync.source;
        SyncState* state = track->getSyncState();
        SyncSource expecting = state->getEffectiveSyncSource();

        if (src != expecting) {
            Trace(l, 1, "Sync: Event SyncSource %s doesn't match Track %s!\n",
                  GetSyncSourceName(src),
                  GetSyncSourceName(expecting));
        }
        else {
            // Decide whether to watch raw or tracker pulses.
            // Yes, this could be shorter but I like commenting the
            // exploded logic to make it easier to understand.

            SyncTracker* tracker = getSyncTracker(src);
            if (tracker == NULL) {
                // Must be TRACK, these won't be duplicated
                pass = true;
            }
            else if (tracker == mOutTracker) {
                // we don't let this generate events, so always pass
                // raw timer events to the master track
                pass = true;
            }

            // MIDI or HOST
            else if (!tracker->isLocked()) {
                if (e->fields.sync.syncTrackerEvent) {
                    // This should only happen if there was a scheduled
                    // reset or a script that reset the loop and the tracker
                    // and it left some events behind.  Could have cleaned
                    // this up in unlockTracker but safer here.
                    Trace(l, 2, "Sync: Ignoring residual tracker event\n");
                }
                else {
                    // pulses always pass, start always passes, 
                    // but continue passes only if we went back exactly
                    // to a beat boundary
                    if (type == SYNC_EVENT_PULSE || 
                        e->fields.sync.pulseType == SYNC_PULSE_BEAT ||
                        e->fields.sync.pulseType == SYNC_PULSE_BAR)
                      pass = true;
                }
            }
            else if (l->isSyncRecording()) {
                // recording is special, even though the tracker is locked
                // we have to pay attention to whether it was locked
                // when the recording began because we can't switch sources
                // int the middle
                if (state->wasTrackerLocked()) {
                    // locked when we started and still locked
                    // only pass tracker events
                    pass = e->fields.sync.syncTrackerEvent;
                }
                else {
                    // not locked when we started but locked now, pass raw
                    if (!e->fields.sync.syncTrackerEvent && 
                        (type == SYNC_EVENT_PULSE || 
                         e->fields.sync.pulseType == SYNC_PULSE_BEAT ||
                         e->fields.sync.pulseType == SYNC_PULSE_BAR))
                      pass = true;
                }
            }
            else {
                // tracker was locked, follow it
                pass = e->fields.sync.syncTrackerEvent;
            }
        }
    }
            
    if (pass) {
        MobiusMode* mode = l->getMode();

        if (mode == SynchronizeMode)
          syncPulseWaiting(l, e);

        else if (l->isSyncRecording())
          syncPulseRecording(l, e);

        else if (l->isSyncPlaying())
          syncPulsePlaying(l, e);

        else
          checkPulseWait(l, e);
    }
    else {
        // TODO: Still allow waits on these? 
        // Have to figure out how to Wait for the "other"
        // kind of pulse: Wait xbeat, Wait xbar, Wait xclock
        // Can't call checkPulseWait here because it doesn't
        // know the difference between the sources "Wait beat"
        // must only wait for the sync relevant pulse.
    }
}

/**
 * Called by pulse event handlers to see if the pulse
 * event matches a pending script Wait event, and if so activates
 * the wait event.
 *
 * This must be done in the SyncEvent handler rather than when we
 * first put the event in the MidiQueue.  This is so the wait ends
 * on the same frame that the Track will process the pulse event.
 *
 * This is only meaningful for MIDI and Host sync, for Out sync
 * you just wait for subcycles.
 *
 * !! Think about what this means for track sync, are these
 * different wait types?
 */
PRIVATE void Synchronizer::checkPulseWait(Loop* l, Event* e)
{
    Track* t = l->getTrack();
    EventManager* em = t->getEventManager();
	Event* wait = em->findEvent(ScriptEvent);

	if (wait != NULL && wait->pending) {

		bool activate = false;
		const char* type = NULL;

		switch (wait->fields.script.waitType) {

			case WAIT_PULSE: {
				type = "pulse";
				activate = true;
			}
			break;

			case WAIT_BEAT: {
				// wait for a full beat's worth of pulses (MIDI) or
				// for the next beat event from the host
				type = "beat";
                SyncPulseType pulse = e->fields.sync.pulseType;
				activate = (pulse == SYNC_PULSE_BEAT || pulse == SYNC_PULSE_BAR);
			}
			break;

			case WAIT_BAR: {
                // wait for a full bar's worth of pulses
				type = "bar";
                SyncPulseType pulse = e->fields.sync.pulseType;
				activate = (pulse == SYNC_PULSE_BAR);
			}
            break;

            case WAIT_EXTERNAL_START: {
                type = "externalStart";
                activate = e->fields.sync.syncStartPoint;
            }
            break;

			default:
				break;
		}

		if (activate) {
			Trace(l, 2, "Sync: Activating pending Wait %s event\n", type);
			wait->pending = false;
			wait->immediate = true;
			wait->frame = l->getFrame();
		}
	}
}

/****************************************************************************
 *                                                                          *
 *   					   SYNCHRONIZE MODE PULSES                          *
 *                                                                          *
 ****************************************************************************/

/**
 * Called on each pulse during Synchronize mode.
 * Ordinarilly we're ready to start recording on the this pulse, but
 * for the BAR and BEAT units, we may have to wait several pulses.
 */
PRIVATE void Synchronizer::syncPulseWaiting(Loop* l, Event* e)
{
    SyncSource src = e->fields.sync.source;
    SyncPulseType pulseType = e->fields.sync.pulseType;
    Track* track = l->getTrack();
    SyncState* state = track->getSyncState();
	bool ready = true;
    
	if (src == SYNC_TRACK) {
        SyncTrackUnit trackUnit = state->getSyncTrackUnit();

		switch (trackUnit) {
			case TRACK_UNIT_SUBCYCLE:
				// finest granularity, always ready
				break;
			case TRACK_UNIT_CYCLE:
				// cycle or loop
				ready = (pulseType != SYNC_PULSE_SUBCYCLE);
				break;
			case TRACK_UNIT_LOOP:
				ready = (pulseType == SYNC_PULSE_LOOP);
				break;
			case TRACK_UNIT_DEFAULT:
				// xcode 5 whines if this isn't here
				break;
		}
	}
    else if (src == SYNC_OUT) {
        // This should never happen.  The master track can't
        // wait on it's own pulses, and slave tracks should
        // be waiting for SYNC_TRACK events.  Should have filtered
        // this in getNextEvent.
        Trace(1, "Sync: not expecting to get pulses here!\n");
		ready = false;
	}
    else {
        // MIDI and HOST, filter for beat/bar sensitivity
        
        if (state->getSyncUnit() == SYNC_UNIT_BAR) {
            ready = (pulseType == SYNC_PULSE_BAR);
        }
        else {
            // Host pulses are only beat/bar but MIDI pulses can be CLOCKS
            ready = (pulseType == SYNC_PULSE_BEAT || 
                     pulseType == SYNC_PULSE_BAR);
        }
    }
            
    // we have historically checked pulse waits before starting
    // the recording, the loop frame will be rewound for
    // input latency I guess that's okay
	checkPulseWait(l, e);

	if (ready)
	  startRecording(l, e);
}

/**
 * Called when we're ready to end Synchronize mode and start recording.
 * Activate the pending Record event, initialize the SyncState, and
 * prepare for recording.
 * 
 * Calculate the number of sync pulses that will be expected in one
 * cycle and store it in the RecordCyclePulses field of the sync state.
 * This is used for three things: 
 * 
 *   1) to increment the cycle counter as we cross cycles during recording
 *   2) to determine when we've recorded enough bars in an AutoRecord
 *   3) to determine when we've recorded enough pulses for a pulsed recording
 *
 * The last two usages only occur if we're using the pulse counting
 * method of ending the reording rather than tempo-based length method.
 * If we're using tempo, then a RecordStop event will have already been
 * scheduled at the desired frame because isRecordStopPulsed() will
 * have returned false.
 *
 * TrackSyncMode=SubCycle is weird.  We could try to keep the master cycle
 * size, then at the end roll it into one cycle if we end up with an uneven
 * number of subcycles.  Or we could treat subcycles like "beats" and let
 * the recordBeats parameter determine the beats per cycle.  The later
 * feels more flexible but perhaps more confusing.
 */
PRIVATE void Synchronizer::startRecording(Loop* l, Event* e)
{
    Track* t = l->getTrack();
    EventManager* em = t->getEventManager();
	Event* start = em->findEvent(RecordEvent);

	if (start == NULL) {
		// I suppose we could make one now but this really shouldn't happen
		Trace(l, 1, "Sync: Record start pulse without RecordEvent!\n");
	}
	else if (!start->pending) {
		// already started somehow
		Trace(l, 1, "Sync: Record start pulse with active RecordEvent!\n");
	}
	else {
        SyncState* state = t->getSyncState();

        // !! TODO: Consider locking source state when recording begins
        // rather than waiting till the end?
        // shouldn't we be getting this from the Event?
        SyncSource src = state->getEffectiveSyncSource();

        if (e->fields.sync.syncTrackerEvent) {
            Trace(l, 2, "Sync: RecordEvent activated on tracker pulse\n");
        }
        else if (src == SYNC_MIDI) {
            // have been tracing song clock for awhile, not sure why
            long clock = getMidiSongClock(src);
            Trace(l, 2, "Sync: RecordEvent activated on MIDI clock %ld\n", clock);
        }
        else {
            Trace(l, 2, "Sync: RecordEvent activated on external pulse\n");
        }

		// activate the event, may be latency delayed
        long startFrame = l->getFrame();
        if (src == SYNC_MIDI && !e->fields.sync.syncTrackerEvent)
          startFrame += l->getInputLatency();

        start->pending = false;
        start->frame = startFrame;

        // have to pretend we're in play to start counting frames if
        // we're doing latency compensation at the beginning
        l->setMode(PlayMode);

		Trace(l, 2, "Sync: RecordEvent scheduled for frame %ld\n",
              startFrame);

		// Obscurity: in a script we might want to wait for the Synchronize
		// mode to end but we may have a latency delay on the Record event.
		// Would need some new kind of special wait type.

		// Calculate the number of pulses in one cycle to detect cycle
		// crossings as we record (not used in all modes).
        // NOTE: Using pulses to determine cycles doesn't work if we're
        // speed shifting before or during recording.  Sorry all bets are off
        // if you do speed shifting during recording.
        int beatsPerBar = getBeatsPerBar(src, l);
        int cyclePulses = 0;

        if (src == SYNC_MIDI) {
            // pulse every clock
            cyclePulses = beatsPerBar * 24;
        }
        else if (src == SYNC_HOST) {
            // pulse every beat
            cyclePulses = beatsPerBar;
        }
        else if (src == SYNC_TRACK) {
            // Will always count master track sub cycles, but need to 
            // know how many in a cycle.
            // !! Currently this comes from the active preset, which
            // I guess is okay, but may want to capture this on the SyncState.
            // Well we do now in SyncState::startRecording, but we won't be
            // using that for the record end pulse if the master preset changes
            Preset* mp = mTrackSyncMaster->getPreset();
            cyclePulses = mp->getSubcycles();
        }
        else {
            // not expecting to be here for SYNC_OUT 
            Trace(l, 1, "Sync:getRecordCyclePulses wrong sync source!\n");
        }

        // initialize the sync state for recording
        // have to know whether the tracker was locked at the start of this
        // so we can consistently follow raw or tracker pulses
        bool trackerLocked = false;
        SyncTracker* tracker = getSyncTracker(src);
        if (tracker != NULL)
          trackerLocked = tracker->isLocked();

        state->startRecording(e->fields.sync.pulseNumber, 
                              cyclePulses, beatsPerBar, trackerLocked);
	}
}

/****************************************************************************
 *                                                                          *
 *   						  RECORD MODE PULSES                            *
 *                                                                          *
 ****************************************************************************/

/**
 * Called on each pulse during Record mode.
 * 
 * Increment the pulse counter on the Track and add cycles if
 * we cross a cycle/bar boundary.  If this is an interesting
 * pulse on which to stop recording, call checkRecordStop.
 *
 * If the SyncTracker for this loop is locked we should be getting
 * beat/bar events generated by the tracker.  Otherwise we will be
 * getting clock/beat/bar events directly from the sync source.
 *
 * There are two methods for ending a recording: 
 *
 *   - pending event activated when the desired number of pulses arrive
 *   - event scheduled at specific frame derived from tempo
 *
 * Pulse counting was the original method, it works fine for track sync
 * and is usually fine for host sync, but is unreliable for MIDI sync
 * because of pulse jitter.  
 *
 * Once a SyncTracker is locked it will have stable pulses and we will
 * follow those as well.
 *
 * For the initial MIDI recording before the tracker is locked, we
 * calculate the ending frame based on the observed tempo during recording.
 * We'll still call syncPulseRecording even though the record ending
 * won't be pulsed so we can watch as we fill cycles and bump the
 * cycle count.
 *
 */
PRIVATE void Synchronizer::syncPulseRecording(Loop* l, Event* e)
{
    Track* t = l->getTrack();
    SyncState* state = t->getSyncState();
	bool ready = false;

    // note that we use the event source, which is the same as the
    // effective source for this track
    SyncSource src = e->fields.sync.source;

    // always increment the track's pulse counter
    state->pulse();

    // !! HORRIBLE KLUDGE: If the tracker is locked we'll only receive
    // beat/bar events and no clocks.  But if the MIDI tracker is unlocked we
    // get raw clock events.  The SyncState pulse counter must be treated
    // consistently as a clock, so when we get a MIDI tracker pulse we have
    // to correct the lagging SyncState pulse counter.  Could also solve
    // this by having SyncTracker::advance generate clock pulses but I'd like
    // to avoid that for now since we can't sync to them reliably anyway.
    if (src == SYNC_MIDI && e->fields.sync.syncTrackerEvent) {
        // we added one, but each beat has 24
        state->addPulses(23);
    }

    if (src == SYNC_TRACK) {
        // any pulse is a potential ending
        ready = true;
    }
    else if (src == SYNC_OUT) {
        // Meaningless since we wait for a function trigger, 
        // though I suppose AutoRecord+AutoRecordTempo combined
        // with Sync=Out could wait for a certain frame
	}
	else if (state->isRounding()) {
        // True if the record ending has been scheduled and we're
        // waiting for a specific frame rather than waiting for a pulse.
        // This is normal for SYNC_MIDI since pulses are jittery.  For
        // other sync modes it is normal if we allow the recording to 
        // start unquantized and round at the end.
        // Don't trace since there can be a lot of these for MIDI clocks.
		//Trace(l, 2, "Sync: pulse during record rounding period\n");
	}
	else if (src == SYNC_MIDI) {
        // we only sync to beats not clocks
        ready = (e->fields.sync.pulseType != SYNC_PULSE_CLOCK);
    }
    else {
        // SYNC_HOST, others
        ready = true;
    }

	// Check for script waits on pulses, this is not dependent on
	// whether we're ready to stop the recording.  Do this before all the
	// stop processing, so we can wait for a boundary then use
	// a record ending function, then activate it later when RecordStopEvent 
	// is processed.
    // !! Revisit this we may want pre/post pulse waits because the loop
    // frame may change 
	checkPulseWait(l, e);

	if (ready)  {
        EventManager* em = t->getEventManager();
        Event* stop = em->findEvent(RecordStopEvent);
        if (stop != NULL && !stop->pending) {
            // Already scheduled the ending, nothing more to do here.
            // This should have been caught in the test for isRounding() above,
            // Wait for Loop to call loopRecordStop
            Trace(l, 1, "Sync: extra pulse after end scheduled\n");
        }
        else
          checkRecordStop(l, e, stop);
    }

}

/**
 * Helper for syncPulseRecording.
 * We've just determined that we're on a pulse where the recording
 * may stop (but not necessarily).   If we're not ready to stop yet,
 * increment the cycle counter whenever we cross a cycle boundary.
 */
PRIVATE void Synchronizer::checkRecordStop(Loop* l, Event* pulse, Event* stop)
{
    Track* track = l->getTrack();
    SyncState* state = track->getSyncState();
    SyncSource source = pulse->fields.sync.source;

    // first calculate the number of completely filled cycles,
    // this will be one less than the loop cycle count unless
    // we're exactly on the cycle boundary
    int recordedCycles = 0;
    bool cycleBoundary = false;
    int cyclePulses = state->getCyclePulses();
    if (cyclePulses <= 0)
      Trace(l, 1, "Sync: Invalid SyncState cycle pulses!\n");
    else {
        int pulse = state->getRecordPulses();
        recordedCycles = (long)(pulse / cyclePulses);
        cycleBoundary = ((pulse % cyclePulses) == 0);

        Trace(l, 2, "Sync: Recording pulse %ld cyclePulses %ld boundary %ld\n",
              (long)pulse,
              (long)cyclePulses,
              (long)cycleBoundary);
    }

    // check varous conditions to see if we're really ready to stop
    if (stop != NULL) {

        if (stop->function == AutoRecord) {
            // Stop when we've recorded the desired number of "units".
            // This is normally a bar which is the same as a cycle.
            int recordedUnits = recordedCycles;
            int requiredUnits = stop->number;

            if (source == SYNC_TRACK) {

                // Track sync units are more complicated, they are
                // defined by the SyncTrackUnit which may be larger
                // or smaller than a cycle.

                if (mTrackSyncMaster == NULL) {
                    // must have been reset this interrupt
                    Trace(l, 2, "Synchronizer::checkRecordStop trackSyncMaster evaporated!\n");
                }
                else {
                    SyncTrackUnit unit = state->getSyncTrackUnit();

                    if (unit == TRACK_UNIT_LOOP) {
                        // a unit is a full loop
                        // we've been counting cycles so have to 
                        // divide down
                        Loop* masterLoop = mTrackSyncMaster->getLoop();
                        recordedUnits /= masterLoop->getCycles();
                    }
                    else if (unit == TRACK_UNIT_SUBCYCLE) {
                        // units are subcycles and we get a pulse for each
                        recordedUnits = state->getRecordPulses();
                    }
                }
            }

            if (recordedUnits < requiredUnits) {
                // not ready yet
                stop = NULL;
            }
            else if (recordedUnits > requiredUnits) {
                // must have missed a pulse?
                Trace(l, 1, "Sync: Too many pulses in AutoRecord!\n");
            }
        }
        else if (source == SYNC_TRACK) {
            // Normal track sync.  We get a pulse every subycle, but when
            // quantizing the end of a recording, have to be more selective.

            SyncTrackUnit required = state->getSyncTrackUnit();
            SyncPulseType pulseType = pulse->fields.sync.pulseType;

            if (required == TRACK_UNIT_CYCLE) {
                // CYCLE or LOOP will do
                if (pulseType != SYNC_PULSE_CYCLE &&
                    pulseType != SYNC_PULSE_LOOP)
                  stop = NULL;
            }
            else if (required == TRACK_UNIT_LOOP) {
                // only LOOP will do
                if (pulseType != SYNC_PULSE_LOOP)
                  stop = NULL;
            }
        }
        else if (source == SYNC_MIDI || source == SYNC_HOST) {
            // may have to wait for a bar
            if (state->getSyncUnit() == SYNC_UNIT_BAR) {
                if (!cycleBoundary) 
                  stop = NULL;
            }
        }
    }


    if (stop != NULL) {
        activateRecordStop(l, pulse, stop);
    }
    else {
        // Not ready to stop yet, but advance cycles
        // If we're doing beat sync this may be optimistically large
        // and have to be rounded down later if we don't fill a cycle
        if (cycleBoundary) {
            if (recordedCycles != l->getCycles())
              Trace(l, 1, "Sync: Unexpected jump in cycle count!\n");
            l->setRecordCycles(recordedCycles + 1);
        }
	}
}

/**
 * Helper for syncPulseRecording.  We're ready to stop recording now.
 * Activate the pending RecordStopEvent and the final sync state.
 * We can begin playing now, but we may have to delay the actual ending
 * of the recording to compensate for input latency.
 *
 * When the loop has finally finished procesisng the RecordStopEvent it
 * will call back to loopRecordStop.  Then we can start sending clocks.
 * We may be able to avoid this distinction, at least for the 
 * purposes of sending clocks, but see comments in loopRecordStop
 * for some history.
 *
 */
PRIVATE void Synchronizer::activateRecordStop(Loop* l, Event* pulse, 
                                              Event* stop)
{
    Track* track = l->getTrack();
    SyncState* state = track->getSyncState();
    SyncSource source = state->getEffectiveSyncSource();

	// let Loop trace about the final frame counts
	Trace(l, 2, "Sync: Activating RecordStop after %ld pulses\n", 
		  (long)state->getRecordPulses());

    // prepareLoop will set the final frame count in the Record layer
    // which is what Loop::getFrames will return.  If we're following raw
    // MIDI pulses have to adjust for latency.
    
    bool inputLatency = (source == SYNC_MIDI && !pulse->fields.sync.syncTrackerEvent);

    // since we almost always want even loops for division, round up
    // if necessary
    // !! this isn't actually working yet, see Loop::prepareLoop
    int extra = 0;
    long currentFrames = l->getFrames();
    if ((currentFrames % 2) > 0) {
        Trace(l, 2, "WARNING: Odd number of frames in new loop\n");
        // actually no, we don't want to do this if we're following
        // a SyncTracker or using SYNC_TRACK, we have to be exact 
        // only do this for HOST/MIDI recording from raw pulses 
        //extra = 1;
    }

	l->prepareLoop(inputLatency, extra);
	long finalFrames = l->getFrames();
	long pulses = state->getRecordPulses();

    // save final state and wait for loopRecordStop
    state->scheduleStop(pulses, finalFrames);

    // activate the event
	stop->pending = false;
	stop->frame = finalFrames;

    // For SYNC_TRACK, recalculate the final cycle count based on our
    // size relative to the master track.  If we recorded an odd number
    // of subcycles this may collapse to one cycle.  We may also need to
    // pull back a cycle if we ended exactly on a cycle boundary
    // (the usual case?)

    if (source == SYNC_TRACK) {
        // get the number of frames recorded
        // sanity check an old differece we shouldn't have any more
        long slaveFrames = l->getRecordedFrames();
        if (slaveFrames != finalFrames)
          Trace(l, 1, "Error in ending frame calculation!\n");

        if (mTrackSyncMaster == NULL) 
          Trace(l, 1, "Synchronizer::stopRecording track sync master gone!\n");
        else {
            Loop* masterLoop = mTrackSyncMaster->getLoop();

            // !! TODO: more consistency checks

            long cycleFrames = masterLoop->getCycleFrames();
            if (cycleFrames > 0) {
                if ((slaveFrames % cycleFrames) > 0) {
                    // Not on a cycle boundary, should only have happened
                    // for TRACK_UNIT_SUBCYCLE.  Collapse to one cycle.
                    l->setRecordCycles(1);
                }
                else {
                    int cycles = slaveFrames / cycleFrames;
                    if (cycles == 0) cycles = 1;
                    int current = l->getCycles();
                    if (current != cycles) {
                        // Is this normal?  I guess we would need this
                        // to pull it back by one if we end recording exactly
                        // on the cycle boundary?
                        Trace(l, 2, "Sync: Adjusting ending cycle count from %ld to %ld\n",
                              (long)current, (long)cycles);
                        l->setRecordCycles(cycles);
                    }
                }
            }
        }
    }
    else if (source == SYNC_HOST || source == SYNC_MIDI) {
        // If the sync unit was Beat we may not have actually
        // filled the final cycle.  If not treat it similar to an 
        // unrounded multiply and reorganize as one cycle.
        int cyclePulses = state->getCyclePulses();
        int remainder = pulses % cyclePulses;
        if (remainder > 0) {
            int missing = cyclePulses - remainder;
            Trace(l, 2, "Sync: Missing %ld pulses in final cycle, restructuring to one cycle\n",
                  (long)missing);
            l->setRecordCycles(1);
        }
    }
}

/****************************************************************************
 *                                                                          *
 *   						   PLAY MODE PULSES                             *
 *                                                                          *
 ****************************************************************************/

/**
 * Called on each pulse after a synchronized loop has finished recoding.
 *
 * There are two things we do here:
 * 
 *   - Check pending Realign events
 *   - Check "external start point" events
 *
 * Originally we checked drift here too but that has to be deferred
 * until the end of the interrupt because we share SyncTrackers among
 * several tracks.
 *
 * Track sync pulses are only interesting if we're in Realign mode
 * waiting for a master track location.  Other pulses just increment
 * the SyncTracker.
 * 
 * SYNC_MIDI: The pulses will be from the SyncTracker since once the
 * first MIDI loop is recorded and the tracker is locked we no longer
 * directly follow MIDI clocks.
 *
 * SYNC_HOST: The pulses be from the SyncTracker.
 *
 * SYNC_TRACK: The pulses will be from the master track.
 *
 * SYNC_OUT: The pulses will be from the timer, not the SyncTracker.
 * ?? Really why ??
 * 
 * EXTERNAL START POINT
 * 
 * For sync sources that have a SyncTracker, the tracker will reach
 * the "external start point" whenever the pulse counter wraps back to zero.
 * This will have been recorded in the Event.isSyncStartPoint property.
 * This can trigger a Realign if RealignTime is START, the
 * SyncStartPoint function, or a script wait statement.
 * 
 * REALIGN
 *
 * If there is a RealignEvent marked pending, then the track is waiting
 * for a realign pulse.  The RealignTime parameter from the Setup
 * determines which pulse we will wait for.  
 * 
 * If we have a pending Realign, SYNC_OUT and OutRealignMode=midiStart,
 * then it is more accurate to wait for the actual loop start point
 * (frame zero) rather than watching the pulses.  In this scenario
 * we are forcing the external device back into sync with the loop, and
 * the pulse counter may have drifted slightly.  When OutRealignMode=restart
 * then we obey the pulse counts because we are forcing the loop to be in sync
 * with the external device.
 *
 * The Realign/SYNC_OUT/OutRealignMode=midiStart case will therefore be
 * handled by the loopLocalStartPoint callback rather than here.
 * Note that in this case we ignore the RealignTime parameter and always
 * wait for the loop start point.  It might be interesting to allow
 * RealignTime, but we would then need Loop callbacks for each cycle
 * and subcycle, and would need to send a MIDI SongPosition event to 
 * orient the external device relative to the loop location.
 * 
 */
PRIVATE void Synchronizer::syncPulsePlaying(Loop* l, Event* e)
{
    Track* t = l->getTrack();
    EventManager* em = t->getEventManager();
	Event* realign = em->findEvent(RealignEvent);
    
    if (realign != NULL) {

        if (!realign->pending) {
			// Might get here in the special case for Sync=Out
			// OutRealignMode=midiStart described above?
            Trace(l, 2, "Sync: Ignoring active Realign event at sync pulse");
        }
        else {
            // determine whether this is the right pulse
            // for SYNC_TRACK we get SUBCYCLE, CYCLE, and LOOP pulses, 
            // for the others we get BEAT and BAR

            Setup* setup = mMobius->getInterruptSetup();
            RealignTime rtime = setup->getRealignTime();
            SyncPulseType pulse = e->fields.sync.pulseType;
            bool ready = false;

            // SYNC_OUT, OutRealignMode=midiStart is a special case
            // handled by loopLocalStartPoint
            if (l->getTrack() != mOutSyncMaster ||
                setup->getOutRealignMode() != REALIGN_MIDI_START) {

                switch (rtime) {
                    case REALIGN_NOW:
                        // REALIGN_NOW will normally have been handled
                        // immediately but just in case handle it here on
                        // the next pulse
                        ready = true;
                        break;

                    case REALIGN_BEAT:
                        // everything except clocks
                        ready = (pulse != SYNC_PULSE_UNDEFINED && 
                                 pulse != SYNC_PULSE_CLOCK);
                        break;

                    case REALIGN_BAR:
                        ready = (pulse == SYNC_PULSE_BAR ||
                                 pulse == SYNC_PULSE_CYCLE ||
                                 pulse == SYNC_PULSE_LOOP);
                        break;

                    case REALIGN_START:
                        ready = e->fields.sync.syncStartPoint;
                        break;
                }
            }

            if (ready)
              doRealign(l, e, realign);
        }
	}

    // Check for pending events that can be activated on this pulse.
    // Note that we have to do this after a realign so we know the
    // new loop frame.

    if (e->fields.sync.syncStartPoint) {

        traceDealign(l);

        // Check for pending SyncStartPoint
        Event* startPoint = em->findEvent(StartPointEvent);
        if (startPoint != NULL &&
            startPoint->function == SyncStartPoint &&
            startPoint->pending) {
			
            long frame = l->getFrame();

            // For SYNC_MIDI if we're directly following the external
            // clock we have to adjust for latency.  This is not necessary
            // when following the SyncTracker which we should always
            // be doing now.
            if (e->fields.sync.source == SYNC_MIDI) {
                if (!e->fields.sync.syncTrackerEvent) {
                    Trace(l, 1, "Sync: Not expecting raw pulse for StartPointEvent\n");
                    frame += l->getInputLatency();
                }
            }

            Trace(l, 2, "Sync: Activating pending SyncStartPoint at frame %ld\n", frame);
            startPoint->pending = false;
            startPoint->immediate = true;
            startPoint->frame = frame;
        }
    }

    // Check for various pulse waits
    checkPulseWait(l, e);
}

/**
 * At the external start point, trace dealign amounts for one
 * of the following tracks.
 *
 * After the tracker is locked for the first time, this should
 * stay in perfect sync.  For reasons I can't explain yet, the 
 * loop start point is usually at the "end point" where the frame
 * number is equal to the loop size rather than zero.  Every once
 * and awhile I see this at zero, need to find out why.
 *
 * For dealign purposes though, they are the same.
 *
 * Note that the tracker frame number will have already advanced
 * so you can't compare it to the loop frame.  This is only called
 * when we're processing a pulse event with isSyncStartPoint so we
 * can assume the tracker frame was zero.
 */
PRIVATE void Synchronizer::traceDealign(Loop* l)
{
    Track* t = l->getTrack();
    SyncState* state = t->getSyncState();
    SyncSource src = state->getEffectiveSyncSource();
    SyncTracker* tracker = getSyncTracker(src);

    if (tracker != NULL) {
        long loopFrames = l->getFrames();
        long trackerFrames = tracker->getLoopFrames();

        if (trackerFrames > 0 && loopFrames > 0) {
            long loopFrame = l->getFrame();
            long trackerFrame = tracker->getAudioFrame();

            // wrap if we're at the end point
            if (loopFrame == loopFrames) loopFrame = 0;

            // if we're a multiple up try not to exagurate the dealign
            // find the closest common boundary
            if (loopFrames > trackerFrames) {
                // loop is more than tracker, must have been a Multiply or
                // multi cycle record
                if ((loopFrames % trackerFrames) == 0)
                  loopFrame = loopFrame % trackerFrames;
            }

            // tracker frame is zero
            // let a negative alighment mean the loop is behind the tracker
            long dealign;
            long line = loopFrames / 2;
            if (loopFrame > line)
              dealign = -(loopFrames - loopFrame);
            else
              dealign = loopFrame;

            Trace(l, 2, "Sync: Tracker %s start point, loop frame %ld dealign %ld\n",
                  tracker->getName(), l->getFrame(), dealign);
        }
    }
}

/****************************************************************************
 *                                                                          *
 *                                  REALIGN                                 *
 *                                                                          *
 ****************************************************************************/

/**
 * Called when we reach a realign point.
 * Determine where the ideal Loop frame should be relative to the
 * sync source and move the loop.
 * 
 * This can be called in two contexts: by suncPulsePlaying during processing
 * of a SyncEvent and by loopLocalStartPoint when the Loop reaches the start
 * point and we're the OutSyncMaster and OutRealignMode=Midistart.
 *
 * When called by syncPulsePlaying the "pulse" event will be non-null 
 * and should have come from the SyncTracker.
 *
 * When we're the OutSyncMaster, we own the clock and can make the external
 * device move.  NOTE: this is only working RealignTime=Loop and
 * we can simply send MS_START.  For other RealignTimes we need to 
 * be sending song position messges!!
 */
PRIVATE void Synchronizer::doRealign(Loop* l, Event* pulse, Event* realign)
{
    Track* t = l->getTrack();
    EventManager* em = t->getEventManager();
    Setup* setup = mMobius->getInterruptSetup();

    // sanity checks since we can be called directly by the Realign function
    // really should be safe by now...
    if (l->getFrames() == 0) {
		Trace(l, 1, "Sync: Ignoring realign of empty loop!\n");
	}
    else if (l->getTrack() == mOutSyncMaster &&
             setup->getOutRealignMode() == REALIGN_MIDI_START) {

        // We don't change position, we tell the external device to 
        // retrigger from the beginning.  We should be at the internal
        // Loop start point (see comments)
        if (l->getFrame() != 0)
          Trace(l, 1, "Sync:doRealign Loop not at start point!\n");

        // !! We have historically disabled sending MS_START if the
        // ManualStart option was on.  But this makes Realign effectively
        // meaningless.  Maybe we should violate ManualStart in this case?
        if (!setup->isManualStart())
          sendStart(l, false, false);
	}
    else if (pulse == NULL) {
        // only the clause above is allowed without a pulse
        Trace(l, 1, "Sync:doRealign no pulse event!\n");
    }
    else if (pulse->fields.sync.source == SYNC_TRACK) {
		realignSlave(l, pulse);
    }
	else {
        // Since the tracker may have generated several pulses in this
        // interrupt we have to store the pulseFrame in the event.
        long newFrame = pulse->fields.sync.pulseFrame;

        // formerly adjusted for MIDI pulse latency, this should
        // no longer be necessary if we're following the SyncTracker
        SyncSource source = pulse->fields.sync.source;
        if (source == SYNC_MIDI && !pulse->fields.sync.syncTrackerEvent) {
            Trace(l, 1, "Sync: Not expecting raw event for MIDI Realign\n");
            newFrame += l->getInputLatency();
        }

        // host realign should always be following the tracker
        if (source == SYNC_HOST && !pulse->fields.sync.syncTrackerEvent)
          Trace(l, 1, "Sync: Not expecting raw event for HOST Realign\n");

        newFrame = wrapFrame(l, newFrame);

        Trace(l, 2, "Sync: Realign to external pulse from frame %ld to %ld\n",
              l->getFrame(), newFrame);

        // save this for the unit tests
        Track* t = l->getTrack();
        SyncState* state = t->getSyncState();
        state->setPreRealignFrame(l->getFrame());

        moveLoopFrame(l, newFrame);
    }

    // Post processing after realign.  RealignEvent doesn't have
    // an invoke handler, it is always pending and evaluated by Synchronzer.
    // If this was scheduled from MuteRealign then cancel mute mode.
    // Wish we could bring cancelSyncMute implementation in here but it is also
    // needed by the MidiStartEvent handler.  
	if (realign->function == MuteRealign)
      l->cancelSyncMute(realign);

    // resume waiting scripts
    realign->finishScriptWait();

	// we didn't process this in the usual way, we own it
    // this will remove and free
    em->freeEvent(realign);

	// Check for "Wait realign"
	Event* wait = em->findEvent(ScriptEvent);
	if (wait != NULL && 
        wait->pending && 
        wait->fields.script.waitType == WAIT_REALIGN) {
		wait->pending = false;
		// note that we use the special immediate option since
		// the loop frame can be chagned by SyncStartPoint
		wait->immediate = true;
		wait->frame = l->getFrame();
	}
}

/**
 * Called by RealignFunction when RealignTime=Now.
 * Here we don't schedule a Realign event and wait for a pulse, 
 * we immediately move the slave loop.
 */
PUBLIC void Synchronizer::loopRealignSlave(Loop* l)
{
	realignSlave(l, NULL);
}

/**
 * Perform a track sync realign with the master.
 *
 * When "pulse" is non-null we're being called for a pending RealignEvent
 * and we've received the proper master track sync pulse.  The pulse
 * will have the master track frame where the pulse was located.  Note
 * that we must use the frame from the event since the master track
 * will have been fully advanced by now and may be after the pulse frame.
 * 
 * When "pulse" is null, we're being called by RealignFunction
 * when RealignTime=Now.  We can take the currrent master track location
 * but we have to do some subtle adjustments.
 * 
 * Example: Master track is at frame 1000 and slave track is at 2000, 
 * interrupt buffer size is 256.  The Realign is scheduled for frame
 * 2128 in the middle of the buffer.  By the time we process the Realign
 * event, the master track will already have advanced to frame 1256.  
 * If we set the slave frame to that, we still have another 128 frames to
 * advance so the state at the end of the interrupt will be master 1256 and
 * slave 1384.   We can compensate for this by factoring in the 
 * current buffer offset of the Realign event which we don't' have but we
 * can assume we're being called by the Realgin event handler and use
 * Track::getRemainingFrames.
 * 
 * It gets messier if the master track is running at a different speed.
 * 
 */
PRIVATE void Synchronizer::realignSlave(Loop* l, Event* pulse)
{
	long loopFrames = l->getFrames();
	
	if (loopFrames == 0) {
		// empty slave, shouldn't be here
		Trace(l, 1, "Sync: Ignoring realign of empty loop\n");
	}
	else if (mTrackSyncMaster == NULL) {
		// also should have caught this
		Trace(l, 1, "Sync: Ignoring realign with no master track\n");
	}
	else {
        Track* track = l->getTrack();
        SyncState* state = track->getSyncState();
		long newFrame = 0;

        if (pulse != NULL) {
            // frame conveyed in the event
            newFrame = (long)pulse->fields.sync.pulseFrame;
        }
        else {
            // subtle, see comments above
            Loop* masterLoop = mTrackSyncMaster->getLoop();

			// the master track at the end of the interrupt (usually)
			long masterFrame = masterLoop->getFrame();

			// the number of frames left in the master interrupt
			// this is usually zero, but in some of the unit tests
			// that wait in the master track, then switch to the
			// slave track there may still be a remainder
			long masterRemaining = mTrackSyncMaster->getRemainingFrames();

			// the number of frames left in the slave interrupt
			long remaining = track->getRemainingFrames();

			// SPEED NOTE
			// Assuming speeds are the same, we should try to have
			// both the master and slave frames be the same at the
			// end of the interrupt.  If speeds are different, we can 
			// cause that to happen, but it is probably ok that they
			// be allowed to drift.

			masterRemaining = (long)((float)masterRemaining * getSpeed(masterLoop));
			remaining = (long)((float)remaining * getSpeed(l));

			remaining -= masterRemaining;

			// remove the advance from the master frame
			// wrapFrame will  handle it if this goes negative
			newFrame = masterFrame - remaining;
		}

		// wrap master frame relative to our length
		newFrame = wrapFrame(l, newFrame);

		Trace(l, 2, "Sync: Realign slave from frame %ld to %ld\n",
			  l->getFrame(), newFrame);

        // save this for the unit tests
        state->setPreRealignFrame(l->getFrame());
        moveLoopFrame(l, newFrame);
	}
}

/**
 * Called by Loop when we're at the local start point.
 * 
 * If we're the out sync master with a pending Realign and 
 * OutRealignMode is REALIGN_MIDI_START, activate the Realign.
 * 
 */
PUBLIC void Synchronizer::loopLocalStartPoint(Loop* l)
{
    Track* t = l->getTrack();

	if (t == mOutSyncMaster) {

        Setup* setup = mMobius->getInterruptSetup();
		OutRealignMode mode = setup->getOutRealignMode();

		if (mode == REALIGN_MIDI_START) {
            EventManager* em = l->getTrack()->getEventManager();
			Event* realign = em->findEvent(RealignEvent);
			if (realign != NULL)
			  doRealign(l, NULL, realign);
		}
	}

}

/****************************************************************************
 *                                                                          *
 *                              DRIFT CORRECTION                            *
 *                                                                          *
 ****************************************************************************/

/**
 * For each tracker, check to see if the drift exceeds the threshold
 * and attempt to correct all tracks that follow the tracker.  If any
 * track is in an incorrectable state (recording) the correction must
 * be deferred.
 * 
 * This could be done at either the beginning or end of the
 * interrupt but if we need to support DriftCheckPoint=loop we have to 
 * let the tracks advance first.  In retrospect I don't really like
 * DriftCheckPoint=Loop since not all tracks will be aligned the same,
 * consider removing it.
 *
 * When exactly we make this correction isn't important, it doesn't have to be
 * adjusted for pulse latency.
 *
 */
PRIVATE void Synchronizer::checkDrift()
{
    checkDrift(mOutTracker);
    checkDrift(mMidiTracker);
    checkDrift(mHostTracker);
    mForceDriftCorrect = false;
}

PRIVATE void Synchronizer::correctDrift()
{
    correctDrift(mOutTracker);
    correctDrift(mMidiTracker);
    correctDrift(mHostTracker);
    mForceDriftCorrect = false;
}

/**
 * Check drift for one sync tracker.
 * 
 * There are two places we can check for drift, defined by the 
 * DriftCheckPoint parameter.  LOOP means the start point of the Mobius loop
 * and EXTERNAL means the start point of the external loop being
 * maintained by the SyncTracker.  This is not currently exposed
 * in the UI, the unit tests expect LOOP.
 *
 * Checking drift at internal loop locations can result in a more musically
 * useful correction since the correction happens near a boundary where 
 * the listener is accustomed to hearing something change.  Checking
 * drift at the external loop boundary makes things seem tighter
 * with the drum pattern or sequence being played which might be preferable.
 * 
 * In either case the drift we check and apply was calculated on the LAST
 * pulse we cannot compare the current pulse frame with the current 
 * audio frame in the tracker.  See comments at the top of SyncTracker
 * about the possible margin of error.
 * 
 */
PRIVATE void Synchronizer::checkDrift(SyncTracker* tracker)
{
    bool checkpoint = false;
    const char* traceMsg;

    if (tracker->isLocked()) {
        if (mDriftCheckPoint == DRIFT_CHECK_EXTERNAL) {
            // See if we have a start point pulse for this tracker.
            // Note that there could be two events with this sync source, 
            // one from the external clock and one from the SyncTracker.
            // It doesn't really matter which we use since the adjustment
            // will be the same, only the timing of the adjustment will 
            // be different.  But since the goal of this is to realign
            // with the external clock, let it control the timing.
            // We do this by checking !e->isSyncTrackerEvent().
            Event* e = mInterruptEvents->getEvents();
            for ( ; e != NULL ; e = e->getNext()) {
                if (e->fields.sync.source == tracker->getSyncSource() &&
                    !e->fields.sync.syncTrackerEvent &&
                    e->fields.sync.syncStartPoint) {
                    checkpoint = true;
                    traceMsg = "Sync:checkDrift %s: External start point drift %ld\n";
                    break;
                }
            }
        }
        else {
            // See if the first slave track crossed its start point.
            // We determine this by looking for a "boundary event" saved
            // in the SyncState.
            Track* slave = NULL;
            int tcount = mMobius->getTrackCount();
            for (int i = 0 ; i < tcount ; i++) {
                Track* t = mMobius->getTrack(i);
                SyncState* state = t->getSyncState();
                if (state->getEffectiveSyncSource() == tracker->getSyncSource()) {
                    slave = t;
                    break;
                }
            }

            if (slave != NULL) {
                SyncState* state = slave->getSyncState();
                checkpoint = (state->getBoundaryEvent() == LoopEvent);
                if (checkpoint)
                  traceMsg = "Sync:checkDrift %s: Internal start point drift %ld\n";
            }
        }
    }

    // Would we ever want to defer forced drift checkpoint to 
    // a checkpoint boundary or are they always immediate?

    if (checkpoint || mForceDriftCorrect) {

        // keep a count of the drift checks for sync test scripts
        tracker->incDriftChecks();

        // tracker has been calculating the amount of drift
        long drift = (long)tracker->getDrift();
        int absdrift = (int)((drift > 0) ? drift : -drift);
        
        // Trackers are already tracing every beat with drift
        // this doesn't tell us anything new other than whether
        // it was an External or Internal start point
        //Trace(2, traceMsg, tracker->getName(), (long)drift);

        if (absdrift > mMaxSyncDrift || 
            (mForceDriftCorrect && absdrift != 0))
          correctDrift(tracker);

        // Wake up a script waiting for the drift check point.
        // Note that this has to be done after the frame is changed.
        // Sigh, yet another track walk, only look at the directly slaved
        // tracks which is enough for unit tests
        int ntracks = mMobius->getTrackCount();
        for (int i = 0 ; i < ntracks ; i++) {
            Track* t = mMobius->getTrack(i);
            SyncState* state = t->getSyncState();
            if (state->getEffectiveSyncSource() == tracker->getSyncSource()) {
                EventManager* em = t->getEventManager();
                Event* wait = em->findEvent(ScriptEvent);
                if (wait != NULL && 
                    wait->pending && 
                    wait->fields.script.waitType == WAIT_DRIFT_CHECK) {
                    // activate it now
                    Loop* loop = t->getLoop();
                    wait->pending = false;
                    wait->immediate = true;
                    wait->frame = loop->getFrame();
                }
            }
        }
    }
}

/**
 * Force drift correction for a tracker regardless of the current
 * amount of drift.  
 * 
 * Factored out of checkDrift so we can call it directly from a function.
 *
 * The correction may be denied if any of the affected tracks are
 * recording or in a state that can't be corrected.
 */
PRIVATE void Synchronizer::correctDrift(SyncTracker* tracker)
{
    // not so fast...all tracks have to be in a correctable state
    bool correctable = true;

    int ntracks = mMobius->getTrackCount();
    for (int i = 0 ; i < ntracks && correctable ; i++) {
        Track* t = mMobius->getTrack(i);
        SyncState* state = t->getSyncState();

        if (state->getEffectiveSyncSource() == tracker->getSyncSource()) {
            // it follows this tracker
            correctable = isDriftCorrectable(t, tracker);
            if (correctable) {
                // tracksync slaves must also be ready
                // currently only one master, eventually may need recursion
                if (t == mTrackSyncMaster) {
                    for (int j = 0 ; j < ntracks && correctable ; j++) {
                        Track* t2 = mMobius->getTrack(j);
                        SyncState* state2 = t2->getSyncState();
                        if (state2->getEffectiveSyncSource() == SYNC_TRACK)
                          correctable = isDriftCorrectable(t2, tracker);
                    }
                }
            }
        }
    }

    if (!correctable) {
        Trace(2, "Sync: Unable to correct drift for tracker %s\n",
              tracker->getName());
    }
    else {
        Trace(2, "Sync: Beginning drift correction for tracker %s\n",
              tracker->getName());

        // keep track of the number of drift corrections we've performed
        tracker->incDriftCorrections();

        // sigh, same walk as we did above, could have saved them 
        // in a List...
        for (int i = 0 ; i < ntracks ; i++) {
            Track* t = mMobius->getTrack(i);
            SyncState* state = t->getSyncState();

            if (state->getEffectiveSyncSource() == tracker->getSyncSource()) {
                            
                correctDrift(t, tracker);

                if (t == mTrackSyncMaster) {
                    for (int j = 0 ; j < ntracks && correctable ; j++) {
                        Track* t2 = mMobius->getTrack(j);
                        SyncState* state2 = t2->getSyncState();
                        if (state2->getEffectiveSyncSource() == SYNC_TRACK) {
                            correctDrift(t2, tracker);
                        }
                    }
                }
            }
        }

        // reset the drift state in this tracker now that all
        // the dependent tracks have been corrected
        tracker->correct();
    }
}

/**
 * Return true if drift correction can be done in this track.
 */
PRIVATE bool Synchronizer::isDriftCorrectable(Track* track, SyncTracker* tracker)
{
    // logic is backward for historical reasons...too lazy to rewrite
    bool suppress = false;

    Loop* loop = track->getLoop();
	MobiusMode* mode = loop->getMode();

    // tracker has been calculating the amount of drift
    long drift = (long)tracker->getDrift();
    int absdrift = (int)((drift > 0) ? drift : -drift);

    // NOTE: Some older logic let a track in Synchronize mode be corrected
    // if this was a track sync slave to the OUT sync master tracn
    // and the direction of the drift was negative.  I don't remember why
    // jumping backard was okay but not forward, either way it seems
    // obscure and not worth the trouble.
		
    if (mode != PlayMode && mode != MuteMode &&	mode != ConfirmMode && 
        mode != ResetMode) {
        Trace(loop, 2, "Sync: Tracker %s: Suppressing drift correction in mode %s\n",
              tracker->getName(), mode->getName());
        suppress = true;
    }

    // Disable drift adjust if continuous feedback is being applied so 
    // we get a clean copy of the layer.
    if (!suppress) {
        Preset* p = track->getPreset();
        if (!p->isNoLayerFlattening()) {
            // !! this may be more complicated since the effective
            // feedback is buried in the smoothers
            int feedback = track->getFeedback();
            if (feedback < 127) {
                Trace(loop, 2, "Sync: Tracker %s: Suppressing drift correction while feedback reduced\n",
                      tracker->getName());
                suppress = true;
            }
        }
    }

    // Disable retrigger for certain pending events
    if (!suppress) {
        EventManager* em = track->getEventManager();
        for (Event* e = em->getEvents() ; e != NULL && !suppress ; e = e->getNext()) {

            if (!e->pending) {
                // Let ReturnEvents finish
                suppress = (e->type == ReturnEvent);
                if (suppress)
                  Trace(loop, 2, "Sync: Tracker %s: Suppressing drift correction due to ReturnEvent\n",
                        tracker->getName());
            }
            else {
                if (e->type == ScriptEvent) {
                    // Suppress if we have a wait event that isn't waiting
                    // for us to actually do the drift check.
                    WaitType wt = e->fields.script.waitType;
                    suppress = (wt != WAIT_DRIFT_CHECK && wt != WAIT_PULSE);
                    if (suppress)
                      Trace(loop, 2, "Sync: Tracker %s: Suppressing drift correction due to ScriptEvent\n",
                            tracker->getName());
                }
                else {	
                    // Old comments say to suppress if there is a pending
                    // ReturnEvent but we've actually been suppressing if
                    // there are ANY pending events for quite awhile.
                    // Revisit this...!!
                    suppress = true;
                    Trace(loop, 2, "Sync: Tracker %s: Suppressing drift correction due to pending event\n",
                          tracker->getName());
                }
            }
        }
    }

    return !suppress;
}

/**
 * Correct the drift in one track.
 */
PRIVATE void Synchronizer::correctDrift(Track* track, SyncTracker* tracker)
{
    Loop* loop = track->getLoop();
    
    // may be other states to ignore?
    if (!loop->isReset()) {

        SyncState* state = track->getSyncState();
        long drift = (long)tracker->getDrift();

        // save this for the unit tests
        state->setPreRealignFrame(loop->getFrame());

        long loopFrames = loop->getFrames();
        long newFrame = loop->getFrame();

        if (loopFrames <= 0) {
            // catch this just to be absoultely sure we don't divide by zero
            Trace(loop, 1, "Sync: Loop frame count hootered!\n");
        }
        else {
            // if drift is positive the audio frame is ahead
            newFrame -= (long)drift;
            newFrame = wrapFrame(loop, newFrame);

            // don't need to worry about pulse latency, right??
            Trace(loop, 2, "Sync: Drift correction of track %ld from %ld to %ld\n",
                  (long)track->getDisplayNumber(), loop->getFrame(), newFrame);

            moveLoopFrame(loop, newFrame);
        }
    }
}

/**
 * Given a logical loop frame calculated for drift correction or realignment,
 * adjust it so that it fits within the target loop.
 */
PRIVATE long Synchronizer::wrapFrame(Loop* l, long frame)
{
    long max = l->getFrames();
    if (max <= 0) {
        Trace(l, 1, "Sync:wrapFrame loop is empty!\n");
        frame = 0;
    }
    else {
        if (frame > 0)
          frame = frame % max;
        else {
            // can be negative after drift correction
            // ugh, must be a better way to do this!
            while (frame < 0)
              frame += max;
        }
    }
    return frame;
}

/**
 * Called when we need to change the loop frame for either drift 
 * correction or realign.
 * 
 * We normally won't call this if we're recording, but the
 * layer still could have unshifted contents in some cases left
 * behind from an eariler operation.
 *
 */
PRIVATE void Synchronizer::moveLoopFrame(Loop* l, long newFrame)
{
	if (newFrame < l->getFrame()) {
		// jumping backwards, this is probably ok if we're
		// at the end, but a shift shouldn't hurt 
		l->shift(true);
	}

	l->setFrame(newFrame);
	l->recalculatePlayFrame();
}

/****************************************************************************
 *                                                                          *
 *                            LOOP RECORD CALLBACKS                         *
 *                                                                          *
 ****************************************************************************/

/**
 * Called by Loop whenever the initial recording of a loop officially
 * starts.  If this is the out sync master, stop sending clocks.
 * Be careful though because we will get here in two contexts:
 *
 *   - the RecordEvent was scheduled by Synchronizer::startRecording when
 *     a suitable pulse was reached
 *
 *   - the RecordEvent was scheduled by RecordFunction without synchronizing,
 *     but this may be the master track that is currently generating clocks
 *
 * In the first case, we have to preserve the RecordCyclePulses
 * counter that was set for cycle detection in startRecord() above.
 *
 * ORIGIN PULSE NOTES
 * 
 * Origin pulse is important for Host and MIDI sync to do pulse rounding
 * at the end if the tracker is unlocked.  Assume all pulses in this
 * interrupt were done at the beginning so we can use the advanced tracker
 * pulse count. That's true right now but if we ever wanted to shift them
 * to relative locations within the buffer then in theory we could  
 * be before the final pulse in this interrupt which would make the
 * origin wrong.  An obscure edge condition, don't worry about it.
 * This is only relevant if the tracker is unlocked.
 */
PUBLIC void Synchronizer::loopRecordStart(Loop* l)
{
    Track* track = l->getTrack();
    SyncState* state = track->getSyncState();

    if (state->isRecording()) {
        // must have been a pulsed start, SyncState was
        // initialized above in startRecording()
    }
    else {
        // a scheduled start
        SyncSource src = state->getEffectiveSyncSource();
        if (src != SYNC_NONE) {

            int originPulse = 0;
            if (src == SYNC_MIDI) 
              originPulse = mMidiTracker->getPulse();
            else if (src == SYNC_HOST)
              originPulse = mHostTracker->getPulse();

            // For SYNC_OUT it doesn't matter what the cycle pulses are
            // because we're defining the cycle length in real time, 
            // could try to guess based on a predefined tempo.
            //
            // !! Should be here for AutoRecord where we can know the pulse
            // count and start sending clocks immediately
            //
            // !! for anything other than SYNC_OUT this is broken because
            // counting pulses isn't accurate, we need to check the actual
            // recorded size.  
            int cyclePulses = 0;

            // have to know whether the tracker was locked at the start of this
            // so we can consistently follow raw or tracker pulses
            // !! I'm hating the SyncState interface
            bool trackerLocked = false;
            SyncTracker* tracker = getSyncTracker(src);
            if (tracker != NULL)
              trackerLocked = tracker->isLocked();

            state->startRecording(originPulse, cyclePulses, 
                                  getBeatsPerBar(src, l),
                                  trackerLocked);
        }
    }

    // this is an inconsistency with Reset
    // if Reset is allowed to select a different master, why not rerecord?
    // I guess you could say the intent is clearer to stay here with rerecord

	if (track == mOutSyncMaster) {
        mTransport->fullStop(l, "Sync: Master track re-record: Stop clocks and send MIDI Stop\n");

        // clear state state from the tracker
        mOutTracker->reset();
	}
}

/**
 * Called by RecordFunction when the RecordStopEvent has been processed 
 * and the loop has been finalized.
 * 
 * IF this is a synchronized recording, SyncState will normally have the
 * final pulse count and loop frames for the tracker.  Claim the tracker 
 * if we can.  For the out sync master, calculate the tempo
 * and begin sending MIDI clocks.
 *
 * OUT SYNC NOTES
 * 
 * This is expected to be called when we're really finished
 * with the recording *not* during the InputLatency delay period.
 * There are too many places where the internal clock is being
 * controlled in "loop event time" rather than "real time" that we
 * have to do it consistently.  Ideally we would schedule events
 * for clock control in advance, similar to the JumpPlay event
 * but that is quite complicated, and at ASIO latencies, provides
 * very little gain.  The best we can do is be more accurate in our
 * initial drift calculations.
 *
 * UPDATE: Reconsider this.  Stopping clocks isn't that critical
 * we can do that before or after latency.  Now that we usually
 * follow the SyncTracker it doesn't matter as much?
 * 
 * Restarting or continuing ideally should be done before latency.
 * I suppose we could do that from the JumpPlay event.  This wouldn't
 * happen much: MidiStart after ManualStart=true and certain mutes that
 * stop the clock.
 *
 * Changing the clock tempo should ideally be done pre-latency, but this
 * only matters if we're trying to maintain a loop-accurate pulse frame.
 * With the new SyncState, we can change the tempo any time and adjust
 * the internal framesPerPulse.
 * 
 */
PUBLIC void Synchronizer::loopRecordStop(Loop* l, Event* stop)
{
	Track* track = l->getTrack();
    SyncState* state = track->getSyncState();
    SyncTracker* tracker = getSyncTracker(l);

    if (tracker == NULL) {
        // must be TRACK sync or something without a tracker
        // !! the only state we have to convey is the relative
        // starting location of the loop, actually need to save this
        // for tracker loops too...
    }
    else if (tracker->isLocked()) {
        // Sanity check on the size.
        // If the tracker was locked from the begging we will have been
        // following its pulses and should be an exact multiple of the beat.
        // If the tracker was not locked from the beginning we followed
        // raw pulses and may not be very close.  It's too late to do 
        // anything about it now, should try to fix this when the trakcer
        // is closed.

        // we were following pulses, calculate the amount of noise
        float pulseFrames = tracker->getPulseFrames();
        float trackerPulses = (float)l->getFrames() / pulseFrames;
        int realPulses = (int)trackerPulses;
        float noise = trackerPulses - (float)realPulses;

        // Noise is often a very small fraction even if we were following
        // a locked tracker since we calculated from pulseFrames which
        // is a float approximation.  Don't trace unless the noise level 
        // is relatively high:
        long inoise = (long)(noise * 1000);
        if (noise != 0) {
            int level = 2;
            if (state->wasTrackerLocked())
              level = 1;
                
            Trace(l, level, "WARNING: Sync recording deviates from master loop %ld (x1000)\n",
                  inoise);
        }
    }
    else if (tracker == mOutTracker) {
        // locking the out tracker means we're also becomming
        // the out sync master, this is more complicated due to 
        // tempo rounding
        Trace(l, 2, "Sync: master track %ld record stopping\n", 
              (long)track->getDisplayNumber());

        // logic error? how can this be set but the tracker be unlocked?
        if (mOutSyncMaster != NULL && mOutSyncMaster != track)
          Trace(l, 1, "Sync: Inconsistent OutSyncMaster and OutSyncTracker!\n");

        lockOutSyncTracker(l, true);
        informFollowers(tracker, l);
    }
    else {
        // Host or MIDI
        if (!state->isRounding()) {
            // We should always be in rounding mode with known tracker state.
            // If the record ending was pulsed, activateRecordStop will have
            // called SyncState::schedulStop because there may have been
            // a latency delay.
            Trace(l, 1, "Sync: Missing tracker state for locking!\n");
        }
        else {
            // !! how is speed supposed to factor in here?  we only use
            // it to detect speed changes when resizeOutSyncTracker is called
            // but this feels inconsistent with the other places we lock
            tracker->lock(l, state->getOriginPulse(),
                          state->getTrackerPulses(), 
                          state->getTrackerFrames(),
                          getSpeed(l),
                          state->getTrackerBeatsPerBar());

            // advance the remaining frames in this buffer
            // this should not be returning any events
            tracker->advance(track->getRemainingFrames(), NULL, NULL);
            informFollowers(tracker, l);
        }
    }

    // any loop can become the track sync master
    if (mTrackSyncMaster == NULL)
      setTrackSyncMaster(track);

    // don't need this any more
    state->stopRecording();
}

/**
 * After locking a SyncTracker, look for other tracks that were 
 * actively following it before it was locked.
 *
 * If they were in Synchronize mode, we simply switch over to follow 
 * tracker pulses.  
 *
 * If they were in Record mode it's more complicated because they've
 * been counting raw pulses and may have even had the ending scheduled.
 * It will not necessarily match the locked tracker.  Should try to 
 * get in there and adjust them...
 */
PRIVATE void Synchronizer::informFollowers(SyncTracker* tracker, Loop* loop)
{
    int tcount = mMobius->getTrackCount();
    for (int i = 0 ; i < tcount ; i++) {
        Track* t = mMobius->getTrack(i);
        SyncState* state = t->getSyncState();
        if (t != loop->getTrack() && 
            state->getEffectiveSyncSource() == tracker->getSyncSource() &&
            !isTrackReset(t)) {

            // some other track was following
            Loop* other = t->getLoop();
            MobiusMode* mode = other->getMode();
            if (mode != RecordMode) {
                Trace(loop, 2, "Sync: Track %ld also followign newly locked tracker\n",
                      (long)t->getDisplayNumber());
            }
            else {
                // If we're using focus lock this isn't a problem, the
                // sizes will end up identical.  This isn't always true since
                // focus lock could have been set during recording, but this
                // traces the most interesting case.
                int level = 2;
                Track* winner = loop->getTrack();
                if (!t->isFocusLock() && t->getGroup() != winner->getGroup())
                  level = 1;

                Trace(loop, level, "Sync: Track %ld was recording and expecting to lock tracker\n",
                      (long)t->getDisplayNumber());
            }
        }
    }
}

/****************************************************************************
 *                                                                          *
 *                            LOOP RESET CALLBACK                           *
 *                                                                          *
 ****************************************************************************/

/**
 * Called by loop when the loop is reset.
 * If this track is the out sync master, turn off MIDI clocks and reset
 * the pulse counters so we no longer try to maintain alignment.
 *
 * TODO: Want an option to keep the SyncTracker going with the last tempo
 * until we finish the new loop?
 *
 * If this the track sync master, then reassign a new master.
 */
PUBLIC void Synchronizer::loopReset(Loop* loop)
{
    Track* track = loop->getTrack();
    SyncState* state = track->getSyncState();

    // initialize recording state
    state->stopRecording();

    if (track == mTrackSyncMaster)
      setTrackSyncMaster(findTrackSyncMaster());

	if (track == mOutSyncMaster) {
        mTransport->fullStop(loop, "Sync: Master track reset, stop clocks and send MIDI Stop\n");

        mOutTracker->reset();
        setOutSyncMaster(findOutSyncMaster());
	}

    // unlock if no other loops
    if (isTrackReset(track))
      state->unlock();

    unlockTrackers();
}

/**
 * Return true if all loops in this track are reset.
 * TODO: move this to Track!!
 */
PRIVATE bool Synchronizer::isTrackReset(Track* t)
{
    bool allReset = true;
    int lcount = t->getLoopCount();
    for (int i = 0 ; i < lcount ; i++) {
        Loop* l = t->getLoop();
        if (!l->isReset()) {
            allReset = false;
            break;
        }
    }
    return allReset;
}

/**
 * Check to see if any of the trackers can be unlocked after
 * a loop has been reset.
 */
PRIVATE void Synchronizer::unlockTrackers()
{
    unlockTracker(mOutTracker);
    unlockTracker(mMidiTracker);
    unlockTracker(mHostTracker);
}

/**
 * Check to see if a tracker can be unlocked after a loop has been reset.
 * All tracks that follow this tracker must be completely reset.
 */
PRIVATE void Synchronizer::unlockTracker(SyncTracker* tracker)
{
    if (tracker->isLocked()) {
        int uses = 0;
        int tcount = mMobius->getTrackCount();
        for (int i = 0 ; i < tcount ; i++) {
            Track* t = mMobius->getTrack(i);
            SyncState* state = t->getSyncState();
            if (state->getEffectiveSyncSource() == tracker->getSyncSource() &&
                !isTrackReset(t))
              uses++;
        }
        if (uses == 0)
          tracker->reset();
    }
}

/****************************************************************************
 *                                                                          *
 *                           LOOP RESIZE CALLBACKS                          *
 *                                                                          *
 ****************************************************************************/

/**
 * Called by Loop after finishing a Multiply, Insert, Divide, 
 * or any other function that changes the loop size in such
 * a way that might impact the generated MIDI tempo if we're
 * the OutSyncMaster.
 *
 * Also called after Undo/Redo since the layers can be of different size.
 * 
 * The sync behavior is controlled by the ResizeSyncAdjust parameter.
 * Normally we don't do anything, the SyncTracker continues incrementing as 
 * before, the external and internal loops may go in and out of phase
 * but we will still monitor and correct drift.
 *
 * If ResizeSyncAdjust=Tempo, we change the output sync tempo so that
 * it matches the new loop length, thereby keeping the external and
 * internal loops in sync and in phase.  
 *
 * NOTES FROM loopChangeLoop
 *
 * If we switch to an empty loop, the tempo remains the same and we
 * keep sending clocks, but we don't treat this like a Reset and send
 * STOP.  Not sure what the EDP does.  Keep the external pulse counter
 * ticking so we can keep track of the external start point.  
 *
 */
PUBLIC void Synchronizer::loopResize(Loop* l, bool restart)
{
    if (l->getTrack() == mOutSyncMaster) {

        Trace(l, 2, "Sync: loopResize\n");

        Setup* setup = mMobius->getInterruptSetup();
        SyncAdjust mode = setup->getResizeSyncAdjust();

		if (mode == SYNC_ADJUST_TEMPO)
          resizeOutSyncTracker();

        // The EDP sends START after unrounded multiply to bring
        // the external device back in sync (at least temporarily)
        // switching loops also often restart
        // !! I don't think this should obey the ManualStart option?

        if (restart) {
            Trace(l, 2, "Sync: loopResize restart\n");
            sendStart(l, true, false);
        }
    }
}

/**
 * Called when we switch loops within a track.
 */
PUBLIC void Synchronizer::loopSwitch(Loop* l, bool restart)
{
    if (l->getTrack() == mOutSyncMaster) {

        Trace(l, 2, "Sync: loopSwitch\n");
        
        Setup* setup = mMobius->getInterruptSetup();
        SyncAdjust mode = setup->getResizeSyncAdjust();

		if (mode == SYNC_ADJUST_TEMPO) {
            if (l->getFrames() > 0)
              resizeOutSyncTracker();
            else {
                // switched to an empty loop, keep the tracker going
                Trace(l, 2, "Sync: Switch to empty loop\n");
            }
        }

        // switching with one of the triggering options sends START
        // !! I don't think this should obey the ManualStart option?
        if (restart) {
            Trace(l, 2, "Sync: loopSwitch restart\n");
            sendStart(l, true, false);
        }
    }
}

/**
 * Called by Loop when we make a speed change.
 * The new speed has already been set.
 * If we're the OutSyncMaster this may adjust the clock tempo.
 * 
 */
PUBLIC void Synchronizer::loopSpeedShift(Loop* l)
{
    if (l->getTrack() == mOutSyncMaster) {

        Trace(l, 2, "Sync: loopSpeedShift\n");

        Setup* setup = mMobius->getInterruptSetup();
        SyncAdjust mode = setup->getSpeedSyncAdjust();

        if (mode == SYNC_ADJUST_TEMPO)
          resizeOutSyncTracker();
    }
}

/****************************************************************************
 *                                                                          *
 *                          LOOP LOCATION CALLBACKS                         *
 *                                                                          *
 ****************************************************************************/
/*
 * Callbacks related to changint he location within a loop or 
 * starting and stopping the loop.  These can effect the MIDI transport
 * messages we send if we are the out sync master.
 *
 */

/**
 * Called by Loop when it enters a pause.
 * If we're the out sync master send an MS_STOP message.
 * !! TODO: Need an option to keep the clocks going during pause?
 */
PUBLIC void Synchronizer::loopPause(Loop* l)
{
	if (l->getTrack() == mOutSyncMaster)
      muteMidiStop(l);
}

/**
 * Called by Loop when it exits a pause.
 */
PUBLIC void Synchronizer::loopResume(Loop* l)
{
	if (l->getTrack() == mOutSyncMaster) {

        Setup* setup = mMobius->getInterruptSetup();
        MuteSyncMode mode = setup->getMuteSyncMode();

        if (mode == MUTE_SYNC_TRANSPORT ||
            mode == MUTE_SYNC_TRANSPORT_CLOCKS) {
            // we sent MS_STOP, now send MS_CONTINUE
            mTransport->midiContinue(l);
        }
        else  {
            // we just stopped sending clocks, resume them
            mTransport->startClocks(l);
        }
	}
}

/**
 * Called by Loop when it enters Mute mode.
 * 
 * When MuteMode=Start the EDP would stop clocks then restart them
 * when we restart comming out of mute.  Feels like another random
 * EDPism we don't necessarily want, should provide an option to keep
 * clocks going and restart later.
 */
PUBLIC void Synchronizer::loopMute(Loop* l)
{
	if (l->getTrack() == mOutSyncMaster) {
		Preset* p = l->getPreset();
		if (p->getMuteMode() == Preset::MUTE_START) 
          muteMidiStop(l);
	}
}

/**
 * After entering Mute or Pause modes, decide whether to send
 * MIDI transport commands and stop clocks.  This is controlled
 * by an obscure option MuteSyncMode.  This is for dumb devices
 * that don't understand STOP/START/CONTINUE messages.
 * 
 */
PRIVATE void Synchronizer::muteMidiStop(Loop* l)
{
    Setup* setup = mMobius->getInterruptSetup();
    MuteSyncMode mode = setup->getMuteSyncMode();

    bool transport = (mode == MUTE_SYNC_TRANSPORT ||
                      mode == MUTE_SYNC_TRANSPORT_CLOCKS);
    
    bool clocks = (mode == MUTE_SYNC_CLOCKS ||
                   mode == MUTE_SYNC_TRANSPORT_CLOCKS);

    mTransport->stop(l, transport, clocks);
}

/**
 * Called by Loop when the loop is being restarted from the beginning.
 * This happens in three cases:
 * 
 *   - Mute cancel when MuteMode=Start
 *   - SpeedStep when SpeedShiftRestart=true
 *   - PitchShift when PitchShiftRestart = true
 *
 * NOTE: The Restart function will be handled as a Switch and end
 * up in loopResize with the restart flag set.
 *
 * ?? Would it be interesting to have a mode where Restart does not
 * restart the external loop?  Might be nice if we're just trying
 * to tempo sync effects boxes, and MidiStart confuses them.
 */
PUBLIC void Synchronizer::loopRestart(Loop* l)
{
	if (l->getTrack() == mOutSyncMaster) {

		Trace(l, 2, "Sync: loopRestart\n");
        // we have historically tried to suppress a START message
        // if were already near it
        sendStart(l, true, true);
	}
}

/**
 * Called when a MidiStartEvent has been processed.
 * These are schedueld by the MidiStart and MuteMidiStart functions
 * as well as a Multiply alternate ending to Mute.  This is what you
 * use to get things started when ManualStart=true.
 * 
 * The event is normally scheduled for the loop start point (actually
 * the last frame in the loop).  The intent is then to send a MIDI Start
 * to resync the external device with the loop.  
 */
PUBLIC void Synchronizer::loopMidiStart(Loop* l)
{
	if (l->getTrack() == mOutSyncMaster) {
		// here we always send Start
        // we have historically tried to suppress a START message
        // if were already near it
		sendStart(l, false, true);
	}
}

/**
 * Called by Loop when it evaluates a MidiStopEvent.
 *
 * Also called by the MuteRealign function after it has scheduled 
 * a pending Realign event and muted.  The EDP supposedly stops
 * clocks when this happens, we keep them going but want to send
 * an MS_STOP.
 * 
 * For MidiStopEvent force is true since it doesn't matter what
 * sync mode we're in.
 * 
 * We do not stop the clocks here, keep the pulses comming so we
 * can check drift.
 *
 * !! May want a parameter like MuteSyncMode to determine
 * whether to stop the clocks or just send stop/start.
 * Might be useful for unintelligent devices that just
 * watch clocks?
 */
PUBLIC void Synchronizer::loopMidiStop(Loop* l, bool force)
{
    if (force || (l->getTrack() == mOutSyncMaster))
      mTransport->stop(l, true, false);
}

/**
 * Called by loop when the start point is changed.
 * If we're the out sync master, send MS_START to the device to 
 * bring it into alignment.  
 *
 * TODO: As always may want a parameter to control this?
 */
PUBLIC void Synchronizer::loopSetStartPoint(Loop* l, Event* e)
{
    if (l->getTrack() == mOutSyncMaster) {
        Trace(l, 2, "Sync: loopChangeStartPoint\n");
        sendStart(l, true, false);
    }
}

/**
 * Unit test function to force a drift.
 */
PUBLIC void Synchronizer::loopDrift(Loop* l, int delta)
{
    SyncTracker* tracker = getSyncTracker(l);
    if (tracker != NULL)
      tracker->drift(delta);
    else 
      Trace(l, 2, "Sync::loopDrift track does not follow a drift tracker\n");
}

/****************************************************************************
 *                                                                          *
 *                          LOOP AND PROJECT LOADING                        *
 *                                                                          *
 ****************************************************************************/

/**
 * This must be called whenever a project has finished loading.
 * Since we won't be recording loops in the usual way we have to 
 * recalculate the symc masters.
 *
 * !! The Project should be saving master selections.
 * !! Way more work to do here for SyncTrackers...project
 * needs to save the SyncTracker state if closed we can guess here but
 * it may not be the same.
 */
PUBLIC void Synchronizer::loadProject(Project* p)
{
    // should have done a globalReset() first but make sure
    // sigh, need a TraceContext for MidiTransport
    TraceContext* tc = mMobius->getTrack(0);
    mTransport->fullStop(tc, "Sync: Loaded project, stop clocks and send MIDI Stop\n");

    mOutSyncMaster = NULL;
    mTrackSyncMaster = NULL;

    mOutTracker->reset();
    mHostTracker->reset();
    mMidiTracker->reset();

    // TODO: check ProjectTracks for master selections
    setTrackSyncMaster(findTrackSyncMaster());
    setOutSyncMaster(findOutSyncMaster());
}

/**
 * Called after a loop is loaded.
 * This may effect the assignment of sync masters or change the
 * behavior of the existing master.
 */
PUBLIC void Synchronizer::loadLoop(Loop* l)
{
    if (!l->isEmpty()) {
        Track* track = l->getTrack();

        if (mTrackSyncMaster == NULL)
          setTrackSyncMaster(track);

        if (mOutSyncMaster == NULL) {
            SyncState* state = track->getSyncState();
            if (state->getDefinedSyncSource() == SYNC_OUT)
              setOutSyncMaster(track);
        }
    }
}

/****************************************************************************
 *                                                                          *
 *   						  SYNC MASTER TRACKS                            *
 *                                                                          *
 ****************************************************************************/

/**
 * Return the current track sync master.
 */
PUBLIC Track* Synchronizer::getTrackSyncMaster()
{
	return mTrackSyncMaster;
}

PUBLIC Track* Synchronizer::getOutSyncMaster()
{
	return mOutSyncMaster;
}

/**
 * Ultimte handler for the SyncMasterTrack function, also called internally
 * when we assign a new sync master.
 * 
 * This one seems relatively harmless but think carefullly.
 * We're calling this directly from the UI thread, should this
 * be evented?
 *
 * We keep the master status in two places, a Track pointer here
 * and a flag on the Track.  Hmm, this argues for eventing, 
 * we'll have a small window where they're out of sync.
 */
PUBLIC void Synchronizer::setTrackSyncMaster(Track* master)
{
	if (master != NULL) {
		if (mTrackSyncMaster == NULL)
		  Trace(master, 2, "Sync: Setting track sync master %ld\n",
				(long)master->getDisplayNumber());

		else if (master != mTrackSyncMaster)
		  Trace(master, 2, "Sync: Changing track sync master from %ld to %ld\n",
				(long)mTrackSyncMaster->getDisplayNumber(), (long)master->getDisplayNumber());
	}
	else if (mTrackSyncMaster != NULL) {
		Trace(mTrackSyncMaster, 2, "Sync: Disabling track sync master %ld\n",
			  (long)mTrackSyncMaster->getDisplayNumber());

        // TODO: Should we remove any SYNC_TYPE_TRACK pulse events
        // for the old track that were left on mInterruptEvents?  
        // I think it shouldn't matter since changing the master is
        // pretty serious and if you do it at exactly the moment    
        // a pending Realign pulse happens, you may not get the alignment
        // you want.   Only change masters when the system is relatively
        // quiet.
	}

	mTrackSyncMaster = master;
}

/**
 * Ultimte handler for the SyncMasterMidi function, also called internally
 * when we assign a new master.
 * 
 * This is much more complicated, and probably must be evented.
 */
PUBLIC void Synchronizer::setOutSyncMaster(Track* master)
{
    setOutSyncMasterInternal(master);

    // control flow is a bit obscure but this will lock or
    // resize the OutSyncTracker
    resizeOutSyncTracker();
}

/**
 * Internal method for assigning the out sync master.  This just
 * does the trace and changes the value.  Higher order semantics
 * like SyncTracker management must be done by the caller.
 */
PRIVATE void Synchronizer::setOutSyncMasterInternal(Track* master)
{
	if (master != NULL) {
		if (mOutSyncMaster == NULL)
		  Trace(master, 2, "Sync: Assigning output sync master %ld\n",
				(long)master->getDisplayNumber());

		else if (master != mOutSyncMaster)
		  Trace(master, 2, "Sync: Changing output sync master from %ld to %ld\n",
				(long)mOutSyncMaster->getDisplayNumber(), (long)master->getDisplayNumber());
	}
	else if (mOutSyncMaster != NULL) {
		Trace(mOutSyncMaster, 2, "Sync: Disabling output sync master %ld\n",
			  (long)mOutSyncMaster->getDisplayNumber());
	}

	mOutSyncMaster = master;
}

/**
 * Find a track able to serve as the SYNC_TRACK master.
 * It doesn't matter what the SyncSource is, the first track
 * we find that isn't empty is the default sync master.
 */
PRIVATE Track* Synchronizer::findTrackSyncMaster()
{
	Track* master = NULL;

	int tcount = mMobius->getTrackCount();
	for (int i = 0 ; i < tcount ; i++) {
		Track* t = mMobius->getTrack(i);
        SyncState* state = t->getSyncState();
		Loop* l = t->getLoop();

		// !! in theory we have the "latency delay" state before the
		// record starts here?
		MobiusMode* mode = l->getMode();
		bool recording = 
			(mode == RecordMode || 
			 mode == ThresholdMode ||
			 mode == SynchronizeMode);

        // Formerly called t->isEmpty which returns true if there is 
        // ANY non-empty loop in the track.  I don't know why I did
        // this but it seems more logical to pick a track that is actually
        // playing now.
		//bool empty = t->isEmpty();
        bool empty = l->isEmpty();

		if ((!empty || recording) && (t == mTrackSyncMaster || master == NULL))
		  master = t;
	}

    return master;
}

/**
 * Find a track able to serve as the SYNC_OUT master.
 */
PRIVATE Track* Synchronizer::findOutSyncMaster()
{
	Track* master = NULL;

	int tcount = mMobius->getTrackCount();
	for (int i = 0 ; i < tcount ; i++) {
		Track* t = mMobius->getTrack(i);
        SyncState* state = t->getSyncState();

        if (state->getDefinedSyncSource() == SYNC_OUT) {
            // if the track was a sync master and isn't empty, let it continue

            // Formerly called t->isEmpty which returns true if there is 
            // ANY non-empty loop in the track.  I don't know why I did
            // this but it seems more logical to pick a track that is actually
            // playing now.
            //bool empty = t->isEmpty();
            Loop* l = t->getLoop();
            bool empty = l->isEmpty();

            if (!empty && (t == mOutSyncMaster || master == NULL))
              master = t;

        }
    }

    return master;
}

/****************************************************************************
 *                                                                          *
 *   							   OUT SYNC                                 *
 *                                                                          *
 ****************************************************************************/

/**
 * Called whenever the size of the out sync master track changes.
 * This can happen for many reasons.  Functions that alter the loop
 * size or cycle size (Multiply, Insert, Divide).  Functions that move
 * between layers that may be of different sizes (Undo, Redo).  Functions
 * that move between loops of different sizes (NextLoop, PrevLoop, LoopX).
 * Functions that replace the contents of a loop (LoadLoop, 
 * LoadProject, Bounce).
 *
 * It doesn't matter here what caused the resize, we look at the
 * new size of the master track's loop and compare it to the loop
 * size in the OutSyncTracker.  If they are not compatible, then a tempo
 * adjustment must be made and the tracker resized.
 */
PRIVATE void Synchronizer::resizeOutSyncTracker()
{
    if (mOutSyncMaster == NULL) {
        // This normally happens only when you reset the master
        // track and there are no others to choose from.
        // It could also happen if you forced an empty track to be the
        // master.  Try to avoid this in the caller.
        Trace(2, "Sync:resizeOutSyncTracker with no master track\n");
    }
    else {
        Loop* l = mOutSyncMaster->getLoop();

        // start from cycle frames rather than the full loop
        // !! really? always? may want control over how many "bars"
        // there are in the external loop so we don't have to rely on
        // tempo wrapping to record a multi-bar loop and get the right
        // tempo
        long newFrames = l->getCycleFrames();
        if (newFrames == 0) {
            // This can happen if you're switching loops within the
            // master track and some are empty.  Leave the old loop
            // size in place.
            Trace(l, 2, "Sync:resizeOutSyncTracker empty loop\n");
        }
        else {
            if (!mOutTracker->isLocked()) {
                // first time here, just lock it
                // this can happen when we load loops or projects rather
                // than record them, and maybe when setting the master manually
                Trace(l, 2, "Sync: Locking master track after loading\n");
                lockOutSyncTracker(l, false);
            }
            else {
                // if either is a perfect multiple of the other, ignore
                // note that we have to use the "future" accessors since
                // there could be several resize events rapidly before the 
                // next pulse
                bool resize = false;
                long trackerFrames = mOutTracker->getFutureLoopFrames();

                if (newFrames > trackerFrames) {
                    // If new size is greater, it is okay as long as
                    // it remains an even multiple of the original cycle size.
                    resize = ((newFrames % trackerFrames) != 0);
                }
                else if (trackerFrames > newFrames) {
                    // If size is less then the original cycle was cut. 
                    // We could also keep the tempo if the tracker is
                    // an even multiple of the new cycle size, but here
                    // it seems more like you want to double speed.
                    // Could have an option for this.
                    bool alwaysKeepTempo = false;
                    if (alwaysKeepTempo)
                      resize = ((trackerFrames % newFrames) != 0);
                    else
                      resize = true;
                }

                // speed changes always force a resize even if the fundamental
                // cycle length doesn't change
                float speed = getSpeed(l);

                if (resize || speed != mOutTracker->getFutureSpeed()) {

                    if (speed != 1.0)
                      newFrames = (long)((float)newFrames / speed);

                    // calculate preferred tempo and pulses
                    int pulses = 0;
                    float tempo = calcTempo(l, mOutTracker->getBeatsPerBar(),
                                            newFrames, &pulses);

                    Trace(l, 2, "Sync: master track %ld resizing to %ld frames, tempo (x100) %ld\n",
                          (long)mOutSyncMaster->getDisplayNumber(), 
                          (long)newFrames,
                          (long)(tempo * 100));

                    // Transport won't change tempo until after generating
                    // the next clock, Tracker will wait for that before
                    // resizing
                    mOutTracker->resize(pulses, newFrames, speed);
                    mTransport->setTempo(l, tempo);
                }
            }
        }
    }
}

/**
 * Lock the out sync tracker.
 * This called at the end of loopRecordStop if we determine that we
 * can be the out symc master.  It will also be called by 
 * resizeOutSyncTracker if the tracker is currently unlocked.
 *
 * The recordStop flag is set if we're called from loopRecordStop.
 *
 * !! Determine if we need the recorStop flag and the difference in 
 * behavior.  We need to be factoring in speed shift when speed shift
 * causes the assignment of a new sync master which locks the tracker.
 * 
 */
PRIVATE void Synchronizer::lockOutSyncTracker(Loop* l, bool recordStop)
{
    // don't call this if already locked
    if (mOutTracker->isLocked()) {
        Trace(1, "Sync: Don't call this if the tracker is locked!\n");
    }
    else {
        // If this was AutoRecord, we may have precalculated a frame and pulse
        // count and left SyncState rounding.  We don't really need that, just
        // work from the final cycle size which may have to be rounded for
        // tempo and adjusted for speed.

        long trackerFrames = l->getCycleFrames();

        // resizeOutSyncTracker factors in speed shift, do we need that here?!!
        if (!recordStop) {
            float speed = getSpeed(l);
            if (speed != 1.0)
              trackerFrames = (long)((float)trackerFrames / speed);
        }

        int pulses = 0;
        int bpb = getBeatsPerBar(SYNC_NONE, l);
        float tempo = calcTempo(l, bpb, trackerFrames, &pulses);

        mOutTracker->lock(l, 0, pulses, trackerFrames, getSpeed(l), bpb);
        // temporary debugging
        Track* t = l->getTrack();
        mOutTracker->setMasterTrack(t);

        Trace(l, 2, "Sync: Locked Out tracker at loop frame %ld\n",
              l->getFrame());

        // advance the remaining frames in the buffer
        // if we're going to send START now, this doesn't matter since we'll 
        // immediately reset the frame counter back to zero on the 
        // next interrupt, but be consistent with the other trackers
        if (recordStop) {
            long advance = t->getRemainingFrames();
            Trace(l, 2, "Sync: initial tracker audio frame advance %ld\n", advance);
            mOutTracker->advance(advance, NULL, NULL);
        }

        mTransport->setTempo(l, tempo);
        
        // if this isn't ManualStart=true, send the MS_START message now
        SyncState* state = t->getSyncState();
        if (!state->isManualStart())
          mTransport->start(l);
        else
          mTransport->startClocks(l);

        // must keep these in sync
        if (t != mOutSyncMaster)
          setOutSyncMasterInternal(t);
    }
}

/**
 * Helper to calculate the tempo and number of sync pulses
 * from a span of frames.  This is normally the length of one "cycle"
 * of the loop, though when creating the initial loop this will be the
 * full length of the master loop.
 *
 * Speed is not factored in here, if you need to adjust for speed it should
 * be factored into the given frame length.
 *
 *    framesForTempo = trueFrames / speed
 *
 * For example with a 120,000 frame loop recorded at 1/2 speed, the effective
 * size of the loop for tempo calculations is 240,000.
 *
 * Without speed adjustment 120,000 at 4 beats per bar results in a tempo of 88.2
 *
 * With 240,000 frames the tempo becomes 44.1, which is 1/2 of 88.2.
 *
 * The number of pulses returned is usually take from the beatsPerBar parameter.
 * 
 * TODO: May need anoter setup parameter for RecordBars in case the external
 * pattern is several bars long, but usually rounding should fix that?
 * It doesn't really matter what the pulse count is, it is really just
 * a starting point that we carry over to the SyncTracker, it does not 
 * necessarily have to match the number of beats in a logical measure.
 */
PRIVATE float Synchronizer::calcTempo(Loop* l, int beatsPerBar, long frames, 
                                      int* retPulses)
{
	float tempo = 0.0;
	int pulses = 0;

	if (frames > 0) {

        Setup* setup = mMobius->getInterruptSetup();
        // SyncState should already have figured out the beat count
        Track* t = l->getTrack();
        SyncState* state = t->getSyncState();

        // 24 MIDI clocks per beat
		pulses = beatsPerBar * 24;
		
        // original forumla
        // I don't know how I arrived at this, it works but
        // it is too obscure to explain in the docs
		//float cycleSeconds = (float)frames /  (float)CD_SAMPLE_RATE;
		//tempo = (float)beatsPerBar * (60.0f / cycleSeconds);

        // more obvoius formula
        float framesPerBeat = (float)frames / (float)beatsPerBar;
        float secondsPerBeat = framesPerBeat / (float)mMobius->getSampleRate();
        tempo = 60.0f / secondsPerBeat;

		float original = tempo;
		float fpulses = (float)pulses;

		// guard against extremely low or high values
		// allow these to be floats?
		int min = setup->getMinTempo();
		int max = setup->getMaxTempo();

		if (max < SYNC_MIN_TEMPO || max > SYNC_MAX_TEMPO)
		  max = SYNC_MAX_TEMPO;

		if (min < SYNC_MIN_TEMPO || min > SYNC_MAX_TEMPO)
		  min = SYNC_MIN_TEMPO;

		while (tempo > max) {
			tempo /= 2.0;
			fpulses /= 2.0;
		}

		// if a conflicting min/max specified, min wins
		while (tempo < min) {
			tempo *= 2.0;
			fpulses *= 2.0;
		}
        
        Trace(l, 2, "Sync: calcTempo frames %ld beatsPerBar %ld pulses %ld tempo %ld (x100)\n",
              frames, (long)beatsPerBar, (long)fpulses, (long)(tempo * 100));

		if (tempo != original) {
			Trace(l, 2, "Sync: calcTempo wrapped from %ld to %ld (x100) pulses from %ld to %ld\n",
				  (long)(original * 100),
				  (long)(tempo * 100), 
				  (long)pulses, 
				  (long)fpulses);

            // care about roundoff?
            // !! yes we do...need to have a integral number of pulses and
            // we ordinally will unless beatsPerBar is odd
            float intpart;
            float frac = modff(fpulses, &intpart);
            if (frac != 0) {
                Trace(l, 1, "WARNING: Sync: non-integral master pulse count %ld (x10)\n",
                      (long)(fpulses * 10));
            }
            pulses = (int)fpulses;
		}
	}

	if (retPulses != NULL) *retPulses = pulses;
    return tempo;
}

/**
 * Helper for several loop callbacks to send a MIDI start event
 * to the external device, and start sending clocks if we aren't already.
 * The tempo must have already been calculated.
 *
 * If the checkManual flag is set, we will only send the START
 * message if the ManualStart setup parameter is off.  
 *
 * If the checkNear flag is set, we will suppress sending START
 * if the tracker says we're already 
 */
PRIVATE void Synchronizer::sendStart(Loop* l, bool checkManual, bool checkNear)
{
	bool doStart = true;

	if (checkManual) {
        Setup* setup = mMobius->getInterruptSetup();
        doStart = !(setup->isManualStart());
	}

	if (doStart) {
		// To avoid a flam, detect if we're already at the external
		// start point so we don't need to send a START.
        // !! We could be a long way from the pulse, should we be
        // checking frame advance as well?
        
        bool nearStart = false;
        if (checkNear) {
            int pulse = mOutTracker->getPulse();
            if (pulse == 0 || pulse == mOutTracker->getLoopPulses())
              nearStart = true;
        }

        if (nearStart && mTransport->isStarted()) {
			// The unit tests want to verify that we at least tried
			// to send a start event.  If we suppressed one because we're
			// already there, still increment the start count.
            Trace(l, 2, "Sync: Suppressing MIDI Start since we're near\n");
			mTransport->incStarts();
        }
        else {
            Trace(l, 2, "Sync: Sending MIDI Start\n");
            mTransport->start(l);
		}
	}
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
