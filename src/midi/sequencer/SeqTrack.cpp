/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Track list maintenance for the sequencer.
 * 
 * NOTE: We can call user callback functions for each note, we must
 * be very careful that the callbacks don't do anything
 * to corrupt the track or sequencer state !!
 *
 * For now, assume that nothing bad will happen, may try a simple
 * semaphore in the sequencer but that isn't entirely safe either.
 *
 * Will be necessary for track processing to allow the addition or deletion
 * of tracks as we sweep !
 *
 */

#include <stdio.h>
#include <stdlib.h>

#include "port.h"
#include "midi.h"
#include "mmdev.h"
#include "smf.h"

#include "seq.h"
#include "track.h"
#include "recording.h"

/****************************************************************************
 *                                                                          *
 *   						  EXTERNAL INTERFACE                            *
 *                                                                          *
 ****************************************************************************/

/****************************************************************************
 * SeqTrack::getChannel
 *
 * Arguments:
 *
 * Returns: channel number
 *
 * Description: 
 * 
 * Tracks can override the channel specified in the installed sequence.
 * 
 ****************************************************************************/

INTERFACE int SeqTrack::getChannel(void)
{
	int ch = 0;

	if (channel != -1)
	  ch = channel;
	else if (seq != NULL)
	  ch = seq->getChannel();

	return ch;
}


/****************************************************************************
 * SeqTrack::clear
 *
 * Arguments:
 *	none: 
 *
 * Returns: none
 *
 * Description: 
 * 
 * Erases the contents of a track.  This will clear out the associated
 * sequence but leave the containing Sequence object intact.
 * 
 * Hmm, probably should stop thinking that a Sequence once installed will
 * live forever, the track should be free to reallocate it at any time.
 * 
 ****************************************************************************/

INTERFACE void SeqTrack::clear(void)
{
	if (seq != NULL)
	  seq->clear();
}

/****************************************************************************
 *                                                                          *
 *   							 CONSTRUCTORS                               *
 *                                                                          *
 ****************************************************************************/

/****************************************************************************
 * SeqTrack
 *
 * Arguments:
 *
 * Returns: track object
 *
 * Description: 
 * 
 * Constructor for a track object.
 ****************************************************************************/


/****************************************************************************
 * ~SeqTrack
 *
 * Arguments:
 *
 * Returns: 
 *
 * Description: 
 * 
 * Destructor for a track object.
 * We now consider that we own the sequence if its in here.
 ****************************************************************************/

PUBLIC SeqTrack::~SeqTrack(void)
{
	// We used to call sequence->setTrack() here, but we don't keep
	// this pointer in the sequence object any more so we have
	// to trust.

	delete seq;
}

/****************************************************************************
 * SeqTrack::dump
 *
 * Arguments:
 *
 * Returns: 
 *
 * Description: 
 * 
 * Display debug information about a track.
 ****************************************************************************/

PUBLIC void SeqTrack::dump(void)
{
	printf("Track:\n");

	printf("  next %p playlink %p sequence %p\n",
		   next, playlink, seq);

	printf("  events %p on %p loops %p\n", events, on, loops);

	printf("  channel %d disabled %d muted %d, being_redorded %d\n",
		   (int)channel, (int)disabled, (int)muted, (int)being_recorded);
}

/****************************************************************************
 *                                                                          *
 *   							  LOOP STATE                                *
 *                                                                          *
 ****************************************************************************/

/****************************************************************************
 * annotateLoopEvent
 *
 * Arguments:
 *	e: event to annotate
 *
 * Returns: none
 *
 * Description: 
 * 
 * Builds a special object that we hang off the MidiEvent representing
 * a loop command.    This maintains pointers to the event list
 * within the sequence so we can get to our loop points quickly.
 * We might not have to do this, if the machine's fast enough.
 ****************************************************************************/

PRIVATE void SeqTrack::annotateLoopEvent(MidiEvent *e)
{
	SeqLoop *l;

	// if the event has no duration, ignore it
	if (e->getDuration()) {

		l = new SeqLoop();
		if (l != NULL) {
			
			l->setStart(e->getClock());
			l->setEnd(e->getClock() + e->getDuration());
			l->setCounter(e->getExtra());

			e->setData((void *)l);
		}
	} 
}

