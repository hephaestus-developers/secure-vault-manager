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
#include <QHBoxLayout>
#include <QIcon>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLayout>
#include <QMouseEvent>
#include <QPropertyAnimation>
#include <QStandardPaths>
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
  connect(m_sidebarView, &views::SidebarView::foldersChanged, this, [this]() {
    if (app::VaultManager::instance().isUnlocked()) {
      loadItemsFromCore();
    }
  });
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
  connect(m_headerBar, &components::UnifiedHeaderBar::aboutRequested, this,
          [this]() {
            if (!m_aboutDialog) {
              m_aboutDialog = new views::AboutDialog(this);
              connect(m_aboutDialog, &QDialog::finished, this, [this]() {
                components::DialogDragHelper::s_activeDialogs.removeOne(
                    m_aboutDialog);
              });
            }
            m_aboutDialog->setModal(false);
            m_aboutDialog->setWindowModality(Qt::NonModal);
            const QPoint center = geometry().center();
            m_aboutDialog->move(center.x() - m_aboutDialog->width() / 2,
                                center.y() - m_aboutDialog->height() / 2);
            if (!components::DialogDragHelper::s_activeDialogs.contains(
                    m_aboutDialog)) {
              components::DialogDragHelper::s_activeDialogs.append(
                  m_aboutDialog);
            }
            m_aboutDialog->show();
            m_aboutDialog->raise();
            m_aboutDialog->activateWindow();
          });
  connect(m_headerBar, &components::UnifiedHeaderBar::settingsRequested, this,
          &MainWindow::openSettingsDialog);
  connect(m_sidebarView, &views::SidebarView::settingsRequested, this,
          &MainWindow::openSettingsDialog);

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
          &MainWindow::openSettingsDialog);

  auto *lockShortcut =
      new QShortcut(QKeySequence(QStringLiteral("Ctrl+L")), this);
  connect(lockShortcut, &QShortcut::activated, this, [this]() {
    if (!app::VaultManager::instance().isLocked()) {
      app::VaultManager::instance().lock();
      if (m_trayManager) {
        m_trayManager->setVaultLocked(true);
      }
    }
  });

  auto *quitShortcut = new QShortcut(QKeySequence::Quit, this);
  connect(quitShortcut, &QShortcut::activated, this, &QWidget::close);
}

void MainWindow::toggleSidebar() { m_sidebarView->toggleCollapsed(); }

void MainWindow::onItemSelected(const QString &itemId) {
  if (itemId.isEmpty()) {
    m_previousSelectedItemId.clear();
    m_detailView->clearItem();
    return;
  }
  auto &bridge = core::CoreBridge::instance();
  std::optional<models::UiVaultItem> resolved;
  if (bridge.isInitialized() && bridge.isVaultUnlocked()) {
    resolved = bridge.getItem(itemId);
  }
  if (!resolved.has_value()) {
    resolved = m_itemListView->sourceModel()->findItemById(itemId);
  }
  if (resolved.has_value()) {
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
    auto &bridge = core::CoreBridge::instance();
    std::optional<models::UiVaultItem> resolved;
    if (bridge.isInitialized() && bridge.isVaultUnlocked()) {
      resolved = bridge.getItem(m_previousSelectedItemId);
    }
    if (!resolved.has_value()) {
      resolved =
          m_itemListView->sourceModel()->findItemById(m_previousSelectedItemId);
    }
    if (resolved.has_value()) {
      m_itemListView->selectItem(m_previousSelectedItemId);
      m_detailView->setItem(resolved.value());
      return;
    }
  }
  m_detailView->clearItem();
}

void MainWindow::onItemSaved(const models::UiVaultItem &item) {
  models::UiVaultItem savedItem = item;
  auto &bridge = core::CoreBridge::instance();
  if (bridge.isInitialized() && bridge.isVaultUnlocked()) {
    const QString savedId = bridge.upsertItem(item);
    if (!savedId.isEmpty()) {
      savedItem.id = savedId;
      const auto refreshed = bridge.getItem(savedId);
      if (refreshed.has_value()) {
        savedItem = refreshed.value();
      }
    }
    app::VaultManager::instance().refreshVaultsFromCore();
  }

  m_itemListView->upsertItem(savedItem);
  m_previousSelectedItemId = savedItem.id;
  m_itemListView->selectItem(savedItem.id);
  m_detailView->setItem(savedItem);
  updateSidebarCounts();
}

void MainWindow::onItemTrashRequested(const QString &itemId) {
  const auto opt = m_itemListView->sourceModel()->findItemById(itemId);
  if (!opt.has_value()) {
    return;
  }

  if (!components::ConfirmDeleteDialog::confirm(this, opt.value(), false)) {
    return;
  }

  auto &bridge = core::CoreBridge::instance();
  if (bridge.isInitialized() && bridge.isVaultUnlocked()) {
    bridge.trashItem(itemId);
  }
  models::UiVaultItem updated = opt.value();
  updated.isDeleted = true;
  updated.updatedAt = QDateTime::currentDateTime();
  m_itemListView->upsertItem(updated);
  m_detailView->clearItem();
  updateSidebarCounts();
}

