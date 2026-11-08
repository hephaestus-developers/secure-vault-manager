/**
 * @file ui/views/item_list/item_list_empty_widget.h
 * @brief Widget hiển thị trạng thái trống (Empty State) cho danh sách bản ghi
 * Cột 2.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#pragma once

#include "models/vault_item.h"
#include "views/sidebar/sidebar_types.h"

#include <QLabel>
#include <QPushButton>
#include <QWidget>

namespace svm::views {

class ItemListEmptyWidget : public QWidget {
  Q_OBJECT

public:
  explicit ItemListEmptyWidget(QWidget *parent = nullptr);
  ~ItemListEmptyWidget() override = default;

  void setState(SidebarFilterType filterType, models::ItemCategory category,
                bool isSearch, const QString &searchQuery);

  void retranslateUi();

signals:
  void createItemRequested(models::ItemCategory category);
  void clearSearchRequested();

protected:
  void changeEvent(QEvent *event) override;

private:
  void setupUI();
  void updateDisplay();

  SidebarFilterType m_filterType{SidebarFilterType::AllItems};
  models::ItemCategory m_category{models::ItemCategory::Login};
  bool m_isSearch{false};
  QString m_searchQuery;

  QWidget *m_iconContainer{nullptr};
  QLabel *m_iconLabel{nullptr};
  QLabel *m_titleLabel{nullptr};
  QLabel *m_descLabel{nullptr};
  QWidget *m_actionContainer{nullptr};
  QPushButton *m_actionBtn{nullptr};
};

} // namespace svm::views
