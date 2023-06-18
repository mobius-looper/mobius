/*
 * copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * GUI for Mobius based on Qwin.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "List.h"
#include "MessageCatalog.h"
#include "Thread.h"
#include "Util.h"
#include "XmlBuffer.h"

#include "MidiInterface.h"

#include "Qwin.h"
#include "Palette.h"

#include "Action.h"
#include "Audio.h"
#include "Messages.h"
#include "MobiusInterface.h"
#include "MobiusConfig.h"
#include "Function.h"
#include "Preset.h"
#include "Project.h"
#include "Setup.h"
#include "Track.h"

#include "UIConfig.h"
#include "UITypes.h"
#include "UI.h"

QWIN_USE_NAMESPACE

#define SHOW_NEW_STUFF 1

/****************************************************************************
 *                                                                          *
 *                               MENU CONSTANTS                             *
 *                                                                          *
 ****************************************************************************/

/**
 * The base id number for items in the Presets menu.
 */
#define PRESET_MENU_BASE 100

/**
 * The base id number for items in the track setup menu.
 */
#define SETUP_MENU_BASE 200

#define IDM_NEW 1
#define IDM_OPEN_PROJECT 2
#define IDM_OPEN_LOOP 3
#define IDM_SAVE_PROJECT 4
#define IDM_SAVE_TEMPLATE 5
#define IDM_SAVE_LOOP 6
#define IDM_SAVE_QUICK 8
#define IDM_SAVE_UNUSED 9
#define IDM_EXIT 10
#define IDM_FILE_SCRIPTS 11
#define IDM_FILE_OSC 12

#define IDM_PRESET 20
#define IDM_MIDI_CONTROL 21
#define IDM_KEY_CONTROL 22
#define IDM_BUTTONS 23
#define IDM_SCRIPTS 24
#define IDM_MIDI 25
#define IDM_AUDIO 26
#define IDM_GLOBAL 27
#define IDM_DISPLAY 28
#define IDM_PALETTE 29
// former EDPDialog no longer used
#define IDM_EXTERNAL 30     
#define IDM_FULLSCREEN 31
#define IDM_SAMPLES 32
#define IDM_PORTS 33
#define IDM_SETUP 34
#define IDM_PLUGIN_PARAMETERS 35
#define IDM_KEYS 36

#define IDM_HELP_KEY 40
#define IDM_HELP_MIDI 41
#define IDM_HELP_ABOUT 42
#define IDM_HELP_REDRAW 43

/****************************************************************************
 *                                                                          *
 *                                   FRAME                                  *
 *                                                                          *
 ****************************************************************************/

static void uitrace(const char* msg) 
{
	if (false)
	  Trace(2, msg);
}

UIFrame::UIFrame(Context *con, MobiusInterface* mobius) : Frame(con)
{
	uitrace("Initializing UIFrame\n");

	mFullScreen = false;

	// SawStudio has the unusual habit of allowing the editor window
	// to be opened before the resume() method.  Since we're deferring
	// a lot of initialization until resume(), call the Mobius start()
	// method to make sure it has been done by now.
	mobius->start();

	MessageCatalog* cat = mobius->getMessageCatalog();
	setTitle(cat->get(MSG_MOBIUS));

    setLayout(new BorderLayout());
	setIcon("Mobius");
    setFocusRequested(true);
	setBackground(GlobalPalette->getColor(COLOR_SPACE_BACKGROUND));

	mUI = new UI(mobius);
	mUI->open(this, false);

}

UIFrame::~UIFrame()
{
	delete mUI;
}

void UIFrame::prepareToDelete()
{
    if (mUI != NULL)
      mUI->prepareToDelete();
}

/**
 * Overload this Window method to start the timer after we've
 * finished opening.  If the timer fires too soon we think we've
 * updated state when it wasn't actually visible.
 * !! There is also a WindowListener event for this, don't especially
 * like having two mechanisms.  Consider getting rid of this.
 */
PUBLIC void UIFrame::opened()
{
	mUI->opened();
}

/**
 * Overload this Window method to save our final window size
 * before exiting.
 */
PUBLIC void UIFrame::closing()
{
	mUI->saveLocations();
}

//////////////////////////////////////////////////////////////////////
//
// UI
//
//////////////////////////////////////////////////////////////////////

/**
 * UI construction has two phases like Mobius.
 * In the first phase we initialize ourselves enough to read the
 * configuration file but we do not have a window.
 * The second phase creates the window.  This is necessary for use
 * under VST where we need to tell the host how large to 
 * create the window (which is in the configuration file) before
 * the window has been created by the host.
 */
PUBLIC UI::UI(MobiusInterface* mobius)
{
	int i;

	uitrace("Initializing UI\n");

	mWindow = NULL;
	mMobius = mobius;
	mUIConfigFile = NULL;
	mUIConfig = NULL;
	mButtons = NULL;

	mPresets = NULL;
	mPopupPresets = NULL;
	mSetups = NULL;
	mPopupSetups = NULL;
	
	mMenuBar = NULL;
	mPopup = NULL;
	mKeyHelpDialog = NULL;
	mMidiHelpDialog = NULL;
    mDialogs = new List();
	mTimer = NULL;
	mMidiEventListener = NULL;

	mSpace = NULL;
	mFloatingStrip = NULL;
	mFloatingStrip2 = NULL;
	mMeter = NULL;
	mStatus = NULL;
	mLoopMeter = NULL;
    mLoopWindow = NULL;
	mCounter = NULL;
	mBeaters = NULL;
	mLoopList = NULL;
	mLayerList = NULL;
	mTrackGrid = NULL;

	mTracks = NULL;
    mTrackCount = 0;
	mParameters = NULL;
	mModes = NULL;
	mSync = NULL;
	mStatusBar = NULL;
	mAlert = NULL;
	mMessages = NULL;

	mPrompts = NULL;
	mPromptsTodo = NULL;
	mInvisible = NULL;

	mLastPreset = -1;
	for (i = 0 ; i < KEY_MAX_CODE ; i++)
	  mKeyState[i] = 0;

	mCsect = new CriticalSection("UI");

	loadConfiguration();

    // give Mobius the definitions of UIControls we support
    mMobius->setUIBindables(UIControls, UIParameters);
}

PUBLIC UIConfig* UI::getUIConfig()
{
	return mUIConfig;
}

/**
 * Phase two of opening.
 * We have the window now, add componentry.
 * When the window eventually finishes opening the native
 * components, our opened()
 * method will be called.
 */
