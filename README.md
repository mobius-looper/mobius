# Developing

Steps to compile on Windows 10

 * Download Visual Studio 2022.
 * Install Visual Studio 2022. In the "Visual Studio Installer":
	* Under Workloads tab: select `Desktop Development with C++`
 * Launch Visual Studio 2022 as an Administrator (required for Make to copy output to C:\Program Files)
 * Open a Developer Command Prompt (Tools -> Command Line -> Developer Command Prompt)
 * Run `nmake` in the project root

Create installation package:

 * Install NSIS (3.x - latest)
 * `cd src/install`
 * `nmake`

Compile for 64-bit:
 * Update common.mak to select common.win64 make profile (Enables CPU=AMD64 architecture for WIN32.mak)
 * Start -> Launch `x64 Native Tools Command Prompt for Visual Studio 2022` (Run as Administrator)
 * Run `nmake` in the project root