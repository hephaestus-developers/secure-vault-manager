/**
 * @file ui/components/menubar/app_menu_bar.cpp
 * @brief Hiện thực thanh menu chính AppMenuBar tích hợp trên UnifiedHeaderBar.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "app_menu_bar.h"
#include "app/language_manager.h"

#include <QEvent>
#include <QKeySequence>

namespace svm::components {

AppMenuBar::AppMenuBar(QWidget *parent) : QMenuBar(parent) {
  setObjectName(QStringLiteral("AppMenuBar"));
  setupMenus();
}

void AppMenuBar::setupMenus() {
  // 1. Menu Kho Dữ Liệu (Vault)
  m_vaultMenu = addMenu(tr("&Vault"));

  m_lockAct = m_vaultMenu->addAction(tr("&Lock Vault"));
  m_lockAct->setShortcut(QKeySequence(QStringLiteral("Ctrl+L")));
  connect(m_lockAct, &QAction::triggered, this,
          &AppMenuBar::lockVaultRequested);

  m_vaultMenu->addSeparator();

  m_exitAct = m_vaultMenu->addAction(tr("E&xit"));
  m_exitAct->setShortcut(QKeySequence::Quit);
  connect(m_exitAct, &QAction::triggered, this, &AppMenuBar::exitRequested);

  // 2. Menu Ngôn ngữ (Language)
  m_langMenu = addMenu(tr("&Language"));
  m_langGroup = new QActionGroup(this);
  m_langGroup->setExclusive(true);

  m_langEnAct = m_langMenu->addAction(tr("English"));
  m_langEnAct->setCheckable(true);
  m_langGroup->addAction(m_langEnAct);
  connect(m_langEnAct, &QAction::triggered, this, []() {
    app::LanguageManager::instance().setLanguage(app::AppLanguage::English);
  });

  m_langViAct = m_langMenu->addAction(tr("Vietnamese"));
  m_langViAct->setCheckable(true);
  m_langGroup->addAction(m_langViAct);
  connect(m_langViAct, &QAction::triggered, this, []() {
    app::LanguageManager::instance().setLanguage(app::AppLanguage::Vietnamese);
  });

  updateLanguageActions();

  connect(&app::LanguageManager::instance(),
          &app::LanguageManager::languageChanged, this,
          &AppMenuBar::retranslateUi);
}

void AppMenuBar::retranslateUi() {
  if (m_vaultMenu) {
    m_vaultMenu->setTitle(tr("&Vault"));
  }
  if (m_lockAct) {
    m_lockAct->setText(tr("&Lock Vault"));
  }
  if (m_exitAct) {
    m_exitAct->setText(tr("E&xit"));
  }
  if (m_langMenu) {
    m_langMenu->setTitle(tr("&Language"));
  }
  if (m_langEnAct) {
    m_langEnAct->setText(tr("English"));
  }
  if (m_langViAct) {
    m_langViAct->setText(tr("Vietnamese"));
  }
  updateLanguageActions();
}

void AppMenuBar::updateLanguageActions() {
  const auto cur = app::LanguageManager::instance().currentLanguage();
  if (m_langEnAct) {
    m_langEnAct->setChecked(cur == app::AppLanguage::English);
  }
  if (m_langViAct) {
    m_langViAct->setChecked(cur == app::AppLanguage::Vietnamese);
  }
}

void AppMenuBar::changeEvent(QEvent *event) {
  if (event->type() == QEvent::LanguageChange) {
    retranslateUi();
  }
  QMenuBar::changeEvent(event);
}

} // namespace svm::components