void MainWindow::onItemRestoreRequested(const QString &itemId) {
  auto &bridge = core::CoreBridge::instance();
  if (bridge.isInitialized() && bridge.isVaultUnlocked()) {
    bridge.restoreItem(itemId);
  }
  const auto opt = m_itemListView->sourceModel()->findItemById(itemId);
  if (opt.has_value()) {
    models::UiVaultItem updated =
        (bridge.isInitialized() && bridge.isVaultUnlocked())
            ? bridge.getItem(itemId).value_or(opt.value())
            : opt.value();
    updated.isDeleted = false;
    updated.updatedAt = QDateTime::currentDateTime();
    m_itemListView->upsertItem(updated);
    m_detailView->setItem(updated);
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

  auto &bridge = core::CoreBridge::instance();
  if (bridge.isInitialized() && bridge.isVaultUnlocked()) {
    bridge.purgeItem(itemId);
  }
  m_itemListView->removeItem(itemId);
  m_detailView->clearItem();
  updateSidebarCounts();
}

void MainWindow::onQuickCopyRequested(const QString &itemId,
                                      const QString &fieldName) {
  const auto opt = m_itemListView->sourceModel()->findItemById(itemId);
  if (!opt.has_value()) {
    return;
  }

  const auto &item = opt.value();
  auto &bridge = core::CoreBridge::instance();
  if (fieldName == QStringLiteral("password")) {
    QString secret = item.password;
    if (bridge.isInitialized() && bridge.isVaultUnlocked()) {
      const QString revealed =
          bridge.revealField(itemId, QStringLiteral("password"));
      if (!revealed.isEmpty()) {
        secret = revealed;
      }
    }
    app::ClipboardManager::instance().copySensitive(
        secret, tr("Password for %1").arg(item.title));
  } else if (fieldName == QStringLiteral("username")) {
    app::ClipboardManager::instance().copyPlainText(item.username);
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
    m_sidebarView->refreshFolders();
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
    m_sidebarView->refreshFolders();
    updateSidebarCounts();
  }
}

void MainWindow::updateSidebarCounts() {
  const auto &items = m_itemListView->sourceModel()->items();
  const QString activeVId = app::VaultManager::instance().activeVaultId();

  QMap<views::SidebarFilterType, int> filterCounts;
  QMap<models::ItemCategory, int> categoryCounts;
  QMap<QString, int> folderCounts;

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
  int weakCount = 0;
  int expiringCount = 0;

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
      if (!item.folderId.isEmpty()) {
        folderCounts[item.folderId] = folderCounts.value(item.folderId, 0) + 1;
      }
      if (models::isWeakPasswordItem(item)) {
        ++weakCount;
      }
      if (models::isExpiringItem(item)) {
        ++expiringCount;
      }
    }
  }

  filterCounts[views::SidebarFilterType::AllItems] = allCount;
  filterCounts[views::SidebarFilterType::Favorites] = favCount;
  filterCounts[views::SidebarFilterType::Trash] = trashCount;
  filterCounts[views::SidebarFilterType::WatchtowerWeak] = weakCount;
  filterCounts[views::SidebarFilterType::WatchtowerExpiring] = expiringCount;

  m_sidebarView->updateCounts(filterCounts, categoryCounts, folderCounts);
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
    m_headerBar->setTitleText(tr("Setup - Secure Vault Manager"));
    m_headerBar->setSettingsVisible(false);
    m_inactivityTimer.stop();
  } else if (app::VaultManager::instance().isLocked()) {
    m_mainStack->setCurrentWidget(m_lockScreenView);
    m_lockScreenView->resetLockScreen();
    m_headerBar->setTitleText(tr("Locked - Secure Vault Manager"));
    m_headerBar->setSettingsVisible(false);
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
    m_headerBar->setTitleText(tr("Secure Vault Manager"));
    m_headerBar->setSettingsVisible(true);
    m_lockScreenView->resetLockScreen();
    m_itemListView->proxyModel()->setVaultFilter(
        app::VaultManager::instance().activeVaultId());
    loadItemsFromCore();
    updateSidebarCounts();
    updateInactivityTimer();
  }
}

void MainWindow::openSettingsDialog() {
  if (!app::VaultManager::instance().isUnlocked()) {
    return;
  }
  if (!m_settingsDialog) {
    m_settingsDialog = new views::SettingsDialog(this);
    connect(m_settingsDialog, &QDialog::finished, this, [this]() {
      components::DialogDragHelper::s_activeDialogs.removeOne(m_settingsDialog);
    });
  }
  m_settingsDialog->setModal(false);
  m_settingsDialog->setWindowModality(Qt::NonModal);
  const QPoint center = geometry().center();
  m_settingsDialog->move(center.x() - m_settingsDialog->width() / 2,
                         center.y() - m_settingsDialog->height() / 2);
  if (!components::DialogDragHelper::s_activeDialogs.contains(
          m_settingsDialog)) {
    components::DialogDragHelper::s_activeDialogs.append(m_settingsDialog);
  }
  m_settingsDialog->show();
  m_settingsDialog->raise();
  m_settingsDialog->activateWindow();
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
