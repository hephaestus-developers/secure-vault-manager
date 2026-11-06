/**
 * @file ui/views/vaults/new_vault_dialog.h
 * @brief Hộp thoại tạo kho lưu trữ mới NewVaultDialog trực quan cao.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#pragma once

#include "models/vault_types.h"

#include <QButtonGroup>
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

namespace svm::views {

class NewVaultDialog : public QDialog {
  Q_OBJECT

public:
  explicit NewVaultDialog(QWidget *parent = nullptr);
  ~NewVaultDialog() override = default;

  void setEditVault(const models::UiVault &vault);
  [[nodiscard]] bool isEditMode() const { return m_isEditMode; }
  [[nodiscard]] models::UiVault createdVault() const { return m_createdVault; }

  void retranslateUi();

protected:
  void changeEvent(QEvent *event) override;

private:
  void setupUI();
  QWidget *createIconPicker();
  QWidget *createColorPicker();
  void onCreateClicked();

  QWidget *m_cardWidget{nullptr};
  QLabel *m_titleLabel{nullptr};
  QLabel *m_descLabel{nullptr};

  QLineEdit *m_nameEdit{nullptr};
  QLineEdit *m_descEdit{nullptr};
  QLabel *m_errorLabel{nullptr};

  QString m_selectedIcon{QStringLiteral("folder_open")};
  QString m_selectedColor{QStringLiteral("#3B82F6")};

  QList<QPushButton *> m_iconButtons;
  QList<QPushButton *> m_colorButtons;

  QPushButton *m_cancelBtn{nullptr};
  QPushButton *m_createBtn{nullptr};

  bool m_isEditMode{false};
  models::UiVault m_createdVault;
};

} // namespace svm::views
