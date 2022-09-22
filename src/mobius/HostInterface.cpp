/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Default implementations for the HostInterface.h abstraction.
 *
 */

#include <stdio.h>

#include "Trace.h"
#include "Util.h"

// for AudioTime
#include "AudioInterface.h"
#include "HostConfig.h"

#include "HostInterface.h"

//////////////////////////////////////////////////////////////////////
//
// PluginParameter
//
//////////////////////////////////////////////////////////////////////

PluginParameter::PluginParameter()
{
	mNext = NULL;
	mId = 0;
	mName = NULL;
	mType = PluginParameterContinuous;
	mMinimum = 0.0;
	mMaximum = 127.0;
	// it is AU assumes mDefault and mLast have the same initial values
	mDefault = 0.0;
	mLast = 0.0;
    mChanged = false;
}

PluginParameter::~PluginParameter()
{
	delete mName;

	PluginParameter *el, *next;
	for (el = mNext ; el != NULL ; el = next) {
		next = el->getNext();
		el->setNext(NULL);
		delete el;
	}
}

PluginParameter* PluginParameter::getNext() 
{
	return mNext;
}
		
void PluginParameter::setNext(PluginParameter* next) 
{
	mNext = next;
}

int PluginParameter::getId() 
{
	return mId;
}
	
const char* PluginParameter::getName() 
{
	return mName;
}

void PluginParameter::setName(const char* name)
{
	delete mName;
	mName = CopyString(name);
}

PluginParameterType PluginParameter::getType()
{
	return mType;

}

void PluginParameter::setType(PluginParameterType t)
{
	mType = t;
}

float PluginParameter::getMinimum()
{
	return mMinimum;
}

void PluginParameter::setMinimum(float f)
{
	mMinimum = f;
}

float PluginParameter::getMaximum()
{
	return mMaximum;
}

void PluginParameter::setMaximum(float f)
{
	mMaximum = f;
}

float PluginParameter::getDefault()
{
	return mDefault;
}

void PluginParameter::setDefault(float f)
{
	mDefault = f;
}

float PluginParameter::getLast()
{
	return mLast;
}

void PluginParameter::setLast(float f)
{
	mLast = f;
}

/**
 * Set the value of a parameter given to us by the host.
 * For AU plugins, these will be done in bulk at the beinning
 * of duty cycle before we process buffers.  For VST plugins
 * I'm not sure but I think they can come in randomly and we
 * are not necessarily in the processReplacing thread, so some
 * parameter settings may have to be deferred.
 *
 * Hosts like to set the same value over and over so keep
 * the last one and ignore if it didn't change.  
 */
bool PluginParameter::setValueIfChanged(float neu)
{
	mChanged = false;
	if (neu != mLast) {
		setValueInternal(neu);
		mLast = neu;
		mChanged = true;
	}
	return mChanged;
}

/**
 * Called at the end of an audio cycle to refresh the value
 * that we return to the host when it asks for a parameter.
 * Until this is called we will always return mLast which
 * is important because setValue() is not necessarily synchronous
 * and the host often immediately asks for the parameter value
 * to make sure it was set.
 */
bool PluginParameter::refreshValue()
{
	bool changed = false;

	float neu = getValueInternal();
	if (neu != mLast) {
		changed = true;
		mLast = neu;
	}

    // If we changed it during this duty cycle also return true
    // to make sure the host is updated
    if (mChanged) {
        changed = true;
        mChanged = false;
    }

	return changed;
}

/**
 * For VST's getParameterValueDisplay.
 * Return the value as a string.
 */
void PluginParameter::getValueString(float value, char* buffer, int max)
{
    if (mType == PluginParameterEnumeration) {
        const char** labels = getValueLabels();
        if (labels != NULL) {
            int ivalue = (int)value;
            int pmin = (int)getMinimum();
            int pmax = (int)getMaximum();
            if (ivalue >= pmin && ivalue <= pmax) {
                // labels are always zero based
                ivalue -= pmin;
                const char* str = labels[ivalue];
                CopyString(str, buffer, max);
            }
            else
              CopyString("?", buffer, max);
        }
    }
    else if (mType == PluginParameterContinuous) {
        // these are always 0-127 don't have to factor in min/max
        int ivalue = (int)value;
        char ibuf[128];
        sprintf(ibuf, "%d", ivalue);
        CopyString(ibuf, buffer, max);
    }
    else if (mType == PluginParameterBoolean) {
        int ivalue = (int)value;
        if (ivalue > 0)
          CopyString("On", buffer, max);
        else
          CopyString("Off", buffer, max);
    }
    else if (mType == PluginParameterButton) {
        int ivalue = (int)value;
        if (ivalue > 0)
          CopyString("Down", buffer, max);
        else
          CopyString("Up", buffer, max);
    }
}

