/**
 * @file ui/views/item_list/item_card_delegate.h
 * @brief Custom Item Delegate vẽ card bản ghi hiện đại với badges và hover copy
 * actions.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#pragma once

#include <QPoint>
#include <QStyledItemDelegate>

namespace svm::views {

class ItemCardDelegate : public QStyledItemDelegate {
  Q_OBJECT

public:
  explicit ItemCardDelegate(QObject *parent = nullptr);
  ~ItemCardDelegate() override = default;

  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override;
  [[nodiscard]] QSize sizeHint(const QStyleOptionViewItem &option,
                               const QModelIndex &index) const override;

signals:
  void quickCopyUsernameRequested(const QString &itemId);
  void quickCopyPasswordRequested(const QString &itemId);

protected:
  bool editorEvent(QEvent *event, QAbstractItemModel *model,
                   const QStyleOptionViewItem &option,
                   const QModelIndex &index) override;

private:
  mutable QPoint m_hoverPos;
  mutable QModelIndex m_hoverIndex;
};

} // namespace svm::views
