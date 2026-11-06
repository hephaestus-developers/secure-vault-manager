/**
 * @file ui/views/onboarding/onboarding_wizard_view.cpp
 * @brief Hiện thực giao diện OnboardingWizardView dạng Centered Card.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "onboarding_wizard_view.h"
#include "app/clipboard_manager.h"
#include "app/theme_manager.h"
#include "app/vault_manager.h"

#include <QEvent>
#include <QFileDialog>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QIcon>
#include <QMessageBox>
#include <QStandardPaths>
#include <QVBoxLayout>

namespace svm::views {

OnboardingWizardView::OnboardingWizardView(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("OnboardingWizardView"));
  setupUI();
  resetWizard();
}

void OnboardingWizardView::setupUI() {
  auto *rootLayout = new QVBoxLayout(this);
  rootLayout->setAlignment(Qt::AlignCenter);
  rootLayout->setContentsMargins(20, 20, 20, 20);

  // Thẻ Card chính ở giữa màn hình
  m_cardWidget = new QWidget(this);
  m_cardWidget->setObjectName(QStringLiteral("OnboardingCard"));
  m_cardWidget->setFixedSize(560, 520);

  auto *shadow = new QGraphicsDropShadowEffect(this);
  shadow->setBlurRadius(32);
  shadow->setColor(QColor(0, 0, 0, 160));
  shadow->setOffset(0, 10);
  m_cardWidget->setGraphicsEffect(shadow);

  auto *cardLayout = new QVBoxLayout(m_cardWidget);
  cardLayout->setContentsMargins(28, 24, 28, 24);
  cardLayout->setSpacing(14);

  // Thanh chỉ số bước (Step Indicator)
  auto *topBar = new QHBoxLayout();
  topBar->setSpacing(6);

  for (int i = 0; i < 4; ++i) {
    m_stepDots[i] = new QLabel(m_cardWidget);
    m_stepDots[i]->setFixedHeight(4);
    m_stepDots[i]->setStyleSheet(
        QStringLiteral("background-color: #313244; border-radius: 2px;"));
    topBar->addWidget(m_stepDots[i], 1);
  }
  cardLayout->addLayout(topBar);

  m_stepTitleLabel = new QLabel(m_cardWidget);
  m_stepTitleLabel->setStyleSheet(
      QStringLiteral("font-size: 11px; font-weight: 600; color: #89b4fa; "
                     "text-transform: uppercase;"));
  cardLayout->addWidget(m_stepTitleLabel);

  // Stack chứa các bước
  m_stack = new QStackedWidget(m_cardWidget);
  m_stack->addWidget(createStepWelcome());
  m_stack->addWidget(createStepPassword());
  m_stack->addWidget(createStepRecoveryKey());
  m_stack->addWidget(createStepVaultSetup());
  cardLayout->addWidget(m_stack, 1);

  rootLayout->addWidget(m_cardWidget);
}

QWidget *OnboardingWizardView::createStepWelcome() {
  auto *page = new QWidget(m_stack);
  auto *layout = new QVBoxLayout(page);
  layout->setContentsMargins(0, 8, 0, 0);
  layout->setSpacing(16);
  layout->setAlignment(Qt::AlignCenter);

  auto *logo = new QLabel(page);
  logo->setPixmap(QIcon(QStringLiteral(":/icons/app_logo.svg")).pixmap(64, 64));
  logo->setAlignment(Qt::AlignCenter);
  layout->addWidget(logo);

  m_welcomeTitle = new QLabel(tr("Welcome to Secure Vault Manager"), page);
  QFont f = m_welcomeTitle->font();
  f.setPointSize(16);
  f.setBold(true);
  m_welcomeTitle->setFont(f);
  m_welcomeTitle->setAlignment(Qt::AlignCenter);
  layout->addWidget(m_welcomeTitle);

  m_welcomeDesc = new QLabel(
      tr("Native Zero-Knowledge Password & Secret Manager.\n"
         "Your sensitive data is encrypted locally using Argon2id & "
         "AES-256-GCM.\n\n"
         "Let's get your secure vault set up in a few simple steps."),
      page);
  m_welcomeDesc->setAlignment(Qt::AlignCenter);
  m_welcomeDesc->setWordWrap(true);
  m_welcomeDesc->setStyleSheet(
      QStringLiteral("color: #a6adc8; font-size: 12px; line-height: 1.5;"));
  layout->addWidget(m_welcomeDesc);

  layout->addStretch(1);

  m_createVaultBtn = new QPushButton(tr("Create a New Vault →"), page);
  m_createVaultBtn->setObjectName(QStringLiteral("OnboardingPrimaryBtn"));
  m_createVaultBtn->setFixedHeight(40);
  m_createVaultBtn->setCursor(Qt::PointingHandCursor);
  connect(m_createVaultBtn, &QPushButton::clicked, this,
          [this]() { goToStep(1); });
  layout->addWidget(m_createVaultBtn);

  return page;
}

QWidget *OnboardingWizardView::createStepPassword() {
  auto *page = new QWidget(m_stack);
  auto *layout = new QVBoxLayout(page);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(10);

  m_pwdTitle = new QLabel(tr("Set Your Master Password"), page);
  QFont f = m_pwdTitle->font();
  f.setPointSize(14);
  f.setBold(true);
  m_pwdTitle->setFont(f);
  layout->addWidget(m_pwdTitle);

  m_pwdDesc = new QLabel(
      tr("This master password decrypts your vault. Choose a strong, unique "
         "passphrase.\n"
         "Because SVM is zero-knowledge, we cannot reset it for you."),
      page);
  m_pwdDesc->setWordWrap(true);
  m_pwdDesc->setStyleSheet(
      QStringLiteral("color: #a6adc8; font-size: 11.5px;"));
  layout->addWidget(m_pwdDesc);

  auto *pwdLabel = new QLabel(tr("Master Password"), page);
  pwdLabel->setStyleSheet(
      QStringLiteral("font-size: 11px; font-weight: 600; color: #bac2de;"));
  layout->addWidget(pwdLabel);

  m_pwdEdit = new QLineEdit(page);
  m_pwdEdit->setEchoMode(QLineEdit::Password);
  m_pwdEdit->setPlaceholderText(tr("Enter master password..."));
  m_pwdEdit->setFixedHeight(36);
  layout->addWidget(m_pwdEdit);

  m_entropyMeter = new PasswordEntropyMeter(page);
  layout->addWidget(m_entropyMeter);

  connect(m_pwdEdit, &QLineEdit::textChanged, this,
          [this](const QString &text) {
            m_entropyMeter->setPassword(text);
            m_pwdMismatchLabel->clear();
          });

  auto *confirmLabel = new QLabel(tr("Confirm Master Password"), page);
  confirmLabel->setStyleSheet(
      QStringLiteral("font-size: 11px; font-weight: 600; color: #bac2de;"));
  layout->addWidget(confirmLabel);

  m_confirmPwdEdit = new QLineEdit(page);
  m_confirmPwdEdit->setEchoMode(QLineEdit::Password);
  m_confirmPwdEdit->setPlaceholderText(tr("Repeat master password..."));
  m_confirmPwdEdit->setFixedHeight(36);
  layout->addWidget(m_confirmPwdEdit);

  m_pwdMismatchLabel = new QLabel(page);
  m_pwdMismatchLabel->setStyleSheet(
      QStringLiteral("color: #ef4444; font-size: 11px; font-weight: 500;"));
  layout->addWidget(m_pwdMismatchLabel);

  layout->addStretch(1);

  // Bottom action bar
  auto *btnLayout = new QHBoxLayout();
  m_pwdBackBtn = new QPushButton(tr("← Back"), page);
  m_pwdBackBtn->setFixedHeight(36);
  m_pwdBackBtn->setCursor(Qt::PointingHandCursor);
  connect(m_pwdBackBtn, &QPushButton::clicked, this, [this]() { goToStep(0); });
  btnLayout->addWidget(m_pwdBackBtn);

  btnLayout->addStretch(1);

  m_pwdNextBtn = new QPushButton(tr("Continue →"), page);
  m_pwdNextBtn->setObjectName(QStringLiteral("OnboardingPrimaryBtn"));
  m_pwdNextBtn->setFixedHeight(36);
  m_pwdNextBtn->setCursor(Qt::PointingHandCursor);
  connect(m_pwdNextBtn, &QPushButton::clicked, this, [this]() {
    const QString p1 = m_pwdEdit->text();
    const QString p2 = m_confirmPwdEdit->text();
    if (p1.isEmpty()) {
      m_pwdMismatchLabel->setText(tr("Master password cannot be empty."));
      return;
    }
    if (p1.length() < 8) {
      m_pwdMismatchLabel->setText(
          tr("Master password must be at least 8 characters."));
      return;
    }
    if (p1 != p2) {
      m_pwdMismatchLabel->setText(tr("Passwords do not match."));
      return;
    }
    m_pwdMismatchLabel->clear();
    goToStep(2);
  });
  btnLayout->addWidget(m_pwdNextBtn);

  layout->addLayout(btnLayout);
  return page;
}

QWidget *OnboardingWizardView::createStepRecoveryKey() {
  auto *page = new QWidget(m_stack);
  auto *layout = new QVBoxLayout(page);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(12);

  m_recTitle = new QLabel(tr("Save Your Emergency Recovery Key"), page);
  QFont f = m_recTitle->font();
  f.setPointSize(14);
  f.setBold(true);
  m_recTitle->setFont(f);
  layout->addWidget(m_recTitle);

  m_recDesc = new QLabel(tr("If you ever forget your master password, this "
                            "recovery key is the ONLY "
                            "way to restore access to your encrypted vault. "
                            "Store it somewhere safe."),
                         page);
  m_recDesc->setWordWrap(true);
  m_recDesc->setStyleSheet(
      QStringLiteral("color: #a6adc8; font-size: 11.5px;"));
  layout->addWidget(m_recDesc);

  // Hộp hiển thị mã Recovery Key
  auto *box = new QWidget(page);
  box->setObjectName(QStringLiteral("RecoveryBox"));
  auto *boxLayout = new QVBoxLayout(box);
  boxLayout->setContentsMargins(16, 14, 16, 14);
  boxLayout->setSpacing(12);

  m_recKeyLabel = new QLabel(box);
  m_recKeyLabel->setObjectName(QStringLiteral("RecoveryKeyDisplay"));
  m_recKeyLabel->setAlignment(Qt::AlignCenter);
  m_recKeyLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
  boxLayout->addWidget(m_recKeyLabel);

  auto *boxBtns = new QHBoxLayout();
  boxBtns->setSpacing(10);

  m_copyRecBtn = new QPushButton(tr("Copy Key"), box);
  m_copyRecBtn->setObjectName(QStringLiteral("RecoveryActionBtn"));
  m_copyRecBtn->setIcon(
      app::ThemeManager::instance().icon(QStringLiteral("copy")));
  m_copyRecBtn->setFixedHeight(34);
  m_copyRecBtn->setCursor(Qt::PointingHandCursor);
  connect(m_copyRecBtn, &QPushButton::clicked, this, [this]() {
    app::ClipboardManager::instance().copySensitive(
        m_generatedRecoveryKey, tr("Emergency Recovery Key"));
    m_copyRecBtn->setText(tr("✓ Copied!"));
  });
  boxBtns->addWidget(m_copyRecBtn);

  m_saveRecBtn = new QPushButton(tr("Save to File..."), box);
  m_saveRecBtn->setObjectName(QStringLiteral("RecoveryActionBtn"));
  m_saveRecBtn->setFixedHeight(34);
  m_saveRecBtn->setCursor(Qt::PointingHandCursor);
  connect(m_saveRecBtn, &QPushButton::clicked, this, [this]() {
    const QString filePath = QFileDialog::getSaveFileName(
        this, tr("Save Emergency Recovery Key"),
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) +
            QStringLiteral("/SVM_Recovery_Key.txt"),
        tr("Text Files (*.txt);;All Files (*)"));
    if (!filePath.isEmpty()) {
      QFile file(filePath);
      if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        file.write(
            QStringLiteral("SECURE VAULT MANAGER - EMERGENCY RECOVERY KEY\n"
                           "Generated: %1\n\n"
                           "Recovery Key: %2\n\n"
                           "Keep this key confidential. Anyone with this key "
                           "can reset your master password.\n")
                .arg(QDateTime::currentDateTime().toString(Qt::ISODate),
                     m_generatedRecoveryKey)
                .toUtf8());
        file.close();
        m_saveRecBtn->setText(tr("✓ Saved!"));
      }
    }
  });
  boxBtns->addWidget(m_saveRecBtn);

  boxLayout->addLayout(boxBtns);
  layout->addWidget(box);

  // Checkbox xác nhận bắt buộc
  m_recConfirmBox = new QCheckBox(
      tr("I have saved this recovery key in a secure location."), page);
  m_recConfirmBox->setStyleSheet(
      QStringLiteral("color: #cdd6f4; font-size: 11.5px; font-weight: 500;"));
  connect(m_recConfirmBox, &QCheckBox::toggled, this,
          [this](bool checked) { m_recNextBtn->setEnabled(checked); });
  layout->addWidget(m_recConfirmBox);

  layout->addStretch(1);

  // Bottom action bar
  auto *btnLayout = new QHBoxLayout();
  m_recBackBtn = new QPushButton(tr("← Back"), page);
  m_recBackBtn->setFixedHeight(36);
  m_recBackBtn->setCursor(Qt::PointingHandCursor);
  connect(m_recBackBtn, &QPushButton::clicked, this, [this]() { goToStep(1); });
  btnLayout->addWidget(m_recBackBtn);

  btnLayout->addStretch(1);

  m_recNextBtn = new QPushButton(tr("Continue →"), page);
  m_recNextBtn->setObjectName(QStringLiteral("OnboardingPrimaryBtn"));
  m_recNextBtn->setFixedHeight(36);
  m_recNextBtn->setCursor(Qt::PointingHandCursor);
  m_recNextBtn->setEnabled(
      false); // Bắt buộc tick checkbox mới cho phép đi tiếp
  connect(m_recNextBtn, &QPushButton::clicked, this, [this]() { goToStep(3); });
  btnLayout->addWidget(m_recNextBtn);

  layout->addLayout(btnLayout);
  return page;
}

QWidget *OnboardingWizardView::createStepVaultSetup() {
  auto *page = new QWidget(m_stack);
  auto *layout = new QVBoxLayout(page);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(12);

  m_vaultTitle = new QLabel(tr("Name Your First Vault"), page);
  QFont f = m_vaultTitle->font();
  f.setPointSize(14);
  f.setBold(true);
  m_vaultTitle->setFont(f);
  layout->addWidget(m_vaultTitle);

  m_vaultDesc = new QLabel(
      tr("Vaults help organize passwords, keys, and cards. You can create more "
         "vaults (e.g. Work, Family) anytime from the sidebar."),
      page);
  m_vaultDesc->setWordWrap(true);
  m_vaultDesc->setStyleSheet(
      QStringLiteral("color: #a6adc8; font-size: 11.5px;"));
  layout->addWidget(m_vaultDesc);

  auto *vLabel = new QLabel(tr("Vault Name"), page);
  vLabel->setStyleSheet(
      QStringLiteral("font-size: 11px; font-weight: 600; color: #bac2de;"));
  layout->addWidget(vLabel);

  m_vaultNameEdit = new QLineEdit(page);
  m_vaultNameEdit->setText(tr("Personal Vault"));
  m_vaultNameEdit->setFixedHeight(36);
  layout->addWidget(m_vaultNameEdit);

  m_biometricCheck = new QCheckBox(
      tr("Enable Biometric Unlock (Touch ID / Windows Hello / PAM)"), page);
  m_biometricCheck->setChecked(true);
  m_biometricCheck->setStyleSheet(
      QStringLiteral("color: #cdd6f4; font-size: 11.5px; margin-top: 6px;"));
  layout->addWidget(m_biometricCheck);

  layout->addStretch(1);

  // Bottom action bar
  auto *btnLayout = new QHBoxLayout();
  m_vaultBackBtn = new QPushButton(tr("← Back"), page);
  m_vaultBackBtn->setFixedHeight(36);
  m_vaultBackBtn->setCursor(Qt::PointingHandCursor);
  connect(m_vaultBackBtn, &QPushButton::clicked, this,
          [this]() { goToStep(2); });
  btnLayout->addWidget(m_vaultBackBtn);

  btnLayout->addStretch(1);

  m_finishBtn = new QPushButton(tr("Complete Setup & Open Vault →"), page);
  m_finishBtn->setObjectName(QStringLiteral("OnboardingPrimaryBtn"));
  m_finishBtn->setFixedHeight(36);
  m_finishBtn->setCursor(Qt::PointingHandCursor);
  connect(m_finishBtn, &QPushButton::clicked, this, [this]() {
    const QString pwd = m_pwdEdit->text();
    const QString vName = m_vaultNameEdit->text();
    const bool success =
        app::VaultManager::instance().setupInitialMasterPassword(
            pwd, m_generatedRecoveryKey, vName);
    if (success) {
      emit onboardingFinished();
    }
  });
  btnLayout->addWidget(m_finishBtn);

  layout->addLayout(btnLayout);
  return page;
}

void OnboardingWizardView::goToStep(int stepIndex) {
  m_currentStep = qBound(0, stepIndex, 3);
  m_stack->setCurrentIndex(m_currentStep);
  updateStepIndicator();
}

void OnboardingWizardView::updateStepIndicator() {
  const QString titles[] = {
      tr("Step 1 of 4: Welcome"),
      tr("Step 2 of 4: Master Password"),
      tr("Step 3 of 4: Recovery Key"),
      tr("Step 4 of 4: Vault Setup"),
  };
  m_stepTitleLabel->setText(titles[m_currentStep]);

  for (int i = 0; i < 4; ++i) {
    if (i <= m_currentStep) {
      m_stepDots[i]->setStyleSheet(
          QStringLiteral("background-color: #89b4fa; border-radius: 2px;"));
    } else {
      m_stepDots[i]->setStyleSheet(
          QStringLiteral("background-color: #313244; border-radius: 2px;"));
    }
  }
}

void OnboardingWizardView::resetWizard() {
  m_generatedRecoveryKey = app::VaultManager::generateRecoveryKey();
  if (m_recKeyLabel) {
    m_recKeyLabel->setText(m_generatedRecoveryKey);
  }
  if (m_pwdEdit) {
    m_pwdEdit->clear();
  }
  if (m_confirmPwdEdit) {
    m_confirmPwdEdit->clear();
  }
  if (m_entropyMeter) {
    m_entropyMeter->setPassword(QString());
  }
  if (m_pwdMismatchLabel) {
    m_pwdMismatchLabel->clear();
  }
  if (m_recConfirmBox) {
    m_recConfirmBox->setChecked(false);
  }
  if (m_copyRecBtn) {
    m_copyRecBtn->setText(tr("Copy Key"));
  }
  if (m_saveRecBtn) {
    m_saveRecBtn->setText(tr("Save to File..."));
  }
  goToStep(0);
}

void OnboardingWizardView::retranslateUi() {
  updateStepIndicator();
  if (m_welcomeTitle) {
    m_welcomeTitle->setText(tr("Welcome to Secure Vault Manager"));
  }
  if (m_welcomeDesc) {
    m_welcomeDesc->setText(
        tr("Native Zero-Knowledge Password & Secret Manager.\n"
           "Your sensitive data is encrypted locally using Argon2id & "
           "AES-256-GCM.\n\n"
           "Let's get your secure vault set up in a few simple steps."));
  }
  if (m_createVaultBtn) {
    m_createVaultBtn->setText(tr("Create a New Vault →"));
  }
  if (m_pwdTitle) {
    m_pwdTitle->setText(tr("Set Your Master Password"));
  }
  if (m_pwdDesc) {
    m_pwdDesc->setText(
        tr("This master password decrypts your vault. Choose a strong, unique "
           "passphrase.\n"
           "Because SVM is zero-knowledge, we cannot reset it for you."));
  }
  if (m_pwdEdit) {
    m_pwdEdit->setPlaceholderText(tr("Enter master password..."));
  }
  if (m_confirmPwdEdit) {
    m_confirmPwdEdit->setPlaceholderText(tr("Repeat master password..."));
  }
  if (m_pwdBackBtn) {
    m_pwdBackBtn->setText(tr("← Back"));
  }
  if (m_pwdNextBtn) {
    m_pwdNextBtn->setText(tr("Continue →"));
  }
  if (m_recTitle) {
    m_recTitle->setText(tr("Save Your Emergency Recovery Key"));
  }
  if (m_recDesc) {
    m_recDesc->setText(tr("If you ever forget your master password, this "
                          "recovery key is the ONLY "
                          "way to restore access to your encrypted vault. "
                          "Store it somewhere safe."));
  }
  if (m_copyRecBtn) {
    m_copyRecBtn->setText(tr("Copy Key"));
  }
  if (m_saveRecBtn) {
    m_saveRecBtn->setText(tr("Save to File..."));
  }
  if (m_recConfirmBox) {
    m_recConfirmBox->setText(
        tr("I have saved this recovery key in a secure location."));
  }
  if (m_recBackBtn) {
    m_recBackBtn->setText(tr("← Back"));
  }
  if (m_recNextBtn) {
    m_recNextBtn->setText(tr("Continue →"));
  }
  if (m_vaultTitle) {
    m_vaultTitle->setText(tr("Name Your First Vault"));
  }
  if (m_vaultDesc) {
    m_vaultDesc->setText(tr(
        "Vaults help organize passwords, keys, and cards. You can create more "
        "vaults (e.g. Work, Family) anytime from the sidebar."));
  }
  if (m_vaultNameEdit && m_vaultNameEdit->text().isEmpty()) {
    m_vaultNameEdit->setText(tr("Personal Vault"));
  }
  if (m_biometricCheck) {
    m_biometricCheck->setText(
        tr("Enable Biometric Unlock (Touch ID / Windows Hello / PAM)"));
  }
  if (m_vaultBackBtn) {
    m_vaultBackBtn->setText(tr("← Back"));
  }
  if (m_finishBtn) {
    m_finishBtn->setText(tr("Complete Setup & Open Vault →"));
  }
}

void OnboardingWizardView::changeEvent(QEvent *event) {
  if (event->type() == QEvent::LanguageChange) {
    retranslateUi();
  }
  QWidget::changeEvent(event);
}

} // namespace svm::views
