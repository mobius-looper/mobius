/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Sample is a model for sample files that can be loaded for triggering.
 *
 * SampleTrack is an extension of RecorderTrack that adds basic sample
 * playback capabilities to Mobius
 *
 */

#ifndef SAMPLE_H
#define SAMPLE_H

#include <stdio.h>

#include "Util.h"

#include "Audio.h"
#include "Recorder.h"

//////////////////////////////////////////////////////////////////////
//
// Sample
//
//////////////////////////////////////////////////////////////////////

/**
 * Root XML element.
 */
#define EL_SAMPLES "Samples"

/**
 * The definition of a sample that can be played by SampleTrack.
 * A list of these will be found in a Samples object which in turn
 * will be in the MobiusConfig.
 *
 * We convert these to SamplePlayers managed by one SampleTrack.
 */
class Sample
{
  public:

	Sample();
	Sample(const char* file);
	Sample(class XmlElement* e);
	~Sample();

	void setNext(Sample* s);
	Sample* getNext();

	void setFilename(const char* file);
	const char* getFilename();

	void setSustain(bool b);
	bool isSustain();

	void setLoop(bool b);
	bool isLoop();

    void setConcurrent(bool b);
    bool isConcurrent();

	void parseXml(class XmlElement* e);
	void toXml(class XmlBuffer* b);

  private:
	
	void init();

	Sample* mNext;
	char* mFilename;

    //
    // NOTE: These were experimental options that have never
    // been used.  It doesn't feel like these should even be
    // Sample-specific options but maybe...
    // 

	/**
	 * When true, playback continues only as long as the trigger
	 * is sustained.  When false, the sample always plays to the end
	 * and stops.
	 */
	bool mSustain;

	/** 
	 * When true, playback loops for as long as the trigger is sustained
	 * rather than stopping when the audio ends.  This is relevant
	 * only if mSustain is true.
     *
     * ?? Is this really necessary, just make this an automatic
     * part of mSustain behavior.  It might be fun to let this
     * apply to non sustained samples, but then we'd need a way
     * to shut it off!  Possibly the down transition just toggles
     * it on and off.
	 */
	bool mLoop;

    /**
     * When true, multiple overlapping playbacks of the sample
     * are allowed.  This is really meaningful only when mSustain 
     * is false since you would have to have an up transition before
     * another down transition.  But I suppose you could configure
     * a MIDI controller to do that.  This is also what you'd want
     * to implement pitch adjusted playback of the same sample.
     */
    bool mConcurrent;

};

//////////////////////////////////////////////////////////////////////
//
// Samples
//
//////////////////////////////////////////////////////////////////////

/**
 * Encapsulates a collection of samples for configuration storage.
 * One of these can be the MoibusConfig as well as local to a Project.
 * This will also be given to SampleTrack.
 */
class Samples
{
  public:

	Samples();
	Samples(class XmlElement* e);
	~Samples();

	void clear();
	void add(Sample* s);

	Sample* getSamples();

	void parseXml(class XmlElement* e);
	void toXml(class XmlBuffer* b);

  private:

	Sample* mSamples;
	
};

//////////////////////////////////////////////////////////////////////
//
// SampleTrigger
//
//////////////////////////////////////////////////////////////////////

#define MAX_TRIGGERS 8

/**
 * Helper struct to represent one sample trigger event.
 * Each SamplePlayer maintains an array of these which are filled
 * by the ui and/or MIDI thread, and consumed by the audio thread.
 * To avoid a critical section, there are two indexes into the array.
 * The "head" index is the index of the first element that needs
 * to be processed by the audio thread.  The "tail" index is the
 * index of the next element available to be filled by the ui interrupt.
 * When the head and tail indexes are the same there is nothing in the
 * queue.  Only the audio thread advances the head index, only the ui
 * thread advances the tail index.  If the tail indexes advances to
 * the head index, there is an overflow.
 *
 * We don't handle trigger overflow gracefully, but this could only
 * happen if you were triggering more rapidly than audio interrupt
 * interval.  In practice, humans couldn't do this, though another
 * machine feeding us MIDI triggers could cause this.
 *
 * UPDATE: Sample triggering is now handled by the Action model
 * so triggers will always be done inside the interrupt, we don't
 * need the ring buffer.
 */
typedef struct {

    // true if this is a down transition
    bool down;

} SampleTrigger;

//////////////////////////////////////////////////////////////////////
//
// SamplePlayer
//
//////////////////////////////////////////////////////////////////////

/**
 * Represents one loaded sample that can be played by SampleTrack.
 * 
 * These are built from the Samples list in the MobiusConfig and do not
 * retain any references to it.  A list of these is phased into the
 * audio interrupt handler with a SamplePack object.
 *
 * TODO: Might be interesting to give this capabilities like Segemnt
 * or Layer so we could dynamically define samples from loop material.
 * 
 */
class SamplePlayer
{
    friend class SampleCursor;

  public:

	SamplePlayer(class AudioPool* pool, const char* homedir, Sample* s);
	~SamplePlayer();

    void updateConfiguration(int inputLatency, int outputLatency);

	void setNext(SamplePlayer* sp);
	SamplePlayer* getNext();

    const char* getFilename();

