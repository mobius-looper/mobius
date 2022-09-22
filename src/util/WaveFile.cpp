/**
 * Copyright (C) 2005 Jeff Larson.  All rights reserved.
 *
 * Utility to read/write Wave files.
 * 
 * Resources:
 * 
 * http://www.tsp.ece.mcgill.ca/MMSP/Documents/AudioFormats/WAVE/WAVE.html
 * http://www.borg.com/~jglatt/tech/wave.htm
 * 
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <string.h>
#include <math.h>
//#include <sys/types.h>
//#include <sys/stat.h>
//#include <io.h>

#include "util.h"
#include "WaveFile.h"

/****************************************************************************
 *                                                                          *
 *                             SAMPLE CONVERSION                            *
 *                                                                          *
 ****************************************************************************/
/*
 * Code scavenged from PortAudio, need to include copyright notice somewhere.
 */

#define CLIP( val, min, max )  { val = ((val) < (min)) ? min : (((val) > (max)) ? (max) : (val)); }

/*************************************************************
** Calculate 2 LSB dither signal with a triangular distribution.
** Ranged properly for adding to a 32 bit integer prior to >>15.
** Range of output is +/- 32767
*/
#define PA_DITHER_BITS   (15)
#define PA_DITHER_SCALE  (1.0f / ((1<<PA_DITHER_BITS)-1))

long TriangularDither( void )
{
    static unsigned long previous = 0;
    static unsigned long randSeed1 = 22222;
    static unsigned long randSeed2 = 5555555;
    long current, highPass;
    /* Generate two random numbers. */
    randSeed1 = (randSeed1 * 196314165) + 907633515;
    randSeed2 = (randSeed2 * 196314165) + 907633515;
    /* Generate triangular distribution about 0.
     * Shift before adding to prevent overflow which would skew the distribution.
     * Also shift an extra bit for the high pass filter. 
     */
#define DITHER_SHIFT  ((32 - PA_DITHER_BITS) + 1)
    current = (((long)randSeed1)>>DITHER_SHIFT) + (((long)randSeed2)>>DITHER_SHIFT);
    /* High pass filter to reduce audibility. */
    highPass = current - previous;
    previous = current;
    return highPass;
}

myint16 toInt16(float sample)
{
    long intval = 0;
    bool doDither = false;

    if (!doDither)
      intval = (long)(sample * (32767.0f));
    else {
        // use smaller scaler to prevent overflow when we add the dither
        float dither  = TriangularDither() * PA_DITHER_SCALE;
        float dithered = (sample * (32766.0f)) + dither;
        intval = (long)dithered;
    }

    // PortAudio makes clipping optional
    // not sure why you wouldn't want it
    CLIP(intval, -0x8000, 0x7FFF);

    return (myint16)intval;
}


/**
 * This one is easy.
 */
float toFloat(myint16 sample)
{
    return (sample * (1.0f / 32768.0f));
}

/****************************************************************************
 *                                                                          *
 *   							  WAVE FILE                                 *
 *                                                                          *
 ****************************************************************************/

PUBLIC WaveFile::WaveFile()
{
	init();
}

PUBLIC WaveFile::WaveFile(float* samples, long frames, int channels)
{
	init();
	mData = samples;
	mFrames = frames;
	mChannels = channels;
}

PUBLIC WaveFile::WaveFile(const char* file)
{
	init();
	setFile(file);
}

PRIVATE void WaveFile::init()
{
	mFile = NULL;
	mHandle = NULL;
	mDebug = false;
	mError = 0;
	mFormat = WAV_FORMAT_IEEE;
	mChannels = 2;
	mSampleRate = 44100;
    mAverageBytesPerSecond = 0;
    mSampleDepth = 0;
    mBlockAlign = 0;
	mData = NULL;
    mFrames = 0;
	mDataChunkBytes = 0;
}

