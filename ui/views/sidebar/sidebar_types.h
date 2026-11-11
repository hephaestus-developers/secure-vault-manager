/**
 * @file ui/views/sidebar/sidebar_types.h
 * @brief Định nghĩa các kiểu dữ liệu bộ lọc cho SidebarView.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#pragma once

#include "models/vault_item.h"

namespace svm::views {

enum class SidebarFilterType { AllItems, Favorites, Trash, Category };

} // namespace svm::views
