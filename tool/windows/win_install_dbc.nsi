; ��װ�����ʼ���峣��
!define PRODUCT_NAME "DBC for AI"
!define PRODUCT_VERSION "0.3.4.1"
!define PRODUCT_PUBLISHER "Deep Brain Chain, Inc."
!define PRODUCT_WEB_SITE "https://www.deepbrainchain.org"
!define PRODUCT_UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"
!define PRODUCT_UNINST_ROOT_KEY "HKLM"
!define IPFS_CONFIG_PATH "$PROFILE\.ipfs"
!define PATH_KEY "SYSTEM\CurrentControlSet\Control\Session Manager\Environment"
!define CU_PATH_KEY "Environment"

SetCompressor lzma

; ------ MUI �ִ����涨�� (1.67 �汾���ϼ���) ------
!include "MUI.nsh"
!include "LogicLib.nsh"
;!include "UsefulLib.nsh"
!include "WordFunc.nsh"


!addplugindir "${NSISDIR}\Plugins"

; MUI Ԥ���峣��
!define MUI_ABORTWARNING
!define MUI_ICON "dbc.ico"
!define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\modern-uninstall.ico"

; ��ӭҳ��
!insertmacro MUI_PAGE_WELCOME
; ���Э��ҳ��
#!insertmacro MUI_PAGE_LICENSE "licence.txt"
; ��װĿ¼ѡ��ҳ��
!insertmacro MUI_PAGE_DIRECTORY
; ��װ����ҳ��
!insertmacro MUI_PAGE_INSTFILES
; ��װ���ҳ��
!define MUI_FINISHPAGE_RUN "dbc.exe"
!define MUI_FINISHPAGE_RUN_PARAMETERS ""
#!define MUI_FINISHPAGE_SHOWREADME "init dbc"
!insertmacro MUI_PAGE_FINISH

; ��ʼ�˵�����ҳ��
; Var ICONS_GROUP


; ��װж�ع���ҳ��
!insertmacro MUI_UNPAGE_INSTFILES

; ��װ�����������������
!insertmacro MUI_LANGUAGE "English"

; ��װԤ�ͷ��ļ�
!insertmacro MUI_RESERVEFILE_INSTALLOPTIONS
; ------ MUI �ִ����涨����� ------

Name "${PRODUCT_NAME} ${PRODUCT_VERSION}"
OutFile "dbc_win_v${PRODUCT_VERSION}.exe"
InstallDir "C:\DBC"
ShowInstDetails show
ShowUnInstDetails show
BrandingText "DBC for AI"

Var boolInstallSuccess

Section "MainSection" SEC01
  SetOutPath "$INSTDIR"
  SetOverwrite ifdiff
  File /r "dbc-win10-0.3.4.1_install\*.*"
  CreateShortCut "$DESKTOP\dbc.lnk" "$INSTDIR\dbc.exe"  "" "$INSTDIR\dbc.ico" 0
  CreateShortCut "$SMSTARTUP\startup dbc.lnk" "$INSTDIR\dbc.exe"  "" "$INSTDIR\dbc.ico" 0
SectionEnd

Section -AdditionalIcons
  WriteIniStr "$INSTDIR\${PRODUCT_NAME}.url" "InternetShortcut" "URL" "${PRODUCT_WEB_SITE}"
  CreateDirectory "$SMPROGRAMS\DBC for AI"
  CreateShortCut "$SMPROGRAMS\DBC for AI\dbc.lnk" "$INSTDIR\dbc.exe" "" "$INSTDIR\dbc.ico" 0
  CreateShortCut "$SMPROGRAMS\DBC for AI\Website.lnk" "$INSTDIR\${PRODUCT_NAME}.url" \
  "" "$INSTDIR\dbc.ico" 0 SW_SHOWNORMAL   ALT|CTRL|SHIFT|F5 "(www.deepbrainchain.com)"
  CreateShortCut "$SMPROGRAMS\DBC for AI\Uninstall.lnk" "$INSTDIR\uninst.exe"
SectionEnd

