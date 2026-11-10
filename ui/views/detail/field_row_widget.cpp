/**
 * @file ui/views/detail/field_row_widget.cpp
 * @brief Hiện thực hàng trường dữ liệu đa năng.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "field_row_widget.h"
#include "app/clipboard_manager.h"
#include "app/theme_manager.h"
#include "core/core_bridge.h"

#include <QDesktopServices>
#include <QHBoxLayout>
#include <QIcon>
#include <QRandomGenerator>
#include <QUrl>
#include <QVBoxLayout>

namespace svm::views {

FieldRowWidget::FieldRowWidget(const QString &label, FieldType type,
                               QWidget *parent)
    : QWidget(parent), m_label(label), m_type(type) {
  setObjectName(QStringLiteral("FieldRowWidget"));
  setAttribute(Qt::WA_StyledBackground, true);
  setupUI();

  m_autoHideTimer.setSingleShot(true);
  m_autoHideTimer.setInterval(10000); // 10 giây tự động ẩn lại mật khẩu
  connect(&m_autoHideTimer, &QTimer::timeout, this, [this]() {
    m_isRevealed = false;
    updateDisplay();
  });
}

void FieldRowWidget::setupUI() {
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(12, 8, 12, 8);
  layout->setSpacing(4);

  // Label trường
  m_labelWidget = new QLabel(m_label.toUpper(), this);
  m_labelWidget->setObjectName(QStringLiteral("FieldRowLabel"));
  layout->addWidget(m_labelWidget);

  // Container giá trị + các nút thao tác
  auto *valContainer = new QWidget(this);
  auto *valLayout = new QHBoxLayout(valContainer);
  valLayout->setContentsMargins(0, 0, 0, 0);
  valLayout->setSpacing(6);

  // 1. Chế độ xem: QLabel
  m_valueLabel = new QLabel(this);
  m_valueLabel->setObjectName(QStringLiteral("FieldRowValue"));
  m_valueLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
  if (m_type == FieldType::Concealed) {
    QFont monoFont(QStringLiteral("monospace"), 11);
    m_valueLabel->setFont(monoFont);
  } else if (m_type == FieldType::Multiline) {
    m_valueLabel->setWordWrap(true);
  }
  valLayout->addWidget(m_valueLabel, 1);

  // 2. Chế độ sửa: QLineEdit hoặc QPlainTextEdit
  if (m_type == FieldType::Multiline) {
    m_plainTextEdit = new QPlainTextEdit(this);
    m_plainTextEdit->setObjectName(QStringLiteral("FieldRowInput"));
    m_plainTextEdit->setPlaceholderText(tr("Add notes or remarks..."));
    m_plainTextEdit->setMinimumHeight(70);
    m_plainTextEdit->setMaximumHeight(140);
    m_plainTextEdit->hide();
    valLayout->addWidget(m_plainTextEdit, 1);
  } else {
    m_lineEdit = new QLineEdit(this);
    m_lineEdit->setObjectName(QStringLiteral("FieldRowInput"));
    if (m_type == FieldType::Concealed) {
      m_lineEdit->setEchoMode(QLineEdit::Password);
    }
    m_lineEdit->hide();
    valLayout->addWidget(m_lineEdit, 1);
  }

  // 3. Nút tạo mật khẩu nhanh & ẩn/hiện mật khẩu (nếu là Concealed)
  if (m_type == FieldType::Concealed) {
    m_generateBtn = new QPushButton(tr("Generate"), this);
    m_generateBtn->setObjectName(QStringLiteral("FieldRowGenerateBtn"));
    m_generateBtn->setFixedHeight(28);
    m_generateBtn->setCursor(Qt::PointingHandCursor);
    m_generateBtn->setToolTip(tr("Generate strong random password"));
    m_generateBtn->hide();
    connect(m_generateBtn, &QPushButton::clicked, this, [this]() {
      QString generated = core::CoreBridge::instance().generatePassword(
          20, true, true, true, true);
      if (generated.isEmpty()) {
        static const char kCharset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLM"
                                       "NOPQRSTUVWXYZ0123456789!@#$%^&*()-_=+";
        const int charsetLen = static_cast<int>(sizeof(kCharset) - 1);
        for (int i = 0; i < 20; ++i) {
          generated.append(QLatin1Char(
              kCharset[QRandomGenerator::system()->bounded(charsetLen)]));
        }
      }
      m_value = generated;
      if (m_lineEdit) {
        m_lineEdit->setText(generated);
      }
      m_isRevealed = true;
      updateThemeIcons();
      updateDisplay();
    });
    valLayout->addWidget(m_generateBtn);

    m_revealBtn = new QPushButton(this);
    m_revealBtn->setObjectName(QStringLiteral("FieldRowActionBtn"));
    m_revealBtn->setIcon(QIcon(QStringLiteral(":/icons/actions/eye.svg")));
    m_revealBtn->setIconSize(QSize(16, 16));
    m_revealBtn->setFixedSize(28, 28);
    m_revealBtn->setCursor(Qt::PointingHandCursor);
    m_revealBtn->setToolTip(tr("Reveal password (10s)"));
    connect(m_revealBtn, &QPushButton::clicked, this,
            &FieldRowWidget::onToggleReveal);
    valLayout->addWidget(m_revealBtn);
  }

  // 4. Nút mở liên kết (nếu là Url)
  if (m_type == FieldType::Url) {
    m_openUrlBtn = new QPushButton(this);
    m_openUrlBtn->setObjectName(QStringLiteral("FieldRowActionBtn"));
    m_openUrlBtn->setIcon(
        QIcon(QStringLiteral(":/icons/actions/external_link.svg")));
    m_openUrlBtn->setIconSize(QSize(16, 16));
    m_openUrlBtn->setFixedSize(28, 28);
    m_openUrlBtn->setCursor(Qt::PointingHandCursor);
    m_openUrlBtn->setToolTip(tr("Open website"));
    connect(m_openUrlBtn, &QPushButton::clicked, this, [this]() {
      if (!m_value.isEmpty()) {
        QUrl url(m_value);
        if (url.scheme().isEmpty()) {
          url.setScheme(QStringLiteral("https"));
        }
        QDesktopServices::openUrl(url);
      }
    });
    valLayout->addWidget(m_openUrlBtn);
  }

  // 5. Nút Copy
  m_copyBtn = new QPushButton(this);
  m_copyBtn->setObjectName(QStringLiteral("FieldRowActionBtn"));
  m_copyBtn->setIcon(QIcon(QStringLiteral(":/icons/actions/copy.svg")));
  m_copyBtn->setIconSize(QSize(16, 16));
  m_copyBtn->setFixedSize(28, 28);
  m_copyBtn->setCursor(Qt::PointingHandCursor);
  m_copyBtn->setToolTip(tr("Copy to clipboard"));
  connect(m_copyBtn, &QPushButton::clicked, this,
          &FieldRowWidget::onCopyClicked);
  valLayout->addWidget(m_copyBtn);

  layout->addWidget(valContainer);

  connect(&app::ThemeManager::instance(), &app::ThemeManager::themeChanged,
          this, [this]() { updateThemeIcons(); });

  updateThemeIcons();
}

void FieldRowWidget::updateThemeIcons() {
  auto &tm = app::ThemeManager::instance();
  if (m_revealBtn) {
    m_revealBtn->setIcon(tm.icon(m_isRevealed ? QStringLiteral("eye_off")
                                              : QStringLiteral("eye")));
  }
  if (m_openUrlBtn) {
    m_openUrlBtn->setIcon(tm.icon(QStringLiteral("external_link")));
  }
  if (m_copyBtn) {
    m_copyBtn->setIcon(tm.icon(QStringLiteral("copy")));
  }
}

void FieldRowWidget::setValue(const QString &value) {
  m_value = value;
  if (m_plainTextEdit) {
    m_plainTextEdit->setPlainText(value);
  } else if (m_lineEdit) {
    m_lineEdit->setText(value);
  }
  m_isRevealed = false;
  updateDisplay();
}

QString FieldRowWidget::value() const {
  if (m_isEditMode) {
    if (m_plainTextEdit) {
      return m_plainTextEdit->toPlainText();
    }
    if (m_lineEdit) {
      return m_lineEdit->text();
    }
  }
  return m_value;
}

void FieldRowWidget::setLabel(const QString &label) {
  m_label = label;
  if (m_labelWidget) {
    m_labelWidget->setText(m_label.toUpper());
  }
}

void FieldRowWidget::setPlaceholderText(const QString &placeholder) {
  if (m_lineEdit) {
    m_lineEdit->setPlaceholderText(placeholder);
  } else if (m_plainTextEdit) {
    m_plainTextEdit->setPlaceholderText(placeholder);
  }
}

void FieldRowWidget::setEditMode(bool editable) {
  m_isEditMode = editable;

  if (editable) {
    m_valueLabel->hide();
    if (m_plainTextEdit) {
      m_plainTextEdit->show();
    } else if (m_lineEdit) {
      m_lineEdit->show();
    }
    if (m_generateBtn) {
      m_generateBtn->show();
    }
    m_copyBtn->hide();
    if (m_openUrlBtn) {
      m_openUrlBtn->hide();
    }
  } else {
    if (m_plainTextEdit) {
      m_value = m_plainTextEdit->toPlainText();
      m_plainTextEdit->hide();
    } else if (m_lineEdit) {
      m_value = m_lineEdit->text();
      m_lineEdit->hide();
    }
    if (m_generateBtn) {
      m_generateBtn->hide();
    }
    m_valueLabel->show();
    m_copyBtn->show();
    if (m_openUrlBtn) {
      m_openUrlBtn->show();
    }
  }

  updateDisplay();
}

void FieldRowWidget::onToggleReveal() {
  m_isRevealed = !m_isRevealed;
  if (m_lineEdit && m_type == FieldType::Concealed) {
    m_lineEdit->setEchoMode(m_isRevealed ? QLineEdit::Normal
                                         : QLineEdit::Password);
  }
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

void FieldRowWidget::onCopyClicked() {
  if (m_value.isEmpty()) {
    return;
  }

  if (m_type == FieldType::Concealed) {
    app::ClipboardManager::instance().copySensitive(m_value, m_label);
  } else {
    app::ClipboardManager::instance().copyPlainText(m_value);
  }
  emit copyRequested(m_value, m_label);
}

void FieldRowWidget::updateDisplay() {
  if (m_value.isEmpty()) {
    m_valueLabel->setText(tr("(empty)"));
    m_valueLabel->setStyleSheet(
        QStringLiteral("color: #64748B; font-style: italic;"));
    return;
  }

  m_valueLabel->setStyleSheet(QString());

  if (m_type == FieldType::Concealed) {
    if (m_lineEdit) {
      m_lineEdit->setEchoMode(m_isRevealed ? QLineEdit::Normal
                                           : QLineEdit::Password);
    }
    if (m_isRevealed) {
      m_valueLabel->setText(m_value);
    } else {
      m_valueLabel->setText(QStringLiteral("••••••••••••"));
    }
  } else {
    m_valueLabel->setText(m_value);
  }
}

void FieldRowWidget::retranslateUi() {
  if (m_generateBtn) {
    m_generateBtn->setText(tr("Generate"));
    m_generateBtn->setToolTip(tr("Generate strong random password"));
  }
  if (m_revealBtn) {
    m_revealBtn->setToolTip(tr("Reveal password (10s)"));
  }
  if (m_openUrlBtn) {
    m_openUrlBtn->setToolTip(tr("Open website"));
  }
  if (m_copyBtn) {
    m_copyBtn->setToolTip(tr("Copy to clipboard"));
  }
  updateDisplay();
}

void FieldRowWidget::changeEvent(QEvent *event) {
  if (event->type() == QEvent::LanguageChange) {
    retranslateUi();
  }
  QWidget::changeEvent(event);
}

} // namespace svm::views
