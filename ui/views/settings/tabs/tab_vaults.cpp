/**
 * @file ui/views/settings/tabs/tab_vaults.cpp
 * @brief Hiện thực giao diện Quản lý Kho Dữ liệu.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "tab_vaults.h"
#include "app/theme_manager.h"
#include "app/vault_manager.h"
#include "views/vaults/new_vault_dialog.h"

#include <QDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QVBoxLayout>

namespace svm::views {

TabVaults::TabVaults(QWidget *parent) : QWidget(parent) {
  setupUI();
  refreshVaultList();
  connect(&app::VaultManager::instance(), &app::VaultManager::vaultListChanged,
          this, &TabVaults::refreshVaultList);
}

void TabVaults::setupUI() {
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(16);

  // 1. Vaults List Card
  m_vaultsListCard = new SettingSectionCard(tr("Active Vaults"), this);

  auto *topBar = new QHBoxLayout();
  auto *subTitle = new QLabel(
      tr("Manage your isolated cryptographic vaults and default locations."),
      m_vaultsListCard);
  subTitle->setStyleSheet(QStringLiteral("font-size: 11.5px; color: #a6adc8;"));
  topBar->addWidget(subTitle);
  topBar->addStretch(1);

  m_newVaultBtn = new QPushButton(tr("+ New Vault..."), m_vaultsListCard);
  m_newVaultBtn->setObjectName(QStringLiteral("OnboardingPrimaryBtn"));
  m_newVaultBtn->setFixedHeight(30);
  m_newVaultBtn->setCursor(Qt::PointingHandCursor);
  connect(m_newVaultBtn, &QPushButton::clicked, this,
          &TabVaults::onNewVaultClicked);
  topBar->addWidget(m_newVaultBtn);

  auto *topBarWidget = new QWidget(m_vaultsListCard);
  topBarWidget->setLayout(topBar);
  m_vaultsListCard->addWidget(topBarWidget);

  m_vaultsContainerLayout = new QVBoxLayout();
  m_vaultsContainerLayout->setContentsMargins(0, 8, 0, 0);
  m_vaultsContainerLayout->setSpacing(8);

  auto *containerWidget = new QWidget(m_vaultsListCard);
  containerWidget->setLayout(m_vaultsContainerLayout);
  m_vaultsListCard->addWidget(containerWidget);

  layout->addWidget(m_vaultsListCard);

  // 2. Danger Zone Card
  m_dangerCard = new SettingSectionCard(tr("Danger Zone"), this);

  auto *delControl = new QWidget(m_dangerCard);
  auto *delLayout = new QHBoxLayout(delControl);
  delLayout->setContentsMargins(0, 0, 0, 0);
  delLayout->setSpacing(8);

  m_deleteCombo = new QComboBox(delControl);
  m_deleteCombo->setFixedWidth(160);
  delLayout->addWidget(m_deleteCombo);

  m_deleteBtn = new QPushButton(tr("Delete Vault..."), delControl);
  m_deleteBtn->setObjectName(QStringLiteral("DangerButton"));
  m_deleteBtn->setFixedHeight(32);
  m_deleteBtn->setCursor(Qt::PointingHandCursor);
  connect(m_deleteBtn, &QPushButton::clicked, this,
          &TabVaults::onDeleteVaultClicked);
  delLayout->addWidget(m_deleteBtn);

  m_deleteRow = new SettingRowWidget(
      tr("Permanently Delete Vault"),
      tr("Permanently erase a vault and all secrets stored inside it. This "
         "action cannot be undone."),
      delControl, m_dangerCard);
  m_dangerCard->addRow(m_deleteRow);

  layout->addWidget(m_dangerCard);
  layout->addStretch(1);
}

void TabVaults::refreshVaultList() {
  if (!m_vaultsContainerLayout) {
    return;
  }

  // Xóa sạch widget cũ trong container
  QLayoutItem *child = nullptr;
  while ((child = m_vaultsContainerLayout->takeAt(0)) != nullptr) {
    if (child->widget()) {
      child->widget()->deleteLater();
    }
    delete child;
  }

  const auto vaults = app::VaultManager::instance().vaults();

  if (m_deleteCombo) {
    m_deleteCombo->clear();
  }

  for (const auto &v : vaults) {
    auto *rowWidget = new QWidget(this);
    rowWidget->setStyleSheet(
        QStringLiteral("background-color: #1e1e2e; border: 1px solid #313244; "
                       "border-radius: 6px; padding: 4px;"));
    auto *rowLayout = new QHBoxLayout(rowWidget);
    rowLayout->setContentsMargins(12, 8, 12, 8);
    rowLayout->setSpacing(12);

    // Color dot
    auto *dot = new QLabel(rowWidget);
    dot->setFixedSize(12, 12);
    dot->setStyleSheet(
        QStringLiteral("background-color: %1; border-radius: 6px;")
            .arg(v.accentColor));
    rowLayout->addWidget(dot);

    // Icon
    auto *iconLbl = new QLabel(rowWidget);
    iconLbl->setFixedSize(18, 18);
    iconLbl->setPixmap(
        app::ThemeManager::instance().icon(v.iconName).pixmap(18, 18));
    rowLayout->addWidget(iconLbl);

    // Name & Description
    auto *infoLayout = new QVBoxLayout();
    infoLayout->setSpacing(2);

    auto *nameLbl = new QLabel(v.name, rowWidget);
    nameLbl->setStyleSheet(
        QStringLiteral("font-size: 13px; font-weight: 600; color: #cdd6f4;"));
    infoLayout->addWidget(nameLbl);

    if (!v.description.isEmpty()) {
      auto *descLbl = new QLabel(v.description, rowWidget);
      descLbl->setStyleSheet(
          QStringLiteral("font-size: 11px; color: #a6adc8;"));
      infoLayout->addWidget(descLbl);
    }
    rowLayout->addLayout(infoLayout, 1);

    // Default badge
    if (v.isDefault) {
      auto *badge = new QLabel(tr("DEFAULT"), rowWidget);
      badge->setStyleSheet(QStringLiteral(
          "background-color: #89b4fa; color: #11111b; font-size: "
          "9.5px; font-weight: 700; border-radius: 3px; "
          "padding: 2px 6px;"));
      rowLayout->addWidget(badge);
    } else {
      auto *setDefaultBtn = new QPushButton(tr("Set Default"), rowWidget);
      setDefaultBtn->setFixedHeight(28);
      setDefaultBtn->setCursor(Qt::PointingHandCursor);
      const QString vId = v.id;
      connect(setDefaultBtn, &QPushButton::clicked, this,
              [vId]() { app::VaultManager::instance().setDefaultVault(vId); });
      rowLayout->addWidget(setDefaultBtn);

      // Thêm vào combo xóa (không cho xóa vault mặc định)
      if (m_deleteCombo) {
        m_deleteCombo->addItem(v.name, v.id);
      }
    }

    // Edit button
    auto *editBtn = new QPushButton(tr("Edit"), rowWidget);
    editBtn->setFixedHeight(28);
    editBtn->setCursor(Qt::PointingHandCursor);
    const auto vaultCopy = v;
    connect(editBtn, &QPushButton::clicked, this,
            [this, vaultCopy]() { onEditVaultClicked(vaultCopy); });
    rowLayout->addWidget(editBtn);

    m_vaultsContainerLayout->addWidget(rowWidget);
  }

  // Khóa nút xóa nếu không còn vault nào có thể xóa hoặc tổng vault <= 1
  if (m_deleteBtn) {
    m_deleteBtn->setEnabled(vaults.size() > 1 && m_deleteCombo->count() > 0);
  }
}

void TabVaults::onNewVaultClicked() {
  NewVaultDialog dlg(this);
  if (dlg.exec() == QDialog::Accepted) {
    refreshVaultList();
  }
}

void TabVaults::onEditVaultClicked(const models::UiVault &vault) {
  NewVaultDialog dlg(this);
  dlg.setEditVault(vault);
  if (dlg.exec() == QDialog::Accepted) {
    refreshVaultList();
  }
}

void TabVaults::onDeleteVaultClicked() {
  if (!m_deleteCombo || m_deleteCombo->count() == 0) {
    return;
  }
  const QString vId = m_deleteCombo->currentData().toString();
  const QString vName = m_deleteCombo->currentText();

  QDialog dlg(this);
  dlg.setWindowTitle(tr("Confirm Vault Deletion"));
  dlg.setModal(true);
  dlg.setFixedSize(440, 240);

  auto *layout = new QVBoxLayout(&dlg);
  layout->setContentsMargins(24, 20, 24, 20);
  layout->setSpacing(12);

  auto *title = new QLabel(tr("Permanently Delete Vault?"), &dlg);
  QFont f = title->font();
  f.setPointSize(13);
  f.setBold(true);
  title->setFont(f);
  title->setStyleSheet(QStringLiteral("color: #f38ba8;"));
  layout->addWidget(title);

  auto *warn = new QLabel(tr("This action will erase ALL secrets inside "
                             "\"%1\". Type the vault name below to confirm:")
                              .arg(vName),
                          &dlg);
  warn->setWordWrap(true);
  layout->addWidget(warn);

  auto *confirmEdit = new QLineEdit(&dlg);
  confirmEdit->setPlaceholderText(vName);
  confirmEdit->setFixedHeight(34);
  layout->addWidget(confirmEdit);

  auto *btnLayout = new QHBoxLayout();
  btnLayout->addStretch(1);
  auto *cancelBtn = new QPushButton(tr("Cancel"), &dlg);
  cancelBtn->setFixedHeight(32);
  connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
  btnLayout->addWidget(cancelBtn);

  auto *confirmBtn =
      new QPushButton(tr("I understand, delete this vault"), &dlg);
  confirmBtn->setObjectName(QStringLiteral("DangerButton"));
  confirmBtn->setFixedHeight(32);
  confirmBtn->setEnabled(false);
  connect(confirmEdit, &QLineEdit::textChanged, &dlg,
          [confirmBtn, vName](const QString &t) {
            confirmBtn->setEnabled(t == vName);
          });
  connect(confirmBtn, &QPushButton::clicked, &dlg, [&]() {
    app::VaultManager::instance().deleteVault(vId);
    dlg.accept();
    refreshVaultList();
  });
  btnLayout->addWidget(confirmBtn);
  layout->addLayout(btnLayout);

  dlg.exec();
}

void TabVaults::retranslateUi() {
  if (m_vaultsListCard) {
    m_vaultsListCard->setTitle(tr("Active Vaults"));
  }
  if (m_newVaultBtn) {
    m_newVaultBtn->setText(tr("+ New Vault..."));
  }
  if (m_dangerCard) {
    m_dangerCard->setTitle(tr("Danger Zone"));
  }
  if (m_deleteBtn) {
    m_deleteBtn->setText(tr("Delete Vault..."));
  }
  if (m_deleteRow) {
    m_deleteRow->updateText(
        tr("Permanently Delete Vault"),
        tr("Permanently erase a vault and all secrets stored inside it. This "
           "action cannot be undone."));
  }
  refreshVaultList();
}

} // namespace svm::views
