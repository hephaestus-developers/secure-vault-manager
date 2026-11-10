/**
 * @file ui/views/detail/detail_inspector_view.cpp
 * @brief Hiện thực DetailInspectorView với kiến trúc Dynamic Category-Driven
 * Form.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "detail_inspector_view.h"
#include "app/dev_logger.h"
#include "app/theme_manager.h"
#include "app/vault_manager.h"
#include "components/in_window_combobox.h"
#include "components/notification_dialog.h"
#include "core/core_bridge.h"

#include <QDateTime>
#include <QDialog>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QStandardPaths>
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
  connect(m_emptyState, &EmptyStateWidget::createItemRequested, this,
          [this]() { createNewItem(models::ItemCategory::Login); });
  m_stackedWidget->addWidget(m_emptyState);

  // 2. Trang 1: ScrollArea chứa Form chi tiết
  m_scrollArea = new QScrollArea(this);
  m_scrollArea->setObjectName(QStringLiteral("DetailScrollArea"));
  m_scrollArea->setWidgetResizable(true);
  m_scrollArea->setFrameShape(QFrame::NoFrame);
  m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
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
  connect(m_headerWidget, &DetailHeaderWidget::deleteRequested, this, [this]() {
    if (m_currentItem.id.isEmpty()) {
      return;
    }
    if (m_currentItem.isDeleted) {
      emit itemPurgeRequested(m_currentItem.id);
    } else {
      emit itemTrashRequested(m_currentItem.id);
    }
  });
  connect(m_headerWidget, &DetailHeaderWidget::restoreRequested, this,
          [this]() {
            if (!m_currentItem.id.isEmpty()) {
              emit itemRestoreRequested(m_currentItem.id);
            }
          });
  connect(m_headerWidget, &DetailHeaderWidget::historyRequested, this,
          &DetailInspectorView::onHistoryRequested);
  connect(m_headerWidget, &DetailHeaderWidget::favoriteToggled, this,
          [this](bool isFav) {
            m_currentItem.isFavorite = isFav;
            if (!m_isEditMode && !m_isNewItem && !m_currentItem.id.isEmpty()) {
              emit itemSaved(m_currentItem);
            }
          });
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
  m_wifiSecurityField = new FieldRowWidget(
      tr("Security Type"), FieldRowWidget::FieldType::Text, m_primaryCard);
  m_dbHostField = new FieldRowWidget(
      tr("Database Host"), FieldRowWidget::FieldType::Text, m_primaryCard);
  m_dbPortField = new FieldRowWidget(
      tr("Database Port"), FieldRowWidget::FieldType::Text, m_primaryCard);
  m_dbNameField = new FieldRowWidget(
      tr("Database Name"), FieldRowWidget::FieldType::Text, m_primaryCard);
  m_sshFingerprintField =
      new FieldRowWidget(tr("Key Fingerprint (SHA256)"),
                         FieldRowWidget::FieldType::Text, m_primaryCard);
  m_sshKeyField = new FieldRowWidget(
      tr("Public Key"), FieldRowWidget::FieldType::Multiline, m_primaryCard);

  m_totpWidget = new TotpWidget(m_primaryCard);

  m_allPrimaryFields = {
      m_usernameField,   m_passwordField,     m_websiteField,
      m_cardNumberField, m_cardExpiryField,   m_cardCvvField,
      m_wifiSsidField,   m_wifiSecurityField, m_dbHostField,
      m_dbPortField,     m_dbNameField,       m_sshFingerprintField,
      m_sshKeyField};

  m_allPrimaryWidgets = {
      m_usernameField,       m_passwordField,   m_totpWidget,   m_websiteField,
      m_cardNumberField,     m_cardExpiryField, m_cardCvvField, m_wifiSsidField,
      m_wifiSecurityField,   m_dbHostField,     m_dbPortField,  m_dbNameField,
      m_sshFingerprintField, m_sshKeyField};

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
  m_addTotpBtn->setText(tr("Set Up 2FA (One-Time Code)"));
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

  m_genSshKeyBtn = new QPushButton(m_primaryCard);
  m_genSshKeyBtn->setObjectName(QStringLiteral("AddTotpBtn"));
  m_genSshKeyBtn->setIcon(
      app::ThemeManager::instance().icon(QStringLiteral("plus")));
  m_genSshKeyBtn->setIconSize(QSize(14, 14));
  m_genSshKeyBtn->setText(tr("Generate Ed25519 SSH Keypair"));
  m_genSshKeyBtn->setFixedHeight(32);
  m_genSshKeyBtn->setCursor(Qt::PointingHandCursor);
  m_genSshKeyBtn->hide();
  connect(m_genSshKeyBtn, &QPushButton::clicked, this, [this]() {
    auto &bridge = core::CoreBridge::instance();
    if (!bridge.isInitialized() || !bridge.isVaultUnlocked()) {
      return;
    }
    const QString title = m_headerWidget->title().trimmed().isEmpty()
                              ? QStringLiteral("Generated SSH Key (Ed25519)")
                              : m_headerWidget->title().trimmed();
    const QString comment = m_usernameField->value().trimmed().isEmpty()
                                ? QStringLiteral("user@secure-vault")
                                : m_usernameField->value().trimmed();
    const QJsonObject res =
        bridge.generateSshKey(title, comment, m_currentItem.vaultId);
    const QString genId = res.value(QStringLiteral("id")).toString();
    if (genId.isEmpty()) {
      return;
    }
    if (const auto generated = bridge.getItem(genId)) {
      m_sshFingerprintField->setValue(generated->sshFingerprint);
      m_sshKeyField->setValue(generated->sshPublicKey);
      m_notesField->setValue(generated->notes);
      m_currentItem.sshFingerprint = generated->sshFingerprint;
      m_currentItem.sshPublicKey = generated->sshPublicKey;
      m_currentItem.notes = generated->notes;
      m_currentItem.isSshLoaded = true;
      if (m_isNewItem) {
        m_currentItem.id = genId;
        m_isNewItem = false;
      } else if (genId != m_currentItem.id) {
        bridge.purgeItem(genId);
      }
    }
  });
  m_primaryLayout->addWidget(m_genSshKeyBtn);

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

  // Tầng 6: Folder Assignment Card
  m_metaCard = new QWidget(m_contentContainer);
  m_metaCard->setObjectName(QStringLiteral("DetailSectionCard"));
  m_metaCard->setAttribute(Qt::WA_StyledBackground, true);
  auto *metaLayout = new QHBoxLayout(m_metaCard);
  metaLayout->setContentsMargins(14, 10, 14, 10);
  metaLayout->setSpacing(10);

  m_folderHeader = new QLabel(tr("FOLDER"), m_metaCard);
  m_folderHeader->setObjectName(QStringLiteral("DetailSectionHeader"));
  metaLayout->addWidget(m_folderHeader);

  m_folderValueLabel = new QLabel(tr("No Folder"), m_metaCard);
  m_folderValueLabel->setObjectName(QStringLiteral("DetailUpdatedLabel"));
  metaLayout->addWidget(m_folderValueLabel, 1);

  auto *folderCombo = new svm::ui::InWindowComboBox(m_metaCard);
  folderCombo->setFixedHeight(28);
  folderCombo->setPopupMinWidth(180);
  m_folderCombo = folderCombo;
  m_folderCombo->hide();
  metaLayout->addWidget(m_folderCombo, 1);
  contentLayout->addWidget(m_metaCard);

  // Tầng 7: Encrypted Attachments Card
  m_attachmentsCard = new QWidget(m_contentContainer);
  m_attachmentsCard->setObjectName(QStringLiteral("DetailSectionCard"));
  m_attachmentsCard->setAttribute(Qt::WA_StyledBackground, true);
  auto *attOuter = new QVBoxLayout(m_attachmentsCard);
  attOuter->setContentsMargins(14, 12, 14, 12);
  attOuter->setSpacing(8);

  auto *attHeaderRow = new QHBoxLayout();
  m_attachmentsHeader = new QLabel(tr("ATTACHMENTS"), m_attachmentsCard);
  m_attachmentsHeader->setObjectName(QStringLiteral("DetailSectionHeader"));
  attHeaderRow->addWidget(m_attachmentsHeader);
  attHeaderRow->addStretch(1);

  m_addAttachmentBtn = new QPushButton(tr("Attach File..."), m_attachmentsCard);
  m_addAttachmentBtn->setObjectName(QStringLiteral("AddTotpBtn"));
  m_addAttachmentBtn->setIcon(
      app::ThemeManager::instance().icon(QStringLiteral("plus")));
  m_addAttachmentBtn->setIconSize(QSize(14, 14));
  m_addAttachmentBtn->setFixedHeight(28);
  m_addAttachmentBtn->setCursor(Qt::PointingHandCursor);
  connect(m_addAttachmentBtn, &QPushButton::clicked, this,
          &DetailInspectorView::onAddAttachmentClicked);
  attHeaderRow->addWidget(m_addAttachmentBtn);
  attOuter->addLayout(attHeaderRow);

  m_attachmentsListLayout = new QVBoxLayout();
  m_attachmentsListLayout->setContentsMargins(0, 0, 0, 0);
  m_attachmentsListLayout->setSpacing(6);
  attOuter->addLayout(m_attachmentsListLayout);

  m_noAttachmentsLabel =
      new QLabel(tr("No encrypted attachments"), m_attachmentsCard);
  m_noAttachmentsLabel->setObjectName(QStringLiteral("DetailUpdatedLabel"));
  attOuter->addWidget(m_noAttachmentsLabel);

  contentLayout->addWidget(m_attachmentsCard);

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
  SVM_ACTION_SCOPE_DETAIL("UI", "DetailInspectorView::setItem",
                          QStringLiteral("title=\"%1\"").arg(item.title));
  m_currentItem = item;
  m_isNewItem = false;
  m_isTotpActive = (!item.totpSecret.trimmed().isEmpty() || item.hasTotp);

  m_headerWidget->setItem(item);

  // TOTP
  if (!item.totpSecret.trimmed().isEmpty()) {
    m_totpWidget->setTotpSecret(item.totpSecret);
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
  m_dbPortField->setValue(item.dbPort > 0 ? QString::number(item.dbPort)
                                          : QStringLiteral("5432"));
  m_dbNameField->setValue(item.dbName);
  m_wifiSsidField->setValue(item.wifiSsid);
  m_wifiSecurityField->setValue(item.wifiSecurity.isEmpty()
                                    ? QStringLiteral("WPA2/WPA3")
                                    : item.wifiSecurity);

  // Custom Fields
  m_customFieldsWidget->setCustomFields(item.customFields);

  refreshFolderOptions();
  refreshAttachments();

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
  QString activeVId = app::VaultManager::instance().activeVaultId();
  if (activeVId.isEmpty() || activeVId == QStringLiteral("all")) {
    activeVId = app::VaultManager::instance().defaultVault().id;
  }
  if (activeVId.isEmpty()) {
    activeVId = QStringLiteral("personal");
  }
  m_currentItem.vaultId = activeVId;
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
  if (m_folderCombo && m_folderValueLabel) {
    if (editable) {
      refreshFolderOptions();
    }
    m_folderCombo->setVisible(editable);
    m_folderValueLabel->setVisible(!editable);
  }

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
  m_currentItem.vaultId = m_headerWidget->vaultId();
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
  bool portOk = false;
  const int parsedPort = m_dbPortField->value().trimmed().toInt(&portOk);
  m_currentItem.dbPort =
      (portOk && parsedPort > 0 && parsedPort <= 65535) ? parsedPort : 5432;
  m_currentItem.dbName = m_dbNameField->value();
  m_currentItem.wifiSsid = m_wifiSsidField->value();
  m_currentItem.wifiSecurity = m_wifiSecurityField->value().trimmed().isEmpty()
                                   ? QStringLiteral("WPA2/WPA3")
                                   : m_wifiSecurityField->value().trimmed();

  // Thu thập custom fields & folder
  m_currentItem.customFields = m_customFieldsWidget->customFields();
  if (m_folderCombo && m_folderCombo->currentIndex() >= 0) {
    m_currentItem.folderId = m_folderCombo->currentData().toString();
  }

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
    m_wifiSecurityField->setLabel(tr("Security Type"));
    m_wifiSecurityField->setPlaceholderText(tr("WPA2/WPA3"));
    m_passwordField->setLabel(tr("Wi-Fi Password / Key"));
    m_passwordField->setPlaceholderText(tr("WPA2/WPA3 network password"));
    refreshPrimaryCard({m_wifiSsidField, m_wifiSecurityField, m_passwordField},
                       tr("Wi-Fi Network Credentials"));
    m_notesHeader->setText(tr("NOTES"));
    break;

  case models::ItemCategory::Database: {
    m_dbHostField->setLabel(tr("Database Host"));
    m_dbHostField->setPlaceholderText(tr("db.internal.net"));
    m_dbPortField->setLabel(tr("Database Port"));
    m_dbPortField->setPlaceholderText(tr("5432"));
    m_dbNameField->setLabel(tr("Database Name"));
    m_dbNameField->setPlaceholderText(tr("production_db"));
    m_usernameField->setLabel(tr("Database User"));
    m_usernameField->setPlaceholderText(tr("postgres_admin"));
    m_passwordField->setLabel(tr("Database Password"));
    m_passwordField->setPlaceholderText(tr("db_password"));

    QList<QWidget *> fields = {m_dbHostField, m_dbPortField, m_dbNameField,
                               m_usernameField, m_passwordField};
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
  if (m_genSshKeyBtn) {
    m_genSshKeyBtn->setVisible(m_isEditMode &&
                               category == models::ItemCategory::SshKey);
  }
}

void DetailInspectorView::retranslateUi() {
  if (m_emptyState) {
    m_emptyState->retranslateUi();
  }
  if (m_headerWidget) {
    m_headerWidget->retranslateUi();
  }
  if (m_totpWidget) {
    m_totpWidget->retranslateUi();
  }
  if (m_customFieldsWidget) {
    m_customFieldsWidget->retranslateUi();
  }
  if (m_notesField) {
    m_notesField->setLabel(tr("Remarks / Content"));
    m_notesField->setPlaceholderText(tr("Add notes or remarks..."));
    m_notesField->retranslateUi();
  }
  if (m_addTotpBtn) {
    m_addTotpBtn->setText(tr("Set Up 2FA (One-Time Code)"));
  }
  if (m_genSshKeyBtn) {
    m_genSshKeyBtn->setText(tr("Generate Ed25519 SSH Keypair"));
  }
  if (m_folderHeader) {
    m_folderHeader->setText(tr("FOLDER"));
  }
  if (m_attachmentsHeader) {
    m_attachmentsHeader->setText(tr("ATTACHMENTS"));
  }
  if (m_addAttachmentBtn) {
    m_addAttachmentBtn->setText(tr("Attach File..."));
  }
  if (m_noAttachmentsLabel) {
    m_noAttachmentsLabel->setText(tr("No encrypted attachments"));
  }
  for (auto *f : m_allPrimaryFields) {
    if (f) {
      f->retranslateUi();
    }
  }
  updateVisibilityForCategory(m_currentItem.category);
}

void DetailInspectorView::refreshFolderOptions() {
  if (!m_folderCombo || !m_folderValueLabel) {
    return;
  }
  m_folderCombo->blockSignals(true);
  m_folderCombo->clear();
  m_folderCombo->addItem(tr("No Folder"), QString());

  QString resolvedName = tr("No Folder");
  int selectedIdx = 0;

  auto &bridge = core::CoreBridge::instance();
  if (bridge.isInitialized() && bridge.isVaultUnlocked()) {
    const auto folders = bridge.listFolders();
    for (int i = 0; i < folders.size(); ++i) {
      m_folderCombo->addItem(folders[i].name, folders[i].id);
      if (folders[i].id == m_currentItem.folderId) {
        selectedIdx = i + 1;
        resolvedName = folders[i].name;
      }
    }
  }
  m_folderCombo->setCurrentIndex(selectedIdx);
  m_folderValueLabel->setText(resolvedName);
  m_folderCombo->blockSignals(false);
}

void DetailInspectorView::refreshAttachments() {
  if (!m_attachmentsListLayout || !m_noAttachmentsLabel || !m_attachmentsCard) {
    return;
  }
  QLayoutItem *child = nullptr;
  while ((child = m_attachmentsListLayout->takeAt(0)) != nullptr) {
    if (child->widget()) {
      child->widget()->deleteLater();
    }
    delete child;
  }

  auto &bridge = core::CoreBridge::instance();
  QList<models::UiAttachmentMeta> atts;
  if (!m_isNewItem && !m_currentItem.id.isEmpty() && bridge.isInitialized() &&
      bridge.isVaultUnlocked()) {
    atts = bridge.listAttachments(m_currentItem.id);
  }
  m_noAttachmentsLabel->setVisible(atts.isEmpty());
  if (m_addAttachmentBtn) {
    m_addAttachmentBtn->setEnabled(!m_isNewItem && !m_currentItem.id.isEmpty());
  }

  for (const auto &att : atts) {
    auto *row = new QWidget(m_attachmentsCard);
    auto *rl = new QHBoxLayout(row);
    rl->setContentsMargins(0, 2, 0, 2);
    rl->setSpacing(8);

    const double kb = static_cast<double>(att.sizeBytes) / 1024.0;
    const QString sizeStr =
        kb >= 1024.0 ? QStringLiteral("%1 MB").arg(kb / 1024.0, 0, 'f', 1)
                     : QStringLiteral("%1 KB").arg(qMax(0.1, kb), 0, 'f', 1);
    auto *lbl =
        new QLabel(QStringLiteral("%1 (%2)").arg(att.fileName, sizeStr), row);
    lbl->setObjectName(QStringLiteral("DetailUpdatedLabel"));
    rl->addWidget(lbl, 1);

    auto *exportBtn = new QPushButton(tr("Save As..."), row);
    exportBtn->setFixedHeight(26);
    exportBtn->setCursor(Qt::PointingHandCursor);
    const QString attId = att.id;
    const QString defName = att.fileName;
    connect(exportBtn, &QPushButton::clicked, this, [this, attId, defName]() {
      const QString dst = QFileDialog::getSaveFileName(
          this, tr("Export Attachment"),
          QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) +
              QLatin1Char('/') + defName);
      if (dst.isEmpty()) {
        return;
      }
      if (core::CoreBridge::instance().exportAttachment(attId, dst)) {
        components::NotificationDialog::success(
            this, tr("Attachment Exported"),
            tr("Decrypted file saved to %1").arg(dst));
      } else {
        components::NotificationDialog::warning(
            this, tr("Export Failed"),
            core::CoreBridge::instance().lastError());
      }
    });
    rl->addWidget(exportBtn);

    auto *delBtn = new QPushButton(tr("Delete"), row);
    delBtn->setFixedHeight(26);
    delBtn->setCursor(Qt::PointingHandCursor);
    connect(delBtn, &QPushButton::clicked, this, [this, attId]() {
      if (core::CoreBridge::instance().deleteAttachment(attId)) {
        refreshAttachments();
      }
    });
    rl->addWidget(delBtn);

    m_attachmentsListLayout->addWidget(row);
  }
}

void DetailInspectorView::onAddAttachmentClicked() {
  if (m_currentItem.id.isEmpty()) {
    return;
  }
  const QString srcPath = QFileDialog::getOpenFileName(
      this, tr("Attach Encrypted File"),
      QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
  if (srcPath.isEmpty()) {
    return;
  }
  if (core::CoreBridge::instance().addAttachment(m_currentItem.id, srcPath)) {
    refreshAttachments();
  } else {
    components::NotificationDialog::warning(
        this, tr("Attach Failed"), core::CoreBridge::instance().lastError());
  }
}

void DetailInspectorView::onHistoryRequested() {
  if (m_currentItem.id.isEmpty()) {
    return;
  }
  auto &bridge = core::CoreBridge::instance();
  const auto revisions = bridge.listHistory(m_currentItem.id);
  QDialog dlg(this);
  dlg.setWindowTitle(tr("Revision History"));
  dlg.resize(460, 340);
  auto *vl = new QVBoxLayout(&dlg);
  vl->setContentsMargins(16, 16, 16, 16);
  vl->setSpacing(10);

  auto *info =
      new QLabel(revisions.isEmpty()
                     ? tr("No revision snapshots recorded for this item yet.")
                     : tr("Select a previous revision snapshot to restore:"),
                 &dlg);
  vl->addWidget(info);

  auto *list = new QListWidget(&dlg);
  for (int i = revisions.size() - 1; i >= 0; --i) {
    const auto &rev = revisions[i];
    const QDateTime dt = QDateTime::fromMSecsSinceEpoch(rev.timestamp);
    const QString label =
        tr("Revision #%1 — %2 (%3)")
            .arg(rev.revision)
            .arg(dt.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")),
                 rev.snapshot.title);
    auto *li = new QListWidgetItem(label, list);
    li->setData(Qt::UserRole, rev.revision);
  }
  vl->addWidget(list, 1);

  auto *btnRow = new QHBoxLayout();
  btnRow->addStretch(1);
  auto *closeBtn = new QPushButton(tr("Close"), &dlg);
  connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
  btnRow->addWidget(closeBtn);

  auto *restoreBtn = new QPushButton(tr("Restore Selected"), &dlg);
  restoreBtn->setEnabled(!revisions.isEmpty());
  connect(restoreBtn, &QPushButton::clicked, &dlg, [this, &dlg, list]() {
    auto *cur = list->currentItem();
    if (!cur) {
      return;
    }
    const int revNum = cur->data(Qt::UserRole).toInt();
    if (core::CoreBridge::instance().restoreHistory(m_currentItem.id, revNum)) {
      if (const auto updated =
              core::CoreBridge::instance().getItem(m_currentItem.id)) {
        setItem(*updated);
      }
      emit historyRestored(m_currentItem.id);
      dlg.accept();
    }
  });
  btnRow->addWidget(restoreBtn);
  vl->addLayout(btnRow);
  dlg.exec();
}

void DetailInspectorView::changeEvent(QEvent *event) {
  if (event->type() == QEvent::LanguageChange) {
    retranslateUi();
  }
  QWidget::changeEvent(event);
}

} // namespace svm::views