/**
 * For VST's string2parameter.
 * Set the value as a string.  Should be symetrical with 
 * getValueString.
 *
 * Similar work being done by Parmeter, we could just push
 * these into MobiusPluginParameter?
 * 
 */
void PluginParameter::setValueString(const char* value)
{
    if (value == NULL) {
        // does this make sense?
        setValueIfChanged(0.0);
    }
    else if (mType == PluginParameterEnumeration) {
        const char** labels = getValueLabels();
        if (labels == NULL) {
            // ignore?
        }
        else {
            int max = (int)getMaximum();
            for (int i = 0 ; i <= max ; i++) {
                if (StringEqual(value, labels[i])) {
                    setValueIfChanged((float)i);
                    break;
                }
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
// HostSyncState
//
//////////////////////////////////////////////////////////////////////

/**
 * The three initializations to -1 have been done
 * for a long time but I don't think they're all necessary.
 * mLastSamplePosition is only relevant when trying to detect transport
 * changes from the sample position.
 *
 * Since we don't reset sync state when the transport stops, we're
 * in a very small "unknown" state at the beginning.  Feels better just
 * to assum we're at zero?
 */
PUBLIC HostSyncState::HostSyncState()
{
	// changes to stream state
	mTraceChanges = false;
	// SyncTracker traces enough, don't need this too if  
	// things are working
    mTraceBeats = false;

    mHostRewindsOnResume = false;
    mHostPpqPosTransport = false;
    mHostSamplePosTransport = false;

    mSampleRate = 0;
    mTempo = 0;
    mTimeSigNumerator = 0;
    mTimeSigDenominator = 0;
    mBeatsPerFrame = 0.0;
    mBeatsPerBar = 0.0;

    mPlaying = false;
    mLastSamplePosition = -1.0;
    mLastBeatPosition = -1.0;

    mResumed = false;
    mStopped = false;
    mAwaitingRewind = false;

    mLastBeatRange = 0.0;
    mBeatBoundary = false;
    mBarBoundary = false;
    mBeatOffset = 0;
    mLastBeat = -1;
    mBeatCount = 0;
    mBeatDecay = 0;
}

PUBLIC HostSyncState::~HostSyncState()
{
}

PUBLIC void HostSyncState::setHost(HostConfigs* config)
{
	if (config != NULL) {
        mHostRewindsOnResume = config->isRewindsOnResume();
        mHostPpqPosTransport = config->isPpqPosTransport();
        mHostSamplePosTransport = config->isSamplePosTransport();
    }
}

PUBLIC void HostSyncState::setHostRewindsOnResume(bool b)
{
	mHostRewindsOnResume = b;
}

/**
 * Export our sync state to an AudioTime.
 * There is model redundancy here, but I don't want
 * AudioTime to contain the method implementations and there
 * is more state we need to keep in HostSyncState.
 */
PUBLIC void HostSyncState::transfer(AudioTime* autime)
{
	autime->tempo = mTempo;
    autime->beatPosition = mLastBeatPosition;
	autime->playing = mPlaying;
    autime->beatBoundary = mBeatBoundary;
    autime->barBoundary = mBarBoundary;
    autime->boundaryOffset = mBeatOffset;
    autime->beat = mLastBeat;
    // can this ever be fractional?
	autime->beatsPerBar = (int)mBeatsPerBar;
}

/**
 * Update tempo related state.
 */
PUBLIC void HostSyncState::updateTempo(int sampleRate, double tempo, 
                                       int numerator,  int denominator)
{
    bool tempoChanged = false;
    
    if (sampleRate != mSampleRate) {
		if (mTraceChanges)
		  trace("HostSync: Sample rate changing from %d to %d\n",
				mSampleRate, sampleRate);
        mSampleRate = sampleRate;
        tempoChanged = true;
    }

    if (tempo != mTempo) {
		if (mTraceChanges)
		  trace("HostSync: Tempo changing from %lf to %lf\n", mTempo, tempo);
        mTempo = tempo;
        tempoChanged = true;
    }

    // recalculate when any component changes
    if (tempoChanged) {
        int framesPerMinute = 60 * mSampleRate;
        double bpf = mTempo / (double)framesPerMinute;
        if (bpf != mBeatsPerFrame) {
			if (mTraceChanges)
			  trace("HostSync: BeatsPerFrame changing to %lf\n", bpf);
            mBeatsPerFrame = bpf;
        }
    }

    // !! Comments in VstMobius indiciate that denominator at least
    // can be fractional for things like 5/8.  Really!?

    bool tsigChange = false;

    if (numerator != mTimeSigNumerator) {
		if (mTraceChanges)
		  trace("HostSync: Time sig numerator changing to %d\n", numerator);
        mTimeSigNumerator = numerator;
        tsigChange = true;
    }

    if (denominator != mTimeSigDenominator) {
		if (mTraceChanges)
		  trace("HostSync: Time sig denominator changing to %d\n", denominator);
        mTimeSigDenominator = denominator;
        tsigChange = true;
    }
    
    if (tsigChange) {
        double bpb = mTimeSigNumerator / (mTimeSigDenominator / 4);
        if (bpb != mBeatsPerBar) {
			if (mTraceChanges)
			  trace("HostSync: BeatsPerBar changing to %lf\n", bpb);
            mBeatsPerBar = bpb;
        }
    }
}

/**
 * Update stream state.
 *
 * "frames" is the number of frames in the current audio buffer.
 *
 * newSamplePosition is what VST calls "samplePos" and what AU calls 
 * currentSampleInTimeLine.  It increments on each buffer relative
 * to the start of the tracks which is sample zero.
 * 
 * newBeatPosition is what VST calls "ppqPos" and what AU calls "currentBeat".
 * It is a fractional beat counter relative to the START of the current buffer.
 * 
 * transportChanged and transportPlaying are true if the host can provide them.
 * Some hosts don't so we can detect transport changes based on changes
 * in the eatPosition or samplePosition.
 *
 */
PUBLIC void HostSyncState::advance(int frames, 
                                   double newSamplePosition, 
                                   double newBeatPosition,
                                   bool transportChanged, 
                                   bool transportPlaying)
{
    // update transport related state
    // sets mPlaying, mResumed, mStopped 
    updateTransport(newSamplePosition, newBeatPosition, 
                    transportChanged, transportPlaying);

    bool traceBuffers = false;
    if (traceBuffers && mPlaying) {
        trace("HostSync: samplePosition %lf beatPosition %lf frames %ld\n", 
              newSamplePosition, newBeatPosition, frames);
    }

    // kludge for Cubase that likes to rewind AFTER the transport 
    // status changes to play
    if (mResumed) {
        if (mHostRewindsOnResume) {
			if (mTraceChanges)
			  trace("HostSync: awaiting rewind\n");
            mAwaitingRewind = true;
        }
    }
    else if (mStopped) {
        // clear this?  I guess it doesn't matter since
        // we'll set it when we're resumed and we don't
        // care when !mPlaying
        mAwaitingRewind = false;
    }
    else if (mAwaitingRewind) {
        if (mLastBeatPosition != newBeatPosition) {
            mAwaitingRewind = false;
            // make it look like a resume for the beat logic below
            mResumed = true;
			if (mTraceChanges)
			  trace("HostSync: rewind detected\n");
        }
    }

    // set if we detect a beat in this buffer
    // don't trash mBeatBoundary yet, we still need it 
    bool newBeatBoundary = false;
    bool newBarBoundary = false;
    int newBeatOffset = 0;
    double newBeatRange = 0.0;

    // Determine if there is a beat boundary in this buffer
    if (mPlaying && !mAwaitingRewind) {

        // remove the fraction
        long baseBeat = (long)newBeatPosition;
        long newBeat = baseBeat;

        // determine the last ppqPos within this buffer
        newBeatRange = newBeatPosition + (mBeatsPerFrame * (frames - 1));

        // determine if there is a beat boundary at the beginning
        // or within the current buffer, and set beatBoundary
        if (newBeatPosition == (double)newBeat) {
            // no fraction, first frame is exactly on the beat
            // NOTE: this calculation, like any involving direct equality
            // of floats may fail due to rounding error, in one case
            // AudioMulch seems to reliably hit beat 128 with a ppqPos
            // of 128.00000000002 this will have to be caught in
            // the jump detector below, which means we really don't
            // need this clause
            if (!mBeatBoundary)
              newBeatBoundary = true;
            else { 
                // we advanced the beat in the previous buffer,
                // must be an error in the edge condition?
                // UPDATE: this might happen due to float rounding
                // so we should probably drop it to level 2?
                Trace(1, "HostSync: Ignoring redundant beat edge condition!\n");
            }
        }
        else {
            // detect beat crossing within this buffer
            long lastBeatInBuffer = (long)newBeatRange;
            if (baseBeat != lastBeatInBuffer ||
                // fringe case, crossing zero
                (newBeatPosition < 0 && newBeatRange > 0)) {
                newBeatBoundary = true;
                newBeatOffset = (long)
                    (((double)lastBeatInBuffer - newBeatPosition) / mBeatsPerFrame);
                newBeat = lastBeatInBuffer;
            }
        }

        // check for jumps and missed beats
        // when checking forward movement look at beat counts rather
        // than expected beatPosition to avoid rounding errors
        bool jumped = false;
        if (newBeatPosition <= mLastBeatPosition) {
            // the transport was rewound, this happens with some hosts
            // such as Usine that maintain a "cycle" and wrap the
            // beat counter from the end of the cycle back to the front
			if (mTraceChanges)
			  trace("HostSync: Transport was rewound\n");
            jumped = true;
        }
        else if (newBeat > (mLastBeat + 1)) {
            // a jump of more than one beat, transport must be forwarding
			if (mTraceChanges)
			  trace("HostSync: Transport was forwarded\n");
            jumped = true;
        }
        else if (!newBeatBoundary && (newBeat != mLastBeat)) {
            // A single beat jump, without detecting a beat boundary.
            // This can happen when the beat falls exactly on the first
            // frame of the buffer, but due to float rounding we didn't
            // catch it in the (beatPosition == (double)newBeat) clause above.
            // In theory, we should check to see if mLastBeatRange is
            // "close enough" to the current beatPosition to prove they are
            // adjacent, otherwise, we could have done a fast forward
            // from the middle of the previous beat to the start of this 
            // one, and should treat that as a jump?  I guess it doesn't
            // hurt the state machine, we just won't get accurately sized
            // loops if we're doing sync at the moment.
            if (!mBeatBoundary)
              newBeatBoundary = true;
            else {
                // this could only happen if we had generated a beat on the
                // previous buffer, then instantly jumped to the next beat
                // it is a special case of checking mLastPpqRange, the
                // two buffers cannot be adjacent in time
				if (mTraceChanges)
				  trace("HostSync: Transport was forwarded one beat\n");
                jumped = true;
            }
        }

        // when we resume or jump, have to recalculate the beat counter
        if (mResumed || jumped) {
            // !! this will be wrong if mBeatsPerBar is not an integer,
            // when would that happen?
            mBeatCount = (int)(baseBeat % (long)mBeatsPerBar);
			if (mTraceChanges) {
				if (mResumed)
				  trace("HostSync: Resuming playback at bar beat %d\n",
						mBeatCount);
				else 
				  trace("HostSync: Playback jumped to bar beat %d\n",
						mBeatCount);
			}
        }

        // For hosts like Usine that rewind to the beginning of a cycle,
        // have to suppress detection of the beat at the start of the
        // cycle since we already generated one for the end of the cycle
        // on the last buffer.  This will also catch odd situations
        // like instantly moving the location from one beat to another.
        if (newBeatBoundary) {
            if (mBeatBoundary) {
                // had one on the last buffer
                newBeatBoundary = false;
                if (!mResumed && !jumped) 
                  Trace(1, "HostSync: Supressed double beat, possible calculation error!\n");
                // sanity check, mBeatDecay == 0 should be the same as
                // mBeatBoundary since it happened on the last buffer
                if (mBeatDecay != 0)
                  Trace(1, "HostSync: Unexpected beat decay value!\n");
            }
            else {
                int minDecay = 4; // need configurable maximum?
                if (mBeatDecay < minDecay) {
                    // We generated a beat/bar a few buffers ago, this
                    // happens in Usine when it rewinds to the start
                    // of the cycle, but let's it play a buffer past the
                    // end of the cycle before rewinding.  This is a host
                    // error since the bar length Mobius belives is
                    // actually shorter than the one Usine will be playing.
                    Trace(1, "HostSync: Suppressed double beat, host is not advancing the transport correctly!\n");
                    newBeatBoundary = false;
                }
            }
        }


        // Detect bars
        // VST barStartPos is useless because hosts don't implement
        // it consistently, see vst notes for more details.
        if (newBeatBoundary) {
            if ((mResumed || jumped) && newBeatOffset == 0) {
                // don't need to update the beat counter, but we may
                // be starting on a bar
                if (mBeatCount == 0 || mBeatCount >= mBeatsPerBar) {
                    newBarBoundary = true;
                    mBeatCount = 0;
                }
            }
            else {
                mBeatCount++;
                if (mBeatCount >= mBeatsPerBar) {
                    newBarBoundary = true;
                    mBeatCount = 0;
                }
            }
        }

        // selectively enable these to reduce clutter in the stream
        if (mTraceBeats) {
            if (newBarBoundary)
              trace("HostSync: BAR: position: %lf range: %lf offset %d\n", 
                    newBeatPosition, newBeatRange, newBeatOffset);

            else if (newBeatBoundary)
              trace("HostSync: BEAT: postition: %lf range: %lf offset %d\n", 
                    newBeatPosition, newBeatRange, newBeatOffset);
        }

        mLastBeat = newBeat;
    }

    // save state for the next interrupt
    mLastSamplePosition = newSamplePosition;
    mLastBeatPosition = newBeatPosition;
    mLastBeatRange = newBeatRange;
    mBeatBoundary = newBeatBoundary;
    mBarBoundary = newBarBoundary;
    mBeatOffset = newBeatOffset;

    if (mBeatBoundary)
      mBeatDecay = 0;
    else
      mBeatDecay++;
}

/**
 * Update state related to host transport changes.
 */
PRIVATE void HostSyncState::updateTransport(double samplePosition, 
                                            double beatPosition,
                                            bool transportChanged, 
                                            bool transportPlaying)
{
    mResumed = false;
    mStopped = false;

    // detect transport changes
	if (transportChanged) {
		if (transportPlaying != mPlaying) {
			if (transportPlaying) {
				if (mTraceChanges)
				  trace("HostSync: PLAY\n");
				mResumed = true;
			}
			else {
				if (mTraceChanges)
				  trace("HostSync: STOP\n");
				// Clear out all sync status
				// or just keep going pretending there are beats and bars?
                mStopped = true;
			}
			mPlaying = transportPlaying;
		}
		else {
			// shouldn't be getting redundant signals?
		}
	}
    else if (mHostSamplePosTransport) {
        // set only for hosts that don't reliably do transport 
		if (mLastSamplePosition >= 0.0) { 
			bool playing = (mLastSamplePosition != samplePosition);
			if (playing != mPlaying) {
				mPlaying = playing;
				if (mPlaying) {
					if (mTraceChanges)
					  trace("HostSync: PLAY (via sample position) %lf %lf\n",
							mLastSamplePosition,
							samplePosition);
					mResumed = true;
				}
				else {
					if (mTraceChanges)
					  trace("HostSync: STOP (via sample position)\n");
					// Clear out all sync status
					// or just keep going pretending there are beats and bars?
                    mStopped = true;
				}
			}
		}
	}
	else if (mHostPpqPosTransport) {
        // Similar to mCheckSamplePosTransport we could try to detect
        // this with movement of ppqPos.  This seems even less likely
        // to be necessary
		if (mLastBeatPosition >= 0.0) {
			bool playing = (mLastBeatPosition != beatPosition);
			if (playing != mPlaying) {
				mPlaying = playing;
				if (mPlaying) {
					if (mTraceChanges)
					  trace("HostSync: PLAY (via beat position) %lf %lf\n",
							mLastBeatPosition, beatPosition);
					mResumed = true;
				}
				else {
					if (mTraceChanges)
					  trace("HostSync: STOP (via beat position)\n");
                    mStopped = true;
				}
			}
		}
	}
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
