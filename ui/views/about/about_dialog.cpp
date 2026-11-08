/**
 * @file ui/views/about/about_dialog.cpp
 * @brief Hiện thực AboutDialog thẩm mỹ cao phong cách Notera.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "about_dialog.h"
#include "app/theme_manager.h"
#include "components/dialog_drag_helper.h"
#include "components/window_resize_handler.h"

#include <QEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QVBoxLayout>

namespace svm::views {

AboutDialog::AboutDialog(QWidget *parent)
    : QDialog(parent, Qt::FramelessWindowHint | Qt::Dialog) {
  setObjectName(QStringLiteral("AboutDialog"));
  setAttribute(Qt::WA_TranslucentBackground, false);
  setModal(false);
  setWindowModality(Qt::NonModal);
  setMinimumSize(480, 420);
  resize(540, 480);

  setupUI();

  new components::WindowResizeHandler(this);
}

void AboutDialog::setupUI() {
  auto *rootLayout = new QVBoxLayout(this);
  rootLayout->setContentsMargins(0, 0, 0, 0);

  m_cardWidget = new QWidget(this);
  m_cardWidget->setObjectName(QStringLiteral("AboutDialogCard"));
  m_cardWidget->setAttribute(Qt::WA_StyledBackground, true);

  auto *cardLayout = new QVBoxLayout(m_cardWidget);
  cardLayout->setContentsMargins(0, 0, 0, 14);
  cardLayout->setSpacing(0);

  // 1. Header Bar
  m_headerWidget = new QWidget(m_cardWidget);
  m_headerWidget->setObjectName(QStringLiteral("AboutDialogHeader"));
  m_headerWidget->setAttribute(Qt::WA_StyledBackground, true);
  m_headerWidget->setFixedHeight(38);

  auto *headerLayout = new QHBoxLayout(m_headerWidget);
  headerLayout->setContentsMargins(14, 0, 0, 0);
  headerLayout->setSpacing(8);

  auto *logoIconLabel = new QLabel(m_headerWidget);
  logoIconLabel->setPixmap(
      QIcon(QStringLiteral(":/icons/app_logo.svg")).pixmap(18, 18));
  headerLayout->addWidget(logoIconLabel);

  m_headerTitle = new QLabel(tr("About Secure Vault Manager"), m_headerWidget);
  m_headerTitle->setObjectName(QStringLiteral("AboutDialogTitle"));
  headerLayout->addWidget(m_headerTitle);

  headerLayout->addStretch(1);

  m_closeIconBtn = new QPushButton(m_headerWidget);
  m_closeIconBtn->setObjectName(QStringLiteral("WinCloseBtn"));
  m_closeIconBtn->setFixedSize(46, 38);
  m_closeIconBtn->setIconSize(QSize(12, 12));
  m_closeIconBtn->setFlat(true);
  m_closeIconBtn->setFocusPolicy(Qt::NoFocus);
  m_closeIconBtn->setCursor(Qt::PointingHandCursor);
  m_closeIconBtn->setIcon(
      app::ThemeManager::instance().icon(QStringLiteral("win_close")));
  connect(m_closeIconBtn, &QPushButton::clicked, this, &QDialog::reject);
  headerLayout->addWidget(m_closeIconBtn);

  cardLayout->addWidget(m_headerWidget);

  // 2. Nội dung thân: QTabWidget
  auto *contentContainer = new QWidget(m_cardWidget);
  contentContainer->setAttribute(Qt::WA_StyledBackground, true);
  auto *contentLayout = new QVBoxLayout(contentContainer);
  contentLayout->setContentsMargins(14, 12, 14, 0);
  contentLayout->setSpacing(10);

  m_tabWidget = new QTabWidget(contentContainer);
  m_tabWidget->setObjectName(QStringLiteral("AboutTabWidget"));

  m_infoTab = createInfoTab();
  m_licenseTab = createLicenseTab();

  m_tabWidget->addTab(m_infoTab, tr("Information"));
  m_tabWidget->addTab(m_licenseTab, tr("License"));

  contentLayout->addWidget(m_tabWidget);

  // 3. Bottom Action Bar
  auto *bottomBar = new QWidget(contentContainer);
  auto *bottomLayout = new QHBoxLayout(bottomBar);
  bottomLayout->setContentsMargins(0, 4, 0, 0);

  bottomLayout->addStretch(1);

  m_bottomCloseBtn = new QPushButton(tr("Close"), bottomBar);
  m_bottomCloseBtn->setObjectName(QStringLiteral("AboutBottomCloseBtn"));
  m_bottomCloseBtn->setCursor(Qt::PointingHandCursor);
  m_bottomCloseBtn->setFixedHeight(32);
  connect(m_bottomCloseBtn, &QPushButton::clicked, this, &QDialog::accept);
  bottomLayout->addWidget(m_bottomCloseBtn);

  contentLayout->addWidget(bottomBar);
  cardLayout->addWidget(contentContainer);

  components::DialogDragHelper::makeDraggable(m_cardWidget, this);
  rootLayout->addWidget(m_cardWidget);
}

QWidget *AboutDialog::createInfoTab() {
  auto *tab = new QWidget(m_tabWidget);
  auto *layout = new QVBoxLayout(tab);
  layout->setContentsMargins(14, 14, 14, 14);
  layout->setSpacing(12);

  // Top branding section
  auto *brandWidget = new QWidget(tab);
  auto *brandLayout = new QHBoxLayout(brandWidget);
  brandLayout->setContentsMargins(0, 0, 0, 0);
  brandLayout->setSpacing(14);

  auto *bigLogo = new QLabel(brandWidget);
  bigLogo->setPixmap(
      QIcon(QStringLiteral(":/icons/app_logo.svg")).pixmap(52, 52));
  bigLogo->setFixedSize(56, 56);
  bigLogo->setAlignment(Qt::AlignCenter);
  brandLayout->addWidget(bigLogo);

  auto *titleCol = new QVBoxLayout();
  titleCol->setContentsMargins(0, 0, 0, 0);
  titleCol->setSpacing(2);

  m_appNameLabel =
      new QLabel(QStringLiteral("Secure Vault Manager"), brandWidget);
  QFont nameFont = m_appNameLabel->font();
  nameFont.setPointSize(14);
  nameFont.setBold(true);
  m_appNameLabel->setFont(nameFont);
  titleCol->addWidget(m_appNameLabel);

  m_versionLabel =
      new QLabel(tr("Version 0.1.0 • Rust Core Engine v0.1.0"), brandWidget);
  m_versionLabel->setObjectName(QStringLiteral("AboutMetaLabelVal"));
  titleCol->addWidget(m_versionLabel);

  m_taglineLabel =
      new QLabel(tr("Private, Fast & Secure Password Manager"), brandWidget);
  m_taglineLabel->setObjectName(QStringLiteral("AboutTaglineLabel"));
  m_taglineLabel->setWordWrap(true);
  titleCol->addWidget(m_taglineLabel);

  brandLayout->addLayout(titleCol, 1);
  layout->addWidget(brandWidget);

  // Description
  m_descLabel = new QLabel(tr("Secure Vault Manager is crafted with high "
                              "performance, minimal memory "
                              "footprint, offline privacy, and "
                              "native modern UI aesthetics "
                              "across desktop platforms."),
                           tab);
  m_descLabel->setWordWrap(true);
  m_descLabel->setObjectName(QStringLiteral("AboutDescLabel"));
  layout->addWidget(m_descLabel);

  // Divider
  auto *line = new QFrame(tab);
  line->setObjectName(QStringLiteral("AboutDividerLine"));
  line->setFrameShape(QFrame::HLine);
  line->setFrameShadow(QFrame::Plain);
  layout->addWidget(line);

  // Key-value metadata grid
  auto *gridWidget = new QWidget(tab);
  auto *gridLayout = new QGridLayout(gridWidget);
  gridLayout->setContentsMargins(0, 0, 0, 0);
  gridLayout->setHorizontalSpacing(12);
  gridLayout->setVerticalSpacing(6);

  m_authorKeyLabel = new QLabel(tr("Author:"), gridWidget);
  m_authorKeyLabel->setObjectName(QStringLiteral("AboutMetaLabelKey"));
  m_authorValLabel = new QLabel(QStringLiteral("Hai Pham Ngoc"), gridWidget);
  m_authorValLabel->setObjectName(QStringLiteral("AboutMetaLabelVal"));
  m_authorValLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
  gridLayout->addWidget(m_authorKeyLabel, 0, 0);
  gridLayout->addWidget(m_authorValLabel, 0, 1);

  m_emailKeyLabel = new QLabel(tr("Email:"), gridWidget);
  m_emailKeyLabel->setObjectName(QStringLiteral("AboutMetaLabelKey"));
  m_emailValLabel =
      new QLabel(QStringLiteral("ngochai285nd@gmail.com"), gridWidget);
  m_emailValLabel->setObjectName(QStringLiteral("AboutMetaLabelVal"));
  m_emailValLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
  gridLayout->addWidget(m_emailKeyLabel, 1, 0);
  gridLayout->addWidget(m_emailValLabel, 1, 1);

  m_orgKeyLabel = new QLabel(tr("Organization:"), gridWidget);
  m_orgKeyLabel->setObjectName(QStringLiteral("AboutMetaLabelKey"));
  m_orgValLabel =
      new QLabel(QStringLiteral("Hephaestus Developers"), gridWidget);
  m_orgValLabel->setObjectName(QStringLiteral("AboutMetaLabelVal"));
  m_orgValLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
  gridLayout->addWidget(m_orgKeyLabel, 2, 0);
  gridLayout->addWidget(m_orgValLabel, 2, 1);

  m_copyKeyLabel = new QLabel(tr("Copyright:"), gridWidget);
  m_copyKeyLabel->setObjectName(QStringLiteral("AboutMetaLabelKey"));
  m_copyValLabel = new QLabel(
      QStringLiteral("© 2026 Hephaestus Developers. All rights reserved."),
      gridWidget);
  m_copyValLabel->setObjectName(QStringLiteral("AboutMetaLabelVal"));
  m_copyValLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
  gridLayout->addWidget(m_copyKeyLabel, 3, 0);
  gridLayout->addWidget(m_copyValLabel, 3, 1);

  m_repoKeyLabel = new QLabel(tr("Repository:"), gridWidget);
  m_repoKeyLabel->setObjectName(QStringLiteral("AboutMetaLabelKey"));
  m_repoValLabel = new QLabel(
      QStringLiteral(
          "https://github.com/hephaestus-studio/secure-vault-manager"),
      gridWidget);
  m_repoValLabel->setObjectName(QStringLiteral("AboutMetaLabelVal"));
  m_repoValLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
  gridLayout->addWidget(m_repoKeyLabel, 4, 0);
  gridLayout->addWidget(m_repoValLabel, 4, 1);

  layout->addWidget(gridWidget);
  layout->addStretch(1);

  return tab;
}

QWidget *AboutDialog::createLicenseTab() {
  auto *tab = new QWidget(m_tabWidget);
  auto *layout = new QVBoxLayout(tab);
  layout->setContentsMargins(10, 10, 10, 10);

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
  if (m_tabWidget) {
    m_tabWidget->setTabText(0, tr("Information"));
    m_tabWidget->setTabText(1, tr("License"));
  }
  if (m_versionLabel) {
    m_versionLabel->setText(tr("Version 0.1.0 • Rust Core Engine v0.1.0"));
  }
  if (m_taglineLabel) {
    m_taglineLabel->setText(tr("Private, Fast & Secure Password Manager"));
  }
  if (m_descLabel) {
    m_descLabel->setText(tr(
        "Secure Vault Manager is crafted with high performance, minimal memory "
        "footprint, offline privacy, and native modern UI "
        "aesthetics "
        "across desktop platforms."));
  }
  if (m_authorKeyLabel) {
    m_authorKeyLabel->setText(tr("Author:"));
  }
  if (m_emailKeyLabel) {
    m_emailKeyLabel->setText(tr("Email:"));
  }
  if (m_orgKeyLabel) {
    m_orgKeyLabel->setText(tr("Organization:"));
  }
  if (m_copyKeyLabel) {
    m_copyKeyLabel->setText(tr("Copyright:"));
  }
  if (m_repoKeyLabel) {
    m_repoKeyLabel->setText(tr("Repository:"));
  }
  if (m_bottomCloseBtn) {
    m_bottomCloseBtn->setText(tr("Close"));
  }
}

void AboutDialog::changeEvent(QEvent *event) {
  if (event->type() == QEvent::LanguageChange) {
    retranslateUi();
  }
  QDialog::changeEvent(event);
}

void AboutDialog::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Escape) {
    reject();
    return;
  }
  QDialog::keyPressEvent(event);
}

} // namespace svm::views
