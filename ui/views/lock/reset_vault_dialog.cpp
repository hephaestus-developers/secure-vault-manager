/**
 * @file ui/views/lock/reset_vault_dialog.cpp
 * @brief Hiện thực hộp thoại cảnh báo và xác nhận đặt lại kho an toàn khi mất
 * cả Master Password và Recovery Key.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "reset_vault_dialog.h"
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

ResetVaultDialog::ResetVaultDialog(QWidget *parent)
    : QDialog(parent, Qt::FramelessWindowHint | Qt::Dialog) {
  setObjectName(QStringLiteral("ResetVaultDialog"));
  setAttribute(Qt::WA_TranslucentBackground, false);
  setModal(false);
  setWindowModality(Qt::NonModal);
  setMinimumSize(520, 500);
  resize(540, 520);

  setupUI();

  new components::WindowResizeHandler(this);
}

void ResetVaultDialog::setupUI() {
  auto *rootLayout = new QVBoxLayout(this);
  rootLayout->setContentsMargins(0, 0, 0, 0);

  m_cardWidget = new QWidget(this);
  m_cardWidget->setObjectName(QStringLiteral("RecoveryDialogCard"));
  m_cardWidget->setAttribute(Qt::WA_StyledBackground, true);

  auto *cardLayout = new QVBoxLayout(m_cardWidget);
  cardLayout->setContentsMargins(0, 0, 0, 16);
  cardLayout->setSpacing(0);

  // 1. Thanh tiêu đề kéo thả
  m_headerWidget = new QWidget(m_cardWidget);
  m_headerWidget->setObjectName(QStringLiteral("RecoveryDialogHeader"));
  m_headerWidget->setAttribute(Qt::WA_StyledBackground, true);
  m_headerWidget->setFixedHeight(38);

  auto *headerLayout = new QHBoxLayout(m_headerWidget);
  headerLayout->setContentsMargins(16, 0, 0, 0);
  headerLayout->setSpacing(10);

  m_headerIcon = new QLabel(m_headerWidget);
  m_headerIcon->setFixedSize(18, 18);
  m_headerIcon->setPixmap(
      QIcon(QStringLiteral(":/icons/brand/app_logo.svg")).pixmap(18, 18));
  headerLayout->addWidget(m_headerIcon);

  m_headerTitle = new QLabel(tr("Reset Vault"), m_headerWidget);
  m_headerTitle->setObjectName(QStringLiteral("RecoveryDialogTitle"));
  QFont hf = m_headerTitle->font();
  hf.setPointSize(11);
  hf.setBold(true);
  m_headerTitle->setFont(hf);
  headerLayout->addWidget(m_headerTitle, 1);

  m_closeBtn = new QPushButton(m_headerWidget);
  m_closeBtn->setObjectName(QStringLiteral("WinCloseBtn"));
  m_closeBtn->setFixedSize(46, 38);
  m_closeBtn->setIconSize(QSize(12, 12));
  m_closeBtn->setFlat(true);
  m_closeBtn->setFocusPolicy(Qt::NoFocus);
  m_closeBtn->setCursor(Qt::PointingHandCursor);
  m_closeBtn->setIcon(
      app::ThemeManager::instance().icon(QStringLiteral("win_close")));
  connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::reject);
  headerLayout->addWidget(m_closeBtn);

  cardLayout->addWidget(m_headerWidget);

  // 2. Nội dung nhiều trang
  m_pageStack = new QStackedWidget(m_cardWidget);
  m_pageStack->addWidget(createWarningPage()); // Index 0
  m_pageStack->addWidget(createConfirmPage()); // Index 1
  cardLayout->addWidget(m_pageStack, 1);

  components::DialogDragHelper::makeDraggable(m_headerWidget, this);
  rootLayout->addWidget(m_cardWidget);
}

QWidget *ResetVaultDialog::createWarningPage() {
  auto *page = new QWidget(m_pageStack);
  auto *layout = new QVBoxLayout(page);
  layout->setContentsMargins(24, 20, 24, 8);
  layout->setSpacing(14);

  // Icon cảnh báo lớn
  auto *iconRow = new QHBoxLayout();
  iconRow->setContentsMargins(0, 0, 0, 0);
  iconRow->setSpacing(12);
  iconRow->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

  auto *bigIcon = new QLabel(page);
  bigIcon->setFixedSize(36, 36);
  bigIcon->setPixmap(app::ThemeManager::instance()
                         .icon(QStringLiteral("watchtower_weak"))
                         .pixmap(36, 36));
  iconRow->addWidget(bigIcon);

  m_warnTitleLabel = new QLabel(tr("Cannot Recover Encrypted Data"), page);
  QFont tf = m_warnTitleLabel->font();
  tf.setPointSize(14);
  tf.setBold(true);
  m_warnTitleLabel->setFont(tf);
  m_warnTitleLabel->setStyleSheet(QStringLiteral("color: #f38ba8;"));
  iconRow->addWidget(m_warnTitleLabel, 1);
  layout->addLayout(iconRow);

  m_warnDescLabel = new QLabel(
      tr("Secure Vault Manager operates on a strict Zero-Knowledge "
         "architecture. Your vault is encrypted end-to-end using your Master "
         "Password and Recovery Key.\n\nWithout both of these credentials, it "
         "is cryptographically impossible for anyone - including the "
         "developers - "
         "to decrypt or retrieve your stored passwords."),
      page);
  m_warnDescLabel->setWordWrap(true);
  m_warnDescLabel->setObjectName(QStringLiteral("DialogDescLabel"));
  m_warnDescLabel->setStyleSheet(
      QStringLiteral("line-height: 1.5; font-size: 13px;"));
  layout->addWidget(m_warnDescLabel);

  // Khung thông báo Safety Net
  auto *safetyBox = new QWidget(page);
  safetyBox->setObjectName(QStringLiteral("SafetyNetBox"));
  safetyBox->setStyleSheet(
      QStringLiteral("background-color: rgba(137, 180, 250, 0.08); border: 1px "
                     "solid rgba(137, 180, 250, 0.25); border-radius: 8px; "
                     "padding: 10px;"));
  auto *safetyLayout = new QVBoxLayout(safetyBox);
  safetyLayout->setContentsMargins(12, 10, 12, 10);
  safetyLayout->setSpacing(6);

  m_safetyTitle = new QLabel(tr("Safety Net Automatic Backup"), safetyBox);
  QFont sf = m_safetyTitle->font();
  sf.setBold(true);
  m_safetyTitle->setFont(sf);
  m_safetyTitle->setStyleSheet(
      QStringLiteral("color: #89b4fa; font-size: 12px;"));
  safetyLayout->addWidget(m_safetyTitle);

  m_backupNoticeLabel = new QLabel(
      tr("Before resetting, an archived backup of your current encrypted vault "
         "file will be automatically preserved in:\n"
         "~/.local/share/hephaestus/secure-vault-manager-backups/\n\n"
         "If you remember your password in the future, your old data can still "
         "be rescued."),
      safetyBox);
  m_backupNoticeLabel->setWordWrap(true);
  m_backupNoticeLabel->setStyleSheet(
      QStringLiteral("color: #cdd6f4; font-size: 11.5px; line-height: 1.4;"));
  safetyLayout->addWidget(m_backupNoticeLabel);
  layout->addWidget(safetyBox);

  layout->addStretch(1);

  // Thanh nút hành động
  auto *btnLayout = new QHBoxLayout();
  m_cancelBtn = new QPushButton(tr("Go Back / Try Again"), page);
  m_cancelBtn->setFixedHeight(36);
  m_cancelBtn->setCursor(Qt::PointingHandCursor);
  connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
  btnLayout->addWidget(m_cancelBtn);

  btnLayout->addStretch(1);

  m_proceedBtn = new QPushButton(tr("I Understand, Continue Reset"), page);
  m_proceedBtn->setFixedHeight(36);
  m_proceedBtn->setCursor(Qt::PointingHandCursor);
  m_proceedBtn->setStyleSheet(QStringLiteral(
      "QPushButton { background-color: transparent; border: 1px "
      "solid #f38ba8; color: #f38ba8; border-radius: 6px; "
      "padding: 0 14px; font-weight: 600; font-size: 12.5px; } "
      "QPushButton:hover { background-color: rgba(243, 139, 168, "
      "0.15); }"));
  connect(m_proceedBtn, &QPushButton::clicked, this,
          &ResetVaultDialog::onProceedToConfirm);
  btnLayout->addWidget(m_proceedBtn);

  layout->addLayout(btnLayout);
  return page;
}

QWidget *ResetVaultDialog::createConfirmPage() {
  auto *page = new QWidget(m_pageStack);
  auto *layout = new QVBoxLayout(page);
  layout->setContentsMargins(24, 20, 24, 8);
  layout->setSpacing(14);

  auto *confirmTitleRow = new QHBoxLayout();
  confirmTitleRow->setContentsMargins(0, 0, 0, 0);
  confirmTitleRow->setSpacing(10);
  confirmTitleRow->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

  auto *confirmIcon = new QLabel(page);
  confirmIcon->setFixedSize(22, 22);
  confirmIcon->setPixmap(app::ThemeManager::instance()
                             .icon(QStringLiteral("watchtower_weak"))
                             .pixmap(22, 22));
  confirmTitleRow->addWidget(confirmIcon);

  m_confirmTitleLabel = new QLabel(tr("Confirm Vault Reset"), page);
  QFont tf = m_confirmTitleLabel->font();
  tf.setPointSize(14);
  tf.setBold(true);
  m_confirmTitleLabel->setFont(tf);
  confirmTitleRow->addWidget(m_confirmTitleLabel, 1);
  layout->addLayout(confirmTitleRow);

  m_confirmPromptLabel = new QLabel(
      tr("This action will erase the active local vault and return the "
         "application to initial setup.<br><br>To confirm, please type "
         "<b>RESET</b> in the box below:"),
      page);
  m_confirmPromptLabel->setTextFormat(Qt::RichText);
  m_confirmPromptLabel->setWordWrap(true);
  m_confirmPromptLabel->setObjectName(QStringLiteral("DialogDescLabel"));
  m_confirmPromptLabel->setStyleSheet(
      QStringLiteral("font-size: 13px; line-height: 1.5;"));
  layout->addWidget(m_confirmPromptLabel);

  m_confirmInput = new QLineEdit(page);
  m_confirmInput->setPlaceholderText(tr("Type RESET to confirm..."));
  m_confirmInput->setFixedHeight(36);
  m_confirmInput->setStyleSheet(
      QStringLiteral("font-size: 13px; padding: 0 10px; font-weight: bold;"));
  layout->addWidget(m_confirmInput);

  m_confirmErrorLabel = new QLabel(page);
  m_confirmErrorLabel->setObjectName(QStringLiteral("DialogErrorLabel"));
  m_confirmErrorLabel->setStyleSheet(
      QStringLiteral("color: #f38ba8; font-size: 12px;"));
  layout->addWidget(m_confirmErrorLabel);

  layout->addStretch(1);

  // Thanh nút hành động
  auto *btnLayout = new QHBoxLayout();
  m_backBtn = new QPushButton(tr("Back"), page);
  m_backBtn->setFixedHeight(36);
  m_backBtn->setCursor(Qt::PointingHandCursor);
  connect(m_backBtn, &QPushButton::clicked, this,
          [this]() { m_pageStack->setCurrentIndex(0); });
  btnLayout->addWidget(m_backBtn);

  btnLayout->addStretch(1);

  m_executeResetBtn =
      new QPushButton(tr("Permanently Reset && Start Fresh"), page);
  m_executeResetBtn->setObjectName(QStringLiteral("OnboardingDestructiveBtn"));
  m_executeResetBtn->setFixedHeight(36);
  m_executeResetBtn->setEnabled(false);
  m_executeResetBtn->setCursor(Qt::PointingHandCursor);
  m_executeResetBtn->setStyleSheet(
      QStringLiteral("QPushButton { background-color: #f38ba8; color: #11111b; "
                     "border: none; border-radius: 6px; padding: 0 16px; "
                     "font-weight: 700; font-size: 12.5px; } "
                     "QPushButton:hover { background-color: #eba0ac; } "
                     "QPushButton:disabled { background-color: #45475a; color: "
                     "#6c7086; }"));
  connect(m_executeResetBtn, &QPushButton::clicked, this,
          &ResetVaultDialog::onPerformReset);
  btnLayout->addWidget(m_executeResetBtn);

  layout->addLayout(btnLayout);

  connect(m_confirmInput, &QLineEdit::textChanged, this,
          [this](const QString &text) {
            const bool matches =
                text.trimmed().compare(QStringLiteral("RESET"),
                                       Qt::CaseInsensitive) == 0;
            m_executeResetBtn->setEnabled(matches);
            if (matches) {
              m_confirmErrorLabel->clear();
            }
          });

  connect(m_confirmInput, &QLineEdit::returnPressed, this, [this]() {
    if (m_executeResetBtn->isEnabled()) {
      onPerformReset();
    }
  });

  return page;
}

void ResetVaultDialog::onProceedToConfirm() {
  m_pageStack->setCurrentIndex(1);
  m_confirmInput->clear();
  m_confirmInput->setFocus();
}

void ResetVaultDialog::onPerformReset() {
  if (m_confirmInput->text().trimmed().compare(QStringLiteral("RESET"),
                                               Qt::CaseInsensitive) != 0) {
    m_confirmErrorLabel->setText(tr("Please type RESET exactly to confirm."));
    return;
  }

  // Thực hiện đặt lại kho toàn diện
  app::VaultManager::instance().resetAllData();
  accept();
}

void ResetVaultDialog::retranslateUi() {
  if (m_headerTitle) {
    m_headerTitle->setText(tr("Reset Vault"));
  }
  if (m_warnTitleLabel) {
    m_warnTitleLabel->setText(tr("Cannot Recover Encrypted Data"));
  }
  if (m_warnDescLabel) {
    m_warnDescLabel->setText(tr(
        "Secure Vault Manager operates on a strict Zero-Knowledge "
        "architecture. Your vault is encrypted end-to-end using your Master "
        "Password and Recovery Key.\n\nWithout both of these credentials, it "
        "is cryptographically impossible for anyone - including the developers "
        "- "
        "to decrypt or retrieve your stored passwords."));
  }
  if (m_safetyTitle) {
    m_safetyTitle->setText(tr("Safety Net Automatic Backup"));
  }
  if (m_backupNoticeLabel) {
    m_backupNoticeLabel->setText(tr(
        "Before resetting, an archived backup of your current encrypted vault "
        "file will be automatically preserved in:\n"
        "~/.local/share/hephaestus/secure-vault-manager-backups/\n\n"
        "If you remember your password in the future, your old data can still "
        "be rescued."));
  }
  if (m_cancelBtn) {
    m_cancelBtn->setText(tr("Go Back / Try Again"));
  }
  if (m_proceedBtn) {
    m_proceedBtn->setText(tr("I Understand, Continue Reset"));
  }
  if (m_confirmTitleLabel) {
    m_confirmTitleLabel->setText(tr("Confirm Vault Reset"));
  }
  if (m_confirmPromptLabel) {
    m_confirmPromptLabel->setText(
        tr("This action will erase the active local vault and return the "
           "application to initial setup.<br><br>To confirm, please type "
           "<b>RESET</b> in the box below:"));
  }
  if (m_confirmInput) {
    m_confirmInput->setPlaceholderText(tr("Type RESET to confirm..."));
  }
  if (m_backBtn) {
    m_backBtn->setText(tr("Back"));
  }
  if (m_executeResetBtn) {
    m_executeResetBtn->setText(tr("Permanently Reset && Start Fresh"));
  }
}

void ResetVaultDialog::changeEvent(QEvent *event) {
  if (event->type() == QEvent::LanguageChange) {
    retranslateUi();
  }
  QDialog::changeEvent(event);
}

void ResetVaultDialog::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Escape) {
    reject();
    return;
  }
  QDialog::keyPressEvent(event);
}

} // namespace svm::views