/****************************************************************************
 * SeqTrack::annotateLoops
 *
 * Arguments:
 *	none: 
 *
 * Returns: none
 *
 * Description: 
 * 
 * Go through all the events looking for those that represent loop commands.
 * When found, add a SeqLoop object to each one that keeps state 
 * related to the loop.
 * 
 * This must only be done immediately prior to playing the track, the
 * loop state shouldn't be carried around in the sequence very long.
 ****************************************************************************/

PRIVATE void SeqTrack::annotateLoops(void)
{
	MidiEvent *e;

	if (seq != NULL) {

		for (e = seq->getEvents() ; e != NULL ; e = e->getNext()) {

			if (e->getStatus() == MS_CMD_LOOP)
			  annotateLoopEvent(e);

			// We will be using the "value" field in note events to hold
			// the note off time for stacked events.  Make sure this
			// gets initialized to zero.  
			// Formerly used the "data" field for this.
			if (e->getStatus() == MS_NOTEON)
			  e->setExtra(0);
		}
	}
}

/****************************************************************************
 * SeqTrack::cleanupLoops
 *
 * Arguments:
 *	none: 
 *
 * Returns: none
 *
 * Description: 
 * 
 * Go clean out any transient SeqLoop objects that were attached
 * to MidiEvents by annotateLoops before the track was played.  
 ****************************************************************************/

PRIVATE void SeqTrack::cleanupLoops(void)
{
	MidiEvent *e;

	if (seq != NULL) {
		for (e = seq->getEvents() ; e != NULL ; e = e->getNext()) {
			if (e->getStatus() == MS_CMD_LOOP && e->getData() != NULL) {

				SeqLoop *l = (SeqLoop *)e->getData();
				delete l;
				e->setData(NULL);
			}        
		}
	}
}

/****************************************************************************
 * SeqTrack::pushLoop
 *
 * Arguments:
 *	e: loop event
 *
 * Returns: none
 *
 * Description: 
 * 
 * Called by SeqTrack::setStart during pre-processing and SeqTrack::sweep
 * during playing.
 *
 * Here we have encountered an MS_CMD_LOOP event, and we need to check
 * to see if it should be added to the loop stack.
 * 
 * We have a sticky problem here since the loop event we want to push
 * may on the starting clock of the same loop that we just took.  The old
 * code would avoid this by keeping the loop events on a seperate list, and
 * keeping the originating loop event off the list.  Now we just keep
 * a "pushed" flag in the loop to prevent recursion.
 ****************************************************************************/

PRIVATE void SeqTrack::pushLoop(MidiEvent *e)
{
	SeqLoop *current, *l;

	// If the loop has no width, ignore it.
	// also ignore if its already pushed and active

	l = (SeqLoop *)e->getData();
	if (l != NULL && !l->isPushed() && (l->getEnd() > l->getStart())) {

		current = loops;

		// Don't stack this if the currently stacked loop ends 
		// before this one.
		if (current == NULL || (current->getEnd() >= l->getEnd())) {

			// push a new loop state
			l->setNext(current);
			loops = l;

			l->setPushed(1);

			// always reset the loop counter
			l->setCounter(e->getExtra());

			// capture current position
			// !! To prevent recursion, need to keep the loop event
			// itself out of this list.  If we obey the rule that loops
			// will be at the front of the list, and each in descending
			// order of length, then the next event is the one we want
			// to loop back to.
			// Hmm, this may eliminate the need for the "pushed" flag
			// in the loop annotation, but keep it around for safety.
			l->setEvent(e->getNext());
		}
	}
}

/****************************************************************************
 * SeqTrack::doLoop
 *
 * Arguments:
 *	none: 
 *
 * Returns: none
 *
 * Description: 
 * 
 * Work function called by SeqTrack::start during pre-processing
 * and by SeqTrack::sweep while running.
 * Here we've determined that we must perform a loop, so set the
 * track pointers and handle the loop stack.
 * 
 * To prevent loop recursion, since we can loop back to the same event
 * that started the loop, we keep a "pushed" flag in the loop annotation
 * that tells us not to touch this.
 ****************************************************************************/

