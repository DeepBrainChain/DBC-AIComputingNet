cmake_minimum_required (VERSION 3.10)
project("dbcproj")

# Bumped C++11 -> C++17 in feature/container-mode (2026-05-21):
#   - std::optional / std::variant / std::filesystem / structured bindings
#     for the new container/ddn_client/chain_sidecar_client modules
#   - Existing code (Boost 1.66 era) compiles cleanly under -std=c++17;
#     boost::signals / io_service deprecated but still present in 1.66
#   - If we later upgrade Boost to >=1.69 we'll need to swap boost::signals ->
#     boost::signals2 and io_service -> io_context. Not required for MVP-1.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(cmake/compiler_option.cmake)
include(cmake/find_external_lib.cmake)

add_subdirectory(src)
