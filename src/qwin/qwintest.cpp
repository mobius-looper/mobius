/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * The standard cross-platform test application.
 * wintest and mactest may have other platform specific things.
 *
 * MAC NOTES
 *
 * Static icons
 *   - not working, need
 * 
 * Tree
 *   - not implemented, ignore
 *   
 * GroupBox
 *   - not implemented, could do
 * 
 * Timer
 *   - not working, could avoid
 * 
 * WINDOWS NOTES
 * 
 */

#include <stdio.h>
#include <memory.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "Trace.h"
#include "XomParser.h"

#include "QwinExt.h"
#include "Palette.h"
//#include "UIManager.h"

QWIN_USE_NAMESPACE

//////////////////////////////////////////////////////////////////////
//
// Menu constants
//
//////////////////////////////////////////////////////////////////////

#define IDM_MIDI 4
#define IDM_AUDIO 5
#define IDM_ABOUT 6

#define IDM_WHITE 7
#define IDM_GRAY 8
#define IDM_BLACK 9

#define IDM_DIALOG 10
#define IDM_MODELESS_DIALOG 11
#define IDM_MESSAGE 14
#define IDM_OPENDIALOG 12
#define IDM_SAVEDIALOG 13
#define IDM_COLORDIALOG 15
#define IDM_PALETTEDIALOG 16
#define IDM_PACKED_DIALOG 17

//////////////////////////////////////////////////////////////////////
//
// Modal Dialog
//
//////////////////////////////////////////////////////////////////////

class TestDialog : public SimpleDialog {

  public:

	TestDialog(Window* parent);
	~TestDialog();
	const char* getValue();
	void closing();

  private:
	
	Radios* mRadio;
	char* mValue;
};

TestDialog::TestDialog(Window* parent)
{
	setParent(parent);
    setModal(true);
	setTitle("Dialog Window");
	
	// give it some girth so we don't pack to the minimum
	setWidth(500);
	setHeight(300);

    Panel* root = getPanel();
	root->setLayout(new BorderLayout());

	StringList* labels = new StringList();
	labels->add("this");
	labels->add("that");
	labels->add("the other");
	mRadio = new Radios(labels);
	root->add(mRadio, BORDER_LAYOUT_CENTER);
}

TestDialog::~TestDialog()
{
	delete mValue;
}

/**
 * Overload to capture the final radio button
 * value before we close the native handles.
 */
void TestDialog::closing()
{
	mValue = CopyString(mRadio->getValue());
}

const char* TestDialog::getValue()
{
	return mValue;
}

//////////////////////////////////////////////////////////////////////
//
// Modeless Dialog
//
//////////////////////////////////////////////////////////////////////

class ModelessDialog : public SimpleDialog {

  public:
    ModelessDialog(Window* p);
	void closing();

  private:

};

ModelessDialog::ModelessDialog(Window* parent)
{
	setParent(parent);
    setModal(false);
	setTitle("Modeless Dialog");

	// give it some girth to prevent packing
    setSize(200, 100);

    Panel* p = getPanel();
    p->add(new Label("A modless dialog"), BORDER_LAYOUT_CENTER);
}

void ModelessDialog::closing()
{
	if (isCanceled())
	  printf("qwintest: Modeless dialog canceled\n");
	else
	  printf("qwintest: Modeless dialog closed\n");
    fflush(stdout);
}

//////////////////////////////////////////////////////////////////////
//
// Packed Dialog
//
//////////////////////////////////////////////////////////////////////

class PackedDialog : public SimpleDialog {

  public:

	PackedDialog(Window* parent);
	~PackedDialog();

  private:
	
	ListBox* mList;
};

