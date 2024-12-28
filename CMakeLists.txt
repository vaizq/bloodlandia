cmake_minimum_required(VERSION 3.10)

# Project Name
project(PongOnline)

# Add Raylib package
find_package(raylib REQUIRED)

# Add source files
add_executable(${PROJECT_NAME} src/main.cpp src/game.cpp)

target_include_directories(${PROJECT_NAME} PRIVATE src)

# Link Raylib to your project
target_link_libraries(${PROJECT_NAME} raylib)

# Optionally enable optimizations for Release builds
# set(CMAKE_BUILD_TYPE Release)