PUBLIC void UI::open(Window* win, bool vst)
{
	uitrace("Opening UI\n");
	
	SleepMillis(100); // #022 c: test |wrong size menu and window at first open in VST [4k + scale w10]

	//vst = true; // test... #020 - c: There is a bug - vst is always false... problaby we should use
	//    MobiusContext* mc = mMobius->getContext();
	//	  if (!mc->isPlugin())
	Trace(3,"open (vst) : %s", vst ? "true" : "false" );

	mWindow = win;
    Component::PaintTraceEnabled = mUIConfig->isPaintTrace();

    win->addKeyListener(this);

	// configuration is already loaded, check for things now that
	// we can popup dialogs
	checkDevices();

	MessageCatalog* cat = mMobius->getMessageCatalog();

	// do this after the Palette, FontConfig and other localizable
    // config objects have been loaded
	localize(cat);

    // install the global pallete and font config
    GlobalPalette->assign(mUIConfig->getPalette());
    GlobalFontConfig->assign(mUIConfig->getFontConfig());
	
	//Cas: Ok I did not implement assign, not stole, not clone... I use UiDimension in 1 point, I access directly to mConfig without global obj now
	//GlobalUiDimensions->assign(mUIConfig->getUiDimensions()); //#003 Cas: here my bug, I forgot to assign GlobalUiDimensions!


	// if we're a VST this will already have been sized
	if (!vst) {
		class Bounds* b = mUIConfig->getBounds();
        if (b == NULL)
          win->setAutoCenter(true);
        else {
            win->setBounds(new Bounds(b));
            // center the first time we open and the origin isn't configured
            // unfortunately this means you can't move it to exactly 0,0
            // and have it stick
            if (b->x == 0 && b->y == 0)
              win->setAutoCenter(true);
        }
		win->setMaximized(mUIConfig->isMaximized());

	}

	// We're using the MobiusRefresh callback now so we don't need
	// the timer.  Need to retool Beaters to not depend on this...
	mTimer = new SimpleTimer(100);
    mTimer->addActionListener(this);

	mSpace = new Space();
    mSpace->setPreferredSize(200, 400);
	win->add(mSpace, BORDER_LAYOUT_CENTER);

	mFloatingStrip = new TrackStrip(mMobius, 0);
    mFloatingStrip->setEnabled(false);
    mSpace->add(mFloatingStrip);

	mFloatingStrip2 = new TrackStrip2(mMobius, 0);
    mFloatingStrip2->setEnabled(false);
    mSpace->add(mFloatingStrip2);

	mMeter = new AudioMeter();
    mMeter->setEnabled(false);
	mSpace->add(mMeter);

	mStatus = new ModeDisplay();
	mStatus->setEnabled(false);
	mStatus->setValue(cat->get(MSG_MODE_RESET));
	mSpace->add(mStatus);

	// boolean args request tick marks and event markers
	mLoopMeter = new LoopMeter(true, true);
	mLoopMeter->setEnabled(false);
	mSpace->add(mLoopMeter);

	// boolean args request tick marks and event markers
	mLoopWindow = new LoopWindow();
	mLoopWindow->setEnabled(false);
	mSpace->add(mLoopWindow);

	mCounter = new EDPDisplay(mMobius->getSampleRate());
	mCounter->setEnabled(false);
	mSpace->add(mCounter);

    // even though they don't update dynamically, they need a timer
    // for decay
    mBeaters = new Beaters(mTimer);
    mBeaters->setEnabled(false);
    mSpace->add(mBeaters);

	mLoopList = new LoopList();
    mLoopList->setEnabled(false);
	mSpace->add(mLoopList);

	mLayerList = new LayerList();
    mLayerList->setEnabled(false);
	mSpace->add(mLayerList);

    // this isn't actually shown, but we use it to record the enable state
	//mAlert = new PresetAlert();
	//mAlert->setName(DisplayPresetAlert.name);
	//mSpace->add(mAlert);

	// new replacement for mAlert
	mMessages = new MessageArea();
    mMessages->setEnabled(false);
    mMessages->setDuration(mUIConfig->getMessageDuration());
	mSpace->add(mMessages);

	mParameters = new ParameterDisplay(mMobius);
    mParameters->setEnabled(false);
	mSpace->add(mParameters);

	mModes = new ModeMarkers();
    mModes->setEnabled(false);
	mSpace->add(mModes);

	mSync = new SyncMarkers();
    mSync->setEnabled(false);
	mSpace->add(mSync);

	// never used this?
    //mStatus = new StatusBar();
    //add(mStatus, BORDER_LAYOUT_SOUTH);

	// put the weird invisible buttons at the bottom
	Panel* south = new Panel("UI South");
	south->setLayout(new BorderLayout());
    win->add(south, BORDER_LAYOUT_SOUTH);

    // a row of track strips, configured later
    // get the track count config so it can expand and contract
    //mTrackGrid = new BorderedGrid(1, DEFAULT_TRACKS);
    MobiusConfig* mc = mMobius->getConfiguration();
    mTrackGrid = new BorderedGrid(1, mc->getTracks());

    mTrackGrid->setBackground(GlobalPalette->getColor(COLOR_SPACE_BACKGROUND));
    mTrackGrid->addActionListener(this);
    mTrackGrid->setInsets(0, 0, 0, 10);
	mTracks = NULL;
    mTrackCount = 0;
    south->add(mTrackGrid, BORDER_LAYOUT_CENTER);

	// this is a special button that we "click" in the MobiusThread
	// to get an event pushed into the UI thread
	// NOTE: This must have a non-zero size to get events posted.  If we put
	// this at the top of the Space, the background color isn't set
	// and it shows up as a dot.  For unknown reasons (background lingers
	// after the previous components?) it will be invisible if we put it last.
	mInvisible = new InvisibleButton();
	mInvisible->addActionListener(this);
	south->add(mInvisible, BORDER_LAYOUT_SOUTH);

	MobiusConfig* config = mMobius->getConfiguration();

    buildMenus(vst);
	buildDockedTrackStrips(config);

	// now that everything is installed can do this
	updateDisplayConfig();

	// to be informed of MIDI events and other things
	// do this AFTER we're fully initialized because MobiusThread can
	// start sending us time boundary events
	mMobius->setListener(this);

	uitrace("UI Opening complete\n");

	// until we can support sample rates other than 44.1K, popup
	// an error dialog if we notice that the host has a different rate

	AudioStream* stream = mMobius->getAudioStream();
	if (stream != NULL) {
		int rate = stream->getSampleRate();
        if (rate != CD_SAMPLE_RATE) {
            // no more warns, though could check a max rate?
            bool warn = false;
            if (warn) {
                char buf[1024];
                sprintf(buf, "WARNING: The VST host is using a sample rate of %d.\nMobius currently requires a rate of 44100 for accurate synchronization.", rate);
                MessageDialog::showError(mWindow, "Mobius Warning", buf);
                // can't trace with a stack buffer
                Trace(1, "VST host using sample rate of %ld!\n", (long)rate);
            }
            else {
				Trace(1, "VST host using sample rate of %ld!\n", (long)rate);
                printf("Mobius is starting with sample rate %d\n", rate);
                fflush(stdout);
            }
        }
    }
}

PRIVATE void UI::localize(MessageCatalog* cat)
{
	DisplayElement::localizeAll(cat);

	// Localize our private Palette before assigning to the global palette
	Palette* p = mUIConfig->getPalette();
	if (p != NULL) {
		upgradePalette(p);
		p->localize(cat);
	}

    // TODO: same for FontConfig
    FontConfig* fc = mUIConfig->getFontConfig();
    if (fc != NULL) {
        // fc->localize(cat);
    }

	// these are harder since they can't know about keys
	const char* okButton = cat->get(MSG_DLG_OK);
	const char* cancelButton = cat->get(MSG_DLG_CANCEL);
	const char* helpButton = cat->get(MSG_DLG_HELP);
	SimpleDialog::localizeButtons(okButton, cancelButton, helpButton);
}

/**
 * Runtime palette upgrades.
 */
PRIVATE void UI::upgradePalette(Palette* p)
{
	// !! actually we should be completely rebuilding the list
	// so we can filter out obsolte colors, and reorder them
	for (int i = 0 ; ColorDefinitions[i] != NULL ; i++) {
		ColorDefinition* def = ColorDefinitions[i];
		PaletteColor* color = p->getPaletteColor(def->name);
		if (color == NULL) {
            // note that PaletteColor owns it's Color object so you
            // can't call setColor with a Color constant, pass the rbg
            // and let it make its own
			color = new PaletteColor(def->name, Color::White->getRgb());
			p->add(color);
		}

        // keep these refreshed, now that we have static definitions
        // don't need to be serializing keys
        color->setKey(def->key);
	}
}

PRIVATE void UI::buildDockedTrackStrips(MobiusConfig* config)
{
	int count = config->getTracks();

	mTrackGrid->removeAll();
	delete mTracks;

	mTracks = new TrackStrip*[count];
    mTrackCount = count;

    for (int i = 0 ; i < count ; i++) {
		TrackStrip* ts = new TrackStrip(mMobius, i + 1);
		mTracks[i] = ts;
		mTrackGrid->add(ts);
    }

    mTrackGrid->setSelectedIndex(0);
}

/**
 * Disconnect anything that may be sending events to the UI
 * while we're in the process of closing.  This will be called
 * before the containing Frame is deleted.
 */
void UI::prepareToDelete()
{
	if (mMobius != NULL)
	  mMobius->setListener(NULL);

	if (mTimer != NULL) {
		// this actually doesn't do anything
		mTimer->stop();
		// this is supposed to kill it
		delete mTimer;
        mTimer = NULL;
		// turn this on if you want to see if the timer still fires, 
		// but this leaves it on for the next opening of the window
		// in VST/AU mode which emits alarming messages to the console
		//SimpleTimer::KludgeTraceTimer = true;
	}

    // try to close any open dialogs
    cancelDialogs();

	gcPrompts(true);
}

UI::~UI()
{
    // just in case it hasn't been called yet
    prepareToDelete();

	delete mUIConfigFile;
	delete mUIConfig;
	delete mTracks;
    delete mDialogs;

	// this is needed by gcPrompts so delete it last
	delete mCsect;
}

MobiusInterface* UI::getMobius()
{
	return mMobius;
}

/**
 * Called by UIFrame after we know we've been fully opened.
 * If the timer fires too soon we think we've updated state when
 * it wasn't actually visible.
 */
PUBLIC void UI::opened()
{
	if (mTimer != NULL) {
		uitrace("Starting timer\n");
		mTimer->start();
	}
	if (mWindow != NULL)
	  mWindow->invalidate();
}

/**
 * Redraw the window, sometimes necessary to remove rendering turds.
 */
PRIVATE void UI::redraw()
{
    mWindow->invalidate();
}

/****************************************************************************
 *                                                                          *
 *                                   MENUS                                  *
 *                                                                          *
 ****************************************************************************/

/**
 * The Swing way would be to give each menu item an action listener,
 * but it's harder without anonymous inner classes.  Instead we'll
 * assign each item a unique id, and put an action listener on the 
 * top-level menus.
 */
