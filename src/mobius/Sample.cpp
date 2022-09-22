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

#include <stdio.h>
#include <memory.h>

#include "Trace.h"
#include "Util.h"
#include "XmlBuffer.h"
#include "XmlModel.h"

#include "Mobius.h"
#include "MobiusConfig.h"

#include "Sample.h"

//////////////////////////////////////////////////////////////////////
//
// XML Constants
//
//////////////////////////////////////////////////////////////////////

#define EL_SAMPLE "Sample"
#define ATT_PATH "path"
#define ATT_SUSTAIN "sustain"
#define ATT_LOOP "loop"
#define ATT_CONCURRENT "concurrent"

//////////////////////////////////////////////////////////////////////
//
// Sample
//
//////////////////////////////////////////////////////////////////////

Sample::Sample()
{
	init();
}

Sample::Sample(const char* file)
{
	init();
	setFilename(file);
}

Sample::Sample(XmlElement* e)
{
	init();
	parseXml(e);
}

void Sample::init()
{
	mNext = NULL;
	mFilename = NULL;
	mSustain = false;
	mLoop = false;
	mConcurrent = false;
}

Sample::~Sample()
{
	delete mFilename;
	
    Sample* next = NULL;
    for (Sample* s = mNext ; s != NULL ; s = next) {
        next = s->getNext();
		s->setNext(NULL);
        delete s;
    }
}

void Sample::setNext(Sample* s) 
{
	mNext = s;
}

Sample* Sample::getNext()
{
	return mNext;
}

void Sample::setFilename(const char* s)
{
	delete mFilename;
	mFilename = CopyString(s);
}

const char* Sample::getFilename()
{
	return mFilename;
}

void Sample::setSustain(bool b)
{
	mSustain = b;
}

bool Sample::isSustain()
{
	return mSustain;
}

void Sample::setLoop(bool b)
{
	mLoop = b;
}

bool Sample::isLoop()
{
	return mLoop;
}

void Sample::setConcurrent(bool b)
{
	mConcurrent = b;
}

bool Sample::isConcurrent()
{
	return mConcurrent;
}

void Sample::toXml(XmlBuffer* b)
{
	b->addOpenStartTag(EL_SAMPLE);
	if (mFilename != NULL)
	  b->addAttribute(ATT_PATH, mFilename);
	b->addAttribute(ATT_SUSTAIN, mSustain);
	b->addAttribute(ATT_LOOP, mLoop);
	b->addAttribute(ATT_CONCURRENT, mConcurrent);
	b->add("/>\n");
}

void Sample::parseXml(XmlElement* e)
{
	setFilename(e->getAttribute(ATT_PATH));
	mSustain = e->getBoolAttribute(ATT_SUSTAIN);
	mLoop = e->getBoolAttribute(ATT_LOOP);
	mConcurrent = e->getBoolAttribute(ATT_CONCURRENT);
}

//////////////////////////////////////////////////////////////////////
//
// Samples
//
//////////////////////////////////////////////////////////////////////

Samples::Samples()
{
	mSamples = NULL;
}

Samples::Samples(XmlElement* e)
{
	mSamples = NULL;
	parseXml(e);
}

Samples::~Samples()
{
	delete mSamples;
}

Sample* Samples::getSamples()
{
	return mSamples;
}

void Samples::clear()
{
	delete mSamples;
	mSamples = NULL;
}

void Samples::add(Sample* neu)
{
	Sample* last = NULL;
	for (Sample* s = mSamples ; s != NULL ; s = s->getNext())
	  last = s;

	if (last == NULL)
	  mSamples = neu;
	else
	  last->setNext(neu);
}

void Samples::toXml(XmlBuffer* b)
{
	b->addStartTag(EL_SAMPLES);
	b->incIndent();
	if (mSamples != NULL) {
		for (Sample* s = mSamples ; s != NULL ; s = s->getNext()) {
			s->toXml(b);
		}
	}
	b->decIndent();
	b->addEndTag(EL_SAMPLES);
}

void Samples::parseXml(XmlElement* e)
{
	Sample* last = NULL;

	for (XmlElement* child = e->getChildElement() ; child != NULL ; 
		 child = child->getNextElement()) {

		Sample* s = new Sample(child);
		if (last == NULL)
		  mSamples = s;
		else
		  last->setNext(s);
		last = s;
	}
}

//////////////////////////////////////////////////////////////////////
//
// SamplePlayer
//
//////////////////////////////////////////////////////////////////////

SamplePlayer::SamplePlayer(AudioPool* pool, const char* homedir, Sample* src)
{
	init();
	
    mFilename = CopyString(src->getFilename());
	mSustain = src->isSustain();
	mLoop = src->isLoop();
	mConcurrent = src->isConcurrent();

	if (mFilename != NULL) {
		// always check CWD or always relative to homedir?
		char buffer[1024 * 8];
		MergePaths(homedir, mFilename, buffer, sizeof(buffer));
		Trace(2, "Loading sample %s\n", buffer);
		mAudio = pool->newAudio(buffer);
	}
}

void SamplePlayer::init()
{
	mNext = NULL;
    mFilename = NULL;
	mAudio = NULL;
	mSustain = false;
	mLoop = false;
	mConcurrent = false;

    mCursors = NULL;
    mCursorPool = NULL;
    mTriggerHead = 0;
    mTriggerTail = 0;
	mDown = false;

	mFadeFrames = 0;
    mInputLatency = 0;
    mOutputLatency = 0;
}

SamplePlayer::~SamplePlayer()
{
    delete mFilename;
	delete mAudio;

    // if we had a global cursor pool, this should
    // return it to the pool instead of deleting
    delete mCursors;
    delete mCursorPool;

    SamplePlayer* nextp = NULL;
    for (SamplePlayer* sp = mNext ; sp != NULL ; sp = nextp) {
        nextp = sp->getNext();
		sp->setNext(NULL);
        delete sp;
    }

}

const char* SamplePlayer::getFilename()
{
    return mFilename;
}

void SamplePlayer::setNext(SamplePlayer* sp)
{
	mNext = sp;
}

SamplePlayer* SamplePlayer::getNext()
{
	return mNext;
}

void SamplePlayer::setAudio(Audio* a)
{
	mAudio = a;
}

Audio* SamplePlayer::getAudio()
{
	return mAudio;
}

void SamplePlayer::setSustain(bool b)
{
	mSustain = b;
}

bool SamplePlayer::isSustain()
{
	return mSustain;
}

void SamplePlayer::setLoop(bool b)
{
	mLoop = b;
}

bool SamplePlayer::isLoop()
{
	return mLoop;
}

void SamplePlayer::setConcurrent(bool b)
{
	mConcurrent = b;
}

bool SamplePlayer::isConcurrent()
{
	return mConcurrent;
}

long SamplePlayer::getFrames()
{
	long frames = 0;
	if (mAudio != NULL)
	  frames = mAudio->getFrames();
	return frames;
}

/**
 * Incorporate changes made to the global configuration.
 * Trying to avoid a Mobius dependency here so pass in what we need.
 * Keep this out of SampleCursor so we don't have to mess with
 * synchronization between the UI and audio threads.
 *
 * UPDATE: Sample triggers are handled by Actions now so we will
 * always be in the audio thread.
 */
void SamplePlayer::updateConfiguration(int inputLatency, int outputLatency)
{
    mInputLatency = inputLatency;
    mOutputLatency = outputLatency;
}

/**
 * If this is bound to the keyboard, auto-repeat will keep
 * feeding us triggers rapidly.  If this isn't a sustain sample,
 * then assume this means we're supposed to restart.  If it is a
 * sustain sample, then we need to wait for an explicit up trigger.
 * This state has to be held even after a non-loop sample has finished
 * playing and become inactive.
 */
