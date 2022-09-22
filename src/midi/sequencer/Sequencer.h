/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Multi track sequencer/recorder.
 * 
 * Builds upon the MidiIn, MidiOut, and Timer device interfaces into
 * a higher level environment for playing andr recording MidiSequence objects.
 * 
 * This is rather old and unfortunately Windows specific due to the
 * current design of MidiIn/Out and Timer.  Eventually try to rebuild
 * this on top of MidiInterface so we can have one on OSX.
 *
 */

#ifndef MIDI_SEQUENCER_H
#define MIDI_SEQUENCER_H

/****************************************************************************
 *                                                                          *
 *                                 CONSTANTS                                *
 *                                                                          *
 ****************************************************************************/

/**
 * Value for clock arguments that means infinite.
 * Should be in midi.h?
 * Don't use -1 here, it screws up too many comparisons.
 */
#define SEQ_CLOCK_INFINITE 0x7FFFFFFF

/****************************************************************************
 *                                                                          *
 *                                 CALLBACKS                                *
 *                                                                          *
 ****************************************************************************/
/*
 * Callbacks can only be used if you're careful to restrict what you do 
 * in them, since you are in an interrupt service routine.
 *
 * Its much safer to poll for "sequencer events", which can be created
 * by the interrupt handlers and left on a list.  See SeqEvent below.
 *
 * !! Revisit these probably don't need all of them, and could
 * combine some.  Start using the MidiListener or SeqEvent approach instead.
 */

/**
 * Called on each beat, return non-zero to stop the clock.
 */
typedef int (*SEQ_CALLBACK_BEAT)(class Sequencer *s);

/**
 * Called on each outgoing note on/off event in the sequencer if installed
 * as the "note" callback, or only for notes in a specific sequence if
 * installed as the "watch" callback.
 */
typedef void (*SEQ_CALLBACK_NOTE)(class Sequencer *s, MidiEvent *e, int on);

/**
 * Called on each start/stop event, first arg is non-zero to tell the 
 * difference.  Second arg is non-zero only when stopping and if there
 * were new events added to the record sequence.
 */
typedef void (*SEQ_CALLBACK_COMMAND)(class Sequencer *s, int start, int events);

/**
 * Called on each incomming event during recording.
 */
typedef void (*SEQ_CALLBACK_RECORD)(class Sequencer *s, MidiEvent *e);

/**
 * Called on each incomming event when NOT recording.
 */
typedef void (*SEQ_CALLBACK_EVENT)(class Sequencer *s, MidiEvent *e);

/**
 * Called whenever an edit loop is performed.
 * The sequence is the one performing the loop if called from the
 * SeqTrack handlers.  It can be NULL if called from the Timer handler
 * when the global edit loop is encountered.
 *
 * events is non-zero if there were events recorded since the last loop.
 */
typedef void (*SEQ_CALLBACK_LOOP)(class Sequencer *seq, MidiSequence *s,
								  int events);

/**
 * MidiListener
 *
 * Updated callback interafce.  The interface of a class that will
 * be called for each incomming MIDI event.
 */
class MidiListener {

  public:

	virtual void MIDIEvent(MidiEvent* e) = 0;

};

/****************************************************************************
 *                                                                          *
 *                              SEQUENCER EVENTS                            *
 *                                                                          *
 ****************************************************************************/

/**
 * SeqEventType
 *
 * The types of events we record with SeqEvent objects.
 * These correspond approxomately to the callbacks, but have been
 * altered slightly.  Consider simplifying the callbacks as well.
 * 
 * The numeric values are powers of 2 so they can be OR'd together
 * to specify a set of events the application wishes to monitor.
 *
 * SeqEventBeat
 * 		Created on each "beat" (as defined by clock parameters)
 * 		during play or record.
 *
 * SeqEventNoteOn
 * 		Created on each outgoing note ON event during play or record.
 *
 * SeqEventNoteOff
 * 		Created on each outgoing note OFF event during play or record.
 * 
 * SeqEventStart
 * 		Created whenever the sequencer starts, and sends a MIDI start event.
 *
 * SeqEventStop
 * 		Created whenever the sequencer stops, and sends a MIDI stop event.
 * 		
 * SeqEventLoop
 * 		Created whenever the sequencer loops.
 *
 * SeqEventRecordNoteOn
 * 		Created whenever the sequencer finds an incomming note ON event.
 * 
 * SeqEventRecordNoteOff
 * 		Created whenever the sequencer finds an incomming note OFF event.
 * 
 * The SeqEventRecordNoteOn and Off event types are used for both
 * the "packet" callback and "record" callback.  Should try to eliminate
 * one of these callbacks, not sure why they're both necessary.
 *         
 */