PRIVATE void UI::buildMenus(bool vst)
{
	MessageCatalog* cat = mMobius->getMessageCatalog();

	if (!vst && !mUIConfig->isNoMenu()) { // C: vst is always false...
	//if(true){
		mMenuBar = new MenuBar();
		mMenuBar->addActionListener(this);
		mMenuBar->addMenuListener(this);
		mMenuBar->add(buildFileMenu(vst));
		mSetups = new Menu(cat->get(MSG_MENU_SETUPS));
		mMenuBar->add(mSetups);
		mPresets = new Menu(cat->get(MSG_MENU_PRESETS));
		mMenuBar->add(mPresets);
		mMenuBar->add(buildConfigMenu());
		mMenuBar->add(buildHelpMenu());

		mWindow->setMenuBar(mMenuBar);
	}

	// always a popup?
    mPopup = new PopupMenu();
    mPopup->addActionListener(this);
	mPopup->addMenuListener(this);
	mPopup->add(buildFileMenu(vst));
	mPopupSetups = new Menu(cat->get(MSG_MENU_SETUPS));
	mPopup->add(mPopupSetups);
	mPopupPresets = new Menu(cat->get(MSG_MENU_PRESETS));
	mPopup->add(mPopupPresets);
	mPopup->add(buildConfigMenu());
	mPopup->add(buildHelpMenu());

    mWindow->setPopupMenu(mPopup);

    refreshSetupMenu();
    refreshPresetMenu();

}

PRIVATE Menu* UI::buildFileMenu(bool vst)
{

	//#017 Reorder File Menu 
	MessageCatalog* cat = mMobius->getMessageCatalog();

    Menu* menu = new Menu(cat->get(MSG_MENU_FILE));

	menu->add(new MenuItem(cat->get(MSG_MENU_FILE_OPEN_PROJECT), 
							IDM_OPEN_PROJECT));

    menu->add(new MenuItem(cat->get(MSG_MENU_FILE_SAVE_PROJECT),
						   IDM_SAVE_PROJECT));

	menu->addSeparator(); 

    menu->add(new MenuItem(cat->get(MSG_MENU_FILE_OPEN_LOOP),
						   IDM_OPEN_LOOP));
    
					   
    menu->add(new MenuItem(cat->get(MSG_MENU_FILE_SAVE_LOOP),
						   IDM_SAVE_LOOP));

	// we don't need project templates any more now that we have setups
    // keep around for awhile but don't advertise
    //menu->add(new MenuItem(cat->get(MSG_MENU_FILE_SAVE_TEMPLATE),
    //IDM_SAVE_TEMPLATE));

    menu->add(new MenuItem(cat->get(MSG_MENU_FILE_SAVE_QUICK),
						   IDM_SAVE_QUICK));


	//Move Reload in Configuration Menu #021
	// menu->add(new MenuItem(cat->get(MSG_FUNC_RELOAD_SCRIPTS), 
    //                        IDM_FILE_SCRIPTS));

    // menu->add(new MenuItem("Reload OSC", IDM_FILE_OSC));

	// this confuses VST, probably could be made to work but why bother
	if (!vst){ //<--- c: here vst has not the right value (is always false)...
		menu->addSeparator(); 
	  	menu->add(new MenuItem(cat->get(MSG_MENU_FILE_EXIT), IDM_EXIT));
	}

	return menu;
}

PRIVATE bool UI::isShowNewStuff()
{
    return (getenv("MOBIUS_DEVELOPMENT") != NULL);
}

PRIVATE Menu* UI::buildConfigMenu()
{
	MessageCatalog* cat = mMobius->getMessageCatalog();
    Menu* menu = new Menu(cat->get(MSG_MENU_CONFIG));

	//#018 Reorder Config  Menu 

	if (isShowNewStuff()) {
		//menu->add(new MenuItem("New Key Config", IDM_KEYS));
	}

    
	//Moved to other section (c) #019/#020
	// menu->add(new MenuItem(cat->get(MSG_MENU_CONFIG_SETUP), IDM_SETUP));
	// menu->add(new MenuItem(cat->get(MSG_MENU_CONFIG_PRESETS), IDM_PRESET));
    // menu->addSeparator(); 

	
	menu->add(new MenuItem(cat->get(MSG_MENU_CONFIG_MIDI), IDM_MIDI_CONTROL));
    menu->add(new MenuItem(cat->get(MSG_MENU_CONFIG_BUTTONS), IDM_BUTTONS));
	menu->add(new MenuItem(cat->get(MSG_MENU_CONFIG_KEYBOARD), IDM_KEY_CONTROL));
	
	menu->addSeparator(); 
    menu->add(new MenuItem(cat->get(MSG_MENU_CONFIG_DISPLAY), IDM_DISPLAY));
    menu->add(new MenuItem(cat->get(MSG_MENU_CONFIG_PALETTE), IDM_PALETTE));

	menu->addSeparator(); 
    
	
	menu->add(new MenuItem(cat->get(MSG_MENU_CONFIG_SCRIPTS), IDM_SCRIPTS));
	menu->add(new MenuItem(cat->get(MSG_FUNC_RELOAD_SCRIPTS), //#021
                           IDM_FILE_SCRIPTS));
	
	menu->addSeparator(); 
	menu->add(new MenuItem(cat->get(MSG_MENU_CONFIG_SAMPLES), IDM_SAMPLES));
	menu->add(new MenuItem("Reload OSC", IDM_FILE_OSC));//#021

    
   
	menu->addSeparator(); 


	// this is meaningless and confusing when running as a plugin
    MobiusContext* mc = mMobius->getContext();
	if (!mc->isPlugin())
	  menu->add(new MenuItem(cat->get(MSG_MENU_CONFIG_AUDIO_DEVICES),IDM_AUDIO));

    menu->add(new MenuItem(cat->get(MSG_MENU_CONFIG_GLOBAL), IDM_GLOBAL));
    menu->add(new MenuItem(cat->get(MSG_MENU_CONFIG_PLUGIN_PARAMETERS), IDM_PLUGIN_PARAMETERS));
	menu->add(new MenuItem(cat->get(MSG_MENU_CONFIG_MIDI_DEVICES), IDM_MIDI));


	return menu;
}

PRIVATE Menu* UI::buildHelpMenu()
{
	MessageCatalog* cat = mMobius->getMessageCatalog();

    Menu* menu = new Menu(cat->get(MSG_MENU_HELP));
    menu->add(new MenuItem(cat->get(MSG_MENU_HELP_KEY), IDM_HELP_KEY));
    menu->add(new MenuItem(cat->get(MSG_MENU_HELP_MIDI), IDM_HELP_MIDI));
    menu->add(new MenuItem("Refresh UI", IDM_HELP_REDRAW));
	
	menu->addSeparator(); 
    menu->add(new MenuItem(cat->get(MSG_MENU_HELP_ABOUT), IDM_HELP_ABOUT));
	
	return menu;
}

PRIVATE void UI::refreshPresetMenu()
{
	refreshPresetMenu(mPresets);
	refreshPresetMenu(mPopupPresets);
}

PRIVATE void UI::refreshPresetMenu(Menu* menu)
{
	if (menu != NULL) {
		int id = PRESET_MENU_BASE;

		menu->removeAll();
		
		
		//#019 Cas
		MessageCatalog* cat = mMobius->getMessageCatalog();
		menu->add(new MenuItem(cat->get(MSG_MENU_CONFIG_PRESETS), IDM_PRESET));
    	menu->addSeparator(); 


		MobiusConfig* config = mMobius->getConfiguration();
		for (Preset* p = config->getPresets() ; p != NULL ; p = p->getNext())
		  menu->add(new MenuItem(p->getName(), id++));
	}
}


PRIVATE void UI::refreshSetupMenu()
{
	refreshSetupMenu(mSetups);
	refreshSetupMenu(mPopupSetups);
}

PRIVATE void UI::refreshSetupMenu(Menu* menu)
{
	if (menu != NULL) {
		int id = SETUP_MENU_BASE;

		menu->removeAll();

		//#020 Menu Setups + config
		MessageCatalog* cat = mMobius->getMessageCatalog();
		menu->add(new MenuItem(cat->get(MSG_MENU_CONFIG_SETUP), IDM_SETUP));
		menu->addSeparator();


		MobiusConfig* config = mMobius->getConfiguration();
		for (Setup* s = config->getSetups() ; s != NULL ; s = s->getNext())
		  menu->add(new MenuItem(s->getName(), id++));
	}
}

/****************************************************************************
 *                                                                          *
 *                              ACTION LISTENER                             *
 *                                                                          *
 ****************************************************************************/

/**
 * Called when a menu is about to open, check selection state.
 * 
 * KLUDGE: On Windows the WM_INITMENU message seems to only be called with
 * the topmost MenuBar, on Mac we can get it on submenus but since
 * we have to pick the LCD to assingn the listener assume that it doesn't
 * matter which menu it is, refresh all of them.
 */
