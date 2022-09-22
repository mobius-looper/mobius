/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 *
 * Various shared utilities for functions.
 *
 */

#ifndef FUNCTION_UTIL_H
#define FUNCTION_UTIL_H


/**
 * Used by SpeedFunction and PitchFunction to rescale step and
 * bend values based on Preset parameters.
 */
extern bool RescaleActionValue(class Action* action, class Loop* loop,
                               int halfRange, bool bend, int* value);


#endif
