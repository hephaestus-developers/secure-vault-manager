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
#include <QPushButton>
#include <QStackedWidget>
#include <QWidget>

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

protected:
  void changeEvent(QEvent *event) override;

private slots:
  void onSearchTextChanged(const QString &text);
  void onListSelectionChanged(const QItemSelection &selected,
                              const QItemSelection &deselected);

private:
  void setupUI();
  void updateEmptyStateVisibility();

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
