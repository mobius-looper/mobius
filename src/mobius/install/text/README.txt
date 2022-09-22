Welcome to Mobius!

Mobius documentation is available on the web at:

   www.circularlabs.com

Always read the Installation Guide for details on installing
Mobius on Windows and OS X.

----------------------------------------------------------------------
Version 2.5 Release Notes
----------------------------------------------------------------------

New features:

   Loop Windowing!  If you know what this means you should like it.
   See the Loop Windowing section in the Mobius Techniques manual for
   more information.   

Bugs fixed:

  Script action buttons always behaving like !momentary scripts

  GlobalReset called from within a script was not canceling other scripts

  Take out the common bindings used for development since they often
  conflict with new user bindings.
  
  Remove some EDPisms that most people don't expect.  These can be restored     
  with an special parameter if you really want them.

    Mute+Multiply = Realign
    Mute+Insert = RestartOnce (aka SamplePlay)
    Reset+Mute = previous preset
    Reset+Insert = next preset

----------------------------------------------------------------------
Version 2.4 Release Notes
----------------------------------------------------------------------

Bugs fixed:

  Slow memory leak on Mac when drawing text in the UI

  Large Memory leak if Max Redo Count is more than 1

  Changing "Beats per Bar" unstable with mouse

  Input Port and a few other parameters not displaying
  correctly in the main status area

  Binding dialogs always show 8 tracks and 4 groups rather than
  the number of tracks and groups configured

  Crash on Mac at startup when OSC is enabled

  Missing Mac osc.xml file

  Synchronization beats missed when Ableton transport is playing
  in a loop

New features:

  The "Periodic Status Log" global parameter enables printing
  of Mobius memory consumption once a minute for debugging

----------------------------------------------------------------------
Version 2.3 Release Notes
----------------------------------------------------------------------

The focus of this release was fixing bugs reported in the 2.2 release
related to continuous speed and rate shifting, and a few assorted old bugs.

In addition the OSC interface has been completely redesigned and a new
manual has been written and is available on the documentataion page.

Bugs fixed:

  - SpeedBend can now be set as a control in scripts: set speedBend 0

  - CC bindings to Auto Record Bars and other integer parameters should work

  - Empty Loop Action and a few other parameter can now be bound

  - Midi Binding to Setup should now work

  - Custom bend ranges were not active until a preset was edited

  - Custom bend range could not reach the maximum value with a MIDI CC

  - Custom Speed Bend Range was wonky when bound to a MIDI pitch wheel

  - Spread function ranges on different channels won't cancel each other

  - MuteOff will always force mute off instead of toggle

New features:

  - Interrupt statement can be used to cancel a script wait

  - Instant Multiply 2 is now just "Instant Multiply" and can take a 
    binding argument

  - Instant Divide 2 is now just "Instant Divide" and can take a 
    binding argument

  - Binding arguments can be enumerated parameter names, "subcycle" "off" etc.

  - "ReloadScripts" function can be used to reload all scripts from
    a menu or a MIDI event

  - Setup, Preset, and Bindings are now parameters that may be bound
    to a MIDI CC to sweep through the values

  - "UIRedraw" function can be used in scripts or bound to a switch
    to force a redisplay of the UI

  - Redesigned OSC support

----------------------------------------------------------------------
Version 2.2 Release Notes
----------------------------------------------------------------------

The focus of this release was a redesign of the speed and pitch
functions and controls.  Of particular interest is the new "Speed Bend"
control that can be used to change the playback rate smoothly using a 
MIDI CC or pitch wheel rather than making semitone jumps.

Also of interest is the "Time Stretch" control that can smoothly
change the loop duration and tempo without changing the pitch.
Note however that since Time Stretch relies on the old pitch
shifting algorithm, there are still some remaining issues with added
latency that may make it unsuitable when synchronizing multiple loops.
We consider this a "beta" feature, but can be be fun in some
situations.

