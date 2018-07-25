if(__COMPILER_OPTION_FILE_VAR__)
    return()
endif()
set(__COMPILER_OPTION_FILE_VAR__ TRUE)


#
#   Define new variables
#       CXX_PREPROCESS_FLAGS:   prepocess options, example -Dxxx
#       CXX_COMPILE_FLAGS:      cxx compiler options, example -O2
#       CXX_LINK_FLAGS:         link options, example -pthread
#


if (UNIX AND NOT APPLE)
    set(LINUX TRUE)
endif ()



if(APPLE)
    set(CXX_FLAGS "-std=c++11 -Wno-inconsistent-missing-override")
    set(CXX_PREPROCESS_FLAGS "-DMAC_OSX -D_DARWIN_C_SOURCE -DEVENT__HAVE_OPENSSL")
endif()

if(LINUX)
    set(CMAKE_CXX_COMPILER "/usr/bin/g++")
    set(CXX_FLAGS "-std=c++11 -fpie")
    set(CXX_PREPROCESS_FLAGS "${CXX_PREPROCESS_FLAGS} -D__linux__ -D__x86_64__ -DEVENT__HAVE_OPENSSL")
#	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -no-pie")
endif()


# common options

if ((DEBUG_MODE) OR (CMAKE_BUILD_TYPE STREQUAL "Debug"))
    set(CXX_FLAGS "${CXX_FLAGS} -O0 -Wall -g -ggdb -pthread")
    set(CXX_PREPROCESS_FLAGS "${CXX_PREPROCESS_FLAGS} -DDEBUG -D_REENTRANT -pthread")
else ()
    set(CXX_FLAGS "${CXX_FLAGS} -O3 -Wall -pthread")
    set(CXX_PREPROCESS_FLAGS "${CXX_PREPROCESS_FLAGS} -DNDEBUG -pthread")
endif ()


# dbc 3rd library path
if (APPLE)
    set(DBC_3RD_LIB_ROOT_PATH ${CMAKE_SOURCE_DIR}/lib/macosx)
endif ()

if (LINUX)
    set(DBC_3RD_LIB_ROOT_PATH ${CMAKE_SOURCE_DIR}//lib/linux)
endif ()


#
#   add_defintions() add preprocessor options
#

#set(CMAKE_CXX_FLAGS "${CXX_FLAGS}" CACHE STRING "compile flags" FORCE)
#add_compile_options(${CXX_FLAGS})
add_definitions("${CXX_PREPROCESS_FLAGS} ${CXX_FLAGS}")
