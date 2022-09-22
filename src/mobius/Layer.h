/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * A model for a segment of audio.  The Loop class  deals with Layers, 
 *
 */

#ifndef LAYER_H
#define LAYER_H

#include "Trace.h"
#include "Audio.h"
#include "MobiusState.h"

/****************************************************************************
 *                                                                          *
 *   							LAYER CONTEXT                               *
 *                                                                          *
 ****************************************************************************/

/**
 * State that must be passed down through Loop to Layer.
 * Extends AudioBuffer so we can pass a buffer, adds various
 * options.
 *
 * Formerly known as AudioContext.  Try to factor this out further?
 */
class LayerContext : public AudioBuffer {

  public:

    LayerContext();
    virtual ~LayerContext();

    void init();

	void setReverse(bool b);
	bool isReverse();

	float getLevel();
	void setLevel(float f);

  private:

	/**
	 * True if we're in reverse mode.
	 */
	bool mReverse;

	/** 
	 * Level adjustment to apply.
	 */
	float mLevel;

};


/****************************************************************************
 *                                                                          *
 *                                   LAYER                                  *
 *                                                                          *
 ****************************************************************************/

/**
 * Checkpoint status needs tri-state logic, so we can properly
 * transfer modified state from the record layer to the play layer
 * but only if modified.
 */
typedef enum {

	CHECKPOINT_OFF,
	CHECKPOINT_ON,
	CHECKPOINT_UNSPECIFIED,

} CheckpointState;

class Layer : public TraceContext
{
    friend class LayerPool;
	friend class Segment;

  public:

	void reset();
	Layer* copy();
	void copy(Layer* src);
    void free();
    void freeAll();
    void freeUndo();

	// Basic accessors

	void setLoop(class Loop* l);
	Loop* getLoop();
	void setNumber(int i);
	int getNumber();
	void setAllocation(int i);
	int getAllocation();
	void setPrev(Layer* l);
	Layer* getPrev();
	void setRedo(Layer* l);
	Layer* getRedo();
    void setReferences(int i);
    void incReferences();
    int decReferences();
    int getReferences();

	void setIsolatedOverdub(bool b);
	bool isIsolatedOverdub();
    void setHistoryOffset(long offset);
    long getHistoryOffset();
    void setWindowing(bool b);
    bool isWindowing();
    void setWindowOffset(long offset);
    long getWindowOffset();
    void setWindowSubcycleFrames(long frames);
    long getWindowSubcycleFrames();

	int getFeedback();
	int getStartingFeedback();
	int lockStartingFeedback();

	bool isFeedbackApplied();
	void setPulseFrames(float f);
	float getPulseFrames();
	void setCycles(int i);
	int getCycles();
	void setFadeOverride(bool b);

	long getFrames();
	long getRecordedFrames();
    long getCycleFrames();
	float getMaxSample();
    bool isStructureChanged();
    bool isAudioChanged();
    bool isChanged();
    bool isNoFlattening();
	
	Audio* getAudio();
	Audio* getOverdub();
	Audio* flatten();

	CheckpointState getCheckpoint();
	bool isCheckpoint();
	void setCheckpoint(CheckpointState c);
	Layer* getPrevCheckpoint();
	Layer* getCheckpointTail();
	Layer* getTail();
	void getState(class LayerState* s);

	// Recording

	void record(LayerContext* con, long startFrame, int feedback);
	void advance(LayerContext* con, long startFrame, int feedback);
	void setFrames(LayerContext* con, long frames);
    void setPendingFrames(LayerContext* con, long frames, long pending);
	void pause(LayerContext* con, long startFrame);
	void finalize(LayerContext* con, Layer* next);

	void startInsert(LayerContext* con, long startFrame);
	void insert(LayerContext* con, long startFrame, int feedback);
	void continueInsert(LayerContext* con, long frame);
	void endInsert(LayerContext* con, long endFrame, bool unrounded);

	void multiplyCycle(LayerContext* con, Layer* src, long startFrame);
	void splice(LayerContext* con, long start, long frames, int cycles);
	void zero(long frames, int cycles);
	void zero();

	void stutterCycle(LayerContext* con, Layer* src, long startFrame, 
					  long offset);

	// Playback

	void restore(bool undo);
    void play(LayerContext* con, long startFrame, bool fadeIn);
    void cancelPlayFade();
	void transferPlayFade(Layer *dest);
	long captureTail(LayerContext* con, long startFrame, float adjust);

