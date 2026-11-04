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
#include <QTimer>

#include "app/theme_manager.h"
#include "core/core_bridge.h"
#include "main_window.h"

int main(int argc, char *argv[]) {
  // Kích hoạt hỗ trợ High-DPI
  QApplication::setHighDpiScaleFactorRoundingPolicy(
      Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

  QApplication app(argc, argv);
  app.setApplicationName(QStringLiteral("Secure Vault Manager"));
  app.setApplicationVersion(QStringLiteral("0.1.0"));
  app.setOrganizationName(QStringLiteral("Hephaestus Studio"));
  app.setOrganizationDomain(QStringLiteral("hephaestus.io"));

  // 1. Khởi tạo cầu nối C-ABI Rust Core
  auto &bridge = svm::core::CoreBridge::instance();
  if (!bridge.initialize()) {
    qWarning() << "[Main] Failed to initialize CoreBridge. Operating in "
                  "offline/degraded mode.";
  }

  // 2. Thiết lập Theme mặc định (Dark Theme chuẩn SAD)
  svm::app::ThemeManager::instance().applyTheme(svm::app::ThemeMode::Dark);

  // 3. Khởi tạo và hiển thị cửa sổ chính
  svm::MainWindow mainWindow;

  for (int i = 1; i < argc; ++i) {
    if (QString::fromLatin1(argv[i]) == QStringLiteral("--theme") &&
        i + 1 < argc) {
      QString theme = QString::fromLatin1(argv[++i]);
      if (theme == QStringLiteral("light")) {
        svm::app::ThemeManager::instance().applyTheme(
            svm::app::ThemeMode::Light);
      }
    }
  }

  mainWindow.show();

  for (int i = 1; i < argc; ++i) {
    if (QString::fromLatin1(argv[i]) == QStringLiteral("--screenshot") &&
        i + 1 < argc) {
      QString path = QString::fromLocal8Bit(argv[++i]);
      QTimer::singleShot(300, [&mainWindow, path, &app]() {
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
