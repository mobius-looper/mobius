/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * A VST plugin base class that provides trace and other common
 * services.  This is not specific to Mobius, consider moving this to vst?
 *
 */

#ifndef VST_PLUGIN
#define VST_PLUGIN

#include "audioeffectx.h"

// VstInt32 was added in 2.4, 2.1 used long everywhere
// A few methods changed return types from long to bool, 
// VstLongBool defines a type for those.  VstLongbool is only
// necessary if we wanted to compile against either version.

// nothing should assert this, left behind as an example
#ifdef VST_2_1

#include "AEffEditor.hpp"
typedef long VstInt32;
typedef long VstLongBool;

#else

#include "aeffeditor.h"
typedef bool VstLongBool;

#endif

/****************************************************************************
 *                                                                          *
 *   						EXTERNAL CONFIGURATION                          *
 *                                                                          *
 ****************************************************************************/
/*
 * Each VST DLL will be linked with a different object module that
 * defines the following global variables.  This provides a way to build
 * several DLLs with different port configurations among other things.
 *
 */

extern long VstUniqueId;
extern const char* VstProductName;
extern int VstInputPins;
extern int VstOutputPins;

#define MAX_VST_PORTS 8

/****************************************************************************
 *                                                                          *
 *   							  VST PLUGIN                                *
 *                                                                          *
 ****************************************************************************/

class VstPlugin : public AudioEffectX 
{
  public:

	VstPlugin(audioMasterCallback audioMaster, int progs, int params);
	~VstPlugin();

	void setParameter(VstInt32 index, float value);
	float getParameter(VstInt32 index);
	void getParameterLabel(VstInt32 index, char *label);
	void getParameterDisplay(VstInt32 index, char *text);
	void getParameterName(VstInt32 index, char *text);
	VstInt32 getProgram();
	void setProgram(VstInt32 program);
	void setProgramName(char* name);
	void getProgramName(char* name);
	void process(float** inputs, float** outputs, VstInt32 sampleFrames);
	void processReplacing(float** inputs, float** outputs, VstInt32 sampleFrames);
	VstInt32 dispatcher(VstInt32 opCode, VstInt32 index, VstInt32 value, void *ptr, float opt);
	void open();
	void close();
	void suspend();
	void resume();
	float getVu();
	VstInt32 getChunk(void** data, bool isPreset = false);
	VstInt32 setChunk(void* data, VstInt32 byteSize, bool isPreset = false) ;
	void setSampleRate(float sampleRate);
	void setBlockSize(VstInt32 blockSize);
	AEffEditor* getEditor();
	VstInt32 canDo(char* text);
	VstInt32 processEvents(VstEvents* events);
	bool canParameterBeAutomated(VstInt32 index);
	bool string2parameter(VstInt32 index, char* text);
	float getChannelParameter(VstInt32 channel, VstInt32 index);
	bool getProgramNameIndexed(VstInt32 category, VstInt32 index, char* text);
    VstInt32 getNumCategories();
	bool copyProgram(VstInt32 destination);
	bool beginSetProgram();
	bool endSetProgram();
	void inputConnected(VstInt32 index, bool state);
	void outputConnected(VstInt32 index, bool state);
	bool getInputProperties(VstInt32 index, VstPinProperties* properties);
	bool getOutputProperties(VstInt32 index, VstPinProperties* properties);
	VstPlugCategory getPlugCategory ();
	VstInt32 reportCurrentPosition();
	float* reportDestinationBuffer();
	bool processVariableIo(VstVariableIo* varIo);
	bool setSpeakerArrangement(VstSpeakerArrangement* pluginInput,
							   VstSpeakerArrangement* pluginOutput);
	bool getSpeakerArrangement(VstSpeakerArrangement** pluginInput, 
							   VstSpeakerArrangement** pluginOutput);
	void setBlockSizeAndSampleRate(VstInt32 blockSize, float sampleRate);
	bool setBypass(bool onOff);
	bool getEffectName(char* name);
	bool getErrorText(char* text);
	bool getVendorString(char* text);
	bool getProductString(char* text);
	VstInt32 getVendorVersion();
	VstInt32 vendorSpecific(VstInt32 lArg, VstInt32 lArg2, void* ptrArg, float floatArg);
	void* getIcon();
	bool setViewPosition(VstInt32 x, VstInt32 y);
	VstInt32 getTailSize();
	VstInt32 fxIdle();
	bool getParameterProperties(VstInt32 index, VstParameterProperties* p);
	bool keysRequired();
	VstInt32 getVstVersion();
	VstInt32 getMidiProgramName(VstInt32 channel, MidiProgramName* midiProgramName);
	VstInt32 getCurrentMidiProgram(VstInt32 channel, MidiProgramName* currentProgram);
	VstInt32 getMidiProgramCategory(VstInt32 channel, MidiProgramCategory* category);
	bool hasMidiProgramsChanged(VstInt32 channel);
	bool getMidiKeyName(VstInt32 channel, MidiKeyName* keyName);
	VstInt32 getNextShellPlugin(char* name);
	VstInt32 startProcess();
	VstInt32 stopProcess();
	bool setPanLaw(VstInt32 type, float val);
	VstInt32 beginLoadBank(VstPatchChunkInfo* ptr);
	VstInt32 beginLoadProgram(VstPatchChunkInfo* ptr);

	// Extended methods

	void setParameterCount(int i);
	void setProgramCount(int i);
    void setHostTempo(float tempo);

  protected:
	
	bool mTrace;

	// supposed to be max 64 characters
	char mHostVendor[72];
	char mHostProduct[72];

    // this is a VstInt32, but HostInterface wants a string
    char mHostVersion[64];

    // experiment for audioMasterSetTime
    VstTimeInfo mTimeInfo;
};

/****************************************************************************
 *                                                                          *
 *   							  VST EDITOR                                *
 *                                                                          *
 ****************************************************************************/

class VstEditor : public AEffEditor {

  public:

	VstEditor(AudioEffect* e);
	~VstEditor();

	virtual VstLongBool getRect(ERect **rect);
	virtual VstLongBool open(void *ptr);
	virtual void close();
	virtual void idle();
	
#ifdef VST_2_1
	virtual void update();
	virtual void postUpdate();
#endif

	// 2.1 extensions
	virtual VstLongBool onKeyDown(VstKeyCode &keyCode);
	virtual VstLongBool onKeyUp(VstKeyCode &keyCode);
	virtual VstLongBool setKnobMode(int val);

	virtual bool onWheel(float distance);

	// relevant only on the MAC
	virtual void draw(ERect *rect);
	virtual VstInt32 mouse(VstInt32 x, VstInt32 y);
	virtual VstInt32 key(VstInt32 keyCode);
	virtual void top();
	virtual void sleep();
	
	// extensions
	void setTrace(bool b);
	void setHalting(bool b);

  protected:

	bool mTrace;
	bool mHalting;

  private:

	int mIdleCount;
	ERect mRect;

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif

