Mon Feb 08 00:44:54 2010

Xcode session files for OS X testing.

When compiled, xcode will leave a "build" directory as a peer to the
project file.  This is not checked into source control.  


----------------------------------------------------------------------

Creating an XCODE project from a makefile project

!! To set breakpoints in plugins, you need to disable "lazy loading" of symbols.

Bring up general preferences (Xcode->Preferences) click the Debugging tab.
Under "Symbol Loading Options" uncheck "Load symbols lazily".


This is the official documentation:

http://developer.apple.com/documentation/Porting/Conceptual/PortingUnix/preparing/chapter_3_section_4.html#//apple_ref/doc/uid/TP30001003-CH220-BBCJABGC


----------------------------------------------------------------------
Xcode Project for Mobius or other Custom Executable
----------------------------------------------------------------------

1 Launch Xcode.

2 Choose New Project from the File menu.

3 Select whatever project type you are targeting. If you ultimately
  want an application, select something like Cocoa Application. If you
  are just trying to build a command-line utility, select one of the
  tools—for example, Standard Tool.

  - I've used "Carbon C++ Application" but you get more junk to clean
    out later.  "Console C++ Tool" is simpler but you don't get
    some "External Frameworks" for Carbon, this doesn't appear to be 
    significant.

4 Follow the prompts to name and save your project. A new default
  project of that type is opened.

  - Save under the "xcode" directory of the main directory.

5 Open the Targets disclosure triangle and delete any default targets that 
  may exist.
  - Delete main.cpp and other unnecessary generated files.  There will
    usually be a documentation file named foo.1.
 
6 From the Project menu, Choose New Target.

7 Select “External Target” from the list. If this is not shown in the
  “Special Targets” list, you are not running the latest version of Xcode. 
  Upgrade first.
  
   - in the latest Xcode select "other" in the left tree, then
     selecxt the External icon

8 Follow the prompts to name that target. When you have done this, a
  target icon with the name you just gave it appears in the Targets pane
  of the open Xcode window.

  - I usually name this same as the project, e.g. "mobius" in the "mobius" 
    project

9 Double-click that new target. You should now see a new window with
  the build information for this target. This is not the same thing as
  clicking info. You must double-click the target itself.
  - in newer Xcode this also appears in the details panel within the main window

10 In the “Custom Build Command” section of the target inspector,
   change the field called “Directory” to point to the directory
   containing your makefile, and change any other settings as needed. For
   example, in the Custom Build Settings pane, you could change Build
   Tool from /usr/bin/gnumake to /usr/bin/bsdmake. More information on
   the fields is available in Xcode Help.
   
   - add "-f makefile.mac" to the Arguments
   - change Directory: to the source directory of the Mobius module	
     (qwin, mobius, etc.)

   - NOTE: This is not necessary if you're going to build from the 
     command line but not sure what it breaks

   - This is not necessary for debugging plugin hosts.  If you are configuring
     a pre-build application delete all build info.

11 Change the active target to your new target by choosing "Set Active
   Target" from the Project menu.

   - this is usually already done, there will be a green circle with a checkbox
     in the active target icon

12 Add the source files to the project. To do this, first open the
   disclosure triangle beside the “Source” folder in the left side of the
   project window. Next, drag the folder containing the sources from the
   Finder into that “Source” folder in Xcode. Tell Xcode not to copy
   files. Xcode will recursively find all of the files in that
   folder. Delete anything you don’t want listed.
   
   - You don not appear to need to add sources in the current working directory.

13 When you are ready to build the project, click the Build and Run
   button in the toolbar, select Build from the Build menu, or just press
   Command-B.

   - You do not need to build from within xcode, you can build
     on the command line and come back to xcode for debugging

14 Once the project is built, tell Xcode where to find the executable
   by choosing “New Custom Executable” from the Project menu. Choose the
   path where the executable is located, then add the name of the
   executable.

   - If you navigate to the directory containing the bundle directories, 
     navigation will stop at at the .app directory.  This is NOT enough.
     After using a file chooser edit the path to include the 
     "Contents/MacOS/<exename>" suffix.

   - Set working directory to "Custom" and and enter the path
     to the bundle's Contents/Resources directory.

     - in the new Xcode, this happens in a popup window

15 Run the resulting program by pressing Command-R.

----------------------------------------------------------------------
Xcode Project for Plugin Hosts
----------------------------------------------------------------------

1 Launch Xcode, create New Project
2 Select Console application
3 Follow the prompts to name and save the project
4 Delete any source or targets left by the wizard.
5 Optionally add the source files to the project for easier access

6 Click on "Executables" and from the menu do "New Custom Executable"
   
   - Choose the path where the executable is located, then add 
     "/Contents/MacOS/<exename>" to the application package path.

   - Set working directory to "Custom" and and enter the path
     to the bundle's Contents/Resources directory.

7 Run the resulting program by pressing Command-R.

8 Open a plugin file and set a breakpoint, it will appear yellow
  until the plugin is loaded by the host.  

----------------------------------------------------------------------
