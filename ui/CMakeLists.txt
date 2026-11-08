cmake_minimum_required(VERSION 3.20)
project(svm_ui LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

set(UI_SOURCES
    main.cpp
    main_window.cpp
    core/core_bridge.cpp
    app/dev_logger.cpp
    app/theme_manager.cpp
    app/tray_manager.cpp
    app/clipboard_manager.cpp
    app/language_manager.cpp
    app/vault_manager.cpp
    app/settings_manager.cpp
    components/unified_header_bar.cpp
    components/window_resize_handler.cpp
    components/notification_dialog.cpp
    views/sidebar/sidebar_item_delegate.cpp
    views/sidebar/sidebar_view.cpp
    views/item_list/item_list_model.cpp
    views/item_list/item_sort_filter_proxy.cpp
    views/item_list/item_card_delegate.cpp
    views/item_list/item_list_empty_widget.cpp
    views/item_list/item_list_view.cpp
    views/detail/empty_state_widget.cpp
    views/detail/totp_widget.cpp
    views/detail/field_row_widget.cpp
    views/detail/custom_field_row_widget.cpp
    views/detail/custom_fields_widget.cpp
    views/detail/detail_header_widget.cpp
    views/detail/detail_inspector_view.cpp
    views/about/about_dialog.cpp
    views/onboarding/password_entropy_meter.cpp
    views/onboarding/onboarding_wizard_view.cpp
    views/lock/recovery_dialog.cpp
    views/lock/lock_screen_view.cpp
    views/vaults/new_vault_dialog.cpp
    views/settings/setting_row_widget.cpp
    views/settings/tabs/tab_general.cpp
    views/settings/tabs/tab_security.cpp
    views/settings/tabs/tab_vaults.cpp
    views/settings/tabs/tab_browser.cpp
    views/settings/tabs/tab_ssh.cpp
    views/settings/tabs/tab_data.cpp
    views/settings/settings_dialog.cpp
    resources/resources.qrc
)

set(UI_HEADERS
    main_window.h
    models/vault_item.h
    models/vault_types.h
    core/core_bridge.h
    app/dev_logger.h
    app/theme_manager.h
    app/tray_manager.h
    app/clipboard_manager.h
    app/language_manager.h
    app/vault_manager.h
    app/settings_manager.h
    components/unified_header_bar.h
    components/window_resize_handler.h
    components/notification_dialog.h
    components/elided_label.h
    components/combobox_popup_helper.h
    components/in_window_combobox.h
    views/sidebar/sidebar_types.h
    views/sidebar/sidebar_item_delegate.h
    views/sidebar/sidebar_view.h
    views/item_list/item_list_model.h
    views/item_list/item_sort_filter_proxy.h
    views/item_list/item_card_delegate.h
    views/item_list/item_list_empty_widget.h
    views/item_list/item_list_view.h
    views/detail/empty_state_widget.h
    views/detail/totp_widget.h
    views/detail/field_row_widget.h
    views/detail/custom_field_row_widget.h
    views/detail/custom_fields_widget.h
    views/detail/detail_header_widget.h
    views/detail/detail_inspector_view.h
    views/about/about_dialog.h
    views/onboarding/password_entropy_meter.h
    views/onboarding/onboarding_wizard_view.h
    views/lock/recovery_dialog.h
    views/lock/lock_screen_view.h
    views/vaults/new_vault_dialog.h
    views/settings/setting_row_widget.h
    views/settings/tabs/tab_general.h
    views/settings/tabs/tab_security.h
    views/settings/tabs/tab_vaults.h
    views/settings/tabs/tab_browser.h
    views/settings/tabs/tab_ssh.h
    views/settings/tabs/tab_data.h
    views/settings/settings_dialog.h
)

add_executable(svm_ui ${UI_SOURCES} ${UI_HEADERS})

target_include_directories(svm_ui PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(svm_ui PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    svm_core
)

# Đảm bảo rpath trỏ đến thư mục chứa libsvm_core.so khi chạy
set_target_properties(svm_ui PROPERTIES
    BUILD_RPATH "${CMAKE_CURRENT_SOURCE_DIR}/../core/target/${CARGO_TARGET_DIR}"
    INSTALL_RPATH "$ORIGIN/../../core/target/${CARGO_TARGET_DIR}"
)
