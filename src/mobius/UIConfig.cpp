/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Model for Mobius UI configuration.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "Util.h"
#include "MessageCatalog.h"
#include "XmlModel.h"
#include "XmlBuffer.h"

#include "Qwin.h"
#include "Palette.h"

#include "Function.h"
#include "Messages.h"
#include "Mobius.h"

#include "UIConfig.h"
#include "UITypes.h"
#include "UI.h"

/****************************************************************************
 *                                                                          *
 *   							XML CONSTANTS                               *
 *                                                                          *
 ****************************************************************************/

#define EL_UI_CONFIG "UIConfig"
#define ATT_NAME "name"
#define ATT_REFRESH "refreshInterval"
#define ATT_ALERT_INTERVALS "alertIntervals"
#define ATT_MESSAGE_DURATION "messageDuration"
 
#define EL_LOCATIONS "Locations"
#define EL_LOCATION "Location"
#define ATT_X "x"
#define ATT_Y "y"
#define ATT_WIDTH "width"
#define ATT_HEIGHT "height"
#define ATT_DISABLED "disabled"
#define ATT_MAXIMIZED "maximized"
#define ATT_NOMENU "noMenu"
#define ATT_PAINT_TRACE "paintTrace"

// #define ATT_RADAR_DIAMETER "radarDiameter" 			//#004
// #define ATT_LEVEL_METER_HEIGHT "levelMeterHeight" 	//#004
// #define ATT_LEVEL_METER_WIDTH "levelMeterWidth" 	//#004

#define EL_BUTTONS "Buttons"
#define EL_BUTTON "Button"
#define ATT_FUNCTION_NAME "function"

// don't really like these as top level things, would make more sense
// inside the <Location> element, consider generalizing <Location>
// to <Component> and allowing it to have arbitrary <Property>s.

#define EL_PARAMETERS "InstantParameters"
#define EL_PARAMETER "Parameter"

#define EL_KEY_CONFIG "KeyConfig"
#define EL_KEY_BINDING "KeyBinding"
#define ATT_KEY "key"

#define EL_OLD_TRACK_CONTROLS "TrackControls"
#define EL_FLOATING_TRACK_STRIP "FloatingTrackStrip"
#define EL_FLOATING_TRACK_STRIP2 "FloatingTrackStrip2"

#define EL_OLD_TRACK_STRIP "TrackStripControls"
#define EL_DOCKED_TRACK_STRIP "DockedTrackStrip"

#define EL_COMPONENT "Component"

/****************************************************************************
 *                                                                          *
 *   							  UI CONFIG                                 *
 *                                                                          *
 ****************************************************************************/
/*
 * Mobius UI configuration.  Seperated from MobiusConfig so we can
 * build a core mobius library that doesn't require qwin.lib
 * or any windows support.
 *
 * Could consider having more than one of these to implement
 * "scenes" but don't have to be that complicated yet.
 */

PUBLIC UIConfig::UIConfig()
{
    init();
}

PUBLIC UIConfig::UIConfig(const char *xml)
{
	init();
	parseXml(xml);
}

PUBLIC UIConfig::UIConfig(XmlElement* e)
{
    init();
    parseXml(e);
}

PUBLIC void UIConfig::init()
{
    mError[0] = 0;
    mName = NULL;
    mRefreshInterval = DEFAULT_REFRESH_INTERVAL;
    mAlertIntervals = DEFAULT_ALERT_INTERVALS;
    mMessageDuration = DEFAULT_MESSAGE_DURATION;
	mBounds = NULL;
	mMaximized = false;
	mNoMenu = false;
    mPaintTrace = false;
	mPalette = NULL;
    mFontConfig = NULL;
    mLocations = NULL;
    mButtons = NULL;
    mKeyConfig = NULL;
	mParameters = NULL;
    mFloatingStrip = NULL;
    mFloatingStrip2 = NULL;
    mDockedStrip = NULL;
	
	// mRadarDiameter = 0;		//#004
	// mLevelMeterHeight = 0;	//#004
	// mLevelMeterWidth = 0;	//#004

	mDimensions = NULL;  //#003
}

PUBLIC UIConfig::~UIConfig()
{
    delete mName;
	delete mPalette;
	delete mFontConfig;
	delete mButtons;
    delete mLocations;
	delete mParameters;
    delete mFloatingStrip;
    delete mFloatingStrip2;
    delete mDockedStrip;
    delete mKeyConfig;
	delete mBounds;

	delete mDimensions; //#003
}

PUBLIC UIConfig* UIConfig::clone()
{
    return new UIConfig(toXml());
}

PUBLIC void UIConfig::setName(const char* s)
{
	delete mName;
	mName = CopyString(s);
}

PUBLIC void UIConfig::setRefreshInterval(int i)
{
    // guard against insanely low intervals
    if (i < 10) i = 10;
    mRefreshInterval = i;
}

PUBLIC int UIConfig::getRefreshInterval()
{
    return mRefreshInterval;
}

PUBLIC void UIConfig::setAlertIntervals(int i)
{
    mAlertIntervals = i;
}

PUBLIC int UIConfig::getAlertIntervals()
{
    return mAlertIntervals;
}

PUBLIC void UIConfig::setMessageDuration(int i)
{
    // looks funny in the UI for this to be zero, bootstrap it if we
    // have an old config
    if (i == 0) i = DEFAULT_MESSAGE_DURATION;
    mMessageDuration = i;
}

PUBLIC int UIConfig::getMessageDuration()
{
    return mMessageDuration;
}

PUBLIC void UIConfig::setPalette(Palette* p)
{
	if (p != mPalette) {
		delete mPalette;
		mPalette = p;
	}
}

PUBLIC Palette* UIConfig::getPalette()
{
	if (mPalette == NULL)
	  mPalette = new Palette();
	return mPalette;
}

PUBLIC Palette* UIConfig::stealPalette()
{
    Palette* p = mPalette;
    mPalette = NULL;
    return p;
}

PUBLIC void UIConfig::setFontConfig(FontConfig* c)
{
	if (c != mFontConfig) {
		delete mFontConfig;
		mFontConfig = c;
	}
}

// PUBLIC void UIConfig::setUiDimensions(UiDimensions* d)
// {
// 	if (d != mDimensions) {
// 		delete mDimensions;
// 		mDimensions = d;
// 	}
// }

PUBLIC FontConfig* UIConfig::getFontConfig()
{
    // unlike Palette don't create an empty one
    // so we can tell we need to bootstrap one and save it
	//if (mFontConfig == NULL)
    //mFontConfig = new FontConfig();
	return mFontConfig;
}

PUBLIC FontConfig* UIConfig::stealFontConfig()
{
    FontConfig* c = mFontConfig;
    mFontConfig = NULL;
    return c;
}


PUBLIC void UIConfig::setUiDimensions(class UiDimensions* d)  	//#003
{
	if (d != mDimensions) {
		delete mDimensions;
		mDimensions = d;
	}
}

PUBLIC class UiDimensions* UIConfig::getUiDimensions() 	//#003
{
	return mDimensions;
}





PUBLIC List* UIConfig::getButtons()
{
    return mButtons;
}

PUBLIC void UIConfig::setButtons(ObjectList* l)
{
	delete mButtons;
    mButtons = l;
}

#if 0
PUBLIC ObjectList* UIConfig::stealButtons()
{
    ObjectList* b = mButtons;
    mButtons = NULL;
    return b;
}
#endif

/**
 * Deprecated but we have to parse them for upgrade.
 */
PUBLIC void UIConfig::addButton(ButtonConfig* b)
{
    if (mButtons == NULL)
      mButtons = new ObjectList();
    mButtons->add(b);
}

PUBLIC StringList* UIConfig::getParameters()
{
    return mParameters;
}

PUBLIC StringList* UIConfig::stealParameters()
{
    StringList* p = mParameters;
    mParameters = NULL;
    return p;
}

PUBLIC void UIConfig::setParameters(StringList* l)
{
    delete mParameters;
    mParameters = l;
}

PUBLIC void UIConfig::addParameter(const char* s)
{
    if (mParameters == NULL)
      mParameters = new StringList();
    mParameters->add(s);
}

PUBLIC StringList* UIConfig::getFloatingStrip()
{
    return mFloatingStrip;
}

PUBLIC void UIConfig::addFloatingStrip(const char* s)
{
    if (mFloatingStrip == NULL)
      mFloatingStrip = new StringList();
    mFloatingStrip->add(s);
}

PUBLIC void UIConfig::setFloatingStrip(StringList* l)
{
    delete mFloatingStrip;
    mFloatingStrip = l;
}



PUBLIC StringList* UIConfig::getFloatingStrip2()
{
    return mFloatingStrip2;
}

