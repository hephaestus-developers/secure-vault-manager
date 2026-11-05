/**
 * @file ui/views/sidebar/sidebar_item_delegate.cpp
 * @brief Hiện thực SidebarItemDelegate vẽ item dạng card liền mạch và badge
 * pill tinh tế.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "sidebar_item_delegate.h"
#include "app/theme_manager.h"

#include <QIcon>
#include <QPainter>
#include <QPainterPath>

namespace svm::views {

SidebarItemDelegate::SidebarItemDelegate(QObject *parent)
    : QStyledItemDelegate(parent) {}

QSize SidebarItemDelegate::sizeHint(const QStyleOptionViewItem &option,
                                    const QModelIndex &index) const {
  Q_UNUSED(option);
  const bool isHeader = index.data(IsHeaderRole).toBool();
  if (isHeader) {
    return {0, 30};
  }
  return {0, 34};
}

void SidebarItemDelegate::paint(QPainter *painter,
                                const QStyleOptionViewItem &option,
                                const QModelIndex &index) const {
  if (!index.isValid()) {
    return;
  }

  painter->save();
  painter->setRenderHint(QPainter::Antialiasing);

  const bool isLight =
      (app::ThemeManager::instance().currentMode() == app::ThemeMode::Light);
  const bool isHeader = index.data(IsHeaderRole).toBool();

  // 1. Vẽ Tiêu đề Nhóm (Header: CATEGORIES, WATCHTOWER)
  if (isHeader) {
    const QString headerText = index.data(Qt::DisplayRole).toString();
    painter->setPen(isLight ? QColor(0x6C, 0x6F, 0x85)
                            : QColor(0x6C, 0x70, 0x86));
    QFont headerFont = option.font;
    headerFont.setBold(true);
    headerFont.setPointSize(8);
    headerFont.setLetterSpacing(QFont::AbsoluteSpacing, 1.0);
    painter->setFont(headerFont);

    const QRect headerRect = option.rect.adjusted(18, 8, -14, -2);
    painter->drawText(headerRect, Qt::AlignLeft | Qt::AlignVCenter, headerText);
    painter->restore();
    return;
  }

  // 2. Vẽ Item Thông Thường
  const QRect rect = option.rect.adjusted(10, 1, -10, -1);
  const bool isSelected = (option.state & QStyle::State_Selected);
  const bool isHovered = (option.state & QStyle::State_MouseOver);

  // Background bo góc 6px
  QPainterPath bgPath;
  bgPath.addRoundedRect(rect, 6, 6);

  if (isSelected) {
    painter->fillPath(bgPath, isLight ? QColor(0xDC, 0xE0, 0xE8)
                                      : QColor(0x31, 0x32, 0x44));

    // Thanh chỉ thị xanh bên mép trái
    QPainterPath indicatorPath;
    indicatorPath.addRoundedRect(
        QRect(rect.left() + 2, rect.top() + (rect.height() - 14) / 2, 3, 14),
        1.5, 1.5);
    painter->fillPath(indicatorPath, isLight ? QColor(0x1E, 0x66, 0xF5)
                                             : QColor(0x89, 0xB4, 0xFA));
  } else if (isHovered) {
    painter->fillPath(bgPath, isLight ? QColor(0x00, 0x00, 0x00, 0x0A)
                                      : QColor(0xFF, 0xFF, 0xFF, 0x0C));
  }

  // 3. Icon (16x16) căn đúng trục x = 18px (10px margin + 8px inset)
  const QIcon itemIcon = index.data(Qt::DecorationRole).value<QIcon>();
  const QRect iconRect(rect.left() + 8, rect.top() + (rect.height() - 16) / 2,
                       16, 16);
  if (!itemIcon.isNull()) {
    itemIcon.paint(painter, iconRect, Qt::AlignCenter);
  }

  // 4. Badge Số Lượng bên phải
  const int count = index.data(CountRole).toInt();
  const QString countText = QString::number(count);
  int badgeWidth = 0;

  QFont badgeFont = option.font;
  badgeFont.setPointSize(8);
  badgeFont.setBold(true);
  const QFontMetrics fmBadge(badgeFont);

  if (count > 0) {
    const int textWidth = fmBadge.horizontalAdvance(countText);
    badgeWidth = std::max(20, textWidth + 10);
    const QRect badgeRect(rect.right() - badgeWidth - 6,
                          rect.top() + (rect.height() - 18) / 2, badgeWidth,
                          18);

    QPainterPath badgePath;
    badgePath.addRoundedRect(badgeRect, 9, 9);
    painter->fillPath(badgePath,
                      isLight ? (isSelected ? QColor(0xCC, 0xD0, 0xDA)
                                            : QColor(0xDC, 0xE0, 0xE8))
                              : (isSelected ? QColor(0x45, 0x47, 0x5A)
                                            : QColor(0xFF, 0xFF, 0xFF, 0x14)));

    painter->setPen(isLight ? (isSelected ? QColor(0x1E, 0x66, 0xF5)
                                          : QColor(0x4C, 0x4F, 0x69))
                            : (isSelected ? QColor(0xFF, 0xFF, 0xFF)
                                          : QColor(0xCD, 0xD6, 0xF4)));
    painter->setFont(badgeFont);
    painter->drawText(badgeRect, Qt::AlignCenter, countText);
  } else {
    // Nếu count = 0, hiển thị số mờ tinh tế không đóng khung
    badgeWidth = 14;
    const QRect badgeRect(rect.right() - badgeWidth - 8,
                          rect.top() + (rect.height() - 18) / 2, badgeWidth,
                          18);
    painter->setPen(isLight ? QColor(0x9C, 0xA0, 0xB0)
                            : QColor(0x58, 0x5B, 0x70));
    painter->setFont(badgeFont);
    painter->drawText(badgeRect, Qt::AlignCenter, QStringLiteral("0"));
  }

  // 5. Tiêu đề / Tên danh mục (chiếm toàn bộ khoảng giữa, không bị cắt chữ)
  const QString text = index.data(Qt::DisplayRole).toString();
  const int textLeft = iconRect.right() + 10;
  const int textRight = rect.right() - badgeWidth - 12;
  const int availableWidth = textRight - textLeft;

  painter->setPen(
      isLight
          ? (isSelected ? QColor(0x1E, 0x66, 0xF5) : QColor(0x4C, 0x4F, 0x69))
          : (isSelected ? QColor(0xFF, 0xFF, 0xFF) : QColor(0xCD, 0xD6, 0xF4)));
  QFont textFont = option.font;
  textFont.setPointSize(9);
  textFont.setBold(isSelected);
  painter->setFont(textFont);

  const QFontMetrics fmText(textFont);
  const QString elidedText =
      fmText.elidedText(text, Qt::ElideRight, availableWidth);
  painter->drawText(QRect(textLeft, rect.top(), availableWidth, rect.height()),
                    Qt::AlignLeft | Qt::AlignVCenter, elidedText);

  painter->restore();
}

} // namespace svm::views
