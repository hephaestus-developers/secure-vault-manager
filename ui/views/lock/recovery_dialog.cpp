/**
 * @file ui/views/lock/recovery_dialog.cpp
 * @brief Hiện thực hộp thoại khôi phục khẩn cấp bằng Recovery Key.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "recovery_dialog.h"
#include "app/vault_manager.h"

#include <QEvent>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QVBoxLayout>

namespace svm::views {

RecoveryDialog::RecoveryDialog(QWidget *parent)
    : QDialog(parent, Qt::FramelessWindowHint | Qt::Dialog) {
  setObjectName(QStringLiteral("RecoveryDialog"));
  setAttribute(Qt::WA_TranslucentBackground, true);
  setModal(true);
  setFixedSize(500, 440);

  setupUI();
}

void RecoveryDialog::setupUI() {
  auto *rootLayout = new QVBoxLayout(this);
  rootLayout->setContentsMargins(16, 16, 16, 16);

  m_cardWidget = new QWidget(this);
  m_cardWidget->setObjectName(QStringLiteral("RecoveryDialogCard"));

  auto *shadow = new QGraphicsDropShadowEffect(this);
  shadow->setBlurRadius(28);
  shadow->setColor(QColor(0, 0, 0, 160));
  shadow->setOffset(0, 8);
  m_cardWidget->setGraphicsEffect(shadow);

  auto *layout = new QVBoxLayout(m_cardWidget);
  layout->setContentsMargins(24, 20, 24, 20);
  layout->setSpacing(12);

  m_titleLabel = new QLabel(tr("Emergency Vault Recovery"), m_cardWidget);
  QFont f = m_titleLabel->font();
  f.setPointSize(14);
  f.setBold(true);
  m_titleLabel->setFont(f);
  layout->addWidget(m_titleLabel);

  m_descLabel = new QLabel(tr("Enter your Emergency Recovery Key to restore "
                              "access and set a new Master Password."),
                           m_cardWidget);
  m_descLabel->setWordWrap(true);
  m_descLabel->setStyleSheet(
      QStringLiteral("color: #a6adc8; font-size: 11.5px;"));
  layout->addWidget(m_descLabel);

  // Recovery Key
  auto *keyLabel = new QLabel(tr("Emergency Recovery Key"), m_cardWidget);
  keyLabel->setStyleSheet(
      QStringLiteral("font-size: 11px; font-weight: 600; color: #bac2de;"));
  layout->addWidget(keyLabel);

  m_recKeyEdit = new QLineEdit(m_cardWidget);
  m_recKeyEdit->setPlaceholderText(QStringLiteral("SVM-XXXX-XXXX-XXXX-XXXX"));
  m_recKeyEdit->setStyleSheet(
      QStringLiteral("font-family: monospace; font-size: 13px;"));
  m_recKeyEdit->setFixedHeight(36);
  layout->addWidget(m_recKeyEdit);

  // New Password
  auto *newLabel = new QLabel(tr("New Master Password"), m_cardWidget);
  newLabel->setStyleSheet(
      QStringLiteral("font-size: 11px; font-weight: 600; color: #bac2de;"));
  layout->addWidget(newLabel);

  m_newPwdEdit = new QLineEdit(m_cardWidget);
  m_newPwdEdit->setEchoMode(QLineEdit::Password);
  m_newPwdEdit->setPlaceholderText(tr("Enter new master password..."));
  m_newPwdEdit->setFixedHeight(36);
  layout->addWidget(m_newPwdEdit);

  // Confirm New Password
  auto *confirmLabel =
      new QLabel(tr("Confirm New Master Password"), m_cardWidget);
  confirmLabel->setStyleSheet(
      QStringLiteral("font-size: 11px; font-weight: 600; color: #bac2de;"));
  layout->addWidget(confirmLabel);

  m_confirmPwdEdit = new QLineEdit(m_cardWidget);
  m_confirmPwdEdit->setEchoMode(QLineEdit::Password);
  m_confirmPwdEdit->setPlaceholderText(tr("Repeat new master password..."));
  m_confirmPwdEdit->setFixedHeight(36);
  layout->addWidget(m_confirmPwdEdit);

  m_errorLabel = new QLabel(m_cardWidget);
  m_errorLabel->setStyleSheet(
      QStringLiteral("color: #ef4444; font-size: 11px; font-weight: 500;"));
  layout->addWidget(m_errorLabel);

  layout->addStretch(1);

  // Bottom action bar
  auto *btnLayout = new QHBoxLayout();
  m_cancelBtn = new QPushButton(tr("Cancel"), m_cardWidget);
  m_cancelBtn->setFixedHeight(34);
  m_cancelBtn->setCursor(Qt::PointingHandCursor);
  connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
  btnLayout->addWidget(m_cancelBtn);

  btnLayout->addStretch(1);

  m_restoreBtn = new QPushButton(tr("Restore & Unlock Vault"), m_cardWidget);
  m_restoreBtn->setObjectName(QStringLiteral("OnboardingPrimaryBtn"));
  m_restoreBtn->setFixedHeight(34);
  m_restoreBtn->setCursor(Qt::PointingHandCursor);
  connect(m_restoreBtn, &QPushButton::clicked, this,
          &RecoveryDialog::onRestoreClicked);
  btnLayout->addWidget(m_restoreBtn);

  layout->addLayout(btnLayout);
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
  if (m_titleLabel) {
    m_titleLabel->setText(tr("Emergency Vault Recovery"));
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
    m_restoreBtn->setText(tr("Restore & Unlock Vault"));
  }
}

void RecoveryDialog::changeEvent(QEvent *event) {
  if (event->type() == QEvent::LanguageChange) {
    retranslateUi();
  }
  QDialog::changeEvent(event);
}

} // namespace svm::views