void SamplePlayer::trigger(bool down)
{

	// !! still having the auto-repeat problem with non-sustained
	// concurrent samples

    bool doTrigger = false;
    if (down) {
        if (!mDown || !mSustain)
          doTrigger = true;
        mDown = true;
    }
    else {
        // only relevant for sustained samples
        if (mSustain)
          doTrigger = true;
        mDown = false;
    }

    if (doTrigger) {
        int nextTail = mTriggerTail + 1;
        if (nextTail >= MAX_TRIGGERS)
          nextTail = 0;

        if (nextTail == mTriggerHead) {
            // trigger overflow, audio must be unresponsive or
            // we're receiving triggers VERY rapidly, would be nice
            // to detect unresponse audio and just start ignoring
            // triggers
            Trace(1, "SamplePlayer::trigger trigger overflow\n");
        }
        else {
            // eventually have other interesting things here, like key
            mTriggers[mTriggerTail].down = down;
            mTriggerTail = nextTail;
        }

    }
}

/**
 * Play/Record the sample.
 * 
 * Playback is currently inaccurate in that we'll play from the beggining
 * when we should logically start from mOutputLatency in order to synchronize
 * the recording with the output.  
 *
 * Recording is done accurately.  The frame counter is decremented by
 * mInputLatency, and when this goes positive we begin filling the input 
 * buffer.
 * 
 */
void SamplePlayer::play(float* inbuf, float* outbuf, long frames)
{
    // process triggers
    while (mTriggerHead != mTriggerTail) {
        SampleTrigger* t = &mTriggers[mTriggerHead++];
		if (mTriggerHead >= MAX_TRIGGERS)
		  mTriggerHead = 0;

        if (!t->down) {
            if (mConcurrent) {
                // the up transition belongs to the first cursor
                // that isn't already in the process of stopping
                for (SampleCursor* c = mCursors ; c != NULL ; 
                     c = c->getNext()) {
                    if (!c->isStopping()) {
                        c->stop();
                        break;
                    }
                }
            }
            else {
                // should be only one cursor, make it stop
                if (mCursors != NULL)
                  mCursors->stop();
            }
        }
        else if (mConcurrent) {
            // We start another cursor and let the existing ones finish
            // as they may.  Keep these ordered.
            SampleCursor* c = newCursor();
			SampleCursor* last = NULL;
            for (last = mCursors ; last != NULL && last->getNext() != NULL ; 
                 last = last->getNext());
            if (last != NULL)
              last->setNext(c);
            else
              mCursors = c;
        }
        else {
            // stop existing cursors, start a new one
            // the effect is similar to a forced up transition but
            // we want the current cursor to end cleanly so that it
            // gets properly recorded and fades nicely.

            SampleCursor* c = newCursor();
			SampleCursor* last = NULL;
            for (last = mCursors ; last != NULL && last->getNext() != NULL ; 
                 last = last->getNext()) {
                // stop them while we look for the last one
                last->stop();
            }
            if (last != NULL)
              last->setNext(c);
            else
              mCursors = c;
        }
    }

    // now process cursors

    SampleCursor* prev = NULL;
    SampleCursor* next = NULL;
    for (SampleCursor* c = mCursors ; c != NULL ; c = next) {
        next = c->getNext();

        c->play(inbuf, outbuf, frames);
        if (!c->isStopped())
          prev = c;
        else {
            // splice it out of the list
            if (prev == NULL)
              mCursors = next;
            else
              prev->setNext(next);
            freeCursor(c);
        }
    }
}

/**
 * Allocate a cursor.
 * Keep these pooled since there are several things in them.
 * Ideally there should be only one pool, but we would have
 * to root it in SampleTrack and pass it down.
 */
SampleCursor* SamplePlayer::newCursor()
{
    SampleCursor* c = mCursorPool;
    if (c == NULL) {
        c = new SampleCursor(this);
    }
    else {
        mCursorPool = c->getNext();
		c->setNext(NULL);
        c->setSample(this);
    }
    return c;
}

