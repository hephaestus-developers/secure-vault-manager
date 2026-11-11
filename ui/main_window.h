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
#include "views/about/keyboard_shortcuts_dialog.h"
#include "views/detail/detail_inspector_view.h"
#include "views/item_list/item_list_view.h"
#include "views/lock/lock_screen_view.h"
#include "views/onboarding/onboarding_wizard_view.h"
#include "views/sidebar/sidebar_view.h"

#include <QMainWindow>
#include <QProcess>
#include <QShortcut>
#include <QSplitter>
#include <QStackedWidget>

#include <optional>

namespace svm::app {
class TrayManager;
} // namespace svm::app

namespace svm::components {
class UnifiedHeaderBar;
class WindowResizeHandler;
} // namespace svm::components

namespace svm::views {
class AboutDialog;
class SettingsDialog;
} // namespace svm::views

namespace svm {

class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow() override;

  void stopSleepMonitor();

  void setTrayManager(app::TrayManager *trayManager);
  [[nodiscard]] views::SidebarView *sidebarView() const {
    return m_sidebarView;
  }

  void retranslateUi();
  void openSettingsDialog(int tabIndex = 0);
  void openAboutDialog();
  void openKeyboardShortcutsDialog();

protected:
  void resizeEvent(QResizeEvent *event) override;
  void changeEvent(QEvent *event) override;
  void closeEvent(QCloseEvent *event) override;
  bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
  void toggleSidebar();
  void onItemSelected(const QString &itemId);
  void onItemSaved(const models::UiVaultItem &item);
  void onItemEditRequested(const QString &itemId);
  void onItemFavoriteToggleRequested(const QString &itemId);
  void onItemMoveToVaultRequested(const QString &itemId,
                                  const QString &targetVaultId);
  void onItemTrashRequested(const QString &itemId);
  void onItemRestoreRequested(const QString &itemId);
  void onItemPurgeRequested(const QString &itemId);
  void onQuickCopyRequested(const QString &itemId, const QString &fieldName);
  void onAddNewItemRequested(models::ItemCategory category);
  void onDetailCancelRequested();

private:
  void setupUI();
  void setupShortcuts();
  void setupHeaderMenuBar();
  void populateVaultMenu(QMenu &menu);
  void populateItemMenu(QMenu &menu);
  void populateViewMenu(QMenu &menu);
  void populateHelpMenu(QMenu &menu);
  void lockVaultSession();
  void quitApplication();
  void onEmptyTrashRequested();
  void showNonModalDialog(QDialog *dialog);
  void updateHeaderTitle();
  void loadItemsFromCore();
  void updateSidebarCounts();
  void updateNavigationState();
  [[nodiscard]] std::optional<models::UiVaultItem>
  resolveFullItem(const QString &itemId) const;
  void persistItemUpdate(const models::UiVaultItem &item,
                         bool focusIfVisible = false);
  void syncSelectionAfterMutation(const QString &itemId,
                                  const models::UiVaultItem *updated,
                                  bool focusOrWasSelected);

  app::TrayManager *m_trayManager{nullptr};
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

  // About & Keyboard Shortcuts Dialogs
  views::AboutDialog *m_aboutDialog{nullptr};
  views::KeyboardShortcutsDialog *m_shortcutsDialog{nullptr};

  // Settings Dialog
  views::SettingsDialog *m_settingsDialog{nullptr};
  void updateInactivityTimer();
  void setupSleepMonitor();

  // Modal Dialog Guardian
  QDialog *findActiveModalDialog() const;

  // Inactivity Auto-Lock Timer & Sleep Monitor
  QTimer m_inactivityTimer;
  QProcess *m_sleepMonitorProcess{nullptr};

  // Phím tắt toàn cục
  QShortcut *m_toggleSidebarShortcut{nullptr};
  QShortcut *m_toggleThemeShortcut{nullptr};
  QShortcut *m_searchShortcut{nullptr};
  QShortcut *m_newItemShortcut{nullptr};
  QShortcut *m_saveShortcut{nullptr};
  QShortcut *m_cancelShortcut{nullptr};
  QShortcut *m_editShortcut{nullptr};
};

} // namespace svm
