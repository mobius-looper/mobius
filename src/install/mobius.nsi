;
; Mobius Installation Script
; Based on the NSIS Modern User Interface example
;

;
;Includes
;

!include "MUI.nsh"

;---------------------------------------------------------------
;General
;---------------------------------------------------------------

Name "Möbius 2.6.0 Beta 2"
OutFile "setup.exe"

;Default installation folder
InstallDir "$PROGRAMFILES\Mobius 2.6"
  
;Get installation folder from registry if available
InstallDirRegKey HKCU "Software\Circular Labs\Mobius 2" ""

;---------------------------------------------------------------
;Variables
;---------------------------------------------------------------

Var MUI_TEMP
Var STARTMENU_FOLDER

;---------------------------------------------------------------
;Interface Settings
;---------------------------------------------------------------

!define MUI_ABORTWARNING

!define MUI_ICON "${NSISDIR}\Contrib\Graphics\Icons\modern-install-blue.ico"

;!define MUI_BGCOLOR 777777

;---------------------------------------------------------------
;Pages
;---------------------------------------------------------------

; lobotomize the default text requesting to shut down all running apps
!define MUI_WELCOMEPAGE_TEXT "Click next to continue."
!insertmacro MUI_PAGE_WELCOME

; text that appears above the license box
;!define MUI_LICENSEPAGE_TEXT_TOP "Top text"

; text that appears under the license box
; Deafult is "Please review...click I Agree"
!define MUI_LICENSEPAGE_TEXT_BOTTOM  " "

; sets the text in the "I Agree" button
!define MUI_LICENSEPAGE_BUTTON  "Continue"

!insertmacro MUI_PAGE_LICENSE "license.txt"

; don't need a components page
;!insertmacroMUI_PAGE_COMPONENTS

!insertmacro MUI_PAGE_DIRECTORY

; default folder is the full product name
!define MUI_STARTMENUPAGE_DEFAULTFOLDER "Möbius 2"
!insertmacro MUI_PAGE_STARTMENU "Application" $STARTMENU_FOLDER

!insertmacro MUI_PAGE_INSTFILES
  
; having this will prevent you from looking at the details 
; in the INSTFILES page
;!insertmacro MUI_PAGE_FINISH


;
; Uninstall pages
;

!insertmacro MUI_UNPAGE_CONFIRM

!insertmacro MUI_UNPAGE_INSTFILES
  
;---------------------------------------------------------------
;Languages
;---------------------------------------------------------------
 
!insertmacro MUI_LANGUAGE "English"

;---------------------------------------------------------------
;Installer Sections
;---------------------------------------------------------------

Section "Core Section" SecCore

  SetOutPath "$INSTDIR"
  
  File license.txt
  File ..\mobius\mobius.exe
  File ..\mobius\mobius.pdb
  File ..\mobius\MobiusVst2.dll
  File ..\mobius\Catalog_USEnglish.txt
  File ..\mobius\install\text\README.txt
  File ..\mobius\install\config\host.xml

  IfFileExists "$INSTDIR\mobius.xml" DoUi
  File ..\mobius\install\config\mobius.xml

  DoUi:
  IfFileExists "$INSTDIR\ui.xml" DoOsc
  File ..\mobius\install\config\ui.xml

  DoOsc:
  IfFileExists "$INSTDIR\osc.xml" DoDocs
  File ..\mobius\install\config\osc.xml

  DoDocs:
  ;; not yet, make them go to the web to stay current
  ;;  SetOutPath "$INSTDIR\docs"
  ;;  File ..\mobius\doc\public\*.htm
  ;;  SetOutPath "$INSTDIR\docs\images"
  ;;  File ..\mobius\doc\public\images\*.gif

  SetOutPath "$INSTDIR\scripts"
  File ..\mobius\install\scripts\*.*

  SetOutPath "$INSTDIR\loops"
  File ..\mobius\install\loops\*.*

  SetOutPath "$INSTDIR\samples"
  File ..\mobius\install\samples\*.*

  IfFileExists "$PROGRAMFILES\VstPlugins" VstPluginsExists DoneWithFiles

  VstPluginsExists:
  SetOutPath "$PROGRAMFILES\VstPlugins"
  File ..\mobius\MobiusVst2.dll 

  DoneWithFiles:
  SetOutPath "$INSTDIR"

  ;Store installation folder
  WriteRegStr HKCU "Software\Circular Labs\Mobius 2" "InstDirectory" $INSTDIR

  ;Create uninstaller
  WriteUninstaller "$INSTDIR\Uninstall.exe"

  ;Add start menu items
  !insertmacro MUI_STARTMENU_WRITE_BEGIN Application

    CreateDirectory "$SMPROGRAMS\$STARTMENU_FOLDER"
    CreateShortCut "$SMPROGRAMS\$STARTMENU_FOLDER\Möbius.lnk" "$INSTDIR\mobius.exe"
    CreateShortCut "$SMPROGRAMS\$STARTMENU_FOLDER\Readme.lnk" "$INSTDIR\README.txt"

    ;CreateShortCut "$SMPROGRAMS\$STARTMENU_FOLDER\1 Installation Guide.lnk" "http://www.circularlabs.com/doc/v2/installation.htm"
    ;CreateShortCut "$SMPROGRAMS\$STARTMENU_FOLDER\2 Getting Started.lnk" "http://www.circularlabs.com/doc/v2/starting.htm"
    ;CreateShortCut "$SMPROGRAMS\$STARTMENU_FOLDER\3 Exploring Features.lnk" "http://www.circularlabs.com/doc/v2/exploring.htm"
    ;CreateShortCut "$SMPROGRAMS\$STARTMENU_FOLDER\4 Reference Manual.lnk" "http://www.circularlabs.com/doc/v2/reference.htm"
    ;CreateShortCut "$SMPROGRAMS\$STARTMENU_FOLDER\5 Scripting Manual.lnk" "http://www.circularlabs.com/doc/v2/scripts.htm"

    CreateShortCut "$SMPROGRAMS\$STARTMENU_FOLDER\Documentation.lnk" "http://www.circularlabs.com/documentation2/documentation.html"

    ; trying to delete old links that didn't go anywyere, this doesn't work 
    Delete "$SMPROGRAMS\$STARTMENU_FOLDER\1 Installation Guide.lnk"
    Delete "$SMPROGRAMS\$STARTMENU_FOLDER\2 Getting Started.lnk"
    Delete "$SMPROGRAMS\$STARTMENU_FOLDER\3 Exploring Features.lnk"
    Delete "$SMPROGRAMS\$STARTMENU_FOLDER\4 Reference Manual.lnk"
    Delete "$SMPROGRAMS\$STARTMENU_FOLDER\5 Scripting Manual.lnk"

    CreateShortCut "$SMPROGRAMS\$STARTMENU_FOLDER\Uninstall.lnk" "$INSTDIR\Uninstall.exe"

  !insertmacro MUI_STARTMENU_WRITE_END

