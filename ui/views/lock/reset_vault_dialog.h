/**
 * @file ui/views/lock/reset_vault_dialog.h
 * @brief Hộp thoại cảnh báo và xác nhận đặt lại kho an toàn khi mất cả Master
 * Password và Recovery Key.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#pragma once

#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>

namespace svm::views {

class ResetVaultDialog : public QDialog {
  Q_OBJECT

public:
  explicit ResetVaultDialog(QWidget *parent = nullptr);
  ~ResetVaultDialog() override = default;

  void retranslateUi();

protected:
  void changeEvent(QEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;

private:
  void setupUI();
  QWidget *createWarningPage();
  QWidget *createConfirmPage();

  void onProceedToConfirm();
  void onPerformReset();

  QWidget *m_cardWidget{nullptr};
  QWidget *m_headerWidget{nullptr};
  QLabel *m_headerIcon{nullptr};
  QLabel *m_headerTitle{nullptr};
  QPushButton *m_closeBtn{nullptr};

  QStackedWidget *m_pageStack{nullptr};

  // Page 1: Warning
  QLabel *m_warnTitleLabel{nullptr};
  QLabel *m_warnDescLabel{nullptr};
  QLabel *m_safetyTitle{nullptr};
  QLabel *m_backupNoticeLabel{nullptr};
  QPushButton *m_cancelBtn{nullptr};
  QPushButton *m_proceedBtn{nullptr};

  // Page 2: Typed Confirmation
  QLabel *m_confirmTitleLabel{nullptr};
  QLabel *m_confirmPromptLabel{nullptr};
  QLineEdit *m_confirmInput{nullptr};
  QLabel *m_confirmErrorLabel{nullptr};
  QPushButton *m_backBtn{nullptr};
  QPushButton *m_executeResetBtn{nullptr};
};

} // namespace svm::views
