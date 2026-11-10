/**
 * @file ui/components/notification_dialog.cpp
 * @brief Hiện thực NotificationDialog hiện đại thay thế MessageBox hệ thống.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "notification_dialog.h"
#include "app/theme_manager.h"
#include "components/dialog_drag_helper.h"

#include <QHBoxLayout>
#include <QIcon>
#include <QKeyEvent>
#include <QVBoxLayout>

namespace svm::components {

NotificationDialog::NotificationDialog(QWidget *parent)
    : QDialog(parent ? parent->window() : nullptr,
              Qt::FramelessWindowHint | Qt::Dialog) {
  setObjectName(QStringLiteral("NotificationDialog"));
  setAttribute(Qt::WA_TranslucentBackground, false);
  setModal(false);
  setWindowModality(Qt::NonModal);
  setProperty("svmModalDialog", true);
  setFixedWidth(440);

  setupUI();

  connect(&app::ThemeManager::instance(), &app::ThemeManager::themeChanged,
          this, [this]() { updateThemeIcon(); });
}

void NotificationDialog::setupUI() {
  auto *rootLayout = new QVBoxLayout(this);
  rootLayout->setContentsMargins(0, 0, 0, 0);

  // Khung Card chính (chứa toàn bộ giao diện hộp thoại và tham gia vào hiệu ứng
  // rung)
  m_cardWidget = new QWidget(this);
  m_cardWidget->setObjectName(QStringLiteral("NotificationDialogCard"));
  m_cardWidget->setAttribute(Qt::WA_StyledBackground, true);

  auto *cardLayout = new QVBoxLayout(m_cardWidget);
  cardLayout->setContentsMargins(0, 0, 0, 14);
  cardLayout->setSpacing(0);

  // 1. Thanh tiêu đề (Header Bar)
  m_headerWidget = new QWidget(m_cardWidget);
  m_headerWidget->setObjectName(QStringLiteral("NotificationDialogHeader"));
  m_headerWidget->setAttribute(Qt::WA_StyledBackground, true);
  m_headerWidget->setFixedHeight(36);

  auto *headerLayout = new QHBoxLayout(m_headerWidget);
  headerLayout->setContentsMargins(14, 0, 0, 0);
  headerLayout->setSpacing(8);

  m_headerIconLabel = new QLabel(m_headerWidget);
  m_headerIconLabel->setPixmap(
      QIcon(QStringLiteral(":/icons/brand/app_logo.svg")).pixmap(16, 16));
  headerLayout->addWidget(m_headerIconLabel);

  m_titleLabel = new QLabel(m_headerWidget);
  m_titleLabel->setObjectName(QStringLiteral("NotificationDialogTitle"));
  headerLayout->addWidget(m_titleLabel, 1);

  m_closeBtn = new QPushButton(m_headerWidget);
  m_closeBtn->setObjectName(QStringLiteral("WinCloseBtn"));
  m_closeBtn->setFixedSize(40, 36);
  m_closeBtn->setIconSize(QSize(12, 12));
  m_closeBtn->setFlat(true);
  m_closeBtn->setFocusPolicy(Qt::NoFocus);
  m_closeBtn->setCursor(Qt::PointingHandCursor);
  m_closeBtn->setIcon(
      app::ThemeManager::instance().icon(QStringLiteral("win_close")));
  connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::reject);
  headerLayout->addWidget(m_closeBtn);

  cardLayout->addWidget(m_headerWidget);

  // Kéo thả độc lập qua Header Bar
  DialogDragHelper::makeDraggable(m_headerWidget, this);

  // 2. Vùng nội dung (Icon + Message)
  auto *bodyWidget = new QWidget(m_cardWidget);
  bodyWidget->setAttribute(Qt::WA_StyledBackground, true);
  auto *bodyLayout = new QHBoxLayout(bodyWidget);
  bodyLayout->setContentsMargins(16, 16, 16, 12);
  bodyLayout->setSpacing(14);

  // Container biểu tượng loại thông báo (38x38)
  m_iconContainer = new QWidget(bodyWidget);
  m_iconContainer->setObjectName(QStringLiteral("NotificationIconContainer"));
  m_iconContainer->setAttribute(Qt::WA_StyledBackground, true);
  m_iconContainer->setFixedSize(38, 38);

  auto *iconLayout = new QVBoxLayout(m_iconContainer);
  iconLayout->setContentsMargins(0, 0, 0, 0);
  iconLayout->setAlignment(Qt::AlignCenter);

  m_iconLabel = new QLabel(m_iconContainer);
  iconLayout->addWidget(m_iconLabel);
  bodyLayout->addWidget(m_iconContainer, 0, Qt::AlignTop);

  // Nhãn văn bản thông điệp
  m_messageLabel = new QLabel(bodyWidget);
  m_messageLabel->setObjectName(QStringLiteral("NotificationDialogMessage"));
  m_messageLabel->setWordWrap(true);
  m_messageLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
  bodyLayout->addWidget(m_messageLabel, 1);

  cardLayout->addWidget(bodyWidget, 1);

  // 3. Vùng nút bấm hành động (Action Buttons)
  m_btnContainer = new QWidget(m_cardWidget);
  m_btnContainer->setAttribute(Qt::WA_StyledBackground, true);
  auto *btnLayout = new QHBoxLayout(m_btnContainer);
  btnLayout->setContentsMargins(16, 0, 16, 0);
  btnLayout->setSpacing(8);

  btnLayout->addStretch(1);

  m_cancelBtn = new QPushButton(m_btnContainer);
  m_cancelBtn->setObjectName(QStringLiteral("NotificationCancelBtn"));
  m_cancelBtn->setFixedHeight(32);
  m_cancelBtn->setMinimumWidth(84);
  m_cancelBtn->setCursor(Qt::PointingHandCursor);
  m_cancelBtn->setVisible(false);
  connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
  btnLayout->addWidget(m_cancelBtn);

  m_confirmBtn = new QPushButton(m_btnContainer);
  m_confirmBtn->setObjectName(QStringLiteral("NotificationPrimaryBtn"));
  m_confirmBtn->setFixedHeight(32);
  m_confirmBtn->setMinimumWidth(84);
  m_confirmBtn->setCursor(Qt::PointingHandCursor);
  m_confirmBtn->setDefault(true);
  connect(m_confirmBtn, &QPushButton::clicked, this, &QDialog::accept);
  btnLayout->addWidget(m_confirmBtn);

  cardLayout->addWidget(m_btnContainer);

  rootLayout->addWidget(m_cardWidget);

  updateThemeIcon();
}

void NotificationDialog::setType(NotificationType type) {
  m_type = type;
  updateThemeIcon();
}

void NotificationDialog::setTitle(const QString &title) {
  if (m_titleLabel) {
    m_titleLabel->setText(title);
  }
}

void NotificationDialog::setMessage(const QString &message) {
  if (m_messageLabel) {
    m_messageLabel->setText(message);
  }
}

void NotificationDialog::setButtons(const QString &confirmText,
                                    const QString &cancelText,
                                    bool destructive) {
  if (m_confirmBtn) {
    m_confirmBtn->setText(confirmText);
    m_confirmBtn->setObjectName(destructive
                                    ? QStringLiteral("NotificationDangerBtn")
                                    : QStringLiteral("NotificationPrimaryBtn"));
    m_confirmBtn->style()->unpolish(m_confirmBtn);
    m_confirmBtn->style()->polish(m_confirmBtn);
  }

  if (m_cancelBtn) {
    if (!cancelText.isEmpty()) {
      m_cancelBtn->setText(cancelText);
      m_cancelBtn->setVisible(true);
    } else {
      m_cancelBtn->setVisible(false);
    }
  }
}

void NotificationDialog::updateThemeIcon() {
  if (m_closeBtn) {
    m_closeBtn->setIcon(
        app::ThemeManager::instance().icon(QStringLiteral("win_close")));
  }

  if (!m_iconLabel) {
    return;
  }

  auto &tm = app::ThemeManager::instance();
  QString iconName;
  switch (m_type) {
  case NotificationType::Success:
    iconName = QStringLiteral("shield_check");
    break;
  case NotificationType::Warning:
  case NotificationType::Question:
    iconName = QStringLiteral("watchtower_weak");
    break;
  case NotificationType::Information:
  default:
    iconName = QStringLiteral("info");
    break;
  }

  m_iconLabel->setPixmap(tm.icon(iconName).pixmap(24, 24));
}

void NotificationDialog::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Escape) {
    reject();
    return;
  }
  if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
    accept();
    return;
  }
  QDialog::keyPressEvent(event);
}

// =============================================================================
// Static Convenience Methods
// =============================================================================

void NotificationDialog::information(QWidget *parent, const QString &title,
                                     const QString &message) {
  NotificationDialog dlg(parent);
  dlg.setType(NotificationType::Information);
  dlg.setTitle(title);
  dlg.setMessage(message);
  dlg.setButtons(tr("OK"));
  DialogDragHelper::execDialog(dlg);
}

void NotificationDialog::success(QWidget *parent, const QString &title,
                                 const QString &message) {
  NotificationDialog dlg(parent);
  dlg.setType(NotificationType::Success);
  dlg.setTitle(title);
  dlg.setMessage(message);
  dlg.setButtons(tr("OK"));
  DialogDragHelper::execDialog(dlg);
}

void NotificationDialog::warning(QWidget *parent, const QString &title,
                                 const QString &message) {
  NotificationDialog dlg(parent);
  dlg.setType(NotificationType::Warning);
  dlg.setTitle(title);
  dlg.setMessage(message);
  dlg.setButtons(tr("OK"));
  DialogDragHelper::execDialog(dlg);
}

bool NotificationDialog::question(QWidget *parent, const QString &title,
                                  const QString &message,
                                  const QString &confirmText,
                                  const QString &cancelText, bool destructive) {
  NotificationDialog dlg(parent);
  dlg.setType(NotificationType::Question);
  dlg.setTitle(title);
  dlg.setMessage(message);
  dlg.setButtons(confirmText.isEmpty() ? tr("Confirm") : confirmText,
                 cancelText.isEmpty() ? tr("Cancel") : cancelText, destructive);
  return DialogDragHelper::execDialog(dlg) == QDialog::Accepted;
}

} // namespace svm::components
