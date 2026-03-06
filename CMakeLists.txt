cmake_minimum_required (VERSION 3.8)
project(CS2-AI)

# --- START OF FIX ---
# This tells CMake exactly where to look for Qt. 
# IMPORTANT: Verify this path matches your actual installation folder!
set(CMAKE_PREFIX_PATH "C:/Qt/6.10.2/msvc2022_64")
# --- END OF FIX ---

set(CMAKE_CXX_STANDARD 17)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

add_subdirectory(CS2-AI)

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT CS2_AI)