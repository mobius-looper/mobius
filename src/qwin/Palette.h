/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Useful classes that build upon the Qwin base classes but are not
 * fundamental components.
 *
 */

#ifndef QPALETTE_H
#define QPALETTE_H

// for SimpleDialog
#include "Qwinext.h"

QWIN_BEGIN_NAMESPACE

/****************************************************************************
 *                                                                          *
 *                                  PALETTE                                 *
 *                                                                          *
 ****************************************************************************/

class PaletteColor {

    friend class Palette;

  public:

    PaletteColor(const char* name, int rgb);
    ~PaletteColor();

    void setNext(PaletteColor* c);
	void setDisplayName(const char* name);
	void setKey(int key);
	int getKey();
    PaletteColor* getNext();
    const char* getName();
	const char* getDisplayName();

    Color* getColor();
	void setColor(Color* c);
	void localize(MessageCatalog* cat);

  protected:

    PaletteColor* mNext;
	int mKey;
    char* mName;
    char* mDisplayName;
    Color* mColor;

};

/**
 * Need to figure out a good way to register palettes,
 * I'd rather not hang one on the Window, we'd have to keep them
 * in sync over several windows and it isn't in the Swing model.
 * Making them a pure application thing means we have to pass them
 * through several levels of constructor.
 *
 * For now, allow a singleton global Palette to be registered.
 * Note that since the Color objects in here will be directly
 * directly referenced by many Components, you can't just delete them.
 * Either need to intern colors or come up with a versioning mechanism.
 */
class Palette {

  public:

	static const char* ELEMENT;

    Palette();
    Palette(XmlElement* e);
    ~Palette();
	void localize(MessageCatalog* cat);

	Palette* clone();
    PaletteColor* getColors();
    void add(PaletteColor* pc);
    void add(const char* name, int rgb);
    void remove(PaletteColor* pc);
    void assign(Palette* p);

	PaletteColor* getPaletteColor(const char* name);
	Color* getColor(const char* name, Color* dflt);
	Color* getColor(const char* name);

    void parseXml(XmlElement* e);
    void toXml(XmlBuffer* b);

  private:

    PaletteColor* mColors;

};

/**
 * The global singleton Palette.
 */
extern Palette* GlobalPalette;

class PaletteDialog : public SimpleDialog {

  public:
    
    PaletteDialog(Window* parent, Palette* p);
    ~PaletteDialog();
	void localize(const char* title, const char* colorTitle);

    bool commit();
	const char* getColorTitle();

  private:

    Palette* mSrcPalette;
    Palette* mPalette;
	char* mColorTitle;

};

QWIN_END_NAMESPACE
/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
