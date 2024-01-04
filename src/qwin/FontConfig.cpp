/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * The FontConfig is similar to the Palette, it maintains a global
 * registry of fonts keyed by some application-specific id. 
 * A FontConfig is expected to be maintained in an XML file, edited
 * by the application UI.  This is read when the application starts
 * and set into the singleton GlobalFontConfig.  The singleton is sort
 * of a kludge but is saves having to pass this to every component
 * that needs to lookup a font.
 *
 */

#include <stdio.h>
#include <memory.h>
#include <string.h>

#include "Util.h"
#include "Trace.h"
#include "MessageCatalog.h"
#include "XmlModel.h"
#include "XmlBuffer.h"

#include "Qwin.h"
#include "FontConfig.h"

QWIN_BEGIN_NAMESPACE

/****************************************************************************
 *                                                                          *
 *                               XML CONSTANTS                              *
 *                                                                          *
 ****************************************************************************/

const char* FontConfig::ELEMENT = "FontConfig";

#define EL_FONT_BINDING "FontBinding"

#define ATT_NAME "name"
#define ATT_FONT_NAME "fontName"
#define ATT_STYLE "style"
#define ATT_SIZE "size"

/****************************************************************************
 *                                                                          *
 *                               GLOBAL CONFIG                              *
 *                                                                          *
 ****************************************************************************/

PUBLIC FontConfig* GlobalFontConfig = new FontConfig();

/****************************************************************************
 *                                                                          *
 *                                FONT BINDING                              *
 *                                                                          *
 ****************************************************************************/

PUBLIC FontBinding::FontBinding()
{
    init();
}

PUBLIC FontBinding::FontBinding(XmlElement* e) 
{
    init();
    parseXml(e);
}

PUBLIC void FontBinding::init()
{
    mNext = NULL;
    strcpy(mName, "");
    strcpy(mFontName, "");
    mStyle = 0;
    mSize = 0;
    mFont = NULL;
}

PUBLIC FontBinding::~FontBinding()
{
    FontBinding* el, *next = NULL;
    for (el = mNext ; el != NULL ; el = next) {
        next = el->mNext;
        el->mNext = NULL;
        delete el;
    }
}

PUBLIC FontBinding* FontBinding::getNext()
{
    return mNext;
}

PUBLIC void FontBinding::setNext(FontBinding* c)
{
    mNext = c;
}

PUBLIC const char* FontBinding::getName()
{
	return mName;
}

PUBLIC void FontBinding::setName(const char* name)
{
    CopyString(name, mName, sizeof(mName));
}

/**
 * !! TODO Need a messages catalog key for a display name.
 */
PUBLIC const char* FontBinding::getDisplayName()
{
    return mName;
}

PUBLIC void FontBinding::setDisplayName(const char* s)
{
    // ignore
}

PUBLIC const char* FontBinding::getFontName()
{
	return mFontName;
}

PUBLIC void FontBinding::setFontName(const char* name)
{
    CopyString(name, mFontName, sizeof(mFontName));
}

PUBLIC int FontBinding::getStyle()
{
    return mStyle;
}

PUBLIC void FontBinding::setStyle(int i)
{
    mStyle = i;
}

PUBLIC int FontBinding::getSize()
{
    return mSize;
}

PUBLIC void FontBinding::setSize(int i)
{
    mSize = i;
}

PUBLIC Font* FontBinding::getFont()
{
    if (mFont == NULL) {
        const char* fname;
        if (strlen(mFontName) > 0)
          fname = mFontName;
        else
          fname = "Arial";
        int size = mSize;
        if (size == 0) size = 10;

        mFont = Font::getFont(fname, mStyle, size);
    }
    return mFont;
}

PRIVATE void FontBinding::parseXml(XmlElement* e) 
{
    setName(e->getAttribute(ATT_NAME));
    setFontName(e->getAttribute(ATT_FONT_NAME));

    mStyle = e->getIntAttribute(ATT_STYLE);
    mSize = e->getIntAttribute(ATT_SIZE);
}

PUBLIC void FontBinding::toXml(XmlBuffer* b)
{
	b->addOpenStartTag(EL_FONT_BINDING);

    if (strlen(mName) > 0)
	  b->addAttribute(ATT_NAME, mName);

    if (strlen(mFontName) > 0)
	  b->addAttribute(ATT_FONT_NAME, mFontName);

    if (mStyle > 0) 
      b->addAttribute(ATT_STYLE, mStyle);

    if (mSize > 0) 
      b->addAttribute(ATT_SIZE, mSize);

	b->closeEmptyElement();
}

