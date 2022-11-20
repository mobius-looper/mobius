# Developing

Steps to compile on Windows 10

 * Download Visual Studio 2022.
 * Install Visual Studio 2022. In the "Visual Studio Installer":
	* Under Workloads tab: select `Desktop Development with C++`
	* Under IniInstall `C++ Windows XP Support for VS 2017 (v141) tools [Deprecated]` to gain access to Win32.Mak
 * Launch Visual Studio 2022 as an Administrator (required for Make to copy output to C:\Program Files)
 * Open a Developer Command Prompt (Tools -> Command Line -> Developer Command Prompt)
 * Add the Win32.Mak folder to INCLUDE: `set INCLUDE=%INCLUDE%;C:\Program Files (x86)\Microsoft SDKs\Windows\v7.1A\Include`. This step is manual for now for each Developer Window, this should get replaced with a project setting.
 * Run `nmake` in the project root

Create installation package:

 * Install NSIS (3.x - latest)
 * cd src/install
 * make

TODO:  
 * Move build outputs out of /src directory
 * Figure out how to execute make with cp/rm/mkdir posix command support
 * Figure out the installer
 * Fix .gitignore to properly ignore generated (non-source) files
 * Setup separate install target for copying outputs to Program Files
 * Investigate CMake / msbuild alternative to raw Makefiles
 * Evaluate current state of 64-bit support (Setup 64-bit compilation target)
 * Evaluate dependencies:
	* CoreAudio SDK 1.4.3
	* PortAudio v19 (20071207)
	* VST 2/3
	* OSC
	* qwin (UI?)
	* SoundTouch 1.3.1 (Bose integration?)
 * Tests?
 * Investigate JUCE as a potential port target

Notes:
 * Jeff used cygwin on Windows to run emacs and a linux-like environment to execute the builds. See /src/make/.bashrc
   We can probably get a comparable build environment by recreating the necessary .bashrc edits in git-bash (mingw64) to allow executing nmake with proper includes