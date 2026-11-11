/**
 * @file ui/views/item_list/item_list_view.h
 * @brief View danh sách bản ghi Cột 2 (Master Item List) với Card view và thanh
 * công cụ.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#pragma once

#include "item_card_delegate.h"
#include "item_list_empty_widget.h"
#include "item_list_model.h"
#include "item_sort_filter_proxy.h"
#include "models/vault_item.h"
#include "views/sidebar/sidebar_types.h"

#include <QLineEdit>
#include <QListView>
#include <QModelIndex>
#include <QPoint>
#include <QPushButton>
#include <QStackedWidget>
#include <QWidget>

class QMenu;

namespace svm::views {

class ItemListView : public QWidget {
  Q_OBJECT

public:
  explicit ItemListView(QWidget *parent = nullptr);
  ~ItemListView() override = default;

  void setItems(const QList<models::UiVaultItem> &items);
  void upsertItem(const models::UiVaultItem &item);
  void removeItem(const QString &itemId);
  void setFilter(SidebarFilterType type, models::ItemCategory category);
  void selectItem(const QString &itemId);
  void clearSelection();
  [[nodiscard]] QString selectedItemId() const;
  [[nodiscard]] bool isItemVisible(const QString &itemId) const;
  void focusSearch();

  [[nodiscard]] ItemListModel *sourceModel() const { return m_model; }
  [[nodiscard]] ItemSortFilterProxy *proxyModel() const { return m_proxyModel; }
  [[nodiscard]] models::ItemCategory currentCategory() const {
    return m_currentCategory;
  }

  void retranslateUi();

signals:
  void itemSelected(const QString &itemId);
  void addNewItemRequested(models::ItemCategory defaultCategory);
  void quickCopyRequested(const QString &itemId, const QString &fieldName);
  void itemEditRequested(const QString &itemId);
  void itemFavoriteToggleRequested(const QString &itemId);
  void itemMoveToVaultRequested(const QString &itemId,
                                const QString &targetVaultId);
  void itemTrashRequested(const QString &itemId);
  void itemRestoreRequested(const QString &itemId);
  void itemPurgeRequested(const QString &itemId);

protected:
  void changeEvent(QEvent *event) override;
  bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
  void onSearchTextChanged(const QString &text);
  void onListSelectionChanged(const QItemSelection &selected,
                              const QItemSelection &deselected);
  void onContextMenuRequested(const QPoint &pos);

private:
  void setupUI();
  void updateEmptyStateVisibility();
  [[nodiscard]] QModelIndex findProxyIndexById(const QString &itemId) const;
  void populateActiveContextMenu(QMenu &menu, const models::UiVaultItem &item);
  void populateTrashContextMenu(QMenu &menu, const QString &itemId);
  void populateMoveToVaultMenu(QMenu &vaultMenu,
                               const models::UiVaultItem &item);

  QLineEdit *m_searchEdit{nullptr};
  QPushButton *m_newBtn{nullptr};
  QListView *m_listView{nullptr};
  QStackedWidget *m_stack{nullptr};
  ItemListEmptyWidget *m_emptyWidget{nullptr};

  ItemListModel *m_model{nullptr};
  ItemSortFilterProxy *m_proxyModel{nullptr};
  ItemCardDelegate *m_delegate{nullptr};

  SidebarFilterType m_currentFilterType{SidebarFilterType::AllItems};
  models::ItemCategory m_currentCategory{models::ItemCategory::Login};
};

} // namespace svm::views
