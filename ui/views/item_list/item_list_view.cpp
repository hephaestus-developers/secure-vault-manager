/**
 * @file ui/views/item_list/item_list_view.cpp
 * @brief Hiện thực ItemListView hiển thị danh sách thẻ bản ghi.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "item_list_view.h"
#include "app/theme_manager.h"

#include <QAction>
#include <QHBoxLayout>
#include <QIcon>
#include <QVBoxLayout>

namespace svm::views {

ItemListView::ItemListView(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("ItemListView"));
  setAttribute(Qt::WA_StyledBackground, true);
  setMinimumWidth(260);

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
  m_searchEdit->addAction(QIcon(QStringLiteral(":/icons/search.svg")),
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

  // 2. Row 2: Metadata Toolbar (Item count & Sort Filter)
  auto *toolContainer = new QWidget(this);
  auto *toolLayout = new QHBoxLayout(toolContainer);
  toolLayout->setContentsMargins(2, 0, 2, 0);
  toolLayout->setSpacing(8);

  m_countLabel = new QLabel(tr("0 items"), toolContainer);
  m_countLabel->setObjectName(QStringLiteral("ItemListCountLabel"));
  toolLayout->addWidget(m_countLabel);

  toolLayout->addStretch(1);

  m_sortCombo = new QComboBox(toolContainer);
  m_sortCombo->setObjectName(QStringLiteral("ItemListSortCombo"));
  m_sortCombo->setFixedHeight(26);
  m_sortCombo->addItem(
      tr("Recently Updated"),
      static_cast<int>(ItemSortFilterProxy::SortOption::UpdatedDesc));
  m_sortCombo->addItem(
      tr("Title (A-Z)"),
      static_cast<int>(ItemSortFilterProxy::SortOption::TitleAsc));
  m_sortCombo->addItem(
      tr("Title (Z-A)"),
      static_cast<int>(ItemSortFilterProxy::SortOption::TitleDesc));
  connect(m_sortCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &ItemListView::onSortChanged);
  toolLayout->addWidget(m_sortCombo);

  layout->addWidget(toolContainer);

  // 3. QListView với Proxy Model & Delegate
  m_model = new ItemListModel(this);
  m_proxyModel = new ItemSortFilterProxy(this);
  m_proxyModel->setSourceModel(m_model);

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
          this, [this, updateNewBtnIcon]() {
            updateNewBtnIcon();
            if (m_listView && m_listView->viewport()) {
              m_listView->viewport()->update();
            }
          });

  connect(m_delegate, &ItemCardDelegate::quickCopyUsernameRequested, this,
          [this](const QString &itemId) {
            emit quickCopyRequested(itemId, QStringLiteral("username"));
          });
  connect(m_delegate, &ItemCardDelegate::quickCopyPasswordRequested, this,
          [this](const QString &itemId) {
            emit quickCopyRequested(itemId, QStringLiteral("password"));
          });

  connect(m_listView->selectionModel(), &QItemSelectionModel::selectionChanged,
          this, &ItemListView::onListSelectionChanged);

  layout->addWidget(m_listView, 1);
}

void ItemListView::setItems(const QList<models::UiVaultItem> &items) {
  m_model->setItems(items);
  updateCountLabel();
}

void ItemListView::upsertItem(const models::UiVaultItem &item) {
  m_model->upsertItem(item);
  updateCountLabel();
}

void ItemListView::removeItem(const QString &itemId) {
  m_model->removeItem(itemId);
  updateCountLabel();
}

void ItemListView::setFilter(SidebarFilterType type,
                             models::ItemCategory category) {
  m_currentCategory = category;
  m_proxyModel->setFilterType(type, category);
  updateCountLabel();
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
  m_proxyModel->setSearchText(text);
  updateCountLabel();
}

void ItemListView::onSortChanged(int index) {
  const auto opt = static_cast<ItemSortFilterProxy::SortOption>(
      m_sortCombo->itemData(index).toInt());
  m_proxyModel->setSortOption(opt);
}

void ItemListView::onListSelectionChanged(const QItemSelection &selected,
                                          const QItemSelection &deselected) {
  Q_UNUSED(deselected);
  const auto indexes = selected.indexes();
  if (!indexes.isEmpty()) {
    const QString itemId =
        indexes.first().data(ItemListModel::IdRole).toString();
    emit itemSelected(itemId);
  }
}

void ItemListView::updateCountLabel() {
  const int count = m_proxyModel->rowCount();
  m_countLabel->setText(tr("%1 items").arg(count));
}

} // namespace svm::views
