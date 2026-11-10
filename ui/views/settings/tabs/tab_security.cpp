/**
 * @file ui/views/settings/tabs/tab_security.cpp
 * @brief Hiện thực giao diện Cài đặt Bảo mật & Quyền riêng tư.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "tab_security.h"
#include "app/clipboard_manager.h"
#include "app/settings_manager.h"
#include "app/theme_manager.h"
#include "app/vault_manager.h"
#include "components/dialog_drag_helper.h"
#include "components/in_window_combobox.h"
#include "components/notification_dialog.h"
#include "components/window_resize_handler.h"
#include "core/core_bridge.h"
#include "views/onboarding/password_entropy_meter.h"

#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace svm::views {

TabSecurity::TabSecurity(QWidget *parent) : QWidget(parent) {
  setAttribute(Qt::WA_StyledBackground, true);
  setupUI();
}

void TabSecurity::setupUI() {
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(18, 14, 14, 16);
  layout->setSpacing(16);

  auto &settings = app::SettingsManager::instance();

  // 1. Auto-Lock Card
  m_autoLockCard = new SettingSectionCard(tr("Auto-Lock"), this);

  m_timeoutCombo = new svm::ui::InWindowComboBox(m_autoLockCard);
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
  m_timeoutCombo->setFixedWidth(200);
  m_timeoutCombo->setFixedHeight(32);
  m_timeoutCombo->setCursor(Qt::PointingHandCursor);
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

  m_clipTimeoutCombo = new svm::ui::InWindowComboBox(m_clipboardCard);
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
  m_clipTimeoutCombo->setFixedWidth(200);
  m_clipTimeoutCombo->setFixedHeight(32);
  m_clipTimeoutCombo->setCursor(Qt::PointingHandCursor);
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

  auto &vaultMgr = app::VaultManager::instance();
  m_biometricCheck = new QCheckBox(m_biometricCard);
  const bool bioEnabled = vaultMgr.isBiometricEnabled();
  m_biometricCheck->setChecked(bioEnabled);
  m_biometricCheck->setEnabled(bioEnabled || vaultMgr.isBiometricAvailable());
  connect(m_biometricCheck, &QCheckBox::toggled, this, [this](bool checked) {
    if (!app::VaultManager::instance().setBiometricEnabled(checked)) {
      const QSignalBlocker blocker(m_biometricCheck);
      m_biometricCheck->setChecked(!checked);
      components::NotificationDialog::warning(
          this, tr("Biometrics"),
          tr("Unable to update biometric unlock while the vault is locked or "
             "OS authentication is unavailable."));
    }
  });
  m_biometricRow =
      new SettingRowWidget(tr("Biometric Unlock"),
                           tr("Use Touch ID, Windows Hello, or Fingerprint to "
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

  m_rotateRecKeyBtn =
      new QPushButton(tr("Regenerate Key..."), m_credentialsCard);
  m_rotateRecKeyBtn->setFixedHeight(32);
  m_rotateRecKeyBtn->setCursor(Qt::PointingHandCursor);
  connect(m_rotateRecKeyBtn, &QPushButton::clicked, this,
          &TabSecurity::openRotateRecoveryKeyDialog);
  m_recKeyRow = new SettingRowWidget(
      tr("Emergency Recovery Key"),
      tr("Generate a new 24-character recovery key. "
         "The current key will be permanently invalidated."),
      m_rotateRecKeyBtn, m_credentialsCard);
  m_credentialsCard->addRow(m_recKeyRow);

  layout->addWidget(m_credentialsCard);
  layout->addStretch(1);
}

void TabSecurity::openChangePasswordDialog() {
  QDialog dlg(this, Qt::FramelessWindowHint | Qt::Dialog);
  dlg.setAttribute(Qt::WA_TranslucentBackground, false);
  dlg.setModal(false);
  dlg.setWindowModality(Qt::NonModal);
  dlg.setMinimumSize(420, 380);
  dlg.resize(480, 440);

  auto *rootLayout = new QVBoxLayout(&dlg);
  rootLayout->setContentsMargins(0, 0, 0, 0);

  auto *card = new QWidget(&dlg);
  card->setObjectName(QStringLiteral("RecoveryDialogCard"));
  card->setAttribute(Qt::WA_StyledBackground, true);

  new components::WindowResizeHandler(&dlg);

  auto *cardLayout = new QVBoxLayout(card);
  cardLayout->setContentsMargins(0, 0, 0, 16);
  cardLayout->setSpacing(0);

  auto *header = new QWidget(card);
  header->setObjectName(QStringLiteral("RecoveryDialogHeader"));
  header->setAttribute(Qt::WA_StyledBackground, true);
  header->setFixedHeight(38);
  auto *headerLayout = new QHBoxLayout(header);
  headerLayout->setContentsMargins(16, 0, 0, 0);
  headerLayout->setSpacing(10);

  auto *headerIcon = new QLabel(header);
  headerIcon->setFixedSize(18, 18);
  headerIcon->setPixmap(app::ThemeManager::instance()
                            .icon(QStringLiteral("lock"))
                            .pixmap(18, 18));
  headerLayout->addWidget(headerIcon);

  auto *title = new QLabel(tr("Change Master Password"), header);
  title->setObjectName(QStringLiteral("RecoveryDialogTitle"));
  headerLayout->addWidget(title, 1);

  auto *closeBtn = new QPushButton(header);
  closeBtn->setObjectName(QStringLiteral("WinCloseBtn"));
  closeBtn->setFixedSize(46, 38);
  closeBtn->setIconSize(QSize(12, 12));
  closeBtn->setFlat(true);
  closeBtn->setFocusPolicy(Qt::NoFocus);
  closeBtn->setCursor(Qt::PointingHandCursor);
  closeBtn->setIcon(
      app::ThemeManager::instance().icon(QStringLiteral("win_close")));
  connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
  headerLayout->addWidget(closeBtn);

  cardLayout->addWidget(header);

  auto *body = new QWidget(card);
  auto *layout = new QVBoxLayout(body);
  layout->setContentsMargins(20, 14, 20, 0);
  layout->setSpacing(8);

  auto *curLabel = new QLabel(tr("Current Master Password:"), body);
  curLabel->setObjectName(QStringLiteral("DialogFieldLabel"));
  layout->addWidget(curLabel);
  auto *curEdit = new QLineEdit(body);
  curEdit->setEchoMode(QLineEdit::Password);
  curEdit->setFixedHeight(34);
  layout->addWidget(curEdit);

  auto *newLabel = new QLabel(tr("New Master Password:"), body);
  newLabel->setObjectName(QStringLiteral("DialogFieldLabel"));
  layout->addWidget(newLabel);
  auto *newEdit = new QLineEdit(body);
  newEdit->setEchoMode(QLineEdit::Password);
  newEdit->setFixedHeight(34);
  layout->addWidget(newEdit);

  auto *meter = new PasswordEntropyMeter(body);
  layout->addWidget(meter);
  connect(newEdit, &QLineEdit::textChanged, meter,
          &PasswordEntropyMeter::setPassword);

  auto *confirmLabel = new QLabel(tr("Confirm New Master Password:"), body);
  confirmLabel->setObjectName(QStringLiteral("DialogFieldLabel"));
  layout->addWidget(confirmLabel);
  auto *confirmEdit = new QLineEdit(body);
  confirmEdit->setEchoMode(QLineEdit::Password);
  confirmEdit->setFixedHeight(34);
  layout->addWidget(confirmEdit);

  auto *errorLabel = new QLabel(body);
  errorLabel->setObjectName(QStringLiteral("DialogErrorLabel"));
  layout->addWidget(errorLabel);

  layout->addStretch(1);

  auto *btnLayout = new QHBoxLayout();
  auto *cancelBtn = new QPushButton(tr("Cancel"), body);
  cancelBtn->setFixedHeight(32);
  cancelBtn->setCursor(Qt::PointingHandCursor);
  connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
  btnLayout->addWidget(cancelBtn);
  btnLayout->addStretch(1);

  auto *okBtn = new QPushButton(tr("Update Password"), body);
  okBtn->setObjectName(QStringLiteral("OnboardingPrimaryBtn"));
  okBtn->setFixedHeight(32);
  okBtn->setCursor(Qt::PointingHandCursor);
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

    if (!app::VaultManager::instance().changeMasterPassword(cur, p1)) {
      errorLabel->setText(tr("Failed to update master password."));
      return;
    }
    components::NotificationDialog::success(
        &dlg, tr("Success"), tr("Master Password updated successfully!"));
    dlg.accept();
  });
  btnLayout->addWidget(okBtn);
  layout->addLayout(btnLayout);

  cardLayout->addWidget(body, 1);
  components::DialogDragHelper::makeDraggable(header, &dlg);
  rootLayout->addWidget(card);

  components::DialogDragHelper::execDialog(dlg);
}

void TabSecurity::openRotateRecoveryKeyDialog() {
  QString newKey;
  bool confirmed = false;

  // Bước 1: Hộp thoại xác minh Master Password & Cảnh báo
  {
    QDialog dlg(window(), Qt::FramelessWindowHint | Qt::Dialog);
    dlg.setAttribute(Qt::WA_TranslucentBackground, false);
    dlg.setModal(false);
    dlg.setWindowModality(Qt::NonModal);
    dlg.setMinimumSize(440, 310);
    dlg.resize(480, 330);

    auto *rootLayout = new QVBoxLayout(&dlg);
    rootLayout->setContentsMargins(0, 0, 0, 0);

    auto *card = new QWidget(&dlg);
    card->setObjectName(QStringLiteral("RecoveryDialogCard"));
    card->setAttribute(Qt::WA_StyledBackground, true);

    new components::WindowResizeHandler(&dlg);

    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(0, 0, 0, 16);
    cardLayout->setSpacing(0);

    auto *header = new QWidget(card);
    header->setObjectName(QStringLiteral("RecoveryDialogHeader"));
    header->setAttribute(Qt::WA_StyledBackground, true);
    header->setFixedHeight(38);
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(16, 0, 0, 0);
    headerLayout->setSpacing(10);

    auto *headerIcon = new QLabel(header);
    headerIcon->setFixedSize(18, 18);
    headerIcon->setPixmap(app::ThemeManager::instance()
                              .icon(QStringLiteral("watchtower_weak"))
                              .pixmap(18, 18));
    headerLayout->addWidget(headerIcon);

    auto *title = new QLabel(tr("Regenerate Emergency Key"), header);
    title->setObjectName(QStringLiteral("RecoveryDialogTitle"));
    headerLayout->addWidget(title, 1);

    auto *closeBtn = new QPushButton(header);
    closeBtn->setObjectName(QStringLiteral("WinCloseBtn"));
    closeBtn->setFixedSize(46, 38);
    closeBtn->setIconSize(QSize(12, 12));
    closeBtn->setFlat(true);
    closeBtn->setFocusPolicy(Qt::NoFocus);
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setIcon(
        app::ThemeManager::instance().icon(QStringLiteral("win_close")));
    connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    headerLayout->addWidget(closeBtn);

    cardLayout->addWidget(header);

    auto *body = new QWidget(card);
    auto *layout = new QVBoxLayout(body);
    layout->setContentsMargins(20, 14, 20, 0);
    layout->setSpacing(10);

    auto *warningLabel = new QLabel(
        tr("Warning: Regenerating your recovery key will immediately and "
           "permanently invalidate your existing recovery key. Anyone holding "
           "the previous key will no longer be able to restore your vault."),
        body);
    warningLabel->setObjectName(QStringLiteral("DialogFieldLabel"));
    warningLabel->setWordWrap(true);
    layout->addWidget(warningLabel);

    auto *pwdLabel =
        new QLabel(tr("Enter your Master Password to confirm:"), body);
    pwdLabel->setObjectName(QStringLiteral("DialogFieldLabel"));
    layout->addWidget(pwdLabel);

    auto *pwdEdit = new QLineEdit(body);
    pwdEdit->setEchoMode(QLineEdit::Password);
    pwdEdit->setFixedHeight(34);
    layout->addWidget(pwdEdit);

    auto *errorLabel = new QLabel(body);
    errorLabel->setObjectName(QStringLiteral("DialogErrorLabel"));
    layout->addWidget(errorLabel);

    layout->addStretch(1);

    auto *btnLayout = new QHBoxLayout();
    auto *cancelBtn = new QPushButton(tr("Cancel"), body);
    cancelBtn->setFixedHeight(32);
    cancelBtn->setCursor(Qt::PointingHandCursor);
    connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    btnLayout->addWidget(cancelBtn);
    btnLayout->addStretch(1);

    auto *rotateBtn = new QPushButton(tr("Regenerate Key"), body);
    rotateBtn->setObjectName(QStringLiteral("NotificationDangerBtn"));
    rotateBtn->setFixedHeight(32);
    rotateBtn->setCursor(Qt::PointingHandCursor);

    auto doRotate = [&]() {
      const QString pwd = pwdEdit->text();
      if (pwd.isEmpty()) {
        errorLabel->setText(tr("Please enter your Master Password."));
        return;
      }
      if (!app::VaultManager::instance().validateMasterPassword(pwd)) {
        errorLabel->setText(tr("Incorrect master password."));
        return;
      }

      newKey = app::VaultManager::instance().rotateRecoveryKey(pwd);
      if (newKey.isEmpty()) {
        errorLabel->setText(tr("Failed to regenerate recovery key."));
        return;
      }

      confirmed = true;
      dlg.accept();
    };

    connect(rotateBtn, &QPushButton::clicked, &dlg, doRotate);
    connect(pwdEdit, &QLineEdit::returnPressed, &dlg, doRotate);
    btnLayout->addWidget(rotateBtn);
    layout->addLayout(btnLayout);

    cardLayout->addWidget(body, 1);
    components::DialogDragHelper::makeDraggable(header, &dlg);
    rootLayout->addWidget(card);

    if (components::DialogDragHelper::execDialog(dlg) != QDialog::Accepted ||
        !confirmed || newKey.isEmpty()) {
      return;
    }
  }

  // Bước 2: Hiển thị mã khôi phục mới 1 lần duy nhất (hoàn toàn tách biệt)
  {
    QDialog showDlg(window(), Qt::FramelessWindowHint | Qt::Dialog);
    showDlg.setAttribute(Qt::WA_TranslucentBackground, false);
    showDlg.setModal(false);
    showDlg.setWindowModality(Qt::NonModal);
    showDlg.setMinimumSize(480, 280);
    showDlg.resize(520, 310);

    auto *showRoot = new QVBoxLayout(&showDlg);
    showRoot->setContentsMargins(0, 0, 0, 0);

    auto *showCard = new QWidget(&showDlg);
    showCard->setObjectName(QStringLiteral("RecoveryDialogCard"));
    showCard->setAttribute(Qt::WA_StyledBackground, true);

    new components::WindowResizeHandler(&showDlg);

    auto *showCardLayout = new QVBoxLayout(showCard);
    showCardLayout->setContentsMargins(0, 0, 0, 16);
    showCardLayout->setSpacing(0);

    auto *showHeader = new QWidget(showCard);
    showHeader->setObjectName(QStringLiteral("RecoveryDialogHeader"));
    showHeader->setAttribute(Qt::WA_StyledBackground, true);
    showHeader->setFixedHeight(38);
    auto *showHeaderLayout = new QHBoxLayout(showHeader);
    showHeaderLayout->setContentsMargins(16, 0, 0, 0);
    showHeaderLayout->setSpacing(10);

    auto *showIcon = new QLabel(showHeader);
    showIcon->setFixedSize(18, 18);
    showIcon->setPixmap(app::ThemeManager::instance()
                            .icon(QStringLiteral("shield_check"))
                            .pixmap(18, 18));
    showHeaderLayout->addWidget(showIcon);

    auto *h = new QLabel(tr("Your New Emergency Recovery Key"), showHeader);
    h->setObjectName(QStringLiteral("RecoveryDialogTitle"));
    showHeaderLayout->addWidget(h, 1);

    auto *showCloseBtn = new QPushButton(showHeader);
    showCloseBtn->setObjectName(QStringLiteral("WinCloseBtn"));
    showCloseBtn->setFixedSize(46, 38);
    showCloseBtn->setIconSize(QSize(12, 12));
    showCloseBtn->setFlat(true);
    showCloseBtn->setFocusPolicy(Qt::NoFocus);
    showCloseBtn->setCursor(Qt::PointingHandCursor);
    showCloseBtn->setIcon(
        app::ThemeManager::instance().icon(QStringLiteral("win_close")));
    connect(showCloseBtn, &QPushButton::clicked, &showDlg, &QDialog::accept);
    showHeaderLayout->addWidget(showCloseBtn);

    showCardLayout->addWidget(showHeader);

    auto *showBody = new QWidget(showCard);
    auto *showLayout = new QVBoxLayout(showBody);
    showLayout->setContentsMargins(20, 14, 20, 0);
    showLayout->setSpacing(12);

    auto *noticeLabel =
        new QLabel(tr("Important: This new recovery key is shown ONCE ONLY. "
                      "Write it down or save it in a safe offline location. "
                      "Your old key is now completely invalid."),
                   showBody);
    noticeLabel->setObjectName(QStringLiteral("DialogFieldLabel"));
    noticeLabel->setWordWrap(true);
    showLayout->addWidget(noticeLabel);

    auto *box = new QWidget(showBody);
    box->setObjectName(QStringLiteral("RecoveryBox"));
    auto *boxLayout = new QVBoxLayout(box);
    boxLayout->setContentsMargins(14, 12, 14, 12);

    auto *keyLabel = new QLabel(newKey, box);
    keyLabel->setObjectName(QStringLiteral("RecoveryKeyDisplay"));
    keyLabel->setAlignment(Qt::AlignCenter);
    keyLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    boxLayout->addWidget(keyLabel);
    showLayout->addWidget(box);

    auto *btnRow = new QHBoxLayout();
    auto *copyBtn = new QPushButton(tr("Copy to Clipboard"), showBody);
    copyBtn->setObjectName(QStringLiteral("RecoveryActionBtn"));
    copyBtn->setFixedHeight(34);
    copyBtn->setCursor(Qt::PointingHandCursor);
    connect(copyBtn, &QPushButton::clicked, &showDlg, [copyBtn, newKey]() {
      app::ClipboardManager::instance().copySensitive(newKey,
                                                      tr("Recovery Key"));
      copyBtn->setText(tr("Copied!"));
    });
    btnRow->addWidget(copyBtn);

    auto *doneBtn = new QPushButton(tr("I Have Saved My Key"), showBody);
    doneBtn->setObjectName(QStringLiteral("OnboardingPrimaryBtn"));
    doneBtn->setFixedHeight(34);
    doneBtn->setCursor(Qt::PointingHandCursor);
    connect(doneBtn, &QPushButton::clicked, &showDlg, &QDialog::accept);
    btnRow->addWidget(doneBtn);

    showLayout->addLayout(btnRow);

    showCardLayout->addWidget(showBody, 1);
    components::DialogDragHelper::makeDraggable(showHeader, &showDlg);
    showRoot->addWidget(showCard);

    components::DialogDragHelper::execDialog(showDlg);
  }
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
                               tr("Use Touch ID, Windows Hello, or Fingerprint "
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
    m_recKeyRow->updateText(
        tr("Emergency Recovery Key"),
        tr("Generate a new 24-character recovery key. "
           "The current key will be permanently invalidated."));
  }
  if (m_rotateRecKeyBtn) {
    m_rotateRecKeyBtn->setText(tr("Regenerate Key..."));
  }
}

} // namespace svm::views
