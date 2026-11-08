/**
 * @file ui/views/item_list/item_list_view.cpp
 * @brief Hiện thực ItemListView hiển thị danh sách thẻ bản ghi.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "item_list_view.h"
#include "app/dev_logger.h"
#include "app/theme_manager.h"
#include "app/vault_manager.h"

#include <QAction>
#include <QEvent>
#include <QHBoxLayout>
#include <QIcon>
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

  m_delegate = new ItemCardDelegate(this);
  m_listView->setItemDelegate(m_delegate);

  auto updateNewBtnIcon = [this]() {
    const bool isLight =
        (app::ThemeManager::instance().currentMode() == app::ThemeMode::Light);
    m_newBtn->setIcon(QIcon(isLight ? QStringLiteral(":/icons/plus_light.svg")
                                    : QStringLiteral(":/icons/plus_dark.svg")));
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

void ItemListView::selectItem(const QString &itemId) {
  for (int row = 0; row < m_proxyModel->rowCount(); ++row) {
    const QModelIndex idx = m_proxyModel->index(row, 0);
    if (idx.data(ItemListModel::IdRole).toString() == itemId) {
      m_listView->setCurrentIndex(idx);
      m_listView->scrollTo(idx);
      break;
    }
  }
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
