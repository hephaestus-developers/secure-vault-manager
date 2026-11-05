/**
 * @file ui/app/theme_manager.h
 * @brief Quản lý chủ đề giao diện (Dark / Light Theme) bằng Qt Style Sheets
 * (QSS).
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#pragma once

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
   * @brief Áp dụng Theme cho toàn bộ ứng dụng từ tài nguyên QRC.
   */
  void applyTheme(ThemeMode mode);

  /**
   * @brief Lấy chế độ Theme hiện tại.
   */
  [[nodiscard]] ThemeMode currentMode() const;

  /**
   * @brief Lấy đường dẫn tài nguyên QRC của icon tương ứng với chủ đề hiện tại.
   */
  [[nodiscard]] QString iconPath(const QString &baseName) const;

  /**
   * @brief Tạo đối tượng QIcon từ iconPath theo chủ đề hiện tại.
   */
  [[nodiscard]] QIcon icon(const QString &baseName) const;

signals:
  void themeChanged(ThemeMode newMode);

private:
  ThemeManager();

  ThemeMode m_currentMode{ThemeMode::Dark};
};

} // namespace svm::app
