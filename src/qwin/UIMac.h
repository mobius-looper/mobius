/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Implementations of ComponentUI for the OS X
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
 *
 */

#ifndef UI_MAC_H
#define UI_MAC_H

#include <Carbon/Carbon.h>
#include <ApplicationServices/ApplicationServices.h>
#include "UIManager.h"

// don't know where the fuck this is from...
typedef float CGFloat;

QWIN_BEGIN_NAMESPACE

//////////////////////////////////////////////////////////////////////
//
// Events
//
//////////////////////////////////////////////////////////////////////

/**
 * The "class" of the custom event we send to redraw components
 * from a thread other than the UI thread.  Necessary because API
 * methods do not work reliably outside the UI thread.
 */
const UInt32 kEventClassCustom = 'cust';

/**
 * The "kind" of the custom event we send to redraw components.
 */
const UInt32 kEventCustomInvalidate = 1;

/**
 * The "kind" of the custom event we send to change
 * a component value.
 */
const UInt32 kEventCustomChange = 2;

/**
 * The EventParamType used for the custom event parameters.
 * We just need something big enough for an object pointer.
 */
const OSType typeQwinComponent ='qwin';

/**
 * The EventParamName used for the event parameter that holds
 * the MacComponent to handle the invalidateNative method.
 */
const OSType kEventParamCustomPeer = 'peer';

/**
 * The EventParamName used for the event parameter that holds
 * the Component we're trying to invalidate.
 */
const OSType kEventParamCustomComponent = 'comp';

/**
 * The EventParamName used for the event parameter that holds
 * the value type for a CustomChange event.
 */
const OSType kEventParamCustomType = 'type';

/**
 * The EventParamName used for the event parameter that holds
 * the value for a CustomChange event.
 */
const OSType kEventParamCustomValue = 'valu';

/****************************************************************************
 *                                                                          *
 *                                  CONTEXT                                 *
 *                                                                          *
 ****************************************************************************/

/**
 * MacContext
 */
class MacContext : public Context {
    
  public:

    MacContext(int argc, char* argv[]);
    ~MacContext();

	const char* getInstallationDirectory();
	void printContext();

  private:

};

/****************************************************************************
 *                                                                          *
 *   								COLOR                                   *
 *                                                                          *
 ****************************************************************************/

#define MAX_PEN_WIDTH 4

/**
 * Convert a Windows style RGB value from 0 to 255 to a
 * Mac RGBColor value from 0 to 65535.
 */
#define RGB_WIN_TO_MAC(value) (int)(((float)(value) / 255.0f) * 65535.0f)
#define RGB_WIN_TO_FLOAT(value) ((float)(value) / 255.0f)

/**
 * Convert a Mac RGB value from 0 to 65535 to a Windows style
 * value from 0 to 255.
 */
#define RGB_MAC_TO_WIN(value) (int)(((float)(value) / 65535.0f) * 255.0f)

class MacColor : public NativeColor {

  public:

	MacColor();
	MacColor(Color* c);
	~MacColor();

	void setRgb(int rgb);
	
	MacColor* getMacColor();
	RGBColor* getRGBColor();
	void getRGBColor(RGBColor* color);

	CGFloat getRed();
	CGFloat getGreen();
	CGFloat getBlue();
	CGFloat getAlpha();


  private:

	Color* mColor;
	RGBColor mRGBColor;
	CGFloat mRed;
	CGFloat mGreen;
	CGFloat mBlue;
	CGFloat mAlpha;

};

/****************************************************************************
 *                                                                          *
 *   								 FONT                                   *
 *                                                                          *
 ****************************************************************************/

class MacFont : public NativeFont {

  public:

	MacFont(Font* f);
	~MacFont();

	int getAscent();
	int getDescent();
	int getHeight();

   	ATSFontRef getATSFontRef();
	ATSUStyle getStyle();

  private:

	void dumpMetrics(const char* type, struct ATSFontMetrics* metrics);

	Font* mFont;
	ATSFontRef mHandle;
	ATSUStyle mStyle;
	int mAscent;
	int mDescent;
	int mLeading;

};

/****************************************************************************
 *                                                                          *
 *                                   TIMER                                  *
 *                                                                          *
 ****************************************************************************/

class MacTimer : public NativeTimer {

  public:

	MacTimer(SimpleTimer* t);
	~MacTimer();

	void fire();

  private:

	SimpleTimer* mTimer;
	EventLoopTimerRef mNative;
};

/****************************************************************************
 *                                                                          *
 *   							 TEXT METRICS                               *
 *                                                                          *
 ****************************************************************************/

class MacTextMetrics : public TextMetrics {

  public:
	
	MacTextMetrics();
	void init();
	void init(Font* font);

	// TextMetrics interface methods
	// don't need all of these but should try to flesh out someday!

	int getHeight();
	int getAscent();
	int getDescent();
	int getMaxWidth();
	int getAverageWidth();
	int getExternalLeading();

  private:

	int mHeight;
	int mMaxWidth;
	int mAverageWidth;
	int mAscent;
	int mDescent;
	int mExternalLeading;

};

/****************************************************************************
 *                                                                          *
 *   							   GRAPHICS                                 *
 *                                                                          *
 ****************************************************************************/

//
// Utility functions in MacGraphics, needed by Font
//

extern int GetStyleAttribute(ATSUStyle style, ATSUAttributeTag attribute);
extern void SetStyleFont(ATSUStyle style, ATSUFontID font);
extern void SetStyleFontSize(ATSUStyle style, int size);
extern void SetStyleBold(ATSUStyle style, bool bold);
extern void SetStyleItalic(ATSUStyle style, bool italic);
extern int GetStyleDescent(ATSUStyle style);

/**
 * Maximum size of the UniChar buffer we use when converting
 * C strings for use with ATSUI. 
 */
#define MAX_UNICHAR_BUFFER 4096

class MacGraphics : public Graphics {

    friend class MacWindow;

  public:

	MacGraphics();
	MacGraphics(class MacWindow* win);
	~MacGraphics();

	void setWindow(MacWindow* win);
	void save();
	void restore();

    Color* getColor();

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
    //LPDRAWITEMSTRUCT getDrawItem();

  protected:

    //void setDrawItem(LPDRAWITEMSTRUCT di);

  private:

	void init();
    Font* getDefaultFont();
    Font* getEffectiveFont();
	WindowRef getWindowRef();
	ATSUTextLayout getLayout(CGContextRef context, const char* text, Font* font);
	void measureText(ATSUTextLayout layout, Dimension* d);
	void setUniChars(const char* str);
	CGContextRef beginContext();
	CGContextRef beginContextBasic();
	void endContext(CGContextRef context);
	MacColor* getMacForeground();
	MacColor* getMacBackground();
	void setFillColor(CGContextRef context);
	void setStrokeColor(CGContextRef context);
	CGContextRef pathRoundRect(int x, int y, int width, int height,
							   int arcWidth, int arcHeight);

	MacWindow* mWindow;
	MacTextMetrics mTextMetrics;

	UniChar mUniChars[MAX_UNICHAR_BUFFER];
	int mUniCharsLength;

    Font* mDefaultFont;

	// when created in response to a WM_DRAWITEM message
	//LPDRAWITEMSTRUCT mDrawItem;

    Color* mColor;
    Color* mBackground;

    Font* mFont;

};

/****************************************************************************
 *                                                                          *
 *   							SYSTEM DIALOGS                              *
 *                                                                          *
 ****************************************************************************/

class MacOpenDialog : public SystemDialogUI {

  public:

	MacOpenDialog(OpenDialog* od);
	~MacOpenDialog();

	void show();

	void callback(NavEventCallbackMessage callBackSelector,
				  NavCBRecPtr callBackParms);

  private:
    
	OSErr extractFSRef(const NavReplyRecord *reply, FSRef *item);

    OpenDialog* mDialog;
	NavDialogRef mHandle;
	bool mTerminated;
};

class MacColorDialog : public SystemDialogUI {

  public:

	MacColorDialog(ColorDialog* cd);
	~MacColorDialog();

	void show();

  private:
    
    ColorDialog* mDialog;

};

class MacMessageDialog : public SystemDialogUI {

  public:

	MacMessageDialog(MessageDialog* cd);
	~MacMessageDialog();

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
 * The base class for all Mac implementations of ComponentUI.
 * Unfortunately have to get to these via proxy due to odd 
 * MSVC/C++ behavior with multiple inheritance.
 */
class MacComponent : public NativeComponent {

  public:

	MacComponent();
    virtual ~MacComponent();
    void subclassWindowProc();

	// return the native handle of this component
	// defined by the NativeComponent interface so it must return void*
    void* getHandle();

