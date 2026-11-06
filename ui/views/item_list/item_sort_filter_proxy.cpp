/**
 * @file ui/views/item_list/item_sort_filter_proxy.cpp
 * @brief Hiện thực ItemSortFilterProxy với bộ lọc đa điều kiện.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "item_sort_filter_proxy.h"
#include "item_list_model.h"

namespace svm::views {

ItemSortFilterProxy::ItemSortFilterProxy(QObject *parent)
    : QSortFilterProxyModel(parent) {
  setDynamicSortFilter(true);
  setSortCaseSensitivity(Qt::CaseInsensitive);
}

void ItemSortFilterProxy::setFilterType(SidebarFilterType type,
                                        models::ItemCategory category) {
  m_filterType = type;
  m_targetCategory = category;
  invalidateFilter();
}

void ItemSortFilterProxy::setVaultFilter(const QString &vaultId) {
  if (m_vaultFilter != vaultId) {
    m_vaultFilter = vaultId;
    invalidateFilter();
  }
}

void ItemSortFilterProxy::setSearchText(const QString &query) {
  m_searchQuery = query.trimmed();
  invalidateFilter();
}

void ItemSortFilterProxy::setSortOption(SortOption option) {
  m_sortOption = option;
  invalidate();
}

bool ItemSortFilterProxy::filterAcceptsRow(
    int sourceRow, const QModelIndex &sourceParent) const {
  const QModelIndex idx = sourceModel()->index(sourceRow, 0, sourceParent);
  if (!idx.isValid()) {
    return false;
  }

  const bool isDeleted = idx.data(ItemListModel::IsDeletedRole).toBool();
  const bool isArchived = idx.data(ItemListModel::IsArchivedRole).toBool();

  // 1. Lọc theo trạng thái Trash
  if (m_filterType == SidebarFilterType::Trash) {
    if (!isDeleted) {
      return false;
    }
  } else {
    if (isDeleted) {
      return false; // Mọi view thông thường đều ẩn mục trong thùng rác
    }
  }

  // 2. Lọc theo Vault
  if (!m_vaultFilter.isEmpty() && m_vaultFilter != QStringLiteral("all")) {
    const QString itemVaultId = idx.data(ItemListModel::VaultIdRole).toString();
    if (itemVaultId != m_vaultFilter) {
      return false;
    }
  }

  // 2. Lọc theo Sidebar Filter Type
  switch (m_filterType) {
  case SidebarFilterType::AllItems:
    if (isArchived) {
      return false;
    }
    break;
  case SidebarFilterType::Favorites:
    if (!idx.data(ItemListModel::IsFavoriteRole).toBool() || isArchived) {
      return false;
    }
    break;
  case SidebarFilterType::Category:
    if (idx.data(ItemListModel::CategoryRole).toInt() !=
            static_cast<int>(m_targetCategory) ||
        isArchived) {
      return false;
    }
    break;
  case SidebarFilterType::WatchtowerWeak: {
    const int score = idx.data(ItemListModel::PasswordScoreRole).toInt();
    if (score >= 60 || isArchived) {
      return false;
    }
    break;
  }
  case SidebarFilterType::WatchtowerExpiring:
    // Tạm thời lọc các token có hạn dùng
    break;
  case SidebarFilterType::Trash:
    break;
  }

  // 3. Lọc theo từ khóa tìm kiếm (Search Query)
  if (!m_searchQuery.isEmpty()) {
    const QString title = idx.data(ItemListModel::TitleRole).toString();
    const QString username = idx.data(ItemListModel::UsernameRole).toString();
    const QString website = idx.data(ItemListModel::WebsiteRole).toString();

    const bool matches =
        title.contains(m_searchQuery, Qt::CaseInsensitive) ||
        username.contains(m_searchQuery, Qt::CaseInsensitive) ||
        website.contains(m_searchQuery, Qt::CaseInsensitive);
    if (!matches) {
      return false;
    }
  }

  return true;
}

bool ItemSortFilterProxy::lessThan(const QModelIndex &sourceLeft,
                                   const QModelIndex &sourceRight) const {
  switch (m_sortOption) {
  case SortOption::TitleAsc:
    return sourceLeft.data(ItemListModel::TitleRole)
               .toString()
               .localeAwareCompare(
                   sourceRight.data(ItemListModel::TitleRole).toString()) < 0;
  case SortOption::TitleDesc:
    return sourceLeft.data(ItemListModel::TitleRole)
               .toString()
               .localeAwareCompare(
                   sourceRight.data(ItemListModel::TitleRole).toString()) > 0;
  case SortOption::UpdatedDesc:
    return sourceLeft.data(ItemListModel::UpdatedDateRole).toDateTime() >
           sourceRight.data(ItemListModel::UpdatedDateRole).toDateTime();
  case SortOption::UpdatedAsc:
    return sourceLeft.data(ItemListModel::UpdatedDateRole).toDateTime() <
           sourceRight.data(ItemListModel::UpdatedDateRole).toDateTime();
  case SortOption::CategoryAsc:
    return sourceLeft.data(ItemListModel::CategoryRole).toInt() <
           sourceRight.data(ItemListModel::CategoryRole).toInt();
  }
  return false;
}

} // namespace svm::views
