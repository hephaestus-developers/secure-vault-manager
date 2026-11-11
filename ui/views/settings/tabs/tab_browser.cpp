/**
 * @file ui/views/settings/tabs/tab_browser.cpp
 * @brief Hiện thực giao diện Tích hợp Trình duyệt Web & Trình duyệt phát hiện
 * trên hệ thống.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "tab_browser.h"
#include "app/settings_manager.h"
#include "core/core_bridge.h"

#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QStandardPaths>
#include <QUrl>

namespace svm::views {

TabBrowser::TabBrowser(QWidget *parent) : QWidget(parent) {
  setAttribute(Qt::WA_StyledBackground, true);
  setupUI();

  connect(&core::CoreBridge::instance(),
          &core::CoreBridge::bridgeClientsChangedEvent, this,
          &TabBrowser::refreshSupportedBrowsers);
}

void TabBrowser::setupUI() {
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(18, 14, 14, 16);
  layout->setSpacing(16);

  auto &settings = app::SettingsManager::instance();

  // 1. Thẻ Tích hợp Trình duyệt Web (Master Switch)
  m_bridgeCard = new SettingSectionCard(tr("Web Browser Integration"), this);

  m_enableCheck = new QCheckBox(m_bridgeCard);
  m_enableCheck->setChecked(settings.browserBridgeEnabled());
  if (settings.browserBridgeEnabled()) {
    core::CoreBridge::instance().startBridge();
    core::CoreBridge::instance().installNativeHostManifests();
  }

  connect(m_enableCheck, &QCheckBox::toggled, this, [this](bool checked) {
    app::SettingsManager::instance().setBrowserBridgeEnabled(checked);
    if (checked) {
      core::CoreBridge::instance().startBridge();
      core::CoreBridge::instance().installNativeHostManifests();
    } else {
      core::CoreBridge::instance().stopBridge();
    }
    refreshSupportedBrowsers();
  });

  m_enableRow = new SettingRowWidget(
      tr("Enable Browser Integration"),
      tr("Automatically connect, sync two-way lock/unlock state, and enable "
         "autofill in supported web browsers."),
      m_enableCheck, m_bridgeCard);
  m_bridgeCard->addRow(m_enableRow);

  // Hint Callout Box
  m_hintBox = new QWidget(m_bridgeCard);
  m_hintBox->setObjectName(QStringLiteral("BrowserHintBox"));
  m_hintBox->setAttribute(Qt::WA_StyledBackground, true);
  auto *hintLayout = new QHBoxLayout(m_hintBox);
  hintLayout->setContentsMargins(12, 10, 12, 10);
  hintLayout->setSpacing(10);

  auto *hintIcon = new QLabel(m_hintBox);
  hintIcon->setPixmap(
      QIcon(QStringLiteral(":/icons/actions/info.svg")).pixmap(18, 18));
  hintLayout->addWidget(hintIcon);

  m_hintLabel = new QLabel(
      tr("The application automatically registers security manifests for "
         "browsers "
         "installed on this device. Extensions communicate over an encrypted "
         "local channel."),
      m_hintBox);
  m_hintLabel->setObjectName(QStringLiteral("BrowserHintText"));
  m_hintLabel->setWordWrap(true);
  hintLayout->addWidget(m_hintLabel, 1);

  m_bridgeCard->addWidget(m_hintBox);
  layout->addWidget(m_bridgeCard);

  // 2. Thẻ Danh sách Trình duyệt Đã cài đặt trên Hệ thống
  m_detectedCard = new SettingSectionCard(tr("Detected System Browsers"), this);

  m_browsersLayout = new QVBoxLayout();
  m_browsersLayout->setContentsMargins(0, 4, 0, 0);
  m_browsersLayout->setSpacing(8);

  auto *listContainer = new QWidget(m_detectedCard);
  listContainer->setLayout(m_browsersLayout);
  m_detectedCard->addWidget(listContainer);

  layout->addWidget(m_detectedCard);
  layout->addStretch(1);

  refreshSupportedBrowsers();
}

bool TabBrowser::isBrowserInstalled(const DetectedBrowserInfo &info) const {
  // 1. Kiểm tra trực tiếp file thực thi trong PATH
  for (const auto &bin : info.binaryCandidates) {
    if (!QStandardPaths::findExecutable(bin).isEmpty()) {
      return true;
    }
  }

  // 2. Kiểm tra file cấu hình Desktop Entry của hệ thống
  const QStringList desktopDirs = {
      QStringLiteral("/usr/share/applications/"),
      QStringLiteral("/usr/local/share/applications/"),
      QDir::homePath() + QStringLiteral("/.local/share/applications/"),
      QStringLiteral("/var/lib/flatpak/exports/share/applications/"),
      QDir::homePath() +
          QStringLiteral("/.local/share/flatpak/exports/share/applications/"),
      QStringLiteral("/var/lib/snapd/desktop/applications/")};

  for (const auto &desk : info.desktopCandidates) {
    for (const auto &dir : desktopDirs) {
      if (QFile::exists(dir + desk)) {
        return true;
      }
    }
  }

  return false;
}

void TabBrowser::populateBrowsers() {
  if (!m_browsersLayout) {
    return;
  }

  // Dọn dẹp widget cũ
  QLayoutItem *child = nullptr;
  while ((child = m_browsersLayout->takeAt(0)) != nullptr) {
    if (child->widget()) {
      child->widget()->deleteLater();
    }
    delete child;
  }

  const bool isBridgeActive =
      app::SettingsManager::instance().browserBridgeEnabled();
  const QJsonArray coreClients =
      core::CoreBridge::instance().listBridgeClients();

  // Danh mục nhận diện toàn diện các trình duyệt trên hệ điều hành
  const QList<DetectedBrowserInfo> catalog = {
      {QStringLiteral("chrome"),
       QStringLiteral("Google Chrome"),
       tr("Popular Chromium browser - Two-way unlock & Autofill supported"),
       QStringLiteral(":/icons/browsers/browser_chrome.svg"),
       {QStringLiteral("google-chrome-stable"),
        QStringLiteral("google-chrome")},
       {QStringLiteral("google-chrome.desktop"),
        QStringLiteral("com.google.Chrome.desktop")},
       QStringLiteral("https://chromewebstore.google.com/"),
       true,
       false,
       false},
      {QStringLiteral("firefox"),
       QStringLiteral("Mozilla Firefox"),
       tr("Open-source privacy browser - Autofill & synchronized unlock"),
       QStringLiteral(":/icons/browsers/browser_firefox.svg"),
       {QStringLiteral("firefox"), QStringLiteral("firefox-esr")},
       {QStringLiteral("firefox.desktop"),
        QStringLiteral("firefox-esr.desktop"),
        QStringLiteral("org.mozilla.firefox.desktop")},
       QStringLiteral("https://addons.mozilla.org/"),
       true,
       false,
       false},
      {QStringLiteral("brave"),
       QStringLiteral("Brave Browser"),
       tr("Privacy-focused browser - Full Chromium compatibility"),
       QStringLiteral(":/icons/browsers/browser_brave.svg"),
       {QStringLiteral("brave-browser"), QStringLiteral("brave")},
       {QStringLiteral("brave-browser.desktop"),
        QStringLiteral("com.brave.Browser.desktop")},
       QStringLiteral("https://chromewebstore.google.com/"),
       true,
       false,
       false},
      {QStringLiteral("edge"),
       QStringLiteral("Microsoft Edge"),
       tr("Modern cross-platform browser - Deep system integration"),
       QStringLiteral(":/icons/browsers/browser_edge.svg"),
       {QStringLiteral("microsoft-edge"),
        QStringLiteral("microsoft-edge-stable")},
       {QStringLiteral("microsoft-edge.desktop"),
        QStringLiteral("com.microsoft.Edge.desktop")},
       QStringLiteral("https://microsoftedge.microsoft.com/addons/"),
       true,
       false,
       false},
      {QStringLiteral("chromium"),
       QStringLiteral("Chromium"),
       tr("Standard open-source browser platform"),
       QStringLiteral(":/icons/browsers/browser_chromium.svg"),
       {QStringLiteral("chromium"), QStringLiteral("chromium-browser")},
       {QStringLiteral("chromium.desktop"),
        QStringLiteral("chromium-browser.desktop"),
        QStringLiteral("org.chromium.Chromium.desktop")},
       QStringLiteral("https://chromewebstore.google.com/"),
       true,
       false,
       false},
      {QStringLiteral("opera"),
       QStringLiteral("Opera"),
       tr("Feature-rich browser with integrated tools"),
       QStringLiteral(":/icons/categories/category_all.svg"),
       {QStringLiteral("opera"), QStringLiteral("opera-stable")},
       {QStringLiteral("opera.desktop"),
        QStringLiteral("com.opera.Opera.desktop")},
       QStringLiteral("https://addons.opera.com/"),
       true,
       false,
       false},
      {QStringLiteral("vivaldi"),
       QStringLiteral("Vivaldi"),
       tr("Highly customizable browser for power users"),
       QStringLiteral(":/icons/categories/category_all.svg"),
       {QStringLiteral("vivaldi"), QStringLiteral("vivaldi-stable")},
       {QStringLiteral("vivaldi-stable.desktop"),
        QStringLiteral("com.vivaldi.Vivaldi.desktop")},
       QStringLiteral("https://chromewebstore.google.com/"),
       true,
       false,
       false},
      {QStringLiteral("epiphany"),
       QStringLiteral("GNOME Web (Epiphany)"),
       tr("Default browser of the GNOME desktop environment"),
       QStringLiteral(":/icons/categories/category_all.svg"),
       {QStringLiteral("epiphany"), QStringLiteral("epiphany-browser")},
       {QStringLiteral("org.gnome.Epiphany.desktop")},
       QString(),
       false,
       false,
       false},
      {QStringLiteral("falkon"),
       QStringLiteral("Falkon"),
       tr("Lightweight QtWebEngine-based web browser"),
       QStringLiteral(":/icons/categories/category_all.svg"),
       {QStringLiteral("falkon")},
       {QStringLiteral("org.kde.falkon.desktop")},
       QString(),
       false,
       false,
       false},
      {QStringLiteral("tor"),
       QStringLiteral("Tor Browser"),
       tr("Privacy browser built to resist tracking"),
       QStringLiteral(":/icons/categories/category_all.svg"),
       {QStringLiteral("tor-browser"), QStringLiteral("torbrowser-launcher")},
       {QStringLiteral("torbrowser.desktop")},
       QString(),
       false,
       false,
       false}};

  QList<DetectedBrowserInfo> installedBrowsers;

  for (auto item : catalog) {
    if (isBrowserInstalled(item)) {
      item.isInstalled = true;

      // Kiểm tra xem trình duyệt này đã có phiên tiện ích kết nối hay chưa
      for (const auto &clientVal : coreClients) {
        if (!clientVal.isObject()) {
          continue;
        }
        const QString clientBrowser = clientVal.toObject()
                                          .value(QStringLiteral("browser"))
                                          .toString()
                                          .toLower();
        if (clientBrowser.contains(item.id) ||
            (item.id == QStringLiteral("chrome") &&
             (clientBrowser.contains(QStringLiteral("chrome")) ||
              clientBrowser.contains(QStringLiteral("chromium")))) ||
            (item.id == QStringLiteral("firefox") &&
             clientBrowser.contains(QStringLiteral("firefox")))) {
          item.isConnected = true;
          break;
        }
      }

      installedBrowsers.append(item);
    }
  }

  // Trường hợp không phát hiện bất kỳ trình duyệt nào trên hệ thống
  if (installedBrowsers.isEmpty()) {
    auto *emptyCard = new QWidget(m_detectedCard);
    emptyCard->setObjectName(QStringLiteral("BrowserEmptyCard"));
    emptyCard->setAttribute(Qt::WA_StyledBackground, true);

    auto *emptyLayout = new QVBoxLayout(emptyCard);
    emptyLayout->setContentsMargins(24, 20, 24, 20);
    emptyLayout->setSpacing(8);

    auto *titleLbl =
        new QLabel(tr("No web browsers detected on this machine"), emptyCard);
    titleLbl->setObjectName(QStringLiteral("BrowserEmptyTitle"));
    titleLbl->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(titleLbl);

    auto *descLbl = new QLabel(tr("Install a supported browser such as Google "
                                  "Chrome or Mozilla Firefox "
                                  "to enable browser extension integration."),
                               emptyCard);
    descLbl->setObjectName(QStringLiteral("BrowserEmptyDesc"));
    descLbl->setAlignment(Qt::AlignCenter);
    descLbl->setWordWrap(true);
    emptyLayout->addWidget(descLbl);

    m_browsersLayout->addWidget(emptyCard);
    return;
  }

  // Hiển thị danh sách các trình duyệt thực tế đang có trên hệ thống
  for (const auto &item : installedBrowsers) {
    auto *row = new QWidget(m_detectedCard);
    row->setObjectName(QStringLiteral("BrowserItemCard"));
    row->setAttribute(Qt::WA_StyledBackground, true);

    auto *rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(14, 10, 14, 10);
    rowLayout->setSpacing(14);

    // 1. Browser Logo Icon
    auto *iconLbl = new QLabel(row);
    iconLbl->setFixedSize(28, 28);
    iconLbl->setScaledContents(true);
    iconLbl->setPixmap(QIcon(item.iconPath).pixmap(28, 28));
    rowLayout->addWidget(iconLbl);

    // 2. Browser Information
    auto *infoLayout = new QVBoxLayout();
    infoLayout->setSpacing(2);

    auto *nameLbl = new QLabel(item.name, row);
    nameLbl->setObjectName(QStringLiteral("BrowserItemTitle"));
    infoLayout->addWidget(nameLbl);

    auto *descLbl = new QLabel(row);
    descLbl->setObjectName(QStringLiteral("BrowserItemDesc"));
    if (!item.isSupported) {
      descLbl->setText(
          tr("Browser integration is not yet supported for this browser"));
    } else if (item.isConnected && isBridgeActive) {
      descLbl->setText(tr("Extension active and syncing in real time"));
    } else {
      descLbl->setText(item.description);
    }
    infoLayout->addWidget(descLbl);

    rowLayout->addLayout(infoLayout, 1);

    // 3. Trạng thái hỗ trợ & Trạng thái kết nối
    if (!item.isSupported) {
      auto *unsupportedBadge = new QLabel(tr("Unsupported"), row);
      unsupportedBadge->setObjectName(
          QStringLiteral("BrowserUnsupportedBadge"));
      rowLayout->addWidget(unsupportedBadge);
    } else if (!isBridgeActive) {
      auto *inactiveBadge = new QLabel(tr("Integration Off"), row);
      inactiveBadge->setObjectName(QStringLiteral("BrowserInactiveBadge"));
      rowLayout->addWidget(inactiveBadge);

      if (!item.storeUrl.isEmpty()) {
        auto *installBtn = new QPushButton(tr("Install Extension"), row);
        installBtn->setObjectName(QStringLiteral("BrowserInstallBtn"));
        installBtn->setFixedHeight(30);
        installBtn->setCursor(Qt::PointingHandCursor);
        const QString storeUrl = item.storeUrl;
        connect(installBtn, &QPushButton::clicked, this,
                [storeUrl]() { QDesktopServices::openUrl(QUrl(storeUrl)); });
        rowLayout->addWidget(installBtn);
      }
    } else if (item.isConnected) {
      auto *connectedBadge = new QLabel(tr("Active"), row);
      connectedBadge->setObjectName(QStringLiteral("BrowserConnectedBadge"));
      rowLayout->addWidget(connectedBadge);

      auto *connectedBtn = new QPushButton(tr("Connected"), row);
      connectedBtn->setObjectName(QStringLiteral("BrowserConnectedBtn"));
      connectedBtn->setFixedHeight(30);
      connectedBtn->setEnabled(false);
      rowLayout->addWidget(connectedBtn);
    } else {
      auto *pendingBadge = new QLabel(tr("Not Connected"), row);
      pendingBadge->setObjectName(QStringLiteral("BrowserPendingBadge"));
      rowLayout->addWidget(pendingBadge);

      if (!item.storeUrl.isEmpty()) {
        auto *installBtn = new QPushButton(tr("Install Extension"), row);
        installBtn->setObjectName(QStringLiteral("BrowserInstallBtn"));
        installBtn->setFixedHeight(30);
        installBtn->setCursor(Qt::PointingHandCursor);
        const QString storeUrl = item.storeUrl;
        connect(installBtn, &QPushButton::clicked, this,
                [storeUrl]() { QDesktopServices::openUrl(QUrl(storeUrl)); });
        rowLayout->addWidget(installBtn);
      }
    }

    m_browsersLayout->addWidget(row);
  }
}

void TabBrowser::showEvent(QShowEvent *event) {
  QWidget::showEvent(event);
  refreshSupportedBrowsers();
}

void TabBrowser::refreshSupportedBrowsers() { populateBrowsers(); }

void TabBrowser::retranslateUi() {
  if (m_bridgeCard) {
    m_bridgeCard->setTitle(tr("Web Browser Integration"));
  }
  if (m_enableRow) {
    m_enableRow->updateText(
        tr("Enable Browser Integration"),
        tr("Automatically connect, sync two-way lock/unlock state, and enable "
           "autofill in supported web browsers."));
  }
  if (m_hintLabel) {
    m_hintLabel->setText(
        tr("The application automatically registers security manifests for "
           "browsers installed on this device. Extensions communicate over an "
           "encrypted local channel."));
  }
  if (m_detectedCard) {
    m_detectedCard->setTitle(tr("Detected System Browsers"));
  }
  refreshSupportedBrowsers();
}

} // namespace svm::views
