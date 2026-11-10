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
  void keyPressEvent(QKeyEvent *event) override;

private:
  void setupUI();
  QWidget *createIconPicker();
  void onCreateClicked();

  QWidget *m_cardWidget{nullptr};
  QWidget *m_headerWidget{nullptr};
  QLabel *m_headerIcon{nullptr};
  QLabel *m_headerTitle{nullptr};
  QPushButton *m_closeBtn{nullptr};
  QLabel *m_descLabel{nullptr};

  QLineEdit *m_nameEdit{nullptr};
  QLineEdit *m_descEdit{nullptr};
  QLabel *m_errorLabel{nullptr};

  QString m_selectedIcon{QStringLiteral("vault_personal")};

  QList<QPushButton *> m_iconButtons;

  QPushButton *m_cancelBtn{nullptr};
  QPushButton *m_createBtn{nullptr};

  bool m_isEditMode{false};
  models::UiVault m_createdVault;
};

} // namespace svm::views
