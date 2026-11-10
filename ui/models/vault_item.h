/**
 * @file ui/models/vault_item.h
 * @brief Định nghĩa cấu trúc dữ liệu bản ghi UiVaultItem cho tầng Qt UI.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#pragma once

#include <QDate>
#include <QDateTime>
#include <QMap>
#include <QObject>
#include <QRegularExpression>
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
  return QStringLiteral(":/icons/categories/") + categoryIconName(cat) + suffix;
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
  QString folderId;
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

/**
 * @brief Thực thể thư mục tổ chức bản ghi.
 */
struct UiFolder {
  QString id;
  QString name;
  QString parentId;
};

/**
 * @brief Bản ghi lịch sử sửa đổi của một mục dữ liệu.
 */
struct UiItemRevision {
  int revision{1};
  qint64 timestamp{0};
  UiVaultItem snapshot;
};

/**
 * @brief Siêu dữ liệu tệp đính kèm được mã hóa.
 */
struct UiAttachmentMeta {
  QString id;
  QString itemId;
  QString fileName;
  quint64 sizeBytes{0};
  qint64 createdAt{0};
};

/**
 * @brief Kiểm tra một mục có đang có mật khẩu yếu (< 60%) phục vụ Watchtower.
 */
inline bool isWeakPasswordItem(const UiVaultItem &item) {
  if (item.isDeleted || item.isArchived) {
    return false;
  }
  const bool hasPasswordCategory = (item.category == ItemCategory::Login ||
                                    item.category == ItemCategory::Database ||
                                    item.category == ItemCategory::Server ||
                                    item.category == ItemCategory::Wifi);
  return hasPasswordCategory && item.passwordStrengthScore < 60;
}

/**
 * @brief Kiểm tra thẻ tín dụng / trường ngày hết hạn đã hoặc sắp hết hạn trong
 * 60 ngày.
 */
inline bool isExpiringItem(const UiVaultItem &item) {
  if (item.isDeleted || item.isArchived) {
    return false;
  }
  const QDate today = QDate::currentDate();
  const QDate cutoff = today.addDays(60);

  if (!item.cardExpiry.trimmed().isEmpty()) {
    static const QRegularExpression reCard(
        QStringLiteral("^\\s*(\\d{1,2})\\s*/\\s*(\\d{2,4})\\s*$"));
    const QRegularExpressionMatch m = reCard.match(item.cardExpiry);
    if (m.hasMatch()) {
      const int month = m.captured(1).toInt();
      int year = m.captured(2).toInt();
      if (m.captured(2).length() == 2) {
        year += 2000;
      }
      if (month >= 1 && month <= 12) {
        const QDate expDate(year, month, QDate(year, month, 1).daysInMonth());
        if (expDate.isValid() && expDate <= cutoff) {
          return true;
        }
      }
    }
  }

  for (const auto &cf : item.customFields) {
    if (cf.kind == CustomFieldKind::Date ||
        cf.label.contains(QStringLiteral("expir"), Qt::CaseInsensitive)) {
      const QDate d = QDate::fromString(cf.value.trimmed(), Qt::ISODate);
      if (d.isValid() && d <= cutoff) {
        return true;
      }
    }
  }
  return false;
}

} // namespace svm::models