PUBLIC void UIConfig::setFloatingStrip2(StringList* l)
{
    delete mFloatingStrip2;
    mFloatingStrip2 = l;
}

PUBLIC StringList* UIConfig::getDockedStrip()
{
    return mDockedStrip;
}

PUBLIC void UIConfig::addDockedStrip(const char* s)
{
    if (mDockedStrip == NULL)
      mDockedStrip = new StringList();
    mDockedStrip->add(s);
}

PUBLIC void UIConfig::setDockedStrip(StringList* l)
{
    delete mDockedStrip;
    mDockedStrip = l;
}

PUBLIC KeyConfig* UIConfig::getKeyConfig()
{
    if (mKeyConfig == NULL)
      mKeyConfig = new KeyConfig();
    return mKeyConfig;
}

PUBLIC KeyConfig* UIConfig::stealKeyConfig()
{
    KeyConfig* k = mKeyConfig;
    mKeyConfig = NULL;
    return k;
}

PUBLIC void UIConfig::setKeyConfig(KeyConfig* dc)
{
    if (dc != mKeyConfig) {
        delete mKeyConfig;
        mKeyConfig = dc;
    }
}

PUBLIC Bounds* UIConfig::getBounds()
{
	return mBounds;
}

PUBLIC void UIConfig::setBounds(Bounds* b)
{
	delete mBounds;
	mBounds = b;

	// not sure how, but the bounds can be corrupted leading to strange
	// behavior
	if (b->x < 0) b->x = 0;
	if (b->y < 0) b->y = 0;
	if (b->width < 20) b->width = 20;
	if (b->height < 20) b->height = 20;

}

PUBLIC void UIConfig::setMaximized(bool b)
{
	mMaximized = b;
}

PUBLIC bool UIConfig::isMaximized()
{
	return mMaximized;
}

PUBLIC void UIConfig::setNoMenu(bool b)
{
	mNoMenu = b;
}

PUBLIC bool UIConfig::isNoMenu()
{
	return mNoMenu;
}

PUBLIC void UIConfig::setPaintTrace(bool b)
{
	mPaintTrace = b;
}

PUBLIC bool UIConfig::isPaintTrace()
{
	return mPaintTrace;
}

// Cas - Old implementation before #003 works UiDimensions
// PUBLIC void UIConfig::setRadarDiameter(int i)
// {
// 	mRadarDiameter = i;
// }

// PUBLIC int UIConfig::getRadarDiameter()
// {
// 	return mRadarDiameter;
// }

// PUBLIC void UIConfig::setLevelMeterHeight(int i)
// {
// 	mLevelMeterHeight = i;
// }

// PUBLIC int UIConfig::getLevelMeterHeight()
// {
// 	return mLevelMeterHeight;
// }


// PUBLIC void UIConfig::setLevelMeterWidth(int i)
// {
// 	mLevelMeterWidth = i;
// }

// PUBLIC int UIConfig::getLevelMeterWidth()
// {
// 	return mLevelMeterWidth;
// }


void UIConfig::parseXml(const char *src) 
{
    mError[0] = 0;
	XomParser* p = new XomParser();
	XmlDocument* d = p->parse(src);
    XmlElement* e = NULL;

	if (d != NULL)
      e = d->getChildElement();

	if (e != NULL)
      parseXml(e);
    else {
        // capture the error since the parser no longer throws
        CopyString(p->getError(), mError, sizeof(mError));
    }
    delete d;
	delete p;
}

/**
 * Return the error message if it is set.
 */
PUBLIC const char* UIConfig::getError()
{
    return (mError[0] != 0) ? mError : NULL;
}

//#define KNOB_DEFAULT_DIAMETER 50  //#004 Taken from Components.cpp

