/*
 * Constants needed both for the resource and code compilation.
 */

#ifndef BASE_AUDIO_UNIT_CONSTANTS_H
#define BASE_AUDIO_UNIT_CONSTANTS_H

//////////////////////////////////////////////////////////////////////
//
// kBaseAudioUnitVersion
//
//////////////////////////////////////////////////////////////////////

/*
 * The examples I looked at used a version constant with
 * two possible values depending if DEBUG was asserted.
 * Don't know why, maybe to make sure you could deploy 
 * and use both, or check for version mismatches between
 * the unit and view?
 *
 * This is used in the resource file to define the
 * VERSION constant used by AUResources.r when building
 * the resource structure.  
 */

#ifdef DEBUG
	#define kBaseAudioUnitVersion 0xFFFFFFFF
#else
	#define kBaseAudioUnitVersion 0x00010000	
#endif

//////////////////////////////////////////////////////////////////////
//
// kBaseAudioUnitManufacturer
//
//////////////////////////////////////////////////////////////////////

/*
 * The "manufacturer code" must be registered as a "creator code" 
 * at this Apple site:
 * 
 *     http://developer.apple.com/datatype/
 *
 * It may be the same for all components produced by a manufacturer.
 * I registered "Zmob" 9/15/2008
 * ---
 * Company:  Zone Mobius
 * First Name:  Jeffrey
 * Last Name:  Larson
 * Address:  10305 Skyflower Drive
 * City:  Austin
 * State:  TX
 * Postal/Zip Code:  78759
 * Country:  United States
 * 
 * Phone Number:  512-2580543
 * Email Address:  jeff@zonemobius.com
 * Application:  Mobius
 * 
 * Application Signatures:
 * ASCII: Zmob  HEX: 5A6D6F62
 * ---
 *
 * COMPONENT TYPES
 *
 * kAudioUnitType_MusicDevice
 *    softsynth, receives MIDI, sends audio
 *
 * kAudioUnitType_Effect
 *    send/receive audio
 * 
 * kAudioUnitType_MusicEffect
 *    send/recieve audio and receive MIDI
 *    this is closest to Mobius
 * 
 */

#define kBaseAudioUnitManufacturer 'Zmob'

//////////////////////////////////////////////////////////////////////
//
// kBaseAudioUnitSubType
//
//////////////////////////////////////////////////////////////////////

/*
 * This is the subtype that the MultitapDelay example used.
 * I don't see it in the include files.
 * This doesn't seem to be necessary, docs are very vague.
 * Not sure what would make use of this.
 * Used by both BaseAudioUnit.r and BaseAudioUnit.cpp
 */
#define kBaseAudioUnitSubType 'asmd'

#endif