PRIVATE void SeqTrack::doLoop(void)
{
	SeqLoop 	*l;
	int 		lcount;

	l = loops;
	if (l != NULL) {

		// point the track event list at the start of the loop
		events = l->getEvent();

		// increment track clock adjust counter by the duration of the loop
		loop_adjust += (l->getEnd() - l->getStart());

		// If the count is at zero, its infinite, otherwise decrement and
		// remove when it goes to zero.
		lcount = l->getCounter();
		if (lcount) {
			lcount--;
			l->setCounter(lcount);
			if (!lcount) {
				// finished looping, go on to the next
				loops = l->getNext();
				l->setNext(NULL);
				l->setPushed(0);
			}
		}
	}
}

/****************************************************************************
 *                                                                          *
 *   							PREPROCESSING                               *
 *                                                                          *
 ****************************************************************************/
/* 
 * Whenever the recorder is started, we go through and initialize the track
 * state for each track that is enabled for playing.
 * This involves caching some of the information contained in the sequence
 * structure in the track structure and searching for the first events
 * in the sequence that will played beginning with the current start
 * time set for the recorder.  
 * This is run-time state only, it becomes invalid as soon as the
 * recorder stops and must be re-initialized each time the recorder
 * is started.
 */

/****************************************************************************
 * SeqTrack::reset
 *
 * Arguments:
 *
 * Returns: none
 *
 * Description: 
 * 
 * Work function for SeqTrack::start, SeqTrack::stop.
 * Initializes the runtime state to NULL, after it has been stopped
 * and before it is pre-processed.
 * 
 ****************************************************************************/

PRIVATE void SeqTrack::reset(void)
{

	// hmm, keep channel as is so it can be explicitly changed
	// by the app, need to think about how best to set channels!
	// channel 		= 0;

	disabled  		= 0;
	muted			= 0;
	being_recorded 	= 0;

	events 			= NULL;
	on 				= NULL;
	loops 			= NULL;
	loop_adjust 	= 0;

	cleanupLoops();
}

/****************************************************************************
 * SeqTrack::getNextClock
 *
 * Arguments:
 *	none: 
 *
 * Returns: clock
 *
 * Description: 
 * 
 * Look at the various upcomming events within the track, and return
 * the next time that something interesting happens.  The returned
 * clock will be an absolute clock, de-normalized from track relative
 * time if there were any loops.
 * 
 ****************************************************************************/

PRIVATE int SeqTrack::getNextClock(void)
{
	int clk;

	clk = SEQ_CLOCK_INFINITE;

	// look at upcomming events
	if (events != NULL && events->getClock() < clk)
	  clk = events->getClock();

	// look at pending loops
	if (loops != NULL && loops->getEnd() < clk)
	  clk = loops->getEnd();

	// De-normalize the clk based on the loop adjustments to see
	// where we really are.
	if (clk != SEQ_CLOCK_INFINITE)
	  clk += loop_adjust;

	// look at the note off events, off time stored in value
	// do this AFTER denormalization since these are stored absolute
	if (on != NULL && on->getExtra() < clk)
	  clk = on->getExtra();

	return clk;
}

/****************************************************************************
 * SeqTrack::start
 *
 * Arguments:
 *	clock: clock to move to
 *
 * Returns: none
 *
 * Description: 
 * 
 * Work function for Sequencer::startTracks, called for each track in the
 * sequencer.  Advance the track state up to the given clock.  This
 * is similar to playing the tracks, without emitting any events
 * or calling any callback functions.
 *
 * This may also be called by SeqRecording::sweep, when we have to 
 * perform an "edit" loop by starting over from the beginning of
 * the sequence?
 *
 * NOTE: As this sweeps, it should be collecting information about
 * controller and program events.  When finished we should send the final
 * state of programs and controllers so that the subsequent events will
 * be played in the proper context.
 ****************************************************************************/

PUBLIC void SeqTrack::start(int clock)
{
	MidiEvent *e;
	int c, tr_clock;

	events = NULL;
	loops = NULL;		// dangerous ?
	loop_adjust = 0;

	// annotate any loop events before we get started
	// will get undone by cleanupLoops() or reset()
	annotateLoops();

	// start the event list back at the beginning
	if (seq != NULL)
	  events = seq->getEvents();

	if (clock) {
		// pretend like we're playing this thing up to the given clock
		for (c = 0 ; c < clock ; c = getNextClock()) {

			// normalize the clock relative to the track
			tr_clock = c - loop_adjust;

			// if we're not recording this track, check for play loops
			if (!being_recorded) {
				while (loops != NULL && loops->getEnd() <= tr_clock) {
					doLoop();
					tr_clock = c - loop_adjust;
				}
			}

			// process events
			for (e = events ; e != NULL && e->getClock() <= tr_clock ; 
				 e = e->getNext()) {

				if (e->getStatus() == MS_CMD_LOOP) {
					// only process loops if we're not recording 
					// this sequence
					if (!being_recorded) 
					  pushLoop(e);
				}
			}
			events = e;
		}
	}
}

/****************************************************************************
 * Sequencer::startTracks
 *
 * Arguments:
 *	none: 
 *
 * Returns: none
 *
 * Description: 
 * 
 * Main pre-processing function called by Sequencer::start.
 * Given the desired start time, pre-process all of the currently 
 * installed tracks and build the recorder's play list which may
 * be a subset of the currently installed tracks.
 *
 * Cache various pieces of information from the sequence & recorder
 * directly in the track structure so we don't have to keep hunting
 * for them.
 * 
 * The note callback is only active for the sequence identified as
 * the "watch" sequence.  Formerly, if the watch sequence was NULL
 * we would call the note alert for every note event anywhere.
 * Since this should be only used for GEE, it is only necessary
 * to watch a single sequence.
 * Might want to tie up this definition in the recording sequence instead.
 * 
 ****************************************************************************/

PRIVATE void Sequencer::startTracks(void)
{
	SeqTrack 		*tr;
	MidiSequence 	*seq;
	int 			clock;

	// reset the play list
	playing = NULL;

	for (tr = tracks ; tr != NULL ; tr = tr->getNext()) {

		// is this track disabled ?
		// I don't like keeping this in the sequence!

		seq = tr->getSequence();
		if (!tr->isDisabled() &&  seq != NULL) {

			// initialize the track, shouldn't really be necessary?
			tr->reset();

			// set a flag if this track's sequence is being recorded
			// to prevent complex loops which confuse things
			if (recording != NULL && 
				recording->getSequence() == tr->getSequence())
			  tr->setBeingRecorded(1);

			// cache various information from the associated sequencer
			// who should control what port we use?
			// assume default for now
			tr->setOutput(_outputs[_defaultOutput]);

			// !! don't set channel, let it be overridden???
			if (tr->getChannel() < 0)
			  tr->setChannel(seq->getChannel());
			
			// Wind the track up the desired clock and calculated the
			// next clock of interest for the track.
			tr->start(timer->getClock());
			clock = tr->getNextClock();

			if (clock == SEQ_CLOCK_INFINITE) {
				// The track has nothing to say, make sure the run 
				// time state is clean and don't add it to the play list.
				tr->reset();
			}
			else {
				// the track has something to say, add it to the play list
				tr->setPlayLink(playing);
				playing = tr;
			}
		}
	}
}

/****************************************************************************
 *                                                                          *
 *   						   POST PROCESSING                              *
 *                                                                          *
 ****************************************************************************/
/*
 * When the recorder is stopped, we go through the track list and 
 * perform various housekeeping tasks, most notably shutting off
 * any notes that are still being played.
 *
 * We must also free any SeqLoop structures that were allocated
 * during pre-processing. 
 */

/****************************************************************************
 * SeqTrack::processCallbacks
 *
 * Arguments:
 *	 e: midi event
 *	on: non-zero if note is going on
 *
 * Returns: none
 *
 * Description: 
 * 
 * Calls various registered callbacks when a midi event is either turned
 * on or off.
 ****************************************************************************/

PRIVATE void SeqTrack::processCallbacks(MidiEvent *e, int on)
{
	SEQ_CALLBACK_NOTE cb;

	if (e->getStatus() == MS_NOTEON) {

		// note callback notified for every note event
		cb = sequencer->callback_note;
		if (cb != NULL)
		  (*cb)(sequencer, e, on);

		else if (being_watched) {
			// watch callback notified only if this track is marked 
			// as being watched

			cb = sequencer->callback_watch;
			if (cb != NULL)
			  (*cb)(sequencer, e, on);
		}
		
		if (sequencer->mEventMask & SeqEventNoteOn) {
			SeqEventType t = (on) ? SeqEventNoteOn : SeqEventNoteOff;
			sequencer->addEvent(t, e->getClock(), e->getDuration(),
								e->getKey());
		}
	}
}

/****************************************************************************
 * SeqTrack::flushOn
 *
 * Arguments:
 *	none: 
 *
 * Returns: none
 *
 * Description: 
 * 
 * Called by SeqTrack::stop to turn off any notes that are currently
 * being held on.
 ****************************************************************************/

PRIVATE void SeqTrack::flushOn(void)
{
	MidiEvent *e, *nexte;
	int ch;

	for (e = on, nexte = NULL ; e != NULL ; e = nexte) {

		nexte = e->getStack();

		// Turn the note off.  Formerly we checked for global_mute but 
		// we really should always obey pending "off" events that were queued
		// before the mute was enabled.  Technically, the act of setting
		// the mute should go in and immediately turn off any hanging notes.

		ch = (channel < 0) ? e->getChannel() : channel;
		out->sendNoteOff(ch, e->getKey());

		processCallbacks(e, 0);

		e->setStack(NULL);
		e->setExtra(0);
	}

	on = NULL;
}

/****************************************************************************
 * SeqTrack::centerControllers
 *
 * Arguments:
 *	none: 
 *
 * Returns: none
 *
 * Description: 
 * 
 * Called by Sequencer::stopTracks to center any continuous controllers that 
 * may have moved during playing of this track.
 * Stubbed right now, we should be keeping run-time state
 * in order to be efficient here.
 ****************************************************************************/

PRIVATE void SeqTrack::centerControllers(void)
{
}

/****************************************************************************
 * SeqTrack::stop
 *
 * Arguments:
 *	none: 
 *
 * Returns: none
 *
 * Description: 
 * 
 * Called by Sequencer::stopTracks to stop one track.
 * We perform various internal cleanup operations, and return the track
 * to a initialized state.
 ****************************************************************************/

PUBLIC void SeqTrack::stop(void)
{
	flushOn();
	centerControllers();
	cleanupLoops();
	reset();
}

/****************************************************************************
 * Sequencer::stopTracks
 *
 * Arguments:
 *
 * Returns: 
 *
 * Description: 
 * 
 * Primary interface function called by Sequencer::stop.
 * Clean up any residual state for the track player.
 * This turns off any notes that are hanging and centers all the continuous
 * controllers.
 * 
 ****************************************************************************/

PRIVATE void Sequencer::stopTracks(void)
{
	SeqTrack *tr, *nextt;

	for (tr = playing, nextt = NULL ; tr != NULL ; tr = nextt) {
		nextt = tr->getPlayLink();
		tr->setPlayLink(NULL);
		tr->stop();
	}
	playing = NULL;
}

/****************************************************************************
 *                                                                          *
 *   							TRACK PLAYING                               *
 *                                                                          *
 ****************************************************************************/
/* 
 * Once a track has been pre-processed and the recorder has started,
 * we will periodically "sweep" through the tracks looking for things to
 * do.  The Sequencer::sweepTracks method is the primary interface 
 * called from the clock interrupt handler.
 * 
 */

/****************************************************************************
 * SeqTrack::forceOff
 *
 * Arguments:
 *	e: event
 *
 * Returns: none
 *
 * Description: 
 * 
 * Force a note that was stacked on, off.
 * Used when a loop causes an event to be stacked again before
 * the first note off time was reached.
 * 
 ****************************************************************************/

PRIVATE void SeqTrack::forceOff(MidiEvent *e)
{
	MidiEvent *o, *prev;
	int ch;

	for (o = on, prev = NULL ; o != NULL && o != e ; o = o->getStack())
	  prev = o;

	if (o != NULL) {
		if (prev == NULL)
		  on = e->getStack();
		else
		  prev->setStack(e->getStack());

		ch = (channel < 0) ? e->getChannel() : channel;
		out->sendNoteOff(ch, e->getKey());

		// Inform the callback
		processCallbacks(e, 0);

		e->setStack(NULL);
		e->setExtra(0);
	}
}

/****************************************************************************
 * SeqTrack::sendEvents
 *
 * Arguments:
 *	clock: clock we're currently at
 *
 * Returns: none
 *
 * Description: 
 * 
 * Work function for track_sweep.
 * Send out any events that are ready, calculate next clock time.
 * If the track is muted, don't send any events but keep advancing
 * the list pointers so we can turn the mute off dynamically while
 * the tracks are playing.
 *
 * As notes are played, they are added to the "on" list of the track
 * so they can be turned off later.
 *
 * NOTE: 
 * Formerly, we would slam the track channel in each event before it
 * is sent in order to allow the track to override the channel that
 * may have been stored in the event.  I'm not sure that's a good thing 
 * so I modified MidiOut::send to accept a channel override argument.
 * Think about this.
 * 
 ****************************************************************************/

