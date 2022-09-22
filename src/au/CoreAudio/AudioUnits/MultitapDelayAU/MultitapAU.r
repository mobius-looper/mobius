#include <AudioUnit/AudioUnit.r>
#include <AudioUnit/AudioUnitCarbonView.r>
#include "AUMultitapVersion.h"

// ____________________________________________________________________________
// component resources for Audio Unit
#define RES_ID			1000
#define COMP_TYPE		kAudioUnitType_Effect
#define COMP_SUBTYPE	'asmd'
#define COMP_MANUF		'Acme'
#define VERSION			kAUMultitapVersion
#define NAME			"Acme: Sample Multitap Delay"
#define DESCRIPTION		"Acme's sample multitap delay unit"
#define ENTRY_POINT		"MultitapAUEntry"

#include "CoreAudio/AudioUnits/AUPublic/AUBase/AUResources.r"

// ____________________________________________________________________________
// component resources for Audio Unit Carbon View
#define RES_ID			2000
#define COMP_TYPE		kAudioUnitCarbonViewComponentType
#define COMP_SUBTYPE	'asmd'
#define COMP_MANUF		'Acme'
#define VERSION			kAUMultitapVersion
#define NAME			"Sample Multitap Delay AUView"
#define DESCRIPTION		"Sample Multitap Delay AUView"
#define ENTRY_POINT		"MultitapAUViewEntryShim"

#include "CoreAudio/AudioUnits/AUPublic/AUBase/AUResources.r"
