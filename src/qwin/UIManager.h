/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 */

#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include <stdio.h>

#include "Util.h"
#include "Qwin.h"

QWIN_BEGIN_NAMESPACE

/****************************************************************************
 *                                                                          *
 *                                COMPONENT UI                              *
 *                                                                          *
 ****************************************************************************/

/**
 * Abstract handle to a native component that implements a UI interface.
 * I REALLY tried not to do this and just have the implementation classes
 * implement the UI interfaces, but just couldn't make multiple inheritance
 * work on Windows.  See comments at the top of UIWindows.
 */
class NativeComponent {
  public:

	virtual void* getHandle() = 0;
};

/** 
 * Interface of an object that will implement the OS specific 
 * aspects of a component.  Similar to Swing, but I'm not making
 * any attempt to keep the interfaces the same at this level.  End-users
 * rarely touch these so Swing consistency is less of an issue and allows
 * us to make a number of simplifications.
 */
class ComponentUI {

 public:

	/**
	 * NOTE: I don't fully understand why, but it is vital that this
	 * virtual destructor be defined in MSVC 6.0.  If not, then you will
	 * always crash deleting any subclass implementation.  Purify
	 * shows "Freeing invalid memory".
	 */
	virtual ~ComponentUI(){}

    /**
     * Create the native component object.
     */
    virtual void open() = 0;

	/**
	 * Make adjustments after all the child components have been opened.
	 */
	virtual void postOpen() = 0;

    /**
     * Determine the preferred size of the component after opening.
     */
	virtual void getPreferredSize(Window* w, Dimension* d) = 0;

	/**
	 * Invalidate the component's display region so it will be repainted.
	 * The component will be either the peer to the native component
	 * or a lightweight child of the native component.
	 * !! The name comes from Swing, but I don't like it, requestRepaint
	 * would be more obvious.
	 */
	virtual void invalidate(Component* c) = 0;

    /**
     * Paint the component.
     */
    virtual void paint(Graphics* g) = 0;

	/**
	 * Close the native component handle.
	 */
	virtual void close() = 0;

    /**
     * Forget about the native handle.
     */
    virtual void invalidateHandle() = 0;

    /**
     * Change native bounds to reflect Component.Bounds change.
     */
    virtual void updateBounds() = 0;

	/**
	 * Enable or disable the component.
	 */
	virtual void setEnabled(bool b) = 0;

	/**
	 * Return true if the native component handle is a parent to 
	 * child components.
	 */
	virtual bool isNativeParent() = 0;

	/**
	 * Return true if the native component is open.
	 */
	virtual bool isOpen() = 0;

	/**
	 * Return true if the component is enabled.
	 * Do we really need this? Can't we keep track in the Component?
	 */
	virtual bool isEnabled() = 0;

	/**
	 * Make the component visible or invisible.
	 */
	virtual void setVisible(bool b) = 0;

	/**
	 * Return true if the component is visible.
	 * Do we really need this? Can't we keep track in the Component?
	 */
	virtual bool isVisible() = 0;

	/**
	 * Request keyboard focus.
	 */
	virtual void setFocus() = 0;

    virtual NativeComponent* getNative() = 0;

	// dump random debugging info
	virtual void debug() = 0;


};

/****************************************************************************
 *                                                                          *
 *   								NULL                                    *
 *                                                                          *
 ****************************************************************************/

/**
 * Special UI implementation for components that have no visible 
 * parts or lightweight components that paint themselves.
 */
class NullUI : public ComponentUI {

	/**
	 * This is the one method that isn't a stub so we can walk
	 * up and find a native parent to handle the invalidation.
	 */
    void invalidate(Component* c);

    void open() {}
    void postOpen() {}
    void getPreferredSize(Window* w, Dimension* d) {}
    void paint(Graphics* g) {}
    void close() {}
    void invalidateHandle() {}
    void updateBounds() {}
	void setEnabled(bool b) {}
	void setVisible(bool b) {}
	void setFocus() {}
	void debug() {}

	bool isNativeParent() {
        return false;
    }

	bool isOpen() {
        return false;
    }

	bool isEnabled() {
        return true;
    }

	bool isVisible() {
        return true;
    }

    NativeComponent* getNative() {
        return NULL;
    }

};

/****************************************************************************
 *                                                                          *
 *   								STATIC                                  *
 *                                                                          *
 ****************************************************************************/

class StaticUI : public ComponentUI {

  public:

	virtual void setText(const char* s) = 0;
	virtual void setBitmap(const char* s) = 0;
	virtual void setIcon(const char* s) = 0;
};

class PanelUI : public ComponentUI {

  public:

};