PackedDialog::PackedDialog(Window* parent)
{
	setParent(parent);
    setModal(true);
	setTitle("Dialog Window");
	
    Panel* root = getPanel();
	/*
	Panel* p1 = new Panel();
	p1->setName("Assignments");
	root->add(p1);
	p1->setLayout(new HorizontalLayout(10));

	Panel* p2 = new Panel();
	p1->add(p2);
	p2->setLayout(new VerticalLayout());
	p2->add(new Label("Things"));
	*/

	char buffer[128];
	StringList* labels = new StringList();
	for (int i = 0 ; i < 40 ; i++) {
		sprintf(buffer, "%d", i);
		labels->add(buffer);
	}
	
	mList = new ListBox();
	mList->setRows(20);
	mList->setValues(labels);
	//p2->add(mList);
	root->add(mList);
}

PackedDialog::~PackedDialog()
{
}

//////////////////////////////////////////////////////////////////////
//
// Menu Bar
//
//////////////////////////////////////////////////////////////////////

/**
 * Menu class with handler method.
 */
class TestMenu : public MenuBar, public ActionListener
{
  public:
	
	TestMenu() {
		init();
	}
	~TestMenu() {}

	void init();
	void actionPerformed(void* src);

};

void TestMenu::init()
{
	Menu* menu = new Menu("File");
	add(menu);

	// just to verify sub menus
	Menu* sub = new Menu("Options");
	sub->add(new MenuItem("&Midi Devices" ,IDM_MIDI));
	sub->add(new MenuItem("&Audio Devices" ,IDM_AUDIO));
	menu->add(sub);

	menu->add(new MenuSeparator());

    menu->add(new MenuItem("&Simple Dialog", IDM_DIALOG));
    menu->add(new MenuItem("&Modeless Dialog", IDM_MODELESS_DIALOG));
    menu->add(new MenuItem("&Packed Dialog", IDM_PACKED_DIALOG));
    menu->add(new MenuItem("Message &Box", IDM_MESSAGE));
    menu->add(new MenuItem("&Open Dialog", IDM_OPENDIALOG));
    menu->add(new MenuItem("&Save Dialog", IDM_SAVEDIALOG));
    menu->add(new MenuItem("&Color Dialog", IDM_COLORDIALOG));
    menu->add(new MenuItem("&Palette Dialog", IDM_PALETTEDIALOG));

	menu = new Menu("&Help");
	menu->add(new MenuItem("&About", IDM_ABOUT));

	// we listen to our inner self
	addActionListener(this);
}

