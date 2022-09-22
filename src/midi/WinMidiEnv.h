/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 *
 * Windows implementation of the MidiEnv.
 *
 */

#ifndef WIN_MIDI_ENV_H
#define WIN_MIDI_ENV_H

#include <stdio.h>
#include <windows.h>
#include <mmsystem.h>

#include "MidiEnv.h"
#include "MidiTimer.h"
#include "MidiInput.h"
#include "MidiOutput.h"

class WinMidiEnv : public MidiEnv {

	friend class MidiEnv;

  public:

	~WinMidiEnv();

	void loadDevices();

	class MidiTimer* newMidiTimer();
	class MidiInput* newMidiInput(class MidiPort* port);
	class MidiOutput* newMidiOutput(class MidiPort* port);

  protected:

	WinMidiEnv();

  private:

    bool mDevicesLoaded;


};

class WinMidiTimer : public MidiTimer
{
  public:
	
	WinMidiTimer(WinMidiEnv* env);
	~WinMidiTimer();

	bool start();
	void stop();
	bool isRunning();

  private:

	bool activate(void);
	void deactivate(void);

    int		mTimer;                 // internal timer resource id
    bool    mActive;                // true if registered with the OS timer
};

class WinMidiInput : public MidiInput 
{
    friend class WinSysexBuffer;

  public:
	
	WinMidiInput(WinMidiEnv* env, MidiPort* port);
	~WinMidiInput();

	// MidiInput overloads

	int connect(void);
	void disconnect();
	bool isConnected();
	void notifyEventsReceived();

    // interrupt handler methods
	void processLongData(DWORD p1, DWORD p2, bool error);
    void processEventsReceived();

	// Application callback event accessors

	class WinSysexBuffer* getSysex();
	class WinSysexBuffer* getOneSysex();
	void freeSysex(class WinSysexBuffer *buffers);
	void ignoreSysex(void);

	void setIgnoreSysex(bool ignore);
    bool isIgnoreSysex();
	void setSysexEchoSize(int size);
	int getSysexBytesReceived(void);
	int getSysexBytesReceiving(void);
	void cancelSysex(void);

  protected:

    void setError(int rc);

  private:

    void stopMonitorThread();
	void enable(void);
	void disable(void);

	// sysex handling
    void allocSysexBuffer();
	void processSysexBuffers(class WinSysexBuffer *buffers) ;
	void echoSysex(class WinSysexBuffer *buffers);
	void addSysex(class WinSysexBuffer *b);
	void allocWinSysexBuffer() ;
	void ignoreSysex(class WinSysexBuffer *buffers);

	class WinMidiThread* mMonitorThread;
	HMIDIIN mNativePort;

    // true if we're in the event processing callback
	bool mInCallback;

    // true to echo sysex messages
	bool mEchoSysex;

	// the master list of sysex buffers allocated
	WinSysexBuffer* mSysexBuffers;

	// received sysex buffer list
	WinSysexBuffer* mSysexReceived;
	WinSysexBuffer* mLastSysexReceived;

	// processed sysex buffer list (received and accessible)
	WinSysexBuffer* mSysexProcessed;
	WinSysexBuffer* mLastSysexProcessed;

	// active sysex buffer
	WinSysexBuffer* mSysexActive;
	WinSysexBuffer* mLastSysexActive;

	// expected size of echoed sysex response to ignore
	int mSysexEchoSize;

	// set if we want to ignore sysex buffers we receive while enabled
	bool mIgnoreSysex;

};

class WinMidiOutput : public MidiOutput, public MidiInputListener
{
  public:
	
	WinMidiOutput(WinMidiEnv* env, MidiPort* port);
	~WinMidiOutput();

    // required MidiOutput overloads
	int connect(void);
	void disconnect();
	bool isConnected();
	void printWarnings();
	void send(int msg);
	int sendSysex(const unsigned char *buffer, int length);

	// additional sysex
	void finishedLongData(void);
	int sendSysexNoWait(const unsigned char *buffer, int length);
	int sendSysex(const unsigned char *buffer, int length, bool waitFinished);
	bool isSysexFinished();
	int getSysexBytesSent() ;
	void endSysex();

	// hacky synchronous sysex send/receive
	void midiInputEvent(class MidiInput *in);
	void sysexCallback(class WinMidiInput *in);
	int sysexRequest(const unsigned char *request, int reqlen,
					 WinMidiInput *in, unsigned char *reply, int replen);

    // unit tests
    static void testOpen();

  private:
	
    void setError(int rc);

	HMIDIOUT mNativePort;
	MIDIHDR	 mOutHeader;		// header for sending sysex

	// transient sysex send state
	int             mSysexTimeouts;
	bool            mSysexPrepared;
	bool            mSendingSysex;
	unsigned char*  mSysexBuffer;
	int             mSysexBufferLength;
    int				mSysexLastLength;

	// transient sysex recdeive state if we're
	// doing a combo send/receive with a MidiInput 
	int mSysexRequestLength;
	int mSysexReceived;
	int mSysexDone;

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
