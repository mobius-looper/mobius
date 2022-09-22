/*
 * A simplified version of the standard AUResources.r file
 * from the SDK distro.  Removes all the platform conditionalization shit.
 *
 * I'm actually not using this right now, I figured out the screwy
 * "i386_YES=1" convention for the resource compiler.
 */

/* sample macro definitions -- all of these symbols must be defined
#define RES_ID			kHALOutputResID
#define COMP_TYPE		kAudioUnitComponentType
#define COMP_SUBTYPE	kAudioUnitOutputSubType
#define COMP_MANUF		kAudioUnitAudioHardwareOutputSubSubType
#define VERSION			0x00010000
#define NAME			"AudioHALOutput"
#define DESCRIPTION		"Audio hardware output AudioUnit"
#define ENTRY_POINT		"AUHALEntry"
*/

#define UseExtendedThingResource 1

#include <CoreServices/CoreServices.r>

// this is a define used to indicate that a component has no static data that would mean 
// that no more than one instance could be open at a time - never been true for AUs
#ifndef cmpThreadSafeOnMac
#define cmpThreadSafeOnMac	0x10000000
#endif

#define TARGET_REZ_USE_DLLE		1

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

resource 'STR ' (RES_ID, purgeable) {
	NAME
};

resource 'STR ' (RES_ID + 1, purgeable) {
	DESCRIPTION
};

resource 'dlle' (RES_ID) {
	ENTRY_POINT
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

resource 'thng' (RES_ID, NAME) {
	COMP_TYPE,
	COMP_SUBTYPE,
	COMP_MANUF,
	0, 0, 0, 0,								//	no 68K
	'STR ',	RES_ID,
	'STR ',	RES_ID + 1,
	0,	0,			/* icon */
	VERSION,
	componentHasMultiplePlatforms | componentDoAutoVersion,
	0,
	{
		cmpThreadSafeOnMac, 
		'dlle', 
		RES_ID,
		platformPowerPCNativeEntryPoint,
		cmpThreadSafeOnMac, 
		Target_CodeResType, RES_ID,
		platformIA32NativeEntryPoint,
	}
};

#undef RES_ID
#undef COMP_TYPE
#undef COMP_SUBTYPE
#undef COMP_MANUF
#undef VERSION
#undef NAME
#undef DESCRIPTION
#undef ENTRY_POINT
