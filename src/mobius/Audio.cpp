/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Memory model for digital audio, segmented into blocks.
 * This is relatively general try to avoid Mobius-specific dependencies
 * so we can use it elsewhere.  The big dependency now is ObjectPool.
 *
 */

#include <stdio.h>
#include <memory.h>

#include "Trace.h"
#include "util.h"
#include "Thread.h"
#include "WaveFile.h"

// getting CD_SAMPLE_RATE and AUDIO_MAX_CHANNELS from here
// !! this all needs to be redesigned to 1) allow flexible
// channels and 2) not allocate damn stack buffers
#include "AudioInterface.h"

#include "Audio.h"
#include "ObjectPool.h"

/****************************************************************************
 *                                                                          *
 *   							  CONSTANTS                                 *
 *                                                                          *
 ****************************************************************************/

/**
 * The number of Audio frames per buffer.
 * Actual size of the buffer will depend on the number of channels,
 * which will usually be 2.
 * Probably will want to make this variable and tune it to reduce
 * fragmentation.
 */
#define FRAMES_PER_BUFFER  1024 * 64

/**
 * Number of channels in a buffer.
 */
#define BUFFER_CHANNELS 2

/**
 * The size of one buffer.  In theory the AudioPool should
 * be able to decide this but let's keep it simple and assume
 * a size with stereo channels;
 */
#define BUFFER_SIZE (FRAMES_PER_BUFFER * BUFFER_CHANNELS)

/****************************************************************************
 *                                                                          *
 *   							  UTILITIES                                 *
 *                                                                          *
 ****************************************************************************/

// See portaudio's pa_convert.c for more

PUBLIC short SampleFloatToInt16(float sample)
{
	return (short) (sample * (32767.0f));
}

/****************************************************************************
 *                                                                          *
 *   								AUDIO                                   *
 *                                                                          *
 ****************************************************************************/

/**
 * A global that defines the default file format for writing audio files.
 * This may be shared by multiple plugins.
 */
int Audio::WriteFormat = WAV_FORMAT_IEEE;

/**
 * May be used to set the default setting for writting the file.
 * Must be one of the format constants defined in WaveFile.h.
 */
PUBLIC void Audio::setWriteFormat(int fmt)
{
	if (fmt == WAV_FORMAT_IEEE || fmt == WAV_FORMAT_PCM)
	  WriteFormat = fmt;
}

/**
 * More conveinent interface since we only support two values now.
 */
PUBLIC void Audio::setWriteFormatPCM(bool pcm)
{
	if (pcm)
	  WriteFormat = WAV_FORMAT_PCM;
	else
	  WriteFormat = WAV_FORMAT_IEEE;
}

PUBLIC Audio::Audio() 
{
    // this must be rare and only for debugging
    Trace(1, "Audio::Audio creating unpooled Audio!\n");
    init();
}

PUBLIC Audio::Audio(AudioPool* pool) 
{
	init();
    mPool = pool;
}


PUBLIC Audio::Audio(AudioPool* pool, const char *filename) 
{
	init();
    mPool = pool;
	read(filename);
}

void Audio::init() 
{
    mPool = NULL;
	mSampleRate = CD_SAMPLE_RATE;
	mChannels = BUFFER_CHANNELS;
    mBufferSize = BUFFER_SIZE;

	mVersion = 0;
	mBuffers = NULL;
	mBufferCount = 0;
	mStartFrame = 0;
	mFrames = 0;

	mPlay = new AudioCursor("Play", this);
	mRecord = new AudioCursor("Record", this);
	mRecord->setAutoExtend(true);
}

PUBLIC Audio::~Audio() 
{
	freeBuffers();
	delete mBuffers;
	delete mPlay;
	delete mRecord;
}

PUBLIC AudioPool* Audio::getPool()
{
    return mPool;
}

PUBLIC void Audio::free()
{
    // these aren't actually pooled, just free the buffers
    delete this;
}

PUBLIC int Audio::getSampleRate() 
{
	return mSampleRate;
}

PUBLIC void Audio::setSampleRate(int i) 
{
	mSampleRate = i;
}

PUBLIC int Audio::getChannels() 
{
	return mChannels;
}
	
PUBLIC void Audio::setChannels(int i) 
{
    // this effects how we structure the buffers so must to this
    // before adding content
	freeBuffers();

    // need more work if we have to support this
    if (i > 0 && i != 2)
      Trace(1, "Ignoring attempt to set audio channels to %ld\n", (long)i);
}

/**
 * Return true if the audio is logically empty.  It may have
 * a size, but if it is full of silence, it is empty.
 */
