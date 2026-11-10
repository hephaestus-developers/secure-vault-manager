/**
 * @file ui/views/detail/totp_widget.cpp
 * @brief Hiện thực TotpWidget lồng trong thẻ Primary Credentials.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "totp_widget.h"
#include "app/clipboard_manager.h"
#include "app/theme_manager.h"
#include "core/core_bridge.h"

#include <QDateTime>
#include <QEvent>
#include <QHBoxLayout>
#include <QIcon>
#include <QVBoxLayout>

namespace svm::views {

TotpWidget::TotpWidget(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("TotpFieldWidget"));
  setAttribute(Qt::WA_StyledBackground, true);
  setupUI();

  m_timer.setInterval(1000);
  connect(&m_timer, &QTimer::timeout, this, &TotpWidget::onTick);
}

void TotpWidget::setupUI() {
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(12, 8, 12, 8);
  layout->setSpacing(0);

  // 1. Chế độ Xem (View Mode)
  m_viewContainer = new QWidget(this);
  auto *viewLayout = new QVBoxLayout(m_viewContainer);
  viewLayout->setContentsMargins(0, 0, 0, 0);
  viewLayout->setSpacing(6);

  // Dòng tiêu đề: Label 2FA + Thời gian còn lại
  auto *headerLayout = new QHBoxLayout();
  m_viewTitleLabel = new QLabel(tr("ONE-TIME CODE (2FA)"), m_viewContainer);
  m_viewTitleLabel->setObjectName(QStringLiteral("TotpSectionHeader"));
  headerLayout->addWidget(m_viewTitleLabel);

  headerLayout->addStretch(1);

  m_timerLabel = new QLabel(QStringLiteral("30s"), m_viewContainer);
  m_timerLabel->setObjectName(QStringLiteral("TotpTimerLabel"));
  headerLayout->addWidget(m_timerLabel);
  viewLayout->addLayout(headerLayout);

  // Dòng mã 6 số + Nút Copy
  auto *codeLayout = new QHBoxLayout();
  m_codeLabel = new QLabel(QStringLiteral("--- ---"), m_viewContainer);
  m_codeLabel->setObjectName(QStringLiteral("TotpCodeDisplay"));
  QFont codeFont(QStringLiteral("monospace"), 16, QFont::Bold);
  m_codeLabel->setFont(codeFont);
  m_codeLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
  codeLayout->addWidget(m_codeLabel);

  codeLayout->addStretch(1);

  m_copyBtn = new QPushButton(m_viewContainer);
  m_copyBtn->setObjectName(QStringLiteral("TotpCopyBtn"));
  m_copyBtn->setIcon(QIcon(QStringLiteral(":/icons/actions/copy.svg")));
  m_copyBtn->setIconSize(QSize(16, 16));
  m_copyBtn->setFixedSize(28, 28);
  m_copyBtn->setToolTip(tr("Copy one-time code"));
  m_copyBtn->setCursor(Qt::PointingHandCursor);
  connect(m_copyBtn, &QPushButton::clicked, this, [this]() {
    const QString cleanCode = m_currentCode.trimmed().remove(QLatin1Char(' '));
    if (!cleanCode.isEmpty()) {
      app::ClipboardManager::instance().copySensitive(
          cleanCode, tr("One-Time Code (2FA)"), 30);
    }
  });
  codeLayout->addWidget(m_copyBtn);
  viewLayout->addLayout(codeLayout);

  // Thanh tiến trình đếm lùi
  m_progressBar = new QProgressBar(m_viewContainer);
  m_progressBar->setObjectName(QStringLiteral("TotpProgressBar"));
  m_progressBar->setRange(0, 30);
  m_progressBar->setTextVisible(false);
  m_progressBar->setFixedHeight(4);
  viewLayout->addWidget(m_progressBar);

  layout->addWidget(m_viewContainer);

  // 2. Chế độ Chỉnh sửa (Edit Mode)
  m_editContainer = new QWidget(this);
  auto *editLayout = new QVBoxLayout(m_editContainer);
  editLayout->setContentsMargins(0, 0, 0, 0);
  editLayout->setSpacing(4);

  m_editTitleLabel =
      new QLabel(tr("ONE-TIME CODE SECRET KEY (2FA)"), m_editContainer);
  m_editTitleLabel->setObjectName(QStringLiteral("TotpSectionHeader"));
  editLayout->addWidget(m_editTitleLabel);

  auto *inputRow = new QHBoxLayout();
  inputRow->setContentsMargins(0, 0, 0, 0);
  inputRow->setSpacing(6);

  m_secretInput = new QLineEdit(m_editContainer);
  m_secretInput->setObjectName(QStringLiteral("FieldRowInput"));
  m_secretInput->setEchoMode(QLineEdit::Password);
  m_secretInput->setPlaceholderText(
      tr("Paste Base32 secret (e.g. JBSWY3DPEHPK3PXP)"));
  inputRow->addWidget(m_secretInput, 1);

  m_revealSecretBtn = new QPushButton(m_editContainer);
  m_revealSecretBtn->setObjectName(QStringLiteral("FieldRowActionBtn"));
  m_revealSecretBtn->setIcon(QIcon(QStringLiteral(":/icons/actions/eye.svg")));
  m_revealSecretBtn->setIconSize(QSize(16, 16));
  m_revealSecretBtn->setFixedSize(28, 28);
  m_revealSecretBtn->setCursor(Qt::PointingHandCursor);
  m_revealSecretBtn->setToolTip(tr("Show / Hide Secret Key"));
  connect(m_revealSecretBtn, &QPushButton::clicked, this,
          &TotpWidget::onToggleRevealSecret);
  inputRow->addWidget(m_revealSecretBtn);

  m_removeBtn = new QPushButton(m_editContainer);
  m_removeBtn->setObjectName(QStringLiteral("CustomFieldDeleteBtn"));
  m_removeBtn->setIcon(
      app::ThemeManager::instance().icon(QStringLiteral("trash")));
  m_removeBtn->setIconSize(QSize(16, 16));
  m_removeBtn->setFixedSize(28, 28);
  m_removeBtn->setCursor(Qt::PointingHandCursor);
  m_removeBtn->setToolTip(tr("Remove 2FA"));
  connect(m_removeBtn, &QPushButton::clicked, this,
          &TotpWidget::removeRequested);
  inputRow->addWidget(m_removeBtn);

  editLayout->addLayout(inputRow);
  layout->addWidget(m_editContainer);

  connect(&app::ThemeManager::instance(), &app::ThemeManager::themeChanged,
          this, [this]() { updateThemeIcons(); });

  updateThemeIcons();
  m_editContainer->hide();
}

void TotpWidget::updateThemeIcons() {
  auto &tm = app::ThemeManager::instance();
  if (m_copyBtn) {
    m_copyBtn->setIcon(tm.icon(QStringLiteral("copy")));
  }
  if (m_revealSecretBtn) {
    m_revealSecretBtn->setIcon(tm.icon(m_isSecretRevealed
                                           ? QStringLiteral("eye_off")
                                           : QStringLiteral("eye")));
  }
  if (m_removeBtn) {
    m_removeBtn->setIcon(tm.icon(QStringLiteral("trash")));
  }
}

void TotpWidget::setTotpSecret(const QString &secret) {
  m_secret = secret;
  if (m_secretInput) {
    m_secretInput->setText(secret);
  }
  if (m_secret.isEmpty()) {
    clearTotp();
    return;
  }

  updateCodeAndTimer();
  if (!m_isEditMode) {
    m_timer.start();
  }
}

QString TotpWidget::totpSecret() const {
  QString raw = (m_isEditMode && m_secretInput)
                    ? m_secretInput->text().trimmed()
                    : m_secret.trimmed();
  if (raw.startsWith(QStringLiteral("otpauth://"), Qt::CaseInsensitive)) {
    const auto parsed = core::CoreBridge::instance().parseOtpAuth(raw);
    const QString extracted = parsed.value(QStringLiteral("secret")).toString();
    if (!extracted.isEmpty()) {
      return extracted;
    }
  }
  return raw;
}

void TotpWidget::clearTotp() {
  m_timer.stop();
  m_secret.clear();
  m_currentCode.clear();
  if (m_secretInput) {
    m_secretInput->clear();
  }
  if (m_codeLabel) {
    m_codeLabel->setText(QStringLiteral("--- ---"));
  }
}

void TotpWidget::focusSecretInput() {
  if (m_secretInput) {
    m_secretInput->setFocus();
    m_secretInput->selectAll();
  }
}

void TotpWidget::setEditMode(bool editable) {
  m_isEditMode = editable;
  if (m_isEditMode) {
    m_timer.stop();
    m_viewContainer->hide();
    m_editContainer->show();
    m_secretInput->setText(m_secret);
    m_isSecretRevealed = false;
    m_secretInput->setEchoMode(QLineEdit::Password);
    m_revealSecretBtn->setIcon(
        QIcon(QStringLiteral(":/icons/actions/eye.svg")));
  } else {
    m_editContainer->hide();
    m_secret = m_secretInput->text().trimmed();
    if (!m_secret.isEmpty()) {
      updateCodeAndTimer();
      m_timer.start();
      m_viewContainer->show();
    } else {
      m_viewContainer->hide();
      m_timer.stop();
    }
  }
}

void TotpWidget::onToggleRevealSecret() {
  m_isSecretRevealed = !m_isSecretRevealed;
  if (m_isSecretRevealed) {
    m_secretInput->setEchoMode(QLineEdit::Normal);
    m_revealSecretBtn->setIcon(
        app::ThemeManager::instance().icon(QStringLiteral("eye_off")));
  } else {
    m_secretInput->setEchoMode(QLineEdit::Password);
    m_revealSecretBtn->setIcon(
        app::ThemeManager::instance().icon(QStringLiteral("eye")));
  }
}

void TotpWidget::onTick() { updateCodeAndTimer(); }

void TotpWidget::updateCodeAndTimer() {
  const qint64 currentSec = QDateTime::currentSecsSinceEpoch();
  const int remainder = static_cast<int>(currentSec % 30);
  m_remainingSeconds = 30 - remainder;

  if (!m_secret.isEmpty()) {
    auto &bridge = core::CoreBridge::instance();
    if (bridge.isInitialized()) {
      const auto pair =
          bridge.generateTotp(m_secret, static_cast<quint64>(currentSec));
      QString code = pair.first.trimmed();
      if (code.length() == 6) {
        code = code.left(3) + QLatin1Char(' ') + code.mid(3);
      }
      m_remainingSeconds = pair.second;
      m_currentCode = code;
      m_codeLabel->setText(code);
    } else {
      const quint64 timeStep = static_cast<quint64>(currentSec / 30);
      const quint64 pseudoCode = (qHash(m_secret) ^ timeStep) % 1000000;
      const QString formatted =
          QStringLiteral("%1 %2")
              .arg(pseudoCode / 1000, 3, 10, QLatin1Char('0'))
              .arg(pseudoCode % 1000, 3, 10, QLatin1Char('0'));
      m_currentCode = formatted;
      m_codeLabel->setText(formatted);
    }
  }

  m_timerLabel->setText(QStringLiteral("%1s").arg(m_remainingSeconds));
  m_progressBar->setValue(m_remainingSeconds);
}

void TotpWidget::retranslateUi() {
  if (m_viewTitleLabel) {
    m_viewTitleLabel->setText(tr("ONE-TIME CODE (2FA)"));
  }
  if (m_copyBtn) {
    m_copyBtn->setToolTip(tr("Copy one-time code"));
  }
  if (m_editTitleLabel) {
    m_editTitleLabel->setText(tr("ONE-TIME CODE SECRET KEY (2FA)"));
  }
  if (m_secretInput) {
    m_secretInput->setPlaceholderText(
        tr("Paste Base32 secret (e.g. JBSWY3DPEHPK3PXP)"));
  }
  if (m_revealSecretBtn) {
    m_revealSecretBtn->setToolTip(tr("Show / Hide Secret Key"));
  }
  if (m_removeBtn) {
    m_removeBtn->setToolTip(tr("Remove 2FA"));
  }
}

void TotpWidget::changeEvent(QEvent *event) {
  if (event->type() == QEvent::LanguageChange) {
    retranslateUi();
  }
  QWidget::changeEvent(event);
}

} // namespace svm::views
