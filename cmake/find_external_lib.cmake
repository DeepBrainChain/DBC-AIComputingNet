if (UNIX AND NOT APPLE)
    set(LINUX TRUE)
endif ()

if (LINUX)
    SET(Boost_INCLUDE_DIR　"/usr/local/include/boost")
    SET(Boost_LIBRARY_DIR  "/usr/local/lib")

    SET(Boost_USE_STATIC_LIBS     ON)
    SET(Boost_USE_STATIC_RUNTIME  ON)
    set(Boost_USE_MULTITHREADED   ON)

    find_package(Boost 1.66 REQUIRED COMPONENTS log log_setup thread date_time system filesystem  exception program_options serialization signals serialization chrono unit_test_framework context)
endif()

if (APPLE)
    SET(Boost_USE_STATIC_LIBS     ON)
    #SET(Boost_USE_STATIC_RUNTIME  ON)
    set(Boost_USE_MULTITHREADED   ON)

    find_package(Boost 1.66 REQUIRED COMPONENTS log log_setup thread date_time system filesystem  exception program_options serialization  serialization chrono unit_test_framework )
endif()

if (Boost_FOUND)
    message(STATUS "Boost_VERSION: ${Boost_VERSION} ${Boost_LIBRARIES}")
    include_directories(${Boost_INCLUDE_DIRS})
    link_directories(${Boost_LIBRARY_DIR})
else ()
    message(FATAL " fail to find boost library")
endif ()
