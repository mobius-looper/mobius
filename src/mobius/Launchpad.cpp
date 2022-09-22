/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Class managing communication with the Novation Launchpad.
 *
 * The Launchpad is not programmable so "enabling" it means we have
 * to hard wire a lot of MIDI bindings.  The LP communicates mostly
 * with notes, and always on channel 1 (with one outbound exception 
 * using channel 3).
 *
 * The buttons are divided into three areas:
 *
 *   - grid, the main 8x8 central buttons
 *   - scene, the 8 buttons along the right edge
 *   - page, the 8 buttons at the top
 *
 * It is easiest to think of the scene buttons as a ninth column of the grid.
 * There are two layout options for the grid, the most logical one for
 * us is X-Y where upper left is key zero and bottom right is key 119.
 * 
 * Drum rack layout is designed I guess for use with common drum machine
 * mappings, it is ugly and complicated.
 *
 * A small set of CC's can be sent to the LP to set various things:
 *
 *   B0 00 00
 *    - reset, all leds turn off, mapping mode, buffer setings, 
 *      and duty cycle are reset to default values
 *
 *   B0 00 [01 | 02]
 *    - set the grid mapping mode, xx=1 for X/Y, xx=2 for drum rack
 *      X/Y is the default
 *
 *   B0 00 [20 - 3D]
 *    - controls doubble buffering, complex value with various
 *      flash and copy bits
 *
 *   B0 00 [7D | 7E | 7F]
 *    - led test, sets all leds to low medium or full bright
 *      this also resets all other data like B0 00 00 
 * 
 *   B0 [1E - 1F] [data]
 *     - sets the duty cyclce, data is complex see manual
 *
 *   B0 [68 - 6F] data
 *     - sets the control buttons at the top, data same as grid buttons
 *
 * Notes on channel 3 are treated as a speical "rapid update" mode.
 * Unfortunately this means that channels 1 and 3 are effectively reserved
 * for the Launchpad.  If there is more than one controller or footswitch
 * it is best if they use different channels.
 * 
 * In case someone can use channel mapping between the LP and Mobius,
 * we should have a configurable LP channel that defaults to 1.
 *
 * 
 * Buttons are sent from the LP using notes and CC's with 127 down
 * and 0 up.
 *
 * So the global configuration parameters are:
 *
 *    Launchpad Enabled: 0-1
 *    Launchpad Channel: 0-15
 *     - assume that the "rapid" update channel is +2 the base channel
 *
 * Could put these in the MIDI Control dialog.
 * 
 * This is going to suck for FCB users that probably all use channel 1.
 * Drum rack mode has 36 unused at the bottom which is a little better
 * as it gives a larger contiguous range.  X/Y mode has lots of holes.
 * Easiest to favor LP rather than old MIDI bindings, makes it clear
 * what needs to happen?
 *
 * CC numbers 0, 1E-1F, 68-6F are less likely to conflict.
 * 
 * This needs to factor into BindingResolver when enablement changes.
 * 
 * SHADES
 *
 * COLOR_RED_LOW_GREEN_LOW
 *    This is light "amber".  Splotchy due to the uneven red/green balance.
 *    Some look clearly yellow others are hard to tell apart from pale green.
 * 
 * COLOR_RED_LOW_GREEN_MED
 *    Still splotchy, just a touch of yellow.
 * 
 * COLOR_RED_LOW_GREEN_HIGH
 *    Green with very subtle yellow tint in some.  Probably unusuable.
 * 
 * COLOR_RED_MED_GREEN_LOW
 *    A nice burnt orange, a little uneven but not bad.
 * 
 * COLOR_RED_MED_GREEN_MED
 *    Medium "amber".  Uneven like the low amber.
 * 
 * COLOR_RED_MED_GREEN_HIGH
 *     Still pretty close to green.
 * 
 * COLOR_RED_HIGH_GREEN_LOW
 *     Nice rich orange!
 * 
 * COLOR_RED_HIGH_GREEN_MED
 *     Slightly pale orange, starts to look uneven.
 * 
 * COLOR_RED_HIGH_GREEN_HIGH
 *     Bright "amber" uneven.
 *
 * RED_HIGH, GREEN_HIGH, YELLOW_HIGH are the primaries.
 * RED_HIGH_GREEN_LOW is a very usable orange.
 * RED_MED_GREEN_LOW is a good dark orange.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#include "Util.h"
