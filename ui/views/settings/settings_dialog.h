/**
 * @file ui/views/settings/settings_dialog.h
 * @brief Hộp thoại Cài đặt & Tùy chọn (SettingsDialog) 2 cột chuẩn Enterprise.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#pragma once

#include "tabs/tab_browser.h"
#include "tabs/tab_data.h"
#include "tabs/tab_general.h"
#include "tabs/tab_security.h"
#include "tabs/tab_ssh.h"
#include "tabs/tab_vaults.h"

#include <QDialog>
#include <QLabel>
#include <QListWidget>
#include <QPoint>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>

namespace svm::views {

class SettingsDialog : public QDialog {
  Q_OBJECT

public:
  explicit SettingsDialog(QWidget *parent = nullptr);
  ~SettingsDialog() override = default;

  void retranslateUi();
  void selectTab(int index);

protected:
  void changeEvent(QEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;

private:
  void setupUI();
  void initNavList();
  void updateCategoryHeader(int index);

  QWidget *m_cardWidget{nullptr};
  QWidget *m_headerWidget{nullptr};
  QLabel *m_headerTitle{nullptr};
  QPushButton *m_closeBtn{nullptr};

  QListWidget *m_navList{nullptr};
  QLabel *m_rightCategoryTitle{nullptr};
  QStackedWidget *m_stack{nullptr};
  QScrollArea *m_scrollArea{nullptr};

  TabGeneral *m_tabGeneral{nullptr};
  TabSecurity *m_tabSecurity{nullptr};
  TabVaults *m_tabVaults{nullptr};
  TabBrowser *m_tabBrowser{nullptr};
  TabSsh *m_tabSsh{nullptr};
  TabData *m_tabData{nullptr};

  // Dragging state
  bool m_isDragging{false};
  QPoint m_dragPosition;
};

} // namespace svm::views
