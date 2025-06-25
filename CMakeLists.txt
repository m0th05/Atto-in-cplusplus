# --- Standard CMake setup ---
cmake_minimum_required(VERSION 3.20)
project(atto)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Find Dependencies ---
# This tells CMake to find the nlohmann_json and ftxui libraries
find_package(nlohmann_json 3.2.0 REQUIRED)
find_package(ftxui REQUIRED)

# --- Add Your Executable ---
add_executable(atto
        atto.cpp
        editor.cpp
        editor.hpp
)

# --- Link Libraries (The Important Part) ---
# This links your program to the libraries. Note the corrected FTXUI target name.
target_link_libraries(atto PRIVATE
        nlohmann_json::nlohmann_json
        ftxui::component # Corrected from FTXUI::ftxui-component
)

# --- Add Static Linking Flags (The Fix) ---
# These flags tell the compiler to link statically, preventing DLL issues.
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")