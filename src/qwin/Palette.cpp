/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * The Palette object is relatively general, but the GlobalPalette
 * is sort of a kludge.  I want to be able to make Components that
 * track changes to Palette colors, but not require them to lookup their
 * current color by name every time they paint themselves.  By interning
 * the GlobalPalette, they can obtain an handle to a PaletteColor
 * that will never be released, and can use whatever Color happens
 * to be there.  
 *
 * This means though that modifying the GlobalPalette isn't simply
 * setting a new Palette object, you have to merge the new colors
 * in to the existing Palette/PaletteColor structure so that the 
 * pointers to them in the Components remain valid.  This is a little
 * fragile, but the alternative is runtime searching which could
 * be expensive if there are many colors in the palette.
 *
 */

#include <stdio.h>
#include <memory.h>
#include <string.h>

#include "Util.h"
#include "MessageCatalog.h"
#include "XmlModel.h"
#include "XmlBuffer.h"
#include "XomParser.h"

#include "Qwin.h"
#include "Palette.h"

QWIN_BEGIN_NAMESPACE

/****************************************************************************
 *                                                                          *
 *                               PALETTE COLOR                              *
 *                                                                          *
 ****************************************************************************/

/*
 * Maintains a single named Color
 */

PaletteColor::PaletteColor(const char* name, int rgb)
{
    mNext = NULL;
	mKey = 0;
    mName = CopyString(name);
	mDisplayName = NULL;
    mColor = new Color(rgb);
}

PaletteColor::~PaletteColor()
{
    PaletteColor* next = NULL;
    PaletteColor* pc;
    
    for (pc = mNext ; pc != NULL ; pc = next) {
        next = pc->mNext;
        pc->mNext = NULL;
        delete pc;
    }
    
    delete mName;
    delete mDisplayName;
    delete mColor;
}

PUBLIC void PaletteColor::setKey(int key)
{
	mKey = key;
}

PUBLIC int PaletteColor::getKey()
{
	return mKey;
}

PUBLIC void PaletteColor::setDisplayName(const char* s)
{
	delete mDisplayName;
	mDisplayName = NULL;
	if (s != NULL)
	  mDisplayName = CopyString(s);
}

PUBLIC void PaletteColor::localize(MessageCatalog* cat)
{
	if (mKey > 0) {
		const char* msg = cat->get(mKey);
		if (msg != NULL)
		  setDisplayName(msg);
	}
}

PUBLIC PaletteColor* PaletteColor::getNext()
{
    return mNext;
}

PUBLIC void PaletteColor::setNext(PaletteColor* c)
{
    mNext = c;
}

PUBLIC const char* PaletteColor::getName()
{
    return mName;
}

PUBLIC const char* PaletteColor::getDisplayName()
{
    return ((mDisplayName != NULL) ? mDisplayName : mName);
}

PUBLIC Color* PaletteColor::getColor()
{
    return mColor;
}

PUBLIC void PaletteColor::setColor(Color* c)
{	
	if (c != NULL)
	  mColor = c;
}

/****************************************************************************
 *                                                                          *
 *                                  PALETTE                                 *
 *                                                                          *
 ****************************************************************************/

Palette::Palette()
{
    mColors = NULL;
}

Palette::Palette(XmlElement* e)
{
    mColors = NULL;
    parseXml(e);
}

Palette::~Palette()
{
    delete mColors;
}

