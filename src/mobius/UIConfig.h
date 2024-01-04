/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Model for persistent UI configuration.
 *
 */


#ifndef UI_CONFIG_H
#define UI_CONFIG_H

// sigh, have to get Palette, FontConfig, and Bounds or else the forward
// declarations confuse the Mac compiler
#include "Qwin.h"
#include "QwinExt.h"
#include "FontConfig.h"

QWIN_USE_NAMESPACE

#define DEFAULT_REFRESH_INTERVAL 100
#define DEFAULT_MESSAGE_DURATION 2
#define DEFAULT_ALERT_INTERVALS 10

/**
 * Holds display component locations.
 * This needs to be in the UI model because we can save it
 * with the configuration.  But keep the model generic and independent.
 * We only store names and coordinates.
 */
class Location : public ListElement
{
  public:

	Location();
	Location(class XmlElement* e);
	Location(const char* name);
	~Location();

	void setName(const char *name);
	void setX(int i);
	void setY(int i);
    void setDisabled(bool b);

	const char* getName();
	int getX();
	int getY();
    bool isDisabled();

	void toXml(class XmlBuffer* b);

  private:

	void init();
	void parseXml(class XmlElement* e);

	char* mName;
	int mX;
	int mY;
    bool mDisabled;

};

/**
 * DEPRECATED: We formerly defined UI buttons in UICpnfig but now
 * they are reprensted as Bindings in the MobiusConfig so we can
 * treat them like other triggers.  This may still exist in older
 * UIConfigs but will be immediately upgraded to Bindings when it
 * is read.
 */
class ButtonConfig : public ListElement
{
  public:

    ButtonConfig();
    ButtonConfig(const char* name);
    ButtonConfig(class XmlElement* e);
    ~ButtonConfig();

    void setName(const char *name);
    const char* getName();

    void toXml(class XmlBuffer* b);
    void parseXml(class XmlElement* e);

  private:

    void init();

    char* mName;
};

/**
 * OBSOLETE: Upgraded at rumtime into Bindings inside a BindingConfig.
 */
class KeyBinding {

  public:

    KeyBinding(int key, const char* cmd);
    ~KeyBinding();

    int getKey();
    const char* getName();

  private:

    int mKey;
    char* mName;

};

/**
 * OBSOLETE: Upgraded at rumtime into Bindings inside a BindingConfig.
 */
class KeyConfig {

  public:

    KeyConfig();
    KeyConfig(class XmlElement* e);
    ~KeyConfig();

	KeyBinding** getBindings();

    void parseXml(class XmlElement* e);
    void toXml(class XmlBuffer* b);

  private:

    void init();

    KeyBinding** mBindings;

};

class UIConfig
{
  public:

    UIConfig();
    UIConfig(const char* xml);
    UIConfig(class XmlElement* e);
    ~UIConfig();
    UIConfig* clone();

    void setName(const char* name);

    void setRefreshInterval(int i);
    int getRefreshInterval();

    // not used, what was this for?
    void setAlertIntervals(int i);
    int getAlertIntervals();

    void setMessageDuration(int i);
    int getMessageDuration();

	void setBounds(class Bounds* b);
	class Bounds* getBounds();
	void setMaximized(bool b);
	bool isMaximized();



  // experimental, never exposed
	void setNoMenu(bool b);
	bool isNoMenu();
	void setPaintTrace(bool b);
	bool isPaintTrace();

	void setPalette(class Palette* p);
	class Palette* getPalette();
  class Palette* stealPalette();

	void setFontConfig(class FontConfig* p);
	class FontConfig* getFontConfig();
  class FontConfig* stealFontConfig();

  void setUiDimensions(class UiDimensions* d);  //#003
  class UiDimensions* getUiDimensions(); //#003

  // void setRadarDiameter(int i);     //#004
  // int getRadarDiameter();           //#004

  // void setLevelMeterHeight(int i);  //#004
  // int getLevelMeterHeight();        //#004

  // void setLevelMeterWidth(int i);  //#004
  // int getLevelMeterWidth();        //#004



	List* getLocations();
	List* stealLocations();
	Location* getLocation(const char* name);
	void addLocation(Location* l);
	void updateLocation(const char* name, int x, int y);
	void removeLocation(Location* l);
	void resetLocations();

    List* getButtons();
    void setButtons(ObjectList* l);
    void addButton(ButtonConfig* b);

    KeyConfig* getKeyConfig();
    KeyConfig* stealKeyConfig();
    void setKeyConfig(KeyConfig* c);
    class Function* getKeyFunction(int key);

    StringList* getParameters();
    StringList* stealParameters();
    void setParameters(StringList* l);
    void addParameter(const char* name);

    StringList* getFloatingStrip();
    void addFloatingStrip(const char* s);
    void setFloatingStrip(StringList* l);

    StringList* getFloatingStrip2();
    void setFloatingStrip2(StringList* l);

    StringList* getDockedStrip();
    void addDockedStrip(const char* s);
    void setDockedStrip(StringList* l);

	char* toXml();
	void toXml(class XmlBuffer* b);
    const char* getError();

  private:

    void init();
    void deleteButtons();
    void parseXml(const char* xml);
    void parseXml(class XmlElement* e);
    void checkDisplayComponents();

    char mError[256];   // captured parser error
    char* mName;
    int mRefreshInterval;
    int mAlertIntervals;    // unused, what was this for?
    int mMessageDuration;
    class Bounds* mBounds;
    bool mMaximized;
    bool mNoMenu;   // disables the window menu bar, never exposed
    bool mPaintTrace; // enables component paint trace
    
    //Moved to #003 UiDimensions
    // int mRadarDiameter;     // #004 RadarDiameter Override
    // int mLevelMeterHeight;  // #004 LevelMeterHeight Override
    // int mLevelMeterWidth;   // #004 LevelMeterWidth Override

    class Palette* mPalette;
    class FontConfig* mFontConfig;
    ObjectList* mLocations;
    ObjectList* mButtons;
    KeyConfig* mKeyConfig;
    StringList* mParameters;
    StringList* mFloatingStrip;
    StringList* mFloatingStrip2;
    StringList* mDockedStrip;

    class UiDimensions* mDimensions; //#003

};

#endif
/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
