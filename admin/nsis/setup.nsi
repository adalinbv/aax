!include x64.nsh

;set initial value for $INSTDIR
InstallDir "$PROGRAMFILES\${MY_COMPANY}\${MY_APP}"

${If} ${RunningX64}
  DetailPrint "Installer running on 64-bit host"

  ; disable registry redirection (enable access to 64-bit portion of registry)
  SetRegView 64

  ; 
  ${DisableX64FSRedirection}

  ; change install dir
  StrCpy $INSTDIR "$PROGRAMFILES64\${MY_COMPANY}\${MY_APP}"

${EndIf}

section "GENERAL"
  CopyFiles "$INSTDIR\bin\${LIBAEONWAVE}.dll" "$SYSDIR\${LIBAEONWAVE}.dll"
sectionEnd

section "Uninstall"
  ifFileExists "$SYSDIR\${LIBAEONWAVE}.dll" 0 next2
    Delete "$SYSDIR\${LIBAEONWAVE}.dll"
next2:
sectionEnd

${If} ${RunningX64}
  ${EnableX64FSRedirection}
${EndIf}