Section -Post
  WriteUninstaller "$INSTDIR\uninst.exe"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayName" "$(^Name)"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "UninstallString" "$INSTDIR\uninst.exe"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayVersion" "${PRODUCT_VERSION}"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "URLInfoAbout" "${PRODUCT_WEB_SITE}"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "Publisher" "${PRODUCT_PUBLISHER}"
  ; add dbc and ipfs to PATH
	Var /GLOBAL vPath
  ReadRegStr $vPath HKCU "${CU_PATH_KEY}" "Path"
  strcpy $vPath "$vPath;$INSTDIR"
  strcpy $vPath "$vPath;$INSTDIR\ipfs"
  WriteRegStr HKCU "${CU_PATH_KEY}" "Path" "$vPath"
  
  ReadRegStr $vPath HKLM "${PATH_KEY}" "Path"
  strcpy $vPath "$vPath;$INSTDIR"
  strcpy $vPath "$vPath;$INSTDIR\ipfs"
  WriteRegStr HKLM "${PATH_KEY}" "Path" "$vPath"
SectionEnd

/******************************
 *  �����ǰ�װ�����ж�ز���  *
 ******************************/

Section Uninstall
  Delete "$SMPROGRAMS\DBC for AI\Uninstall.lnk"
  Delete "$SMPROGRAMS\DBC for AI\Website.lnk"
  RMDir /r "$SMPROGRAMS\DBC for AI"
  Delete "$DESKTOP\dbc.lnk"
  
  RMDir /r "$INSTDIR\conf"
  RMDir /r "$INSTDIR\examples"
  RMDir /r "$INSTDIR\ipfs"
  RMDir /r "$INSTDIR\logs"
  RMDir /r "$INSTDIR\tool"
  Delete   "$INSTDIR\dbc.exe"
  Delete   "$INSTDIR\dbc.ico"
  Delete   "$INSTDIR\*.dll"
  Delete   "$INSTDIR\${PRODUCT_NAME}.url"
  Delete   "$INSTDIR\uninst.exe"
  
  RMDir /r "${IPFS_CONFIG_PATH}"

	; rm dbc path from user env variable Path
	Var /GLOBAL envPath
	ReadRegStr $envPath HKCU "${CU_PATH_KEY}" "Path"
 	${WordReplace} $envPath ";$INSTDIR\ipfs\ipfs.exe" "" "+" $envPath
	${WordReplace} $envPath ";$INSTDIR\ipfs" "" "+" $envPath
	${WordReplace} $envPath "$INSTDIR\ipfs\ipfs.exe" "" "+" $envPath
	${WordReplace} $envPath "$INSTDIR\ipfs" "" "+" $envPath
	;MessageBox MB_OK '$envPath - $INSTDIR'
	${WordReplace} $envPath ";$INSTDIR\dbc.exe" "" "+" $envPath
	${WordReplace} $envPath ";$INSTDIR" "" "+" $envPath
	${WordReplace} $envPath "$INSTDIR\dbc.exe" "" "+" $envPath
	${WordReplace} $envPath "$INSTDIR" "" "+" $envPath
	;MessageBox MB_OK '$envPath - $INSTDIR'
	WriteRegExpandStr HKCU "${CU_PATH_KEY}" "Path" "$envPath"

	; rm dbc path from sys env variable Path
	Var /GLOBAL sysEnvPath
	ReadRegStr $sysEnvPath HKLM "${PATH_KEY}" "Path"
 	${WordReplace} $sysEnvPath ";$INSTDIR\ipfs\ipfs.exe" "" "+" $sysEnvPath
	${WordReplace} $sysEnvPath ";$INSTDIR\ipfs" "" "+" $sysEnvPath
	${WordReplace} $sysEnvPath "$INSTDIR\ipfs\ipfs.exe" "" "+" $sysEnvPath
	${WordReplace} $sysEnvPath "$INSTDIR\ipfs" "" "+" $sysEnvPath
	;MessageBox MB_OK '$sysEnvPath - $INSTDIR'
	${WordReplace} $sysEnvPath ";$INSTDIR\dbc.exe" "" "+" $sysEnvPath
	${WordReplace} $sysEnvPath ";$INSTDIR" "" "+" $sysEnvPath
	${WordReplace} $sysEnvPath "$INSTDIR\dbc.exe" "" "+" $sysEnvPath
	${WordReplace} $sysEnvPath "$INSTDIR" "" "+" $sysEnvPath
	;MessageBox MB_OK '$sysEnvPath - $INSTDIR'
	WriteRegExpandStr HKLM "${PATH_KEY}" "Path" "$sysEnvPath"


  DeleteRegKey ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}"
  SetAutoClose true
SectionEnd

#-- ���� NSIS �ű��༭�������� Function ���α�������� Section ����֮���д���Ա��ⰲװ�������δ��Ԥ֪�����⡣--#

