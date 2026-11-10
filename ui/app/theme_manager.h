/**
 * @file ui/app/theme_manager.h
 * @brief Quản lý chủ đề giao diện (Dark / Light Theme) bằng Qt Style Sheets
 * (QSS).
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#pragma once

#include <QHash>
#include <QIcon>
#include <QObject>
#include <QString>

namespace svm::app {

enum class ThemeMode { Dark, Light };

class ThemeManager : public QObject {
  Q_OBJECT

public:
  static ThemeManager &instance();

  ~ThemeManager() override = default;

  ThemeManager(const ThemeManager &) = delete;
  ThemeManager &operator=(const ThemeManager &) = delete;

  /**
   * @brief Khởi tạo và nạp sẵn stylesheet vào bộ nhớ RAM, làm ấm bộ đệm icon.
   */
  void initialize();

  /**
   * @brief Áp dụng Theme cho toàn bộ ứng dụng từ tài nguyên đã cache.
   */
  void applyTheme(ThemeMode mode);

  /**
   * @brief Lấy chế độ Theme hiện tại.
   */
  [[nodiscard]] ThemeMode currentMode() const;

  /**
   * @brief Lấy đường dẫn tài nguyên QRC của icon tương ứng với chủ đề hiện tại
   * (có cache O(1)).
   */
  [[nodiscard]] QString iconPath(const QString &baseName) const;

  /**
   * @brief Lấy đối tượng QIcon từ iconPath theo chủ đề hiện tại (có cache O(1),
   * không đọc đĩa/SVG).
   */
  [[nodiscard]] QIcon icon(const QString &baseName) const;

  /**
   * @brief Xóa toàn bộ bộ đệm icon (nếu cần giải phóng RAM).
   */
  void clearIconCache();

signals:
  void themeChanged(ThemeMode newMode);

private:
  ThemeManager();
  void ensureStylesheetsLoaded();
  void prewarmIcons();

  ThemeMode m_currentMode{ThemeMode::Dark};
  bool m_initialized{false};
  bool m_appliedOnce{false};
  QString m_darkQss;
  QString m_lightQss;

  mutable QHash<QString, QIcon> m_iconCache;
  mutable QHash<QString, QString> m_resolvedPathCache;
  mutable QHash<QString, QString> m_iconFileIndex;
};

} // namespace svm::app
