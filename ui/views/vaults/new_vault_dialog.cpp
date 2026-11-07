/**
 * @file ui/views/vaults/new_vault_dialog.cpp
 * @brief Hiện thực hộp thoại tạo kho lưu trữ mới NewVaultDialog trực quan cao.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "new_vault_dialog.h"
#include "app/theme_manager.h"
#include "app/vault_manager.h"
#include "components/dialog_drag_helper.h"

#include <QEvent>
#include <QGraphicsDropShadowEffect>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeyEvent>
#include <QStyle>
#include <QVBoxLayout>

namespace svm::views {

NewVaultDialog::NewVaultDialog(QWidget *parent)
    : QDialog(parent, Qt::FramelessWindowHint | Qt::Dialog) {
  setObjectName(QStringLiteral("NewVaultDialog"));
  setAttribute(Qt::WA_TranslucentBackground, true);
  setModal(false);
  setWindowModality(Qt::NonModal);
  setFixedSize(480, 520);

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

  auto *cardLayout = new QVBoxLayout(m_cardWidget);
  cardLayout->setContentsMargins(0, 0, 0, 16);
  cardLayout->setSpacing(0);

  // 1. Thanh tiêu đề kéo thả tích hợp
  m_headerWidget = new QWidget(m_cardWidget);
  m_headerWidget->setObjectName(QStringLiteral("NewVaultHeader"));
  m_headerWidget->setFixedHeight(44);

  auto *headerLayout = new QHBoxLayout(m_headerWidget);
  headerLayout->setContentsMargins(16, 0, 8, 0);
  headerLayout->setSpacing(10);

  m_headerIcon = new QLabel(m_headerWidget);
  m_headerIcon->setFixedSize(18, 18);
  m_headerIcon->setPixmap(app::ThemeManager::instance()
                              .icon(QStringLiteral("folder_open"))
                              .pixmap(18, 18));
  headerLayout->addWidget(m_headerIcon);

  m_headerTitle = new QLabel(tr("Create New Vault"), m_headerWidget);
  m_headerTitle->setObjectName(QStringLiteral("NewVaultTitle"));
  QFont hf = m_headerTitle->font();
  hf.setPointSize(12);
  hf.setBold(true);
  m_headerTitle->setFont(hf);
  headerLayout->addWidget(m_headerTitle, 1);

  m_closeBtn = new QPushButton(m_headerWidget);
  m_closeBtn->setObjectName(QStringLiteral("DialogCloseBtn"));
  m_closeBtn->setFixedSize(32, 32);
  m_closeBtn->setIconSize(QSize(12, 12));
  m_closeBtn->setCursor(Qt::PointingHandCursor);
  m_closeBtn->setIcon(
      app::ThemeManager::instance().icon(QStringLiteral("win_close")));
  connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::reject);
  headerLayout->addWidget(m_closeBtn);

  cardLayout->addWidget(m_headerWidget);

  // 2. Nội dung thân hộp thoại
  auto *bodyWidget = new QWidget(m_cardWidget);
  auto *layout = new QVBoxLayout(bodyWidget);
  layout->setContentsMargins(20, 14, 20, 0);
  layout->setSpacing(10);

  m_descLabel = new QLabel(tr("Create an isolated vault to organize passwords, "
                              "tokens, or shared secrets."),
                           bodyWidget);
  m_descLabel->setWordWrap(true);
  m_descLabel->setObjectName(QStringLiteral("DialogDescLabel"));
  layout->addWidget(m_descLabel);

  // Vault Name
  auto *nameLabel = new QLabel(tr("Vault Name"), bodyWidget);
  nameLabel->setObjectName(QStringLiteral("DialogFieldLabel"));
  layout->addWidget(nameLabel);

  m_nameEdit = new QLineEdit(bodyWidget);
  m_nameEdit->setPlaceholderText(tr("e.g. Work Credentials, Finance & Crypto"));
  m_nameEdit->setFixedHeight(36);
  layout->addWidget(m_nameEdit);

  // Icon Picker
  auto *iconTitle = new QLabel(tr("Vault Icon"), bodyWidget);
  iconTitle->setObjectName(QStringLiteral("DialogFieldLabel"));
  layout->addWidget(iconTitle);
  layout->addWidget(createIconPicker());

  // Accent Color Picker
  auto *colorTitle = new QLabel(tr("Accent Color"), bodyWidget);
  colorTitle->setObjectName(QStringLiteral("DialogFieldLabel"));
  layout->addWidget(colorTitle);
  layout->addWidget(createColorPicker());

  // Description
  auto *descTitle = new QLabel(tr("Description (Optional)"), bodyWidget);
  descTitle->setObjectName(QStringLiteral("DialogFieldLabel"));
  layout->addWidget(descTitle);

  m_descEdit = new QLineEdit(bodyWidget);
  m_descEdit->setPlaceholderText(tr("Short purpose or notes for this vault"));
  m_descEdit->setFixedHeight(34);
  layout->addWidget(m_descEdit);

  m_errorLabel = new QLabel(bodyWidget);
  m_errorLabel->setObjectName(QStringLiteral("DialogErrorLabel"));
  layout->addWidget(m_errorLabel);

  layout->addStretch(1);

  // Action buttons
  auto *btnLayout = new QHBoxLayout();
  m_cancelBtn = new QPushButton(tr("Cancel"), bodyWidget);
  m_cancelBtn->setFixedHeight(34);
  m_cancelBtn->setCursor(Qt::PointingHandCursor);
  connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
  btnLayout->addWidget(m_cancelBtn);

  btnLayout->addStretch(1);

  m_createBtn = new QPushButton(tr("Create Vault"), bodyWidget);
  m_createBtn->setObjectName(QStringLiteral("OnboardingPrimaryBtn"));
  m_createBtn->setFixedHeight(34);
  m_createBtn->setCursor(Qt::PointingHandCursor);
  connect(m_createBtn, &QPushButton::clicked, this,
          &NewVaultDialog::onCreateClicked);
  btnLayout->addWidget(m_createBtn);

  layout->addLayout(btnLayout);
  cardLayout->addWidget(bodyWidget, 1);

  components::DialogDragHelper::makeDraggable(m_cardWidget, this);
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
    btn->setObjectName(QStringLiteral("IconPickerBtn"));
    btn->setFixedSize(32, 32);
    btn->setIcon(app::ThemeManager::instance().icon(iconName));
    btn->setIconSize(QSize(16, 16));
    btn->setCursor(Qt::PointingHandCursor);
    btn->setProperty("iconName", iconName);
    btn->setProperty("selected", iconName == m_selectedIcon);

    connect(btn, &QPushButton::clicked, this, [this, btn, iconName]() {
      m_selectedIcon = iconName;
      for (auto *b : m_iconButtons) {
        b->setProperty("selected", b == btn);
        b->style()->unpolish(b);
        b->style()->polish(b);
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

void NewVaultDialog::setEditVault(const models::UiVault &vault) {
  m_isEditMode = true;
  m_createdVault = vault;
  m_selectedIcon = vault.iconName;
  m_selectedColor = vault.accentColor;

  if (m_headerTitle) {
    m_headerTitle->setText(tr("Edit Vault"));
  }
  if (m_descLabel) {
    m_descLabel->setText(tr("Update vault name, icon, and appearance color."));
  }
  if (m_nameEdit) {
    m_nameEdit->setText(vault.name);
  }
  if (m_descEdit) {
    m_descEdit->setText(vault.description);
  }
  if (m_createBtn) {
    m_createBtn->setText(tr("Save Changes"));
  }
  for (auto *b : m_iconButtons) {
    b->setProperty("selected",
                   b->property("iconName").toString() == m_selectedIcon);
    b->style()->unpolish(b);
    b->style()->polish(b);
  }
  for (auto *b : m_colorButtons) {
    const QString bColor = b->property("colorHex").toString();
    if (bColor.compare(m_selectedColor, Qt::CaseInsensitive) == 0) {
      b->setStyleSheet(QStringLiteral("background-color: %1; border: 2px "
                                      "solid #ffffff; border-radius: 13px;")
                           .arg(bColor));
    } else {
      b->setStyleSheet(QStringLiteral("background-color: %1; border: 1px "
                                      "solid #313244; border-radius: 13px;")
                           .arg(bColor));
    }
  }
}

void NewVaultDialog::onCreateClicked() {
  const QString name = m_nameEdit->text().trimmed();
  if (name.isEmpty()) {
    m_errorLabel->setText(tr("Vault name cannot be empty."));
    return;
  }

  if (m_isEditMode) {
    m_createdVault.name = name;
    m_createdVault.iconName = m_selectedIcon;
    m_createdVault.accentColor = m_selectedColor;
    m_createdVault.description = m_descEdit->text().trimmed();

    const bool ok = app::VaultManager::instance().updateVault(m_createdVault);
    if (!ok) {
      m_errorLabel->setText(tr("Could not update vault."));
      return;
    }
    accept();
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
  if (m_headerTitle) {
    m_headerTitle->setText(m_isEditMode ? tr("Edit Vault")
                                        : tr("Create New Vault"));
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
    m_createBtn->setText(m_isEditMode ? tr("Save Changes")
                                      : tr("Create Vault"));
  }
}

void NewVaultDialog::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Escape) {
    reject();
    return;
  }
  QDialog::keyPressEvent(event);
}

void NewVaultDialog::changeEvent(QEvent *event) {
  if (event->type() == QEvent::LanguageChange) {
    retranslateUi();
  }
  QDialog::changeEvent(event);
}

} // namespace svm::views
