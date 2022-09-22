/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * A representation of the runtime state of a Mobius instance, 
 * including audio data.  This allows Mobius state to be saved to
 * and restored from files.
 *
 */

#ifndef PROJECT_H
#define PROJECT_H

/****************************************************************************
 *                                                                          *
 *   							   PROJECT                                  *
 *                                                                          *
 ****************************************************************************/

class ProjectSegment {

  public:

    ProjectSegment();
    ProjectSegment(class MobiusConfig* config, class Segment* src);
    ProjectSegment(XmlElement* e);
    ~ProjectSegment();

	void setOffset(long i);
	long getOffset();

	void setStartFrame(long i);
	long getStartFrame();

	void setFrames(long i);
	long getFrames();
	
	void setLayer(int id);
	int getLayer();

	void setFeedback(int i);
	int getFeedback();

	void toXml(XmlBuffer* b);
	void parseXml(XmlElement* e);

	void setLocalCopyLeft(long frames);
	long getLocalCopyLeft();
	
	void setLocalCopyRight(long frames);
	long getLocalCopyRight();

	class Segment* allocSegment(class Layer* layer);

  private:

    void init();

	long mOffset;
	long mStartFrame;
	long mFrames;
	int  mFeedback;
	int  mLayer;
	long mLocalCopyLeft;
	long mLocalCopyRight;

};

class ProjectLayer {

  public:

    ProjectLayer();
    ProjectLayer(XmlElement* e);
    ProjectLayer(class MobiusConfig* config, class Project* p, class Layer* src);
    ProjectLayer(Audio* src);
    ~ProjectLayer();

	int getId();

	void setCycles(int i);
	int getCycles();

    void setAudio(Audio* a);
    Audio* getAudio();
    Audio* stealAudio();
    void setOverdub(Audio* a);
    Audio* getOverdub();
    Audio* stealOverdub();

	void setBuffers(int i);
	int getBuffers();

    void setPath(const char* path);
    const char* getPath();
    void setOverdubPath(const char* path);
    const char* getOverdubPath();

    void setProtected(bool b);
    bool isProtected();

	void add(ProjectSegment* seg);

	void writeAudio(const char* baseName, int tracknum, int loopnum, 
					int layernum);
	void toXml(XmlBuffer* b);
	void parseXml(XmlElement* e);

	void setDeferredFadeLeft(bool b);
	bool isDeferredFadeLeft();
	void setDeferredFadeRight(bool b);
	bool isDeferredFadeRight();
	void setReverseRecord(bool b);
	bool isReverseRecord();

	Layer* getLayer();
	Layer* allocLayer(class LayerPool* pool);
	void resolveLayers(Project* p);

  private:

    void init();

    /**
     * This is the unique layer number generated for debugging.
     * It is not currently included in the project XML because it's
     * hard to explain and not really necessary.
     */
    int mId;

	int mCycles;
	class List* mSegments;
    Audio* mAudio;
	Audio* mOverdub;
    char* mPath;
    char* mOverdubPath;
    bool mProtected;
	bool mDeferredFadeLeft;
	bool mDeferredFadeRight;
    bool mContainsDeferredFadeLeft;
    bool mContainsDeferredFadeRight;
	bool mReverseRecord;

	/**
	 * True if the mAudio and mOverdub objects are owneed by
	 * the active Layer rather than by the Project.  Should only
	 * be true when saving the active project.
	 */
	bool mExternalAudio;

	/**
	 * Transient, set during project loading.
	 * Segments can reference layers by id, and the layers can appear
	 * anywhere in the project hiearchy in any order.  To resolve references
	 * to layers, we'll first make a pass over the project allocating
	 * Layer objects for each ProjectLayer and attaching them here.
	 * Then we'll make another pass to flesh out the Segment lists
	 * resolving to these Layer objects.	
	 */
	Layer* mLayer;

};

class ProjectLoop {

  public:

