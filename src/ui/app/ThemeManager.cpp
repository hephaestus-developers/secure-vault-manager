/**
 * @file ui/app/theme_manager.cpp
 * @brief Hiện thực lớp ThemeManager áp dụng giao diện hiện đại chuẩn Enterprise
 * Vault.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "ui/app/ThemeManager.hpp"
#include "ui/app/SettingsManager.hpp"

#include "core/LogCategories.hpp"
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

const ThemePalette &ThemeManager::palette() const {
  static const ThemePalette kLightPalette{
      // 1. Background, Surface & Foreground
      QColor(0xf4, 0xf4, 0xf6), // background: #f4f4f6
      QColor(0xff, 0xff, 0xff), // surface: #ffffff
      QColor(0xff, 0xff, 0xff), // surfaceOverlay: #ffffff
      QColor(0x2c, 0x2c, 0x2c), // foreground: #2c2c2c
      QColor(0x66, 0x66, 0x66), // mutedForeground: #666666

      // 2. Borders
      QColor(0xe4, 0xe4, 0xe7),       // border: #e4e4e7
      QColor(0xd4, 0xd4, 0xd8),       // borderStrong: #d4d4d8
      QColor(0x8a, 0x8a, 0x93),       // borderInput: #8a8a93
      QColor(0x1d, 0x4e, 0xd8),       // borderFocus: #1d4ed8
      QColor(0x2c, 0x2c, 0x2c, 0x1a), // tabRail: 10% foreground

      // 3. Primary & Selection
      QColor(0x18, 0x18, 0x18),       // primary: #181818
      QColor(0x2e, 0x2e, 0x2e),       // primaryHover: #2e2e2e
      QColor(0xff, 0xff, 0xff),       // primaryForeground: #ffffff
      QColor(0x25, 0x63, 0xeb, 0x33), // selection: #2563eb33

      // 4. Interaction
      QColor(0x00, 0x00, 0x00, 0x0a), // hover1
      QColor(0x00, 0x00, 0x00, 0x12), // hover2
      QColor(0x00, 0x00, 0x00, 0x1c), // hover3
      QColor(0x00, 0x00, 0x00, 0x0f), // disabledBg
      QColor(0x2c, 0x2c, 0x2c, 0x73), // disabledForeground

      // 5. Semantic
      QColor(0xc7, 0x00, 0x36), // danger: #c70036
      QColor(0xff, 0xf1, 0xf2), // dangerBg: #fff1f2
      QColor(0xfe, 0xcd, 0xd3), // dangerBorder: #fecdd3
      QColor(0xff, 0xff, 0xff), // dangerForeground: #ffffff

      QColor(0x00, 0x7a, 0x55), // success: #007a55
      QColor(0xec, 0xfd, 0xf5), // successBg: #ecfdf5
      QColor(0xa4, 0xf4, 0xcf), // successBorder: #a4f4cf

      QColor(0xbb, 0x4d, 0x00), // warning: #bb4d00
      QColor(0xff, 0xfb, 0xeb), // warningBg: #fffbeb
      QColor(0xfe, 0xe6, 0x85), // warningBorder: #fee685

      QColor(0x1d, 0x4e, 0xd8), // info: #1d4ed8
      QColor(0x1e, 0x40, 0xaf), // infoHover: #1e40af
      QColor(0xef, 0xf6, 0xff), // infoBg: #eff6ff
      QColor(0xbf, 0xdb, 0xfe), // infoBorder: #bfdbfe
      QColor(0xff, 0xff, 0xff), // infoForeground: #ffffff

      QColor(0x52, 0x52, 0x5c), // neutral: #52525c
      QColor(0xf4, 0xf4, 0xf5), // neutralBg: #f4f4f5
      QColor(0xe4, 0xe4, 0xe7), // neutralBorder: #e4e4e7

      // 6. Category
      QColor(0x25, 0x63, 0xeb), // cat1: #2563eb
      QColor(0x0d, 0x94, 0x88), // cat2: #0d9488
      QColor(0x7c, 0x3a, 0xed), // cat3: #7c3aed
      QColor(0xc0, 0x26, 0xd3), // cat4: #c026d3
      QColor(0xd9, 0x77, 0x06), // cat5: #d97706
      QColor(0xe1, 0x1d, 0x48), // cat6: #e11d48
  };

  static const ThemePalette kDarkPalette{
      // 1. Background, Surface & Foreground
      QColor(0x05, 0x06, 0x0f), // background: #05060f
      QColor(0x0f, 0x11, 0x1a), // surface: #0f111a
      QColor(0x17, 0x1a, 0x26), // surfaceOverlay: #171a26
      QColor(0xe9, 0xed, 0xf5), // foreground: #e9edf5
      QColor(0x8b, 0x93, 0xa7), // mutedForeground: #8b93a7

      // 2. Borders
      QColor(0xa0, 0xb4, 0xdc, 0x1f), // border: #a0b4dc1f
      QColor(0xa0, 0xb4, 0xdc, 0x29), // borderStrong: #a0b4dc29
      QColor(0x5f, 0x67, 0x79),       // borderInput: #5f6779
      QColor(0x60, 0xa5, 0xfa, 0xb3), // borderFocus
      QColor(0xe9, 0xed, 0xf5, 0x1a), // tabRail: 10% foreground

      // 3. Primary & Selection
      QColor(0xe9, 0xed, 0xf5),       // primary: #e9edf5
      QColor(0xcf, 0xd5, 0xe0),       // primaryHover: #cfd5e0
      QColor(0x05, 0x06, 0x0f),       // primaryForeground: #05060f
      QColor(0x60, 0xa5, 0xfa, 0x40), // selection: #60a5fa40

      // 4. Interaction
      QColor(0xff, 0xff, 0xff, 0x0d), // hover1
      QColor(0xff, 0xff, 0xff, 0x14), // hover2
      QColor(0xff, 0xff, 0xff, 0x21), // hover3
      QColor(0xff, 0xff, 0xff, 0x0f), // disabledBg
      QColor(0xe9, 0xed, 0xf5, 0x6b), // disabledForeground

      // 5. Semantic
      QColor(0xff, 0x63, 0x7e),       // danger: #ff637e
      QColor(0xff, 0x20, 0x56, 0x1a), // dangerBg: #ff20561a
      QColor(0xff, 0x20, 0x56, 0x4d), // dangerBorder: #ff20564d
      QColor(0x05, 0x06, 0x0f),       // dangerForeground: #05060f

      QColor(0x00, 0xd4, 0x92),       // success: #00d492
      QColor(0x00, 0xbc, 0x7d, 0x26), // successBg: #00bc7d26
      QColor(0x00, 0xbc, 0x7d, 0x4d), // successBorder: #00bc7d4d

      QColor(0xff, 0x89, 0x04),       // warning: #ff8904
      QColor(0xfe, 0x9a, 0x00, 0x26), // warningBg: #fe9a0026
      QColor(0xfe, 0x9a, 0x00, 0x4d), // warningBorder: #fe9a004d

      QColor(0x60, 0xa5, 0xfa),       // info: #60a5fa
      QColor(0x93, 0xc5, 0xfd),       // infoHover: #93c5fd
      QColor(0x3b, 0x82, 0xf6, 0x26), // infoBg: #3b82f626
      QColor(0x3b, 0x82, 0xf6, 0x4d), // infoBorder: #3b82f64d
      QColor(0x05, 0x06, 0x0f),       // infoForeground: #05060f

      QColor(0x9f, 0x9f, 0xa9),       // neutral: #9f9fa9
      QColor(0x71, 0x71, 0x7b, 0x26), // neutralBg: #71717b26
      QColor(0x71, 0x71, 0x7b, 0x4d), // neutralBorder: #71717b4d

      // 6. Category
      QColor(0x60, 0xa5, 0xfa), // cat1: #60a5fa
      QColor(0x2d, 0xd4, 0xbf), // cat2: #2dd4bf
      QColor(0xa7, 0x8b, 0xfa), // cat3: #a78bfa
      QColor(0xe8, 0x79, 0xf9), // cat4: #e879f9
      QColor(0xfb, 0xbf, 0x24), // cat5: #fbbf24
      QColor(0xfb, 0x71, 0x85), // cat6: #fb7185
  };

  return (m_currentMode == ThemeMode::Dark) ? kDarkPalette : kLightPalette;
}

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
