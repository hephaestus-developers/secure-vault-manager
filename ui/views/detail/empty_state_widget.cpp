/**
 * @file ui/views/detail/empty_state_widget.cpp
 * @brief Hiện thực giao diện EmptyStateWidget thẩm mỹ cao theo phong cách hiện
 * đại.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "empty_state_widget.h"
#include "app/theme_manager.h"

#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace svm::views {

EmptyStateWidget::EmptyStateWidget(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("EmptyStateWidget"));
  setAttribute(Qt::WA_StyledBackground, true);
  setupUI();

  connect(&app::ThemeManager::instance(), &app::ThemeManager::themeChanged,
          this, [this]() { updateThemeIcon(); });
}

void EmptyStateWidget::setupUI() {
  auto *rootLayout = new QVBoxLayout(this);
  rootLayout->setAlignment(Qt::AlignCenter);
  rootLayout->setContentsMargins(24, 24, 24, 24);
  rootLayout->setSpacing(0);

  // Container trung tâm giới hạn chiều rộng
  auto *centerContainer = new QWidget(this);
  centerContainer->setMaximumWidth(380);
  auto *centerLayout = new QVBoxLayout(centerContainer);
  centerLayout->setAlignment(Qt::AlignCenter);
  centerLayout->setContentsMargins(0, 0, 0, 0);
  centerLayout->setSpacing(14);

  // 1. Icon Container bo tròn hiện đại (72x72)
  m_iconContainer = new QWidget(centerContainer);
  m_iconContainer->setObjectName(QStringLiteral("EmptyStateIconContainer"));
  m_iconContainer->setFixedSize(72, 72);
  m_iconContainer->setAttribute(Qt::WA_StyledBackground, true);

  auto *iconLayout = new QVBoxLayout(m_iconContainer);
  iconLayout->setContentsMargins(0, 0, 0, 0);
  iconLayout->setAlignment(Qt::AlignCenter);

  m_iconLabel = new QLabel(m_iconContainer);
  m_iconLabel->setAlignment(Qt::AlignCenter);
  iconLayout->addWidget(m_iconLabel);
  centerLayout->addWidget(m_iconContainer, 0, Qt::AlignCenter);

  // 2. Tiêu đề chính
  m_titleLabel = new QLabel(tr("No Item Selected"), centerContainer);
  m_titleLabel->setObjectName(QStringLiteral("EmptyStateTitle"));
  m_titleLabel->setAlignment(Qt::AlignCenter);
  QFont titleFont = m_titleLabel->font();
  titleFont.setPointSize(14);
  titleFont.setBold(true);
  m_titleLabel->setFont(titleFont);
  centerLayout->addWidget(m_titleLabel);

  // 3. Mô tả phụ
  m_descLabel = new QLabel(tr("Select an item from the list to view its "
                              "details, or use the quick shortcuts below."),
                           centerContainer);
  m_descLabel->setObjectName(QStringLiteral("EmptyStateDesc"));
  m_descLabel->setAlignment(Qt::AlignCenter);
  m_descLabel->setWordWrap(true);
  centerLayout->addWidget(m_descLabel);

  centerLayout->addSpacing(4);

  // 4. Bảng danh sách phím tắt dạng Card hiện đại
  m_cardWidget = new QWidget(centerContainer);
  m_cardWidget->setObjectName(QStringLiteral("EmptyStateCard"));
  m_cardWidget->setAttribute(Qt::WA_StyledBackground, true);

  auto *cardLayout = new QVBoxLayout(m_cardWidget);
  cardLayout->setContentsMargins(16, 12, 16, 12);
  cardLayout->setSpacing(8);

  auto createShortcutRow =
      [this, cardLayout](const QString &actionText, const QString &key1,
                         const QString &key2, QLabel *&actionLbl) {
        auto *row = new QWidget(m_cardWidget);
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 2, 0, 2);
        rowLayout->setSpacing(8);

        actionLbl = new QLabel(actionText, row);
        actionLbl->setObjectName(QStringLiteral("EmptyStateActionLabel"));
        rowLayout->addWidget(actionLbl, 1);

        auto *keysLayout = new QHBoxLayout();
        keysLayout->setContentsMargins(0, 0, 0, 0);
        keysLayout->setSpacing(4);

        auto *k1 = new QLabel(key1, row);
        k1->setObjectName(QStringLiteral("EmptyStateKeycap"));
        k1->setAlignment(Qt::AlignCenter);
        keysLayout->addWidget(k1);

        if (!key2.isEmpty()) {
          auto *plus = new QLabel(QStringLiteral("+"), row);
          plus->setObjectName(QStringLiteral("EmptyStateKeycapPlus"));
          plus->setAlignment(Qt::AlignCenter);
          keysLayout->addWidget(plus);

          auto *k2 = new QLabel(key2, row);
          k2->setObjectName(QStringLiteral("EmptyStateKeycap"));
          k2->setAlignment(Qt::AlignCenter);
          keysLayout->addWidget(k2);
        }

        rowLayout->addLayout(keysLayout);
        cardLayout->addWidget(row);
      };

  auto createDivider = [this, cardLayout]() {
    auto *divider = new QFrame(m_cardWidget);
    divider->setObjectName(QStringLiteral("EmptyStateDivider"));
    divider->setFrameShape(QFrame::HLine);
    divider->setFrameShadow(QFrame::Plain);
    cardLayout->addWidget(divider);
  };

  createShortcutRow(tr("Create new item"), QStringLiteral("Ctrl"),
                    QStringLiteral("N"), m_actionNewItemLabel);
  createDivider();
  createShortcutRow(tr("Search vault items"), QStringLiteral("Ctrl"),
                    QStringLiteral("F"), m_actionSearchLabel);
  createDivider();
  createShortcutRow(tr("Toggle navigation sidebar"), QStringLiteral("Ctrl"),
                    QStringLiteral("B"), m_actionSidebarLabel);

  centerLayout->addWidget(m_cardWidget);

  centerLayout->addSpacing(4);

  // 5. Nút bấm tạo bản ghi nhanh
  m_newItemBtn = new QPushButton(centerContainer);
  m_newItemBtn->setObjectName(QStringLiteral("EmptyStateActionBtn"));
  m_newItemBtn->setText(tr("Create New Item"));
  m_newItemBtn->setCursor(Qt::PointingHandCursor);
  m_newItemBtn->setIconSize(QSize(14, 14));
  m_newItemBtn->setFixedHeight(38);
  m_newItemBtn->setMinimumWidth(240);
  connect(m_newItemBtn, &QPushButton::clicked, this,
          &EmptyStateWidget::createItemRequested);

  centerLayout->addWidget(m_newItemBtn, 0, Qt::AlignCenter);

  rootLayout->addWidget(centerContainer);

  updateThemeIcon();
}

void EmptyStateWidget::updateThemeIcon() {
  const bool isLight =
      (app::ThemeManager::instance().currentMode() == app::ThemeMode::Light);
  if (m_iconLabel) {
    m_iconLabel->setPixmap(app::ThemeManager::instance()
                               .icon(QStringLiteral("shield_check"))
                               .pixmap(36, 36));
  }
  if (m_newItemBtn) {
    m_newItemBtn->setIcon(
        QIcon(isLight ? QStringLiteral(":/icons/plus_on_dark.svg")
                      : QStringLiteral(":/icons/plus_on_light.svg")));
  }
}

void EmptyStateWidget::retranslateUi() {
  if (m_titleLabel) {
    m_titleLabel->setText(tr("No Item Selected"));
  }
  if (m_descLabel) {
    m_descLabel->setText(tr("Select an item from the list to view its details, "
                            "or use the quick shortcuts below."));
  }
  if (m_actionNewItemLabel) {
    m_actionNewItemLabel->setText(tr("Create new item"));
  }
  if (m_actionSearchLabel) {
    m_actionSearchLabel->setText(tr("Search vault items"));
  }
  if (m_actionSidebarLabel) {
    m_actionSidebarLabel->setText(tr("Toggle navigation sidebar"));
  }
  if (m_newItemBtn) {
    m_newItemBtn->setText(tr("Create New Item"));
  }
}

void EmptyStateWidget::changeEvent(QEvent *event) {
  if (event->type() == QEvent::LanguageChange) {
    retranslateUi();
  }
  QWidget::changeEvent(event);
}

} // namespace svm::views
