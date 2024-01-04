/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 *
 * ---------------------------------------------------------------------
 *
 * Primary include file for qwin components.
 * QwinExt.h has definitions for components built on top of the qwin core.
 *
 */

#ifndef QWIN_H
#define QWIN_H

#include <stdio.h>

#include "Util.h"
#include "List.h"
#include "Context.h"

// OSX has Component, Button, and Point so have to use namespaces
// Make this simpler!
#ifdef OSX
#define QWIN_NAMESPACE Qwin
#define QWIN_BEGIN_NAMESPACE \
  namespace Qwin             \
  {
#define QWIN_END_NAMESPACE }
#define QWIN_USE_NAMESPACE using namespace Qwin;
#else
#define QWIN_NAMESPACE
#define QWIN_BEGIN_NAMESPACE
#define QWIN_END_NAMESPACE
#define QWIN_USE_NAMESPACE
#endif

QWIN_BEGIN_NAMESPACE

/****************************************************************************
 *                                                                          *
 *   							   CLASSES                                  *
 *                                                                          *
 ****************************************************************************/
/*
 * These are abstract window classes that may be assigned to a Window
 * prior to opening.  In Windows these identify registered window classes.
 */

/**
 * Window class used root frames.
 */
#define FRAME_WINDOW_CLASS "QWIN Frame"

/**
 * Window class used for popups and other child windows.
 */
#define DIALOG_WINDOW_CLASS "QWIN Dialog"

/**
 * Window class used for borderless self-closing popup alert dialogs.
 */
#define ALERT_WINDOW_CLASS "QWIN Alert"

/**
 * Window class used for non-overlapping child windows.
 * Initially added for VST editor windows that have to
 * be placed with a host window that is out of our control.
 */
#define CHILD_WINDOW_CLASS "QWIN Child"

/**
 * Several smaller classes like Border and KeyEvent need to reference
 * the Component class before it is defined.  Just putting "class"
 * in front isn't enough on OSX, there is an ambiguity with
 * a Component class in some framework.   Have to have a declaration.
 */
class Component;

/****************************************************************************
 *                                                                          *
 *   						   WINDOW LISTENER                              *
 *                                                                          *
 ****************************************************************************/

#define WINDOW_EVENT_ACTIVATED 0
#define WINDOW_EVENT_CLOSED 1
#define WINDOW_EVENT_CLOSING 2
#define WINDOW_EVENT_DEACTIVATED 3
#define WINDOW_EVENT_DEICONIFIED 4
#define WINDOW_EVENT_ICONIFIED 5
#define WINDOW_EVENT_OPENED 6

/**
 * In Swing, this extends ComponentEvent which is also used
 * for things like hidden, moved, resized, and shown.
 */
class WindowEvent
{

public:
  WindowEvent();
  WindowEvent(class Window *window, int id);
  ~WindowEvent();

  void setWindow(Window *w);
  Window *getWindow();

  void setId(int id);
  int getId();

private:
  Window *mWindow;
  int mId;
};

class WindowListener
{

public:
  virtual void windowActivated(WindowEvent *e) = 0;
  virtual void windowClosed(WindowEvent *e) = 0;
  virtual void windowClosing(WindowEvent *e) = 0;
  virtual void windowDeactivated(WindowEvent *e) = 0;
  virtual void windowDeiconified(WindowEvent *e) = 0;
  virtual void windowIconified(WindowEvent *e) = 0;
  virtual void windowOpened(WindowEvent *e) = 0;
};

class WindowAdapter : public WindowListener
{

  virtual void windowActivated(WindowEvent *e) {}
  virtual void windowClosed(WindowEvent *e) {}
  virtual void windowClosing(WindowEvent *e) {}
  virtual void windowDeactivated(WindowEvent *e) {}
  virtual void windowDeiconified(WindowEvent *e) {}
  virtual void windowIconified(WindowEvent *e) {}
  virtual void windowOpened(WindowEvent *e) {}
};

/****************************************************************************
 *                                                                          *
 *                              ACTION LISTENER                             *
 *                                                                          *
 ****************************************************************************/

/**
 * We're dispensing with an ActionEvent model and just passing
 * a pointer to the source object
 */
class ActionListener
{

public:
  virtual void actionPerformed(void *source) = 0;
};

/**
 * Helper class to manage a list of listeners.
 */
class Listeners
{

public:
  Listeners();
  ~Listeners();

  int size();
  void addListener(void *l);
  void removeListener(void *l);

  void fireActionPerformed(void *source);
  void fireMouseEvent(class MouseEvent *e);
  void fireMouseMotionEvent(class MouseEvent *e);
  void fireKeyEvent(class KeyEvent *e);
  void fireWindowEvent(class WindowEvent *e);

private:
  List *mListeners;
};

/****************************************************************************
 *                                                                          *
 *                                 CHARACTER                                *
 *                                                                          *
 ****************************************************************************/

/**
 * Bits that may be OR'd with a base key code to represent
 * shift combinations.
 */
class Character
{

public:
  static const char *getString(int code);
  static void getString(int code, char *buffer);

  static int getCode(const char *name);

  static int translateCode(int raw);
};

/****************************************************************************
 *                                                                          *
 *                                   COLOR                                  *
 *                                                                          *
 ****************************************************************************/

/**
 * System defined colors.
 * A zero color code means that the RGB value is valid.
 */
#define COLOR_BUTTON_FACE 1

/**
 * Encode an RGB constant.  This is the format used by Windows.
 */
#define RGB_ENCODE(r, g, b) ((int)(r) | ((int)(g) << 8) | ((int)(b) << 16))

#define RGB_GET_RED(rgb) ((int)(rgb)&0xFF)
#define RGB_GET_GREEN(rgb) (((int)(rgb) >> 8) & 0xFF)
#define RGB_GET_BLUE(rgb) ((int)((rgb) >> 16) & 0xFF)

/**
 * Native color interface.
 */
class NativeColor
{

public:
  virtual ~NativeColor() {}
  /** Reflect a change in RGB value */
  virtual void setRgb(int rgb) = 0;
};

class Color
{

public:
  Color();
  Color(int rgb);
  Color(int r, int g, int b);
  Color(int code, bool system);
  ~Color();

  int getRgb();
  void setRgb(int rgb);
  int getSystemCode();

  NativeColor *getNativeColor();

  static Color *Black;
  static Color *White;
  static Color *Gray;
  static Color *Red;
  static Color *Green;
  static Color *Blue;
  static Color *ButtonFace;

private:
  void init();

  NativeColor *mHandle;
  int mRgb;
  int mSystemCode;
};

/****************************************************************************
 *                                                                          *
 *   							  DIMENSION                                 *
 *                                                                          *
 ****************************************************************************/

class Point
{

public:
  Point()
  {
    x = 0;
    y = 0;
  }
  Point(int ix, int iy)
  {
    x = ix;
    y = iy;
  }
  ~Point() {}

  int x;
  int y;
};

class Dimension
{

public:
  Dimension()
  {
    width = 0;
    height = 0;
  }
  Dimension(int w, int h)
  {
    width = w;
    height = h;
  }
  ~Dimension() {}

  int width;
  int height;
};

class Bounds : public Point, public Dimension
{

public:
  Bounds() {}
  Bounds(int ix, int iy, int w, int h)
  {
    x = ix;
    y = iy;
    width = w;
    height = h;
  }
  Bounds(Bounds *src)
  {
    x = src->x;
    y = src->y;
    width = src->width;
    height = src->height;
  }
  ~Bounds() {}
};

/**
 * Represents the insets that may be specified for a Container.
 * Unlike Swing (or maybe it's the same?) we will use the insets
 * in the LayoutManagers to shift the position of the components.
 * This means that once the components are layed out, the x/y
 * position of a child will be correct relative to the parent and the
 * parent's insets.
 */
class Insets
{

public:
  Insets()
  {
    left = 0;
    top = 0;
    right = 0;
    bottom = 0;
  }

  Insets(int ileft, int itop, int iright, int ibottom)
  {
    left = ileft;
    top = itop;
    right = iright;
    bottom = ibottom;
  }

  ~Insets() {}

  int left;
  int top;
  int right;
  int bottom;
};

/****************************************************************************
 *                                                                          *
 *   							   KEYBOARD                                 *
 *                                                                          *
 ****************************************************************************/

// NOTE: KeyEvent must come after Component on OSX to avoid ambiguity with
// a Component typedef on OSX, trying to forward reference this with a
// namespace qualifier didn't work.  Brought MouseEvent down too just so they
// can be near one another.

#define KEY_EVENT_DOWN 0
#define KEY_EVENT_UP 1

class KeyEvent
{

public:
  KeyEvent();
  ~KeyEvent();

  // swing has an "id" argument, "an integer identifying the type of event"
  // not sure what that's for
  // Swing has a "when" argument
  void init(int modifiers, int keyCode);

  // note that we have to namespace qualify the forward reference
  // to Component to avoid conflict on OSX, we could also move this
  // class definition below Component...

  class Component *getComponent();
  void setComponent(class Component *c);

  int getType();
  void setType(int i);

  int getModifiers();
  void setModifiers(int i);

  int getKeyCode();
  void setKeyCode(int i);

  int getFullKeyCode();
  bool isModifier();
  bool isToggle();

  bool isClaimed();
  void setClaimed(bool b);

  void setRepeatCount(int i);
  int getRepeatCount();

private:
  void init();

  class Component *mComponent;
  int mType;
  int mModifiers;
  int mKeyCode;
  int mRepeatCount;

  // extension/kludge
  // Any listener that sets this flag indicates that
  // they have assumed exclusive rights to this event
  // and it should not be propagated.  Introduced so we could
  // support Container managed DnD while still giving child
  // components the ability to be mouse sensitive.  Not shure how
  // Swing would do this, but it's bound to be complicated.
  bool mClaimed;
};

class KeyListener
{

public:
  virtual void keyPressed(KeyEvent *e) {}
  virtual void keyReleased(KeyEvent *e) {}
  virtual void keyTyped(KeyEvent *e) {}
};

/****************************************************************************
 *                                                                          *
 *                                   MOUSE                                  *
 *                                                                          *
 ****************************************************************************/

/**
 * Button constants.
 * Correspond to MouseEvent.*BUTTON* constants in Swing.
 */
#define MOUSE_EVENT_NOBUTTON 0
#define MOUSE_EVENT_BUTTON1 1
#define MOUSE_EVENT_BUTTON2 2
#define MOUSE_EVENT_BUTTON3 3

/**
 * Mouse event constants.
 * Correspond to MouseEvent.MOUSE_* constants in Swing.
 */
#define MOUSE_EVENT_FIRST 1
#define MOUSE_EVENT_CLICKED 1
#define MOUSE_EVENT_DRAGGED 2
#define MOUSE_EVENT_ENTERED 3
#define MOUSE_EVENT_EXITED 4
#define MOUSE_EVENT_MOVED 5
#define MOUSE_EVENT_PRESSED 6
#define MOUSE_EVENT_RELEASED 7
#define MOUSE_EVENT_WHEEL 8
#define MOUSE_EVENT_LAST 8

/**
 * The MouseEvent superclass hierarchy is quite complicated in Swing,
 * here we distill it to the essentials.
 *
 * Ignoring "popup trigger", just assume it's RMB.
 * Don't bother with event id, it will be obvious from the
 * listener method.
 */
class MouseEvent
{

public:
  MouseEvent();
  ~MouseEvent();
  void init();
  void init(int type, int button, int x, int y);

  int getType();
  void setType(int i);
  int getButton();
  void setButton(int i);
  int getClickCount();
  void setClickCount(int i);
  int getX();
  void setX(int i);
  int getY();
  void setY(int i);

  // key modifier state
  int getModifiers();
  void setModifiers(int i);
  bool isShiftDown();
  bool isControlDown();
  bool isAltDown();

  // convenience methods not in Swing
  bool isLeftButton();
  bool isRightButton();

  bool isClaimed();
  void setClaimed(bool b);

private:
  int mType;
  int mButton;
  int mModifiers;
  int mClickCount;
  int mX;
  int mY;

  // extension/kludge
  // Any listener that sets this flag indicates that
  // they have assumed exclusive rights to this event
  // and it should not be propagated.  Introduced so we could
  // support Container managed DnD while still giving child
  // components the ability to be mouse sensitive.  Not shure how
  // Swing would do this, but it's bound to be complicated.
  bool mClaimed;
};

class MouseListener
{

public:
  virtual void mouseClicked(MouseEvent *e) = 0;
  virtual void mouseEntered(MouseEvent *e) = 0;
  virtual void mouseExited(MouseEvent *e) = 0;
  virtual void mousePressed(MouseEvent *e) = 0;
  virtual void mouseReleased(MouseEvent *e) = 0;
};

class MouseMotionListener
{

public:
  virtual void mouseDragged(MouseEvent *e) = 0;
  virtual void mouseMoved(MouseEvent *e) = 0;
};

class MouseInputListener : public MouseListener, public MouseMotionListener
{
};

class MouseInputAdapter : public MouseInputListener
{

  virtual void mouseClicked(MouseEvent *e) {}
  virtual void mouseEntered(MouseEvent *e) {}
  virtual void mouseExited(MouseEvent *e) {}
  virtual void mousePressed(MouseEvent *e) {}
  virtual void mouseReleased(MouseEvent *e) {}
  virtual void mouseDragged(MouseEvent *e) {}
  virtual void mouseMoved(MouseEvent *e) {}
};

/****************************************************************************
 *                                                                          *
 *   								 FONT                                   *
 *                                                                          *
 ****************************************************************************/

#define FONT_PLAIN 0
#define FONT_BOLD 1
#define FONT_ITALIC 2
#define FONT_UNDERLINE 4
#define FONT_STRIKEOUT 8

class NativeFont
{

public:
  virtual ~NativeFont() {}

  // should just return a TextMetrics?
  virtual int getAscent() = 0;
  virtual int getHeight() = 0;
};

class Font
{

public:
  static Font *getFont(const char *name, int style, int size);
  static void exit(bool dump);
  static void dumpFonts();

  Font(const char *name, int style, int size);
  ~Font();

  NativeFont *getNativeFont();
  Font *getNext();
  const char *getName();
  int getStyle();
  int getSize();

  // extensions
  int getHeight();
  int getAscent();

private:
  static Font *Fonts;

  Font *mNext;
  NativeFont *mHandle;
  char *mName;
  int mStyle;
  int mSize;
};

/****************************************************************************
 *                                                                          *
 *   							   GRAPHICS                                 *
 *                                                                          *
 ****************************************************************************/

class TextMetrics
{

public:
  virtual ~TextMetrics() {}
  virtual int getHeight() = 0;
  virtual int getMaxWidth() = 0;
  virtual int getAverageWidth() = 0;
  virtual int getAscent() = 0;

  // !! find a common typographic name for this
  virtual int getExternalLeading() = 0;
};

class Graphics
{

public:
  virtual Color *getColor() = 0;

  virtual void save() = 0;
  virtual void restore() = 0;
  virtual void setColor(Color *c) = 0;
  virtual void setBrush(Color *c) = 0;
  virtual void setPen(Color *c) = 0;
  virtual void setFont(Font *f) = 0;
  virtual void setBackgroundColor(Color *c) = 0;
  virtual void setXORMode(Color *c) = 0;
  virtual void setXORMode() = 0;

  virtual void drawString(const char *str, int x, int y) = 0;
  virtual void drawLine(int x1, int y1, int x2, int y2) = 0;
  virtual void drawRect(int x, int y, int width, int height) = 0;
  virtual void drawRoundRect(int x, int y, int width, int height,
                             int arcWidth, int arcHeight) = 0;
  virtual void drawOval(int x, int y, int width, int height) = 0;

  virtual void fillRect(int x, int y, int width, int height) = 0;
  virtual void fillRoundRect(int x, int y, int width, int height,
                             int arcWidth, int arcHeight) = 0;
  virtual void fillOval(int x, int y, int width, int height) = 0;

  virtual void fillArc(int x, int y, int width, int height,
                       int startAngle, int arcAngle) = 0;

  // extensions

  virtual TextMetrics *getTextMetrics() = 0;
  virtual void getTextSize(const char *text, Dimension *d) = 0;
  virtual void getTextSize(const char *text, Font *font, Dimension *d) = 0;
};

/****************************************************************************
 *                                                                          *
 *                                   BORDER                                 *
 *                                                                          *
 ****************************************************************************/

// NOTE: Border must come after Component on OSX to avoid ambiguity with
// a Component typedef on OSX, trying to forward reference this with a
// namespace qualifier didn't work.

/*
 * Swing has Border and AbstractBorder, here we'll combine them.
 * Rather than BorderFactory we'll just have some static constants.
 */

class Border
{

public:
  Border();
  virtual ~Border();

  static Border *BlackLine;
  static Border *BlackLine2;
  static Border *WhiteLine;
  static Border *WhiteLine2;
  static Border *RedLine;
  static Border *RedLine2;

  void setThickness(int i);
  void setInsets(int left, int top, int right, int bottom);

  virtual Insets *getInsets(class Component *c);

  // this isn't Swing, but is more useful for C++
  virtual void getInsets(class Component *c, Insets *insets);

  virtual bool isBorderOpaque();

  virtual void paintBorder(class Component *c, Graphics *g, int x, int y,
                           int width, int height) = 0;

protected:
  Insets mInsets;
  int mThickness;
};

class LineBorder : public Border
{

public:
  LineBorder();
  LineBorder(Color *c);
  LineBorder(Color *c, int thickness);
  LineBorder(Color *c, int thickness, bool rounded);
  ~LineBorder();

  void setColor(Color *c);
  void setRoundedCorners(bool b);

  void paintBorder(class Component *c, Graphics *g,
                   int x, int y, int width, int height);

private:
  void init();

  Color *mColor;
  bool mRounded;
};

/****************************************************************************
 *                                                                          *
 *   							  COMPONENT                                 *
 *                                                                          *
 ****************************************************************************/

class Component
{

public:
  Component();
  virtual ~Component();

  virtual class ComponentUI *getUI();

  Component *getNext();
  void setNext(Component *c);
  class Container *getParent();
  void setParent(Container *c);
  void setName(const char *name);
  const char *getName();

  // a few "safe casts", if this gets out of hand may want to
  // switch to a type name to simulate instanceof

  virtual class Container *isContainer();
  virtual class Button *isButton();
  virtual class Label *isLabel();
  virtual class MenuItem *isMenuItem();
  virtual class Panel *isPanel();

  virtual void setForeground(Color *c);
  virtual Color *getForeground();
  virtual void setBackground(Color *c);
  virtual Color *getBackground();

  virtual void setEnabled(bool b);
  bool isSetEnabled();
  bool isEnabled();
  virtual void setVisible(bool b);
  bool isVisible();
  bool isSetVisible();
  void initVisibility();
  void invalidate();
  void setFocusRequested(bool b);
  bool isFocusRequested();

  int getX();
  void setX(int i);
  int getY();
  void setY(int i);
  int getWidth();
  void setWidth(int i);
  int getHeight();
  void setHeight(int i);
  void setLocation(int x, int y);
  void setSize(int width, int height);
  void setSize(Dimension *d);
  Dimension *getSize();
  void setBounds(int x, int y, int width, int height);
  void setBounds(Bounds *b);
  Bounds *getBounds();

  // Since the preferred size is cached, this should only be called
  // after you have fully constructed the hierarchy and specified all
  // the component properties, normally this is called only during
  // the layout process.

  void setPreferredSize(Dimension *d);
  void setPreferredSize(int width, int height);
  Dimension *getCurrentPreferredSize();
  virtual Dimension *getPreferredSize(class Window *w);

  void setMinimumSize(Dimension *d);
  Dimension *getMinimumSize();

  void setMaximumSize(Dimension *d);
  Dimension *getMaximumSize();

  Insets *getInsets();
  void setInsets(Insets *i);
  void setInsets(int left, int top, int right, int bottom);
  void addInsets(Dimension *d);

  Border *getBorder();
  void setBorder(Border *b);

  // returns non-null if this IS a window
  virtual class Window *isWindow()
  {
    return NULL;
  }

  // walks up to find the window
  class Window *getWindow();

  void setToolTip(const char *tip);
  const char *getToolTip();

  /**
   * Return true if this component should be included in the tab focus
   * sequence for a window.
   */
  virtual bool isFocusable();
  virtual void setFocus();

  bool isCovered(Point *p);

  // Native component utilities
  class NativeComponent *getNativeComponent();
  virtual bool isNativeParent();
  Container *getNativeParent();
  Container *getNativeParent(Component *c);
  int getWindowStyle();

  // these are used when embedding and moving native components only!
  // for custom components that draw into the Graphics use getPaintBounds
  void getNativeLocation(Point *p);
  void getNativeBounds(Bounds *b);

  // this must be used for custom components that paint themselves
  // it is a kludge for incorrect support for drawing within Mac user panes
  void getWindowLocation(Point *p);
  void getPaintBounds(Bounds *b);

  void addActionListener(ActionListener *l);
  void removeActionListener(ActionListener *l);
  void fireActionPerformed(void *o);
  void fireActionPerformed();
  Listeners *getActionListeners();

  void addMouseListener(class MouseListener *l);
  void addMouseMotionListener(class MouseMotionListener *l);
  void addKeyListener(class KeyListener *l);
  virtual Component *fireMouseEvent(MouseEvent *e);
  virtual Component *fireKeyEvent(KeyEvent *e);

  // only for the message handler, should be protected
  // !! revisit these after the UI refactoring

  virtual void open();
  virtual bool isOpen();
  virtual void *getNativeHandle();
  virtual void close();
  virtual void paint();
  virtual void paint(Graphics *g);
  virtual void paintBorder(Graphics *g);
  virtual void layout(class Window *root);

  // utilities

  void sleep(int millis);
  virtual Component *getComponent(const char *name);

  // diagnostic stucture dump, geez this feels complicated
  void indent(int indent);
  virtual void dump();
  virtual void dump(int indent);
  virtual void dumpType(int indent, const char *type);
  virtual void dumpLocal(int indent);

  // needs to be accessible by ComponentUI
  virtual void updateNativeBounds();
  virtual void invalidateNativeHandle();

  // utilities for the UI classes
  virtual bool processReturn();
  virtual bool processEscape();
  virtual void processTab();

  // trace utilities
  virtual const char *getTraceClass();
  virtual const char *getTraceName();
  void initTraceLevel();
  void incTraceLevel();
  void decTraceLevel();
  void trace(const char *msg, ...);
  void vtrace(const char *string, va_list args);
  void tracePaint();
  virtual void debug();

  static bool TraceEnabled;
  static bool PaintTraceEnabled;

protected:
  /**
   * Since we have a mixture of native and lightweight components,
   * have to convert logical positions to physical positions
   * relative to the innermost HWND when creating the native.
   */
  void getNativeLocation2(Point *p);

  /**
   * Kludge for Mac.  XCode can't always display subclass info when
   * you are in a function with a local typed as a superclass.  This
   * makes it difficult to know where you are when setting breakpoints
   * in a component hierarchy.  We'll arrange for the constructors to set
   * this to the class name so you can at least manually downcast.
   */
  const char *mClassName;

  Component *mNext;
  Container *mParent;
  Bounds mBounds;
  Border *mBorder;
  Insets *mInsets;

  Dimension *mPreferred;
  Dimension *mMinimum;
  Dimension *mMaximum;

  Color *mForeground;
  Color *mBackground;
  char *mName;
  char *mToolTip;
  bool mEnabled;
  bool mVisible;
  bool mForegroundColorChanged; // #007

  bool mFocusRequested;

  Listeners *mActionListeners;
  Listeners *mMouseListeners;
  Listeners *mMouseMotionListeners;
  Listeners *mKeyListeners;

  /**
   * So we don't get confused in subclasses we'll have a single
   * UI pointer down here and downcase in the type-specific accessors
   * in the subclasses.
   */
  ComponentUI *mUI;

  static int TraceLevel;
};

class Strut : public Component
{

public:
  Strut();
  Strut(int width, int height);
  ~Strut();

  virtual class ComponentUI *getUI();

  void setWidth(int i);
  void setHeight(int i);

  Dimension *getPreferredSize(Window *w);

private:
  Dimension *mDimension;
};

/****************************************************************************
 *                                                                          *
 *   							  CONTAINER                                 *
 *                                                                          *
 ****************************************************************************/

class Container : public Component
{

public:
  Container();
  virtual ~Container();

  Container *isContainer();
  class LayoutManager *getLayoutManager();
  void setLayout(class LayoutManager *lm);

  virtual Component *getComponents();
  virtual Component *getComponent(int index);
  virtual Component *getComponent(const char *name);
  virtual int getComponentCount();

  virtual void add(Component *c);
  virtual void add(Component *c, const char *constraints);
  virtual void remove(Component *c);
  virtual void removeAll();

  void setEnabled(bool b);
  void setVisible(bool b);

  void paint(Graphics *g);
  virtual Dimension *getPreferredSize(class Window *w);
  void layout(class Window *root);
  virtual void dump(int indent);

  virtual Component *fireMouseEvent(MouseEvent *e);
  virtual Component *fireKeyEvent(KeyEvent *e);

  virtual void open();
  virtual void close();
  virtual void invalidateNativeHandle();
  virtual void debug();

  // for use only by ComponentUI after it has created the native container
  void openChildren();

private:
  class LayoutManager *mLayoutManager;
  Component *mComponents;
};

/****************************************************************************
 *                                                                          *
 *                               LAYOUT MANAGER                             *
 *                                                                          *
 ****************************************************************************/

class LayoutManager
{

public:
  virtual ~LayoutManager() {}
  virtual void addLayoutComponent(Component *c, const char *constraints) = 0;
  virtual void removeLayoutComponent(Component *c) = 0;
  virtual void layoutContainer(Container *c, class Window *w) = 0;
  virtual Dimension *preferredLayoutSize(Container *c, class Window *w) = 0;
};

class AbstractLayoutManager : public LayoutManager
{

public:
  virtual ~AbstractLayoutManager() {}
  void addLayoutComponent(Component *c, const char *constraints) {}
  void removeLayoutComponent(Component *c) {}

  /**
   * A common adjustment layout managers must make when
   * calculating preferred size.
   */
  static void addInsets(Container *c, Dimension *d)
  {
    Insets *insets = c->getInsets();
    if (insets != NULL)
    {
      d->width += insets->left + insets->right;
      d->height += insets->top + insets->bottom;
    }
  }
};

/**
 * Not really a layout manager, just provides a static
 * preferredLayoutSize for panels that don't have layout managers
 * and want to set component location and size explicitly.
 */
class NullLayout : public AbstractLayoutManager
{

public:
  // sigh, VC8 whines if these are named the same as the ones
  // in AbstractLayoutManager
  static Dimension *nullPreferredLayoutSize(Container *c, class Window *w);
  static void nullLayoutContainer(Container *c, class Window *w);
};

/**
 * Special manager for TabbedPane.  Assume only one component is visible
 * at a time, calculate the maximum size required to display all components
 * oriented at 0,0 (adjusted by insets)
 */
class StackLayout : public AbstractLayoutManager
{

public:
  Dimension *preferredLayoutSize(Container *c, class Window *w);
  void layoutContainer(Container *c, class Window *w);
};

#define FLOW_LAYOUT_CENTER 0
#define FLOW_LAYOUT_LEFT 1
#define FLOW_LAYOUT_RIGHT 2
#define FLOW_LAYOUT_LEADING FLOW_LAYOUT_LEFT
#define FLOW_LAYOUT_TRAILING FLOW_LAYOUT_RIGHT

class FlowLayout : public AbstractLayoutManager
{

public:
  FlowLayout();
  FlowLayout(int align, int hgap, int vgap);
  FlowLayout(int align);
  ~FlowLayout();

  Dimension *preferredLayoutSize(Container *c, class Window *w);
  void layoutContainer(Container *c, class Window *w);

private:
  void initFlowLayout();
  void adjustBounds(Window *w, int left, int top,
                    int lineWidth, int lineHeight, int maxWidth,
                    Component *first, Component *last);

  int mAlign;
  int mHGap;
  int mVGap;
};

class LinearLayout : public AbstractLayoutManager
{

public:
  virtual ~LinearLayout() {}
  void setGap(int i);
  void setPreGap(int i);
  void setPostGap(int i);
  void setCenterX(bool b);
  void setCenterY(bool b);

  virtual Dimension *preferredLayoutSize(Container *c, class Window *w) = 0;
  virtual void layoutContainer(Container *c, class Window *w) = 0;

protected:
  void init();

  int mGap;
  int mPreGap;
  int mPostGap;
  bool mCenterX;
  bool mCenterY;
};

class VerticalLayout : public LinearLayout
{

public:
  VerticalLayout();
  VerticalLayout(int gap);
  ~VerticalLayout();

  Dimension *preferredLayoutSize(Container *c, class Window *w);
  void layoutContainer(Container *c, class Window *w);
};

class HorizontalLayout : public LinearLayout
{

public:
  HorizontalLayout();
  HorizontalLayout(int gap);
  ~HorizontalLayout();

  Dimension *preferredLayoutSize(Container *c, class Window *w);
  void layoutContainer(Container *c, class Window *w);
};

class GridLayout : public AbstractLayoutManager
{

public:
  GridLayout();
  GridLayout(int rows, int cols);
  ~GridLayout();

  void setCenter(bool b);
  void setGap(int i);
  void setDimension(int rows, int columns);

  Dimension *preferredLayoutSize(Container *c, class Window *w);
  void layoutContainer(Container *c, class Window *w);

private:
  int mRows;
  int mColumns;
  int mGap;
  int mCenter;
};

#define FORM_LAYOUT_LEFT 0
#define FORM_LAYOUT_RIGHT 1

class FormLayout : public AbstractLayoutManager
{

public:
  FormLayout();
  ~FormLayout();

  void setAlign(int i);
  void setVerticalGap(int i);
  void setHorizontalGap(int i);

  Dimension *preferredLayoutSize(Container *c, class Window *w);
  void layoutContainer(Container *c, class Window *w);

private:
  int mAlign;
  int mHGap;
  int mVGap;
};

#define BORDER_LAYOUT_NORTH "north"
#define BORDER_LAYOUT_SOUTH "south"
#define BORDER_LAYOUT_EAST "east"
#define BORDER_LAYOUT_WEST "west"
#define BORDER_LAYOUT_CENTER "center"

class BorderLayout : public AbstractLayoutManager
{

public:
  BorderLayout();
  ~BorderLayout();

  void addLayoutComponent(Component *c, const char *constraints);
  void removeLayoutComponent(Component *c);
  Dimension *preferredLayoutSize(Container *c, class Window *w);
  void layoutContainer(Container *c, class Window *w);

private:
  bool checkConstraint(const char *c1, const char *c2);

  Component *mNorth;
  Component *mSouth;
  Component *mEast;
  Component *mWest;
  Component *mCenter;
};

/****************************************************************************
 *                                                                          *
 *   								 MENU                                   *
 *                                                                          *
 ****************************************************************************/

/**
 * In the Swing model, a MenuItem is also an AbstractButton.
 * Keeping it simpler here until we need more.
 *
 * Windows supports "checking" menu items which will draw a little
 * checkbox next to the item text.  The items can also be configured
 * to behave like radio buttons where the checkmark will only be displayed
 * next to one item.
 *
 * I don't see anything in Swing like that, though you could implement
 * it with icons.  But that requires extra work, so we'll expose the
 * check concept.
 */
class MenuItem : public Container
{

public:
  MenuItem();
  MenuItem(const char *text);
  MenuItem(const char *text, int id);
  virtual ~MenuItem();

  virtual const char *getTraceName();

  virtual class ComponentUI *getUI();
  class MenuUI *getMenuUI();

  MenuItem *isMenuItem()
  {
    return this;
  }

  // downcast helpers so we can use the same MenuItemUI for all classes
  virtual bool isSeparator();
  virtual bool isMenuBar();
  virtual bool isMenu();
  virtual bool isPopupMenu();

  void setText(const char *text);
  const char *getText();
  void setId(int id);
  int getId();

  void setChecked(bool b);
  bool isChecked();
  void setRadio(bool b);
  bool isRadio();

  virtual void setEnabled(bool b);

  void open();
  bool fireSelectionId(int id);
  void fireSelection(MenuItem *item);

  //
  // Selection state and events
  //

  MenuItem *getSelectedItem();
  int getSelectedItemId();

protected:
  void initMenuItem();
  void setNativeState(int mask);

  bool mCreated;
  char *mText;
  int mId;
  bool mChecked;
  bool mRadio;

  // transient, the last selected item
  // left on the nearest MenuItem that had action listeners
  MenuItem *mSelectedItem;
};

// not in Swing, but a nice way to model it
class MenuSeparator : public MenuItem
{

public:
  MenuSeparator();

  virtual bool isSeparator();
};

/**
 * Interface of an object that wishes to receive notification
 * of menu events.
 *
 * Swing passes a MenuEvent object, but the only thing in interesting
 * is the source object so we'll just pass that like ActionListener.
 */
class MenuListener
{

public:
  virtual void menuSelected(class Menu *source) = 0;

  // also have these, but they're not as useful
  // virtual void menuDeselected(void src) = 0;
  // virtual void menuCanceled(void src) = 0;
};

/**
 * Represents a menu in a MenuBar or PopupMenu.
 * In the Swing model, a Menu is also a MenuItem and an AbstractButton.
 * Keeping it simpler here until we need more.
 */
class Menu : public MenuItem
{

public:
  Menu();
  Menu(const char *text);
  Menu(const char *text, int id);
  virtual ~Menu();

  bool isMenu();

  void removeAll();
  void add(Component *c);

  /**
   * Should just be "add" but having a conflict with Component::add
   */
  void addItem(const char *itemText);
  MenuItem *getItem(int index);

  void addSeparator();
  int getItemCount();

  void addMenuListener(MenuListener *l);

  void checkItem(int index);

  /**
   * Called by Windows when the menu is opening,
   * in turn calls the MenuListener.
   */
  void opening();

protected:
  void initMenu();
  MenuListener *getMenuListener();
  MenuListener *getEffectiveListener();

  // just have one of these
  MenuListener *mListener;
};

/**
 * The single menu bar that may be assigned to a Frame.
 *
 * These are Components to get some infrastructre, but they aren't
 * on the child list of the parent Window.  Probably could be, but have
 * to be careful not to mess up layout managers.
 *
 */
class MenuBar : public Menu
{

public:
  MenuBar();
  MenuBar(const char *resource);
  ~MenuBar();

  bool isMenuBar();

  int getMenuCount();
  Menu *getMenu(int index);

  void setResource(const char *name);
  const char *getResource();

protected:
  void initMenuBar();

  char *mResource;
};

/**
 * Subclass for representing popup menus.
 * With the resource implementation, the only difference is that
 * the handle will be the first submenu in the resource menu.
 */
class PopupMenu : public MenuBar
{

public:
  PopupMenu();
  PopupMenu(const char *resource);
  bool isPopupMenu();

  void open(Window *window, int x, int y);

private:
  void build();
};

/****************************************************************************
 *                                                                          *
 *   								WINDOW                                  *
 *                                                                          *
 ****************************************************************************/

/**
 * Note that the bounds of a window will be the "client area" not the
 * outer bounds including the window borders, title bar, etc.
 * This is important for HostFrame since we're given a window we can't
 * control with surrounding elements that are different for every host.
 * Saving the outer dimensions for one host will result in incorrect
 * client bounds when used for another host.
 *
 * Besides being necessary for VST plugins, it makes several calculations
 * easier and "just feels right".  I'm not sure what Swing does.
 */
class Window : public Container
{

  friend class WindowUI;

public:
  virtual const char *getTraceName();

  Window *isWindow()
  {
    return this;
  }

  virtual class Dialog *isDialog()
  {
    return NULL;
  }

  virtual class Frame *isFrame()
  {
    return NULL;
  }

  virtual bool isHostFrame()
  {
    return false;
  }

  virtual bool isOpen();
  virtual bool isRunnable();

  void setClass(const char *name);
  const char *getClass();

  void setIcon(const char *name);
  const char *getIcon();

  void setAccelerators(const char *name);
  const char *getAccelerators();

  void setMenuBar(MenuBar *m);
  MenuBar *getMenuBar();

  void setPopupMenu(PopupMenu *m);
  PopupMenu *getPopupMenu();

  void setTitle(const char *title);
  const char *getTitle();

  void setBackground(Color *c);

  void addWindowListener(WindowListener *l);
  void removeWindowListener(WindowListener *l);
  void fireWindowEvent(WindowEvent *e);

  Context *getContext();
  virtual class ComponentUI *getUI();
  virtual class WindowUI *getWindowUI();

  void open();
  int run();
  void relayout();
  void center();
  void incFocus(int delta);

  void dumpLocal(int indent);

  // These are intended only for the Component's getPreferredSize method
  // so it can obtain font size metrics.  They are transient and valid
  // only for one layout pass.

  Graphics *getGraphics();
  TextMetrics *getTextMetrics();
  void getTextSize(const char *text, Font *f, Dimension *d);
  Container *getContentPane();

  // kludge, need to handle focus properly
  void setForcedFocus(bool b);
  bool isForcedFocus();

  void setAutoSize(bool b);
  bool isAutoSize();
  void setAutoCenter(bool b);
  bool isAutoCenter();
  void setMaximized(bool b);
  bool isMaximized();
  void setMinimized(bool b);
  bool isMinimized();

  // kludge for VST plugin windows to prevent them from being closed
  void setNoClose(bool b);
  bool isNoClose();

  void setRunning(bool b);
  bool isRunning();

  // necessary for WindowsWindwUI, encapsulate better!

  void assignTabOrder(Component *c);
  Component *findFocusedComponent(Component *c);
  void setFocus(int index);
  virtual void opened();
  virtual void closing();

  // only for WindowUI
  void setTextMetrics(TextMetrics *tm);
  void finishOpening();

protected:
  Window();
  void setContext(Context *c);
  Window(Window *parent);
  Window(Window *parent, const char *title);
  virtual ~Window();

  void initWindow();
  void setupToolTips();
  void setupToolTips(Component *c);
  void mouseHandler(int msg, int keys, int x, int y);
  void keyHandler(int msg, int key, long status);

  Context *mContext;
  const char *mClass;
  TextMetrics *mTextMetrics;
  char *mTitle;
  char *mIcon;
  char *mAccelerators;
  MenuBar *mMenuBar;
  PopupMenu *mPopup;
  List *mFocusables;
  int mFocus;
  bool mForcedFocus;
  Listeners *mWindowListeners;

  // when true, the window is auto sized according to the
  // preferred sizes of its components
  bool mAutoSize;

  // when true, the window is automatically centered over its parent
  bool mAutoCenter;

  // set to true when the window is maximized
  bool mMaximized;

  // set to true when the window is minimized
  bool mMinimized;

  // if true, the window cannot be closed using the system menu
  // or X box
  bool mNoClose;

  // if true, the window has been fully opened and we're in the
  // event loop
  bool mRunning;

private:
};

/****************************************************************************
 *                                                                          *
 *   								FRAME                                   *
 *                                                                          *
 ****************************************************************************/

class Frame : public Window
{

public:
  Frame(Context *con);
  Frame(Context *con, const char *title);
  ~Frame();

  Frame *isFrame()
  {
    return this;
  }

  void dumpLocal(int indent);

protected:
  Frame();

private:
  void initFrame(Context *c);
};

/****************************************************************************
 *                                                                          *
 *   								DIALOG                                  *
 *                                                                          *
 ****************************************************************************/

class Dialog : public Window
{

public:
  Dialog();
  Dialog(Window *parent);
  Dialog(Window *parent, const char *title);
  ~Dialog();

  Dialog *isDialog()
  {
    return this;
  }

  Window *getParentWindow()
  {
    return (Window *)getParent();
  }

  virtual class ComponentUI *getUI();
  class DialogUI *getDialogUI();

  void setResource(const char *name);
  const char *getResource();
  void setModal(bool b);
  bool isModal();
  void show();

  void dumpLocal(int indent);
  void incFocus(int delta);

  void processReturn(Component *c);
  void processEscape(Component *c);

protected:
  virtual void prepareToShow();

private:
  void initDialog();
  Button *findDefaultButton(Container *c);

  char *mResource;
  bool mModal;
  Button *mDefault;
};

/****************************************************************************
 *                                                                          *
 *   							  HOST FRAME                                *
 *                                                                          *
 ****************************************************************************/

/**
 * Special class for windows that are created by a host application
 * and are beyond our control.  Initially created for VST editor windows.
 * These have a window handle that is provided at construction.
 * Bounds represents the size of the client area.
 */
class HostFrame : public Window
{

public:
  HostFrame(Context *con, void *window, void *pane, Bounds *b);
  ~HostFrame();

  bool isHostFrame();
  class ComponentUI *getUI();
  bool isRunnable();

  void *getHostWindow();
  void *getHostPane();

  void setNoBoundsCapture(bool b);
  bool isNoBoundsCapture();

private:
  void *mHostWindow;
  void *mHostPane;

  /**
   * This is a kludge to allow plugins to disable calling
   * captureNativeBounds() after we open within the host frame.
   * Mac AudioMulch resizes the window AFTER we're it opens the
   * VST editor, the initial bounds seem to be fixed at 840x420 which
   * causes the Mobius UI to render wrong.
   */
  bool mNoBoundsCapture;
};

/****************************************************************************
 *                                                                          *
 *   							 CHILD WINDOW                               *
 *                                                                          *
 ****************************************************************************/

/**
 * Special class for windows that are placed inside a window created
 * by a host application beyond our control.  Initially created
 * for VST editor windows.  The parent component of these should
 * always be a HostFrame object.
 *
 * NOTE: This is only used on Windows.  See comments in Frame.cpp
 */
class ChildWindow : public Window
{

public:
  ChildWindow(Context *con);
  ~ChildWindow();

  bool isRunnable();
  void close();
  // void updateNativeBounds();

protected:
  ChildWindow();

private:
};

/****************************************************************************
 *                                                                          *
 *   								 TEXT                                   *
 *                                                                          *
 ****************************************************************************/

class Text : public Component
{

  friend class TextAreaUI;

public:
  Text();
  Text(const char *text);
  virtual ~Text();

  virtual class ComponentUI *getUI();
  class TextUI *getTextUI();

  virtual bool isFocusable()
  {
    return true;
  }

  void setEditable(bool b);
  bool isEditable();

  void setColumns(int i);
  int getColumns();

  void setText(const char *s);
  void setValue(const char *s);
  const char *getInitialText();
  const char *getText();
  const char *getValue();
  virtual Dimension *getPreferredSize(class Window *w);
  void open();
  void dumpLocal(int indent);

protected:
  char *mText;
  int mColumns;
  bool mEditable;

private:
  void initText();
};

class TextArea : public Text
{

public:
  TextArea();
  TextArea(const char *text);
  ~TextArea();

  virtual class ComponentUI *getUI();
  class TextAreaUI *getTextAreaUI();

  void setRows(int i);
  int getRows();
  void setScrolling(bool b);
  bool isScrolling();
  bool isFocusable();

  virtual Dimension *getPreferredSize(class Window *w);
  void open();
  void dumpLocal(int indent);
  void processTab();
  bool processReturn();

private:
  void initTextArea();

  bool mScrolling;
  int mRows;
};

/****************************************************************************
 *                                                                          *
 *   							  GROUP BOX                                 *
 *                                                                          *
 ****************************************************************************/

class GroupBox : public Container
{

public:
  GroupBox();
  GroupBox(const char *text);
  ~GroupBox();

  virtual class ComponentUI *getUI();
  class GroupBoxUI *getGroupBoxUI();

  void setText(const char *s);
  const char *getText();

  Dimension *getPreferredSize(class Window *w);
  void open();
  void dumpLocal(int indent);

private:
  char *mText;
};

/****************************************************************************
 *                                                                          *
 *                                   STATIC                                 *
 *                                                                          *
 ****************************************************************************/

/**
 * Windows has several style constants for static areas:
 *   SS_BLACKRECT, SS_GRAYRECT, SS_WHITERECT, SS_BLACKFRAME,
 *   SS_GRAYFRAME, SS_WHITEFRAME, SS_ETCHEDHORZ, SS_ETCHEDVERT,
 *   SS_ETCHEDFRAME, SS_LEFT
 *
 * We're only defining a few of those since this method of creating
 * colored backgrounds isn't expected to be used in practice.
 *
 * !! Mac has nothing like this so stop using it.  Should only
 * use Panel with optional background colors.
 */
#define SS_BLACK 1
#define SS_GRAY 2
#define SS_WHITE 4

class Static : public Component
{

  friend class StaticUI;

public:
  Static();
  Static(const char *text);
  virtual ~Static();

  virtual class ComponentUI *getUI();
  class StaticUI *getStaticUI();

  void setText(const char *s);
  const char *getText();

  void setIcon(const char *name);
  bool isIcon();

  void setBitmap(const char *name);
  bool isBitmap();

  void setStyle(int style);
  int getStyle();

  void setFont(Font *f);
  Font *getFont();

  virtual Dimension *getPreferredSize(class Window *w);
  void open();
  void dumpLocal(int indent);
  void setBackground(Color *c);

protected:
  void init();

  Font *mFont;
  char *mText;
  int mStyle;
  bool mBitmap;
  bool mIcon;

private:
};

/****************************************************************************
 *                                                                          *
 *                                   PANEL                                  *
 *                                                                          *
 ****************************************************************************/

class Panel : public Container
{

public:
  Panel();
  Panel(const char *name);
  virtual ~Panel();

  class Panel *isPanel()
  {
    return this;
  }

  virtual class ComponentUI *getUI();
  class PanelUI *getPanelUI();

  void setBackground(Color *c);

  void setHeavyweight(bool b);
  bool isHeavyweight();

  bool isMouseTracking();

  virtual bool isNativeParent();

  void dumpLocal(int indent);
  void open();
  void paint(Graphics *g);

protected:
  void init();

private:
  bool mHeavyweight;
};

/****************************************************************************
 *                                                                          *
 *   								LABEL                                   *
 *                                                                          *
 ****************************************************************************/

class Label : public Static
{

public:
  Label();
  Label(const char *text);
  Label(const char *text, Color *fore);
  ~Label();

  Label *isLabel()
  {
    return this;
  }

  void setColumns(int cols);
  void setHeavyweight(bool b);
  bool isHeavyweight();
  virtual bool isNativeParent();

  Dimension *getPreferredSize(class Window *w);
  void open();
  void paint(Graphics *g);
  void dumpLocal(int indent);

private:
  void init(const char *text);

  bool mHeavyweight;
  int mColumns;
};

/****************************************************************************
 *                                                                          *
 *                                 SCROLL BAR                               *
 *                                                                          *
 ****************************************************************************/

class ScrollBar : public Component
{

  friend class ScrollBarUI;

public:
  ScrollBar();
  ScrollBar(int min, int max);
  ~ScrollBar();

  virtual class ComponentUI *getUI();
  class ScrollBarUI *getScrollBarUI();

  // must be set before the native component is created
  void setVertical(bool b);
  bool isVertical();
  void setSlider(bool b);
  bool isSlider();
  void setMinimum(int i);
  int getMinimum();
  void setMaximum(int i);
  int getMaximum();
  void setRange(int mim, int max);
  void setPageSize(int i);
  int getPageSize();
  void setValue(int i);

  int getValue();
  bool isFocusable();

  Dimension *getPreferredSize(class Window *w);
  void open();
  void updateUI();
  void dumpLocal(int indent);

  // only for the UI
  void updateValue(int i);

protected:
  bool mSlider;
  bool mVertical;
  int mMinimum;
  int mMaximum;
  int mValue;
  int mPageSize;

private:
  void initScrollBar();
};

/****************************************************************************
 *                                                                          *
 *   						   ABSTRACT BUTTON                              *
 *                                                                          *
 ****************************************************************************/

class AbstractButton : public Component
{

public:
  AbstractButton();
  AbstractButton(const char *text);
  virtual ~AbstractButton();

  const char *getTraceName();
  virtual class ButtonUI *getButtonUI() = 0;

  void setText(const char *s);
  const char *getText();
  void setFont(Font *f);
  Font *getFont();

  virtual void click();

protected:
  char *mText;
  Font *mFont;
};

/****************************************************************************
 *                                                                          *
 *   								BUTTON                                  *
 *                                                                          *
 ****************************************************************************/

class Button : public AbstractButton
{

public:
  Button();
  Button(const char *text);
  virtual ~Button();

  virtual class ComponentUI *getUI();
  class ButtonUI *getButtonUI();

  virtual bool isFocusable();
  class Button *isButton();
  void setDefault(bool b);
  bool isDefault();

  virtual Dimension *getPreferredSize(class Window *w);
  void dumpLocal(int indent);

  // Used only when the ownerdraw option is on, not Swing but
  // I want to avoid another subclass & UI class

  void setOwnerDraw(bool b);
  bool isOwnerDraw();
  void setInvisible(bool b);
  bool isInvisible();
  void setMomentary(bool b);
  bool isMomentary();
  void setToggle(bool b);
  bool isToggle();
  void setImmediate(bool b);
  bool isImmediate();
  void setTextColor(Color *c);
  Color *getTextColor();

  void setPushed(bool b);
  bool isPushed();

  void open();
  void paint(Graphics *g);

protected:
  void initButton();

  bool mDefault;
  bool mOwnerDraw;
  bool mInvisible;
  Color *mTextColor;
  bool mImmediate;
  bool mMomentary;
  bool mToggle;
  bool mPushed;
};

class InvisibleButton : public Button
{

public:
  InvisibleButton();

  bool isFocusable();

private:
};

/****************************************************************************
 *                                                                          *
 *   							 RADIOBUTTON                                *
 *                                                                          *
 ****************************************************************************/

class RadioButton : public AbstractButton
{

  friend class RadioButtonUI;

public:
  RadioButton();
  RadioButton(const char *text);
  virtual ~RadioButton();

  virtual class ComponentUI *getUI();
  class ButtonUI *getButtonUI();
  class RadioButtonUI *getRadioButtonUI();

  void setGroup(bool b);
  bool isGroup();
  void setLeftText(bool b);
  void setSelected(bool b);
  void setValue(bool b);
  bool isSelected();
  bool getValue();

  Dimension *getPreferredSize(class Window *w);
  void dumpLocal(int indent);
  void open();

protected:
  void init();

  bool mLeftText;
  bool mSelected;

  // indiciates that the button is part of a group
  // !! this is a Windows kludge, it does not apply to the
  // first button in the group, need to push this down in to the UI model
  bool mGroup;

private:
};

/****************************************************************************
 *                                                                          *
 *   								RADIOS                                  *
 *                                                                          *
 ****************************************************************************/

class Radios : public Container, public ActionListener
{

public:
  Radios();
  Radios(List *labels);
  Radios(const char **labels);
  ~Radios();

  virtual class ComponentUI *getUI();
  class RadiosUI *getRadiosUI();

  void open();

  void setLabels(List *labels);
  void setLabels(const char **labels);
  void addLabel(const char *label);
  void setVertical(bool b);

  int getSelectedIndex();
  void setSelectedIndex(int i);
  RadioButton *getSelectedButton();
  void setSelectedButton(RadioButton *selected);
  const char *getSelectedValue();
  const char *getValue();

  void actionPerformed(void *src);

private:
  void initRadios();

  GroupBox *mGroup;
};

/****************************************************************************
 *                                                                          *
 *   							   CHECKBOX                                 *
 *                                                                          *
 ****************************************************************************/

class Checkbox : public RadioButton
{

public:
  Checkbox();
  Checkbox(const char *text);
  ~Checkbox();

  virtual class ComponentUI *getUI();
  class CheckboxUI *getCheckboxUI();

  void setTriState(bool b);
  bool isTriState();

  void dumpLocal(int indent);

private:
  void init();

  bool mTriState;
};

/****************************************************************************
 *                                                                          *
 *   							   LISTBOX                                  *
 *                                                                          *
 ****************************************************************************/

class ListBox : public Component
{

  friend class ListBoxUI;

public:
  ListBox();
  ListBox(StringList *values);
  ~ListBox();

  virtual class ComponentUI *getUI();
  class ListBoxUI *getListBoxUI();

  void setRows(int i);
  int getRows();
  void setColumns(int i);
  int getColumns();
  void setAnnotationColumns(int i);
  int getAnnotationColumns();

  /**
   * JList supports three selection modes, Windows has nothing
   * directly corresponding to SINGLE_INTERVAL_SELECTION so
   * simplify mode to a boolean.
   */
  void setMultiSelect(bool b);
  bool isMultiSelect();

  void setValues(StringList *values);
  void setAnnotations(StringList *values);
  void addValue(const char *value);
  void clearSelection();
  void setSelectedIndex(int i);
  int getSelectedIndex();
  void setSelectedValue(const char *value);
  void setSelectedValues(StringList *values);
  const char *getSelectedValue();
  StringList *getValues();
  StringList *getAnnotations();
  StringList *getSelectedValues();
  char *getSelectedCsv();
  List *getSelectedIndexes();

  void deleteValue(int index);
  void moveUp(int index);
  void moveDown(int index);

  Dimension *getPreferredSize(class Window *w);
  void open();
  void paint(Graphics *g);
  void dumpLocal(int indent);

  int getPrivateSelectedIndex();
  List *getInitialSelected();

private:
  void init();
  void rebuild();

  bool mMultiSelect;
  StringList *mValues;
  StringList *mAnnotations;
  List *mSelected;
  int mRows;
  int mColumns;
  int mAnnotationColumns;
};

/****************************************************************************
 *                                                                          *
 *                                  COMBOBOX                                *
 *                                                                          *
 ****************************************************************************/

class ComboBox : public Component
{

  friend class ComboBoxUI;

public:
  ComboBox();
  ComboBox(StringList *values);
  ComboBox(const char **values);
  ~ComboBox();
  virtual class ComponentUI *getUI();
  class ComboBoxUI *getComboBoxUI();

  void setEditable(bool b);
  bool isEditable();
  void setRows(int i);
  int getRows();
  void setColumns(int i);
  int getColumns();

  void setValues(StringList *values);
  void addValue(const char *value);
  StringList *getValues();

  void setSelectedIndex(int i);
  int getSelectedIndex();
  void setSelectedValue(const char *value);
  void setValue(const char *value);
  void setValue(int i);
  const char *getSelectedValue();
  const char *getValue();

  Dimension *getPreferredSize(class Window *w);
  void open();
  void dumpLocal(int indent);

private:
  void initComboBox();

  // todo: allow an initial value that isn't on the select list,
  // makes sense only if this is editable

  StringList *mValues;
  char *mValue;
  bool mEditable;
  int mRows;
  int mColumns;
  int mSelected;
};

/****************************************************************************
 *                                                                          *
 *                                  TOOLBAR                                 *
 *                                                                          *
 ****************************************************************************/

class ToolBar : public Component
{

public:
  ToolBar();
  ToolBar(StringList *values);
  ~ToolBar();

  virtual class ComponentUI *getUI();
  class ToolBarUI *getToolBarUI();

  void addIcon(const char *name);

  Dimension *getPreferredSize(class Window *w);
  void open();
  void dumpLocal(int indent);

private:
  void initToolBar();

  StringList *mIcons;
};

/****************************************************************************
 *                                                                          *
 *                                 STATUS BAR                               *
 *                                                                          *
 ****************************************************************************/

class StatusBar : public Component
{

public:
  StatusBar();
  ~StatusBar();

  virtual class ComponentUI *getUI();
  class StatusBarUI *getStatusBarUI();

  Dimension *getPreferredSize(class Window *w);
  void open();
  void dumpLocal(int indent);

private:
  void initStatusBar();
};

/****************************************************************************
 *                                                                          *
 *                                TABBED PANE                               *
 *                                                                          *
 ****************************************************************************/

/**
 * Note that even though we will have an mHandle (on Windows),
 * isNativeParent() returns false so we're not considered
 * a native parent for any of the child containers.
 */
class TabbedPane : public Container
{

public:
  TabbedPane();
  ~TabbedPane();

  virtual class ComponentUI *getUI();
  class TabbedPaneUI *getTabbedPaneUI();

  int getTabCount();
  int getSelectedIndex();
  Component *getSelectedComponent();
  void setSelectedComponent(Component *c);
  void setSelectedIndex(int index);

  void dumpLocal(int indent);
  Dimension *getPreferredSize(class Window *w);
  Dimension *getCurrentPreferredSize();
  void open();
  void paint(Graphics *g);

  // Kludge for Windows
  int getSelectedIndexNoRefresh();

private:
  int mSelected;
};

/****************************************************************************
 *                                                                          *
 *                                   TABLE                                  *
 *                                                                          *
 ****************************************************************************/

/**
 * Roughly equivalent to the interfce javax.swing.table.TableModel.
 *
 * We don't have the notions of "column class" or editable cells.
 * Also no dynamic adding and deleting rows/columns so we don't
 * need TableModelListener.
 *
 * Cells have to be strings.  Thought about introducing a TableCell
 * interface to encapsulate some of the rendering options but it's
 * simpler just to define them here.  Revisit this if we add too many
 * cell methods to this interface.
 *
 * The most flexible thing would be to have getCell() returna
 * a Component which could be a Label with all the trimmings.
 * But this is inconvenient for the usual case of a simple table
 * of strings with font/color variations.
 */
class TableModel
{

public:
  virtual ~TableModel() {}

  /**
   * Return the number of columns.
   */
  virtual int getColumnCount() = 0;

  /**
   * Return the name of the a column.
   */
  virtual const char *getColumnName(int index) = 0;

  /**
   * Return the preferred width of a column.
   * If zero, the size will be calculated based on the name.
   * (not in Swing).
   */
  virtual int getColumnPreferredWidth(int index) = 0;

  virtual Font *getColumnFont(int index) = 0;
  virtual Color *getColumnForeground(int index) = 0;
  virtual Color *getColumnBackground(int index) = 0;

  /**
   * Return the number of rows.
   */
  virtual int getRowCount() = 0;

  /**
   * Return the text in a cell.
   */
  virtual const char *getCellText(int row, int column) = 0;

  /**
   * Return the font used to render cell text.
   */
  virtual Font *getCellFont(int row, int column) = 0;

  /**
   * Return the colors to render cell text.
   */
  virtual Color *getCellForeground(int row, int colunn) = 0;
  virtual Color *getCellBackground(int row, int colunn) = 0;
  virtual Color *getCellHighlight(int row, int colunn) = 0;
};

/**
 * Implentation of TableModel with common default methods.
 */
class AbstractTableModel : public TableModel
{

public:
  virtual ~AbstractTableModel() {}

  virtual int getColumnPreferredWidth(int index);
  virtual Font *getColumnFont(int index);
  virtual Color *getColumnForeground(int index);
  virtual Color *getColumnBackground(int index);

  virtual Font *getCellFont(int row, int column);
  virtual Color *getCellForeground(int row, int colunn);
  virtual Color *getCellBackground(int row, int colunn);
  virtual Color *getCellHighlight(int row, int colunn);
};

/**
 * Implentation of TableModel if you just need a grid of
 * strings.
 */
class SimpleTableModel : public AbstractTableModel
{

public:
  SimpleTableModel();
  ~SimpleTableModel();

  int getColumnCount();
  const char *getColumnName(int index);

  int getRowCount();
  const char *getCellText(int row, int column);

  // extensions

  void setColumns(StringList *cols);
  void addRow(StringList *row);

private:
  StringList *mColumns;
  List *mRows;
};

/**
 * Represents a table column in GeneralTableModel.
 */
class GeneralTableColumn
{

public:
  GeneralTableColumn();
  GeneralTableColumn(const char *text);
  ~GeneralTableColumn();

  const char *getText();
  void setText(const char *s);

  int getWidth();
  void setWidth(int i);

  Font *getFont();
  void setFont(Font *f);
  Color *getForeground();
  void setForeground(Color *c);
  Color *getBackground();
  void setBackground(Color *c);

private:
  void init();

  char *mText;
  int mWidth;
  Font *mFont;
  Color *mForeground;
  Color *mBackground;
};

/**
 * Represents a table cell in GeneralTableModel.
 */
class GeneralTableCell
{

public:
  GeneralTableCell();
  GeneralTableCell(const char *text);
  ~GeneralTableCell();

  const char *getText();
  void setText(const char *s);

  Font *getFont();
  void setFont(Font *f);

  Color *getForeground();
  void setForeground(Color *c);

  Color *getBackground();
  void setBackground(Color *c);

  Color *getHighlight();
  void setHighlight(Color *c);

private:
  void init();

  char *mText;
  Font *mFont;
  Color *mForeground;
  Color *mBackground;
  Color *mHighlight;
};

/**
 * Implentation of TableModel allowing colors and fonts.
 */
class GeneralTableModel : public TableModel
{

public:
  GeneralTableModel();
  ~GeneralTableModel();

  // TableModel

  int getColumnCount();
  const char *getColumnName(int index);
  int getColumnPreferredWidth(int index);
  Font *getColumnFont(int index);
  Color *getColumnForeground(int index);
  Color *getColumnBackground(int index);

  int getRowCount();
  const char *getCellText(int row, int column);
  Font *getCellFont(int row, int column);
  Color *getCellForeground(int row, int colunn);
  Color *getCellBackground(int row, int colunn);
  Color *getCellHighlight(int row, int colunn);

  // extensions

  void setColumns(StringList *cols);
  // must be List<GeneralTableColumn>
  void setColumns(List *cols);
  void addColumn(GeneralTableColumn *col);
  void addColumn(const char *text);
  void addColumn(const char *text, int width);

  // must be List<GeneralTableCell>
  void addRow(List *row);
  void addCell(GeneralTableCell *cell, int row, int col);
  void addCell(const char *text, int row, int col);

  void setColumnFont(Font *f);
  void setColumnForeground(Color *c);
  void setColumnBackground(Color *c);

  void setCellFont(Font *f);
  void setCellForeground(Color *c);
  void setCellBackground(Color *c);
  void setCellHighlight(Color *c);

private:
  void deleteColumns();
  void deleteRows();

  GeneralTableColumn *getColumn(int i);
  List *getRow(int i);
  GeneralTableCell *getCell(int row, int col);

  List *mColumns;
  List *mRows;

  Font *mColumnFont;
  Color *mColumnForeground;
  Color *mColumnBackground;

  Font *mCellFont;
  Color *mCellForeground;
  Color *mCellBackground;
  Color *mCellHighlight;
};

/**
 * The table component.
 * Outwardy these are similar to ListBox but with multiple columns.
 */
class Table : public Component
{

  friend class TableUI;

public:
  Table();
  Table(TableModel *model);
  ~Table();

  virtual class ComponentUI *getUI();
  class TableUI *getTableUI();

  void setModel(TableModel *m);
  TableModel *getModel();
  void rebuild();

  void setVisibleRows(int i);
  int getVisibleRows();

  /**
   * JList supports three selection modes, Windows has nothing
   * directly corresponding to SINGLE_INTERVAL_SELECTION so
   * simplify mode to a boolean.
   */
  void setMultiSelect(bool b);
  bool isMultiSelect();

  void clearSelection();
  void setSelectedIndex(int i);
  int getSelectedIndex();
  List *getInitialSelected();

  void moveUp(int index);
  void moveDown(int index);

  Dimension *getPreferredSize(class Window *w);
  void open();
  void paint(Graphics *g);
  void dumpLocal(int indent);

private:
  void init();

  TableModel *mModel;
  bool mMultiSelect;
  int mVisibleRows;
  List *mSelected;
};

/****************************************************************************
 *                                                                          *
 *                                    TREE                                  *
 *                                                                          *
 ****************************************************************************/

class Tree : public Container
{

public:
  Tree();
  ~Tree();

  virtual class ComponentUI *getUI();
  class TreeUI *getTreeUI();

  void dumpLocal(int indent);
  Dimension *getPreferredSize(class Window *w);
  void open();

private:
};

/****************************************************************************
 *                                                                          *
 *                               SYSTEM DIALOGS                             *
 *                                                                          *
 ****************************************************************************/

class SystemDialog
{

public:
  SystemDialog(Window *parent);
  virtual ~SystemDialog();

  Window *getParent();
  void setTitle(const char *title);
  const char *getTitle();

  void setCanceled(bool b);
  bool isCanceled();

  virtual bool show() = 0;

protected:
  Window *mParent;
  char *mTitle;
  bool mCanceled;
};

#define OPEN_DIALOG_MAX_FILE 1024

class OpenDialog : public SystemDialog
{

public:
  OpenDialog(Window *parent);
  ~OpenDialog();

  void setFilter(const char *filter);
  const char *getFilter();
  void setInitialDirectory(const char *dir);
  const char *getInitialDirectory();
  void setSave(bool b);
  bool isSave();
  void setAllowDirectories(bool b);
  bool isAllowDirectories();
  void setAllowMultiple(bool b);
  bool isAllowMultiple();
  void setFile(const char *file);
  const char *getFile();

  bool show();

private:
  char *mFilter;
  char *mInitialDirectory;
  char *mFile;

  // !! need StringList for multiple files

  /**
   * True if this is a save file dialog.
   */
  bool mSave;

  /**
   * True if we can select either files or directories.
   * When this is on, mSave is irrelevant.
   */
  bool mAllowDirectories;

  /**
   * True if we can select more than one file.
   */
  bool mAllowMultiple;
};

class ColorDialog : public SystemDialog
{

public:
  ColorDialog(Window *parent);
  ~ColorDialog();

  void setRgb(int rgb);
  int getRgb();

  bool show();

private:
  int mRgb;
};

class MessageDialog : public SystemDialog
{

public:
  MessageDialog(Window *parent);
  MessageDialog(Window *parent, const char *title, const char *text);
  ~MessageDialog();

  void setText(const char *text);
  const char *getText();

  /**
   * When true, a cancel button will be added.  Otherwise
   * there will be a single Ok button.
   */
  void setCancelable(bool b);
  bool isCancelable();

  /**
   * When true, an "information" icon will display rather
   * than the exclamation icon.
   */
  void setInformational(bool b);
  bool isInformational();

  bool show();

  static void showError(Window *parent, const char *title, const char *text);
  static void showMessage(Window *parent, const char *title, const char *text);

private:
  void init();

  char *mText;
  bool mCancelable;
  bool mInfo;
};

/****************************************************************************
 *                                                                          *
 *   								TIMER                                   *
 *                                                                          *
 ****************************************************************************/

/**
 * Native timer interface.
 *
 * This accomplishes nothing, get rid of it!
 */
class NativeTimer
{

public:
  virtual ~NativeTimer() {}
};

#define MAX_TIMERS 4

/**
 * A partial implementation of the javax.swing.Timer class.
 * Don't bother with initial delay, logging, coalescing, and repeats.
 *
 * Sigh, the midi library already defines a Timer class so we have
 * to change our name.
 *
 * The delay unit is milliseconds.
 */
class SimpleTimer
{

  friend class NativeTimer;

public:
  SimpleTimer(int delay);
  SimpleTimer(int delay, ActionListener *l);
  ~SimpleTimer();

  NativeTimer *getNativeTimer();

  void addActionListener(ActionListener *l);
  void removeActionListener(ActionListener *l);

  int getId();
  int getDelay();
  void setDelay(int delay);

  void start();
  void stop();
  bool isRunning();

  void fireActionPerformed();

  // Since we can't pass an argument through to the callback
  // function, will have to search for Timer objects by id.
  // I don't like this, but we normally only have one of these.
  // !! THIS APPLIES TO WINDOWS ONLY
  static SimpleTimer *Timers[MAX_TIMERS];
  static int TimerCount;

  static bool KludgeTraceTimer;

private:
  void init(int delay);

  class NativeTimer *mNativeTimer;
  int mId;
  bool mRunning;
  int mDelay;
  Listeners *mListeners;
};

/****************************************************************************
 *                                                                          *
 *   								 QWIN                                   *
 *                                                                          *
 ****************************************************************************/

/**
 * Global utility functions.
 */
class Qwin
{

public:
  static void csectEnter();
  static void csectLeave();
  static void exit(bool dump);

private:
};

/**
 * Namespaces made using Qwin::exit ambiguous, provide some functions.
 */
PUBLIC void QwinExit(bool dump);

QWIN_END_NAMESPACE

#endif
