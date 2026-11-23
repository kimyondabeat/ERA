; ERA - Raboday Drums - Windows installer (NSIS)
Unicode true
!include "MUI2.nsh"
!include "x64.nsh"

!define APPNAME   "ERA"
!define FULLNAME  "ERA - Raboday Drums"
!define COMPANY   "kimyondabeat"
!define VERSION   "1.0.0"
!ifndef SRC
  !define SRC "..\stage\ERA"
!endif
!define UNINSTKEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\ERA"

Name "${FULLNAME}"
!ifndef OUTFILE
  !define OUTFILE "..\ERA_v1.0_Setup.exe"
!endif
OutFile "${OUTFILE}"
!ifdef NOADMIN
  !define VST3SHOW "%LOCALAPPDATA%\Programs\Common\VST3"
  !define VST3DIR "$LOCALAPPDATA\Programs\Common\VST3"
  !define REGROOT HKCU
  InstallDir "$LOCALAPPDATA\Programs\ERA"
  RequestExecutionLevel user
!else
  !define VST3SHOW "C:\Program Files\Common Files\VST3"
  !define VST3DIR "$COMMONFILES64\VST3"
  !define REGROOT HKLM
  InstallDir "$PROGRAMFILES64\ERA"
  RequestExecutionLevel admin
!endif
SetCompressor /SOLID lzma
BrandingText "ERA v1.0  -  by kimyondabeat"

VIProductVersion "1.0.0.0"
VIAddVersionKey "ProductName" "${FULLNAME}"
VIAddVersionKey "CompanyName" "${COMPANY}"
VIAddVersionKey "FileVersion" "${VERSION}"
VIAddVersionKey "ProductVersion" "${VERSION}"
VIAddVersionKey "FileDescription" "${FULLNAME} installer"
VIAddVersionKey "LegalCopyright" "(c) ${COMPANY}"

!define MUI_ICON "era.ico"
!define MUI_UNICON "era.ico"
!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_RIGHT
!define MUI_HEADERIMAGE_BITMAP "header.bmp"
!define MUI_HEADERIMAGE_UNBITMAP "header.bmp"
!define MUI_WELCOMEFINISHPAGE_BITMAP "welcome.bmp"
!define MUI_UNWELCOMEFINISHPAGE_BITMAP "welcome.bmp"
!define MUI_ABORTWARNING

!define MUI_WELCOMEPAGE_TITLE "Welcome to ERA"
!define MUI_WELCOMEPAGE_TEXT "This will install ERA - Raboday Drums v1.0 on your computer.$\r$\n$\r$\n- The ERA VST3 plugin, for any DAW that supports VST3 instruments$\r$\n- The ERA desktop app, to play ERA without a DAW$\r$\n$\r$\nClick Next to continue."
!define MUI_COMPONENTSPAGE_SMALLDESC
!define MUI_FINISHPAGE_TITLE "ERA is installed"
!define MUI_FINISHPAGE_TEXT "Open your DAW and rescan your plugins to find ERA.$\r$\n$\r$\nThe VST3 plugin is in:$\r$\n${VST3SHOW}\ERA.vst3"
!define MUI_FINISHPAGE_RUN "$INSTDIR\ERA.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Open the ERA app now"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

Function .onInit
  ${IfNot} ${RunningX64}
    MessageBox MB_ICONSTOP "ERA needs a 64-bit version of Windows."
    Abort
  ${EndIf}
  SetRegView 64
!ifdef NOADMIN
  SetShellVarContext current
!else
  SetShellVarContext all
!endif
FunctionEnd

Function un.onInit
  SetRegView 64
!ifdef NOADMIN
  SetShellVarContext current
!else
  SetShellVarContext all
!endif
FunctionEnd

Section "ERA VST3 plugin (for your DAW)" SecVST3
  SectionIn RO
  SetOutPath "${VST3DIR}"
  RMDir /r "${VST3DIR}\ERA.vst3"
  File /r "${SRC}\ERA.vst3"
SectionEnd

Section "ERA app (play without a DAW)" SecApp
  SetOutPath "$INSTDIR"
  File "${SRC}\Standalone\ERA.exe"
  File "era.ico"
  CreateDirectory "$SMPROGRAMS\ERA"
  CreateShortcut "$SMPROGRAMS\ERA\ERA.lnk" "$INSTDIR\ERA.exe" "" "$INSTDIR\era.ico"
  CreateShortcut "$SMPROGRAMS\ERA\Uninstall ERA.lnk" "$INSTDIR\Uninstall ERA.exe"
SectionEnd

Section "Desktop shortcut" SecDesktop
  CreateShortcut "$DESKTOP\ERA.lnk" "$INSTDIR\ERA.exe" "" "$INSTDIR\era.ico"
SectionEnd

Section "-Uninstaller"
  SetOutPath "$INSTDIR"
  File "${SRC}\README.txt"
  File "era.ico"
  WriteUninstaller "$INSTDIR\Uninstall ERA.exe"
  WriteRegStr ${REGROOT} "${UNINSTKEY}" "DisplayName" "${FULLNAME}"
  WriteRegStr ${REGROOT} "${UNINSTKEY}" "DisplayVersion" "${VERSION}"
  WriteRegStr ${REGROOT} "${UNINSTKEY}" "Publisher" "${COMPANY}"
  WriteRegStr ${REGROOT} "${UNINSTKEY}" "DisplayIcon" "$INSTDIR\era.ico"
  WriteRegStr ${REGROOT} "${UNINSTKEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr ${REGROOT} "${UNINSTKEY}" "UninstallString" '"$INSTDIR\Uninstall ERA.exe"'
  WriteRegStr ${REGROOT} "${UNINSTKEY}" "QuietUninstallString" '"$INSTDIR\Uninstall ERA.exe" /S'
  WriteRegDWORD ${REGROOT} "${UNINSTKEY}" "NoModify" 1
  WriteRegDWORD ${REGROOT} "${UNINSTKEY}" "NoRepair" 1
  WriteRegDWORD ${REGROOT} "${UNINSTKEY}" "EstimatedSize" 30000
SectionEnd

LangString DESC_VST3 ${LANG_ENGLISH} "Installs ERA.vst3 in ${VST3SHOW}. Required."
LangString DESC_APP ${LANG_ENGLISH} "The ERA desktop app, with a Start menu shortcut."
LangString DESC_DESK ${LANG_ENGLISH} "Puts a ERA shortcut on your desktop."
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SecVST3} $(DESC_VST3)
  !insertmacro MUI_DESCRIPTION_TEXT ${SecApp} $(DESC_APP)
  !insertmacro MUI_DESCRIPTION_TEXT ${SecDesktop} $(DESC_DESK)
!insertmacro MUI_FUNCTION_DESCRIPTION_END

Section "Uninstall"
  RMDir /r "${VST3DIR}\ERA.vst3"
  Delete "$INSTDIR\ERA.exe"
  Delete "$INSTDIR\era.ico"
  Delete "$INSTDIR\README.txt"
  Delete "$INSTDIR\Uninstall ERA.exe"
  RMDir "$INSTDIR"
  Delete "$SMPROGRAMS\ERA\ERA.lnk"
  Delete "$SMPROGRAMS\ERA\Uninstall ERA.lnk"
  RMDir "$SMPROGRAMS\ERA"
  Delete "$DESKTOP\ERA.lnk"
  DeleteRegKey ${REGROOT} "${UNINSTKEY}"
SectionEnd
