cmake_minimum_required(VERSION 3.20)
project(svm VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Luôn tự động xuất compile_commands.json cho clangd / C++ IntelliSense
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Rust Core C-ABI library
add_subdirectory(core)

# UI layer Qt6 Widgets
add_subdirectory(ui)
