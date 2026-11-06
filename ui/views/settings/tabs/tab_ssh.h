/**
 * @file ui/views/settings/tabs/tab_ssh.h
 * @brief Phân hệ Tích hợp SSH Agent daemon dành cho lập trình viên.
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

class TabSsh : public QWidget {
  Q_OBJECT

public:
  explicit TabSsh(QWidget *parent = nullptr);
  ~TabSsh() override = default;

  void retranslateUi();

private:
  void setupUI();

  SettingSectionCard *m_agentCard{nullptr};
  SettingRowWidget *m_enableRow{nullptr};
  QCheckBox *m_enableCheck{nullptr};
  SettingRowWidget *m_socketRow{nullptr};
  QPushButton *m_copySocketBtn{nullptr};

  SettingSectionCard *m_policyCard{nullptr};
  SettingRowWidget *m_policyRow{nullptr};
  QComboBox *m_policyCombo{nullptr};
};

} // namespace svm::views