	ProjectLoop();
	ProjectLoop(XmlElement* e);
	ProjectLoop(class MobiusConfig* config, class Project* proj, 
				class Loop* loop);
	~ProjectLoop();

	void add(ProjectLayer* a);
	class List* getLayers();
	Layer* findLayer(int id);
	void allocLayers(class LayerPool* pool);
	void resolveLayers(Project* p);

	void setFrame(long f);
	long getFrame();

    void setNumber(int i);
    int getNumber();

	void setActive(bool b);
	bool isActive();

	void writeAudio(const char* baseName, int tracknum, int loopnum);
	void toXml(XmlBuffer* b);
	void parseXml(XmlElement* e);

  private:

	void init();

    /**
     * Ordinal number of this loop from zero.
     * This is used only for incremental projects where each track
     * and loop must specify the target number.
     */
    int mNumber;

	/**
	 * A list of ProjectLayer objects representing the layers of this loop.
	 */
	class List* mLayers;

	/**
	 * The frame at the time of capture.
	 */
	long mFrame;

	/**
	 * True if this was the active loop at the time of capture.
	 */
	bool mActive;

    // TODO: If they're using "restore" transfer modes we should
    // save the speed and pitch state for each loop.

};

class ProjectTrack {

  public:

	ProjectTrack();
	ProjectTrack(XmlElement* e);
	ProjectTrack(class MobiusConfig* config, class Project* proj, 
				 class Track* track);
	~ProjectTrack();

    void setNumber(int n);
    int getNumber();

	void setActive(bool b);
	bool isActive();

    void setGroup(int i);
    int getGroup();

	void setFocusLock(bool b);
	bool isFocusLock();

	void setPreset(const char* preset);
	const char* getPreset();

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

	void setReverse(bool b);
	bool isReverse();

    void setSpeedOctave(int i);
    int getSpeedOctave();

    void setSpeedStep(int i);
    int getSpeedStep();

    void setSpeedBend(int i);
    int getSpeedBend();

    void setSpeedToggle(int i);
    int getSpeedToggle();

    void setPitchOctave(int i);
    int getPitchOctave();

    void setPitchStep(int i);
    int getPitchStep();

    void setPitchBend(int i);
    int getPitchBend();

    void setTimeStretch(int i);
    int getTimeStretch();

	void add(ProjectLoop* l);
	class List* getLoops();

	void setVariable(const char* name, ExValue* value);
	void getVariable(const char* name, ExValue* value);

	Layer* findLayer(int id);
	void allocLayers(class LayerPool* pool);
	void resolveLayers(Project* p);

	void writeAudio(const char* baseName, int tracknum);
	void toXml(XmlBuffer* b);
	void toXml(XmlBuffer* b, bool isTemplate);
	void parseXml(XmlElement* e);


  private:

	void init();

    /**
     * Ordinal number of this loop from zero.
     * This is used only for incremental projects where each track
     * and loop must specify the target number.
     */
    int mNumber;

	/**
	 * The name of the preset used in this track (if different
	 * than the Setup).
	 */
	char* mPreset;

	// state at the time of the project snapshot, may be different
	// than the state in the Setup
	bool mActive;
	bool mFocusLock;
    int mGroup;
	int mInputLevel;
	int mOutputLevel;
	int mFeedback;
	int mAltFeedback;
	int mPan;

	bool mReverse;
    int mSpeedOctave;
    int mSpeedStep;
    int mSpeedBend;
    int mSpeedToggle;
    int mPitchOctave;
    int mPitchStep;
    int mPitchBend;
    int mTimeStretch;

	/**
	 * A list of ProjectLoop objects representing the loops
	 * in this track.  The length of the list is not necessarily the
	 * same as the MoreLoops parameter in the Mobius you are loading
	 * it into.  If it is less, empty loops are added, if it is more,
	 * MoreLoops is increased.
	 */
	class List* mLoops;
	
