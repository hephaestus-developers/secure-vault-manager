/**
 * @file ui/views/settings/settings_dialog.h
 * @brief Hộp thoại Cài đặt & Tùy chọn (SettingsDialog) 2 cột chuẩn Enterprise.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#pragma once

#include "ui/views/settings/tabs/TabBrowser.hpp"
#include "ui/views/settings/tabs/TabData.hpp"
#include "ui/views/settings/tabs/TabGeneral.hpp"
#include "ui/views/settings/tabs/TabSecurity.hpp"
#include "ui/views/settings/tabs/TabSsh.hpp"
#include "ui/views/settings/tabs/TabVaults.hpp"

#include "ui/window/FramelessDialog.hpp"
#include <QLabel>
#include <QListWidget>
#include <QPoint>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>

namespace svm::views {

class SettingsDialog : public FramelessDialog {
  Q_OBJECT

public:
  explicit SettingsDialog(QWidget *parent = nullptr);
  ~SettingsDialog() override = default;

  void retranslateUi();
  void selectTab(int index);

protected:
  void changeEvent(QEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;

private:
  void setupUI();
  void initNavList();
  void updateNavListIcons();

  QWidget *m_cardWidget{nullptr};
  QWidget *m_headerWidget{nullptr};
  QLabel *m_headerTitle{nullptr};
  QPushButton *m_closeBtn{nullptr};

  QListWidget *m_navList{nullptr};
  QStackedWidget *m_stack{nullptr};
  QScrollArea *m_scrollArea{nullptr};

  TabGeneral *m_tabGeneral{nullptr};
  TabSecurity *m_tabSecurity{nullptr};
  TabVaults *m_tabVaults{nullptr};
  TabBrowser *m_tabBrowser{nullptr};
  TabSsh *m_tabSsh{nullptr};
  TabData *m_tabData{nullptr};
};

} // namespace svm::views