PRIVATE void UIConfig::parseXml(XmlElement* e)
{
	//Trace(1,"UIConfig::parseXml(XmlElement* e)");

    setName(e->getAttribute(ATT_NAME));
	mBounds = new Bounds();

	mBounds->x = e->getIntAttribute(ATT_X, 0);
	mBounds->y = e->getIntAttribute(ATT_Y, 0);
	mBounds->width = e->getIntAttribute(ATT_WIDTH, 0);
	mBounds->height = e->getIntAttribute(ATT_HEIGHT, 0);
	mMaximized = e->getBoolAttribute(ATT_MAXIMIZED);
	mNoMenu = e->getBoolAttribute(ATT_NOMENU);
	mPaintTrace = e->getBoolAttribute(ATT_PAINT_TRACE);

	// Old Impl -> #003 UiDimensions
	// mRadarDiameter = e->getIntAttribute(ATT_RADAR_DIAMETER, KNOB_DEFAULT_DIAMETER);		//#004
	// mLevelMeterHeight = e->getIntAttribute(ATT_LEVEL_METER_HEIGHT, 10);					//#004
	// mLevelMeterWidth = e->getIntAttribute(ATT_LEVEL_METER_WIDTH, 75);					//#004
	
    setRefreshInterval(e->getIntAttribute(ATT_REFRESH, DEFAULT_REFRESH_INTERVAL));
    setAlertIntervals(e->getIntAttribute(ATT_ALERT_INTERVALS, DEFAULT_ALERT_INTERVALS));
    setMessageDuration(e->getIntAttribute(ATT_MESSAGE_DURATION, DEFAULT_MESSAGE_DURATION));
 
 	Trace(3,"=======>  [UIConfig::parseXml]");

	for (XmlElement* child = e->getChildElement() ; child != NULL ; 
		 child = child->getNextElement()) {


		Trace(1,"(child->isName(%s))", child->getName());

		if (child->isName(EL_LOCATIONS)) {
			for (XmlElement* le = child->getChildElement() ; le != NULL ; 
				 le = le->getNextElement()) {
				addLocation(new Location(le));
			}
		}
		else if (child->isName(EL_PARAMETERS)) {
			for (XmlElement* pe = child->getChildElement() ; pe != NULL ; 
				 pe = pe->getNextElement()) {
				addParameter(pe->getAttribute(ATT_NAME));
			}
		}
		else if (child->isName(EL_OLD_TRACK_CONTROLS) ||
                 child->isName(EL_FLOATING_TRACK_STRIP)) {
			for (XmlElement* pe = child->getChildElement() ; pe != NULL ; 
				 pe = pe->getNextElement()) {
				addFloatingStrip(pe->getAttribute(ATT_NAME));
			}
		}
		else if (child->isName(EL_FLOATING_TRACK_STRIP2)) { 
            StringList* controls = NULL;
			for (XmlElement* pe = child->getChildElement() ; pe != NULL ; 
				 pe = pe->getNextElement()) {
                if (controls == NULL)
                  controls = new StringList();
				controls->add(pe->getAttribute(ATT_NAME));
			}
            setFloatingStrip2(controls);
		}
		else if (child->isName(EL_OLD_TRACK_STRIP) ||
                 child->isName(EL_DOCKED_TRACK_STRIP)) {
			for (XmlElement* pe = child->getChildElement() ; pe != NULL ; 
				 pe = pe->getNextElement()) {
				addDockedStrip(pe->getAttribute(ATT_NAME));
			}
		}
		else if (child->isName(EL_BUTTONS)) {
            // deprecated but we have to parse them for upgrade
			for (XmlElement* bce = child->getChildElement() ; bce != NULL ; 
				 bce = bce->getNextElement()) {
				addButton(new ButtonConfig(bce));
			}
		}
		else if (child->isName(EL_KEY_CONFIG)) {
			mKeyConfig = new KeyConfig(child);
		}
		else if (child->isName(Palette::ELEMENT)) {
            setPalette(new Palette(child));
        }
		else if (child->isName(FontConfig::ELEMENT)) {
            setFontConfig(new FontConfig(child));
		}
		else if (child->isName(UiDimensions::ELEMENT)) { //#003 Crea UiDimensionS a partire da sottoalbero xml
            Trace(3,"->SetUiDimension");
			setUiDimensions(new UiDimensions(child));
		}
	}

 	Trace(3,"<======= End [UIConfig::parseXml]");
   
    checkDisplayComponents();
}

/**
 * Cleanup after parsing.
 * For each display component, add a Location for any new ones, and
 * remove obsolete Locations.
 */
