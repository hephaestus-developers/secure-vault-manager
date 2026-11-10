/**
 * @file ui/views/settings/tabs/tab_data.cpp
 * @brief Hiện thực giao diện Quản lý Dữ liệu, Nhập/Xuất & Dọn dẹp Thùng rác.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "tab_data.h"
#include "app/settings_manager.h"
#include "app/theme_manager.h"
#include "app/vault_manager.h"
#include "components/dialog_drag_helper.h"
#include "components/in_window_combobox.h"
#include "components/notification_dialog.h"
#include "components/window_resize_handler.h"
#include "core/core_bridge.h"

#include <QFileDialog>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QStandardPaths>
#include <QVBoxLayout>

namespace svm::views {

TabData::TabData(QWidget *parent) : QWidget(parent) {
  setAttribute(Qt::WA_StyledBackground, true);
  setupUI();
}

void TabData::setupUI() {
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(18, 14, 14, 16);
  layout->setSpacing(16);

  auto &settings = app::SettingsManager::instance();

  // 1. Import Card
  m_importCard = new SettingSectionCard(tr("Import Data"), this);

  auto *importControl = new QWidget(m_importCard);
  auto *importLayout = new QHBoxLayout(importControl);
  importLayout->setContentsMargins(0, 0, 0, 0);
  importLayout->setSpacing(8);

  m_importFormatCombo = new svm::ui::InWindowComboBox(importControl);
  m_importFormatCombo->addItem(QStringLiteral("Bitwarden (JSON)"));
  m_importFormatCombo->addItem(QStringLiteral("1Password (1PUX)"));
  m_importFormatCombo->addItem(QStringLiteral("KeePass (XML)"));
  m_importFormatCombo->addItem(QStringLiteral("CSV Spreadsheet"));
  m_importFormatCombo->setFixedWidth(200);
  m_importFormatCombo->setFixedHeight(32);
  m_importFormatCombo->setCursor(Qt::PointingHandCursor);
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
    if (core::CoreBridge::instance().emptyTrash()) {
      components::NotificationDialog::information(
          this, tr("Trash Purged"),
          tr("All deleted items in trash have been purged."));
    } else {
      components::NotificationDialog::warning(
          this, tr("Trash Clean Failed"),
          core::CoreBridge::instance().lastError().isEmpty()
              ? tr("Unable to empty trash while vault is locked.")
              : core::CoreBridge::instance().lastError());
    }
  });
  m_emptyTrashNowRow = new SettingRowWidget(
      tr("Manual Trash Clean"),
      tr("Immediately empty all discarded credentials across your vaults."),
      m_emptyTrashNowBtn, m_trashCard);
  m_trashCard->addRow(m_emptyTrashNowRow);

  layout->addWidget(m_trashCard);

  // 4. Danger Zone - Factory Reset
  m_dangerCard = new SettingSectionCard(tr("Danger Zone"), this);

  m_factoryResetBtn = new QPushButton(tr("Factory Reset..."), m_dangerCard);
  m_factoryResetBtn->setObjectName(QStringLiteral("DangerButton"));
  m_factoryResetBtn->setFixedHeight(32);
  m_factoryResetBtn->setCursor(Qt::PointingHandCursor);
  connect(m_factoryResetBtn, &QPushButton::clicked, this,
          &TabData::onFactoryResetClicked);

  m_factoryResetRow = new SettingRowWidget(
      tr("Reset All Data & Return to Setup"),
      tr("Permanently erase all vaults, secrets, and master passwords. "
         "Restores the application to its initial onboarding state."),
      m_factoryResetBtn, m_dangerCard);
  m_dangerCard->addRow(m_factoryResetRow);

  layout->addWidget(m_dangerCard);
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

    QString password;
    if (filePath.endsWith(QStringLiteral(".svm"), Qt::CaseInsensitive)) {
      bool ok = false;
      password = QInputDialog::getText(
          this, tr("Encrypted Backup Password"),
          tr("Enter password to decrypt this .svm backup:"),
          QLineEdit::Password, QString(), &ok);
      if (!ok || password.isEmpty()) {
        return;
      }
    }

    const int importedCount =
        core::CoreBridge::instance().importItems(fmt, filePath, password);
    if (importedCount >= 0) {
      components::NotificationDialog::success(
          this, tr("Import Complete"),
          tr("Successfully parsed import file: %1\nImported "
             "%2 items into your vault.")
              .arg(filePath)
              .arg(importedCount));
    } else {
      components::NotificationDialog::warning(
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
    bool ok = false;
    const QString password = QInputDialog::getText(
        this, tr("Backup Encryption Password"),
        tr("Enter a password to encrypt this .svm backup:"),
        QLineEdit::Password, QString(), &ok);
    if (!ok || password.isEmpty()) {
      return;
    }
    const QString confirmPw =
        QInputDialog::getText(this, tr("Confirm Backup Password"),
                              tr("Re-enter the backup password to confirm:"),
                              QLineEdit::Password, QString(), &ok);
    if (!ok || confirmPw != password) {
      components::NotificationDialog::warning(
          this, tr("Export Failed"), tr("Backup passwords do not match."));
      return;
    }

    if (core::CoreBridge::instance().exportItems(SVM_FORMAT_SVM, filePath,
                                                 password)) {
      components::NotificationDialog::success(
          this, tr("Export Complete"),
          tr("Vault backup successfully encrypted and saved."));
    } else {
      components::NotificationDialog::warning(
          this, tr("Export Failed"), core::CoreBridge::instance().lastError());
    }
  }
}

void TabData::onExportPlainClicked() {
  const bool confirmed = components::NotificationDialog::question(
      this, tr("Security Risk Warning"),
      tr("You are about to export your entire password "
         "vault in PLAINTEXT format.\n"
         "Anyone who accesses this file will be able to "
         "read your passwords, cards, and keys.\n\n"
         "Are you sure you want to proceed?"),
      tr("Export Anyway"), tr("Cancel"), true);

  if (confirmed) {
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
        components::NotificationDialog::success(
            this, tr("Export Complete"),
            tr("Plaintext file exported successfully."));
      } else {
        components::NotificationDialog::warning(
            this, tr("Export Failed"),
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

  if (m_dangerCard) {
    m_dangerCard->setTitle(tr("Danger Zone"));
  }
  if (m_factoryResetBtn) {
    m_factoryResetBtn->setText(tr("Factory Reset..."));
  }
  if (m_factoryResetRow) {
    m_factoryResetRow->updateText(
        tr("Reset All Data & Return to Setup"),
        tr("Permanently erase all vaults, secrets, and master passwords. "
           "Restores the application to its initial onboarding state."));
  }
}

void TabData::onFactoryResetClicked() {
  QDialog dlg(this, Qt::FramelessWindowHint | Qt::Dialog);
  dlg.setAttribute(Qt::WA_TranslucentBackground, false);
  dlg.setModal(false);
  dlg.setWindowModality(Qt::NonModal);
  dlg.setMinimumSize(420, 260);
  dlg.resize(480, 290);

  auto *rootLayout = new QVBoxLayout(&dlg);
  rootLayout->setContentsMargins(0, 0, 0, 0);

  auto *card = new QWidget(&dlg);
  card->setObjectName(QStringLiteral("NewVaultCard"));
  card->setAttribute(Qt::WA_StyledBackground, true);

  new components::WindowResizeHandler(&dlg);

  auto *cardLayout = new QVBoxLayout(card);
  cardLayout->setContentsMargins(0, 0, 0, 16);
  cardLayout->setSpacing(0);

  auto *header = new QWidget(card);
  header->setObjectName(QStringLiteral("NewVaultHeader"));
  header->setAttribute(Qt::WA_StyledBackground, true);
  header->setFixedHeight(38);
  auto *headerLayout = new QHBoxLayout(header);
  headerLayout->setContentsMargins(16, 0, 0, 0);
  headerLayout->setSpacing(10);

  auto *headerIcon = new QLabel(header);
  headerIcon->setFixedSize(18, 18);
  headerIcon->setPixmap(app::ThemeManager::instance()
                            .icon(QStringLiteral("trash"))
                            .pixmap(18, 18));
  headerLayout->addWidget(headerIcon);

  auto *title = new QLabel(tr("Factory Reset Secure Vault Manager?"), header);
  title->setObjectName(QStringLiteral("NewVaultTitle"));
  headerLayout->addWidget(title, 1);

  auto *closeBtn = new QPushButton(header);
  closeBtn->setObjectName(QStringLiteral("WinCloseBtn"));
  closeBtn->setFixedSize(46, 38);
  closeBtn->setIconSize(QSize(12, 12));
  closeBtn->setFlat(true);
  closeBtn->setFocusPolicy(Qt::NoFocus);
  closeBtn->setCursor(Qt::PointingHandCursor);
  closeBtn->setIcon(
      app::ThemeManager::instance().icon(QStringLiteral("win_close")));
  connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
  headerLayout->addWidget(closeBtn);

  cardLayout->addWidget(header);

  auto *body = new QWidget(card);
  auto *layout = new QVBoxLayout(body);
  layout->setContentsMargins(20, 16, 20, 0);
  layout->setSpacing(12);

  auto *warn = new QLabel(
      tr("This action will PERMANENTLY ERASE all vaults, credentials, and "
         "master keys. "
         "Secure Vault Manager will return to the Welcome & Setup wizard.\n\n"
         "Type \"RESET\" below to confirm:"),
      body);
  warn->setObjectName(QStringLiteral("DialogDescLabel"));
  warn->setWordWrap(true);
  layout->addWidget(warn);

  auto *confirmEdit = new QLineEdit(body);
  confirmEdit->setPlaceholderText(QStringLiteral("RESET"));
  confirmEdit->setFixedHeight(34);
  layout->addWidget(confirmEdit);

  layout->addStretch(1);

  auto *btnLayout = new QHBoxLayout();
  auto *cancelBtn = new QPushButton(tr("Cancel"), body);
  cancelBtn->setFixedHeight(32);
  cancelBtn->setCursor(Qt::PointingHandCursor);
  connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
  btnLayout->addWidget(cancelBtn);
  btnLayout->addStretch(1);

  auto *confirmBtn = new QPushButton(tr("Erase All Data & Reset"), body);
  confirmBtn->setObjectName(QStringLiteral("DangerButton"));
  confirmBtn->setFixedHeight(32);
  confirmBtn->setCursor(Qt::PointingHandCursor);
  confirmBtn->setEnabled(false);
  connect(confirmEdit, &QLineEdit::textChanged, &dlg,
          [confirmBtn](const QString &t) {
            confirmBtn->setEnabled(t.trimmed().toUpper() ==
                                   QStringLiteral("RESET"));
          });
  connect(confirmBtn, &QPushButton::clicked, &dlg, [&]() {
    dlg.accept();
    app::VaultManager::instance().resetAllData();
  });
  btnLayout->addWidget(confirmBtn);
  layout->addLayout(btnLayout);

  cardLayout->addWidget(body, 1);
  components::DialogDragHelper::makeDraggable(header, &dlg);
  rootLayout->addWidget(card);

  components::DialogDragHelper::execDialog(dlg);
}

} // namespace svm::views
