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

#include "app/theme_manager.h"

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
    int h = 30;
    if (option.fontMetrics.height() + 10 > h) {
      h = option.fontMetrics.height() + 10;
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

    const bool isLight = (svm::app::ThemeManager::instance().currentMode() ==
                          svm::app::ThemeMode::Light);
    const bool isSelected = (option.state & QStyle::State_Selected);
    const bool isHovered = (option.state & QStyle::State_MouseOver);

    const QRect rect = option.rect.adjusted(2, 1, -2, -1);

    // 1. Vẽ nền bo góc nổi bật khi Hover hoặc Selected
    if (isSelected || isHovered) {
      QPainterPath bgPath;
      bgPath.addRoundedRect(rect, 6, 6);

      if (isLight) {
        painter->fillPath(bgPath, QColor(0xEA, 0xEE, 0xF5));
        painter->setPen(QPen(QColor(0xCD, 0xD4, 0xE2), 1));
        painter->drawPath(bgPath);
      } else {
        painter->fillPath(bgPath, QColor(0x36, 0x3D, 0x59));
        painter->setPen(QPen(QColor(0x54, 0x5E, 0x85), 1));
        painter->drawPath(bgPath);
      }
    }

    // 2. Vẽ Icon nếu có
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

    // 3. Vẽ Tiêu đề / Tên item
    const QString text = index.data(Qt::DisplayRole).toString();
    const QRect textRect(textLeft, rect.top(), rect.right() - textLeft - 6,
                         rect.height());

    QFont font = option.font;
    font.setPointSize(9);
    if (isSelected || isHovered) {
      font.setBold(true);
      painter->setPen(isLight ? QColor(0x1E, 0x1E, 0x2E)
                              : QColor(0xFF, 0xFF, 0xFF));
    } else {
      font.setBold(false);
      painter->setPen(isLight ? QColor(0x4C, 0x4F, 0x69)
                              : QColor(0xCD, 0xD6, 0xF4));
    }
    painter->setFont(font);
    painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, text);

    painter->restore();
  }
};

/**
 * @brief Event filter để vô hiệu hóa việc vẽ khung chữ nhật mặc định của
 * QComboBoxPrivateContainer.
 */
class FramelessPopupEventFilter : public QObject {
public:
  using QObject::QObject;

protected:
  bool eventFilter(QObject *watched, QEvent *event) override {
    if (event->type() == QEvent::Paint) {
      return true; // Ngăn container vẽ viền vuông mặc định
    }
    return QObject::eventFilter(watched, event);
  }
};

/**
 * @brief Cấu hình container popup của QComboBox để loại bỏ viền vuông/đen,
 * kích hoạt nền trong suốt (WA_TranslucentBackground) cho góc bo tròn,
 * cài đặt delegate hover mượt mà và đảm bảo độ rộng tối thiểu cho các mục không
 * bị cắt cụt chữ.
 *
 * @param combo Con trỏ tới QComboBox
 * @param minWidth Chiều rộng tối thiểu cho view popup (mặc định 140px)
 */
inline void setupFramelessComboBoxPopup(QComboBox *combo, int minWidth = 140) {
  if (!combo) {
    return;
  }

  auto *view = qobject_cast<QListView *>(combo->view());
  if (!view) {
    view = new QListView(combo);
    combo->setView(view);
  }

  view->setItemDelegate(new ComboBoxPopupDelegate(view));
  view->setMouseTracking(true);
  if (view->viewport()) {
    view->viewport()->setMouseTracking(true);
  }

  if (minWidth > 0) {
    view->setMinimumWidth(minWidth);
  }

  if (QWidget *container = view->parentWidget()) {
    container->setWindowFlags(Qt::Popup | Qt::FramelessWindowHint |
                              Qt::NoDropShadowWindowHint);
    container->setAttribute(Qt::WA_TranslucentBackground, true);
    if (auto *frame = qobject_cast<QFrame *>(container)) {
      frame->setFrameShape(QFrame::NoFrame);
      frame->setLineWidth(0);
    }
    container->installEventFilter(new FramelessPopupEventFilter(container));
  }
}

} // namespace svm::ui
