/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Model for a "track setup", a collection of parameters that apply to 
 * all tracks.
 *
 */

#ifndef SETUP_H
#define SETUP_H

#include "Binding.h"

/****************************************************************************
 *                                                                          *
 *                                 CONSTANTS                                *
 *                                                                          *
 ****************************************************************************/

/**
 * Root XML element name.
 */
#define EL_SETUP "Setup"

/**
 * A special name that may be used for the Bindings property that
 * means to cancel the current binding overlay.  Normally a NULL value
 * here means "preserve the current overlay".
 */
#define SETUP_OVERLAY_CANCEL "cancel"

/**
 * Default number of tracks in a setup.
 */
#define DEFAULT_TRACK_COUNT 8

/****************************************************************************
 *                                                                          *
 *                                ENUMERATIONS                              *
 *                                                                          *
 ****************************************************************************/

/**
 * An eumeration defining the possible synchronization sources.
 * This is what older releases called SyncMode.
 * DEFAULT is only a valid value in SetupTrack, it will never be seen
 * in a SyncState.
 */
typedef enum {

    SYNC_DEFAULT,
    SYNC_NONE,
    SYNC_TRACK,
    SYNC_OUT,
    SYNC_HOST,
    SYNC_MIDI

} SyncSource;

extern const char* GetSyncSourceName(SyncSource src);

/**
 * Defines the granularity of MIDI and HOST quantization.
 * While it's just a boolean now, keep it open for more options.
 * It would be nice if we could merge this with SyncTrackUnit but
 * then it would be messy to deal with subranges.
 */
typedef enum {

    SYNC_UNIT_BEAT,
    SYNC_UNIT_BAR

} SyncUnit;

/**
 * Defines the granularity of SYNC_TRACK quantization.
 * DEFAULT is only a valid value in SetupTrack, it will never be seen
 * in a SyncState.
 */
typedef enum {

    TRACK_UNIT_DEFAULT,
    TRACK_UNIT_SUBCYCLE,
    TRACK_UNIT_CYCLE,
    TRACK_UNIT_LOOP

} SyncTrackUnit;

/**
 * Defines what happens when muting during SYNC_OUT
 */
typedef enum {

    MUTE_SYNC_TRANSPORT,
    MUTE_SYNC_TRANSPORT_CLOCKS,
    MUTE_SYNC_CLOCKS,
    MUTE_SYNC_NONE

} MuteSyncMode;

/**
 * Defines what happens to the SYNC_OUT tempo when various
 * changes are made to the sync master track
 */
typedef enum {

    SYNC_ADJUST_NONE,
    SYNC_ADJUST_TEMPO

} SyncAdjust;

/**
 * Defines when a Realign function is performed.
 * START is the most common and means that the realign will be performed
 * on the next pulse that represents the "external start point" of the
 * sync loop.  BAR and BEAT can be used if you want the realign
 * to happen at a smaller yet still musically significant granule.
 * 
 * NOW means the realign will happen as soon as possible.  For
 * SYNC_TRACK it will happen immediately, for other sync modes it will
 * happen on the next pulse.  Note that SYNC_HOST since a pulse is the same
 * as a beat, REALIGN_NOW will behave the same as REALIGN_BEAT.
 */
typedef enum {

    REALIGN_START,
    REALIGN_BAR,
    REALIGN_BEAT,
    REALIGN_NOW

} RealignTime;

/**
 * Defines out SYNC_OUT Realign is performed.
 */
typedef enum {

    REALIGN_MIDI_START,
    REALIGN_RESTART

} OutRealignMode;

/****************************************************************************
 *                                                                          *
 *   								SETUP                                   *
 *                                                                          *
 ****************************************************************************/

class Setup : public Bindable {

  public:

    Setup();
    Setup(class XmlElement* e);
    void reset(class Preset* p);
	Setup* clone();
    ~Setup();

    Bindable* getNextBindable();
	class Target* getTarget();
	void select();

	void setNext(Setup* p);
	Setup* getNext();

	void setBindings(const char* name);
	const char* getBindings();

	void setActiveTrack(int i);
	int getActiveTrack();

	void setResetables(class StringList* list);
	class StringList* getResetables();
	bool isResetable(class Parameter* p);

	class SetupTrack* getTracks();
	class SetupTrack* stealTracks();
	void setTracks(SetupTrack* list);
	SetupTrack* getTrack(int index);

    // Sync options

    SyncSource getSyncSource();
    void setSyncSource(SyncSource src);

    SyncUnit getSyncUnit();
    void setSyncUnit(SyncUnit unit);

    SyncTrackUnit getSyncTrackUnit();
    void setSyncTrackUnit(SyncTrackUnit unit);

    bool isManualStart();
    void setManualStart(bool b);

	void setMinTempo(int t);
	int getMinTempo();
	void setMaxTempo(int t);
	int getMaxTempo();
	void setBeatsPerBar(int t);
	int getBeatsPerBar();

	void setMuteSyncMode(MuteSyncMode m);
	void setMuteSyncMode(int m);
	MuteSyncMode getMuteSyncMode();

	void setResizeSyncAdjust(SyncAdjust i);
	void setResizeSyncAdjust(int i);
	SyncAdjust getResizeSyncAdjust();