	// Deferred fades, setters only for Project

	bool isDeferredFadeLeft();
	bool isDeferredFadeRight();
	bool isContainsDeferredFadeLeft();
	bool isContainsDeferredFadeRight();
	bool hasDeferredFadeLeft();
	bool hasDeferredFadeRight();

	bool isReverseRecord();
	bool isDeferredFadeIn();
	bool isDeferredFadeOut();
	bool isContainsDeferredFadeIn();
	bool isContainsDeferredFadeOut();
	bool hasDeferredFadeIn(LayerContext* con);
	bool hasDeferredFadeOut(LayerContext* con);
	
	// Restruturing for Function implementations

	long calcFrames();
	long getSegmentFrames();
	void resize(long frames);
	void resizeFromSegments();
    void resetSegments();
	void setStructureChanged(bool b);

	// Projects

	class Segment* getSegments();
    void setAudio(Audio* a);
    void setOverdub(Audio* a);
	void addSegment(class Segment* seg);
	void setSegments(class Segment* list);
	void setReverseRecord(bool b);
	void setDeferredFadeLeft(bool b);
	void setDeferredFadeRight(bool b);
	void setContainsDeferredFadeLeft(bool b);
	void setContainsDeferredFadeRight(bool b);

	// Diagnostics

	void getTraceContext(int* context, long* time);
	void dump(TraceBuffer* b);
	void save(const char* file);
	void saveRegion(long startFrame, long frames, const char* name);

	// Coverage Testing

	static void initCoverage();
	static void showCoverage();

    // Segment restructuring

	void compileSegmentFades(bool checkConsistency);
    void setFinalized(bool b);
    bool isFinalized();

  protected:

	// for use by Segment 
	void getNoReflect(LayerContext* con, long startFrame, AudioCursor* cursor,
					  bool root, bool play);

  private:

	Layer(class LayerPool* lpool, class AudioPool* aupool);
	~Layer();

	void init() ;
	Layer* spawn();
	void watchMax(LayerContext* con);
    long reflectFrame(LayerContext* con, long frame);
    long reflectRegion(LayerContext* con, long frame, long frames);
	void forceFeedback(int level);

	void pruneSegments();
	void removeSegment(Segment* seg);
	Segment* addSegment(Layer* src);

	void checkRecording(LayerContext* con, long startFrame);
	void advanceInternal(LayerContext* con, long startFrame, int feedback);
	void prepare(LayerContext* con);
    void get(LayerContext* con, long startFrame, bool play);
	void insertCycle(LayerContext* con, long startFrame);
	void adjustSegmentFades(Segment* s);
	void insertSegmentGap(long startFrame, long frames);
	void initInsertLayer(LayerContext* con, long startFrame);
	void insertCycleInner(LayerContext* con, long startFrame);
	void occlude(long startFrame, long frames, bool seamless);
	void coalesce();
	void checkSegmentEdges();
	void startRecordFade(LayerContext* con);
    void setDeferredFadeIn(LayerContext* con);
    void setDeferredFadeOut(LayerContext* con);
	void setContainsDeferredFadeOut(LayerContext* con);

	void applyDeferredFadeLeft();
	void applyDeferredFadeRight();
    void fadeLeft(bool foreground, bool background, float baseLevel);
    void fadeRight(bool foreground, bool background, float baseLevel);
	void raiseBackgroundHead(LayerContext* con);
	void fadeBackgroundTail(LayerContext* con);
	void lowerBackgroundHead(LayerContext* con);
	void fadeBackground(LayerContext* con, long startFrame);
    void checkMaxUndo();

    //void applyDeferredFades(bool undo);
    void fadeOut(LayerContext* con);
	void fadeOut(LayerContext* con, long frames);

    /**
     * Pool from which we were allocated.
     */
    class LayerPool* mLayerPool;

    /**
     * Pool from which we can audio buffers.
     */
    class AudioPool* mAudioPool;

