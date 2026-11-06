/**
 * @file ui/models/vault_item.h
 * @brief Định nghĩa cấu trúc dữ liệu bản ghi UiVaultItem cho tầng Qt UI.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#pragma once

#include <QDateTime>
#include <QMap>
#include <QObject>
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
 * @brief Lấy tên định danh cơ bản của icon danh mục.
 */
inline QString categoryIconName(ItemCategory cat) {
  switch (cat) {
  case ItemCategory::Login:
    return QStringLiteral("category_login");
  case ItemCategory::Passkey:
    return QStringLiteral("category_passkey");
  case ItemCategory::Card:
    return QStringLiteral("category_card");
  case ItemCategory::Identity:
    return QStringLiteral("category_identity");
  case ItemCategory::SecureNote:
    return QStringLiteral("category_note");
  case ItemCategory::SshKey:
    return QStringLiteral("category_ssh");
  case ItemCategory::Database:
    return QStringLiteral("category_database");
  case ItemCategory::Server:
    return QStringLiteral("category_server");
  case ItemCategory::Wifi:
    return QStringLiteral("category_wifi");
  }
  return QStringLiteral("category_all");
}

/**
 * @brief Chuyển đổi ItemCategory sang chuỗi định danh icon SVG tài nguyên theo
 * chủ đề.
 */
inline QString categoryIconPath(ItemCategory cat, bool isLight = false) {
  const QString suffix =
      isLight ? QStringLiteral("_light.svg") : QStringLiteral("_dark.svg");
  return QStringLiteral(":/icons/") + categoryIconName(cat) + suffix;
}

/**
 * @brief Chuyển đổi ItemCategory sang chuỗi hiển thị thân thiện tiếng Anh /
 * Việt.
 */
inline QString categoryDisplayName(ItemCategory cat) {
  switch (cat) {
  case ItemCategory::Login:
    return QObject::tr("Logins");
  case ItemCategory::Passkey:
    return QObject::tr("Passkeys");
  case ItemCategory::Card:
    return QObject::tr("Credit Cards");
  case ItemCategory::Identity:
    return QObject::tr("Identities");
  case ItemCategory::SecureNote:
    return QObject::tr("Secure Notes");
  case ItemCategory::SshKey:
    return QObject::tr("SSH Keys");
  case ItemCategory::Database:
    return QObject::tr("Databases");
  case ItemCategory::Server:
    return QObject::tr("Servers");
  case ItemCategory::Wifi:
    return QObject::tr("Wi-Fi Networks");
  }
  return QObject::tr("Unknown");
}

/**
 * @brief Kiểu dữ liệu của trường tùy biến động do người dùng định nghĩa.
 */
enum class CustomFieldKind {
  Text,      // Văn bản thông thường
  Concealed, // Trường bảo mật (Mật khẩu phụ, Token, PIN, Recovery Code)
  Boolean,   // Cờ logic đúng/sai (Yes/No, Bật/Tắt)
  Date       // Ngày tháng định dạng ISO 8601 (YYYY-MM-DD)
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
  QString vaultId{QStringLiteral("personal")};
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
