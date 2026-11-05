/**
 * @file ui/views/item_list/item_list_model.h
 * @brief Qt Model quản lý danh sách bản ghi UiVaultItem nạp trong bộ nhớ RAM.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#pragma once

#include "models/vault_item.h"

#include <QAbstractListModel>
#include <QList>

namespace svm::views {

class ItemListModel : public QAbstractListModel {
  Q_OBJECT

public:
  enum ItemRoles {
    IdRole = Qt::UserRole + 1,
    TitleRole,
    CategoryRole,
    UsernameRole,
    WebsiteRole,
    IsFavoriteRole,
    HasTotpRole,
    HasPasskeyRole,
    IsSshLoadedRole,
    IsDeletedRole,
    IsArchivedRole,
    PasswordScoreRole,
    UpdatedDateRole,
    VaultItemRole
  };

  explicit ItemListModel(QObject *parent = nullptr);
  ~ItemListModel() override = default;

  [[nodiscard]] int
  rowCount(const QModelIndex &parent = QModelIndex()) const override;
  [[nodiscard]] QVariant data(const QModelIndex &index,
                              int role = Qt::DisplayRole) const override;
  [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

  void setItems(const QList<models::UiVaultItem> &items);
  void upsertItem(const models::UiVaultItem &item);
  void removeItem(const QString &itemId);
  [[nodiscard]] std::optional<models::UiVaultItem>
  findItemById(const QString &itemId) const;
  [[nodiscard]] const QList<models::UiVaultItem> &items() const {
    return m_items;
  }

private:
  QList<models::UiVaultItem> m_items;
};

} // namespace svm::views
Q_DECLARE_METATYPE(svm::models::UiVaultItem)
