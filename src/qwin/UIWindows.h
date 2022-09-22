/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Implementations of ComponentUI for the Windows platform.
 *
 * C++ multiple inheritance just sucks so bad it's hard to know where to begin.
 * 
 * I tried to accomplsih something similar to Java interfaces
 * with an inherited base class that provided default implementations
 * for most of the things in ComponentUI.  But under MSVC 6.0 this
 * doesn't work because the inherited methods don't "satisfy" the
 * pure virtual methods, and making WindowsComponent subclass
 * ComponentUI results in ambiguous inheritance.  
 *
 * Next I tried defining every damn ComponentUI method in each subclass and
 * redirected them to a common implementation with a different name
 * in WindowsComponent.  That works but is ugly.
 *
 * But this still resulted in creepy vtable confusion when casting
 * between ComponentUI and WindowsWindowUI.  The pointers were the same
 * in the debugger, but the contents came out completely different.  It is
 * possibly an MSVC 6 bug, try it the right way on Mac.
 *
 * Next I tried punting and writing a single base class for Windows UIs
 * that implements all of the abstract classes and stubs out the methods.
 * But since several of the "interfaces" have methods with the same name,
 * you have ambiguous methods.  
 * 
 * After sticking soldering irons in my urethra for a few days, I gave
 * up and defined a collection of proxy classes that implement the
 * abstract interfaces, and just forward the methods to handler classes
 * That have their own hierarchy.  
 *
 * If you can find a way to do this with multiple inheritance let me know
 * and you will win a fabulous prize.  Oh, and don't tell me to read
 * page 345-9871 subparagraph 45 of the fucking C++ manual, I've been there.
 * If you can come up with WORKING code, then show me.    And don't tell
 * me to use templates, just don't go there.
 */

#ifndef UI_WINDOWS_H
#define UI_WINDOWS_H

#include <windows.h>

#include "UIManager.h"

/****************************************************************************
 *                                                                          *
 *   								COLOR                                   *
 *                                                                          *
 ****************************************************************************/

#define MAX_PEN_WIDTH 4

class WindowsColor : public NativeColor {

  public:

	WindowsColor(Color* c);
	~WindowsColor();

	void setRgb(int rgb);
    HBRUSH getBrush();
    HPEN getPen();
    HPEN getPen(int width);

  private:

	Color* mColor;
	HBRUSH mBrush;
	HPEN mPens[MAX_PEN_WIDTH];
};

/****************************************************************************
 *                                                                          *
 *   								 FONT                                   *
 *                                                                          *
 ****************************************************************************/

class WindowsFont : public NativeFont {

  public:

	WindowsFont(Font* f);
	~WindowsFont();

	int getAscent();
	int getHeight();

    // is this used?
   	HFONT getHandle();

    // used by WindowsGraphics
	HFONT getHandle(HDC dc);

  private:

	Font* mFont;
	HFONT mHandle;
    TEXTMETRIC mTextMetric;

};

/****************************************************************************
 *                                                                          *
 *                                   TIMER                                  *
 *                                                                          *
 ****************************************************************************/

class WindowsTimer : public NativeTimer {

  public:

	WindowsTimer(SimpleTimer* t);
	~WindowsTimer();

    int getId();

    static SimpleTimer* getTimer(int id);

  private:

	SimpleTimer* mTimer;
    int mId;
};

/****************************************************************************
 *                                                                          *
 *   							 TEXT METRICS                               *
 *                                                                          *
 ****************************************************************************/

class WindowsTextMetrics : public TextMetrics {

  public:
	
	WindowsTextMetrics();
	void init(HDC dc);

	// TextMetrics interface methods

	int getHeight();
	int getMaxWidth();
	int getAverageWidth();
	int getAscent();
	int getExternalLeading();

  private:

    TEXTMETRIC mHandle;

};

/****************************************************************************
 *                                                                          *
 *   							   GRAPHICS                                 *
 *                                                                          *
 ****************************************************************************/

class WindowsGraphics : public Graphics {

    friend class WindowsWindow;

  public:

	WindowsGraphics();
	WindowsGraphics(HDC dc);
	~WindowsGraphics();

    Color* getColor();

	void save();
	void restore();
	void setColor(Color* c);
	void setBrush(Color* c);
	void setPen(Color* c);
	void setFont(Font* f);
	void setBackgroundColor(Color* c);
    void setXORMode(Color* c);
    void setXORMode();

	void drawString(const char* str, int x, int y);
	void drawLine(int x1, int y1, int x2, int y2);
    void drawRect(int x, int y, int width, int height);
    void drawRoundRect(int x, int y, int width, int height,
                       int arcWidth, int arcHeight);
    void drawOval(int x, int y, int width, int height);

    void fillRect(int x, int y, int width, int height);
    void fillRoundRect(int x, int y, int width, int height,
                       int arcWidth, int arcHeight);
    void fillOval(int x, int y, int width, int height);

    void fillArc(int x, int y, int width, int height,
                 int startAngle, int arcAngle);

	// extensions

    TextMetrics* getTextMetrics();
	void getTextSize(const char *text, Dimension* d);
	void getTextSize(const char *text, Font* font, Dimension* d);

    // used by Button and ListBox for "ownerdraw" 
    LPDRAWITEMSTRUCT getDrawItem();

  protected:

    void setDeviceContext(HDC dc);
    void setDrawItem(LPDRAWITEMSTRUCT di);

  private:

	void init();
    void startHollowShape();
    void endHollowShape();
    void getRadial(int centerx, int centery, int radius, int angle,
                   int* radialx, int* radialy);


	HDC mHandle;
	HFONT mDefaultFont;
    HBRUSH mHollowBrush;
    HBRUSH mSaveBrush;
	WindowsTextMetrics mTextMetrics;

	// when created in response to a WM_DRAWITEM message
	LPDRAWITEMSTRUCT mDrawItem;

    Color* mColor;
    Color* mBackground;
    Font* mFont;
};

/****************************************************************************
 *                                                                          *
 *   							SYSTEM DIALOGS                              *
 *                                                                          *
 ****************************************************************************/

class WindowsOpenDialog : public SystemDialogUI {

  public:

	WindowsOpenDialog(OpenDialog* od);
	~WindowsOpenDialog();

	void show();

  private:
    
    void getWindowsFilter(const char* src, char* dest);
    void getExtension(const char* filter, int index, char* extension);

    OpenDialog* mDialog;
};

class WindowsColorDialog : public SystemDialogUI {

  public:

	WindowsColorDialog(ColorDialog* cd);
	~WindowsColorDialog();

	void show();

  private:
    
    ColorDialog* mDialog;

};

class WindowsMessageDialog : public SystemDialogUI {

  public:

	WindowsMessageDialog(MessageDialog* cd);
	~WindowsMessageDialog();

	void show();

  private:
    
    MessageDialog* mDialog;
};

/****************************************************************************
 *                                                                          *
 *                            WINDOWS COMPONENT UI                          *
 *                                                                          *
 ****************************************************************************/

/**
 * The base class for all Windows implementations of ComponentUI.
 * Unfortunately have to get to these via proxy due to odd 
 * MSVC/C++ behavior with multiple inheritance.
 */
