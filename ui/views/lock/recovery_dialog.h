/**
 * @file ui/views/lock/recovery_dialog.h
 * @brief Hộp thoại khôi phục khẩn cấp bằng Recovery Key.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#pragma once

#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

namespace svm::views {

class RecoveryDialog : public QDialog {
  Q_OBJECT

public:
  explicit RecoveryDialog(QWidget *parent = nullptr);
  ~RecoveryDialog() override = default;

  void retranslateUi();

protected:
  void changeEvent(QEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;

private:
  void setupUI();
  void onRestoreClicked();

  QWidget *m_cardWidget{nullptr};
  QWidget *m_headerWidget{nullptr};
  QLabel *m_headerIcon{nullptr};
  QLabel *m_headerTitle{nullptr};
  QPushButton *m_closeBtn{nullptr};
  QLabel *m_descLabel{nullptr};

  QLineEdit *m_recKeyEdit{nullptr};
  QLineEdit *m_newPwdEdit{nullptr};
  QLineEdit *m_confirmPwdEdit{nullptr};
  QLabel *m_errorLabel{nullptr};

  QPushButton *m_cancelBtn{nullptr};
  QPushButton *m_restoreBtn{nullptr};
};

} // namespace svm::views
