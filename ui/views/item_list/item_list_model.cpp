/**
 * @file ui/views/item_list/item_list_model.cpp
 * @brief Hiện thực ItemListModel cho danh sách bản ghi SVM.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "item_list_model.h"

namespace svm::views {

ItemListModel::ItemListModel(QObject *parent) : QAbstractListModel(parent) {}

int ItemListModel::rowCount(const QModelIndex &parent) const {
  if (parent.isValid()) {
    return 0;
  }
  return static_cast<int>(m_items.size());
}

QVariant ItemListModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid() || index.row() < 0 ||
      index.row() >= static_cast<int>(m_items.size())) {
    return {};
  }

  const auto &item = m_items.at(index.row());

  switch (role) {
  case Qt::DisplayRole:
  case TitleRole:
    return item.title;
  case IdRole:
    return item.id;
  case CategoryRole:
    return static_cast<int>(item.category);
  case UsernameRole:
    return item.username;
  case WebsiteRole:
    return item.websiteUrl;
  case IsFavoriteRole:
    return item.isFavorite;
  case HasTotpRole:
    return item.hasTotp;
  case HasPasskeyRole:
    return item.hasPasskey;
  case IsSshLoadedRole:
    return item.isSshLoaded;
  case IsDeletedRole:
    return item.isDeleted;
  case IsArchivedRole:
    return item.isArchived;
  case PasswordScoreRole:
    return item.passwordStrengthScore;
  case UpdatedDateRole:
    return item.updatedAt;
  case VaultItemRole:
    return QVariant::fromValue(item);
  default:
    return {};
  }
}

QHash<int, QByteArray> ItemListModel::roleNames() const {
  return {{IdRole, "id"},
          {TitleRole, "title"},
          {CategoryRole, "category"},
          {UsernameRole, "username"},
          {WebsiteRole, "website"},
          {IsFavoriteRole, "isFavorite"},
          {HasTotpRole, "hasTotp"},
          {HasPasskeyRole, "hasPasskey"},
          {IsSshLoadedRole, "isSshLoaded"},
          {IsDeletedRole, "isDeleted"},
          {IsArchivedRole, "isArchived"},
          {PasswordScoreRole, "passwordScore"},
          {UpdatedDateRole, "updatedDate"},
          {VaultItemRole, "vaultItem"}};
}

void ItemListModel::setItems(const QList<models::UiVaultItem> &items) {
  beginResetModel();
  m_items = items;
  endResetModel();
}

void ItemListModel::upsertItem(const models::UiVaultItem &item) {
  for (int i = 0; i < m_items.size(); ++i) {
    if (m_items[i].id == item.id) {
      m_items[i] = item;
      const QModelIndex idx = index(i);
      emit dataChanged(idx, idx);
      return;
    }
  }

  beginInsertRows(QModelIndex(), 0, 0);
  m_items.prepend(item);
  endInsertRows();
}

void ItemListModel::removeItem(const QString &itemId) {
  for (int i = 0; i < m_items.size(); ++i) {
    if (m_items[i].id == itemId) {
      beginRemoveRows(QModelIndex(), i, i);
      m_items.removeAt(i);
      endRemoveRows();
      return;
    }
  }
}

std::optional<models::UiVaultItem>
ItemListModel::findItemById(const QString &itemId) const {
  for (const auto &item : m_items) {
    if (item.id == itemId) {
      return item;
    }
  }
  return std::nullopt;
}

} // namespace svm::views
