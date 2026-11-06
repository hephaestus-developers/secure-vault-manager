/**
 * @file ui/views/settings/tabs/tab_security.h
 * @brief Phân hệ Cài đặt Bảo mật & Quyền riêng tư (Auto-lock, Clipboard, Đổi
 * Master Password).
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#pragma once

#include "views/settings/setting_row_widget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QWidget>

namespace svm::views {

class TabSecurity : public QWidget {
  Q_OBJECT

public:
  explicit TabSecurity(QWidget *parent = nullptr);
  ~TabSecurity() override = default;

  void retranslateUi();

private:
  void setupUI();
  void openChangePasswordDialog();
  void openViewRecoveryKeyDialog();

  SettingSectionCard *m_autoLockCard{nullptr};
  SettingRowWidget *m_timeoutRow{nullptr};
  QComboBox *m_timeoutCombo{nullptr};
  SettingRowWidget *m_sleepRow{nullptr};
  QCheckBox *m_sleepCheck{nullptr};

  SettingSectionCard *m_clipboardCard{nullptr};
  SettingRowWidget *m_clipTimeoutRow{nullptr};
  QComboBox *m_clipTimeoutCombo{nullptr};
  SettingRowWidget *m_clipLockRow{nullptr};
  QCheckBox *m_clipLockCheck{nullptr};

  SettingSectionCard *m_biometricCard{nullptr};
  SettingRowWidget *m_biometricRow{nullptr};
  QCheckBox *m_biometricCheck{nullptr};

  SettingSectionCard *m_credentialsCard{nullptr};
  SettingRowWidget *m_pwdRow{nullptr};
  QPushButton *m_changePwdBtn{nullptr};
  SettingRowWidget *m_recKeyRow{nullptr};
  QPushButton *m_viewRecKeyBtn{nullptr};
};

} // namespace svm::views