PUBLIC bool Audio::isEmpty()
{
	bool empty = true;
	for (int i = 0 ; i < mBufferCount && empty ; i++) {
		if (mBuffers[i] != NULL)
		  empty = false;
	}
	return empty;
}

/****************************************************************************
 *                                                                          *
 *   							   BUFFERS                                  *
 *                                                                          *
 ****************************************************************************/

/**
 * Formerly tried to keep allocated buffers, but now we just pool them.
 */
PUBLIC void Audio::reset() 
{
	freeBuffers();
	initIndex();
}

/**
 * Set all samples to zero, but retain the frame counter.
 * Don't have to actually zero the buffers, just release them.
 */
PUBLIC void Audio::zero() 
{
	for (int i = 0 ; i < mBufferCount ; i++) {
		freeBuffer(mBuffers[i]);
		mBuffers[i] = NULL;
	}
	mVersion++;

	// we could set mStartFrame back to zero now??
}

/**
 * Determine the buffer index and offset of a given logical frame.
 */
void Audio::locate(long frame, int* buffer, int* offset)
{
    long absoluteFrame = frame + mStartFrame;
	long sample = absoluteFrame * mChannels;

	*buffer = (sample / mBufferSize);
	*offset = sample % mBufferSize;
}

void Audio::locateStart(int* buffer, int* offset)
{
	locate(0, buffer, offset);
}

/**
 * Initialize the buffer index array.
 * At 64K frames per buffer, there is about 1.4 seconds per buffer.
 * To prevent excessive growth, allocate an index big enough for
 * about a minute of audio then grow it in chunks.
 */
void Audio::initIndex()
{
	if (mBuffers == NULL) {

		mBufferCount = 60;			// configurable?
		mBuffers = new float*[mBufferCount];
		for (int i = 0 ; i < mBufferCount ; i++)
		  mBuffers[i] = NULL;

		// We'll normally record forward but if we reverse then
		// we can start pushing new buffers on the front.  Though 
		// not really necessary, set the starting buffer higher
		// than zero so we can do a short reverse without reallocating
		// the index and test the other calculations.

		long framesPerBuffer = mBufferSize / mChannels;
		mStartFrame = framesPerBuffer * 10;
		mVersion++;
	}
}

/**
 * Release all buffer memory.
 * Keep the index to avoid heap churn.  The main thing is that
 * we return the buffers to the pool.
 */
void Audio::freeBuffers() 
{
	if (mBuffers != NULL) {
		for (int i = 0 ; i < mBufferCount ; i++) {
			freeBuffer(mBuffers[i]);
			mBuffers[i] = NULL;
		}
	}
	mStartFrame = 0;
    mFrames = 0;
	mVersion++;
}

/**
 * Increase the size of the the index in the given direction.
 * Here "up" means to extend the index on the "left" (in reverse)
 * and "down" is a normal forward extension.
 */
void Audio::growIndex(int count, bool up)
{
	if (count > 0) {
		float **buffers;
		int i, newcount;

		newcount = mBufferCount + count;
		buffers  = new float*[newcount];

		if (up) {
			for (i = 0 ; i < mBufferCount ; i++)
			  buffers[i+count] = mBuffers[i];
	
			for (i = 0 ; i < count ; i++)
			  buffers[i] = NULL;
		}
		else {
			for (i = 0 ; i < mBufferCount ; i++)
			  buffers[i] = mBuffers[i];
	
			for (i = mBufferCount ; i < newcount ; i++)
			  buffers[i] = NULL;
		}

		mBufferCount = newcount;
		delete mBuffers;
		mBuffers = buffers;

		// when growing up, the current content range must also be adjusted
		if (up)
		  mStartFrame += (count * (mBufferSize / mChannels));

		mVersion++;
	}
}

/**
 * Make the index large enough to hold a potential buffer.
 * We may grow larger than requested.
 */
void Audio::prepareIndex(int index) 
{
	if (index >= mBufferCount) {
		// always add a few extra
		int count = (index - mBufferCount + 1) + 10;
		growIndex(count, false);
	}
}

/**
 * Make the index large enough to hold a particular frame.
 * While we don't allocate buffers until necessary, we do depend on 
 * having the index be large enough.
 * ?? Really, why?
 */
void Audio::prepareIndexFrame(long frame)
{
	int buffer;
	int offset;
	locate(frame, &buffer, &offset);
	prepareIndex(buffer);
}

/**
 * Return the buffer at a given index, or null if there
 * is no buffer at this index.
 */
float* Audio::getBuffer(int i) 
{
	return ((i >= 0 && i < mBufferCount) ? mBuffers[i] : NULL);
}

/**
 * Return the buffer at a given index, allocating one if necessary.
 */
float *Audio::allocBuffer(int index) 
{
	float* buffer;

	prepareIndex(index);
	buffer = mBuffers[index];
	if (buffer == NULL) {
		buffer = allocBuffer();
		mBuffers[index] = buffer;
		mVersion++;
	}

	return buffer;
}

/**
 * Add a buffer at the specified index. 
 * Used only in the implementation of file reading.
 */
void Audio::addBuffer(float* buffer, int index)
{
	prepareIndex(index);
	float* existing = mBuffers[index];
	if (existing != NULL) {
		// ordinarily not supposed to be replacing buffers, but allow it
		Trace(1, "Audio::addBuffer replacing existing buffer!\n");
        freeBuffer(existing);
	}
	mBuffers[index] = buffer;
	mVersion++;
}

/**
 * Allocate one buffer.
 */
float* Audio::allocBuffer() 
{
    float* buffer = NULL;

    if (mPool != NULL) {
        // Sincwe Audio and AudioPool share the same buffer size
        // we dont' have to pass it down.
        buffer = mPool->newBuffer();
    }
    else {
        // In theory we could just allocate them on the fly
        // but I want these to always be used with a pool.  Convenient
        // for a handful of debug traces so allow with a warning.
        Trace(1, "Audio::allocBuffer no pool!\n");
        int bytesize = (BUFFER_SIZE * sizeof(float));
        buffer = (float*)new char[bytesize];
		memset(buffer, 0, BUFFER_SIZE * sizeof(float));
    }

	return buffer;
}

/**
 * Release one buffer.
 */
void Audio::freeBuffer(float* buffer)
{
    if (mPool != NULL)
      mPool->freeBuffer(buffer);
    else {
        // shouldn't be happening
        Trace(1, "Audio::freeBuffer with no pool!\n");
        delete buffer;
    }
}

/****************************************************************************
 *                                                                          *
 *   							 FRAME RANGES                               *
 *                                                                          *
 ****************************************************************************/
/*
 * Most of the fun in concentrated here.
 * These are the most subtle and crucial of the Audio/AudioCursor 
 * calculations, be extremely careful here or mayhem ensues.
 */

/**
 * Return the logical number of frames in this audio.
 * There may not actually be this many buffers allocated.
 */
PUBLIC long Audio::getFrames() 
{
	return mFrames;
}

/**
 * Return the logical number of samples, calculated as number
 * of frames times channels.
 */
PUBLIC long Audio::getSamples()
{
	return mFrames * mChannels;
}

/**
 * Logically splice out a section of frames.
 */
PUBLIC void Audio::splice(long frame, long length)
{
	setStartFrame(mStartFrame + frame);
	setFrames(length);
}

/**
 * Set the number of valid frames.
 * 
 * This does not grow the index or allocate buffers, it it simply
 * a logical frame count.  The "content" of the Audio in a region
 * where there is no buffer is returned as zero samples by the
 * AudioCursor.
 *
 * If the new frame count is less than the old one, we must
 * zero out any partially filled buffers and release any buffers
 * that are now completely unused.  This results in better buffer
 * allocation, and makes the job of prepareFrame easier.
 */
PUBLIC void Audio::setFrames(long frames) 
{
	// is this what we want?
	if (frames < 0) {
		Trace(1, "Audio::setFrames negative, collapsing to zero\n");
		frames = 0;
	}

	if (frames >= 0) {
		if (frames < mFrames) {
			// have to reclaim and/or initialize the old space
			int index, offset;
			locate(frames, &index, &offset);
			if (index < mBufferCount) {

				// partially clear the new last buffer
				float* buffer = mBuffers[index];
				if (buffer != NULL) {
					// may be more than we need if we're in the same
					// buffer as the current last frame, but this shouldn't
					// happen very often
					int bytes = (mBufferSize - offset) * sizeof(float);
					memset(&buffer[offset], 0, bytes);
				}

				// then release any remaining buffers
				// actually mFrames - 1 should be enough but I'm worried about
				// boundary conditions
				int lastIndex;
				locate(mFrames, &lastIndex, &offset);
				if (lastIndex >= mBufferCount)
				  lastIndex = mBufferCount - 1;

				for (int i = index + 1 ; i <= lastIndex ; i++) {
					freeBuffer(mBuffers[i]);
					mBuffers[i] = NULL;
					mVersion++;
				}
			}
		}

		mFrames = frames;
    }
}