PRIVATE void WaveFile::clear()
{
	mError = 0;
	mHandle = NULL;
	mFormat = WAV_FORMAT_IEEE;
	mChannels = 2;
	mSampleRate = 44100;
    mAverageBytesPerSecond = 0;
    mSampleDepth = 0;
    mBlockAlign = 0;
    mFrames = 0;
	mDataChunkBytes = 0;

	delete mData;
	mData = NULL;
}

PUBLIC WaveFile::~WaveFile()
{
	delete mFile;
	delete mData;
}

PUBLIC void WaveFile::setFile(const char* file)
{
	delete mFile;
	mFile = CopyString(file);
}

PUBLIC void WaveFile::setDebug(bool b)
{
	mDebug = b;
}

PUBLIC int WaveFile::getError()
{
	return mError;
}

PUBLIC void WaveFile::setError(int e)
{
	mError = e;
}

PUBLIC int WaveFile::getFormat()
{
	return mFormat;
}

PUBLIC void WaveFile::setFormat(int i)
{
	mFormat = i;
}

PUBLIC int WaveFile::getChannels()
{
	return mChannels;
}

PUBLIC void WaveFile::setChannels(int i)
{
	mChannels = i;
}

PUBLIC long WaveFile::getFrames()
{
	return mFrames;
}

PUBLIC void WaveFile::setFrames(long l)
{
	mFrames = l;
}

PUBLIC int WaveFile::getSampleRate()
{
	return mSampleRate;
}

PUBLIC void WaveFile::setSampleRate(int i)
{
	mSampleRate = i;
}

PUBLIC int WaveFile::getSampleDepth()
{
	return mSampleDepth;
}

PUBLIC void WaveFile::setSampleDepth(int i)
{
	mSampleDepth = i;
}

PUBLIC float* WaveFile::getData()	
{
	return mData;
}

PUBLIC float* WaveFile::stealData()
{
    float* data = mData;
    mData = NULL;
    return data;
}

PUBLIC void WaveFile::setData(float* data)
{
	delete mData;
	mData = data;
}

PUBLIC const char* WaveFile::getErrorMessage(int e)
{
	const char* msg = NULL;
	switch (e) {
		case AUF_ERROR_INPUT_FILE:
			msg = "Invalid input file";
			break;
		case AUF_ERROR_NOT_RIFF:
			msg = "Not a RIFF file";
			break;
		case AUF_ERROR_NOT_WAVE:
			msg = "Not a WAVE file";
			break;
		case AUF_ERROR_FORMAT_CHUNK_SIZE:
			msg = "Invalid chunk size";
			break;
		case AUF_ERROR_COMPRESSED:
			msg = "File is in a compressed format";
			break;
		case AUF_ERROR_SAMPLE_RATE:
			msg = "Unsupported sample rate";
			break;
		case AUF_ERROR_SAMPLE_BITS:
			msg = "Unsupported sample depth";
			break;
		case AUF_ERROR_CHANNELS:
			msg = "Unsupported number of channels";
			break;
		case AUF_ERROR_BLOCK_ALIGN:
			msg = "Invalid block align";
			break;
		case AUF_ERROR_SEEK:
			msg = "Unable to seek";
			break;
		case AUF_ERROR_EOF:
			msg = "Unexpected end of file";
			break;
		case AUF_ERROR_OUTPUT_FILE:
			msg = "Invalid output file";
			break;
		case AUF_ERROR_NO_INPUT_FILE:
			msg = "No input file specified";
			break;
		case AUF_ERROR_NO_OUTPUT_FILE:
			msg = "No output file specified";
			break;
	}
	return msg;
}

PUBLIC void WaveFile::printError(int e)
{
	const char* msg = getErrorMessage(e);
	if (msg != NULL)
	  printf("%s\n", msg);
	else
	  printf("Unknown error code %d\n", e);
}

/**
 * Extract a single channel of samples.  The returned buffer is
 * owned by the caller and must be freed.
 */
