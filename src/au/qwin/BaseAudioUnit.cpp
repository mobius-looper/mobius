
#include <AudioUnit/AudioUnit.h>
#include "AUEffectBase.h"

#include "Trace.h"
#include "BaseAudioUnit.h"

//////////////////////////////////////////////////////////////////////
//
// Component Entry Point
//
//////////////////////////////////////////////////////////////////////

/*
 * Not sure what this macro does, but it looks like it constructs
 * the given class and calls a method on it.
 */

COMPONENT_ENTRY(BaseAudioUnit)

//////////////////////////////////////////////////////////////////////
//
// Constructor/Info
//
//////////////////////////////////////////////////////////////////////

/**
 * Create a new BaseAudioUnit
 * !!TODO: extend AUMIDIEffectBase
 */
BaseAudioUnit::BaseAudioUnit(AudioUnit component)
	: AUEffectBase(component)
{

	// defined in AUPublic/AUBase.cpp
	// standard initialization for inputs, outputs, groups, and parts
	CreateElements();
	
	// convenience accessor defined in AUPublic/OtherBases/AUEffectBase
	// to set parameters in the global scope
	SetParameter(kParamRandomValue, 0);

	mTrace = true;

	// todo, need overridable fields for name etc....
	if (mTrace)
	  trace("BaseAudioUnit::BaseAudioUnit\n");
}

/**
 * Return the number of custom UI components
 * Examples show 1 for a Carbon UI.
 */
int BaseAudioUnit::GetNumCustomUIComponents () 
{
	// todo, need overridable fields for name etc....
	if (mTrace)
	  trace("BaseAudioUnit::GetNumCustomUIComponents\n");
	return 1; 
}

/**
 * Return info about our UI.
 */
void BaseAudioUnit::GetUIComponentDescs(ComponentDescription* inDescArray)
{
	if (mTrace)
	  trace("BaseAudioUnit::GetUIComponentDescs\n");

	inDescArray[0].componentType = kAudioUnitCarbonViewComponentType;
	inDescArray[0].componentSubType = kBaseAudioUnitSubType;
	inDescArray[0].componentManufacturer = kBaseAudioUnitManufacturer;
	inDescArray[0].componentFlags = 0;
	inDescArray[0].componentFlagsMask = 0;
}

//////////////////////////////////////////////////////////////////////
//
// Virtual Info/Setup Functions
//
//////////////////////////////////////////////////////////////////////

/**
 * Create a new AUKernelBase, this is where the Process method lives.
 * This is virtual.
 */
AUKernelBase* BaseAudioUnit::NewKernel()
{
	if (mTrace)
	  trace("BaseAudioUnit::NewKernel\n");

	return new BaseAudioUnitKernel(this, mTrace); 
}

/**
 * Return the version number.
 * This is virtual.
 */
ComponentResult BaseAudioUnit::Version()
{ 
	if (mTrace)
	  trace("BaseAudioUnit::Version\n");

	return kBaseAudioUnitVersion;
}

/**
 * Return true if we "support tail".
 * This is virtual.
 * Personally, I'm all for tail, not sure we're talking
 * about the same thing though.
 */
bool BaseAudioUnit::SupportsTail()
{
	if (mTrace)
	  trace("BaseAudioUnit::SupportsTail\n");

	return true; 
}

//////////////////////////////////////////////////////////////////////
// 
// ParameterInfo
//
//////////////////////////////////////////////////////////////////////

/**
 * This is virtual.
 */
ComponentResult BaseAudioUnit::GetParameterInfo(AudioUnitScope scope,
												AudioUnitParameterID id,
												AudioUnitParameterInfo& info)
{
	ComponentResult result = noErr;

	if (mTrace)
	  trace("BaseAudioUnit::GetParameterInfo %d\n", id);

	// seems to be an unconditional declaration of R/W
	info.flags =
		kAudioUnitParameterFlag_IsWritable +
		kAudioUnitParameterFlag_IsReadable;	
	

	// only interested in global scope
	if (scope != kAudioUnitScope_Global) {

		result = kAudioUnitErr_InvalidParameter;
	}
	else {

		switch(id) {

			case kParamRandomValue: {
				AUBase::FillInParameterName (info, CFSTR("Random Value"), false);

				// see AudioUnitProperties.h in 
				// /Sytem/Library/Frameworks/AudioUnit.frameworks/Headers
				// 
				// kAudioUnitParameterUnit_Generic 
				//   - untyped value generally between 0.0 and 1.0 
				// kAudioUnitParameterUnit_Boolean
				//   - 0.0 means FALSE, non-zero means TRUE
				// kAudioUnitParameterUnit_MIDIController
				//   - a generic MIDI controller value from 0 -> 127
				// kAudioUnitParameterUnit_CustomUnit
				//   - this is the parameter unit type for parameters that 
				//     present a custom unit name

				info.unit = kAudioUnitParameterUnit_MIDIController;
				info.minValue = 0.0;
				info.maxValue = 127.0;
				info.defaultValue = 0.0;
			}
			break;

			default: {
				result = kAudioUnitErr_InvalidParameter;
			}
			break;
		}
	}

	return result;
}


//////////////////////////////////////////////////////////////////////
//
// Kernel
//
//////////////////////////////////////////////////////////////////////

BaseAudioUnitKernel::BaseAudioUnitKernel(AUEffectBase *inAudioUnit, bool dotrace)
	: AUKernelBase(inAudioUnit)
{
	mTrace = dotrace;
	if (mTrace)
	  trace("BaseAudioUnitKernel::BaseAudioUnitKernel\n");
}


void BaseAudioUnitKernel::Reset()
{
	if (mTrace)
	  trace("BaseAudioUnit::Reset\n");
}

void BaseAudioUnitKernel::Process(const Float32* src, Float32* dest,
								  UInt32 frames, UInt32 channels,
								  bool& silence)
{
	// called WAY too much
	//if (mTrace)
	//trace("BaseAudioUnit::Process\n");
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
