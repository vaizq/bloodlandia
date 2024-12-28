cmake_minimum_required(VERSION 3.10)

# Project Name
project(PongOnline)

# Add Raylib package
find_package(raylib REQUIRED)

set(COMMON_SOURCES internal/game.cpp)

# Add source files
add_executable(client src/client/main.cpp ${COMMON_SOURCES})
add_executable(server src/server/main.cpp ${COMMON_SOURCES})

target_include_directories(client PRIVATE internal)
target_include_directories(server PRIVATE internal)


# Link Raylib to your project
target_link_libraries(client raylib)
target_link_libraries(server raylib)


# Optionally enable optimizations for Release builds
# set(CMAKE_BUILD_TYPE Release)