PUBLIC float* WaveFile::getChannelSamples(int channel)
{
	float* samples = NULL;
	if (channel < mChannels && mFrames > 0) {
		samples = new float[mFrames];
		for (int i = 0 ; i < mFrames ; i++)
		  samples[i] = mData[(i * mChannels) + channel];
	}
	return samples;
}

/**
 * Merge two split sample arrays back into an interleaved frame array.
 * We take ownership of the arrays and free them.
 */
PUBLIC void WaveFile::setSamples(float* left, float* right, long frames)
{
	delete mData;
	mData = NULL;

	if (frames > 0 && (left != NULL || right != NULL)) {
		mData = new float[frames * mChannels];
		for (int i = 0 ; i < mFrames ; i++) {
			float* dest = &(mData[i * mChannels]);
			dest[0] = (left != NULL) ? left[i] : 0.0f;
			dest[1] = (right != NULL) ? right[i] : 0.0f;
		}
	}

	delete left;
	delete right;
}

/****************************************************************************
 *                                                                          *
 *   							  WAVE READ                                 *
 *                                                                          *
 ****************************************************************************/

PUBLIC int WaveFile::read(const char* path)
{
	setFile(path);
	return read();
}

PUBLIC int WaveFile::read()
{
	clear();

	if (mFile == NULL)
	  mError = AUF_ERROR_NO_INPUT_FILE;
	else {
		FILE* fp = fopen(mFile, "rb");
		if (fp == NULL)
		  mError = AUF_ERROR_INPUT_FILE;
		else {
			// riff header
			char id[4];
			readId(fp, id);
			if (strncmp(id, "RIFF", 4))
			  mError = AUF_ERROR_NOT_RIFF;
			else {
				// file size
				myuint32 fileSize = read32(fp);
				if (mDebug)
				  printf("File size: %d\n", (int)fileSize);
				// file type
				readId(fp, id);
				if (strncmp(id, "WAVE", 4))
				  mError = AUF_ERROR_NOT_WAVE;
				else {
					myuint32 chunkSize;
					while (mError == 0 && mData == NULL) {
						// read chunk header
						readId(fp, id);
						if (!mError) {
							// read chunk size
							chunkSize = read32(fp);
							if (!mError) {
								if (mDebug)
								  printf("Chunk size %d\n", (int)chunkSize);
								if (!strncmp(id, "fmt ", 4)) 
								  processFormatChunk(fp, chunkSize);
								else if (!strncmp(id, "data", 4))
								  processDataChunk(fp, chunkSize);
								else if (chunkSize > 0) {
									// ignore this chunk, pad to even boundary
									if (chunkSize & 1)
									  chunkSize++;
									if (fseek(fp, chunkSize, SEEK_CUR) != 0)
									  mError = AUF_ERROR_SEEK;
								}
							}
						}
					}
				}
			}
			fclose(fp);
		}
	}

	return mError;
}

/**
 * Read a chunk id.
 * These are 4 bytes and do not need byte translation, I guess because we read
 * and write them one byte at a time?
 */
PRIVATE void WaveFile::readId(FILE* fp, char* buffer)
{
	buffer[0] = 0;
	// size=1, count=4
	size_t count = fread(buffer, 1, 4, fp);
	if (count != 4)
	  mError = AUF_ERROR_EOF;
	else if (mDebug) {
		char string[8];
		strncpy(string, buffer, 4);
		string[4] = 0;
		printf("Header: %s\n", string);
	}
}

/**
 * Read a 4 byte integer. These need byte translation on PPC.
 */
PRIVATE myuint32 WaveFile::read32(FILE* fp)
{
	myuint32 value;

	// size=4, count=1
	size_t count = fread(&value, 4, 1, fp);
	if (count != 1)
	  mError = AUF_ERROR_EOF;

#ifdef __BIG_ENDIAN__
	swap32((unsigned char*)&value);
#endif

	return value;
}

/**
 * Read a 2 byte integer. These need byte translation on PPC.
 */
PRIVATE myuint16 WaveFile::read16(FILE* fp)
{
	myuint16 value;

	// size=2, count=1
	size_t count = fread(&value, 2, 1, fp);
	if (count != 1)
	  mError = AUF_ERROR_EOF;

#ifdef __BIG_ENDIAN__
	swap16((unsigned char*)&value);
#endif
	return value;
}

/**
 * Swap byte order in a 64 bit value.
 */
PRIVATE void WaveFile::swap64(unsigned char* bytes)
{
	unsigned char b = bytes[0];
	bytes[0] = bytes[7];
	bytes[7] = b;

	b = bytes[1];
	bytes[1] = bytes[6];
	bytes[6] = b;

	b = bytes[2];
	bytes[2] = bytes[5];
	bytes[5] = b;

	b = bytes[3];
	bytes[3] = bytes[4];
	bytes[4] = b;
}

/**
 * Swap byte order in a 32 bit value.
 */
PRIVATE void WaveFile::swap32(unsigned char* bytes)
{
	unsigned char b = bytes[0];
	bytes[0] = bytes[3];
	bytes[3] = b;

	b = bytes[1];
	bytes[1] = bytes[2];
	bytes[2] = b;
}

/**
 * Swap byte order in a 16 bit value.
 */
PRIVATE void WaveFile::swap16(unsigned char* bytes)
{
	unsigned char b = bytes[0];
	bytes[0] = bytes[1];
	bytes[1] = b;
}

PRIVATE void WaveFile::processFormatChunk(FILE* fp, long size)
{
	mFormat = read16(fp);
	mChannels = read16(fp);
	mSampleRate = read32(fp);
	mAverageBytesPerSecond = read32(fp);
	mBlockAlign = read16(fp);
	mSampleDepth = read16(fp);

	if (mDebug) {
		printf("Format %d\n", mFormat);
		printf("Channels %d\n", mChannels);
		printf("Sample Rate %d\n", (int)mSampleRate);
		printf("Average Bytes Per Second %d\n", (int)mAverageBytesPerSecond);
		printf("Block Align %d\n", mBlockAlign);
		printf("Sample Depth %d\n", mSampleDepth);
	}

	if (!mError) {
		// block align is the number of bytes per frame
		int expectedBlockAlign = (int)ceil((float)(mChannels * (mSampleDepth / 8)));
		if (mBlockAlign != expectedBlockAlign)
		  mError = AUF_ERROR_BLOCK_ALIGN;

		else if (mFormat != WAV_FORMAT_PCM && mFormat != WAV_FORMAT_IEEE) {
			mError = AUF_ERROR_COMPRESSED;
			printf("Unknown format tag %d\n", mFormat);
		}
        /*
		else if (mSampleRate != 44100) {
			// we don't really care what this is, just load it and
			// let it play back funny?
			mError = AUF_ERROR_SAMPLE_RATE;
		}
        */

		else if (mChannels <= 0 || mChannels == 5 || mChannels > 6)
		  mError = AUF_ERROR_CHANNELS;

		else if (size > 16) {
			// extra stuff, but not compressed, ignore
			// this should be zero for PCM, for IEEE it shoudl have at
			// least a 16 bits of extension size, not sure what if anything
			// is interesting in here
			if (fseek(fp, size - 16, SEEK_CUR))
			  mError = AUF_ERROR_SEEK;
		}
	}
}

/**
 * Frame formats:
 * stereo: left, right
 * 3 channel: left, right, center
 * quad: front left, front right, rear left, rear right
 * 4 channel: left, center, right, surround
 * 6 channel: left center, left, center, right center, right, surround
 */
PRIVATE void WaveFile::processDataChunk(FILE* fp, long size)
{
	unsigned char* data = (unsigned char*)new unsigned char[size];

	// purify giving an uninitialized memory read on the source buffer
	// sometimes, not sure why
	memset(data, 0, size);

	// read the raw bytes all at once, we'll swap them later if necessary
	long count = fread(data, 1, size, fp);

	if (count != size) {
        mError = AUF_ERROR_EOF;
    }
	else if (mFormat == WAV_FORMAT_PCM) {
		processPcmDataChunk(data, size);
	}
	else if (mFormat == WAV_FORMAT_IEEE) {
		processIEEEDataChunk(data, size);
	}
	else {
		// should have caught this by now
		mError = AUF_ERROR_COMPRESSED;
	}

	delete data;
}