void TestMenu::actionPerformed(void* src)
{
	// this is TestMenu itself
	MenuItem* handler = (MenuItem*)src;
	int id = handler->getSelectedItemId();

	printf("qwintest: Selected menu item %d\n", id);
	fflush(stdout);

	if (id == IDM_ABOUT) {
		// wintest uses a resource for these, don't bother for the common tests
		//Dialog* d = new AboutDialog(getWindow());
		//d->show();
		//delete d;
	}
	else if (id == IDM_DIALOG) {

		TestDialog* d = new TestDialog(getWindow());
		d->show();

        if (d->isCanceled())
          printf("qwintest: Simple dialog was canceled\n");
		else 
          printf("qwintest: Simple dialog was approved with: %s\n", d->getValue());

        fflush(stdout);

        // this better be modal!
        delete d;
	}
    else if (id == IDM_MODELESS_DIALOG) {

		ModelessDialog* d = new ModelessDialog(getWindow());
		d->show();
    }
	else if (id == IDM_PACKED_DIALOG) {

		PackedDialog* d = new PackedDialog(getWindow());
		d->show();

        // this better be modal!
        delete d;
	}
	else if (id == IDM_MESSAGE) {

		MessageDialog* d = new MessageDialog(getWindow());
        d->setTitle("An Important Word From Our Sponsor");
        d->setText("Something Happened!");
		// in retrospect, this option doesn't seem to be very useful?
		d->setCancelable(true);

		d->show();
		if (d->isCanceled())
		  printf("qwintest: MessageDialog was canceled\n");
		else
		  printf("qwintest: MessageDialog was approved\n");
        fflush(stdout);

		// better be modal!
        delete d;
	}
	else if (id == IDM_OPENDIALOG) {

		OpenDialog* d = new OpenDialog(getWindow());
        d->setTitle("Open A Damn File");
		d->show();
        if (d->isCanceled())
          printf("qwintest: Open dialog was canceled\n");
        else
          printf("qwintest: Open dialog selected: %s\n", d->getFile());
        fflush(stdout);
		
        delete d;
	}
	else if (id == IDM_SAVEDIALOG) {

		OpenDialog* d = new OpenDialog(getWindow());
        d->setTitle("Save A Damn File");
		d->setSave(true);
		d->show();
        if (d->isCanceled())
          printf("qwintest: Save dialog was canceled\n");
        else
          printf("qwintest: Save dialog selected: %s\n", d->getFile());
        fflush(stdout);
		
        delete d;
	}
	else if (id == IDM_COLORDIALOG) {

		ColorDialog* d = new ColorDialog(getWindow());
        d->setTitle("Color Dialog Window");
		d->setRgb(RGB_ENCODE(128, 128, 128));
		d->show();
        if (d->isCanceled())
          printf("qwintest: Color dialog was canceled\n");
		else {
			int rgb = d->getRgb();
			int red = RGB_GET_RED(rgb);
			int green = RGB_GET_GREEN(rgb);
			int blue = RGB_GET_BLUE(rgb);

			printf("qwintest: Color dialog selected: red %d green %d blue %d\n",
				   red, green, blue);
		}
        fflush(stdout);
        delete d;
	}
	else if (id == IDM_PALETTEDIALOG) {

        char* xml = ReadFile("palette.xml");
        XmlDocument* doc = XomParser::quickParse(xml);
        XmlElement* el = doc->getChildElement();
        Palette* p = new Palette(el);
        delete doc;
        delete xml;

        PaletteDialog* pd = new PaletteDialog(getWindow(), p);
        pd->show();
		if (pd->isCanceled())
		  printf("qwintest: Palette dialog was canceled\n");
		else {
            XmlBuffer* xb = new XmlBuffer();
            p->toXml(xb);
            WriteFile("palette.xml", xb->getString());
            delete xb;
			printf("qwintest: Palette dialog wrote: palette.xml\n");
        }
        fflush(stdout);
        delete pd;
        //delete p;
	}
    else {
        printf("qwintest: Menu item not implemented %d\n", id);
        fflush(stdout);
    }

}

//////////////////////////////////////////////////////////////////////
//
// Popup Menu
//
//////////////////////////////////////////////////////////////////////

/**
 * Popup menu class without handler.
 */
class TestPopupMenu : public PopupMenu, public ActionListener
{
  public:
	
	TestPopupMenu() {
		init();
	}
	~TestPopupMenu() {}

	void init();
	void actionPerformed(void* src);
};

void TestPopupMenu::init()
{
	Menu* menu = new Menu("Colors");
	add(menu);

	// this is the Windows convention for specifying accelerators
	// need something generic!
    //menu->add(new MenuItem("&White\tCtrl+W", IDM_WHITE));
    //menu->add(new MenuItem("&Gray\tCtrl+G", IDM_GRAY));
    //menu->add(new MenuItem("&Black\tCtrl+B", IDM_BLACK));

    menu->add(new MenuItem("&White\tCtrl+W", IDM_WHITE));
    menu->add(new MenuItem("&Gray\tCtrl+G", IDM_GRAY));
    menu->add(new MenuItem("&Black\tCtrl+B", IDM_BLACK));

	// we listen to our inner self
	addActionListener(this);
}

void TestPopupMenu::actionPerformed(void* src) 
{
	// this is the PopupMenu itself, it will have saved the selection here
	MenuItem* item = (MenuItem*)src;
	MenuItem* selected = item->getSelectedItem();

	printf("qwintest: Selected popup menu item %s\n", selected->getText());
	fflush(stdout);
}

//////////////////////////////////////////////////////////////////////
//
// Timer
//
//////////////////////////////////////////////////////////////////////

class TimerHandler : public ActionListener {

  public:

	TimerHandler() {
	}

	~TimerHandler() {	
	}

	void actionPerformed(void* o) {
		//printf("qwintest: Timer tick\n");
		//fflush(stdout);
	}

};

