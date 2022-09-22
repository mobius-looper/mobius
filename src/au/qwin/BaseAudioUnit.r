/*
 * Resource definitions for the base Audio Unit.
 * We define a set of constants and include the
 * file AUResources.r twice to define the resources.
 * 
 */

// in /System/Library/Frameworks/AudioUnit.framework/Versions/A/Headers
// defines k constants for types and subtypes
#include <AudioUnit/AudioUnit.r>
#include <AudioUnit/AudioUnitCarbonView.r>

// for the definition of kBaseAudioUnitVersion and kBaseAudioUnitManufacdturer
#include "BaseAudioUnitConstants.h"

// 
// Definitions common to both the core unit and the view.
// 

#define COMP_MANUF		kBaseAudioUnitManufacturer
#define VERSION			kBaseAudioUnitVersion
#define COMP_SUBTYPE		kBaseAudioUnitSubType

//
// Definitions for the core unit
// MusicEffect is necessary to receive both MIDI and audio

#define RES_ID			1000
#define COMP_TYPE		kAudioUnitType_MusicEffect
#define NAME			"Zone Mobius: Example Audio Unit"
#define DESCRIPTION		"Zone Mobius Example Audio Unit"
#define ENTRY_POINT		"BaseAudioUnitEntry"

#include "../CoreAudio/AudioUnits/AUPublic/AUBase/AUResources.r"

// 
// Definition for the unit view
// This one uses a view shim

// I don't understand why but we have to define these again...
#define COMP_MANUF		kBaseAudioUnitManufacturer
#define VERSION			kBaseAudioUnitVersion
#define COMP_SUBTYPE		kBaseAudioUnitSubType

// these are different
#define RES_ID			2000
#define COMP_TYPE		kAudioUnitCarbonViewComponentType
#define NAME			"Base Audio Unit View"
#define DESCRIPTION		"Base Audio Unit View - a description goes here"
#define ENTRY_POINT		"BaseAudioUnitViewEntryShim"

#include "../CoreAudio/AudioUnits/AUPublic/AUBase/AUResources.r"


