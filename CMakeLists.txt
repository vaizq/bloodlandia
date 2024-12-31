cmake_minimum_required(VERSION 3.10)

# Project Name
project(PongOnline)

# Set the C++ standard to C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Optionally set build type to Release
#set(CMAKE_BUILD_TYPE Release)

if(WIN32)
	set(raylib_LIBRARY ~/Software/raylib/build-windows)
endif()

# Add Raylib package
# Find Raylib if it's installed
find_package(raylib REQUIRED)

set(COMMON_SOURCES internal/game.cpp)

# Add source files for client and server
add_executable(client src/client/main.cpp ${COMMON_SOURCES})
add_executable(server src/server/main.cpp ${COMMON_SOURCES})


if(WIN32)
	message("MEAN AND LEAN!")
	target_compile_definitions(client PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
	target_compile_definitions(server PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
	target_link_libraries(client PRIVATE ws2_32 gdi32 raylib)
	target_link_libraries(server PRIVATE ws2_32 gdi32 raylib)
else()
	target_link_libraries(client raylib)
	target_link_libraries(server raylib)
endif()



# Include directories for client and server
target_include_directories(client PRIVATE internal /usr/local/include/)
target_include_directories(server PRIVATE internal /usr/local/include/)
