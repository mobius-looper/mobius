/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Multi track sequencer/recorder.
 *
 * The recorder encapsulates the MidiDev and Timer device interfaces into
 * a higher level environment for playing and mRecording MidiSequence objects.
 * 
 */

#include <stdio.h>
#include <stdlib.h>

#include "util.h"
#include "midi.h"
#include "mmdev.h"
#include "smf.h"

#include "seq.h"
#include "track.h"
#include "recording.h"

extern void seq_timer_callback(Timer *t, void *args);
extern void seq_midi_in_callback(MidiIn *in, void *args);

/****************************************************************************
 *                                                                          *
 *                                CONSTRUCTOR                               *
 *                                                                          *
 ****************************************************************************/

/**
 * Create a new sequencer object.
 * The MidiEnv provides access to the common MidiIn, MidiOut, and Timer
 * devices that all Sequencers share.  Note that because devices
 * are shared and we register callbacks, you can only have one active
 * Sequencer object in an application.  Could be smarter about
 * switching control over the devices but this is rarely necessary.
 */
PRIVATE Sequencer::Sequencer(MidiEnv* env) 
{
	mEnv                = env;
	mTracks				= NULL;
	mPlaying 			= NULL;
	mRecording 			= NULL;
	mMetronome 			= NULL;
	mCallbackbeat 		= NULL;
	mCallbacknote		= NULL;
	mCallbackwatch 		= NULL;
	mCallbackcommand 	= NULL;
	mCallbackrecord 	= NULL;
	mCallbackevent 		= NULL;
	mCallbackloop 		= NULL;
	mListener			= NULL;
	mEventMask			= 0;
	mEvents				= NULL;
	mLastEvent			= NULL;
	mEventPool			= NULL;
	mCsect				= new CriticalSection;
	timer 				= NULL;
	start_clock 		= 0;
	start_enable		= 0;
	end_clock 			= SEQ_CLOCK_INFINITE;
	end_enable 			= 0;
	punch_in			= 0;
	punch_in_enable		= 0;
	punch_out			= 0;
	punch_out_enable	= 0;
	loop_start			= 0;
	loop_start_enable 	= 0;
	loop_end			= SEQ_CLOCK_INFINITE;
	loop_end_enable 	= 0;
	rec_merge			= 0;
	rec_cut				= 0;
	running 			= 0;
	sweeping 			= 0;
	pending_stop		= 0;
	next_beat_clock 	= 0;
	next_sweep_clock 	= 0;
	debug_track_sweep	= 1;

	_echoInput			= NULL;
	_sysexInput			= NULL;
	_sysexOutput		= NULL;

	for (int i = 0 ; i < SEQ_MAX_PORT ; i++) {
		_inputs[i] = NULL;
		_outputs[i] = NULL;
	}

	_lastInput = 0;
	_defaultInput = 0;
	_lastOutput = 0;
	_defaultOutput = 0;

    // start off with a default metronome
    mMetronome = new SeqMetronome();

    // get the timer device and default MIDI devices
    mTimer = mEnv->getTimer();

    mDefaultInput = ...;
    mDefaultOutput = ...;

}

/**
 * Destroys a sequencer.  
 */
INTERFACE Sequencer::~Sequencer(void)
{
	stop();

	delete mRecording;
	delete tracks;
	delete mMetronome;

    // don't let these point back here in case they start up again
    // !! have a MidiEnv method to do this?
    mTimer->setCallback(NULL);

}

/**
 * Called to generally initialize the sequencer to some known
 * default state. If it is running, it is stopped.  The tracks
 * will be "rewound" to clock zero.  The recording will be thrown away.
 */
INTERFACE void Sequencer::reset(void)
{
	stop();

	// aren't some of these done by stop() ? 

	running 		= 0;
	sweeping 		= 0;
	pending_stop	= 0;
	start_clock 	= 0;
	end_clock		= SEQ_CLOCK_INFINITE;
	mPlaying 		= NULL;

	if (mRecording != NULL) {
		delete mRecording;
		mRecording = NULL;
	}

	mMetronome->setClock(0);
	timer->setClock(0);
}

/****************************************************************************
 *                                                                          *
 *   						   TIMER PARAMETERS                             *
 *                                                                          *
 ****************************************************************************/
/*
 * These are mostly just pass-through methods for the internal Timer.
 * Occasionally we maintain parallel state in the SeqMetronome too.
 * The names are pretty obvious, see Timer methods for more information.
 */
 
INTERFACE float Sequencer::getTempo(void) 
{
	return mTimer->getTempo();
}