	bool		mPooled;
	Layer*  	mPrev;
	Layer*		mRedo;		// only for the redo list
	int			mNumber;
    int         mAllocation;
    int         mReferences;
	Loop*		mLoop;
    Segment*    mSegments;
	Audio*		mAudio;
	Audio*		mOverdub;
	long		mFrames;
	long		mPendingFrames;
	long		mLastFeedbackFrame;
	int 		mCycles;
	float 		mMax;
    int         mStartingFeedback;
	int			mFeedback;
	bool 		mStarted;
	bool 		mRecordable;
	bool 		mPlayable;
	bool		mPaused;
	bool		mMuted;
	bool		mFinalized;
	bool		mAudioChanged;
	bool		mStructureChanged;
	bool		mFeedbackApplied;
	bool		mInserting;
	long 		mInsertRemaining;
    bool        mContainsDeferredFadeLeft;
    bool        mContainsDeferredFadeRight;
    bool        mDeferredFadeLeft;
    bool        mDeferredFadeRight;
	bool		mReverseRecord;
	bool 		mNoFlattening;
	CheckpointState mCheckpoint;

	/**
     * This is intended to have a copy of the MobiusConfig.isolateOverdubs parameter.
	 * When true we save a copy of just the new content added to each layer
     * as well as maintaining the flattened layer.  This is then saved in the
     * project so you can process just the overdub.  This was an experimental
     * feature added around the time layer flattening was introduced.  It is 
     * no longer exposed in the user interface because it's hard to explain,
     * it isn't obvious when it has been enabled, and it can up to double
     * the amount of memory required for each layer.  
     *
     * When this is false, mOverdub will won't be used.
	 */
	bool mIsolatedOverdub;

	/**
	 * Maintains state for a transient play fade in.
	 * ?? Can move this to the fade window?
	 */
	AudioFade	mFade;

	/**
	 * Feedback smoother.
	 */
	class Smoother* mSmoother;

	/**
	 * Window that tracks the end of the recorded content for 
	 * deferred fade processing.
	 */	
	class FadeWindow* mTailWindow;

	/**
	 * Optional window that captures the recorded content at the
	 * front of the layer for deferred fade processing.
	 * !! Now that we have these, could use them instead of
	 * mDeferredFadeLeft & mDeferredFadeRight?
	 */
	class FadeWindow* mHeadWindow;

	/**
	 * Cursor used while extracting frames for playback.
	 */
	AudioCursor* mPlayCursor;

	/**
	 * Cursor used while extracting frames from the previous
	 * layer for feedback.
	 */
	AudioCursor* mCopyCursor;

	/**
	 * Cursor used to write frames extracted from the CopyCursor
	 * into the local audio.
	 */
	AudioCursor* mFeedbackCursor;

	/**
	 * Cursor used to write recorded frames into the local audio.
	 */
	AudioCursor* mRecordCursor;

	/**
	 * Cursor used to write recorded frames in to the isolated
	 * local audio mOverdub;
	 */
	AudioCursor* mOverdubCursor;

	/**
	 * Special option to suppress the next fade in or out.
	 * Currently used only with scripts for some special unit tests.
	 */
	bool mFadeOverride;

    /**
     * The frame offset of this layer within the entire layer history.
     * Used to locate windows.
     */
    long mHistoryOffset;

    /**
     * The frame offset within the entire layer history of the 
     * loop window.  If this is less than zero it means that a loop window
     * is not active.
     */
    long mWindowOffset;

    /**
     * The length of the subcycle in the original window layer.  
     * This is used when WindowEdgeUnit is subcycle because changing
     * the length of the window also changes the size of the subcycle.
     * So adding a subcycle then immediately removing it will not return
     * the window to it's original size unless the removal is done using
     * the original subcycle length.  For example, loop 1000 frames, 4 subcycles
     * subcycles, subcycle is 250 frames.  Add a subcycle for 1250 and
     * subcycle frames is 312.5.  This is only relevant during windowing.
     */
    long mWindowSubcycleFrames;

};

/****************************************************************************
 *                                                                          *
 *                                    POOL                                  *
 *                                                                          *
 ****************************************************************************/

/**
 * A pool of layers.  Normally only one of these managed
 * by a Mobius instance.
 */
class LayerPool {

  public:

    LayerPool(class AudioPool* aupool);
    ~LayerPool();

    Layer* newLayer(class Loop* l);
    void freeLayer(Layer* l);
    void freeLayerList(Layer* l);
    
    Layer* getMuteLayer();

    LayerContext* getCopyContext();

    void resetCounter();
    void dump();

  private:

	void flush();

    class AudioPool* mAudioPool;
    Layer* mLayers;
    int mCounter;
    int mAllocated;
    
    Layer* mMuteLayer;
    LayerContext* mCopyContext;

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
