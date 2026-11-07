/**
 * @file ui/views/settings/tabs/tab_data.cpp
 * @brief Hiện thực giao diện Quản lý Dữ liệu, Nhập/Xuất & Dọn dẹp Thùng rác.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "tab_data.h"
#include "app/settings_manager.h"
#include "core/core_bridge.h"

#include <QFileDialog>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QStandardPaths>
#include <QVBoxLayout>

namespace svm::views {

TabData::TabData(QWidget *parent) : QWidget(parent) {
  setAttribute(Qt::WA_StyledBackground, true);
  setupUI();
}

void TabData::setupUI() {
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(16);

  auto &settings = app::SettingsManager::instance();

  // 1. Import Card
  m_importCard = new SettingSectionCard(tr("Import Data"), this);

  auto *importControl = new QWidget(m_importCard);
  auto *importLayout = new QHBoxLayout(importControl);
  importLayout->setContentsMargins(0, 0, 0, 0);
  importLayout->setSpacing(8);

  m_importFormatCombo = new QComboBox(importControl);
  m_importFormatCombo->addItem(QStringLiteral("Bitwarden (JSON)"));
  m_importFormatCombo->addItem(QStringLiteral("1Password (1PUX)"));
  m_importFormatCombo->addItem(QStringLiteral("KeePass (XML)"));
  m_importFormatCombo->addItem(QStringLiteral("CSV Spreadsheet"));
  m_importFormatCombo->setFixedWidth(160);
  importLayout->addWidget(m_importFormatCombo);

  m_importBtn = new QPushButton(tr("Select File & Import..."), importControl);
  m_importBtn->setObjectName(QStringLiteral("OnboardingPrimaryBtn"));
  m_importBtn->setFixedHeight(32);
  m_importBtn->setCursor(Qt::PointingHandCursor);
  connect(m_importBtn, &QPushButton::clicked, this, &TabData::onImportClicked);
  importLayout->addWidget(m_importBtn);

  m_importRow =
      new SettingRowWidget(tr("Import from Password Managers"),
                           tr("Import logins, secure notes, credit cards, and "
                              "SSH keys from other applications."),
                           importControl, m_importCard);
  m_importCard->addRow(m_importRow);

  layout->addWidget(m_importCard);

  // 2. Export Card
  m_exportCard = new SettingSectionCard(tr("Export & Backup"), this);

  m_exportEncryptedBtn =
      new QPushButton(tr("Export Encrypted Backup..."), m_exportCard);
  m_exportEncryptedBtn->setFixedHeight(32);
  m_exportEncryptedBtn->setCursor(Qt::PointingHandCursor);
  connect(m_exportEncryptedBtn, &QPushButton::clicked, this,
          &TabData::onExportEncryptedClicked);
  m_exportEncryptedRow = new SettingRowWidget(
      tr("Encrypted Backup (Recommended)"),
      tr("Export your vault encrypted with your master key. Safe to "
         "store on external drives or cloud backups."),
      m_exportEncryptedBtn, m_exportCard);
  m_exportCard->addRow(m_exportEncryptedRow);

  m_exportPlainBtn = new QPushButton(tr("Export Plaintext..."), m_exportCard);
  m_exportPlainBtn->setObjectName(QStringLiteral("DangerButton"));
  m_exportPlainBtn->setFixedHeight(32);
  m_exportPlainBtn->setCursor(Qt::PointingHandCursor);
  connect(m_exportPlainBtn, &QPushButton::clicked, this,
          &TabData::onExportPlainClicked);
  m_exportPlainRow = new SettingRowWidget(
      tr("Unencrypted Export"),
      tr("Export plaintext CSV/JSON. Warning: Anyone who gains access to this "
         "file can read your secrets."),
      m_exportPlainBtn, m_exportCard);
  m_exportCard->addRow(m_exportPlainRow);

  layout->addWidget(m_exportCard);

  // 3. Trash Card
  m_trashCard = new SettingSectionCard(tr("Trash Maintenance"), this);

  m_trashAutoEmptyCheck = new QCheckBox(m_trashCard);
  m_trashAutoEmptyCheck->setChecked(settings.autoEmptyTrashDaysEnabled());
  connect(m_trashAutoEmptyCheck, &QCheckBox::toggled, this, [](bool checked) {
    app::SettingsManager::instance().setAutoEmptyTrashDaysEnabled(checked);
  });
  m_trashAutoEmptyRow =
      new SettingRowWidget(tr("Auto-Purge Trash"),
                           tr("Automatically delete items that have been in "
                              "the trash for longer than 30 days."),
                           m_trashAutoEmptyCheck, m_trashCard);
  m_trashCard->addRow(m_trashAutoEmptyRow);

  m_emptyTrashNowBtn = new QPushButton(tr("Empty Trash Now"), m_trashCard);
  m_emptyTrashNowBtn->setFixedHeight(30);
  m_emptyTrashNowBtn->setCursor(Qt::PointingHandCursor);
  connect(m_emptyTrashNowBtn, &QPushButton::clicked, this, [this]() {
    core::CoreBridge::instance().emptyTrash();
    QMessageBox::information(
        this, tr("Trash Purged"),
        tr("All deleted items in trash have been purged."));
  });
  m_emptyTrashNowRow = new SettingRowWidget(
      tr("Manual Trash Clean"),
      tr("Immediately empty all discarded credentials across your vaults."),
      m_emptyTrashNowBtn, m_trashCard);
  m_trashCard->addRow(m_emptyTrashNowRow);

  layout->addWidget(m_trashCard);
  layout->addStretch(1);
}

void TabData::onImportClicked() {
  const QString filePath = QFileDialog::getOpenFileName(
      this, tr("Select Import File"),
      QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
      tr("All Supported Formats (*.json *.1pux *.xml *.csv);;JSON Files "
         "(*.json);;CSV Files (*.csv);;All Files (*)"));
  if (!filePath.isEmpty()) {
    int fmt = SVM_FORMAT_BITWARDEN_JSON;
    const int idx =
        m_importFormatCombo ? m_importFormatCombo->currentIndex() : 0;
    if (idx == 1 ||
        filePath.endsWith(QStringLiteral(".1pux"), Qt::CaseInsensitive)) {
      fmt = SVM_FORMAT_1PUX;
    } else if (idx == 2 ||
               filePath.endsWith(QStringLiteral(".xml"), Qt::CaseInsensitive)) {
      fmt = SVM_FORMAT_KEEPASS_XML;
    } else if (idx == 3 ||
               filePath.endsWith(QStringLiteral(".csv"), Qt::CaseInsensitive)) {
      fmt = SVM_FORMAT_CSV;
    }

    const int importedCount =
        core::CoreBridge::instance().importItems(fmt, filePath);
    if (importedCount >= 0) {
      QMessageBox::information(
          this, tr("Import Complete"),
          tr("Successfully parsed import file: %1\nImported "
             "%2 items into your vault.")
              .arg(filePath)
              .arg(importedCount));
    } else {
      QMessageBox::warning(
          this, tr("Import Failed"),
          core::CoreBridge::instance().lastError().isEmpty()
              ? tr("Could not parse or import the selected file.")
              : core::CoreBridge::instance().lastError());
    }
  }
}

void TabData::onExportEncryptedClicked() {
  const QString filePath = QFileDialog::getSaveFileName(
      this, tr("Save Encrypted Vault Backup"),
      QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) +
          QStringLiteral("/svm_backup.svm"),
      tr("Secure Vault Backup (*.svm);;All Files (*)"));
  if (!filePath.isEmpty()) {
    if (core::CoreBridge::instance().exportItems(SVM_FORMAT_SVM, filePath)) {
      QMessageBox::information(
          this, tr("Export Complete"),
          tr("Vault backup successfully encrypted and saved."));
    } else {
      QMessageBox::warning(this, tr("Export Failed"),
                           core::CoreBridge::instance().lastError());
    }
  }
}

void TabData::onExportPlainClicked() {
  auto res =
      QMessageBox::warning(this, tr("Security Risk Warning"),
                           tr("You are about to export your entire password "
                              "vault in PLAINTEXT format.\n"
                              "Anyone who accesses this file will be able to "
                              "read your passwords, cards, and keys.\n\n"
                              "Are you sure you want to proceed?"),
                           QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

  if (res == QMessageBox::Yes) {
    const QString filePath = QFileDialog::getSaveFileName(
        this, tr("Save Plaintext Export"),
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) +
            QStringLiteral("/svm_export.csv"),
        tr("CSV Files (*.csv);;JSON Files (*.json);;All Files (*)"));
    if (!filePath.isEmpty()) {
      const int fmt =
          filePath.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive)
              ? SVM_FORMAT_BITWARDEN_JSON
              : SVM_FORMAT_CSV;
      if (core::CoreBridge::instance().exportItems(fmt, filePath)) {
        QMessageBox::information(this, tr("Export Complete"),
                                 tr("Plaintext file exported successfully."));
      } else {
        QMessageBox::warning(this, tr("Export Failed"),
                             core::CoreBridge::instance().lastError());
      }
    }
  }
}

void TabData::retranslateUi() {
  if (m_importCard) {
    m_importCard->setTitle(tr("Import Data"));
  }
  if (m_importBtn) {
    m_importBtn->setText(tr("Select File & Import..."));
  }
  if (m_importRow) {
    m_importRow->updateText(tr("Import from Password Managers"),
                            tr("Import logins, secure notes, credit cards, and "
                               "SSH keys from other applications."));
  }

  if (m_exportCard) {
    m_exportCard->setTitle(tr("Export & Backup"));
  }
  if (m_exportEncryptedBtn) {
    m_exportEncryptedBtn->setText(tr("Export Encrypted Backup..."));
  }
  if (m_exportEncryptedRow) {
    m_exportEncryptedRow->updateText(
        tr("Encrypted Backup (Recommended)"),
        tr("Export your vault encrypted with your master key. Safe to "
           "store on external drives or cloud backups."));
  }
  if (m_exportPlainBtn) {
    m_exportPlainBtn->setText(tr("Export Plaintext..."));
  }
  if (m_exportPlainRow) {
    m_exportPlainRow->updateText(
        tr("Unencrypted Export"),
        tr("Export plaintext CSV/JSON. Warning: Anyone who gains access to "
           "this file can read your secrets."));
  }

  if (m_trashCard) {
    m_trashCard->setTitle(tr("Trash Maintenance"));
  }
  if (m_trashAutoEmptyRow) {
    m_trashAutoEmptyRow->updateText(
        tr("Auto-Purge Trash"), tr("Automatically delete items that have been "
                                   "in the trash for longer than 30 days."));
  }
  if (m_emptyTrashNowBtn) {
    m_emptyTrashNowBtn->setText(tr("Empty Trash Now"));
  }
  if (m_emptyTrashNowRow) {
    m_emptyTrashNowRow->updateText(
        tr("Manual Trash Clean"),
        tr("Immediately empty all discarded credentials across your vaults."));
  }
}

} // namespace svm::views
