/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Runtime parameters for the VST plugin.
 * These are defined in VstPlugin.h but defined in VstConfig and VstConfig2
 * so we can compile the same plugin with different pin counts.
 *
 * KLUDGE: This would be cleaner with different -D options, but really
 * now that we can read pin counts from mobius.xml we don't 
 * need this any more.
 *
 * UPDATE: This is no longer necessary, we only build one pludgin DLL
 * and rely on dynamci configuration to control the number of pins.
 *
 * Version 2 note!
 *
 * Bidule at least requires that VstUniqueId be different for all plugins
 * so we can't use "Mobi" any more in version 2.
 */

// this is what Bidule shows, so keep it versioned
const char* VstProductName = "Mobius 2";

long VstUniqueId = 'Mob2';
int VstInputPins = 16;
int VstOutputPins = 16;

