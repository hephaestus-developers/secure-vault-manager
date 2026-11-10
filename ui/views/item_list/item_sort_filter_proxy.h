/**
 * @file ui/views/item_list/item_sort_filter_proxy.h
 * @brief Proxy model lọc danh mục, bộ lọc và tìm kiếm tức thời không độ trễ.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#pragma once

#include "models/vault_item.h"
#include "views/sidebar/sidebar_types.h"

#include <QSortFilterProxyModel>

namespace svm::views {

class ItemSortFilterProxy : public QSortFilterProxyModel {
  Q_OBJECT

public:
  enum class SortOption {
    TitleAsc,
    TitleDesc,
    UpdatedDesc,
    UpdatedAsc,
    CategoryAsc
  };

  explicit ItemSortFilterProxy(QObject *parent = nullptr);
  ~ItemSortFilterProxy() override = default;

  void setFilterType(SidebarFilterType type, models::ItemCategory category,
                     const QString &folderId = QString());
  void setVaultFilter(const QString &vaultId);
  void setSearchText(const QString &query);
  void setSortOption(SortOption option);

protected:
  [[nodiscard]] bool
  filterAcceptsRow(int sourceRow,
                   const QModelIndex &sourceParent) const override;
  [[nodiscard]] bool lessThan(const QModelIndex &sourceLeft,
                              const QModelIndex &sourceRight) const override;

private:
  SidebarFilterType m_filterType{SidebarFilterType::AllItems};
  models::ItemCategory m_targetCategory{models::ItemCategory::Login};
  QString m_targetFolderId;
  QString m_vaultFilter;
  QString m_searchQuery;
  SortOption m_sortOption{SortOption::UpdatedDesc};
};

} // namespace svm::views
