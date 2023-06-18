/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Global registry for application fonts.
 *
 */

#ifndef FONT_CONFIG_H
#define FONT_CONFIG_H

#include <stdio.h>

// can't reference these with "class" or else they'll
// get stuck in the Qwin namespace
#include "MessageCatalog.h"
#include "XomParser.h"
#include "XmlBuffer.h"

QWIN_BEGIN_NAMESPACE

class FontBinding {

  public:

    FontBinding();
    FontBinding(XmlElement* e);
    ~FontBinding();

    void setNext(FontBinding* b);
    FontBinding* getNext();

    void setName(const char* name);
    const char* getName();

    const char* getDisplayName();
    void setDisplayName(const char* s);

    void setFontName(const char* name);
    const char* getFontName();

    void setStyle(int style);
    int getStyle();

    void setSize(int size);
    int getSize();

    Font* getFont();

    void toXml(XmlBuffer* b);

  private:

    void init();
    void parseXml(XmlElement* e);

    FontBinding* mNext;
    char mName[128];
    char mFontName[128];
    int mStyle;
    int mSize;

    Font* mFont;
};

class FontConfig {
  public:

	static const char* ELEMENT;

    FontConfig();
    FontConfig(class XmlElement* e);
    ~FontConfig();

    void addBinding(FontBinding* b);
    FontBinding* getBindings();
    FontBinding* getBinding(const char* name);

    void toXml(XmlBuffer* b);

    FontConfig* clone();
    void assign(FontConfig* src);

    Font* intern(const char* name, int defaultSize);
    Font* intern(const char* name, int defaultStyle, int defaultSize);

  private:

    void init();
    void parseXml(XmlElement* e);
    FontBinding* stealBindings();

    FontBinding* mBindings;
};

/**
 * The global singleton Palette.
 */
extern FontConfig* GlobalFontConfig;

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/






/*********    UiDimension   ************/

class UiDimension {

  public:

    UiDimension();
    UiDimension(XmlElement* e);
    ~UiDimension();

    void setNext(UiDimension* b);
    UiDimension* getNext();

    void setName(const char* name);
    const char* getName();


	  void setWidth(int i);
    int getWidth();

	  void setHeight(int i);
    int getHeight();

    void setDiameter(int i);
    int getDiameter();

    void setSpacing(int i);
    int getSpacing();


    void toXml(XmlBuffer* b);

  private:

    void init();
    void parseXml(XmlElement* e);

    UiDimension* mNext;
    char mName[128];
    int mWidth;
    int mHeight;
    int mDiameter;
    int mSpacing;
};



/*********    UiDimensions   ************/

class UiDimensions {
  public:

	static const char* ELEMENT;

    UiDimensions();
    UiDimensions(class XmlElement* e);
    ~UiDimensions();

    void addDimension(UiDimension* d);
    UiDimension* getDimensions();
    UiDimension* getDimension(const char* name);

    void toXml(XmlBuffer* b);

  private:

    void init();
    void parseXml(XmlElement* e);
    UiDimension* mDimensions;
};


/**
 * The global singleton Dimensions.
 */
extern UiDimensions* GlobalUiDimensions;








QWIN_END_NAMESPACE
#endif