void UI::menuSelected(Menu* menu)	// c: Menu is not used here as parameter... 
{
	MobiusConfig* config = mMobius->getConfiguration();
    
    // note that we check the currently active preset, which may not be
    // what is selected in the MobiusConfig
	int current = mMobius->getTrackPreset();
	int index = 0;



	for (Preset* p = config->getPresets() ; p != NULL ; 
		 p = p->getNext(), index++) {

		if (p->getNumber() == current) {
			
			//c: offset Configure on top + separator //#019		 	
			index = index + 2; //#019
			
			if (mPresets != NULL)
			  mPresets->checkItem(index);
			if (mPopupPresets != NULL)
			  mPopupPresets->checkItem(index);
			break;
		}
	}


	Setup* setup = config->getCurrentSetup();
	index = 0;


	//Trace(3,"menuSelectedmenuSelectedmenuSelectedmenuSelectedmenuSelectedmenuSelected %ld", (long)setup->getNumber());

	if (setup != NULL) {
		for (Setup* s = config->getSetups() ; s != NULL ; 
			 s = s->getNext(), index++) {

			if (s->getNumber() == setup->getNumber()) {
				
				//c: offset Configure on top + separator //#020			 	
				index = index + 2; //#020

				if (mSetups != NULL) 
				  mSetups->checkItem(index);
				if (mPopupSetups != NULL)
				  mPopupSetups->checkItem(index);
				
				break;
			}
		}
	}
}

/**
 * Since we're not getting an event for both press and release of
 * the buttons, we can't implement SUS functions here.  Not so
 * bad since we'll use MIDI most of the time, but still should work.
 */
void UI::actionPerformed(void *c) 
{
	MessageCatalog* cat = mMobius->getMessageCatalog();
	char filter[1024];

    if (c == mTimer) {
		// we're using MobiusRefresh now, but leave the Timer around
		// for awhile until we're sure
        // !! sigh, still need this for the beaters
		//printf(".\n");
		//fflush(stdout);
		updateUI();
    }
	else if (c == mInvisible) {
		doInvisible();
	}
	else if (c == mTrackGrid) {
        int index = mTrackGrid->getSelectedIndex();
        if (index >= 0) {
            Action* a = new Action();
            a->setFunction(TrackN);
            a->trigger = TriggerUI;
            a->down = true;
            // expected to be 1 based
            a->arg.setInt(index + 1);
            mMobius->doAction(a);

            // HORRIBLE KLUDGE
            // It is common to click on a loop in the stack which
            // will also change the track, This Action runs first
            // but since it is queued when we schedule the loop switch
            // it happens in the current track, not the pending track.
            // Wait for the track switch to happen.  It would be best
            // if we had a way to wait on the completion of a 
            // submitted action.
            for (int i = 0 ; i < 10 ; i++) {
                if (mMobius->getActiveTrack() == index)
                  break;
                else
                  SleepMillis(100);
            }

            if (mMobius->getActiveTrack() != index)
              Trace(1, "Timeout waiting for track to change\n");
		}
	}
	else if (c == mMenuBar || c == mPopup) {
        Menu* menu = (Menu*)c;
        int id = menu->getSelectedItemId();

		if (id >= SETUP_MENU_BASE) {
            // one of the setup menu items
            int index = id - SETUP_MENU_BASE;
            Action* a = new Action();
            a->setTarget(TargetSetup);
            a->arg.setInt(index);
            // special operator to save the selected setup in the config file
            a->actionOperator = OperatorPermanent;
            mMobius->doAction(a);
		}
		else if (id >= PRESET_MENU_BASE) {
            // one of the preset menu items
            int index = id - PRESET_MENU_BASE;
            Action* a = new Action();
            a->setTarget(TargetPreset);
            a->arg.setInt(index);
            mMobius->doAction(a);
        }
        else if (id == IDM_MIDI) {
            MobiusConfig* config = mMobius->editConfiguration();
			MidiDialog* d = new MidiDialog(mWindow, mMobius, config);
            showDialog(d);
            if (d->isCanceled())
              delete config;   
            else {
                mMobius->setGeneralConfiguration(config);
                checkDevices();
            }
            delete d;
		}
		else if (id == IDM_AUDIO) {
            MobiusConfig* config = mMobius->editConfiguration();
			AudioDialog* d = new AudioDialog(mWindow,  mMobius, config);
            showDialog(d);
            if (d->isCanceled())
              delete config;
            else {
                mMobius->setGeneralConfiguration(config);
                checkDevices();
            }
			delete d;
		}
		else if (id == IDM_PLUGIN_PARAMETERS) {
            MobiusConfig* config = mMobius->editConfiguration();
			PluginBindingDialog* d = new PluginBindingDialog(mWindow, this, mMobius, config);
            showDialog(d);
            if (d->isCanceled())
              delete config;
            else
              mMobius->setBindingConfiguration(config);
			delete d;
		}
		else if (id == IDM_MIDI_CONTROL) {
            MobiusConfig* config = mMobius->editConfiguration();
			MidiBindingDialog* d = new MidiBindingDialog(mWindow, this, mMobius, config);
			showDialog(d);
            if (d->isCanceled())
              delete config;
            else
              mMobius->setBindingConfiguration(config);
			delete d;
		}
		else if (id == IDM_KEY_CONTROL) {
            MobiusConfig* config = mMobius->editConfiguration();
			KeyBindingDialog* d = new KeyBindingDialog(mWindow, this, mMobius, config);
            showDialog(d);
            if (d->isCanceled())
              delete config;
            else
              mMobius->setBindingConfiguration(config);
			delete d;
		}
		else if (id == IDM_PRESET) {
            MobiusConfig* config = mMobius->editConfiguration();
			PresetDialog* d = new PresetDialog(mWindow, mMobius, config);
            showDialog(d);
            if (d->isCanceled())
              delete config;
            else {
                mMobius->setPresetConfiguration(config);
                refreshPresetMenu();
				mParameters->refresh();
                // !! todo: initialize feedback from the new preset
                // may not want to, EDP does't let you accidentally
                // trash the feedback when switching presets
            }
			delete d;
		}
		else if (id == IDM_SETUP) {
            MobiusConfig* config = mMobius->editConfiguration();
			SetupDialog* d = new SetupDialog(mWindow, mMobius, config);
            showDialog(d);
            if (d->isCanceled())
              delete config;
            else {
                mMobius->setSetupConfiguration(config);
                refreshSetupMenu();
				mParameters->refresh(); 	// fully repaint this
				mWindow->invalidate();
            }
			delete d;
		}
		else if (id == IDM_GLOBAL) {
            // MobiusConfig must be a clone but UIConfig can be edited 
            // directly since it's only a few numeric fields
            MobiusConfig* config = mMobius->editConfiguration();
			GlobalDialog* d = new GlobalDialog(mWindow, mMobius, config, mUIConfig);
            showDialog(d);
            if (d->isCanceled())
              delete config;
            else {
                mMobius->setGeneralConfiguration(config);
                // this also saves the ui.xml file
                saveLocations();
                updateGlobalConfig();
            }
			delete d;
		}
		else if (id == IDM_DISPLAY) {
			DisplayDialog* d = new DisplayDialog(mWindow, mMobius, mUIConfig);
            showDialog(d);
            if (!d->isCanceled()) {
                // also saves the configuration file
                saveLocations();
                updateDisplayConfig();
				mWindow->invalidate();
            }
			delete d;
		}
		else if (id == IDM_BUTTONS) {
            MobiusConfig* config = mMobius->editConfiguration();
            ButtonBindingDialog* d = new ButtonBindingDialog(mWindow, this, mMobius, config);
            showDialog(d);
            if (d->isCanceled())
              delete config;
            else {
                mMobius->setBindingConfiguration(config);
                saveLocations();
                updateButtons();
            }
            delete d;
		}
		else if (id == IDM_PALETTE) {
			Palette* p = mUIConfig->getPalette();
			PaletteDialog* pd = new PaletteDialog(mWindow, p);
			// ugh, could set these statically like the buttons?
			MessageCatalog* cat = mMobius->getMessageCatalog();
			const char* pTitle = cat->get(MSG_DLG_PALETTE_TITLE);
			const char* cTitle = cat->get(MSG_DLG_PALETTE_COLOR);
			pd->localize(pTitle, cTitle);
            showDialog(pd);
			if (!pd->isCanceled()) {
                GlobalPalette->assign(p);
                saveLocations();
				//updateDisplayConfig();

                // refreshing isn't enough, have to call this to get the
                // new rgb value pased into the native window
                // this will also refresh
				// ?? is this a windows thing or a mac thing
                mWindow->setBackground(GlobalPalette->getColor(COLOR_SPACE_BACKGROUND));
				// this ought to be enough
				mWindow->invalidate();
			}
			delete pd;
		}
		else if (id == IDM_SCRIPTS) {
            MobiusConfig* config = mMobius->editConfiguration();
			ScriptDialog* d = new ScriptDialog(mWindow, mMobius, config);
            showDialog(d);
            if (d->isCanceled())
              delete config;
            else
              mMobius->setGeneralConfiguration(config);
			delete d;
            updateButtons();
        }
		else if (id == IDM_SAMPLES) {
            MobiusConfig* config = mMobius->editConfiguration();
		    SampleDialog* d = new SampleDialog(mWindow, mMobius, config);
            showDialog(d);
            if (d->isCanceled())
              delete config;
            else
              mMobius->setGeneralConfiguration(config);
			delete d;
		}
		else if (id == IDM_FULLSCREEN) {
			// TODO: only if we're UIFrame can we do this
			/*
			if (mFullScreen) {
				mFullScreen = false;
				buildMenus();
			}
			else {
				mFullScreen = true;
				setMenuBar(NULL);
			}
			*/
		}
		else if (id == IDM_EXIT) {
			// prompt for save...
			mWindow->close();
		}
		else if (id == IDM_OPEN_PROJECT) {

			sprintf(filter, "%s (.mob)|*.mob;*.MOB", 
					cat->get(MSG_DLG_OPEN_PROJECT_FILTER));

			OpenDialog* od = new OpenDialog(mWindow);
			od->setTitle(cat->get(MSG_DLG_OPEN_PROJECT));
			od->setFilter(filter);
            showSystemDialog(od);
			if (!od->isCanceled()) {
				const char* file = od->getFile();

                // it would encapsulate AudioPool if we had
                // Mobius::loadProject(const char* file)

                AudioPool* pool = mMobius->getAudioPool();
                Project* p = new Project(file);
                p->read(pool);

				if (!p->isError()){
					mMobius->loadProject(p);

					Trace(3, "Loaded project from UI : set current Setup %s\n ", p->getSetup()); //#023
					mMobius->getConfiguration()->setCurrentSetup(p->getSetup());				 //#023
				}
				else {
					// some errors are worse than others, might want
					// an option to load what we can 
					alert(p->getErrorMessage());
				}

				mWindow->invalidate();
			}
			delete od;

		}
		else if (id == IDM_OPEN_LOOP) {

			sprintf(filter, "%s (.wav)|*.wav;*.WAV", 
					cat->get(MSG_DLG_OPEN_LOOP_FILTER));

			OpenDialog* od = new OpenDialog(mWindow);
			od->setTitle(cat->get(MSG_DLG_OPEN_LOOP));
			od->setFilter(filter);
            showSystemDialog(od);
			if (!od->isCanceled()) {
				const char* file = od->getFile();
                AudioPool* pool = mMobius->getAudioPool();
				Audio* au = pool->newAudio(file);
				mMobius->loadLoop(au);
				// loop meter is sensitive to this, maybe others
				mSpace->invalidate();
			}
			delete od;
		}
		else if (id == IDM_SAVE_PROJECT || id == IDM_SAVE_TEMPLATE) {
			bool isTemplate = (id == IDM_SAVE_TEMPLATE);

			sprintf(filter, "%s (.mob)|*.mob", 
					cat->get(MSG_DLG_SAVE_PROJECT_FILTER));

			OpenDialog* od = new OpenDialog(mWindow);
			od->setSave(true);
			if (isTemplate)
			  od->setTitle(cat->get(MSG_DLG_SAVE_TEMPLATE));
			else
			  od->setTitle(cat->get(MSG_DLG_SAVE_PROJECT));
			od->setFilter(filter);
			showSystemDialog(od);
			if (!od->isCanceled()) {
				Project* p = mMobius->saveProject();
				if (!p->isError()) {
                    const char* file = od->getFile();
					p->write(file, isTemplate);
                    // might have write errors
					if (p->isError())
					  alert(p->getErrorMessage());
				}
				else {
					// some errors are worse than others, might want
					// an option to load what we can 
					alert(p->getErrorMessage());
				}
                delete p;
			}
			delete od;
		}
		else if (id == IDM_SAVE_LOOP) {

			sprintf(filter, "%s (.wav)|*.wav", 
					cat->get(MSG_DLG_OPEN_LOOP_FILTER));

			OpenDialog* od = new OpenDialog(mWindow);
			od->setSave(true);
			od->setTitle(cat->get(MSG_DLG_SAVE_LOOP));
			od->setFilter(filter);
			showSystemDialog(od);
			if (!od->isCanceled()) {
				char buffer[2048];
				const char* file = od->getFile();
				strcpy(buffer, file);

				// need to be smarter about the selected extension
                // actually, Mobius should be smart about this now?
				if (!strstr(file, "."))
				  sprintf(buffer, "%s.wav", file);

                mMobius->saveLoop(buffer);
			}
			delete od;
		}
		else if (id == IDM_SAVE_QUICK) {

			mMobius->saveLoop(NULL);
		}
		else if (id == IDM_HELP_KEY) {
			// not modal so have to GC them as we go
			if (mKeyHelpDialog != NULL && !mKeyHelpDialog->isOpen()) {
				delete mKeyHelpDialog;
				mKeyHelpDialog = NULL;
			}

			if (mKeyHelpDialog == NULL) {
				mKeyHelpDialog = new KeyHelpDialog(mWindow, mMobius);
				mKeyHelpDialog->show();
			}
			else {
				// TODO: force it to the front
			}
		}
		else if (id == IDM_HELP_MIDI) {
			// not modal so have to GC them as we go
			if (mMidiHelpDialog != NULL && !mMidiHelpDialog->isOpen()) {
				delete mMidiHelpDialog;
				mMidiHelpDialog = NULL;
			}
			if (mMidiHelpDialog == NULL) {
				mMidiHelpDialog = new MidiHelpDialog(mWindow, mMobius);
				mMidiHelpDialog->show();
			}
			else {
				// TODO: force it to the front
			}
		}
		else if (id == IDM_HELP_REDRAW) {
            redraw();
		}
		else if (id == IDM_FILE_SCRIPTS) {
            mMobius->reloadScripts();
		}
		else if (id == IDM_FILE_OSC) {
            mMobius->reloadOscConfiguration();
		}
		else if (id == IDM_HELP_ABOUT) {
			AboutDialog* d = new AboutDialog(mWindow);
			showDialog(d);
			delete d;
		}
	}
}