class WindowsComponent : public NativeComponent {

  public:

	WindowsComponent();
    virtual ~WindowsComponent();
    void subclassWindowProc();

    static HWND getHandle(Component* c);
    static HWND getWindowHandle(Component* c);
	
	// this is defined by the NativeComponent interface so it must 
	// return void*
    void* getHandle();

    virtual Component* getComponent() = 0;
    HWND getParentHandle();
	int getWindowStyle();
    WindowsContext* getWindowsContext(Component* c);
    void detach();
	virtual void updateNativeBounds(Bounds* b);
    bool isOpen();

	virtual void command(int code);
	virtual void notify(int code);

    virtual long messageHandler(UINT msg, WPARAM wparam, LPARAM lparam);

    // default implementations for ComponentUI methods

    virtual void paint(Graphics* g);
	virtual Color* colorHook(Graphics* g);
    virtual void invalidate(Component* c);
    virtual void updateBounds();
    virtual void close();
    virtual void invalidateHandle();
    virtual void setEnabled(bool b);
    virtual bool isEnabled();
    virtual void setVisible(bool b);
    virtual bool isVisible();
    virtual void setFocus();
	virtual void debug();

  protected:

    HWND mHandle;
    WNDPROC mWindowProc;

};

/****************************************************************************
 *                                                                          *
 *   								STATIC                                  *
 *                                                                          *
 ****************************************************************************/

class WindowsStatic : public WindowsComponent {

  public:
	
	WindowsStatic(Static* s);
	~WindowsStatic();

    Component* getComponent() {
        return mStatic;
    }

	void setText(const char* s);
	void setBitmap(const char* s);
	void setIcon(const char* s);
	void getPreferredSize(Window* w, Dimension* d);

	void open();
	Color* colorHook(Graphics* g);

  private:

	Static* mStatic;
    bool mAutoColor;

};

class WindowsStaticUI : public StaticUI {

  public:

	WindowsStaticUI(WindowsStatic* s) {
        mNative = s;
    }
	~WindowsStaticUI() {
        delete mNative;
    }
    NativeComponent* getNative() {
        return mNative;
    }
	void setText(const char* s) {
        mNative->setText(s);
    }
	void setBitmap(const char* s) {
        mNative->setBitmap(s);
    }
	void setIcon(const char* s) {
        mNative->setIcon(s);
    }
	void open() {
        mNative->open();
    }
	void postOpen() {
    }
	void getPreferredSize(Window* w, Dimension* d) {
		mNative->getPreferredSize(w, d);
	}
	bool isNativeParent() {
		// on Windows statics are always parents
		return true;
	}
	bool isOpen() {
		return mNative->isOpen();
	}
	void command(int code) {
        mNative->command(code);
    }
	Color* colorHook(Graphics* g) {
        return mNative->colorHook(g);
    }
    void invalidate(Component* c) {
        mNative->invalidate(c);
    }
	void paint(Graphics* g) {
        mNative->paint(g);
    }
    void close() {
        mNative->close();
    }
    void invalidateHandle() {
        mNative->invalidateHandle();
    }
    void updateBounds() {
        mNative->updateBounds();
    }
    void setEnabled(bool b) {
        mNative->setEnabled(b);
    }
    bool isEnabled() {
        return mNative->isEnabled();
    }
    void setVisible(bool b) {
        mNative->setVisible(b);
    }
    bool isVisible() {
        return mNative->isVisible();
    }
    void setFocus() {
        mNative->setFocus();
    }
    void debug() {
        mNative->debug();
    }

  private:

	class WindowsStatic* mNative;

};

/****************************************************************************
 *                                                                          *
 *                                   PANEL                                  *
 *                                                                          *
 ****************************************************************************/

class WindowsPanel : public WindowsComponent {

  public:
	
	WindowsPanel(Panel* p);
	~WindowsPanel();

    Component* getComponent() {
        return mPanel;
    }

    bool isNativeParent();
	void open();
    void postOpen();
	Color* colorHook(Graphics* g);

  private:

	Panel* mPanel;

};

class WindowsPanelUI : public PanelUI {

  public:

	WindowsPanelUI(WindowsPanel* p) {
        mNative = p;
    }
	~WindowsPanelUI() {
        delete mNative;
    }
    NativeComponent* getNative() {
        return mNative;
    }
	void open() {
        mNative->open();
    }
	void postOpen() {
    }
	void getPreferredSize(Window* w, Dimension* d) {
        // defined by children
	}
	bool isNativeParent() {
		return mNative->isNativeParent();
	}
	bool isOpen() {
		return mNative->isOpen();
	}
	void command(int code) {
        mNative->command(code);
    }
	Color* colorHook(Graphics* g) {
        return mNative->colorHook(g);
    }
    void invalidate(Component* c) {
        mNative->invalidate(c);
    }
	void paint(Graphics* g) {
        mNative->paint(g);
    }
    void close() {
        mNative->close();
    }
    void invalidateHandle() {
        mNative->invalidateHandle();
    }
    void updateBounds() {
        mNative->updateBounds();
    }
    void setEnabled(bool b) {
        mNative->setEnabled(b);
    }
    bool isEnabled() {
        return mNative->isEnabled();
    }
    void setVisible(bool b) {
        mNative->setVisible(b);
    }
    bool isVisible() {
        return mNative->isVisible();
    }
    void setFocus() {
        mNative->setFocus();
    }
    void debug() {
        mNative->debug();
    }

  private:

	class WindowsPanel* mNative;

};

/****************************************************************************
 *                                                                          *
 *   								BUTTON                                  *
 *                                                                          *
 ****************************************************************************/

class WindowsButton : public WindowsComponent {

  public:

	WindowsButton(Button * b);
	~WindowsButton();

    Component* getComponent() {
        return mButton;
    }

    void setText(const char* text);
	void click();
	void getPreferredSize(Window* w, Dimension* d);
	void open();

	Color* colorHook(Graphics* g);
	void command(int code);
	void updateBounds();

	void paint(Graphics* g);

  private:

	Button* mButton;

};

class WindowsButtonUI : public ButtonUI {

  public:

	WindowsButtonUI(WindowsButton* p) {
        mNative = p;
    }
	~WindowsButtonUI() {
        delete mNative;
    }
    NativeComponent* getNative() {
        return mNative;
    }
	void open() {
        mNative->open();
    }
	void postOpen() {
    }
	bool isNativeParent() {
		return false;
	}
	bool isOpen() {
		return mNative->isOpen();
	}
	void command(int code) {
        mNative->command(code);
    }
	Color* colorHook(Graphics* g) {
        return mNative->colorHook(g);
    }
    void invalidate(Component* c) {
        mNative->invalidate(c);
    }
	void paint(Graphics* g) {
        mNative->paint(g);
    }
    void close() {
        mNative->close();
    }
    void invalidateHandle() {
        mNative->invalidateHandle();
    }
	void getPreferredSize(Window* w, Dimension* d) {
		mNative->getPreferredSize(w, d);
	}
    void updateBounds() {
        mNative->updateBounds();
    }
    void setEnabled(bool b) {
        mNative->setEnabled(b);
    }
    bool isEnabled() {
        return mNative->isEnabled();
    }
    void setVisible(bool b) {
        mNative->setVisible(b);
    }
    bool isVisible() {
        return mNative->isVisible();
    }
    void setFocus() {
        mNative->setFocus();
    }
    void debug() {
        mNative->debug();
    }