/**
 * Return a cursor to the pool.
 */
void SamplePlayer::freeCursor(SampleCursor* c)
{
    c->setNext(mCursorPool);
    mCursorPool = c;
}

//////////////////////////////////////////////////////////////////////
//
// SamplePack
//
//////////////////////////////////////////////////////////////////////

SamplePack::SamplePack()
{
    mSamples = NULL;
}

SamplePack::SamplePack(AudioPool* pool, const char* homedir, Samples* samples)
{
    mSamples = NULL;

	SamplePlayer* last = NULL;

    if (samples != NULL) {
        for (Sample* s = samples->getSamples() ; s != NULL ; s = s->getNext()) {
            SamplePlayer* p = new SamplePlayer(pool, homedir, s);
            if (last == NULL)
              mSamples = p;
            else
              last->setNext(p);
            last = p;
        }
    }
}

SamplePack::~SamplePack()
{
    // currently we always make one of these and call stealSamples so
    // techniqlly we don't need to free anything, but we should 
}

SamplePlayer* SamplePack::getSamples()
{
    return mSamples;
}

SamplePlayer* SamplePack::stealSamples()
{
    SamplePlayer* samples = mSamples;
    mSamples = NULL;
    return samples;
}

//////////////////////////////////////////////////////////////////////
//
// SampleCursor
//
//////////////////////////////////////////////////////////////////////

/*
 * Each cursor represents the playback of one trigger of the
 * sample.  To implement the insertion of the sample into
 * the recorded audio stream, we'll actually maintain two cursors.
 * The outer cursor handles the realtime playback of the sample, 
 * the inner cursor handles the "recording" of the sample into the
 * input stream.  
 *
 * Implementing this as cursor pairs was easy since they have to 
 * do almost identical processing, and opens up some interesting
 * possibilities.
 */

/**
 * Constructor for record cursors.
 */
SampleCursor::SampleCursor()
{
    init();
}

/**
 * Constructor for play cursors.
 * UPDATE: I haven't been back here for a long time, but it looks like
 * we're always creating combo play/record cursors.
 */
SampleCursor::SampleCursor(SamplePlayer* s)
{
    init();
	mRecord = new SampleCursor();
	setSample(s);
}


/**
 * Initialize a freshly allocated cursor.
 */
void SampleCursor::init()
{
    mNext = NULL;
    mRecord = NULL;
    mSample = NULL;
	mAudioCursor = new AudioCursor();
    mStop = false;
    mStopped = false;
    mFrame = 0;
    mMaxFrames = 0;
}

SampleCursor::~SampleCursor()
{
	delete mAudioCursor;
    delete mRecord;

    SampleCursor *el, *next;
    for (el = mNext ; el != NULL ; el = next) {
        next = el->getNext();
		el->setNext(NULL);
        delete el;
    }
}

void SampleCursor::setNext(SampleCursor* c)
{
    mNext = c;
}

SampleCursor* SampleCursor::getNext()
{
    return mNext;
}

/**
 * Reinitialize a pooled cursor.
 *
 * The logic is quite contorted here, but every cursor appears to 
 * have an embedded record cursor.  
 */
void SampleCursor::setSample(SamplePlayer* s)
{
    mSample = s;
	mAudioCursor->setAudio(mSample->getAudio());
    mStop = false;
    mStopped = false;
    mMaxFrames = 0;

    if (mRecord != NULL) {
        // we're a play cursor
        mRecord->setSample(s);
        mFrame = 0;
    }
    else {
        // we're a record cursor

		// !! This stopped working after the great autorecord/sync 
		// rewrite.  Scripts are expecting samples to play into the input 
		// buffer immediately, at least after a Wait has been executed
		// and we're out of latency compensation mode.  We probably need to 
		// be more careful about passing the latency context down
		// from SampleTrack::trigger, until then assume we're not
		// compensating for latency

        //mFrame = -(mSample->mInputLatency);
		mFrame = 0;
    }

}