    void detach();
	void convertBounds(Bounds* in, Rect* out);
	virtual void updateNativeBounds(Bounds* b);
	virtual void adjustControlBounds(Rect* bounds);
	virtual bool isOpen();

	virtual void command(int code);
	virtual void notify(int code);

    virtual long messageHandler(int msg, int wparam, int lparam);

	//
	// Model flipping
	//
	// The "component" model is the one the application builds, 
	// based on Component.  The "native" model are the Windows and
	// Mac implementation classes such as WindowsWindow and MacStatic.
	// The "proxy" or "ui" classes are the ones that sit between
	// the component model and the native model to workaround the MSVC6
	// multiple-inheritance issues.  Hopefully those are temporary.
	//
	// The Compoenent model is hierarchical, the NativeComponent model
	// is not.  So when a NativeComponent needs to know its parent
	// compoment we have to locate the peer in the Component model, 
	// get it's parent, then from the parent get the native compoment.
	//

    MacWindow* getMacWindow(Component* c);
    MacWindow* getMacWindow();
    MacContext* getMacContext(Component* c);
	MacContext* getMacContext();

	/**
	 * Given a native component, return the generic component.
	 * This is virtual so the native comonents can have a specific
	 * field to hold it.
	 */
    virtual Component* getComponent() = 0;

	/**
	 * Convenience method to return the native handle of any Component.
	 */
    static void* getHandle(Component* c);

	/**
	 * Return the native component that is logically a parent to this
	 * component.
	 */
	MacComponent* getParent();

	/**
	 * Return the native handle for the native component that is logically
	 * a parent to this component.
	 */
	void* getParentHandle();

	/**
	 * Get the native window handle for the native component.
	 */
	WindowRef getWindowRef();

	/**
	 * Get the MacGraphics for the window.
	 */
	MacGraphics* getMacGraphics();

	/**
	 * Return true if we're using a compositing window.
	 */
	virtual bool isCompositing();

    // default implementations for ComponentUI methods

	virtual void getPreferredSize(Window* w, Dimension* d);
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

	// general embedding utility
	void embedChildren(ControlRef parent);
	void getEmbeddingParent(WindowRef* retwindow, ControlRef* retcontrol);

	// second half of the two-phase invalidation process
	void invalidateNative(Component* c);

	// change requests

	void sendChangeRequest(int type, void* value);
	void handleChangeRequest(EventRef event);
	virtual void handleChangeRequest(int type, void* value);

  protected:

	void embedChildren(ControlRef parent, bool compositing);

    void* mHandle;

};

/****************************************************************************
 *                                                                          *
 *   								STATIC                                  *
 *                                                                          *
 ****************************************************************************/

class MacStatic : public MacComponent {

  public:
	
	MacStatic(Static* s);
	~MacStatic();

    Component* getComponent() {
        return mStatic;
    }

	void setText(const char* s);
	void setBitmap(const char* s);
	void setIcon(const char* s);

	void open();
	void getPreferredSize(Window* w, Dimension* d);
	void updateNativeBounds(Bounds* b);

  private:

	Static* mStatic;
    bool mAutoColor;

};

class MacStaticUI : public StaticUI {

  public:

	MacStaticUI(MacStatic* s) {
        mNative = s;
    }
	~MacStaticUI() {
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

	class MacStatic* mNative;

};

/****************************************************************************
 *                                                                          *
 *                                   PANEL                                  *
 *                                                                          *
 ****************************************************************************/

class MacPanel : public MacComponent {

  public:
	
	MacPanel(Panel* p);
	~MacPanel();

    Component* getComponent() {
        return mPanel;
    }

	void open();
	void close();
	void postOpen();
	bool isNativeParent();
	void draw();
	bool hitTest(EventRef event);
	
  private:

	Panel* mPanel;

};

class MacPanelUI : public PanelUI {

  public:

	MacPanelUI(MacPanel* p) {
        mNative = p;
    }
	~MacPanelUI() {
        delete mNative;
    }
    NativeComponent* getNative() {
        return mNative;
    }
	void open() {
        mNative->open();
    }
	void postOpen() {
		mNative->postOpen();
    }
	void getPreferredSize(Window* w, Dimension* d) {
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

	class MacPanel* mNative;

};

/****************************************************************************
 *                                                                          *
 *   								BUTTON                                  *
 *                                                                          *
 ****************************************************************************/

class MacButton : public MacComponent {

  public:

	MacButton(Button * b);
	~MacButton();

    Component* getComponent() {
        return mButton;
    }

    void setText(const char* text);
	void click();
	void open();
	void close();
	void adjustControlBounds(Rect* bounds);
	void getPreferredSize(Window* w, Dimension* d);
	void fireActionPerformed(bool hit);
	void fireMouseReleased();
	void hiliteChanged();

  private:

	Button* mButton;
	bool mDown;
	int mHilites;

};

class MacButtonUI : public ButtonUI {

  public:

	MacButtonUI(MacButton* p) {
        mNative = p;
    }
	~MacButtonUI() {
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

    // ButtonUI

    void setText(const char* text) {
        mNative->setText(text);
    }
	void click() {
        mNative->click();
    }

  private:

	class MacButton* mNative;

};

/****************************************************************************
 *                                                                          *
 *   							 RADIO BUTTON                               *
 *                                                                          *
 ****************************************************************************/

class MacRadioButton : public MacComponent {

  public:

	MacRadioButton();
	MacRadioButton(RadioButton *b);
	~MacRadioButton();

    Component* getComponent() {
        return mButton;
    }

	void setSelected(bool b);
	bool isSelected();

	void open();
	void fireActionPerformed();

  private:

	RadioButton* mButton;

};

class MacRadioButtonUI : public RadioButtonUI {

  public:

	MacRadioButtonUI(MacRadioButton* b) {
        mNative = b;
    }
	~MacRadioButtonUI() {
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

	class MacRadioButton* mNative;

};

/****************************************************************************
 *                                                                          *
 *   							 RADIOS                                     *
 *                                                                          *
 ****************************************************************************/

class MacRadios : public MacComponent {

  public:

	MacRadios();
	MacRadios(Radios *b);
	~MacRadios();

    Component* getComponent() {
        return mRadios;
    }

	void open();
	void changeSelection(RadioButton* b);

  private:

	Radios* mRadios;

};

class MacRadiosUI : public RadiosUI {

  public:

	MacRadiosUI(MacRadios* b) {
        mNative = b;
    }
	~MacRadiosUI() {
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

	class MacRadios* mNative;

};

/****************************************************************************
 *                                                                          *
 *   							   CHECKBOX                                 *
 *                                                                          *
 ****************************************************************************/

class MacCheckbox : public MacComponent {

  public:

	MacCheckbox(Checkbox *cb);
	~MacCheckbox();

    Component* getComponent() {
        return mCheckbox;
    }

	void setSelected(bool b);
	bool isSelected();
	void open();
	void fireActionPerformed();

  private:

	Checkbox* mCheckbox;

};

class MacCheckboxUI : public CheckboxUI {
  public:

	MacCheckboxUI(MacCheckbox* b) {
        mNative = b;
    }
	~MacCheckboxUI() {
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

    // RadioButtonUI, CheckboxUI

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

	class MacCheckbox* mNative;

};

/****************************************************************************
 *                                                                          *
 *   							  COMBO BOX                                 *
 *                                                                          *
 ****************************************************************************/

class MacComboBox : public MacComponent {

  public:

	MacComboBox(ComboBox* cb);
	~MacComboBox();

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
	void fireActionPerformed();
	
	void handleChangeRequest(int type, void* value);

  private:

	void setSelectedIndexNow(int index);

	ComboBox* mComboBox;

};

class MacComboBoxUI : public ComboBoxUI {
  public:

	MacComboBoxUI(MacComboBox* b) {
        mNative = b;
    }
	~MacComboBoxUI() {
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

	class MacComboBox* mNative;

};

/****************************************************************************
 *                                                                          *
 *   							   LIST BOX                                 *
 *                                                                          *
 ****************************************************************************/

class MacListBox : public MacComponent {

  public:

	MacListBox(ListBox* cb);
	~MacListBox();

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

  private:

	void addColumn(ControlRef control, int id, int width);
	void calcColumnWidths(Window* w);
	int getMaxWidth(Graphics* g, StringList* list);

	ListBox* mListBox;
	int mMainWidth;
	int mAnnotationWidth;

};

class MacListBoxUI : public ListBoxUI {
  public:

	MacListBoxUI(MacListBox* b) {
        mNative = b;
    }
	~MacListBoxUI() {
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

	class MacListBox* mNative;

};

/****************************************************************************
 *                                                                          *
 *   							  GROUP BOX                                 *
 *                                                                          *
 ****************************************************************************/

class MacGroupBox : public MacComponent {

