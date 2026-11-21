/**
 * @file ui/components/combobox_popup_helper.h
 * @brief Tiện ích cấu hình popup drop-down của QComboBox bo góc và không có
 * viền đen ngoài.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#pragma once

#include <QComboBox>
#include <QEvent>
#include <QFrame>
#include <QListView>
#include <QPainter>
#include <QPainterPath>
#include <QStyledItemDelegate>
#include <QWidget>

#include "ui/app/ThemeManager.hpp"

namespace svm::ui {

/**
 * @brief Delegate vẽ item cho popup combobox với bo góc, loại bỏ outline vuông,
 * và hiển thị trạng thái hover/selected rõ ràng, thẩm mỹ.
 */
class ComboBoxPopupDelegate : public QStyledItemDelegate {
public:
  using QStyledItemDelegate::QStyledItemDelegate;

  QSize sizeHint(const QStyleOptionViewItem &option,
                 const QModelIndex &index) const override {
    Q_UNUSED(index);
    int h = 34;
    if (option.fontMetrics.height() + 12 > h) {
      h = option.fontMetrics.height() + 12;
    }
    return {option.rect.width(), h};
  }

  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override {
    if (!index.isValid()) {
      return;
    }

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    const auto &pal = svm::app::ThemeManager::instance().palette();
    const bool isLight = (svm::app::ThemeManager::instance().currentMode() ==
                          svm::app::ThemeMode::Light);
    const bool isSelected = (option.state & QStyle::State_Selected);
    const bool isHovered = (option.state & QStyle::State_MouseOver);

    const QRect rect = option.rect.adjusted(2, 1, -2, -1);

    // 1. Vẽ nền bo góc nổi bật khi Hover hoặc Selected
    if (isHovered) {
      QPainterPath bgPath;
      bgPath.addRoundedRect(rect, 6, 6);
      painter->fillPath(bgPath, pal.hover2);
      painter->setPen(QPen(pal.borderStrong, 1));
      painter->drawPath(bgPath);
    } else if (isSelected) {
      QPainterPath bgPath;
      bgPath.addRoundedRect(rect, 6, 6);
      painter->fillPath(bgPath, pal.hover1);
    }

    // 2. Vẽ Icon nếu có (căn giữa theo chiều dọc, lề trái 8px)
    const QVariant iconData = index.data(Qt::DecorationRole);
    int textLeft = rect.left() + 8;
    if (iconData.isValid() && !iconData.isNull()) {
      const QIcon icon = qvariant_cast<QIcon>(iconData);
      if (!icon.isNull()) {
        const int iconSize = 16;
        const int iconY = rect.top() + (rect.height() - iconSize) / 2;
        const QRect iconRect(textLeft, iconY, iconSize, iconSize);
        icon.paint(painter, iconRect, Qt::AlignCenter);
        textLeft = iconRect.right() + 8;
      }
    }

    // 3. Vẽ Checkmark Icon phía bên phải nếu mục đang được chọn (Selected)
    int textRight = rect.right() - 8;
    if (isSelected) {
      const int checkSize = 14;
      const int checkX = rect.right() - checkSize - 8;
      const int checkY = rect.top() + (rect.height() - checkSize) / 2;
      const QRect checkRect(checkX, checkY, checkSize, checkSize);
      const QIcon checkIcon(
          isLight ? QStringLiteral(":/icons/controls/menu_check_light.svg")
                  : QStringLiteral(":/icons/controls/menu_check_dark.svg"));
      checkIcon.paint(painter, checkRect, Qt::AlignCenter);
      textRight = checkX - 6;
    }

    // 4. Vẽ Tiêu đề / Tên item
    const QString text = index.data(Qt::DisplayRole).toString();
    const QRect textRect(textLeft, rect.top(), textRight - textLeft,
                         rect.height());

    QFont font = option.font;
    font.setPointSize(9);
    if (isSelected || isHovered) {
      font.setBold(true);
      painter->setPen(pal.foreground);
    } else {
      font.setBold(false);
      painter->setPen(pal.foreground);
    }
    painter->setFont(font);
    painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, text);

    painter->restore();
  }
};

} // namespace svm::ui