    // ButtonUI

    void setText(const char* text) {
        mNative->setText(text);
    }
	void click() {
        mNative->click();
    }

  private:

	class WindowsButton* mNative;

};

/****************************************************************************
 *                                                                          *
 *   							 RADIO BUTTON                               *
 *                                                                          *
 ****************************************************************************/

class WindowsRadioButton : public WindowsComponent {

  public:

	WindowsRadioButton();
	WindowsRadioButton(RadioButton *b);
	~WindowsRadioButton();

    Component* getComponent() {
        return mButton;
    }

	void setSelected(bool b);
	bool isSelected();

	void open();
	void getPreferredSize(Window* w, Dimension* d);
	void command(int code);

  private:

	RadioButton* mButton;

};

class WindowsRadioButtonUI : public RadioButtonUI {

  public:

	WindowsRadioButtonUI(WindowsRadioButton* b) {
        mNative = b;
    }
	~WindowsRadioButtonUI() {
        delete mNative;
    }
    NativeComponent* getNative() {
        return mNative;
    }
	void open() {
        mNative->open();
    }
	void postOpen() {
    }
	void getPreferredSize(Window* w, Dimension* d) {
		mNative->getPreferredSize(w, d);
	}
	bool isNativeParent() {
		return false;
	}
	bool isOpen() {
		return mNative->isOpen();
	}
	void command(int code) {
        mNative->command(code);
    }
	Color* colorHook(Graphics* g) {
        return mNative->colorHook(g);
    }
    void invalidate(Component* c) {
        mNative->invalidate(c);
    }
	void paint(Graphics* g) {
        mNative->paint(g);
    }
    void close() {
        mNative->close();
    }
    void invalidateHandle() {
        mNative->invalidateHandle();
    }
    void updateBounds() {
        mNative->updateBounds();
    }
    void setEnabled(bool b) {
        mNative->setEnabled(b);
    }
    bool isEnabled() {
        return mNative->isEnabled();
    }
    void setVisible(bool b) {
        mNative->setVisible(b);
    }
    bool isVisible() {
        return mNative->isVisible();
    }
    void setFocus() {
        mNative->setFocus();
    }
    void debug() {
        mNative->debug();
    }

    // RadioButtonUI

    void setSelected(bool b) {
        mNative->setSelected(b);
    }
    bool isSelected() {
        return mNative->isSelected();
    }

    // ButtonUI

    void setText(const char* text) {
    }
	void click() {
    }

  private:

	class WindowsRadioButton* mNative;

};

//////////////////////////////////////////////////////////////////////
//
// Radios
//
//////////////////////////////////////////////////////////////////////

class WindowsRadios : public WindowsComponent {

  public:

	WindowsRadios();
	WindowsRadios(Radios *r);
	~WindowsRadios();

    Component* getComponent() {
        return mRadios;
    }

	void open();
	void changeSelection(RadioButton* b);

  private:

	Radios* mRadios;

};

class WindowsRadiosUI : public RadiosUI {

  public:

	WindowsRadiosUI(WindowsRadios* b) {
        mNative = b;
    }
	~WindowsRadiosUI() {
        delete mNative;
    }
    NativeComponent* getNative() {
        return mNative;
    }
	void open() {
    }
	void postOpen() {
    }
	void getPreferredSize(Window* w, Dimension* d) {
        // defined by children
	}
	bool isNativeParent() {
		return false;
	}
	bool isOpen() {
		return true;
	}
	void command(int code) {
    }
	Color* colorHook(Graphics* g) {
        return NULL;
    }
    void invalidate(Component* c) {
    }
	void paint(Graphics* g) {
    }
    void close() {
    }
    void invalidateHandle() {
    }
    void updateBounds() {
    }
    void setEnabled(bool b) {
    }
    bool isEnabled() {
        return false;
    }
    void setVisible(bool b) {
    }
    bool isVisible() {
        return false;
    }
    void setFocus() {
    }
    void debug() {
        mNative->debug();
    }

    // RadiosUI

    void changeSelection(RadioButton* b) {
        mNative->changeSelection(b);
    }

  private:

	class WindowsRadios* mNative;

};

/****************************************************************************
 *                                                                          *
 *   							   CHECKBOX                                 *
 *                                                                          *
 ****************************************************************************/

class WindowsCheckbox : public WindowsComponent {

  public:

	WindowsCheckbox(Checkbox *cb);
	~WindowsCheckbox();

    Component* getComponent() {
        return mCheckbox;
    }

	void setSelected(bool b);
	bool isSelected();
    void getPreferredSize(Window* w, Dimension* d);
	void open();
	void command(int code);

  private:

	Checkbox* mCheckbox;

};

class WindowsCheckboxUI : public CheckboxUI {
  public:

	WindowsCheckboxUI(WindowsCheckbox* b) {
        mNative = b;
    }
	~WindowsCheckboxUI() {
        delete mNative;
    }
    NativeComponent* getNative() {
        return mNative;
    }
	void open() {
        mNative->open();
    }
	void postOpen() {
    }
	void getPreferredSize(Window* w, Dimension* d) {
		mNative->getPreferredSize(w, d);
	}
	bool isNativeParent() {
		return false;
	}
	bool isOpen() {
		return mNative->isOpen();
	}
	void command(int code) {
        mNative->command(code);
    }
	Color* colorHook(Graphics* g) {
        return mNative->colorHook(g);
    }
    void invalidate(Component* c) {
        mNative->invalidate(c);
    }
	void paint(Graphics* g) {
        mNative->paint(g);
    }
    void close() {
        mNative->close();
    }
    void invalidateHandle() {
        mNative->invalidateHandle();
    }
    void updateBounds() {
        mNative->updateBounds();
    }
    void setEnabled(bool b) {
        mNative->setEnabled(b);
    }
    bool isEnabled() {
        return mNative->isEnabled();
    }
    void setVisible(bool b) {
        mNative->setVisible(b);
    }
    bool isVisible() {
        return mNative->isVisible();
    }
    void setFocus() {
        mNative->setFocus();
    }
    void debug() {
        mNative->debug();
    }

    // RadioButtonUI

    void setSelected(bool b) {
        mNative->setSelected(b);
    }
    bool isSelected() {
        return mNative->isSelected();
    }

    // ButtonUI

    void setText(const char* text) {
    }
	void click() {
    }

  private:

	class WindowsCheckbox* mNative;

};

/****************************************************************************
 *                                                                          *
 *   							  COMBO BOX                                 *
 *                                                                          *
 ****************************************************************************/

class WindowsComboBox : public WindowsComponent {

  public:

	WindowsComboBox(ComboBox* cb);
	~WindowsComboBox();

    Component* getComponent() {
        return mComboBox;
    }

