/**
 * @file ui/main_window.h
 * @brief Cửa sổ chính MainWindow tích hợp UnifiedHeaderBar và bố cục 3 cột
 * thích ứng.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#pragma once

#include "models/vault_item.h"
#include "views/about/about_dialog.h"
#include "views/command_palette/command_palette_dialog.h"
#include "views/detail/detail_inspector_view.h"
#include "views/item_list/item_list_view.h"
#include "views/lock/lock_screen_view.h"
#include "views/onboarding/onboarding_wizard_view.h"
#include "views/sidebar/sidebar_view.h"

#include <QMainWindow>
#include <QShortcut>
#include <QSplitter>
#include <QStackedWidget>

namespace svm::app {
class TrayManager;
} // namespace svm::app

namespace svm::components {
class AppMenuBar;
class UnifiedHeaderBar;
class WindowResizeHandler;
} // namespace svm::components

namespace svm {

class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow() override = default;

  void setTrayManager(app::TrayManager *trayManager);
  [[nodiscard]] views::SidebarView *sidebarView() const {
    return m_sidebarView;
  }

  void retranslateUi();

protected:
  void resizeEvent(QResizeEvent *event) override;
  void changeEvent(QEvent *event) override;
  void closeEvent(QCloseEvent *event) override;

private slots:
  void toggleSidebar();
  void openCommandPalette();
  void onItemSelected(const QString &itemId);
  void onItemSaved(const models::UiVaultItem &item);
  void onQuickCopyRequested(const QString &itemId, const QString &fieldName);
  void onAddNewItemRequested(models::ItemCategory category);
  void onDetailCancelRequested();

private:
  void setupUI();
  void setupShortcuts();
  void populateMockData();
  void updateSidebarCounts();
  void updateNavigationState();

  app::TrayManager *m_trayManager{nullptr};
  components::AppMenuBar *m_menuBar{nullptr};
  components::UnifiedHeaderBar *m_headerBar{nullptr};
  components::WindowResizeHandler *m_resizeHandler{nullptr};

  // Stack điều hướng chính (Onboarding / Lock Screen / 3 Cột)
  QStackedWidget *m_mainStack{nullptr};
  views::OnboardingWizardView *m_onboardingView{nullptr};
  views::LockScreenView *m_lockScreenView{nullptr};

  // 3 Cột chính
  QSplitter *m_mainSplitter{nullptr};
  views::SidebarView *m_sidebarView{nullptr};
  views::ItemListView *m_itemListView{nullptr};
  views::DetailInspectorView *m_detailView{nullptr};

  // Trạng thái điều hướng
  QString m_previousSelectedItemId;

  // Command Palette
  views::CommandPaletteDialog *m_commandPalette{nullptr};

  // About Dialog
  views::AboutDialog *m_aboutDialog{nullptr};

  // Phím tắt toàn cục
  QShortcut *m_toggleSidebarShortcut{nullptr};
  QShortcut *m_toggleThemeShortcut{nullptr};
  QShortcut *m_commandPaletteShortcut{nullptr};
  QShortcut *m_searchShortcut{nullptr};
  QShortcut *m_newItemShortcut{nullptr};
  QShortcut *m_saveShortcut{nullptr};
  QShortcut *m_cancelShortcut{nullptr};
  QShortcut *m_editShortcut{nullptr};
};

} // namespace svm