bool SampleCursor::isStopping()
{
    return mStop;
}

bool SampleCursor::isStopped()
{
    bool stopped = mStopped;
    
    // if we're a play cursor, then we're not considered stopped
    // until the record cursor is also stopped
    if (mRecord != NULL)
      stopped = mRecord->isStopped();

    return stopped;
}

/**
 * Called when we're supposed to stop the cursor.
 * We'll continue on for a little while longer so we can fade
 * out smoothly.  This is called only for the play cursor,
 * the record cursor lags behind so we call stop(frame) when
 * we know the play frame to stop on.
 * 
 */
void SampleCursor::stop()
{
    if (!mStop) {
		long maxFrames = 0;
        Audio* audio = mSample->getAudio();
		long sampleFrames = audio->getFrames();
		maxFrames = mFrame + AudioFade::getRange();
		if (maxFrames >= sampleFrames) {
			// must play to the end assume it has been trimmed
			// !! what about mLoop, should we set this
			// to sampleFrames so it can end?
			maxFrames = 0;
		}

		stop(maxFrames);
		if (mRecord != NULL)
		  mRecord->stop(maxFrames);
	}
}

/**
 * Called for both the play and record cursors to stop on a given
 * frame.  If the frame is before the end of the audio, then we set
 * up a fade.
 */
void SampleCursor::stop(long maxFrames)
{
    if (!mStop) {
        Audio* audio = mSample->getAudio();
		if (maxFrames > 0)
		  mAudioCursor->setFadeOut(maxFrames);
		mMaxFrames = maxFrames;
        mStop = true;
    }
}

/**
 * Play/Record more frames in the sample.
 */
void SampleCursor::play(float* inbuf, float* outbuf, long frames)
{
	// play
	if (outbuf != NULL)
	  play(outbuf, frames);

	// record
	if (mRecord != NULL && inbuf != NULL)
	  mRecord->play(inbuf, frames);
}

/**
 * Play more frames in the sample.
 */