Many of the function and control names were changed during the
redesign.  In previous releases, changing both the playback speed and pitch
of the loop was referred to as "rate shift".  We now consistnetly use the 
word "speed" instead of "rate".  The following tables shows
how old speed and pitch function names have changed.

  Rate Shift     - is now Speed Step
  Rate Up        - is now Speed Up
  Rate Down      - is now Speed Down
  Rate Next      - is now Speed Next
  Rate Previous  - is now Speed Previous
  Rate Normal    - is now Speed Cancel
  Speed          - is now Speed Toggle
  Sustain Speed  - is now Sustain Speed Toggle
  Fullspeed      - is now Speed Cancel
  Halfspeed      - is still Halfspeed
  Pitch Shift    - is now Pitch Step
  Pitch Normal   - is now Pitch Cancel


The following track controls have been added:

  Speed Octave 
  Speed Step 
  Speed Bend 
  Pitch Octave 
  Pitch Step 
  Pitch Bend 
  Time Stretch 

Please read the new Speed Shift, Pitch Shift, and Time Stretch sections
in the Mobius Techniques manual for full details.  

Other new features:

   - Function: UI Redraw
       Can be used to repaint the UI from a MIDI footswitch or key

   - Component: Track Strip 2
       There are now two "floating" track strips that can be added
       to the status area.  The original one normally contains the
       track controls such as Input Level and Output Level.  With the
       addition of the speed and pitch controls you may need more
       controls displayed than will fit.  The Track Strip 2 control
       may be added to provide another horizontal column of track controls.
       
   - In the track setup, it is possible to select [none] for the preset
     which means changing the setup will not change the current preset.

Bugs fixed:

  - Active setup is now restored when Mobius is restarted

  - Version number displayed in OS X Finder is accurate
  
  - Max Sync Drift can be set lower than 512

  - EmptyTrackAction=Record not working

  - Fix OSC bindings to functions

  - Strange behavior after using Instant Multiply 

----------------------------------------------------------------------
Version 2.1 Release Notes
----------------------------------------------------------------------

  - Loops can be loaded into any loop of any track

  - Fix missing display of loop length in the track strip

  - Binding list on the left side of the binding window is sorted

  - The Audio Unit parameter for Rate Shift can now be bound to a 
    continuous controller to sweep through the possible values rather
    than being a simple on/off button

  - Bindigns for setups, presets, and binding overlays now work

  - Preset bindings now obey group scope and focus lock

  - The loop meter doesn't partially display when not selected

  - Fixed EmptyLoopAction=CopyTiming and TimeCopyMode=Multiply
    with new track gets into a strange state

  - Track Leave Action parameter added

  - Fixed crash receiving OSC messages

  - Removed excessive "Correcting sync event" console log messages

  - Fixed crash undoing stacked events with Next Loop awaiting confirmation

  - Fixed behavior of InstantMultiply bound to MIDI note

  - Fixed Solo not following track specific binding scope

  - Fixed crash loading project or loop files with paths containing the & character

  - Fixed MIDI bindigns of the same note/control to several tracks not working


Track Leave Action

The new parameter "Track Leave Action" determines what happens to the current track
when the track selection is changed.  The possible values are:

    None
    Cancel Current Mode
    Cancel and Wait

When the value is "None" the track selection is changed without
effecting the current track.  If the track is recording, muted, etc. it
will continue in that mode.

When the value is "Cancel Current Mode" the mode active in the
current track will be canceled and the track returned to "Play" mode.
Note that for modes that do rounding like "Mulitply" the actual ending
of the mode may happen long after the track selection has changed.

When the value is "Cancel and Wait" the mode active in the
current track will be canceled, and Mobius will wait for the track to 
return to "Play" mode before changing tracks.

Before release 2.1 the behavior was "None".  The new options make
it possible to gracefully shut down the current track's recording mode 
before selecting a new track since recording is normally intended for
only one track at a time.

----------------------------------------------------------------------
Version 2.0 Release Notes
----------------------------------------------------------------------

This release has been in development for over a year and contains
a large number of changes to improve synchronization.  Due to the
number of changes it is recommended that you do not upgrade unless
you have time test the new version.  The 2.0 release will be installed
in a different location than the 1.45 release so you can use both 
at the same time and revert back to 1.45 if you encounter problems.

See this document on the web site for full information on this release.

   www.circularlabs.com/doc/v2/release.htm

