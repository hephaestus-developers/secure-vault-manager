/**
 * @file ui/main.cpp
 * @brief Điểm khởi nhập (Entry Point) của ứng dụng Secure Vault Manager Qt6
 * Desktop.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include <QApplication>
#include <QDebug>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>

#include "app/language_manager.h"
#include "app/theme_manager.h"
#include "app/tray_manager.h"
#include "app/vault_manager.h"
#include "core/core_bridge.h"
#include "main_window.h"

int main(int argc, char *argv[]) {
  // Kích hoạt hỗ trợ High-DPI
  QApplication::setHighDpiScaleFactorRoundingPolicy(
      Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

  QApplication app(argc, argv);
  app.setApplicationName(QStringLiteral("Secure Vault Manager"));
  app.setApplicationVersion(QStringLiteral("0.1.0"));
  app.setOrganizationName(QStringLiteral("Hephaestus Developers"));
  app.setOrganizationDomain(QStringLiteral("hephaestus.io"));

  // Đảm bảo ứng dụng tiếp tục chạy ngầm trong System Tray khi đóng cửa sổ chính
  QApplication::setQuitOnLastWindowClosed(false);

  // 1. Khởi tạo cầu nối C-ABI Rust Core
  auto &bridge = svm::core::CoreBridge::instance();
  if (!bridge.initialize()) {
    qWarning() << "[Main] Failed to initialize CoreBridge. Operating in "
                  "offline/degraded mode.";
  }

  // 2. Khởi tạo Đa ngôn ngữ, Quản lý Vault và Theme
  svm::app::LanguageManager::instance().initialize();
  svm::app::VaultManager::instance().initialize();
  svm::app::ThemeManager::instance().applyTheme(svm::app::ThemeMode::Dark);

  // 3. Khởi tạo cửa sổ chính và TrayManager
  svm::MainWindow mainWindow;
  svm::app::TrayManager trayManager(&mainWindow);
  trayManager.initialize();
  mainWindow.setTrayManager(&trayManager);

  bool startMinimized = false;

  for (int i = 1; i < argc; ++i) {
    const QString arg = QString::fromLatin1(argv[i]);
    if (arg == QStringLiteral("--lang") && i + 1 < argc) {
      QString lang = QString::fromLatin1(argv[++i]);
      svm::app::LanguageManager::instance().setLanguage(
          svm::app::LanguageManager::languageFromCode(lang));
    } else if (arg == QStringLiteral("--theme") && i + 1 < argc) {
      QString theme = QString::fromLatin1(argv[++i]);
      if (theme == QStringLiteral("light")) {
        svm::app::ThemeManager::instance().applyTheme(
            svm::app::ThemeMode::Light);
      }
    } else if (arg == QStringLiteral("--minimized") ||
               arg == QStringLiteral("--tray")) {
      startMinimized = true;
    } else if (arg == QStringLiteral("--collapsed") ||
               arg == QStringLiteral("--collapse-sidebar")) {
      if (mainWindow.sidebarView()) {
        mainWindow.sidebarView()->setCollapsed(true);
      }
    } else if (arg == QStringLiteral("--edit")) {
      QTimer::singleShot(50, [&mainWindow]() {
        if (auto *editBtn = mainWindow.findChild<QPushButton *>(
                QStringLiteral("DetailHeaderEditBtn"))) {
          editBtn->click();
        }
      });
    }
  }

  if (!startMinimized) {
    mainWindow.show();
  }

  bool scrollBottom = false;
  for (int i = 1; i < argc; ++i) {
    if (QString::fromLatin1(argv[i]) == QStringLiteral("--scroll-bottom")) {
      scrollBottom = true;
    }
  }

  for (int i = 1; i < argc; ++i) {
    if (QString::fromLatin1(argv[i]) == QStringLiteral("--screenshot") &&
        i + 1 < argc) {
      QString path = QString::fromLocal8Bit(argv[++i]);
      QTimer::singleShot(400, [&mainWindow, path, scrollBottom, &app]() {
        if (scrollBottom) {
          for (auto *sa : mainWindow.findChildren<QScrollArea *>()) {
            if (sa->verticalScrollBar()) {
              sa->verticalScrollBar()->setValue(
                  sa->verticalScrollBar()->maximum());
            }
          }
        }
        QPixmap pix = mainWindow.grab();
        pix.save(path);
        qInfo() << "[Main] Screenshot saved to:" << path;
        app.quit();
      });
    }
  }

  qInfo() << "[Main] Secure Vault Manager UI started successfully.";
  return app.exec();
}