INTERFACE void Sequencer::setTempo(float t) 
{
	mTimer->setTempo(t);
}

INTERFACE void Sequencer::setTempo(float t, int cpb) 
{
	mTimer->setTempo(t);
	mTimer->setResolution(cpb);
}

INTERFACE int Sequencer::getResolution(void) 
{
	return mTimer->getResolution();
}

INTERFACE void Sequencer::setResolution(int cpb) 
{
	mTimer->setResolution(cpb);
}

INTERFACE int Sequencer::getBeatsPerMeasure(void) 
{
	return mTimer->getBeatsPerMeasure();
}

INTERFACE void Sequencer::setBeatsPerMeasure(int b) 
{
	mTimer->setBeatsPerMeasure(b);
	mMetronome->setBeat(b);
}

INTERFACE int Sequencer::getClock(void) 
{
	return mTimer->getClock();
}

INTERFACE int Sequencer::getSongPosition(void) 
{
	return mTimer->getSongPosition();
}

INTERFACE void Sequencer::setClock(int c)
{
	// have to stop
	stop();

	// adjust timer, may be rounding
	mTimer->setClock(c);
	mMetronome->setClock(mTimer->getClock());
}

INTERFACE void Sequencer::setSongPosition(int psn)
{
	stop();
	mTimer->setSongPosition(psn);
	// adjust metrome based on clock after song position was set
	mMetronome->setClock(mTimer->getClock());
}

/****************************************************************************
 *                                                                          *
 *   						  TRANSPORT COMMANDS                            *
 *                                                                          *
 ****************************************************************************/

/****************************************************************************
 * MIN
 *
 * Arguments:
 *	a: 
 *	b: 
 *
 * Returns: minimum value
 *
 * Description: 
 * 
 * Simple macro to find the minimum of two integers.
 ****************************************************************************/

#define MIN(a, b) ((a < b) ? a : b)

/****************************************************************************
 * Sequencer::clockKickoff
 *
 * Arguments:
 *
 * Returns: none
 *
 * Description: 
 * 
 * Internal function to start the sequencer.  Since Timer signals our
 * process after a requsted time delay has expired, when we want to start
 * the sequencer, we first have to see if there are any events that must
 * be handled immediatley rather than waiting for Timer to timeout.
 * Once the immediate events have been taken care of, we arm Timer
 * and wait for the next event time.
 * 
 ****************************************************************************/

#define umin(x, y) ((x < y) ? x : y)

PRIVATE void Sequencer::clockKickoff(void)
{
	int now, beat, nextclock, delay, cpb;

	now = mTimer->getClock();

	// determine the next time the tracks need attention
	next_sweep_clock = getFirstSweepClock();

	// determine the next time a beat happens
	cpb = mTimer->getResolution();
	beat = (cpb - (now % cpb));
	if (beat == cpb)
	  next_beat_clock = now;
	else
	  next_beat_clock = now + beat;

	// take the smaller of the two important event times
	nextclock = umin(next_sweep_clock, next_beat_clock);

	// If the current time is AFTER the time in which we needed to 
	// do something call the timer callback as if we got a timer interrupt.

	if (now >= nextclock) {
		timerCallback();
		nextclock = umin(next_sweep_clock, next_beat_clock);
	}

	// If the callback turned the clock off, stop now, not sure under
	// what conditions this would happen.
	if (running) {
		delay = nextclock - now;
		mTimer->start(delay);
	}
}

/****************************************************************************
 * Sequencer::startInternal
 *
 * Arguments:
 *	do_callback: non-zero to call "command" callback
 *
 * Returns: none
 *
 * Description: 
 * 
 * Starts the recorder at its current position.
 * The "internal" designation is necessary to allow control over the
 * command callback.  if this reorder has not been activated, nothing happens.
 * if the recorder is already running nothing happens.
 * To position the recorder, first call setClock, setSongPosition etc.
 * 
 * Note well! This can be called within the seq_timer_callback interrupt
 * handler so be careful what you do here.
 * 
 ****************************************************************************/

PRIVATE void Sequencer::startInternal(int do_callback)
{
	if (!running) {

		// reset the tracks and setup the play list
		startTracks();

		// reset the recording state
		if (mRecording)
		  mRecording->start(mTimer->getClock());

		// be sure this is set this before we start calling the clock handlers
		running = 1;

		// call the command spy if one exists
		// second arg is "start", third arg is event count 
		if (do_callback && mCallbackcommand != NULL)
		  (*mCallbackcommand)(this, 1, 0);
		
		// do this now?
		if (mEventMask & SeqEventStart)
		  addEvent(SeqEventStart, mTimer->getClock(), 0, 0);

		// Only the default input is enabled
		// The current code isn't prepared to have two input devices
		// feeding events at the same time.

		if (_inputs[_defaultInput] != NULL)
		  _inputs[_defaultInput]->enable();

		// Call the clock handler without advancing time to get things started
		// before we enter the usual timer delay loop.
		clockKickoff();
	}
}

