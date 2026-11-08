/**
 * @file ui/views/settings/tabs/tab_browser.cpp
 * @brief Hiện thực giao diện Tích hợp Trình duyệt Web & Cầu nối Extension.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "tab_browser.h"
#include "app/settings_manager.h"
#include "components/notification_dialog.h"
#include "core/core_bridge.h"
#include "views/settings/pairing_dialog.h"

#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>

namespace svm::views {

TabBrowser::TabBrowser(QWidget *parent) : QWidget(parent) {
  setAttribute(Qt::WA_StyledBackground, true);
  setupUI();

  connect(&core::CoreBridge::instance(),
          &core::CoreBridge::bridgeClientsChangedEvent, this,
          &TabBrowser::refreshClientsList);
}

void TabBrowser::setupUI() {
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(18, 14, 14, 16);
  layout->setSpacing(16);

  auto &settings = app::SettingsManager::instance();

  // 1. Cầu nối Trình duyệt Web (Bridge Server Card)
  m_bridgeCard = new SettingSectionCard(tr("Cầu nối Trình duyệt Web"), this);

  m_enableCheck = new QCheckBox(m_bridgeCard);
  m_enableCheck->setChecked(settings.browserBridgeEnabled());
  if (settings.browserBridgeEnabled()) {
    core::CoreBridge::instance().startBridge();
  }
  connect(m_enableCheck, &QCheckBox::toggled, this, [](bool checked) {
    app::SettingsManager::instance().setBrowserBridgeEnabled(checked);
    if (checked) {
      core::CoreBridge::instance().startBridge();
    } else {
      core::CoreBridge::instance().stopBridge();
    }
  });
  m_enableRow = new SettingRowWidget(
      tr("Bật Tích hợp Trình duyệt"),
      tr("Cho phép tiện ích trình duyệt Secure Vault Manager đã xác thực "
         "tự động điền mật khẩu qua Native Messaging."),
      m_enableCheck, m_bridgeCard);
  m_bridgeCard->addRow(m_enableRow);

  // Nút Ghép nối tiện ích mới
  m_pairBtn = new QPushButton(tr("Ghép nối Tiện ích mới..."), m_bridgeCard);
  m_pairBtn->setFixedHeight(32);
  m_pairBtn->setCursor(Qt::PointingHandCursor);
  connect(m_pairBtn, &QPushButton::clicked, this, &TabBrowser::onPairClicked);
  m_pairRow = new SettingRowWidget(tr("Ghép nối Tiện ích Mở rộng"),
                                   tr("Sinh mã PIN 6 số an toàn để xác thực và "
                                      "cấp quyền cho tiện ích trình duyệt."),
                                   m_pairBtn, m_bridgeCard);
  m_bridgeCard->addRow(m_pairRow);

  // Nút Cài đặt Manifests
  m_manifestBtn = new QPushButton(tr("Cài đặt Manifests..."), m_bridgeCard);
  m_manifestBtn->setFixedHeight(32);
  m_manifestBtn->setCursor(Qt::PointingHandCursor);
  connect(m_manifestBtn, &QPushButton::clicked, this,
          &TabBrowser::onInstallManifestsClicked);
  m_manifestRow =
      new SettingRowWidget(tr("Cài đặt Cấu hình Native Host"),
                           tr("Đăng ký tệp cấu hình Native Host vào các trình "
                              "duyệt trên hệ điều hành "
                              "(Chrome, Firefox, Brave, Edge, Chromium)."),
                           m_manifestBtn, m_bridgeCard);
  m_bridgeCard->addRow(m_manifestRow);

  layout->addWidget(m_bridgeCard);

  // 2. Tiện ích Trình duyệt Đã kết nối (Paired Extensions Card)
  m_pairedCard =
      new SettingSectionCard(tr("Tiện ích Trình duyệt Đã kết nối"), this);

  m_pairedListLayout = new QVBoxLayout();
  m_pairedListLayout->setContentsMargins(0, 4, 0, 0);
  m_pairedListLayout->setSpacing(8);

  auto *listContainer = new QWidget(m_pairedCard);
  listContainer->setLayout(m_pairedListLayout);
  m_pairedCard->addWidget(listContainer);

  layout->addWidget(m_pairedCard);
  layout->addStretch(1);

  refreshClientsList();
}

void TabBrowser::refreshClientsList() {
  if (!m_pairedListLayout) {
    return;
  }

  // Xóa sạch widget hiện tại trong list
  QLayoutItem *child = nullptr;
  while ((child = m_pairedListLayout->takeAt(0)) != nullptr) {
    if (child->widget()) {
      child->widget()->deleteLater();
    }
    delete child;
  }

  const QJsonArray coreClients =
      core::CoreBridge::instance().listBridgeClients();

  if (coreClients.isEmpty()) {
    // Hiển thị Empty State
    auto *emptyCard = new QWidget(m_pairedCard);
    emptyCard->setObjectName(QStringLiteral("BrowserEmptyCard"));
    emptyCard->setAttribute(Qt::WA_StyledBackground, true);

    auto *emptyLayout = new QVBoxLayout(emptyCard);
    emptyLayout->setContentsMargins(24, 20, 24, 20);
    emptyLayout->setSpacing(8);

    auto *titleLbl = new QLabel(tr("Chưa có tiện ích nào kết nối"), emptyCard);
    titleLbl->setObjectName(QStringLiteral("BrowserEmptyTitle"));
    titleLbl->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(titleLbl);

    auto *descLbl = new QLabel(
        tr("Cài đặt tiện ích mở rộng Secure Vault Manager trên trình duyệt "
           "và nhấn \"Ghép nối Tiện ích mới\" để tự động điền mật khẩu an "
           "toàn."),
        emptyCard);
    descLbl->setObjectName(QStringLiteral("BrowserEmptyDesc"));
    descLbl->setAlignment(Qt::AlignCenter);
    descLbl->setWordWrap(true);
    descLbl->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    emptyLayout->addWidget(descLbl);

    m_pairedListLayout->addWidget(emptyCard);
    return;
  }

  // Hiển thị danh sách tiện ích thực tế
  for (const auto &val : coreClients) {
    if (!val.isObject()) {
      continue;
    }
    const QJsonObject obj = val.toObject();
    const QString id = obj.value(QStringLiteral("id")).toString();
    const QString browser = obj.value(QStringLiteral("browser")).toString();
    const QString version = obj.value(QStringLiteral("version")).toString();
    const QString lastUsed = obj.value(QStringLiteral("lastUsed")).toString();

    auto *row = new QWidget(m_pairedCard);
    row->setObjectName(QStringLiteral("SettingsListItemCard"));
    row->setAttribute(Qt::WA_StyledBackground, true);
    auto *rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(12, 8, 12, 8);
    rowLayout->setSpacing(12);

    auto *infoLayout = new QVBoxLayout();
    infoLayout->setSpacing(2);

    auto *nameLbl = new QLabel(browser + QStringLiteral(" ") + version, row);
    nameLbl->setObjectName(QStringLiteral("SettingsListItemTitle"));
    infoLayout->addWidget(nameLbl);

    auto *descLbl = new QLabel(
        lastUsed + QStringLiteral(" • ") + tr("Tiện ích đã xác thực"), row);
    descLbl->setObjectName(QStringLiteral("SettingsListItemDesc"));
    infoLayout->addWidget(descLbl);

    rowLayout->addLayout(infoLayout, 1);

    auto *revokeBtn = new QPushButton(tr("Thu hồi"), row);
    revokeBtn->setObjectName(QStringLiteral("DangerButton"));
    revokeBtn->setFixedHeight(28);
    revokeBtn->setCursor(Qt::PointingHandCursor);
    connect(revokeBtn, &QPushButton::clicked, this, [this, id]() {
      core::CoreBridge::instance().revokeBridgeClient(id);
      components::NotificationDialog::information(
          this, tr("Thu hồi Tiện ích"),
          tr("Đã thu hồi quyền kết nối của tiện ích trình duyệt thành công."));
      refreshClientsList();
    });
    rowLayout->addWidget(revokeBtn);

    m_pairedListLayout->addWidget(row);
  }
}

void TabBrowser::onPairClicked() {
  if (PairingDialog::isPairingDialogOpen()) {
    return;
  }

  // Gọi Core để bắt đầu ghép nối và sinh mã PIN.
  // CoreBridge sẽ tự động phát sự kiện bridgePairingRequestedEvent,
  // từ đó MainWindow sẽ hiển thị duy nhất 1 PairingDialog tập trung.
  const QString rawJson = core::CoreBridge::instance().beginBridgePairing();
  if (rawJson.isEmpty()) {
    components::NotificationDialog::warning(
        this, tr("Lỗi Ghép nối"),
        tr("Không thể khởi tạo mã PIN ghép nối. Vui lòng kiểm tra lại cầu "
           "nối."));
  }
}

void TabBrowser::onInstallManifestsClicked() {
  const bool ok = core::CoreBridge::instance().installNativeHostManifests();
  if (ok) {
    components::NotificationDialog::information(
        this, tr("Cài đặt Manifests Thành công"),
        tr("Đã đăng ký tệp cấu hình Native Messaging Host cho Google Chrome, "
           "Mozilla Firefox, Brave, Microsoft Edge và Chromium vào hệ điều "
           "hành."));
  } else {
    components::NotificationDialog::warning(
        this, tr("Lỗi Cài đặt"),
        tr("Không thể đăng ký tệp cấu hình Native Messaging Host."));
  }
}

void TabBrowser::retranslateUi() {
  if (m_bridgeCard) {
    m_bridgeCard->setTitle(tr("Cầu nối Trình duyệt Web"));
  }
  if (m_enableRow) {
    m_enableRow->updateText(
        tr("Bật Tích hợp Trình duyệt"),
        tr("Cho phép tiện ích trình duyệt Secure Vault Manager đã xác thực "
           "tự động điền mật khẩu qua Native Messaging."));
  }
  if (m_pairBtn) {
    m_pairBtn->setText(tr("Ghép nối Tiện ích mới..."));
  }
  if (m_pairRow) {
    m_pairRow->updateText(tr("Ghép nối Tiện ích Mở rộng"),
                          tr("Sinh mã PIN 6 số an toàn để xác thực và cấp "
                             "quyền cho tiện ích trình duyệt."));
  }
  if (m_manifestBtn) {
    m_manifestBtn->setText(tr("Cài đặt Manifests..."));
  }
  if (m_manifestRow) {
    m_manifestRow->updateText(tr("Cài đặt Cấu hình Native Host"),
                              tr("Đăng ký tệp cấu hình Native Host vào các "
                                 "trình duyệt trên hệ điều hành "
                                 "(Chrome, Firefox, Brave, Edge, Chromium)."));
  }
  if (m_pairedCard) {
    m_pairedCard->setTitle(tr("Tiện ích Trình duyệt Đã kết nối"));
  }
  refreshClientsList();
}

} // namespace svm::views
