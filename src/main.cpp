/**
 * @file ui/main.cpp
 * @brief Điểm khởi nhập (Entry Point) của ứng dụng Secure Vault Manager Qt6
 * Desktop.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include <QApplication>
#include <QDebug>
#include <QListWidget>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>

#include "core/CoreBridge.hpp"
#include "core/LogCategories.hpp"
#include "ui/MainWindow.hpp"
#include "ui/app/LanguageManager.hpp"
#include "ui/app/SettingsManager.hpp"
#include "ui/app/ThemeManager.hpp"
#include "ui/app/TrayManager.hpp"
#include "ui/app/VaultManager.hpp"
#include "ui/components/NotificationDialog.hpp"
#include "ui/views/detail/EmptyStateWidget.hpp"
#include "ui/views/lock/ResetVaultDialog.hpp"
#include "ui/window/SingleInstanceController.hpp"
#include "ui/window/WindowManager.hpp"

int main(int argc, char *argv[]) {
  // Cài đặt bộ ghi log có cấu trúc cho ứng dụng
  svm::core::installCustomMessageHandler();

  // Kích hoạt hỗ trợ High-DPI
  QApplication::setHighDpiScaleFactorRoundingPolicy(
      Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

  QApplication app(argc, argv);
  Q_INIT_RESOURCE(svm);
  app.setApplicationName(QStringLiteral("Secure Vault Manager"));
  app.setApplicationVersion(QStringLiteral("0.1.0"));
  app.setOrganizationName(QStringLiteral("Hephaestus Developers"));
  app.setOrganizationDomain(QStringLiteral("hephaestus.io"));

  // Khởi tạo WindowManager quản lý vòng đời và cây cửa sổ ứng dụng
  WindowManager::instance().init(&app);

  // Đảm bảo ứng dụng tiếp tục chạy ngầm trong System Tray khi đóng cửa sổ chính
  QApplication::setQuitOnLastWindowClosed(false);

  // Kiểm tra Single Instance (bỏ qua khi chạy các chế độ preview/screenshot)
  bool isPreviewMode = false;
  for (int i = 1; i < argc; ++i) {
    const QString arg = QString::fromLatin1(argv[i]);
    if (arg == QStringLiteral("--screenshot") ||
        arg == QStringLiteral("--preview-notification") ||
        arg == QStringLiteral("--preview-empty-state") ||
        arg == QStringLiteral("--preview-reset-dialog")) {
      isPreviewMode = true;
      break;
    }
  }

  SingleInstanceController singleInstance;
  if (!isPreviewMode && !singleInstance.checkOrStart(
                            &app, QStringLiteral("SecureVaultManager"))) {
    qCInfo(logWindow) << "Another instance is already running. Activated "
                         "existing instance and exiting.";
    return 0;
  }

  // 1. Khởi tạo cầu nối CXX Rust Core
  auto &bridge = svm::core::CoreBridge::instance();
  if (!bridge.initialize()) {
    qCWarning(logVault) << "Failed to initialize CoreBridge. Operating in "
                           "offline/degraded mode.";
  }

  // 2. Khởi tạo Settings, Language, Vault, và Theme
  svm::app::SettingsManager::instance().initialize();
  svm::app::LanguageManager::instance().initialize();
  svm::app::VaultManager::instance().initialize();
  svm::app::ThemeManager::instance().initialize();

  const auto initialTheme = svm::app::SettingsManager::instance().themeMode();
  svm::app::ThemeManager::instance().applyTheme(initialTheme);

  // 3. Khởi tạo cửa sổ chính và đăng ký làm Root Window với WindowManager
  svm::MainWindow mainWindow;
  WindowManager::instance().registerRootWindow(&mainWindow);

  svm::app::TrayManager trayManager(&mainWindow);
  trayManager.initialize();
  mainWindow.setTrayManager(&trayManager);

  QObject::connect(&app, &QApplication::aboutToQuit, &app, [&]() {
    mainWindow.stopSleepMonitor();
    trayManager.hide();
    bridge.shutdown();
  });

  bool startMinimized =
      !isPreviewMode && svm::app::SettingsManager::instance().startMinimized();

  for (int i = 1; i < argc; ++i) {
    const QString arg = QString::fromLatin1(argv[i]);
    if (arg == QStringLiteral("--lang") && i + 1 < argc) {
      QString lang = QString::fromLatin1(argv[++i]);
      svm::app::LanguageManager::instance().setLanguage(
          svm::app::LanguageManager::languageFromCode(lang));
    } else if (arg == QStringLiteral("--theme") && i + 1 < argc) {
      QString theme = QString::fromLatin1(argv[++i]);
      if (theme.compare(QStringLiteral("light"), Qt::CaseInsensitive) == 0) {
        svm::app::ThemeManager::instance().applyTheme(
            svm::app::ThemeMode::Light);
      } else if (theme.compare(QStringLiteral("dark"), Qt::CaseInsensitive) ==
                 0) {
        svm::app::ThemeManager::instance().applyTheme(
            svm::app::ThemeMode::Dark);
      }
    } else if (arg == QStringLiteral("--minimized") ||
               arg == QStringLiteral("--tray")) {
      startMinimized = true;
    } else if (arg == QStringLiteral("--collapsed") ||
               arg == QStringLiteral("--collapse-sidebar")) {
      if (mainWindow.sidebarView()) {
        mainWindow.sidebarView()->setCollapsed(true);
      }
    } else if (arg == QStringLiteral("--unlock")) {
      svm::app::VaultManager::instance().unlockWithBiometrics();
    } else if (arg == QStringLiteral("--settings")) {
      int tabIdx = 2;
      if (i + 1 < argc && argv[i + 1][0] >= '0' && argv[i + 1][0] <= '9') {
        tabIdx = QString::fromLatin1(argv[++i]).toInt();
      }
      QTimer::singleShot(100, [&mainWindow, tabIdx]() {
        mainWindow.openSettingsDialog();
        QDialog *dlg = nullptr;
        for (auto *child : mainWindow.findChildren<QDialog *>()) {
          if (child->isVisible()) {
            dlg = child;
            break;
          }
        }
        if (dlg) {
          if (auto *navList = dlg->findChild<QListWidget *>(
                  QStringLiteral("SettingsNavList"))) {
            navList->setCurrentRow(tabIdx);
          }
        }
      });
    } else if (arg == QStringLiteral("--about")) {
      QTimer::singleShot(50, [&mainWindow]() {
        if (auto *aboutBtn = mainWindow.findChild<QPushButton *>(
                QStringLiteral("HeaderAboutBtn"))) {
          aboutBtn->click();
        }
      });
    } else if (arg == QStringLiteral("--select-sidebar-row") && i + 1 < argc) {
      int row = QString::fromLatin1(argv[++i]).toInt();
      QTimer::singleShot(100, [&mainWindow, row]() {
        for (auto *listWidget : mainWindow.findChildren<QListWidget *>()) {
          if (listWidget->count() > row) {
            listWidget->setCurrentRow(row);
            emit listWidget->itemClicked(listWidget->item(row));
            break;
          }
        }
      });
    } else if (arg == QStringLiteral("--edit")) {
      QTimer::singleShot(50, [&mainWindow]() {
        if (auto *editBtn = mainWindow.findChild<QPushButton *>(
                QStringLiteral("DetailHeaderEditBtn"))) {
          editBtn->click();
        }
      });
    } else if (arg == QStringLiteral("--preview-notification")) {
      QString notifType = (i + 1 < argc) ? QString::fromLatin1(argv[++i])
                                         : QStringLiteral("info");
      QTimer::singleShot(50, [&mainWindow, notifType]() {
        auto *dlg = new svm::components::NotificationDialog(&mainWindow);
        if (notifType == QStringLiteral("success")) {
          dlg->setType(svm::components::NotificationType::Success);
          dlg->setTitle(QObject::tr("Success"));
          dlg->setMessage(QObject::tr("Master Password updated successfully!"));
          dlg->setButtons(QObject::tr("OK"));
        } else if (notifType == QStringLiteral("warning")) {
          dlg->setType(svm::components::NotificationType::Warning);
          dlg->setTitle(QObject::tr("Export Failed"));
          dlg->setMessage(
              QObject::tr("Could not parse or import the selected file."));
          dlg->setButtons(QObject::tr("OK"));
        } else if (notifType == QStringLiteral("question")) {
          dlg->setType(svm::components::NotificationType::Question);
          dlg->setTitle(QObject::tr("Security Risk Warning"));
          dlg->setMessage(
              QObject::tr("You are about to export your entire password vault "
                          "in PLAINTEXT format.\nAnyone who accesses this file "
                          "will be able to read your passwords, cards, and "
                          "keys.\n\nAre you sure you want to proceed?"));
          dlg->setButtons(QObject::tr("Export Anyway"), QObject::tr("Cancel"),
                          true);
        } else {
          dlg->setType(svm::components::NotificationType::Information);
          dlg->setTitle(QObject::tr("Trash Purged"));
          dlg->setMessage(
              QObject::tr("All deleted items in trash have been purged."));
          dlg->setButtons(QObject::tr("OK"));
        }
        dlg->show();
      });
    } else if (arg == QStringLiteral("--preview-empty-state")) {
      QTimer::singleShot(50, [&mainWindow]() {
        auto *dlg = new QDialog(&mainWindow);
        dlg->setObjectName(QStringLiteral("EmptyStatePreviewDialog"));
        dlg->resize(620, 620);
        auto *l = new QVBoxLayout(dlg);
        l->setContentsMargins(0, 0, 0, 0);
        l->addWidget(new svm::views::EmptyStateWidget(dlg));
        dlg->show();
      });
    } else if (arg == QStringLiteral("--preview-reset-dialog")) {
      QTimer::singleShot(50, [&mainWindow]() {
        auto *dlg = new svm::views::ResetVaultDialog(&mainWindow);
        dlg->show();
      });
    } else if (arg == QStringLiteral("--onboarding-step") && i + 1 < argc) {
      int step = QString::fromLatin1(argv[++i]).toInt();
      QTimer::singleShot(150, [&mainWindow, step]() {
        if (auto *createVaultBtn = mainWindow.findChild<QPushButton *>(
                QStringLiteral("OnboardingPrimaryBtn"))) {
          createVaultBtn->click();
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
      QTimer::singleShot(900, [&mainWindow, path, scrollBottom, &app]() {
        QDialog *dlg = nullptr;
        for (auto *child : mainWindow.findChildren<QDialog *>()) {
          if (child->isVisible()) {
            dlg = child;
            break;
          }
        }
        if (dlg) {
          if (dlg->isVisible()) {
            if (scrollBottom) {
              for (auto *sa : dlg->findChildren<QScrollArea *>()) {
                if (sa->verticalScrollBar()) {
                  sa->verticalScrollBar()->setValue(
                      sa->verticalScrollBar()->maximum());
                }
              }
            }
            QPixmap pix = dlg->grab();
            pix.save(path);
            qInfo() << "[Main] Dialog screenshot saved to:" << path;
            app.quit();
            return;
          }
        }
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