/****************************************************************************
 * Sequencer::start
 *
 * Arguments:
 *	none: 
 *
 * Returns: none
 *
 * Description: 
 * 
 * Main command for starting the sequencer.
 * Play/Record will start from the current clock position.
 * Most of the work handled by startInternal.
 ****************************************************************************/

INTERFACE void Sequencer::start(void)
{
	// auto-activate
	(void)activate();

	// If there is a fixed start clock defined, zoom over
	// there before starting.
	if (start_enable)
	  setClock(start_clock);

	// If we're recording, and there is a loop start set, go there
	else if (mRecording != NULL && loop_start_enable)
	  setClock(loop_start);

	// now go damnit, and call the command callback
	startInternal(1);
}

/****************************************************************************
 * Sequencer::stopInternal
 *
 * Arguments:
 *	do_callback: 
 *
 * Returns: none
 *
 * Description: 
 * 
 * Internal function for stopping the recorder.
 * Optional argument allows control over if the command callback is called.
 * This is only so that the stop function can 
 * be called from within the seq_timer_handler when an edit loop
 * is being performed.  The edit loop will actually stop the clock, 
 * set the time back to the start of the loop and then start the clock
 * again.  As this is happening, we don't want to call the various 
 * callbacks functions since we aren't "really" stopping and starting.
 * Instead, the "loop" callback is called.
 ****************************************************************************/

PRIVATE void Sequencer::stopInternal(int do_callback)
{
	int neu;

	// ignore if we're not running, should have been caught by now
	if (running) {

		if (sweeping) {
			// We can't stop in the middle of a track sweep, 
			// set the delayed stop flag. Does this ever happen anymore ? 
			// I've seen this happen during the simple record test, 
			// we set pending stop on a record loop and still sweep ? 
			pending_stop = 1;
			module->getEnv()->
				message("stopInternal: delayed stop performed!\n");
		}
		else {
			running = 0;
			pending_stop = 0;

			// stop the clock
			mTimer->stop();

			// disable default midi input
			if (_inputs[_defaultInput] != NULL)
			  _inputs[_defaultInput]->disable();

			// clear up any run time state kept by the track sweeper
			stopTracks();

			// clean up the recording state, get indicator of new events
			neu = 0;
			if (mRecording != NULL)
			  neu = mRecording->stop();

			// call the command callback
			// second arg is "start", third arg is new event count
			if (do_callback && mCallbackcommand != NULL)
			  (*(mCallbackcommand))(this, 0, neu);      

			if (mEventMask & SeqEventStop)
			  addEvent(SeqEventStop, mTimer->getClock(), 0, 0);
		}
	}
}

/****************************************************************************
 * Sequencer::stop
 *
 * Arguments:
 *	none: 
 *
 * Returns: none
 *
 * Description: 
 * 
 * Main interface for stopping the sequencer.
 * stopInternal does the work, we ask it to call the command callback.
 ****************************************************************************/

INTERFACE void Sequencer::stop(void)
{
	stopInternal(1);
}

/****************************************************************************
 * Sequencer::playZero
 *
 * Arguments:
 *	none: 
 *
 * Returns: none
 *
 * Description: 
 * 
 * Convenience method to rewind the sequencer to zero and start.
 ****************************************************************************/

INTERFACE void Sequencer::playZero(void)
{
	if (!running) {
		setClock(0);
		start();
	}
}

/****************************************************************************
 * Sequencer::playRange
 *
 * Arguments:
 *	start: start clock
 *	  end: end clock
 *
 * Returns: none
 *
 * Description: 
 * 
 * Convenience method to setup a fixed start/stop point, then play.
 ****************************************************************************/

INTERFACE void Sequencer::playRange(int startclk, int endclk)
{
	if (!running) {

		if (endclk < startclk)
		  endclk = SEQ_CLOCK_INFINITE;
		
		setClock(startclk);
	    setEndClock(endclk);

		start();
	}
}

/****************************************************************************
 *                                                                          *
 *                                MIDI DEVICES                              *
 *                                                                          *
 ****************************************************************************/