PRIVATE void SeqTrack::sendEvents(int clock)
{
	MidiEvent *e, *o, *prev;

	// send the events
	for (e = events ; e != NULL && e->getClock() <= clock ; e = e->getNext()) {
		if (!muted) {
			if (e->getStatus() != MS_NOTEON)
			  out->send(e, channel);

			else if (e->getDuration() == 0) {
				// Duration is zero, drum note or unresolved record note.
				// Send it but don't queue a note off event and don't 
				// alert the callback.
				out->send(e, channel);
			}
			else {
				// If this event has already been stacked and we're 
				// trying to play it again due to a loop, we must 
				// first forceably stop the previous version since we
				// don't have any way to stack a note more than once.

				if (e->getExtra())
				  forceOff(e);

				out->send(e, channel);

				processCallbacks(e, 1);
				
				// Off time is stored in the value field of the 
				// event for notes.
				// Must store absolute off time for accuracy over loops.
				// Formerly had (clock + duration - 1) here but that 
				// screws up for events with 1 clock duration. 

				e->setExtra(e->getClock() + e->getDuration() +
							loop_adjust);

				// Stack the event, keep this ordered according to 
				// note off time.

				for (o = on, prev = NULL ; 
					 o != NULL && o->getExtra() < e->getExtra() ; 
					 o = o->getStack())
				  prev = o;

				e->setStack(o);
				if (prev == NULL)
				  on = e;
				else
				  prev->setStack(e);
			}
		}
	}

	// save the new position
	events = e;
}

/****************************************************************************
 * SeqTrack::endEvents
 *
 * Arguments:
 *	clock: current clock
 *
 * Returns: none
 *
 * Description: 
 * 
 * Work function for Sequencer::sweepTracks.
 * Turn off any hanging notes that have reached their expiration time.
 * The "off" time is stored in the value field for note events.
 * Note that the clock here is the absolute clock, not the normalized
 * track clock.
 * 
 ****************************************************************************/

PRIVATE void SeqTrack::endEvents(int clock)
{
	MidiEvent *e, *nexte;
	int ch;

	for (e = on ; e != NULL && e->getExtra() <= clock ; e = nexte) {
		nexte = e->getStack();

		ch = (channel < 0) ? e->getChannel() : channel;
		out->sendNoteOff(ch, e->getKey());

		processCallbacks(e, 0);

		e->setStack(NULL);
		e->setExtra(0);
		
		// pop it off the list
		on = nexte;
	}
}

/****************************************************************************
 * SeqTrack::sweep
 *
 * Arguments:
 *	clock: current clock
 *
 * Returns: next clock of interest
 *
 * Description: 
 * 
 * Sweeps through one track.
 * Sends events that are ready, and processing any loops.
 ****************************************************************************/

PUBLIC void SeqTrack::sweep(int clock)
{
	SEQ_CALLBACK_LOOP cb;
	MidiEvent *e;
	int tr_clock;

	// normalize the clock relative to the track
	tr_clock = clock - loop_adjust;

	// if we're not recording this track, check for loops
	if (!being_recorded) {
		while (loops != NULL && loops->getEnd() <= tr_clock) {

			// save this for the event
			int loopend = loops->getEnd();

			doLoop();

			// re-normalize the time after the loop
			tr_clock = clock - loop_adjust;

			cb = sequencer->callback_loop;
			if (cb != NULL)
			  (*cb)(sequencer, seq, 0);

			if (sequencer->mEventMask & SeqEventLoop)
			  sequencer->addEvent(SeqEventLoop, loopend, 0, 0);
		}
	}

	// Process loop events, which must be maintained first 
	// on this clock.  
	for (e = events ; e != NULL && e->getClock() <= tr_clock ; 
		 e = e->getNext()) {

		if (e->getStatus() == MS_CMD_LOOP) {
			// only allow play loops if we're not recording into 
			// this sequence
			if (!being_recorded)
			  pushLoop(e);
		}
		else {
			// else, its not a loop, start processing other events
			break;
		}
	}
	events = e;

	// turn off notes, note we pass in the real clock here
	endEvents(clock);

	// process the output events
	sendEvents(tr_clock);
}

