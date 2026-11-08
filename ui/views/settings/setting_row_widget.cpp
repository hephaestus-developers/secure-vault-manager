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
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

  m_layout = new QBoxLayout(QBoxLayout::LeftToRight, this);
  m_layout->setContentsMargins(0, 6, 0, 6);
  m_layout->setSpacing(16);

  // Cột văn bản (Title + Description)
  m_textLayout = new QVBoxLayout();
  m_textLayout->setSpacing(3);

  m_titleLabel = new QLabel(title, this);
  m_titleLabel->setObjectName(QStringLiteral("SettingRowTitle"));
  m_titleLabel->setWordWrap(true);
  m_titleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  m_titleLabel->setMinimumWidth(0);
  m_textLayout->addWidget(m_titleLabel);

  if (!description.isEmpty()) {
    m_descLabel = new QLabel(description, this);
    m_descLabel->setObjectName(QStringLiteral("SettingRowDesc"));
    m_descLabel->setWordWrap(true);
    m_descLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_descLabel->setMinimumWidth(0);
    m_textLayout->addWidget(m_descLabel);
  }

  m_layout->addLayout(m_textLayout, 1);

  if (m_controlWidget) {
    m_controlWidget->setParent(this);
    m_controlWidget->show();
    m_layout->addWidget(m_controlWidget, 0, Qt::AlignRight | Qt::AlignVCenter);
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
  adjustLayoutForWidth(width());
}

void SettingRowWidget::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
  adjustLayoutForWidth(event->size().width());
}

void SettingRowWidget::adjustLayoutForWidth(int width) {
  if (!m_controlWidget || !m_layout) {
    return;
  }

  const int ctrlWidth = m_controlWidget->sizeHint().width();
  // Nếu bề ngang không đủ cho phần text tối thiểu (~220px) thì tự động chuyển
  // sang layout xếp chồng
  const bool shouldStack = (width > 0 && (width - ctrlWidth) < 220);

  if (shouldStack != m_isStacked) {
    m_isStacked = shouldStack;
    if (m_isStacked) {
      m_layout->setDirection(QBoxLayout::TopToBottom);
      m_layout->setSpacing(8);
      m_layout->setAlignment(m_controlWidget, Qt::AlignRight | Qt::AlignTop);
    } else {
      m_layout->setDirection(QBoxLayout::LeftToRight);
      m_layout->setSpacing(16);
      m_layout->setAlignment(m_controlWidget,
                             Qt::AlignRight | Qt::AlignVCenter);
    }
  }
}

// ==============================================================================
// SettingSectionCard
// ==============================================================================

SettingSectionCard::SettingSectionCard(const QString &title, QWidget *parent)
    : QWidget(parent) {
  setObjectName(QStringLiteral("SettingSectionCard"));
  setAttribute(Qt::WA_StyledBackground, true);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

  auto *rootLayout = new QVBoxLayout(this);
  rootLayout->setContentsMargins(14, 12, 14, 12);
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
