/**
 * @file ui/app/tray_manager.cpp
 * @brief Hiện thực chi tiết TrayManager điều phối System Tray.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "tray_manager.h"
#include "app/language_manager.h"
#include "app/theme_manager.h"
#include "app/vault_manager.h"
#include "main_window.h"

#include <QApplication>
#include <QIcon>
#include <QKeySequence>

namespace svm::app {

TrayManager::TrayManager(MainWindow *mainWindow, QObject *parent)
    : QObject(parent), m_mainWindow(mainWindow) {}

TrayManager::~TrayManager() {
  hide();
  delete m_trayMenu;
  m_trayMenu = nullptr;
}

void TrayManager::hide() {
  if (m_trayIcon) {
    m_trayIcon->hide();
  }
}

bool TrayManager::initialize() {
  // Kết nối sự kiện thay đổi theme và language
  connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this,
          &TrayManager::onThemeChanged);
  connect(&LanguageManager::instance(), &LanguageManager::languageChanged, this,
          &TrayManager::retranslateUi);
  connect(&VaultManager::instance(), &VaultManager::lockStateChanged, this,
          &TrayManager::setVaultLocked);
  m_isVaultLocked = VaultManager::instance().isLocked();

  if (!QSystemTrayIcon::isSystemTrayAvailable()) {
    return false;
  }

  m_trayIcon = new QSystemTrayIcon(this);
  setupContextMenu();
  updateTrayTooltipAndIcon();

  connect(m_trayIcon, &QSystemTrayIcon::activated, this,
          &TrayManager::onTrayActivated);

  m_trayIcon->show();
  return true;
}

bool TrayManager::isSystemTrayAvailable() const {
  return QSystemTrayIcon::isSystemTrayAvailable();
}

void TrayManager::setupContextMenu() {
  m_trayMenu = new QMenu();
  m_trayMenu->setObjectName(QStringLiteral("TrayContextMenu"));

  m_openAction = m_trayMenu->addAction(
      ThemeManager::instance().icon(QStringLiteral("app_logo")),
      tr("Open Secure Vault Manager"), this, [this]() {
        if (m_mainWindow) {
          m_mainWindow->showNormal();
          m_mainWindow->raise();
          m_mainWindow->activateWindow();
        }
      });
  QFont font = m_openAction->font();
  font.setBold(true);
  m_openAction->setFont(font);

  m_trayMenu->addSeparator();

  m_lockAction = m_trayMenu->addAction(
      ThemeManager::instance().icon(QStringLiteral("lock")), tr("Lock Vault"),
      this, &TrayManager::toggleVaultLock);

  m_themeAction = m_trayMenu->addAction(
      ThemeManager::instance().icon(QStringLiteral("category_settings")),
      tr("Toggle Theme"), this, [this]() {
        auto &tm = ThemeManager::instance();
        auto next = (tm.currentMode() == ThemeMode::Dark) ? ThemeMode::Light
                                                          : ThemeMode::Dark;
        tm.applyTheme(next);
      });

  m_trayMenu->addSeparator();

  m_quitAction = m_trayMenu->addAction(
      ThemeManager::instance().icon(QStringLiteral("win_close")),
      tr("Quit Application"), this, [this]() {
        hide();
        QApplication::quit();
      });

  m_trayIcon->setContextMenu(m_trayMenu);
}

void TrayManager::updateTrayTooltipAndIcon() {
  if (!m_trayIcon) {
    return;
  }

  QIcon icon(QStringLiteral(":/icons/brand/app_logo.svg"));
  m_trayIcon->setIcon(icon);

  const QString statusText = m_isVaultLocked ? tr("Locked") : tr("Unlocked");
  m_trayIcon->setToolTip(
      QStringLiteral("Secure Vault Manager (%1)").arg(statusText));

  if (m_lockAction) {
    m_lockAction->setText(m_isVaultLocked ? tr("Unlock Vault")
                                          : tr("Lock Vault"));
    m_lockAction->setIcon(ThemeManager::instance().icon(
        m_isVaultLocked ? QStringLiteral("unlock") : QStringLiteral("lock")));
  }
}

void TrayManager::retranslateUi() {
  if (m_openAction) {
    m_openAction->setText(tr("Open Secure Vault Manager"));
  }
  if (m_themeAction) {
    m_themeAction->setText(tr("Toggle Theme"));
  }
  if (m_quitAction) {
    m_quitAction->setText(tr("Quit Application"));
  }
  updateTrayTooltipAndIcon();
}

void TrayManager::onTrayActivated(QSystemTrayIcon::ActivationReason reason) {
  switch (reason) {
  case QSystemTrayIcon::Trigger:
  case QSystemTrayIcon::DoubleClick:
    toggleMainWindow();
    break;
  case QSystemTrayIcon::MiddleClick:
    toggleVaultLock();
    break;
  default:
    break;
  }
}

void TrayManager::toggleMainWindow() {
  if (!m_mainWindow) {
    return;
  }
  if (m_mainWindow->isVisible() && !m_mainWindow->isMinimized() &&
      m_mainWindow->isActiveWindow()) {
    m_mainWindow->hide();
  } else {
    m_mainWindow->showNormal();
    m_mainWindow->raise();
    m_mainWindow->activateWindow();
  }
}

void TrayManager::toggleVaultLock() {
  if (!VaultManager::instance().isLocked()) {
    VaultManager::instance().lock();
    setVaultLocked(true);
    showNotification(tr("Secure Vault Manager"),
                     tr("Vault has been locked and memory zeroized."));
  } else if (m_mainWindow) {
    m_mainWindow->showNormal();
    m_mainWindow->raise();
    m_mainWindow->activateWindow();
  }
}

void TrayManager::setVaultLocked(bool locked) {
  m_isVaultLocked = locked;
  updateTrayTooltipAndIcon();
}

void TrayManager::showNotification(const QString &title, const QString &message,
                                   QSystemTrayIcon::MessageIcon icon) {
  if (m_trayIcon && m_trayIcon->isVisible()) {
    m_trayIcon->showMessage(title, message, icon, 3000);
  }
}

void TrayManager::onThemeChanged() { updateTrayTooltipAndIcon(); }

} // namespace svm::app