#include "MidiByte.h"
#include "MidiEvent.h"
#include "MidiInterface.h"
#include "HostMidiInterface.h"

#include "Mobius.h"
#include "MobiusConfig.h"
#include "Track.h"
#include "Loop.h"

#include "Launchpad.h"

//////////////////////////////////////////////////////////////////////
//
// Constants
//
//////////////////////////////////////////////////////////////////////

/**
 * MIDI CC number of the first top button.
 */
#define BUTTON_BASE 0x68 
#define BUTTON_LAST BUTTON_BASE + TOP_BUTTONS

/**
 * MIDI note number for the first arrow button on the right.
 * The first four are used for sub pages.
 */
#define ARROW_BASE 0x08
#define ARROW_DELTA 0x10

/**
 * The number of keys on a "row" in X/Y mapping mode.
 */
#define NATIVE_GRID_COLUMNS 16

/**
 * Color values without flag bits.
 */
#define COLOR_OFF 0
#define COLOR_RED_LOW 1
#define COLOR_RED_MED 2
#define COLOR_RED_HIGH 3
#define COLOR_GREEN_LOW 0x10
#define COLOR_GREEN_MED 0x20
#define COLOR_GREEN_HIGH 0x30
#define COLOR_YELLOW_LOW 0x11
#define COLOR_YELLOW_MED 0x22
#define COLOR_YELLOW_HIGH 0x33

#define COLOR_RED_LOW_GREEN_LOW 0x11
#define COLOR_RED_LOW_GREEN_MED 0x21
#define COLOR_RED_LOW_GREEN_HIGH 0x31

#define COLOR_RED_MED_GREEN_LOW 0x12
#define COLOR_RED_MED_GREEN_MED 0x22
#define COLOR_RED_MED_GREEN_HIGH 0x32

#define COLOR_RED_HIGH_GREEN_LOW 0x13
#define COLOR_RED_HIGH_GREEN_MED 0x23
#define COLOR_RED_HIGH_GREEN_HIGH 0x33

/**
 * Color values for specific purposes.
 */
#define COLOR_BUTTON_DEFAULT COLOR_GREEN_LOW
#define COLOR_BUTTON_PRESSED COLOR_GREEN_HIGH
#define COLOR_BUTTON_SELECTED COLOR_YELLOW_HIGH

/**
 * Loop status colors.
 */
#define COLOR_LOOP_RESET COLOR_YELLOW_LOW
#define COLOR_LOOP_FULL COLOR_YELLOW_HIGH
#define COLOR_LOOP_RECORD COLOR_RED_HIGH
#define COLOR_LOOP_MUTE COLOR_GREEN_LOW
#define COLOR_LOOP_PLAY COLOR_GREEN_HIGH

/**
 * Top button numbers.
 */
#define BUTTON_UP 0
#define BUTTON_DOWN 1
#define BUTTON_LEFT 2
#define BUTTON_RIGHT 3
#define BUTTON_SESSION 4
#define BUTTON_USER1 5
#define BUTTON_USER2 6
#define BUTTON_MIXER 7

/**
 * Right button cell numbers.
 */
#define RIGHT_VOL 8
#define RIGHT_PAN 16
#define RIGHT_SNDA 24
#define RIGHT_SNDB 32
#define RIGHT_STOP 40
#define RIGHT_TRKON 48
#define RIGHT_SOLO 56
#define RIGHT_ARM 64

/**
 * Virtual pages.
 */
