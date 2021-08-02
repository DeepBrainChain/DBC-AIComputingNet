if (UNIX AND NOT APPLE)
    set(LINUX TRUE)
endif ()

# 3rd library path
if (APPLE)
    set(3RD_LIB_PATH ${CMAKE_SOURCE_DIR}/src/3rd/lib/macosx)
endif ()

if (LINUX)
    set(3RD_LIB_PATH ${CMAKE_SOURCE_DIR}/src/3rd/lib/linux)
endif ()