/**
 * Kludge to work around an application lockup if we try to open
 * dialogs from the MobiusThread.  Instead MobiusPrompt will save
 * the prompts to be opened in a variable, and programatically click
 * an invisible button.  We can open dialogs from the event handler thread.
 * There is probably a better way to do this, but I'm tired of messing
 * with it and this works well enough.
 * 
 * The window must be non-modal so the script does not disrupt normal
 * control.  The PromptDialog will call our finishPrompt method when
 * it is closed.
 */
PRIVATE void UI::doInvisible()
{
	PromptDialog* todo = NULL;

	// capture the todo list in a csect just in case the script
	// is asking for another one at the same instant
	mCsect->enter();
	todo = mPromptsTodo;
	mPromptsTodo = NULL;
	mCsect->leave();

	// transfer the todo list to the active list, and show them
	try {
		PromptDialog* next = NULL;
		for (PromptDialog* d = todo ; d != NULL ; d = next) {
			next = d->getNext();
			mCsect->enter();
			d->setNext(mPrompts);
			mPrompts = d;
			mCsect->leave();
			d->show();
		}
	}
	catch (...) {
		// if we threw we might leak, but that's the least of our worries
		Trace(1, "Exception opening prompt windows!\n");
	}
}

/**
 * Open a dialog and track it in a list.
 * The tracking is a kludge for VST windows where I'm having trouble
 * getting it truly modal.  The host is able to delete the editor window
 * even if there are dialogs open so we have to clean them up before
 * shutting down the UI or else we can get menu events sent the deleted
 * objects.
 * 
 * Well, this doesn't actually work, calling cancelDialog to close
 * the dialogs can't ensure that the threads that opened the dialog
 * will unwind, there is still a race condition.  MobiusPlugin will
 * have to defer the deletion of the UI and HostFrame.
 */
PRIVATE void UI::showDialog(Dialog* d)
{   
    mDialogs->add(d);
    d->show();
    mDialogs->remove(d, false);
}

/**
 * Can't do the same thing for system dialogs since there is
 * no way I can tell to force them to close.  
 * We're hanging in something like SHBrowseForFolder.
 */
PRIVATE void UI::showSystemDialog(SystemDialog* d)
{   
    // sigh, it would be nice if Dialog and SystemDialog shared
    // a common superclass so we could force close them consistently
    // mSystemDialogs->add(d);
    d->show();
    // mSystemDialogs->remove(d);
}

/**
 * Attempt to close any open dialogs during shutdown.
 * This isn't 100% foolproof since there may still be race conditions
 * in the thread that is waiting for the dialog.  But it's better than
 * nothing.  This happens only in plugin editor windows where the
 * host closes the window while we have dialogs up.
 */
PRIVATE void UI::cancelDialogs()
{
    // these should be okay since they're non modal and don't
    // have a thread hanging on them

	// can we safely delete these if they're open? hmm, probably not
	if (mKeyHelpDialog != NULL) {
        if (mKeyHelpDialog->isOpen())
          mKeyHelpDialog->close();

        delete mKeyHelpDialog;
        mKeyHelpDialog = NULL;
    }

    if (mMidiHelpDialog != NULL) {
        if (mMidiHelpDialog->isOpen())
          mMidiHelpDialog->close();

        delete mMidiHelpDialog;
        mMidiHelpDialog = NULL;
    }

    // these are dangerous
    for (int i = 0 ; i < mDialogs->size() ; i++) {
        Trace(1, "UI: Canceling lingering dialog!\n");
        Dialog* d = (Dialog*)mDialogs->get(i);
        d->close();
    }
    mDialogs->clear(false);
}