typedef enum {

	SeqEventBeat 			= 1,
	SeqEventNoteOn			= 1 << 1,
	SeqEventNoteOff			= 1 << 2,
	SeqEventStart			= 1 << 3,
	SeqEventStop			= 1 << 4,
	SeqEventLoop			= 1 << 5,
	SeqEventRecordNoteOn	= 1 << 6,
	SeqEventRecordNoteOff	= 1 << 7

} SeqEventType;

/**
 * SeqEvent
 *
 * A class used to represent events that could result in the firing
 * of one of the sequencer callbacks.  SeqEvents are a safer alternative
 * to using callbacks, as you're not limited to the functions you can
 * call in an interrupt service routine.  
 * 
 * As the sequencer runs, if events are enabled, one will be created
 * at each point that a callback would be called.  The events are
 * left on a list, that the application may monitor and process. 
 * For apps that display visuals, you can poll 10 times a second or less
 * and still have the display sync reasonably well with the midi events.
 * 
 * For apps that need to be very tighly syncd with the midi events, such
 * as filtering apps, you still should use callbacks.
 * 
 * Hmm, these are an awful lot like MidiEvents, if they get any more similar,
 * should try to combine the two.
 * 
 */
class SeqEvent {

	friend class Sequencer;

  public:

	SeqEventType getType(void) {
		return mType;
	}

	int getClock(void) {
		return mClock;
	}

	int getDuration(void) {	
		return mDuration;
	}

	int getValue(void) {
		return mValue;
	}

	SeqEvent *getNext(void) {
		return mNext;
	}

	INTERFACE void free(void);

  protected:

	SeqEvent(class Sequencer *seq) {
		mSequencer	= seq;
		mNext 		= NULL;
		mType 		= SeqEventStart;
		mClock 		= 0;
		mDuration	= 0;
		mValue		= 0;
	}

	~SeqEvent(void) {}

	void setNext(SeqEvent *n) {
		mNext = n;
	}

	void setType(SeqEventType t) {
		mType = t;
	}

	void setClock(int c) {
		mClock = c;
	}

	void setDuration(int d) {
		mDuration = d;
	}

	void setValue(int v) {
		mValue = v;
	}

  private:

	Sequencer 		*mSequencer;		// sequencer that owns us
	SeqEvent 		*mNext;				// list link
	SeqEventType	mType;
	int				mClock;
	int				mDuration;
	int				mValue;
};

/****************************************************************************
 *                                                                          *
 *   							  METRONOME                                 *
 *                                                                          *
 ****************************************************************************/

/**
 * Object encapsulating metronome state for the sequencer.
 */
class SeqMetronome {

  public:

	//
	// construtors
	//

	INTERFACE SeqMetronome(void);
	INTERFACE ~SeqMetronome(void);

	INTERFACE void init(void);
	INTERFACE void setBeat(int b);
	INTERFACE void setClock(int clock);
	INTERFACE void setCpb(int c);

	//
	// simple accessors
	//

	int isEnabled(void) {
		return mEnabled;
	}
	void setEnable(int e) {
		mEnabled = e;
	}

	int getChannel(void) {
		return mChannel;
	}
	void setChannel(int c) {
		mChannel = c;
	}

	int getNote(void) {
		return mNote;
	}
	void setNote(int n) {
		mNote = n;
	}

