/**
 * @file ui/views/item_list/item_list_view.cpp
 * @brief Hiện thực ItemListView hiển thị danh sách thẻ bản ghi.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "item_list_view.h"
#include "app/theme_manager.h"
#include "app/vault_manager.h"
#include "components/combobox_popup_helper.h"

#include <QAction>
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
  m_sortCombo->setFixedHeight(28);
  m_sortCombo->setIconSize(QSize(16, 16));
  m_sortCombo->setCursor(Qt::PointingHandCursor);
  svm::ui::setupFramelessComboBoxPopup(m_sortCombo, 175);

  auto updateSortIcons = [this]() {
    const bool isLight =
        (app::ThemeManager::instance().currentMode() == app::ThemeMode::Light);
    const QString suffix =
        isLight ? QStringLiteral("_light.svg") : QStringLiteral("_dark.svg");
    m_sortCombo->setItemIcon(
        0, QIcon(QStringLiteral(":/icons/sort_time") + suffix));
    m_sortCombo->setItemIcon(
        1, QIcon(QStringLiteral(":/icons/sort_alpha_asc") + suffix));
    m_sortCombo->setItemIcon(
        2, QIcon(QStringLiteral(":/icons/sort_alpha_desc") + suffix));
  };

  m_sortCombo->addItem(
      tr("Recently Updated"),
      static_cast<int>(ItemSortFilterProxy::SortOption::UpdatedDesc));
  m_sortCombo->addItem(
      tr("Title (A-Z)"),
      static_cast<int>(ItemSortFilterProxy::SortOption::TitleAsc));
  m_sortCombo->addItem(
      tr("Title (Z-A)"),
      static_cast<int>(ItemSortFilterProxy::SortOption::TitleDesc));
  updateSortIcons();

  connect(&app::ThemeManager::instance(), &app::ThemeManager::themeChanged,
          this, updateSortIcons);
  connect(m_sortCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &ItemListView::onSortChanged);
  toolLayout->addWidget(m_sortCombo);

  layout->addWidget(toolContainer);

  // 3. QListView với Proxy Model & Delegate
  m_model = new ItemListModel(this);
  m_proxyModel = new ItemSortFilterProxy(this);
  m_proxyModel->setSourceModel(m_model);

  connect(&app::VaultManager::instance(),
          &app::VaultManager::activeVaultChanged, this,
          [this](const QString &vaultId) {
            m_proxyModel->setVaultFilter(vaultId);
            updateCountLabel();
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
  if (count == 1) {
    m_countLabel->setText(tr("%1 item").arg(count));
  } else {
    m_countLabel->setText(tr("%1 items").arg(count));
  }
}

void ItemListView::retranslateUi() {
  if (m_searchEdit) {
    m_searchEdit->setPlaceholderText(tr("Search vault... (Ctrl+F)"));
  }
  if (m_newBtn) {
    m_newBtn->setToolTip(tr("Create new item (Ctrl+N)"));
  }
  if (m_sortCombo && m_sortCombo->count() >= 3) {
    m_sortCombo->setItemText(0, tr("Recently Updated"));
    m_sortCombo->setItemText(1, tr("Title (A-Z)"));
    m_sortCombo->setItemText(2, tr("Title (Z-A)"));
  }
  updateCountLabel();
}

void ItemListView::changeEvent(QEvent *event) {
  if (event->type() == QEvent::LanguageChange) {
    retranslateUi();
  }
  QWidget::changeEvent(event);
}

} // namespace svm::views
