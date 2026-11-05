/**
 * @file ui/views/item_list/item_card_delegate.cpp
 * @brief Hiện thực Card Delegate vẽ bản ghi đa tầng thẩm mỹ cao.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "item_card_delegate.h"
#include "app/theme_manager.h"
#include "item_list_model.h"

#include <QEvent>
#include <QIcon>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

namespace svm::views {

ItemCardDelegate::ItemCardDelegate(QObject *parent)
    : QStyledItemDelegate(parent) {}

QSize ItemCardDelegate::sizeHint(const QStyleOptionViewItem &option,
                                 const QModelIndex &index) const {
  Q_UNUSED(option);
  Q_UNUSED(index);
  return {0, 68};
}

void ItemCardDelegate::paint(QPainter *painter,
                             const QStyleOptionViewItem &option,
                             const QModelIndex &index) const {
  if (!index.isValid()) {
    return;
  }

  painter->save();
  painter->setRenderHint(QPainter::Antialiasing);

  const bool isLight =
      (app::ThemeManager::instance().currentMode() == app::ThemeMode::Light);
  const QRect rect = option.rect.adjusted(6, 3, -6, -3);
  const bool isSelected = (option.state & QStyle::State_Selected);
  const bool isHovered = (option.state & QStyle::State_MouseOver);

  // 1. Vẽ nền Card bo góc 8px (Elevated Surface cho Dark Theme, Pure White cho
  // Light Theme)
  QPainterPath cardPath;
  cardPath.addRoundedRect(rect, 8, 8);

  if (isSelected) {
    painter->fillPath(cardPath,
                      isLight ? QColor(0x1E, 0x66, 0xF5, 0x1A)
                              : QColor(0x89, 0xB4, 0xFA, 0x24)); // Accent tint
    painter->setPen(QPen(isLight ? QColor(0x1E, 0x66, 0xF5, 0x88)
                                 : QColor(0x89, 0xB4, 0xFA, 0x88),
                         1.2));
    painter->drawPath(cardPath);
  } else if (isHovered) {
    painter->fillPath(cardPath, isLight ? QColor(0xF4, 0xF5, 0xF8)
                                        : QColor(0x31, 0x32, 0x44, 0xAA));
    painter->setPen(
        QPen(isLight ? QColor(0xBC, 0xC0, 0xCC) : QColor(0x45, 0x47, 0x5A), 1));
    painter->drawPath(cardPath);
  } else {
    painter->fillPath(cardPath, isLight ? QColor(0xFF, 0xFF, 0xFF)
                                        : QColor(0x23, 0x26, 0x34, 0x88));
    painter->setPen(QPen(isLight ? QColor(0xDC, 0xE0, 0xE8)
                                 : QColor(0x36, 0x3A, 0x4F, 0x88),
                         1));
    painter->drawPath(cardPath);
  }

  // 2. Icon danh mục bên trái (36x36 container)
  const int catInt = index.data(ItemListModel::CategoryRole).toInt();
  const auto cat = static_cast<models::ItemCategory>(catInt);
  const QString iconPath = models::categoryIconPath(cat);
  const QRect iconRect(rect.left() + 12, rect.top() + (rect.height() - 36) / 2,
                       36, 36);

  QPainterPath iconBgPath;
  iconBgPath.addRoundedRect(iconRect, 8, 8);
  painter->fillPath(iconBgPath, isLight ? QColor(0xEA, 0xED, 0xF3)
                                        : QColor(0x18, 0x18, 0x25));

  const QIcon itemIcon(iconPath);
  itemIcon.paint(painter, iconRect.adjusted(7, 7, -7, -7), Qt::AlignCenter);

  // 3. Tính toán không gian Badge chính xác tránh chồng lấn (Overlapping)
  const bool isFav = index.data(ItemListModel::IsFavoriteRole).toBool();
  const bool hasTotp = index.data(ItemListModel::HasTotpRole).toBool();

  int badgesWidth = 0;
  if (isFav) {
    badgesWidth += 22; // 16px star + 6px spacing
  }
  if (hasTotp) {
    badgesWidth += 40; // 34px badge + 6px spacing
  }

  const int textLeft = iconRect.right() + 12;
  const int rightReserved =
      isHovered ? 76 : (badgesWidth > 0 ? (badgesWidth + 8) : 12);
  const int textWidth = std::max(20, rect.right() - textLeft - rightReserved);

  // 4. Tiêu đề và Username/Subtitle
  const QString title = index.data(ItemListModel::TitleRole).toString();
  QString subtitle = index.data(ItemListModel::UsernameRole).toString();
  if (subtitle.isEmpty()) {
    subtitle = index.data(ItemListModel::WebsiteRole).toString();
  }
  if (subtitle.isEmpty()) {
    subtitle = models::categoryDisplayName(cat);
  }

  // Dòng 1: Title (Rút gọn tự động với dấu ... trước mép badge)
  painter->setPen(
      isLight
          ? (isSelected ? QColor(0x1E, 0x66, 0xF5) : QColor(0x31, 0x32, 0x44))
          : (isSelected ? QColor(0xFF, 0xFF, 0xFF) : QColor(0xCD, 0xD6, 0xF4)));
  QFont titleFont = option.font;
  titleFont.setBold(true);
  titleFont.setPointSize(10);
  painter->setFont(titleFont);

  const QFontMetrics fmTitle(titleFont);
  const QString elidedTitle =
      fmTitle.elidedText(title, Qt::ElideRight, textWidth);
  painter->drawText(QRect(textLeft, rect.top() + 13, textWidth, 20),
                    Qt::AlignLeft | Qt::AlignVCenter, elidedTitle);

  // Dòng 2: Subtitle
  painter->setPen(isLight ? QColor(0x6C, 0x6F, 0x85)
                          : QColor(0x93, 0x99, 0xB2));
  QFont subFont = option.font;
  subFont.setBold(false);
  subFont.setPointSize(9);
  painter->setFont(subFont);

  const QFontMetrics fmSub(subFont);
  const QString elidedSub =
      fmSub.elidedText(subtitle, Qt::ElideRight, textWidth);
  painter->drawText(QRect(textLeft, rect.top() + 35, textWidth, 18),
                    Qt::AlignLeft | Qt::AlignVCenter, elidedSub);

  // 5. Badges & Icons bên phải (Chỉ vẽ khi không Hover)
  if (!isHovered) {
    int badgeX = rect.right() - 10;

    if (isFav) {
      const QRect starRect(badgeX - 16, rect.top() + (rect.height() - 16) / 2,
                           16, 16);
      const QIcon starIcon(QStringLiteral(":/icons/star_filled.svg"));
      starIcon.paint(painter, starRect);
      badgeX -= 22;
    }

    if (hasTotp) {
      const QRect totpRect(badgeX - 34, rect.top() + (rect.height() - 18) / 2,
                           34, 18);
      QPainterPath badgePath;
      badgePath.addRoundedRect(totpRect, 4, 4);
      painter->fillPath(badgePath,
                        isLight ? QColor(0x88, 0x39, 0xEF, 0x22)
                                : QColor(0x8B, 0x5C, 0xF6, 0x33)); // Purple
      painter->setPen(isLight ? QColor(0x88, 0x39, 0xEF)
                              : QColor(0xA7, 0x8B, 0xFA));
      QFont badgeFont(QStringLiteral("sans-serif"), 7, QFont::Bold);
      painter->setFont(badgeFont);
      painter->drawText(totpRect, Qt::AlignCenter, QStringLiteral("2FA"));
    }
  } else {
    // 6. Hover Actions (Nút Copy Username & Copy Password)
    const int btnY = rect.top() + (rect.height() - 26) / 2;
    const QRect copyPassRect(rect.right() - 34, btnY, 26, 26);
    const QRect copyUserRect(rect.right() - 66, btnY, 26, 26);

    QPainterPath btnPassPath;
    btnPassPath.addRoundedRect(copyPassRect, 5, 5);
    painter->fillPath(btnPassPath, isLight ? QColor(0xDC, 0xE0, 0xE8)
                                           : QColor(0x31, 0x32, 0x44));
    painter->setPen(
        QPen(isLight ? QColor(0xCC, 0xD0, 0xDA) : QColor(0x45, 0x47, 0x5A), 1));
    painter->drawPath(btnPassPath);
    QIcon(QStringLiteral(":/icons/lock.svg"))
        .paint(painter, copyPassRect.adjusted(5, 5, -5, -5));

    QPainterPath btnUserPath;
    btnUserPath.addRoundedRect(copyUserRect, 5, 5);
    painter->fillPath(btnUserPath, isLight ? QColor(0xDC, 0xE0, 0xE8)
                                           : QColor(0x31, 0x32, 0x44));
    painter->setPen(
        QPen(isLight ? QColor(0xCC, 0xD0, 0xDA) : QColor(0x45, 0x47, 0x5A), 1));
    painter->drawPath(btnUserPath);
    QIcon(QStringLiteral(":/icons/copy.svg"))
        .paint(painter, copyUserRect.adjusted(5, 5, -5, -5));
  }

  painter->restore();
}

bool ItemCardDelegate::editorEvent(QEvent *event, QAbstractItemModel *model,
                                   const QStyleOptionViewItem &option,
                                   const QModelIndex &index) {
  if (!index.isValid()) {
    return false;
  }

  if (event->type() == QEvent::MouseButtonRelease) {
    auto *mouseEvent = static_cast<QMouseEvent *>(event);
    if (mouseEvent->button() == Qt::LeftButton) {
      const QRect rect = option.rect.adjusted(6, 3, -6, -3);
      const int btnY = rect.top() + (rect.height() - 26) / 2;
      const QRect copyPassRect(rect.right() - 34, btnY, 26, 26);
      const QRect copyUserRect(rect.right() - 66, btnY, 26, 26);

      const QString itemId = index.data(ItemListModel::IdRole).toString();

      if (copyPassRect.contains(mouseEvent->pos())) {
        emit quickCopyPasswordRequested(itemId);
        return true;
      }
      if (copyUserRect.contains(mouseEvent->pos())) {
        emit quickCopyUsernameRequested(itemId);
        return true;
      }
    }
  }

  return QStyledItemDelegate::editorEvent(event, model, option, index);
}

} // namespace svm::views
