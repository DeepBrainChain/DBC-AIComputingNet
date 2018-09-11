#!/bin/bash
#---------------------------------------------------
#
#          DeepBrainChain Unit Test
#   
#-------------------------------------------------

BUILD_ROOT=./cmake-build-debug
OUTPUT_DIR=../../output

function pre_env(){
	 [ ! -d "$BUILD_ROOT" ] && mkdir $BUILD_ROOT
	 cd $BUILD_ROOT

	 echo "cmake"
	 cmake DEBUG_MODE=ON  --build ../.. > ../build_cmake.log 2>&1
	 if [ $? -ne 0 ]
	 then
    		echo "fail: see build_cmake.log for more details"
    		exit 1
	 fi

	 echo "-----------------------------------------------------"
	 echo "[make dbc unit test]"
	 echo "-----------------------------------------------------"
}

function build_unit_test(){
	app=$1
	logfile="build_${app}_ut.log"
	echo "make -j 6 $app 2>&1 | tee  ../$logfile"
	make -j 6 $app 2>&1 | tee  ../$logfile
	if [ ${PIPESTATUS[0]} -ne 0 ]
	then
    		echo "fail: see ${logfile} for more details"
    		exit 1
	fi
}	


function run_unit_test(){
	app=$1
	app_path="${OUTPUT_DIR}/${app}"
	#app=${app_path##*/}
	
	echo "________________________________________________"
	echo "$app_path  -r detailed -l all ${@:1}"
	echo "_______________________________________________"

	$app_path  -r detailed -l all ${@:1}
}	

function main(){
	pre_env
	
	build_unit_test  dbc_core_test
	build_unit_test  dbc_core_network_test
	build_unit_test  dbc_service_core_test
	build_unit_test  dbc_service_test

	
	echo
	echo "================================== run ====================================="
	echo 

	run_unit_test dbc_core_test
	run_unit_test dbc_core_network_test
	run_unit_test dbc_service_core_test
	run_unit_test dbc_service_test

}


main