#define PAGE_SESSION 0
#define PAGE_USER1 1
#define PAGE_USER2 2
#define PAGE_MIXER 3

/**
 * Mixer sub-pages.
 */
#define PAGE_MIXER_VOLUME 0
#define PAGE_MIXER_PAN 1 
#define PAGE_MIXER_SEND_A 2
#define PAGE_MIXER_FEEDBACK 2
#define PAGE_MIXER_SEND_B 3
#define PAGE_MIXER_ALTFEEDBACK 3

/**
 * Internal cell column for the arrows.
 */
#define ARROW_CELL_COLUMN 8


/**
 * Inner grid characteristics.
 */
#define INNER_GRID_COLUMNS 8
#define INNER_GRID_ROWS 8

//////////////////////////////////////////////////////////////////////
//
// Constructor/Properties
//
//////////////////////////////////////////////////////////////////////

Launchpad::Launchpad(Mobius* m)
{
    mMobius = m;
    mInitialized = false;
    mPage = PAGE_SESSION;
    mMixerPage = PAGE_MIXER_VOLUME;

    mSessionLoops = 4;

    initButtons(COLOR_OFF);
    initGrid(COLOR_OFF);
}

Launchpad::~Launchpad()
{
}

//////////////////////////////////////////////////////////////////////
//
// Generic Grid
//
//////////////////////////////////////////////////////////////////////

PRIVATE void Launchpad::initButtons(int color)
{
    for (int button = 0 ; button < TOP_BUTTONS ; button++)
      mButtons[button] = color;
}

PRIVATE void Launchpad::initGrid(int color)
{
    for (int cell = 0 ; cell < GRID_CELLS ; cell++)
      mGrid[cell] = color;
}

PRIVATE void Launchpad::resetLaunchpad()
{
    MobiusContext* con = mMobius->getContext();
    MidiInterface* midi = con->getMidiInterface();
    MidiEvent* event = midi->newEvent(MS_CONTROL, 0, 0, 0);
    midi->send(event);
    event->free();
}

PRIVATE void Launchpad::setGridMappingMode(bool drum)
{
    MobiusContext* con = mMobius->getContext();
    MidiInterface* midi = con->getMidiInterface();
    int value = (drum) ? 2 : 1;
    MidiEvent* event = midi->newEvent(MS_CONTROL, 0, 0, value);
    midi->send(event);
    event->free();
}

//////////////////////////////////////////////////////////////////////
//
// Export
//
//////////////////////////////////////////////////////////////////////

/**
 * Called periodically to send Mobius runtime state to the launchpad.
 */
PUBLIC void Launchpad::refresh()
{
    if (!mInitialized) {
        // clear out everything so incremental updates aren't fooled
        // by false positives
        initButtons(COLOR_BUTTON_DEFAULT);
        initGrid(COLOR_OFF);

        sendButtons();
        sendGrid();
        mInitialized = true;
    }
    
    refreshPage();
}

PRIVATE void Launchpad::refreshPage()
{
    switch (mPage) {
        case PAGE_SESSION:
            refreshSession();
            break;
        case PAGE_USER1:
            refreshUser1();
            break;
        case PAGE_USER2:
            refreshUser2();
            break;
        case PAGE_MIXER:
            refreshMixer();
            break;
    }   
}

PRIVATE void Launchpad::sendButton(int button, int color)
{
    int cc = BUTTON_BASE + button;

    MobiusContext* con = mMobius->getContext();
    MidiInterface* midi = con->getMidiInterface();
    MidiEvent* event = midi->newEvent(MS_CONTROL, 0, cc, color);
    midi->send(event);
    event->free();
}

PRIVATE void Launchpad::sendButtons()
{
    for (int button = 0 ; button < TOP_BUTTONS ; button++) {
        sendButton(button, mButtons[button]);
    }
}

PRIVATE void Launchpad::refreshButton(int button, int color)
{
    if (mButtons[button] != color) {
        sendButton(button, color);
        mButtons[button] = color;
    }
}

