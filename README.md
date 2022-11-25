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
 * Open a Developer Command Prompt
 * `cd src/install`
 * `nmake`

Compile for 64-bit:
 * Start -> Launch `x64 Native Tools Command Prompt for Visual Studio 2022` (Run as Administrator)
 * Run `nmake` in the project root