	void setAudio(Audio* a);
	Audio* getAudio();
	long getFrames();

	void setSustain(bool b);
	bool isSustain();

	void setLoop(bool b);
	bool isLoop();

    void setConcurrent(bool b);
    bool isConcurrent();
	void trigger(bool down);
	void play(float* inbuf, float* outbuf, long frames);

  protected:

    //
    // Configuration caches.
    // I don't really like having these here but I don't want to 
    // introduce a dependency on Mobius at this level.  Although these
    // are only used by SampleCursor, they're maintained here to 
    // make them easier to udpate.
    //

	/**
	 * Number of frames to perform a gradual fade out when ending
	 * the playback early.  Supposed to be synchronized with
	 * the MobiusConfig, but could be independent.
	 */
	long mFadeFrames;

    /**
     * Number of frames of input latency.
     */
    long mInputLatency;

    /**
     * Number of frames of output latency.
     */
    long mOutputLatency;

  private:
	
	void init();
    class SampleCursor* newCursor();
    void freeCursor(class SampleCursor* c);

	SamplePlayer* mNext;
    char* mFilename;
	Audio* mAudio;

	// flags copied from the Sample
	bool mSustain;
	bool mLoop;
    bool mConcurrent;

    /**
     * A queue of trigger events, filled by the ui thread and
     * consumed by the audio thread.
     */
    SampleTrigger mTriggers[MAX_TRIGGERS];

    int mTriggerHead;
    int mTriggerTail;

    /**
     * As the sample is triggered, we will active one or more 
     * SampleCursors.  This is the list of active cursors.
     */
    class SampleCursor* mCursors;

    class SampleCursor* mCursorPool;

    /**
     * Transient runtime trigger state to detect keyboard autorepeat.
     * This may conflict with MIDI triggering!
     */
    bool mDown;

};

//////////////////////////////////////////////////////////////////////
//
// SamplePack
//
//////////////////////////////////////////////////////////////////////

/**
 * A temporary structure used to pass a list of SamplePlayers
 * from the UI thread that has been editing samples into the 
 * audio interrupt handler.  We need this because in order to clear
 * the sample list we have to have some non-null object to pass down,
 * so the SamplePack may be empty.
 */
class SamplePack {
  public:

    SamplePack();
    SamplePack(class AudioPool* pool, const char* homedir, Samples* samples);

    ~SamplePack();

    SamplePlayer* getSamples();
    SamplePlayer* stealSamples();

  private:

    SamplePlayer* mSamples;
};

//////////////////////////////////////////////////////////////////////
//
// SampleCursor
//
//////////////////////////////////////////////////////////////////////

/**
 * Encapsulates the state of one trigger of a SamplePlayer.
 */
class SampleCursor
{
    friend class SamplePlayer;

  public:
    
    SampleCursor();
    SampleCursor(SamplePlayer* s);
    ~SampleCursor();

    SampleCursor* getNext();
    void setNext(SampleCursor* next);

    void play(float* inbuf, float* outbuf, long frames);
    void play(float* outbuf, long frames);

    void stop();
    bool isStopping();
    bool isStopped();

  protected:

    // for SamplePlayer
    void setSample(class SamplePlayer* s);

  private:

    void init();
	void stop(long maxFrames);

    SampleCursor* mNext;
    SampleCursor* mRecord;
    SamplePlayer* mSample;
	AudioCursor* mAudioCursor;

    bool mStop;
    bool mStopped;
    long mFrame;

	/**
	 * When non-zero, the number of frames to play, which may
     * be less than the number of available frames.
	 * This is used when a sustained sample is ended prematurely.  
	 * We set up a fade out and continue past the trigger frame 
	 * to this frame.  Note that this is a frame counter, 
     * not the offset to the last frame.  It will be one beyond
     * the last frame that is to be played.
	 */
	long mMaxFrames;

};

//////////////////////////////////////////////////////////////////////
//
// SampleTrack
//
//////////////////////////////////////////////////////////////////////

/**
 * The maximum number of samples that SampleTrack can manage.
 */
#define MAX_SAMPLES 8

/**
 * Makes a collection of SamplePlayers available for realtime playback
 * through the Recorder.
 */
class SampleTrack : public RecorderTrack {

  public:

	SampleTrack(class Mobius* mob);
	~SampleTrack();

    bool isDifference(Samples* samples);

	bool isPriority();
    int getSampleCount();
	void setSamples(class SamplePack* pack);
	void updateConfiguration(MobiusConfig* config);

	/**
	 * Triggering by internal sample index.
	 * TODO: Trigger by MIDI note with velocity!
	 */
	void trigger(int index);
	long getLastSampleFrames();

	/**
	 * Trigger a sustained sample.
	 * Only for use by Mobius in response to function handlers.
	 */
	void trigger(AudioStream* stream, int index, bool down);

	void prepareForInterrupt();
	void processBuffers(AudioStream* stream,
						float* inbuf, float *outbuf, long frames, 
						long frameOffset);

  private:
	
	void init();

	class Mobius* mMobius;
	SamplePlayer* mPlayerList;
	SamplePlayer* mPlayers[MAX_SAMPLES];
	int mSampleCount;
	int mLastSample;
	bool mTrackProcessed;

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