//////////////////////////////////////////////////////////////////////
//
// Custom Button
//
//////////////////////////////////////////////////////////////////////

class FunctionButton : public CustomButton, public ActionListener {

  public:

	FunctionButton(const char *text);
    void actionPerformed(void *o);

  private:

    void init();

};

PUBLIC FunctionButton::FunctionButton(const char* text)
{
	mClassName = "FunctionButton";

    setBackground(Color::Black);
    setForeground(Color::Red);
    setTextColor(Color::White);

	setText(text);

    setMomentary(true);

	addActionListener(this);
}

PUBLIC void FunctionButton::actionPerformed(void *o)
{
	if (isPushed())
	  printf("Custom Button Down!\n");
	else
	  printf("Custom Button Up!\n");

    fflush(stdout);
}

//////////////////////////////////////////////////////////////////////
//
// Pie
//
//////////////////////////////////////////////////////////////////////

// hmm, unfortunately Pie is used on Windows for something

class PieChart : public CustomExample
{
  public:
    PieChart() {}
	void paint(Graphics* g);
};

PUBLIC void PieChart::paint(Graphics* g)
{
	tracePaint();

	Bounds b;
	getPaintBounds(&b);

	g->setColor(Color::Blue);
	g->fillRect(b.x, b.y, b.width, b.height);

	g->setColor(Color::Red);
    // clockwise is negative
	g->fillArc(b.x, b.y, b.width, b.height, 0, 45);

}

//////////////////////////////////////////////////////////////////////
//
// Table
//
//////////////////////////////////////////////////////////////////////

class TestTable : public Table {

  public:
    TestTable();
};

TestTable::TestTable() : Table()
{
    GeneralTableModel* model = new GeneralTableModel();

    model->addColumn("Target");
    model->addColumn("Trigger");
    model->addColumn("Arguments");

    model->addCell("Record", 0, 0);
    model->addCell("C4", 0, 1);
    model->addCell("Pitch Shift", 1, 0);
    model->addCell("CC 42", 1, 1);
    model->addCell("4", 1, 2);
    model->addCell("Reset", 2, 0);
    model->addCell("Pgm 42", 2, 1);

    setModel(model);
}

//////////////////////////////////////////////////////////////////////
//
// Application Frame
//
//////////////////////////////////////////////////////////////////////

class TestApp : public ActionListener, public MouseInputAdapter, public KeyListener {

  public:

	TestApp() {}
	~TestApp() {}

	int run(Context* con);
	int run2(Context* con);

	void actionPerformed(void* src);
	void mousePressed(MouseEvent* e);
	void mouseReleased(MouseEvent* e);
	void keyPressed(KeyEvent* e);
	void keyReleased(KeyEvent* e);
	void keyTyped(KeyEvent* e);

  private:

	Frame* mFrame;
	MenuBar* mMenuBar;
	PopupMenu* mPopupMenu;
	Button* mButton;
	Button* mDefButton;
    CustomButton* mCustom;
	Checkbox *mCheckbox;
	Radios* mRadio;
	Text* mText;
	TextArea* mArea;
	ComboBox* mCombo;
	ListBox* mList;
	ScrollBar* mScroll;
	ScrollBar* mVScroll;
	TabbedPane* mTabs;

};

