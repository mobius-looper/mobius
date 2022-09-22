/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * A VstPlugin that also implements the Mobius AudioStream interface.
 *
 * An AudioStream is given to Mobius during construction, it will
 * in turn be given to the Recorder and overide the AudioStream 
 * returned by the AudioInterface.  
 *
 * Need to make this more flexible so there can be several audio streams
 * allowing tracks to either be connected to VST or to another
 * port on the machine.
 *
 */

#ifndef VST_MOBIUS
#define VST_MOBIUS

#include "VstPlugin.h"
#include "AudioInterface.h"
#include "HostInterface.h"

//////////////////////////////////////////////////////////////////////
//
// Ports
//
//////////////////////////////////////////////////////////////////////

/**
 * Helper structure used to maintain processing state for
 * each "port" we expose to the Recorder.
 *
 * This is the same as AUMobius/AudioStreamPort, consider breaking out a 
 * utility class for port management and interleave/deinterleave.
 */
typedef struct {

	float* input;
	bool inputPrepared;

	float* output;
	bool outputPrepared;

} VstPort;

//////////////////////////////////////////////////////////////////////
//
// AudioStreamProxy
//
//////////////////////////////////////////////////////////////////////

/**
 * Unfortunately there are several methods on the AudioStream
 * interface that conflict with VstPlugin: open, close, setSampleRate
 * etc.  Need to make a proxy class to keep the bulk of the 
 * interface off of VstMobius.  Sigh, AUMobius got lucky and doesn't
 * have to do this.
 */
class AudioStreamProxy : public AudioStream 
{
  public:

    AudioStreamProxy(class VstMobius* vst);
    ~AudioStreamProxy();

	AudioInterface* getInterface();

	bool setInputDevice(int id);
	bool setInputDevice(const char* name);
	AudioDevice* getInputDevice();

	bool setOutputDevice(int id);
	bool setOutputDevice(const char* name);
	AudioDevice* getOutputDevice();

    int getInputChannels();
	int getInputPorts();

	int getOutputChannels();
	int getOutputPorts();

	void setSampleRate(int i);
	int getSampleRate();

	void setHandler(AudioHandler* h);
	AudioHandler* getHandler();

	bool open();
	void close();

	// this is PortAudio specific, weed this out!!
	const char* getLastError();
	void setSuggestedLatencyMsec(int msec);
    int getInputLatencyFrames();
    void setInputLatencyFrames(int frames);
    int getOutputLatencyFrames();
    void setOutputLatencyFrames(int frames);
	void printStatistics();

    double getStreamTime();
    double getLastInterruptStreamTime();

	// AudioHandler callbacks
	long getInterruptFrames();
	void getInterruptBuffers(int inport, float** inbuf, 
							 int outport, float** outbuf);
	AudioTime* getTime();

  private:

    class VstMobius* mVst;
};

//////////////////////////////////////////////////////////////////////
//
// VstMobius
//
//////////////////////////////////////////////////////////////////////

