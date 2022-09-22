/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * A model for conveying internal Mobius state to the UI.
 *
 */

#ifndef MOBIUS_STATE
#define MOBIUS_STATE

// unfortunately now have dependencies on SyncSource and SyncUnit
#include "Setup.h"

/**
 * Maximum number of events we'll return in a LoopState.
 */
#define MAX_INFO_EVENTS 10

/**
 * Maximum number of layers we'll return in a LoopState.
 */
// Crank these down temporarily for testing
#define MAX_INFO_LAYERS 10

/**
 * Maximum number of redo layers we'll return in a LoopState.
 */
#define MAX_INFO_REDO_LAYERS 10

/**
 * Maximum number of LoopSummary elements in TrackState.
 */
#define MAX_INFO_LOOPS 8

/**
 * Structure found in LoopState that describes a scheduled event.
 * Can't return pointers to the actual events because those may
 * be being processed by the interrupt thread at the same time as the 
 * display thread is examining them.
 */
class EventSummary {
  public:

    EventSummary() {}
    ~EventSummary() {}

    class EventType* type;
    class Function* function;
    long      frame;
    long      argument;
       
};

/**
 * Structure found in LoopState that describes a layer.
 */
class LayerState {
  public:

    LayerState() {}
    ~LayerState() {}

	bool checkpoint;

};

/**
 * State maintained by each Loop for consumption by the UI.
 * Most of this will be updated at the end of each interrupt by 
 * the Loop, with the event and layer lists being updated when 
 * things change.  This structure may be directly accessible by the UI
 * thread, so changes must be made with the assumption that the UI
 * thread may be reading it at that moment.  We're not going to introduce
 * a csect around this so there may be some temporary inconsistencies.
 *
 * This avoids having to have csect protection around the event lists
 * and layer lists within each loop.  Instead the loop keeps a copy of its
 * internal state refreshed here. 
 */
class LoopState {

  public:

	LoopState();
	~LoopState();
    void init();

	int 	number;
    class MobiusMode* mode;
	bool	recording;
	bool	paused;
    long    frame;
	int		cycle;
	int 	cycles;
    long    frames;
    int     nextLoop;
    int     returnLoop;
	bool 	overdub;
	bool	mute;

    EventSummary events[MAX_INFO_EVENTS];
	int		eventCount;

    LayerState layers[MAX_INFO_LAYERS];
	int		layerCount;
	int 	lostLayers;

    LayerState redoLayers[MAX_INFO_REDO_LAYERS];
	int		redoCount;
	int 	lostRedo;

	bool 	beatLoop;
	bool	beatCycle;
	bool 	beatSubCycle;

    long    windowOffset;
    long    historyFrames;
};

/**
 * Smaller loop state structure used to convey the state of all loops
 * in a track, not just the active one.  The cycles field can be used
 * to tell if the loop is empty.  The mode fields will be true only
 * if the various "follow modes" will not force it off when 
 * the loop is triggered.
 *
 * Speed and pitch state don't have actual values, they're true
 * if there is some amount of change being applied so they
 * can be rendered differently in the loop list.  Not sure
 * I like this...
 */
class LoopSummary {
  public:

	long frames;
	int cycles;
	bool active;
	bool pending;
	bool reverse;
	bool speed;
	bool pitch;
    bool mute;  // only meaningful for the selected track

};

/**
 * Class used to convey runtime state information to the UI.
 * One of these will be maintained by each Track and may be requsted
 * with Mobius::getState.  The UI can assume that the same object
 * will be returned for each track, but some things may be updated 
 * live during the interrupt handler.
 * 
 * Think about turning this around and having the Mobius push one
 * of these to the listener whenever something interesting changes.
 * Other than the beat counters, that would probably be more effecient.
 */
class TrackState {

  public:

	TrackState();
	~TrackState();
    void init();
	
	/**
	 * Track number (zero based)
	 */
    int number;

    /**
     * Track name.  This will be a direct reference to the
     * character array maintained within the Track.  Should be safe?
     */
    char* name;

	/**
	 * Current preset.
	 * This will be a private copy of the preset from the MobiusConfig.
	 * but there are still potential race conditions on the structure!!
	 */
	class Preset*	preset;

	/**
	 * Number of loops (should match the Preset)
	 */
	int 	loops;

	// Stream state
	int     inputMonitorLevel;
	int     outputMonitorLevel;
	int		inputLevel;
	int 	outputLevel;
	int 	feedback;
	int 	altFeedback;
	int 	pan;
    int     speedToggle;
    int     speedOctave;
    int     speedStep;
    int     speedBend;
    int     pitchOctave;
    int     pitchStep;
    int     pitchBend;
    int     timeStretch;
	bool	reverse;
	bool	focusLock;
    bool    solo;
    bool    globalMute;
    bool    globalPause;
	int		group;

	// Sync state
	// Tracks can't have different tempos, but it's convenient
	// to put global things in here too.
    SyncSource syncSource;
    SyncUnit syncUnit;
	float   tempo;
	int 	beat;
	int 	bar;
	bool    outSyncMaster;
	bool    trackSyncMaster;

	/**
	 * State of the active loop.	
	 */
	LoopState *loop;

	/**
	 * State summary for all loops.
	 */
	LoopSummary summaries[MAX_INFO_LOOPS];
	int summaryCount;

};

/**
 * Maximum length of the custom mode string returned in TrackState.
 */
#define MAX_CUSTOM_MODE 80

/**
 * Class used to convey overall runtime state information to the UI.
 * One of these will be maintained by the Mobius instance, with the
 * TrackState set to the state for the active track.
 */
class MobiusState {

  public:

	MobiusState();
	~MobiusState();
    void init();

	/**
	 * Currently selected configuration.	
	 * !! Race condition on the reference, can we just store the name?
	 */
	class BindingConfig* bindings;
    
	/**
	 * Custom mode name.
	 */
	char customMode[MAX_CUSTOM_MODE];

	/**
	 * True when the global recorder is on.
	 */
	bool globalRecording;

	// TODO: Capture global variables here, or have the UI pull
	// them one at a time?

	/**
	 * State of the selected track.
	 */
	TrackState* track;

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