PRIVATE void UIConfig::checkDisplayComponents()
{
    int i;

    // add missing components
    for (i = 0 ; SpaceElements[i] != NULL ; i++) {
        DisplayElement* el = SpaceElements[i];
        Location* loc = getLocation(el->getName());
        if (loc == NULL)
          loc = getLocation(el->alias);

        if (loc == NULL) {
            loc = new Location(el->getName());
            // these start off disabled
            loc->setDisabled(true);
            addLocation(loc);
        }
        else if (StringEqual(loc->getName(), el->alias)) {
            loc->setName(el->getName());
        }
    }

    // remove obsolete components
    if (mLocations != NULL) {
        for (i = 0 ; i < mLocations->size() ; i++) {
            Location* l = (Location*)mLocations->get(i);
            DisplayElement* el = DisplayElement::get(l->getName());
            if (el == NULL)
              mLocations->remove(l);
        }
    }
}

PUBLIC char* UIConfig::toXml()
{
	char* xml = NULL;
	XmlBuffer* b = new XmlBuffer();
	toXml(b);
	xml = b->stealString();
	delete b;
	return xml;
}



PUBLIC void UIConfig::toXml(XmlBuffer* b)
{

	Trace(3,"=======> [UIConfig::toXml(XmlBuffer* b)]");

	b->addOpenStartTag(EL_UI_CONFIG);

    // these won't ever have names currently
    b->addAttribute(ATT_NAME, mName);

    if (mBounds != NULL) {
		b->addAttribute(ATT_X, mBounds->x);
		b->addAttribute(ATT_Y, mBounds->y);
		b->addAttribute(ATT_WIDTH, mBounds->width);
		b->addAttribute(ATT_HEIGHT, mBounds->height);
	}
	b->addAttribute(ATT_MAXIMIZED, mMaximized);
    // disables the window menu bar, this is never been exposed
	b->addAttribute(ATT_NOMENU, mNoMenu);
	b->addAttribute(ATT_PAINT_TRACE, mPaintTrace);
    b->addAttribute(ATT_REFRESH, mRefreshInterval);
    b->addAttribute(ATT_MESSAGE_DURATION, mMessageDuration);

	// b->addAttribute(ATT_RADAR_DIAMETER, mRadarDiameter);		//#004
	// b->addAttribute(ATT_LEVEL_METER_HEIGHT, mLevelMeterHeight);	//#004
	// b->addAttribute(ATT_LEVEL_METER_WIDTH, mLevelMeterWidth);	//#004

    // this has never been used and I'm not even sure what it was for
    //b->addAttribute(ATT_ALERT_INTERVALS, mAlertIntervals);

	b->add(">\n");
	b->incIndent();

	if (mLocations != NULL) {
		b->addStartTag(EL_LOCATIONS);
		b->incIndent();
		for (int i = 0 ; i < mLocations->size() ; i++) {
			Location* l = (Location*)mLocations->get(i);
			l->toXml(b);
		}
		b->decIndent();
		b->addEndTag(EL_LOCATIONS);
	}

	if (mParameters != NULL) {
		b->addStartTag(EL_PARAMETERS);
		b->incIndent();
		for (int i = 0 ; i < mParameters->size() ; i++) {
			const char* name = mParameters->getString(i);
			if (name != NULL) {
				b->addOpenStartTag(EL_PARAMETER);
				b->addAttribute(ATT_NAME, name);
				b->add("/>\n");
			}
		}
		b->decIndent();
		b->addEndTag(EL_PARAMETERS);
	}

    if (mFloatingStrip != NULL) {
        b->addStartTag(EL_FLOATING_TRACK_STRIP);
		b->incIndent();
		for (int i = 0 ; i < mFloatingStrip->size() ; i++) {
			const char* name = mFloatingStrip->getString(i);
			if (name != NULL) {
				b->addOpenStartTag(EL_COMPONENT);
				b->addAttribute(ATT_NAME, name);
				b->add("/>\n");
			}
		}
		b->decIndent();
		b->addEndTag(EL_FLOATING_TRACK_STRIP);
	}

    if (mFloatingStrip2 != NULL) {
        b->addStartTag(EL_FLOATING_TRACK_STRIP2);
		b->incIndent();
		for (int i = 0 ; i < mFloatingStrip2->size() ; i++) {
			const char* name = mFloatingStrip2->getString(i);
			if (name != NULL) {
				b->addOpenStartTag(EL_COMPONENT);
				b->addAttribute(ATT_NAME, name);
				b->add("/>\n");
			}
		}
		b->decIndent();
		b->addEndTag(EL_FLOATING_TRACK_STRIP2);
	}

    if (mDockedStrip != NULL) {
        b->addStartTag(EL_DOCKED_TRACK_STRIP);
		b->incIndent();
		for (int i = 0 ; i < mDockedStrip->size() ; i++) {
			const char* name = mDockedStrip->getString(i);
			if (name != NULL) {
				b->addOpenStartTag(EL_COMPONENT);
				b->addAttribute(ATT_NAME, name);
				b->add("/>\n");
			}
		}
		b->decIndent();
		b->addEndTag(EL_DOCKED_TRACK_STRIP);
	}

	if (mKeyConfig != NULL)
      mKeyConfig->toXml(b);

    // deprecated, this should be upgraded immediately into Bindings
	if (mButtons != NULL) {
		b->addStartTag(EL_BUTTONS);
		b->incIndent();
		for (int i = 0 ; i < mButtons->size() ; i++) {
			ButtonConfig* bc = (ButtonConfig*)mButtons->get(i);
			bc->toXml(b);
		}
		b->decIndent();
		b->addEndTag(EL_BUTTONS);
	}
	
	if (mPalette != NULL)
	  mPalette->toXml(b);

	if (mFontConfig != NULL)
	  mFontConfig->toXml(b);

	 if(mDimensions != NULL)  //#003
	 	mDimensions->toXml(b);

    b->decIndent();

	b->addEndTag(EL_UI_CONFIG);

	Trace(3,"<======= End [UIConfig::toXml(XmlBuffer* b)]");
}