	void setValues(StringList* values);
	void addValue(const char* value);
	void setSelectedIndex(int i);
	void setSelectedValue(const char* value);
	int getSelectedIndex();
	char* getSelectedValue();

    void open();
	void getPreferredSize(Window* w, Dimension* d);
    void command(int code);

    // we have an unusal bounds updater
    void updateBounds();

  private:

    int getFullHeight();

	ComboBox* mComboBox;

};

class WindowsComboBoxUI : public ComboBoxUI {
  public:

	WindowsComboBoxUI(WindowsComboBox* b) {
        mNative = b;
    }
	~WindowsComboBoxUI() {
        delete mNative;
    }
    NativeComponent* getNative() {
        return mNative;
    }
	void open() {
        mNative->open();
    }
	void postOpen() {
    }
	void getPreferredSize(Window* w, Dimension* d) {
		mNative->getPreferredSize(w, d);
	}
	bool isNativeParent() {
		return false;
	}
	bool isOpen() {
		return mNative->isOpen();
	}
	void command(int code) {
        mNative->command(code);
    }
	Color* colorHook(Graphics* g) {
        return mNative->colorHook(g);
    }
    void invalidate(Component* c) {
        mNative->invalidate(c);
    }
	void paint(Graphics* g) {
        mNative->paint(g);
    }
    void close() {
        mNative->close();
    }
    void invalidateHandle() {
        mNative->invalidateHandle();
    }
    void updateBounds() {
        mNative->updateBounds();
    }
    void setEnabled(bool b) {
        mNative->setEnabled(b);
    }
    bool isEnabled() {
        return mNative->isEnabled();
    }
    void setVisible(bool b) {
        mNative->setVisible(b);
    }
    bool isVisible() {
        return mNative->isVisible();
    }
    void setFocus() {
        mNative->setFocus();
    }
    void debug() {
        mNative->debug();
    }

    // ComboBoxUI

    void setValues(StringList* values) {
        mNative->setValues(values);
    }
    void addValue(const char* value) {
        mNative->addValue(value);
    }
    void setSelectedIndex(int i) {
        mNative->setSelectedIndex(i);
    }
    void setSelectedValue(const char* value) {
        mNative->setSelectedValue(value);
    }
    int getSelectedIndex() {
        return mNative->getSelectedIndex();
    }
    char* getSelectedValue() {
        return mNative->getSelectedValue();
    }

  private:

	class WindowsComboBox* mNative;

};

/****************************************************************************
 *                                                                          *
 *   							   LIST BOX                                 *
 *                                                                          *
 ****************************************************************************/

class WindowsListBox : public WindowsComponent {

  public:

	WindowsListBox(ListBox* cb);
	~WindowsListBox();

    Component* getComponent() {
        return mListBox;
    }

	void setValues(StringList* values);
	void addValue(const char* value);
	void setAnnotations(StringList* values);

	void setSelectedIndex(int i);
	int getSelectedIndex();
	bool isSelected(int i);

	void open();
    void getPreferredSize(Window* w, Dimension* d);
	void command(int code);

    // ownerdraw support
	void paint(Graphics* g);

  private:

	ListBox* mListBox;

};

class WindowsListBoxUI : public ListBoxUI {
  public:

	WindowsListBoxUI(WindowsListBox* b) {
        mNative = b;
    }
	~WindowsListBoxUI() {
        delete mNative;
    }
    NativeComponent* getNative() {
        return mNative;
    }
	void open() {
        mNative->open();
    }
	void postOpen() {
    }
	void getPreferredSize(Window* w, Dimension* d) {
        mNative->getPreferredSize(w, d);
	}
	bool isNativeParent() {
		return false;
	}
	bool isOpen() {
		return mNative->isOpen();
	}
	void command(int code) {
        mNative->command(code);
    }
	Color* colorHook(Graphics* g) {
        return mNative->colorHook(g);
    }
    void invalidate(Component* c) {
        mNative->invalidate(c);
    }
	void paint(Graphics* g) {
        mNative->paint(g);
    }
    void close() {
        mNative->close();
    }
    void invalidateHandle() {
        mNative->invalidateHandle();
    }
    void updateBounds() {
        mNative->updateBounds();
    }
    void setEnabled(bool b) {
        mNative->setEnabled(b);
    }
    bool isEnabled() {
        return mNative->isEnabled();
    }
    void setVisible(bool b) {
        mNative->setVisible(b);
    }
    bool isVisible() {
        return mNative->isVisible();
    }
    void setFocus() {
        mNative->setFocus();
    }
    void debug() {
        mNative->debug();
    }

    // ListBoxUI

    void setValues(StringList* values) {
        mNative->setValues(values);
    }
    void addValue(const char* value) {
        mNative->addValue(value);
    }
    void setAnnotations(StringList* values) {
        mNative->setAnnotations(values);
    }
    void setSelectedIndex(int i) {
        mNative->setSelectedIndex(i);
    }
	int getSelectedIndex() {
        return mNative->getSelectedIndex();
    }
    bool isSelected(int i) {
        return mNative->isSelected(i);
    }

  private:

	class WindowsListBox* mNative;

};

/****************************************************************************
 *                                                                          *
 *   							  GROUP BOX                                 *
 *                                                                          *
 ****************************************************************************/

class WindowsGroupBox : public WindowsComponent {

  public:

	WindowsGroupBox(GroupBox* gb);
	~WindowsGroupBox();

    Component* getComponent() {
        return mGroupBox;
    }

	void setText(const char* s);
	void open();

  private:

	GroupBox* mGroupBox;

};

class WindowsGroupBoxUI : public GroupBoxUI {
  public:

	WindowsGroupBoxUI(WindowsGroupBox* b) {
        mNative = b;
    }
	~WindowsGroupBoxUI() {
        delete mNative;
    }
    NativeComponent* getNative() {
        return mNative;
    }
	void open() {
        mNative->open();
    }
	void postOpen() {
    }
	void getPreferredSize(Window* w, Dimension* d) {
        // defined by GroupBox but should be in the UI
	}
	bool isNativeParent() {
		return false;
	}
	bool isOpen() {
		return mNative->isOpen();
	}
	void command(int code) {
        mNative->command(code);
    }
	Color* colorHook(Graphics* g) {
        return mNative->colorHook(g);
    }
    void invalidate(Component* c) {
        mNative->invalidate(c);
    }
	void paint(Graphics* g) {
        mNative->paint(g);
    }
    void close() {
        mNative->close();
    }
    void invalidateHandle() {
        mNative->invalidateHandle();
    }
    void updateBounds() {
        mNative->updateBounds();
    }
    void setEnabled(bool b) {
        mNative->setEnabled(b);
    }
    bool isEnabled() {
        return mNative->isEnabled();
    }
    void setVisible(bool b) {
        mNative->setVisible(b);
    }
    bool isVisible() {
        return mNative->isVisible();
    }
    void setFocus() {
        mNative->setFocus();
    }
    void debug() {
        mNative->debug();
    }

    // GroupBoxUI

    void setText(const char* s) {
        mNative->setText(s);
    }

  private:

	class WindowsGroupBox* mNative;

};

/****************************************************************************
 *                                                                          *
 *                                    TEXT                                  *
 *                                                                          *
 ****************************************************************************/

