/**
 * @file ui/views/detail/detail_inspector_view.cpp
 * @brief Hiện thực DetailInspectorView với kiến trúc Dynamic Category-Driven
 * Form.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "detail_inspector_view.h"
#include "app/theme_manager.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QUuid>
#include <QVBoxLayout>

namespace svm::views {

static QFrame *createDivider(QWidget *parent) {
  auto *divider = new QFrame(parent);
  divider->setObjectName(QStringLiteral("FieldRowDivider"));
  divider->setFixedHeight(1);
  divider->setFrameShape(QFrame::HLine);
  divider->setFrameShadow(QFrame::Plain);
  return divider;
}

DetailInspectorView::DetailInspectorView(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("DetailInspectorView"));
  setAttribute(Qt::WA_StyledBackground, true);
  setMinimumWidth(400);

  setupUI();
  clearItem();
}

void DetailInspectorView::setupUI() {
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);

  m_stackedWidget = new QStackedWidget(this);
  m_stackedWidget->setObjectName(QStringLiteral("DetailStackedWidget"));
  m_stackedWidget->setAttribute(Qt::WA_StyledBackground, true);

  // 1. Trang 0: Empty State
  m_emptyState = new EmptyStateWidget(this);
  m_stackedWidget->addWidget(m_emptyState);

  // 2. Trang 1: ScrollArea chứa Form chi tiết
  m_scrollArea = new QScrollArea(this);
  m_scrollArea->setObjectName(QStringLiteral("DetailScrollArea"));
  m_scrollArea->setWidgetResizable(true);
  m_scrollArea->setFrameShape(QFrame::NoFrame);
  m_scrollArea->viewport()->setObjectName(
      QStringLiteral("DetailScrollViewport"));
  m_scrollArea->viewport()->setAttribute(Qt::WA_StyledBackground, true);

  m_contentContainer = new QWidget(m_scrollArea);
  m_contentContainer->setObjectName(QStringLiteral("DetailContentContainer"));
  m_contentContainer->setAttribute(Qt::WA_StyledBackground, true);

  // Full-width layout đồng bộ với Cột 2, cách biên trái/phải 14px
  auto *contentLayout = new QVBoxLayout(m_contentContainer);
  contentLayout->setContentsMargins(14, 12, 14, 18);
  contentLayout->setSpacing(10);

  // Tầng 1: Header Item
  m_headerWidget = new DetailHeaderWidget(m_contentContainer);
  connect(m_headerWidget, &DetailHeaderWidget::editRequested, this,
          [this]() { setEditMode(true); });
  connect(m_headerWidget, &DetailHeaderWidget::saveRequested, this,
          &DetailInspectorView::onSaveRequested);
  connect(m_headerWidget, &DetailHeaderWidget::cancelRequested, this,
          &DetailInspectorView::onCancelRequested);
  connect(m_headerWidget, &DetailHeaderWidget::categoryChanged, this,
          [this](models::ItemCategory newCat) {
            m_currentItem.category = newCat;
            updateVisibilityForCategory(newCat);
          });
  contentLayout->addWidget(m_headerWidget);

  // Tầng 2: Primary Details Card (Khối cốt lõi biến hình theo Category)
  m_primaryCard = new QWidget(m_contentContainer);
  m_primaryCard->setObjectName(QStringLiteral("DetailSectionCard"));
  m_primaryCard->setAttribute(Qt::WA_StyledBackground, true);
  m_primaryLayout = new QVBoxLayout(m_primaryCard);
  m_primaryLayout->setContentsMargins(14, 12, 14, 12);
  m_primaryLayout->setSpacing(2);

  m_primaryHeader = new QLabel(m_primaryCard);
  m_primaryHeader->setObjectName(QStringLiteral("DetailSectionHeader"));
  m_primaryLayout->addWidget(m_primaryHeader);

  // Khởi tạo các trường nghiệp vụ
  m_usernameField = new FieldRowWidget(
      tr("Username / Email"), FieldRowWidget::FieldType::Text, m_primaryCard);
  m_passwordField = new FieldRowWidget(
      tr("Password"), FieldRowWidget::FieldType::Concealed, m_primaryCard);
  m_websiteField = new FieldRowWidget(
      tr("Website / URL"), FieldRowWidget::FieldType::Url, m_primaryCard);
  m_cardNumberField = new FieldRowWidget(
      tr("Card Number"), FieldRowWidget::FieldType::Concealed, m_primaryCard);
  m_cardExpiryField = new FieldRowWidget(
      tr("Expires (MM/YY)"), FieldRowWidget::FieldType::Text, m_primaryCard);
  m_cardCvvField =
      new FieldRowWidget(tr("Security Code (CVV)"),
                         FieldRowWidget::FieldType::Concealed, m_primaryCard);
  m_wifiSsidField =
      new FieldRowWidget(tr("Network Name (SSID)"),
                         FieldRowWidget::FieldType::Text, m_primaryCard);
  m_dbHostField =
      new FieldRowWidget(tr("Database Host / Port"),
                         FieldRowWidget::FieldType::Text, m_primaryCard);
  m_dbNameField = new FieldRowWidget(
      tr("Database Name"), FieldRowWidget::FieldType::Text, m_primaryCard);
  m_sshFingerprintField =
      new FieldRowWidget(tr("Key Fingerprint (SHA256)"),
                         FieldRowWidget::FieldType::Text, m_primaryCard);
  m_sshKeyField = new FieldRowWidget(
      tr("Public Key"), FieldRowWidget::FieldType::Multiline, m_primaryCard);

  m_totpWidget = new TotpWidget(m_primaryCard);

  m_allPrimaryFields = {
      m_usernameField,       m_passwordField,   m_websiteField,
      m_cardNumberField,     m_cardExpiryField, m_cardCvvField,
      m_wifiSsidField,       m_dbHostField,     m_dbNameField,
      m_sshFingerprintField, m_sshKeyField};

  m_allPrimaryWidgets = {
      m_usernameField, m_passwordField,       m_totpWidget,
      m_websiteField,  m_cardNumberField,     m_cardExpiryField,
      m_cardCvvField,  m_wifiSsidField,       m_dbHostField,
      m_dbNameField,   m_sshFingerprintField, m_sshKeyField};

  for (auto *w : m_allPrimaryWidgets) {
    m_primaryLayout->addWidget(w);
  }

  // Khởi tạo 8 divider dùng luân phiên
  for (int i = 0; i < 8; ++i) {
    auto *div = createDivider(m_primaryCard);
    m_primaryDividers.append(div);
    m_primaryLayout->addWidget(div);
  }

  // Nút thiết lập 2FA chủ động cho Primary Credentials
  m_addTotpBtn = new QPushButton(m_primaryCard);
  m_addTotpBtn->setObjectName(QStringLiteral("AddTotpBtn"));
  m_addTotpBtn->setIcon(
      app::ThemeManager::instance().icon(QStringLiteral("plus")));
  m_addTotpBtn->setIconSize(QSize(14, 14));
  m_addTotpBtn->setText(tr(" Set Up 2FA (One-Time Code)"));
  m_addTotpBtn->setFixedHeight(32);
  m_addTotpBtn->setCursor(Qt::PointingHandCursor);
  m_addTotpBtn->hide();
  m_primaryLayout->addWidget(m_addTotpBtn);

  connect(&app::ThemeManager::instance(), &app::ThemeManager::themeChanged,
          this, [this]() {
            m_addTotpBtn->setIcon(
                app::ThemeManager::instance().icon(QStringLiteral("plus")));
          });

  connect(m_addTotpBtn, &QPushButton::clicked, this, [this]() {
    m_isTotpActive = true;
    m_totpWidget->setEditMode(true);
    updateVisibilityForCategory(m_currentItem.category);
    m_totpWidget->focusSecretInput();
  });

  connect(m_totpWidget, &TotpWidget::removeRequested, this, [this]() {
    m_isTotpActive = false;
    m_totpWidget->clearTotp();
    updateVisibilityForCategory(m_currentItem.category);
  });

  contentLayout->addWidget(m_primaryCard);

  // Tầng 3: Custom Fields Card
  m_customFieldsWidget = new CustomFieldsWidget(m_contentContainer);
  contentLayout->addWidget(m_customFieldsWidget);

  // Tầng 5: Notes Card
  m_notesCard = new QWidget(m_contentContainer);
  m_notesCard->setObjectName(QStringLiteral("DetailSectionCard"));
  m_notesCard->setAttribute(Qt::WA_StyledBackground, true);
  auto *notesLayout = new QVBoxLayout(m_notesCard);
  notesLayout->setContentsMargins(14, 12, 14, 12);
  notesLayout->setSpacing(2);

  m_notesHeader = new QLabel(tr("NOTES"), m_notesCard);
  m_notesHeader->setObjectName(QStringLiteral("DetailSectionHeader"));
  notesLayout->addWidget(m_notesHeader);

  m_notesField =
      new FieldRowWidget(tr("Remarks / Content"),
                         FieldRowWidget::FieldType::Multiline, m_notesCard);
  notesLayout->addWidget(m_notesField);

  contentLayout->addWidget(m_notesCard);

  contentLayout->addStretch(1);

  m_scrollArea->setWidget(m_contentContainer);
  m_stackedWidget->addWidget(m_scrollArea);

  layout->addWidget(m_stackedWidget);
}

void DetailInspectorView::refreshPrimaryCard(
    const QList<QWidget *> &visibleWidgets, const QString &headerTitle) {
  for (auto *w : m_allPrimaryWidgets) {
    w->hide();
  }
  for (auto *d : m_primaryDividers) {
    d->hide();
  }

  if (visibleWidgets.isEmpty()) {
    m_primaryCard->hide();
    return;
  }

  m_primaryCard->show();
  m_primaryHeader->setText(headerTitle.toUpper());

  for (int i = 0; i < visibleWidgets.size(); ++i) {
    auto *w = visibleWidgets[i];
    w->show();
    m_primaryLayout->removeWidget(w);
    m_primaryLayout->addWidget(w);

    if (i < visibleWidgets.size() - 1 && i < m_primaryDividers.size()) {
      auto *d = m_primaryDividers[i];
      d->show();
      m_primaryLayout->removeWidget(d);
      m_primaryLayout->addWidget(d);
    }
  }

  // Đảm bảo m_addTotpBtn luôn ở cuối thẻ
  if (m_addTotpBtn) {
    m_primaryLayout->removeWidget(m_addTotpBtn);
    m_primaryLayout->addWidget(m_addTotpBtn);
  }
}

void DetailInspectorView::setItem(const models::UiVaultItem &item) {
  m_currentItem = item;
  m_isNewItem = false;
  m_isTotpActive = (!item.totpSecret.trimmed().isEmpty() || item.hasTotp);

  m_headerWidget->setItem(item);

  // TOTP
  if (m_isTotpActive) {
    m_totpWidget->setTotpSecret(item.totpSecret.isEmpty()
                                    ? QStringLiteral("JBSWY3DPEHPK3PXP")
                                    : item.totpSecret);
  } else {
    m_totpWidget->clearTotp();
  }

  // Tiêu chuẩn
  m_usernameField->setValue(item.username);
  m_passwordField->setValue(item.password);
  m_websiteField->setValue(item.websiteUrl);
  m_notesField->setValue(item.notes);

  // Chuyên biệt
  m_sshFingerprintField->setValue(item.sshFingerprint);
  m_sshKeyField->setValue(item.sshPublicKey);
  m_cardNumberField->setValue(item.cardNumber);
  m_cardExpiryField->setValue(item.cardExpiry);
  m_cardCvvField->setValue(item.cardCvv);
  m_dbHostField->setValue(item.dbHost);
  m_dbNameField->setValue(item.dbName);
  m_wifiSsidField->setValue(item.wifiSsid);

  // Custom Fields
  m_customFieldsWidget->setCustomFields(item.customFields);

  updateVisibilityForCategory(item.category);
  setEditMode(false);
  m_stackedWidget->setCurrentIndex(1);
}

void DetailInspectorView::clearItem() {
  m_currentItem = {};
  m_isNewItem = false;
  m_customFieldsWidget->setCustomFields({});
  m_stackedWidget->setCurrentIndex(0);
}

void DetailInspectorView::createNewItem(models::ItemCategory category) {
  m_currentItem = {};
  m_currentItem.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
  m_currentItem.category = category;
  m_currentItem.title = QString();
  m_currentItem.createdAt = QDateTime();
  m_currentItem.updatedAt = QDateTime();
  m_currentItem.customFields = {};
  m_isTotpActive = false;

  setItem(m_currentItem);
  m_headerWidget->setCategory(category);
  m_isNewItem = true;
  setEditMode(true);
  m_headerWidget->focusTitleInput();
}

void DetailInspectorView::saveEdit() {
  if (m_isEditMode) {
    onSaveRequested();
  }
}

void DetailInspectorView::cancelEdit() {
  if (m_isEditMode) {
    onCancelRequested();
  }
}

void DetailInspectorView::setEditMode(bool editable) {
  m_isEditMode = editable;
  m_headerWidget->setEditMode(editable);

  for (auto *f : m_allPrimaryFields) {
    f->setEditMode(editable);
  }
  m_totpWidget->setEditMode(editable);
  m_notesField->setEditMode(editable);
  m_customFieldsWidget->setEditMode(editable);

  updateVisibilityForCategory(m_currentItem.category);
}

void DetailInspectorView::onSaveRequested() {
  QString enteredTitle = m_headerWidget->title().trimmed();
  if (enteredTitle.isEmpty()) {
    enteredTitle =
        tr("Untitled %1")
            .arg(models::categoryDisplayName(m_headerWidget->category()));
  }
  m_currentItem.title = enteredTitle;
  m_currentItem.category = m_headerWidget->category();
  m_currentItem.username = m_usernameField->value();
  m_currentItem.password = m_passwordField->value();
  m_currentItem.websiteUrl = m_websiteField->value();
  m_currentItem.notes = m_notesField->value();

  const bool supportsTotp =
      (m_currentItem.category == models::ItemCategory::Login ||
       m_currentItem.category == models::ItemCategory::Database ||
       m_currentItem.category == models::ItemCategory::Server);
  if (supportsTotp && m_isTotpActive) {
    m_currentItem.totpSecret = m_totpWidget->totpSecret();
    m_currentItem.hasTotp = !m_currentItem.totpSecret.trimmed().isEmpty();
    if (!m_currentItem.hasTotp) {
      m_isTotpActive = false;
    }
  } else {
    m_currentItem.totpSecret.clear();
    m_currentItem.hasTotp = false;
    m_isTotpActive = false;
  }

  m_currentItem.sshPublicKey = m_sshKeyField->value();
  m_currentItem.sshFingerprint = m_sshFingerprintField->value();
  m_currentItem.cardNumber = m_cardNumberField->value();
  m_currentItem.cardExpiry = m_cardExpiryField->value();
  m_currentItem.cardCvv = m_cardCvvField->value();
  m_currentItem.dbHost = m_dbHostField->value();
  m_currentItem.dbName = m_dbNameField->value();
  m_wifiSsidField->setValue(m_wifiSsidField->value());
  m_currentItem.wifiSsid = m_wifiSsidField->value();

  // Thu thập custom fields
  m_currentItem.customFields = m_customFieldsWidget->customFields();

  if (!m_currentItem.createdAt.isValid()) {
    m_currentItem.createdAt = QDateTime::currentDateTime();
  }
  m_currentItem.updatedAt = QDateTime::currentDateTime();

  setEditMode(false);
  m_isNewItem = false;
  emit itemSaved(m_currentItem);
}

void DetailInspectorView::onCancelRequested() {
  setEditMode(false);
  if (m_isNewItem) {
    m_isNewItem = false;
    emit cancelRequested();
  } else {
    setItem(m_currentItem);
  }
}

void DetailInspectorView::updateVisibilityForCategory(
    models::ItemCategory category) {
  switch (category) {
  case models::ItemCategory::Login: {
    m_usernameField->setLabel(tr("Username / Email"));
    m_usernameField->setPlaceholderText(tr("user@example.com"));
    m_passwordField->setLabel(tr("Password"));
    m_passwordField->setPlaceholderText(tr("Account password"));
    m_websiteField->setLabel(tr("Website / URL"));
    m_websiteField->setPlaceholderText(tr("https://example.com/login"));

    QList<QWidget *> fields = {m_usernameField, m_passwordField};
    if (m_isTotpActive) {
      fields.append(m_totpWidget);
    }
    fields.append(m_websiteField);

    refreshPrimaryCard(fields, tr("Login Credentials"));
    m_notesHeader->setText(tr("NOTES"));
    break;
  }

  case models::ItemCategory::Passkey:
    m_usernameField->setLabel(tr("Username / User Handle"));
    m_usernameField->setPlaceholderText(tr("octocat@github"));
    m_websiteField->setLabel(tr("Relying Party / Domain"));
    m_websiteField->setPlaceholderText(tr("https://github.com"));
    refreshPrimaryCard({m_usernameField, m_websiteField},
                       tr("Passkey Metadata"));
    m_notesHeader->setText(tr("NOTES"));
    break;

  case models::ItemCategory::Card:
    m_usernameField->setLabel(tr("Cardholder Name"));
    m_usernameField->setPlaceholderText(tr("NAME ON CARD"));
    m_cardNumberField->setLabel(tr("Card Number"));
    m_cardNumberField->setPlaceholderText(tr("4111 2222 3333 4444"));
    m_cardExpiryField->setLabel(tr("Expires (MM/YY)"));
    m_cardExpiryField->setPlaceholderText(tr("12/28"));
    m_cardCvvField->setLabel(tr("Security Code (CVV)"));
    m_cardCvvField->setPlaceholderText(tr("123"));
    refreshPrimaryCard(
        {m_usernameField, m_cardNumberField, m_cardExpiryField, m_cardCvvField},
        tr("Credit Card Details"));
    m_notesHeader->setText(tr("NOTES"));
    break;

  case models::ItemCategory::Wifi:
    m_wifiSsidField->setLabel(tr("Network Name (SSID)"));
    m_wifiSsidField->setPlaceholderText(tr("MyHome_5GHz"));
    m_passwordField->setLabel(tr("Wi-Fi Password / Key"));
    m_passwordField->setPlaceholderText(tr("WPA2/WPA3 network password"));
    refreshPrimaryCard({m_wifiSsidField, m_passwordField},
                       tr("Wi-Fi Network Credentials"));
    m_notesHeader->setText(tr("NOTES"));
    break;

  case models::ItemCategory::Database: {
    m_dbHostField->setLabel(tr("Database Host & Port"));
    m_dbHostField->setPlaceholderText(tr("db.internal.net:5432"));
    m_dbNameField->setLabel(tr("Database Name"));
    m_dbNameField->setPlaceholderText(tr("production_db"));
    m_usernameField->setLabel(tr("Database User"));
    m_usernameField->setPlaceholderText(tr("postgres_admin"));
    m_passwordField->setLabel(tr("Database Password"));
    m_passwordField->setPlaceholderText(tr("db_password"));

    QList<QWidget *> fields = {m_dbHostField, m_dbNameField, m_usernameField,
                               m_passwordField};
    if (m_isTotpActive) {
      fields.append(m_totpWidget);
    }

    refreshPrimaryCard(fields, tr("Database Connection Details"));
    m_notesHeader->setText(tr("NOTES"));
    break;
  }

  case models::ItemCategory::Server: {
    m_websiteField->setLabel(tr("Server Host / IP Address"));
    m_websiteField->setPlaceholderText(
        tr("192.168.1.100 or server.domain.com"));
    m_usernameField->setLabel(tr("Admin Username"));
    m_usernameField->setPlaceholderText(tr("root / admin"));
    m_passwordField->setLabel(tr("Password / Root Secret"));
    m_passwordField->setPlaceholderText(tr("root_password"));

    QList<QWidget *> fields = {m_websiteField, m_usernameField,
                               m_passwordField};
    if (m_isTotpActive) {
      fields.append(m_totpWidget);
    }

    refreshPrimaryCard(fields, tr("Server Credentials"));
    m_notesHeader->setText(tr("NOTES"));
    break;
  }

  case models::ItemCategory::SshKey:
    m_sshFingerprintField->setLabel(tr("Key Fingerprint (SHA256)"));
    m_sshFingerprintField->setPlaceholderText(tr("SHA256:abc123..."));
    m_passwordField->setLabel(tr("Key Passphrase"));
    m_passwordField->setPlaceholderText(tr("Optional passphrase"));
    m_sshKeyField->setLabel(tr("Public Key"));
    m_sshKeyField->setPlaceholderText(tr("ssh-ed25519 AAAAC3..."));
    refreshPrimaryCard({m_sshFingerprintField, m_passwordField, m_sshKeyField},
                       tr("SSH Key Credentials"));
    m_notesHeader->setText(tr("NOTES"));
    break;

  case models::ItemCategory::Identity:
    m_usernameField->setLabel(tr("Full Name"));
    m_usernameField->setPlaceholderText(tr("Full legal name"));
    m_cardNumberField->setLabel(tr("ID / Passport Number"));
    m_cardNumberField->setPlaceholderText(tr("National ID or Passport"));
    m_websiteField->setLabel(tr("Contact Email / Phone"));
    m_websiteField->setPlaceholderText(tr("contact@domain.com"));
    refreshPrimaryCard({m_usernameField, m_cardNumberField, m_websiteField},
                       tr("Identity Profile"));
    m_notesHeader->setText(tr("NOTES"));
    break;

  case models::ItemCategory::SecureNote:
    refreshPrimaryCard({}, "");
    m_notesHeader->setText(tr("SECURE NOTE CONTENT"));
    break;
  }

  // Hiển thị nút "+ Set Up 2FA" ở cuối thẻ nếu đang ở Edit Mode và chưa kích
  // hoạt 2FA
  const bool supportsTotp = (category == models::ItemCategory::Login ||
                             category == models::ItemCategory::Database ||
                             category == models::ItemCategory::Server);
  const bool showAddTotp = m_isEditMode && supportsTotp && !m_isTotpActive;
  m_addTotpBtn->setVisible(showAddTotp);
}

} // namespace svm::views
