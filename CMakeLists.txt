cmake_minimum_required(VERSION 3.22)
project(svm VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)

# Add Corrosion
add_subdirectory(cmake/corrosion)

corrosion_import_crate(
    MANIFEST_PATH "${CMAKE_CURRENT_SOURCE_DIR}/crates/svm-core/Cargo.toml"
    CRATES svm-core
    CRATE_TYPES cdylib
    IMPORTED_CRATES svm_core_imported_targets
)
message(STATUS "Corrosion imported targets for svm-core: ${svm_core_imported_targets}")

corrosion_import_crate(
    MANIFEST_PATH "${CMAKE_CURRENT_SOURCE_DIR}/crates/svm-cli/Cargo.toml"
    CRATES svm-cli
    IMPORTED_CRATES svm_cli_targets
)
message(STATUS "Corrosion imported targets for svm-cli: ${svm_cli_targets}")

corrosion_import_crate(
    MANIFEST_PATH "${CMAKE_CURRENT_SOURCE_DIR}/crates/svm-native-host/Cargo.toml"
    CRATES svm-native-host
    IMPORTED_CRATES svm_host_targets
)
message(STATUS "Corrosion imported targets for svm-native-host: ${svm_host_targets}")

# CXX generated headers directory & source
set(CXX_BRIDGE_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/target/cxxbridge")
set(CXX_BRIDGE_HEADER "${CXX_BRIDGE_INCLUDE_DIR}/svm-core/src/bridge.rs.h")
set(CXX_BRIDGE_SRC "${CXX_BRIDGE_INCLUDE_DIR}/svm-core/src/bridge.rs.cc")

add_custom_command(
    OUTPUT "${CXX_BRIDGE_SRC}" "${CXX_BRIDGE_HEADER}"
    COMMAND cargo build --package svm-core
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/crates/svm-core/src/bridge.rs"
            "${CMAKE_CURRENT_SOURCE_DIR}/crates/svm-core/build.rs"
    COMMENT "Generating CXX bridge bindings (bridge.rs.cc / bridge.rs.h)"
)

add_custom_target(generate_cxx_bridge
    DEPENDS "${CXX_BRIDGE_SRC}" "${CXX_BRIDGE_HEADER}"
)

set_source_files_properties(${CXX_BRIDGE_SRC} ${CXX_BRIDGE_HEADER} PROPERTIES
    GENERATED TRUE
    SKIP_AUTOGEN ON
    SKIP_AUTOMOC ON
)

# ==============================================================================
# Window Infrastructure Framework (In-Tree Static Library)
# ==============================================================================
add_library(window_framework STATIC
    src/ui/window/FramelessUtil.hpp
    src/ui/window/FramelessWindow.hpp
    src/ui/window/FramelessWindow.cpp
    src/ui/window/FramelessDialog.hpp
    src/ui/window/FramelessDialog.cpp
    src/ui/window/WindowNode.hpp
    src/ui/window/WindowNode.cpp
    src/ui/window/WindowManager.hpp
    src/ui/window/WindowManager.cpp
    src/ui/window/SingleInstanceController.hpp
    src/ui/window/SingleInstanceController.cpp
)

target_include_directories(window_framework PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(window_framework PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
)

target_compile_definitions(window_framework PRIVATE
    $<$<CONFIG:Release>:QT_NO_DEBUG_OUTPUT QT_NO_INFO_OUTPUT>
)

# ==============================================================================
# Desktop Application Executable
# ==============================================================================
set(SOURCES
    src/core/cxx.cc
    ${CXX_BRIDGE_SRC}
    src/core/LogCategories.hpp
    src/core/LogCategories.cpp
    src/core/CoreBridge.hpp
    src/core/CoreBridge.cpp
    src/ui/MainWindow.hpp
    src/ui/MainWindow.cpp
    src/ui/app/ClipboardManager.hpp
    src/ui/app/ClipboardManager.cpp
    src/ui/app/LanguageManager.hpp
    src/ui/app/LanguageManager.cpp
    src/ui/app/SettingsManager.hpp
    src/ui/app/SettingsManager.cpp
    src/ui/app/ThemeManager.hpp
    src/ui/app/ThemeManager.cpp
    src/ui/app/TrayManager.hpp
    src/ui/app/TrayManager.cpp
    src/ui/app/VaultManager.hpp
    src/ui/app/VaultManager.cpp
    src/ui/components/ComboboxPopupHelper.hpp
    src/ui/components/ConfirmDeleteDialog.hpp
    src/ui/components/ConfirmDeleteDialog.cpp
    src/ui/components/ElidedLabel.hpp
    src/ui/components/InWindowCombobox.hpp
    src/ui/components/NotificationDialog.hpp
    src/ui/components/NotificationDialog.cpp
    src/ui/components/UnifiedHeaderBar.hpp
    src/ui/components/UnifiedHeaderBar.cpp
    src/ui/models/VaultItem.hpp
    src/ui/models/VaultTypes.hpp
    src/ui/views/about/AboutDialog.hpp
    src/ui/views/about/AboutDialog.cpp
    src/ui/views/about/KeyboardShortcutsDialog.hpp
    src/ui/views/about/KeyboardShortcutsDialog.cpp
    src/ui/views/detail/CustomFieldRowWidget.hpp
    src/ui/views/detail/CustomFieldRowWidget.cpp
    src/ui/views/detail/CustomFieldsWidget.hpp
    src/ui/views/detail/CustomFieldsWidget.cpp
    src/ui/views/detail/DetailHeaderWidget.hpp
    src/ui/views/detail/DetailHeaderWidget.cpp
    src/ui/views/detail/DetailInspectorView.hpp
    src/ui/views/detail/DetailInspectorView.cpp
    src/ui/views/detail/EmptyStateWidget.hpp
    src/ui/views/detail/EmptyStateWidget.cpp
    src/ui/views/detail/FieldRowWidget.hpp
    src/ui/views/detail/FieldRowWidget.cpp
    src/ui/views/detail/RevisionHistoryDialog.hpp
    src/ui/views/detail/RevisionHistoryDialog.cpp
    src/ui/views/detail/TotpWidget.hpp
    src/ui/views/detail/TotpWidget.cpp
    src/ui/views/item_list/ItemCardDelegate.hpp
    src/ui/views/item_list/ItemCardDelegate.cpp
    src/ui/views/item_list/ItemListEmptyWidget.hpp
    src/ui/views/item_list/ItemListEmptyWidget.cpp
    src/ui/views/item_list/ItemListModel.hpp
    src/ui/views/item_list/ItemListModel.cpp
    src/ui/views/item_list/ItemListView.hpp
    src/ui/views/item_list/ItemListView.cpp
    src/ui/views/item_list/ItemSortFilterProxy.hpp
    src/ui/views/item_list/ItemSortFilterProxy.cpp
    src/ui/views/lock/LockScreenView.hpp
    src/ui/views/lock/LockScreenView.cpp
    src/ui/views/lock/RecoveryDialog.hpp
    src/ui/views/lock/RecoveryDialog.cpp
    src/ui/views/lock/ResetVaultDialog.hpp
    src/ui/views/lock/ResetVaultDialog.cpp
    src/ui/views/onboarding/OnboardingWizardView.hpp
    src/ui/views/onboarding/OnboardingWizardView.cpp
    src/ui/views/onboarding/PasswordEntropyMeter.hpp
    src/ui/views/onboarding/PasswordEntropyMeter.cpp
    src/ui/views/settings/SettingRowWidget.hpp
    src/ui/views/settings/SettingRowWidget.cpp
    src/ui/views/settings/SettingsDialog.hpp
    src/ui/views/settings/SettingsDialog.cpp
    src/ui/views/settings/tabs/TabBrowser.hpp
    src/ui/views/settings/tabs/TabBrowser.cpp
    src/ui/views/settings/tabs/TabData.hpp
    src/ui/views/settings/tabs/TabData.cpp
    src/ui/views/settings/tabs/TabGeneral.hpp
    src/ui/views/settings/tabs/TabGeneral.cpp
    src/ui/views/settings/tabs/TabSecurity.hpp
    src/ui/views/settings/tabs/TabSecurity.cpp
    src/ui/views/settings/tabs/TabSsh.hpp
    src/ui/views/settings/tabs/TabSsh.cpp
    src/ui/views/settings/tabs/TabVaults.hpp
    src/ui/views/settings/tabs/TabVaults.cpp
    src/ui/views/sidebar/SidebarItemDelegate.hpp
    src/ui/views/sidebar/SidebarItemDelegate.cpp
    src/ui/views/sidebar/SidebarTypes.hpp
    src/ui/views/sidebar/SidebarView.hpp
    src/ui/views/sidebar/SidebarView.cpp
    src/ui/views/vaults/NewVaultDialog.hpp
    src/ui/views/vaults/NewVaultDialog.cpp
    src/resources/svm.qrc
)

if(WIN32)
    list(APPEND SOURCES packaging/windows/svm.rc)
elseif(APPLE)
    list(APPEND SOURCES packaging/macos/svm.icns)
    set_source_files_properties(packaging/macos/svm.icns PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources"
    )
endif()

add_library(svm_app_core STATIC ${SOURCES})
add_dependencies(svm_app_core ${svm_core_imported_targets} generate_cxx_bridge)
set_property(TARGET svm_app_core PROPERTY AUTOGEN_TARGET_DEPENDS generate_cxx_bridge)

target_include_directories(svm_app_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CXX_BRIDGE_INCLUDE_DIR}"
)

target_compile_definitions(svm_app_core PUBLIC
    $<$<CONFIG:Release>:QT_NO_DEBUG_OUTPUT QT_NO_INFO_OUTPUT>
)

target_link_libraries(svm_app_core PUBLIC
    window_framework
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    ${svm_core_imported_targets}
)

add_executable(svm src/main.cpp)
target_link_libraries(svm PRIVATE svm_app_core)

# Dynamic linking RPATH configuration for LGPL v3 relinking
if(APPLE)
    set_target_properties(svm PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/packaging/macos/Info.plist"
        MACOSX_BUNDLE_ICON_FILE "svm.icns"
        MACOSX_BUNDLE_BUNDLE_NAME "Secure Vault Manager"
        MACOSX_BUNDLE_GUI_IDENTIFIER "io.github.hephaestus.svm"
        MACOSX_BUNDLE_BUNDLE_VERSION "0.1.0"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "0.1.0"
        BUILD_WITH_INSTALL_RPATH TRUE
        INSTALL_RPATH "@executable_path/../Frameworks;@executable_path/../lib"
    )
else()
    set_target_properties(svm PROPERTIES
        BUILD_WITH_INSTALL_RPATH TRUE
        INSTALL_RPATH "$ORIGIN;$ORIGIN/../lib;$ORIGIN/../lib/svm"
    )
endif()

# Testing & Dialog Verification
enable_testing()
add_executable(dialog_smoke_test tests/dialog_smoke_test.cpp)
target_link_libraries(dialog_smoke_test PRIVATE svm_app_core)
if(NOT APPLE)
    set_target_properties(dialog_smoke_test PROPERTIES
        BUILD_WITH_INSTALL_RPATH TRUE
        INSTALL_RPATH "$ORIGIN;$ORIGIN/../lib;$ORIGIN/../lib/svm"
    )
endif()
add_test(NAME dialog_smoke_test COMMAND dialog_smoke_test)

# Install rules
install(TARGETS svm DESTINATION bin)
corrosion_install(TARGETS ${svm_cli_targets} ${svm_host_targets} ${svm_core_imported_targets})
