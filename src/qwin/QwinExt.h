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

#ifndef QWINLIB_H
#define QWINLIB_H

#include <stdio.h>

// can't reference these with "class" or else they'll
// get stuck in the Qwin namespace
#include "XmlModel.h"
#include "XmlBuffer.h"
#include "MessageCatalog.h"

#include "Qwin.h"

QWIN_BEGIN_NAMESPACE

/****************************************************************************
 *                                                                          *
 *                                LABELED TEXT                              *
 *                                                                          *
 ****************************************************************************/

class LabeledText : public Panel, public ActionListener {

	public:

	LabeledText();
	LabeledText(const char *label, const char *value);
	~LabeledText();

	void setLabel(const char *label);
	void setText(const char *text);
	void setColumns(int i);
	const char *getText();
	void actionPerformed(void* o);

  private:
	
	void init(const char *label, const char *value);
	Label* mLabel;
	Text* mText;
};

/****************************************************************************
 *                                                                          *
 *                               SIMPLE DIALOG                              *
 *                                                                          *
 ****************************************************************************/

class SimpleDialog : public Dialog, public ActionListener {

  public:
	
	SimpleDialog();
	SimpleDialog(Window* parent);
	SimpleDialog(Window* parent, const char *title);
	SimpleDialog(Window* parent, const char *title, bool cancel);
 	~SimpleDialog();

	static void localizeButtons(const char* ok, const char* cancel, const char* help);
	static void freeLocalizations();

    void addButton(Button* b);
    void addHelpButton();
    Panel* getPanel();
    bool isCommitted();
    bool isCanceled();
    void actionPerformed(void* o);
	void simulateOk();

    // overloadable methods
	virtual const char* getOkName();
	virtual const char* getCancelName();
    virtual bool commit();
    virtual void help();

  protected:

    void prepareToShow();
    void closing();

  private:

	static char* OkButtonText;
	static char* CancelButtonText;
	static char* HelpButtonText;

	void initSimpleDialog(bool cancel);
    void setupButtons();

    Panel* mPanel;
    Panel* mButtons;
    Button* mOk;
    Button* mCancel;
    Button* mHelp;
    bool mCommitted;
    bool mCanceled;
};

/****************************************************************************
 *                                                                          *
 *   							 NUMBER FIELD                               *
 *                                                                          *
 ****************************************************************************/ 

class NumberField : public Panel, public ActionListener 
{
  public:

	NumberField();
	NumberField(int low, int high);
	~NumberField();

	void setNullValue(int i);
	void setHideNull(bool b);
	void setLow(int i);
	void setHigh(int i);
	void addException(int i);
	void setDisplayOffset(int i);

	void setValue(int i);
	int getValue();

	void actionPerformed(void* o);

  private:

	void init();

	Text* mText;
	int mValue;
	int mLow;
	int mHigh;
	int mNullValue;
	bool mHideNull;

};

/****************************************************************************
 *                                                                          *
 *   							  FORM PANEL                                *
 *                                                                          *
 ****************************************************************************/

class FormPanel : public Panel
{
  public:

	FormPanel();
	~FormPanel();

	void setAlign(int i);
	void setVerticalGap(int i);
	void setHorizontalGap(int i);

	void add(const char *label, Component* c);

	Text* addText(ActionListener* l, const char *label);
	NumberField* addNumber(ActionListener* l, const char *label,
						   int low, int high);
	ComboBox* addCombo(ActionListener* l, const char *label,
					   const char **labels);
	ComboBox* addCombo(ActionListener* l, const char *label,
					   const char **labels, int columns);
	Checkbox* addCheckbox(ActionListener* l, const char *label);
	
  private:

	Panel* mInner;

};


/****************************************************************************
 *                                                                          *
 *                                  DRAGABLE                                *
 *                                                                          *
 ****************************************************************************/

class Dragable
{
  public:

    Dragable();
    virtual ~Dragable();

    virtual void trackMouse(int x, int y) = 0;
    virtual void finish() = 0;

    void getBounds(Bounds* b);

  protected:

    void paintRect(Graphics* g);

    Component* mParent;
    int mLeft;
    int mTop;
    int mRight; 
    int mBottom;
};

class DragRegion : public Dragable {

  public:

    DragRegion(Component* parent, int x, int y);
    void trackMouse(int x, int y) ;
    void finish();

  private:

};

class DragBox : public Dragable {

  public:

    DragBox(Component* parent, int mouseX, int mouseY, 
            int left, int top, int right, int bottom);

    void trackMouse(int x, int y);
    void finish();

  private:

    int mAnchorX;
    int mAnchorY;

};

class DragComponent : public Dragable {

  public:

    DragComponent(Component* parent, int mouseX, int mouseY, Component* c);

    void trackMouse(int mouseX, int mouseY);
    void finish();

  private:

    Component* mComponent;
    int mAnchorX;
    int mAnchorY;

};

/****************************************************************************
 *                                                                          *
 *                                MULTI SELECT                              *
 *                                                                          *
 ****************************************************************************/

class MultiSelect : public Panel, public ActionListener
{
  public:

	MultiSelect();
	MultiSelect(bool moveAll);
	~MultiSelect();

    StringList* getValues();

	void setMoveAll(bool b);
    void setAllowedValues(StringList* values);
    void setValues(StringList* values);
    void setRows(int i);
    void setColumns(int i);

    void actionPerformed(void* src);
	
  private:

    void init(bool moveAll);
    void prepare();
    void updateAvailableValues();

	bool mMoveAll;
	int mRows;
	int mColumns;
    StringList* mAllowedValues;
    StringList* mValues;

    ListBox* mAvailableBox;
    ListBox* mValuesBox;
    Button* mMoveLeft;
    Button* mMoveAllLeft;
    Button* mMoveRight;
    Button* mMoveAllRight;


};

/****************************************************************************
 *                                                                          *
 *   								SLIDER                                  *
 *                                                                          *
 ****************************************************************************/

class Slider : public Panel, public ActionListener {

	public:

	Slider(bool vertical, bool showValue);
	~Slider();

	void setValue(int i);
	int getValue();

	void setMinimum(int i);
	int getMinimum();

	void setMaximum(int i);
	int getMaximum();

	void setSliderLength(int i);
	void setLabelColumns(int i);

	void actionPerformed(void* o);

  private:

	void updateLabel();
	
	ScrollBar* mScroll;
	Label* mLabel;

};

/****************************************************************************
 *                                                                          *
 *   							   DIVIDER                                  *
 *                                                                          *
 ****************************************************************************/

class Divider : public Static {

  public:

	Divider();
	Divider(int width);
	Divider(int width, int height);
	~Divider();

	Dimension* getPreferredSize(Window* w);

  private:

	void initDivider();

};

/****************************************************************************
 *                                                                          *
 *                                   CUSTOM                                 *
 *                                                                          *
 ****************************************************************************/

class CustomExample : public Panel, 
                      public MouseInputAdapter, 
                      public KeyListener 
{
  public:
    
	CustomExample();
	~CustomExample();
	void nextLevel();
    
	Dimension* getPreferredSize(Window* w);
	virtual void paint(Graphics* g);

    void keyPressed(KeyEvent* e);
	void keyReleased(KeyEvent* e);
	void keyTyped(KeyEvent* e);
                    
    void mousePressed(MouseEvent* e);
    void mouseReleased(MouseEvent* e);
    void mouseMoved(MouseEvent* e);

  private:

	int mLevel;
};

/**
 * Originally this was a Button subclass, but it has to be a Panel
 * on Mac to get the necessary mouse click events.
 * Unfortunately this means we have to duplicate a bunch of Button properties,
 * but this is a better basis for a mouse-sensntive customs.  
 */
class CustomButton : public Panel, public MouseInputAdapter {

  public:
	
	CustomButton();
	CustomButton(const char *text);
	~CustomButton();

	void setText(const char *s);
    const char* getText();
	void setFont(Font* f);
    Font* getFont();
    void setTextColor(Color* c);
    Color* getTextColor();

	void setVerticalPad(int i);
	void setHorizontalPad(int i);
	void setArcWidth(int i);

	void setMomentary(bool b);
    bool isMomentary();
	void setToggle(bool b);
    bool isToggle();

    void setPushed(bool b);
	bool isPushed();

	void click();

	Dimension* getPreferredSize(Window* w);
    void paint(Graphics* g);
    void mousePressed(MouseEvent* e);
    void mouseReleased(MouseEvent* e);
    void mouseClicked(MouseEvent* e);

  private:

	void initCustomButton();
	
	char* mText;
	Font* mFont;
    Color* mTextColor;

	bool mMomentary;
    bool mToggle;
	bool mPushed;

	int mVerticalPad;
	int mHorizontalPad;
	int mArcWidth;

};

QWIN_END_NAMESPACE
/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
