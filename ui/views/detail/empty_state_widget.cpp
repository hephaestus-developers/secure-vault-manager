/**
 * @file ui/views/detail/empty_state_widget.cpp
 * @brief Hiện thực giao diện EmptyStateWidget thẩm mỹ cao.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "empty_state_widget.h"

#include <QEvent>
#include <QIcon>
#include <QLabel>
#include <QVBoxLayout>

namespace svm::views {

EmptyStateWidget::EmptyStateWidget(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("EmptyStateWidget"));
  setAttribute(Qt::WA_StyledBackground, true);
  setupUI();
}

void EmptyStateWidget::setupUI() {
  auto *layout = new QVBoxLayout(this);
  layout->setAlignment(Qt::AlignCenter);
  layout->setSpacing(16);

  // Icon biểu trưng
  auto *iconLabel = new QLabel(this);
  iconLabel->setPixmap(
      QIcon(QStringLiteral(":/icons/shield_check.svg")).pixmap(64, 64));
  iconLabel->setAlignment(Qt::AlignCenter);
  layout->addWidget(iconLabel);

  // Tiêu đề
  m_titleLabel = new QLabel(tr("No Item Selected"), this);
  m_titleLabel->setObjectName(QStringLiteral("EmptyStateTitle"));
  m_titleLabel->setAlignment(Qt::AlignCenter);
  layout->addWidget(m_titleLabel);

  // Hướng dẫn phím tắt
  m_descLabel =
      new QLabel(tr("Select an item from the list to view its details,\n"
                    "or use keyboard shortcuts:\n\n"
                    "• Ctrl+N : Create a new item\n"
                    "• Ctrl+F : Search vault items\n"
                    "• Ctrl+K : Open Command Palette\n"
                    "• Ctrl+B : Toggle navigation sidebar"),
                 this);
  m_descLabel->setObjectName(QStringLiteral("EmptyStateDesc"));
  m_descLabel->setAlignment(Qt::AlignCenter);
  layout->addWidget(m_descLabel);
}

void EmptyStateWidget::retranslateUi() {
  if (m_titleLabel) {
    m_titleLabel->setText(tr("No Item Selected"));
  }
  if (m_descLabel) {
    m_descLabel->setText(
        tr("Select an item from the list to view its details,\n"
           "or use keyboard shortcuts:\n\n"
           "• Ctrl+N : Create a new item\n"
           "• Ctrl+F : Search vault items\n"
           "• Ctrl+K : Open Command Palette\n"
           "• Ctrl+B : Toggle navigation sidebar"));
  }
}

void EmptyStateWidget::changeEvent(QEvent *event) {
  if (event->type() == QEvent::LanguageChange) {
    retranslateUi();
  }
  QWidget::changeEvent(event);
}

} // namespace svm::views
