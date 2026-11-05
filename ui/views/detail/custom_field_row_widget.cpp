/**
 * @file ui/views/detail/custom_field_row_widget.cpp
 * @brief Hiện thực widget hàng trường dữ liệu tùy biến.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "custom_field_row_widget.h"
#include "app/clipboard_manager.h"

#include <QHBoxLayout>
#include <QIcon>
#include <QUuid>
#include <QVBoxLayout>

namespace svm::views {

CustomFieldRowWidget::CustomFieldRowWidget(const models::UiCustomField &field,
                                           QWidget *parent)
    : QWidget(parent), m_field(field) {
  setObjectName(QStringLiteral("CustomFieldRowWidget"));
  setAttribute(Qt::WA_StyledBackground, true);

  if (m_field.id.isEmpty()) {
    m_field.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
  }

  setupUI();

  m_autoHideTimer.setSingleShot(true);
  m_autoHideTimer.setInterval(10000);
  connect(&m_autoHideTimer, &QTimer::timeout, this, [this]() {
    m_isRevealed = false;
    updateDisplay();
  });

  setField(field);
}

void CustomFieldRowWidget::setupUI() {
  auto *rootLayout = new QVBoxLayout(this);
  rootLayout->setContentsMargins(0, 4, 0, 4);
  rootLayout->setSpacing(4);

  // 1. View Mode Container
  m_viewContainer = new QWidget(this);
  m_viewContainer->setObjectName(QStringLiteral("CustomFieldViewContainer"));
  auto *viewLayout = new QVBoxLayout(m_viewContainer);
  viewLayout->setContentsMargins(0, 2, 0, 2);
  viewLayout->setSpacing(4);

  m_labelView = new QLabel(this);
  m_labelView->setObjectName(QStringLiteral("FieldRowLabel"));
  viewLayout->addWidget(m_labelView);

  auto *valRow = new QWidget(m_viewContainer);
  auto *valRowLayout = new QHBoxLayout(valRow);
  valRowLayout->setContentsMargins(0, 0, 0, 0);
  valRowLayout->setSpacing(6);

  m_valueView = new QLabel(this);
  m_valueView->setObjectName(QStringLiteral("FieldRowValue"));
  m_valueView->setTextInteractionFlags(Qt::TextSelectableByMouse);
  valRowLayout->addWidget(m_valueView, 1);

  m_revealBtn = new QPushButton(this);
  m_revealBtn->setObjectName(QStringLiteral("FieldRowActionBtn"));
  m_revealBtn->setIcon(QIcon(QStringLiteral(":/icons/eye.svg")));
  m_revealBtn->setIconSize(QSize(16, 16));
  m_revealBtn->setFixedSize(28, 28);
  m_revealBtn->setCursor(Qt::PointingHandCursor);
  m_revealBtn->setToolTip(tr("Reveal (10s)"));
  connect(m_revealBtn, &QPushButton::clicked, this,
          &CustomFieldRowWidget::onToggleReveal);
  valRowLayout->addWidget(m_revealBtn);

  m_copyBtn = new QPushButton(this);
  m_copyBtn->setObjectName(QStringLiteral("FieldRowActionBtn"));
  m_copyBtn->setIcon(QIcon(QStringLiteral(":/icons/copy.svg")));
  m_copyBtn->setIconSize(QSize(16, 16));
  m_copyBtn->setFixedSize(28, 28);
  m_copyBtn->setCursor(Qt::PointingHandCursor);
  m_copyBtn->setToolTip(tr("Copy to clipboard"));
  connect(m_copyBtn, &QPushButton::clicked, this,
          &CustomFieldRowWidget::onCopyClicked);
  valRowLayout->addWidget(m_copyBtn);

  viewLayout->addWidget(valRow);
  rootLayout->addWidget(m_viewContainer);

  // 2. Edit Mode Container
  m_editContainer = new QWidget(this);
  m_editContainer->setObjectName(QStringLiteral("CustomFieldEditContainer"));
  auto *editLayout = new QVBoxLayout(m_editContainer);
  editLayout->setContentsMargins(0, 2, 0, 2);
  editLayout->setSpacing(6);

  // Row 1: Label Edit + Kind Combo + Delete Button
  auto *editHeaderRow = new QWidget(m_editContainer);
  auto *headerRowLayout = new QHBoxLayout(editHeaderRow);
  headerRowLayout->setContentsMargins(0, 0, 0, 0);
  headerRowLayout->setSpacing(8);

  m_labelEdit = new QLineEdit(this);
  m_labelEdit->setObjectName(QStringLiteral("FieldRowInput"));
  m_labelEdit->setPlaceholderText(tr("Field Name (e.g. PIN, Recovery Code)"));
  headerRowLayout->addWidget(m_labelEdit, 1);

  m_kindCombo = new QComboBox(this);
  m_kindCombo->setObjectName(QStringLiteral("CustomFieldKindCombo"));
  m_kindCombo->setFixedHeight(30);
  m_kindCombo->addItem(tr("Text"),
                       static_cast<int>(models::CustomFieldKind::Text));
  m_kindCombo->addItem(tr("Concealed"),
                       static_cast<int>(models::CustomFieldKind::Concealed));
  connect(m_kindCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &CustomFieldRowWidget::onKindChanged);
  headerRowLayout->addWidget(m_kindCombo, 0);

  m_deleteBtn = new QPushButton(this);
  m_deleteBtn->setObjectName(QStringLiteral("CustomFieldDeleteBtn"));
  m_deleteBtn->setIcon(QIcon(QStringLiteral(":/icons/trash.svg")));
  m_deleteBtn->setIconSize(QSize(15, 15));
  m_deleteBtn->setFixedSize(30, 30);
  m_deleteBtn->setCursor(Qt::PointingHandCursor);
  m_deleteBtn->setToolTip(tr("Delete this field"));
  connect(m_deleteBtn, &QPushButton::clicked, this,
          &CustomFieldRowWidget::deleteRequested);
  headerRowLayout->addWidget(m_deleteBtn, 0);

  editLayout->addWidget(editHeaderRow);

  // Row 2: Value Edit + Reveal Button
  auto *editValRow = new QWidget(m_editContainer);
  auto *valLayout = new QHBoxLayout(editValRow);
  valLayout->setContentsMargins(0, 0, 0, 0);
  valLayout->setSpacing(6);

  m_valueEdit = new QLineEdit(this);
  m_valueEdit->setObjectName(QStringLiteral("FieldRowInput"));
  m_valueEdit->setPlaceholderText(tr("Field Value"));
  valLayout->addWidget(m_valueEdit, 1);

  m_editRevealBtn = new QPushButton(this);
  m_editRevealBtn->setObjectName(QStringLiteral("FieldRowActionBtn"));
  m_editRevealBtn->setIcon(QIcon(QStringLiteral(":/icons/eye.svg")));
  m_editRevealBtn->setIconSize(QSize(16, 16));
  m_editRevealBtn->setFixedSize(30, 30);
  m_editRevealBtn->setCursor(Qt::PointingHandCursor);
  m_editRevealBtn->setToolTip(tr("Show / Hide password"));
  connect(m_editRevealBtn, &QPushButton::clicked, this, [this]() {
    if (m_valueEdit->echoMode() == QLineEdit::Password) {
      m_valueEdit->setEchoMode(QLineEdit::Normal);
      m_editRevealBtn->setIcon(QIcon(QStringLiteral(":/icons/eye_off.svg")));
    } else {
      m_valueEdit->setEchoMode(QLineEdit::Password);
      m_editRevealBtn->setIcon(QIcon(QStringLiteral(":/icons/eye.svg")));
    }
  });
  valLayout->addWidget(m_editRevealBtn);

  editLayout->addWidget(editValRow);
  rootLayout->addWidget(m_editContainer);

  setEditMode(false);
}

void CustomFieldRowWidget::setField(const models::UiCustomField &field) {
  m_field = field;
  m_isRevealed = false;

  m_labelEdit->setText(field.label);
  m_valueEdit->setText(field.value);

  const int idx = (field.kind == models::CustomFieldKind::Concealed) ? 1 : 0;
  m_kindCombo->setCurrentIndex(idx);
  onKindChanged(idx);

  updateDisplay();
}

models::UiCustomField CustomFieldRowWidget::field() const {
  models::UiCustomField f = m_field;
  if (m_isEditMode) {
    f.label = m_labelEdit->text().trimmed();
    f.value = m_valueEdit->text();
    f.kind = static_cast<models::CustomFieldKind>(
        m_kindCombo->currentData().toInt());
  }
  return f;
}

void CustomFieldRowWidget::setEditMode(bool editable) {
  m_isEditMode = editable;

  if (editable) {
    m_viewContainer->hide();
    m_editContainer->show();
  } else {
    // Lưu các thay đổi hiện thời
    m_field.label = m_labelEdit->text().trimmed();
    m_field.value = m_valueEdit->text();
    m_field.kind = static_cast<models::CustomFieldKind>(
        m_kindCombo->currentData().toInt());

    m_editContainer->hide();
    m_viewContainer->show();
    updateDisplay();
  }
}

void CustomFieldRowWidget::focusLabelInput() {
  m_labelEdit->setFocus();
  m_labelEdit->selectAll();
}

void CustomFieldRowWidget::onKindChanged(int index) {
  const auto kind = static_cast<models::CustomFieldKind>(
      m_kindCombo->itemData(index).toInt());
  const bool isConcealed = (kind == models::CustomFieldKind::Concealed);

  m_valueEdit->setEchoMode(isConcealed ? QLineEdit::Password
                                       : QLineEdit::Normal);
  m_editRevealBtn->setVisible(isConcealed);
  m_editRevealBtn->setIcon(QIcon(QStringLiteral(":/icons/eye.svg")));
  m_revealBtn->setVisible(isConcealed);

  updateDisplay();
}

void CustomFieldRowWidget::onToggleReveal() {
  m_isRevealed = !m_isRevealed;
  if (m_isRevealed) {
    m_revealBtn->setIcon(QIcon(QStringLiteral(":/icons/eye_off.svg")));
    m_autoHideTimer.start();
  } else {
    m_revealBtn->setIcon(QIcon(QStringLiteral(":/icons/eye.svg")));
    m_autoHideTimer.stop();
  }
  updateDisplay();
}

void CustomFieldRowWidget::onCopyClicked() {
  if (m_field.value.isEmpty()) {
    return;
  }
  if (m_field.kind == models::CustomFieldKind::Concealed) {
    app::ClipboardManager::instance().copySensitive(m_field.value,
                                                    m_field.label, 30);
  } else {
    app::ClipboardManager::instance().copyPlainText(m_field.value);
  }
  emit copyRequested(m_field.value, m_field.label);
}

void CustomFieldRowWidget::updateDisplay() {
  m_labelView->setText(m_field.label.isEmpty() ? tr("CUSTOM FIELD")
                                               : m_field.label.toUpper());

  if (m_field.value.isEmpty()) {
    m_valueView->setText(tr("(empty)"));
    m_valueView->setStyleSheet(
        QStringLiteral("color: #64748B; font-style: italic;"));
    return;
  }

  m_valueView->setStyleSheet(QString());

  if (m_field.kind == models::CustomFieldKind::Concealed) {
    if (m_isRevealed) {
      m_valueView->setText(m_field.value);
    } else {
      m_valueView->setText(QStringLiteral("••••••••••••"));
    }
  } else {
    m_valueView->setText(m_field.value);
  }
}

} // namespace svm::views
