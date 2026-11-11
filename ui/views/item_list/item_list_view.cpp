/**
 * @file ui/views/item_list/item_list_view.cpp
 * @brief Hiện thực ItemListView hiển thị danh sách thẻ bản ghi.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "item_list_view.h"
#include "app/dev_logger.h"
#include "app/settings_manager.h"
#include "app/theme_manager.h"
#include "app/vault_manager.h"
#include "core/core_bridge.h"

#include <QAction>
#include <QContextMenuEvent>
#include <QDesktopServices>
#include <QEvent>
#include <QHBoxLayout>
#include <QIcon>
#include <QMenu>
#include <QMouseEvent>
#include <QUrl>
#include <QVBoxLayout>

namespace svm::views {

ItemListView::ItemListView(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("ItemListView"));
  setAttribute(Qt::WA_StyledBackground, true);
  setMinimumWidth(280);

  setupUI();
}

void ItemListView::setupUI() {
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(12, 12, 12, 10);
  layout->setSpacing(8);

  // 1. Row 1: Search Bar (flex 1) + Primary "+" Button (32x32)
  auto *topBarContainer = new QWidget(this);
  auto *topBarLayout = new QHBoxLayout(topBarContainer);
  topBarLayout->setContentsMargins(0, 0, 0, 0);
  topBarLayout->setSpacing(6);

  m_searchEdit = new QLineEdit(topBarContainer);
  m_searchEdit->setObjectName(QStringLiteral("ItemListSearchEdit"));
  m_searchEdit->setPlaceholderText(tr("Search vault... (Ctrl+F)"));
  m_searchEdit->setClearButtonEnabled(true);
  m_searchEdit->setFixedHeight(30);
  auto *searchAction = m_searchEdit->addAction(
      app::ThemeManager::instance().icon(QStringLiteral("search")),
      QLineEdit::LeadingPosition);
  connect(m_searchEdit, &QLineEdit::textChanged, this,
          &ItemListView::onSearchTextChanged);
  topBarLayout->addWidget(m_searchEdit, 1);

  m_newBtn = new QPushButton(topBarContainer);
  m_newBtn->setObjectName(QStringLiteral("ItemListNewBtn"));
  m_newBtn->setIconSize(QSize(16, 16));
  m_newBtn->setFixedSize(30, 30);
  m_newBtn->setToolTip(tr("Create new item (Ctrl+N)"));
  m_newBtn->setCursor(Qt::PointingHandCursor);
  connect(m_newBtn, &QPushButton::clicked, this,
          [this]() { emit addNewItemRequested(m_currentCategory); });
  topBarLayout->addWidget(m_newBtn, 0);

  layout->addWidget(topBarContainer);

  // 2. QListView với Proxy Model & Delegate
  m_model = new ItemListModel(this);
  m_proxyModel = new ItemSortFilterProxy(this);
  m_proxyModel->setSourceModel(m_model);
  m_proxyModel->setVaultFilter(app::VaultManager::instance().activeVaultId());

  connect(&app::VaultManager::instance(),
          &app::VaultManager::activeVaultChanged, this,
          [this](const QString &vaultId) {
            m_proxyModel->setVaultFilter(vaultId);
          });

  m_listView = new QListView(this);
  m_listView->setObjectName(QStringLiteral("ItemCardListView"));
  m_listView->setModel(m_proxyModel);
  m_listView->setUniformItemSizes(true);
  m_listView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
  m_listView->setMouseTracking(true);
  m_listView->setSelectionMode(QAbstractItemView::SingleSelection);
  m_listView->setContextMenuPolicy(Qt::CustomContextMenu);
  m_listView->installEventFilter(this);
  if (m_listView->viewport()) {
    m_listView->viewport()->installEventFilter(this);
  }

  m_delegate = new ItemCardDelegate(this);
  m_listView->setItemDelegate(m_delegate);

  connect(&app::SettingsManager::instance(),
          &app::SettingsManager::listDensityChanged, this, [this]() {
            if (m_listView) {
              m_listView->doItemsLayout();
              if (m_listView->viewport()) {
                m_listView->viewport()->update();
              }
            }
          });
  connect(&core::CoreBridge::instance(), &core::CoreBridge::faviconUpdated,
          this, [this](const QString &) {
            if (m_listView && m_listView->viewport()) {
              m_listView->viewport()->update();
            }
          });

  auto updateNewBtnIcon = [this]() {
    const bool isLight =
        (app::ThemeManager::instance().currentMode() == app::ThemeMode::Light);
    m_newBtn->setIcon(
        QIcon(isLight ? QStringLiteral(":/icons/actions/plus_light.svg")
                      : QStringLiteral(":/icons/actions/plus_dark.svg")));
  };
  updateNewBtnIcon();

  connect(&app::ThemeManager::instance(), &app::ThemeManager::themeChanged,
          this, [this, searchAction, updateNewBtnIcon]() {
            updateNewBtnIcon();
            searchAction->setIcon(
                app::ThemeManager::instance().icon(QStringLiteral("search")));
            if (m_listView && m_listView->viewport()) {
              m_listView->viewport()->update();
            }
          });

  connect(m_listView->selectionModel(), &QItemSelectionModel::selectionChanged,
          this, &ItemListView::onListSelectionChanged);
  connect(m_listView, &QListView::customContextMenuRequested, this,
          &ItemListView::onContextMenuRequested);

  m_stack = new QStackedWidget(this);
  m_stack->setObjectName(QStringLiteral("ItemListStack"));
  m_stack->addWidget(m_listView);

  m_emptyWidget = new ItemListEmptyWidget(m_stack);
  connect(m_emptyWidget, &ItemListEmptyWidget::clearSearchRequested,
          m_searchEdit, &QLineEdit::clear);
  connect(m_emptyWidget, &ItemListEmptyWidget::createItemRequested, this,
          &ItemListView::addNewItemRequested);
  m_stack->addWidget(m_emptyWidget);

  layout->addWidget(m_stack, 1);

  connect(m_proxyModel, &QAbstractItemModel::modelReset, this,
          &ItemListView::updateEmptyStateVisibility);
  connect(m_proxyModel, &QAbstractItemModel::rowsInserted, this,
          &ItemListView::updateEmptyStateVisibility);
  connect(m_proxyModel, &QAbstractItemModel::rowsRemoved, this,
          &ItemListView::updateEmptyStateVisibility);
  connect(m_proxyModel, &QAbstractItemModel::layoutChanged, this,
          &ItemListView::updateEmptyStateVisibility);

  updateEmptyStateVisibility();
}

void ItemListView::setItems(const QList<models::UiVaultItem> &items) {
  m_proxyModel->setVaultFilter(app::VaultManager::instance().activeVaultId());
  m_model->setItems(items);
}

void ItemListView::upsertItem(const models::UiVaultItem &item) {
  m_model->upsertItem(item);
}

void ItemListView::removeItem(const QString &itemId) {
  m_model->removeItem(itemId);
}

void ItemListView::setFilter(SidebarFilterType type,
                             models::ItemCategory category) {
  SVM_ACTION_SCOPE_DETAIL("UI", "ItemListView::setFilter",
                          QStringLiteral("type=%1, cat=%2")
                              .arg(static_cast<int>(type))
                              .arg(static_cast<int>(category)));
  m_currentFilterType = type;
  m_currentCategory = category;
  m_proxyModel->setFilterType(type, category);
  updateEmptyStateVisibility();
}

QModelIndex ItemListView::findProxyIndexById(const QString &itemId) const {
  if (itemId.isEmpty() || !m_proxyModel) {
    return {};
  }
  for (int row = 0; row < m_proxyModel->rowCount(); ++row) {
    const QModelIndex idx = m_proxyModel->index(row, 0);
    if (idx.data(ItemListModel::IdRole).toString() == itemId) {
      return idx;
    }
  }
  return {};
}

void ItemListView::selectItem(const QString &itemId) {
  const QModelIndex idx = findProxyIndexById(itemId);
  if (idx.isValid()) {
    m_listView->setCurrentIndex(idx);
    m_listView->scrollTo(idx);
  }
}

bool ItemListView::isItemVisible(const QString &itemId) const {
  return findProxyIndexById(itemId).isValid();
}

void ItemListView::clearSelection() {
  m_listView->clearSelection();
  m_listView->setCurrentIndex(QModelIndex());
}

QString ItemListView::selectedItemId() const {
  const auto idx = m_listView->currentIndex();
  if (idx.isValid()) {
    return idx.data(ItemListModel::IdRole).toString();
  }
  return QString();
}

void ItemListView::focusSearch() {
  m_searchEdit->setFocus();
  m_searchEdit->selectAll();
}

void ItemListView::onSearchTextChanged(const QString &text) {
  SVM_ACTION_SCOPE_DETAIL("UI", "ItemListView::search",
                          QStringLiteral("query=\"%1\"").arg(text));
  m_proxyModel->setSearchText(text);
  updateEmptyStateVisibility();
}

void ItemListView::updateEmptyStateVisibility() {
  if (!m_stack || !m_emptyWidget || !m_proxyModel) {
    return;
  }
  const bool isEmpty = (m_proxyModel->rowCount() == 0);
  m_stack->setCurrentIndex(isEmpty ? 1 : 0);
  if (isEmpty) {
    const QString q = m_searchEdit ? m_searchEdit->text().trimmed() : QString();
    m_emptyWidget->setState(m_currentFilterType, m_currentCategory,
                            !q.isEmpty(), q);
  }
}

void ItemListView::onListSelectionChanged(const QItemSelection &selected,
                                          const QItemSelection &deselected) {
  Q_UNUSED(deselected);
  const auto indexes = selected.indexes();
  if (!indexes.isEmpty()) {
    const QString itemId =
        indexes.first().data(ItemListModel::IdRole).toString();
    emit itemSelected(itemId);
  } else if (m_listView->selectionModel()->selectedIndexes().isEmpty()) {
    emit itemSelected(QString());
  }
}

bool ItemListView::eventFilter(QObject *obj, QEvent *event) {
  if (m_listView && obj == m_listView->viewport()) {
    if (event->type() == QEvent::MouseButtonPress) {
      auto *me = static_cast<QMouseEvent *>(event);
      if (me->button() == Qt::RightButton) {
        // Mở context menu trực tiếp mà không cho QListView đổi focus/selection
        // sang item vừa bấm chuột phải.
        onContextMenuRequested(me->pos());
        return true;
      }
    } else if (event->type() == QEvent::MouseButtonRelease ||
               event->type() == QEvent::MouseButtonDblClick) {
      auto *me = static_cast<QMouseEvent *>(event);
      if (me->button() == Qt::RightButton) {
        return true;
      }
    } else if (event->type() == QEvent::ContextMenu) {
      auto *ce = static_cast<QContextMenuEvent *>(event);
      if (ce->reason() == QContextMenuEvent::Mouse) {
        ce->accept();
        return true;
      }
    }
  } else if (obj == m_listView && event->type() == QEvent::ContextMenu) {
    auto *ce = static_cast<QContextMenuEvent *>(event);
    if (ce->reason() == QContextMenuEvent::Mouse) {
      ce->accept();
      return true;
    }
  }
  return QWidget::eventFilter(obj, event);
}

void ItemListView::onContextMenuRequested(const QPoint &pos) {
  const QModelIndex proxyIdx = m_listView->indexAt(pos);
  if (!proxyIdx.isValid()) {
    return;
  }

  const QString itemId = proxyIdx.data(ItemListModel::IdRole).toString();
  if (itemId.isEmpty()) {
    return;
  }

  auto &bridge = core::CoreBridge::instance();
  std::optional<models::UiVaultItem> resolved;
  if (bridge.isInitialized() && bridge.isVaultUnlocked()) {
    resolved = bridge.getItem(itemId);
  }
  if (!resolved.has_value()) {
    resolved = m_model->findItemById(itemId);
  }
  if (!resolved.has_value()) {
    return;
  }

  QMenu menu(this);
  menu.setObjectName(QStringLiteral("ItemListContextMenu"));

  if (resolved->isDeleted) {
    populateTrashContextMenu(menu, itemId);
  } else {
    populateActiveContextMenu(menu, resolved.value());
  }

  menu.exec(m_listView->viewport()->mapToGlobal(pos));
}

void ItemListView::populateTrashContextMenu(QMenu &menu,
                                            const QString &itemId) {
  const auto &tm = app::ThemeManager::instance();

  auto *restoreAct = menu.addAction(tr("Restore"));
  connect(restoreAct, &QAction::triggered, this,
          [this, itemId]() { emit itemRestoreRequested(itemId); });

  menu.addSeparator();

  auto *purgeAct = menu.addAction(tm.icon(QStringLiteral("trash")),
                                  tr("Delete Permanently"));
  connect(purgeAct, &QAction::triggered, this,
          [this, itemId]() { emit itemPurgeRequested(itemId); });
}

void ItemListView::populateActiveContextMenu(QMenu &menu,
                                             const models::UiVaultItem &item) {
  const auto &tm = app::ThemeManager::instance();
  bool hasQuickActions = false;

  auto addCopyAction = [&](const QString &label, const QIcon &icon,
                           const QString &fieldName) {
    auto *act = menu.addAction(icon, label);
    connect(act, &QAction::triggered, this, [this, id = item.id, fieldName]() {
      emit quickCopyRequested(id, fieldName);
    });
    hasQuickActions = true;
  };

  if (!item.username.isEmpty()) {
    addCopyAction(tr("Copy Username"), tm.icon(QStringLiteral("copy")),
                  QStringLiteral("username"));
  }
  if (!item.password.isEmpty()) {
    addCopyAction(tr("Copy Password"), tm.icon(QStringLiteral("copy")),
                  QStringLiteral("password"));
  }
  if (!item.cardNumber.isEmpty()) {
    addCopyAction(tr("Copy Card Number"), tm.icon(QStringLiteral("copy")),
                  QStringLiteral("cardNumber"));
  }
  if (!item.cardCvv.isEmpty()) {
    addCopyAction(tr("Copy CVV"), tm.icon(QStringLiteral("copy")),
                  QStringLiteral("cardCvv"));
  }
  if (item.hasTotp || !item.totpSecret.isEmpty()) {
    addCopyAction(tr("Copy 2FA Code"), tm.icon(QStringLiteral("shield_check")),
                  QStringLiteral("totp"));
  }
  if (!item.websiteUrl.trimmed().isEmpty()) {
    auto *openUrlAct = menu.addAction(tm.icon(QStringLiteral("external_link")),
                                      tr("Open Website"));
    connect(openUrlAct, &QAction::triggered, this,
            [rawUrl = item.websiteUrl.trimmed()]() {
              QUrl url(rawUrl);
              if (url.scheme().isEmpty()) {
                url.setScheme(QStringLiteral("https"));
              }
              QDesktopServices::openUrl(url);
            });
    hasQuickActions = true;
  }

  if (hasQuickActions) {
    menu.addSeparator();
  }

  auto *editAct = menu.addAction(tm.icon(QStringLiteral("edit")), tr("Edit"));
  connect(editAct, &QAction::triggered, this,
          [this, id = item.id]() { emit itemEditRequested(id); });

  const QString favLabel =
      item.isFavorite ? tr("Remove from Favorites") : tr("Add to Favorites");
  const QIcon favIcon = tm.icon(item.isFavorite ? QStringLiteral("star_filled")
                                                : QStringLiteral("star"));
  auto *favAct = menu.addAction(favIcon, favLabel);
  connect(favAct, &QAction::triggered, this,
          [this, id = item.id]() { emit itemFavoriteToggleRequested(id); });

  auto *vaultMenu =
      menu.addMenu(tm.icon(QStringLiteral("vault_safe")), tr("Move to Vault"));
  vaultMenu->setObjectName(QStringLiteral("ItemListMoveToVaultMenu"));
  populateMoveToVaultMenu(*vaultMenu, item);

  menu.addSeparator();

  auto *trashAct =
      menu.addAction(tm.icon(QStringLiteral("trash")), tr("Move to Trash"));
  connect(trashAct, &QAction::triggered, this,
          [this, id = item.id]() { emit itemTrashRequested(id); });
}

void ItemListView::populateMoveToVaultMenu(QMenu &vaultMenu,
                                           const models::UiVaultItem &item) {
  const auto &tm = app::ThemeManager::instance();
  const QList<models::UiVault> vaults = app::VaultManager::instance().vaults();
  int movableVaultCount = 0;

  for (const auto &v : vaults) {
    const QString iconKey =
        v.iconName.isEmpty() ? QStringLiteral("vault_personal") : v.iconName;
    if (v.id == item.vaultId) {
      auto *currentAct =
          vaultMenu.addAction(tm.icon(iconKey), tr("%1 (Current)").arg(v.name));
      currentAct->setEnabled(false);
    } else {
      auto *targetAct = vaultMenu.addAction(tm.icon(iconKey), v.name);
      connect(targetAct, &QAction::triggered, this,
              [this, id = item.id, targetVaultId = v.id]() {
                emit itemMoveToVaultRequested(id, targetVaultId);
              });
      ++movableVaultCount;
    }
  }

  if (movableVaultCount == 0) {
    if (!vaults.isEmpty()) {
      vaultMenu.addSeparator();
    }
    auto *emptyAct = vaultMenu.addAction(tr("No other vaults available"));
    emptyAct->setEnabled(false);
  }
}

void ItemListView::retranslateUi() {
  if (m_searchEdit) {
    m_searchEdit->setPlaceholderText(tr("Search vault... (Ctrl+F)"));
  }
  if (m_newBtn) {
    m_newBtn->setToolTip(tr("Create new item (Ctrl+N)"));
  }
  if (m_emptyWidget) {
    m_emptyWidget->retranslateUi();
  }
}

void ItemListView::changeEvent(QEvent *event) {
  if (event->type() == QEvent::LanguageChange) {
    retranslateUi();
  }
  QWidget::changeEvent(event);
}

} // namespace svm::views
