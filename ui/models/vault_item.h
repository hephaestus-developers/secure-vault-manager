/**
 * @file ui/models/vault_item.h
 * @brief Định nghĩa cấu trúc dữ liệu bản ghi UiVaultItem cho tầng Qt UI.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#pragma once

#include <QDateTime>
#include <QMap>
#include <QString>
#include <QStringList>

namespace svm::models {

/**
 * @brief 9 danh mục nghiệp vụ chuẩn hóa của kho dữ liệu SVM.
 */
enum class ItemCategory {
  Login,
  Passkey,
  Card,
  Identity,
  SecureNote,
  SshKey,
  Database,
  Server,
  Wifi
};

/**
 * @brief Chuyển đổi ItemCategory sang chuỗi định danh icon SVG tài nguyên.
 */
inline QString categoryIconPath(ItemCategory cat) {
  switch (cat) {
  case ItemCategory::Login:
    return QStringLiteral(":/icons/category_login.svg");
  case ItemCategory::Passkey:
    return QStringLiteral(":/icons/category_passkey.svg");
  case ItemCategory::Card:
    return QStringLiteral(":/icons/category_card.svg");
  case ItemCategory::Identity:
    return QStringLiteral(":/icons/category_identity.svg");
  case ItemCategory::SecureNote:
    return QStringLiteral(":/icons/category_note.svg");
  case ItemCategory::SshKey:
    return QStringLiteral(":/icons/category_ssh.svg");
  case ItemCategory::Database:
    return QStringLiteral(":/icons/category_database.svg");
  case ItemCategory::Server:
    return QStringLiteral(":/icons/category_server.svg");
  case ItemCategory::Wifi:
    return QStringLiteral(":/icons/category_wifi.svg");
  }
  return QStringLiteral(":/icons/category_all.svg");
}

/**
 * @brief Chuyển đổi ItemCategory sang chuỗi hiển thị thân thiện tiếng Anh /
 * Việt.
 */
inline QString categoryDisplayName(ItemCategory cat) {
  switch (cat) {
  case ItemCategory::Login:
    return QStringLiteral("Logins");
  case ItemCategory::Passkey:
    return QStringLiteral("Passkeys");
  case ItemCategory::Card:
    return QStringLiteral("Credit Cards");
  case ItemCategory::Identity:
    return QStringLiteral("Identities");
  case ItemCategory::SecureNote:
    return QStringLiteral("Secure Notes");
  case ItemCategory::SshKey:
    return QStringLiteral("SSH Keys");
  case ItemCategory::Database:
    return QStringLiteral("Databases");
  case ItemCategory::Server:
    return QStringLiteral("Servers");
  case ItemCategory::Wifi:
    return QStringLiteral("Wi-Fi Networks");
  }
  return QStringLiteral("Unknown");
}

/**
 * @brief Kiểu dữ liệu của trường tùy biến động do người dùng định nghĩa.
 */
enum class CustomFieldKind {
  Text,     // Văn bản thông thường
  Concealed // Trường bảo mật (Mật khẩu phụ, Token, PIN, Recovery Code)
};

/**
 * @brief Cấu trúc một trường tùy biến động của Vault Item.
 */
struct UiCustomField {
  QString id;
  QString label;
  QString value;
  CustomFieldKind kind{CustomFieldKind::Text};
};

/**
 * @brief Cấu trúc dữ liệu chi tiết của một bản ghi trong kho SVM phục vụ UI.
 */
struct UiVaultItem {
  QString id;
  QString title;
  ItemCategory category{ItemCategory::Login};
  QString username;        // Hoặc account/fingerprint/cardholder
  QString password;        // Secret chính (nếu có)
  QString websiteUrl;      // URL đăng nhập hoặc Server host
  QString notes;           // Ghi chú đính kèm
  QString totpSecret;      // Khóa bí mật TOTP nếu kích hoạt 2FA
  QString totpCurrentCode; // Mã 6 số hiện thời
  int totpRemainingSeconds{30};

  // Trạng thái cờ bảo mật & nghiệp vụ
  bool isFavorite{false};
  bool isArchived{false};
  bool isDeleted{false};
  bool hasTotp{false};
  bool hasPasskey{false};
  bool isSshLoaded{false}; // Dành cho SSH Key đã nạp vào agent

  // Trường chuyên biệt theo từng danh mục
  QString sshPublicKey;
  QString sshFingerprint;
  QString cardNumber;
  QString cardExpiry;
  QString cardCvv;
  QString dbHost;
  int dbPort{5432};
  QString dbName;
  QString wifiSsid;
  QString wifiSecurity;

  // Trường tùy biến động (Custom Fields)
  QList<UiCustomField> customFields;

  // Metadata audit
  QDateTime createdAt;
  QDateTime updatedAt;
  int passwordStrengthScore{85}; // 0 - 100%
};

} // namespace svm::models
