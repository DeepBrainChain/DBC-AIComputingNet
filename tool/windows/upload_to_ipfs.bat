@rem created by Allan
@rem 2018/08/04
:: Example:
:: upload_to_ipfs.bat "file name"
:: upload_to_ipfs.bat "file directory"
::----------------------------------------

@echo off

@rem echo "check param cnt"
set /a cnt=0
for %%a in (%*) do set /a cnt+=1
if %cnt% LSS 1 (
	echo "you should append a file or directory as a param"
	pause>nul
	exit
	)
if %cnt% GTR 1 (
	echo "you should append only one param"
	pause>nul
	exit
	)

@rem echo "check file or dir exist"
if not exist %1 (
	echo "%1 not exist"
	pause>nul
	exit	
	)

@rem echo "start to add file to ipfs"
set file_dir=%1
cd ..
set parent_path=%cd%
cd tool
%parent_path%/ipfs/ipfs.exe add -r %file_dir% > tmp.txt

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

	%cd%/curl.exe http://!IP[%index%]!:5001/api/v0/get?arg=/ipfs/%hash_code% --output nul

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
		goto err_exit
		)
	) else (
	echo "add to ipfs failed, restart ipfs may fix it."
	goto err_exit
)
echo upload "%file_dir%" finished, hash_code=%hash_code%.
echo ------------------------------------------
pause > nul

:err_exit
pause > nul
::del output.txt
::exit