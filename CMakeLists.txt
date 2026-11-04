cmake_minimum_required(VERSION 3.20)
project(svm VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Rust Core C-ABI library
add_subdirectory(core)

# UI layer will be added when Qt6 UI scaffold is connected:
# add_subdirectory(ui)
