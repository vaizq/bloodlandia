cmake_minimum_required(VERSION 3.10)

# Project Name
project(PongOnline)

# Set the C++ standard to C++20
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_BUILD_TYPE Release)

set(BUILD_TESTS OFF)
set(BUILD_SERVER ON)

# Optionally set build type to Release
#set(CMAKE_BUILD_TYPE Release)

Include(FetchContent)

# Add Raylib package
# Find Raylib if it's installed
find_package(raylib)
if (NOT raylib_FOUND) # If there's none, fetch and build raylib
  FetchContent_Declare(
    raylib
    DOWNLOAD_EXTRACT_TIMESTAMP OFF
    URL https://github.com/raysan5/raylib/archive/refs/tags/5.5.tar.gz
  )
  FetchContent_GetProperties(raylib)
  if (NOT raylib_POPULATED) # Have we downloaded raylib yet?
    set(FETCHCONTENT_QUIET NO)
    FetchContent_MakeAvailable(raylib)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE) # don't build the supplied examples
  endif()
endif()

FetchContent_Declare(
    asio
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG        asio-1-28-0 # Replace with the desired version
)

FetchContent_MakeAvailable(asio)

# Add source files for client and server
add_executable(client src/client/main.cpp src/client/game.cpp)

target_link_libraries(client raylib)
target_include_directories(client PRIVATE internal ${asio_SOURCE_DIR}/asio/include)
target_compile_definitions(client PRIVATE ASIO_STANDALONE)

if (BUILD_TESTS)
  FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.4.0 # or a later release
  )

  FetchContent_MakeAvailable(Catch2)

  add_executable(tests test/rand.cpp)
  target_link_libraries(tests raylib Catch2::Catch2WithMain)
  target_include_directories(tests PRIVATE internal)
endif()

if (BUILD_SERVER)
  add_executable(server src/server/main.cpp)
  target_link_libraries(server raylib)
  target_include_directories(server PRIVATE internal ${asio_SOURCE_DIR}/asio/include)
  target_compile_definitions(server PRIVATE ASIO_STANDALONE)
endif()
