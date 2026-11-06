/**
 * @file ui/views/lock/lock_screen_view.cpp
 * @brief Hiện thực màn hình khóa bảo mật tập trung LockScreenView.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "lock_screen_view.h"
#include "app/theme_manager.h"
#include "app/vault_manager.h"
#include "recovery_dialog.h"

#include <QApplication>
#include <QEvent>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeyEvent>
#include <QShowEvent>
#include <QVBoxLayout>

namespace svm::views {

LockScreenView::LockScreenView(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("LockScreenView"));
  setupUI();
}

void LockScreenView::setupUI() {
  auto *rootLayout = new QVBoxLayout(this);
  rootLayout->setAlignment(Qt::AlignCenter);
  rootLayout->setContentsMargins(20, 20, 20, 20);

  m_cardWidget = new QWidget(this);
  m_cardWidget->setObjectName(QStringLiteral("LockScreenCard"));
  m_cardWidget->setFixedSize(440, 390);

  auto *shadow = new QGraphicsDropShadowEffect(this);
  shadow->setBlurRadius(36);
  shadow->setColor(QColor(0, 0, 0, 180));
  shadow->setOffset(0, 12);
  m_cardWidget->setGraphicsEffect(shadow);

  auto *cardLayout = new QVBoxLayout(m_cardWidget);
  cardLayout->setContentsMargins(32, 28, 32, 28);
  cardLayout->setSpacing(14);
  cardLayout->setAlignment(Qt::AlignCenter);

  // Logo biểu trưng
  m_logoLabel = new QLabel(m_cardWidget);
  m_logoLabel->setPixmap(
      QIcon(QStringLiteral(":/icons/app_logo.svg")).pixmap(64, 64));
  m_logoLabel->setAlignment(Qt::AlignCenter);
  cardLayout->addWidget(m_logoLabel);

  // Tiêu đề
  m_titleLabel = new QLabel(tr("Unlock Secure Vault Manager"), m_cardWidget);
  QFont f = m_titleLabel->font();
  f.setPointSize(15);
  f.setBold(true);
  m_titleLabel->setFont(f);
  m_titleLabel->setAlignment(Qt::AlignCenter);
  cardLayout->addWidget(m_titleLabel);

  m_subtitleLabel = new QLabel(
      tr("Enter your master password to decrypt your vaults"), m_cardWidget);
  m_subtitleLabel->setAlignment(Qt::AlignCenter);
  m_subtitleLabel->setStyleSheet(
      QStringLiteral("color: #a6adc8; font-size: 11.5px;"));
  cardLayout->addWidget(m_subtitleLabel);

  cardLayout->addSpacing(4);

  // Container ô mật khẩu + nút ẩn hiện
  auto *inputBox = new QWidget(m_cardWidget);
  inputBox->setObjectName(QStringLiteral("LockPasswordContainer"));
  auto *inputLayout = new QHBoxLayout(inputBox);
  inputLayout->setContentsMargins(0, 0, 0, 0);
  inputLayout->setSpacing(4);

  m_passwordEdit = new QLineEdit(inputBox);
  m_passwordEdit->setObjectName(QStringLiteral("LockPasswordInput"));
  m_passwordEdit->setEchoMode(QLineEdit::Password);
  m_passwordEdit->setPlaceholderText(tr("Master password..."));
  m_passwordEdit->setFixedHeight(38);
  connect(m_passwordEdit, &QLineEdit::returnPressed, this,
          &LockScreenView::attemptUnlock);
  connect(m_passwordEdit, &QLineEdit::textChanged, this,
          [this]() { m_errorLabel->clear(); });
  inputLayout->addWidget(m_passwordEdit, 1);

  m_revealBtn = new QPushButton(inputBox);
  m_revealBtn->setObjectName(QStringLiteral("LockRevealBtn"));
  m_revealBtn->setFixedSize(38, 38);
  m_revealBtn->setCursor(Qt::PointingHandCursor);
  m_revealBtn->setIcon(
      app::ThemeManager::instance().icon(QStringLiteral("eye")));
  connect(m_revealBtn, &QPushButton::clicked, this, [this]() {
    m_isRevealed = !m_isRevealed;
    m_passwordEdit->setEchoMode(m_isRevealed ? QLineEdit::Normal
                                             : QLineEdit::Password);
    m_revealBtn->setIcon(app::ThemeManager::instance().icon(
        m_isRevealed ? QStringLiteral("eye_off") : QStringLiteral("eye")));
  });
  inputLayout->addWidget(m_revealBtn);

  cardLayout->addWidget(inputBox);

  // Caps lock warning
  m_capsLockLabel = new QLabel(m_cardWidget);
  m_capsLockLabel->setAlignment(Qt::AlignCenter);
  m_capsLockLabel->setStyleSheet(
      QStringLiteral("color: #f59e0b; font-size: 11px; font-weight: 500;"));
  m_capsLockLabel->hide();
  cardLayout->addWidget(m_capsLockLabel);

  // Error label
  m_errorLabel = new QLabel(m_cardWidget);
  m_errorLabel->setAlignment(Qt::AlignCenter);
  m_errorLabel->setStyleSheet(
      QStringLiteral("color: #ef4444; font-size: 11.5px; font-weight: 500;"));
  cardLayout->addWidget(m_errorLabel);

  // Hàng nút Unlock & Sinh trắc học
  auto *btnRow = new QHBoxLayout();
  btnRow->setSpacing(8);

  m_unlockBtn = new QPushButton(tr("Unlock Vault"), m_cardWidget);
  m_unlockBtn->setObjectName(QStringLiteral("OnboardingPrimaryBtn"));
  m_unlockBtn->setFixedHeight(38);
  m_unlockBtn->setCursor(Qt::PointingHandCursor);
  connect(m_unlockBtn, &QPushButton::clicked, this,
          &LockScreenView::attemptUnlock);
  btnRow->addWidget(m_unlockBtn, 1);

  m_biometricBtn = new QPushButton(m_cardWidget);
  m_biometricBtn->setObjectName(QStringLiteral("LockBiometricBtn"));
  m_biometricBtn->setFixedSize(38, 38);
  m_biometricBtn->setCursor(Qt::PointingHandCursor);
  m_biometricBtn->setToolTip(tr("Unlock with Biometrics (Touch / Face / PAM)"));
  m_biometricBtn->setIcon(
      app::ThemeManager::instance().icon(QStringLiteral("shield_check")));
  connect(m_biometricBtn, &QPushButton::clicked, this,
          &LockScreenView::attemptBiometricUnlock);
  btnRow->addWidget(m_biometricBtn);

  cardLayout->addLayout(btnRow);

  // Forgot password link
  m_forgotBtn =
      new QPushButton(tr("Forgot password? Use Recovery Key"), m_cardWidget);
  m_forgotBtn->setObjectName(QStringLiteral("LockForgotBtn"));
  m_forgotBtn->setFlat(true);
  m_forgotBtn->setCursor(Qt::PointingHandCursor);
  m_forgotBtn->setStyleSheet(
      QStringLiteral("color: #89b4fa; font-size: 11.5px; text-decoration: "
                     "underline; background: transparent; border: none;"));
  connect(m_forgotBtn, &QPushButton::clicked, this,
          &LockScreenView::openRecoveryDialog);
  cardLayout->addWidget(m_forgotBtn);

  rootLayout->addWidget(m_cardWidget);
}

void LockScreenView::attemptUnlock() {
  const QString pwd = m_passwordEdit->text();
  if (pwd.isEmpty()) {
    m_errorLabel->setText(tr("Please enter your master password."));
    triggerShakeAnimation();
    return;
  }

  const bool ok = app::VaultManager::instance().unlock(pwd);
  if (ok) {
    m_passwordEdit->clear();
    m_errorLabel->clear();
    emit unlocked();
  } else {
    m_errorLabel->setText(tr("Incorrect master password. Please try again."));
    triggerShakeAnimation();
    m_passwordEdit->selectAll();
    m_passwordEdit->setFocus();
  }
}

void LockScreenView::attemptBiometricUnlock() {
  const bool ok = app::VaultManager::instance().unlockWithBiometrics();
  if (ok) {
    m_passwordEdit->clear();
    m_errorLabel->clear();
    emit unlocked();
  }
}

void LockScreenView::openRecoveryDialog() {
  RecoveryDialog dlg(this);
  if (dlg.exec() == QDialog::Accepted) {
    m_passwordEdit->clear();
    m_errorLabel->clear();
    emit unlocked();
  }
}

void LockScreenView::triggerShakeAnimation() {
  if (!m_cardWidget) {
    return;
  }
  auto *anim = new QPropertyAnimation(m_cardWidget, "pos", this);
  anim->setDuration(350);
  const QPoint orig = m_cardWidget->pos();
  anim->setKeyValueAt(0.0, orig);
  anim->setKeyValueAt(0.15, orig + QPoint(-10, 0));
  anim->setKeyValueAt(0.35, orig + QPoint(10, 0));
  anim->setKeyValueAt(0.55, orig + QPoint(-7, 0));
  anim->setKeyValueAt(0.75, orig + QPoint(7, 0));
  anim->setKeyValueAt(0.9, orig + QPoint(-3, 0));
  anim->setKeyValueAt(1.0, orig);
  anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void LockScreenView::resetLockScreen() {
  if (m_passwordEdit) {
    m_passwordEdit->clear();
    m_isRevealed = false;
    m_passwordEdit->setEchoMode(QLineEdit::Password);
  }
  if (m_revealBtn) {
    m_revealBtn->setIcon(
        app::ThemeManager::instance().icon(QStringLiteral("eye")));
  }
  if (m_errorLabel) {
    m_errorLabel->clear();
  }
  focusPasswordInput();
}

void LockScreenView::focusPasswordInput() {
  if (m_passwordEdit) {
    m_passwordEdit->setFocus();
  }
}

void LockScreenView::checkCapsLockState() {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  const bool caps =
      QGuiApplication::queryKeyboardModifiers().testFlag(Qt::KeypadModifier);
  if (m_capsLockLabel) {
    m_capsLockLabel->setVisible(caps);
    if (caps) {
      m_capsLockLabel->setText(tr("⚠ Caps Lock is ON"));
    }
  }
#endif
}

void LockScreenView::showEvent(QShowEvent *event) {
  QWidget::showEvent(event);
  resetLockScreen();
}

void LockScreenView::keyPressEvent(QKeyEvent *event) {
  checkCapsLockState();
  QWidget::keyPressEvent(event);
}

void LockScreenView::retranslateUi() {
  if (m_titleLabel) {
    m_titleLabel->setText(tr("Unlock Secure Vault Manager"));
  }
  if (m_subtitleLabel) {
    m_subtitleLabel->setText(
        tr("Enter your master password to decrypt your vaults"));
  }
  if (m_passwordEdit) {
    m_passwordEdit->setPlaceholderText(tr("Master password..."));
  }
  if (m_unlockBtn) {
    m_unlockBtn->setText(tr("Unlock Vault"));
  }
  if (m_biometricBtn) {
    m_biometricBtn->setToolTip(
        tr("Unlock with Biometrics (Touch / Face / PAM)"));
  }
  if (m_forgotBtn) {
    m_forgotBtn->setText(tr("Forgot password? Use Recovery Key"));
  }
}

void LockScreenView::changeEvent(QEvent *event) {
  if (event->type() == QEvent::LanguageChange) {
    retranslateUi();
  }
  QWidget::changeEvent(event);
}

} // namespace svm::views
