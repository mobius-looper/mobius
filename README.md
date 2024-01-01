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

## Releasing

Tag the repository on master with a pre-release number: `v2.x.y-rc.z`.
A GitHub Action workflow will build a "pre-release" release and attach the artifacts.

When it's time to release a new version, update the following files with the new build version:
 * src/install/releases.txt
 * src/installx64/releases.txt
 * src/mobius/AboutDialog.cpp
 * src/mobius/install/text/README.txt

Tag the repository on master with the release number: `v2.x.y`.
A GitHub Action workflow will build the release and attach the artifacts.

Manually update the Release description in GitHub with the release notes.
