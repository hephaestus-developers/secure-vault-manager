/**
 * @file ui/components/menubar/app_menu_bar.h
 * @brief Thanh menu chính nhúng trực tiếp vào UnifiedHeaderBar.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#pragma once

#include <QAction>
#include <QActionGroup>
#include <QMenu>
#include <QMenuBar>
#include <QWidget>

namespace svm::components {

class AppMenuBar : public QMenuBar {
  Q_OBJECT

public:
  explicit AppMenuBar(QWidget *parent = nullptr);
  ~AppMenuBar() override = default;

  void retranslateUi();

signals:
  void lockVaultRequested();
  void exitRequested();
  void toggleThemeRequested();
  void aboutRequested();

protected:
  void changeEvent(QEvent *event) override;

private:
  void setupMenus();
  void updateLanguageActions();

  QMenu *m_vaultMenu{nullptr};
  QAction *m_lockAct{nullptr};
  QAction *m_exitAct{nullptr};

  QMenu *m_langMenu{nullptr};
  QActionGroup *m_langGroup{nullptr};
  QAction *m_langEnAct{nullptr};
  QAction *m_langViAct{nullptr};

  QMenu *m_helpMenu{nullptr};
  QAction *m_aboutAct{nullptr};
};

} // namespace svm::components
