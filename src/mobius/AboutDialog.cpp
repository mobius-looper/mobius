/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 */

#include <stdio.h> 
#include <stdlib.h>
#include <math.h>

#include "Util.h"
#include "QwinExt.h"

#include "UI.h"

#ifdef WIN64
	#define VERSION "M\u00F6bius version 2.6.0 [alpha Build 3 64bit / 2023]"
#else
	#define VERSION "M\u00F6bius version 2.6.0 [Beta 2 x86 / 2023]"
	#define V_CAS "Beta 2 | Build #023 - 18/06/2023 | ClaudioCas"
#endif

PUBLIC AboutDialog::AboutDialog(Window* parent)
{
	setParent(parent);
	setModal(true);
	setIcon("Mobius");
	//setTitle("About M�bius");
	setTitle("About Mobius");
	setInsets(20, 20, 20, 0);

	Panel* root = getPanel();

	Panel* text = new Panel();
	root->add(text, BORDER_LAYOUT_CENTER);
	text->setLayout(new VerticalLayout());

	text->add(new Label(VERSION));
	text->add(new Label("Copyright (c) 2005-2012 Jeffrey S. Larson"));
	text->add(new Label(V_CAS));
#ifdef WIN64
	text->add(new Label("64bit Porting | Christopher Lunsford"));
#endif
	text->add(new Label("All rights reserved."));

    // TODO: Need to credit Oli for pitch shifting, link
    // to LGPL libraries for relinking
}

PUBLIC const char* AboutDialog::getCancelName()
{
	return NULL;
}

PUBLIC AboutDialog::~AboutDialog()
{
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/


/*
	ClaudioCas Notes History
	
	Build 2|23 - 14/04/2023
	- #001 Fix Reverse in Load/Save Mobius Project 
	- #002 Fix "setup" while loading Mobius Project - Now the setup is set correctly 

	Build 3|23 - 07/05/2023
	- #003 Configurable PUBLIC void TrackStrip::updateConfiguration (Altezza TrackStrip Meter 75/10)
	Ho dei memory heap corruption e non riscrive l'xml della mia parte.
	Ho ripristinato la versione git e ora sto cercando di riportare le modifiche nella speranza di non rifare gli stessi errori
	Ma non funziona
	----- To Recheck!

	Build 4|23 - 11/05/2023 - La giornata delle brutte notizie
	- #004 Radar Diameter / Level meter height on UI.xml
	- #005 Increase Messages Lenght to 50
	
	- #007 Attempt to repaint background when change forecolor on loopRadar... | Non va... si blocca e non capisco come mai
		Update: HeapCorruption and Blocks -> I need to clean the obj and recompile to avoid some memory issues...
		So if I add a variable in qwin I need to clean and recompile manually all the library
		So now the 007 bug is resolved!
		I will try to recompile also my UIDimensions Modification

	- #008 Get Configuration from Current Directory and not from Registry (Works with Vst DLL and Standalone Exe)
	- #009 Overlap counter EDP Issue

	- WORKING // #010 Audio Meter Height Default  
	- WORKING // #011 Test per Size LoopMeter...
	- #012  Beater Diameter  -> Esporre setDiameter per il Beater e impostarlo da config
	- WIP #013 Layer Bars Size!

	Buid 5|23 - 15/05/2023
	- #014 Midi Out and VST Fixed! HostMidiInterface 
	
	16/05/2023 MidiOut still buggy :D
	- #014b Bug MidiVstHostOut, -> copy midiEvent + right reference in queue
	- #015 PeakMeter in AudioMeter?

	//#016 Fix flickering background AudioMeter      TODO:: #SPC uso spacing parameter, dovrei usare uno più appropriato

	// 17/06/2023
	//#017 Reordered File Menu
	//#018 Reordered Config Menu
	//#019/#020 Moved Config Track Setups and Presets on other menu (+fix selected index + offset)
	//#020 Moved Config Presets Item

	// 18/06/2023
	//#021 | Moved "Reload Scripts and OSC" in Configuration Menu
	//#022 | Wrong menu + window size at first open? | a Thread Sleep Walkaround...
	//#023 | Bug - rel#002 : the setup was corretly set in engine but not in UI, so if you saved the project after loaded, it saved wrong setup. Now fixed in LoadProject from UI menu and from Script
	

	#C?  - Question 



/*
	//Message Dialog example
    MessageDialog* d = new MessageDialog(mWindow);
	d->setTitle(cat->get(MSG_ALERT_CONFIG_FILE));
	d->setText(mUIConfig->getError());
	d->show();
	delete d;
*/
