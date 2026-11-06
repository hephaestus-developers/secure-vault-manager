/**
 * @file ui/views/onboarding/password_entropy_meter.cpp
 * @brief Hiện thực widget trực quan đánh giá độ mạnh của Master Password.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "password_entropy_meter.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QVBoxLayout>

namespace svm::views {

PasswordEntropyMeter::PasswordEntropyMeter(QWidget *parent) : QWidget(parent) {
  setupUI();
}

void PasswordEntropyMeter::setupUI() {
  auto *rootLayout = new QVBoxLayout(this);
  rootLayout->setContentsMargins(0, 4, 0, 4);
  rootLayout->setSpacing(6);

  // Hàng chứa 4 thanh segment
  auto *barsContainer = new QWidget(this);
  auto *barsLayout = new QHBoxLayout(barsContainer);
  barsLayout->setContentsMargins(0, 0, 0, 0);
  barsLayout->setSpacing(4);

  for (int i = 0; i < 4; ++i) {
    m_segments[i] = new QWidget(barsContainer);
    m_segments[i]->setFixedHeight(4);
    m_segments[i]->setStyleSheet(
        QStringLiteral("background-color: #313244; border-radius: 2px;"));
    barsLayout->addWidget(m_segments[i]);
  }

  rootLayout->addWidget(barsContainer);

  m_label = new QLabel(this);
  m_label->setStyleSheet(QStringLiteral("font-size: 11px; color: #a6adc8;"));
  rootLayout->addWidget(m_label);

  updateDisplay();
}

void PasswordEntropyMeter::setPassword(const QString &password) {
  m_level = calculateStrength(password);
  updateDisplay();
}

PasswordEntropyMeter::StrengthLevel
PasswordEntropyMeter::calculateStrength(const QString &pwd) const {
  if (pwd.isEmpty()) {
    return StrengthLevel::Empty;
  }
  if (pwd.length() < 8) {
    return StrengthLevel::Weak;
  }

  bool hasLower = false;
  bool hasUpper = false;
  bool hasDigit = false;
  bool hasSymbol = false;

  for (const QChar &ch : pwd) {
    if (ch.isLower())
      hasLower = true;
    else if (ch.isUpper())
      hasUpper = true;
    else if (ch.isDigit())
      hasDigit = true;
    else
      hasSymbol = true;
  }

  int variety = (hasLower ? 1 : 0) + (hasUpper ? 1 : 0) + (hasDigit ? 1 : 0) +
                (hasSymbol ? 1 : 0);

  if (pwd.length() >= 16 && variety >= 3) {
    return StrengthLevel::Excellent;
  }
  if (pwd.length() >= 12 && variety >= 3) {
    return StrengthLevel::Strong;
  }
  if (pwd.length() >= 8 && variety >= 2) {
    return StrengthLevel::Fair;
  }
  return StrengthLevel::Weak;
}

void PasswordEntropyMeter::updateDisplay() {
  QString activeColor = QStringLiteral("#313244");
  int activeCount = 0;
  QString text;

  switch (m_level) {
  case StrengthLevel::Empty:
    activeCount = 0;
    text = tr("Enter a master password");
    break;
  case StrengthLevel::Weak:
    activeCount = 1;
    activeColor = QStringLiteral("#EF4444"); // Đỏ
    text = tr("Weak - Use at least 8 characters with numbers & symbols");
    break;
  case StrengthLevel::Fair:
    activeCount = 2;
    activeColor = QStringLiteral("#F59E0B"); // Vàng cam
    text = tr("Fair - Good, but can be stronger with more length");
    break;
  case StrengthLevel::Strong:
    activeCount = 3;
    activeColor = QStringLiteral("#3B82F6"); // Xanh lam
    text = tr("Strong - Safe and reliable master password");
    break;
  case StrengthLevel::Excellent:
    activeCount = 4;
    activeColor = QStringLiteral("#10B981"); // Xanh ngọc
    text = tr("Excellent - Outstanding entropy and resilience");
    break;
  }

  for (int i = 0; i < 4; ++i) {
    if (i < activeCount) {
      m_segments[i]->setStyleSheet(
          QStringLiteral("background-color: %1; border-radius: 2px;")
              .arg(activeColor));
    } else {
      m_segments[i]->setStyleSheet(
          QStringLiteral("background-color: #313244; border-radius: 2px;"));
    }
  }

  if (m_label) {
    m_label->setText(text);
    if (activeCount > 0) {
      m_label->setStyleSheet(
          QStringLiteral("font-size: 11px; font-weight: 500; color: %1;")
              .arg(activeColor));
    } else {
      m_label->setStyleSheet(
          QStringLiteral("font-size: 11px; color: #a6adc8;"));
    }
  }
}

void PasswordEntropyMeter::retranslateUi() { updateDisplay(); }

void PasswordEntropyMeter::changeEvent(QEvent *event) {
  if (event->type() == QEvent::LanguageChange) {
    retranslateUi();
  }
  QWidget::changeEvent(event);
}

} // namespace svm::views
