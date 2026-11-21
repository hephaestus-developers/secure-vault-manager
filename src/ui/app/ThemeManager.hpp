/**
 * @file ui/app/theme_manager.h
 * @brief Quản lý chủ đề giao diện (Dark / Light Theme) bằng Qt Style Sheets
 * (QSS).
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#pragma once

#include <QColor>
#include <QHash>
#include <QIcon>
#include <QObject>
#include <QString>

namespace svm::app {

enum class ThemeMode { Dark, Light };

struct ThemePalette {
  QColor background;
  QColor surface;
  QColor surfaceOverlay;
  QColor foreground;
  QColor mutedForeground;

  QColor border;
  QColor borderStrong;
  QColor borderInput;
  QColor borderFocus;
  QColor tabRail;

  QColor primary;
  QColor primaryHover;
  QColor primaryForeground;
  QColor selection;

  QColor hover1;
  QColor hover2;
  QColor hover3;
  QColor disabledBg;
  QColor disabledForeground;

  QColor danger;
  QColor dangerBg;
  QColor dangerBorder;
  QColor dangerForeground;

  QColor success;
  QColor successBg;
  QColor successBorder;

  QColor warning;
  QColor warningBg;
  QColor warningBorder;

  QColor info;
  QColor infoHover;
  QColor infoBg;
  QColor infoBorder;
  QColor infoForeground;

  QColor neutral;
  QColor neutralBg;
  QColor neutralBorder;

  QColor cat1;
  QColor cat2;
  QColor cat3;
  QColor cat4;
  QColor cat5;
  QColor cat6;
};

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
  [[nodiscard]] bool isDarkTheme() const {
    return currentMode() == ThemeMode::Dark;
  }

  /**
   * @brief Lấy bảng màu thiết kế tương ứng với chủ đề hiện tại.
   */
  [[nodiscard]] const ThemePalette &palette() const;

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
