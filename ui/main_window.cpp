/**
 * @file ui/main_window.cpp
 * @brief Hiện thực chi tiết MainWindow tích hợp UnifiedHeaderBar và 3 cột thích
 * ứng.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "main_window.h"
#include "app/clipboard_manager.h"
#include "app/dev_logger.h"
#include "app/language_manager.h"
#include "app/settings_manager.h"
#include "app/theme_manager.h"
#include "app/tray_manager.h"
#include "app/vault_manager.h"
#include "components/confirm_delete_dialog.h"
#include "components/dialog_drag_helper.h"
#include "components/notification_dialog.h"
#include "components/unified_header_bar.h"
#include "components/window_resize_handler.h"
#include "core/core_bridge.h"
#include "views/settings/settings_dialog.h"

#include <QApplication>
#include <QCloseEvent>
#include <QDateTime>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QIcon>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLayout>
#include <QMenu>
#include <QMouseEvent>
#include <QPropertyAnimation>
#include <QStandardPaths>
#include <QUrl>
#include <QVBoxLayout>
#include <QWindow>

namespace svm {

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
  // Cấu hình cờ cửa sổ Frameless sử dụng UnifiedHeaderBar làm Title Bar
  setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
  setAttribute(Qt::WA_TranslucentBackground, false);
  setMinimumSize(900, 560);
  resize(1120, 680);
  setWindowTitle(QStringLiteral("Secure Vault Manager"));

  setupUI();
  setupShortcuts();
  m_itemListView->setItems({});
  m_detailView->clearItem();

  // Khởi tạo bộ xử lý co giãn kích thước cửa sổ không viền 8 hướng
  m_resizeHandler = new components::WindowResizeHandler(this);

  // Cài đặt Global Event Filter để bảo vệ tính Modal của các Dialog
  qApp->installEventFilter(this);
}

void MainWindow::setupUI() {
  auto *centralWidget = new QWidget(this);
  centralWidget->setObjectName(QStringLiteral("CentralWidget"));
  centralWidget->setAttribute(Qt::WA_StyledBackground, true);
  auto *layout = new QVBoxLayout(centralWidget);
  const int m = isMaximized() ? 0 : 1;
  layout->setContentsMargins(m, m, m, m);
  layout->setSpacing(0);

  // 1. Khởi tạo UnifiedHeaderBar
  m_headerBar = new components::UnifiedHeaderBar(this);
  m_headerBar->setTitleText(windowTitle());
  layout->addWidget(m_headerBar);

  // 2. Vùng chứa nội dung chính dạng 3 Cột (QSplitter)
  m_mainSplitter = new QSplitter(Qt::Horizontal, centralWidget);
  m_mainSplitter->setObjectName(QStringLiteral("MainThreeColumnSplitter"));

  m_sidebarView = new views::SidebarView(m_mainSplitter);
  m_itemListView = new views::ItemListView(m_mainSplitter);
  m_detailView = new views::DetailInspectorView(m_mainSplitter);

  m_mainSplitter->addWidget(m_sidebarView);
  m_mainSplitter->addWidget(m_itemListView);
  m_mainSplitter->addWidget(m_detailView);

  // Thiết lập tỷ lệ kích thước khởi tạo: Sidebar (240px) | ItemList (300px) |
  // Detail (phần còn lại)
  m_mainSplitter->setSizes({240, 300, 580});
  m_mainSplitter->setChildrenCollapsible(false);
  m_mainSplitter->setCollapsible(0, false);
  m_mainSplitter->setCollapsible(1, false);
  m_mainSplitter->setCollapsible(2, false);
  m_mainSplitter->setStretchFactor(0, 0);
  m_mainSplitter->setStretchFactor(1, 0);
  m_mainSplitter->setStretchFactor(2, 1);

  // Stack điều hướng chính (Onboarding / Lock Screen / 3 Cột)
  m_mainStack = new QStackedWidget(centralWidget);
  m_mainStack->setObjectName(QStringLiteral("MainNavigationStack"));

  m_onboardingView = new views::OnboardingWizardView(m_mainStack);
  m_lockScreenView = new views::LockScreenView(m_mainStack);

  m_mainStack->addWidget(m_onboardingView); // Index 0
  m_mainStack->addWidget(m_lockScreenView); // Index 1
  m_mainStack->addWidget(m_mainSplitter);   // Index 2

  layout->addWidget(m_mainStack, 1);
  setCentralWidget(centralWidget);

  // Kết nối luồng vòng đời Vault
  connect(m_onboardingView, &views::OnboardingWizardView::onboardingFinished,
          this, &MainWindow::updateNavigationState);
  connect(m_lockScreenView, &views::LockScreenView::unlocked, this,
          &MainWindow::updateNavigationState);
  connect(&app::VaultManager::instance(), &app::VaultManager::lockStateChanged,
          this, &MainWindow::updateNavigationState);
  connect(&app::VaultManager::instance(), &app::VaultManager::configuredChanged,
          this, &MainWindow::updateNavigationState);
  connect(&app::VaultManager::instance(),
          &app::VaultManager::activeVaultChanged, this,
          [this]() { updateSidebarCounts(); });
  connect(&core::CoreBridge::instance(), &core::CoreBridge::itemsChangedEvent,
          this, [this]() {
            if (app::VaultManager::instance().isUnlocked()) {
              loadItemsFromCore();
              app::VaultManager::instance().refreshVaultsFromCore();
            }
          });
  connect(
      &core::CoreBridge::instance(), &core::CoreBridge::promptRequestedEvent,
      this, [this](const QJsonObject &prompt) {
        const QString reqId =
            prompt.value(QStringLiteral("request_id")).toString();
        if (reqId.isEmpty()) {
          return;
        }
        const QString keyName = prompt.value(QStringLiteral("key")).toString();
        const QString fp =
            prompt.value(QStringLiteral("fingerprint")).toString();
        const bool approved = components::NotificationDialog::question(
            this, tr("SSH Signing Request"),
            tr("An SSH client is requesting a cryptographic signature using "
               "your vault key:\n\nKey: %1\nFingerprint: %2\n\nAllow this "
               "request?")
                .arg(keyName, fp),
            tr("Approve"), tr("Deny"), false);
        core::CoreBridge::instance().respondPrompt(reqId, approved);
      });

  // 3. Kết nối tín hiệu Header Bar
  connect(m_headerBar, &components::UnifiedHeaderBar::minimizeRequested, this,
          &MainWindow::showMinimized);
  connect(m_headerBar, &components::UnifiedHeaderBar::maximizeRestoreRequested,
          this, [this]() {
            if (isMaximized()) {
              showNormal();
            } else {
              showMaximized();
            }
          });
  connect(m_headerBar, &components::UnifiedHeaderBar::closeRequested, this,
          &MainWindow::close);
  connect(m_headerBar, &components::UnifiedHeaderBar::themeToggleRequested,
          this, []() {
            auto &tm = app::ThemeManager::instance();
            const auto nextMode = (tm.currentMode() == app::ThemeMode::Dark)
                                      ? app::ThemeMode::Light
                                      : app::ThemeMode::Dark;
            tm.applyTheme(nextMode);
          });

  // 5. Kết nối tín hiệu giữa các Cột
  connect(m_sidebarView, &views::SidebarView::collapsedChanged, this,
          [this](bool collapsed) {
            const int totalWidth = width();
            if (collapsed) {
              m_mainSplitter->setSizes({60, 300, totalWidth - 360});
            } else {
              m_mainSplitter->setSizes({240, 300, totalWidth - 540});
            }
          });
  connect(m_sidebarView, &views::SidebarView::filterSelected, m_itemListView,
          &views::ItemListView::setFilter);
  connect(m_sidebarView, &views::SidebarView::lockVaultRequested, this,
          [this]() {
            app::VaultManager::instance().lock();
            if (m_trayManager) {
              m_trayManager->setVaultLocked(true);
            }
          });

  connect(m_itemListView, &views::ItemListView::itemSelected, this,
          &MainWindow::onItemSelected);
  connect(m_itemListView, &views::ItemListView::addNewItemRequested, this,
          &MainWindow::onAddNewItemRequested);
  connect(m_itemListView, &views::ItemListView::quickCopyRequested, this,
          &MainWindow::onQuickCopyRequested);
  connect(m_itemListView, &views::ItemListView::itemEditRequested, this,
          &MainWindow::onItemEditRequested);
  connect(m_itemListView, &views::ItemListView::itemFavoriteToggleRequested,
          this, &MainWindow::onItemFavoriteToggleRequested);
  connect(m_itemListView, &views::ItemListView::itemMoveToVaultRequested, this,
          &MainWindow::onItemMoveToVaultRequested);
  connect(m_itemListView, &views::ItemListView::itemTrashRequested, this,
          &MainWindow::onItemTrashRequested);
  connect(m_itemListView, &views::ItemListView::itemRestoreRequested, this,
          &MainWindow::onItemRestoreRequested);
  connect(m_itemListView, &views::ItemListView::itemPurgeRequested, this,
          &MainWindow::onItemPurgeRequested);

  connect(m_detailView, &views::DetailInspectorView::itemSaved, this,
          &MainWindow::onItemSaved);
  connect(m_detailView, &views::DetailInspectorView::itemTrashRequested, this,
          &MainWindow::onItemTrashRequested);
  connect(m_detailView, &views::DetailInspectorView::itemRestoreRequested, this,
          &MainWindow::onItemRestoreRequested);
  connect(m_detailView, &views::DetailInspectorView::itemPurgeRequested, this,
          &MainWindow::onItemPurgeRequested);
  connect(m_detailView, &views::DetailInspectorView::historyRestored, this,
          [this](const QString &itemId) {
            Q_UNUSED(itemId);
            loadItemsFromCore();
          });
  connect(m_detailView, &views::DetailInspectorView::cancelRequested, this,
          &MainWindow::onDetailCancelRequested);

  // 6. Kết nối HeaderBar, Sidebar & MenuBar Actions
  setupHeaderMenuBar();
  connect(m_headerBar, &components::UnifiedHeaderBar::settingsRequested, this,
          [this]() { openSettingsDialog(0); });
  connect(m_sidebarView, &views::SidebarView::settingsRequested, this,
          [this]() { openSettingsDialog(0); });

  // 7. Thiết lập Inactivity Timer tự động khóa Vault & giám sát Sleep
  connect(&m_inactivityTimer, &QTimer::timeout, this, [this]() {
    if (app::VaultManager::instance().isUnlocked()) {
      const quint64 mins = static_cast<quint64>(
          qMax(1, app::SettingsManager::instance().autoLockMinutes()));
      core::CoreBridge::instance().reportIdle(mins * 60000ULL);
      app::VaultManager::instance().lock();
      if (m_trayManager) {
        m_trayManager->setVaultLocked(true);
      }
    }
  });
  connect(&app::SettingsManager::instance(),
          &app::SettingsManager::autoLockTimeoutChanged, this,
          &MainWindow::updateInactivityTimer);
  updateInactivityTimer();
  setupSleepMonitor();

  // 8. Đồng bộ trạng thái điều hướng khởi đầu
  updateNavigationState();

  // 9. Khởi động Cầu nối Trình duyệt Web và SSH Agent nếu được kích hoạt
  if (app::SettingsManager::instance().browserBridgeEnabled()) {
    core::CoreBridge::instance().startBridge();
    core::CoreBridge::instance().installNativeHostManifests();
  }
  if (app::SettingsManager::instance().sshAgentEnabled()) {
    core::CoreBridge::instance().startSshAgent();
  }
  if (!app::SettingsManager::instance().hasFaviconFetchSetting() &&
      core::CoreBridge::instance().isInitialized()) {
    const QJsonObject coreSettings =
        core::CoreBridge::instance().getCoreSettings();
    if (coreSettings.value(QStringLiteral("favicon_fetch_enabled")).toBool()) {
      app::SettingsManager::instance().setFaviconFetchEnabled(true);
    }
  }
  emit app::SettingsManager::instance().settingsChanged();
}

void MainWindow::setupShortcuts() {
  m_toggleSidebarShortcut =
      new QShortcut(QKeySequence(QStringLiteral("Ctrl+B")), this);
  connect(m_toggleSidebarShortcut, &QShortcut::activated, this, [this]() {
    if (!app::VaultManager::instance().isLocked()) {
      toggleSidebar();
    }
  });

  m_toggleThemeShortcut =
      new QShortcut(QKeySequence(QStringLiteral("Ctrl+T")), this);
  connect(m_toggleThemeShortcut, &QShortcut::activated, this, []() {
    auto &tm = app::ThemeManager::instance();
    const auto nextMode = (tm.currentMode() == app::ThemeMode::Dark)
                              ? app::ThemeMode::Light
                              : app::ThemeMode::Dark;
    tm.applyTheme(nextMode);
  });

  m_searchShortcut =
      new QShortcut(QKeySequence(QStringLiteral("Ctrl+F")), this);
  connect(m_searchShortcut, &QShortcut::activated, this, [this]() {
    if (!app::VaultManager::instance().isLocked()) {
      m_itemListView->focusSearch();
    }
  });

  m_newItemShortcut =
      new QShortcut(QKeySequence(QStringLiteral("Ctrl+N")), this);
  connect(m_newItemShortcut, &QShortcut::activated, this, [this]() {
    if (!app::VaultManager::instance().isLocked()) {
      onAddNewItemRequested(m_itemListView->currentCategory());
    }
  });

  m_saveShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+S")), this);
  connect(m_saveShortcut, &QShortcut::activated, this, [this]() {
    if (!app::VaultManager::instance().isLocked() &&
        m_detailView->isEditMode()) {
      m_detailView->saveEdit();
    }
  });

  m_cancelShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
  connect(m_cancelShortcut, &QShortcut::activated, this, [this]() {
    if (!app::VaultManager::instance().isLocked() &&
        m_detailView->isEditMode()) {
      m_detailView->cancelEdit();
    }
  });

  m_editShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+E")), this);
  connect(m_editShortcut, &QShortcut::activated, this, [this]() {
    if (!app::VaultManager::instance().isLocked() &&
        !m_detailView->isEditMode() &&
        !m_itemListView->selectedItemId().isEmpty()) {
      m_detailView->setEditMode(true);
    }
  });

  auto *settingsShortcut =
      new QShortcut(QKeySequence(QStringLiteral("Ctrl+,")), this);
  connect(settingsShortcut, &QShortcut::activated, this,
          [this]() { openSettingsDialog(0); });

  auto *lockShortcut =
      new QShortcut(QKeySequence(QStringLiteral("Ctrl+L")), this);
  connect(lockShortcut, &QShortcut::activated, this,
          &MainWindow::lockVaultSession);

  auto *helpShortcut = new QShortcut(QKeySequence(Qt::Key_F1), this);
  connect(helpShortcut, &QShortcut::activated, this,
          &MainWindow::openKeyboardShortcutsDialog);

  auto *quitShortcut = new QShortcut(QKeySequence::Quit, this);
  connect(quitShortcut, &QShortcut::activated, this,
          &MainWindow::quitApplication);
}

void MainWindow::lockVaultSession() {
  if (!app::VaultManager::instance().isLocked()) {
    app::VaultManager::instance().lock();
    if (m_trayManager) {
      m_trayManager->setVaultLocked(true);
    }
  }
}

void MainWindow::quitApplication() {
  stopSleepMonitor();
  QApplication::quit();
}

void MainWindow::setupHeaderMenuBar() {
  if (!m_headerBar) {
    return;
  }
  if (QMenu *m = m_headerBar->vaultMenu()) {
    connect(m, &QMenu::aboutToShow, this,
            [this, m]() { populateVaultMenu(*m); });
  }
  if (QMenu *m = m_headerBar->itemMenu()) {
    connect(m, &QMenu::aboutToShow, this,
            [this, m]() { populateItemMenu(*m); });
  }
  if (QMenu *m = m_headerBar->viewMenu()) {
    connect(m, &QMenu::aboutToShow, this,
            [this, m]() { populateViewMenu(*m); });
  }
  if (QMenu *m = m_headerBar->helpMenu()) {
    connect(m, &QMenu::aboutToShow, this,
            [this, m]() { populateHelpMenu(*m); });
  }
}

namespace {
void setMenuShortcutHint(QAction *action, const QString &shortcut) {
  if (action && !shortcut.isEmpty()) {
    action->setShortcut(QKeySequence(shortcut));
    action->setShortcutContext(Qt::WidgetShortcut);
  }
}
} // namespace

void MainWindow::populateVaultMenu(QMenu &menu) {
  menu.clear();
  const auto &tm = app::ThemeManager::instance();
  const bool unlocked = app::VaultManager::instance().isUnlocked();
  if (!unlocked) {
    return;
  }

  auto *newVaultAct =
      menu.addAction(tm.icon(QStringLiteral("plus")), tr("New Vault..."));
  connect(newVaultAct, &QAction::triggered, m_sidebarView,
          &views::SidebarView::openNewVaultDialog);

  auto *switchMenu =
      menu.addMenu(tm.icon(QStringLiteral("vault_safe")), tr("Switch Vault"));
  const QString activeId = app::VaultManager::instance().activeVaultId();
  auto *allAct = switchMenu->addAction(tr("All Vaults"));
  allAct->setCheckable(true);
  allAct->setChecked(activeId == QStringLiteral("all"));
  connect(allAct, &QAction::triggered, this, []() {
    app::VaultManager::instance().setActiveVaultId(QStringLiteral("all"));
  });

  const auto vaults = app::VaultManager::instance().vaults();
  if (!vaults.isEmpty()) {
    switchMenu->addSeparator();
  }
  for (const auto &v : vaults) {
    auto *vaultAct = switchMenu->addAction(v.name);
    vaultAct->setCheckable(true);
    vaultAct->setChecked(activeId == v.id);
    connect(vaultAct, &QAction::triggered, this, [id = v.id]() {
      app::VaultManager::instance().setActiveVaultId(id);
    });
  }

  menu.addSeparator();

  auto *importAct = menu.addAction(tr("Import Data..."));
  connect(importAct, &QAction::triggered, this,
          [this]() { openSettingsDialog(5); });

  auto *exportAct = menu.addAction(tr("Export / Backup..."));
  connect(exportAct, &QAction::triggered, this,
          [this]() { openSettingsDialog(5); });

  auto *manageVaultsAct = menu.addAction(tr("Manage Vaults..."));
  connect(manageVaultsAct, &QAction::triggered, this,
          [this]() { openSettingsDialog(2); });

  menu.addSeparator();

  auto *lockAct =
      menu.addAction(tm.icon(QStringLiteral("lock")), tr("Lock Vault"));
  setMenuShortcutHint(lockAct, QStringLiteral("Ctrl+L"));
  connect(lockAct, &QAction::triggered, this, &MainWindow::lockVaultSession);

  auto *settingsAct = menu.addAction(
      tm.icon(QStringLiteral("category_settings")), tr("Settings..."));
  setMenuShortcutHint(settingsAct, QStringLiteral("Ctrl+,"));
  connect(settingsAct, &QAction::triggered, this,
          [this]() { openSettingsDialog(0); });

  menu.addSeparator();

  if (m_trayManager && m_trayManager->isSystemTrayAvailable()) {
    auto *trayAct = menu.addAction(tr("Close to Tray"));
    connect(trayAct, &QAction::triggered, this, &QWidget::hide);
  }

  auto *quitAct = menu.addAction(tr("Quit"));
  setMenuShortcutHint(quitAct, QStringLiteral("Ctrl+Q"));
  connect(quitAct, &QAction::triggered, this, &MainWindow::quitApplication);
}

void MainWindow::populateItemMenu(QMenu &menu) {
  menu.clear();
  const auto &tm = app::ThemeManager::instance();
  if (!app::VaultManager::instance().isUnlocked()) {
    return;
  }

  auto *newItemAct =
      menu.addAction(tm.icon(QStringLiteral("plus")), tr("New Item"));
  setMenuShortcutHint(newItemAct, QStringLiteral("Ctrl+N"));
  connect(newItemAct, &QAction::triggered, this, [this]() {
    onAddNewItemRequested(m_itemListView->currentCategory());
  });

  auto *newByTypeMenu = menu.addMenu(tr("New Item by Type"));
  const models::ItemCategory allCategories[] = {
      models::ItemCategory::Login,      models::ItemCategory::Passkey,
      models::ItemCategory::Card,       models::ItemCategory::Identity,
      models::ItemCategory::SecureNote, models::ItemCategory::SshKey,
      models::ItemCategory::Database,   models::ItemCategory::Server,
      models::ItemCategory::Wifi,
  };
  for (const auto cat : allCategories) {
    auto *catAct =
        newByTypeMenu->addAction(tm.icon(models::categoryIconName(cat)),
                                 models::categoryDisplayName(cat));
    connect(catAct, &QAction::triggered, this,
            [this, cat]() { onAddNewItemRequested(cat); });
  }

  menu.addSeparator();

  const QString selectedId = m_itemListView->selectedItemId();
  const auto resolved = resolveFullItem(selectedId);
  const bool hasActiveItem = resolved.has_value() && !resolved->isDeleted;
  const bool hasTrashItem = resolved.has_value() && resolved->isDeleted;
  const bool inEditMode = m_detailView->isEditMode();

  auto *editAct =
      menu.addAction(tm.icon(QStringLiteral("edit")), tr("Edit Item"));
  setMenuShortcutHint(editAct, QStringLiteral("Ctrl+E"));
  editAct->setEnabled(hasActiveItem && !inEditMode);
  connect(editAct, &QAction::triggered, this,
          [this, selectedId]() { onItemEditRequested(selectedId); });

  auto *saveAct = menu.addAction(tr("Save Changes"));
  setMenuShortcutHint(saveAct, QStringLiteral("Ctrl+S"));
  saveAct->setEnabled(inEditMode);
  connect(saveAct, &QAction::triggered, m_detailView,
          &views::DetailInspectorView::saveEdit);

  auto *cancelEditAct = menu.addAction(tr("Cancel Editing"));
  setMenuShortcutHint(cancelEditAct, QStringLiteral("Esc"));
  cancelEditAct->setEnabled(inEditMode);
  connect(cancelEditAct, &QAction::triggered, m_detailView,
          &views::DetailInspectorView::cancelEdit);

  menu.addSeparator();

  auto *copyUserAct =
      menu.addAction(tm.icon(QStringLiteral("copy")), tr("Copy Username"));
  copyUserAct->setEnabled(hasActiveItem && !resolved->username.isEmpty());
  connect(copyUserAct, &QAction::triggered, this, [this, selectedId]() {
    onQuickCopyRequested(selectedId, QStringLiteral("username"));
  });

  auto *copyPassAct =
      menu.addAction(tm.icon(QStringLiteral("copy")), tr("Copy Password"));
  copyPassAct->setEnabled(hasActiveItem && !resolved->password.isEmpty());
  connect(copyPassAct, &QAction::triggered, this, [this, selectedId]() {
    onQuickCopyRequested(selectedId, QStringLiteral("password"));
  });

  auto *copyTotpAct = menu.addAction(tm.icon(QStringLiteral("shield_check")),
                                     tr("Copy 2FA Code"));
  copyTotpAct->setEnabled(
      hasActiveItem && (resolved->hasTotp || !resolved->totpSecret.isEmpty()));
  connect(copyTotpAct, &QAction::triggered, this, [this, selectedId]() {
    onQuickCopyRequested(selectedId, QStringLiteral("totp"));
  });

  auto *openUrlAct = menu.addAction(tm.icon(QStringLiteral("external_link")),
                                    tr("Open Website"));
  const QString rawUrl =
      resolved.has_value() ? resolved->websiteUrl.trimmed() : QString();
  openUrlAct->setEnabled(hasActiveItem && !rawUrl.isEmpty());
  connect(openUrlAct, &QAction::triggered, this, [rawUrl]() {
    QUrl url(rawUrl);
    if (url.scheme().isEmpty()) {
      url.setScheme(QStringLiteral("https"));
    }
    QDesktopServices::openUrl(url);
  });

  menu.addSeparator();

  const bool isFav = hasActiveItem && resolved->isFavorite;
  auto *favAct = menu.addAction(
      tm.icon(isFav ? QStringLiteral("star_filled") : QStringLiteral("star")),
      isFav ? tr("Remove from Favorites") : tr("Add to Favorites"));
  favAct->setEnabled(hasActiveItem);
  connect(favAct, &QAction::triggered, this,
          [this, selectedId]() { onItemFavoriteToggleRequested(selectedId); });

  auto *moveMenu =
      menu.addMenu(tm.icon(QStringLiteral("vault_safe")), tr("Move to Vault"));
  moveMenu->setEnabled(hasActiveItem);
  if (hasActiveItem) {
    const auto vaults = app::VaultManager::instance().vaults();
    int movableCount = 0;
    for (const auto &v : vaults) {
      const QString iconKey =
          v.iconName.isEmpty() ? QStringLiteral("vault_personal") : v.iconName;
      if (v.id == resolved->vaultId) {
        auto *curAct = moveMenu->addAction(tm.icon(iconKey),
                                           tr("%1 (Current)").arg(v.name));
        curAct->setEnabled(false);
      } else {
        auto *dstAct = moveMenu->addAction(tm.icon(iconKey), v.name);
        connect(dstAct, &QAction::triggered, this,
                [this, selectedId, targetId = v.id]() {
                  onItemMoveToVaultRequested(selectedId, targetId);
                });
        ++movableCount;
      }
    }
    if (movableCount == 0) {
      if (!vaults.isEmpty()) {
        moveMenu->addSeparator();
      }
      auto *emptyAct = moveMenu->addAction(tr("No other vaults available"));
      emptyAct->setEnabled(false);
    }
  }

  auto *historyAct = menu.addAction(tm.icon(QStringLiteral("sort_time")),
                                    tr("Revision History..."));
  historyAct->setEnabled(hasActiveItem);
  connect(historyAct, &QAction::triggered, m_detailView,
          &views::DetailInspectorView::showRevisionHistory);

  menu.addSeparator();

  if (hasTrashItem) {
    auto *restoreAct = menu.addAction(tr("Restore"));
    connect(restoreAct, &QAction::triggered, this,
            [this, selectedId]() { onItemRestoreRequested(selectedId); });

    auto *purgeAct = menu.addAction(tm.icon(QStringLiteral("trash")),
                                    tr("Delete Permanently"));
    connect(purgeAct, &QAction::triggered, this,
            [this, selectedId]() { onItemPurgeRequested(selectedId); });
  } else {
    auto *trashAct =
        menu.addAction(tm.icon(QStringLiteral("trash")), tr("Move to Trash"));
    trashAct->setEnabled(hasActiveItem);
    connect(trashAct, &QAction::triggered, this,
            [this, selectedId]() { onItemTrashRequested(selectedId); });
  }

  bool hasAnyTrashed = false;
  for (const auto &it : m_itemListView->sourceModel()->items()) {
    if (it.isDeleted) {
      hasAnyTrashed = true;
      break;
    }
  }
  auto *emptyTrashAct = menu.addAction(tr("Empty Trash..."));
  emptyTrashAct->setEnabled(hasAnyTrashed);
  connect(emptyTrashAct, &QAction::triggered, this,
          &MainWindow::onEmptyTrashRequested);
}

void MainWindow::populateViewMenu(QMenu &menu) {
  menu.clear();
  const auto &tm = app::ThemeManager::instance();
  const bool unlocked = app::VaultManager::instance().isUnlocked();

  if (unlocked) {
    auto *searchAct =
        menu.addAction(tm.icon(QStringLiteral("search")), tr("Search Vault"));
    setMenuShortcutHint(searchAct, QStringLiteral("Ctrl+F"));
    connect(searchAct, &QAction::triggered, m_itemListView,
            &views::ItemListView::focusSearch);

    auto *sidebarAct = menu.addAction(tm.icon(QStringLiteral("sidebar_toggle")),
                                      tr("Toggle Sidebar"));
    setMenuShortcutHint(sidebarAct, QStringLiteral("Ctrl+B"));
    connect(sidebarAct, &QAction::triggered, this, &MainWindow::toggleSidebar);

    menu.addSeparator();

    auto *allAct = menu.addAction(tm.icon(QStringLiteral("category_all")),
                                  tr("All Items"));
    connect(allAct, &QAction::triggered, this, [this]() {
      m_sidebarView->selectFilter(views::SidebarFilterType::AllItems);
    });

    auto *favAct =
        menu.addAction(tm.icon(QStringLiteral("star")), tr("Favorites"));
    connect(favAct, &QAction::triggered, this, [this]() {
      m_sidebarView->selectFilter(views::SidebarFilterType::Favorites);
    });

    auto *trashAct =
        menu.addAction(tm.icon(QStringLiteral("trash")), tr("Trash"));
    connect(trashAct, &QAction::triggered, this, [this]() {
      m_sidebarView->selectFilter(views::SidebarFilterType::Trash);
    });

    menu.addSeparator();

    auto *densityMenu = menu.addMenu(tr("List Density"));
    const auto curDensity = app::SettingsManager::instance().listDensity();
    auto *compactAct = densityMenu->addAction(tr("Compact"));
    compactAct->setCheckable(true);
    compactAct->setChecked(curDensity == app::ListDensity::Compact);
    connect(compactAct, &QAction::triggered, this, []() {
      app::SettingsManager::instance().setListDensity(
          app::ListDensity::Compact);
    });

    auto *comfortableAct = densityMenu->addAction(tr("Comfortable"));
    comfortableAct->setCheckable(true);
    comfortableAct->setChecked(curDensity == app::ListDensity::Comfortable);
    connect(comfortableAct, &QAction::triggered, this, []() {
      app::SettingsManager::instance().setListDensity(
          app::ListDensity::Comfortable);
    });

    menu.addSeparator();
  }

  auto *langMenu = menu.addMenu(tr("Language"));
  const auto curLang = app::SettingsManager::instance().language();
  auto *enAct = langMenu->addAction(QStringLiteral("English"));
  enAct->setCheckable(true);
  enAct->setChecked(curLang == app::AppLanguage::English);
  connect(enAct, &QAction::triggered, this, []() {
    app::SettingsManager::instance().setLanguage(app::AppLanguage::English);
  });

  auto *viAct = langMenu->addAction(QStringLiteral("Tiếng Việt"));
  viAct->setCheckable(true);
  viAct->setChecked(curLang == app::AppLanguage::Vietnamese);
  connect(viAct, &QAction::triggered, this, []() {
    app::SettingsManager::instance().setLanguage(app::AppLanguage::Vietnamese);
  });

  const bool isDark = (tm.currentMode() == app::ThemeMode::Dark);
  auto *themeAct = menu.addAction(tm.icon(QStringLiteral("theme_toggle")),
                                  isDark ? tr("Switch to Light Theme")
                                         : tr("Switch to Dark Theme"));
  setMenuShortcutHint(themeAct, QStringLiteral("Ctrl+T"));
  connect(themeAct, &QAction::triggered, this, [isDark]() {
    app::ThemeManager::instance().applyTheme(isDark ? app::ThemeMode::Light
                                                    : app::ThemeMode::Dark);
  });
}

void MainWindow::populateHelpMenu(QMenu &menu) {
  menu.clear();
  const auto &tm = app::ThemeManager::instance();
  const bool unlocked = app::VaultManager::instance().isUnlocked();

  if (unlocked) {
    auto *browserAct =
        menu.addAction(tm.icon(QStringLiteral("category_server")),
                       tr("Browser Extension Setup..."));
    connect(browserAct, &QAction::triggered, this,
            [this]() { openSettingsDialog(3); });

    auto *sshAct = menu.addAction(tm.icon(QStringLiteral("category_ssh")),
                                  tr("SSH Agent Configuration..."));
    connect(sshAct, &QAction::triggered, this,
            [this]() { openSettingsDialog(4); });

    menu.addSeparator();
  }

  auto *shortcutsAct = menu.addAction(tr("Keyboard Shortcuts"));
  setMenuShortcutHint(shortcutsAct, QStringLiteral("F1"));
  connect(shortcutsAct, &QAction::triggered, this,
          &MainWindow::openKeyboardShortcutsDialog);

  menu.addSeparator();

  auto *aboutAct = menu.addAction(tm.icon(QStringLiteral("info")),
                                  tr("About Secure Vault Manager"));
  connect(aboutAct, &QAction::triggered, this, &MainWindow::openAboutDialog);
}

void MainWindow::onEmptyTrashRequested() {
  if (!app::VaultManager::instance().isUnlocked()) {
    return;
  }
  const bool confirmed = components::NotificationDialog::question(
      this, tr("Empty Trash?"),
      tr("Permanently delete all items in Trash? This action cannot be "
         "undone."),
      tr("Empty Trash"), tr("Cancel"), true);
  if (!confirmed) {
    return;
  }
  if (core::CoreBridge::instance().emptyTrash()) {
    app::VaultManager::instance().refreshVaultsFromCore();
    loadItemsFromCore();
  } else {
    components::NotificationDialog::warning(
        this, tr("Trash Clean Failed"),
        core::CoreBridge::instance().lastError());
  }
}

void MainWindow::toggleSidebar() { m_sidebarView->toggleCollapsed(); }

std::optional<models::UiVaultItem>
MainWindow::resolveFullItem(const QString &itemId) const {
  if (itemId.isEmpty()) {
    return std::nullopt;
  }
  auto &bridge = core::CoreBridge::instance();
  if (bridge.isInitialized() && bridge.isVaultUnlocked()) {
    if (auto full = bridge.getItem(itemId); full.has_value()) {
      return full;
    }
  }
  return m_itemListView->sourceModel()->findItemById(itemId);
}

void MainWindow::syncSelectionAfterMutation(const QString &itemId,
                                            const models::UiVaultItem *updated,
                                            bool focusOrWasSelected) {
  if (focusOrWasSelected) {
    if (updated && m_itemListView->isItemVisible(itemId)) {
      m_previousSelectedItemId = itemId;
      m_itemListView->selectItem(itemId);
      m_detailView->setItem(*updated);
    } else {
      m_previousSelectedItemId.clear();
      m_itemListView->clearSelection();
      m_detailView->clearItem();
    }
  }
  updateSidebarCounts();
}

void MainWindow::persistItemUpdate(const models::UiVaultItem &item,
                                   bool focusIfVisible) {
  models::UiVaultItem savedItem = item;
  const bool wasSelected =
      !item.id.isEmpty() && (m_itemListView->selectedItemId() == item.id);
  auto &bridge = core::CoreBridge::instance();
  if (bridge.isInitialized() && bridge.isVaultUnlocked()) {
    const QString savedId = bridge.upsertItem(item);
    if (!savedId.isEmpty()) {
      savedItem.id = savedId;
      if (const auto refreshed = bridge.getItem(savedId);
          refreshed.has_value()) {
        savedItem = refreshed.value();
      }
    }
    app::VaultManager::instance().refreshVaultsFromCore();
  }

  m_itemListView->upsertItem(savedItem);
  syncSelectionAfterMutation(savedItem.id, &savedItem,
                             focusIfVisible || wasSelected);
}

void MainWindow::onItemSelected(const QString &itemId) {
  if (itemId.isEmpty()) {
    m_previousSelectedItemId.clear();
    m_detailView->clearItem();
    return;
  }
  if (const auto resolved = resolveFullItem(itemId); resolved.has_value()) {
    m_previousSelectedItemId = itemId;
    m_detailView->setItem(resolved.value());
  } else {
    m_previousSelectedItemId.clear();
    m_detailView->clearItem();
  }
}

void MainWindow::onAddNewItemRequested(models::ItemCategory category) {
  m_previousSelectedItemId = m_itemListView->selectedItemId();
  m_itemListView->clearSelection();
  m_detailView->createNewItem(category);
}

void MainWindow::onDetailCancelRequested() {
  if (!m_previousSelectedItemId.isEmpty()) {
    if (const auto resolved = resolveFullItem(m_previousSelectedItemId);
        resolved.has_value()) {
      m_itemListView->selectItem(m_previousSelectedItemId);
      m_detailView->setItem(resolved.value());
      return;
    }
  }
  m_detailView->clearItem();
}

void MainWindow::onItemSaved(const models::UiVaultItem &item) {
  persistItemUpdate(item, true);
}

void MainWindow::onItemEditRequested(const QString &itemId) {
  const auto resolved = resolveFullItem(itemId);
  if (!resolved.has_value() || resolved->isDeleted) {
    return;
  }
  m_previousSelectedItemId = itemId;
  m_itemListView->selectItem(itemId);
  m_detailView->setItem(resolved.value());
  m_detailView->setEditMode(true);
}

void MainWindow::onItemFavoriteToggleRequested(const QString &itemId) {
  auto resolved = resolveFullItem(itemId);
  if (!resolved.has_value() || resolved->isDeleted) {
    return;
  }
  resolved->isFavorite = !resolved->isFavorite;
  resolved->updatedAt = QDateTime::currentDateTime();
  persistItemUpdate(resolved.value(), false);
}

void MainWindow::onItemMoveToVaultRequested(const QString &itemId,
                                            const QString &targetVaultId) {
  if (targetVaultId.isEmpty()) {
    return;
  }
  auto resolved = resolveFullItem(itemId);
  if (!resolved.has_value() || resolved->vaultId == targetVaultId) {
    return;
  }
  resolved->vaultId = targetVaultId;
  resolved->updatedAt = QDateTime::currentDateTime();
  persistItemUpdate(resolved.value(), false);
}

void MainWindow::onItemTrashRequested(const QString &itemId) {
  const auto opt = resolveFullItem(itemId);
  if (!opt.has_value()) {
    return;
  }

  if (!components::ConfirmDeleteDialog::confirm(this, opt.value(), false)) {
    return;
  }

  const bool wasSelected = (m_itemListView->selectedItemId() == itemId);
  auto &bridge = core::CoreBridge::instance();
  if (bridge.isInitialized() && bridge.isVaultUnlocked()) {
    bridge.trashItem(itemId);
    app::VaultManager::instance().refreshVaultsFromCore();
  }
  models::UiVaultItem updated = opt.value();
  updated.isDeleted = true;
  updated.updatedAt = QDateTime::currentDateTime();
  m_itemListView->upsertItem(updated);
  syncSelectionAfterMutation(itemId, &updated, wasSelected);
}

void MainWindow::onItemRestoreRequested(const QString &itemId) {
  const bool wasSelected = (m_itemListView->selectedItemId() == itemId);
  auto &bridge = core::CoreBridge::instance();
  if (bridge.isInitialized() && bridge.isVaultUnlocked()) {
    bridge.restoreItem(itemId);
    app::VaultManager::instance().refreshVaultsFromCore();
  }
  if (const auto opt = resolveFullItem(itemId); opt.has_value()) {
    models::UiVaultItem updated = opt.value();
    updated.isDeleted = false;
    updated.updatedAt = QDateTime::currentDateTime();
    m_itemListView->upsertItem(updated);
    syncSelectionAfterMutation(itemId, &updated, wasSelected);
    return;
  }
  updateSidebarCounts();
}

void MainWindow::onItemPurgeRequested(const QString &itemId) {
  const auto opt = m_itemListView->sourceModel()->findItemById(itemId);
  if (!opt.has_value()) {
    return;
  }

  if (!components::ConfirmDeleteDialog::confirm(this, opt.value(), true)) {
    return;
  }

  const bool wasSelected = (m_itemListView->selectedItemId() == itemId);
  auto &bridge = core::CoreBridge::instance();
  if (bridge.isInitialized() && bridge.isVaultUnlocked()) {
    bridge.purgeItem(itemId);
    app::VaultManager::instance().refreshVaultsFromCore();
  }
  m_itemListView->removeItem(itemId);
  syncSelectionAfterMutation(itemId, nullptr, wasSelected);
}

void MainWindow::onQuickCopyRequested(const QString &itemId,
                                      const QString &fieldName) {
  const auto opt = resolveFullItem(itemId);
  if (!opt.has_value()) {
    return;
  }

  const auto &item = opt.value();
  auto &bridge = core::CoreBridge::instance();
  const bool unlocked = bridge.isInitialized() && bridge.isVaultUnlocked();

  auto revealOrFallback = [&](const QString &localValue,
                              const QString &revealKey) -> QString {
    if (unlocked) {
      const QString revealed = bridge.revealField(itemId, revealKey);
      if (!revealed.isEmpty()) {
        return revealed;
      }
    }
    return localValue;
  };

  if (fieldName == QStringLiteral("username")) {
    app::ClipboardManager::instance().copyPlainText(item.username);
  } else if (fieldName == QStringLiteral("password")) {
    const QString secret =
        revealOrFallback(item.password, QStringLiteral("password"));
    app::ClipboardManager::instance().copySensitive(
        secret, tr("Password for %1").arg(item.title));
  } else if (fieldName == QStringLiteral("cardNumber")) {
    const QString secret =
        revealOrFallback(item.cardNumber, QStringLiteral("cardNumber"));
    app::ClipboardManager::instance().copySensitive(
        secret, tr("Card Number for %1").arg(item.title));
  } else if (fieldName == QStringLiteral("cardCvv")) {
    const QString secret =
        revealOrFallback(item.cardCvv, QStringLiteral("cardCvv"));
    app::ClipboardManager::instance().copySensitive(
        secret, tr("CVV for %1").arg(item.title));
  } else if (fieldName == QStringLiteral("totp") && unlocked) {
    const QString totpInput =
        item.totpSecret.isEmpty() ? itemId : item.totpSecret;
    const QString code =
        bridge.generateTotp(totpInput).first.trimmed().remove(QLatin1Char(' '));
    if (!code.isEmpty() && !code.startsWith(QLatin1Char('-'))) {
      app::ClipboardManager::instance().copySensitive(
          code, tr("One-Time Code (2FA)"));
    }
  }
}

void MainWindow::loadItemsFromCore() {
  SVM_ACTION_SCOPE("UI", "MainWindow::loadItemsFromCore");
  auto &bridge = core::CoreBridge::instance();
  if (!bridge.isInitialized() || !bridge.isVaultUnlocked()) {
    return;
  }
  m_itemListView->proxyModel()->setVaultFilter(
      app::VaultManager::instance().activeVaultId());
  const QList<models::UiVaultItem> coreItems = bridge.listItems();
  if (!coreItems.isEmpty()) {
    const QString prevId = m_itemListView->selectedItemId();
    m_itemListView->setItems(coreItems);
    updateSidebarCounts();

    if (!prevId.isEmpty()) {
      const auto opt = m_itemListView->sourceModel()->findItemById(prevId);
      if (opt.has_value()) {
        m_itemListView->selectItem(prevId);
        const auto fullOpt = bridge.getItem(prevId);
        m_detailView->setItem(fullOpt.value_or(opt.value()));
      } else {
        m_itemListView->clearSelection();
        m_detailView->clearItem();
      }
    } else {
      m_itemListView->clearSelection();
      m_detailView->clearItem();
    }
  } else {
    m_itemListView->setItems({});
    m_detailView->clearItem();
    updateSidebarCounts();
  }
}

void MainWindow::updateSidebarCounts() {
  const auto &items = m_itemListView->sourceModel()->items();
  const QString activeVId = app::VaultManager::instance().activeVaultId();

  QMap<views::SidebarFilterType, int> filterCounts;
  QMap<models::ItemCategory, int> categoryCounts;

  const std::array<models::ItemCategory, 9> allCategories = {
      models::ItemCategory::Login,      models::ItemCategory::Passkey,
      models::ItemCategory::Card,       models::ItemCategory::Identity,
      models::ItemCategory::SecureNote, models::ItemCategory::SshKey,
      models::ItemCategory::Database,   models::ItemCategory::Server,
      models::ItemCategory::Wifi};
  for (const auto cat : allCategories) {
    categoryCounts[cat] = 0;
  }

  int allCount = 0;
  int favCount = 0;
  int trashCount = 0;

  for (const auto &item : items) {
    if (!activeVId.isEmpty() && activeVId != QStringLiteral("all")) {
      if (item.vaultId != activeVId) {
        continue;
      }
    }
    if (item.isDeleted) {
      ++trashCount;
    } else if (!item.isArchived) {
      ++allCount;
      categoryCounts[item.category] =
          categoryCounts.value(item.category, 0) + 1;

      if (item.isFavorite) {
        ++favCount;
      }
    }
  }

  filterCounts[views::SidebarFilterType::AllItems] = allCount;
  filterCounts[views::SidebarFilterType::Favorites] = favCount;
  filterCounts[views::SidebarFilterType::Trash] = trashCount;

  m_sidebarView->updateCounts(filterCounts, categoryCounts);
}

void MainWindow::updateHeaderTitle() {
  if (!m_headerBar) {
    return;
  }
  if (!app::VaultManager::instance().isConfigured()) {
    m_headerBar->setTitleText(tr("Setup - Secure Vault Manager"));
  } else if (app::VaultManager::instance().isLocked()) {
    m_headerBar->setTitleText(tr("Locked - Secure Vault Manager"));
  } else {
    m_headerBar->setTitleText(tr("Secure Vault Manager"));
  }
}

void MainWindow::updateNavigationState() {
  if (!app::VaultManager::instance().isConfigured()) {
    if (m_settingsDialog && m_settingsDialog->isVisible()) {
      m_settingsDialog->reject();
    }
    if (m_itemListView) {
      m_itemListView->setItems({});
    }
    if (m_detailView) {
      m_detailView->clearItem();
    }
    updateSidebarCounts();
    m_mainStack->setCurrentWidget(m_onboardingView);
    m_onboardingView->resetWizard();
    updateHeaderTitle();
    m_headerBar->setUnlockedMenuVisible(false);
    m_inactivityTimer.stop();
  } else if (app::VaultManager::instance().isLocked()) {
    m_mainStack->setCurrentWidget(m_lockScreenView);
    m_lockScreenView->resetLockScreen();
    updateHeaderTitle();
    m_headerBar->setUnlockedMenuVisible(false);
    m_inactivityTimer.stop();
    if (m_itemListView) {
      m_itemListView->clearSelection();
      m_itemListView->setItems({});
    }
    if (m_detailView) {
      m_detailView->clearItem();
    }
    if (m_settingsDialog && m_settingsDialog->isVisible()) {
      m_settingsDialog->reject();
    }
    if (app::SettingsManager::instance().clearClipboardOnLock()) {
      app::ClipboardManager::instance().clearClipboard();
    }
  } else {
    m_mainStack->setCurrentWidget(m_mainSplitter);
    updateHeaderTitle();
    m_headerBar->setUnlockedMenuVisible(true);
    m_lockScreenView->resetLockScreen();
    m_itemListView->proxyModel()->setVaultFilter(
        app::VaultManager::instance().activeVaultId());
    loadItemsFromCore();
    updateSidebarCounts();
    updateInactivityTimer();
  }
}

void MainWindow::showNonModalDialog(QDialog *dialog) {
  if (!dialog) {
    return;
  }
  dialog->setModal(false);
  dialog->setWindowModality(Qt::NonModal);
  const QPoint center = geometry().center();
  dialog->move(center.x() - dialog->width() / 2,
               center.y() - dialog->height() / 2);
  if (!components::DialogDragHelper::s_activeDialogs.contains(dialog)) {
    components::DialogDragHelper::s_activeDialogs.append(dialog);
  }
  dialog->show();
  dialog->raise();
  dialog->activateWindow();
}

void MainWindow::openSettingsDialog(int tabIndex) {
  if (!app::VaultManager::instance().isUnlocked()) {
    return;
  }
  if (!m_settingsDialog) {
    m_settingsDialog = new views::SettingsDialog(this);
    connect(m_settingsDialog, &QDialog::finished, this, [this]() {
      components::DialogDragHelper::s_activeDialogs.removeOne(m_settingsDialog);
    });
  }
  m_settingsDialog->selectTab(tabIndex);
  showNonModalDialog(m_settingsDialog);
}

void MainWindow::openAboutDialog() {
  if (!m_aboutDialog) {
    m_aboutDialog = new views::AboutDialog(this);
    connect(m_aboutDialog, &QDialog::finished, this, [this]() {
      components::DialogDragHelper::s_activeDialogs.removeOne(m_aboutDialog);
    });
  }
  showNonModalDialog(m_aboutDialog);
}

void MainWindow::openKeyboardShortcutsDialog() {
  if (!m_shortcutsDialog) {
    m_shortcutsDialog = new views::KeyboardShortcutsDialog(this);
    connect(m_shortcutsDialog, &QDialog::finished, this, [this]() {
      components::DialogDragHelper::s_activeDialogs.removeOne(
          m_shortcutsDialog);
    });
  }
  showNonModalDialog(m_shortcutsDialog);
}

void MainWindow::updateInactivityTimer() {
  const int mins = app::SettingsManager::instance().autoLockMinutes();
  if (mins <= 0) {
    m_inactivityTimer.stop();
  } else {
    m_inactivityTimer.setInterval(mins * 60 * 1000);
    if (app::VaultManager::instance().isUnlocked()) {
      m_inactivityTimer.start();
    }
  }
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event) {
  if (event->type() == QEvent::MouseMove ||
      event->type() == QEvent::MouseButtonPress ||
      event->type() == QEvent::KeyPress) {
    if (app::VaultManager::instance().isUnlocked() &&
        m_inactivityTimer.isActive()) {
      m_inactivityTimer.start(); // reset inactivity timeout
      core::CoreBridge::instance().reportIdle(0);
    }
  }

  // Chặn tương tác ngoài vùng nếu có Dialog đang mở
  auto *activeDlg = findActiveModalDialog();
  if (activeDlg) {
    const QEvent::Type t = event->type();
    const bool isClick =
        (t == QEvent::MouseButtonPress || t == QEvent::MouseButtonDblClick);
    const bool isBlockedInput =
        isClick || t == QEvent::MouseButtonRelease || t == QEvent::Wheel ||
        t == QEvent::ContextMenu || t == QEvent::KeyPress ||
        t == QEvent::KeyRelease || t == QEvent::ShortcutOverride;

    if (isBlockedInput) {
      // 1. Kiểm tra ở cấp độ QWidget (cho phép 100% nếu thuộc về activeDlg)
      if (auto *widget = qobject_cast<QWidget *>(obj)) {
        if (widget == activeDlg || widget->window() == activeDlg ||
            activeDlg->isAncestorOf(widget)) {
          return false;
        }

        // Cho phép InWindow child overlay (như popup combobox)
        if (widget->objectName() == QStringLiteral("InWindowPopupOverlay") ||
            (widget->parentWidget() &&
             widget->parentWidget()->objectName() ==
                 QStringLiteral("InWindowPopupOverlay"))) {
          return false;
        }
      }

      // 2. Kiểm tra ở cấp độ QWindow (cho phép nếu thuộc windowHandle của
      // activeDlg)
      if (auto *win = qobject_cast<QWindow *>(obj)) {
        if (win == activeDlg->windowHandle()) {
          return false;
        }

        QWindow *tp = win->transientParent();
        while (tp) {
          if (tp == activeDlg->windowHandle()) {
            return false;
          }
          tp = tp->transientParent();
        }
      }

      // 3. Cho phép popup widget đang hiển thị
      if (QWidget *popup = QApplication::activePopupWidget()) {
        if (popup == activeDlg || popup->window() == activeDlg) {
          return false;
        }
      }

      // 4. Nếu là click bên ngoài activeDlg (ví dụ vào SettingsDialog phía sau
      // hoặc MainWindow)
      if (isClick) {
        activeDlg->raise();
        activeDlg->activateWindow();
        components::DialogDragHelper::shakeDialog(activeDlg);
      }
      return true;
    }
  }

  return QMainWindow::eventFilter(obj, event);
}

void MainWindow::setTrayManager(app::TrayManager *trayManager) {
  m_trayManager = trayManager;
  if (m_trayManager) {
    connect(&app::VaultManager::instance(),
            &app::VaultManager::lockStateChanged, m_trayManager,
            &app::TrayManager::setVaultLocked);
  }
}

void MainWindow::resizeEvent(QResizeEvent *event) {
  QMainWindow::resizeEvent(event);
  if (m_resizeHandler) {
    m_resizeHandler->updateGeometry();
  }
}

void MainWindow::changeEvent(QEvent *event) {
  if (event->type() == QEvent::WindowStateChange) {
    const bool max = isMaximized();
    if (m_headerBar) {
      m_headerBar->updateMaximizeIcon(max);
    }
    if (m_resizeHandler) {
      m_resizeHandler->setVisible(!max);
    }
    if (centralWidget()) {
      const int m = max ? 0 : 1;
      if (centralWidget()->layout()) {
        centralWidget()->layout()->setContentsMargins(m, m, m, m);
      }
      centralWidget()->setProperty("maximized", max);
      centralWidget()->style()->unpolish(centralWidget());
      centralWidget()->style()->polish(centralWidget());
    }
  } else if (event->type() == QEvent::LanguageChange) {
    retranslateUi();
  }
  QMainWindow::changeEvent(event);
}

void MainWindow::retranslateUi() {
  setWindowTitle(tr("Secure Vault Manager"));
  if (m_headerBar) {
    m_headerBar->retranslateUi();
    updateHeaderTitle();
  }
  if (m_onboardingView) {
    m_onboardingView->retranslateUi();
  }
  if (m_lockScreenView) {
    m_lockScreenView->retranslateUi();
  }
  if (m_sidebarView) {
    m_sidebarView->retranslateUi();
  }
  if (m_itemListView) {
    m_itemListView->retranslateUi();
  }
  if (m_detailView) {
    m_detailView->retranslateUi();
  }
  if (m_aboutDialog) {
    m_aboutDialog->retranslateUi();
  }
  if (m_shortcutsDialog) {
    m_shortcutsDialog->retranslateUi();
  }
  if (m_settingsDialog) {
    m_settingsDialog->retranslateUi();
  }
}

void MainWindow::setupSleepMonitor() {
#if defined(Q_OS_LINUX)
  const QString gdbus = QStandardPaths::findExecutable(QStringLiteral("gdbus"));
  if (gdbus.isEmpty()) {
    return;
  }
  m_sleepMonitorProcess = new QProcess(this);
  connect(m_sleepMonitorProcess, &QProcess::readyReadStandardOutput, this,
          [this]() {
            if (!app::SettingsManager::instance().lockOnSleep() ||
                !app::VaultManager::instance().isUnlocked()) {
              return;
            }
            const QByteArray data =
                m_sleepMonitorProcess->readAllStandardOutput();
            if (data.contains("PrepareForSleep (true") ||
                data.contains(".Lock ()")) {
              app::VaultManager::instance().lock();
              if (m_trayManager) {
                m_trayManager->setVaultLocked(true);
              }
            }
          });
  m_sleepMonitorProcess->start(
      gdbus,
      {QStringLiteral("monitor"), QStringLiteral("--system"),
       QStringLiteral("--dest"), QStringLiteral("org.freedesktop.login1")});
#endif
}

MainWindow::~MainWindow() { stopSleepMonitor(); }

void MainWindow::stopSleepMonitor() {
  if (!m_sleepMonitorProcess) {
    return;
  }
  m_sleepMonitorProcess->disconnect(this);
  if (m_sleepMonitorProcess->state() != QProcess::NotRunning) {
    m_sleepMonitorProcess->terminate();
    if (!m_sleepMonitorProcess->waitForFinished(300)) {
      m_sleepMonitorProcess->kill();
      m_sleepMonitorProcess->waitForFinished(300);
    }
  }
}

void MainWindow::closeEvent(QCloseEvent *event) {
  if (app::SettingsManager::instance().closeToTray() && m_trayManager &&
      m_trayManager->isSystemTrayAvailable()) {
    hide();
    event->ignore();
  } else {
    stopSleepMonitor();
    event->accept();
    QApplication::quit();
  }
}

QDialog *MainWindow::findActiveModalDialog() const {
  QDialog *topModal = components::DialogDragHelper::activeModalDialog();
  if (topModal) {
    // Tìm nốt lá sâu nhất nếu có dialog con đang hiển thị đè lên
    const auto topLevels = QApplication::topLevelWidgets();
    for (auto *w : topLevels) {
      if (auto *dlg = qobject_cast<QDialog *>(w)) {
        if (dlg->isVisible() && dlg != topModal) {
          if (dlg->parentWidget() == topModal || dlg->parent() == topModal) {
            return dlg;
          }
          if (dlg->windowHandle() && topModal->windowHandle() &&
              dlg->windowHandle()->transientParent() ==
                  topModal->windowHandle()) {
            return dlg;
          }
        }
      }
    }
    return topModal;
  }

  const auto topLevels = QApplication::topLevelWidgets();
  QDialog *topDlg = nullptr;
  for (auto *w : topLevels) {
    if (auto *dlg = qobject_cast<QDialog *>(w)) {
      if (dlg->isVisible()) {
        topDlg = dlg;
      }
    }
  }
  return topDlg;
}

} // namespace svm