/****************************************************************************
 *                                                                          *
 *                                FONT CONFIG                               *
 *                                                                          *
 ****************************************************************************/

PUBLIC FontConfig::FontConfig()
{
    mBindings = NULL;
}

PUBLIC FontConfig::FontConfig(XmlElement* e)
{
    mBindings = NULL;
    parseXml(e);
}

PUBLIC FontConfig::~FontConfig()
{
    delete mBindings;
}

PUBLIC FontBinding* FontConfig::getBindings()
{
	return mBindings;
}

PRIVATE FontBinding* FontConfig::stealBindings()
{
    FontBinding* bindings = mBindings;
    mBindings = NULL;
	return bindings;
}

/**
 * Lookup a biding for a key. 
 * Obviously not effecient if you have a lot of bindings.
 */
PUBLIC FontBinding* FontConfig::getBinding(const char* name)
{
    FontBinding* found = NULL;
    for (FontBinding* b = mBindings ; b != NULL ; b = b->getNext()) {
        // TODO: Search on display name if we have one
        if (StringEqual(name, b->getName())) {
            found = b;
            break;
        }
    }
    return found;
}

/**
 * Add a binding.
 */
PUBLIC void FontConfig::addBinding(FontBinding* b)
{
	// keep them ordered
	FontBinding *prev;
	for (prev = mBindings ; prev != NULL && prev->getNext() != NULL ; 
		 prev = prev->getNext());

	if (prev == NULL)
	  mBindings = b;
	else
	  prev->setNext(b);
}

PRIVATE void FontConfig::parseXml(XmlElement* e)
{
    FontBinding* bindings = NULL;
    FontBinding* last = NULL;

	for (XmlElement* child = e->getChildElement() ; child != NULL ; 
		 child = child->getNextElement()) {

		if (child->isName(EL_FONT_BINDING)) {
            FontBinding* b = new FontBinding(child);
            if (last == NULL)
              bindings = b;
            else
              last->setNext(b);
            last = b;
        }
    }

    delete mBindings;
    mBindings = bindings;
}

PUBLIC void FontConfig::toXml(XmlBuffer* b)
{
    if (mBindings != NULL) {
        b->addStartTag(ELEMENT);
        b->incIndent();
        for (FontBinding* f = mBindings ; f!= NULL ; f = f->getNext())
          f->toXml(b);
        b->decIndent();
        b->addEndTag(ELEMENT);
    }
}

/**
 * Make a full copy of the config.
 */
PUBLIC FontConfig* FontConfig::clone()
{
    FontConfig* clone = NULL;

	XmlBuffer* b = new XmlBuffer();
	toXml(b);
	char* xml = b->stealString();
    if (xml == NULL || strlen(xml) == 0) {
        // sigh, XomParser throws an exception if there is no input
        // avoid that and make an empty one
        clone = new FontConfig();
    }
    else {
        XomParser* p = new XomParser();
        XmlDocument* d = p->parse(xml);
        if (d != NULL) {
            XmlElement* e = d->getChildElement();
            clone = new FontConfig(e);
            delete d;
        }
        delete p;
        delete xml;
        delete b;

        // KLUDGE: displayNames are not serialized in the XML because
        // we localize them at runtime.  But here we need to preserve them,
        // in the clone.  I'd rather not have to introduce message catalog
        // awareness down here, so assume the lists are the same size
        // and clone manually.

        FontBinding* srcBinding = mBindings;
        FontBinding* destBinding = clone->getBindings();
        while (srcBinding != NULL && destBinding != NULL) {
            destBinding->setDisplayName(srcBinding->getDisplayName());
            srcBinding = srcBinding->getNext();
            destBinding = destBinding->getNext();
        }
    }

    return clone;
}

/**
 * Install a new font configuration after reading one from a file
 * or editing.  Callers expect this to be treated like Palette,
 * the source FontConfig remains owned by the caller and can be deleted
 * at any time, we have to make an autonomous copy.
 *
 * Unlike Palette we don't have anything in here that applications
 * are going to be holding onto so we can just replace the entire
 * object with a copy of the source.
 */
PUBLIC void FontConfig::assign(FontConfig* src)
{
    if (src != NULL && src != GlobalFontConfig) {

        FontConfig* clone = src->clone();
        delete mBindings;
        mBindings = clone->stealBindings();
        delete clone;
    }
}

