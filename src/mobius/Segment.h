/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * An object maintaining a "window" into a Layer.  Used by layers
 * define their content by referencing other layers.  See commentary
 * in Layer.cpp for more.
 *
 */

#ifndef SEGMENT_H
#define SEGMENT_H

/****************************************************************************
 *                                                                          *
 *                                  SEGMENT                                 *
 *                                                                          *
 ****************************************************************************/

class Segment {

  public:

    Segment();
	Segment(class Layer* src);
    Segment(class Audio* src);
    Segment(Segment* src);
    ~Segment();

    void setNext(Segment* ref);
    Segment* getNext();

    void setOffset(long f);
    long getOffset();

    void setLayer(class Layer* l);
    class Layer* getLayer();
	
    void setAudio(class Audio* l);
    class Audio* getAudio();
	
    void setStartFrame(long f);
    long getStartFrame();

    void setFrames(long l);
    long getFrames();

    void setFeedback(int i);
    int getFeedback();

    void setReverse(bool b);
    bool isReverse();

	void setLocalCopyLeft(long frames);
	long getLocalCopyLeft();

	void setLocalCopyRight(long frames);
	long getLocalCopyRight();

	void setFadeLeft(bool b);
	bool isFadeLeft();

	void setFadeRight(bool b);
	bool isFadeRight();

	void saveFades();
	bool isSaveFadeLeft();
	bool isSaveFadeRight();
	
	void setUnused(bool b);
	bool isUnused();

    void get(class LayerContext* con, long startFrame, class AudioCursor* cursor, 
			 bool play);

	void trimLeft(long frames, bool copy);
	void trimRight(long frames, bool copy);

	bool isAtStart(class Layer* parent);
	bool isAtEnd(class Layer* parent);

	void dump(class TraceBuffer* b);

  private:

    void init();
    void checkFades();

    /**
     * Next layer reference on the chain.
     */
    Segment* mNext;

    /**
     * The location of this segment within the parent Layer.
     * This is normally zero if there is only one
     * reference.  If there is more than once segment this is not
     * necessarily greater than the preceeding segment.  Segments may
     * appear on the list in any order, and they may overlap.
     */
    long mOffset;

    /**
     * The referenced Layer.
     */
    class Layer* mLayer;

    /**
     * The referenced audio.
     * If you have both mLayer and mAudio set, mLayer will have priority.   
     */
    class Audio* mAudio;

	/**
	 * Cursor for playing local audio if necessary.
	 */
	class AudioCursor* mCursor;

    /**
     * The starting frame in the referenced layer.
     */
    long mStartFrame;

    /**
     * The number of frames in the referenced layer.
     */
    long mFrames;

    /**
     * The amount of feedback (volume reduction) to apply to the samples
	 * returned from the reference.  The value is an index into the
	 * 128 level feedback ramp.
     */
    int mFeedback;

	/**
	 * True to indicate that the referenced audio is to be played
	 * in reverse.  
	 */
	bool mReverse;

	/**
	 * The number of frames to the left of this segment that have been copied
	 * into the local Audio.  Normally disables a left fade.
	 */
	long mLocalCopyLeft;

	/**
	 * The number of frames to the right of this segment that have been copied
	 * into the local Audio.  Normally disables a right fade.
	 */
	long mLocalCopyRight;

	/**
	 * Internal calculated value, true if we need to perform
	 * a left fade.  
	 */
	bool mFadeLeft;

	/**
	 * Internal calculated value, true if we need to perform
	 * a right fade.  Suppressed by mNoFadeRight;
	 */
	bool mFadeRight;

	/**
	 * Temporary validation state.
	 */
    bool mSaveFadeLeft;
    bool mSaveFadeRight;

	/**
	 * Transient flag set during segment processing to indiciate
	 * that the segment is nolonger within the range of this layer.
	 * Normally it will be removed by pruneSegments()
	 */
	bool mUnused;

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