INTERFACE MidiFilter *Sequencer::getFilters(int port) 
{
	return _inputs[port]->getFilters();
}

/****************************************************************************
 *                                                                          *
 *   							MISC UTILITIES                              *
 *                                                                          *
 ****************************************************************************/

/****************************************************************************
 * Sequencer::getMeasureClock
 * Sequencer::getMeasureWithClock
 *
 * Arguments:
 *
 * Returns: clock at the start of the measure
 *
 * Description: 
 * 
 * Convenience functions to perform a common calculation.
 * Measures are defined by the setBeatsPerMeasure option.
 * The first measure is numbered 0.
 ****************************************************************************/

// returns the start clock of the measure

INTERFACE int Sequencer::getMeasureClock(int measure)
{
	int cpb, bpm, clock;

	clock = 0;
	if (measure > 0) {
		cpb = mTimer->getResolution();
		bpm	= mTimer->getBeatsPerMeasure();
		clock = measure * (cpb * bpm);
	}
	return clock;
}

// returns the measure containing the given clock

INTERFACE int Sequencer::getMeasureWithClock(int clock)
{
	int cpb, bpm, measure;
	
	cpb   = mTimer->getResolution();
	bpm   = mTimer->getBeatsPerMeasure();
	measure = clock / (cpb * bpm);

	return measure;
}

/****************************************************************************
 *                                                                          *
 *   						SEQUENCE INSTALLATION                           *
 *                                                                          *
 ****************************************************************************/

/****************************************************************************
 * Sequencer::addSequence
 *
 * Arguments:
 *	s: sequence to add
 *
 * Returns: none
 *
 * Description: 
 * 
 * Adds a sequence to the sequencer.
 * 
 * We used to cache a copy of the SeqTrack in the MidiSequence object,
 * try to see if we can avoid this.
 * 
 * Hmm, we need to have a channel assigned to this track, and sequences
 * commonly come in with a -1 channel number, which is supposed to mean
 * to let the events carry their channel, but this breaks other things.
 * Need to think about this more, until then, force sequences that don't
 * have a channel to zero.  
 * 
 ****************************************************************************/

INTERFACE SeqTrack *Sequencer::addSequence(MidiSequence *s)
{
	SeqTrack *t;

	t = NULL;

	// allow them to be dynamically added ? 
	stop();
  
	// make sure we haven't already installed this, mayem ensues
	for (t = tracks ; t != NULL ; t = t->getNext()) {
		if (t->getSequence() == s)
		  break;
	}

	if (t == NULL) {
		// not installed
		t = new SeqTrack;
		t->setSequencer(this);
		t->setSequence(s);
		t->setNext(tracks);
		tracks = t;

		// formerly stored a pointer in the MidiSequence to 
		// the track...

		// hack the channel, see commentary above
		if (s->getChannel() < 0)
		  s->setChannel(0);
	}

	return t;
}

/****************************************************************************
 * Sequencer::removeTrack
 *
 * Arguments:
 *	tr: sequence to remove
 *
 * Returns: non-zero if the track was removed
 *
 * Description: 
 * 
 * Removes a track from the recorder, deleting the associated sequence.
 * If you want to keep the sequence, then use removeSequence or 
 * steal the sequence from the track.
 * 
 ****************************************************************************/

INTERFACE void Sequencer::removeTrack(SeqTrack *tr)
{
	SeqTrack *t, *prev;

	// find it
	prev = NULL;
	for (t = tracks ; t != NULL && t != tr ; t = t->getNext())
	  prev = t;

	if (t != NULL) {

		stop();
		
		// make sure the recorder isn't looking at this
		if (tr->isRecording())
		  stopRecording();
   
		if (prev == NULL)
		  tracks = t->getNext();
		else
		  prev->setNext(t->getNext());

		t->setNext(NULL);
		// deleting the track will delete the sequence!
		delete t;
	}
}

/****************************************************************************
 * Sequencer::removeSequence
 *
 * Arguments:
 *	s: sequence to remove
 *
 * Returns: non-zero if the track was removed
 *
 * Description: 
 * 
 * Like removeTrack, but returns ownership of the MidiSequence to the
 * caller.
 * 
 ****************************************************************************/

INTERFACE int Sequencer::removeSequence(MidiSequence *s)
{
	SeqTrack *t;
	int removed;
	
	removed = 0;
	t = findTrack(s);
	if (t != NULL) {
		removed = 1;

		// remove it to prevent deletion
		t->setSequence(NULL);
	 
		removeTrack(t);
	}

	return removed;
}

