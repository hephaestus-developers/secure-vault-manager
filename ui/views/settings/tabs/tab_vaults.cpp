/**
 * @file ui/views/settings/tabs/tab_vaults.cpp
 * @brief Hiện thực giao diện Quản lý Kho Dữ liệu.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "tab_vaults.h"
#include "app/theme_manager.h"
#include "app/vault_manager.h"
#include "components/dialog_drag_helper.h"
#include "components/in_window_combobox.h"
#include "components/window_resize_handler.h"
#include "views/vaults/new_vault_dialog.h"

#include <QDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

namespace svm::views {

TabVaults::TabVaults(QWidget *parent) : QWidget(parent) {
  setAttribute(Qt::WA_StyledBackground, true);
  setupUI();
  refreshVaultList();
  connect(&app::VaultManager::instance(), &app::VaultManager::vaultListChanged,
          this, &TabVaults::refreshVaultList);
  connect(&app::ThemeManager::instance(), &app::ThemeManager::themeChanged,
          this, &TabVaults::updateThemeIcons);
}

void TabVaults::setupUI() {
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(18, 14, 14, 16);
  layout->setSpacing(16);

  // 1. Vaults List Card
  m_vaultsListCard = new SettingSectionCard(tr("Active Vaults"), this);

  auto *topBar = new QHBoxLayout();
  auto *subTitle = new QLabel(
      tr("Manage your isolated cryptographic vaults and default locations."),
      m_vaultsListCard);
  subTitle->setObjectName(QStringLiteral("SettingRowDesc"));
  subTitle->setWordWrap(true);
  subTitle->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  subTitle->setMinimumWidth(0);
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

  m_deleteCombo = new svm::ui::InWindowComboBox(delControl);
  m_deleteCombo->setFixedWidth(200);
  m_deleteCombo->setFixedHeight(32);
  m_deleteCombo->setCursor(Qt::PointingHandCursor);
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
    rowWidget->setObjectName(QStringLiteral("SettingsListItemCard"));
    rowWidget->setAttribute(Qt::WA_StyledBackground, true);
    auto *rowLayout = new QHBoxLayout(rowWidget);
    rowLayout->setContentsMargins(12, 8, 12, 8);
    rowLayout->setSpacing(12);

    // Icon
    auto *iconLbl = new QLabel(rowWidget);
    iconLbl->setObjectName(QStringLiteral("SettingsVaultRowIcon"));
    iconLbl->setFixedSize(18, 18);
    iconLbl->setPixmap(
        app::ThemeManager::instance().icon(v.iconName).pixmap(18, 18));
    rowLayout->addWidget(iconLbl);

    // Name & Description
    auto *infoLayout = new QVBoxLayout();
    infoLayout->setSpacing(2);

    auto *nameLbl = new QLabel(v.name, rowWidget);
    nameLbl->setObjectName(QStringLiteral("SettingsListItemTitle"));
    infoLayout->addWidget(nameLbl);

    if (!v.description.isEmpty()) {
      auto *descLbl = new QLabel(v.description, rowWidget);
      descLbl->setObjectName(QStringLiteral("SettingsListItemDesc"));
      infoLayout->addWidget(descLbl);
    }
    rowLayout->addLayout(infoLayout, 1);

    // Default badge
    if (v.isDefault) {
      auto *badge = new QLabel(tr("Default"), rowWidget);
      badge->setObjectName(QStringLiteral("SettingsDefaultBadge"));
      badge->setAttribute(Qt::WA_StyledBackground, true);
      badge->setAlignment(Qt::AlignCenter);
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

  // Nếu chỉ còn đúng 1 vault duy nhất (chính là vault mặc định), cho phép chọn
  // để xóa và đặt lại ứng dụng về Onboarding
  if (vaults.size() == 1 && m_deleteCombo) {
    m_deleteCombo->addItem(vaults.first().name, vaults.first().id);
  }

  // Cập nhật trạng thái combobox xóa và nút xóa
  if (m_deleteCombo) {
    if (m_deleteCombo->count() == 0) {
      m_deleteCombo->addItem(tr("No deletable vaults"), QString());
      m_deleteCombo->setEnabled(false);
    } else {
      m_deleteCombo->setEnabled(true);
    }
  }

  if (m_deleteBtn) {
    const bool canDelete = (m_deleteCombo && m_deleteCombo->isEnabled() &&
                            !m_deleteCombo->currentData().toString().isEmpty());
    m_deleteBtn->setEnabled(canDelete);
    if (vaults.size() <= 1) {
      m_deleteBtn->setToolTip(
          tr("Delete this vault and reset application to setup wizard"));
    } else {
      m_deleteBtn->setToolTip(tr("Permanently delete selected vault"));
    }
  }
}

void TabVaults::updateThemeIcons() {
  if (!m_vaultsContainerLayout) {
    return;
  }
  const auto vaults = app::VaultManager::instance().vaults();
  for (int i = 0; i < m_vaultsContainerLayout->count() && i < vaults.size();
       ++i) {
    if (auto *item = m_vaultsContainerLayout->itemAt(i)) {
      if (auto *rowWidget = item->widget()) {
        if (auto *iconLbl = rowWidget->findChild<QLabel *>(
                QStringLiteral("SettingsVaultRowIcon"))) {
          iconLbl->setPixmap(app::ThemeManager::instance()
                                 .icon(vaults[i].iconName)
                                 .pixmap(18, 18));
        }
      }
    }
  }
}

void TabVaults::onNewVaultClicked() {
  NewVaultDialog dlg(this);
  if (components::DialogDragHelper::execDialog(dlg) == QDialog::Accepted) {
    refreshVaultList();
  }
}

void TabVaults::onEditVaultClicked(const models::UiVault &vault) {
  NewVaultDialog dlg(this);
  dlg.setEditVault(vault);
  if (components::DialogDragHelper::execDialog(dlg) == QDialog::Accepted) {
    refreshVaultList();
  }
}

void TabVaults::onDeleteVaultClicked() {
  if (!m_deleteCombo || m_deleteCombo->count() == 0 ||
      m_deleteCombo->currentData().toString().isEmpty()) {
    return;
  }
  const QString vId = m_deleteCombo->currentData().toString();
  const QString vName = m_deleteCombo->currentText();
  const bool isOnlyVault = (app::VaultManager::instance().vaults().size() <= 1);

  QDialog dlg(this, Qt::FramelessWindowHint | Qt::Dialog);
  dlg.setAttribute(Qt::WA_TranslucentBackground, false);
  dlg.setModal(false);
  dlg.setWindowModality(Qt::NonModal);
  dlg.setMinimumSize(420, 260);
  dlg.resize(480, 290);

  auto *rootLayout = new QVBoxLayout(&dlg);
  rootLayout->setContentsMargins(0, 0, 0, 0);

  auto *card = new QWidget(&dlg);
  card->setObjectName(QStringLiteral("NewVaultCard"));
  card->setAttribute(Qt::WA_StyledBackground, true);

  new components::WindowResizeHandler(&dlg);

  auto *cardLayout = new QVBoxLayout(card);
  cardLayout->setContentsMargins(0, 0, 0, 16);
  cardLayout->setSpacing(0);

  auto *header = new QWidget(card);
  header->setObjectName(QStringLiteral("NewVaultHeader"));
  header->setAttribute(Qt::WA_StyledBackground, true);
  header->setFixedHeight(38);
  auto *headerLayout = new QHBoxLayout(header);
  headerLayout->setContentsMargins(16, 0, 0, 0);
  headerLayout->setSpacing(10);

  auto *headerIcon = new QLabel(header);
  headerIcon->setFixedSize(18, 18);
  headerIcon->setPixmap(app::ThemeManager::instance()
                            .icon(QStringLiteral("trash"))
                            .pixmap(18, 18));
  headerLayout->addWidget(headerIcon);

  auto *title = new QLabel(isOnlyVault ? tr("Delete Vault & Reset App?")
                                       : tr("Permanently Delete Vault?"),
                           header);
  title->setObjectName(QStringLiteral("NewVaultTitle"));
  headerLayout->addWidget(title, 1);

  auto *closeBtn = new QPushButton(header);
  closeBtn->setObjectName(QStringLiteral("WinCloseBtn"));
  closeBtn->setFixedSize(46, 38);
  closeBtn->setIconSize(QSize(12, 12));
  closeBtn->setFlat(true);
  closeBtn->setFocusPolicy(Qt::NoFocus);
  closeBtn->setCursor(Qt::PointingHandCursor);
  closeBtn->setIcon(
      app::ThemeManager::instance().icon(QStringLiteral("win_close")));
  connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
  headerLayout->addWidget(closeBtn);

  cardLayout->addWidget(header);

  auto *body = new QWidget(card);
  auto *layout = new QVBoxLayout(body);
  layout->setContentsMargins(20, 16, 20, 0);
  layout->setSpacing(12);

  auto *warn = new QLabel(
      isOnlyVault
          ? tr("This is your only remaining vault. Deleting \"%1\" will "
               "permanently erase "
               "ALL secrets, reset your Master Password, and return Secure "
               "Vault Manager to the "
               "initial Setup Wizard.\n\nType the vault name below to confirm:")
                .arg(vName)
          : tr("This action will erase ALL secrets inside \"%1\". Type the "
               "vault name below to confirm:")
                .arg(vName),
      body);
  warn->setObjectName(QStringLiteral("DialogDescLabel"));
  warn->setWordWrap(true);
  layout->addWidget(warn);

  auto *confirmEdit = new QLineEdit(body);
  confirmEdit->setPlaceholderText(vName);
  confirmEdit->setFixedHeight(34);
  layout->addWidget(confirmEdit);

  layout->addStretch(1);

  auto *btnLayout = new QHBoxLayout();
  auto *cancelBtn = new QPushButton(tr("Cancel"), body);
  cancelBtn->setFixedHeight(32);
  cancelBtn->setCursor(Qt::PointingHandCursor);
  connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
  btnLayout->addWidget(cancelBtn);
  btnLayout->addStretch(1);

  auto *confirmBtn =
      new QPushButton(isOnlyVault ? tr("Delete Vault & Reset to Setup")
                                  : tr("I understand, delete this vault"),
                      body);
  confirmBtn->setObjectName(QStringLiteral("DangerButton"));
  confirmBtn->setFixedHeight(32);
  confirmBtn->setCursor(Qt::PointingHandCursor);
  confirmBtn->setEnabled(false);
  connect(confirmEdit, &QLineEdit::textChanged, &dlg,
          [confirmBtn, vName](const QString &t) {
            confirmBtn->setEnabled(t == vName);
          });
  connect(confirmBtn, &QPushButton::clicked, &dlg, [&, isOnlyVault, vId]() {
    if (isOnlyVault) {
      dlg.accept();
      app::VaultManager::instance().resetAllData();
    } else {
      app::VaultManager::instance().deleteVault(vId);
      dlg.accept();
      refreshVaultList();
    }
  });
  btnLayout->addWidget(confirmBtn);
  layout->addLayout(btnLayout);

  cardLayout->addWidget(body, 1);
  components::DialogDragHelper::makeDraggable(header, &dlg);
  rootLayout->addWidget(card);

  components::DialogDragHelper::execDialog(dlg);
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