PUBLIC bool UI::isPushed(void* o) 
{
	ActionButton* b = (ActionButton*)o;
	return b->isPushed();
}

PUBLIC void UI::alert(const char* msg)
{
	MessageCatalog* cat = mMobius->getMessageCatalog();
	const char* title = cat->get(MSG_ALERT_TITLE);
	MessageDialog* d = new MessageDialog(mWindow, title, msg);
	d->show();
	delete d;
}

/**
 * Do a full update of the UI.
 * Called by both the timer and the MobiusThread when it is
 * signaled that something needs redrawing immediately.
 *
 * UPDATE: Beginning with the Mac port we now use the MobiusRefresh 
 * callback rather than our own private timer so we won't have 
 * concurrency issues with the timer thread.
 */
PUBLIC void UI::updateUI()
{
	static bool updateUIEntered = false;
	bool ok = false;

	// have to be careful here because the MobiusThread and the Timer
	// are calling this at the same time and they can step on each other

	mCsect->enter();
	if (!updateUIEntered) {
		updateUIEntered = true;
		ok = true;
	}
	mCsect->leave();

	if (ok) {
        try {
            int tracknum = mMobius->getActiveTrack();
            MobiusState* state = mMobius->getState(tracknum);
            TrackState* tstate = state->track;
        
            mTrackGrid->setSelectedIndex(tracknum); //CurrentTrackSelected

            mMeter->update(tstate->inputMonitorLevel);
            mFloatingStrip->update(state);
            mFloatingStrip2->update(state);
            mBeaters->update(state);
            mCounter->update(state);
            mLoopList->update(state);
            mLayerList->update(state);
            mLoopMeter->update(state);
            mLoopWindow->update(state);
            //mAlert->update(state);
            mParameters->update(state);
            mModes->update(state);
            mSync->update(state);
            mStatus->update(state);

            // monitor preset changes by displaying a message
            // since the message area is generic, have to do the change
            // detection out here
            Preset* preset = tstate->preset;
            int pnum = preset->getNumber();
            if (pnum != mLastPreset) {
                mMessages->add(preset->getName());
                mLastPreset = pnum;
            }
            mMessages->update();

            // also state for each of the other tracks
            for (int i = 0 ; i < mTrackCount ; i++) {
                state = mMobius->getState(i);
                if (state != NULL)
                  mTracks[i]->update(state);
                else {
                    // must be out of range, should have resized
                    // our track list!
                }
            }
        }
        catch (...) {
            Trace(1, "ERROR: Exception during updateUI\n");
        }

		// we own this now, don't need a csect to turn it off
		updateUIEntered = false;
	}
    else {
        Trace(2, "UI:updateUI overlap\n");
    }
}

/****************************************************************************
 *                                                                          *
 *   						 CONFIGURATION FILES                            *
 *                                                                          *
 ****************************************************************************/

/**
 * Load the configuration files.
 * Normally we look in the same directory that we found mobius.xml.
 * We also support saving the ui config file path in the MobiusConfig,
 * I think this was added for Mac and we never ended up using it.
 * 
 */
PRIVATE void UI::loadConfiguration()
{
	MobiusConfig* config = mMobius->getConfiguration();
	MessageCatalog* cat = mMobius->getMessageCatalog();
	char* xml = NULL;
	char buffer[1024 * 8];

	// !! todo: see if there are any pending errors in Mobius
	// and display them

    // mobius config may point to a ui config
    // !! I used this for debugging, but it is bad in practice
    // because we always get mobius.xml from the installation directory
    // and the one referenced here may not match, warn
    const char* file = config->getUIConfig();
    if (file != NULL && IsFile(file)) {
        Trace(1, "Overriding UI config file from mobius.xml");
        GetFullPath(file, buffer, sizeof(buffer));
        mUIConfigFile = CopyString(buffer);
    }
    else {
        // relative to the directory containing mobius.xml
        MobiusContext* mc = mMobius->getContext();
        const char* mobiusFile = mc->getConfigFile();

        if (file != NULL) {
            ReplacePathFile(mobiusFile, file, buffer);
            if (IsFile(buffer))
              mUIConfigFile = CopyString(buffer);
        }
				
        if (mUIConfigFile == NULL) {
            // else assume it's here
            ReplacePathFile(mobiusFile, "ui.xml", buffer);
            mUIConfigFile = CopyString(buffer);
        }
    }
			
    xml = ReadFile(mUIConfigFile);
    if (xml == NULL || strlen(xml) == 0) {
        MessageDialog* d = new MessageDialog(mWindow);
        d->setTitle(cat->get(MSG_ALERT_CONFIG_FILE));
        d->setText(cat->get(MSG_ALERT_CONFIG_FILE_EMPTY));
        d->show();
        delete d;
    }
    else {
        //Trace(2, "Reading UI configuration from %s\n", mUIConfigFile);
        printf("Reading UI configuration file: %s\n", mUIConfigFile);
        uitrace("parsing UI config\n");
        mUIConfig = new UIConfig(xml);
        delete xml;

        if (mUIConfig->getError() != NULL) {
            // formerly popped up a dialog which in practice rarely happened
            // suppressing this so we don't popup things when the VST is
            // being probed, maybe it isn't so bad since we won't open
            // an editor until after probing, but some hosts open the
            // editor too and close it right away
            if (mWindow != NULL) {
                MessageDialog* d = new MessageDialog(mWindow);
                d->setTitle(cat->get(MSG_ALERT_CONFIG_FILE));
                d->setText(mUIConfig->getError());
                d->show();
                delete d;
            }
            else {
                printf("ERROR: Exception reading UI configuration\n");
                printf("%s", mUIConfig->getError());
            }
        }
    }

	if (mUIConfig == NULL) {
		// must always have one
		mUIConfig = new UIConfig();
	}

	// formerly did this here but we don't want dialogs popping
	// up when the VST is probed...or do we?
	//checkDevices();

    convertKeyConfig();
    convertButtonConfig();
}

/**
 * Upgrade the old KeyConfig from ui.xml into Bindings in the
 * BindingConfig.
 */
PRIVATE void UI::convertKeyConfig()
{
    KeyConfig* kconfig = mUIConfig->getKeyConfig();
    if (kconfig != NULL) {
        KeyBinding** kbindings = kconfig->getBindings();
        if (kbindings != NULL) {
            printf("Converting ui.xml key bindigs to mobius.xml bindings\n");
            fflush(stdout);
            MobiusConfig* mconfig = mMobius->editConfiguration();
            BindingConfig* bconfig = mconfig->getBaseBindingConfig();
            int changes = 0;
            for (int i = 0 ; kbindings[i] != NULL ; i++) {
                KeyBinding* kb = kbindings[i];
                int key = kb->getKey();
                const char* name = kb->getName();
                if (key > 0 && key < KEY_MAX_CODE && name != NULL) {

                    // these aren't typed but they've only been allowed
                    // to be functions or UI commands
                    Target* target = NULL;
                    Function* func = mMobius->getFunction(name);
                    if (func != NULL)
                      target = TargetFunction;
                    else {
                        UIControl* uic = getUIControl(name);
                        if (uic != NULL)
                          target = TargetUIControl;
                    }

                    if (target != NULL) {
                        Binding* b = bconfig->getBinding(TriggerKey, key);
                        if (b == NULL) {
                            b = new Binding();
                            b->setTrigger(TriggerKey);
                            b->setValue(key);
                            b->setName(name);
                            b->setTarget(target);

                            bconfig->addBinding(b);
                            changes++;
                        }
                    }
                }
            }

            if (changes > 0)
              mMobius->setBindingConfiguration(mconfig);
            else
              delete mconfig;
        }

		// remove this so we only upgrade once
        mUIConfig->setKeyConfig(NULL);
        writeConfig(mUIConfig);
    }

}

/**
 * Upgrade the old ButtonConfig from ui.xml into Bindings in the
 * BindingConfig.
 */
PRIVATE void UI::convertButtonConfig()
{
    List* buttons = mUIConfig->getButtons();
    if (buttons != NULL && buttons->size() > 0) {
        printf("Converting ui.xml button bindigs to mobius.xml bindings\n");
        fflush(stdout);
        MobiusConfig* mconfig = mMobius->editConfiguration();
        BindingConfig* bconfig = mconfig->getBaseBindingConfig();

        for (int i = 0 ; i < buttons->size() ; i++) {
            ButtonConfig* bc = (ButtonConfig*)buttons->get(i);

            printf("Converting binding for button %s\n", bc->getName());

            Binding* b = new Binding();
            b->setTrigger(TriggerUI);
            b->setTarget(TargetFunction);
            b->setName(bc->getName());
            bconfig->addBinding(b);
        }
        mMobius->setBindingConfiguration(mconfig);

        // remove this so we only upgrade once
        mUIConfig->setButtons(NULL);
        writeConfig(mUIConfig);
    }
}

/**
 * Lookup a UIControl by name.
 * Used only during conversion of an old KeyConfig into a Binding list.
 * Normal resolution of these is done inside Mobius from the array
 * we give it.
 */
PRIVATE UIControl* UI::getUIControl(const char* name)
{
    UIControl* found = NULL;
    if (name != NULL) {
        for (int i = 0 ; UIControls[i] != NULL ; i++) {
            UIControl* c = UIControls[i];
            if (StringEqualNoCase(name, c->getName()) ||
				StringEqualNoCase(name, c->getDisplayName())) {
                found = c;
                break;
            }
        }
    }
    return found;
}

void UI::writeConfig(UIConfig* config)
{
	if (config != NULL && mUIConfigFile != NULL) {
		char* xml = config->toXml();
		WriteFile(mUIConfigFile, xml);
		delete xml;
	}
}


void UI::updateDisplayConfig()
{
	//#Cas 
    if (mUIConfig != NULL) {
		
		//Display Location elements
        List* locs = mUIConfig->getLocations();
        if (locs != NULL) {
            for (int i = 0 ; i < locs->size() ; i++) {
                Location* l = (Location*)locs->get(i);
                DisplayElement* el = DisplayElement::get(l->getName());
                if (el == NULL) {
                    printf("WARNING: DisplayElement not found %s\n", 
                           l->getName());
                }
                else {
                    Component* c = mSpace->getComponent(el->getName());
                    
					if (c == NULL) {
                        printf("WARNING: Component %s not found\n", 
                               el->getName());
                    }
                    else {
						//printf("Enabling %s\n", c->getName());
						//fflush(stdout);

                        // kludge, until we have a way to drag them,
                        // ignore locations for buttons
                        // when would this happen?  Can't have buttons in Space?
                        if (c->isButton() == NULL) {
                            //printf("Moving %s to %d %d\n", c->getName(),
                            //l->getX(), l->getY());
                        	
							//Restore Location of current Component
							c->setLocation(l->getX(), l->getY());  
							
							//#012 Test Size of "Location Elements" None of them works :D
							// c->setMinimumSize(new Dimension(500, 500)); //<<-- non va :D
							Trace(3,"updateDisplayConfig::Locations ->  |%s|", el->getName());
							
							//if(el->getName() == "LoopMeter")  //<-- così no
							if(el == LoopMeterElement)	// <--- così si  //#011
							{
								//Trace(3,"LoopMeter SetPreferredSize? -> x3 ");
								//c->setPreferredSize(600, 250); //<-- need to cast to current object!!!!  
								
								//UiDimension* d = GlobalUiDimensions->getDimension("LoopMeter");
								UiDimension* d = mUIConfig->getUiDimensions()->getDimension("LoopMeter");
								if(d != NULL)
								{
									Trace(3,"LoopMeter::CustomDimension");
									((LoopMeter*) c)->setPreferredSize(d->getWidth(), d->getHeight());  //#011
								}
							}
							else if(el == BeatersElement)
							{
								UiDimension* d = mUIConfig->getUiDimensions()->getDimension("Beater");
								if(d != NULL)
								{
									Trace(3,"Beater::CustomDimension");
									((Beaters*) c)->setBeaterDiameter(d->getDiameter());  //#012
								}
								
							}
							else if(el == AudioMeterElement)  //#010
							{
								
								UiDimension* d = mUIConfig->getUiDimensions()->getDimension("AudioMeter");
								if(d != NULL)
								{
									Trace(3,"AudioMeter::CustomDimension");
									((AudioMeter*)c)->setRequiredSize(new Dimension(d->getWidth(), d->getHeight()));
									
									if(d->getSpacing() > 0) //Range custom  #SPC
										((AudioMeter*)c)->setRange(d->getSpacing()); //uso spacing per ora
								}
								
								
							}
							else if(el == LayerBarsElement)   //#013 
							{
								UiDimension* d = mUIConfig->getUiDimensions()->getDimension("LayerBar");  //Bar without "s" Dimension of a single Bar
								if(d != NULL)
								{
									Trace(3,"LayerBars::CustomDimension (of a single Bar)");
									((LayerList*) c)->setBarWidth(d->getWidth());   	//#013
									((LayerList*) c)->setBarHeight(d->getHeight());  	//#013
								}
							}
							
							// c->setSize(new Dimension(700, 700));
							// c->setWidth(900);
						
							//c->setPreferredSize(200,200); //<-- non va :D
							//c->setMinimumSize(new Dimension(300, 300)); <<-- non va :D
                        }
                        
						c->setEnabled(!l->isDisabled());
                        
						//if (l->isDisabled())
                        //printf("Disabling %s\n", c->getName());
                    }
                }
            }
        }

		//Buttons
		updateButtons();

		//Parameters
		mParameters->update(mUIConfig->getParameters());

		//Floating Strip 1
		mFloatingStrip->updateConfiguration(mUIConfig->getFloatingStrip(), mUIConfig);  	//#004 mod 2parametr
        
		//Floating Strip 2
		mFloatingStrip2->updateConfiguration(mUIConfig->getFloatingStrip2(), mUIConfig);	//#004 mod 2parametr

		//Docked Track Strips
        for (int i = 0 ; i < mTrackCount ; i++)
          mTracks[i]->updateConfiguration(mUIConfig->getDockedStrip(), mUIConfig);			//#004 mod 2parametr

		// Change to the Palette colors should have been made
		// directly to the model used by the Components, assume
		// we don't have to propagate them
		mWindow->relayout();
    }
}

/**
 * Build or rebuild the button rows from the UIConfig.
 *
 * Because I almost always want scripts to have buttons, 
 * also auto generate buttons for any scripts that used
 * the !button option.  These don't get permanently assigned
 * which might be confusing but I'm probably the only one
 * that will ever used this.
 */
void UI::updateButtons()
{
	if (mButtons != NULL) {
		mWindow->remove(mButtons);
		mButtons = NULL;
	}
	
	// it is important that this NOT be lightweight, 
	// on Mac we can be embedded in a non-compositing window
	// and a lightweight panel will overwrite the buttons
    // !! but on windows, putting this in a static window somehow
    // screws up the transmission of WM_DRAWITEM messages
    // UPDATE: Now that we use CustomButtons and never try to 
    // embed native Mac buttons, we can let this lightweight
	mButtons = new Panel("Function Buttons");

	mButtons->setBackground(GlobalPalette->getColor(COLOR_SPACE_BACKGROUND));
	mButtons->setLayout(new FlowLayout());
	mButtons->setInsets(new Insets(10, 10, 10, 10));
	mWindow->add(mButtons, BORDER_LAYOUT_NORTH);

    MobiusConfig* mconfig = mMobius->getConfiguration();
    BindingConfig* bconfig = mconfig->getBaseBindingConfig();
    int id = 1;
    for (Binding* b = bconfig->getBindings() ; b != NULL ; 
         b = b->getNext()) {
        if (b->getTrigger() == TriggerUI) {
            // make sure it resolves 
            Action* a = mMobius->resolveAction(b);
            if (a != NULL) {
                a->setRegistered(true);
                // these need to have unique non-zero ids
                // don't rely on the Binding ids though we could?
                a->id = id++;
                mButtons->add(new ActionButton(mMobius, a));
            }
        }
    }

    Action* actions = mMobius->getScriptButtonActions();
    if (actions != NULL) {
        Action* next = NULL;
        for (Action* action = actions ; action != NULL ; action = next) {
            next = action->getNext();
            action->setNext(NULL);
            action->setRegistered(true);
            action->id = id++;
            mButtons->add(new ActionButton(mMobius, action));
        }
    }

	mWindow->relayout();

}

/**
 * Save information about the current window size and position and the
 * positions of the space components.
 *
 * Note that this can be called twice in some situations during shutdown.
 * Since most components have names now be sure to only save locations
 * for things in mSpace!  Otherwise things outside space get marked
 * disabled.
 */
void UI::saveLocations()
{
	mUIConfig->setBounds(new Bounds(mWindow->getBounds()));
	mUIConfig->setMaximized(mWindow->isMaximized());

	saveLocations(mSpace);

    // If we started off without a FontConfig, let the components
    // bootstrap one and capture it at the end.
    FontConfig* fonts = mUIConfig->getFontConfig();
    if (fonts == NULL || fonts->getBindings() == NULL)
      mUIConfig->setFontConfig(GlobalFontConfig->clone());

	writeConfig(mUIConfig);
}

void UI::saveLocations(Component* c)
{
	if (c->getName() != NULL) {
		mUIConfig->updateLocation(c->getName(), c->getX(), c->getY());
	}

	Container* cnt = c->isContainer();
	if (cnt != NULL) {
		for (Component* child = cnt->getComponents() ; child != NULL ; 
			 child = child->getNext())
		  saveLocations(child);
	}
}

/**
 * Called after changes to the UICOnfig fromn the GlobalDialog.
 */
void UI::updateGlobalConfig()
{
    mMessages->setDuration(mUIConfig->getMessageDuration());
}

