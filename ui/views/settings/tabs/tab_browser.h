/**
 * @file ui/views/settings/tabs/tab_browser.h
 * @brief Phân hệ Tích hợp Trình duyệt Web & Cầu nối Browser Extension.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#pragma once

#include "views/settings/setting_row_widget.h"

#include <QCheckBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

namespace svm::views {

class TabBrowser : public QWidget {
  Q_OBJECT

public:
  explicit TabBrowser(QWidget *parent = nullptr);
  ~TabBrowser() override = default;

  void retranslateUi();

private:
  void setupUI();

  SettingSectionCard *m_bridgeCard{nullptr};
  SettingRowWidget *m_enableRow{nullptr};
  QCheckBox *m_enableCheck{nullptr};
  SettingRowWidget *m_manifestRow{nullptr};
  QPushButton *m_manifestBtn{nullptr};

  SettingSectionCard *m_pairedCard{nullptr};
  QVBoxLayout *m_pairedListLayout{nullptr};
};

} // namespace svm::views
