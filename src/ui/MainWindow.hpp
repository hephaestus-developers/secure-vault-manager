/**
 * @file ui/main_window.h
 * @brief Cửa sổ chính MainWindow tích hợp UnifiedHeaderBar và bố cục 3 cột
 * thích ứng.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#pragma once

#include "ui/models/VaultItem.hpp"
#include "ui/views/about/AboutDialog.hpp"
#include "ui/views/about/KeyboardShortcutsDialog.hpp"
#include "ui/views/detail/DetailInspectorView.hpp"
#include "ui/views/item_list/ItemListView.hpp"
#include "ui/views/lock/LockScreenView.hpp"
#include "ui/views/onboarding/OnboardingWizardView.hpp"
#include "ui/views/sidebar/SidebarView.hpp"
#include "ui/window/FramelessWindow.hpp"

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
} // namespace svm::components

namespace svm::views {
class AboutDialog;
class SettingsDialog;
} // namespace svm::views

namespace svm {

class MainWindow : public FramelessWindow {
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
#if defined(Q_OS_WIN)
  bool nativeEvent(const QByteArray &eventType, void *message,
                   qintptr *result) override;
#endif

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
#if defined(Q_OS_MAC)
  QTimer *m_macSleepTimer{nullptr};
#endif
#if defined(Q_OS_WIN)
  bool m_wtsRegistered{false};
#endif

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