  public:

	MacGroupBox(GroupBox* gb);
	~MacGroupBox();

    Component* getComponent() {
        return mGroupBox;
    }

	void setText(const char* s);
	void open();

  private:

	GroupBox* mGroupBox;

};

class MacGroupBoxUI : public GroupBoxUI {
  public:

	MacGroupBoxUI(MacGroupBox* b) {
        mNative = b;
    }
	~MacGroupBoxUI() {
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

    // GroupBoxUI

    void setText(const char* s) {
        mNative->setText(s);
    }

  private:

	class MacGroupBox* mNative;

};

/****************************************************************************
 *                                                                          *
 *                                    TEXT                                  *
 *                                                                          *
 ****************************************************************************/

class MacText : public MacComponent {

  public:

    MacText(Text* t);
    ~MacText();

    Component* getComponent() {
        return mText;
    }

	void setEditable(bool b);
    void setText(const char* s);
    char* getText();

    virtual void open();
	virtual void getPreferredSize(Window* w, Dimension* d);
	void adjustControlBounds(Rect* bounds);
	void fireActionPerformed();
	void handleChangeRequest(int type, void* value);

  protected:

  	void setTextNow();

    Text* mText;
	int mHeight;
	int mEmWidth;

};

class MacTextUI : public TextUI {
  public:

	MacTextUI(MacText* b) {
        mNative = b;
    }
	~MacTextUI() {
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

  private:

	class MacText* mNative;

};

/****************************************************************************
 *                                                                          *
 *                                 TEXT AREA                                *
 *                                                                          *
 ****************************************************************************/

class MacTextArea : public MacText {

  public:

    MacTextArea(TextArea* t);
    ~MacTextArea();

    void open();
	void getPreferredSize(Window* w, Dimension* d);

  private:

};

class MacTextAreaUI : public TextAreaUI {
  public:

	MacTextAreaUI(MacTextArea* a) {
        mNative = a;
    }
	~MacTextAreaUI() {
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

    // TextAreaUI

    void setEditable(bool b) {
        mNative->setEditable(b);
    }
    void setText(const char* s) {
        mNative->setText(s);
    }
    char* getText() {
        return mNative->getText();
    }

  private:

	class MacTextArea* mNative;

};

/****************************************************************************
 *                                                                          *
 *   							   TOOL BAR                                 *
 *                                                                          *
 ****************************************************************************/

class MacToolBar : public MacComponent {

  public:

	MacToolBar(ToolBar* tb);
	~MacToolBar();

    Component* getComponent() {
        return mToolBar;
    }

	void open();

   private:

	ToolBar* mToolBar;

};

class MacToolBarUI : public ToolBarUI {
  public:

	MacToolBarUI(MacToolBar* b) {
        mNative = b;
    }
	~MacToolBarUI() {
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

  private:

	class MacToolBar* mNative;

};

/****************************************************************************
 *                                                                          *
 *   							  STATUS BAR                                *
 *                                                                          *
 ****************************************************************************/

class MacStatusBar : public MacComponent {

  public:

	MacStatusBar(StatusBar* sb);
	~MacStatusBar();

    Component* getComponent() {
        return mStatusBar;
    }

	void open();

  private:

	StatusBar* mStatusBar;

};

class MacStatusBarUI : public StatusBarUI {
  public:

	MacStatusBarUI(MacStatusBar* b) {
        mNative = b;
    }
	~MacStatusBarUI() {
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

	class MacStatusBar* mNative;

};

/****************************************************************************
 *                                                                          *
 *   							 TABBED PANE                                *
 *                                                                          *
 ****************************************************************************/

class MacTabbedPane : public MacComponent {

  public:
	
	MacTabbedPane(TabbedPane* sb);
	~MacTabbedPane();

    Component* getComponent() {
        return mTabbedPane;
    }

	void setSelectedIndex(int i);
    int getSelectedIndex();

	void open();
	void showTabPane(int index);
	void getPreferredSize(Window* w, Dimension* d);
	void updateNativeBounds(Bounds* b);
	void postOpen();
	void fireActionPerformed();

   private:

	TabbedPane* mTabbedPane;
	ControlRef mPanes[3];
};

class MacTabbedPaneUI : public TabbedPaneUI {
  public:

	MacTabbedPaneUI(MacTabbedPane* b) {
        mNative = b;
    }
	~MacTabbedPaneUI() {
        delete mNative;
    }
    NativeComponent* getNative() {
        return mNative;
    }
	void open() {
        mNative->open();
    }
	void postOpen() {
		mNative->postOpen();
    }
	void getPreferredSize(Window* w, Dimension* d) {
		mNative->getPreferredSize(w, d);
	}
	bool isNativeParent() {
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

    // TabbedPaneUI

    void setSelectedIndex(int i) {
        mNative->setSelectedIndex(i);
    }
    int getSelectedIndex() {
        return mNative->getSelectedIndex();
    }

  private:

	class MacTabbedPane* mNative;

};

/****************************************************************************
 *                                                                          *
 *                                   TABLE                                  *
 *                                                                          *
 ****************************************************************************/

class MacTable : public MacComponent {

  public:

	MacTable(Table* t);
	~MacTable();

    Component* getComponent() {
        return mTable;
    }

    void rebuild();
	void setSelectedIndex(int i);
	int getSelectedIndex();
	bool isSelected(int i);
    List* getColumnWidths(Window* w);

	void open();
	void getPreferredSize(Window* w, Dimension* d);

  private:

	int getMaxColumnWidth(Graphics* g, TableModel* model, int col);
	void addColumn(ControlRef control, int id, const char* name, int width);

	Table* mTable;
    List* mColumnWidths;
	Font* mDefaultColumnFont;
	Font* mDefaultCellFont;
	int mHeaderHeight;

};

class MacTableUI : public TableUI {
  public:

	MacTableUI(MacTable* b) {
        mNative = b;
    }
	~MacTableUI() {
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

	class MacTable* mNative;

};

/****************************************************************************
 *                                                                          *
 *   								 TREE                                   *
 *                                                                          *
 ****************************************************************************/

class MacTree : public MacComponent {

  public:
	
	MacTree(Tree* t);
	~MacTree();

    Component* getComponent() {
        return mTree;
    }

	void open();
	
   private:

	Tree* mTree;

};

class MacTreeUI : public TreeUI {
  public:

	MacTreeUI(MacTree* b) {
        mNative = b;
    }
	~MacTreeUI() {
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

	class MacTree* mNative;

};

/****************************************************************************
 *                                                                          *
 *   							  SCROLL BAR                                *
 *                                                                          *
 ****************************************************************************/

class MacScrollBar : public MacComponent {

  public:

	MacScrollBar(ScrollBar* sb);
	~MacScrollBar();

    Component* getComponent() {
        return mScrollBar;
    }

	void open();
	void getPreferredSize(Window* w, Dimension* d);
	void update();
	void moved();

  private:

	ScrollBar* mScrollBar;

};

class MacScrollBarUI : public ScrollBarUI {
  public:

	MacScrollBarUI(MacScrollBar* b) {
        mNative = b;
    }
	~MacScrollBarUI() {
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

	class MacScrollBar* mNative;

};

/****************************************************************************
 *                                                                          *
 *   								WINDOW                                  *
 *                                                                          *
 ****************************************************************************/

/** 
 * Maximum number of components we'll manage on the paint list before
 * punting and assuming that the entire window needs to be repainted.
 */
#define MAX_PAINT_LIST 256

class MacWindow : public MacComponent {

  public:
	
    MacWindow(Window* win);
    ~MacWindow();

    Component* getComponent() {
        return mWindow;
    }

    Graphics* getGraphics();

	bool isCompositing();
	bool isChild();
	virtual void open();
	void captureNativeBounds(bool warn);
	void close();
	int run();
	void relayout();
	void toFront();

	int getTitleBarHeight();
    void setBackground(Color* c);
    // Temorary transition support
    MacContext* getContext();

	void addPaint(Component* c);
	void doPaints();

	// event handlers
	void repaint();
	void resize();
	virtual bool mouseHandler(EventRef event);
	bool keyHandler(EventRef event);
	virtual void closeEvent();
	virtual void quitEvent();

	/**
	 * Mac kludge, since we can't AFAIK get mouse up events sent to the
	 * button, we have to catch them on the window and redirect to the
	 * registered button.
	 */
	void setDownButton(MacButton* b);

  protected:

	void installEventHandlers(WindowRef window);
	void removeEventHandlers();
	void center();

    void setupToolTips();
    void setupToolTips(Component* c);