/**
 * Set the number of frames in the layer, when recording in reverse.
 * This is a subtlety used only by Layer during reverse recording.
 * See commentary at the top of this file.
 */
PUBLIC void Audio::setFramesReverse(long frames)
{
	long extension = frames - mFrames;

	// extension will normally be positive, but may also be negative
	// for an unrounded multiply in reverse
	long newStartFrame = mStartFrame - extension;

	setStartFrame(newStartFrame);
	
	mFrames = frames;
}

/**
 * Set the logical start frame.  This is not relative to the current
 * start frame, it is an absolute frame offset from the first buffer
 * in the index.
 * 
 * The index will always be large enough to hold the start frame, though
 * not necessarily the end frame. (Does it need to)?
 * 
 * If the new start frame is greater than the current start frame, then
 * we are truncating on the left and must zero out any partially filled
 * buffers and release any buffers that are now completely unused. If
 * the new start frame is beyond the current end frame, the loop logically
 * collapses to zero length, and the new start frame remains at the former
 * end frame.
 *
 * If the new start frame is less than the current start frame, we are
 * increasing the logical size by extending on the left.  If the new start
 * frame is positive we simply set it and let the buffer be allocated
 * in prepareFrame.  
 *
 * If the new start frame is negative, it means we need to extend
 * the index on the left.  The index is extended, the current contents
 * are logically shifted, and the resulting start frame will be
 * zero or positive.
 *
 */
void Audio::setStartFrame(long frame)
{
	if (frame >= 0) {
		if (frame <= mStartFrame) {
			// extension on the left within the current index range
			// just set the frame and bump the size
			mFrames += mStartFrame - frame;
			mStartFrame = frame;
		}
		else {
			// truncation on the left

			long endFrame = mStartFrame + mFrames - 1;
			if (frame > endFrame) {
				Trace(2, "Audio:setStartFrame collapsing to zero\n");
				// hmm, has to be one greater to zero properly,
				// I don't like this because it could cause us to 	
				// grow without bounds if something funny happens,
				// when we get to this state it doesn't matter where
				// the start frame actually is since we have a zero
				// length loop, should just reset the index!!
				frame = endFrame + 1;
			}

			long relframe = frame - mStartFrame;
			int index, offset;
			locate(relframe, &index, &offset);

			if (index < mBufferCount) {
				// partially clear the new first buffer
				float* buffer = mBuffers[index];
				if (buffer != NULL) {
					// may be more than we need if we're in the same
					// buffer as the current start frame, but this shouldn't
					// happen often enough to be worth optimizing?
					int bytes = offset * sizeof(float);
					memset(buffer, 0, bytes);
				}
			
				// then release any remaining buffers
				int firstIndex;
				locate(0, &firstIndex, &offset);

				int lastIndex = index - 1;
				if (lastIndex >= mBufferCount)
				  lastIndex = mBufferCount - 1;

				for (int i = firstIndex ; i <= lastIndex ; i++) {
					freeBuffer(mBuffers[i]);
					mBuffers[i] = NULL;
					mVersion++;
				}
			}
			
			mStartFrame = frame;
			mFrames -= relframe;
			if (mFrames < 0)
			  mFrames = 0;
		}
	}
	else {
		// index extension on the left, unlike extending the end frame
		// on the right, we have to keep reallocating the index so 
		// we can maintain a positive start frame.

		// determine the number of buffers we need to contain
		// the desired number of frames
		long needFrames = -frame;
		long needSamples = needFrames * mChannels;
		int needBuffers = needSamples / mBufferSize;
		if ((needSamples % mBufferSize) > 0)
		  needBuffers++;

		// since it looks like we're in reverse, add a few extra so we
		// don't have to grow one buffer at a time
		needBuffers += 10;

		// extend the index up by this amount
		// this will increase mStartFrame by the corresponding amount
		long origStartFrame = mStartFrame;
		growIndex(needBuffers, true);

		// this logically decrements to zero, then offsets relative
		// to the extra room in the buffer
		// !! check this carefully
		mStartFrame = mStartFrame - origStartFrame - needFrames;
		mFrames += needFrames;

		Trace(2, "Audio::added %ld buffers\n", (long)needBuffers);
	}
}