	int getVelocity(void) {
		return mVelocity;
	}
	void setVelocity(int v) {
		mVelocity = v;
	}

	int getAccent(void) {
		return mAccentNote;
	}
	void setAccent(int a) {
		mAccentNote = a;
	}

	int getAccentVelocity(void) {
		return mAccentVelocity;
	}
	void setAccentVelocity(int v) {
		mAccentVelocity = v;
	}

	int getRecordNote(void) {
		return mRecordNote;
	}
	void setRecordNote(int n) {
		mRecordNote = n;
	}

	int getRecordVelocity(void) {
		return mRecordVelocity;
	}
	void setRecordVelocity(int v) {
		mRecordVelocity = v;
	}

	bool isNoteOff(void) {	
		return mNoteOff;
	}
	void setNoteOff(bool b) {
		mNoteOff = b;
	}

	//
	// For use by the Sequencer
	//

	void advance(class MidiOut *out);
	void sendRecord(class MidiOut *out);

  private:

	int mEnabled;			// non-zero to enable
	int mBeats;				// beats per measure (for accents)
	int mBeat;				// transient accent beat counter
	int mCpb;				// clocks per beat (for accents)

	int mChannel;			// output MIDI channel
	int mNote;				// unaccented note
	int mVelocity;
	int mAccentNote;		// accented note
	int mAccentVelocity;
	int mRecordNote;		// recording notification note
	int mRecordVelocity;
	bool mNoteOff;			// true to send note off's after note on

};

/****************************************************************************
 *                                                                          *
 *                                 SEQUENCER                                *
 *                                                                          *
 ****************************************************************************/

/**
 * The main object encapsulating the MIDI sequencer.
 */
class Sequencer {

	friend INTERFACE void seq_timer_callback(class Timer *t, void *args);
	friend INTERFACE void seq_midi_in_callback(class MidiIn *in, void *args);

	friend class SeqTrack;

  public:

	//////////////////////////////////////////////////////////////////////
	//
	// constructors
	//
	//////////////////////////////////////////////////////////////////////

    Sequencer(class MidiEnv* env);
    ~Sequencer();

    //
    // let this be an objct factory too
    //

	MidiSequence *newSequence(void);

	MidiEvent *newEvent(int status, int channel, int key, int velocity);

    /**
     * Interface for MidiFileReader
     */
	INTERFACE MidiSequence *readMidiFile(const char *file);

    /**
     * Interface for MidiFileAnalyzer.
     * Though since we have to expose MidiFileSummary, it doesn't
     * buy much to encapsulate it here
     */
	INTERFACE class MidiFileSummary *analyzeMidiFile(const char *file);

	//////////////////////////////////////////////////////////////////////
	//
	// User Callbacks, typically to trigger display events
	//
	//////////////////////////////////////////////////////////////////////

	void setCallbackBeat(SEQ_CALLBACK_BEAT cb) {
		mCallbackBeat = cb;
	}
	void setCallbackWatch(SEQ_CALLBACK_NOTE cb) {
		mCallbackwatch = cb;
	}
	void setCallbackNote(SEQ_CALLBACK_NOTE cb) {
		mCallbacknote = cb;
	}
	void setCallbackCommand(SEQ_CALLBACK_COMMAND cb) {
		mCallbackcommand = cb;
	}
	void setCallbackRecord(SEQ_CALLBACK_RECORD cb) {
		mCallbackrecord = cb;
	}
	void setCallbackEvent(SEQ_CALLBACK_EVENT cb) {
		mCallbackevent = cb;
	}
	void setCallbackLoop(SEQ_CALLBACK_LOOP cb) {
		mCallbackloop = cb;
	}

	/**
	 * New style, callback listener so we can retain state.
	 */
	void setMidiListener(MidiListener* l) {
		mListener = l;
	}

	//////////////////////////////////////////////////////////////////////
	//
	// asynchronous event processing
	//
	//////////////////////////////////////////////////////////////////////