	void setSpeedSyncAdjust(SyncAdjust i);
	void setSpeedSyncAdjust(int i);
	SyncAdjust getSpeedSyncAdjust();

	void setRealignTime(RealignTime sm);
	void setRealignTime(int i);
	RealignTime getRealignTime();

	void setOutRealignMode(OutRealignMode m);
	void setOutRealignMode(int i);
	OutRealignMode getOutRealignMode();

	char* toXml();
	void toXml(XmlBuffer* b);

  private:

	void init();
	void initParameters();
	void parseXml(XmlElement* e);

	/**
	 * Next setup in the chain.
	 */
	Setup* mNext;

	/**
	 * Currently active track.
	 */
	int mActive;

	/**
	 * User defined group names. (not used yet)
	 */
	//StringList* mGroupNames;

	/**
	 * List of track parameter names that will be restored from the
	 * setup after an individual (non-global) reset.
	 */
	class StringList* mResetables;

	/**
	 * A list of track configurations.
	 */
	class SetupTrack* mTracks;

	/**
	 * Currently overlay BindingConfig.
	 */
	char* mBindings;

    //
    // Synchronization
    //

    /**
     * The default sync source for all tracks.
     */
    SyncSource mSyncSource;

    /**
     * The sync unit for MIDI and HOST.
     * Tracks can't override this, should they be able to?
     */
    SyncUnit mSyncUnit;

    /**
     * The default track sync unit for all tracks.
     */
    SyncTrackUnit mSyncTrackUnit;

    /**
     * True if an MS_START message should be sent manually rather
     * than automatically when the loop closes during SYNC_OUT.
     * Formerly SyncMode=OutUserStart
     */
    bool mManualStart;

	/**
	 * The minimum tempo we will try to use in SYNC_OUT.
	 */
	int mMinTempo;

	/**
	 * The maximum tempo we will try to use in SYNC_OUT.
	 */
	int mMaxTempo;

    /**
     * The number of beats in a bar.
     * If not set we'll attempt to get it from the host when host
     * syncing, then fall back to the Preset::8thsPerCycle.
     *
     * This is used in SYNC_OUT to define the cycle size (really?)
     * This is used in SYNC_MIDI to define the bar widths for
     * quantization.
     */
    int mBeatsPerBar;

    /**
     * During SYNC_OUT, determines how MIDI transport and clocks 
     * are sent during mute modes Start and Pause.
     */
    MuteSyncMode mMuteSyncMode;

    /**
     * During SYNC_OUT, specifies how unrounded cycle resizes are
     * supposed to be processed.
     */
    SyncAdjust mResizeSyncAdjust;

	/**
	 * During SYNC_OUT, specifies how rate changes are handled.
	 */
	SyncAdjust mSpeedSyncAdjust;

	/**
	 * Controls the time at which a Realign occurs.
	 */
	RealignTime mRealignTime;

	/**
	 * When using SYNC_OUT, determines how to realign the external sync loop.
	 */
	OutRealignMode mOutRealignMode;


};

/****************************************************************************
 *                                                                          *
 *                                SETUP TRACK                               *
 *                                                                          *
 ****************************************************************************/


/**
 * The state of one track in a Setup.
 */
class SetupTrack {

  public:

	SetupTrack();
	SetupTrack(XmlElement* e);
	~SetupTrack();
	SetupTrack* clone();
	void reset();
	void capture(class MobiusState* state);
	void toXml(XmlBuffer* b);

	void setNext(SetupTrack* n);
	SetupTrack* getNext();

	void setName(const char* name);
	const char* getName();

	void setPreset(const char* preset);
	const char* getPreset();

    void setGroup(int i);
    int getGroup();

	void setFocusLock(bool b);
	bool isFocusLock();

	void setInputLevel(int i);
	int getInputLevel();

	void setOutputLevel(int i);
	int getOutputLevel();

	void setFeedback(int i);
	int getFeedback();

	void setAltFeedback(int i);
	int getAltFeedback();

	void setPan(int i);
	int getPan();

	void setMono(bool b);
	bool isMono();

	void setAudioInputPort(int i);
	int getAudioInputPort();
	
	void setAudioOutputPort(int i);
	int getAudioOutputPort();

	void setPluginInputPort(int i);
	int getPluginInputPort();
	
	void setPluginOutputPort(int i);
	int getPluginOutputPort();

    SyncSource getSyncSource();
    void setSyncSource(SyncSource src);
    SyncTrackUnit getSyncTrackUnit();
    void setSyncTrackUnit(SyncTrackUnit unit);

	void setVariable(const char* name, class ExValue* value);
	void getVariable(const char* name, class ExValue* value);

  private:

	void parseXml(XmlElement* e);
	void init();

	SetupTrack* mNext;
	char* mName;
	char* mPreset;
	bool mFocusLock;
	bool mMono;
    int mGroup;
	int mInputLevel;
	int mOutputLevel;
	int mFeedback;
	int mAltFeedback;
	int mPan;
	int mAudioInputPort;
	int mAudioOutputPort;
	int mPluginInputPort;
	int mPluginOutputPort;

    // Sync overrides

    SyncSource mSyncSource;
    SyncTrackUnit mSyncTrackUnit;

	/**
	 * User defined variable saved with the track.
	 */
	class UserVariables* mVariables;
};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
