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
QWIN_END_NAMESPACE