/****************************************************************************
 *                                                                          *
 *   								BUTTON                                  *
 *                                                                          *
 ****************************************************************************/

class ButtonUI : public ComponentUI {

  public:

    virtual void setText(const char* text) = 0;
	virtual void click() = 0;

};

/****************************************************************************
 *                                                                          *
 *   							 RADIO BUTTON                               *
 *                                                                          *
 ****************************************************************************/

class RadioButtonUI : public ButtonUI {

  public:

    virtual void setSelected(bool b) = 0;
	virtual bool isSelected() = 0;

};

class RadiosUI : public ComponentUI {

  public:

	virtual void changeSelection(RadioButton* b) = 0;

};

/****************************************************************************
 *                                                                          *
 *   							   CHECKBOX                                 *
 *                                                                          *
 ****************************************************************************/

class CheckboxUI : public RadioButtonUI {

  public:

};

/****************************************************************************
 *                                                                          *
 *   							  COMBO BOX                                 *
 *                                                                          *
 ****************************************************************************/

class ComboBoxUI : public ComponentUI {

  public:

	virtual void setValues(StringList* values) = 0;
	virtual void addValue(const char* value) = 0;
	virtual void setSelectedIndex(int i) = 0;
	virtual void setSelectedValue(const char* value) = 0;
	virtual int getSelectedIndex() = 0;
	virtual char* getSelectedValue() = 0;
};

/****************************************************************************
 *                                                                          *
 *   							   LIST BOX                                 *
 *                                                                          *
 ****************************************************************************/

class ListBoxUI : public ComponentUI {

  public:

	virtual void setValues(StringList* values) = 0;
	virtual void addValue(const char* value) = 0;
	virtual void setAnnotations(StringList* values) = 0;

	virtual void setSelectedIndex(int i) = 0;
	virtual int getSelectedIndex() = 0;
	virtual bool isSelected(int i) = 0;
};

/****************************************************************************
 *                                                                          *
 *                                    TEXT                                  *
 *                                                                          *
 ****************************************************************************/

class TextUI : public ComponentUI {

  public:

	virtual void setEditable(bool b) = 0;
    virtual void setText(const char* s) = 0;
    virtual char* getText() = 0;
};

class TextAreaUI : public TextUI {

  public:

};

/****************************************************************************
 *                                                                          *
 *   							  GROUP BOX                                 *
 *                                                                          *
 ****************************************************************************/

class GroupBoxUI : public ComponentUI {

  public:

	virtual void setText(const char* s) = 0;

};

/****************************************************************************
 *                                                                          *
 *   							 TABBED PANE                                *
 *                                                                          *
 ****************************************************************************/

class TabbedPaneUI : public ComponentUI {

  public:
	
	virtual void setSelectedIndex(int i) = 0;
    virtual int getSelectedIndex() = 0;

};

/****************************************************************************
 *                                                                          *
 *                                   TABLE                                  *
 *                                                                          *
 ****************************************************************************/

class TableUI : public ComponentUI {

  public:

    virtual void rebuild() = 0;
	virtual void setSelectedIndex(int i) = 0;
	virtual int getSelectedIndex() = 0;
	virtual bool isSelected(int i) = 0;
    virtual List* getColumnWidths(Window* w) = 0;
};

/****************************************************************************
 *                                                                          *
 *   								 TREE                                   *
 *                                                                          *
 ****************************************************************************/

class TreeUI : public ComponentUI {

  public:
	
	// nothing yet, but will be

};

/****************************************************************************
 *                                                                          *
 *   							  SCROLL BAR                                *
 *                                                                          *
 ****************************************************************************/

class ScrollBarUI : public ComponentUI {

  public:

	virtual void update() = 0;

};

/****************************************************************************
 *                                                                          *
 *                                   WINDOW                                 *
 *                                                                          *
 ****************************************************************************/

class WindowUI : public ComponentUI {

  public:

    virtual Graphics* getGraphics() = 0;

    virtual int run() = 0;
    virtual void relayout() = 0;
	virtual bool isChild() = 0;
	virtual void toFront() = 0;
    virtual void setBackground(Color* c) = 0;

};

//////////////////////////////////////////////////////////////////////
//
// HostFrame
//
//////////////////////////////////////////////////////////////////////

class HostFrameUI : public WindowUI {

  public:


};

/****************************************************************************
 *                                                                          *
 *                                   DIALOG                                 *
 *                                                                          *
 ****************************************************************************/

class DialogUI : public WindowUI {

  public:

    virtual void show() = 0;
};

/****************************************************************************
 *                                                                          *
 *                                    MENU                                  *
 *                                                                          *
 ****************************************************************************/

/**
 * Interface of the ComponentUI for menus.
 */
