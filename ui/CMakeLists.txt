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
    app/theme_manager.cpp
    app/tray_manager.cpp
    app/clipboard_manager.cpp
    components/menubar/app_menu_bar.cpp
    components/unified_header_bar.cpp
    components/window_resize_handler.cpp
    views/sidebar/sidebar_item_delegate.cpp
    views/sidebar/sidebar_view.cpp
    views/item_list/item_list_model.cpp
    views/item_list/item_sort_filter_proxy.cpp
    views/item_list/item_card_delegate.cpp
    views/item_list/item_list_view.cpp
    views/detail/empty_state_widget.cpp
    views/detail/totp_widget.cpp
    views/detail/field_row_widget.cpp
    views/detail/custom_field_row_widget.cpp
    views/detail/custom_fields_widget.cpp
    views/detail/detail_header_widget.cpp
    views/detail/detail_inspector_view.cpp
    views/command_palette/command_palette_dialog.cpp
    resources/resources.qrc
)

set(UI_HEADERS
    main_window.h
    models/vault_item.h
    core/core_bridge.h
    app/theme_manager.h
    app/tray_manager.h
    app/clipboard_manager.h
    components/menubar/app_menu_bar.h
    components/unified_header_bar.h
    components/window_resize_handler.h
    views/sidebar/sidebar_types.h
    views/sidebar/sidebar_item_delegate.h
    views/sidebar/sidebar_view.h
    views/item_list/item_list_model.h
    views/item_list/item_sort_filter_proxy.h
    views/item_list/item_card_delegate.h
    views/item_list/item_list_view.h
    views/detail/empty_state_widget.h
    views/detail/totp_widget.h
    views/detail/field_row_widget.h
    views/detail/custom_field_row_widget.h
    views/detail/custom_fields_widget.h
    views/detail/detail_header_widget.h
    views/detail/detail_inspector_view.h
    views/command_palette/command_palette_dialog.h
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