PUBLIC Location* UIConfig::getLocation(const char* name)
{
	Location* found = NULL;

	if (name != NULL && mLocations != NULL) {
		for (int i = 0 ; i < mLocations->size() ; i++) {
			Location* l = (Location*)mLocations->get(i);
			const char* lname = l->getName();
			if (lname != NULL && !strcmp(lname, name)) {
				found = l;
				break;
			}
		}
	}
	return found;
}

PUBLIC void UIConfig::addLocation(Location* l)
{
	const char* name = l->getName();
	if (name != NULL) {
		Location* existing = getLocation(name);
		if (existing == NULL) {
			if (mLocations == NULL)
			  mLocations = new ObjectList();
			mLocations->add(l);
		}
		else {
			existing->setX(l->getX());
			existing->setY(l->getY());
            existing->setDisabled(l->isDisabled());
			delete l;
		}
	}
	else {
		// malformed, ignore
		delete l;
	}
}

PUBLIC void UIConfig::updateLocation(const char* name, int x, int y)
{
	if (name != NULL) {
		Location* loc = getLocation(name);
		if (loc == NULL) {
            loc = new Location(name);
			if (mLocations == NULL)
			  mLocations = new ObjectList();
			mLocations->add(loc);
		}
        loc->setX(x);
        loc->setY(y);
	}
}

PUBLIC List* UIConfig::getLocations()
{
	return mLocations;
}

PUBLIC List* UIConfig::stealLocations()
{
	List* l = mLocations;
    mLocations = NULL;
    return l;
}

PUBLIC void UIConfig::resetLocations()
{
	delete mLocations;
    mLocations = NULL;
}

/****************************************************************************
 *                                                                          *
 *   							   LOCATION                                 *
 *                                                                          *
 ****************************************************************************/


PUBLIC Location::Location()
{
	init();
}

PUBLIC Location::Location(XmlElement* e)
{
	init();
	parseXml(e);
}

PUBLIC Location::Location(const char* name)
{
	init();
	setName(name);
}

PRIVATE void Location::init()
{
	mName = NULL;
	mX = 0;
	mY = 0;
    mDisabled = false;
}

PUBLIC Location::~Location()
{
	delete mName;
}

PUBLIC void Location::setName(const char* s)
{
	delete mName;
	mName = CopyString(s);
}

PUBLIC const char* Location::getName()
{
	return mName;
}

PUBLIC void Location::setX(int i)
{
	mX = i;
}

PUBLIC int Location::getX()
{
	return mX;
}

PUBLIC void Location::setY(int i)
{
	mY = i;
}

PUBLIC int Location::getY()
{
	return mY;
}

PUBLIC void Location::setDisabled(bool b)
{
    mDisabled = b;
}

PUBLIC bool Location::isDisabled()
{
    return mDisabled;
}

PUBLIC void Location::toXml(XmlBuffer* b)
{
	b->addOpenStartTag(EL_LOCATION);
	b->addAttribute(ATT_NAME, mName);
	b->addAttribute(ATT_X, mX);
	b->addAttribute(ATT_Y, mY);
	b->addAttribute(ATT_DISABLED, mDisabled);
	b->add("/>\n");
}