int TestApp::run(Context* con)
{
	int result = 0;
	int y = 10;

	Component::TraceEnabled = true;

	mFrame = new Frame(con, "Test Frame");
	mFrame->setMenuBar(new TestMenu());
	mFrame->setPopupMenu(new TestPopupMenu());
    mFrame->setLayout(new VerticalLayout(2));
	//Frame->setBackground(Color::Black);

	// these are currently Windows-only
	mFrame->setIcon("chef");
	mFrame->setAccelerators("KeyAccelerators");
    mFrame->setToolTip("You're in the frame");

	// 44 seems to be exactly the height of the Mac menu bar,
	// but need to find this in a reliable way
	mFrame->setLocation(100, 100);
	// !! auto size isn't working on Mac
    //mFrame->setAutoSize(true);
	mFrame->setSize(500, 800);

	mFrame->addMouseListener(this);
	mFrame->addKeyListener(this);

	Label* l = new Label("Label: Red on Gray Helvetica 20");
    Font* f = Font::getFont("Helvetica", 0, 20);
	l->setHeavyweight(false);
    l->setFont(f);
    l->setForeground(Color::Red);
    l->setBackground(Color::Gray);
	mFrame->add(l);

    l = new Label("Label: Heavyweight Green on Gray Helvetica 30");
    // just verify that we can, should never use these?
	l->setHeavyweight(true);
    f = Font::getFont("Helvetica", 0, 30);
    l->setFont(f);
    l->setForeground(Color::Green);
    l->setBackground(Color::Gray);
    mFrame->add(l);

    Panel* buttons = new Panel("Buttons");
    buttons->setLayout(new HorizontalLayout());
    mFrame->add(buttons);

	mButton = new Button("Press Me");
    mButton->setToolTip("A button");
	mButton->addActionListener(this);
	buttons->add(mButton);

	// TODO: Need a generic way to do "default" buttons
	mDefButton = new Button("Default Button"); 
	mDefButton->setDefault(true);
	mDefButton->addActionListener(this);
	// let this also be a momentary for testing
	mDefButton->setMomentary(true);
	buttons->add(mDefButton);

    mCustom = new FunctionButton("Custom Button");
    buttons->add(mCustom);

    Panel* checks = new Panel("Checks");
    checks->setLayout(new HorizontalLayout());
    mFrame->add(checks);

	mCheckbox = new Checkbox("Check Me"); 
	mCheckbox->addActionListener(this);
	checks->add(mCheckbox);

	// no action, just make sure we can create them
	// independently
	RadioButton* rb = new RadioButton("Select Me"); 
	checks->add(rb);

	StringList* labels = new StringList();
	labels->add("this");
	labels->add("that");
	labels->add("the other");
	mRadio = new Radios(labels);
	mRadio->addActionListener(this);
	mFrame->add(mRadio);

	mText = new Text("Edit Me"); 
	mText->setColumns(10);
	mText->addActionListener(this);
	mFrame->add(mText);

	mArea = new TextArea("a\nb\nc"); 
	mArea->setColumns(20);
	mArea->setRows(3);
	mArea->addActionListener(this);
	mFrame->add(mArea);

	StringList* values = new StringList();
	values->add("this");
	values->add("that");
	values->add("the other");
	values->add("1");
	values->add("2");
	values->add("3");
	values->add("4");
	values->add("5");
	values->add("6");
	values->add("7");
	values->add("8");
	values->add("9");
	values->add("10");
    mCombo = new ComboBox();
    mCombo->setValues(values);
	mCombo->setColumns(20);
	mCombo->addActionListener(this);
    mFrame->add(mCombo);

	mList = new ListBox();
	values = new StringList();
	values->add("The quick brown fox jumped");
	values->add("over the lazy dog's back,");
	values->add("while all good men came");
	values->add("to the aid of the party.");
	values->add("Saturn is fallen, am I too to fall?");
	values->add("Am I to leave this haven of my rest,");
	values->add("This cradle of my glory, this soft clime,");
	values->add("This calm luxuriance of blissful light");
	mList->setValues(values);
	values = new StringList();
	values->add("1");
	values->add("2");
	values->add("3");
	values->add("4");
	values->add("5");
	values->add("6");
	values->add("7");
	values->add("8");
	mList->setAnnotations(values);
	// short so it gets a scroll bar
	mList->setRows(4);
	mList->setColumns(30);
	//mList->setMultiSelect(true);
	mList->addActionListener(this);

	mFrame->add(mList);
	
	mScroll = new ScrollBar();
    mScroll->setRange(0, 255);
	mScroll->setPreferredSize(200, 0);
	mScroll->addActionListener(this);
	mFrame->add(mScroll);

    mVScroll = new ScrollBar();
    mVScroll->setRange(0, 255);
    mVScroll->setVertical(true);
    mVScroll->setPreferredSize(0, 100);
	mVScroll->addActionListener(this);
    mFrame->add(mVScroll);

	// TODO: Not working on Mac
	GroupBox* gb = new GroupBox("A Group");
	gb->setLocation(10, 0);
	mFrame->add(gb);

    mTabs = new TabbedPane();
    Panel* tp = new Panel("Tab1");
	tp->setLayout(new HorizontalLayout());
	//tp->add(new Button("Button 1"));
	tp->add(new Label("now is the time"));
    mTabs->add(tp);
    tp = new Panel("Tab2");
	tp->setLayout(new HorizontalLayout());
	//tp->add(new Button("Button 2"));
	tp->add(new Label("for all good men"));
    mTabs->add(tp);
    tp = new Panel("Tab3");
	tp->setLayout(new HorizontalLayout());
	//tp->add(new Button("Button 3"));
	tp->add(new Label("to come to the aid"));
    mTabs->add(tp);
    mFrame->add(mTabs);

	// TODO: not working on Mac
    /*
    Tree* tree = new Tree();
    tree->setLocation(10, 0);
    mFrame->add(tree);
    */

	// TODO: Static bitmaps not working on Mac
    Panel* sp = new Panel("Bitmaps");
    sp->setLayout(new HorizontalLayout());
    mFrame->add(sp);
    Static* ico = new Static();
    ico->setIcon("Chef");
    sp->add(ico);
    Static* bmp = new Static();
    bmp->setBitmap("Earth");
    sp->add(bmp);

    Panel* custom = new Panel("Custom");
    custom->setLayout(new HorizontalLayout());
    mFrame->add(custom);
    custom->add(new Label("Mouse In Me!   "));
	custom->add(new CustomExample());

	PieChart* pie = new PieChart();
	mFrame->add(pie);

	l = new Label("Helvetica 8");
	l->setFont(Font::getFont("Helvetica", 0, 8));
	mFrame->add(l);
	l = new Label("Helvetica 10");
	l->setFont(Font::getFont("Helvetica", 0, 10));
	mFrame->add(l);
	l = new Label("Helvetica 12");
	l->setFont(Font::getFont("Helvetica", 0, 12));
	mFrame->add(l);
	l = new Label("Helvetica 14");
	l->setFont(Font::getFont("Helvetica", 0, 14));
	mFrame->add(l);
	l = new Label("Helvetica 16");
	l->setFont(Font::getFont("Helvetica", 0, 16));
	mFrame->add(l);

    mFrame->add(new TestTable());

	TimerHandler* th = new TimerHandler();
	SimpleTimer* timer = new SimpleTimer(1000, th);

	result = mFrame->run();

	delete mFrame;
	delete timer;
	delete th;
	delete con;
	
	QwinExit(true);

	return result;
}

