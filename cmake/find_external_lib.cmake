
if(__FIND_EXTERNAL_FILE_VAR__)
    return()
endif()
set(__FIND_EXTERNAL_FILE_VAR__ TRUE)


if (UNIX AND NOT APPLE)
    set(LINUX TRUE)
endif ()


if (LINUX)
    SET(Boost_INCLUDE_DIR　"/usr/local/include/boost")
    SET(Boost_LIBRARY_DIR  "/usr/local/lib")

    SET(Boost_USE_STATIC_LIBS     ON)
    SET(Boost_USE_STATIC_RUNTIME  ON)
    set(Boost_USE_MULTITHREADED   ON)

    set(PREFER_BOOST_VER 1.66)
endif()

if (APPLE)
    SET(Boost_USE_STATIC_LIBS     ON)
#    SET(Boost_USE_STATIC_RUNTIME  ON)
    set(Boost_USE_MULTITHREADED   ON)
    set(PREFER_BOOST_VER 1.66)

#    add_definitions("-DBOOST_LOG_DYN_LINK")  # boost specific macro
endif()



find_package(Boost ${PREFER_BOOST_VER} REQUIRED COMPONENTS log log_setup thread date_time system filesystem  exception program_options serialization signals serialization chrono unit_test_framework context)

if (Boost_FOUND)
    message(STATUS "Boost_VERSION: ${Boost_VERSION} ${Boost_LIBRARIES}")

    include_directories(${Boost_INCLUDE_DIRS})
    link_directories(${Boost_LIBRARY_DIR})
else ()
    message(FATAL " fail to find boost library")

endif ()