PUBLIC void Location::parseXml(XmlElement* e)
{
	setName(e->getAttribute(ATT_NAME));
	setX(e->getIntAttribute(ATT_X));
	setY(e->getIntAttribute(ATT_Y));
    setDisabled(e->getBoolAttribute(ATT_DISABLED));
}

/****************************************************************************
 *                                                                          *
 *                               BUTTON CONFIG                              *
 *                                                                          *
 ****************************************************************************/
/*
 * This is now deprecated, we keep the structure around for upgrades
 * but it will be immediately converted into Bindings.
 */

PUBLIC ButtonConfig::ButtonConfig()
{
	init();
}

PUBLIC ButtonConfig::ButtonConfig(const char* name)
{
	init();
	setName(name);
}

PUBLIC ButtonConfig::ButtonConfig(XmlElement* e)
{
	init();
	parseXml(e);
}

PRIVATE void ButtonConfig::init()
{
	mName = NULL;
}

PUBLIC ButtonConfig::~ButtonConfig()
{
    delete mName;
}

PUBLIC void ButtonConfig::setName(const char* name)
{
	delete mName;
	mName = CopyString(name);
}

PUBLIC const char* ButtonConfig::getName()
{
	return mName;
}

PUBLIC void ButtonConfig::toXml(XmlBuffer* b)
{
	b->addOpenStartTag(EL_BUTTON);
	b->addAttribute(ATT_FUNCTION_NAME, getName());
	b->add("/>\n");
}

PUBLIC void ButtonConfig::parseXml(XmlElement* e)
{
	setName(e->getAttribute(ATT_FUNCTION_NAME));
}

/****************************************************************************
 *                                                                          *
 *                                 KEY CONFIG                               *
 *                                                                          *
 ****************************************************************************/
//
// This is necessary only until everyone has upgraded once and
// converted this into a BindingConfig
//

PUBLIC KeyBinding::KeyBinding(int key, const char* name)
{
    mKey = key;
	mName = CopyString(name);
}

PUBLIC KeyBinding::~KeyBinding()
{
    delete mName;
}

PUBLIC int KeyBinding::getKey()
{
    return mKey;
}

PUBLIC const char* KeyBinding::getName()
{
	return mName;
}

PUBLIC KeyConfig::KeyConfig()
{
    init();
}

PUBLIC void KeyConfig::init()
{
    mBindings = NULL;
}

PUBLIC KeyConfig::KeyConfig(XmlElement* e)
{
    init();
    parseXml(e);
}

PUBLIC KeyConfig::~KeyConfig()
{
    if (mBindings != NULL) {
		for (int i = 0 ; mBindings[i] != NULL ; i++)
          delete mBindings[i];
        delete mBindings;
    }
}

PUBLIC void KeyConfig::parseXml(XmlElement* e)
{
    List* bindings = new List();

	for (XmlElement* child = e->getChildElement() ; child != NULL ; 
		 child = child->getNextElement()) {

		if (child->isName(EL_KEY_BINDING)) {
            int key = child->getIntAttribute(ATT_KEY);
            const char* cmd = child->getAttribute(ATT_FUNCTION_NAME);
            // filter out bogus bindings
            if (key > 0 && cmd != NULL) {
				KeyBinding* kb = new KeyBinding(key, cmd);
                bindings->add(kb);
			}
        }
	}

    if (bindings->size() > 0) 
      mBindings = (KeyBinding**)bindings->toArray();

    delete bindings;
}

PUBLIC void KeyConfig::toXml(XmlBuffer* b)
{
    if (mBindings != NULL) {
        b->addStartTag(EL_KEY_CONFIG);
        b->incIndent();
		for (int i = 0 ; mBindings[i] != NULL ; i++) {
            KeyBinding* kb = mBindings[i];
            int key = kb->getKey();
            const char* name = kb->getName();
            if (key > 0 && name != NULL) {
                b->addOpenStartTag(EL_KEY_BINDING);
                b->addAttribute(ATT_KEY, key);
                b->addAttribute(ATT_FUNCTION_NAME, name);
                b->add("/>\n");
            }
		}
        b->decIndent();
        b->addEndTag(EL_KEY_CONFIG);
    }
}

PUBLIC KeyBinding** KeyConfig::getBindings()
{
	return mBindings;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
