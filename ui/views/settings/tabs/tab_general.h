/**
 * @file ui/views/settings/tabs/tab_general.h
 * @brief Phân hệ Cài đặt Chung (Theme, Ngôn ngữ, Khởi động cùng hệ thống).
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#pragma once

#include "views/settings/setting_row_widget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QWidget>

namespace svm::views {

class TabGeneral : public QWidget {
  Q_OBJECT

public:
  explicit TabGeneral(QWidget *parent = nullptr);
  ~TabGeneral() override = default;

  void retranslateUi();

private:
  void setupUI();

  SettingSectionCard *m_appearanceCard{nullptr};
  SettingRowWidget *m_themeRow{nullptr};
  QComboBox *m_themeCombo{nullptr};
  SettingRowWidget *m_densityRow{nullptr};
  QComboBox *m_densityCombo{nullptr};

  SettingSectionCard *m_languageCard{nullptr};
  SettingRowWidget *m_langRow{nullptr};
  QComboBox *m_langCombo{nullptr};

  SettingSectionCard *m_systemCard{nullptr};
  SettingRowWidget *m_startupRow{nullptr};
  QCheckBox *m_startupCheck{nullptr};
  SettingRowWidget *m_trayRow{nullptr};
  QCheckBox *m_trayCheck{nullptr};
  SettingRowWidget *m_minimizedRow{nullptr};
  QCheckBox *m_minimizedCheck{nullptr};
  SettingRowWidget *m_faviconRow{nullptr};
  QCheckBox *m_faviconCheck{nullptr};
};

} // namespace svm::views