SectionEnd

;---------------------------------------------------------------
;Descriptions
;---------------------------------------------------------------

; necessary only if we're displaying the components page

;Language strings
LangString DESC_SecCore ${LANG_ENGLISH} "Core section."

;Assign language strings to sections
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN

!insertmacro MUI_DESCRIPTION_TEXT ${SecCore} $(DESC_SecCore)

!insertmacro MUI_FUNCTION_DESCRIPTION_END

;---------------------------------------------------------------
;Uninstaller Section
;---------------------------------------------------------------

Section "Uninstall"

  Delete "$INSTDIR\license.txt"
  Delete "$INSTDIR\README.txt"
  Delete "$INSTDIR\mobius.exe"
  Delete "$INSTDIR\mobius.pdb"
  Delete "$INSTDIR\Mobius.dll"
  Delete "$INSTDIR\MobiusVst2.dll"
  Delete "$INSTDIR\Catalog_USEnglish.txt"
  Delete "$INSTDIR\Uninstall.exe"

  ;; hmm, may want to keep these?
  ;Delete "$INSTDIR\mobius.xml"
  ;Delete "$INSTDIR\ui.xml"
  ;Delete "$INSTDIR\osc.xml"
  Delete "$INSTDIR\host.xml"
  Delete "$INSTDIR\docs\*.*"
  Delete "$INSTDIR\scripts\*.*"
  Delete "$INSTDIR\loops\*.*"
  Delete "$INSTDIR\samples\*.*"

  RMDir "$INSTDIR\docs"
  RMDir "$INSTDIR\scripts"
  RMDir "$INSTDIR\samples"
  RMDir "$INSTDIR\projects"
  ;RMDir "$INSTDIR"

  !insertmacro MUI_STARTMENU_GETFOLDER Application $MUI_TEMP
    
  Delete "$SMPROGRAMS\$MUI_TEMP\Möbius.lnk"
  Delete "$SMPROGRAMS\$MUI_TEMP\Readme.lnk"
  Delete "$SMPROGRAMS\$MUI_TEMP\1 Installation Guide.lnk"
  Delete "$SMPROGRAMS\$MUI_TEMP\2 Getting Started.lnk"
  Delete "$SMPROGRAMS\$MUI_TEMP\3 Exploring Features.lnk"
  Delete "$SMPROGRAMS\$MUI_TEMP\4 Reference Manual.lnk"
  Delete "$SMPROGRAMS\$MUI_TEMP\5 Scripting Manual.lnk"
  Delete "$SMPROGRAMS\$MUI_TEMP\Uninstall.lnk"
  
  ;Delete empty start menu parent diretories
  StrCpy $MUI_TEMP "$SMPROGRAMS\$MUI_TEMP"
 
  startMenuDeleteLoop:
    RMDir $MUI_TEMP
    GetFullPathName $MUI_TEMP "$MUI_TEMP\.."
    
    IfErrors startMenuDeleteLoopDone
  
    StrCmp $MUI_TEMP $SMPROGRAMS startMenuDeleteLoopDone startMenuDeleteLoop
  startMenuDeleteLoopDone:

  DeleteRegKey HKCU "Software\Circular Labs\Mobius 2"

SectionEnd
