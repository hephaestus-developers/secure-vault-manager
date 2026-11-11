cmake_minimum_required(VERSION 3.20)
project(svm_core_native LANGUAGES C CXX)

set(CARGO_CMD cargo)
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CARGO_BUILD_FLAG "--release")
    set(CARGO_TARGET_DIR "release")
else()
    set(CARGO_BUILD_FLAG "")
    set(CARGO_TARGET_DIR "debug")
endif()

if(WIN32)
    set(LIB_PREFIX "")
    set(LIB_SUFFIX ".dll")
    set(LIB_IMPORT_SUFFIX ".dll.lib")
elseif(APPLE)
    set(LIB_PREFIX "lib")
    set(LIB_SUFFIX ".dylib")
else()
    set(LIB_PREFIX "lib")
    set(LIB_SUFFIX ".so")
endif()

set(SVM_CORE_LIB_NAME "${LIB_PREFIX}svm_core${LIB_SUFFIX}")
set(SVM_CORE_LIB_PATH "${CMAKE_CURRENT_SOURCE_DIR}/target/${CARGO_TARGET_DIR}/${SVM_CORE_LIB_NAME}")

add_custom_target(svm_core_cargo
    COMMAND ${CARGO_CMD} build --package svm-core --package svm-native-host --package svm-cli ${CARGO_BUILD_FLAG}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/core"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/target/${CARGO_TARGET_DIR}/svm-native-host"
        "${CMAKE_BINARY_DIR}/core/svm-native-host"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/target/${CARGO_TARGET_DIR}/svm-cli"
        "${CMAKE_BINARY_DIR}/core/svm-cli"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    BYPRODUCTS ${SVM_CORE_LIB_PATH}
        "${CMAKE_CURRENT_SOURCE_DIR}/target/${CARGO_TARGET_DIR}/svm-native-host"
        "${CMAKE_BINARY_DIR}/core/svm-native-host"
        "${CMAKE_CURRENT_SOURCE_DIR}/target/${CARGO_TARGET_DIR}/svm-cli"
        "${CMAKE_BINARY_DIR}/core/svm-cli"
    COMMENT "Compiling Rust Core (svm_core, svm-native-host, svm-cli) using Cargo..."
)

add_library(svm_core SHARED IMPORTED GLOBAL)
add_dependencies(svm_core svm_core_cargo)
set_target_properties(svm_core PROPERTIES
    IMPORTED_LOCATION "${SVM_CORE_LIB_PATH}"
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

install(FILES "${SVM_CORE_LIB_PATH}"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(PROGRAMS
    "${CMAKE_BINARY_DIR}/core/svm-native-host"
    "${CMAKE_BINARY_DIR}/core/svm-cli"
    DESTINATION ${CMAKE_INSTALL_BINDIR}
)