void UI::checkDevices()
{
	MobiusConfig* config = mMobius->getConfiguration();
	MessageCatalog* cat = mMobius->getMessageCatalog();
	const char* title = cat->get(MSG_ALERT_CONFIG);
    char buf[1024];

    MobiusAlerts* alerts = mMobius->getAlerts();

    if (alerts->midiInputError != NULL) {
        sprintf(buf, cat->get(MSG_ALERT_MIDI_INPUT),
                alerts->midiInputError);
        MessageDialog::showError(mWindow, title, buf);
    }

    if (alerts->midiOutputError != NULL) {
        sprintf(buf, cat->get(MSG_ALERT_MIDI_OUTPUT),
                alerts->midiOutputError);
        MessageDialog::showError(mWindow, title, buf);
    }

    if (alerts->midiThroughError != NULL) {
        sprintf(buf, cat->get(MSG_ALERT_MIDI_OUTPUT),
                alerts->midiThroughError);
        MessageDialog::showError(mWindow, title, buf);
    }

    if (alerts->audioInputInvalid) {
        sprintf(buf, cat->get(MSG_ALERT_AUDIO_INPUT),
                config->getAudioInput());
        MessageDialog::showError(mWindow, title, buf);
    }

    if (alerts->audioOutputInvalid) {
        sprintf(buf, cat->get(MSG_ALERT_AUDIO_OUTPUT),
                config->getAudioOutput());
        MessageDialog::showError(mWindow, title, buf);
    }
}

/**
 * Called by a dialog when it wants to listen for midi events.
 */
PUBLIC UIMidiEventListener* UI::setMidiEventListener(UIMidiEventListener* l)
{
	UIMidiEventListener* current = mMidiEventListener;
	mMidiEventListener = l;
	return current;
} 

/****************************************************************************
 *                                                                          *
 *                              MOBIUS LISTENER                             *
 *                                                                          *
 ****************************************************************************/

/**
 * Called by Mobius when something significant happens that must be
 * displayed.  
 */
PUBLIC void UI::MobiusAlert(const char* msg) 
{
	MessageCatalog* cat = mMobius->getMessageCatalog();
	const char* title = cat->get(MSG_ALERT_MESSAGE);
	MessageDialog::showMessage(mWindow, title, msg);
}

/**
 * Called by Mobius when an operational message needs to be displayed.
 * Do not pop up  a dialog for these, use a space component.
 */
PUBLIC void UI::MobiusMessage(const char* msg) 
{
	// punt on internationalizing these, assume they're comming from scripts
	mMessages->add(msg);
}

/**
 * Clear the message area on a global reset in case the duration
 * is set way high.  I don't like the interface here but the alternative
 * is to make the definitions of messages and their duration a more
 * formal part of the Mobius interface.
 */
PUBLIC void UI::MobiusGlobalReset() 
{
	mMessages->add(NULL);
}

/**
 * Called by MobiusThread in response to a Prompt script statement.
 * We're responsible for displaying the prompt and getting a response.
 */
PUBLIC void UI::MobiusPrompt(Prompt* p)
{
	gcPrompts(false);

	// the Primpt list is owned by Mobius, but we own the dialog list
	PromptDialog* d = new PromptDialog(mWindow, this, p);

	// this can be accessed by the UI thread so be careful
	mCsect->enter();
	d->setNext(mPromptsTodo);
	mPromptsTodo = d;
	mCsect->leave();

	// transfer to the UI thread
	mInvisible->click();
}

/**
 * Called by the PromptDialog dialog when it closes.
 */
PUBLIC void UI::finishPrompt(PromptDialog* d, Prompt* p)
{
	// can't remove from the list yet since its still closing
	mMobius->finishPrompt(p);
}

/**
 * Remove completed prompts.
 * If the force flag is true, we're shutting down and need to 
 * get rid of everything.
 */
PRIVATE void UI::gcPrompts(bool force)
{
	PromptDialog* prev = NULL;
	PromptDialog* next = NULL;

	if (force) {
		// ignore any waiting prompts
		mCsect->enter();
		for (PromptDialog* d = mPromptsTodo ; d != NULL ; d = next) {
			next = d->getNext();
			Prompt* p = d->getPrompt();
			if (p != NULL)
			  delete p;
			delete d;
		}
		mPromptsTodo = NULL;
		mCsect->leave();
	}

	// look for dialogs that are done closing, if they aren't closed
	// yet, we'll have to ignore them even if they are still open,
	// this will leak but it's too risky to coordinate an orderly close
	// during a system shutdown

	mCsect->enter();
	for (PromptDialog* d = mPrompts ; d != NULL ; d = next) {
		next = d->getNext();
		if (d->isOpen())
		  prev = d;
		else {
			if (prev != NULL)
			  prev->setNext(next);
			else
			  mPrompts = next;
			delete d;
		}
	}
	mCsect->leave();

}

/**
 * Called when an internal configuration change is made.
 * Currently called only when the unitTestSetup script function is 
 * called, but in theory can happen whenever a change is made to 
 * the Preset or Setup lists.
 */
PUBLIC void UI::MobiusConfigChanged()
{
    refreshPresetMenu();
    refreshSetupMenu();
    mParameters->refresh();
}

/**
 * Called by Mobius when a MIDI event is received.  If a dialog
 * is interested in displaying events, it will register a listener.
 */
PUBLIC bool UI::MobiusMidiEvent(MidiEvent* e)
{
	bool processIt = true;

	// some dialogs (well, only MidiControlDialog) may want
	// to register interest in MIDI events for capture binding
	if (mMidiEventListener != NULL)
	  processIt = mMidiEventListener->midiEvent(e);

	return processIt;
}

/**
 * Called by MobiusThread every 1/10 second.
 * Actually this is very jittery because any other event handled
 * by the thread resets the timeout interval.  This includes trace messages
 * which happen all the time as the engine runs.  You need to use a Timer.
 * I'm leaving the callback in place just in case a use turns up, we
 * could even make Mobius manage the Timer but that seems wrong.
 */
PUBLIC void UI::MobiusRefresh()
{
	//updateUI();
}

/**
 * Called by MobiusThread when a significant time boundary (beat, cycle, loop)
 * has been crossed.  Update the display now rather than waiting for the
 * next timer event.
 */
PUBLIC void UI::MobiusTimeBoundary()
{
	bool conservative = false;

	// at first I tried just refreshing a few things that
	// change regularly, but since subcycle quantized events
	// can make lots of visible changes, just go ahead
	// and refresh everything
	if (conservative) {
		MobiusState* state = mMobius->getState(mMobius->getActiveTrack());
        
		// this is track specific!
		//mMeter->update(mMobius->getInputLevel());

		mBeaters->update(state);
		// if parameters have updated, go ahead and do them too
		mParameters->update(state);
	}
	else {
		// wait till we're open
		if (mTimer->isRunning())
		  updateUI();
	}
}

/**
 * Called by Mobius when it detects a trigger for a UIControl.
 * Usually these are from keyboard triggers.
 */
PUBLIC void UI::MobiusAction(Action* action)
{
    Target* target = action->getTarget();

    if (target != TargetUIControl) {
        Trace(1, "UI::MobiusAction unsupported target\n");
    }
    else {
        UIControl* control = (UIControl*)action->getTargetObject();

        if (control == SpaceDragControl) {
            if (!action->repeat) {
                mSpace->setDragging(action->down);
            }
        }
        else if (action->down) {
            if (control == NextParameterControl)
              mParameters->nextParameter();

            else if (control == PrevParameterControl)
              mParameters->prevParameter();

            else if (control == IncParameterControl)
              mParameters->incParameter();

            else if (control == DecParameterControl)
              mParameters->decParameter();
        }
    }
}

/**
 * Called when the entire UI should be redrawn.
 * This distinction between this and MobiusRefresh is subtle.  Refresh
 * is supposed to be smart about what to redraw, this is supposed to
 * do all of it every call.  Need to move to an event based refresh
 * mechanism.
 */
PUBLIC void UI::MobiusRedraw()
{
    redraw();
}

/****************************************************************************
 *                                                                          *
 *                                KEY LISTENER                              *
 *                                                                          *
 ****************************************************************************/

/**
 * KeyListener interface.
 */
PUBLIC void UI::keyPressed(KeyEvent* e) 
{
    int code = e->getFullKeyCode();

    if (code < 0 || code >= KEY_MAX_CODE)
	  Trace(1, "Key press code out of range %ld\n", (long)code);
    else {
        bool repeat = (mKeyState[code] != 0);
		mKeyState[code] = 1;

        mMobius->doKeyEvent(code, true, repeat);
	}
}

/**
 * KeyListener interface.
 */
PUBLIC void UI::keyReleased(KeyEvent* e)
{
	int code = e->getFullKeyCode();

    if (code < 0 || code >= KEY_MAX_CODE)
	  Trace(1, "Key release code out of range %ld\n", (long)code);

    else if (!mKeyState[code]) {
        // redundant key up, shouldn't happen?
        // these can happen when using keys like Alt-Tab to switch
        // focus
        //Trace(1, "Redundant key up: %s\n", Character::getString(code));
    }
    else {
		mKeyState[code] = 0;
        
        mMobius->doKeyEvent(code, false, false);
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