void SampleCursor::play(float* outbuf, long frames)
{
    Audio* audio = mSample->getAudio();
    if (audio != NULL && !mStopped) {

        // consume dead input latency frames in record cursors
        if (mFrame < 0) {
            mFrame += frames;
            if (mFrame > 0) {
                // we advanced into "real" frames, back up
                int ignored = frames - mFrame;
                outbuf += (ignored * audio->getChannels());
                frames = mFrame;
                mFrame = 0;
            }
            else {
                // nothing of interest for this buffer
                frames = 0;
            }
        }

        // now record if there is anything left in the buffer
        if (frames > 0) {

			// !! awkward interface
			AudioBuffer b;
			b.buffer = outbuf;
			b.frames = frames;
			b.channels = 2;
			mAudioCursor->setAudio(audio);
			mAudioCursor->setFrame(mFrame);

            long sampleFrames = audio->getFrames();
            if (mMaxFrames > 0)
              sampleFrames = mMaxFrames;
            
            long lastBufferFrame = mFrame + frames - 1;
            if (lastBufferFrame < sampleFrames) {
				mAudioCursor->get(&b);
                mFrame += frames;
            }
            else {
                long avail = sampleFrames - mFrame;
                if (avail > 0) {
					b.frames = avail;
					mAudioCursor->get(&b);
                    mFrame += avail;
                }

                // if we get to the end of a sustained sample, and the
                // trigger is still down, loop again even if the loop 
                // option isn't on

                if (!mSample->mLoop &&
                    !(mSample->mDown && mSample->mSustain)) {
                    // we're done
                    mStopped = true;
                }
                else {
                    // loop back to the beginning
                    long remainder = frames - avail;
                    outbuf += (avail * audio->getChannels());

                    // should already be zero since if we ended a sustained
                    // sample early, it would have been handled in stop()?
                    if (mMaxFrames > 0)
                      Trace(1, "SampleCursor::play unexpected maxFrames\n");
                    mMaxFrames = 0;
                    mFrame = 0;

                    sampleFrames = audio->getFrames();
                    if (sampleFrames < remainder) {
                        // sample is less than the buffer size?
                        // shouldn't happen, handling this would make this
                        // much more complicated, we'd have to loop until
                        // the buffer was full
                        remainder = sampleFrames;
                    }

					b.buffer = outbuf;
					b.frames = remainder;
					mAudioCursor->setFrame(mFrame);
					mAudioCursor->get(&b);
                    mFrame += remainder;
                }
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
// SampleTrack
//
//////////////////////////////////////////////////////////////////////

SampleTrack::SampleTrack(Mobius* mob) 
{
	init();
	mMobius = mob;
}

void SampleTrack::init() 
{
    initRecorderTrack();

	mMobius = NULL;
	mPlayerList = NULL;
	mSampleCount = 0;
	mLastSample = -1;
	mTrackProcessed = false;

	for (int i = 0 ; i < MAX_SAMPLES ; i++)
	  mPlayers[i] = NULL;
}

SampleTrack::~SampleTrack()
{
	delete mPlayerList;
}

/**
 * Must overload this so we're processed first and can insert audio
 * into the input buffer.
 */
bool SampleTrack::isPriority()
{
    return true;
}

/**
 * Compare the sample definitions in a Samples object with the
 * active loaded samples.  If there are any differences it is a signal
 * the caller to reload the samples and phase them in to the next interrupt.
 *
 * Differencing is relatively crude, any order or length difference is
 * considered to be enough to reload the samples.  In theory if we have lots
 * of samples and people make small add/remove actions we could optimize
 * the rereading but for now that's not very important.
 *
 */
bool SampleTrack::isDifference(Samples* samples)
{
    bool difference = false;
    
    if (samples == NULL) {
        // diff if we have anything
        if (mPlayerList != NULL)
          difference = true;
    }
    else {
        int srcCount = 0;
        for (Sample* s = samples->getSamples() ; s != NULL ; s = s->getNext())
          srcCount++;

        int curCount = 0;
        for (SamplePlayer* p = mPlayerList ; p != NULL ; p = p->getNext())
          curCount++;

        if (srcCount != curCount) {
            // doesn't matter what changed
            difference = true;
        }
        else {
            Sample* s = samples->getSamples();
            SamplePlayer* p = mPlayerList;
            while (s != NULL && !difference) {
                // note that we're comparing against the relative path
                // not the absolute path we built in the SamplePlayer
                // constructor 
                if (!StringEqual(s->getFilename(), p->getFilename())) {
                    difference = true;
                }
                else {
                    s = s->getNext();
                    p = p->getNext();
                }
            }
        }
    }

    return difference;
}

/**
 * This MUST be called from within the audio interrupt handler.
 */
void SampleTrack::setSamples(SamplePack* pack)
{
	delete mPlayerList;
	mPlayerList = pack->stealSamples();
    delete pack;

	mSampleCount = 0;
	mLastSample = -1;

	if (mPlayerList != NULL) {
		// index them for easier access
		for (SamplePlayer* sp = mPlayerList ; 
			 sp != NULL && mSampleCount < MAX_SAMPLES ;
			 sp = sp->getNext()) {
			mPlayers[mSampleCount++] = sp;
		}

		for (int i = mSampleCount ; i < MAX_SAMPLES ; i++)
		  mPlayers[i] = NULL;
	}
}

int SampleTrack::getSampleCount()
{
    int count = 0;
    for (SamplePlayer* p = mPlayerList ; p != NULL ; p = p->getNext())
      count++;
    return count;
}

/**
 * Called whenever a new MobiusConfig is installed in the interrupt
 * handler.  Check for changes in latency overrides.
 * Note that we have to go to Mobius rather than look in the MobiusConfig
 * since the config is often zero so we default to what the device tells us.
 * !! This will end up using the "master" MobiusConfig rather than
 * mInterruptConfig.  For this it shouldn't matter but it still feels
 * unclean.  
 */
void SampleTrack::updateConfiguration(MobiusConfig* config)
{
    // config is ignored since we're only interested in latencies right now
	for (int i = 0 ; i < mSampleCount ; i++)
	  mPlayers[i]->updateConfiguration(mMobius->getEffectiveInputLatency(),
									   mMobius->getEffectiveOutputLatency());
}

/**
 * Trigger a sample to begin playing.
 *
 * !!! This feels full of race conditions.  The unit tests do this
 * in scripts so often we will be inside the interrupt.  But the
 * SampleTrigger function is declared as global so if triggered
 * from MIDI it will be run outside the interrupt.
 * 
 * If we are being run during the script pass at the start of the interrupt,
 * then the sample will be immediately played into the input/output buffers in
 * the processBufffers interrupt handler below.  This is normally done
 * for testing with scripts using the "Wait block" statement to ensure
 * that the sample is aligned with an interrupt block.
 *
 * For scripts triggered with MIDI, keys, or buttons, we will trigger them
 * but won't actually begin playing until the next interrupt when 
 * processBuffers is run again.  This means that for predictable content
 * in the unit tests you must use "Wait block" 
 *
 * KLUDGE: Originally triggering always happened during processing of a Track
 * after we had called the SamplerTrack interrupt handler.  So we could begin
 * hearing the sample in the current block we begin proactively playing it here
 * rather than waiting for the next block.  This is arguably incorrect, we should
 * just queue it up and wait for the next interrupt.  But unfortunately we have
 * a lot of captured unit tests that rely on this.  Until we're prepared
 * to recapture the test files, follow the old behavior.  But note that we have
 * to be careful *not* to do this twice in one interupt.  Now that scripts
 * are called before any tracks are processed, we may not need to begin playing, 
 * but wait for SampleTrack::processBuffers below.
 */

void SampleTrack::trigger(AudioStream* stream, int index, bool down)
{
	if (index < mSampleCount) {
		mPlayers[index]->trigger(down);
		mLastSample = index;

		// KLUDGE: With the original script implementation, we would
		// begin playing the sample immediately if we were still in the
		// interrupt handler, after the SampleTrack buffers were processed.
		
		// test hack, if we're still in an interrupt, process it now
		if (mTrackProcessed && stream != NULL) {
			long frames = stream->getInterruptFrames();
			float* inbuf;
			float* outbuf;

			// always port 0, any need to change?
			stream->getInterruptBuffers(0, &inbuf, 0, &outbuf);

			mPlayers[index]->play(inbuf, outbuf, frames);

			// only the initial trigger needs to notify the other tracks,
			// after ward we're the first one so we've modified it before
			// the others start copying
			mRecorder->inputBufferModified(this, inbuf);
		}

	}
	else {
		// this is sometimes caused by a misconfiguration of the
		// the unit tests
		Trace(1, "ERROR: No sample at index %ld\n", (long)index);
	}

}

long SampleTrack::getLastSampleFrames()
{
	long frames = 0;
	if (mLastSample >= 0)
	  frames = mPlayers[mLastSample]->getFrames();
	return frames;
}

//////////////////////////////////////////////////////////////////////
//
// Interrupt Handler
//
//////////////////////////////////////////////////////////////////////

void SampleTrack::prepareForInterrupt()
{
	// kludge see comments in trigger()
	mTrackProcessed = false;
}


void SampleTrack::processBuffers(AudioStream* stream, 
								 float* inbuf, float *outbuf, long frames, 
								 long frameOffset)
{
	for (int i = 0 ; i < mSampleCount ; i++)
	  mPlayers[i]->play(inbuf, outbuf, frames);

	mTrackProcessed = true;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