int TestApp::run2(Context* con)
{
	int result = 0;
	int y = 10;

	//Component::TraceEnabled = true;

	mFrame = new Frame(con, "Test Frame");
	mFrame->setMenuBar(new TestMenu());
	mFrame->setPopupMenu(new TestPopupMenu());
    mFrame->setLayout(new VerticalLayout(2));
	mFrame->setBackground(Color::Black);

	// these are currently Windows-only
	mFrame->setIcon("chef");
	mFrame->setAccelerators("KeyAccelerators");
    mFrame->setToolTip("You're in the frame");

	// 44 seems to be exactly the height of the Mac menu bar,
	// but need to find this in a reliable way
	mFrame->setLocation(100, 100);
	// !! auto size isn't working on Mac
    //mFrame->setAutoSize(true);
	mFrame->setSize(500, 800);

	mFrame->addMouseListener(this);
	mFrame->addKeyListener(this);

    Label* l = new Label("y");
    Font* f = Font::getFont("Arial", 0, 40);
    l->setFont(f);
    l->setForeground(Color::Blue);
    mFrame->add(l);

	result = mFrame->run();

	delete mFrame;
	delete con;
	
	QwinExit(true);

	return result;
}

//////////////////////////////////////////////////////////////////////
//
// Mouse/Key Listeners
//
//////////////////////////////////////////////////////////////////////

