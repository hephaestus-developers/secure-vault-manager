/**
 * @file ui/models/vault_types.h
 * @brief Định nghĩa cấu trúc dữ liệu đại diện cho một kho lưu trữ (Vault).
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#pragma once

#include <QDateTime>
#include <QString>

namespace svm::models {

/**
 * @brief Cấu trúc dữ liệu đại diện cho một kho lưu trữ (Vault).
 */
struct UiVault {
  QString id;
  QString name;
  QString iconName{QStringLiteral("vault_personal")};
  QString description;
  int itemCount{0};
  bool isDefault{false};
  QDateTime createdAt{QDateTime::currentDateTime()};
};

} // namespace svm::models
