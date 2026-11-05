/**
 * @file ui/components/menubar/app_menu_bar.h
 * @brief Thanh menu chính nhúng trực tiếp vào UnifiedHeaderBar.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#pragma once

#include <QAction>
#include <QMenu>
#include <QMenuBar>
#include <QWidget>

namespace svm::components {

class AppMenuBar : public QMenuBar {
  Q_OBJECT

public:
  explicit AppMenuBar(QWidget *parent = nullptr);
  ~AppMenuBar() override = default;

signals:
  void lockVaultRequested();
  void exitRequested();
  void toggleThemeRequested();
  void aboutRequested();

private:
  void setupMenus();

  QMenu *m_vaultMenu{nullptr};
  QAction *m_lockAct{nullptr};
  QAction *m_exitAct{nullptr};

  QMenu *m_viewMenu{nullptr};
  QAction *m_themeAct{nullptr};

  QMenu *m_helpMenu{nullptr};
  QAction *m_aboutAct{nullptr};
};

} // namespace svm::components
