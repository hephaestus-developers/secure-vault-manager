/**
 * @file ui/app/theme_manager.cpp
 * @brief Hiện thực lớp ThemeManager áp dụng giao diện hiện đại chuẩn Enterprise
 * Vault.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "theme_manager.h"
#include "app/settings_manager.h"

#include "app/dev_logger.h"
#include <QApplication>
#include <QDebug>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QWidget>

namespace svm::app {

ThemeManager &ThemeManager::instance() {
  static ThemeManager s_instance;
  return s_instance;
}

ThemeManager::ThemeManager() = default;

void ThemeManager::initialize() {
  if (m_initialized) {
    return;
  }
  m_currentMode = SettingsManager::instance().themeMode();

  ensureStylesheetsLoaded();
  prewarmIcons();
  m_initialized = true;
}

void ThemeManager::ensureStylesheetsLoaded() {
  if (m_darkQss.isEmpty()) {
    QFile darkFile(QStringLiteral(":/styles/dark.qss"));
    if (darkFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
      m_darkQss = QString::fromUtf8(darkFile.readAll());
    } else {
      qWarning() << "[ThemeManager] Failed to read :/styles/dark.qss";
    }
  }

  if (m_lightQss.isEmpty()) {
    QFile lightFile(QStringLiteral(":/styles/light.qss"));
    if (lightFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
      m_lightQss = QString::fromUtf8(lightFile.readAll());
    } else {
      qWarning() << "[ThemeManager] Failed to read :/styles/light.qss";
    }
  }
}

void ThemeManager::prewarmIcons() {
  const QString commonIcons[] = {
      QStringLiteral("category_settings"),
      QStringLiteral("shield_check"),
      QStringLiteral("folder_open"),
      QStringLiteral("folder_empty"),
      QStringLiteral("category_server"),
      QStringLiteral("category_ssh"),
      QStringLiteral("trash"),
      QStringLiteral("win_close"),
      QStringLiteral("win_min"),
      QStringLiteral("win_max"),
      QStringLiteral("win_restore"),
      QStringLiteral("eye"),
      QStringLiteral("eye_off"),
      QStringLiteral("search"),
      QStringLiteral("plus"),
      QStringLiteral("copy"),
      QStringLiteral("chevron_down"),
      QStringLiteral("chevron_left"),
      QStringLiteral("chevron_right"),
      QStringLiteral("lock"),
      QStringLiteral("lock_neutral"),
      QStringLiteral("unlock"),
      QStringLiteral("star"),
      QStringLiteral("star_filled"),
      QStringLiteral("edit"),
      QStringLiteral("category_login"),
      QStringLiteral("category_passkey"),
      QStringLiteral("category_card"),
      QStringLiteral("category_identity"),
      QStringLiteral("category_note"),
      QStringLiteral("category_database"),
      QStringLiteral("category_wifi"),
      QStringLiteral("category_all"),
      QStringLiteral("category_trash"),
      QStringLiteral("sidebar_collapse"),
      QStringLiteral("sidebar_expand"),
      QStringLiteral("vault_personal"),
      QStringLiteral("vault_work"),
      QStringLiteral("vault_home"),
      QStringLiteral("vault_shield"),
      QStringLiteral("vault_safe"),
      QStringLiteral("vault_finance"),
      QStringLiteral("vault_cloud"),
      QStringLiteral("vault_team"),
  };

  const ThemeMode originalMode = m_currentMode;

  for (const auto mode : {ThemeMode::Dark, ThemeMode::Light}) {
    m_currentMode = mode;
    for (const auto &name : commonIcons) {
      (void)icon(name);
    }
  }

  m_currentMode = originalMode;
}

void ThemeManager::applyTheme(ThemeMode mode) {
  SVM_ACTION_SCOPE_DETAIL("ThemeManager", "applyTheme",
                          mode == ThemeMode::Dark ? QStringLiteral("Dark")
                                                  : QStringLiteral("Light"));
  if (m_appliedOnce && m_currentMode == mode) {
    return;
  }
  m_appliedOnce = true;
  m_currentMode = mode;
  ensureStylesheetsLoaded();

  QElapsedTimer timer;
  timer.start();

  // 1. Tạm dừng vẽ trên các cửa sổ đỉnh (Freeze GUI - loại bỏ layout thrashing
  // và flicker)
  const auto topWidgets = QApplication::topLevelWidgets();
  for (QWidget *w : topWidgets) {
    if (w && w->isVisible()) {
      w->setUpdatesEnabled(false);
    }
  }

  // 2. Nạp QSS từ chuỗi đã cache sẵn trong RAM
  if (qApp) {
    qApp->setStyleSheet(mode == ThemeMode::Dark ? m_darkQss : m_lightQss);
  }

  // 3. Thông báo cho các widget cập nhật icon / trạng thái
  emit themeChanged(mode);

  // 4. Mở khóa và vẽ lại 1 lần đồng nhất (Atomic Repaint)
  for (QWidget *w : topWidgets) {
    if (w && w->isVisible()) {
      w->setUpdatesEnabled(true);
      w->update();
    }
  }

  const qint64 elapsedMs = timer.elapsed();
  qInfo().noquote() << QStringLiteral(
                           "[ThemeManager] Theme switched to %1 in %2 ms")
                           .arg(mode == ThemeMode::Dark
                                    ? QStringLiteral("Dark")
                                    : QStringLiteral("Light"))
                           .arg(elapsedMs);
}

ThemeMode ThemeManager::currentMode() const { return m_currentMode; }

QString ThemeManager::iconPath(const QString &baseName) const {
  const QString key =
      baseName + ((m_currentMode == ThemeMode::Dark) ? QStringLiteral(":d")
                                                     : QStringLiteral(":l"));
  auto it = m_resolvedPathCache.constFind(key);
  if (it != m_resolvedPathCache.constEnd()) {
    return it.value();
  }

  if (m_iconFileIndex.isEmpty()) {
    QDirIterator dit(QStringLiteral(":/icons"), {QStringLiteral("*.svg")},
                     QDir::Files, QDirIterator::Subdirectories);
    while (dit.hasNext()) {
      const QString fullPath = dit.next();
      const QString fileName = dit.fileName();
      if (!m_iconFileIndex.contains(fileName)) {
        m_iconFileIndex.insert(fileName, fullPath);
      }
    }
  }

  const QString suffix = (m_currentMode == ThemeMode::Dark)
                             ? QStringLiteral("_dark.svg")
                             : QStringLiteral("_light.svg");
  QString resolved;

  if (baseName.contains(QLatin1Char('/'))) {
    const QString themed = QStringLiteral(":/icons/") + baseName + suffix;
    if (QFile::exists(themed)) {
      resolved = themed;
    } else {
      const QString fallback =
          QStringLiteral(":/icons/") + baseName + QStringLiteral(".svg");
      if (QFile::exists(fallback)) {
        resolved = fallback;
      }
    }
  } else {
    resolved = m_iconFileIndex.value(baseName + suffix);
    if (resolved.isEmpty()) {
      resolved = m_iconFileIndex.value(baseName + QStringLiteral(".svg"));
    }
  }

  m_resolvedPathCache.insert(key, resolved);
  return resolved;
}

QIcon ThemeManager::icon(const QString &baseName) const {
  const QString key =
      baseName + ((m_currentMode == ThemeMode::Dark) ? QStringLiteral(":d")
                                                     : QStringLiteral(":l"));
  auto it = m_iconCache.constFind(key);
  if (it != m_iconCache.constEnd()) {
    return it.value();
  }

  const QString path = iconPath(baseName);
  QIcon ico;
  if (!path.isEmpty()) {
    ico = QIcon(path);
  }
  m_iconCache.insert(key, ico);
  return ico;
}

void ThemeManager::clearIconCache() {
  m_iconCache.clear();
  m_resolvedPathCache.clear();
}

} // namespace svm::app