	INTERFACE void enableEvents(int eventmask);
	INTERFACE SeqEvent *getEvents(void);
	INTERFACE void freeEvents(SeqEvent *ev);
	INTERFACE void freeEvents(MidiEvent *events);
	INTERFACE MidiEvent* getInputEvents(int port);

	//////////////////////////////////////////////////////////////////////
	//
	// sequence installation 
	//
	//////////////////////////////////////////////////////////////////////

	INTERFACE class SeqTrack *addSequence(MidiSequence *s);
	INTERFACE int	removeSequence(MidiSequence *s);
	INTERFACE void	freeSequence(MidiSequence *seq);

	//////////////////////////////////////////////////////////////////////
	//
	// track accessors
	//
	//////////////////////////////////////////////////////////////////////

	SeqTrack *getTracks(void) {
		return tracks;
	}

	INTERFACE class SeqTrack *findTrack(MidiSequence *s);
	INTERFACE class SeqTrack *getTrack(int index);

	INTERFACE void removeTrack(SeqTrack *t);
	INTERFACE void clearTracks();

	//////////////////////////////////////////////////////////////////////
	// 
	// commands
	//
	//////////////////////////////////////////////////////////////////////

	INTERFACE int  activate(void);
	INTERFACE void deactivate(void);
	INTERFACE void reset(void);
	INTERFACE void start(void);
	INTERFACE void stop(void);
	INTERFACE void playZero(void);
	INTERFACE void playRange(int start, int end);

	//////////////////////////////////////////////////////////////////////
	//
	// timing parameters
	//
	//////////////////////////////////////////////////////////////////////

	INTERFACE float getTempo(void);
	INTERFACE void setTempo(float t);
	INTERFACE void setTempo(float t, int cpb);
	INTERFACE int getResolution(void);
	INTERFACE void setResolution(int cpb);
	INTERFACE int getBeatsPerMeasure(void);
	INTERFACE void setBeatsPerMeasure(int b);
	INTERFACE int getClock(void);
	INTERFACE void setClock(int c);
	INTERFACE int getSongPosition(void);
	INTERFACE void setSongPosition(int psn);
	INTERFACE int getMeasureWithClock(int clock);
	INTERFACE int getMeasureClock(int measure);

	//////////////////////////////////////////////////////////////////////	
	//
	// recording parameters
	//
	//////////////////////////////////////////////////////////////////////	

	INTERFACE void startRecording(SeqTrack *tr, int direct = 0);
	INTERFACE void stopRecording(void);
	INTERFACE int  isRecordingSequence(MidiSequence *seq);
	INTERFACE void acceptRecording(void);
	INTERFACE void revertRecording(void);
	INTERFACE void clearRecording(void);
	INTERFACE void startRecordErase(void);
	INTERFACE void stopRecordErase(void);

	int getLoopStart(void) {
		return loop_start;
	}
	INTERFACE void setLoopStart(int clock);

	void setLoopStartMeasure(int measure) {
		setLoopStart(getMeasureClock(measure));
	}

	int getLoopEnd(void) {
		return loop_end;
	}
	INTERFACE void setLoopEnd(int clock);

	void setLoopEndMeasure(int measure) {
		setLoopEnd(getMeasureClock(measure));
	}

	int getRecordMerge(void) {
		return rec_merge;
	}
	void setRecordMerge(int m) {
		rec_merge = m;
	}

	int getRecordCut(void) {
		return rec_cut;
	}
	void setRecordCut(int c) {
		rec_cut = c;
	}

	int getLoopStartEnable(void) {
		return loop_start_enable;
	}
	void setLoopStartEnable(int e) {
		loop_start_enable = e;
	}

	int getLoopEndEnable(void) {
		return loop_end_enable;
	}
	void setLoopEndEnable(int e) {
		loop_end_enable = e;
	}

	int getPunchIn(void) {
		return punch_in;
	}
	void setPunchIn(int p) {
		punch_in = p;
	}

	int getPunchOut(void) {
		return punch_out;
	}
	void setPunchOut(int p) {
		punch_out = p;
	}

