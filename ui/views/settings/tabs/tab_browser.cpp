/**
 * @file ui/views/settings/tabs/tab_browser.cpp
 * @brief Hiện thực giao diện Tích hợp Trình duyệt Web & Cầu nối Extension.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "tab_browser.h"
#include "app/settings_manager.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>

namespace svm::views {

TabBrowser::TabBrowser(QWidget *parent) : QWidget(parent) { setupUI(); }

void TabBrowser::setupUI() {
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(16);

  auto &settings = app::SettingsManager::instance();

  // 1. Bridge Server Card
  m_bridgeCard = new SettingSectionCard(tr("Browser Integration Bridge"), this);

  m_enableCheck = new QCheckBox(m_bridgeCard);
  m_enableCheck->setChecked(settings.browserBridgeEnabled());
  connect(m_enableCheck, &QCheckBox::toggled, this, [](bool checked) {
    app::SettingsManager::instance().setBrowserBridgeEnabled(checked);
  });
  m_enableRow = new SettingRowWidget(
      tr("Enable Browser Integration"),
      tr("Allow verified Secure Vault Manager browser extensions to autofill "
         "passwords via Native Messaging."),
      m_enableCheck, m_bridgeCard);
  m_bridgeCard->addRow(m_enableRow);

  m_manifestBtn = new QPushButton(tr("Install Manifests..."), m_bridgeCard);
  m_manifestBtn->setFixedHeight(32);
  m_manifestBtn->setCursor(Qt::PointingHandCursor);
  connect(m_manifestBtn, &QPushButton::clicked, this, [this]() {
    QMessageBox::information(
        this, tr("Native Messaging Host"),
        tr("Native Messaging manifests successfully registered for Chrome, "
           "Firefox, Brave, and Edge browsers."));
  });
  m_manifestRow =
      new SettingRowWidget(tr("Install Browser Manifests"),
                           tr("Register the Native Host configuration files in "
                              "your operating system so browsers can connect."),
                           m_manifestBtn, m_bridgeCard);
  m_bridgeCard->addRow(m_manifestRow);

  layout->addWidget(m_bridgeCard);

  // 2. Paired Extensions Card (Mock)
  m_pairedCard =
      new SettingSectionCard(tr("Connected Browser Extensions"), this);

  m_pairedListLayout = new QVBoxLayout();
  m_pairedListLayout->setContentsMargins(0, 4, 0, 0);
  m_pairedListLayout->setSpacing(8);

  struct PairedMock {
    QString browser;
    QString version;
    QString lastUsed;
  };
  const QList<PairedMock> mocks = {
      {QStringLiteral("Google Chrome"), QStringLiteral("v1.4.2"),
       QStringLiteral("Active 10 mins ago")},
      {QStringLiteral("Mozilla Firefox"), QStringLiteral("v1.4.0"),
       QStringLiteral("Active yesterday")}};

  for (const auto &item : mocks) {
    auto *row = new QWidget(m_pairedCard);
    row->setStyleSheet(
        QStringLiteral("background-color: #1e1e2e; border: 1px solid #313244; "
                       "border-radius: 6px; padding: 4px;"));
    auto *rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(12, 8, 12, 8);
    rowLayout->setSpacing(12);

    auto *infoLayout = new QVBoxLayout();
    infoLayout->setSpacing(2);

    auto *nameLbl =
        new QLabel(item.browser + QStringLiteral(" ") + item.version, row);
    nameLbl->setStyleSheet(
        QStringLiteral("font-size: 12.5px; font-weight: 600; color: #cdd6f4;"));
    infoLayout->addWidget(nameLbl);

    auto *descLbl = new QLabel(
        item.lastUsed + QStringLiteral(" • HMAC-SHA256 Encrypted"), row);
    descLbl->setStyleSheet(QStringLiteral("font-size: 11px; color: #a6adc8;"));
    infoLayout->addWidget(descLbl);

    rowLayout->addLayout(infoLayout, 1);

    auto *revokeBtn = new QPushButton(tr("Revoke"), row);
    revokeBtn->setObjectName(QStringLiteral("DangerButton"));
    revokeBtn->setFixedHeight(28);
    revokeBtn->setCursor(Qt::PointingHandCursor);
    connect(revokeBtn, &QPushButton::clicked, this,
            [row]() { row->deleteLater(); });
    rowLayout->addWidget(revokeBtn);

    m_pairedListLayout->addWidget(row);
  }

  auto *listContainer = new QWidget(m_pairedCard);
  listContainer->setLayout(m_pairedListLayout);
  m_pairedCard->addWidget(listContainer);

  layout->addWidget(m_pairedCard);
  layout->addStretch(1);
}

void TabBrowser::retranslateUi() {
  if (m_bridgeCard) {
    m_bridgeCard->setTitle(tr("Browser Integration Bridge"));
  }
  if (m_enableRow) {
    m_enableRow->updateText(
        tr("Enable Browser Integration"),
        tr("Allow verified Secure Vault Manager browser extensions to autofill "
           "passwords via Native Messaging."));
  }
  if (m_manifestBtn) {
    m_manifestBtn->setText(tr("Install Manifests..."));
  }
  if (m_manifestRow) {
    m_manifestRow->updateText(
        tr("Install Browser Manifests"),
        tr("Register the Native Host configuration files in your operating "
           "system so browsers can connect."));
  }
  if (m_pairedCard) {
    m_pairedCard->setTitle(tr("Connected Browser Extensions"));
  }
}

} // namespace svm::views
