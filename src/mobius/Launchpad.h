/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * An implementation of ControlSurface to interface with 
 * the Novation Launchpad.
 * 
 * I wanted to hide this in Launchpad.cpp but it has to be public
 * so something (currently Mobius.cpp) can create a new instance.
 * These can't be static instalces like Functions and Modes.
 * Well I guess they could be if we only allowed one to be connected
 * at a time.
 *
 */

#ifndef LAUNCHPAD_H
#define LAUNCHPAD_H

#include "ControlSurface.h"

#define TOP_BUTTONS 8
#define GRID_ROWS 8
#define GRID_COLUMNS 9
#define GRID_CELLS GRID_ROWS * GRID_COLUMNS

class Launchpad : public ControlSurface
{
  public:

    Launchpad(class Mobius* m);
    ~Launchpad();

    // ControlSurface interface

    bool handleEvent(class MidiEvent* event);
    void refresh();
    void scriptInvoke(class Action* action);

  private:

    void handleTopButton(int button, bool down);
    void handleGridButton(int cell, bool down);
    int keyToCell(int key);
    int cellToKey(int cell);
    int getFaderValue(int row);

    int rowToFader(int row);
    int faderToRow(int value);
    int rowToPan(int row);
    int panToRow(int value);

    void sendButton(int button, int color);
    void sendButtons();
    void refreshButton(int button, int color);
    void refreshArrows(int color);
    void refreshArrows(int offset, int color);
    void refreshPageButton(int page);
    void refreshPageMutex(int offset, int color);
    void refreshSubPageButton(int page);
    void refreshSubPageMutex(int offset, int color);
    void refreshColumn(int column, int row, int span, int color);

    void sendCell(int cell, int color);
    void sendGrid();
    void refreshCell(int button, int color);
    void refreshGrid(int color);
    void refreshInnerGrid(int color);

    void initGrid(int color);
    void initButtons(int color);
    void resetLaunchpad();
    void setGridMappingMode(bool drum);

    void refreshPage();
    void refreshSession();
    void refreshUser1();
    void refreshUser2();
    void refreshMixer();
    void refreshControl();
    void refreshFader(int column, int value, int color);
    void refreshPan(int column, int value, int color);

    class Mobius* mMobius;
    bool mInitialized;
    int mPage;
    int mMixerPage;

    // session page parameters
    int mSessionTracks;
    int mSessionLoops;

    char mButtons[TOP_BUTTONS];
    char mGrid[GRID_CELLS];

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