	int getPunchInEnable(void) {
		return punch_in_enable;
	}
	void setPunchInEnable(int e) {
		punch_in_enable = e;
	}

	int getPunchOutEnable(void) {
		return punch_out_enable;
	}
	void setPunchOutEnable(int e) {
		punch_out_enable = e;
	}

	//////////////////////////////////////////////////////////////////////
	//
	// midi parameters
	//
	//////////////////////////////////////////////////////////////////////

	INTERFACE int openInputPort(const char *portName);
	INTERFACE int openInputPort(int id);
	INTERFACE void removeInputPort(int id);
	INTERFACE int openOutputPort(const char *portName);
	INTERFACE int openOutputPort(int id);
	INTERFACE void removeOutputPort(int id);
	INTERFACE void setDefaultInputPort(int port);
	INTERFACE void setDefaultOutputPort(int port);

	INTERFACE void setMidiSync(int port, int enable);

	INTERFACE void sendNote(int port, int channel, int key, int velocity);
	INTERFACE void sendProgram(int port, int channel, int program);
	INTERFACE void sendSongSelect(int port, int song);
	INTERFACE void sendControl(int port, int channel, int cont, int value);
	INTERFACE int sendSysex(int port, const unsigned char *bytes, int length);
	INTERFACE int sendSysexNoWait(int port, const unsigned char *buffer, 
								  int length);
	INTERFACE int requestSysex(int outPort, int inPort,
							   const unsigned char *buffer, int length,
							   unsigned char *reply, int maxlen);


	INTERFACE int startSysex(int outPort, int inPort,
							 const unsigned char *request, int length);
	INTERFACE int startSysex(int inPort);
	INTERFACE void startNextSysex();
	INTERFACE int getSysexSendStatus(void);
	INTERFACE int getSysexBytesReceived();
	INTERFACE int getSysexBytesReceiving();
	INTERFACE int getSysexResult(unsigned char *reply, int maxlen);
	INTERFACE void endSysex(void);

	INTERFACE void enableEcho(int inPort, int outPort, int channel);
	INTERFACE void setEchoKey(int key);
	INTERFACE void disableEcho(void);

	//////////////////////////////////////////////////////////////////////
	//
	// misc options
	//
	//////////////////////////////////////////////////////////////////////
	
	// input filters
	INTERFACE class MidiFilter *getFilters(int port);

	// built-in metronome
	class SeqMetronome *getMetronome(void) {
		return metronome;
	}

	// optional start/stop clocks
	
	int getStartClock(void) {
		return start_clock;
	}
	void setStartClock(int clock) {
		start_clock = clock;
	}
	int getStartClockEnable(void) {
		return start_enable;
	}
	void setStartClockEnable(int e) {
		start_enable = e;
	}
	int getEndClock(void) {
		return end_clock;
	}
	void setEndClock(int clock) {
		end_clock = clock;
	}
	int getEndClockEnable(void) {
		return end_enable;
	}
	void setEndClockEnable(int e) {
		end_enable = e;
	}

	//////////////////////////////////////////////////////////////////////
	//
	// 	status
	//
	//////////////////////////////////////////////////////////////////////

	int isRunning(void) {
		return running;
	}

	int isRecording(void) {
		return (recording != NULL);
	}

	INTERFACE int areEventsWaiting(void);
	INTERFACE int areNotesHanging(void);

	//////////////////////////////////////////////////////////////////////
	//
	// private
	//
	//////////////////////////////////////////////////////////////////////

  private:

	// our associated global module state
	class SeqTrack		*mTracks;		// list of installed tracks
	class SeqTrack		*mPlaying;		// tracks currently playing
	class SeqRecording	*mRecording;	// recording state
	class SeqMetronome	*mMetronome;	// metronome state

	//////////////////////////////////////////////////////////////////////
	//
	// user callbacks
	//
	//////////////////////////////////////////////////////////////////////