/**
 * Prepare a frame for recording.  The frame here is relative to
 * mStartFrame.  Extend the index if the frame is beyond the current range.
 * 
 * Called by AudioCursor when it needs to record a frame but doesn't
 * have enough context of its own yet.
 *
 * THINK: In the situation where we're doing occasional overdubs
 * into a sparse Audio, we could end up with a frame count that includes
 * the end of the last overdub, but not the entire surrounding loop.
 * This could lead to playback synchronization problems?  Maybe not
 * because overflowing will just result in zero playback, also the
 * loop is only dynamically extended on the initial record, after which
 * we presize it. No! in the case of extending on the left, when we return
 * to forward play, it won't start from the right position.
 */
long Audio::prepareFrame(long frame, int* retIndex, 
                         int* retOffset, float** retBuffer)
{
	int index = 0;
	int offset = 0;
	float* buffer = NULL;

	if (frame >= 0) {
		// normal forward positioning

		locate(frame, &index, &offset);
		buffer = allocBuffer(index);

		// extend if this is beyond the current end frame
		if (frame >= mFrames)
		  mFrames = frame + 1;
	}
	else {
		// setStartFrame does the heavy lifting
		setStartFrame(mStartFrame + frame);

		// the resulting relative frame is always zero
		frame = 0;

		locate(frame, &index, &offset);
		buffer = allocBuffer(index);
	}

	*retIndex = index;
	*retOffset = offset;
	*retBuffer = buffer;

	return frame;
}

/****************************************************************************
 *                                                                          *
 *   								FILES                                   *
 *                                                                          *
 ****************************************************************************/

/**
 * Load a wave file.
 * Formerly used libsndfile, now have WaveFile.
 * Only supporting 16 bit PCM or IEEE single float, 2 channel, 44.1Khz.
 * 
 * If we were smarter we could read directly into buffers rather than
 * one large one then splitting it up.  In this context, I like
 * libsndfile's interface better we can read in the chunk size we want.
 */
PUBLIC int Audio::read(const char *name) 
{
	int error = 0;

	WaveFile* wav = new WaveFile();
	error = wav->read(name);
	if (error) {
		Trace(1, "Error reading file %s %s\n", name, 
			  wav->getErrorMessage(error));
	}
	else {
		AudioBuffer b;

		reset();
		mSampleRate = wav->getSampleRate();
        // ignore channels until we can support variable buffer size
        int channels = wav->getChannels();
        if (channels != 0 && channels != 2)
          Trace(1, "Ignoring channel count in file: %ld\n", (long)channels);

		initIndex();

		// !! Another disadvantage with this interface relative to 
        // libsndfile is that we can't call isEmpty to detect blocks of
        // zero and skip them

		b.buffer = wav->getData();
		b.frames = wav->getFrames();
		b.channels = mChannels;
		append(&b);

	}
	delete wav;

	return error;
}

/**
 * Helper for read().  Return true if this buffer contains
 * no non-zero frames.  Used to create sparse Audio objects.
 */
bool Audio::isEmpty(float* buffer)
{
	bool empty = true;
	for (int i = 0 ; i < mBufferSize ; i++) {
		if (buffer[i] != 0.0f) {
			empty = false;
			break;
		}
	}
	return empty;
}

/**
 * Static debug utility to quickly write a buffer of frames to a file.
 * Should only be used for debugging.
 */
void Audio::write(const char* name, float* buffer, long frames)
{
	Audio* a = new Audio();
	AudioBuffer b;
	b.buffer = buffer;
	b.frames = frames;
	b.channels = 2;
	a->append(&b);
	a->write(name);
	delete a;
}


int Audio::write(const char *name) 
{
	return write(name, WriteFormat);
}

int Audio::write(const char *name, int format) 
{
	int error = 0;

	WaveFile* wav = new WaveFile();
	wav->setChannels(mChannels);
	wav->setFrames(mFrames);
	wav->setFormat(format);
	wav->setFile(name);

	error = wav->writeStart();
	if (error) {
		Trace(1, "Error writing file %s: %s\n", name, 
			  wav->getErrorMessage(error));
	}
	else {
		// write one frame at a time not terribly effecient but messing
		// with blocking at this level isn't going to save much
		AudioBuffer b;
		float buffer[AUDIO_MAX_CHANNELS];
		b.buffer = buffer;
		b.frames = 1;
		b.channels = mChannels;

		for (long i = 0 ; i < mFrames ; i++) {
			memset(buffer, 0, sizeof(buffer));
			get(&b, i);
			wav->write(buffer, 1);
		}

		error = wav->writeFinish();
		if (error) {
			Trace(1, "Error finishing file %s: %s\n", name, 
				  wav->getErrorMessage(error));
		}
	}

	delete wav;
	return error;
}

