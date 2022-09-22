/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Metronome object for the sequencer.
 *
 * One of these will be created automatically for each Sequencer.
 * use Sequencer::getMetronome to obtain a handle, then wail away on it.
 *
 */

#include <stdio.h>

#include "port.h"

#include "WinMidiEnv.h"
#include "Sequencer.h"

/****************************************************************************
 *                                                                          *
 *   						  INTERFACE METHODS                             *
 *                                                                          *
 ****************************************************************************/

PUBLIC SeqMetronome::SeqMetronome(void) {
	init();
}

PUBLIC SeqMetronome::~SeqMetronome(void) {
}

/**
 * 
 * Initializes the metronome to its default state.
 */
PUBLIC void SeqMetronome::init(void)
{
	mEnabled  		= true;
	mChannel 		= 9;			// standard drum channel (base 0)
	mNote    		= 37;			// side stick
	mVelocity 		= 60;
	mAccentNote		= mNote;
	mAccentVelocity	= 127;
	mRecordNote		= 55;			// splash cymbal
	mRecordVelocity	= 127;

	// should be getting this from the timer !
	mCpb 			= 96;

	// should be enabled for non-Drum machines and OMNI mode devices
	mNoteOff			= 1;

	// no accent beat by default
	setBeat(4);
}

/**
 * Sets the beat count, used to determining accent beats.
 * This is normally done whenever the underlying clock CPB changes
 * or when a beats-per-measure is specified for the sequencer.
 * If the beat parameters are not set, the metronome will always
 * use the non-accent note.
 */
PUBLIC void SeqMetronome::setBeat(int b)
{
	mBeats = b;
	mBeat = 0;
	setClock(0);
}

/**
 * Sets the metronome phase based on a clock value.
 * Beats per measure must have been previously set with setBeat.
 * If no bpm has been set, the metronome will not be using
 * accents and setClock will have no effect.
 */
PUBLIC void SeqMetronome::setClock(int clock)
{
	if (mCpb && mBeats) {
		mBeat = ((clock % (mBeats * mCpb)) / mCpb);
		if (mBeat != 0)
		  mBeat = mBeats - mBeat;
	}
}

/**
 * Sets the clock resolution, used when calculating metronome event 
 * boundaries.  This should be set to the same as whatever the underlying
 * MIDI Timer object uses.
 */
PUBLIC void SeqMetronome::setCpb(int c)
{
	mCpb = c;
	// reset related settings
	mBeat = 0;
}

/****************************************************************************
 *                                                                          *
 *   						   INTERNAL METHODS                             *
 *                                                                          *
 ****************************************************************************/

/**
 * Called by the sequencer on beat boundaries.
 * Advance the internal state, and issue MIDI notes as appropriate.
 */
PRIVATE void SeqMetronome::advance(MidiOut *out)
{
	if (mEnabled) {
		if (mBeat) {
			// unaccented note
			out->sendNoteOn(mChannel, mNote, mVelocity);
			if (mNoteOff)
			  out->sendNoteOff(mChannel, mNote);

			mBeat--;
		}
		else {
			// accented note
			out->sendNoteOn(mChannel, mAccentNote, mAccentVelocity);
			if (mNoteOff)
			  out->sendNoteOff(mChannel, mAccentNote);

			mBeat = mBeats - 1;
			if (mBeat < 0)
			  mBeat = 0;
		}  
	}
}

/**
 * Called by the sequencer when we're about to start recording or
 * a recording loop is taken.
 *
 * If the metronome defines a special record note, we emit it.
 */
PRIVATE void SeqMetronome::sendRecord(MidiOut *out)
{
	if (mRecordNote && mRecordVelocity) {
		out->sendNoteOn(mChannel, mRecordNote, mRecordVelocity);
		if (mNoteOff)
		  out->sendNoteOff(mChannel, mRecordNote);
	}
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