/****************************************************************************
 * Sequencer::sweepTracks
 *
 * Arguments:
 *	clock: current clock
 *
 * Returns: next clock of interest in all tracks
 *
 * Description: 
 * 
 * Primary interface function called by the Sequencer clock interrupt handler
 * to emit any track events for this clock.
 * Returns the next time when any of the tracks need attention.
 * Due to looping, the clock passed in here is normalized for
 * most of the clock calculations.  The nextclock returned from
 * this function will however have been de-normalized back into
 * an absolute clock.
 * 
 ****************************************************************************/

PRIVATE int Sequencer::sweepTracks(int clock)
{
	SeqTrack	*tr, *prev;
	int 		nextclock, tr_nextclock;

	// Check for various things during recording
	nextclock = SEQ_CLOCK_INFINITE;

	// if recording, check punch events
	if (recording != NULL) {
		if (punch_in_enable) {
			// check punch in
			if (punch_in <= clock)
			  recording->enable();
			else if (punch_in < nextclock)
			  nextclock = punch_in;

			// check punch out
			if (punch_out_enable) {
				if (punch_out <= clock)
				  recording->disable();
				else if (punch_out < nextclock)
				  nextclock = punch_out;
			}
		}
	}

	// sweep over each playing track
	for (tr = playing, prev = NULL ; tr != NULL ; tr = tr->getPlayLink()) {

		// If the disable flag is on, remove the track from the playlist.
		// Can be used to remove tracks during playing. 
		// ?? What is this for ??
		// What about "on" notes, would have to call flush_on_note() if 
		// a track can be disabled at any time.  Probably other 
		// cleanup stuff too.

		if (tr->isDisabled()) {
			if (prev == NULL)
			  playing = tr->getPlayLink();
			else
			  prev->setPlayLink(tr->getPlayLink());
			tr->reset();	// remember to free the SeqLoop objects
		}
		else {      
			// not disabled, process the track 
			prev = tr;

			tr->sweep(clock);
			tr_nextclock = tr->getNextClock();
			
			// maintain a running minimum for all tracks
			if (tr_nextclock < nextclock)
			  nextclock = tr_nextclock;
		}
	}

	return nextclock;
}

/****************************************************************************
 * Sequencer::getFirstSweepClock
 *
 * Arguments:
 *
 * Returns: first sweep clock
 *
 * Description: 
 * 
 * Determines the next time when any of the playing tracks or the recorded
 * track needs attention.  Must have first called Sequencer::startTracks 
 * so that the nextclock fields in the track structures have all 
 * been calculated.
 *
 * This is used only by the clockKickoff method to calculate the time
 * without actually sending any events.
 * 
 ****************************************************************************/
 
PRIVATE int Sequencer::getFirstSweepClock(void)
{
	SeqTrack *tr;
	int nextclock, tr_nextclock;

	// start with the first time the recording needs attention
	nextclock = SEQ_CLOCK_INFINITE;
	if (recording != NULL) {

		// check punch in time
		if (punch_in_enable && punch_in < nextclock)
		  nextclock = punch_in;

		// Check for recording a sequence with a loop end time, there
		// is no loop time adjustment here.

		if (loop_end_enable && loop_end && loop_end < nextclock)
		  nextclock = loop_end;
	}

	// check each track
	for (tr = playing ; tr != NULL ; tr = tr->getPlayLink()) {
		tr_nextclock = tr->getNextClock();
		if (tr_nextclock < nextclock)
		  nextclock = tr_nextclock;
	}

	return nextclock;
}

/****************************************************************************
 * Sequencer::findTrack
 *
 * Arguments:
 *	seq: sequence of interest
 *
 * Returns: track associated with the sequence
 *
 * Description: 
 * 
 * Finds the SeqTrack currently assigned to the sequence if any.
 ****************************************************************************/

INTERFACE SeqTrack *Sequencer::findTrack(MidiSequence *seq)
{
	SeqTrack *tr, *found;

	found = NULL;
	for (tr = tracks ; tr != NULL && found == NULL ; tr = tr->getNext()) {
		if (tr->getSequence() == seq) 
		  found = tr;
	}
	return found;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