void TestApp::mousePressed(MouseEvent* e)
{
	printf("qwintest: MouseEvent pressed type %d button %d clicks %d x %d y%d\n",
		   e->getType(), 
		   e->getButton(),
		   e->getClickCount(),
		   e->getX(),
		   e->getY());
    fflush(stdout);
}

void TestApp::mouseReleased(MouseEvent* e)
{
	printf("qwintest: MouseEvent released type %d button %d clicks %d x %d y%d\n",
		   e->getType(), 
		   e->getButton(),
		   e->getClickCount(),
		   e->getX(),
		   e->getY());
    fflush(stdout);
}

void TestApp::keyPressed(KeyEvent* e) 
{
	printf("qwintest: KeyEvent pressed type %d code %d modifiers %d repeat %d\n",
		   e->getType(), 
		   e->getKeyCode(),
		   e->getModifiers(),
		   e->getRepeatCount());
    fflush(stdout);
}

void TestApp::keyReleased(KeyEvent* e) 
{
	printf("qwintest: KeyEvent released type %d code %d modifiers %d repeat %d\n",
		   e->getType(), 
		   e->getKeyCode(),
		   e->getModifiers(),
		   e->getRepeatCount());
    fflush(stdout);
}

void TestApp::keyTyped(KeyEvent* e) 
{
	printf("qwintest: KeyEvent typed type %d code %d modifiers %d repeat %d\n",
		   e->getType(), 
		   e->getKeyCode(),
		   e->getModifiers(),
		   e->getRepeatCount());
    fflush(stdout);
}

//////////////////////////////////////////////////////////////////////
//
// Action Handlers
//
//////////////////////////////////////////////////////////////////////

void TestApp::actionPerformed(void* src)
{
	Component* c = (Component*)src;

	if (src == mMenuBar) {
		Menu* menu = (Menu*)src;
        int id = menu->getSelectedItemId();

		printf("qwintest: Menu item %d\n", id);

		if (id == 45) {
			TestDialog* d = new TestDialog(mFrame);

			// default width/height doesn't seem to take, always ends up 1?
			d->setTitle("Dialog Window");
			d->setWidth(500);
			d->setHeight(300);

			d->show();

			if (d->isCanceled())
			  printf("qwintest: Dialog was canceled\n");
			fflush(stdout);

			// this better be modal!
			delete d;
		}

	}
	else if (src == mButton) {
		printf("qwintest: Button '%s' pressed\n", mButton->getText());
		fflush(stdout);

		// dump status by simulating an action
		actionPerformed(mCheckbox);
		actionPerformed(mRadio);
		actionPerformed(mText);
		actionPerformed(mArea);
		actionPerformed(mCombo);
		actionPerformed(mList);
	}
	else if (src == mDefButton) {
		if (mDefButton->isPushed())
		  printf("qwintest: Button '%s' pressed\n", mButton->getText());
		else
		  printf("qwintest: Button '%s' released\n", mButton->getText());
		fflush(stdout);
	}
	else if (src == mCheckbox) {
		if (mCheckbox->isSelected())
		  printf("qwintest: Checkbox selected\n");
		else
		  printf("qwintest: Checkbox unselected\n");
	}
	else if (src == mRadio) {
		printf("qwintest: Radios selection %d: %s\n", 
			   mRadio->getSelectedIndex(), mRadio->getValue());
	}
	else if (src == mText) {
		const char* value = mText->getValue();
		if (value == NULL)
		  printf("qwintest: Text set to null\n");
		else
		  printf("qwintest: Text set to: %s\n", value);
	}
	else if (src == mArea) {
		const char* value = mArea->getValue();
		if (value == NULL)
		  printf("qwintest: Text area set to null\n");
		else
		  printf("qwintest: Text area set to: %s\n", value);
	}
	else if (src == mCombo) {
		printf("qwintest: ComboBox selection %d: %s\n", 
			   mCombo->getSelectedIndex(), mCombo->getValue());
	}
	else if (src == mList) {
		StringList* values = mList->getSelectedValues();
		if (values == NULL || values->size() == 0)
		  printf("qwintest: ListBox empty selection\n");
		else {
			printf("qwintest: ListBox selection: ");
			for (int i = 0 ; i < values->size() ; i++) {
				if (i > 0) printf(", ");
			  printf("%s", values->getString(i));
			}
			printf("\n");
		}
	}
	else if (src == mScroll) {
		printf("qwintest: ScrollBar %d\n", mScroll->getValue());
	}
	else if (src == mVScroll) {
		printf("qwintest: VScrollBar %d\n", mVScroll->getValue());
	}
	else if (src == mTabs) {
		printf("qwintest: TabbedPanel %d\n", mTabs->getSelectedIndex());
	}
	else {
		printf("qwintest: ActionPerformed!!!!!!!!!!!\n");
	}
}

