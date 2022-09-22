/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Resource definitions for the Mobius Audio Unit.
 * We define a set of constants and include the
 * file AUResources.r twice to define the resources.
 * 
 */

// in /System/Library/Frameworks/AudioUnit.framework/Versions/A/Headers
// defines k constants for types and subtypes
#include <AudioUnit/AudioUnit.r>
#include <AudioUnit/AudioUnitCarbonView.r>

#include "AUMobiusConstants.h"

// 
// Definitions common to both the core unit and the view.
// 

#define COMP_MANUF		kAUMobiusManufacturer
#define VERSION			kAUMobiusVersion
#define COMP_SUBTYPE		kAUMobiusSubType

//
// Definitions for the core unit
// MusicEffect is necessary to receive both MIDI and audio

#define RES_ID			1000
#define COMP_TYPE		kAudioUnitType_MusicEffect
#define NAME			"Circular Labs: Mobius 2"
#define DESCRIPTION		"Circular Labs: Mobius 2"
#define ENTRY_POINT		"AUMobiusEntry"

#include "../au/CoreAudio/AudioUnits/AUPublic/AUBase/AUResources.r"

// 
// Definition for the unit view
//

// I don't understand why but we have to define these again...
#define COMP_MANUF		kAUMobiusManufacturer
#define VERSION			kAUMobiusVersion
#define COMP_SUBTYPE		kAUMobiusSubType

// these are different
#define RES_ID			2000
#define COMP_TYPE		kAudioUnitCarbonViewComponentType
#define NAME			"AUMobius 2 View"
#define DESCRIPTION		"AUMobius 2 View"
#define ENTRY_POINT		"AUMobiusViewEntry"

#include "../au/CoreAudio/AudioUnits/AUPublic/AUBase/AUResources.r"