class WindowsText : public WindowsComponent {

  public:

    WindowsText(Text* t);
    ~WindowsText();

    Component* getComponent() {
        return mText;
    }

	void setEditable(bool b);
    void setText(const char* s);
    char* getText();

    virtual void open();
    virtual void getPreferredSize(Window* w, Dimension* d);

    void command(int code);
    long messageHandler(UINT msg, WPARAM wparam, LPARAM lparam);

  protected:

    Text* mText;

};

class WindowsTextUI : public TextUI {
  public:

	WindowsTextUI() {
    }
	WindowsTextUI(WindowsText* b) {
        mNative = b;
    }
	~WindowsTextUI() {
        delete mNative;
    }
    NativeComponent* getNative() {
        return mNative;
    }
	void open() {
        mNative->open();
    }
	void postOpen() {
    }
	void getPreferredSize(Window* w, Dimension* d) {
		mNative->getPreferredSize(w, d);
	}
	bool isNativeParent() {
		return false;
	}
	bool isOpen() {
		return mNative->isOpen();
	}
	void command(int code) {
        mNative->command(code);
    }
	Color* colorHook(Graphics* g) {
        return mNative->colorHook(g);
    }
    void invalidate(Component* c) {
        mNative->invalidate(c);
    }
	void paint(Graphics* g) {
        mNative->paint(g);
    }
    void close() {
        mNative->close();
    }
    void invalidateHandle() {
        mNative->invalidateHandle();
    }
    void updateBounds() {
        mNative->updateBounds();
    }
    void setEnabled(bool b) {
        mNative->setEnabled(b);
    }
    bool isEnabled() {
        return mNative->isEnabled();
    }
    void setVisible(bool b) {
        mNative->setVisible(b);
    }
    bool isVisible() {
        return mNative->isVisible();
    }
    void setFocus() {
        mNative->setFocus();
    }
    void debug() {
        mNative->debug();
    }

    // TextUI

    void setEditable(bool b) {
        mNative->setEditable(b);
    }
    void setText(const char* s) {
        mNative->setText(s);
    }
    char* getText() {
        return mNative->getText();
    }

  protected:

	class WindowsText* mNative;

};

/****************************************************************************
 *                                                                          *
 *                                 TEXT AREA                                *
 *                                                                          *
 ****************************************************************************/

class WindowsTextArea : public WindowsText 
{
  public:

    WindowsTextArea(TextArea* t);
    ~WindowsTextArea();

    void open();
	void getPreferredSize(Window* w, Dimension* d);
 
  private:

};

class WindowsTextAreaUI : public TextAreaUI {
  public:

	WindowsTextAreaUI() {
    }
	WindowsTextAreaUI(WindowsTextArea* ta) {
        mNative = ta;
    }
	~WindowsTextAreaUI() {
        delete mNative;
    }
    NativeComponent* getNative() {
        return mNative;
    }
	void open() {
        mNative->open();
    }
	void postOpen() {
    }
	void getPreferredSize(Window* w, Dimension* d) {
		mNative->getPreferredSize(w, d);
	}
	bool isNativeParent() {
		return false;
	}
	bool isOpen() {
		return mNative->isOpen();
	}
	void command(int code) {
        mNative->command(code);
    }
	Color* colorHook(Graphics* g) {
        return mNative->colorHook(g);
    }
    void invalidate(Component* c) {
        mNative->invalidate(c);
    }
	void paint(Graphics* g) {
        mNative->paint(g);
    }
    void close() {
        mNative->close();
    }
    void invalidateHandle() {
        mNative->invalidateHandle();
    }
    void updateBounds() {
        mNative->updateBounds();
    }
    void setEnabled(bool b) {
        mNative->setEnabled(b);
    }
    bool isEnabled() {
        return mNative->isEnabled();
    }
    void setVisible(bool b) {
        mNative->setVisible(b);
    }
    bool isVisible() {
        return mNative->isVisible();
    }
    void setFocus() {
        mNative->setFocus();
    }
    void debug() {
        mNative->debug();
    }

    // TextUI

    void setEditable(bool b) {
        mNative->setEditable(b);
    }
    void setText(const char* s) {
        mNative->setText(s);
    }
    char* getText() {
        return mNative->getText();
    }

  protected:

	class WindowsTextArea* mNative;

};


/****************************************************************************
 *                                                                          *
 *   							   TOOL BAR                                 *
 *                                                                          *
 ****************************************************************************/

class WindowsToolBar : public WindowsComponent {

  public:

	WindowsToolBar(ToolBar* tb);
	~WindowsToolBar();

    Component* getComponent() {
        return mToolBar;
    }

	void open();

   private:

	ToolBar* mToolBar;

};

class WindowsToolBarUI : public ToolBarUI {
  public:

	WindowsToolBarUI(WindowsToolBar* b) {
        mNative = b;
    }
	~WindowsToolBarUI() {
        delete mNative;
    }
    NativeComponent* getNative() {
        return mNative;
    }
	void open() {
        mNative->open();
    }
	void postOpen() {
    }
	void getPreferredSize(Window* w, Dimension* d) {
        // not implemented yet
	}
	bool isNativeParent() {
		return false;
	}
	bool isOpen() {
		return mNative->isOpen();
	}
	void command(int code) {
        mNative->command(code);
    }
	Color* colorHook(Graphics* g) {
        return mNative->colorHook(g);
    }
    void invalidate(Component* c) {
        mNative->invalidate(c);
    }
	void paint(Graphics* g) {
        mNative->paint(g);
    }
    void close() {
        mNative->close();
    }
    void invalidateHandle() {
        mNative->invalidateHandle();
    }
    void updateBounds() {
        mNative->updateBounds();
    }
    void setEnabled(bool b) {
        mNative->setEnabled(b);
    }
    bool isEnabled() {
        return mNative->isEnabled();
    }
    void setVisible(bool b) {
        mNative->setVisible(b);
    }
    bool isVisible() {
        return mNative->isVisible();
    }
    void setFocus() {
        mNative->setFocus();
    }
    void debug() {
        mNative->debug();
    }

  private:

	class WindowsToolBar* mNative;

};

/****************************************************************************
 *                                                                          *
 *   							  STATUS BAR                                *
 *                                                                          *
 ****************************************************************************/

class WindowsStatusBar : public WindowsComponent {

  public:

	WindowsStatusBar(StatusBar* sb);
	~WindowsStatusBar();

    Component* getComponent() {
        return mStatusBar;
    }

	void open();

  private:

	StatusBar* mStatusBar;

};

class WindowsStatusBarUI : public StatusBarUI {
  public:

	WindowsStatusBarUI(WindowsStatusBar* b) {
        mNative = b;
    }
	~WindowsStatusBarUI() {
        delete mNative;
    }
    NativeComponent* getNative() {
        return mNative;
    }
	void open() {
        mNative->open();
    }
	void postOpen() {
    }
	void getPreferredSize(Window* w, Dimension* d) {
        // not implemented yet
	}
	bool isNativeParent() {
		return false;
	}
	bool isOpen() {
		return mNative->isOpen();
	}
	void command(int code) {
        mNative->command(code);
    }
	Color* colorHook(Graphics* g) {
        return mNative->colorHook(g);
    }
    void invalidate(Component* c) {
        mNative->invalidate(c);
    }
	void paint(Graphics* g) {
        mNative->paint(g);
    }
    void close() {
        mNative->close();
    }
    void invalidateHandle() {
        mNative->invalidateHandle();
    }
    void updateBounds() {
        mNative->updateBounds();
    }
    void setEnabled(bool b) {
        mNative->setEnabled(b);
    }
    bool isEnabled() {
        return mNative->isEnabled();
    }
    void setVisible(bool b) {
        mNative->setVisible(b);
    }
    bool isVisible() {
        return mNative->isVisible();
    }
    void setFocus() {
        mNative->setFocus();
    }
    void debug() {
        mNative->debug();
    }

  private:

	class WindowsStatusBar* mNative;

};

/****************************************************************************
 *                                                                          *
 *   							 TABBED PANE                                *
 *                                                                          *
 ****************************************************************************/

class WindowsTabbedPane : public WindowsComponent {

  public:
	
	WindowsTabbedPane(TabbedPane* sb);
	~WindowsTabbedPane();

    Component* getComponent() {
        return mTabbedPane;
    }

	void setSelectedIndex(int i);
    int getSelectedIndex();

	void open();
	void getPreferredSize(Window* w, Dimension* d);
    void command(int code);
    void notify(int code);
	
   private:

    void forceHeavyLabels(Component* c);

	TabbedPane* mTabbedPane;

};

class WindowsTabbedPaneUI : public TabbedPaneUI {
  public:

	WindowsTabbedPaneUI(WindowsTabbedPane* b) {
        mNative = b;
    }
	~WindowsTabbedPaneUI() {
        delete mNative;
    }
    NativeComponent* getNative() {
        return mNative;
    }
	void open() {
        mNative->open();
    }
	void postOpen() {
    }
	void getPreferredSize(Window* w, Dimension* d) {
        mNative->getPreferredSize(w, d);
	}
	bool isNativeParent() {
        // This feels like it should be true but apparently not
        // we have historically just overlayed a child window on top.
        // I tried setting it true to debug the problem with lightweight
        // labels but that didn't work.
		return false;
	}
	bool isOpen() {
		return mNative->isOpen();
	}
	void command(int code) {
        mNative->command(code);
    }
	Color* colorHook(Graphics* g) {
        return mNative->colorHook(g);
    }
    void invalidate(Component* c) {
        mNative->invalidate(c);
    }
	void paint(Graphics* g) {
        mNative->paint(g);
    }
    void close() {
        mNative->close();
    }
    void invalidateHandle() {
        mNative->invalidateHandle();
    }
    void updateBounds() {
        mNative->updateBounds();
    }
    void setEnabled(bool b) {
        mNative->setEnabled(b);
    }
    bool isEnabled() {
        return mNative->isEnabled();
    }
    void setVisible(bool b) {
        mNative->setVisible(b);
    }
    bool isVisible() {
        return mNative->isVisible();
    }
    void setFocus() {
        mNative->setFocus();
    }
    void debug() {
        mNative->debug();
    }

    // TabbedPaneUI

    void setSelectedIndex(int i) {
        mNative->setSelectedIndex(i);
    }
    int getSelectedIndex() {
        return mNative->getSelectedIndex();
    }

  private:

	class WindowsTabbedPane* mNative;

};

/****************************************************************************
 *                                                                          *
 *                                   TABLE                                  *
 *                                                                          *
 ****************************************************************************/

class WindowsTable : public WindowsComponent {

  public:

	WindowsTable(Table* cb);
	~WindowsTable();

    Component* getComponent() {
        return mTable;
    }

    void updateBounds();
    void rebuild();
	void setSelectedIndex(int i);
	int getSelectedIndex();
	bool isSelected(int i);

	void open();
    void getPreferredSize(Window* w, Dimension* d);
	void command(int code);
    List* getColumnWidths(Window* w);

    // ownerdraw support
	void paint(Graphics* g);

  private:

	int getMaxColumnWidth(Window* w, TableModel* model, int col);

	Table* mTable;
    List* mColumnWidths;
	Font* mDefaultColumnFont;
	Font* mDefaultCellFont;
	int mHeaderHeight;
};

class WindowsTableUI : public TableUI {
  public:

	WindowsTableUI(WindowsTable* b) {
        mNative = b;
    }
	~WindowsTableUI() {
        delete mNative;
    }
    NativeComponent* getNative() {
        return mNative;
    }
	void open() {
        mNative->open();
    }
	void postOpen() {
    }
	void getPreferredSize(Window* w, Dimension* d) {
        mNative->getPreferredSize(w, d);
	}
	bool isNativeParent() {
		return false;
	}
	bool isOpen() {
		return mNative->isOpen();
	}
	void command(int code) {
        mNative->command(code);
    }
	Color* colorHook(Graphics* g) {
        return mNative->colorHook(g);
    }
    void invalidate(Component* c) {
        mNative->invalidate(c);
    }
	void paint(Graphics* g) {
        mNative->paint(g);
    }
    void close() {
        mNative->close();
    }
    void invalidateHandle() {
        mNative->invalidateHandle();
    }
    void updateBounds() {
        mNative->updateBounds();
    }
    void setEnabled(bool b) {
        mNative->setEnabled(b);
    }
    bool isEnabled() {
        return mNative->isEnabled();
    }
    void setVisible(bool b) {
        mNative->setVisible(b);
    }
    bool isVisible() {
        return mNative->isVisible();
    }
    void setFocus() {
        mNative->setFocus();
    }
    void debug() {
        mNative->debug();
    }

    // TableUI

    void rebuild() {
        mNative->rebuild();
    }
    void setSelectedIndex(int i) {
        mNative->setSelectedIndex(i);
    }
	int getSelectedIndex() {
        return mNative->getSelectedIndex();
    }
    bool isSelected(int i) {
        return mNative->isSelected(i);
    }
    List* getColumnWidths(Window* w) {
        return mNative->getColumnWidths(w);
    }

  private:

	class WindowsTable* mNative;

};

/****************************************************************************
 *                                                                          *
 *   								 TREE                                   *
 *                                                                          *
 ****************************************************************************/

class WindowsTree : public WindowsComponent {

  public:
	
	WindowsTree(Tree* t);
	~WindowsTree();

    Component* getComponent() {
        return mTree;
    }

	void open();
	
   private:

	Tree* mTree;

};

class WindowsTreeUI : public TreeUI {
  public:

	WindowsTreeUI(WindowsTree* b) {
        mNative = b;
    }
	~WindowsTreeUI() {
        delete mNative;
    }
    NativeComponent* getNative() {
        return mNative;
    }
	void open() {
        mNative->open();
    }
	void postOpen() {
    }
	void getPreferredSize(Window* w, Dimension* d) {
        // not implemented yet
	}
	bool isNativeParent() {
		return false;
	}
	bool isOpen() {
		return mNative->isOpen();
	}
	void command(int code) {
        mNative->command(code);
    }
	Color* colorHook(Graphics* g) {
        return mNative->colorHook(g);
    }
    void invalidate(Component* c) {
        mNative->invalidate(c);
    }
	void paint(Graphics* g) {
        mNative->paint(g);
    }
    void close() {
        mNative->close();
    }
    void invalidateHandle() {
        mNative->invalidateHandle();
    }
    void updateBounds() {
        mNative->updateBounds();
    }
    void setEnabled(bool b) {
        mNative->setEnabled(b);
    }
    bool isEnabled() {
        return mNative->isEnabled();
    }
    void setVisible(bool b) {
        mNative->setVisible(b);
    }
    bool isVisible() {
        return mNative->isVisible();
    }
    void setFocus() {
        mNative->setFocus();
    }
    void debug() {
        mNative->debug();
    }

  private:

	class WindowsTree* mNative;

};

/****************************************************************************
 *                                                                          *
 *   							  SCROLL BAR                                *
 *                                                                          *
 ****************************************************************************/

class WindowsScrollBar : public WindowsComponent {

  public:

	WindowsScrollBar(ScrollBar* sb);
	~WindowsScrollBar();

    Component* getComponent() {
        return mScrollBar;
    }

	void open();
	void getPreferredSize(Window* w, Dimension* d);
	void update();

	void scroll(int code);
	Color* colorHook(Graphics* g);

  private:

	ScrollBar* mScrollBar;

};

class WindowsScrollBarUI : public ScrollBarUI {
  public:

	WindowsScrollBarUI(WindowsScrollBar* b) {
        mNative = b;
    }
	~WindowsScrollBarUI() {
        delete mNative;
    }
    NativeComponent* getNative() {
        return mNative;
    }
	void open() {
        mNative->open();
    }
	void postOpen() {
    }
	void getPreferredSize(Window* w, Dimension* d) {
        mNative->getPreferredSize(w, d);
	}
	bool isNativeParent() {
		return false;
	}
	bool isOpen() {
		return mNative->isOpen();
	}
	void command(int code) {
        mNative->command(code);
    }
	Color* colorHook(Graphics* g) {
        return mNative->colorHook(g);
    }
    void invalidate(Component* c) {
        mNative->invalidate(c);
    }
	void paint(Graphics* g) {
        mNative->paint(g);
    }
    void close() {
        mNative->close();
    }
    void invalidateHandle() {
        mNative->invalidateHandle();
    }
    void updateBounds() {
        mNative->updateBounds();
    }
    void setEnabled(bool b) {
        mNative->setEnabled(b);
    }
    bool isEnabled() {
        return mNative->isEnabled();
    }
    void setVisible(bool b) {
        mNative->setVisible(b);
    }
    bool isVisible() {
        return mNative->isVisible();
    }
    void setFocus() {
        mNative->setFocus();
    }
    void debug() {
        mNative->debug();
    }

    // ScrollBarUI

    void update() {
        mNative->update();
    }

  private:

	class WindowsScrollBar* mNative;

};

/****************************************************************************
 *                                                                          *
 *   								WINDOW                                  *
 *                                                                          *
 ****************************************************************************/

class WindowsWindow : public WindowsComponent {

  public:
	
    WindowsWindow(Window* win);
    ~WindowsWindow();

    Component* getComponent() {
        return mWindow;
    }

    Graphics* getGraphics();
	virtual long messageHandler(UINT msg, WPARAM wparam, LPARAM lparam);

    void updateNativeBounds(Bounds* b);
	bool isChild();
	virtual void open();
	void close();
	int run();
	void relayout();
	void toFront();

    void setBackground(Color* c);

    // Temorary transition support

    WindowsContext* getContext();
	HINSTANCE getInstance();

	// has to be public so we can tear down under
	// special cirumstances (VST DLL unloading)
	
  protected:

	/**
	 * True if we've registered window classes.
	 */
	static bool ClassesRegistered;
	void registerClasses(const char* icon);

	void menuHandler(int id);

	void captureNativeBounds(bool warn);
	void center();

	virtual HWND getParentWindowHandle();

    void setupToolTips();
    void setupToolTips(Component* c);
    void mouseHandler(int msg, int keys, int x, int y);
    void keyHandler(int msg, int key, long status);
    Menu* getMenu(HMENU handle);

    Window* mWindow;
	HACCEL mAccel;
    HWND mToolTip;

	// This one has the cannonical HDC for use outside of message handlers
	HDC mDeviceContext;
    WindowsGraphics* mGraphics;

	// This one is initialized by message handlers and has a transient HDC
	WindowsGraphics* mEventGraphics;

    // event cache
	WindowEvent* mWindowEvent;
    MouseEvent* mMouseEvent;
    KeyEvent* mKeyEvent;

    // the component receiving mouseDragged events
    Component* mDragComponent;

	// true if this is a child window owned by a window out of
	// our control
	bool mChild;

    // kludge necessary to get the origin of the damn client region
    // since GetClientRect sucks
    int mFuckingClientTopOffset;
    int mFuckingClientLeftOffset;

  private:

};

class WindowsWindowUI : public WindowUI {
  public:

	WindowsWindowUI(WindowsWindow* b) {
        mNative = b;
    }
	~WindowsWindowUI() {
        delete mNative;
    }
    NativeComponent* getNative() {
        return mNative;
    }
	void open() {
        mNative->open();
    }
	void postOpen() {
    }
	void getPreferredSize(Window* w, Dimension* d) {
        // size defined by children
	}
	bool isNativeParent() {
		return true;
	}
	bool isOpen() {
		return mNative->isOpen();
	}
	bool isChild() {
		return mNative->isChild();
	}
	void toFront() {
		mNative->toFront();
	}
	void command(int code) {
        mNative->command(code);
    }
	Color* colorHook(Graphics* g) {
        return mNative->colorHook(g);
    }
    void invalidate(Component* c) {
        mNative->invalidate(c);
    }
	void paint(Graphics* g) {
        mNative->paint(g);
    }
    void close() {
        mNative->close();
    }
    void invalidateHandle() {
        mNative->invalidateHandle();
    }
    void updateBounds() {
        mNative->updateBounds();
    }
    void setEnabled(bool b) {
        mNative->setEnabled(b);
    }
    bool isEnabled() {
        return mNative->isEnabled();
    }
    void setVisible(bool b) {
        mNative->setVisible(b);
    }
    bool isVisible() {
        return mNative->isVisible();
    }
    void setFocus() {
        mNative->setFocus();
    }
    void debug() {
        mNative->debug();
    }

    // WindowUI

    Graphics* getGraphics() {
        return mNative->getGraphics();
    }
    int run() {
        return mNative->run();
    }
    void relayout() {
        mNative->relayout();
    }
    void setBackground(Color* c) {
        mNative->setBackground(c);
    }

  protected:

	class WindowsWindow* mNative;

};

//////////////////////////////////////////////////////////////////////
//
// WindowsHostFrame
//
//////////////////////////////////////////////////////////////////////

class WindowsHostFrame : public WindowsWindow {

  public:
	
    WindowsHostFrame(HostFrame* f);
    ~WindowsHostFrame();