	/**
	 * User defined variable saved with the track.
	 */
	class UserVariables* mVariables;

};

/**
 * An object representing a snapshot of Mobius audio data and other
 * settings.  This may be as simple as a single .wav file for
 * the current loop, or as complicated as 8 tracks of 8 loops
 * with unlimited undo layers.
 *
 * NOTE: There are many relatively unusual things that are not 
 * saved in the project such as input and output port overrides.
 * Potentially everything that is in the Setup needs to be
 * in the ProjectTrack since it may be overridden.
 *
 * There are also lost of loop modes that aren't being saved
 * such as rate and pitch shift, mute mode, etc.  
 */
class Project {

  public:

	Project();
	Project(XmlElement* e);
	Project(const char* file);
	Project(Audio* a, int trackNumber, int loopNumber);
	~Project();

	void setNumber(int i);
	int getNumber();
	
	//
	// Audio hierarchy specification
	//

	void clear();
	Layer* findLayer(int id);
	void resolveLayers(class LayerPool* pool);
	void setTracks(Mobius* m);
	void add(ProjectTrack* t);
	class List* getTracks();

	void setVariable(const char* name, ExValue* value);
	void getVariable(const char* name, ExValue* value);

	void setBindings(const char* name);
	const char* getBindings();

	void setSetup(const char* name);
	const char* getSetup();

    // callers must check isError 
    void read();
	void read(class AudioPool* pool);
    void write();
	void write(const char* file, bool isTemplate);

    void setPath(const char* path);
    const char* getPath();

	//
	// Load options
	//

	void setIncremental(bool b);
	bool isIncremental();

	//
	// Save options
	//

	//void setIncludeUndoLayers(bool b);
	//bool isIncludeUndoLayers();

	//
	// Transient save/load state
	//

	bool isError();
	const char* getErrorMessage();
	void setErrorMessage(const char* msg);
	int getNextLayerId();
	
	bool isFinished();
	void setFinished(bool b);

    void deleteAudioFiles();

	void toXml(XmlBuffer* b);
	void toXml(XmlBuffer* b, bool isTemplate);
	void parseXml(XmlElement* e);

  private:

	void init();
    void read(class AudioPool* pool, const char* file);
	void readAudio(class AudioPool* pool);
	void writeAudio(const char* baseName);

	//
	// Persistent fields
	//

	/**
	 * Projects that can be referenced as VST parameters must
	 * have a unique number.
     * !! Huh?  I don't think this ever worked, you can't ref
     * projects as VST parameters, really?
	 */
	int mNumber;

    /**
     * The file we were loaded from or will save to.
     */
    char* mPath;

	/**
	 * A list of ProjectTrack objects.
	 */
	class List* mTracks;

	/**
	 * User defined global variables.
     * Might want these to move these to the Setup...
	 */
	class UserVariables* mVariables;

	/**
	 * Currently selected binding overlay.
	 */
	char* mBindings;

	/**
	 * Currently selected track setup.	
	 */
	char* mSetup;

	//
	// Runtime fields
	//

	/**
	 * Used to generate unique layer ids for segment references.
	 */
	int mLayerIds;

	/**
	 * Set during read() if an error was encountered.
	 */
	bool mError;

	/**
	 * Set during read() if an error was encountered.
	 */
	char mMessage[1024 * 2];

	/**
	 * When true, the project is incrementally merged with existing
	 * tracks rather than resetting all tracks first.
	 */
	bool mIncremental;

    /**
     * When true, layer Audio will loaded with the project.
     * WHen false, only the path name to the layer Audio file
     * is loaded.
     */
    bool mIncludeAudio;


	//
	// Transient parse state
	//

	FILE* mFile;
	char mBuffer[1024];
	char** mTokens;
	int mNumTokens;

	//
	// Transient save state

	/**
	 * Set by the interrupt handler when the state of the project
	 * has been captured.
	 */
	bool mFinished;


};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif

