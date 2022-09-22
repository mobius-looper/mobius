/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Sequencer internal definitions.
 */

#ifndef SEQ_INTERNAL_H
#define SEQ_INTERNAL_H

#include "seq.h"

/****************************************************************************
 * SeqLoop
 *
 * Description: 
 * 
 * Run-time object that holds loop state within a track.
 * When a loop is taken, we need to set the various track event
 * pointers back to the start of the loop, to avoid having to search
 * from the beginiing of the track each time, we cache pointers
 * into the event lists at the moment we become aware that a loop "start"
 * time has been  encountered.
 * The loops states are maintained on a push down stack chained through
 * the next field.  This stack could be maintained with the "stack" field
 * in the CMD_LOOP event but its somewhat awkward since we always want
 * to deal with the streamlined loop structure.
 *
 * This state is stored in the "data" field of the Midievent representing
 * the loop command  when the loop event is activated. 
 * Active loop events are further
 * maintained in a stack rooted in the "loops" field of the SeqTrack
 * and chained through the "stack" field of the MidiEvent.
 * 
 * One of these is allocated for each CMD_LOOP event during the pre-processing
 * phase and freed when the recorder stops.  
 * 
 * The loop stack is maintained independently of the event list, note that
 * the "others" list in the loop state may point directly or eventually
 * to the same CMD_LOOP event that has been stacked.  Once a loop event
 * has been stacked, we must ignore it if it is encountered in the event
 * list until its loop state is unstacked.  If you don't do this, you
 * will end up with an endless cycle where we keep stacking loop states
 * every time we loop back to the starting position and find our original
 * loop event.  
 *
 * First attempt: assuming that the CMD_LOOP event that resulted in the
 * stack is always the first such loop in the list, simply increment
 * the "others" pointer by one when it is stored in the loop state.
 * This requires that loop events on the same clock be ordered with the
 * longest loops first.  This works ok as long as there are only CMD_LOOP
 * events on the list, if there are other event types, they can be lost
 * since the loop we're taking can be toward the end of the list of events
 * on this clock.   I tried this with a "stacked" flag and that didn't
 * work because we just pushed the loop on again when the loop timed
 * out, it became unstacked and we loop back around to the beginning.
 * The rule now is, CMD_LOOP events must be ordered as mentioned above and
 * also must be before any other events on this clock that are to participate
 * in the loop. 
 ****************************************************************************/

class SeqLoop {

  public:

	//
	// constructors
	//

	SeqLoop(void) {
		next		= NULL;
		start		= 0;
		end			= 0;
		counter		= 0;
		event		= NULL;
		pushed		= 0;
	}
		
	~SeqLoop(void) {
	}

	void setNext(SeqLoop *n) {
		next = n;
	}

	void setStart(int s) {
		start = s;
	}

	void setEnd(int e) {
		end = e;
	}

	void setCounter(int c) {
		counter = c;
	}

	void setEvent(MidiEvent *e) {
		event = e;
	}
	
	void setPushed(int p) {
		pushed = p;
	}

	//
	// accessors
	//

	SeqLoop *getNext(void) {
		return next;
	}

	int getStart(void) {
		return start;
	}

	int getEnd(void) {
		return end;
	}

	int getCounter(void) {
		return counter;
	}

	MidiEvent *getEvent(void) {
		return event;
	}
	
	int isPushed(void) {
		return pushed;
	}

  private:
	
	SeqLoop			*next;		// run-time stack
	int				start;		// start clock
	int				end;		// end clock
	int				counter;	// loop iterations remaining
	MidiEvent		*event;		// event list position
	int				pushed;		// non-zero if currently being processed

};

/****************************************************************************
 * SeqRecording
 *
 * Description: 
 * 
 * State related to recording incomming events during recording. 
 * The sequencer itself may or may not be recording events, it can
 * simply be playing the track list.  
 *
 * Recording happens when a sequence is "installed" as the recording sequence.
 * The sequence can either be installed for "buffered" or "direct" recording.
 * If "buffered", recording will always happen into an internal buffer and
 * will only be sent to the destination sequence through an explicit request.
 * If "direct" recording is performed directly into the sequence without
 * any buffering.
 * 
 * This is not part of the external interface, probably should be
 * in a seperate file.
 * 
 ****************************************************************************/

class SeqRecording {

  public:

	//////////////////////////////////////////////////////////////////////
	//
	// constructors
	//
	//////////////////////////////////////////////////////////////////////