/****************************************************************************
 *                                                                          *
 *                                    COPY                                  *
 *                                                                          *
 ****************************************************************************/
/*
 * Copy methods should no longer be used by the interrupt handler
 *  classes, but may still be usefull elsewhere?
 *
 */

/**
 * Copy the contents of one Audio into another.
 * Note that this assumes the buffer doesn't have a lot of wasted
 * space at the front or back, could check that and compress.
 */
void Audio::copy(Audio* src)
{
	copy(src, 127);
}

void Audio::copy(Audio* src, int feedback)
{
	reset();
	if (src != NULL) {
		// assume we're all using the same buffer size
		if (src->mBufferSize != mBufferSize)
		  Trace(1, "Mismatched Audio buffer size!\n");
		else {
			int srcmax = src->mBufferCount;
			for (int i = 0 ; i < srcmax ; i++) {
				float* srcb = src->getBuffer(i);
				if (srcb != NULL) {
					// !! todo: if the buffer is empty don't bother allocating

					float* destb = allocBuffer(i);

					memcpy(destb, srcb, mBufferSize * sizeof(float));
					applyFeedback(destb, feedback);
				}
			}
		}

        mStartFrame = src->mStartFrame;
		setFrames(src->mFrames);
	}
}

void Audio::applyFeedback(float* buffer, int feedback)
{
	if (feedback < 127 && feedback >= 0) {
		// old way, linear
		// float modifier = (float)feedback / 127.0f;
		// new way, pseudo-log
		float* ramp = AudioFade::getRamp128();
		float modifier = ramp[feedback];

		for (int i = 0 ; i < mBufferSize ; i++)
		  buffer[i] = buffer[i] * modifier;
	}
}

/****************************************************************************
 *                                                                          *
 *   							 DIAGNOSTICS                                *
 *                                                                          *
 ****************************************************************************/

void Audio::dump() 
{
	printf("Audio\n");
	printf("Sample rate %d, Channels %d, Frames %ld StartFrame %ld\n",
		   mSampleRate, mChannels, mFrames, mStartFrame);

	int allocated = 0;
	if (mBuffers != NULL) {
		for (int i = 0 ; i < mBufferCount ; i++) {
			if (mBuffers[i] != NULL)
			  allocated++;
		}
	}

	printf("Buffer size %d, Buffers reserved %d Buffers allocated %d\n",
		   mBufferSize, mBufferCount, allocated);

	fflush(stdout);
}

void Audio::dump(TraceBuffer* b) 
{
	int allocated = 0;
	if (mBuffers != NULL) {
		for (int i = 0 ; i < mBufferCount ; i++) {
			if (mBuffers[i] != NULL)
			  allocated++;
		}
	}

	b->add("Audio: start %ld length %ld index %d, buffers %d\n",
		   mStartFrame, mFrames, mBufferCount, allocated);
}

/**
 * Check for differences between two Audios.
 * Makes use of the internal play cursor.
 */
PUBLIC void Audio::diff(Audio* a)
{
	if (mFrames != a->getFrames()) {
		printf("Frame counts differ this=%ld other=%ld\n",
			   mFrames, a->getFrames());
	}
	else if (mChannels != a->getChannels()) {
		printf("Channel counts differ this=%d other=%d\n",
			   (int)mChannels, (int)a->getChannels());
	}
	else {
		AudioBuffer b1;
		float f1[AUDIO_MAX_CHANNELS];
		b1.buffer = f1;
		b1.frames = 1;
		b1.channels = mChannels;

		AudioBuffer b2;
		float f2[AUDIO_MAX_CHANNELS];
		b2.buffer = f2;
		b2.frames = 1;
		b2.channels = mChannels;

		bool stop = false;
		for (int i = 0 ; i < mFrames && !stop ; i++) {

			get(&b1, i);
			a->get(&b2, i);
			
			for (int j = 0 ; j < mChannels && !stop ; j++) {
				if (f1[j] != f2[j]) {
					printf("Difference at frame %d\n", i);
					stop = true;
				}
			}
		}
	}
}

/****************************************************************************/
/****************************************************************************
 *                                                                          *
 *   						  INTERNAL CURSORRS                             *
 *                                                                          *
 ****************************************************************************/
/****************************************************************************/

PUBLIC long Audio::getPlayFrame()
{
	return mPlay->getFrame();
}

PUBLIC void Audio::rewind()
{
	mPlay->setFrame(0);
	mRecord->setFrame(0);
}

/**
 * Return a range of frames.
 */
