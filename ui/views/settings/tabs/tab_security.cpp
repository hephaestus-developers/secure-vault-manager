/**
 * @file ui/views/settings/tabs/tab_security.cpp
 * @brief Hiện thực giao diện Cài đặt Bảo mật & Quyền riêng tư.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "tab_security.h"
#include "app/clipboard_manager.h"
#include "app/settings_manager.h"
#include "app/vault_manager.h"
#include "views/onboarding/password_entropy_meter.h"

#include <QDialog>
#include <QGraphicsDropShadowEffect>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QVBoxLayout>

namespace svm::views {

TabSecurity::TabSecurity(QWidget *parent) : QWidget(parent) { setupUI(); }

void TabSecurity::setupUI() {
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(16);

  auto &settings = app::SettingsManager::instance();

  // 1. Auto-Lock Card
  m_autoLockCard = new SettingSectionCard(tr("Auto-Lock"), this);

  m_timeoutCombo = new QComboBox(m_autoLockCard);
  m_timeoutCombo->addItem(tr("1 minute"), 1);
  m_timeoutCombo->addItem(tr("5 minutes"), 5);
  m_timeoutCombo->addItem(tr("15 minutes (Default)"), 15);
  m_timeoutCombo->addItem(tr("30 minutes"), 30);
  m_timeoutCombo->addItem(tr("1 hour"), 60);
  m_timeoutCombo->addItem(tr("Never"), 0);

  const int curMinutes = settings.autoLockMinutes();
  int foundIdx = m_timeoutCombo->findData(curMinutes);
  if (foundIdx < 0) {
    foundIdx = 2; // Default 15m
  }
  m_timeoutCombo->setCurrentIndex(foundIdx);
  m_timeoutCombo->setFixedWidth(160);
  connect(m_timeoutCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this](int index) {
            const int minutes = m_timeoutCombo->itemData(index).toInt();
            app::SettingsManager::instance().setAutoLockMinutes(minutes);
          });

  m_timeoutRow = new SettingRowWidget(
      tr("Inactivity Lock"),
      tr("Automatically lock your vault after a period of user inactivity."),
      m_timeoutCombo, m_autoLockCard);
  m_autoLockCard->addRow(m_timeoutRow);

  m_sleepCheck = new QCheckBox(m_autoLockCard);
  m_sleepCheck->setChecked(settings.lockOnSleep());
  connect(m_sleepCheck, &QCheckBox::toggled, this, [](bool checked) {
    app::SettingsManager::instance().setLockOnSleep(checked);
  });
  m_sleepRow =
      new SettingRowWidget(tr("Lock on System Sleep"),
                           tr("Instantly lock your vault when the operating "
                              "system sleeps or the screen locks."),
                           m_sleepCheck, m_autoLockCard);
  m_autoLockCard->addRow(m_sleepRow);

  layout->addWidget(m_autoLockCard);

  // 2. Clipboard Security Card
  m_clipboardCard = new SettingSectionCard(tr("Clipboard Security"), this);

  m_clipTimeoutCombo = new QComboBox(m_clipboardCard);
  m_clipTimeoutCombo->addItem(tr("10 seconds"), 10);
  m_clipTimeoutCombo->addItem(tr("30 seconds (Default)"), 30);
  m_clipTimeoutCombo->addItem(tr("60 seconds"), 60);
  m_clipTimeoutCombo->addItem(tr("90 seconds"), 90);
  m_clipTimeoutCombo->addItem(tr("Never"), 0);

  const int curSecs = settings.clipboardClearSeconds();
  int foundSecIdx = m_clipTimeoutCombo->findData(curSecs);
  if (foundSecIdx < 0) {
    foundSecIdx = 1; // Default 30s
  }
  m_clipTimeoutCombo->setCurrentIndex(foundSecIdx);
  m_clipTimeoutCombo->setFixedWidth(160);
  connect(m_clipTimeoutCombo,
          QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          [this](int index) {
            const int secs = m_clipTimeoutCombo->itemData(index).toInt();
            app::SettingsManager::instance().setClipboardClearSeconds(secs);
          });

  m_clipTimeoutRow =
      new SettingRowWidget(tr("Clear Clipboard"),
                           tr("Automatically wipe copied passwords and secrets "
                              "from your system clipboard."),
                           m_clipTimeoutCombo, m_clipboardCard);
  m_clipboardCard->addRow(m_clipTimeoutRow);

  m_clipLockCheck = new QCheckBox(m_clipboardCard);
  m_clipLockCheck->setChecked(settings.clearClipboardOnLock());
  connect(m_clipLockCheck, &QCheckBox::toggled, this, [](bool checked) {
    app::SettingsManager::instance().setClearClipboardOnLock(checked);
  });
  m_clipLockRow =
      new SettingRowWidget(tr("Clear on Vault Lock"),
                           tr("Immediately clear sensitive data from clipboard "
                              "when vault is locked."),
                           m_clipLockCheck, m_clipboardCard);
  m_clipboardCard->addRow(m_clipLockRow);

  layout->addWidget(m_clipboardCard);

  // 3. Biometrics Card
  m_biometricCard = new SettingSectionCard(tr("Biometrics"), this);

  m_biometricCheck = new QCheckBox(m_biometricCard);
  m_biometricCheck->setChecked(settings.biometricEnabled());
  connect(m_biometricCheck, &QCheckBox::toggled, this, [](bool checked) {
    app::SettingsManager::instance().setBiometricEnabled(checked);
  });
  m_biometricRow =
      new SettingRowWidget(tr("Biometric Unlock"),
                           tr("Use Touch ID, Windows Hello, or Linux PAM to "
                              "quickly unlock your vault."),
                           m_biometricCheck, m_biometricCard);
  m_biometricCard->addRow(m_biometricRow);

  layout->addWidget(m_biometricCard);

  // 4. Credentials & Emergency Card
  m_credentialsCard =
      new SettingSectionCard(tr("Master Password & Emergency Key"), this);

  m_changePwdBtn =
      new QPushButton(tr("Change Master Password..."), m_credentialsCard);
  m_changePwdBtn->setFixedHeight(32);
  m_changePwdBtn->setCursor(Qt::PointingHandCursor);
  connect(m_changePwdBtn, &QPushButton::clicked, this,
          &TabSecurity::openChangePasswordDialog);
  m_pwdRow = new SettingRowWidget(tr("Master Password"),
                                  tr("Update the primary passphrase used to "
                                     "encrypt and decrypt your vault data."),
                                  m_changePwdBtn, m_credentialsCard);
  m_credentialsCard->addRow(m_pwdRow);

  m_viewRecKeyBtn =
      new QPushButton(tr("View Emergency Key..."), m_credentialsCard);
  m_viewRecKeyBtn->setFixedHeight(32);
  m_viewRecKeyBtn->setCursor(Qt::PointingHandCursor);
  connect(m_viewRecKeyBtn, &QPushButton::clicked, this,
          &TabSecurity::openViewRecoveryKeyDialog);
  m_recKeyRow =
      new SettingRowWidget(tr("Emergency Recovery Key"),
                           tr("Inspect your 24-character recovery key. "
                              "Required if you ever forget your password."),
                           m_viewRecKeyBtn, m_credentialsCard);
  m_credentialsCard->addRow(m_recKeyRow);

  layout->addWidget(m_credentialsCard);
  layout->addStretch(1);
}

void TabSecurity::openChangePasswordDialog() {
  QDialog dlg(this);
  dlg.setWindowTitle(tr("Change Master Password"));
  dlg.setModal(true);
  dlg.setFixedSize(440, 380);

  auto *layout = new QVBoxLayout(&dlg);
  layout->setContentsMargins(24, 20, 24, 20);
  layout->setSpacing(10);

  auto *title = new QLabel(tr("Change Master Password"), &dlg);
  QFont f = title->font();
  f.setPointSize(13);
  f.setBold(true);
  title->setFont(f);
  layout->addWidget(title);

  auto *curLabel = new QLabel(tr("Current Master Password:"), &dlg);
  layout->addWidget(curLabel);
  auto *curEdit = new QLineEdit(&dlg);
  curEdit->setEchoMode(QLineEdit::Password);
  curEdit->setFixedHeight(34);
  layout->addWidget(curEdit);

  auto *newLabel = new QLabel(tr("New Master Password:"), &dlg);
  layout->addWidget(newLabel);
  auto *newEdit = new QLineEdit(&dlg);
  newEdit->setEchoMode(QLineEdit::Password);
  newEdit->setFixedHeight(34);
  layout->addWidget(newEdit);

  auto *meter = new PasswordEntropyMeter(&dlg);
  layout->addWidget(meter);
  connect(newEdit, &QLineEdit::textChanged, meter,
          &PasswordEntropyMeter::setPassword);

  auto *confirmLabel = new QLabel(tr("Confirm New Master Password:"), &dlg);
  layout->addWidget(confirmLabel);
  auto *confirmEdit = new QLineEdit(&dlg);
  confirmEdit->setEchoMode(QLineEdit::Password);
  confirmEdit->setFixedHeight(34);
  layout->addWidget(confirmEdit);

  auto *errorLabel = new QLabel(&dlg);
  errorLabel->setStyleSheet(QStringLiteral("color: #ef4444; font-size: 11px;"));
  layout->addWidget(errorLabel);

  auto *btnLayout = new QHBoxLayout();
  btnLayout->addStretch(1);
  auto *cancelBtn = new QPushButton(tr("Cancel"), &dlg);
  cancelBtn->setFixedHeight(32);
  connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
  btnLayout->addWidget(cancelBtn);

  auto *okBtn = new QPushButton(tr("Update Password"), &dlg);
  okBtn->setObjectName(QStringLiteral("OnboardingPrimaryBtn"));
  okBtn->setFixedHeight(32);
  connect(okBtn, &QPushButton::clicked, &dlg, [&]() {
    const QString cur = curEdit->text();
    const QString p1 = newEdit->text();
    const QString p2 = confirmEdit->text();

    if (!app::VaultManager::instance().validateMasterPassword(cur)) {
      errorLabel->setText(tr("Current master password is incorrect."));
      return;
    }
    if (p1.length() < 8) {
      errorLabel->setText(tr("New password must be at least 8 characters."));
      return;
    }
    if (p1 != p2) {
      errorLabel->setText(tr("New passwords do not match."));
      return;
    }

    const auto v = app::VaultManager::instance().currentVault();
    const QString recKey = app::VaultManager::instance().recoveryKey();
    app::VaultManager::instance().setupInitialMasterPassword(p1, recKey,
                                                             v.name);
    QMessageBox::information(&dlg, tr("Success"),
                             tr("Master Password updated successfully!"));
    dlg.accept();
  });
  btnLayout->addWidget(okBtn);
  layout->addLayout(btnLayout);

  dlg.exec();
}

void TabSecurity::openViewRecoveryKeyDialog() {
  QDialog dlg(this);
  dlg.setWindowTitle(tr("View Emergency Recovery Key"));
  dlg.setModal(true);
  dlg.setFixedSize(460, 260);

  auto *layout = new QVBoxLayout(&dlg);
  layout->setContentsMargins(24, 20, 24, 20);
  layout->setSpacing(12);

  auto *title = new QLabel(tr("Verify Identity to View Recovery Key"), &dlg);
  QFont f = title->font();
  f.setPointSize(13);
  f.setBold(true);
  title->setFont(f);
  layout->addWidget(title);

  auto *pwdLabel = new QLabel(tr("Enter your Master Password:"), &dlg);
  layout->addWidget(pwdLabel);

  auto *pwdEdit = new QLineEdit(&dlg);
  pwdEdit->setEchoMode(QLineEdit::Password);
  pwdEdit->setFixedHeight(34);
  layout->addWidget(pwdEdit);

  auto *errorLabel = new QLabel(&dlg);
  errorLabel->setStyleSheet(QStringLiteral("color: #ef4444; font-size: 11px;"));
  layout->addWidget(errorLabel);

  auto *btnLayout = new QHBoxLayout();
  btnLayout->addStretch(1);
  auto *cancelBtn = new QPushButton(tr("Cancel"), &dlg);
  cancelBtn->setFixedHeight(32);
  connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
  btnLayout->addWidget(cancelBtn);

  auto *verifyBtn = new QPushButton(tr("Reveal Key"), &dlg);
  verifyBtn->setObjectName(QStringLiteral("OnboardingPrimaryBtn"));
  verifyBtn->setFixedHeight(32);
  connect(verifyBtn, &QPushButton::clicked, &dlg, [&]() {
    const QString pwd = pwdEdit->text();
    if (!app::VaultManager::instance().validateMasterPassword(pwd)) {
      errorLabel->setText(tr("Incorrect master password."));
      return;
    }

    // Hiển thị mã
    dlg.accept();

    QDialog showDlg(this);
    showDlg.setWindowTitle(tr("Emergency Recovery Key"));
    showDlg.setModal(true);
    showDlg.setFixedSize(480, 220);

    auto *showLayout = new QVBoxLayout(&showDlg);
    showLayout->setContentsMargins(24, 20, 24, 20);
    showLayout->setSpacing(14);

    auto *h = new QLabel(tr("Your Emergency Recovery Key"), &showDlg);
    QFont hf = h->font();
    hf.setPointSize(13);
    hf.setBold(true);
    h->setFont(hf);
    showLayout->addWidget(h);

    auto *box = new QWidget(&showDlg);
    box->setObjectName(QStringLiteral("RecoveryBox"));
    auto *boxLayout = new QVBoxLayout(box);
    boxLayout->setContentsMargins(14, 12, 14, 12);

    const QString key = app::VaultManager::instance().recoveryKey();
    auto *keyLabel = new QLabel(key, box);
    keyLabel->setObjectName(QStringLiteral("RecoveryKeyDisplay"));
    keyLabel->setAlignment(Qt::AlignCenter);
    keyLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    boxLayout->addWidget(keyLabel);
    showLayout->addWidget(box);

    auto *copyBtn = new QPushButton(tr("Copy to Clipboard"), &showDlg);
    copyBtn->setObjectName(QStringLiteral("RecoveryActionBtn"));
    copyBtn->setFixedHeight(34);
    copyBtn->setCursor(Qt::PointingHandCursor);
    connect(copyBtn, &QPushButton::clicked, &showDlg, [&]() {
      app::ClipboardManager::instance().copySensitive(key, tr("Recovery Key"));
      copyBtn->setText(tr("✓ Copied!"));
    });
    showLayout->addWidget(copyBtn);

    showDlg.exec();
  });
  btnLayout->addWidget(verifyBtn);
  layout->addLayout(btnLayout);

  dlg.exec();
}

void TabSecurity::retranslateUi() {
  if (m_autoLockCard) {
    m_autoLockCard->setTitle(tr("Auto-Lock"));
  }
  if (m_timeoutRow) {
    m_timeoutRow->updateText(
        tr("Inactivity Lock"),
        tr("Automatically lock your vault after a period of user inactivity."));
  }
  if (m_sleepRow) {
    m_sleepRow->updateText(tr("Lock on System Sleep"),
                           tr("Instantly lock your vault when the operating "
                              "system sleeps or the screen locks."));
  }

  if (m_clipboardCard) {
    m_clipboardCard->setTitle(tr("Clipboard Security"));
  }
  if (m_clipTimeoutRow) {
    m_clipTimeoutRow->updateText(tr("Clear Clipboard"),
                                 tr("Automatically wipe copied passwords and "
                                    "secrets from your system clipboard."));
  }
  if (m_clipLockRow) {
    m_clipLockRow->updateText(tr("Clear on Vault Lock"),
                              tr("Immediately clear sensitive data from "
                                 "clipboard when vault is locked."));
  }

  if (m_biometricCard) {
    m_biometricCard->setTitle(tr("Biometrics"));
  }
  if (m_biometricRow) {
    m_biometricRow->updateText(tr("Biometric Unlock"),
                               tr("Use Touch ID, Windows Hello, or Linux PAM "
                                  "to quickly unlock your vault."));
  }

  if (m_credentialsCard) {
    m_credentialsCard->setTitle(tr("Master Password & Emergency Key"));
  }
  if (m_pwdRow) {
    m_pwdRow->updateText(tr("Master Password"),
                         tr("Update the primary passphrase used to encrypt and "
                            "decrypt your vault data."));
  }
  if (m_changePwdBtn) {
    m_changePwdBtn->setText(tr("Change Master Password..."));
  }
  if (m_recKeyRow) {
    m_recKeyRow->updateText(tr("Emergency Recovery Key"),
                            tr("Inspect your 24-character recovery key. "
                               "Required if you ever forget your password."));
  }
  if (m_viewRecKeyBtn) {
    m_viewRecKeyBtn->setText(tr("View Emergency Key..."));
  }
}

} // namespace svm::views
