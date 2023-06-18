# mobius
This is my github about Mobius.

You can find more information about mobius on the [Mobius official website](https://www.circularlabs.com/).
You can keep in touch with me through the [Facebook group](https://www.facebook.com/groups/mobiuscentral).


# Version 2.6.0 Beta 2 18/06/2023
- Mod #017 | Reordered File Menu
- Mod #018 | Reordered Config Menu
- Mod #019/#020 | Moved Config Track Setups and Presets on other menu (+fix selected index + offset)
- Mod #020 | Moved Config Presets 
- Fix #021 | Moved "Reload Scripts and OSC" in Configuration Menu
- Fix #022 | Wrong menu + window size at first open? | a Thread Sleep Walkaround...
- Mod #023 | Bug - rel#002 : the setup was corretly set in engine but not in UI, so if you saved the project after loaded, it saved wrong setup. Now fixed in LoadProject from UI menu and from Script



# Version 2.6.0 Beta 1 (internal) 11/06/2023
- Fix #001 | Reverse Loop in Load/Save Mobius Project 
- Fix #002 | "setup" while loading Mobius Project was not set - Now the setup is set correctly 
- Add #003 | Add new section in ui.xml
- Add #004 | Radar Diameter and Level Meter is now configurable
- Upd #005 | Increase Messages Lenght to 50 characters
- Fix #007 | Fix issue in loopRadar when you change color of radar (some pixel still visible in wrong color due to approximation. - Now when the color is changed the black background is redrawn)
- Add #008 | Get Configuration from Current Directory and not from Registry (Works with Vst DLL and Standalone Exe)
- Fix #009 | Overlap counter EDP Issue
- Add #010 | Customizable AudioMeter Size
- Add #011 | Customizable LoopMeter Size
- Add #012 | Customizable Beater Diameter
- Add #013 | Customizable Layer Bars Size
- Fix #014 | Midi Out and VST Host Port! [HostMidiInterface] Now the midi events generated by the scripts from the MidiOut command are send to the MidiOut port of the Plugin! (not only in Plugin Output Devices set in Midi Device Selection)
- Add #015 | Implemented decay in AudioMeter (a kind of peak meter)
- Fix #016 | Fix flickering background AudioMeter

