/**
 * @file src/ui/views/about/AboutDialog.cpp
 * @brief Hiện thực AboutDialog thiết kế theo kiến trúc Card + Multi-Tab hiện
 * đại.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "ui/views/about/AboutDialog.hpp"
#include "core/CoreBridge.hpp"
#include "ui/app/ThemeManager.hpp"
#include "ui/window/FramelessDialog.hpp"

#include <QCoreApplication>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeyEvent>
#include <QScrollArea>
#include <QVBoxLayout>

namespace svm::views {

AboutDialog::AboutDialog(QWidget *parent, int initialTab)
    : FramelessDialog(parent, app::ThemeManager::instance().isDarkTheme()) {
  setObjectName(QStringLiteral("AboutDialog"));
  setModal(false);
  setWindowModality(Qt::NonModal);
  setMinimumSize(520, 460);
  resize(580, 540);

  setupUI(initialTab);

  connect(&app::ThemeManager::instance(), &app::ThemeManager::themeChanged,
          this, [this](app::ThemeMode) {
            setDark(app::ThemeManager::instance().isDarkTheme());
            if (m_closeIconBtn) {
              m_closeIconBtn->setIcon(app::ThemeManager::instance().icon(
                  QStringLiteral("win_close")));
            }
            populateAppTabContent();
            populateQtTabContent();
          });
}

void AboutDialog::setupUI(int initialTab) {
  m_cardWidget = new QWidget(this);
  m_cardWidget->setObjectName(QStringLiteral("AboutDialogCard"));
  m_cardWidget->setAttribute(Qt::WA_StyledBackground, true);

  auto *cardLayout = new QVBoxLayout(m_cardWidget);
  cardLayout->setContentsMargins(0, 0, 0, 0);
  cardLayout->setSpacing(0);

  // 1. Unified Header Bar
  m_headerWidget = createHeaderBar();
  cardLayout->addWidget(m_headerWidget);

  // 2. Central Tab Widget
  auto *contentContainer = new QWidget(m_cardWidget);
  contentContainer->setAttribute(Qt::WA_StyledBackground, true);
  auto *contentLayout = new QVBoxLayout(contentContainer);
  contentLayout->setContentsMargins(14, 10, 14, 10);
  contentLayout->setSpacing(8);

  m_tabWidget = new QTabWidget(contentContainer);
  m_tabWidget->setObjectName(QStringLiteral("AboutTabWidget"));

  m_appTab = createAppTab();
  m_qtTab = createQtTab();
  m_licenseTab = createLicenseTab();

  m_tabWidget->addTab(m_appTab, QStringLiteral("Secure Vault Manager"));
  m_tabWidget->addTab(m_qtTab, tr("Qt Framework"));
  m_tabWidget->addTab(m_licenseTab, tr("License"));

  if (initialTab >= 0 && initialTab < m_tabWidget->count()) {
    m_tabWidget->setCurrentIndex(initialTab);
  }

  contentLayout->addWidget(m_tabWidget, 1);
  cardLayout->addWidget(contentContainer, 1);

  // 3. Bottom Action Bar
  auto *bottomBar = new QFrame(m_cardWidget);
  bottomBar->setObjectName(QStringLiteral("AboutBottomBar"));
  bottomBar->setFixedHeight(50);
  auto *bottomLayout = new QHBoxLayout(bottomBar);
  bottomLayout->setContentsMargins(18, 10, 18, 10);
  bottomLayout->setSpacing(10);
  bottomLayout->setAlignment(Qt::AlignVCenter);

  bottomLayout->addStretch(1);

  m_bottomCloseBtn = new QPushButton(tr("Close"), bottomBar);
  m_bottomCloseBtn->setObjectName(QStringLiteral("AboutBottomCloseBtn"));
  m_bottomCloseBtn->setFixedHeight(30);
  m_bottomCloseBtn->setMinimumWidth(88);
  m_bottomCloseBtn->setDefault(true);
  m_bottomCloseBtn->setCursor(Qt::PointingHandCursor);
  connect(m_bottomCloseBtn, &QPushButton::clicked, this, &QDialog::accept);
  bottomLayout->addWidget(m_bottomCloseBtn);

  cardLayout->addWidget(bottomBar);

  setHeaderBar(m_headerWidget);
  setCardWidget(m_cardWidget);
}

QWidget *AboutDialog::createHeaderBar() {
  auto *header = new QWidget(m_cardWidget);
  header->setObjectName(QStringLiteral("AboutDialogHeader"));
  header->setAttribute(Qt::WA_StyledBackground, true);
  header->setFixedHeight(38);

  auto *headerLayout = new QHBoxLayout(header);
  headerLayout->setContentsMargins(14, 0, 0, 0);
  headerLayout->setSpacing(8);

  auto *logoIconLabel = new QLabel(header);
  logoIconLabel->setPixmap(
      QIcon(QStringLiteral(":/icons/brand/app_logo.svg")).pixmap(18, 18));
  logoIconLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
  headerLayout->addWidget(logoIconLabel, 0, Qt::AlignVCenter);

  m_headerTitle = new QLabel(tr("About Secure Vault Manager"), header);
  m_headerTitle->setObjectName(QStringLiteral("AboutDialogTitle"));
  m_headerTitle->setAttribute(Qt::WA_TransparentForMouseEvents);
  headerLayout->addWidget(m_headerTitle, 0, Qt::AlignVCenter);

  headerLayout->addStretch(1);

  m_closeIconBtn = new QPushButton(header);
  m_closeIconBtn->setObjectName(QStringLiteral("WinCloseBtn"));
  m_closeIconBtn->setFixedSize(46, 38);
  m_closeIconBtn->setIconSize(QSize(12, 12));
  m_closeIconBtn->setFlat(true);
  m_closeIconBtn->setFocusPolicy(Qt::NoFocus);
  m_closeIconBtn->setCursor(Qt::PointingHandCursor);
  m_closeIconBtn->setToolTip(tr("Close"));
  m_closeIconBtn->setIcon(
      app::ThemeManager::instance().icon(QStringLiteral("win_close")));
  connect(m_closeIconBtn, &QPushButton::clicked, this, &QDialog::reject);
  headerLayout->addWidget(m_closeIconBtn, 0, Qt::AlignVCenter);

  return header;
}

QWidget *AboutDialog::createAppTab() {
  auto *scrollArea = new QScrollArea(m_tabWidget);
  scrollArea->setWidgetResizable(true);
  scrollArea->setFrameShape(QFrame::NoFrame);
  scrollArea->setObjectName(QStringLiteral("AboutScrollArea"));
  scrollArea->viewport()->setAutoFillBackground(false);

  auto *tab = new QWidget(scrollArea);
  tab->setAutoFillBackground(false);
  auto *layout = new QVBoxLayout(tab);
  layout->setContentsMargins(14, 14, 14, 14);
  layout->setSpacing(12);

  // Hero Banner
  auto *heroLayout = new QHBoxLayout();
  heroLayout->setSpacing(14);

  auto *appIcon = new QLabel(tab);
  appIcon->setPixmap(
      QIcon(QStringLiteral(":/icons/brand/app_logo.svg")).pixmap(52, 52));
  appIcon->setFixedSize(56, 56);
  appIcon->setAlignment(Qt::AlignCenter);
  heroLayout->addWidget(appIcon, 0, Qt::AlignTop);

  auto *heroTextLayout = new QVBoxLayout();
  heroTextLayout->setSpacing(3);

  m_appHeroTitle = new QLabel(tab);
  m_appHeroTitle->setObjectName(QStringLiteral("AboutHeroTitle"));
  heroTextLayout->addWidget(m_appHeroTitle);

  m_appHeroSub = new QLabel(tab);
  m_appHeroSub->setObjectName(QStringLiteral("AboutHeroSubtitle"));
  m_appHeroSub->setWordWrap(true);
  heroTextLayout->addWidget(m_appHeroSub);

  heroLayout->addLayout(heroTextLayout, 1);
  layout->addLayout(heroLayout);

  // Key Features Card
  auto *featuresCard = new QFrame(tab);
  featuresCard->setObjectName(QStringLiteral("AboutInfoCard"));
  auto *featuresLayout = new QVBoxLayout(featuresCard);
  featuresLayout->setContentsMargins(14, 12, 14, 12);
  featuresLayout->setSpacing(8);

  m_featuresTitle = new QLabel(featuresCard);
  m_featuresTitle->setObjectName(QStringLiteral("AboutCardTitle"));
  featuresLayout->addWidget(m_featuresTitle);

  m_featuresDesc = new QLabel(featuresCard);
  m_featuresDesc->setObjectName(QStringLiteral("AboutCardBody"));
  m_featuresDesc->setWordWrap(true);
  featuresLayout->addWidget(m_featuresDesc);

  layout->addWidget(featuresCard);

  // Product & Author Information Card
  auto *infoCard = new QFrame(tab);
  infoCard->setObjectName(QStringLiteral("AboutInfoCard"));
  auto *infoLayout = new QVBoxLayout(infoCard);
  infoLayout->setContentsMargins(14, 12, 14, 12);
  infoLayout->setSpacing(8);

  m_productTitle = new QLabel(infoCard);
  m_productTitle->setObjectName(QStringLiteral("AboutCardTitle"));
  infoLayout->addWidget(m_productTitle);

  m_productDesc = new QLabel(infoCard);
  m_productDesc->setObjectName(QStringLiteral("AboutCardBody"));
  m_productDesc->setWordWrap(true);
  m_productDesc->setOpenExternalLinks(true);
  m_productDesc->setTextInteractionFlags(Qt::TextBrowserInteraction);
  infoLayout->addWidget(m_productDesc);

  layout->addWidget(infoCard);
  layout->addStretch(1);

  scrollArea->setWidget(tab);
  populateAppTabContent();
  return scrollArea;
}

void AboutDialog::populateAppTabContent() {
  const auto &pal = app::ThemeManager::instance().palette();
  const QString textColor = pal.foreground.name();
  const QString subColor = pal.mutedForeground.name();
  const QString linkColor = pal.info.name();

  const QString appVer = QCoreApplication::applicationVersion().isEmpty()
                             ? QStringLiteral("2026.11.0")
                             : QCoreApplication::applicationVersion();
  const QString coreVer = core::CoreBridge::instance().coreVersion().isEmpty()
                              ? appVer
                              : core::CoreBridge::instance().coreVersion();

  if (m_appHeroTitle) {
    m_appHeroTitle->setText(
        QStringLiteral("Secure Vault Manager <span style='font-size:12px; "
                       "font-weight:normal; color:%1;'>v%2 (Core v%3)</span>")
            .arg(subColor, appVer, coreVer));
  }
  if (m_appHeroSub) {
    m_appHeroSub->setText(tr(
        "Zero-knowledge local password, passkey, and SSH credential manager."));
  }
  if (m_featuresTitle) {
    m_featuresTitle->setText(tr("Key Features"));
  }
  if (m_featuresDesc) {
    m_featuresDesc->setText(
        QStringLiteral("<ul style='margin:0; padding-left:18px; "
                       "line-height:1.5; color:%1;'>"
                       "<li><b>%2:</b> %3</li>"
                       "<li><b>%4:</b> %5</li>"
                       "<li><b>%6:</b> %7</li>"
                       "<li><b>%8:</b> %9</li>"
                       "</ul>")
            .arg(textColor, tr("Zero-Knowledge Encryption"),
                 tr("ChaCha20-Poly1305 authenticated encryption with Argon2id "
                    "key derivation and local-first storage."),
                 tr("FIDO2 / WebAuthn Passkeys"),
                 tr("Hardware-grade passwordless authentication synced "
                    "seamlessly with Chrome, Edge, Brave, and Firefox."),
                 tr("TOTP & OpenSSH Agent"),
                 tr("Integrated RFC 6238 2FA authenticator codes and native "
                    "Unix/Windows OpenSSH agent forwarding."),
                 tr("Cross-Platform Architecture"),
                 tr("High-performance Rust cryptographic core paired with a "
                    "fast, modern Qt 6 desktop interface.")));
  }
  if (m_productTitle) {
    m_productTitle->setText(tr("Product Information"));
  }
  if (m_productDesc) {
    m_productDesc->setText(
        QStringLiteral(
            "<table style='margin:0; padding:0; border-collapse:collapse; "
            "color:%1; line-height:1.5;'>"
            "<tr>"
            "<td style='padding:2px 14px 2px 0; font-weight:600;'>%2</td>"
            "<td style='padding:2px 0;'>%3 (Rust Core v%4)</td>"
            "</tr>"
            "<tr>"
            "<td style='padding:2px 14px 2px 0; font-weight:600;'>%5</td>"
            "<td style='padding:2px 0;'>Hai Pham Ngoc &lt;<a "
            "href='mailto:ngochai285nd@gmail.com' "
            "style='color:%6; "
            "text-decoration:underline;'>ngochai285nd@gmail.com</a>&gt;</td>"
            "</tr>"
            "<tr>"
            "<td style='padding:2px 14px 2px 0; font-weight:600;'>%7</td>"
            "<td style='padding:2px 0;'>Hephaestus Developers</td>"
            "</tr>"
            "<tr>"
            "<td style='padding:2px 14px 2px 0; font-weight:600;'>%8</td>"
            "<td style='padding:2px 0;'><a "
            "href='https://opensource.org/licenses/MIT' "
            "style='color:%6; text-decoration:underline;'>MIT License</a></td>"
            "</tr>"
            "<tr>"
            "<td style='padding:2px 14px 2px 0; font-weight:600;'>%9</td>"
            "<td style='padding:2px 0;'><a "
            "href='https://github.com/hephaestus-developers/"
            "secure-vault-manager' "
            "style='color:%6; "
            "text-decoration:underline;'>github.com/hephaestus-developers/"
            "secure-vault-manager</a></td>"
            "</tr>"
            "</table>")
            .arg(textColor, tr("Version:"), appVer, coreVer, tr("Author:"),
                 linkColor, tr("Organization:"), tr("License:"),
                 tr("Repository:")));
  }
}

QWidget *AboutDialog::createQtTab() {
  auto *scrollArea = new QScrollArea(m_tabWidget);
  scrollArea->setWidgetResizable(true);
  scrollArea->setFrameShape(QFrame::NoFrame);
  scrollArea->setObjectName(QStringLiteral("AboutScrollArea"));
  scrollArea->viewport()->setAutoFillBackground(false);

  auto *tab = new QWidget(scrollArea);
  tab->setAutoFillBackground(false);
  auto *layout = new QVBoxLayout(tab);
  layout->setContentsMargins(14, 14, 14, 14);
  layout->setSpacing(12);

  // Hero banner
  auto *heroLayout = new QHBoxLayout();
  heroLayout->setSpacing(14);

  auto *qtIcon = new QLabel(tab);
  qtIcon->setPixmap(
      QIcon(QStringLiteral(":/icons/brand/qt_logo.svg")).pixmap(52, 52));
  qtIcon->setFixedSize(56, 56);
  qtIcon->setAlignment(Qt::AlignCenter);
  heroLayout->addWidget(qtIcon, 0, Qt::AlignTop);

  auto *heroTextLayout = new QVBoxLayout();
  heroTextLayout->setSpacing(3);

  m_qtHeroTitle = new QLabel(tab);
  m_qtHeroTitle->setObjectName(QStringLiteral("AboutHeroTitle"));
  heroTextLayout->addWidget(m_qtHeroTitle);

  m_qtHeroSub = new QLabel(tab);
  m_qtHeroSub->setObjectName(QStringLiteral("AboutHeroSubtitle"));
  m_qtHeroSub->setWordWrap(true);
  heroTextLayout->addWidget(m_qtHeroSub);

  heroLayout->addLayout(heroTextLayout, 1);
  layout->addLayout(heroLayout);

  // Qt Description Card
  auto *qtCard = new QFrame(tab);
  qtCard->setObjectName(QStringLiteral("AboutInfoCard"));
  auto *qtCardLayout = new QVBoxLayout(qtCard);
  qtCardLayout->setContentsMargins(14, 12, 14, 12);
  qtCardLayout->setSpacing(6);

  m_qtDescLabel = new QLabel(qtCard);
  m_qtDescLabel->setObjectName(QStringLiteral("AboutCardBody"));
  m_qtDescLabel->setWordWrap(true);
  qtCardLayout->addWidget(m_qtDescLabel);

  layout->addWidget(qtCard);

  // LGPL Compliance Card
  auto *lgplCard = new QFrame(tab);
  lgplCard->setObjectName(QStringLiteral("AboutInfoCard"));
  auto *lgplLayout = new QVBoxLayout(lgplCard);
  lgplLayout->setContentsMargins(14, 12, 14, 12);
  lgplLayout->setSpacing(6);

  m_lgplTitle = new QLabel(lgplCard);
  m_lgplTitle->setObjectName(QStringLiteral("AboutCardTitle"));
  lgplLayout->addWidget(m_lgplTitle);

  m_lgplDesc = new QLabel(lgplCard);
  m_lgplDesc->setObjectName(QStringLiteral("AboutCardBody"));
  m_lgplDesc->setWordWrap(true);
  lgplLayout->addWidget(m_lgplDesc);

  layout->addWidget(lgplCard);

  // Copyright & Trademark Notice Card
  auto *tmCard = new QFrame(tab);
  tmCard->setObjectName(QStringLiteral("AboutInfoCard"));
  auto *tmLayout = new QVBoxLayout(tmCard);
  tmLayout->setContentsMargins(14, 12, 14, 12);
  tmLayout->setSpacing(6);

  m_tmTitle = new QLabel(tmCard);
  m_tmTitle->setObjectName(QStringLiteral("AboutCardTitle"));
  tmLayout->addWidget(m_tmTitle);

  m_tmDesc = new QLabel(tmCard);
  m_tmDesc->setObjectName(QStringLiteral("AboutCardBody"));
  m_tmDesc->setWordWrap(true);
  m_tmDesc->setOpenExternalLinks(true);
  m_tmDesc->setTextInteractionFlags(Qt::TextBrowserInteraction);
  tmLayout->addWidget(m_tmDesc);

  layout->addWidget(tmCard);
  layout->addStretch(1);

  scrollArea->setWidget(tab);
  populateQtTabContent();
  return scrollArea;
}

void AboutDialog::populateQtTabContent() {
  const auto &pal = app::ThemeManager::instance().palette();
  const QString textColor = pal.foreground.name();
  const QString subColor = pal.mutedForeground.name();
  const QString linkColor = pal.info.name();
  const QString qtVer = QString::fromLatin1(qVersion());

  if (m_qtHeroTitle) {
    m_qtHeroTitle->setText(
        QStringLiteral("Qt Framework <span style='font-size:12px; "
                       "font-weight:normal; color:%1;'>v%2</span>")
            .arg(subColor, qtVer));
  }
  if (m_qtHeroSub) {
    m_qtHeroSub->setText(tr("Cross-platform C++ application and UI framework"));
  }
  if (m_qtDescLabel) {
    m_qtDescLabel->setText(
        QStringLiteral("<p style='margin:0 0 6px 0; color:%1;'>%2</p>"
                       "<p style='margin:0 0 6px 0; color:%1;'>%3</p>"
                       "<p style='margin:0; color:%1;'>%4</p>")
            .arg(textColor,
                 tr("This program uses <b>Qt version %1</b> for its desktop "
                    "graphical user interface.")
                     .arg(qtVer),
                 tr("Qt is a C++ toolkit for cross-platform development "
                    "providing single-source portability across Linux, macOS, "
                    "Windows, and mobile platforms."),
                 tr("Qt is available under multiple licenses: GNU LGPL v3, GNU "
                    "GPL v2/v3, and commercial licenses.")));
  }
  if (m_lgplTitle) {
    m_lgplTitle->setText(tr("GNU LGPL v3 Compliance Statement"));
  }
  if (m_lgplDesc) {
    m_lgplDesc->setText(
        QStringLiteral("<p style='margin:0 0 6px 0; color:%1;'>%2</p>"
                       "<p style='margin:0; color:%1;'>%3</p>")
            .arg(textColor,
                 tr("The Qt framework libraries utilized by this application "
                    "are free software licensed under the terms of the <b>GNU "
                    "Lesser General Public License (LGPL v3)</b>."),
                 tr("Qt runtime libraries are dynamically linked. You have the "
                    "right and freedom to inspect, relink, or replace these "
                    "libraries with compatible versions in accordance with GNU "
                    "LGPL v3.")));
  }
  if (m_tmTitle) {
    m_tmTitle->setText(tr("Copyright & Trademarks"));
  }
  if (m_tmDesc) {
    m_tmDesc->setText(
        QStringLiteral("<p style='margin:0 0 4px 0; color:%1;'>%2</p>"
                       "<p style='margin:0; color:%1;'>%3 <a style='color:%4; "
                       "text-decoration:underline;' "
                       "href='https://www.qt.io/'>https://www.qt.io/</a>.</p>")
            .arg(textColor,
                 tr("Qt is a registered trademark of The Qt Company Ltd. "
                    "and/or its subsidiaries."),
                 tr("Copyright (C) 2026 The Qt Company Ltd. and other "
                    "contributors. For more information, visit"),
                 linkColor));
  }
}

QWidget *AboutDialog::createLicenseTab() {
  auto *tab = new QWidget(m_tabWidget);
  auto *layout = new QVBoxLayout(tab);
  layout->setContentsMargins(12, 12, 12, 12);

  m_licenseEdit = new QPlainTextEdit(tab);
  m_licenseEdit->setObjectName(QStringLiteral("AboutLicenseView"));
  m_licenseEdit->setReadOnly(true);

  const QString mitText = QStringLiteral(
      "MIT License\n\n"
      "Copyright (c) 2026 Hephaestus Developers\n\n"
      "Permission is hereby granted, free of charge, to any person obtaining a "
      "copy\n"
      "of this software and associated documentation files (the \"Software\"), "
      "to deal\n"
      "in the Software without restriction, including without limitation the "
      "rights\n"
      "to use, copy, modify, merge, publish, distribute, sublicense, and/or "
      "sell\n"
      "copies of the Software, and to permit persons to whom the Software is\n"
      "furnished to do so, subject to the following conditions:\n\n"
      "The above copyright notice and this permission notice shall be included "
      "in all\n"
      "copies or substantial portions of the Software.\n\n"
      "THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, "
      "EXPRESS OR\n"
      "IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF "
      "MERCHANTABILITY,\n"
      "FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL "
      "THE\n"
      "AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER\n"
      "LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING "
      "FROM,\n"
      "OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS "
      "IN THE\n"
      "SOFTWARE.");

  m_licenseEdit->setPlainText(mitText);
  layout->addWidget(m_licenseEdit);

  return tab;
}

void AboutDialog::retranslateUi() {
  if (m_headerTitle) {
    m_headerTitle->setText(tr("About Secure Vault Manager"));
  }
  if (m_closeIconBtn) {
    m_closeIconBtn->setToolTip(tr("Close"));
  }
  if (m_tabWidget) {
    m_tabWidget->setTabText(0, QStringLiteral("Secure Vault Manager"));
    m_tabWidget->setTabText(1, tr("Qt Framework"));
    m_tabWidget->setTabText(2, tr("License"));
  }
  populateAppTabContent();
  populateQtTabContent();
  if (m_bottomCloseBtn) {
    m_bottomCloseBtn->setText(tr("Close"));
  }
}

void AboutDialog::changeEvent(QEvent *event) {
  if (event->type() == QEvent::LanguageChange) {
    retranslateUi();
  }
  FramelessDialog::changeEvent(event);
}

void AboutDialog::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Escape) {
    reject();
    return;
  }
  FramelessDialog::keyPressEvent(event);
}

} // namespace svm::views
