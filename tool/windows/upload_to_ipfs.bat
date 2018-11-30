@rem created by Allan
@rem 2018/08/04
:: Example:
:: upload_to_ipfs.bat "file name";
:: or upload_to_ipfs.bat "file directory";
:: or run upload_to_ipfs.bat directly, then input full name of file or directory;
::----------------------------------------


::------upload process------
@echo off
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
	:start
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
"%parent_path%/ipfs/ipfs.exe" add -r "%file_dir%" > "%temp%\tmp.txt"

setlocal enabledelayedexpansion
set hash_code=
for /f "tokens=2 delims= " %%i in (%temp%\tmp.txt) do (
 set hash_code=%%i
 )
del "%temp%\tmp.txt"
echo "hash_code: "
echo %hash_code%
if "%hash_code%"=="" (
	echo "add to ipfs failed, restart ipfs may fix it."
	goto err_exit
)
echo ------------------------------------------
echo begin to upload "%file_dir%" to ipfs ......

::set IP[0]=122.112.243.44
::set IP[1]=18.223.4.215
::download bootstrap_nodes
"%parent_path%/tool/curl.exe" -o "bootstrap_nodes" -L "https://github.com/DeepBrainChain/deepbrainchain-release/releases/download/latest/bootstrap_nodes"

set /a n=0
for /f "tokens=2 delims=/" %%i in (bootstrap_nodes) do (	
	::echo !n!, %%i	
	set IP[!n!]=%%i
	set /a n+=1
	)
del "bootstrap_nodes"
echo count of IPs: %n%
::check if got IPs
if %n% EQU 0 (
	echo "no ipfs peer address"
	pause > nul
	exit
)

set /a index=%random%%%%n%
set first_idx=%index%
:checkIP
echo use IP[%index%]: !IP[%index%]!
"%cd%/curl.exe" --connect-timeout 5 -Y 5120 -y 30 http:/!IP[%index%]!:5001/api/v0/get?arg=/ipfs/%hash_code% --output nul
if errorlevel 1 (
	echo !IP[%index%]! is not available.
	set /a tmp_index=!index!+1
	set /a index=!tmp_index!%%%n%
	echo next ip: !index!
	if !first_idx! equ !index! (
		echo "no available ipfs peers"
		echo failed to add "%file_dir%"
		pause > nul
		exit
	)
	goto checkIP	
)

:succ
echo upload "%file_dir%" finished, hash_code=%hash_code%.
echo ------------------------------------------

:err_exit
pause
set file_dir=
goto start