/****************************************************************************
 *                                                                          *
 *   							   WINMAIN                                  *
 *                                                                          *
 ****************************************************************************/

/** 
 * Under Windows you have to have a function named WinMain which 
 * will be called automatically.
 *
 * The function is expected to initialize the application, create
 * the main window, and enter a message loop.  The loop should be 
 * terminated when the WM_QUIT message is received and WinMain
 * should return the value as the first parameter of the WM_QUIT message.
 * 
 * The "instance" argument has a handle to the current instance
 * of the application.  Not sure what this is, a process handle?
 *
 * The "prevInstance" will always be NULL according to the 
 * documentation.  (Then why have it?)
 * 
 * The "commandLine" will have the command line excluding the program
 * name.  If you want the entire command line use GetCommandLine.
 *
 * The "cmdShow" argument determines how the initial window is to be shown,
 * but I don't understand how these are set.  Maybe in CreateProcess?
 * 
 * SW_HIDE
 *   Hides the window and activates another window. 
 * 
 * SW_MAXIMIZE
 *   Maximizes the specified window. 
 * 
 * SW_MINIMIZE
 *   Minimizes the specified window and activates the next top-level
 *   window in the Z order. 
 * 
 * SW_RESTORE
 *   Activates and displays the window. If the window is minimized or
 *   maximized, the system restores it to its original size and position. 
 *   An application should specify this flag when restoring a 
 *   minimized window. 
 * 
 * SW_SHOW
 *   Activates the window and displays it in its current size and position.  
 * 
 * SW_SHOWMAXIMIZED
 *   Activates the window and displays it as a maximized window. 
 * 
 * SW_SHOWMINIMIZED
 *   Activates the window and displays it as a minimized window. 
 * 
 * SW_SHOWMINNOACTIVE
 *   Displays the window as a minimized window. 
 *   This value is similar to SW_SHOWMINIMIZED, except the window is not
 *    activated.
 * 
 * SW_SHOWNA Displays the window in its current size and position.
 *   This value is similar to SW_SHOW, except the window is not activated.
 *  
 * SW_SHOWNOACTIVATE
 *   Displays a window in its most recent size and position. 
 *   This value is similar to SW_SHOWNORMAL, except the window is not actived.
 *  
 * SW_SHOWNORMAL
 *  Activates and displays a window. If the window is minimized or maximized,
 *  the system restores it to its original size and position. 
 *  An application should specify this flag when displaying the window for
 *  the first time. 
 */

#ifdef _WIN32
#include "UIWindows.h"

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prevInstance,
				   LPSTR commandLine, int cmdShow)
{
	Context* con = new WindowsContext(instance, commandLine, cmdShow);

	TestApp* app  = new TestApp();

	int result = app->run(con);

	delete app;

	return result;
}

#endif

//////////////////////////////////////////////////////////////////////
//
// Mac Main
//
////////////////////////////////////////////////////////////////////////

#ifdef OSX

int main(int argc, char *argv[])
{
	Context* con = Context::getContext(argc, argv);

	TestApp* app  = new TestApp();

	int result = app->run(con);
	//int result = app->run2(con);

	delete app;

	return result;
}

#endif

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
