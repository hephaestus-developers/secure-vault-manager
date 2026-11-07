/**
 * @file ui/views/lock/recovery_dialog.cpp
 * @brief Hiện thực hộp thoại khôi phục khẩn cấp bằng Recovery Key.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "recovery_dialog.h"
#include "app/theme_manager.h"
#include "app/vault_manager.h"
#include "components/dialog_drag_helper.h"
#include "components/window_resize_handler.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeyEvent>
#include <QVBoxLayout>

namespace svm::views {

RecoveryDialog::RecoveryDialog(QWidget *parent)
    : QDialog(parent, Qt::FramelessWindowHint | Qt::Dialog) {
  setObjectName(QStringLiteral("RecoveryDialog"));
  setAttribute(Qt::WA_TranslucentBackground, false);
  setModal(false);
  setWindowModality(Qt::NonModal);
  setMinimumSize(440, 420);
  resize(500, 480);

  setupUI();

  new components::WindowResizeHandler(this);
}

void RecoveryDialog::setupUI() {
  auto *rootLayout = new QVBoxLayout(this);
  rootLayout->setContentsMargins(0, 0, 0, 0);

  m_cardWidget = new QWidget(this);
  m_cardWidget->setObjectName(QStringLiteral("RecoveryDialogCard"));
  m_cardWidget->setAttribute(Qt::WA_StyledBackground, true);

  auto *cardLayout = new QVBoxLayout(m_cardWidget);
  cardLayout->setContentsMargins(0, 0, 0, 16);
  cardLayout->setSpacing(0);

  // 1. Thanh tiêu đề kéo thả tích hợp
  m_headerWidget = new QWidget(m_cardWidget);
  m_headerWidget->setObjectName(QStringLiteral("RecoveryDialogHeader"));
  m_headerWidget->setAttribute(Qt::WA_StyledBackground, true);
  m_headerWidget->setFixedHeight(44);

  auto *headerLayout = new QHBoxLayout(m_headerWidget);
  headerLayout->setContentsMargins(16, 0, 8, 0);
  headerLayout->setSpacing(10);

  m_headerIcon = new QLabel(m_headerWidget);
  m_headerIcon->setFixedSize(18, 18);
  m_headerIcon->setPixmap(app::ThemeManager::instance()
                              .icon(QStringLiteral("shield_check"))
                              .pixmap(18, 18));
  headerLayout->addWidget(m_headerIcon);

  m_headerTitle = new QLabel(tr("Emergency Vault Recovery"), m_headerWidget);
  m_headerTitle->setObjectName(QStringLiteral("RecoveryDialogTitle"));
  QFont hf = m_headerTitle->font();
  hf.setPointSize(12);
  hf.setBold(true);
  m_headerTitle->setFont(hf);
  headerLayout->addWidget(m_headerTitle, 1);

  m_closeBtn = new QPushButton(m_headerWidget);
  m_closeBtn->setObjectName(QStringLiteral("DialogCloseBtn"));
  m_closeBtn->setFixedSize(32, 32);
  m_closeBtn->setIconSize(QSize(12, 12));
  m_closeBtn->setCursor(Qt::PointingHandCursor);
  m_closeBtn->setIcon(
      app::ThemeManager::instance().icon(QStringLiteral("win_close")));
  connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::reject);
  headerLayout->addWidget(m_closeBtn);

  cardLayout->addWidget(m_headerWidget);

  // 2. Thân nội dung
  auto *bodyWidget = new QWidget(m_cardWidget);
  auto *layout = new QVBoxLayout(bodyWidget);
  layout->setContentsMargins(20, 14, 20, 0);
  layout->setSpacing(10);

  m_descLabel = new QLabel(tr("Enter your Emergency Recovery Key to restore "
                              "access and set a new Master Password."),
                           bodyWidget);
  m_descLabel->setWordWrap(true);
  m_descLabel->setObjectName(QStringLiteral("DialogDescLabel"));
  layout->addWidget(m_descLabel);

  // Recovery Key
  auto *keyLabel = new QLabel(tr("Emergency Recovery Key"), bodyWidget);
  keyLabel->setObjectName(QStringLiteral("DialogFieldLabel"));
  layout->addWidget(keyLabel);

  m_recKeyEdit = new QLineEdit(bodyWidget);
  m_recKeyEdit->setPlaceholderText(QStringLiteral("SVM-XXXX-XXXX-XXXX-XXXX"));
  m_recKeyEdit->setStyleSheet(
      QStringLiteral("font-family: monospace; font-size: 13px;"));
  m_recKeyEdit->setFixedHeight(36);
  layout->addWidget(m_recKeyEdit);

  // New Password
  auto *newLabel = new QLabel(tr("New Master Password"), bodyWidget);
  newLabel->setObjectName(QStringLiteral("DialogFieldLabel"));
  layout->addWidget(newLabel);

  m_newPwdEdit = new QLineEdit(bodyWidget);
  m_newPwdEdit->setEchoMode(QLineEdit::Password);
  m_newPwdEdit->setPlaceholderText(tr("Enter new master password..."));
  m_newPwdEdit->setFixedHeight(34);
  layout->addWidget(m_newPwdEdit);

  // Confirm New Password
  auto *confirmLabel =
      new QLabel(tr("Confirm New Master Password"), bodyWidget);
  confirmLabel->setObjectName(QStringLiteral("DialogFieldLabel"));
  layout->addWidget(confirmLabel);

  m_confirmPwdEdit = new QLineEdit(bodyWidget);
  m_confirmPwdEdit->setEchoMode(QLineEdit::Password);
  m_confirmPwdEdit->setPlaceholderText(tr("Repeat new master password..."));
  m_confirmPwdEdit->setFixedHeight(34);
  layout->addWidget(m_confirmPwdEdit);

  m_errorLabel = new QLabel(bodyWidget);
  m_errorLabel->setObjectName(QStringLiteral("DialogErrorLabel"));
  layout->addWidget(m_errorLabel);

  layout->addStretch(1);

  // Bottom action bar
  auto *btnLayout = new QHBoxLayout();
  m_cancelBtn = new QPushButton(tr("Cancel"), bodyWidget);
  m_cancelBtn->setFixedHeight(34);
  m_cancelBtn->setCursor(Qt::PointingHandCursor);
  connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
  btnLayout->addWidget(m_cancelBtn);

  btnLayout->addStretch(1);

  m_restoreBtn = new QPushButton(tr("Restore && Unlock Vault"), bodyWidget);
  m_restoreBtn->setObjectName(QStringLiteral("OnboardingPrimaryBtn"));
  m_restoreBtn->setFixedHeight(34);
  m_restoreBtn->setCursor(Qt::PointingHandCursor);
  connect(m_restoreBtn, &QPushButton::clicked, this,
          &RecoveryDialog::onRestoreClicked);
  btnLayout->addWidget(m_restoreBtn);

  layout->addLayout(btnLayout);
  cardLayout->addWidget(bodyWidget, 1);

  components::DialogDragHelper::makeDraggable(m_cardWidget, this);
  rootLayout->addWidget(m_cardWidget);
}

void RecoveryDialog::onRestoreClicked() {
  const QString key = m_recKeyEdit->text().trimmed();
  const QString p1 = m_newPwdEdit->text();
  const QString p2 = m_confirmPwdEdit->text();

  if (key.isEmpty()) {
    m_errorLabel->setText(tr("Please enter your Emergency Recovery Key."));
    return;
  }
  if (p1.isEmpty()) {
    m_errorLabel->setText(tr("New master password cannot be empty."));
    return;
  }
  if (p1.length() < 8) {
    m_errorLabel->setText(tr("Master password must be at least 8 characters."));
    return;
  }
  if (p1 != p2) {
    m_errorLabel->setText(tr("Passwords do not match."));
    return;
  }

  const bool ok = app::VaultManager::instance().unlockWithRecoveryKey(key, p1);
  if (!ok) {
    m_errorLabel->setText(
        tr("Invalid Recovery Key. Please check and try again."));
    return;
  }
  m_errorLabel->clear();
  accept();
}

void RecoveryDialog::retranslateUi() {
  if (m_headerTitle) {
    m_headerTitle->setText(tr("Emergency Vault Recovery"));
  }
  if (m_descLabel) {
    m_descLabel->setText(tr("Enter your Emergency Recovery Key to restore "
                            "access and set a new Master Password."));
  }
  if (m_newPwdEdit) {
    m_newPwdEdit->setPlaceholderText(tr("Enter new master password..."));
  }
  if (m_confirmPwdEdit) {
    m_confirmPwdEdit->setPlaceholderText(tr("Repeat new master password..."));
  }
  if (m_cancelBtn) {
    m_cancelBtn->setText(tr("Cancel"));
  }
  if (m_restoreBtn) {
    m_restoreBtn->setText(tr("Restore && Unlock Vault"));
  }
}

void RecoveryDialog::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Escape) {
    reject();
    return;
  }
  QDialog::keyPressEvent(event);
}

void RecoveryDialog::changeEvent(QEvent *event) {
  if (event->type() == QEvent::LanguageChange) {
    retranslateUi();
  }
  QDialog::changeEvent(event);
}

} // namespace svm::views