PUBLIC Palette* Palette::clone()
{
    Palette* clone = NULL;

	XmlBuffer* b = new XmlBuffer();
	toXml(b);
	char* xml = b->stealString();
    if (xml == NULL || strlen(xml) == 0) {
        // sigh, XomParser throws an exception if there is no input
        // avoid that and make an empty one
        clone = new Palette();
    }
    else {
        XomParser* p = new XomParser();
        XmlDocument* d = p->parse(xml);
        if (d != NULL) {
            XmlElement* e = d->getChildElement();
            clone = new Palette(e);
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

        PaletteColor* srcColor = mColors;
        PaletteColor* destColor = clone->getColors();
        while (srcColor != NULL && destColor != NULL) {
            destColor->setDisplayName(srcColor->getDisplayName());
            srcColor = srcColor->getNext();
            destColor = destColor->getNext();
        }
    }

    return clone;
}

PaletteColor* Palette::getColors()
{
    return mColors;
}

void Palette::add(PaletteColor* c)
{
	// maintain order
    if (c != NULL) {
		PaletteColor* last = NULL;
		for (last = mColors ; 
			 last != NULL && last->mNext != NULL ; last = last->mNext);
		if (last != NULL)
		  last->mNext = c;
		else
		  mColors = c;
	}
}

void Palette::add(const char* name, int rgb)
{
    Color* c = getColor(name, NULL);
    if (c != NULL)
      c->setRgb(rgb);
    else if (name != NULL)
      add(new PaletteColor(name, rgb));
}

PUBLIC PaletteColor* Palette::getPaletteColor(const char* name)
{
    PaletteColor* found = NULL;
    if (mColors != NULL && name != NULL) {
        for (PaletteColor* c = mColors ; c != NULL ; c = c->mNext) {
            if (!strcmp(name, c->mName)) {
                found = c;
                break;
            }
        }
    }
    return found;
}

/**
 * Return a named color in the palette.
 * If the color is not defined, create one with a default color.
 * Note that it is important that we make a local copy of the default
 * rather than just returning, this will allow the palette to own
 * the Color object so that it can track changes made in the 
 * PaletteDialog.
 */
PUBLIC Color* Palette::getColor(const char* name, Color* dflt)
{
    Color* color;
	PaletteColor* pc = getPaletteColor(name);
	if (pc != NULL)
	  color = pc->getColor();
    else {
        // default to something visible if not passed
        int rgb = (dflt != NULL) ? dflt->getRgb() : 65535;
        pc = new PaletteColor(name, rgb);
        add(pc);
        color = pc->getColor();
    }
	return color;
}

/**
 * Return a named color.
 */
PUBLIC Color* Palette::getColor(const char* name)
{
    return getColor(name, NULL);
}

void Palette::remove(PaletteColor* pc)
{
    PaletteColor* found = NULL;
    PaletteColor* prev = NULL;

    for (found = mColors ; found != NULL ; found = found->getNext()) {
        if (found != pc)
          prev = found;
        else
          break;  
    }

    if (found != NULL) {
        if (prev == NULL)
          mColors = pc->getNext();
        else
          prev->setNext(pc->getNext());
        pc->setNext(NULL);
    }
}

/**
 * Copy the colors defined in a new palette into anohter.
 * 
 * This is used when changing the Global Palette so that references
 * to the palette from components remain valid.  It is also used
 * by PaletteDialog to commit changes made in a copy of the source palette
 * back into the palette.
 *
 * The source palette is not modified.
 *
 */
PUBLIC void Palette::assign(Palette* p)
{
    if (p != NULL) {
        PaletteColor* next = NULL;

        for (PaletteColor* pc = p->getColors() ; pc != NULL ; pc = next) {
            next = pc->getNext();

            PaletteColor* mypc = getPaletteColor(pc->getName());
            if (mypc == NULL) {
                Color* color = pc->getColor();
                mypc = new PaletteColor(pc->getName(), color->getRgb());
				mypc->setKey(pc->getKey());
				mypc->setDisplayName(pc->getDisplayName());
                add(mypc);
            }
            else {
                // transfer color values but not structure
                Color* color = pc->getColor();
                Color* mycolor = mypc->getColor();
                mycolor->setRgb(color->getRgb());
				mypc->setKey(pc->getKey());
				if (pc->getDisplayName() != NULL)
				  mypc->setDisplayName(pc->getDisplayName());
            }
        }
    }
}

PUBLIC void Palette::localize(MessageCatalog* cat)
{
	for (PaletteColor* pc = mColors ; pc != NULL ; pc = pc->getNext()) {
		pc->localize(cat);
	}
}

/****************************************************************************
 *                                                                          *
 *                                    XML                                   *
 *                                                                          *
 ****************************************************************************/

const char* Palette::ELEMENT = "Palette";

#define EL_PALETTE_COLOR "PaletteColor"
#define ATT_NAME "name"
#define ATT_KEY "key"
#define ATT_DISPLAY_NAME "displayName"
#define ATT_RGB "rgb"

void Palette::parseXml(XmlElement* e)
{
    PaletteColor* last = NULL;

    for (XmlElement* child = e->getChildElement() ; child != NULL ; 
         child = child->getNextElement()) {

        const char* name = child->getAttribute(ATT_NAME);
        int rgb = child->getIntAttribute(ATT_RGB);
        if (name != NULL) {
            PaletteColor* pc = new PaletteColor(name, rgb);
			pc->setKey(child->getIntAttribute(ATT_KEY));
			pc->setDisplayName(child->getAttribute(ATT_DISPLAY_NAME));
            if (last == NULL)
              mColors = pc;
            else
              last->mNext = pc;
            last = pc;
        }
    }
}

void Palette::toXml(XmlBuffer* b)
{
    if (mColors != NULL) {
        b->addStartTag(ELEMENT);
        b->incIndent();
        for (PaletteColor* pc = mColors ; pc != NULL ; pc = pc->mNext) {
            b->addOpenStartTag(EL_PALETTE_COLOR);
            b->addAttribute(ATT_NAME, pc->mName);
            b->addAttribute(ATT_KEY, pc->mKey);
			// if we have a key, don't save the display name to reduce clutter
			if (pc->mKey == 0)
			  b->addAttribute(ATT_DISPLAY_NAME, pc->mDisplayName);
            if (pc->mColor != NULL)
              b->addAttribute(ATT_RGB, (int)(pc->mColor->getRgb()));
            b->add("/>\n");
        }
        b->decIndent();
        b->addEndTag(ELEMENT);
    }
}

/****************************************************************************
 *                                                                          *
 *                               GLOBAL PALETTE                             *
 *                                                                          *
 ****************************************************************************/

PUBLIC Palette* GlobalPalette = new Palette();

/****************************************************************************
 *                                                                          *
 *                               COLOR BUTTON                               *
 *                                                                          *
 ****************************************************************************/

/**
 * Keep this out of QwinExt.h, it conflicts with color definition
 * object in Mobius.
 */
class ColorButton : public Container, public MouseInputAdapter  {

  public:

    ColorButton(class PaletteDialog* dlg, PaletteColor* pc);
	void mousePressed(MouseEvent* e);
	void paint(Graphics* g);
    PaletteColor* getColor();
    
  private:

	class PaletteDialog* mDialog;
	PaletteColor* mColor;
    Panel* mPanel;

};

PUBLIC ColorButton::ColorButton(PaletteDialog* dlg, PaletteColor* color)
{
    setLayout(new HorizontalLayout(8));

	mDialog = dlg;
    mColor = color;

	// formerly had a color panel and an Edit button, but now that
	// we listen for mouse events in the panel, don't need a button

    mPanel = new Panel();
    mPanel->setBackground(color->getColor());
	mPanel->addMouseListener(this);
    // preferred size doesn't work with containers, have to use a strut
    mPanel->add(new Strut(20, 20));

	// this looks nice on Mac
	setBorder(Border::BlackLine);

    add(mPanel);
}

PUBLIC void ColorButton::paint(Graphics* g) 
{
	tracePaint();
	Container::paint(g);
}

PUBLIC PaletteColor* ColorButton::getColor()
{
    return mColor;
}

PUBLIC void ColorButton::mousePressed(MouseEvent* e)
{
    Color* color = mColor->getColor();
    ColorDialog* cd = new ColorDialog(getWindow());

	const char* title = mDialog->getColorTitle();
	if (title == NULL)
	  title = "Palette Color";
    cd->setTitle(title);

    cd->setRgb(color->getRgb());
    cd->show();
    color->setRgb(cd->getRgb());
    delete cd;

	// parent needs to refresh the colors
	mParent->invalidate();
}

/****************************************************************************
 *                                                                          *
 *                               PALETTE DIALOG                             *
 *                                                                          *
 ****************************************************************************/

#define MAX_ROWS 10

PUBLIC PaletteDialog::PaletteDialog(Window* parent, Palette* p) 
{
	mColorTitle = NULL;

	setParent(parent);
	setModal(true);

	// size defaulting doesn't seem to work?
	setWidth(100);
	setHeight(100);

	setTitle("Palette");
	setInsets(new Insets(20, 20, 20, 0));

    // make a copy so we can cancel
    mSrcPalette = p;
    mPalette = p->clone();

	Panel* root = getPanel();
	root->setLayout(new HorizontalLayout(20));

	FormPanel* form = new FormPanel();
	root->add(form);

    PaletteColor* color = mPalette->getColors();
	int count = 0;
	while (color != NULL) {
        ColorButton* cb = new ColorButton(this, color);
        cb->addActionListener(this);
        form->add(color->getDisplayName(), cb);
		color = color->getNext();
		count++;
		if (color != NULL && count >= MAX_ROWS) {
			form = new FormPanel();
			root->add(form);
			count = 0;
		}
		
	}

}

PaletteDialog::~PaletteDialog()
{
    delete mPalette;
	delete mColorTitle;
}

/**
 * Ugh, I hate this interface, but  I don't want to wire in catalog
 * key constants at this level.
 */
PUBLIC void PaletteDialog::localize(const char* title, const char* title2)
{
	if (title != NULL)
	  setTitle(title);
	if (title2 != NULL) {
		delete mColorTitle;
		mColorTitle = CopyString(title2);
	}
}

PUBLIC const char* PaletteDialog::getColorTitle()
{
	return mColorTitle;
}

PUBLIC bool PaletteDialog::commit()
{
    mSrcPalette->assign(mPalette);
    return true;
}

QWIN_END_NAMESPACE

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