	void open();
    void close();

    // overload from WindowsWindow to return the host window handle
    HWND getParentWindowHandle();

  protected:

  private:

};

class WindowsHostFrameUI : public HostFrameUI {
  public:

	WindowsHostFrameUI(WindowsHostFrame* f) {
        mNative = f;
    }
	~WindowsHostFrameUI() {
        delete mNative;
    }
    NativeComponent* getNative() {
        return mNative;
    }
	void open() {
        mNative->open();
    }
	void postOpen() {
    }
	void getPreferredSize(Window* w, Dimension* d) {
        // size defined by children
	}
	bool isNativeParent() {
		return true;
	}
	bool isOpen() {
		return mNative->isOpen();
	}
	bool isChild() {
		return mNative->isChild();
	}
	void toFront() {
		mNative->toFront();
	}
	void command(int code) {
        mNative->command(code);
    }
	Color* colorHook(Graphics* g) {
        return mNative->colorHook(g);
    }
    void invalidate(Component* c) {
        mNative->invalidate(c);
    }
	void paint(Graphics* g) {
        mNative->paint(g);
    }
    void close() {
        mNative->close();
    }
    void invalidateHandle() {
        mNative->invalidateHandle();
    }
    void updateBounds() {
        mNative->updateBounds();
    }
    void setEnabled(bool b) {
        mNative->setEnabled(b);
    }
    bool isEnabled() {
        return mNative->isEnabled();
    }
    void setVisible(bool b) {
        mNative->setVisible(b);
    }
    bool isVisible() {
        return mNative->isVisible();
    }
    void setFocus() {
        mNative->setFocus();
    }
    void debug() {
        mNative->debug();
    }

    // WindowUI

    Graphics* getGraphics() {
        return mNative->getGraphics();
    }
    int run() {
        return mNative->run();
    }
    void relayout() {
        mNative->relayout();
    }
    void setBackground(Color* c) {
        mNative->setBackground(c);
    }

  protected:

	class WindowsHostFrame* mNative;

};

/****************************************************************************
 *                                                                          *
 *                                   DIALOG                                 *
 *                                                                          *
 ****************************************************************************/

class WindowsDialog : public WindowsWindow {

  public:
	
    WindowsDialog(Dialog* d);
    ~WindowsDialog();

    BOOL dialogHandler(UINT msg, WPARAM wparam, LPARAM lparam);
    void show();

  protected:

    unsigned long modalEventLoop();

};

class WindowsDialogUI : public DialogUI {
  public:

	WindowsDialogUI(WindowsDialog * d) {
        mNative = d;
    }
	~WindowsDialogUI() {
        delete mNative;
    }
    NativeComponent* getNative() {
        return mNative;
    }
	void open() {
        mNative->open();
    }
	void postOpen() {
    }
	void getPreferredSize(Window* w, Dimension* d) {
        // size defined by children
	}
	bool isNativeParent() {
		return true;
	}
	bool isOpen() {
		return mNative->isOpen();
	}
	bool isChild() {
		return mNative->isChild();
	}
	void toFront() {
		mNative->toFront();
	}
	void command(int code) {
        mNative->command(code);
    }
	Color* colorHook(Graphics* g) {
        return mNative->colorHook(g);
    }
    void invalidate(Component* c) {
        mNative->invalidate(c);
    }
	void paint(Graphics* g) {
        mNative->paint(g);
    }
    void close() {
        mNative->close();
    }
    void invalidateHandle() {
        mNative->invalidateHandle();
    }
    void updateBounds() {
        mNative->updateBounds();
    }
    void setEnabled(bool b) {
        mNative->setEnabled(b);
    }
    bool isEnabled() {
        return mNative->isEnabled();
    }
    void setVisible(bool b) {
        mNative->setVisible(b);
    }
    bool isVisible() {
        return mNative->isVisible();
    }
    void setFocus() {
        mNative->setFocus();
    }
    void debug() {
        mNative->debug();
    }

    // WindowUI

    Graphics* getGraphics() {
        return mNative->getGraphics();
    }
    int run() {
        return mNative->run();
    }
    void relayout() {
        mNative->relayout();
    }
    void setBackground(Color* c) {
        mNative->setBackground(c);
    }

    // DialogUI
    
    void show() {
        mNative->show();
    }

  protected:

	class WindowsDialog* mNative;

};

/****************************************************************************
 *                                                                          *
 *                                    MENU                                  *
 *                                                                          *
 ****************************************************************************/

/**
 * The same UI class is used for all MenuItems.
 */
class WindowsMenuItem : public WindowsComponent {

  public: 

    WindowsMenuItem(MenuItem* item);
    ~WindowsMenuItem();

    Component* getComponent() {
        return mItem;
    }

    HMENU getMenuHandle();

	void setChecked(bool b);
	void setEnabled(bool b);
	void removeAll();

    void open();
    void close();
    void invalidateHandle();
    void openPopup(Window* window, int x, int y);

	// for use by WindowsWindowUI?
    Menu* findMenu(HMENU handle);

  private:

    void setNativeState(int mask);
    HMENU openResourceMenu(const char* resource);
    // hey!! we have one of these inherited from WindowsComponent
    // which isn't marked virtual and differs only in return type 
    // (which is probably void*) anyway
    HMENU getParentHandle();
    void openPopupMenu();
    void openMenuBar();
    void openMenu();
    void openItem();


    MenuItem* mItem;
    HMENU mMenuHandle;
    bool mCreated;

};

class WindowsMenuUI : public MenuUI {
  public:

	WindowsMenuUI(WindowsMenuItem* i) {
        mNative = i;
    }
	~WindowsMenuUI() {
        delete mNative;
    }
    NativeComponent* getNative() {
        return mNative;
    }
	void open() {
        mNative->open();
    }
	void postOpen() {
    }
	void getPreferredSize(Window* w, Dimension* d) {
        // not an embedded component
	}
	bool isNativeParent() {
		return false;
	}
	bool isOpen() {
		return mNative->isOpen();
	}
	void command(int code) {
        mNative->command(code);
    }
	Color* colorHook(Graphics* g) {
        return mNative->colorHook(g);
    }
    void invalidate(Component* c) {
        mNative->invalidate(c);
    }
	void paint(Graphics* g) {
        mNative->paint(g);
    }
    void close() {
        mNative->close();
    }
    void invalidateHandle() {
        mNative->invalidateHandle();
    }
    void updateBounds() {
        mNative->updateBounds();
    }
    void setEnabled(bool b) {
        mNative->setEnabled(b);
    }
    bool isEnabled() {
        return mNative->isEnabled();
    }
    void setVisible(bool b) {
        mNative->setVisible(b);
    }
    bool isVisible() {
        return mNative->isVisible();
    }
    void setFocus() {
        mNative->setFocus();
    }
    void debug() {
        mNative->debug();
    }

    // MenuUI

    void setChecked(bool b) {
        mNative->setChecked(b);
    }
    void removeAll() {
        mNative->removeAll();
    }
    void openPopup(Window* window, int x, int y) {
        mNative->openPopup(window, x, y);
    }

  private:

	class WindowsMenuItem* mNative;

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