	SEQ_CALLBACK_BEAT		mCallbackBeat;
	SEQ_CALLBACK_NOTE		mCallbackNote;
	SEQ_CALLBACK_NOTE		mCallbackwatch;
	SEQ_CALLBACK_COMMAND	mCallbackcommand;
	SEQ_CALLBACK_RECORD		mCallbackrecord;
	SEQ_CALLBACK_EVENT		mCallbackevent;
	SEQ_CALLBACK_LOOP		mCallbackloop;
	MidiListener* 			mListener;

	// event state

	class CriticalSection	*mCsect;
	int						mEventMask;
	SeqEvent				*mEvents;
	SeqEvent				*mLastEvent;
	SeqEvent				*mEventPool;

	//////////////////////////////////////////////////////////////////////
	//
	// device state
	//
	//////////////////////////////////////////////////////////////////////

	class Timer			*timer;

	class MidiIn		*_inputs[1];
	int _lastInput;
	int _defaultInput;

	class MidiOut		*_outputs[1];
	int _lastOutput;
	int _defaultOutput;

	// transient state for the last sysex request

	class MidiIn *_sysexInput;
	class MidiOut *_sysexOutput;

	// input device that's echoing
	class MidiIn *_echoInput;

	//////////////////////////////////////////////////////////////////////
	//
	// misc parameters
	//
	//////////////////////////////////////////////////////////////////////
	
	int				start_clock;		// clock to start on
	int				start_enable;	    // set to enable start_clock
	int				end_clock;			// clock to stop on
	int				end_enable;			// set to enable end_clock

	//////////////////////////////////////////////////////////////////////
	//
	// recording parameters
	// these are kept here since the SeqRecording object can come and
	// go, but the parameters apply forever
	//
	//////////////////////////////////////////////////////////////////////
	
	int 			punch_in;			// punch registers
	int 			punch_in_enable;
	int				punch_out;
	int 			punch_out_enable;

	int				loop_start;			//  default zero
	int 			loop_start_enable;
	int				loop_end;			// SEQ_CLOCK_INFINITE for no looping
	int 			loop_end_enable;

	// perform a simple merge after the recording finishes
	int 			rec_merge;

	// to cut recorded notes to the range of the loop/punch
	int 			rec_cut;

	//////////////////////////////////////////////////////////////////////
	//
	// internal transient state
	//
	//////////////////////////////////////////////////////////////////////

	int				running;			// non-zero if currently running
	int				sweeping;			// if in the interrupt handler
	int				pending_stop;		// when needing to stop
	int 			next_beat_clock;	// time when next beat occurs
	int 			next_sweep_clock;	// time when tracks need attention
	int				debug_track_sweep;	// set when debugging interrupts

	//////////////////////////////////////////////////////////////////////
	//
	// private methods
	//
	//////////////////////////////////////////////////////////////////////
	
	// use create method...
	Sequencer();

	//
	// seq.cxx
	//

	void clockKickoff(void);
	void startInternal(int do_callback);
	void stopInternal(int do_callback);

	void enterCriticalSection(void);
	void leaveCriticalSection(void);
	void addEvent(SeqEventType t, int clock, int duration, int value);

	//
	// track.cxx
	//

	void startTracks(void);
	int  sweepTracks(int clock);
	void stopTracks(void);
	int  getFirstSweepClock(void);

	//
	// seqint.cxx
	//

	void timerCallback(void);
	void midiInCallback(class MidiIn *input);

};

/****************************************************************************
 *                                                                          *
 *                                   TRACK                                  *
 *                                                                          *
 ****************************************************************************/

/**
 * A sequencer will contain zero or more tracks, that maintain state 
 * about a sequence installed in the sequencer.  A track will
 * usually have a Sequence object that it is managing, though I suppose
 * we could allow for a fixed number of tracks, that are unused.
 */
class SeqTrack {

	friend class Sequencer;
	friend class SeqRecording;

  public:

	//////////////////////////////////////////////////////////////////////
	// 
	// field accessors
	//
	//////////////////////////////////////////////////////////////////////

	SeqTrack *getNext(void) {
		return next;
	}