PUBLIC void Audio::get(AudioBuffer* buf, long frame)
{
	mPlay->setFrame(frame);
    mPlay->get(buf, 1.0);
}

PUBLIC void Audio::get(float* dest, long frames, long frame)
{
	AudioBuffer b;
	b.buffer = dest;
	b.frames = frames;
	b.channels = 2;
	get(&b, frame);
}

void Audio::put(AudioBuffer* buf, long frame)
{
	// this one gets to auto extend
	mRecord->setFrame(frame);
	mRecord->put(buf, OpAdd);
}

void Audio::put(float* src, long frames, long frame)
{
	AudioBuffer b;
	b.buffer = src;
	b.frames = frames;
	b.channels = 2;
	put(&b, frame);
}

void Audio::put(Audio* src, long frame)
{
	if (src->getChannels() == mChannels &&  src->getFrames() > 0) {

		mRecord->setFrame(frame);

		// transfer one rame at a time
		AudioBuffer b;
		float buffer[AUDIO_MAX_CHANNELS];
		b.buffer = buffer;
		b.frames = 1;
		b.channels = src->getChannels();

		long srcFrame = 0;
		long srcFrames = src->getFrames();

		while (srcFrame < srcFrames) {
			memset(buffer, 0, sizeof(buffer));
			src->get(&b, srcFrame);
			mRecord->put(&b, OpAdd);
			srcFrame++;
		}
	}
}

void Audio::append(AudioBuffer* buf)
{
	long frames = buf->frames;

	if (frames > 0) {
		if (buf->buffer == NULL) {
			// special "append silence" option
			// put should probably support this too
			setFrames(mFrames + frames);
		}
		else {
			mRecord->setFrame(mFrames);
			mRecord->put(buf, OpAdd);
		}
	}
}

void Audio::append(float* src, long frames)
{
	AudioBuffer b;
	b.buffer = src;
	b.frames = frames;
	b.channels = 2;
	append(&b);
}

void Audio::append(Audio* src)
{
	if (src != NULL && src->getFrames() > 0)
	  put(src, mFrames);
}

/**
 * Fade the edges of a raw recording.
 */
void Audio::fadeEdges()
{
	mRecord->fadeIn();
	mRecord->fadeOut();
}

/****************************************************************************
 *                                                                          *
 *   							   OBSOLETE                                 *
 *                                                                          *
 ****************************************************************************/

/**
 * Old implementation of insert, now performed at the Layer level
 * with Segments.  Keep around for awhile as an example.
 *
 * Need to figure out a way to optmize this without wholesale shifting.
 * Now that we can push buffers, could figure out if we're closer to the
 * front and push rather than append.  Would be nice if we could
 * have variable length buffers but that complicates indexing.
 * With the introduction of AudioCursor, variable length buffers become much
 * more feasable, though still harder to determine the initial location.
 *
 * This takes an AudioContext like the others, but we only support
 * insertion of other Audio objects, not individual float buffers.
 */
void Audio::insert(Audio* audio, long insertFrame)
{
    if (audio != NULL && audio->getSamples() > 0) {
        if (insertFrame >= mFrames) {
            // just an append
            append(audio);
        }
        else {
            // first shift everything down
            long lastFrame = mFrames - 1;
            long newFrames = audio->getFrames();
            int srcBuffer;
            int srcSample;
            int destBuffer;
            int destSample;

            locate(lastFrame, &srcBuffer, &srcSample);
            locate(lastFrame + newFrames, &destBuffer, &destSample);

			// Note that the sample offsets point to the first
			// sample in the frame, have to advance to the last sample
			srcSample += (mChannels - 1);
			destSample += (mChannels - 1);

			int shiftSamples = (mFrames - insertFrame) * mChannels;
            float* src = mBuffers[srcBuffer];

            // todo: could try to be smart about sparse copying
            // a buffer that happens to be empty but it's hard
            float* dest = allocBuffer(destBuffer);

            for (int i = 0 ; i < shiftSamples ; i++) {
                // allow copying from a sparse buffer
                float sample = 0.0;
                if (src != NULL)
                    sample = src[srcSample];
                dest[destSample] = sample;
                destSample--;
                if (destSample < 0) {
                    destBuffer--;
                    // don't assume this exists
                    dest = allocBuffer(destBuffer);
                    destSample = mBufferSize - 1;
                }
                srcSample--;
                if (srcSample < 0) {
                    srcBuffer--;
                    src = mBuffers[srcBuffer];
                    srcSample = mBufferSize - 1;
                }
            }

            setFrames(mFrames + newFrames);
			mVersion++;

            // Now replace the opened area
			put(audio, insertFrame);
        }
    }
}

