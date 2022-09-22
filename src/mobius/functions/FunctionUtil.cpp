/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Misc utilities needed by several functions.
 *
 * We don't have a good way to share these except by inheritance and
 * they don't apply to everything.  They're relatively generic too
 * so keep them isolated until we can find a better home.
 *
 */

#include <stdio.h>
#include <memory.h>
#include <string.h>

#include "Util.h"
#include "MidiByte.h"

#include "Action.h"
#include "Function.h"
#include "Loop.h"
#include "Resampler.h"

//////////////////////////////////////////////////////////////////////
//
// Rescale
//
//////////////////////////////////////////////////////////////////////

/**
 * This is used by SpeedFunction and PitchFunction to rescale
 * an action value based on track-specific ranges.  We could have
 * done this when the Action was scheduled but that would require
 * more Function extensions.  Instead the Action will be created
 * with the default range, then at the time we're ready
 * to convert that to an Event we'll rescale the value.
 *
 * halfRange is expected to be one of the semitone range
 * parmeters: speedStepRange, speedBendRange, etc.
 * 
 * Bend targets are weird.  Step target values have a  clear
 * meaning, they're positive or negative semitones.  Bend targets
 * have a hard coded range that matches the MIDI pitch bend 
 * range of 16384.  So even though we specify range constraints
 * in semitones, that isn't the actual target range we still have
 * to calculate something that fits within 16384, we'll just narrow
 * the window around the center.
 *
 * This is actually wrong, it would be better to smooth out
 * the trigger values over the constrained range but that would
 * require recalculating some roots and powers in Resampler every time.
 * THINK!!
 */
PUBLIC bool RescaleActionValue(Action* action, Loop* loop,
                               int halfRange,
                               bool bend,
                               int* value)
{
    bool rescaled = false;

    if (action->trigger == TriggerMidi) {

        int status = action->getMidiStatus();
        if (status == MS_CONTROL || status == MS_BEND) {

            // ignore if invalid range
            int max = (bend) ? MAX_BEND_STEP : MAX_RATE_STEP; 
            if (halfRange > 0 && halfRange <= max) {
                rescaled = true;

                int original = action->triggerValue;
                float triggerRange = 128.0f;
                if (status == MS_BEND)
                  triggerRange = 16384.0f;

                if (bend) {
                    // because of the silly offset center
                    // don't do this if we're already at the max
                    if (halfRange < MAX_BEND_STEP) {
                        // this is the amount of change equal to one semitone
                        float semitoneUnit = (float)(RATE_BEND_RANGE / 2) / (float)MAX_BEND_STEP;
                        int newMax = (int)(semitoneUnit * (float)halfRange);
                        int newMin = -newMax;
                        int targetRange = (newMax * 2) + 1;
                        float adjust = (float)targetRange / triggerRange;
                        int offset = (int)(original * adjust);
                        *value = newMin + offset;
                    }
                }
                else {
                    // this spreads the trigger evenly
                    // over the adjusted target range
                    // remember to leave an extra spot in the center for zero
                    int targetRange = (halfRange * 2) + 1;
                    float adjust = (float)targetRange / triggerRange;
                    int offset = (int)(original * adjust);
                    *value = -halfRange + offset;
                }
            }
        }
    }
    else if (action->trigger == TriggerHost ||
             action->trigger == TriggerOsc) {
        // These could be rescaled like MIDI CCs
        // but the original values are different.
        // punt for now and use the default range.
    }

    return rescaled;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
