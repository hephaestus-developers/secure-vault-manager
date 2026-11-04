/**
 * @file ui/components/tray_launcher/tray_launcher.cpp
 * @brief Hiện thực chi tiết TrayLauncher cửa sổ bật nhanh tìm kiếm và thao tác.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "tray_launcher.h"

#include <QEvent>
#include <QGraphicsDropShadowEffect>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeyEvent>
#include <QScreen>
#include <QVBoxLayout>

namespace svm::components {

namespace {

class LauncherItemWidget : public QWidget {
public:
  LauncherItemWidget(const QString &iconPath, const QString &title,
                     const QString &subtitle, const QString &shortcut,
                     QWidget *parent = nullptr)
      : QWidget(parent) {
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 6, 8, 6);
    layout->setSpacing(12);

    auto *iconLabel = new QLabel(this);
    iconLabel->setFixedSize(22, 22);
    iconLabel->setPixmap(QIcon(iconPath).pixmap(20, 20));
    iconLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(iconLabel);

    auto *textLayout = new QVBoxLayout();
    textLayout->setSpacing(2);
    textLayout->setContentsMargins(0, 0, 0, 0);

    auto *titleLabel = new QLabel(title, this);
    titleLabel->setObjectName(QStringLiteral("TrayLauncherItemTitle"));
    textLayout->addWidget(titleLabel);

    if (!subtitle.isEmpty()) {
      auto *subLabel = new QLabel(subtitle, this);
      subLabel->setObjectName(QStringLiteral("TrayLauncherItemSub"));
      textLayout->addWidget(subLabel);
    }
    layout->addLayout(textLayout, 1);

    if (!shortcut.isEmpty()) {
      auto *shortcutLabel = new QLabel(shortcut, this);
      shortcutLabel->setObjectName(QStringLiteral("TrayLauncherKeyBadge"));
      shortcutLabel->setAlignment(Qt::AlignCenter);
      layout->addWidget(shortcutLabel);
    }
  }
};

} // namespace

TrayLauncher::TrayLauncher(QWidget *parent)
    : QDialog(parent,
              Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint) {
  setAttribute(Qt::WA_TranslucentBackground);
  setObjectName(QStringLiteral("TrayLauncherWindow"));
  setFixedSize(520, 430);

  setupUI();
  populateActions();
}

void TrayLauncher::setupUI() {
  auto *outerLayout = new QVBoxLayout(this);
  outerLayout->setContentsMargins(10, 10, 10, 10);

  // Khung chứa chính (Container)
  m_containerWidget = new QWidget(this);
  m_containerWidget->setObjectName(QStringLiteral("TrayLauncherContainer"));

  // Hiệu ứng đổ bóng mượt
  auto *shadow = new QGraphicsDropShadowEffect(m_containerWidget);
  shadow->setBlurRadius(20);
  shadow->setColor(QColor(0, 0, 0, 120));
  shadow->setOffset(0, 6);
  m_containerWidget->setGraphicsEffect(shadow);

  auto *containerLayout = new QVBoxLayout(m_containerWidget);
  containerLayout->setContentsMargins(0, 0, 0, 0);
  containerLayout->setSpacing(0);

  // 1. Header
  auto *headerWidget = new QWidget(m_containerWidget);
  headerWidget->setObjectName(QStringLiteral("TrayLauncherHeader"));
  auto *headerLayout = new QHBoxLayout(headerWidget);
  headerLayout->setContentsMargins(14, 8, 14, 8);
  headerLayout->setSpacing(10);

  auto *logoLabel = new QLabel(headerWidget);
  logoLabel->setPixmap(
      QIcon(QStringLiteral(":/icons/app_logo.svg")).pixmap(18, 18));
  headerLayout->addWidget(logoLabel);

  auto *titleLabel = new QLabel(tr("Secure Vault Launcher"), headerWidget);
  titleLabel->setObjectName(QStringLiteral("TrayLauncherTitle"));
  headerLayout->addWidget(titleLabel);

  m_statusBadge = new QLabel(headerWidget);
  m_statusBadge->setObjectName(QStringLiteral("TrayLauncherStatusBadge"));
  headerLayout->addWidget(m_statusBadge);
  headerLayout->addStretch(1);

  auto *escLabel = new QLabel(tr("Esc to close"), headerWidget);
  escLabel->setObjectName(QStringLiteral("TrayLauncherKeyBadge"));
  headerLayout->addWidget(escLabel);

  containerLayout->addWidget(headerWidget);

  // 2. Ô tìm kiếm (Search Box)
  auto *searchBoxWidget = new QWidget(m_containerWidget);
  searchBoxWidget->setObjectName(QStringLiteral("TrayLauncherSearchBox"));
  auto *searchBoxLayout = new QHBoxLayout(searchBoxWidget);
  searchBoxLayout->setContentsMargins(8, 2, 8, 2);
  searchBoxLayout->setSpacing(8);

  auto *searchIconLabel = new QLabel(searchBoxWidget);
  searchIconLabel->setPixmap(
      QIcon(QStringLiteral(":/icons/search.svg")).pixmap(16, 16));
  searchBoxLayout->addWidget(searchIconLabel);

  m_searchEdit = new QLineEdit(searchBoxWidget);
  m_searchEdit->setObjectName(QStringLiteral("TrayLauncherSearchEdit"));
  m_searchEdit->setPlaceholderText(
      tr("Type to search vault items or execute actions..."));
  m_searchEdit->installEventFilter(this);
  connect(m_searchEdit, &QLineEdit::textChanged, this,
          &TrayLauncher::onSearchTextChanged);
  searchBoxLayout->addWidget(m_searchEdit, 1);

  containerLayout->addWidget(searchBoxWidget);

  // 3. Danh sách mục thao tác & dữ liệu (Action List)
  m_listWidget = new QListWidget(m_containerWidget);
  m_listWidget->setObjectName(QStringLiteral("TrayLauncherList"));
  m_listWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  m_listWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  connect(m_listWidget, &QListWidget::itemDoubleClicked, this,
          &TrayLauncher::onItemDoubleClicked);
  containerLayout->addWidget(m_listWidget, 1);

  // 4. Footer
  auto *footerWidget = new QWidget(m_containerWidget);
  footerWidget->setObjectName(QStringLiteral("TrayLauncherFooter"));
  auto *footerLayout = new QHBoxLayout(footerWidget);
  footerLayout->setContentsMargins(14, 6, 14, 6);

  auto *footerAppLabel =
      new QLabel(tr("Secure Vault Manager • Native Desktop"), footerWidget);
  footerLayout->addWidget(footerAppLabel);
  footerLayout->addStretch(1);

  auto *footerHintLabel =
      new QLabel(tr("↑↓ Navigate • ↵ Open • Esc Close"), footerWidget);
  footerLayout->addWidget(footerHintLabel);

  containerLayout->addWidget(footerWidget);
  outerLayout->addWidget(m_containerWidget);

  updateStatusBadge();
}

void TrayLauncher::populateActions() {
  m_listWidget->clear();

  addActionItem(
      LauncherActionId::OpenMainWindow, QStringLiteral(":/icons/app_logo.svg"),
      tr("Open Secure Vault Manager"),
      tr("Bring primary dashboard to foreground"), QStringLiteral("↵"));

  addActionItem(LauncherActionId::ToggleVaultLock,
                m_isVaultLocked ? QStringLiteral(":/icons/unlock.svg")
                                : QStringLiteral(":/icons/lock.svg"),
                m_isVaultLocked ? tr("Unlock Vault") : tr("Lock Vault"),
                m_isVaultLocked
                    ? tr("Unlock memory structures with master key")
                    : tr("Zeroize memory and lock database immediately"),
                QStringLiteral("Ctrl+L"));

  addActionItem(LauncherActionId::SearchCredentials,
                QStringLiteral(":/icons/category_all.svg"),
                tr("Browse All Vault Items"),
                tr("Search logins, credit cards, secure notes & passkeys"),
                QStringLiteral(""));

  addActionItem(LauncherActionId::NewCredential,
                QStringLiteral(":/icons/plus.svg"),
                tr("Create New Credential..."),
                tr("Store a new login password or private data"),
                QStringLiteral("Ctrl+N"));

  addActionItem(LauncherActionId::PasswordGenerator,
                QStringLiteral(":/icons/shield_check.svg"),
                tr("Password Generator..."),
                tr("Generate cryptographically secure high-entropy secrets"),
                QStringLiteral(""));

  addActionItem(LauncherActionId::ToggleTheme,
                QStringLiteral(":/icons/category_settings.svg"),
                tr("Toggle Theme (Dark / Light)"),
                tr("Switch between Catppuccin Mocha and Latte schemes"),
                QStringLiteral("Ctrl+T"));

  addActionItem(LauncherActionId::QuitApplication,
                QStringLiteral(":/icons/win_close.svg"), tr("Quit Application"),
                tr("Close vault service and exit tray"),
                QStringLiteral("Ctrl+Q"));

  if (m_listWidget->count() > 0) {
    m_listWidget->setCurrentRow(0);
  }
}

void TrayLauncher::addActionItem(LauncherActionId id, const QString &iconPath,
                                 const QString &title, const QString &subtitle,
                                 const QString &shortcut) {
  auto *item = new QListWidgetItem(m_listWidget);
  item->setData(Qt::UserRole, static_cast<int>(id));
  item->setData(Qt::UserRole + 1, title);
  item->setData(Qt::UserRole + 2, subtitle);
  item->setSizeHint(QSize(0, 52));

  auto *widget =
      new LauncherItemWidget(iconPath, title, subtitle, shortcut, m_listWidget);
  m_listWidget->addItem(item);
  m_listWidget->setItemWidget(item, widget);
}

void TrayLauncher::setVaultLocked(bool locked) {
  m_isVaultLocked = locked;
  updateStatusBadge();
  populateActions();
}

void TrayLauncher::updateStatusBadge() {
  if (!m_statusBadge) {
    return;
  }
  if (m_isVaultLocked) {
    m_statusBadge->setProperty("locked", true);
    m_statusBadge->setText(tr("● Locked"));
  } else {
    m_statusBadge->setProperty("locked", false);
    m_statusBadge->setText(tr("● Unlocked"));
  }
  m_statusBadge->style()->unpolish(m_statusBadge);
  m_statusBadge->style()->polish(m_statusBadge);
}

void TrayLauncher::showNearTray(const QRect &trayGeometry) {
  const int w = width();
  const int h = height();

  QScreen *screen = nullptr;
  if (trayGeometry.isValid()) {
    screen = QGuiApplication::screenAt(trayGeometry.center());
  }
  if (!screen) {
    screen = QGuiApplication::primaryScreen();
  }

  if (!screen) {
    showCentered();
    return;
  }

  const QRect screenGeom = screen->availableGeometry();

  if (!trayGeometry.isValid()) {
    int x = screenGeom.x() + (screenGeom.width() - w) / 2;
    int y = screenGeom.y() + (screenGeom.height() - h) / 4;
    move(x, y);
  } else {
    int x = trayGeometry.center().x() - w / 2;
    x = std::max(screenGeom.left() + 10,
                 std::min(x, screenGeom.right() - w - 10));

    int y = 0;
    if (trayGeometry.center().y() < screenGeom.center().y()) {
      y = trayGeometry.bottom() + 6;
    } else {
      y = trayGeometry.top() - h - 6;
    }
    y = std::max(screenGeom.top() + 10,
                 std::min(y, screenGeom.bottom() - h - 10));
    move(x, y);
  }

  if (m_searchEdit) {
    m_searchEdit->clear();
    m_searchEdit->setFocus();
  }

  show();
  raise();
  activateWindow();
}

void TrayLauncher::showCentered() {
  const int w = width();
  const int h = height();
  QScreen *screen = QGuiApplication::primaryScreen();
  if (screen) {
    const QRect screenGeom = screen->availableGeometry();
    int x = screenGeom.x() + (screenGeom.width() - w) / 2;
    int y = screenGeom.y() + (screenGeom.height() - h) / 4;
    move(x, y);
  }

  if (m_searchEdit) {
    m_searchEdit->clear();
    m_searchEdit->setFocus();
  }

  show();
  raise();
  activateWindow();
}

void TrayLauncher::onSearchTextChanged(const QString &text) {
  const QString query = text.trimmed();
  int visibleCount = 0;
  QListWidgetItem *firstVisible = nullptr;

  for (int i = 0; i < m_listWidget->count(); ++i) {
    auto *item = m_listWidget->item(i);
    const QString title = item->data(Qt::UserRole + 1).toString();
    const QString subtitle = item->data(Qt::UserRole + 2).toString();

    bool match = query.isEmpty() ||
                 title.contains(query, Qt::CaseInsensitive) ||
                 subtitle.contains(query, Qt::CaseInsensitive);

    item->setHidden(!match);
    if (match) {
      if (!firstVisible) {
        firstVisible = item;
      }
      visibleCount++;
    }
  }

  if (firstVisible) {
    m_listWidget->setCurrentItem(firstVisible);
  }
}

void TrayLauncher::onItemDoubleClicked(QListWidgetItem *item) {
  Q_UNUSED(item);
  executeCurrentAction();
}

void TrayLauncher::executeCurrentAction() {
  auto *item = m_listWidget->currentItem();
  if (!item || item->isHidden()) {
    return;
  }

  auto id = static_cast<LauncherActionId>(item->data(Qt::UserRole).toInt());
  hide();

  switch (id) {
  case LauncherActionId::OpenMainWindow:
    emit openMainWindowRequested();
    break;
  case LauncherActionId::ToggleVaultLock:
    emit toggleVaultLockRequested();
    break;
  case LauncherActionId::SearchCredentials:
    emit searchCredentialsRequested();
    break;
  case LauncherActionId::NewCredential:
    emit newCredentialRequested();
    break;
  case LauncherActionId::PasswordGenerator:
    emit passwordGeneratorRequested();
    break;
  case LauncherActionId::ToggleTheme:
    emit toggleThemeRequested();
    break;
  case LauncherActionId::QuitApplication:
    emit quitRequested();
    break;
  }
}

void TrayLauncher::changeEvent(QEvent *event) {
  if (event->type() == QEvent::ActivationChange) {
    if (!isActiveWindow()) {
      hide();
    }
  }
  QDialog::changeEvent(event);
}

void TrayLauncher::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Escape) {
    hide();
    event->accept();
    return;
  }
  QDialog::keyPressEvent(event);
}

bool TrayLauncher::eventFilter(QObject *watched, QEvent *event) {
  if (watched == m_searchEdit && event->type() == QEvent::KeyPress) {
    auto *keyEvent = static_cast<QKeyEvent *>(event);
    if (keyEvent->key() == Qt::Key_Down) {
      int nextRow = m_listWidget->currentRow() + 1;
      while (nextRow < m_listWidget->count() &&
             m_listWidget->item(nextRow)->isHidden()) {
        nextRow++;
      }
      if (nextRow < m_listWidget->count()) {
        m_listWidget->setCurrentRow(nextRow);
      }
      return true;
    } else if (keyEvent->key() == Qt::Key_Up) {
      int prevRow = m_listWidget->currentRow() - 1;
      while (prevRow >= 0 && m_listWidget->item(prevRow)->isHidden()) {
        prevRow--;
      }
      if (prevRow >= 0) {
        m_listWidget->setCurrentRow(prevRow);
      }
      return true;
    } else if (keyEvent->key() == Qt::Key_Return ||
               keyEvent->key() == Qt::Key_Enter) {
      executeCurrentAction();
      return true;
    } else if (keyEvent->key() == Qt::Key_Escape) {
      hide();
      return true;
    }
  }
  return QDialog::eventFilter(watched, event);
}

} // namespace svm::components