Function checkDBCRunning
;	LoopC:
;	StrCpy $0 0
;	ExecWait "tasklist | findstr dbc.exe > %TEMP%/check_res_dbc.txt" $0
;	MessageBox MB_OK "tasklist: $0"
;	IntCmp $0 0 +1 +2 +2
;	Goto LoopC
	FindProcDLL::FindProc "dbc.exe"
	Pop $R0
	IntCmp $R0 0 +3 +1
	MessageBox MB_OKCANCEL|MB_ICONEXCLAMATION "dbc.exe is running, please stop it and try again." IDOK -4 IDCANCEL end
	end:
	Abort
FunctionEnd

Function checkIPFSRunning
  loopDR:
	FindProcDLL::FindProc "ipfs.exe"
	IntCmp $R0 1 +1 +3
	MessageBox MB_OKCANCEL|MB_ICONEXCLAMATION "$(^Name) is running, please stop it and try again." IDOK loopDR IDCANCEL end
	end:
	Abort
FunctionEnd


Function checkInstalling
	System::Call 'kernel32::CreateMutexA(i 0, i 0, t "dbc_mutex_installing") i .r1 ?e'
	Pop $R0
	StrCmp $R0 0 +3
	MessageBox MB_OK|MB_ICONEXCLAMATION "dbc installation program is running."
	Abort
FunctionEnd

Function un.checkDBCRunning
  loopDR:
	FindProcDLL::FindProc "dbc.exe"
	IntCmp $R0 1 0 +4
	MessageBox MB_OKCANCEL|MB_ICONEXCLAMATION "$(^Name) is running, please stop it and try again." IDOK loopDR IDCANCEL end
	end:
	Abort
FunctionEnd

Function un.checkInstalling
	System::Call 'kernel32::CreateMutexA(i 0, i 0, t "dbc_mutex_installing") i .r1 ?e'
	Pop $R0
	StrCmp $R0 0 +3
    MessageBox MB_OK|MB_ICONEXCLAMATION "dbc installation program is running."
    Abort
FunctionEnd

Function .onInit
	Call checkDBCRunning
	Call checkIPFSRunning
	Call checkInstalling
FunctionEnd

Function .onInstSuccess
	StrCpy $boolInstallSuccess True
FunctionEnd

Function .onInstFailed
	StrCpy $boolInstallSuccess False
FunctionEnd

Function .onGUIEnd
  ${If} $boolInstallSuccess == True
    RMDir /r "${IPFS_CONFIG_PATH}"
		nsExec::Exec '"$INSTDIR\ipfs\ipfs.exe" init'
		CopyFiles /FILESONLY /SILENT "$INSTDIR\ipfs\swarm.key" "${IPFS_CONFIG_PATH}\swarm.key"
		Exec	 '"$INSTDIR\ipfs\ipfs.exe" daemon'
		Sleep 45000
		nsExec::Exec '"$INSTDIR\ipfs\ipfs.exe" bootstrap rm --all'
		nsExec::Exec '"$INSTDIR\ipfs\ipfs.exe" bootstrap add /ip4/18.223.4.215/tcp/4001/ipfs/QmeZR4HygPvdBvheovYR4oVTvaA4tWxDPTgskkPWqbjkGy'
		nsExec::Exec '"$INSTDIR\ipfs\ipfs.exe" bootstrap add /ip4/49.51.49.192/tcp/4001/ipfs/QmRVgowTGwm2FYhAciCgA5AHqFLWG4AvkFxv9bQcVB7m8c'
;		nsExec::Exec '"$INSTDIR\ipfs\ipfs.exe" bootstrap add /ip4/49.51.49.145/tcp/4001/ipfs/QmTBEUexjn5PQfwnTFccREdXdnh7qB1qvx2To2ESE8UJ5T'
		nsExec::Exec '"$INSTDIR\ipfs\ipfs.exe" bootstrap add /ip4/122.112.243.44/tcp/4001/ipfs/QmPC1D9HWpyP7e9bEYJYbRov3q2LJ35fy5QnH19nb52kd5'
	${EndIf}
FunctionEnd


Function un.onInit
;  Call un.checkDBCRunning
  Call un.checkInstalling
  MessageBox MB_ICONQUESTION|MB_YESNO|MB_DEFBUTTON2 "Will you remove $(^Name) ?" IDYES +2
  Abort
FunctionEnd

Function un.onUninstSuccess
  HideWindow
  MessageBox MB_ICONINFORMATION|MB_OK "$(^Name) has removed from your computer��"
FunctionEnd

