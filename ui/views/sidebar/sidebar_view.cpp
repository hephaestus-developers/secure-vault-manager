/**
 * @file ui/views/sidebar/sidebar_view.cpp
 * @brief Hiện thực SidebarView điều hướng phân cấp chuẩn Modern Desktop không
 * bị vỡ layout.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "sidebar_view.h"
#include "app/theme_manager.h"
#include "app/vault_manager.h"
#include "components/dialog_drag_helper.h"
#include "components/in_window_combobox.h"
#include "views/vaults/new_vault_dialog.h"

#include <QAction>
#include <QActionGroup>
#include <QEvent>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QVBoxLayout>

namespace svm::views {

SidebarView::SidebarView(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("SidebarView"));
  setAttribute(Qt::WA_StyledBackground, true);
  setMinimumWidth(200);

  setupUI();
  populateNavItems();
}

void SidebarView::setupUI() {
  auto *rootLayout = new QVBoxLayout(this);
  rootLayout->setContentsMargins(0, 0, 0, 0);
  rootLayout->setSpacing(0);

  m_stackedWidget = new QStackedWidget(this);
  m_stackedWidget->setObjectName(QStringLiteral("SidebarStackedWidget"));
  m_stackedWidget->setAttribute(Qt::WA_StyledBackground, true);

  setupExpandedUI();
  setupCollapsedUI();

  m_stackedWidget->addWidget(m_expandedWidget);  // Index 0: Mở rộng
  m_stackedWidget->addWidget(m_collapsedWidget); // Index 1: Thu gọn

  connect(&app::ThemeManager::instance(), &app::ThemeManager::themeChanged,
          this, [this]() {
            updateThemeIcons();
            if (m_navList && m_navList->viewport()) {
              m_navList->viewport()->update();
            }
            if (m_compactList && m_compactList->viewport()) {
              m_compactList->viewport()->update();
            }
          });

  rootLayout->addWidget(m_stackedWidget);
}

void SidebarView::setupExpandedUI() {
  m_expandedWidget = new QWidget(m_stackedWidget);
  m_expandedWidget->setObjectName(QStringLiteral("SidebarExpandedContainer"));
  m_expandedWidget->setAttribute(Qt::WA_StyledBackground, true);

  auto *layout = new QVBoxLayout(m_expandedWidget);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  // 1. Header: Vault Profile Button + Collapse Toggle Button
  auto *headerWidget = new QWidget(m_expandedWidget);
  headerWidget->setObjectName(QStringLiteral("SidebarHeaderWidget"));
  auto *headerLayout = new QHBoxLayout(headerWidget);
  headerLayout->setContentsMargins(10, 10, 10, 8);
  headerLayout->setSpacing(6);

  // Vault Profile ComboBox dạng In-Window Child Overlay (không sử dụng OS
  // Popup)
  m_vaultComboBox = new ui::InWindowComboBox(headerWidget);
  m_vaultComboBox->setObjectName(QStringLiteral("VaultProfileComboBox"));
  m_vaultComboBox->setFixedHeight(32);
  m_vaultComboBox->setIconSize(QSize(16, 16));
  m_vaultComboBox->setCursor(Qt::PointingHandCursor);
  m_vaultComboBox->setPopupMinWidth(180);

  populateVaultCombo();

  connect(&app::VaultManager::instance(), &app::VaultManager::vaultListChanged,
          this, &SidebarView::populateVaultCombo);

  connect(m_vaultComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this](int index) {
            if (index >= 0 && index < m_vaultComboBox->count()) {
              const QString vaultId =
                  m_vaultComboBox->itemData(index).toString();
              if (!vaultId.isEmpty() &&
                  vaultId != app::VaultManager::instance().activeVaultId()) {
                app::VaultManager::instance().setActiveVaultId(vaultId);
              }
            }
          });

  connect(
      &app::VaultManager::instance(), &app::VaultManager::activeVaultChanged,
      this, [this](const QString &vId) {
        const auto v = app::VaultManager::instance().vault(vId);
        m_currentVaultName = v.name.isEmpty() ? tr("Personal Vault") : v.name;
        m_collapsedVaultBtn->setToolTip(m_currentVaultName);
        const QString iconName = v.iconName.isEmpty()
                                     ? QStringLiteral("vault_personal")
                                     : v.iconName;
        if (m_collapsedVaultBtn) {
          m_collapsedVaultBtn->setIcon(
              app::ThemeManager::instance().icon(iconName));
        }
        for (int i = 0; i < m_vaultComboBox->count(); ++i) {
          if (m_vaultComboBox->itemData(i).toString() == vId) {
            if (m_vaultComboBox->currentIndex() != i) {
              m_vaultComboBox->blockSignals(true);
              m_vaultComboBox->setCurrentIndex(i);
              m_vaultComboBox->blockSignals(false);
            }
            break;
          }
        }
        emit vaultChanged(m_currentVaultName);
      });

  headerLayout->addWidget(m_vaultComboBox, 1);

  // Nút tạo mới Vault (Dấu cộng cạnh Vault Profile)
  m_newVaultBtn = new QPushButton(headerWidget);
  m_newVaultBtn->setObjectName(QStringLiteral("SidebarNewVaultBtn"));
  m_newVaultBtn->setIcon(
      app::ThemeManager::instance().icon(QStringLiteral("plus")));
  m_newVaultBtn->setIconSize(QSize(16, 16));
  m_newVaultBtn->setFixedSize(32, 32);
  m_newVaultBtn->setToolTip(tr("Create New Vault..."));
  m_newVaultBtn->setCursor(Qt::PointingHandCursor);
  connect(m_newVaultBtn, &QPushButton::clicked, this, [this]() {
    NewVaultDialog dlg(this);
    components::DialogDragHelper::execDialog(dlg);
  });
  headerLayout->addWidget(m_newVaultBtn, 0, Qt::AlignVCenter);

  layout->addWidget(headerWidget);

  // 2. Navigation List với Custom Delegate liền mạch
  m_navList = new QListWidget(m_expandedWidget);
  m_navList->setObjectName(QStringLiteral("SidebarNavList"));
  m_navList->setFrameShape(QFrame::NoFrame);
  m_navList->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  m_navList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_navList->setSelectionMode(QAbstractItemView::SingleSelection);
  m_navList->setSpacing(1);

  m_itemDelegate = new SidebarItemDelegate(this);
  m_navList->setItemDelegate(m_itemDelegate);

  connect(m_navList, &QListWidget::itemClicked, this,
          &SidebarView::onListItemClicked);
  layout->addWidget(m_navList, 1);

  // 3. Footer: Lock Vault Button
  auto *footerWidget = new QWidget(m_expandedWidget);
  footerWidget->setObjectName(QStringLiteral("SidebarFooterWidget"));
  auto *footerLayout = new QHBoxLayout(footerWidget);
  footerLayout->setContentsMargins(10, 8, 10, 10);
  footerLayout->setSpacing(6);

  m_lockBtn = new QPushButton(footerWidget);
  m_lockBtn->setObjectName(QStringLiteral("SidebarLockBtn"));
  m_lockBtn->setCursor(Qt::PointingHandCursor);
  m_lockBtn->setToolTip(tr("Lock vault immediately (Ctrl+L)"));

  auto *lockBtnLayout = new QHBoxLayout(m_lockBtn);
  lockBtnLayout->setContentsMargins(11, 0, 11, 0);
  lockBtnLayout->setSpacing(10);

  m_lockIconLabel = new QLabel(m_lockBtn);
  m_lockIconLabel->setFixedSize(18, 18);
  m_lockIconLabel->setPixmap(
      QIcon(QStringLiteral(":/icons/security/lock.svg")).pixmap(18, 18));
  lockBtnLayout->addWidget(m_lockIconLabel);

  m_lockTextLabel = new QLabel(tr("Lock Vault"), m_lockBtn);
  m_lockTextLabel->setObjectName(QStringLiteral("LockBtnText"));
  lockBtnLayout->addWidget(m_lockTextLabel, 1);

  connect(m_lockBtn, &QPushButton::clicked, this,
          &SidebarView::lockVaultRequested);
  footerLayout->addWidget(m_lockBtn, 1);

  m_collapseBtn = new QPushButton(footerWidget);
  m_collapseBtn->setObjectName(QStringLiteral("SidebarCollapseBtn"));
  m_collapseBtn->setFixedSize(32, 32);
  m_collapseBtn->setIconSize(QSize(16, 16));
  m_collapseBtn->setIcon(
      app::ThemeManager::instance().icon(QStringLiteral("sidebar_collapse")));
  m_collapseBtn->setToolTip(tr("Collapse Sidebar (Ctrl+B)"));
  m_collapseBtn->setCursor(Qt::PointingHandCursor);
  connect(m_collapseBtn, &QPushButton::clicked, this,
          &SidebarView::toggleCollapsed);
  footerLayout->addWidget(m_collapseBtn, 0);

  layout->addWidget(footerWidget);
}

void SidebarView::setupCollapsedUI() {
  m_collapsedWidget = new QWidget(m_stackedWidget);
  m_collapsedWidget->setObjectName(QStringLiteral("SidebarCollapsedContainer"));
  m_collapsedWidget->setAttribute(Qt::WA_StyledBackground, true);

  auto *layout = new QVBoxLayout(m_collapsedWidget);
  layout->setContentsMargins(0, 12, 0, 10);
  layout->setSpacing(6);
  layout->setAlignment(Qt::AlignHCenter);

  // 1. Vault Icon Button (36x36, icon 18x18)
  m_collapsedVaultBtn = new QPushButton(m_collapsedWidget);
  m_collapsedVaultBtn->setObjectName(QStringLiteral("SidebarCompactVaultBtn"));
  const auto activeV = app::VaultManager::instance().activeVault();
  const QString vaultIcon = activeV.iconName.isEmpty()
                                ? QStringLiteral("vault_personal")
                                : activeV.iconName;
  m_collapsedVaultBtn->setIcon(app::ThemeManager::instance().icon(vaultIcon));
  m_collapsedVaultBtn->setIconSize(QSize(18, 18));
  m_collapsedVaultBtn->setFixedSize(36, 36);
  m_collapsedVaultBtn->setToolTip(m_currentVaultName);
  m_collapsedVaultBtn->setCursor(Qt::PointingHandCursor);
  connect(m_collapsedVaultBtn, &QPushButton::clicked, this,
          &SidebarView::toggleCollapsed);
  layout->addWidget(m_collapsedVaultBtn, 0, Qt::AlignHCenter);

  // 2. Compact Navigation Icon List
  m_compactList = new QListWidget(m_collapsedWidget);
  m_compactList->setObjectName(QStringLiteral("SidebarCompactList"));
  m_compactList->setFixedWidth(60);
  m_compactList->setFrameShape(QFrame::NoFrame);
  m_compactList->setUniformItemSizes(false);
  m_compactList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_compactList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  m_compactDelegate = new CompactSidebarDelegate(this);
  m_compactList->setItemDelegate(m_compactDelegate);

  connect(m_compactList, &QListWidget::itemClicked, this,
          &SidebarView::onCompactItemClicked);
  layout->addWidget(m_compactList, 1, Qt::AlignHCenter);

  // 3. Bottom Lock & Expand Toggle Buttons (36x36, icon 18x18)
  m_collapsedLockBtn = new QPushButton(m_collapsedWidget);
  m_collapsedLockBtn->setObjectName(QStringLiteral("SidebarCompactLockBtn"));
  m_collapsedLockBtn->setIcon(
      app::ThemeManager::instance().icon(QStringLiteral("lock")));
  m_collapsedLockBtn->setIconSize(QSize(18, 18));
  m_collapsedLockBtn->setFixedSize(36, 36);
  m_collapsedLockBtn->setToolTip(tr("Lock Vault (Ctrl+L)"));
  m_collapsedLockBtn->setCursor(Qt::PointingHandCursor);
  connect(m_collapsedLockBtn, &QPushButton::clicked, this,
          &SidebarView::lockVaultRequested);
  layout->addWidget(m_collapsedLockBtn, 0, Qt::AlignHCenter);

  m_expandBtn = new QPushButton(m_collapsedWidget);
  m_expandBtn->setObjectName(QStringLiteral("SidebarToggleBtn"));
  m_expandBtn->setIcon(
      app::ThemeManager::instance().icon(QStringLiteral("sidebar_expand")));
  m_expandBtn->setIconSize(QSize(18, 18));
  m_expandBtn->setFixedSize(36, 36);
  m_expandBtn->setToolTip(tr("Expand Sidebar (Ctrl+B)"));
  m_expandBtn->setCursor(Qt::PointingHandCursor);
  connect(m_expandBtn, &QPushButton::clicked, this,
          &SidebarView::toggleCollapsed);
  layout->addWidget(m_expandBtn, 0, Qt::AlignHCenter);
}

void SidebarView::populateNavItems() {
  m_navList->clear();
  m_compactList->clear();
  m_compactFilterItems.clear();
  m_compactCategoryItems.clear();
  m_categoryItems.clear();

  auto addHeader = [this](const QString &title) -> QListWidgetItem * {
    auto *header = new QListWidgetItem(title, m_navList);
    header->setFlags(Qt::NoItemFlags);
    header->setData(SidebarItemDelegate::IsHeaderRole, true);

    auto *compactDivider = new QListWidgetItem(m_compactList);
    compactDivider->setFlags(Qt::NoItemFlags);
    compactDivider->setData(SidebarItemDelegate::IsHeaderRole, true);
    compactDivider->setSizeHint(QSize(60, 16));
    return header;
  };

  auto addNavItem = [this](const QIcon &icon, const QString &text,
                           SidebarFilterType type,
                           int categoryInt = -1) -> QListWidgetItem * {
    auto *item = new QListWidgetItem(icon, text, m_navList);
    item->setData(Qt::UserRole, static_cast<int>(type));
    item->setData(SidebarItemDelegate::CountRole, 0);
    item->setData(SidebarItemDelegate::IsHeaderRole, false);
    if (categoryInt >= 0) {
      item->setData(Qt::UserRole + 1, categoryInt);
    }
    return item;
  };

  auto addCompactItem = [this](const QIcon &icon, const QString &tooltip,
                               SidebarFilterType filterType,
                               int categoryInt = -1) -> QListWidgetItem * {
    auto *item = new QListWidgetItem(icon, QString(), m_compactList);
    item->setToolTip(tooltip);
    item->setSizeHint(QSize(60, 38));
    item->setData(Qt::UserRole, static_cast<int>(filterType));
    if (categoryInt >= 0) {
      item->setData(Qt::UserRole + 1, categoryInt);
    }
    return item;
  };

  // 1. Nhóm bộ lọc nhanh
  m_allItem =
      addNavItem(QIcon(QStringLiteral(":/icons/categories/category_all.svg")),
                 tr("All Items"), SidebarFilterType::AllItems);
  m_compactFilterItems[SidebarFilterType::AllItems] = addCompactItem(
      QIcon(QStringLiteral(":/icons/categories/category_all.svg")),
      tr("All Items (0)"), SidebarFilterType::AllItems);

  m_favItem =
      addNavItem(QIcon(QStringLiteral(":/icons/actions/star_filled.svg")),
                 tr("Favorites"), SidebarFilterType::Favorites);
  m_compactFilterItems[SidebarFilterType::Favorites] =
      addCompactItem(QIcon(QStringLiteral(":/icons/actions/star_filled.svg")),
                     tr("Favorites (0)"), SidebarFilterType::Favorites);

  m_trashItem =
      addNavItem(QIcon(QStringLiteral(":/icons/categories/category_trash.svg")),
                 tr("Trash"), SidebarFilterType::Trash);
  m_compactFilterItems[SidebarFilterType::Trash] = addCompactItem(
      QIcon(QStringLiteral(":/icons/categories/category_trash.svg")),
      tr("Trash (0)"), SidebarFilterType::Trash);

  // 2. Nhóm 9 Danh Mục Nghiệp Vụ
  m_categoriesHeaderItem = addHeader(tr("CATEGORIES"));

  const std::array<models::ItemCategory, 9> categories = {
      models::ItemCategory::Login,      models::ItemCategory::Passkey,
      models::ItemCategory::Card,       models::ItemCategory::Identity,
      models::ItemCategory::SecureNote, models::ItemCategory::SshKey,
      models::ItemCategory::Database,   models::ItemCategory::Server,
      models::ItemCategory::Wifi};

  for (const auto cat : categories) {
    const QString name = models::categoryDisplayName(cat);
    const QIcon icon(models::categoryIconPath(cat));

    auto *item = addNavItem(icon, name, SidebarFilterType::Category,
                            static_cast<int>(cat));
    m_categoryItems.insert(cat, item);

    auto *compactItem =
        addCompactItem(icon, QStringLiteral("%1 (0)").arg(name),
                       SidebarFilterType::Category, static_cast<int>(cat));
    m_compactCategoryItems.insert(cat, compactItem);
  }

  // Chọn mặc định All Items
  m_navList->setCurrentItem(m_allItem);
  if (auto *allCompact =
          m_compactFilterItems.value(SidebarFilterType::AllItems, nullptr)) {
    m_compactList->setCurrentItem(allCompact);
  }

  updateThemeIcons();
}

void SidebarView::updateThemeIcons() {
  const bool isLight =
      (app::ThemeManager::instance().currentMode() == app::ThemeMode::Light);
  auto &tm = app::ThemeManager::instance();

  const auto activeV = app::VaultManager::instance().activeVault();
  const QString vaultIcon = activeV.iconName.isEmpty()
                                ? QStringLiteral("vault_personal")
                                : activeV.iconName;
  if (m_collapsedVaultBtn) {
    m_collapsedVaultBtn->setIcon(tm.icon(vaultIcon));
  }
  if (m_vaultComboBox) {
    for (int i = 0; i < m_vaultComboBox->count(); ++i) {
      const QString vId = m_vaultComboBox->itemData(i).toString();
      const auto v = app::VaultManager::instance().vault(vId);
      const QString iconName =
          v.iconName.isEmpty() ? QStringLiteral("vault_personal") : v.iconName;
      m_vaultComboBox->setItemIcon(i, tm.icon(iconName));
    }
  }
  if (m_lockIconLabel) {
    m_lockIconLabel->setPixmap(tm.icon(QStringLiteral("lock")).pixmap(18, 18));
  }
  if (m_collapsedLockBtn) {
    m_collapsedLockBtn->setIcon(tm.icon(QStringLiteral("lock")));
  }
  if (m_newVaultBtn) {
    m_newVaultBtn->setIcon(tm.icon(QStringLiteral("plus")));
  }
  if (m_collapseBtn) {
    m_collapseBtn->setIcon(tm.icon(QStringLiteral("sidebar_collapse")));
  }
  if (m_expandBtn) {
    m_expandBtn->setIcon(tm.icon(QStringLiteral("sidebar_expand")));
  }

  const QIcon allIcon = tm.icon(QStringLiteral("category_all"));
  if (m_allItem) {
    m_allItem->setIcon(allIcon);
  }
  if (auto *ci =
          m_compactFilterItems.value(SidebarFilterType::AllItems, nullptr)) {
    ci->setIcon(allIcon);
  }

  const QIcon favIcon = tm.icon(QStringLiteral("star_filled"));
  if (m_favItem) {
    m_favItem->setIcon(favIcon);
  }
  if (auto *ci =
          m_compactFilterItems.value(SidebarFilterType::Favorites, nullptr)) {
    ci->setIcon(favIcon);
  }

  const QIcon trashIcon = tm.icon(QStringLiteral("category_trash"));
  if (m_trashItem) {
    m_trashItem->setIcon(trashIcon);
  }
  if (auto *ci =
          m_compactFilterItems.value(SidebarFilterType::Trash, nullptr)) {
    ci->setIcon(trashIcon);
  }

  for (auto it = m_categoryItems.begin(); it != m_categoryItems.end(); ++it) {
    const QIcon catIcon = tm.icon(models::categoryIconName(it.key()));
    it.value()->setIcon(catIcon);
    if (auto *ci = m_compactCategoryItems.value(it.key(), nullptr)) {
      ci->setIcon(catIcon);
    }
  }
}

void SidebarView::setCollapsed(bool collapsed) {
  if (m_isCollapsed == collapsed) {
    return;
  }
  m_isCollapsed = collapsed;

  if (m_isCollapsed) {
    setFixedWidth(60);
    m_expandBtn->setIcon(
        app::ThemeManager::instance().icon(QStringLiteral("sidebar_expand")));
    m_stackedWidget->setCurrentIndex(1);
  } else {
    setMinimumWidth(200);
    setMaximumWidth(QWIDGETSIZE_MAX);
    m_collapseBtn->setIcon(
        app::ThemeManager::instance().icon(QStringLiteral("sidebar_collapse")));
    m_stackedWidget->setCurrentIndex(0);
  }

  emit collapsedChanged(m_isCollapsed);
}

void SidebarView::toggleCollapsed() { setCollapsed(!m_isCollapsed); }

void SidebarView::onListItemClicked(QListWidgetItem *item) {
  if (!item || item->data(SidebarItemDelegate::IsHeaderRole).toBool()) {
    return;
  }

  const auto filterType =
      static_cast<SidebarFilterType>(item->data(Qt::UserRole).toInt());
  models::ItemCategory category = models::ItemCategory::Login;

  if (filterType == SidebarFilterType::Category) {
    category =
        static_cast<models::ItemCategory>(item->data(Qt::UserRole + 1).toInt());
  }

  syncSelectionToCompact(filterType, category);
  emit filterSelected(filterType, category);
}

void SidebarView::onCompactItemClicked(QListWidgetItem *item) {
  if (!item || item->data(SidebarItemDelegate::IsHeaderRole).toBool()) {
    return;
  }

  const auto filterType =
      static_cast<SidebarFilterType>(item->data(Qt::UserRole).toInt());
  models::ItemCategory category = models::ItemCategory::Login;

  if (filterType == SidebarFilterType::Category) {
    category =
        static_cast<models::ItemCategory>(item->data(Qt::UserRole + 1).toInt());
  }

  syncSelectionToList(filterType, category);
  emit filterSelected(filterType, category);
}

void SidebarView::populateVaultCombo() {
  if (!m_vaultComboBox) {
    return;
  }
  m_vaultComboBox->blockSignals(true);
  m_vaultComboBox->clear();

  const auto vaults = app::VaultManager::instance().vaults();
  const QString activeId = app::VaultManager::instance().activeVaultId();
  int activeIndex = 0;

  for (int i = 0; i < vaults.size(); ++i) {
    const auto &v = vaults[i];
    const QString iconName =
        v.iconName.isEmpty() ? QStringLiteral("vault_personal") : v.iconName;
    m_vaultComboBox->addItem(app::ThemeManager::instance().icon(iconName),
                             v.name, v.id);
    if (v.id == activeId) {
      activeIndex = i;
    }
  }

  if (!vaults.isEmpty()) {
    m_vaultComboBox->setCurrentIndex(activeIndex);
    m_currentVaultName = m_vaultComboBox->currentText();
  }

  m_vaultComboBox->blockSignals(false);
}

void SidebarView::syncSelectionToCompact(SidebarFilterType type,
                                         models::ItemCategory category) {
  if (type == SidebarFilterType::Category) {
    if (auto *compactItem = m_compactCategoryItems.value(category, nullptr)) {
      m_compactList->setCurrentItem(compactItem);
    }
  } else {
    if (auto *compactItem = m_compactFilterItems.value(type, nullptr)) {
      m_compactList->setCurrentItem(compactItem);
    }
  }
}

void SidebarView::syncSelectionToList(SidebarFilterType type,
                                      models::ItemCategory category) {
  if (type == SidebarFilterType::Category) {
    if (auto *listItem = m_categoryItems.value(category, nullptr)) {
      m_navList->setCurrentItem(listItem);
    }
  } else {
    switch (type) {
    case SidebarFilterType::AllItems:
      m_navList->setCurrentItem(m_allItem);
      break;
    case SidebarFilterType::Favorites:
      m_navList->setCurrentItem(m_favItem);
      break;
    case SidebarFilterType::Trash:
      m_navList->setCurrentItem(m_trashItem);
      break;
    default:
      break;
    }
  }
}

void SidebarView::updateCounts(
    const QMap<SidebarFilterType, int> &filterCounts,
    const QMap<models::ItemCategory, int> &categoryCounts) {
  m_lastFilterCounts = filterCounts;
  m_lastCategoryCounts = categoryCounts;

  auto updateCount = [](QListWidgetItem *item, int count) {
    if (item) {
      item->setData(SidebarItemDelegate::CountRole, count);
    }
  };

  const int allCount = filterCounts.value(SidebarFilterType::AllItems, 0);
  const int favCount = filterCounts.value(SidebarFilterType::Favorites, 0);
  const int trashCount = filterCounts.value(SidebarFilterType::Trash, 0);

  updateCount(m_allItem, allCount);
  updateCount(m_favItem, favCount);
  updateCount(m_trashItem, trashCount);

  // Cập nhật Compact Tooltips
  if (auto *it =
          m_compactFilterItems.value(SidebarFilterType::AllItems, nullptr)) {
    it->setToolTip(tr("All Items (%1)").arg(allCount));
  }
  if (auto *it =
          m_compactFilterItems.value(SidebarFilterType::Favorites, nullptr)) {
    it->setToolTip(tr("Favorites (%1)").arg(favCount));
  }
  if (auto *it =
          m_compactFilterItems.value(SidebarFilterType::Trash, nullptr)) {
    it->setToolTip(tr("Trash (%1)").arg(trashCount));
  }

  // Cập nhật Danh mục: Duyệt toàn bộ m_categoryItems để đảm bảo danh mục không
  // có item luôn reset về 0
  for (auto it = m_categoryItems.begin(); it != m_categoryItems.end(); ++it) {
    const int count = categoryCounts.value(it.key(), 0);
    if (it.value()) {
      it.value()->setData(SidebarItemDelegate::CountRole, count);
    }
    if (auto *compactItem = m_compactCategoryItems.value(it.key(), nullptr)) {
      compactItem->setToolTip(
          tr("%1 (%2)").arg(models::categoryDisplayName(it.key())).arg(count));
    }
  }

  m_navList->viewport()->update();
}

void SidebarView::retranslateUi() {
  populateVaultCombo();
  if (m_newVaultBtn) {
    m_newVaultBtn->setToolTip(tr("Create New Vault..."));
  }
  if (m_collapseBtn) {
    m_collapseBtn->setToolTip(tr("Collapse Sidebar (Ctrl+B)"));
  }
  if (m_expandBtn) {
    m_expandBtn->setToolTip(tr("Expand Sidebar (Ctrl+B)"));
  }
  if (m_lockBtn) {
    m_lockBtn->setToolTip(tr("Lock vault immediately (Ctrl+L)"));
  }
  if (m_collapsedLockBtn) {
    m_collapsedLockBtn->setToolTip(tr("Lock Vault (Ctrl+L)"));
  }
  if (m_lockTextLabel) {
    m_lockTextLabel->setText(tr("Lock Vault"));
  }

  if (m_allItem) {
    m_allItem->setText(tr("All Items"));
  }
  if (m_favItem) {
    m_favItem->setText(tr("Favorites"));
  }
  if (m_trashItem) {
    m_trashItem->setText(tr("Trash"));
  }

  if (m_categoriesHeaderItem) {
    m_categoriesHeaderItem->setText(tr("CATEGORIES"));
  }

  for (auto it = m_categoryItems.begin(); it != m_categoryItems.end(); ++it) {
    if (it.value()) {
      it.value()->setText(models::categoryDisplayName(it.key()));
    }
  }

  updateCounts(m_lastFilterCounts, m_lastCategoryCounts);
}

void SidebarView::changeEvent(QEvent *event) {
  if (event->type() == QEvent::LanguageChange) {
    retranslateUi();
  }
  QWidget::changeEvent(event);
}

} // namespace svm::views
