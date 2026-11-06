/**
 * @file ui/views/settings/settings_dialog.cpp
 * @brief Hiện thực giao diện Hộp thoại Cài đặt & Tùy chọn (SettingsDialog).
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "settings_dialog.h"
#include "app/theme_manager.h"

#include <QEvent>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QVBoxLayout>

namespace svm::views {

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent, Qt::FramelessWindowHint | Qt::WindowSystemMenuHint) {
  setObjectName(QStringLiteral("SettingsDialog"));
  setAttribute(Qt::WA_TranslucentBackground, true);
  setModal(true);
  setFixedSize(780, 560);

  setupUI();
  initNavList();
  selectTab(0);
}

void SettingsDialog::setupUI() {
  auto *rootLayout = new QVBoxLayout(this);
  rootLayout->setContentsMargins(16, 16, 16, 16);

  // Thẻ Card chính
  m_cardWidget = new QWidget(this);
  m_cardWidget->setObjectName(QStringLiteral("SettingsDialogCard"));

  auto *shadow = new QGraphicsDropShadowEffect(this);
  shadow->setBlurRadius(36);
  shadow->setColor(QColor(0, 0, 0, 180));
  shadow->setOffset(0, 10);
  m_cardWidget->setGraphicsEffect(shadow);

  auto *cardLayout = new QVBoxLayout(m_cardWidget);
  cardLayout->setContentsMargins(0, 0, 0, 0);
  cardLayout->setSpacing(0);

  // 1. Header Bar tích hợp
  m_headerWidget = new QWidget(m_cardWidget);
  m_headerWidget->setObjectName(QStringLiteral("SettingsDialogHeader"));
  m_headerWidget->setFixedHeight(44);

  auto *headerLayout = new QHBoxLayout(m_headerWidget);
  headerLayout->setContentsMargins(16, 0, 8, 0);
  headerLayout->setSpacing(10);

  auto *gearIcon = new QLabel(m_headerWidget);
  gearIcon->setFixedSize(18, 18);
  gearIcon->setPixmap(app::ThemeManager::instance()
                          .icon(QStringLiteral("category_settings"))
                          .pixmap(18, 18));
  headerLayout->addWidget(gearIcon);

  m_headerTitle = new QLabel(tr("Settings & Preferences"), m_headerWidget);
  m_headerTitle->setObjectName(QStringLiteral("SettingsDialogTitle"));
  QFont titleFont = m_headerTitle->font();
  titleFont.setPointSize(12);
  titleFont.setBold(true);
  m_headerTitle->setFont(titleFont);
  headerLayout->addWidget(m_headerTitle);

  headerLayout->addStretch(1);

  m_closeBtn = new QPushButton(m_headerWidget);
  m_closeBtn->setObjectName(QStringLiteral("WinCloseBtn"));
  m_closeBtn->setFixedSize(36, 32);
  m_closeBtn->setIconSize(QSize(12, 12));
  m_closeBtn->setCursor(Qt::PointingHandCursor);
  m_closeBtn->setIcon(
      app::ThemeManager::instance().icon(QStringLiteral("win_close")));
  connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::reject);
  headerLayout->addWidget(m_closeBtn);

  cardLayout->addWidget(m_headerWidget);

  // 2. Nội dung 2 cột Master-Detail
  auto *bodyWidget = new QWidget(m_cardWidget);
  auto *bodyLayout = new QHBoxLayout(bodyWidget);
  bodyLayout->setContentsMargins(0, 0, 0, 0);
  bodyLayout->setSpacing(0);

  // Cột trái: Navigation Sidebar (200px)
  m_navList = new QListWidget(bodyWidget);
  m_navList->setObjectName(QStringLiteral("SettingsNavList"));
  m_navList->setFixedWidth(200);
  m_navList->setFrameShape(QFrame::NoFrame);
  m_navList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_navList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_navList->setIconSize(QSize(16, 16));
  connect(m_navList, &QListWidget::currentRowChanged, this,
          &SettingsDialog::selectTab);
  bodyLayout->addWidget(m_navList);

  // Đường phân cách dọc
  auto *vLine = new QFrame(bodyWidget);
  vLine->setFrameShape(QFrame::VLine);
  vLine->setFrameShadow(QFrame::Plain);
  vLine->setStyleSheet(QStringLiteral("color: #313244;"));
  bodyLayout->addWidget(vLine);

  // Cột phải: Header danh mục + QScrollArea chứa StackedWidget
  auto *rightPane = new QWidget(bodyWidget);
  auto *rightLayout = new QVBoxLayout(rightPane);
  rightLayout->setContentsMargins(20, 16, 20, 16);
  rightLayout->setSpacing(12);

  m_rightCategoryTitle = new QLabel(rightPane);
  QFont catFont = m_rightCategoryTitle->font();
  catFont.setPointSize(14);
  catFont.setBold(true);
  m_rightCategoryTitle->setFont(catFont);
  m_rightCategoryTitle->setStyleSheet(QStringLiteral("color: #cdd6f4;"));
  rightLayout->addWidget(m_rightCategoryTitle);

  m_scrollArea = new QScrollArea(rightPane);
  m_scrollArea->setObjectName(QStringLiteral("SettingsScrollArea"));
  m_scrollArea->setFrameShape(QFrame::NoFrame);
  m_scrollArea->setWidgetResizable(true);
  m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  m_stack = new QStackedWidget(m_scrollArea);
  m_stack->setObjectName(QStringLiteral("SettingsTabStack"));

  m_tabGeneral = new TabGeneral(m_stack);
  m_tabSecurity = new TabSecurity(m_stack);
  m_tabVaults = new TabVaults(m_stack);
  m_tabBrowser = new TabBrowser(m_stack);
  m_tabSsh = new TabSsh(m_stack);
  m_tabData = new TabData(m_stack);

  m_stack->addWidget(m_tabGeneral);
  m_stack->addWidget(m_tabSecurity);
  m_stack->addWidget(m_tabVaults);
  m_stack->addWidget(m_tabBrowser);
  m_stack->addWidget(m_tabSsh);
  m_stack->addWidget(m_tabData);

  m_scrollArea->setWidget(m_stack);
  rightLayout->addWidget(m_scrollArea, 1);

  bodyLayout->addWidget(rightPane, 1);
  cardLayout->addWidget(bodyWidget, 1);

  rootLayout->addWidget(m_cardWidget);
}

void SettingsDialog::initNavList() {
  m_navList->clear();

  struct NavMeta {
    QString title;
    QString icon;
  };
  const NavMeta items[] = {
      {tr("General"), QStringLiteral("category_settings")},
      {tr("Security & Privacy"), QStringLiteral("shield_check")},
      {tr("Vaults Management"), QStringLiteral("folder_open")},
      {tr("Browser Integration"), QStringLiteral("category_server")},
      {tr("SSH Agent & Dev"), QStringLiteral("category_ssh")},
      {tr("Data & Backup"), QStringLiteral("trash")}};

  for (const auto &item : items) {
    auto *listItem = new QListWidgetItem(
        app::ThemeManager::instance().icon(item.icon), item.title, m_navList);
    listItem->setSizeHint(QSize(180, 36));
  }
}

void SettingsDialog::selectTab(int index) {
  if (index >= 0 && index < m_stack->count()) {
    m_stack->setCurrentIndex(index);
    updateCategoryHeader(index);
  }
}

void SettingsDialog::updateCategoryHeader(int index) {
  const QString titles[] = {tr("General Settings"),
                            tr("Security & Privacy"),
                            tr("Vaults Management"),
                            tr("Browser Integration"),
                            tr("SSH Agent & Developer Tools"),
                            tr("Data & Backup")};

  if (index >= 0 && index < 6) {
    m_rightCategoryTitle->setText(titles[index]);
  }
}

void SettingsDialog::retranslateUi() {
  if (m_headerTitle) {
    m_headerTitle->setText(tr("Settings & Preferences"));
  }

  const int cur = m_navList->currentRow();
  initNavList();
  if (cur >= 0) {
    m_navList->setCurrentRow(cur);
    updateCategoryHeader(cur);
  }

  if (m_tabGeneral)
    m_tabGeneral->retranslateUi();
  if (m_tabSecurity)
    m_tabSecurity->retranslateUi();
  if (m_tabVaults)
    m_tabVaults->retranslateUi();
  if (m_tabBrowser)
    m_tabBrowser->retranslateUi();
  if (m_tabSsh)
    m_tabSsh->retranslateUi();
  if (m_tabData)
    m_tabData->retranslateUi();
}

void SettingsDialog::changeEvent(QEvent *event) {
  if (event->type() == QEvent::LanguageChange) {
    retranslateUi();
  }
  QDialog::changeEvent(event);
}

void SettingsDialog::mousePressEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton && m_headerWidget) {
    if (m_headerWidget->geometry().contains(
            m_cardWidget->mapFrom(this, event->pos()))) {
      m_isDragging = true;
      m_dragPosition =
          event->globalPosition().toPoint() - frameGeometry().topLeft();
      event->accept();
      return;
    }
  }
  QDialog::mousePressEvent(event);
}

void SettingsDialog::mouseMoveEvent(QMouseEvent *event) {
  if (m_isDragging && (event->buttons() & Qt::LeftButton)) {
    move(event->globalPosition().toPoint() - m_dragPosition);
    event->accept();
    return;
  }
  QDialog::mouseMoveEvent(event);
}

void SettingsDialog::mouseReleaseEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    m_isDragging = false;
  }
  QDialog::mouseReleaseEvent(event);
}

void SettingsDialog::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Escape) {
    reject();
    return;
  }
  QDialog::keyPressEvent(event);
}

} // namespace svm::views
