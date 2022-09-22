/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * More tests just for windows.
 * There isn't much of interest in here, I use it as a scratchpad
 * for ad-hoc debugging on Windows, if anything interesting
 * comes out of this it should be moved to qwintest.cpp
 *
 * The main thing this does is show how to get menu definitions
 * from a Windows resource file but since that isn't cross-platform
 * it isn't very useful other than as an example.  
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "XomParser.h"

#include "QwinExt.h"
#include "UIManager.h"
#include "wintest.h"

QWIN_USE_NAMESPACE

#include "UIWindows.h"

// menu ids
#include "wintest.h"

/****************************************************************************
 *                                                                          *
 *   							 ABOUT DIALOG                               *
 *                                                                          *
 ****************************************************************************/

class AboutDialog : public Dialog {

  public:

	AboutDialog(Window* parent);

};

AboutDialog::AboutDialog(Window *parent)
{
	setParent(parent);
	setResource("AboutBox");
}

/****************************************************************************
 *                                                                          *
 *   								MENUS                                   *
 *                                                                          *
 ****************************************************************************/

/**
 * Menu class with handler method.
 */
class TestMenu : public MenuBar
{
  public:
	
	TestMenu() {
		setResource("MainMenu");
	}
	~TestMenu() {}

	void menuCommand(int id);
};

void TestMenu::menuCommand(int id)
{
	printf("Selected menu item %d\n", id);
	fflush(stdout);

	if (id == IDM_ABOUT) {
		// rather than build these every time, should be able
		// to construct them once and then null out the handles when it closes
		Dialog* d = new AboutDialog(getWindow());
		d->show();
		delete d;
	}
    else {
        printf("Menu not implemented %d\n", id);
    }

}

//////////////////////////////////////////////////////////////////////
//
// Main Window
//
//////////////////////////////////////////////////////////////////////

int openWindow(Context* con)
{
	int result = 0;
	int y = 10;

	Frame* frame = new Frame(con, "Test Frame");
    frame->setToolTip("You're in the frame");
	frame->setIcon("chef");
	frame->setMenuBar(new TestMenu());
    frame->setAutoSize(true);

    /*
	Panel *p = new Panel();
	p->setLayout(new HorizontalLayout());
	p->setLocation(10, 0);
	frame->add(p);

    p->add(new Label("Icons..."));

    Static* ico = new Static();
    ico->setIcon("Chef");
    p->add(ico);
    Static* bmp = new Static();
    bmp->setBitmap("Earth");
    p->add(bmp);
    */

    Panel* custom = new Panel("Custom");
    custom->setLayout(new HorizontalLayout());
    frame->add(custom);
    custom->add(new Label("Mouse In Me!   "));
	custom->add(new CustomExample());

	result = frame->run();

	delete frame;
	delete con;
	
	QwinExit(true);

	return result;
}

/****************************************************************************
 *                                                                          *
 *   							   WINMAIN                                  *
 *                                                                          *
 ****************************************************************************/

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prevInstance,
				   LPSTR commandLine, int cmdShow)
{
	int result = 0;

	WindowsContext* con = new WindowsContext(instance, commandLine, cmdShow);

	result = openWindow(con);

	return result;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