	MidiSequence *getSequence(void) {
		return seq;
	}

	// let this come from the sequence, unless overridden
	INTERFACE int getChannel(void);

	int isMute(void) {
		return muted;
	}

	int isDisabled(void) {
		return disabled;
	}

	int isWatched(void) {
		return being_watched;
	}

	int isRecording(void) {
		return being_recorded;
	}

	//////////////////////////////////////////////////////////////////////
	//
	// operations
	//
	//////////////////////////////////////////////////////////////////////

	void setChannel(int c) {
		mChannel = c;
	}

	void setMute(int m) {
		muted = m;
	}

	void setDisabled(int d) {
		disabled = d;
	}

	void setWatched(int w) {
		being_watched = w;
	}

	INTERFACE void clear(void);

	INTERFACE void dump(void);

	void startRecording(int direct = 0) {
		sequencer->startRecording(this, direct);
	}

	void stopRecording(void) {
		sequencer->stopRecording();
	}

  protected:

	//////////////////////////////////////////////////////////////////////
	// 
	// field accessors
	//
	//////////////////////////////////////////////////////////////////////

	SeqTrack *getPlayLink(void) {
		return playlink;
	}

	MidiEvent *getEvents(void) {
		return events;
	}

	MidiEvent *getOn(void) {
		return on;
	}

	int getLoopAdjust(void) {
		return loop_adjust;
	}

	//////////////////////////////////////////////////////////////////////
	//
	// constructors
	//
	//////////////////////////////////////////////////////////////////////

	SeqTrack(void) {

		next	 		= NULL;
		playlink		= NULL;
		sequencer		= NULL;
		seq 			= NULL;
		out				= NULL;

		// potential override
		mChannel			= -1;

		disabled		= 0;
		muted			= 0;
		being_recorded 	= 0;

		events			= NULL;
		on 				= NULL;
		loops			= NULL;
		loop_adjust		= 0;
	}

	~SeqTrack(void);

	void setSequencer(Sequencer *s) {
		sequencer = s;
	}

	// should this be public ? 
	void setSequence(MidiSequence *s) {
		seq = s;
	}

	void setBeingRecorded(int r) {
		being_recorded = r;
	}

	void setNext(SeqTrack *n) {
		next = n;
	}

	void setPlayLink(SeqTrack *tr) {
		playlink = tr;
	}

	void setOutput(class MidiOut *o) {
		out = o;
	}

	//////////////////////////////////////////////////////////////////////
	//
	// operations
	// 
	//////////////////////////////////////////////////////////////////////

	void start(int clock);
	void sweep(int clock);
	void stop(void);
	int  getNextClock(void);
	void reset(void);

	//////////////////////////////////////////////////////////////////////
	//
	// private
	//
	//////////////////////////////////////////////////////////////////////

  private:

	SeqTrack		*next;		// link within the global track list
	SeqTrack		*playlink;	// link within the playing track list
	Sequencer		*sequencer;	// owning sequencer
	MidiSequence	*seq;		// sequence we're playing
	class MidiOut	*out;		// output device

	// various flags

	int mChannel;
	int disabled;
	int muted;
	int being_recorded;
	int being_watched;

	// set to remove track during next pass, who uses this??
	int pending_disable;

	//
	// transient play state
	//

	MidiEvent 		*events;  	// current position in the event list
	MidiEvent 		*on;		// list of "on" notes
	class SeqLoop	*loops;		// list of loops in progress

	// clock adjustment when loops are present
	int				loop_adjust;

	//////////////////////////////////////////////////////////////////////
	//
	// internal methods
	//
	//////////////////////////////////////////////////////////////////////

	void processCallbacks(MidiEvent *e, int on);
	void pushLoop(MidiEvent *e);
	void annotateLoopEvent(MidiEvent *e);
	void annotateLoops(void);
	void cleanupLoops(void);
	void doLoop(void);
	void flushOn(void);
	void centerControllers(void);
	void forceOff(MidiEvent *e);
	void sendEvents(int clock);
	void endEvents(int clock);

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
