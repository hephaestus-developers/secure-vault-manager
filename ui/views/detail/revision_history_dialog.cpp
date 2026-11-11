/**
 * @file ui/views/detail/revision_history_dialog.cpp
 * @brief Hiện thực RevisionHistoryDialog với danh sách bản chụp theo theme và
 * khung xem trước thông tin trước khi khôi phục.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "revision_history_dialog.h"

#include "app/theme_manager.h"
#include "components/dialog_drag_helper.h"
#include "components/window_resize_handler.h"

#include <QDateTime>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QPainter>
#include <QPainterPath>
#include <QStyledItemDelegate>
#include <QVBoxLayout>
#include <algorithm>

namespace svm::views {

namespace {

enum RevisionItemRoles {
  RevNumberRole = Qt::UserRole + 1,
  RevTimestampRole,
  RevTitleRole,
  RevSubtitleRole,
  RevHasTotpRole,
  RevIsLatestRole
};

class RevisionItemDelegate : public QStyledItemDelegate {
public:
  explicit RevisionItemDelegate(QObject *parent = nullptr)
      : QStyledItemDelegate(parent) {}

  QSize sizeHint(const QStyleOptionViewItem &option,
                 const QModelIndex &index) const override {
    Q_UNUSED(option);
    Q_UNUSED(index);
    return {0, 60};
  }

  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override {
    if (!index.isValid()) {
      return;
    }

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    const bool isLight =
        (app::ThemeManager::instance().currentMode() == app::ThemeMode::Light);
    const QRect rect = option.rect.adjusted(2, 3, -2, -3);
    const bool isSelected = (option.state & QStyle::State_Selected);
    const bool isHovered = (option.state & QStyle::State_MouseOver);

    QPainterPath cardPath;
    cardPath.addRoundedRect(rect, 7, 7);

    if (isSelected) {
      painter->fillPath(cardPath, isLight ? QColor(0xFF, 0xFF, 0xFF)
                                          : QColor(0x89, 0xB4, 0xFA, 0x24));
      painter->setPen(QPen(isLight ? QColor(0x1E, 0x66, 0xF5)
                                   : QColor(0x89, 0xB4, 0xFA, 0xB0),
                           1.5));
      painter->drawPath(cardPath);
    } else if (isHovered) {
      painter->fillPath(cardPath, isLight ? QColor(0xEF, 0xF1, 0xF5)
                                          : QColor(0x31, 0x32, 0x44, 0xAA));
      painter->setPen(QPen(
          isLight ? QColor(0xBC, 0xC0, 0xCC) : QColor(0x45, 0x47, 0x5A), 1));
      painter->drawPath(cardPath);
    } else {
      painter->fillPath(cardPath, isLight ? QColor(0xFF, 0xFF, 0xFF)
                                          : QColor(0x1E, 0x1E, 0x2E));
      painter->setPen(QPen(
          isLight ? QColor(0xDC, 0xE0, 0xE8) : QColor(0x31, 0x32, 0x44), 1));
      painter->drawPath(cardPath);
    }

    const int revNum = index.data(RevNumberRole).toInt();
    const QString tsText = index.data(RevTimestampRole).toString();
    const QString titleText = index.data(RevTitleRole).toString();
    const QString subText = index.data(RevSubtitleRole).toString();
    const bool hasTotp = index.data(RevHasTotpRole).toBool();
    const bool isLatest = index.data(RevIsLatestRole).toBool();

    // Badge số phiên bản (#8)
    const QString revBadgeText = QStringLiteral("#%1").arg(revNum);
    QFont badgeFont = option.font;
    badgeFont.setBold(true);
    badgeFont.setPointSize(9);
    const QFontMetrics fmBadge(badgeFont);
    const int badgeW =
        std::max(34, fmBadge.horizontalAdvance(revBadgeText) + 14);
    const QRect badgeRect(rect.left() + 12,
                          rect.top() + (rect.height() - 26) / 2, badgeW, 26);
    QPainterPath badgePath;
    badgePath.addRoundedRect(badgeRect, 6, 6);
    painter->fillPath(badgePath, isLight ? QColor(0x1E, 0x66, 0xF5, 0x1A)
                                         : QColor(0x89, 0xB4, 0xFA, 0x26));
    painter->setPen(isLight ? QColor(0x1E, 0x66, 0xF5)
                            : QColor(0x89, 0xB4, 0xFA));
    painter->setFont(badgeFont);
    painter->drawText(badgeRect, Qt::AlignCenter, revBadgeText);

    // Các tag nhỏ bên phải (Latest, 2FA)
    int rightEdge = rect.right() - 12;
    if (isLatest) {
      const QString latestLabel = QObject::tr("Latest");
      QFont tagFont(QStringLiteral("sans-serif"), 7, QFont::Bold);
      const QFontMetrics fmTag(tagFont);
      const int tagW = fmTag.horizontalAdvance(latestLabel) + 14;
      const QRect tagRect(rightEdge - tagW, rect.top() + 10, tagW, 18);
      QPainterPath tagPath;
      tagPath.addRoundedRect(tagRect, 4, 4);
      painter->fillPath(tagPath, isLight ? QColor(0x40, 0xA0, 0x2B, 0x20)
                                         : QColor(0xA6, 0xE3, 0xA1, 0x26));
      painter->setPen(isLight ? QColor(0x40, 0xA0, 0x2B)
                              : QColor(0xA6, 0xE3, 0xA1));
      painter->setFont(tagFont);
      painter->drawText(tagRect, Qt::AlignCenter, latestLabel);
      rightEdge = tagRect.left() - 6;
    }

    if (hasTotp) {
      const QString totpLabel = QStringLiteral("2FA");
      QFont tagFont(QStringLiteral("sans-serif"), 7, QFont::Bold);
      const int tagW = 30;
      const QRect tagRect(rightEdge - tagW, rect.top() + 10, tagW, 18);
      QPainterPath tagPath;
      tagPath.addRoundedRect(tagRect, 4, 4);
      painter->fillPath(tagPath, isLight ? QColor(0x88, 0x39, 0xEF, 0x1E)
                                         : QColor(0x8B, 0x5C, 0xF6, 0x33));
      painter->setPen(isLight ? QColor(0x88, 0x39, 0xEF)
                              : QColor(0xA7, 0x8B, 0xFA));
      painter->setFont(tagFont);
      painter->drawText(tagRect, Qt::AlignCenter, totpLabel);
      rightEdge = tagRect.left() - 6;
    }

    // Nội dung 2 dòng bên phải badge
    const int textLeft = badgeRect.right() + 12;
    const int tsWidth = std::max(40, rightEdge - textLeft);
    const int fullWidth = std::max(40, rect.right() - 12 - textLeft);

    QFont tsFont = option.font;
    tsFont.setBold(true);
    tsFont.setPointSize(10);
    painter->setFont(tsFont);
    painter->setPen(isLight ? QColor(0x1E, 0x1E, 0x2E)
                            : (isSelected ? QColor(0xFF, 0xFF, 0xFF)
                                          : QColor(0xCD, 0xD6, 0xF4)));
    const QString elidedTs =
        QFontMetrics(tsFont).elidedText(tsText, Qt::ElideRight, tsWidth);
    painter->drawText(QRect(textLeft, rect.top() + 9, tsWidth, 18),
                      Qt::AlignLeft | Qt::AlignVCenter, elidedTs);

    QFont subFont = option.font;
    subFont.setBold(false);
    subFont.setPointSize(9);
    painter->setFont(subFont);
    painter->setPen(isLight ? QColor(0x6C, 0x6F, 0x85)
                            : QColor(0x93, 0x99, 0xB2));
    const QString combinedSub =
        subText.isEmpty() ? titleText
                          : QStringLiteral("%1  •  %2").arg(titleText, subText);
    const QString elidedSub = QFontMetrics(subFont).elidedText(
        combinedSub, Qt::ElideRight, fullWidth);
    painter->drawText(QRect(textLeft, rect.top() + 29, fullWidth, 16),
                      Qt::AlignLeft | Qt::AlignVCenter, elidedSub);

    painter->restore();
  }
};

} // namespace

RevisionHistoryDialog::RevisionHistoryDialog(
    const QList<models::UiItemRevision> &revisions, QWidget *parent)
    : QDialog(parent ? parent->window() : nullptr,
              Qt::FramelessWindowHint | Qt::Dialog),
      m_revisions(revisions) {
  setObjectName(QStringLiteral("RevisionHistoryDialog"));
  setAttribute(Qt::WA_TranslucentBackground, false);
  setModal(false);
  setWindowModality(Qt::NonModal);
  setProperty("svmModalDialog", true);
  setMinimumSize(460, 380);
  resize(520, 480);

  setupUI();
  populateRevisions();
  updateThemeIcons();

  new components::WindowResizeHandler(this);
  connect(&app::ThemeManager::instance(), &app::ThemeManager::themeChanged,
          this, [this]() {
            updateThemeIcons();
            if (m_listWidget && m_listWidget->viewport()) {
              m_listWidget->viewport()->update();
            }
          });
}

void RevisionHistoryDialog::setupUI() {
  auto *rootLayout = new QVBoxLayout(this);
  rootLayout->setContentsMargins(0, 0, 0, 0);

  m_cardWidget = new QWidget(this);
  m_cardWidget->setObjectName(QStringLiteral("NotificationDialogCard"));
  m_cardWidget->setAttribute(Qt::WA_StyledBackground, true);

  auto *cardLayout = new QVBoxLayout(m_cardWidget);
  cardLayout->setContentsMargins(0, 0, 0, 16);
  cardLayout->setSpacing(0);

  // 1. Thanh tiêu đề tích hợp
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

  m_titleLabel = new QLabel(tr("Revision History"), m_headerWidget);
  m_titleLabel->setObjectName(QStringLiteral("NotificationDialogTitle"));
  headerLayout->addWidget(m_titleLabel, 1);

  m_closeHeaderBtn = new QPushButton(m_headerWidget);
  m_closeHeaderBtn->setObjectName(QStringLiteral("WinCloseBtn"));
  m_closeHeaderBtn->setFixedSize(42, 38);
  m_closeHeaderBtn->setIconSize(QSize(12, 12));
  m_closeHeaderBtn->setFlat(true);
  m_closeHeaderBtn->setFocusPolicy(Qt::NoFocus);
  m_closeHeaderBtn->setCursor(Qt::PointingHandCursor);
  connect(m_closeHeaderBtn, &QPushButton::clicked, this, &QDialog::reject);
  headerLayout->addWidget(m_closeHeaderBtn);

  cardLayout->addWidget(m_headerWidget);
  components::DialogDragHelper::makeDraggable(m_headerWidget, this);

  // 2. Nội dung thân hộp thoại
  auto *bodyWidget = new QWidget(m_cardWidget);
  auto *bodyLayout = new QVBoxLayout(bodyWidget);
  bodyLayout->setContentsMargins(18, 16, 18, 14);
  bodyLayout->setSpacing(12);

  m_subtitleLabel =
      new QLabel(m_revisions.isEmpty()
                     ? tr("No revision snapshots recorded for this item yet.")
                     : tr("Select a previous revision snapshot to restore:"),
                 bodyWidget);
  m_subtitleLabel->setObjectName(QStringLiteral("NotificationDialogMessage"));
  m_subtitleLabel->setWordWrap(true);
  bodyLayout->addWidget(m_subtitleLabel);

  // Danh sách bản chụp
  m_listWidget = new QListWidget(bodyWidget);
  m_listWidget->setObjectName(QStringLiteral("RevisionHistoryList"));
  m_listWidget->setItemDelegate(new RevisionItemDelegate(m_listWidget));
  m_listWidget->setSelectionMode(QAbstractItemView::SingleSelection);
  m_listWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
  m_listWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_listWidget->setMouseTracking(true);
  m_listWidget->setFocusPolicy(Qt::StrongFocus);
  connect(m_listWidget, &QListWidget::currentRowChanged, this,
          &RevisionHistoryDialog::updatePreview);
  connect(m_listWidget, &QListWidget::itemDoubleClicked, this,
          [this](QListWidgetItem *) { acceptSelection(); });
  bodyLayout->addWidget(m_listWidget, 1);

  // Trạng thái rỗng
  m_emptyBox = new QWidget(bodyWidget);
  m_emptyBox->setObjectName(QStringLiteral("RevisionEmptyBox"));
  m_emptyBox->setAttribute(Qt::WA_StyledBackground, true);
  auto *emptyLayout = new QVBoxLayout(m_emptyBox);
  emptyLayout->setContentsMargins(20, 28, 20, 28);
  emptyLayout->setSpacing(10);
  emptyLayout->setAlignment(Qt::AlignCenter);

  m_emptyIconLabel = new QLabel(m_emptyBox);
  m_emptyIconLabel->setFixedSize(32, 32);
  m_emptyIconLabel->setAlignment(Qt::AlignCenter);
  emptyLayout->addWidget(m_emptyIconLabel, 0, Qt::AlignCenter);

  m_emptyDescLabel = new QLabel(
      tr("No revision snapshots recorded for this item yet."), m_emptyBox);
  m_emptyDescLabel->setObjectName(QStringLiteral("RevisionPreviewMeta"));
  m_emptyDescLabel->setAlignment(Qt::AlignCenter);
  m_emptyDescLabel->setWordWrap(true);
  emptyLayout->addWidget(m_emptyDescLabel);
  bodyLayout->addWidget(m_emptyBox, 1);

  // Thẻ xem trước snapshot đang chọn
  m_previewCard = new QWidget(bodyWidget);
  m_previewCard->setObjectName(QStringLiteral("RevisionPreviewCard"));
  m_previewCard->setAttribute(Qt::WA_StyledBackground, true);
  auto *previewLayout = new QVBoxLayout(m_previewCard);
  previewLayout->setContentsMargins(14, 10, 14, 10);
  previewLayout->setSpacing(4);

  m_previewHeaderLabel = new QLabel(tr("SNAPSHOT PREVIEW"), m_previewCard);
  m_previewHeaderLabel->setObjectName(QStringLiteral("RevisionPreviewHeader"));
  previewLayout->addWidget(m_previewHeaderLabel);

  m_previewTitleLabel = new QLabel(m_previewCard);
  m_previewTitleLabel->setObjectName(QStringLiteral("RevisionPreviewTitle"));
  previewLayout->addWidget(m_previewTitleLabel);

  m_previewMetaLabel = new QLabel(m_previewCard);
  m_previewMetaLabel->setObjectName(QStringLiteral("RevisionPreviewMeta"));
  m_previewMetaLabel->setWordWrap(true);
  previewLayout->addWidget(m_previewMetaLabel);

  bodyLayout->addWidget(m_previewCard);
  cardLayout->addWidget(bodyWidget, 1);

  // 3. Hàng nút chân hộp thoại
  auto *btnContainer = new QWidget(m_cardWidget);
  auto *btnLayout = new QHBoxLayout(btnContainer);
  btnLayout->setContentsMargins(18, 0, 18, 0);
  btnLayout->setSpacing(8);
  btnLayout->addStretch(1);

  m_closeBtn = new QPushButton(tr("Close"), btnContainer);
  m_closeBtn->setObjectName(QStringLiteral("NotificationCancelBtn"));
  m_closeBtn->setFixedHeight(34);
  m_closeBtn->setMinimumWidth(88);
  m_closeBtn->setCursor(Qt::PointingHandCursor);
  connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::reject);
  btnLayout->addWidget(m_closeBtn);

  m_restoreBtn = new QPushButton(tr("Restore Selected"), btnContainer);
  m_restoreBtn->setObjectName(QStringLiteral("NotificationPrimaryBtn"));
  m_restoreBtn->setFixedHeight(34);
  m_restoreBtn->setMinimumWidth(136);
  m_restoreBtn->setCursor(Qt::PointingHandCursor);
  connect(m_restoreBtn, &QPushButton::clicked, this,
          &RevisionHistoryDialog::acceptSelection);
  btnLayout->addWidget(m_restoreBtn);

  cardLayout->addWidget(btnContainer);
  rootLayout->addWidget(m_cardWidget);
}

void RevisionHistoryDialog::populateRevisions() {
  m_listWidget->clear();
  const bool hasRevisions = !m_revisions.isEmpty();
  m_listWidget->setVisible(hasRevisions);
  m_previewCard->setVisible(hasRevisions);
  m_emptyBox->setVisible(!hasRevisions);
  m_restoreBtn->setEnabled(hasRevisions);

  if (!hasRevisions) {
    return;
  }

  for (int i = m_revisions.size() - 1; i >= 0; --i) {
    const auto &rev = m_revisions[i];
    const QDateTime dt = QDateTime::fromMSecsSinceEpoch(rev.timestamp);
    const QString title = rev.snapshot.title.trimmed().isEmpty()
                              ? tr("Untitled Item")
                              : rev.snapshot.title.trimmed();
    QString sub = rev.snapshot.username.trimmed();
    if (sub.isEmpty()) {
      sub = rev.snapshot.websiteUrl.trimmed();
    }
    if (sub.isEmpty()) {
      sub = models::categoryDisplayName(rev.snapshot.category);
    }

    auto *li = new QListWidgetItem(m_listWidget);
    li->setData(Qt::UserRole, rev.revision);
    li->setData(RevNumberRole, rev.revision);
    li->setData(RevTimestampRole,
                dt.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    li->setData(RevTitleRole, title);
    li->setData(RevSubtitleRole, sub);
    li->setData(RevHasTotpRole, rev.snapshot.hasTotp);
    li->setData(RevIsLatestRole, i == m_revisions.size() - 1);
  }

  m_listWidget->setCurrentRow(0);
  updatePreview(0);
}

void RevisionHistoryDialog::updatePreview(int row) {
  if (row < 0 || row >= m_revisions.size()) {
    m_restoreBtn->setEnabled(false);
    return;
  }
  const int revIdx = m_revisions.size() - 1 - row;
  const auto &rev = m_revisions[revIdx];
  const auto &snap = rev.snapshot;

  const QString title = snap.title.trimmed().isEmpty() ? tr("Untitled Item")
                                                       : snap.title.trimmed();
  m_previewTitleLabel->setText(
      QStringLiteral("%1  (%2)")
          .arg(title, models::categoryDisplayName(snap.category)));

  QStringList parts;
  if (!snap.username.trimmed().isEmpty()) {
    parts << snap.username.trimmed();
  }
  if (!snap.websiteUrl.trimmed().isEmpty()) {
    parts << snap.websiteUrl.trimmed();
  }
  if (!snap.password.isEmpty()) {
    parts << QStringLiteral("••••••••");
  }
  if (snap.hasTotp) {
    parts << QStringLiteral("2FA");
  }
  if (!snap.customFields.isEmpty()) {
    parts << tr("%1 custom fields").arg(snap.customFields.size());
  }
  m_previewMetaLabel->setText(parts.isEmpty()
                                  ? tr("No additional details")
                                  : parts.join(QStringLiteral("  •  ")));
  m_restoreBtn->setEnabled(true);
}

void RevisionHistoryDialog::updateThemeIcons() {
  auto &tm = app::ThemeManager::instance();
  if (m_headerIconLabel) {
    m_headerIconLabel->setPixmap(
        tm.icon(QStringLiteral("sort_time")).pixmap(16, 16));
  }
  if (m_closeHeaderBtn) {
    m_closeHeaderBtn->setIcon(tm.icon(QStringLiteral("win_close")));
  }
  if (m_emptyIconLabel) {
    m_emptyIconLabel->setPixmap(
        tm.icon(QStringLiteral("sort_time")).pixmap(28, 28));
  }
}

void RevisionHistoryDialog::acceptSelection() {
  auto *cur = m_listWidget ? m_listWidget->currentItem() : nullptr;
  if (!cur) {
    return;
  }
  m_selectedRevision = cur->data(Qt::UserRole).toInt();
  if (m_selectedRevision > 0) {
    accept();
  }
}

void RevisionHistoryDialog::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Escape) {
    reject();
    return;
  }
  if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) &&
      m_restoreBtn && m_restoreBtn->isEnabled()) {
    acceptSelection();
    return;
  }
  QDialog::keyPressEvent(event);
}

int RevisionHistoryDialog::chooseRevision(
    QWidget *parent, const QList<models::UiItemRevision> &revisions) {
  RevisionHistoryDialog dlg(revisions, parent);
  if (components::DialogDragHelper::execDialog(dlg) == QDialog::Accepted) {
    return dlg.selectedRevision();
  }
  return -1;
}

} // namespace svm::views