/****************************************************************************/
/****************************************************************************
 *                                                                          *
 *                                    POOL                                  *
 *                                                                          *
 ****************************************************************************/
/****************************************************************************/

/**
 * Create an initially empty audio pool.
 * There is normally only one of these in a Mobius instance.
 * Should have another list for all buffers outstanding?
 */
PUBLIC AudioPool::AudioPool()
{
    mCsect = new CriticalSection("AudioPool");
    mPool = NULL;
    mAllocated = 0;
    mInUse = 0;

    // needs more testing
    // !! channels
    //mNewPool = new SampleBufferPool(FRAMES_PER_BUFFER * 2);
    mNewPool = NULL;
}

/**
 * Release the kracken.
 */
PUBLIC AudioPool::~AudioPool()
{
    delete mCsect;
    delete mNewPool;

    OldPooledBuffer* next = NULL;
    for (OldPooledBuffer* p = mPool ; p != NULL ; p = next) {
        next = p->next;
        delete p;
    }
}

/**
 * Allocate a new Audio in this pool.
 * We could pool the outer Audio object too, but the buffers are
 * the most important.
 */
PUBLIC Audio* AudioPool::newAudio()
{
    return new Audio(this);
}

/**
 * Allocate a new Audio in this pool and read a file.
 */
PUBLIC Audio* AudioPool::newAudio(const char* file)
{
    return new Audio(this, file);
}

/**
 * Return an Audio to the pool.
 * These aren't actually pooled, just free the buffers which
 * will happen in the destructor.
 */
PUBLIC void AudioPool::freeAudio(Audio* a)
{
    a->free();
}

/**
 * Allocate a new buffer, using the pool if available.
 * In theory have to have a different pool for each size, assume only
 * one for now.
 * !! channels
 */
PUBLIC float* AudioPool::newBuffer()
{
	float* buffer;

	if (mNewPool) {
		buffer = mNewPool->allocSamples();
	}
	else {
		mCsect->enter();
		if (mPool == NULL) {
			int bytesize = sizeof(OldPooledBuffer) + (BUFFER_SIZE * sizeof(float));
			char* bytes = new char[bytesize];
			OldPooledBuffer* pb = (OldPooledBuffer*)bytes;
			pb->next = NULL;
			pb->pooled = 0;
			buffer = (float*)(bytes + sizeof(OldPooledBuffer));
            mAllocated++;
		}
		else {
			buffer = (float*)(((char*)mPool) + sizeof(OldPooledBuffer));
			if (!mPool->pooled)
			  Trace(1, "Audio buffer in pool not marked as pooled!\n");
			mPool->pooled = 0;
			mPool = mPool->next;
		}
		mInUse++;
		mCsect->leave();

		// in both cases, make sure its empty
		// !! these are big, need to keep the list clean and do it
		// in a worker thread
		memset(buffer, 0, BUFFER_SIZE * sizeof(float));
	}

	return buffer;
}

/**
 * Return a buffer to the pool.
 */
PUBLIC void AudioPool::freeBuffer(float* buffer)
{
	if (buffer != NULL) {

		if (mNewPool != NULL) {
			mNewPool->freeSamples(buffer);
		}
		else {
			OldPooledBuffer* pb = (OldPooledBuffer*)
				(((char*)buffer) - sizeof(OldPooledBuffer));

			if (pb->pooled)
			  Trace(1, "Audio buffer already in pool!\n");
			else {
				mCsect->enter();
				pb->next = mPool;
				pb->pooled = 1;
				mPool = pb;
				mInUse--;
                mCsect->leave();
			}
		}
	}
}

PUBLIC void AudioPool::dump()
{
	if (mNewPool != NULL) {
        // need a dump method for the new one
		printf("NewBufferPool: %d in use ?? in pool\n", mInUse);
	}
	else {
        int pooled = 0;
		mCsect->enter();
		for (OldPooledBuffer* p = mPool ; p != NULL ; p = p->next)
		  pooled++;
		mCsect->leave();

        int used = mAllocated - pooled;

		printf("AudioPool: %d buffers allocated, %d in the pool, %d in use\n",
               mAllocated, pooled, used);

        // this should match
        if (used != mInUse)
          printf("AudioPool: Unmatched usage counters %d %d\n",
                 used, mInUse);

		fflush(stdout);
	}
}

/**
 * Warm the buffer pool with some number of buffers.
 * Was never implemented.
 */
PUBLIC void AudioPool::init(int buffers)
{
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
