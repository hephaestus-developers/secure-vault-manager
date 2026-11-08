/**
 * @file ui/views/item_list/item_list_empty_widget.cpp
 * @brief Hiện thực ItemListEmptyWidget cho cột danh sách bản ghi.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "item_list_empty_widget.h"
#include "app/theme_manager.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QVBoxLayout>

namespace svm::views {

ItemListEmptyWidget::ItemListEmptyWidget(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("ItemListEmptyWidget"));
  setAttribute(Qt::WA_StyledBackground, true);

  setupUI();

  connect(&app::ThemeManager::instance(), &app::ThemeManager::themeChanged,
          this, [this]() { updateDisplay(); });
}

void ItemListEmptyWidget::setupUI() {
  auto *rootLayout = new QVBoxLayout(this);
  rootLayout->setContentsMargins(18, 20, 18, 20);
  rootLayout->setSpacing(0);

  rootLayout->addStretch(2);

  // 1. Icon Container
  auto *iconRow = new QHBoxLayout();
  iconRow->setContentsMargins(0, 0, 0, 0);
  iconRow->setAlignment(Qt::AlignCenter);

  m_iconContainer = new QWidget(this);
  m_iconContainer->setObjectName(QStringLiteral("ItemListEmptyIconContainer"));
  m_iconContainer->setAttribute(Qt::WA_StyledBackground, true);
  m_iconContainer->setFixedSize(54, 54);

  auto *iconLayout = new QVBoxLayout(m_iconContainer);
  iconLayout->setContentsMargins(0, 0, 0, 0);
  iconLayout->setAlignment(Qt::AlignCenter);

  m_iconLabel = new QLabel(m_iconContainer);
  m_iconLabel->setAlignment(Qt::AlignCenter);
  iconLayout->addWidget(m_iconLabel);

  iconRow->addWidget(m_iconContainer);
  rootLayout->addLayout(iconRow);

  rootLayout->addSpacing(14);

  // 2. Title Label
  m_titleLabel = new QLabel(this);
  m_titleLabel->setObjectName(QStringLiteral("ItemListEmptyTitle"));
  m_titleLabel->setAlignment(Qt::AlignCenter);
  rootLayout->addWidget(m_titleLabel);

  rootLayout->addSpacing(8);

  // 3. Description Label
  m_descLabel = new QLabel(this);
  m_descLabel->setObjectName(QStringLiteral("ItemListEmptyDesc"));
  m_descLabel->setAlignment(Qt::AlignCenter);
  m_descLabel->setWordWrap(true);
  m_descLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  rootLayout->addWidget(m_descLabel);

  // 4. Action Button (Only shown for non-add actions, e.g. Clear Search)
  m_actionContainer = new QWidget(this);
  auto *btnRow = new QHBoxLayout(m_actionContainer);
  btnRow->setContentsMargins(0, 18, 0, 0);
  btnRow->setAlignment(Qt::AlignCenter);

  m_actionBtn = new QPushButton(m_actionContainer);
  m_actionBtn->setObjectName(QStringLiteral("ItemListEmptyActionBtn"));
  m_actionBtn->setCursor(Qt::PointingHandCursor);
  m_actionBtn->setFixedHeight(32);
  connect(m_actionBtn, &QPushButton::clicked, this, [this]() {
    if (m_isSearch) {
      emit clearSearchRequested();
    }
  });
  btnRow->addWidget(m_actionBtn);
  rootLayout->addWidget(m_actionContainer);

  rootLayout->addStretch(2);

  updateDisplay();
}

void ItemListEmptyWidget::setState(SidebarFilterType filterType,
                                   models::ItemCategory category, bool isSearch,
                                   const QString &searchQuery) {
  m_filterType = filterType;
  m_category = category;
  m_isSearch = isSearch;
  m_searchQuery = searchQuery;

  updateDisplay();
}

void ItemListEmptyWidget::updateDisplay() {
  if (!m_iconLabel || !m_titleLabel || !m_descLabel || !m_actionBtn ||
      !m_actionContainer) {
    return;
  }

  auto &tm = app::ThemeManager::instance();
  QString iconName;
  QString title;
  QString desc;
  QString btnText;
  bool showBtn = false;

  if (m_isSearch) {
    iconName = QStringLiteral("search");
    title = tr("No Matching Items");
    desc = m_searchQuery.isEmpty()
               ? tr("No items matched your search query.")
               : tr("No results found for \"%1\".").arg(m_searchQuery);
    btnText = tr("Clear Search");
    showBtn = true;
  } else if (m_filterType == SidebarFilterType::Trash) {
    iconName = QStringLiteral("category_trash");
    title = tr("Trash is Empty");
    desc = tr("Items you delete will be kept here until you empty the trash.");
  } else if (m_filterType == SidebarFilterType::Favorites) {
    iconName = QStringLiteral("star");
    title = tr("No Favorites Yet");
    desc = tr("Star any important credential to quickly access it here.");
  } else if (m_filterType == SidebarFilterType::WatchtowerWeak) {
    iconName = QStringLiteral("watchtower_weak");
    title = tr("No Weak Passwords");
    desc = tr("Great job! All your stored credentials meet strong standards.");
  } else if (m_filterType == SidebarFilterType::WatchtowerExpiring) {
    iconName = QStringLiteral("watchtower_expiring");
    title = tr("No Expiring Items");
    desc = tr("None of your credit cards or credentials have upcoming "
              "expiration dates.");
  } else if (m_filterType == SidebarFilterType::Category) {
    switch (m_category) {
    case models::ItemCategory::Login:
      iconName = QStringLiteral("category_login");
      title = tr("No Logins Yet");
      desc = tr("Save web and app credentials for quick auto-fill and secure "
                "access.");
      break;
    case models::ItemCategory::Passkey:
      iconName = QStringLiteral("category_passkey");
      title = tr("No Passkeys Yet");
      desc = tr(
          "Add cryptographic passkeys to sign in securely without passwords.");
      break;
    case models::ItemCategory::Card:
      iconName = QStringLiteral("category_card");
      title = tr("No Cards Yet");
      desc = tr(
          "Keep credit and debit cards protected with end-to-end encryption.");
      break;
    case models::ItemCategory::Identity:
      iconName = QStringLiteral("category_identity");
      title = tr("No Identities Yet");
      desc = tr("Store passports, IDs, and licenses for fast form filling.");
      break;
    case models::ItemCategory::SecureNote:
      iconName = QStringLiteral("category_note");
      title = tr("No Secure Notes Yet");
      desc = tr(
          "Securely store private memos, recovery codes, and sensitive text.");
      break;
    case models::ItemCategory::SshKey:
      iconName = QStringLiteral("category_ssh");
      title = tr("No SSH Keys Yet");
      desc = tr("Store or generate SSH key pairs for secure terminal access.");
      break;
    case models::ItemCategory::Database:
      iconName = QStringLiteral("category_database");
      title = tr("No Databases Yet");
      desc = tr(
          "Store database hosts, ports, connection strings, and credentials.");
      break;
    case models::ItemCategory::Server:
      iconName = QStringLiteral("category_server");
      title = tr("No Servers Yet");
      desc =
          tr("Manage infrastructure endpoints, VPS logins, and admin access.");
      break;
    case models::ItemCategory::Wifi:
      iconName = QStringLiteral("category_wifi");
      title = tr("No Wi-Fi Networks Yet");
      desc = tr("Save Wi-Fi network SSIDs and passwords for your places.");
      break;
    }
  } else {
    iconName = QStringLiteral("folder_empty");
    title = tr("No Items in Vault");
    desc =
        tr("Create your first credential to start securing your digital life.");
  }

  m_iconLabel->setPixmap(tm.icon(iconName).pixmap(26, 26));
  m_titleLabel->setText(title);
  m_descLabel->setText(desc);
  m_actionBtn->setText(btnText);
  m_actionContainer->setVisible(showBtn);
}

void ItemListEmptyWidget::retranslateUi() { updateDisplay(); }

void ItemListEmptyWidget::changeEvent(QEvent *event) {
  if (event->type() == QEvent::LanguageChange) {
    retranslateUi();
  }
  QWidget::changeEvent(event);
}

} // namespace svm::views
