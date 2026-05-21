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

# C++17 build with permissive deprecation handling so legacy boost::signals /
# boost::asio::io_service usages don't break the build. New code (container/,
# ddn_client/, chain_sidecar_client/) should NOT introduce new boost::signals
# or io_service calls; use boost::signals2 / io_context instead.
add_compile_options(
    -Wno-deprecated-declarations
    -Wno-deprecated-copy
)
