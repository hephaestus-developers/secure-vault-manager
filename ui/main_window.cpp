/**
 * @file ui/main_window.cpp
 * @brief Hiện thực chi tiết MainWindow tích hợp UnifiedHeaderBar và 3 cột thích
 * ứng.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "main_window.h"
#include "app/clipboard_manager.h"
#include "app/settings_manager.h"
#include "app/theme_manager.h"
#include "app/tray_manager.h"
#include "app/vault_manager.h"
#include "components/menubar/app_menu_bar.h"
#include "components/unified_header_bar.h"
#include "components/window_resize_handler.h"
#include "core/core_bridge.h"
#include "views/settings/settings_dialog.h"

#include <QApplication>
#include <QCloseEvent>
#include <QDateTime>
#include <QHBoxLayout>
#include <QIcon>
#include <QMessageBox>
#include <QVBoxLayout>

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
  populateMockData();

  // Khởi tạo bộ xử lý co giãn kích thước cửa sổ không viền 8 hướng
  m_resizeHandler = new components::WindowResizeHandler(this);
}

void MainWindow::setupUI() {
  auto *centralWidget = new QWidget(this);
  centralWidget->setObjectName(QStringLiteral("CentralWidget"));
  auto *layout = new QVBoxLayout(centralWidget);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  // 1. Khởi tạo AppMenuBar và UnifiedHeaderBar
  m_menuBar = new components::AppMenuBar(this);
  m_headerBar = new components::UnifiedHeaderBar(m_menuBar, this);
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

  // 3. Command Palette Dialog
  m_commandPalette =
      new views::CommandPaletteDialog(m_itemListView->sourceModel(), this);

  // 4. Kết nối tín hiệu Header Bar
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
  connect(m_headerBar, &components::UnifiedHeaderBar::commandPaletteRequested,
          this, &MainWindow::openCommandPalette);
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
  connect(m_menuBar, &components::AppMenuBar::lockVaultRequested, this,
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
  connect(m_detailView, &views::DetailInspectorView::cancelRequested, this,
          &MainWindow::onDetailCancelRequested);

  // 6. Kết nối HeaderBar, Sidebar & MenuBar Actions
  connect(m_headerBar, &components::UnifiedHeaderBar::aboutRequested, this,
          [this]() {
            if (!m_aboutDialog) {
              m_aboutDialog = new views::AboutDialog(this);
            }
            m_aboutDialog->show();
            m_aboutDialog->raise();
            m_aboutDialog->activateWindow();
          });
  connect(m_headerBar, &components::UnifiedHeaderBar::settingsRequested, this,
          &MainWindow::openSettingsDialog);
  connect(m_sidebarView, &views::SidebarView::settingsRequested, this,
          &MainWindow::openSettingsDialog);

  connect(m_menuBar, &components::AppMenuBar::exitRequested, qApp,
          &QApplication::quit);

  // 7. Thiết lập Inactivity Timer tự động khóa Vault
  connect(&m_inactivityTimer, &QTimer::timeout, this, [this]() {
    if (app::VaultManager::instance().isUnlocked()) {
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
  qApp->installEventFilter(this);

  // 8. Đồng bộ trạng thái điều hướng khởi đầu
  updateNavigationState();
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

  m_commandPaletteShortcut =
      new QShortcut(QKeySequence(QStringLiteral("Ctrl+K")), this);
  connect(m_commandPaletteShortcut, &QShortcut::activated, this, [this]() {
    if (!app::VaultManager::instance().isLocked()) {
      openCommandPalette();
    }
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
}

void MainWindow::toggleSidebar() { m_sidebarView->toggleCollapsed(); }

void MainWindow::openCommandPalette() { m_commandPalette->openPalette(); }

void MainWindow::onItemSelected(const QString &itemId) {
  const auto opt = m_itemListView->sourceModel()->findItemById(itemId);
  if (opt.has_value()) {
    m_previousSelectedItemId = itemId;
    m_detailView->setItem(opt.value());
  }
}

void MainWindow::onAddNewItemRequested(models::ItemCategory category) {
  m_previousSelectedItemId = m_itemListView->selectedItemId();
  m_itemListView->clearSelection();
  m_detailView->createNewItem(category);
}

void MainWindow::onDetailCancelRequested() {
  if (!m_previousSelectedItemId.isEmpty()) {
    const auto opt =
        m_itemListView->sourceModel()->findItemById(m_previousSelectedItemId);
    if (opt.has_value()) {
      m_itemListView->selectItem(m_previousSelectedItemId);
      m_detailView->setItem(opt.value());
      return;
    }
  }
  m_detailView->clearItem();
}

void MainWindow::onItemSaved(const models::UiVaultItem &item) {
  m_itemListView->upsertItem(item);
  m_previousSelectedItemId = item.id;
  m_itemListView->selectItem(item.id);
  m_detailView->setItem(item);
  updateSidebarCounts();
}

void MainWindow::onQuickCopyRequested(const QString &itemId,
                                      const QString &fieldName) {
  const auto opt = m_itemListView->sourceModel()->findItemById(itemId);
  if (!opt.has_value()) {
    return;
  }

  const auto &item = opt.value();
  if (fieldName == QStringLiteral("password")) {
    app::ClipboardManager::instance().copySensitive(
        item.password, tr("Password for %1").arg(item.title), 30);
  } else if (fieldName == QStringLiteral("username")) {
    app::ClipboardManager::instance().copyPlainText(item.username);
  }
}

void MainWindow::populateMockData() {
  QList<models::UiVaultItem> items;

  // 1. GitHub Work
  models::UiVaultItem git;
  git.id = QStringLiteral("item-001");
  git.title = QStringLiteral("GitHub (Work Account)");
  git.category = models::ItemCategory::Login;
  git.vaultId = QStringLiteral("personal");
  git.username = QStringLiteral("octocat@hephaestus.io");
  git.password = QStringLiteral("ghp_9284Fja9028Fh2q8fh0283fh90q");
  git.websiteUrl = QStringLiteral("https://github.com/login");
  git.hasTotp = true;
  git.totpSecret = QStringLiteral("JBSWY3DPEHPK3PXP");
  git.isFavorite = true;
  git.notes = QStringLiteral(
      "Main engineering organization account with SSH & 2FA enabled.");
  git.updatedAt = QDateTime::currentDateTime().addDays(-2);

  models::UiCustomField gitField1;
  gitField1.id = QStringLiteral("cf-git-1");
  gitField1.label = QStringLiteral("Personal Access Token");
  gitField1.value = QStringLiteral("ghp_98418fha09283fhkjashf9283");
  gitField1.kind = models::CustomFieldKind::Concealed;
  git.customFields.append(gitField1);

  models::UiCustomField gitField2;
  gitField2.id = QStringLiteral("cf-git-2");
  gitField2.label = QStringLiteral("Organization Team");
  gitField2.value = QStringLiteral("Core Infrastructure & Security");
  gitField2.kind = models::CustomFieldKind::Text;
  git.customFields.append(gitField2);

  models::UiCustomField gitField3;
  gitField3.id = QStringLiteral("cf-git-3");
  gitField3.label = QStringLiteral("Hardware Key (FIDO2) Required");
  gitField3.value = QStringLiteral("true");
  gitField3.kind = models::CustomFieldKind::Boolean;
  git.customFields.append(gitField3);

  models::UiCustomField gitField4;
  gitField4.id = QStringLiteral("cf-git-4");
  gitField4.label = QStringLiteral("Token Expiration Date");
  gitField4.value = QStringLiteral("2026-12-31");
  gitField4.kind = models::CustomFieldKind::Date;
  git.customFields.append(gitField4);

  items.append(git);

  // 2. AWS Root Account
  models::UiVaultItem aws;
  aws.id = QStringLiteral("item-002");
  aws.title = QStringLiteral("AWS Console (Production)");
  aws.category = models::ItemCategory::Login;
  aws.vaultId = QStringLiteral("work");
  aws.username = QStringLiteral("devops-admin@corp.internal");
  aws.password = QStringLiteral("K8s!Aws#Secure2026$Production");
  aws.websiteUrl = QStringLiteral("https://aws.amazon.com/console");
  aws.hasTotp = true;
  aws.totpSecret = QStringLiteral("HXDMVJECJJWSRB3H");
  aws.isFavorite = true;
  aws.updatedAt = QDateTime::currentDateTime().addDays(-5);

  models::UiCustomField awsField1;
  awsField1.id = QStringLiteral("cf-aws-1");
  awsField1.label = QStringLiteral("AWS Account ID");
  awsField1.value = QStringLiteral("8491-0294-8172");
  awsField1.kind = models::CustomFieldKind::Text;
  aws.customFields.append(awsField1);

  models::UiCustomField awsField2;
  awsField2.id = QStringLiteral("cf-aws-2");
  awsField2.label = QStringLiteral("IAM Role Name");
  awsField2.value = QStringLiteral("DevOpsAdministratorAccess");
  awsField2.kind = models::CustomFieldKind::Text;
  aws.customFields.append(awsField2);

  items.append(aws);

  // 3. SSH Key Production
  models::UiVaultItem ssh;
  ssh.id = QStringLiteral("item-003");
  ssh.title = QStringLiteral("id_ed25519 (Prod Bastion)");
  ssh.category = models::ItemCategory::SshKey;
  ssh.vaultId = QStringLiteral("work");
  ssh.username = QStringLiteral("root@bastion.infra.net");
  ssh.sshFingerprint =
      QStringLiteral("SHA256:7mP0e5K2q9N8w1X4v6Z3t7Y0u5W2r8O1p4Q7s");
  ssh.sshPublicKey = QStringLiteral(
      "ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIExampleProdKeyhephaestus "
      "root@bastion");
  ssh.password = QStringLiteral("Passphrase#SSH#2026");
  ssh.isSshLoaded = true;
  ssh.updatedAt = QDateTime::currentDateTime().addDays(-10);
  items.append(ssh);

  // 4. PostgreSQL Cluster
  models::UiVaultItem db;
  db.id = QStringLiteral("item-004");
  db.title = QStringLiteral("Postgres Cloud Cluster");
  db.category = models::ItemCategory::Database;
  db.vaultId = QStringLiteral("work");
  db.username = QStringLiteral("postgres_superadmin");
  db.password = QStringLiteral("PgSQL#SecurePass!Cluster2026");
  db.dbHost = QStringLiteral("pg-primary.prod.svc.cluster.local:5432");
  db.dbName = QStringLiteral("vault_core_db");
  db.updatedAt = QDateTime::currentDateTime().addDays(-1);
  items.append(db);

  // 5. Office Wi-Fi
  models::UiVaultItem wifi;
  wifi.id = QStringLiteral("item-005");
  wifi.title = QStringLiteral("Hephaestus Studio Wi-Fi");
  wifi.category = models::ItemCategory::Wifi;
  wifi.vaultId = QStringLiteral("work");
  wifi.wifiSsid = QStringLiteral("Hephaestus_5GHz_HighSec");
  wifi.password = QStringLiteral("IronForge#Wifi#2026");
  wifi.updatedAt = QDateTime::currentDateTime().addDays(-30);
  items.append(wifi);

  // 6. Corporate Credit Card
  models::UiVaultItem card;
  card.id = QStringLiteral("item-006");
  card.title = QStringLiteral("Corporate Visa Platinum");
  card.category = models::ItemCategory::Card;
  card.vaultId = QStringLiteral("personal");
  card.username = QStringLiteral("HAI PHAM NGOC");
  card.cardNumber = QStringLiteral("4111 2222 3333 4444");
  card.cardExpiry = QStringLiteral("12/28");
  card.cardCvv = QStringLiteral("888");
  card.updatedAt = QDateTime::currentDateTime().addDays(-15);
  items.append(card);

  m_itemListView->setItems(items);
  updateSidebarCounts();

  // Chọn mặc định item đầu tiên
  m_itemListView->selectItem(git.id);
  m_detailView->setItem(git);
}

void MainWindow::updateSidebarCounts() {
  const auto &items = m_itemListView->sourceModel()->items();
  const QString activeVId = app::VaultManager::instance().activeVaultId();

  QMap<views::SidebarFilterType, int> filterCounts;
  QMap<models::ItemCategory, int> categoryCounts;

  int allCount = 0;
  int favCount = 0;
  int trashCount = 0;
  int weakCount = 0;

  for (const auto &item : items) {
    if (!activeVId.isEmpty() && activeVId != QStringLiteral("all")) {
      if (item.vaultId != activeVId) {
        continue;
      }
    }
    if (item.isDeleted) {
      ++trashCount;
    } else {
      ++allCount;
      categoryCounts[item.category] =
          categoryCounts.value(item.category, 0) + 1;

      if (item.isFavorite) {
        ++favCount;
      }
      if (item.passwordStrengthScore < 60) {
        ++weakCount;
      }
    }
  }

  filterCounts[views::SidebarFilterType::AllItems] = allCount;
  filterCounts[views::SidebarFilterType::Favorites] = favCount;
  filterCounts[views::SidebarFilterType::Trash] = trashCount;
  filterCounts[views::SidebarFilterType::WatchtowerWeak] = weakCount;
  filterCounts[views::SidebarFilterType::WatchtowerExpiring] =
      (allCount > 0) ? 1 : 0;

  m_sidebarView->updateCounts(filterCounts, categoryCounts);
}

void MainWindow::updateNavigationState() {
  if (!app::VaultManager::instance().isConfigured()) {
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
    if (m_headerBar) {
      m_headerBar->updateMaximizeIcon(isMaximized());
    }
  } else if (event->type() == QEvent::LanguageChange) {
    retranslateUi();
  }
  QMainWindow::changeEvent(event);
}

void MainWindow::retranslateUi() {
  setWindowTitle(tr("Secure Vault Manager"));
  if (m_menuBar) {
    m_menuBar->retranslateUi();
  }
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

void MainWindow::closeEvent(QCloseEvent *event) {
  if (m_trayManager && m_trayManager->isSystemTrayAvailable()) {
    hide();
    event->ignore();
  } else {
    event->accept();
  }
}

} // namespace svm
