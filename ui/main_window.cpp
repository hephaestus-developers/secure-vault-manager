/**
 * @file ui/main_window.cpp
 * @brief Hiện thực chi tiết MainWindow tích hợp UnifiedHeaderBar chuẩn Native
 * Desktop.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "main_window.h"
#include "app/theme_manager.h"
#include "app/tray_manager.h"
#include "components/menubar/app_menu_bar.h"
#include "components/unified_header_bar.h"
#include "components/window_resize_handler.h"
#include "core/core_bridge.h"

#include <QApplication>
#include <QCloseEvent>
#include <QEvent>
#include <QHBoxLayout>
#include <QIcon>
#include <QMessageBox>
#include <QVBoxLayout>

namespace svm {

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
  // Cấu hình cờ cửa sổ Frameless sử dụng UnifiedHeaderBar làm Title Bar
  setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
  setAttribute(Qt::WA_TranslucentBackground, false);
  setMinimumSize(720, 480);
  resize(1024, 640);
  setWindowTitle(QStringLiteral("Secure Vault Manager"));

  setupUI();

  // Khởi tạo bộ xử lý co giãn kích thước cửa sổ không viền 8 hướng
  m_resizeHandler = new components::WindowResizeHandler(this);
}

void MainWindow::setupUI() {
  auto *centralWidget = new QWidget(this);
  centralWidget->setObjectName(QStringLiteral("CentralWidget"));
  auto *layout = new QVBoxLayout(centralWidget);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  // 1. Khởi tạo AppMenuBar và UnifiedHeaderBar
  m_menuBar = new components::AppMenuBar(this);
  m_headerBar = new components::UnifiedHeaderBar(m_menuBar, this);
  m_headerBar->setTitleText(windowTitle());
  layout->addWidget(m_headerBar);

  // 2. Vùng chứa nội dung chính (ContentArea)
  m_contentArea = new QWidget(centralWidget);
  m_contentArea->setObjectName(QStringLiteral("ContentArea"));
  auto *contentLayout = new QVBoxLayout(m_contentArea);
  contentLayout->setAlignment(Qt::AlignCenter);
  contentLayout->setContentsMargins(40, 40, 40, 40);

  // Logo & Chào mừng tối giản
  auto *welcomeCard = new QWidget(m_contentArea);
  welcomeCard->setObjectName(QStringLiteral("CardWidget"));
  welcomeCard->setFixedWidth(480);

  auto *cardLayout = new QVBoxLayout(welcomeCard);
  cardLayout->setContentsMargins(32, 32, 32, 32);
  cardLayout->setSpacing(14);
  cardLayout->setAlignment(Qt::AlignCenter);

  auto *logoLabel = new QLabel(welcomeCard);
  logoLabel->setObjectName(QStringLiteral("CardLogo"));
  logoLabel->setPixmap(
      QIcon(QStringLiteral(":/icons/app_logo.svg")).pixmap(48, 48));
  logoLabel->setAlignment(Qt::AlignCenter);

  auto *titleLabel =
      new QLabel(QStringLiteral("Secure Vault Manager"), welcomeCard);
  titleLabel->setObjectName(QStringLiteral("CardTitleLabel"));
  titleLabel->setAlignment(Qt::AlignCenter);

  const QString version = core::CoreBridge::instance().coreVersion();
  auto *descLabel = new QLabel(
      QStringLiteral("Hệ thống bảo mật bộ nhớ Native • Rust Core v%1\n"
                     "Giao diện C++17 Qt6 Frameless với Unified Header Bar.")
          .arg(version),
      welcomeCard);
  descLabel->setObjectName(QStringLiteral("CardDescLabel"));
  descLabel->setAlignment(Qt::AlignCenter);

  cardLayout->addWidget(logoLabel);
  cardLayout->addWidget(titleLabel);
  cardLayout->addWidget(descLabel);

  contentLayout->addWidget(welcomeCard);
  layout->addWidget(m_contentArea, 1);

  setCentralWidget(centralWidget);

  // 3. Kết nối các tín hiệu điều khiển cửa sổ từ UnifiedHeaderBar
  connect(m_headerBar, &components::UnifiedHeaderBar::minimizeRequested, this,
          &QMainWindow::showMinimized);
  connect(m_headerBar, &components::UnifiedHeaderBar::maximizeRestoreRequested,
          this, [this]() {
            if (isMaximized()) {
              showNormal();
            } else {
              showMaximized();
            }
          });
  connect(m_headerBar, &components::UnifiedHeaderBar::closeRequested, this,
          &QMainWindow::close);

  // 4. Kết nối các hành động từ AppMenuBar và ThemeManager
  connect(&app::ThemeManager::instance(), &app::ThemeManager::themeChanged,
          this, [this](app::ThemeMode mode) {
            if (m_headerBar) {
              m_headerBar->updateThemeIcons(mode == app::ThemeMode::Dark);
            }
          });

  connect(m_menuBar, &components::AppMenuBar::lockVaultRequested, this,
          [this]() {
            if (m_trayManager) {
              m_trayManager->toggleVaultLock();
            }
          });
  connect(m_menuBar, &components::AppMenuBar::exitRequested, qApp,
          &QApplication::quit);
  connect(m_menuBar, &components::AppMenuBar::toggleThemeRequested, this,
          [this]() {
            auto &tm = app::ThemeManager::instance();
            auto nextMode = (tm.currentMode() == app::ThemeMode::Dark)
                                ? app::ThemeMode::Light
                                : app::ThemeMode::Dark;
            tm.applyTheme(nextMode);
          });
  connect(m_menuBar, &components::AppMenuBar::aboutRequested, this, [this]() {
    const QString ver = core::CoreBridge::instance().coreVersion();
    QMessageBox::about(
        this, QStringLiteral("About Secure Vault Manager"),
        QStringLiteral("<h3>Secure Vault Manager</h3>"
                       "<p>Phiên bản: 0.1.0<br/>"
                       "Rust Core: v%1 (C-ABI libsvm_core.so)</p>"
                       "<p>Bản quyền © 2026 Hephaestus Developers.<br/>"
                       "All rights reserved.</p>")
            .arg(ver));
  });
}

void MainWindow::setTrayManager(app::TrayManager *trayManager) {
  m_trayManager = trayManager;
}

void MainWindow::closeEvent(QCloseEvent *event) {
  if (m_trayManager && m_trayManager->isSystemTrayAvailable()) {
    hide();
    event->ignore();
  } else {
    event->accept();
  }
}

void MainWindow::resizeEvent(QResizeEvent *event) {
  QMainWindow::resizeEvent(event);
  if (m_resizeHandler) {
    m_resizeHandler->updateGeometry();
  }
}

void MainWindow::changeEvent(QEvent *event) {
  if (event->type() == QEvent::WindowStateChange) {
    bool maximized = isMaximized();
    if (m_headerBar) {
      m_headerBar->updateMaximizeIcon(maximized);
    }
    if (m_resizeHandler) {
      m_resizeHandler->setVisible(!maximized);
    }
  }
  QMainWindow::changeEvent(event);
}

} // namespace svm
