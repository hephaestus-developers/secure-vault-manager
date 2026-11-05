/**
 * @file ui/views/sidebar/sidebar_item_delegate.h
 * @brief Custom Item Delegate vẽ các mục danh mục và bộ lọc trên Sidebar với
 * badge pill hiện đại.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#pragma once

#include <QStyledItemDelegate>

namespace svm::views {

class SidebarItemDelegate : public QStyledItemDelegate {
  Q_OBJECT

public:
  enum Roles { CountRole = Qt::UserRole + 2, IsHeaderRole = Qt::UserRole + 3 };

  explicit SidebarItemDelegate(QObject *parent = nullptr);
  ~SidebarItemDelegate() override = default;

  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override;
  [[nodiscard]] QSize sizeHint(const QStyleOptionViewItem &option,
                               const QModelIndex &index) const override;
};

class CompactSidebarDelegate : public QStyledItemDelegate {
  Q_OBJECT

public:
  explicit CompactSidebarDelegate(QObject *parent = nullptr);
  ~CompactSidebarDelegate() override = default;

  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override;
  [[nodiscard]] QSize sizeHint(const QStyleOptionViewItem &option,
                               const QModelIndex &index) const override;
};

} // namespace svm::views
