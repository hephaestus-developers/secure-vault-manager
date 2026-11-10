/**
 * @file ui/views/detail/custom_field_row_widget.cpp
 * @brief Hiện thực widget hàng trường dữ liệu tùy biến.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "custom_field_row_widget.h"
#include "app/clipboard_manager.h"
#include "app/theme_manager.h"
#include "components/combobox_popup_helper.h"
#include "components/in_window_combobox.h"

#include <QCalendarWidget>
#include <QHBoxLayout>
#include <QIcon>
#include <QListView>
#include <QTextCharFormat>
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
  m_revealBtn->setIcon(QIcon(QStringLiteral(":/icons/actions/eye.svg")));
  m_revealBtn->setIconSize(QSize(16, 16));
  m_revealBtn->setFixedSize(28, 28);
  m_revealBtn->setCursor(Qt::PointingHandCursor);
  m_revealBtn->setToolTip(tr("Reveal (10s)"));
  connect(m_revealBtn, &QPushButton::clicked, this,
          &CustomFieldRowWidget::onToggleReveal);
  valRowLayout->addWidget(m_revealBtn);

  m_copyBtn = new QPushButton(this);
  m_copyBtn->setObjectName(QStringLiteral("FieldRowActionBtn"));
  m_copyBtn->setIcon(QIcon(QStringLiteral(":/icons/actions/copy.svg")));
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

  auto *kindCombo = new svm::ui::InWindowComboBox(this);
  kindCombo->setObjectName(QStringLiteral("CustomFieldKindCombo"));
  kindCombo->setFixedHeight(30);
  kindCombo->setPopupMinWidth(130);
  m_kindCombo = kindCombo;
  m_kindCombo->addItem(tr("Text"),
                       static_cast<int>(models::CustomFieldKind::Text));
  m_kindCombo->addItem(tr("Concealed"),
                       static_cast<int>(models::CustomFieldKind::Concealed));
  m_kindCombo->addItem(tr("Boolean"),
                       static_cast<int>(models::CustomFieldKind::Boolean));
  m_kindCombo->addItem(tr("Date"),
                       static_cast<int>(models::CustomFieldKind::Date));
  connect(m_kindCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &CustomFieldRowWidget::onKindChanged);
  headerRowLayout->addWidget(m_kindCombo, 0);

  m_deleteBtn = new QPushButton(this);
  m_deleteBtn->setObjectName(QStringLiteral("CustomFieldDeleteBtn"));
  m_deleteBtn->setIcon(
      app::ThemeManager::instance().icon(QStringLiteral("trash")));
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

  m_boolEdit = new QCheckBox(tr("No / False"), this);
  m_boolEdit->setObjectName(QStringLiteral("CustomFieldBoolEdit"));
  m_boolEdit->setCursor(Qt::PointingHandCursor);
  m_boolEdit->hide();
  connect(m_boolEdit, &QCheckBox::toggled, this, [this](bool checked) {
    m_boolEdit->setText(checked ? tr("Yes / True") : tr("No / False"));
  });
  valLayout->addWidget(m_boolEdit, 1);

  m_dateEdit = new QDateEdit(this);
  m_dateEdit->setObjectName(QStringLiteral("CustomFieldDateEdit"));
  m_dateEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
  m_dateEdit->setCalendarPopup(true);
  m_dateEdit->setDate(QDate::currentDate());
  m_dateEdit->setFixedHeight(32);
  m_dateEdit->hide();

  if (auto *calendar = m_dateEdit->calendarWidget()) {
    calendar->setVerticalHeaderFormat(QCalendarWidget::NoVerticalHeader);
    calendar->setGridVisible(false);
    calendar->setMinimumWidth(260);
  }
  updateCalendarTheme();

  valLayout->addWidget(m_dateEdit, 1);

  m_editRevealBtn = new QPushButton(this);
  m_editRevealBtn->setObjectName(QStringLiteral("FieldRowActionBtn"));
  m_editRevealBtn->setIcon(QIcon(QStringLiteral(":/icons/actions/eye.svg")));
  m_editRevealBtn->setIconSize(QSize(16, 16));
  m_editRevealBtn->setFixedSize(30, 30);
  m_editRevealBtn->setCursor(Qt::PointingHandCursor);
  m_editRevealBtn->setToolTip(tr("Show / Hide password"));
  connect(m_editRevealBtn, &QPushButton::clicked, this, [this]() {
    if (m_valueEdit->echoMode() == QLineEdit::Password) {
      m_valueEdit->setEchoMode(QLineEdit::Normal);
      m_editRevealBtn->setIcon(
          app::ThemeManager::instance().icon(QStringLiteral("eye_off")));
    } else {
      m_valueEdit->setEchoMode(QLineEdit::Password);
      m_editRevealBtn->setIcon(
          app::ThemeManager::instance().icon(QStringLiteral("eye")));
    }
  });
  valLayout->addWidget(m_editRevealBtn);

  editLayout->addWidget(editValRow);
  rootLayout->addWidget(m_editContainer);

  connect(&app::ThemeManager::instance(), &app::ThemeManager::themeChanged,
          this, [this]() {
            updateThemeIcons();
            updateCalendarTheme();
          });

  updateThemeIcons();
  setEditMode(false);
}

void CustomFieldRowWidget::updateThemeIcons() {
  auto &tm = app::ThemeManager::instance();
  if (m_revealBtn) {
    m_revealBtn->setIcon(tm.icon(m_isRevealed ? QStringLiteral("eye_off")
                                              : QStringLiteral("eye")));
  }
  if (m_copyBtn) {
    m_copyBtn->setIcon(tm.icon(QStringLiteral("copy")));
  }
  if (m_deleteBtn) {
    m_deleteBtn->setIcon(tm.icon(QStringLiteral("trash")));
  }
  if (m_editRevealBtn) {
    const bool isPassword =
        (m_valueEdit && m_valueEdit->echoMode() == QLineEdit::Password);
    m_editRevealBtn->setIcon(tm.icon(isPassword ? QStringLiteral("eye")
                                                : QStringLiteral("eye_off")));
  }
}

void CustomFieldRowWidget::updateCalendarTheme() {
  if (!m_dateEdit) {
    return;
  }
  auto *calendar = m_dateEdit->calendarWidget();
  if (!calendar) {
    return;
  }

  const bool isLight =
      (app::ThemeManager::instance().currentMode() == app::ThemeMode::Light);

  QTextCharFormat defaultFmt;
  defaultFmt.setForeground(isLight ? QColor(0x2F, 0x35, 0x40)
                                   : QColor(0xCD, 0xD6, 0xF4));
  calendar->setWeekdayTextFormat(Qt::Monday, defaultFmt);
  calendar->setWeekdayTextFormat(Qt::Tuesday, defaultFmt);
  calendar->setWeekdayTextFormat(Qt::Wednesday, defaultFmt);
  calendar->setWeekdayTextFormat(Qt::Thursday, defaultFmt);
  calendar->setWeekdayTextFormat(Qt::Friday, defaultFmt);

  // Cuối tuần: dùng màu dịu mắt thay vì màu đỏ gắt mặc định của hệ thống
  QTextCharFormat weekendFmt;
  weekendFmt.setForeground(isLight ? QColor(0xD2, 0x0F, 0x39)
                                   : QColor(0xF3, 0x8B, 0xA8));
  calendar->setWeekdayTextFormat(Qt::Saturday, weekendFmt);
  calendar->setWeekdayTextFormat(Qt::Sunday, weekendFmt);

  QTextCharFormat headerFmt;
  headerFmt.setForeground(isLight ? QColor(0x5C, 0x5F, 0x77)
                                  : QColor(0xA6, 0xAD, 0xC8));
  headerFmt.setFontWeight(QFont::Bold);
  calendar->setHeaderTextFormat(headerFmt);
}

void CustomFieldRowWidget::setField(const models::UiCustomField &field) {
  m_field = field;
  m_isRevealed = false;

  m_labelEdit->setText(field.label);
  m_valueEdit->setText(field.value);

  if (field.kind == models::CustomFieldKind::Boolean) {
    const bool checked =
        (field.value.trimmed().compare(QStringLiteral("true"),
                                       Qt::CaseInsensitive) == 0 ||
         field.value.trimmed() == QStringLiteral("1"));
    m_boolEdit->setChecked(checked);
    m_boolEdit->setText(checked ? tr("Yes / True") : tr("No / False"));
  } else if (field.kind == models::CustomFieldKind::Date) {
    QDate d = QDate::fromString(field.value.trimmed(), Qt::ISODate);
    if (!d.isValid()) {
      d = QDate::currentDate();
    }
    m_dateEdit->setDate(d);
  }

  int idx = m_kindCombo->findData(static_cast<int>(field.kind));
  if (idx < 0) {
    idx = 0;
  }
  m_kindCombo->setCurrentIndex(idx);
  onKindChanged(idx);

  updateDisplay();
}

models::UiCustomField CustomFieldRowWidget::field() const {
  models::UiCustomField f = m_field;
  if (m_isEditMode) {
    f.label = m_labelEdit->text().trimmed();
    f.kind = static_cast<models::CustomFieldKind>(
        m_kindCombo->currentData().toInt());
    if (f.kind == models::CustomFieldKind::Boolean) {
      f.value = m_boolEdit->isChecked() ? QStringLiteral("true")
                                        : QStringLiteral("false");
    } else if (f.kind == models::CustomFieldKind::Date) {
      f.value = m_dateEdit->date().toString(Qt::ISODate);
    } else {
      f.value = m_valueEdit->text();
    }
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
    m_field.kind = static_cast<models::CustomFieldKind>(
        m_kindCombo->currentData().toInt());
    if (m_field.kind == models::CustomFieldKind::Boolean) {
      m_field.value = m_boolEdit->isChecked() ? QStringLiteral("true")
                                              : QStringLiteral("false");
    } else if (m_field.kind == models::CustomFieldKind::Date) {
      m_field.value = m_dateEdit->date().toString(Qt::ISODate);
    } else {
      m_field.value = m_valueEdit->text();
    }

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
  const bool isBoolean = (kind == models::CustomFieldKind::Boolean);
  const bool isDate = (kind == models::CustomFieldKind::Date);

  m_valueEdit->setVisible(!isBoolean && !isDate);
  m_boolEdit->setVisible(isBoolean);
  m_dateEdit->setVisible(isDate);

  m_valueEdit->setEchoMode(isConcealed ? QLineEdit::Password
                                       : QLineEdit::Normal);
  m_editRevealBtn->setVisible(isConcealed);
  m_editRevealBtn->setIcon(
      app::ThemeManager::instance().icon(QStringLiteral("eye")));
  m_revealBtn->setVisible(isConcealed);

  updateDisplay();
}

void CustomFieldRowWidget::onToggleReveal() {
  m_isRevealed = !m_isRevealed;
  if (m_isRevealed) {
    m_revealBtn->setIcon(
        app::ThemeManager::instance().icon(QStringLiteral("eye_off")));
    m_autoHideTimer.start();
  } else {
    m_revealBtn->setIcon(
        app::ThemeManager::instance().icon(QStringLiteral("eye")));
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
                                                    m_field.label);
  } else {
    app::ClipboardManager::instance().copyPlainText(m_field.value);
  }
  emit copyRequested(m_field.value, m_field.label);
}

void CustomFieldRowWidget::updateDisplay() {
  m_labelView->setText(m_field.label.isEmpty() ? tr("CUSTOM FIELD")
                                               : m_field.label.toUpper());

  m_revealBtn->setVisible(m_field.kind == models::CustomFieldKind::Concealed);

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
  } else if (m_field.kind == models::CustomFieldKind::Boolean) {
    const bool isTrue =
        (m_field.value.trimmed().compare(QStringLiteral("true"),
                                         Qt::CaseInsensitive) == 0 ||
         m_field.value.trimmed() == QStringLiteral("1"));
    m_valueView->setText(isTrue ? tr("Yes") : tr("No"));
  } else if (m_field.kind == models::CustomFieldKind::Date) {
    const QDate d = QDate::fromString(m_field.value.trimmed(), Qt::ISODate);
    m_valueView->setText(d.isValid() ? d.toString(QStringLiteral("yyyy-MM-dd"))
                                     : m_field.value);
  } else {
    m_valueView->setText(m_field.value);
  }
}

void CustomFieldRowWidget::retranslateUi() {
  if (m_revealBtn) {
    m_revealBtn->setToolTip(tr("Reveal (10s)"));
  }
  if (m_copyBtn) {
    m_copyBtn->setToolTip(tr("Copy to clipboard"));
  }
  if (m_labelEdit) {
    m_labelEdit->setPlaceholderText(tr("Field Name (e.g. PIN, Recovery Code)"));
  }
  if (m_kindCombo && m_kindCombo->count() >= 4) {
    m_kindCombo->setItemText(0, tr("Text"));
    m_kindCombo->setItemText(1, tr("Concealed"));
    m_kindCombo->setItemText(2, tr("Boolean"));
    m_kindCombo->setItemText(3, tr("Date"));
  }
  if (m_deleteBtn) {
    m_deleteBtn->setToolTip(tr("Delete this field"));
  }
  if (m_valueEdit) {
    m_valueEdit->setPlaceholderText(tr("Field Value"));
  }
  if (m_boolEdit) {
    m_boolEdit->setText(m_boolEdit->isChecked() ? tr("Yes / True")
                                                : tr("No / False"));
  }
  if (m_editRevealBtn) {
    m_editRevealBtn->setToolTip(tr("Show / Hide password"));
  }
  updateDisplay();
}

void CustomFieldRowWidget::changeEvent(QEvent *event) {
  if (event->type() == QEvent::LanguageChange) {
    retranslateUi();
  }
  QWidget::changeEvent(event);
}

} // namespace svm::views
