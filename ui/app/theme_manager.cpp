/**
 * @file ui/app/theme_manager.cpp
 * @brief Hiện thực lớp ThemeManager áp dụng giao diện hiện đại chuẩn Enterprise
 * Vault.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "theme_manager.h"
#include <QApplication>
#include <QDebug>
#include <QFile>

namespace svm::app {

ThemeManager &ThemeManager::instance() {
  static ThemeManager s_instance;
  return s_instance;
}

ThemeManager::ThemeManager() = default;

void ThemeManager::applyTheme(ThemeMode mode) {
  m_currentMode = mode;
  const QString qssPath = (mode == ThemeMode::Dark)
                              ? QStringLiteral(":/styles/dark.qss")
                              : QStringLiteral(":/styles/light.qss");

  QFile file(qssPath);
  if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    if (qApp) {
      qApp->setStyleSheet(QString::fromUtf8(file.readAll()));
    }
  } else {
    qWarning() << "[ThemeManager] Could not open stylesheet:" << qssPath;
  }

  emit themeChanged(mode);
}

ThemeMode ThemeManager::currentMode() const { return m_currentMode; }

} // namespace svm::app