/**
 * This is called by components as they need fonts.
 * Look one up or boostrap one with the given defaults.
 * Not bothering with name and style, assume everything is Arial.
 */
PUBLIC Font* FontConfig::intern(const char* name, int defaultStyle, 
                                int defaultSize)
{
    Font* font = NULL;

    FontBinding* binding = getBinding(name);
    if (binding == NULL) {
        binding = new FontBinding();
        binding->setName(name);
        binding->setStyle(defaultStyle);
        binding->setSize(defaultSize);
        addBinding(binding);
    }
    
    font = binding->getFont();
    if (font == NULL) {
        // shouldn't be here??
        Trace(1, "InvalidFontBinding for %s\n", name);
        font = Font::getFont("Arial", 0, defaultSize);
    }

    return font;
}

PUBLIC Font* FontConfig::intern(const char* name, int defaultSize)
{
    return intern(name, 0, defaultSize);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/





/***************************************************************************
 *                           UI DIMENSIONS CLASS                           *    
 *                        (Cas)    #003 07/05/2023                         *
 ***************************************************************************/



/****************************************************************************
 *                                                                          *
 *                               XML CONSTANTS                              *
 *                                                                          *
 ****************************************************************************/

const char* UiDimensions::ELEMENT = "UiDimensions";

#define EL_UIDIMENSION "UiDimension"

#define ATT_UI_NAME "name"
#define ATT_UI_WIDTH "width"
#define ATT_UI_HEIGHT "height"
#define ATT_UI_DIAMETER "diameter"
#define ATT_UI_SPACING "spacing"

/****************************************************************************
 *                                                                          *
 *                               GLOBAL CONFIG                              *
 *                                                                          *
 ****************************************************************************/

//PUBLIC UiDimensions* GlobalUiDimensions = new UiDimensions();   //<---Cas | it should be assign after with the "clone" of the mConfig ones!!


/****************************************************************************
 *                                                                          *
 *                               UIDimension                                *
 *                                                                          *
 ****************************************************************************/


PUBLIC UiDimension::UiDimension()
{
	init();
}

PUBLIC UiDimension::UiDimension(XmlElement* e)
{
    Trace(3, "new UiDimension::UiDimension");
	init();
	parseXml(e);

}

// PUBLIC UiDimension::UiDimension(const char* name)
// {
// 	init();
// 	setName(name);
// }

PRIVATE void UiDimension::init()
{
    strcpy(mName, "");
	//mName = NULL;
	mWidth = 0;
	mHeight = 0;
    mDiameter = 0;
    mSpacing = 0;

    //Trace(3, "mNext = null | UiDimension::UiDimension");
    mNext = NULL; //<--- Ma vai a dormire :D C++ del cavolo 14/05/2023, il bug dell'anno!
}

PUBLIC UiDimension::~UiDimension()
{
	UiDimension* el, *next = NULL;
    for (el = mNext ; el != NULL ; el = next) {
        next = el->mNext;
        el->mNext = NULL;
        delete el;
    }
}




PUBLIC UiDimension* UiDimension::getNext()
{
    return mNext;
}

PUBLIC void UiDimension::setNext(UiDimension* c)
{
    mNext = c;
}



PUBLIC const char* UiDimension::getName()
{
	return mName;
}

PUBLIC void UiDimension::setName(const char* name)
{
    //Trace(3,"UiDimension::setName>%s", name);
    CopyString(name, mName, sizeof(mName));
}




PUBLIC void UiDimension::setWidth(int i)
{
	mWidth = i;
}

PUBLIC int UiDimension::getWidth()
{
	return mWidth;
}


PUBLIC void UiDimension::setDiameter(int i)
{
	mDiameter = i;
}

PUBLIC int UiDimension::getDiameter()
{
	return mDiameter;
}


PUBLIC void UiDimension::setSpacing(int i)
{
     Trace(3, " UiDimension::setSpacing(i) : %i, i");
	mSpacing = i;
}

PUBLIC int UiDimension::getSpacing()
{
	return mSpacing;
}

PUBLIC void UiDimension::setHeight(int i)
{
	mHeight = i;
}

PUBLIC int UiDimension::getHeight()
{
	return mHeight;
}

PUBLIC void UiDimension::toXml(XmlBuffer* b)
{
    //Trace(3, " UiDimension::toXml(XmlBuffer)! v2");
	b->addOpenStartTag(EL_UIDIMENSION);
	b->addAttribute(ATT_UI_NAME, mName);
	
    if(mWidth > 0)
        b->addAttribute(ATT_UI_WIDTH, mWidth);
    
    if(mHeight > 0)
	    b->addAttribute(ATT_UI_HEIGHT, mHeight);
    
    if(mDiameter > 0)
        b->addAttribute(ATT_UI_DIAMETER, mDiameter);

    if(mSpacing > 0)
        b->addAttribute(ATT_UI_SPACING, mSpacing);

    b->closeEmptyElement();
   // Trace(3, "End UiDimension::toXml(XmlBuffer)!");
}

PUBLIC void UiDimension::parseXml(XmlElement* e)
{
	setName(e->getAttribute(ATT_UI_NAME));
	setWidth(e->getIntAttribute(ATT_UI_WIDTH));
	setHeight(e->getIntAttribute(ATT_UI_HEIGHT));
    setDiameter(e->getIntAttribute(ATT_UI_DIAMETER));
    setSpacing(e->getIntAttribute(ATT_UI_SPACING));
}




/****************************************************************************
 *                                                                          *
 *                               UIDimensions                               *
 *                                                                          *
 ****************************************************************************/


PUBLIC UiDimensions::UiDimensions()
{
    mDimensions = NULL;
}

PUBLIC UiDimensions::UiDimensions(XmlElement* e)
{
    Trace(3, "UiDimensions::init(XmlElement)"); //c

    mDimensions = NULL;
    parseXml(e);
}

PUBLIC UiDimensions::~UiDimensions()
{
    delete mDimensions;
}

PUBLIC UiDimension* UiDimensions::getDimensions()
{
	return mDimensions;
}


/**
 * Lookup a biding for a key. 
 * Obviously not effecient if you have a lot of bindings.
 */
PUBLIC UiDimension* UiDimensions::getDimension(const char* name)
{
    Trace(3, "UiDimensions::getDimension |%s|", name); //c
    //Trace(3, "UiDimensions::getDimension"); //c
    UiDimension* found = NULL;
    
    //Trace(3, "Ciclo For"); //c debug
    for (UiDimension* d = mDimensions ; d != NULL ; d = d->getNext()) {

        //Trace(3, "current -> |%s|",d->getName());  //c debug
        
        if (StringEqual(name, d->getName())) { 
            //Trace(3, "found!"); //c debug
            found = d;
            break;
        }
    }
    
    //Trace(3, "return UIDIm"); //c debug
    return found;
}

/**
 * Add a binding.
 */
PUBLIC void UiDimensions::addDimension(UiDimension* b)
{

    Trace(3, "UiDimensions::addBinding(XmlElement)"); //c


	// keep them ordered
	UiDimension *prev;
	for (prev = mDimensions ; prev != NULL && prev->getNext() != NULL ; 
		 prev = prev->getNext());

	if (prev == NULL)
	  mDimensions = b;
	else
	  prev->setNext(b);
}

PRIVATE void UiDimensions::parseXml(XmlElement* e)
{
    UiDimension* dimensions = NULL;
    UiDimension* last = NULL;
    
    Trace(3, "UiDimensions::parseXml(XmlElement)"); //c

	for (XmlElement* child = e->getChildElement() ; child != NULL ; 
		 child = child->getNextElement()) {

        Trace(3, "for"); //c

		if (child->isName(EL_UIDIMENSION)) {
            Trace(3, "UiDimensions::parseXml!");
            UiDimension* d = new UiDimension(child);
            if (last == NULL)
              dimensions = d;
            else
              last->setNext(d);
            last = d;
        }
        else {
             Trace(3, "Unexpected Token?");
        }

    }

    delete mDimensions;
    mDimensions = dimensions;
}

PUBLIC void UiDimensions::toXml(XmlBuffer* b)
{
    if (mDimensions != NULL) {
        Trace(3, "***********************************UiDimensions::toXml!");
        b->addStartTag(ELEMENT);
        b->incIndent();
        
        for (UiDimension* d = mDimensions ; d != NULL ; d = d->getNext()){
          Trace(3, "for - UiDimensions::toXml! |%s|", d->getName());
          d->toXml(b);
        }
        
        b->decIndent();
        b->addEndTag(ELEMENT);
        Trace(3, "***********************************END-UiDimensions::toXml!");
    }
}






QWIN_END_NAMESPACE