	SeqRecording(void) {
		
		sequencer			= NULL;
		rectrack			= NULL;
		desttrack			= NULL;

		on					= NULL;
		events				= NULL;
		last_event			= NULL;
		commands			= NULL;
		last_command		= NULL;

		recording			= 0;
		new_events_flag 	= 0;
		mute 				= 0;
		drum_mode			= 0;

		callback_record		= NULL;
		mEventMask			= 0;
	}

	~SeqRecording(void);
	
	// set "drum" mode, not sure how we would figure this out
	void setDrums(int d) {
		drum_mode = d;
	}

	void setTrack(SeqTrack *tr) {
		rectrack = tr;
	}

	void setDestTrack(SeqTrack *tr) {
		desttrack = tr;
	}

	void setCallbackRecord(SEQ_CALLBACK_RECORD cb) {
		callback_record = cb;
	}

	void setSequencer(Sequencer *s) {
		sequencer = s;
	}

	void setEventMask(int m) {
		mEventMask = m;
	}

	//////////////////////////////////////////////////////////////////////
	//
	// accessors
	//
	//////////////////////////////////////////////////////////////////////

	int isRecording(void) {
		return recording;
	}

	SeqTrack *getTrack(void) {
		return rectrack;
	}

	SeqTrack *getDestTrack(void) {
		return desttrack;
	}

	// convenience, to get directly to the sequences
	MidiSequence *getSequence(void) {
		return (rectrack != NULL) ? rectrack->getSequence() : NULL;
	}

	MidiSequence *getDestSequence(void) {
		return (desttrack != NULL) ? desttrack->getSequence() : NULL;
	}

	MidiEvent *getOn(void) {
		return on;
	}

	int getNewEvents(void) {
		return new_events_flag;
	}

	void setNewEvents(int e) {
		new_events_flag = e;
	}

	// for internal error messages
	BasicEnvironment *getEnv(void) {
		return sequencer->getEnv();
	}

	//////////////////////////////////////////////////////////////////////
	//
	// operations
	//
	//////////////////////////////////////////////////////////////////////

	// enable/disable active recording
	void enable(void) {
		recording = 1;
	}

	void disable(void) {
		recording = 0;
	}

	int  stop(void);
	void start(int clock);

	void mergePunch(void);
	void mergeDynaPunch(int flush);
	void mergeNormal(int flush);
	void startRecordErase(void);
	void stopRecordErase(void);
	void runtimeInit(int flush);

	//
	// seqint.cxx
	//

	MidiEvent *popRecordNote(int channel, int key);
	void checkHang(int key, int now);
	void recordNoteOn(MidiEvent *e, int now);
	void recordNoteOff(MidiEvent *e, int now);
	void recordPitch(MidiEvent *e, int now);
	void recordTouch(MidiEvent *e, int now);
	void recordControl(MidiEvent *e, int now);
	void recordMisc(MidiEvent *e, int now);

	//////////////////////////////////////////////////////////////////////
	//
	// private
	//
	//////////////////////////////////////////////////////////////////////

  private:

	Sequencer		*sequencer;		// sequencer we're installed in
	SeqTrack		*rectrack;		// track we're recording into
	SeqTrack		*desttrack;		// eventual record track when buffering

	// Transient record state, compiled into the "seq" when 
	// recording stops.

	MidiEvent		*on;			// events still on
	MidiEvent		*events;		// start of event list
	MidiEvent		*last_event;	// last event in the list
	MidiEvent		*commands;		// command events (erase etc)
	MidiEvent		*last_command;	// last event in command list

	// user callback for each note
	SEQ_CALLBACK_RECORD callback_record;

	// Set by the system if we're actually recording something 
	// The clock must be running and either punch is disabled or we're in
	// the punch zone.
	int recording;

	// set by the system if any new events were entered during the last loop
	int new_events_flag;

	// set by the system if the recording track is muted (internal runtime)
	int mute;

	// set by the system if we're recording drums
	// not currently used, not sure how we would determine this
	int drum_mode;

	// event capture flags
	int mEventMask;

	//////////////////////////////////////////////////////////////////////
	//
	// private methods
	//
	//////////////////////////////////////////////////////////////////////

	//
	// recording.cxx
	//

	void flushHangingNotes(void);

	//
	// recmerge.cxx
	//

	void addCommand(MidiEvent *e);
	void processCommands(void);
	void flushCommands(int flushall);
	void mergeEvents(int max_end);
	void mergeEventsDyna(int max_clock);
	void erasePunchRegion(void);

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
