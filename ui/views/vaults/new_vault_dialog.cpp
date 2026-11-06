/**
 * @file ui/views/vaults/new_vault_dialog.cpp
 * @brief Hiện thực hộp thoại tạo kho lưu trữ mới NewVaultDialog trực quan cao.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "new_vault_dialog.h"
#include "app/theme_manager.h"
#include "app/vault_manager.h"

#include <QEvent>
#include <QGraphicsDropShadowEffect>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QVBoxLayout>

namespace svm::views {

NewVaultDialog::NewVaultDialog(QWidget *parent)
    : QDialog(parent, Qt::FramelessWindowHint | Qt::Dialog) {
  setObjectName(QStringLiteral("NewVaultDialog"));
  setAttribute(Qt::WA_TranslucentBackground, true);
  setModal(true);
  setFixedSize(480, 480);

  setupUI();
}

void NewVaultDialog::setupUI() {
  auto *rootLayout = new QVBoxLayout(this);
  rootLayout->setContentsMargins(16, 16, 16, 16);

  m_cardWidget = new QWidget(this);
  m_cardWidget->setObjectName(QStringLiteral("NewVaultCard"));

  auto *shadow = new QGraphicsDropShadowEffect(this);
  shadow->setBlurRadius(28);
  shadow->setColor(QColor(0, 0, 0, 160));
  shadow->setOffset(0, 8);
  m_cardWidget->setGraphicsEffect(shadow);

  auto *layout = new QVBoxLayout(m_cardWidget);
  layout->setContentsMargins(24, 20, 24, 20);
  layout->setSpacing(12);

  m_titleLabel = new QLabel(tr("Create New Vault"), m_cardWidget);
  QFont f = m_titleLabel->font();
  f.setPointSize(14);
  f.setBold(true);
  m_titleLabel->setFont(f);
  layout->addWidget(m_titleLabel);

  m_descLabel = new QLabel(tr("Create an isolated vault to organize passwords, "
                              "tokens, or shared secrets."),
                           m_cardWidget);
  m_descLabel->setWordWrap(true);
  m_descLabel->setStyleSheet(
      QStringLiteral("color: #a6adc8; font-size: 11.5px;"));
  layout->addWidget(m_descLabel);

  // Vault Name
  auto *nameLabel = new QLabel(tr("Vault Name"), m_cardWidget);
  nameLabel->setStyleSheet(
      QStringLiteral("font-size: 11px; font-weight: 600; color: #bac2de;"));
  layout->addWidget(nameLabel);

  m_nameEdit = new QLineEdit(m_cardWidget);
  m_nameEdit->setPlaceholderText(tr("e.g. Work Credentials, Finance & Crypto"));
  m_nameEdit->setFixedHeight(36);
  layout->addWidget(m_nameEdit);

  // Icon Picker
  auto *iconTitle = new QLabel(tr("Vault Icon"), m_cardWidget);
  iconTitle->setStyleSheet(
      QStringLiteral("font-size: 11px; font-weight: 600; color: #bac2de;"));
  layout->addWidget(iconTitle);
  layout->addWidget(createIconPicker());

  // Accent Color Picker
  auto *colorTitle = new QLabel(tr("Accent Color"), m_cardWidget);
  colorTitle->setStyleSheet(
      QStringLiteral("font-size: 11px; font-weight: 600; color: #bac2de;"));
  layout->addWidget(colorTitle);
  layout->addWidget(createColorPicker());

  // Description
  auto *descTitle = new QLabel(tr("Description (Optional)"), m_cardWidget);
  descTitle->setStyleSheet(
      QStringLiteral("font-size: 11px; font-weight: 600; color: #bac2de;"));
  layout->addWidget(descTitle);

  m_descEdit = new QLineEdit(m_cardWidget);
  m_descEdit->setPlaceholderText(tr("Short purpose or notes for this vault"));
  m_descEdit->setFixedHeight(34);
  layout->addWidget(m_descEdit);

  m_errorLabel = new QLabel(m_cardWidget);
  m_errorLabel->setStyleSheet(
      QStringLiteral("color: #ef4444; font-size: 11px; font-weight: 500;"));
  layout->addWidget(m_errorLabel);

  layout->addStretch(1);

  // Action buttons
  auto *btnLayout = new QHBoxLayout();
  m_cancelBtn = new QPushButton(tr("Cancel"), m_cardWidget);
  m_cancelBtn->setFixedHeight(34);
  m_cancelBtn->setCursor(Qt::PointingHandCursor);
  connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
  btnLayout->addWidget(m_cancelBtn);

  btnLayout->addStretch(1);

  m_createBtn = new QPushButton(tr("Create Vault"), m_cardWidget);
  m_createBtn->setObjectName(QStringLiteral("OnboardingPrimaryBtn"));
  m_createBtn->setFixedHeight(34);
  m_createBtn->setCursor(Qt::PointingHandCursor);
  connect(m_createBtn, &QPushButton::clicked, this,
          &NewVaultDialog::onCreateClicked);
  btnLayout->addWidget(m_createBtn);

  layout->addLayout(btnLayout);
  rootLayout->addWidget(m_cardWidget);
}

QWidget *NewVaultDialog::createIconPicker() {
  auto *container = new QWidget(m_cardWidget);
  auto *layout = new QHBoxLayout(container);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(6);

  const QStringList icons = {
      QStringLiteral("folder_open"),   QStringLiteral("folder_empty"),
      QStringLiteral("shield_check"),  QStringLiteral("lock"),
      QStringLiteral("star"),          QStringLiteral("category_server"),
      QStringLiteral("category_card"), QStringLiteral("category_database")};

  for (const QString &iconName : icons) {
    auto *btn = new QPushButton(container);
    btn->setFixedSize(32, 32);
    btn->setIcon(app::ThemeManager::instance().icon(iconName));
    btn->setIconSize(QSize(16, 16));
    btn->setCursor(Qt::PointingHandCursor);
    btn->setProperty("iconName", iconName);

    if (iconName == m_selectedIcon) {
      btn->setStyleSheet(
          QStringLiteral("background-color: #313244; border: 2px solid "
                         "#89b4fa; border-radius: 6px;"));
    } else {
      btn->setStyleSheet(
          QStringLiteral("background-color: #1e1e2e; border: 1px solid "
                         "#313244; border-radius: 6px;"));
    }

    connect(btn, &QPushButton::clicked, this, [this, btn, iconName]() {
      m_selectedIcon = iconName;
      for (auto *b : m_iconButtons) {
        if (b == btn) {
          b->setStyleSheet(
              QStringLiteral("background-color: #313244; border: 2px solid "
                             "#89b4fa; border-radius: 6px;"));
        } else {
          b->setStyleSheet(
              QStringLiteral("background-color: #1e1e2e; border: 1px solid "
                             "#313244; border-radius: 6px;"));
        }
      }
    });

    m_iconButtons.append(btn);
    layout->addWidget(btn);
  }

  layout->addStretch(1);
  return container;
}

QWidget *NewVaultDialog::createColorPicker() {
  auto *container = new QWidget(m_cardWidget);
  auto *layout = new QHBoxLayout(container);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(8);

  const QStringList colors = {
      QStringLiteral("#3B82F6"), // Blue
      QStringLiteral("#8B5CF6"), // Purple
      QStringLiteral("#10B981"), // Emerald
      QStringLiteral("#F59E0B"), // Amber
      QStringLiteral("#F43F5E"), // Rose
      QStringLiteral("#6366F1"), // Indigo
      QStringLiteral("#06B6D4"), // Cyan
      QStringLiteral("#64748B")  // Slate
  };

  for (const QString &colorHex : colors) {
    auto *btn = new QPushButton(container);
    btn->setFixedSize(26, 26);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setProperty("colorHex", colorHex);

    if (colorHex == m_selectedColor) {
      btn->setStyleSheet(QStringLiteral("background-color: %1; border: 2px "
                                        "solid #ffffff; border-radius: 13px;")
                             .arg(colorHex));
    } else {
      btn->setStyleSheet(QStringLiteral("background-color: %1; border: 1px "
                                        "solid #313244; border-radius: 13px;")
                             .arg(colorHex));
    }

    connect(btn, &QPushButton::clicked, this, [this, btn, colorHex]() {
      m_selectedColor = colorHex;
      for (auto *b : m_colorButtons) {
        const QString bColor = b->property("colorHex").toString();
        if (b == btn) {
          b->setStyleSheet(QStringLiteral("background-color: %1; border: 2px "
                                          "solid #ffffff; border-radius: 13px;")
                               .arg(bColor));
        } else {
          b->setStyleSheet(QStringLiteral("background-color: %1; border: 1px "
                                          "solid #313244; border-radius: 13px;")
                               .arg(bColor));
        }
      }
    });

    m_colorButtons.append(btn);
    layout->addWidget(btn);
  }

  layout->addStretch(1);
  return container;
}

void NewVaultDialog::onCreateClicked() {
  const QString name = m_nameEdit->text().trimmed();
  if (name.isEmpty()) {
    m_errorLabel->setText(tr("Vault name cannot be empty."));
    return;
  }

  models::UiVault v;
  v.id = QStringLiteral("vault_%1").arg(QDateTime::currentMSecsSinceEpoch());
  v.name = name;
  v.iconName = m_selectedIcon;
  v.accentColor = m_selectedColor;
  v.description = m_descEdit->text().trimmed();
  v.itemCount = 0;
  v.isDefault = false;
  v.createdAt = QDateTime::currentDateTime();

  const bool ok = app::VaultManager::instance().createVault(v);
  if (!ok) {
    m_errorLabel->setText(tr("A vault with this name already exists."));
    return;
  }

  m_createdVault = v;
  accept();
}

void NewVaultDialog::retranslateUi() {
  if (m_titleLabel) {
    m_titleLabel->setText(tr("Create New Vault"));
  }
  if (m_descLabel) {
    m_descLabel->setText(tr("Create an isolated vault to organize passwords, "
                            "tokens, or shared secrets."));
  }
  if (m_nameEdit) {
    m_nameEdit->setPlaceholderText(
        tr("e.g. Work Credentials, Finance & Crypto"));
  }
  if (m_descEdit) {
    m_descEdit->setPlaceholderText(tr("Short purpose or notes for this vault"));
  }
  if (m_cancelBtn) {
    m_cancelBtn->setText(tr("Cancel"));
  }
  if (m_createBtn) {
    m_createBtn->setText(tr("Create Vault"));
  }
}

void NewVaultDialog::changeEvent(QEvent *event) {
  if (event->type() == QEvent::LanguageChange) {
    retranslateUi();
  }
  QDialog::changeEvent(event);
}

} // namespace svm::views
