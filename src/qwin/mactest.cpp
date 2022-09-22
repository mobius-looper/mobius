/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Ad-hoc focused tests for Mac
 * There isn't much of interest in here, I use it as a scratchpad
 * for ad-hoc debugging on Mac, if anything interesting
 * comes out of this it should be moved to qwintest.cpp
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "Trace.h"
#include "QwinExt.h"

QWIN_USE_NAMESPACE

//////////////////////////////////////////////////////////////////////
//
// Application Frame
//
//////////////////////////////////////////////////////////////////////

class TestApp : public ActionListener {

  public:

	TestApp() {}
	~TestApp() {}

	int run(Context* con);
	int openDrawingWindow(Context* con);

	void actionPerformed(void* src);

  private:

	Frame* mFrame;
	TabbedPane* mTabs;

};

void TestApp::actionPerformed(void* src)
{
}


int TestApp::run(Context* con)
{
	int result = 0;
	int y = 10;

	Component::TraceEnabled = true;

	mFrame = new Frame(con, "Test Frame");
	mFrame->setLayout(new VerticalLayout());

	// 44 seems to be exactly the height of the menu bar,
	// but need to find this in a reliable way
	mFrame->setLocation(100, 100);
	mFrame->setSize(500, 600);

	Panel* p;

	/*
	Panel* p = new Panel();
	p->add(new Label("Before the tabs"));
	mFrame->add(p);
	*/
	mFrame->add(new Strut(0, 100));

    mTabs = new TabbedPane();
    mFrame->add(mTabs);
/*
	Panel* hack = new Panel();
	hack->setLayout(new HorizontalLayout());
	hack->add(mTabs);
	//hack->add(new Button("Over Here"));
	mFrame->add(hack);
*/

    Panel* tp = new Panel("Tab1");
	//tp->add(new CustomExample());
	//mTabs->add(tp);

	tp->setLayout(new HorizontalLayout());
	//tp->add(new Button("Button 1"));
	tp->add(new Label("Tab Panel 1"));
    mTabs->add(tp);
    tp = new Panel("Tab2");
	tp->setLayout(new HorizontalLayout());
	tp->add(new Button("Button 2"));
    mTabs->add(tp);
    tp = new Panel("Tab3");
	tp->setLayout(new HorizontalLayout());
	tp->add(new Button("Button 3"));
    mTabs->add(tp);

	/*
	p = new Panel();
	p->setLayout(new HorizontalLayout(10));
	p->add(new Label("After the tabs"));
	p->add(new Label("And another"));
	mFrame->add(p);
	*/

	result = mFrame->run();

	delete mFrame;
	delete con;
	
	QwinExit(true);

	return result;
}

//////////////////////////////////////////////////////////////////////
//
// Old Drawing Test
//
//////////////////////////////////////////////////////////////////////

int TestApp::openDrawingWindow(Context* con)
{
	int result = 0;

	Frame* frame = new Frame(con, "Test Frame");
	// 44 seems to be exactly the height of the menu bar,
	// but need to find this in a reliable way
	frame->setLocation(100, 100);
	frame->setSize(500, 800);

    frame->setLayout(new VerticalLayout(2));

	// have to open before we can get to a Graphics
	frame->open();
	Graphics* g = frame->getGraphics();

	const char* text = "Now is the time!";
	Font* f2 = Font::getFont("Helvetica", FONT_BOLD | FONT_ITALIC, 20);
	g->setFont(f2);

	g->setColor(Color::Blue);
	g->fillRect(0, 0, 100, 100);

	g->setColor(Color::Red);
	g->drawRect(100, 0, 100, 100);

	g->setColor(Color::Green);
	g->fillOval(200, 0, 100, 100);

	g->setColor(Color::Black);
	g->drawOval(300, 0, 100, 100);

	g->setColor(Color::Gray);
	g->drawLine(400, 0, 500, 100);

	g->setColor(Color::Gray);
	g->drawLine(500, 0, 400, 100);

	g->drawRoundRect(0, 100,100, 100, 20, 20);
	g->setColor(Color::Green);
	g->fillRoundRect(100, 100,100, 100, 20, 20);

	int x = 0;
	int y = 0;

	g->setColor(Color::Black);
	g->drawRect(0, 0, 0, 0);
	g->drawRect(4, 4, 0, 0);
	g->drawRect(8, 8, -1, -1);

	Dimension d;
	g->getTextSize(text, f2, &d);
	g->setColor(Color::Red);
	g->fillRect(x, y, d.width, d.height);

	g->setColor(Color::Blue);
	g->drawString(text, x, y);

	Trace(1, "Trace should be working!\n");

	result = frame->run();

	delete frame;
	delete con;
	
	QwinExit(true);

	return result;
}

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

	return result;
}

#endif
/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
