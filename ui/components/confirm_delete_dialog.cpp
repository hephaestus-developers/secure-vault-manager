/**
 * @file ui/components/confirm_delete_dialog.cpp
 * @brief Hiện thực ConfirmDeleteDialog xác nhận xóa mục dữ liệu với thẻ xem
 * trước trực quan.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "confirm_delete_dialog.h"

#include "app/theme_manager.h"
#include "components/dialog_drag_helper.h"
#include "components/elided_label.h"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QVBoxLayout>

namespace svm::components {

ConfirmDeleteDialog::ConfirmDeleteDialog(const models::UiVaultItem &item,
                                         bool permanent, QWidget *parent)
    : QDialog(parent ? parent->window() : nullptr,
              Qt::FramelessWindowHint | Qt::Dialog),
      m_item(item), m_permanent(permanent) {
  setObjectName(QStringLiteral("ConfirmDeleteDialog"));
  setAttribute(Qt::WA_TranslucentBackground, false);
  setModal(false);
  setWindowModality(Qt::NonModal);
  setProperty("svmModalDialog", true);
  setFixedWidth(440);

  setupUI();

  connect(&app::ThemeManager::instance(), &app::ThemeManager::themeChanged,
          this, [this]() { updateThemeIcons(); });
}

void ConfirmDeleteDialog::setupUI() {
  auto *rootLayout = new QVBoxLayout(this);
  rootLayout->setContentsMargins(0, 0, 0, 0);

  m_cardWidget = new QWidget(this);
  m_cardWidget->setObjectName(QStringLiteral("NotificationDialogCard"));
  m_cardWidget->setAttribute(Qt::WA_StyledBackground, true);

  auto *cardLayout = new QVBoxLayout(m_cardWidget);
  cardLayout->setContentsMargins(0, 0, 0, 16);
  cardLayout->setSpacing(0);

  // 1. Thanh tiêu đề
  m_headerWidget = new QWidget(m_cardWidget);
  m_headerWidget->setObjectName(QStringLiteral("NotificationDialogHeader"));
  m_headerWidget->setAttribute(Qt::WA_StyledBackground, true);
  m_headerWidget->setFixedHeight(38);

  auto *headerLayout = new QHBoxLayout(m_headerWidget);
  headerLayout->setContentsMargins(16, 0, 0, 0);
  headerLayout->setSpacing(8);

  m_headerIconLabel = new QLabel(m_headerWidget);
  m_headerIconLabel->setFixedSize(16, 16);
  headerLayout->addWidget(m_headerIconLabel);

  m_titleLabel =
      new QLabel(m_permanent ? tr("Delete Permanently?") : tr("Move to Trash?"),
                 m_headerWidget);
  m_titleLabel->setObjectName(QStringLiteral("NotificationDialogTitle"));
  headerLayout->addWidget(m_titleLabel, 1);

  m_closeBtn = new QPushButton(m_headerWidget);
  m_closeBtn->setObjectName(QStringLiteral("WinCloseBtn"));
  m_closeBtn->setFixedSize(42, 38);
  m_closeBtn->setIconSize(QSize(12, 12));
  m_closeBtn->setFlat(true);
  m_closeBtn->setFocusPolicy(Qt::NoFocus);
  m_closeBtn->setCursor(Qt::PointingHandCursor);
  connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::reject);
  headerLayout->addWidget(m_closeBtn);

  cardLayout->addWidget(m_headerWidget);
  DialogDragHelper::makeDraggable(m_headerWidget, this);

  // 2. Thân hộp thoại
  auto *bodyWidget = new QWidget(m_cardWidget);
  auto *bodyLayout = new QVBoxLayout(bodyWidget);
  bodyLayout->setContentsMargins(18, 16, 18, 14);
  bodyLayout->setSpacing(14);

  // Hàng cảnh báo chính (Badge đỏ + Câu hỏi & Giải thích)
  auto *promptRow = new QHBoxLayout();
  promptRow->setSpacing(14);

  auto *badgeContainer = new QWidget(bodyWidget);
  badgeContainer->setObjectName(
      m_permanent ? QStringLiteral("ConfirmDeletePermanentBadge")
                  : QStringLiteral("ConfirmDeleteTrashBadge"));
  badgeContainer->setAttribute(Qt::WA_StyledBackground, true);
  badgeContainer->setFixedSize(42, 42);

  auto *badgeLayout = new QVBoxLayout(badgeContainer);
  badgeLayout->setContentsMargins(0, 0, 0, 0);
  badgeLayout->setAlignment(Qt::AlignCenter);

  m_badgeIconLabel = new QLabel(badgeContainer);
  badgeLayout->addWidget(m_badgeIconLabel);
  promptRow->addWidget(badgeContainer, 0, Qt::AlignTop);

  auto *textCol = new QVBoxLayout();
  textCol->setSpacing(5);

  auto *headingLabel =
      new QLabel(m_permanent ? tr("Permanently delete this item?")
                             : tr("Move this item to Trash?"),
                 bodyWidget);
  headingLabel->setObjectName(QStringLiteral("ConfirmDeleteHeading"));
  headingLabel->setWordWrap(true);
  textCol->addWidget(headingLabel);

  auto *descLabel = new QLabel(
      m_permanent
          ? tr("This action cannot be undone. All credentials, notes, and "
               "history for this item will be permanently erased.")
          : tr("This item will be moved to Trash and can be restored anytime "
               "from the Trash folder."),
      bodyWidget);
  descLabel->setObjectName(QStringLiteral("ConfirmDeleteDesc"));
  descLabel->setWordWrap(true);
  textCol->addWidget(descLabel);

  promptRow->addLayout(textCol, 1);
  bodyLayout->addLayout(promptRow);

  // Thẻ xem trước mục sắp xóa
  auto *previewCard = new QWidget(bodyWidget);
  previewCard->setObjectName(QStringLiteral("ConfirmDeleteItemPreview"));
  previewCard->setAttribute(Qt::WA_StyledBackground, true);
  auto *previewLayout = new QHBoxLayout(previewCard);
  previewLayout->setContentsMargins(12, 10, 12, 10);
  previewLayout->setSpacing(10);

  auto *itemIconBox = new QWidget(previewCard);
  itemIconBox->setObjectName(QStringLiteral("ConfirmDeleteItemIconBox"));
  itemIconBox->setAttribute(Qt::WA_StyledBackground, true);
  itemIconBox->setFixedSize(34, 34);
  auto *itemIconLayout = new QVBoxLayout(itemIconBox);
  itemIconLayout->setContentsMargins(0, 0, 0, 0);
  itemIconLayout->setAlignment(Qt::AlignCenter);

  m_itemIconLabel = new QLabel(itemIconBox);
  itemIconLayout->addWidget(m_itemIconLabel);
  previewLayout->addWidget(itemIconBox);

  auto *itemMetaCol = new QVBoxLayout();
  itemMetaCol->setSpacing(2);

  const QString displayTitle =
      m_item.title.trimmed().isEmpty() ? tr("Untitled Item") : m_item.title;
  auto *itemTitleLbl = new ElidedLabel(displayTitle, previewCard);
  itemTitleLbl->setObjectName(QStringLiteral("ConfirmDeleteItemTitle"));
  itemMetaCol->addWidget(itemTitleLbl);

  QString detailText = m_item.username.trimmed();
  if (detailText.isEmpty()) {
    detailText = m_item.websiteUrl.trimmed();
  }
  if (detailText.isEmpty()) {
    detailText = m_item.wifiSsid.trimmed();
  }
  if (detailText.isEmpty()) {
    detailText = m_item.dbHost.trimmed();
  }
  if (detailText.isEmpty()) {
    detailText = m_item.sshFingerprint.trimmed();
  }
  const QString catLabel = models::categoryDisplayName(m_item.category);
  const QString subText =
      detailText.isEmpty()
          ? catLabel
          : QStringLiteral("%1 - %2").arg(catLabel, detailText);
  auto *itemSubLbl = new ElidedLabel(subText, previewCard);
  itemSubLbl->setObjectName(QStringLiteral("ConfirmDeleteItemSub"));
  itemMetaCol->addWidget(itemSubLbl);

  previewLayout->addLayout(itemMetaCol, 1);
  bodyLayout->addWidget(previewCard);

  cardLayout->addWidget(bodyWidget, 1);

  // 3. Vùng nút bấm hành động
  auto *btnContainer = new QWidget(m_cardWidget);
  auto *btnLayout = new QHBoxLayout(btnContainer);
  btnLayout->setContentsMargins(18, 0, 18, 0);
  btnLayout->setSpacing(8);

  btnLayout->addStretch(1);

  m_cancelBtn = new QPushButton(tr("Cancel"), btnContainer);
  m_cancelBtn->setObjectName(QStringLiteral("NotificationCancelBtn"));
  m_cancelBtn->setFixedHeight(34);
  m_cancelBtn->setMinimumWidth(88);
  m_cancelBtn->setCursor(Qt::PointingHandCursor);
  m_cancelBtn->setDefault(true);
  connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
  btnLayout->addWidget(m_cancelBtn);

  m_confirmBtn = new QPushButton(m_permanent ? tr("Delete Permanently")
                                             : tr("Move to Trash"),
                                 btnContainer);
  m_confirmBtn->setObjectName(QStringLiteral("NotificationDangerBtn"));
  m_confirmBtn->setFixedHeight(34);
  m_confirmBtn->setMinimumWidth(116);
  m_confirmBtn->setCursor(Qt::PointingHandCursor);
  connect(m_confirmBtn, &QPushButton::clicked, this, &QDialog::accept);
  btnLayout->addWidget(m_confirmBtn);

  cardLayout->addWidget(btnContainer);
  rootLayout->addWidget(m_cardWidget);

  updateThemeIcons();
  m_cancelBtn->setFocus();
}

void ConfirmDeleteDialog::updateThemeIcons() {
  auto &tm = app::ThemeManager::instance();
  if (m_headerIconLabel) {
    m_headerIconLabel->setPixmap(tm.icon(m_permanent
                                             ? QStringLiteral("watchtower_weak")
                                             : QStringLiteral("trash"))
                                     .pixmap(16, 16));
  }
  if (m_closeBtn) {
    m_closeBtn->setIcon(tm.icon(QStringLiteral("win_close")));
  }
  if (m_badgeIconLabel) {
    m_badgeIconLabel->setPixmap(tm.icon(m_permanent
                                            ? QStringLiteral("watchtower_weak")
                                            : QStringLiteral("trash"))
                                    .pixmap(20, 20));
  }
  if (m_itemIconLabel) {
    m_itemIconLabel->setPixmap(
        tm.icon(models::categoryIconName(m_item.category)).pixmap(18, 18));
  }
}

void ConfirmDeleteDialog::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Escape) {
    reject();
    return;
  }
  QDialog::keyPressEvent(event);
}

bool ConfirmDeleteDialog::confirm(QWidget *parent,
                                  const models::UiVaultItem &item,
                                  bool permanent) {
  ConfirmDeleteDialog dlg(item, permanent, parent);
  return DialogDragHelper::execDialog(dlg) == QDialog::Accepted;
}

} // namespace svm::components
