/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Helper class for Layer to keep track of a short segment of
 * recorded audio over which a deferred fade may need to be applied.
 * Also used by PitchPlugin.
 *
 */

#ifndef FADE_WINDOW_H
#define FADE_WINDOW_H

#include "Audio.h"

/****************************************************************************
 *                                                                          *
 *   							 FADE WINDOW                                *
 *                                                                          *
 ****************************************************************************/

class FadeWindow {

  public:

    FadeWindow();
    ~FadeWindow();

    void reset();
    void prepare(class LayerContext* con, bool head);
    void add(class LayerContext* con, long externalFrame);
	void add(float* src, long frames);
	long getLastExternalFrame();
	void setLastExternalFrame(long frame);
	int getWindowFrames();
	int getFrames();
	void setFrames(long frames);
	bool isForegroundFaded();

	void startFadeIn();

	void removeForeground(class AudioCursor* cursor);
	void addForeground(class AudioCursor* cursor);
	void fadeForeground(class AudioCursor* cursor, float baseLevel);
	void fadeForegroundShifted(class AudioCursor* cursor, int fadeFrames);
	void fadeWindow(long startFrame, long fadeOffset);
	long reverseFade(float* buffer);
	void dump(const char* name);

	// Coverage Testing

	static void initCoverage();
	static void showCoverage();

  private:

	void locateEdges(int fadeFrames);
	void applyWindow(class AudioCursor* cursor, AudioOp op);

    /** 
     * A buffer large enough to hold the maximum fade range with
     * the maximum number of samples per frame.
     */
    float* mBuffer;

    /**
     * The size of mBuffer in samples.
     */
    int mBufferSize;

    /**
     * True if this is a "head" window vs. a "tail" window.
     */
    bool mHeadWindow;

    /**
     * The size of the current window in frames.
     * This is set when the window is prepared, it may be less
     * than the number of frames available in the buffer.
     */
    int mWindowFrames;

    /**
     * The number of samples per frame in the current window.
     * This is set when the window is prepared.
     */
    int mChannels;

    /**
     * True if the window was prepared for recording in reverse.
     * Once prepared, the direction must not change.
     */
    bool mReverse;

    /**
     * The number of frames that have been copied into the window.
     * For a Head window, this will advance to mWindowFrames then stop.
     * For a Tail window, this will keep incrementing and effectively
     * record the total number of frames that have passed through
     * the window.
     */
    int mFrames;

    /**
     * The sample index into mBuffer of the next frame to be overwritten.
     * This must be the first sample in the frame.
     */
    int mCursor;

    /**
     * The external frame number of the last frame copied into the window.
     * Used to detect gap in the window.
     */
    long mLastExternalFrame;

	/**
	 * True once the window is considered "full".  This applies only	
	 * to the head window.
	 */
	bool mFull;

	/**
	 * Fade state used for dynamic up fades during recording.
	 */
	AudioFade mFade;

	/**
	 * Flag set once we've performed a background fade.
	 */
	bool mBackgroundFaded;

	/**
	 * Flag set once we've performed a foreground fade.
	 */
	bool mForegroundFaded;

	// Temporary buffer regions used during fade application

	int mLeftFrames;
	int mLeftOffset;
	int mRightFrames;
	int mRightOffset;
	float* mLeftBuffer;
	float* mRightBuffer;

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