class VstMobius : public VstPlugin, 
  public HostInterface, public AudioInterface
{
  public:

	VstMobius(Context* context, audioMasterCallback audioMaster);
	~VstMobius();

    //
	// VstPlugin
    //

	VstInt32 getVstVersion();

	void process (float **inputs, float **outputs, VstInt32 sampleframes);
	void processReplacing (float **inputs, float **outputs, VstInt32 sampleFrames);

	void open();
	void close();
	void suspend();
	void resume();

	bool getEffectName (char* name);
	bool getVendorString (char* text);
	bool getProductString (char* text);
	VstInt32 getVendorVersion();
	VstPlugCategory getPlugCategory();
	bool getErrorText(char* text);

	VstInt32 canDo(char* text) ;
	bool getInputProperties(VstInt32 index, VstPinProperties* properties);
	bool getOutputProperties(VstInt32 index, VstPinProperties* properties);
	bool keysRequired();
	bool setBypass(bool onOff);
	void setBlockSize(VstInt32 size);
	void setSampleRate(float rate);
	void setBlockSizeAndSampleRate(VstInt32 size, float rate);

	void setParameter(VstInt32 index, float value);
	float getParameter(VstInt32 index);
	void getParameterLabel(VstInt32 index, char *label);
	void getParameterDisplay(VstInt32 index, char *text);
	void getParameterName(VstInt32 index, char *text);
    bool canParameterBeAutomated(VstInt32 index);
    bool string2parameter(VstInt32 index, char* text);
    float getChannelParameter(VstInt32 channel, VstInt32 index);
    bool getParameterProperties(VstInt32 index, VstParameterProperties* p);

	VstInt32 getProgram();
	void setProgram(VstInt32 program);
	void setProgramName(char* name);
	void getProgramName(char* name);
    bool copyProgram(VstInt32 destination);
    bool beginSetProgram();
    bool endSetProgram();
    VstInt32 getNumCategories();
    bool getProgramNameIndexed(VstInt32 category, VstInt32 index, char* text);

	VstInt32 startProcess();
	VstInt32 stopProcess();
	VstInt32 processEvents(VstEvents* events);

	//
	// HostInterface
	//
	
	Context* getContext();
	const char* getHostName();
	const char* getHostVersion();
	AudioInterface* getAudioInterface();
	void notifyParameter(int id, float value);

    //
    // AudioInterface
    //

	void terminate();
	AudioDevice** getDevices();
	AudioDevice* getDevice(int id);
	AudioDevice* getDevice(const char* name, bool output);
	void printDevices();

	// only thing interesting
	AudioStream* getStream();

    // 
    // AudioStreamProxy pass throughs
    //

    int getPortChannels();
	AudioHandler* getHandler();
	void setHandler(AudioHandler* h);
    void setSampleRateInt(int i);
    int getSampleRateInt();

	const char* getLastError();
    int getInputLatencyFrames();
    void setInputLatencyFrames(int frames);
    int getOutputLatencyFrames();
    void setOutputLatencyFrames(int frames);

	long getInterruptFrames();
	void getInterruptBuffers(int inport, float** inbuf, 
							 int outport, float** outbuf);
	AudioTime* getTime();

	//
	// Our extra stuff
	//
	
	PluginInterface* getPlugin();

  protected:
	
    void initParameters();
    void exportParameters();
    int scaleParameterIn(PluginParameter* p, float value);
    float scaleParameterOut(PluginParameter* p, int value);

	void setBlockSizeInternal(int size);
    void setSampleRateInternal(float rate);

	void processInternal(float** inputs, float** outputs, 
						 VstInt32 sampleFrames, bool replace);
	void mergeBuffers(float* dest, float** src, int port, long frames);
	void initSync();
	void checkTime(VstInt32 frames);
	void checkTimeOld(VstInt32 frames);
	bool checkTransportOld(VstTimeInfo* time);
	void checkTempoOld(VstTimeInfo* time);
    void sendMidiEvents();

	class Context* mContext;
    class PluginInterface* mPlugin;
    class AudioStream* mStream;
	class AudioHandler* mHandler;
	class VstMobiusEditor* mEditor;

	int mInputLatency;
	int mOutputLatency;
	int mSampleRate;
	int mInputPins;
	int mOutputPins;
	int mParameters;
    class PluginParameter** mParameterTable;
    class PluginParameter* mDummyParameter;
	int mPrograms;
	bool mHostRewinds;
	char mError[256];
	
	VstPort mPorts[MAX_VST_PORTS];
	float** mInterruptInputs;
	float** mInterruptOutputs;
	long mInterruptFrames;
	bool mProcessing;
	bool mBypass;
	bool mDummy;
    bool mExporting;

	AudioTime mTime;

	int mTempoBlocks;

    // new way
    class HostSyncState* mSyncState;

    // old, soon to be removed
	double mBeatsPerFrame;
	double mBeatsPerBar;
	double mLastSample;
	double mLastPpqRange;
	int mLastBeat;
	int mLastBar;
	int mBeatCount;
	int mBeatDecay;
	bool mAwaitingRewind;
	bool mCheckSamplePosTransport;
	bool mCheckPpqPosTransport;
	bool mTraceBeats;
};

/****************************************************************************
 *                                                                          *
 *   						  VST MOBIUS EDITOR                             *
 *                                                                          *
 ****************************************************************************/

/**
 * Tried to host the main mobius window inside this but having
 * lots of problems.  For now, let an empty host controlled window
 * come up then launch another standalone frame for Mobius, and
 * keep them in sync.
 */
class VstMobiusEditor : public VstEditor
{
  public:

	VstMobiusEditor(VstMobius* mobius);
	~VstMobiusEditor();

	VstLongBool getRect(ERect **rect);
	VstLongBool open(void *ptr);
	void close();
    VstLongBool onKeyDown(VstKeyCode &keyCode);
    VstLongBool onKeyUp(VstKeyCode &keyCode);
    void disconnect();

  private:

	VstMobius* mVst;
	ERect mRect;

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
