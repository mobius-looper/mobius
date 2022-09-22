/*
 * A wrapper around the AudioUnit classes providing some 
 * common implementations and trace.  
 *
 */

#ifndef BASE_AUDIO_UNIT_H
#define BASE_AUDIO_UNIT_H

#include <AudioUnit/AudioUnit.h>
#include "AUEffectBase.h"
#include "BaseAudioUnitConstants.h"


//////////////////////////////////////////////////////////////////////
//
// Parameters
//
//////////////////////////////////////////////////////////////////////

/*
 * Examples have enumerations for parameters, clould use defines too.
 * This is just an example, would not be used in a subclass.
 */

enum {
	kParamRandomValue = 1
};

//////////////////////////////////////////////////////////////////////
//
// AU Core Class
//
//////////////////////////////////////////////////////////////////////

/**
 * Inner class that does most of the work.
 * The examples have this as a true inner class of AUEffectBase.
 */
class BaseAudioUnitKernel : public AUKernelBase
{
  public:

	BaseAudioUnitKernel(AUEffectBase *inAudioUnit, bool dotrace);
		
	// processes one channel of interleaved samples
	virtual void Process(const Float32* src, Float32* dest,
						 UInt32	frames, UInt32 channels,
						 bool&	ioSilence);

	virtual void Reset();

  private:

	bool mTrace;

};

// !! TODO: Need to be a MIDIEffectBase

class BaseAudioUnit : public AUEffectBase {
  public:

	BaseAudioUnit(AudioUnit component);

    int	GetNumCustomUIComponents();
    void GetUIComponentDescs(ComponentDescription* inDescArray);

	virtual AUKernelBase* NewKernel();
	virtual ComponentResult	Version();
	virtual	bool SupportsTail();

	// must be overloaded to handle subclass parameters
	virtual ComponentResult GetParameterInfo(AudioUnitScope	scope,
											 AudioUnitParameterID id,
											 AudioUnitParameterInfo	&info);

  private:

	bool mTrace;

};


//////////////////////////////////////////////////////////////////////
//
// AU View Classes
//
//////////////////////////////////////////////////////////////////////



#endif