/****************************************************************************
 * Sequencer::clearTracks
 *
 * Arguments:
 *	none: 
 *
 * Returns: none
 *
 * Description: 
 * 
 * Removes all the sequences currently installed, and frees them.
 ****************************************************************************/

INTERFACE void Sequencer::clearTracks(void)
{
	MidiSequence *s;

	stop();
	while (tracks != NULL) {
		s = tracks->getSequence();

		// this has the side effect of deleting the track too.
		removeSequence(s);
		delete s;
	}
}

/****************************************************************************
 * Sequencer::getSequence
 *
 * Arguments:
 *	index: track index
 *
 * Returns: installed sequence
 *
 * Description: 
 * 
 * Obtains a track based on its position in the track list.
 ****************************************************************************/

INTERFACE SeqTrack *Sequencer::getTrack(int index)
{
	SeqTrack 		*tr;
	int 			i;

	tr = NULL;

	for (tr = tracks, i = 0 ; i < index && tr != NULL ; 
		 i++, tr = tr->getNext());

	return tr;
}

/****************************************************************************
 *                                                                          *
 *   							STATUS METHODS                              *
 *                                                                          *
 ****************************************************************************/
/* 
 * These are used only by GEE I think.
 * Think more about the need for these.
 */

/****************************************************************************
 * Sequencer::areEventsWaiting
 *
 * Arguments:
 *
 * Returns: non-zero if events are waiting
 *
 * Description: 
 * 
 * Odd status function that returns non-zero if the sequencer has any
 * tracks with events remaining to be played.  This is probably use
 * by GEE for auto shut off when we play the last event.
 ****************************************************************************/

INTERFACE int Sequencer::areEventsWaiting(void)
{
	SeqTrack *t;
	int waiting = 0;

	for (t = tracks ; t != NULL && !waiting ; t = t->getNext()) {
		if (t->getEvents() != NULL)
		  waiting = 1;
	}

	return waiting;
}

/****************************************************************************
 * Sequencer::areNotesHanging
 *
 * Arguments:
 *
 * Returns: non-zero if notes are being played
 *
 * Description: 
 * 
 * Odd status function that returns non-zero if there are any notes
 * currently being played for which note-off events have not been sent.
 * Probably used by GEE to implement an auto-shutoff.
 ****************************************************************************/

INTERFACE int Sequencer::areNotesHanging(void)
{
	SeqTrack *t;
	int hanging = 0;

	for (t = tracks ; t != NULL && !hanging ; t = t->getNext()) {
		if (t->getOn() != NULL)
		  hanging = 1;
	}
	
	// check the recording state too
	if (!hanging && mRecording != NULL && mRecording->getOn() != NULL)
	  hanging = 1;

	return hanging;
}

/****************************************************************************
 *                                                                          *
 *   								EVENTS                                  *
 *                                                                          *
 ****************************************************************************/

/****************************************************************************
 * Sequencer::enterCriticalSection
 *
 * Arguments:
 *
 * Returns: 
 *
 * Description: 
 * 
 * Csect transition methods.
 ****************************************************************************/

PRIVATE void Sequencer::enterCriticalSection(void)
{
	if (mCsect)
	  mCsect->enter();
}

PRIVATE void Sequencer::leaveCriticalSection(void)
{
	if (mCsect)
	  mCsect->leave();
}

/****************************************************************************
 * setEventMask
 *
 * Arguments:
 *
 * Returns: 
 *
 * Description: 
 * 
 * Assings the event mask, which controls which SeqEvents we will 
 * create as things happen.
 * 
 ****************************************************************************/

INTERFACE void Sequencer::enableEvents(int mask)
{
	mEventMask = mask;
}

/****************************************************************************
 * getEvents
 *
 * Arguments:
 *
 * Returns: 
 *
 * Description: 
 * 
 * Returns the current event list, the list is owned by the caller, 
 * and must be freed with SeqEvent::free.  Be careful about collisions
 * on this list with the interrupt handler!
 ****************************************************************************/

INTERFACE SeqEvent *Sequencer::getEvents(void)
{
	SeqEvent *evlist = mEvents;
	
	// reset the list, if the interrupt handler happens to be active,
	// it will still be appending to the end of the current list
	
	enterCriticalSection();
	if (evlist) {
		mEvents = NULL;
		mLastEvent = NULL;
	}
	leaveCriticalSection();

	return evlist;
}

/****************************************************************************
 * Sequencer::addEvent
 *
 * Arguments:
 *
 * Returns: 
 *
 * Description: 
 * 
 * Called by the interrupt handler to allocate a new SeqEvent, 
 * we try to use the pool if possible.
 ****************************************************************************/

PRIVATE void Sequencer::addEvent(SeqEventType t, int clock, int duration,
								 int value)
{
	SeqEvent *ev = NULL;

	enterCriticalSection();
	if (mEventPool != NULL) {
		ev = mEventPool;
		mEventPool = ev->getNext();
	}
	leaveCriticalSection();

	if (!ev)
	  ev = new SeqEvent(this);

	ev->setNext(NULL);
	ev->setType(t);
	ev->setClock(clock);
	ev->setDuration(duration);
	ev->setValue(value);

	enterCriticalSection();
	if (!mEvents) 
	  mEvents = ev;
	else {
		if (mLastEvent)
		  mLastEvent->setNext(ev);
		else
		  mEvents = ev;
	}
	mLastEvent = ev;
	leaveCriticalSection();
}

/****************************************************************************
 * Sequencer::freeEvents
 *
 * Arguments:
 *
 * Returns: 
 *
 * Description: 
 * 
 * Return a list of events to the pool.
 ****************************************************************************/

INTERFACE void Sequencer::freeEvents(SeqEvent *ev)
{
	SeqEvent *e, *last;

	if (ev) {

		// locate the last item in the list
		for (e = ev ; e != NULL ; e = e->getNext())
		  last = e;
		  
		if (last) {
			
			// its probably not necessary but be safe

			enterCriticalSection();
			last->setNext(mEventPool);
			mEventPool = ev;
			leaveCriticalSection();

		}
	}
}

/****************************************************************************
 * SeqEvent::free
 *
 * Arguments:
 *
 * Returns: 
 *
 * Description: 
 * 
 * Return the event to the pool maintained by the owner sequencer. 
 ****************************************************************************/

INTERFACE void SeqEvent::free(void)
{
	if (!mSequencer)
	  delete this;		// not handling lists !!!
	else
	  mSequencer->freeEvents(this);
}

/****************************************************************************
 *                                                                          *
 *   							  MIDI FILES                                *
 *                                                                          *
 ****************************************************************************/

INTERFACE MidiSequence *Sequencer::readMidiFile(const char *filename)
{
	MidiSequence *seq = NULL;

	MidiFileReader *mf = new MidiFileReader;
	try {
		seq = mf->read(module->getMidiModule(), filename);
		delete mf;
	}
	catch (AppException &e) {
		delete mf;
		throw e;
	}

	return seq;
}

INTERFACE MidiFileSummary *Sequencer::analyzeMidiFile(const char *filename)
{
	MidiFileAnalyzer *anal = new MidiFileAnalyzer();
	MidiFileSummary *sum = anal->analyze(filename);
	delete anal;
	return sum;
}

/****************************************************************************
 *                                                                          *
 *   							MIDI MESSAGES                               *
 *                                                                          *
 ****************************************************************************/

INTERFACE void Sequencer::setMidiSync(int port, int enable) 
{
	// should try to support more than one port at a time, but 
	// only need one for now...

	MidiOut *output = _outputs[port];
	mTimer->setMidiDevice(output);
	mTimer->setMidiSync(enable);
}

INTERFACE void Sequencer::sendNote(int port, int channel, int key, 
								   int velocity)
{
	// auto-activate
	(void)activate();

	MidiOut *output = _outputs[port];
	if (output != NULL) {
		if (velocity > 0)
		  output->sendNoteOn(channel, key, velocity);
		else
		  output->sendNoteOff(channel, key);
	}
}

INTERFACE void Sequencer::sendProgram(int port, int channel, int program) 
{
	// auto-activate
	(void)activate();

	MidiOut *output = _outputs[port];
	if (output != NULL)
		output->sendProgram(channel, program);
}

INTERFACE void Sequencer::sendSongSelect(int port, int song) 
{
	// auto-activate
	(void)activate();

	MidiOut *output = _outputs[port];
	if (output != NULL)
		output->sendSongSelect(song);
}

INTERFACE void Sequencer::sendControl(int port, int channel, 
									  int controller, int value) 
{
	// auto-activate
	(void)activate();

	MidiOut *output = _outputs[port];
	if (output != NULL)
		output->sendControl(channel, controller, value);
}

INTERFACE int Sequencer::sendSysex(int port, const unsigned char *buffer, int 
								   length) {
	
	int status = 0;

	// auto-activate
	(void)activate();

	MidiOut *output = _outputs[port];
	if (output != NULL)
	  status = output->sendSysex(buffer, length);

	return status;
}

INTERFACE int Sequencer::sendSysexNoWait(int port, 
										 const unsigned char *buffer, 
										 int length) 
{
	int status = 0;

	// auto-activate
	(void)activate();

	MidiOut *output = _outputs[port];
	if (output != NULL) {

		// save this so the application can get send status
		_sysexOutput = output;

		status = output->sendSysexNoWait(buffer, length);
		if (status) {
			printf("Sequencer::startSysex: sendSysex error %d\n", status);
			endSysex();
		}
	}

	return status;
}

/**
 * Synchronous sysex request/reply.
 * Use this only for relatively short things where we can poll for 
 * completion.
 */
INTERFACE int Sequencer::requestSysex(int outPort, int inPort,
									  const unsigned char *buffer, int length,
									  unsigned char *reply, int maxlen) 
{
	
	int status = 0;

	// auto-activate
	(void)activate();

	MidiIn *input = _inputs[inPort];
	MidiOut *output = _outputs[outPort];

	if (output == NULL)
	  printf("Sequencer::requestSysex Invalid output port number %d\n", 
			 outPort);

	else if (input == NULL)
	  printf("Sequencer::requestSysex Invalid input port number %d\n", 
			 inPort);

	else
	  status = output->sysexRequest(buffer, length, input, reply, maxlen);

	return status;
}

/****************************************************************************
 *                                                                          *
 *   							SYSEX REQUESTS                              *
 *                                                                          *
 ****************************************************************************/
/*
 * Asynchronous sysex request/reply.
 * This should be used for longer sysex requests, that might tie the
 * system up for more than a few seconds.  
 * 
 * The model is to start a request with startSysexRequest.
 * Periodically poll for completion with getSysexBytesReceived
 * And finally retrieve the results with getSysexResult.
 * 
 * The effect is similar to the synchronous sysex request implemented
 * by the MidiOut::sysexRequest method, but we give the application 
 * control over how the polling is performed, allowing it to launch
 * a thread.
 * 
 * I make no attempt to be smart here about concurrent sysex requests,
 * we only allow one at a time.  If you use the MidiOut or MidiIn
 * sysex interface in addition to this one, you can end up with
 * interleaved results that will confuse things.    With more work
 * we might be able to support concurrent requests by queueing 
 * multiple buffers, but the MidiIn handler will need more work.
 *
 * !! Once we assume that received sysex messages may be be broken
 * up into multiple blocks, its difficult to determine when we're done
 * unless we know exactly the number of bytes to expect.  We should
 * Be smarter here and assimilate raw sysex blocks into properly
 * formatted sysex messages.  Without this, the application will
 * have to do it.
 */

INTERFACE int Sequencer::startSysex(int outPort, int inPort,
									const unsigned char *request, int length)
{
	int error = 0;

	// auto-activate
	(void)activate();

	// Don't allow the sequencer to run while this is going on
	// we might be able to allow this, but I'd rather think about it
	// right now
	stop();

	MidiIn *in = _inputs[inPort];
	if (in == NULL) {
		printf("Sequencer::startSysex invalid in port %d\n", inPort);
		return 1;
	}

	MidiOut *out = _outputs[outPort];
	if (out == NULL) {
		printf("Sequencer::startSysex invalid out port %d\n", outPort);
		return 1;
	}

	int bytes = in->getSysexBytesReceiving();
	if (bytes > 0) {
		// actively receiving something
		// do we disallow this, or cancel the previous one?
		in->cancelSysex();
	}

	// make sure we don't have any stray replies hanging around
	in->ignoreSysex();

	in->setIgnoreSysex(false);
	in->setSysexEchoSize(length);
	in->enable();

	// save these for other methods in this family
	_sysexInput = in;
	_sysexOutput = out;

	error = out->sendSysexNoWait(request, length);
	if (error) {
		printf("Sequencer::startSysex: sendSysex error %d\n", error);
		endSysex();
	}

	return error;
}

// set up for a manual transmission

INTERFACE int Sequencer::startSysex(int inPort)
{
	int error = 0;

	// auto-activate
	(void)activate();

	// Don't allow the sequencer to run while this is going on
	// we might be able to allow this, but I'd rather think about it
	// right now
	stop();

	MidiIn *in = _inputs[inPort];
	if (in == NULL) {
		printf("Sequencer::startSysex invalid in port %d\n", inPort);
		return 1;
	}

	if (in->getSysexBytesReceiving()) {
		// do we disallow this, or cancel the previous one?
		// this should only come back false if we're still tracking something
		// which is good since canceling is expensive
		in->cancelSysex();
	}
	else {
		// make sure we don't have any stray replies hanging around
		in->ignoreSysex();
	}

	in->setIgnoreSysex(false);
	in->setSysexEchoSize(0);
	in->enable();

	// save these for other methods in this family
	_sysexInput = in;

	return error;
}

INTERFACE void Sequencer::debug(const char *msg, ...)
{
    va_list args;

    va_start(args, msg);

	module->getEnv()->vdebug(msg, args);

    va_end(args);
}

INTERFACE int Sequencer::getSysexSendStatus()
{
	int status = 0;

	if (_sysexOutput != NULL) {
	   status = _sysexOutput->getSysexBytesSent();

	   if (_sysexOutput->isSysexFinished()) {
		   // negate it so the caller knows we're done
		   status = -status;
	   }
	}

	debug("Sequencer::getSysexSendStatus %d\n", status);

	return status;
}

INTERFACE int Sequencer::getSysexBytesReceived() 
{
	int bytes = 0;

	if (_sysexInput != NULL) 
	  bytes = _sysexInput->getSysexBytesReceived();

	return bytes;
}

INTERFACE int Sequencer::getSysexBytesReceiving() 
{
	int bytes = 0;

	if (_sysexInput != NULL) 
	  bytes = _sysexInput->getSysexBytesReceiving();

	return bytes;
}

INTERFACE int Sequencer::getSysexResult(unsigned char *reply, int maxlen)
{
	int length = 0;

	if (_sysexInput != NULL) {
		SysexBuffer *sysex = _sysexInput->getOneSysex();
		if (sysex == NULL)
		  printf("Sequencer::getSysexResult with no data!\n");
		else {
			length = sysex->getLength();
			if (length > maxlen) {
			  printf("Sequencer::getSysexResult buffer overflow!\n");
			  length = 0;
			}
			else
				memmove(reply, sysex->getBuffer(), length);

			_sysexInput->freeSysex(sysex);
		}
	}

	return length;
}

INTERFACE void Sequencer::endSysex()
{
	if (_sysexOutput != NULL) {
		_sysexOutput->endSysex();
		_sysexOutput = NULL;
	}

	if (_sysexInput != NULL) {

		// if we're well behaved, this should be necessary
		if (_sysexInput->getSysexBytesReceiving() > 0) {
			_sysexInput->cancelSysex();
			// if this is the echo device, should we reenable it?
		}

		_sysexInput->setIgnoreSysex(true);
		_sysexInput->setSysexEchoSize(0);

		// only disable input if we're not also using it for echo

		if (_sysexInput != _echoInput)
		  _sysexInput->disable();

		_sysexInput = NULL;
	}
}

/****************************************************************************
 *                                                                          *
 *   								 ECHO                                   *
 *                                                                          *
 ****************************************************************************/

INTERFACE void Sequencer::enableEcho(int inPort, int outPort, int channel)
{
	// auto-activate
	(void)activate();

	MidiIn *input = _inputs[inPort];
	MidiOut *output = _outputs[outPort];

	// Can call this just to set the echo channel, so try to avoid
	// deactivation of the MIDI device if we can
	
	if (_echoInput != NULL) {
	
		if (_echoInput != input) {
			// full disable
			disableEcho();
		}
		// else, leave it active
	}

	if (input != NULL && output != NULL) {

		input->setEchoDevice(output);
		input->setEchoChannel(channel);

		if (_echoInput != input)
		  input->enable();

		_echoInput = input;
	}
	else
	  printf("Sequencer::enableEcho invalid port  %d, %d\n", inPort, outPort);
}

/**
 * Added so we can force all notes to a specific key for browsing
 * drum patches.
 */
INTERFACE void Sequencer::setEchoKey(int key) 
{
	if (_echoInput != NULL)
		_echoInput->setEchoKey(key);
}

INTERFACE void Sequencer::disableEcho()
{
	// auto-activate
	(void)activate();

	if (_echoInput != NULL) {
		_echoInput->disable();
		_echoInput->setEchoDevice(NULL);

		// don't let these linger, should MidiIn do this?
		_echoInput->setEchoKey(-1);
		_echoInput->setEchoChannel(-1);

		_echoInput = NULL;
	}
}

/****************************************************************************
 *                                                                          *
 *   								EVENTS                                  *
 *                                                                          *
 ****************************************************************************/

INTERFACE MidiEvent* Sequencer::getInputEvents(int port)
{
	MidiEvent *events = NULL;

	MidiIn *input = _inputs[port];
	if (input != NULL)
	  events = input->getEvents();
	
	return events;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

