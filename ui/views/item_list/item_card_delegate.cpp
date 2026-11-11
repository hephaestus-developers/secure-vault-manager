/**
 * @file ui/views/item_list/item_card_delegate.cpp
 * @brief Hiện thực Card Delegate vẽ bản ghi đa tầng thẩm mỹ cao.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "item_card_delegate.h"
#include "app/settings_manager.h"
#include "app/theme_manager.h"
#include "core/core_bridge.h"
#include "item_list_model.h"

#include <QIcon>
#include <QImage>
#include <QPainter>
#include <QPainterPath>

namespace svm::views {

ItemCardDelegate::ItemCardDelegate(QObject *parent)
    : QStyledItemDelegate(parent) {}

QSize ItemCardDelegate::sizeHint(const QStyleOptionViewItem &option,
                                 const QModelIndex &index) const {
  Q_UNUSED(option);
  Q_UNUSED(index);
  const bool compact = (app::SettingsManager::instance().listDensity() ==
                        app::ListDensity::Compact);
  return {0, compact ? 56 : 72};
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
  const bool compact = (app::SettingsManager::instance().listDensity() ==
                        app::ListDensity::Compact);
  const QRect rect =
      option.rect.adjusted(6, compact ? 4 : 5, -6, compact ? -4 : -5);
  const bool isSelected = (option.state & QStyle::State_Selected);
  const bool isHovered = (option.state & QStyle::State_MouseOver);

  // 1. Vẽ nền Card bo góc 8px (Elevated Surface cho Dark Theme, Pure White cho
  // Light Theme)
  QPainterPath cardPath;
  cardPath.addRoundedRect(rect, 8, 8);

  if (isSelected) {
    painter->fillPath(cardPath, isLight ? QColor(0xFF, 0xFF, 0xFF)
                                        : QColor(0x89, 0xB4, 0xFA, 0x24));
    painter->setPen(QPen(isLight ? QColor(0x2F, 0x35, 0x40)
                                 : QColor(0x89, 0xB4, 0xFA, 0x88),
                         1.5));
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
    painter->setPen(QPen(isLight ? QColor(0xCC, 0xD0, 0xDA)
                                 : QColor(0x36, 0x3A, 0x4F, 0x88),
                         1));
    painter->drawPath(cardPath);
  }

  // 2. Icon danh mục / Favicon bên trái (36x36 container)
  const int catInt = index.data(ItemListModel::CategoryRole).toInt();
  const auto cat = static_cast<models::ItemCategory>(catInt);
  const QString websiteUrl = index.data(ItemListModel::WebsiteRole).toString();
  const QRect iconRect(rect.left() + 12, rect.top() + (rect.height() - 36) / 2,
                       36, 36);

  QPainterPath iconBgPath;
  iconBgPath.addRoundedRect(iconRect, 8, 8);
  painter->fillPath(iconBgPath, isLight ? QColor(0xEA, 0xED, 0xF3)
                                        : QColor(0x18, 0x18, 0x25));

  bool paintedFavicon = false;
  if (!websiteUrl.trimmed().isEmpty() &&
      app::SettingsManager::instance().faviconFetchEnabled()) {
    auto &bridge = core::CoreBridge::instance();
    if (bridge.isInitialized()) {
      const QImage fav = bridge.cachedFavicon(websiteUrl);
      if (!fav.isNull()) {
        const QRect bounds = iconRect.adjusted(6, 6, -6, -6);
        const QSize fitted =
            fav.size().scaled(bounds.size(), Qt::KeepAspectRatio);
        const QRect drawRect(
            bounds.left() + (bounds.width() - fitted.width()) / 2,
            bounds.top() + (bounds.height() - fitted.height()) / 2,
            fitted.width(), fitted.height());
        painter->save();
        painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
        QPainterPath clipPath;
        clipPath.addRoundedRect(drawRect, 4, 4);
        painter->setClipPath(clipPath);
        painter->drawImage(drawRect, fav);
        painter->restore();
        paintedFavicon = true;
      } else {
        bridge.requestFaviconAsync(websiteUrl);
      }
    }
  }

  if (!paintedFavicon) {
    const QIcon itemIcon =
        app::ThemeManager::instance().icon(models::categoryIconName(cat));
    itemIcon.paint(painter, iconRect.adjusted(7, 7, -7, -7), Qt::AlignCenter);
  }

  // 3. Tính toán không gian chữ & cột Favorite bên phải (badge 2FA gắn liền sau
  // Title)
  const bool isFav = index.data(ItemListModel::IsFavoriteRole).toBool();
  const bool hasTotp = index.data(ItemListModel::HasTotpRole).toBool();

  const int textLeft = iconRect.right() + 12;
  const int rightReserved = isFav ? 32 : 12;
  const int contentWidth =
      std::max(20, rect.right() - textLeft - rightReserved);

  // 4. Tiêu đề và Username/Subtitle
  const QString title = index.data(ItemListModel::TitleRole).toString();
  QString subtitle = index.data(ItemListModel::UsernameRole).toString();
  if (subtitle.isEmpty()) {
    subtitle = index.data(ItemListModel::WebsiteRole).toString();
  }
  if (subtitle.isEmpty()) {
    subtitle = models::categoryDisplayName(cat);
  }

  // Dòng 1: Title + Badge 2FA (nếu có)
  const int titleY = rect.top() + (compact ? 6 : 13);
  constexpr int kTotpBadgeW = 30;
  constexpr int kTotpBadgeH = 16;
  constexpr int kTotpBadgeGap = 6;

  painter->setPen(
      isLight
          ? (isSelected ? QColor(0x1E, 0x1E, 0x2E) : QColor(0x31, 0x32, 0x44))
          : (isSelected ? QColor(0xFF, 0xFF, 0xFF) : QColor(0xCD, 0xD6, 0xF4)));
  QFont titleFont = option.font;
  titleFont.setBold(true);
  titleFont.setPointSize(10);
  painter->setFont(titleFont);

  const QFontMetrics fmTitle(titleFont);
  const int titleMaxWidth =
      hasTotp ? std::max(20, contentWidth - kTotpBadgeW - kTotpBadgeGap)
              : contentWidth;
  const QString elidedTitle =
      fmTitle.elidedText(title, Qt::ElideRight, titleMaxWidth);
  painter->drawText(QRect(textLeft, titleY, titleMaxWidth, 20),
                    Qt::AlignLeft | Qt::AlignVCenter, elidedTitle);

  if (hasTotp) {
    const int drawnTitleW =
        std::min(titleMaxWidth, fmTitle.horizontalAdvance(elidedTitle));
    const QRect totpRect(textLeft + drawnTitleW + kTotpBadgeGap,
                         titleY + (20 - kTotpBadgeH) / 2, kTotpBadgeW,
                         kTotpBadgeH);
    QPainterPath badgePath;
    badgePath.addRoundedRect(totpRect, 4, 4);
    painter->fillPath(badgePath, isLight ? QColor(0x88, 0x39, 0xEF, 0x22)
                                         : QColor(0x8B, 0x5C, 0xF6, 0x33));
    painter->setPen(isLight ? QColor(0x88, 0x39, 0xEF)
                            : QColor(0xA7, 0x8B, 0xFA));
    QFont badgeFont(QStringLiteral("sans-serif"), 7, QFont::Bold);
    painter->setFont(badgeFont);
    painter->drawText(totpRect, Qt::AlignCenter, QStringLiteral("2FA"));
  }

  // Dòng 2: Subtitle
  painter->setPen(isLight ? QColor(0x6C, 0x6F, 0x85)
                          : QColor(0x93, 0x99, 0xB2));
  QFont subFont = option.font;
  subFont.setBold(false);
  subFont.setPointSize(9);
  painter->setFont(subFont);

  const QFontMetrics fmSub(subFont);
  const QString elidedSub =
      fmSub.elidedText(subtitle, Qt::ElideRight, contentWidth);
  painter->drawText(
      QRect(textLeft, rect.top() + (compact ? 25 : 35), contentWidth, 18),
      Qt::AlignLeft | Qt::AlignVCenter, elidedSub);

  // 5. Icon Favorite cố định sát mép phải
  if (isFav) {
    const QRect starRect(rect.right() - 26,
                         rect.top() + (rect.height() - 16) / 2, 16, 16);
    const QIcon starIcon(
        isLight ? QStringLiteral(":/icons/actions/star_filled_light.svg")
                : QStringLiteral(":/icons/actions/star_filled_dark.svg"));
    starIcon.paint(painter, starRect);
  }

  painter->restore();
}

} // namespace svm::views