class MenuUI : public ComponentUI {

  public:

	virtual void setChecked(bool b) = 0;

	// !! get rid of this
	virtual void removeAll() = 0;

    /**
     * Popup menus can be opened with a position.
     */
    virtual void openPopup(Window* window, int x, int y) = 0;

};

/****************************************************************************
 *                                                                          *
 *                                    MISC                                  *
 *                                                                          *
 ****************************************************************************/

class ToolBarUI : public ComponentUI {

  public:

};

class StatusBarUI : public ComponentUI {

  public:

};

/****************************************************************************
 *                                                                          *
 *   							SYSTEM DIALOGS                              *
 *                                                                          *
 ****************************************************************************/
/*
 * Interface of classes that encapsulate standard system dialogs.
 * These aren't part of the ComponentUI hierarchy.  They're transient
 * objects that are expected to prompt the user and not return until
 * a choice has been made or the cancel button is pressed.
 */

class SystemDialogUI {

  public:

	virtual ~SystemDialogUI(){}
	virtual void show() = 0;

};

/****************************************************************************
 *                                                                          *
 *                                 UI MANAGER                               *
 *                                                                          *
 ****************************************************************************/

/** 
 * Factory class for ComponentUI objects.  In Swing this is also
 * the home to collections of borders, colors, fonts, and icons that
 * may change for different look-and-feels.  
 *
 * May be a good place to encapsulate other things like
 * window class registration?
 *
 * In theory could assign UIFactory objects dynamically to achieve
 * something similar to PLAF, but for simple OS porting we can select
 * an implementation at compile time.
 */
class UIManager {

 public:

	//
	// UI factories
	//

	static SystemDialogUI* getOpenDialogUI(OpenDialog* od);
	static SystemDialogUI* getColorDialogUI(ColorDialog* od);
	static SystemDialogUI* getMessageDialogUI(MessageDialog* md);

	static NullUI* getNullUI();
	static StaticUI* getStaticUI(Static* s);
	static PanelUI* getPanelUI(Panel* p);
	static ButtonUI* getButtonUI(Button* b);
	static TextUI* getTextUI(Text* b);
	static TextAreaUI* getTextAreaUI(TextArea* b);
	static RadioButtonUI* getRadioButtonUI(RadioButton* rb);
	static RadiosUI* getRadiosUI(Radios* r);
	static CheckboxUI* getCheckboxUI(Checkbox* cb);
	static ComboBoxUI* getComboBoxUI(ComboBox* cb);
	static ListBoxUI* getListBoxUI(ListBox* lb);
	static GroupBoxUI* getGroupBoxUI(GroupBox* gb);
	static ToolBarUI* getToolBarUI(ToolBar* tb);
	static StatusBarUI* getStatusBarUI(StatusBar* sb);
	static TabbedPaneUI* getTabbedPaneUI(TabbedPane* tp);
	static TableUI* getTableUI(Table* t);
	static TreeUI* getTreeUI(Tree* t);
	static ScrollBarUI* getScrollBarUI(ScrollBar* sb);
	static WindowUI* getWindowUI(Window* w);
	static HostFrameUI* getHostFrameUI(HostFrame* f);
	static DialogUI* getDialogUI(Dialog* d);
	static MenuUI* getMenuUI(MenuItem* item);

	/**
	 * Kludge for Mac, returns true if all paint() methods
	 * should use component origins relative to the window 
	 * rather than the nearest component whose isNativeParent
	 * method reutrns true.  This is because we're not doing
	 * drawing into user panes correctly.  Panels turn into user panes
	 * which need isNativeParent=true to embed native controls like buttons.
	 * But we're not managing the Graphics properly to allow painting
	 * into the user pane, painting is always done into the QGContext of the window.
	 * Should fix this someday, but it doesn't appear to hurt anything.
	 */
	static bool isPaintWindowRelative();

	//
	// System Interfaces
	//

	/**
	 * Pause for some number of milliseconds.
	 */
	static void sleep(int millis);

	/**
	 * Return the RGB value for a system color.
	 */
	static int getSystemRgb(int code);

	/**
	 * Get a handle to a native color object.
	 */
	static NativeColor* getColor(Color* c);

	/**
	 * Get a handle to a native font object.
	 */
	static NativeFont* getFont(Font* c);

    static NativeTimer* getTimer(SimpleTimer* t);

	static bool isKeyDown(int code);
    static int getScreenWidth();
    static int getScreenHeight();

	static int getVertScrollBarWidth();
	static int getVertScrollBarHeight();
	static int getHorizScrollBarWidth();
	static int getHorizScrollBarHeight();

};

QWIN_END_NAMESPACE
/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
