/**
 * Copyright (C) 2005 Jeff Larson.  All rights reserved.
 *
 * Class to read/write wave files.  Should generalize this to support
 * other file types.
 */

#ifndef WAVE_FILE_H
#define WAVE_FILE_H

#include "port.h"

#define WAV_FORMAT_PCM 1
#define WAV_FORMAT_IEEE 3

#define AUF_ERROR_INPUT_FILE 1
#define AUF_ERROR_NOT_RIFF 2
#define AUF_ERROR_NOT_WAVE 3
#define AUF_ERROR_FORMAT_CHUNK_SIZE 4
#define AUF_ERROR_COMPRESSED 5
#define AUF_ERROR_SAMPLE_RATE 6
#define AUF_ERROR_SAMPLE_BITS 7
#define AUF_ERROR_CHANNELS 8
#define AUF_ERROR_BLOCK_ALIGN 9
#define AUF_ERROR_SEEK 10
#define AUF_ERROR_EOF 11
#define AUF_ERROR_OUTPUT_FILE 12
#define AUF_ERROR_NO_INPUT_FILE 13
#define AUF_ERROR_NO_OUTPUT_FILE 14

PUBLIC class WaveFile {

  public:

	WaveFile();
	WaveFile(const char* file);
    WaveFile(float* samples, long frames, int channels);
	~WaveFile();

	void setFile(const char* file);
	void setDebug(bool b);

	void clear();
	int read();
	int read(const char* file);

    int write();
    int write(const char* file);
	int writeStart();
	int write(float* buffer, long frames);
	int writeFinish();

	int getError();
	void setError(int e);
	const char* getErrorMessage(int e);

	int getFormat();
	void setFormat(int f);

	int getChannels();
	void setChannels(int i);

	int getSampleRate();
	void setSampleRate(int i);

	int getSampleDepth();
	void setSampleDepth(int i);
	
	float* getData();
    float* stealData();
	void setData(float* data);

    long getFrames();
    void setFrames(long f);

	float* getChannelSamples(int channel);
	void setSamples(float* left, float* right, long frames);

	void printError(int e);

  private:

	void init();
	void processFormatChunk(FILE* fp, long size);
	void processDataChunk(FILE* fp, long size);
	void processPcmDataChunk(unsigned char* data, long size);
	void processIEEEDataChunk(unsigned char* data, long size);

	void readId(FILE* fp, char* buffer);
	myuint32 read32(FILE* fp);
	myuint16 read16(FILE* fp);

	myuint16 read16(myuint16* src, int index);
	float readFloat(float* src, int index);
	double readDouble(double* src, int index);

	void swap64(unsigned char* bytes);
	void swap32(unsigned char* bytes);
	void swap16(unsigned char* bytes);

	void writeId(FILE* fp, const char* id);
	void write32(FILE* fp, myuint32 value);
	void writeFloat(FILE* fp, float value);
	void write16(FILE* fp, myuint16 value);

	char* mFile;
	FILE* mHandle;
	bool mDebug;
	int mError;
	myuint16 mFormat;
	myuint16 mChannels;
	myuint32 mSampleRate;
    myuint32 mAverageBytesPerSecond;
	myuint16 mSampleDepth;
    myuint16 mBlockAlign;
	myuint16 mBitsPerSample;

	float* mData;
    long mFrames;

	// transient write state
	long mDataChunkBytes;


};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
