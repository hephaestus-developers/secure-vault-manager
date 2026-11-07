/**
 * @file ui/views/settings/tabs/tab_vaults.h
 * @brief Phân hệ Quản lý Kho Dữ liệu (Vaults Management).
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#pragma once

#include "models/vault_types.h"
#include "views/settings/setting_row_widget.h"

#include <QComboBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

namespace svm::views {

class TabVaults : public QWidget {
  Q_OBJECT

public:
  explicit TabVaults(QWidget *parent = nullptr);
  ~TabVaults() override = default;

  void retranslateUi();
  void refreshVaultList();
  void updateThemeIcons();

private:
  void setupUI();
  void onNewVaultClicked();
  void onEditVaultClicked(const models::UiVault &vault);
  void onDeleteVaultClicked();

  SettingSectionCard *m_vaultsListCard{nullptr};
  QVBoxLayout *m_vaultsContainerLayout{nullptr};
  QPushButton *m_newVaultBtn{nullptr};

  SettingSectionCard *m_dangerCard{nullptr};
  QComboBox *m_deleteCombo{nullptr};
  QPushButton *m_deleteBtn{nullptr};
  SettingRowWidget *m_deleteRow{nullptr};
};

} // namespace svm::views
