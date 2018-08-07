@rem created by Allan
@rem 2018/08/04
:: Example:
:: upload_to_ipfs.bat "file name";
:: or upload_to_ipfs.bat "file directory";
:: or run upload_to_ipfs.bat directly, then input full name of file or directory;
::----------------------------------------

::-----get UAC-----
@echo off 
>nul 2>&1 "%SYSTEMROOT%\system32\cacls.exe" "%SYSTEMROOT%\system32\config\system" 
if '%errorlevel%' NEQ '0' ( 
goto UACPrompt 
) else ( goto gotAdmin ) 
:UACPrompt 
echo Set UAC = CreateObject^("Shell.Application"^) > "%temp%\getadmin.vbs" 
echo UAC.ShellExecute "%~s0", "", "", "runas", 1 >> "%temp%\getadmin.vbs" 
"%temp%\getadmin.vbs"
del "%temp%\getadmin.vbs"
exit /B 
:gotAdmin
if exist "%temp%\getadmin.vbs" (del "%temp%\getadmin.vbs")

::------upload process------
@echo off
:start
@rem echo "check param cnt"
set /a cnt=0
for %%a in (%*) do set /a cnt+=1
if %cnt% GTR 1 (
	echo "you should append only one param"
	pause>nul
	exit
	)
if %cnt% EQU 1 (
	set file_dir=%1
	) else (
	set /p file_dir=please input file or directory to upload to ipfs:
	)

if not defined file_dir goto start

@rem echo "check file or dir exist"
if not exist %file_dir% (
	echo "%file_dir%" not exist.
	pause>nul
	goto start
	)

@rem echo "start to add file to ipfs"
cd ..
set parent_path=%cd%
cd tool
"%parent_path%/ipfs/ipfs.exe" add -r %file_dir% > tmp.txt

setlocal enabledelayedexpansion
set hash_code=
for /f "tokens=2 delims= " %%i in (tmp.txt) do (
 set hash_code=%%i
 )
del tmp.txt
echo "hash_code: "
echo %hash_code%
echo ------------------------------------------
echo begin to upload "%file_dir%" to ipfs ......

set IP[0]=122.112.243.44
set IP[1]=49.51.49.145
set IP[2]=49.51.49.192

set /a index=%random%%%3
set first_idx=%index%
if defined hash_code (
	:checkIP
	ping !IP[%index%]! > nul
	::echo %errorlevel%	
	if errorlevel 1 (
		set /a tmp_index=!index!+1
		set /a index=!tmp_index!%%3
		echo !index!
		if %first_idx% equ %index% (
			echo "no available ipfs peers"
			pause > nul
			exit
			)
		goto checkIP
		)

	"%cd%/curl.exe" http://!IP[%index%]!:5001/api/v0/get?arg=/ipfs/%hash_code% --output nul

	::echo %errorlevel%
	REM for /f %%i in (%cd%/output.txt) do (
		REM echo %%i | findstr "Failed" > nul
		REM echo %errorlevel%
		REM if errorlevel 1 (
			REM echo "failed to add %file_dir%"			
			REM pause>nul
			REM goto err_exit
			REM )
		REM )
	if errorlevel 1 (
		echo failed to add "%file_dir%"
		echo !IP[%index%]! is not available.
		goto err_exit
		)
	) else (
	echo "add to ipfs failed, restart ipfs may fix it."
	goto err_exit
)
echo upload "%file_dir%" finished, hash_code=%hash_code%.
echo ------------------------------------------

:err_exit
pause
set file_dir=
goto start
::del output.txt
::exit