PRIVATE void WaveFile::processPcmDataChunk(unsigned char* data, long size)
{
	if (mSampleDepth <= 8) {
        // always one UNSIGNED byte per sample
        mError = AUF_ERROR_SAMPLE_BITS;
    }
    else if (mSampleDepth < 16) {
        // always two signed bytes, left justified
        mError = AUF_ERROR_SAMPLE_BITS;
    }
    else if (mSampleDepth > 16 && mSampleDepth <= 24) {
        // three signed bytes, left justified
        mError = AUF_ERROR_SAMPLE_BITS;
    }
    else if (mSampleDepth > 24 && mSampleDepth <= 32) {
        // four signed bytes, left justified
        mError = AUF_ERROR_SAMPLE_BITS;
    }
    else if (mSampleDepth != 16) {
        // invalid depth
        mError = AUF_ERROR_SAMPLE_BITS;
    }
    else {
        // ahh, we're here
		// blockAlign is bytesPerSample * channels, effectively the frame size
        // may be padding to bring this up to an even number of bytes?
        mFrames = size / mBlockAlign;

        // convert everything to stereo, add other options someday
        mData = new float[mFrames * 2];
        myuint16* src = (myuint16*)data;
        
        int srcSample = 0;
        int destSample = 0;

        for (int i = 0 ; i < mFrames ; i++) {
            if (mChannels == 1) {
                mData[destSample++] = toFloat(read16(src, srcSample));
                mData[destSample++] = toFloat(read16(src, srcSample));
            }
            else if (mChannels == 2 || mChannels == 3) {
                mData[destSample++] = toFloat(read16(src, srcSample));
                mData[destSample++] = toFloat(read16(src, srcSample + 1));
            }
            else if (mChannels == 4) {
                // assume 4 channel surround rather than quad
                mData[destSample++] = toFloat(read16(src, srcSample));
                mData[destSample++] = toFloat(read16(src, srcSample + 2));
            }
            else if (mChannels == 6) {
                mData[destSample++] = toFloat(read16(src, srcSample + 1));
                mData[destSample++] = toFloat(read16(src, srcSample + 4));
            }
            srcSample += mChannels;
        }
    }
}

/**
 * Extract a 2 byte integer from a raw data block.
 */
PRIVATE myuint16 WaveFile::read16(myuint16* src, int index)
{
	myuint16 value = src[index];
#if __BIG_ENDIAN__
	swap16((unsigned char*)&value);
#endif
	return value;
}

/**
 * Extract a 4 byte float from a raw data block.
 */
PRIVATE float WaveFile::readFloat(float* src, int index)
{
	float value = src[index];
#if __BIG_ENDIAN__
	swap32((unsigned char*)&value);
#endif
	return value;
}

/**
 * Extract an 8 byte double float from a raw data block.
 * We don't write these but I suppose we could read them.
 */
PRIVATE double WaveFile::readDouble(double* src, int index)
{
	double value = src[index];
#if __BIG_ENDIAN__
	swap64((unsigned char*)&value);
#endif
	return value;
}