PRIVATE void Launchpad::refreshPageButton(int page)
{
    refreshPageMutex(page, COLOR_BUTTON_SELECTED);
}

PRIVATE void Launchpad::refreshPageMutex(int page, int color)
{
    for (int i = 0 ; i < 4 ; i++) {
        int button = i + BUTTON_SESSION;
        int buttonColor = ((i == page) ? color : COLOR_BUTTON_DEFAULT);
        refreshButton(button, buttonColor);
    }
}

PRIVATE void Launchpad::refreshSubPageButton(int page)
{
    refreshSubPageMutex(page, COLOR_BUTTON_SELECTED);
}

PRIVATE void Launchpad::refreshSubPageMutex(int page, int color)
{
    for (int i = 0 ; i < 4 ; i++) {
        int cell = ARROW_CELL_COLUMN + (i * GRID_COLUMNS);
        int cellColor = ((i == page) ? color : COLOR_BUTTON_DEFAULT);
        refreshCell(cell, cellColor);
    }
}

PRIVATE void Launchpad::refreshArrows(int offset, int color)
{
    for (int i = offset ; i < GRID_ROWS ; i++) {
        int cell = ARROW_CELL_COLUMN + (i * GRID_COLUMNS);
        refreshCell(cell, color);
    }
}

PRIVATE void Launchpad::refreshArrows(int color)
{
    refreshArrows(0, color);
}

PRIVATE void Launchpad::sendCell(int cell, int color)
{
    int key = cellToKey(cell);

    MobiusContext* con = mMobius->getContext();
    MidiInterface* midi = con->getMidiInterface();
    MidiEvent* event = midi->newEvent(MS_NOTEON, 0, key, color);
    midi->send(event);
    event->free();
}

PRIVATE void Launchpad::sendGrid()
{
    for (int cell = 0 ; cell < GRID_CELLS ; cell++) {
        sendCell(cell, mGrid[cell]);
    }

}

PRIVATE void Launchpad::refreshCell(int cell, int color)
{
    if (mGrid[cell] != color) {
        sendCell(cell, color);
        mGrid[cell] = color;
    }
}

PRIVATE void Launchpad::refreshGrid(int color)
{
    for (int i = 0 ; i < GRID_CELLS ; i++) 
      refreshCell(i, color);
}

PRIVATE void Launchpad::refreshInnerGrid(int color)
{
    for (int row = 0 ; row < GRID_ROWS ; row++) {
        for (int col = 0 ; col < INNER_GRID_COLUMNS ; col++) {
            int cell = (row * GRID_COLUMNS) + col;
            refreshCell(cell, color);
        }
    }
}

