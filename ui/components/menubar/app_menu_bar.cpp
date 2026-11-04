/**
 * @file ui/components/menubar/app_menu_bar.cpp
 * @brief Hiện thực thanh menu chính AppMenuBar tích hợp trên UnifiedHeaderBar.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "app_menu_bar.h"
#include <QKeySequence>

namespace svm::components {

AppMenuBar::AppMenuBar(QWidget *parent) : QMenuBar(parent) {
  setObjectName(QStringLiteral("AppMenuBar"));
  setupMenus();
}

void AppMenuBar::setupMenus() {
  // 1. Menu Kho Dữ Liệu (Vault)
  m_vaultMenu = addMenu(tr("&Vault"));

  m_quickLauncherAct = m_vaultMenu->addAction(tr("&Quick Launcher..."));
  m_quickLauncherAct->setShortcut(
      QKeySequence(QStringLiteral("Ctrl+Shift+Space")));
  connect(m_quickLauncherAct, &QAction::triggered, this,
          &AppMenuBar::quickLauncherRequested);

  m_lockAct = m_vaultMenu->addAction(tr("&Lock Vault"));
  m_lockAct->setShortcut(QKeySequence(QStringLiteral("Ctrl+L")));
  connect(m_lockAct, &QAction::triggered, this,
          &AppMenuBar::lockVaultRequested);

  m_vaultMenu->addSeparator();

  m_exitAct = m_vaultMenu->addAction(tr("E&xit"));
  m_exitAct->setShortcut(QKeySequence::Quit);
  connect(m_exitAct, &QAction::triggered, this, &AppMenuBar::exitRequested);

  // 2. Menu Hiển Thị (View)
  m_viewMenu = addMenu(tr("&View"));

  m_themeAct = m_viewMenu->addAction(tr("Toggle &Theme (Dark/Light)"));
  m_themeAct->setShortcut(QKeySequence(QStringLiteral("Ctrl+T")));
  connect(m_themeAct, &QAction::triggered, this,
          &AppMenuBar::toggleThemeRequested);

  // 3. Menu Trợ Giúp (Help)
  m_helpMenu = addMenu(tr("&Help"));

  m_aboutAct = m_helpMenu->addAction(tr("&About Secure Vault Manager..."));
  connect(m_aboutAct, &QAction::triggered, this, &AppMenuBar::aboutRequested);
}

} // namespace svm::components