PRIVATE void WaveFile::processIEEEDataChunk(unsigned char* data, long size)
{
	if (mSampleDepth != 32 && mSampleDepth != 64) {
        mError = AUF_ERROR_SAMPLE_BITS;
    }
    else {
        mFrames = size / mBlockAlign;

        // convert everything to stereo, add other options someday
		// optimization: if mChannels == 2 which it almost always will be, 
		// we don't have to allocate another block and copy, just use
		// the original block, I want to force it through the logic though
		// for testing
        mData = new float[mFrames * 2];

		if (mSampleDepth == 32) {
			float* src = (float*)data;
			int srcSample = 0;
			int destSample = 0;
			for (int i = 0 ; i < mFrames ; i++) {
				if (mChannels == 1) {
					mData[destSample++] = readFloat(src, srcSample);
					mData[destSample++] = readFloat(src, srcSample);
				}
				else if (mChannels == 2 || mChannels == 3) {
					mData[destSample++] = readFloat(src, srcSample);
					mData[destSample++] = readFloat(src, srcSample + 1);
				}
				else if (mChannels == 4) {
					// assume 4 channel surround rather than quad
					mData[destSample++] = readFloat(src, srcSample);
					mData[destSample++] = readFloat(src, srcSample + 2);
				}
				else if (mChannels == 6) {
					mData[destSample++] = readFloat(src, srcSample + 1);
					mData[destSample++] = readFloat(src, srcSample + 4);
				}
				srcSample += mChannels;
			}
		}
		else {
			double* src = (double*)data;
			int srcSample = 0;
			int destSample = 0;

			for (int i = 0 ; i < mFrames ; i++) {
				if (mChannels == 1) {
					mData[destSample++] = (float)(readDouble(src, srcSample));
					mData[destSample++] = (float)(readDouble(src, srcSample));
				}
				else if (mChannels == 2 || mChannels == 3) {
					mData[destSample++] = (float)(readDouble(src, srcSample));
					mData[destSample++] = (float)(readDouble(src, srcSample + 1));
				}
				else if (mChannels == 4) {
					// assume 4 channel surround rather than quad
					mData[destSample++] = (float)(readDouble(src, srcSample));
					mData[destSample++] = (float)(readDouble(src, srcSample + 2));
				}
				else if (mChannels == 6) {
					mData[destSample++] = (float)(readDouble(src, srcSample + 1));
					mData[destSample++] = (float)(readDouble(src, srcSample + 4));
				}
				srcSample += mChannels;
			}
		}
	}
}

/****************************************************************************
 *                                                                          *
 *                                 WAVE WRITE                               *
 *                                                                          *
 ****************************************************************************/

PUBLIC int WaveFile::write(const char* file)
{
	setFile(file);
	return write();
}

/**
 * Write the contents of a wave file in one chunk.
 *
 * We always write in 16 bit 44.1 stereo.
 * Ignore any format settings that may have been left
 * over from a previous read.
 */
PUBLIC int WaveFile::write()
{
	mError = 0;

	if (writeStart() == 0) {
		write(mData, mFrames);
		writeFinish();
	}

	return mError;
}

/**
 * Prepare to write a wave file incrementally.
 * The data will be written with one or more calls to.
 * write(float*, long) and followed by a writeFinish
 */
PUBLIC int WaveFile::writeStart()
{
	mError = 0;
	mDataChunkBytes = 0;

	if (mFile == NULL)
	  mError = AUF_ERROR_NO_OUTPUT_FILE;

    if (mChannels <= 0 || mChannels == 5 || mChannels > 6)
        mError = AUF_ERROR_CHANNELS;

    else {
		mSampleRate = 44100;
	
		// try to preserve format, but init if we can't
		if (mFormat == WAV_FORMAT_PCM) {
			mSampleDepth = 16;
		}
		else if (mFormat == WAV_FORMAT_IEEE) {
			mSampleDepth = 32;
		}
		else {
			// garbage, shouldn't happen
			mFormat = WAV_FORMAT_IEEE;
			mSampleDepth = 32;
		}

        mHandle = fopen(mFile, "wb");
        if (mHandle == NULL)
            mError = AUF_ERROR_OUTPUT_FILE;
        else {
            // short, ushort, ulong, ulong, ushort, ushort
            myuint32 fmtChunkSize;
			myuint32 fileSize;

			if (mFormat == WAV_FORMAT_PCM) {
				// WAVE, header/chunksize format, header/chunksize data, pad
				fmtChunkSize = 16;
				mDataChunkBytes = (mFrames * mChannels) * 2;
				fileSize = 4 + 8 + fmtChunkSize + 8 + mDataChunkBytes;
				if (mDataChunkBytes & 1)
				  fileSize++;
			}
			else {
				// according to some interpretations of the spec,
				// IEEE is supposed to have an "extension" in the
				// format chunk, just to contain the size of the
				// extension, which will be zero, most applications
				// seem to tolerate not having this
				fmtChunkSize = 16;
				mDataChunkBytes = (mFrames * mChannels) * 4;
				fileSize = 4 + 8 + fmtChunkSize + 8 + mDataChunkBytes;
				if (mDataChunkBytes & 1)
				  fileSize++;
			}

			writeId(mHandle, "RIFF");				// RIFF header
			write32(mHandle, fileSize);
			writeId(mHandle, "WAVE");				// file type
        
            // block align is the number of bytes per frame
            // always recalculate this
            mBlockAlign = (myuint16)ceil((float)(mChannels * (mSampleDepth / 8)));

            // not sure if this is necessary, but this is the recommended
			// formula
            myuint32 averageBytesPerSecond = (myuint32)
				ceil((float)(mSampleRate * mBlockAlign));

			writeId(mHandle, "fmt ");
			write32(mHandle, fmtChunkSize);
			write16(mHandle, mFormat);
			write16(mHandle, mChannels);
			write32(mHandle, mSampleRate);
			write32(mHandle, averageBytesPerSecond);
			write16(mHandle, mBlockAlign);
			write16(mHandle, mSampleDepth);

			// for IEEE may need to store an extra 2 byte "extension" length

            writeId(mHandle, "data");
			write32(mHandle, mDataChunkBytes);
        }
    }

	return mError;
}

/**
 * Write a chunk id.
 * These are 4 bytes and do not need byte translation, I guess because we read
 * and write them one byte at a time?
 */
PRIVATE void WaveFile::writeId(FILE* fp, const char* id)
{
	fwrite(id, 1, 4, fp);
}

/**
 * Write a 4 byte integer.
 */
PRIVATE void WaveFile::write32(FILE* fp, myuint32 value)
{
#ifdef __BIG_ENDIAN__
	swap32((unsigned char*)&value);
#endif
	fwrite(&value, 4, 1, fp);
}

/**
 * Write a 4 byte float.
 */
PRIVATE void WaveFile::writeFloat(FILE* fp, float value)
{
#ifdef __BIG_ENDIAN__
	swap32((unsigned char*)&value);
#endif
	fwrite(&value, 4, 1, fp);
}

/**
 * Write a 2 byte integer.
 */
PRIVATE void WaveFile::write16(FILE* fp, myuint16 value)
{
#ifdef __BIG_ENDIAN__
	swap16((unsigned char*)&value);
#endif
	fwrite(&value, 2, 1, fp);
}

/**
 * Write a block of frames.  A call to writeStart must
 * have been made first.
 */
PUBLIC int WaveFile::write(float* buffer, long frames)
{
	if (!mError) {
		if (mHandle == NULL)
		  mError = AUF_ERROR_NO_OUTPUT_FILE;
		else {
			int samples = frames * mChannels;
			if (mFormat == WAV_FORMAT_PCM) {
				for (int i = 0 ; i < samples ; i++) {
					float sample = buffer[i];
					myint16 isample = toInt16(sample);
					write16(mHandle, isample);
				}
			}
			else {
				// foo, would be simpler just to blast out the entire block
				for (int i = 0 ; i < samples ; i++) {
					float sample = buffer[i];
					writeFloat(mHandle, sample);
				}
			}
        }
    }

	return mError;
}

/**
 * Finish up an incremental write.
 */
PUBLIC int WaveFile::writeFinish()
{
	if (mHandle == NULL)
	  mError = AUF_ERROR_NO_OUTPUT_FILE;
	else {
		if (mDataChunkBytes & 1) {
			int pad = 0;
			fwrite(&pad, 1, 1, mHandle);
		}
		fclose(mHandle);
	}

	return mError;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
