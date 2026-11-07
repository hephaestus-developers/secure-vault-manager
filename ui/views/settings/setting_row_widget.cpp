/**
 * @file ui/views/settings/setting_row_widget.cpp
 * @brief Hiện thực SettingRowWidget và SettingSectionCard.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "setting_row_widget.h"

namespace svm::views {

SettingRowWidget::SettingRowWidget(const QString &title,
                                   const QString &description,
                                   QWidget *controlWidget, QWidget *parent)
    : QWidget(parent), m_controlWidget(controlWidget) {
  setObjectName(QStringLiteral("SettingRowWidget"));
  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 6, 0, 6);
  layout->setSpacing(16);

  // Cột văn bản (Title + Description)
  auto *textLayout = new QVBoxLayout();
  textLayout->setSpacing(3);

  m_titleLabel = new QLabel(title, this);
  m_titleLabel->setObjectName(QStringLiteral("SettingRowTitle"));
  textLayout->addWidget(m_titleLabel);

  if (!description.isEmpty()) {
    m_descLabel = new QLabel(description, this);
    m_descLabel->setObjectName(QStringLiteral("SettingRowDesc"));
    m_descLabel->setWordWrap(true);
    textLayout->addWidget(m_descLabel);
  }

  layout->addLayout(textLayout, 1);

  if (m_controlWidget) {
    m_controlWidget->setParent(this);
    layout->addWidget(m_controlWidget, 0, Qt::AlignRight | Qt::AlignVCenter);
  }
}

void SettingRowWidget::updateText(const QString &title,
                                  const QString &description) {
  if (m_titleLabel) {
    m_titleLabel->setText(title);
  }
  if (m_descLabel) {
    m_descLabel->setText(description);
  }
}

// ==============================================================================
// SettingSectionCard
// ==============================================================================

SettingSectionCard::SettingSectionCard(const QString &title, QWidget *parent)
    : QWidget(parent) {
  setObjectName(QStringLiteral("SettingSectionCard"));
  setAttribute(Qt::WA_StyledBackground, true);

  auto *rootLayout = new QVBoxLayout(this);
  rootLayout->setContentsMargins(16, 14, 16, 14);
  rootLayout->setSpacing(10);

  if (!title.isEmpty()) {
    m_titleLabel = new QLabel(title.toUpper(), this);
    m_titleLabel->setObjectName(QStringLiteral("SettingSectionTitle"));
    rootLayout->addWidget(m_titleLabel);
  }

  m_cardLayout = new QVBoxLayout();
  m_cardLayout->setContentsMargins(0, 0, 0, 0);
  m_cardLayout->setSpacing(10);
  rootLayout->addLayout(m_cardLayout);
}

void SettingSectionCard::setTitle(const QString &title) {
  if (m_titleLabel) {
    m_titleLabel->setText(title.toUpper());
  }
}

void SettingSectionCard::addRow(SettingRowWidget *row) {
  if (m_cardLayout && row) {
    m_cardLayout->addWidget(row);
  }
}

void SettingSectionCard::addWidget(QWidget *widget) {
  if (m_cardLayout && widget) {
    m_cardLayout->addWidget(widget);
  }
}

} // namespace svm::views
