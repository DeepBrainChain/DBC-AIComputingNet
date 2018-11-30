@rem created by Allan
@rem 2018/08/04

@echo off

..\ipfs\ipfs.exe init
copy ..\ipfs\swarm.key %USERPROFILE%\.ipfs\swarm.key

@rem download from github
cd ..
set parent_path=%cd%
cd tool
"%parent_path%/tool/curl.exe" -o "boot_nodes" -L "https://github.com/DeepBrainChain/deepbrainchain-release/releases/download/latest/bootstrap_nodes"
if errorlevel 1 (
	echo failed to download boot_nodes
	pause > nul
	exit
)

setlocal enabledelayedexpansion
set /a n=0
for /f %%i in (boot_nodes) do (
	echo %%i|findstr "ip4" > nul
	if errorlevel 1 (
		echo failed to download boot_nodes.
		pause > nul
		exit
	)
	set ADDR[!n!]=%%i
	set /a n+=1
)
::del "boot_nodes"
echo count of IPs: %n%
..\ipfs\ipfs.exe bootstrap rm --all
::check if got IPs
if %n% EQU 0 (
	@rem add default nodes to dbc network
	..\ipfs\ipfs.exe bootstrap add /ip4/114.116.19.45/tcp/4001/ipfs/QmPEDDvtGBzLWWrx2qpUfetFEFpaZFMCH9jgws5FwS8n1H
	..\ipfs\ipfs.exe bootstrap add /ip4/49.51.49.192/tcp/4001/ipfs/QmRVgowTGwm2FYhAciCgA5AHqFLWG4AvkFxv9bQcVB7m8c
	..\ipfs\ipfs.exe bootstrap add /ip4/49.51.49.145/tcp/4001/ipfs/QmPgyhBk3s4aC4648aCXXGigxqyR5zKnzXtteSkx8HT6K3
	..\ipfs\ipfs.exe bootstrap add /ip4/122.112.243.44/tcp/4001/ipfs/QmPC1D9HWpyP7e9bEYJYbRov3q2LJ35fy5QnH19nb52kd5
	..\ipfs\ipfs.exe daemon
	exit
)

@rem add to dbc network
set /a m=%n%-1
echo %m%
for /L %%i in (0,1,%m%) do (
	::echo !ADDR[%%i]!
	..\ipfs\ipfs.exe bootstrap add !ADDR[%%i]!
)
..\ipfs\ipfs.exe daemon