PRIVATE void Launchpad::refreshColumn(int column, int row, int span, int color)
{
    for (int i = 0 ; i < span ; i++) {
        int r = row + i;
        if (r < GRID_ROWS) {
            int cell = (GRID_COLUMNS * r) + column;
            refreshCell(cell, color);
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
// MIDI Events
//
//////////////////////////////////////////////////////////////////////

/**
 * All events come in on channel 1, need a way to map this!
 *
 * In X/Y layout the range is 0-119, there are 8 left at the top
 * that we could let pass but it may be confusing to pass some but
 * not others?
 *
 * We shouldn't see MS_NOTEOFF but eat them anyway in case a keyboard
 * or footswitch is sending and we're intercepting the MS_NOTEONS.
 *
 * The LP only sends a small range of CC's so we'll let the others pass,
 * this would at least let you use common pedal bindings for volumn etc.
 * without worrying about the channel.
 */
bool Launchpad::handleEvent(MidiEvent* event)
{
    bool handled = false;

    // todo: need channel mapping somewhere
    if (event->getChannel() == 0) {
        int status = event->getStatus();

        // shouldn't see NOTEOFF but since we're capturing all on's
        // eat the off's too in case we have a footswitch or keyboard
        // trying to send stuff on the same channel
        if (status == MS_NOTEON || status == MS_NOTEOFF) {
            // assumign X/Y layout
            int cell = keyToCell(event->getKey());
            if (status == MS_NOTEON && cell >= 0) {
                bool down = (event->getVelocity() > 0);
                handleGridButton(cell, down);
                handled = true;
            }
            else {
                // NOTEOFF or key in the dead zone, ignore but don't pass
                handled = true;
            }
        }
        else if (status == MS_CONTROL) {
            int cc = event->getKey();
            if (cc >= BUTTON_BASE && cc <= BUTTON_LAST) {
                int button = cc - BUTTON_BASE;
                bool down = (event->getVelocity() > 0);
                handleTopButton(button, down);
                handled = true;
            }
            else {
                // not one of ours, let it pass
            }
        }
    }

    return handled;
}

/**
 * Convert a MIDI key number into our internal cell number.
 * This saves having to deal with gaps in the cell range and makes
 * it clearer when we receive keys in the "dead zone".
 */
PRIVATE int Launchpad::keyToCell(int key)
{
    int cell = -1;
    int row = key / NATIVE_GRID_COLUMNS;
    int col = key % NATIVE_GRID_COLUMNS;

    if (row < GRID_ROWS && col < GRID_COLUMNS)
      cell = (row * GRID_COLUMNS) + col;

    return cell;
}

PRIVATE int Launchpad::cellToKey(int cell)
{
    int row = cell / GRID_COLUMNS;
    int col = cell % GRID_COLUMNS;
    return (NATIVE_GRID_COLUMNS * row) + col;
}

/**
 * Value is always zero for up and 127 for down.
 * We don't have any SUS functions in the default bindings.
 */
PRIVATE void Launchpad::handleTopButton(int button, bool down)
{
    if (down) {
        if (button >= BUTTON_SESSION) {
            mPage = button - BUTTON_SESSION;
            refreshPage();
        }
    }
}

/**
 * Value is always zero for up and 127 for down.
 * We don't have any SUS functions in the default bindings.
 */
PRIVATE void Launchpad::handleGridButton(int cell, bool down)
{
    bool flash = true;

    int row = cell / GRID_COLUMNS;
    int col = cell % GRID_COLUMNS;

    switch (mPage) {
        case PAGE_SESSION: {
            if (row < mSessionLoops && col < mSessionTracks) {
                flash = false;
                if (down) {
                    // figure out to call TrackN and LoopTrigger
                    // from here, and if we need different functions?
                }
            }
        }
        break;
        case PAGE_MIXER: {
            if (col == ARROW_CELL_COLUMN) {
                // arrow buttons on the right, sub page selector
                // only four sub pages right now
                if (row < 4) {
                    flash = false;
                    if (down) {
                        mMixerPage = row;
                        refreshPage();
                    }
                }
            }
            else {
                Track* track = mMobius->getTrack(col);
                if (track != NULL && down) {
                    flash = false;
                    int value = rowToFader(row);
                    switch (mMixerPage) {
                        case PAGE_MIXER_VOLUME:
                            track->setOutputLevel(value);
                            break;
                        case PAGE_MIXER_PAN:
                            value = rowToPan(row);
                            track->setPan(value);
                            break;
                        case PAGE_MIXER_SEND_A:
                            track->setFeedback(value);
                            break;
                        case PAGE_MIXER_SEND_B:
                            track->setAltFeedback(value);
                            break;
                    }
                }
            }
        }
        break;
    }

    if (flash) {
        if (down) {
            sendCell(cell, COLOR_RED_HIGH);
        }
        else {
            sendCell(cell, mGrid[cell]);
        }
    }
}


/**
 * Formula here has to match faderToRow.
 * There are 7 "units" of 18 each, top unit must round up to 127.
 */
PRIVATE int Launchpad::rowToFader(int row) 
{
    int value;

    // invert the row so it grows from bottom to top
    row = (GRID_ROWS - 1) - row;

    if (row == 0) 
      value = 0;
    else if (row == 7)
      value = 127;
    else    
      value = (18 * row) - 1;

    return value;
}

PRIVATE int Launchpad::faderToRow(int value)
{
    int row = 0;
    if (value > 0) {
        int units = value / 18;
        row = units + 2;
        if (row > 8)
          row = 8;
    }

    return row;
}

/**
 * In response to a pan button, given a row number from top to bottom, 
 * return the pan value to set.  The top pad is far right (127) the
 * bottom pad is far left (0).
 *
 * There are 6 "chunks" 3 on either side and a small one in the middle
 * with the remainder. For symmetry the 3 chunks on either side of the
 * center (64) represent a span of 21 values.  The center chunk has 2 values
 * (63 & 64).  When you press a button the value becomes the high end of
 * the chunk in the direction away from the center.  So row 7 goes to zero
 * and row 0 goes to 127.
 *
 *  row 0: 107 - 127
 *  row 1: 86 - 106
 *  row 2: 65 - 85
 *  row 3 & 4: center 63-64
 *  row 5: 42 - 62
 *  row 6: 21 - 41
 *  row 7: 0 - 20
 */
PRIVATE int Launchpad::rowToPan(int row) 
{
    int value = 64;

    switch (row) {
        case 0: value = 127; break;
        case 1: value = 106; break;
        case 2: value = 85; break;
        case 3: 
        case 4: value = 64; break;
        case 5: value = 42; break;
        case 6: value = 21; break;
        case 7: value = 0; break;
    }
    
    return value;
}

/**
 * Given a pan value calculate the number of buttons on either side
 * of center we need to light up.  A negative value is pads above center
 * (toward the top) a positive value is pads below center (toward the bottom).
 * direction is ambiguous here since lower numbered cells represent
 * higher pan values.  The return value should be though of as a 
 * cell number span relative to center.
 */
PRIVATE int Launchpad::panToRow(int value)
{
    int span = 0;
    if (value > 64) {
        span = ((value - 65) / 21) + 1;
        // going up
        span = -span;
    }
    else if (value < 63) {
        span = 3 - (value / 21);
    }
    return span;
}

//////////////////////////////////////////////////////////////////////
//
// Script Invoke
//
//////////////////////////////////////////////////////////////////////

/**
 * Temporary test interface called from scripts.
 */
void Launchpad::scriptInvoke(Action* action)
{
}

//////////////////////////////////////////////////////////////////////
//
// Page: Session
//
//////////////////////////////////////////////////////////////////////

PRIVATE void Launchpad::refreshSession()
{
    refreshPageButton(PAGE_SESSION);
    refreshArrows(COLOR_OFF);

    int tcount = mMobius->getTrackCount();

    for (int t = 0 ; t < INNER_GRID_COLUMNS ; t++) {
        Track* track = NULL;
        if (t < tcount)
          track = mMobius->getTrack(t);

        if (track == NULL) {
            for (int i = 0 ; i < mSessionLoops ; i++) {
                int cell = (i * GRID_COLUMNS) + t;
                refreshCell(cell, COLOR_OFF);
            }
        }
        else {
            int lcount = track->getLoopCount();
            for (int l = 0 ; l < mSessionLoops ; l++) {
                Loop* loop = NULL;
                if (l < lcount)
                  loop = track->getLoop(l);

                int cell = (l * GRID_COLUMNS) + t;
                int color;
                    
                if (loop == NULL)
                  color = COLOR_OFF;

                if (loop->isReset())
                  color = COLOR_LOOP_RESET;

                else if (loop->isRecording())
                  color = COLOR_LOOP_RECORD;

                else if (loop->isMute())
                  color = COLOR_LOOP_MUTE;

                else if (track->getLoop() == loop)
                  color = COLOR_LOOP_PLAY;
                    
                else 
                  color = COLOR_LOOP_FULL;
                
                refreshCell(cell, color);
            }
        }
    }

    // don't have anything for the bottom yet
    for (int row = mSessionLoops ; row < GRID_ROWS ; row++) {
        for (int col = 0 ; col < INNER_GRID_COLUMNS ; col++) {
            int cell = (row * GRID_COLUMNS) + col;
            refreshCell(cell, COLOR_OFF);
        }
    }

}

//////////////////////////////////////////////////////////////////////
//
// Page: User1
//
//////////////////////////////////////////////////////////////////////

PRIVATE void Launchpad::refreshUser1()
{
    refreshPageButton(PAGE_USER1);
    refreshGrid(COLOR_OFF);
}

//////////////////////////////////////////////////////////////////////
//
// Page: User2
//
//////////////////////////////////////////////////////////////////////

PRIVATE void Launchpad::refreshUser2()
{
    refreshPageButton(PAGE_USER2);
    refreshGrid(COLOR_OFF);
}

//////////////////////////////////////////////////////////////////////
//
// Page: Mixer
//
//////////////////////////////////////////////////////////////////////

PRIVATE void Launchpad::refreshMixer()
{
    refreshPageButton(PAGE_MIXER);
    refreshSubPageButton(mMixerPage);
    refreshArrows(4, COLOR_OFF);

    refreshControl();
}

PRIVATE void Launchpad::refreshControl()
{
    int tcount = mMobius->getTrackCount();

    for (int t = 0 ; t < INNER_GRID_COLUMNS ; t++) {
        Track* track = NULL;
        if (t < tcount)
          track = mMobius->getTrack(t);

        if (track == NULL) {
            for (int row = 0 ; row < GRID_ROWS ; row++) {
                int cell = (row * GRID_COLUMNS) + t;
                refreshCell(cell, COLOR_OFF);
            }
        }
        else if (mMixerPage == PAGE_MIXER_PAN) {

            refreshPan(t, track->getPan(), COLOR_YELLOW_HIGH);
        }
        else {
            int level = 0;
            int color = COLOR_GREEN_HIGH;
            switch (mMixerPage) {
                case PAGE_MIXER_VOLUME:
                    level = track->getOutputLevel();
                    break;
                case PAGE_MIXER_SEND_A:
                    level = track->getFeedback();
                    color = COLOR_RED_HIGH;
                    break;
                case PAGE_MIXER_SEND_B:
                    level = track->getAltFeedback();
                    color = COLOR_YELLOW_HIGH;
                    break;
            }
            refreshFader(t, level, color);
        }
    }
}

/**
 * Live divdes the range into 7 units, with about 18 values per unit.
 * At zero all lights are off.  From 1 to 17 the first two lights are on
 * from 18 to 35 three lights are on...
 */
PRIVATE void Launchpad::refreshFader(int column, int value, int color)
{

    int row = faderToRow(value);
    int i;

    int unlit = GRID_ROWS - row;
    for (i = 0 ; i < unlit ; i++) {
        int cell = (GRID_COLUMNS * i) + column;
        refreshCell(cell, COLOR_OFF);
    }

    for (i = unlit ; i < GRID_ROWS ; i++) {
        int cell = (GRID_COLUMNS * i) + column;
        refreshCell(cell, color);
    }
}

PRIVATE void Launchpad::refreshPan(int column, int value, int color)
{
    int rows = panToRow(value);

    if (rows < 0) {
        // going up 
        rows = -rows;
        int prefix = 3 - rows;
        refreshColumn(column, 0, prefix, COLOR_OFF);
        int span = rows + 2;
        refreshColumn(column, prefix, span, color);
        refreshColumn(column, 5, 3, COLOR_OFF);
    }
    else {
        // going down, always paint the two rows in the center
        int span = rows + 2;
        refreshColumn(column, 0, 3, COLOR_OFF);
        refreshColumn(column, 3, span, color);
        int remainder = 3 + span;
        refreshColumn(column, remainder, 8 - remainder, COLOR_OFF);
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
