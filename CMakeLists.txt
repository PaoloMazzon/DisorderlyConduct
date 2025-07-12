cmake_minimum_required(VERSION 3.31)
project(jamgame)

set(CMAKE_C_STANDARD 23)
set(CMAKE_CXX_STANDARD 17)

# Compile the engine to link later
add_subdirectory(Octarine/OctarineEngine)

# Find relavant source files
file(GLOB C_FILES src/*.c)

# Include directories
include_directories(include/ Octarine/OctarineEngine/include)

# Final executable
add_executable(${PROJECT_NAME} main.c ${C_FILES})
target_link_libraries(${PROJECT_NAME} PRIVATE OctarineEngine)