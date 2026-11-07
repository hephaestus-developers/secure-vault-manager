/**
 * @file ui/views/settings/settings_dialog.cpp
 * @brief Hiện thực giao diện Hộp thoại Cài đặt & Tùy chọn (SettingsDialog).
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "settings_dialog.h"
#include "app/theme_manager.h"
#include "components/dialog_drag_helper.h"
#include "components/window_resize_handler.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QScrollBar>
#include <QVBoxLayout>

namespace svm::views {

namespace {

class SettingsTabStack : public QStackedWidget {
public:
  explicit SettingsTabStack(QWidget *parent = nullptr)
      : QStackedWidget(parent) {}

  QSize minimumSizeHint() const override {
    if (currentWidget()) {
      return currentWidget()->minimumSizeHint();
    }
    return QSize(0, 0);
  }

  QSize sizeHint() const override {
    if (currentWidget()) {
      return currentWidget()->sizeHint();
    }
    return QSize(0, 0);
  }
};

} // namespace

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent, Qt::FramelessWindowHint | Qt::Dialog) {
  setObjectName(QStringLiteral("SettingsDialog"));
  setAttribute(Qt::WA_TranslucentBackground, false);
  setModal(false);
  setWindowModality(Qt::NonModal);
  setMinimumSize(760, 500);
  resize(860, 580);

  setupUI();
  initNavList();
  selectTab(0);

  new components::WindowResizeHandler(this);
}

void SettingsDialog::setupUI() {
  auto *rootLayout = new QVBoxLayout(this);
  rootLayout->setContentsMargins(0, 0, 0, 0);

  // Thẻ Card chính
  m_cardWidget = new QWidget(this);
  m_cardWidget->setObjectName(QStringLiteral("SettingsDialogCard"));
  m_cardWidget->setAttribute(Qt::WA_StyledBackground, true);

  auto *cardLayout = new QVBoxLayout(m_cardWidget);
  cardLayout->setContentsMargins(0, 0, 0, 0);
  cardLayout->setSpacing(0);

  // 1. Header Bar tích hợp
  m_headerWidget = new QWidget(m_cardWidget);
  m_headerWidget->setObjectName(QStringLiteral("SettingsDialogHeader"));
  m_headerWidget->setAttribute(Qt::WA_StyledBackground, true);
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

  components::DialogDragHelper::makeDraggable(m_headerWidget, this);

  cardLayout->addWidget(m_headerWidget);

  // 2. Nội dung 2 cột Master-Detail
  auto *bodyWidget = new QWidget(m_cardWidget);
  bodyWidget->setObjectName(QStringLiteral("SettingsBodyWidget"));
  bodyWidget->setAttribute(Qt::WA_StyledBackground, true);
  auto *bodyLayout = new QHBoxLayout(bodyWidget);
  bodyLayout->setContentsMargins(0, 0, 0, 0);
  bodyLayout->setSpacing(0);

  // Cột trái: Navigation Sidebar (240px)
  m_navList = new QListWidget(bodyWidget);
  m_navList->setObjectName(QStringLiteral("SettingsNavList"));
  m_navList->setFixedWidth(240);
  m_navList->setFrameShape(QFrame::NoFrame);
  m_navList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_navList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_navList->setIconSize(QSize(16, 16));
  connect(m_navList, &QListWidget::currentRowChanged, this,
          &SettingsDialog::selectTab);
  bodyLayout->addWidget(m_navList);

  // Đường phân cách dọc
  auto *vLine = new QFrame(bodyWidget);
  vLine->setObjectName(QStringLiteral("SettingsVerticalDivider"));
  vLine->setFixedWidth(1);
  vLine->setFrameShape(QFrame::VLine);
  vLine->setFrameShadow(QFrame::Plain);
  bodyLayout->addWidget(vLine);

  // Cột phải: QScrollArea chứa StackedWidget
  auto *rightPane = new QWidget(bodyWidget);
  rightPane->setObjectName(QStringLiteral("SettingsRightPane"));
  rightPane->setAttribute(Qt::WA_StyledBackground, true);
  auto *rightLayout = new QVBoxLayout(rightPane);
  rightLayout->setContentsMargins(20, 16, 20, 16);
  rightLayout->setSpacing(0);

  m_scrollArea = new QScrollArea(rightPane);
  m_scrollArea->setObjectName(QStringLiteral("SettingsScrollArea"));
  m_scrollArea->setFrameShape(QFrame::NoFrame);
  m_scrollArea->setWidgetResizable(true);
  m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_scrollArea->viewport()->setObjectName(
      QStringLiteral("SettingsScrollViewport"));
  m_scrollArea->viewport()->setAttribute(Qt::WA_StyledBackground, true);

  m_stack = new SettingsTabStack(m_scrollArea);
  m_stack->setObjectName(QStringLiteral("SettingsTabStack"));
  m_stack->setAttribute(Qt::WA_StyledBackground, true);

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

  connect(&app::ThemeManager::instance(), &app::ThemeManager::themeChanged,
          this, [this, gearIcon]() {
            if (gearIcon) {
              gearIcon->setPixmap(app::ThemeManager::instance()
                                      .icon(QStringLiteral("category_settings"))
                                      .pixmap(18, 18));
            }
            if (m_closeBtn) {
              m_closeBtn->setIcon(app::ThemeManager::instance().icon(
                  QStringLiteral("win_close")));
            }
            updateNavListIcons();
          });
}

void SettingsDialog::updateNavListIcons() {
  if (!m_navList) {
    return;
  }
  const QString iconNames[] = {
      QStringLiteral("category_settings"), QStringLiteral("shield_check"),
      QStringLiteral("folder_open"),       QStringLiteral("category_server"),
      QStringLiteral("category_ssh"),      QStringLiteral("trash"),
  };
  for (int i = 0; i < m_navList->count() && i < 6; ++i) {
    if (auto *item = m_navList->item(i)) {
      item->setIcon(app::ThemeManager::instance().icon(iconNames[i]));
    }
  }
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
    listItem->setSizeHint(QSize(224, 36));
  }
}

void SettingsDialog::selectTab(int index) {
  if (index >= 0 && index < m_stack->count()) {
    m_stack->setCurrentIndex(index);
    m_stack->updateGeometry();
    if (m_scrollArea && m_scrollArea->verticalScrollBar()) {
      m_scrollArea->verticalScrollBar()->setValue(0);
    }
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

void SettingsDialog::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Escape) {
    reject();
    return;
  }
  QDialog::keyPressEvent(event);
}

} // namespace svm::views
