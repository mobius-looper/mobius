/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Model for managing Sysex buffers on Windows.  
 * We might be able to reuse some of this on mac, refactor!
 *
 */

#ifndef MIDI_WIN_SYSEX_H
#define MIDI_WIN_SYSEX_H

/****************************************************************************
 *                                                                          *
 *   							 SYSEX BUFFER                               *
 *                                                                          *
 ****************************************************************************/

/**
 * Maximum size of the sysex buffers used by MidiInput.
 */
#define MIDI_SYSEX_MAX 1024 * 64

/***
 * Class used to represent state related to buffers registered to 
 * receive "long data" messages from the midi input device.
 * These are used only during reception of sysex buffers, 
 * sending sysex buffers is MUCH simpler.
 */
class WinSysexBuffer {

	friend class WinMidiInput;

  public:

	WinSysexBuffer();
	~WinSysexBuffer();

	WinSysexBuffer* getNext() {
		return mNext;
	}

	int getLength() {
		return mLength;
	}

	unsigned char *getBuffer() {
		return &mBuffer[0];
	}

	bool isFinished() {
		return mFinished;
	}

	bool isError() {
		return mError;
	}

	bool isAccessible() {
		return mAccessible;
	}

  protected:

	void setInputDevice(WinMidiInput* in) {
		mIn = in;
	}

	WinSysexBuffer* getLink() {
		return mLink;
	}

	void setLink(WinSysexBuffer *b) {
		mLink = b;
	}

	void setNext(WinSysexBuffer* b) {
		mNext = b;
	}

	void setLength(int l) {
		mLength = l;
	}

	MIDIHDR *getHeader() {
		return &mHeader;
	}

	void setError(bool b) {
		mError = b;
	}

	void setAdded(bool b) {
		mAdded = b;
	}

	void setAccessible(bool b) {
		mAccessible = b;
	}

	int getBytesReceived();
	void setFinished();
	void init();
	void process();
	int  prepare(HMIDIIN port);
	void add();
	void unprepare();

  private:

	// The input device we're associated with
	class WinMidiInput* mIn;

	// pointer for the master list
	// this is used by MidiIn so it can get to all buffers allocated
	WinSysexBuffer *mLink;

	// used as a list pointer when the buffer is on the 
	// available or received lists
	WinSysexBuffer *mNext;

	// number of bytes in the buffer
	int mLength;

	// the data buffer
	unsigned char mBuffer[MIDI_SYSEX_MAX];
	
	// set when the sysex has been fully received
	bool mFinished;

	// set after we've been processed
	bool mProcessed;

	// set if there was an error on the reception
	bool mError;

	// set if the buffer has been "added" to the driver
	bool mAdded;

	// Set after the finished buffer has been processed and
	// can be retrieved by the application, avoids having to 
	// maintain another list.
	bool mAccessible;


	//
	// TODO: Windows Specific!
	// Need to remove the need for this or make a subclass
	//
#if _WIN32	
	// the structure registered with the device driver that wraps
	// out data buffer
	MIDIHDR mHeader;

	// set when the sysex has been "prepared"
	HMIDIIN	mPort;
#endif

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