	void getMouseLocation(EventRef event, int* retx, int* rety);
	int getMouseButton(EventRef event);
	int getClickCount(EventRef event);
	int getKeyModifiers(EventRef event);

	int getKeyCode(EventRef event);
	int getMacChar(EventRef event);

    Window* mWindow;
	bool mCompositing;
	void* mAccel;
    void* mToolTip;
	
	EventHandlerRef mWindowHandler;
	EventHandlerRef mRootHandler;

    MacGraphics* mGraphics;

    // event cache
	WindowEvent* mWindowEvent;
    MouseEvent* mMouseEvent;
    KeyEvent* mKeyEvent;

    // the component receiving mouseDragged events
    Component* mDragComponent;

	// the component waiting for a mouse UP event (always a button)
	class MacButton* mDownButton;

	// true if this is a child window owned by a window out of
	// our control
	bool mChild;
	
	// cached title bar height
	int mTitleBarHeight;
	
	// flag to indiciate that we've gone through 
	// the close process and called Window::closing
	bool mClosed;

	// paint list 
	class CriticalSection* mCsect;
	Component* mPaintComponents[MAX_PAINT_LIST];
	int mPaintHead;
	int mPaintTail;
	bool mPaintOverflow;

  private:

	void customEventLoop();

};

class MacWindowUI : public WindowUI {
  public:

	MacWindowUI(MacWindow* b) {
        mNative = b;
    }
	~MacWindowUI() {
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

	class MacWindow* mNative;

};

//////////////////////////////////////////////////////////////////////
//
// MacHostFrame
//
//////////////////////////////////////////////////////////////////////

class MacHostFrame : public MacWindow {

  public:
	
    MacHostFrame(HostFrame* f);
    ~MacHostFrame();
	
	void open();
	void postOpen();
	void closeEvent();
	void close();
	bool mouseHandler(EventRef event);

  protected:

  private:

	void setupRootControl();
	void embedChildren(HIViewRef contentView, ControlRef rootControl,
					   Container* parent) ;

	ControlDefSpec mControlSpec;
	ControlRef mControl;

};

class MacHostFrameUI : public HostFrameUI {
  public:

	MacHostFrameUI(MacHostFrame* f) {
        mNative = f;
    }
	~MacHostFrameUI() {
        delete mNative;
    }
    NativeComponent* getNative() {
        return mNative;
    }
	void open() {
        mNative->open();
    }
	void postOpen() {
		mNative->postOpen();
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

	class MacHostFrame* mNative;

};

/****************************************************************************
 *                                                                          *
 *                                   DIALOG                                 *
 *                                                                          *
 ****************************************************************************/

class MacDialog : public MacWindow {

  public:
	
    MacDialog(Dialog* d);
    ~MacDialog();

	void open();
    void show();
	void closeEvent();

  protected:

};

class MacDialogUI : public DialogUI {
  public:

	MacDialogUI(MacDialog * d) {
        mNative = d;
    }
	~MacDialogUI() {
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

	class MacDialog* mNative;

};

/****************************************************************************
 *                                                                          *
 *                                    MENU                                  *
 *                                                                          *
 ****************************************************************************/

/**
 * The same UI class is used for all MenuItems.
 */
class MacMenuItem : public MacComponent {

  public: 

	static int GenMenuId();

    MacMenuItem(MenuItem* item);
    ~MacMenuItem();

    Component* getComponent() {
        return mItem;
    }

	void setChecked(bool b);
	void setEnabled(bool b);
	void removeAll();

    void open();
    void close();
    void invalidateHandle();
    void openPopup(Window* window, int x, int y);

	bool isOpen();
	void opening();
	void fireSelection(int index);
	void fireSelectionById(int id);

  private:

	void getItemLabel(const char* text, char* dest);
	void openMenuBar();
	void openPopupMenu();
	void openMenu();
	void openItem();
	int getItemsInserted();
	void incItemsInserted();
	int getItemIndex(MacMenuItem* item);

    MenuItem* mItem;
    bool mOpen;
	int mItemsInserted;

	static int MenuIdFactory;

};

class MacMenuUI : public MenuUI {
  public:

	MacMenuUI(MacMenuItem* i) {
        mNative = i;
    }
	~MacMenuUI() {
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
		return true;
	}
	bool isOpen() {
		return mNative->isOpen();
	}
	void getPreferredSize(Window* w, Dimension* d) {
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

	class MacMenuItem* mNative;

};

QWIN_END_NAMESPACE
/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
