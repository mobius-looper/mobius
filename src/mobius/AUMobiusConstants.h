/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Resource constants for the Mobius AU plugin.
 *
 * NOTE: I don't know why exactly but it is important
 * that we have a unique SubType within the manufacturer space.
 * BaseAudioUnit uses "asmd", Mobius needs something else.
 *
 */

#ifndef AU_MOBIUS_CONSTANTS
#define AU_MOBIUS_CONSTANTS

/**
 *  kAUMobiusVersion
 * 
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
	#define kAUMobiusVersion 0xFFFFFFFF
#else
	#define kAUMobiusVersion 0x00010000	
#endif

/**
 * kAUMobiusManufacturer
 * 
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
 * Registered "Circ" on 3/18/2012
 * Same info, email jeff@circularlabs.com
 * ASCII: Circ  HEX: 43697263
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
#define kAUMobiusManufacturer 'Circ'

/**
 * kAUMobiusSubType
 *
 * Docs are very vague on what this is for.  It doesn't seem
 * to have any meaning other than it must be unique for 
 * all plugins within a Manufacturer.
 */
#define kAUMobiusSubType 'mobi'

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
