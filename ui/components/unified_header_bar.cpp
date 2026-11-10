/**
 * @file ui/components/unified_header_bar.cpp
 * @brief Hiện thực thanh tiêu đề tích hợp UnifiedHeaderBar tương tự Notera.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "unified_header_bar.h"
#include "app/theme_manager.h"

#include <QFontMetrics>
#include <QHBoxLayout>
#include <QIcon>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QSizePolicy>
#include <QStyleOption>
#include <QWindow>

namespace svm::components {

UnifiedHeaderBar::UnifiedHeaderBar(QWidget *parent)
    : QWidget(parent), m_darkMode(app::ThemeManager::instance().currentMode() ==
                                  app::ThemeMode::Dark) {
  setObjectName(QStringLiteral("UnifiedHeaderBar"));
  setAttribute(Qt::WA_StyledBackground, true);
  setFixedHeight(38);

  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(10, 0, 0, 0);
  layout->setSpacing(4);

  // 1. Biểu tượng ứng dụng (App Logo)
  m_logoLabel = new QLabel(this);
  m_logoLabel->setObjectName(QStringLiteral("HeaderLogo"));
  QIcon appIcon(QStringLiteral(":/icons/brand/app_logo.svg"));
  m_logoLabel->setPixmap(appIcon.pixmap(20, 20));
  m_logoLabel->setFixedSize(24, 24);
  m_logoLabel->setAlignment(Qt::AlignCenter);
  layout->addWidget(m_logoLabel, 0, Qt::AlignVCenter);

  // Khoảng đệm dãn giữa Logo và các nút điều khiển cửa sổ góc phải
  layout->addStretch(1);

  // Nút Chuyển đổi Theme (Dark / Light)
  m_themeToggleBtn = new QPushButton(this);
  m_themeToggleBtn->setObjectName(QStringLiteral("HeaderThemeToggleBtn"));
  m_themeToggleBtn->setIconSize(QSize(15, 15));
  m_themeToggleBtn->setFixedSize(28, 28);
  m_themeToggleBtn->setFlat(true);
  m_themeToggleBtn->setCursor(Qt::PointingHandCursor);
  m_themeToggleBtn->setShortcut(QKeySequence(QStringLiteral("Ctrl+T")));
  connect(m_themeToggleBtn, &QPushButton::clicked, this,
          &UnifiedHeaderBar::themeToggleRequested);
  layout->addWidget(m_themeToggleBtn, 0, Qt::AlignVCenter);

  // Nút Cài đặt (Settings Dialog)
  m_settingsBtn = new QPushButton(this);
  m_settingsBtn->setObjectName(QStringLiteral("HeaderSettingsBtn"));
  m_settingsBtn->setIconSize(QSize(15, 15));
  m_settingsBtn->setFixedSize(28, 28);
  m_settingsBtn->setFlat(true);
  m_settingsBtn->setCursor(Qt::PointingHandCursor);
  m_settingsBtn->setToolTip(tr("Settings (Ctrl+,)"));
  m_settingsBtn->setShortcut(QKeySequence(QStringLiteral("Ctrl+,")));
  connect(m_settingsBtn, &QPushButton::clicked, this,
          &UnifiedHeaderBar::settingsRequested);
  layout->addWidget(m_settingsBtn, 0, Qt::AlignVCenter);

  // Nút Giới thiệu (About Dialog)
  m_aboutBtn = new QPushButton(this);
  m_aboutBtn->setObjectName(QStringLiteral("HeaderAboutBtn"));
  m_aboutBtn->setIconSize(QSize(15, 15));
  m_aboutBtn->setFixedSize(28, 28);
  m_aboutBtn->setFlat(true);
  m_aboutBtn->setCursor(Qt::PointingHandCursor);
  m_aboutBtn->setToolTip(tr("About Secure Vault Manager"));
  connect(m_aboutBtn, &QPushButton::clicked, this,
          &UnifiedHeaderBar::aboutRequested);
  layout->addWidget(m_aboutBtn, 0, Qt::AlignVCenter);

  // 3. Nhãn tiêu đề căn giữa toàn bộ cửa sổ (Center Title Label Overlay)
  m_titleLabel = new QLabel(tr("Secure Vault Manager"), this);
  m_titleLabel->setObjectName(QStringLiteral("HeaderTitleLabel"));
  m_titleLabel->setAlignment(Qt::AlignCenter);
  m_titleLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);

  // 4. Các nút điều khiển cửa sổ với biểu tượng SVG chuẩn
  m_minBtn = new QPushButton(this);
  m_minBtn->setObjectName(QStringLiteral("WinMinBtn"));
  m_minBtn->setFixedSize(46, 38);
  m_minBtn->setIconSize(QSize(12, 12));
  m_minBtn->setFlat(true);
  m_minBtn->setFocusPolicy(Qt::NoFocus);
  m_minBtn->setToolTip(tr("Minimize"));

  m_maxBtn = new QPushButton(this);
  m_maxBtn->setObjectName(QStringLiteral("WinMaxBtn"));
  m_maxBtn->setFixedSize(46, 38);
  m_maxBtn->setIconSize(QSize(12, 12));
  m_maxBtn->setFlat(true);
  m_maxBtn->setFocusPolicy(Qt::NoFocus);
  m_maxBtn->setToolTip(tr("Maximize"));

  m_closeBtn = new QPushButton(this);
  m_closeBtn->setObjectName(QStringLiteral("WinCloseBtn"));
  m_closeBtn->setFixedSize(46, 38);
  m_closeBtn->setIconSize(QSize(12, 12));
  m_closeBtn->setFlat(true);
  m_closeBtn->setFocusPolicy(Qt::NoFocus);
  m_closeBtn->setToolTip(tr("Close"));

  layout->addWidget(m_minBtn);
  layout->addWidget(m_maxBtn);
  layout->addWidget(m_closeBtn);

  // Áp dụng biểu tượng theo chủ đề hiện tại
  updateThemeIcons(m_darkMode);

  // Tự động đồng bộ biểu tượng khi chuyển đổi chủ đề sáng / tối
  connect(&app::ThemeManager::instance(), &app::ThemeManager::themeChanged,
          this, [this](app::ThemeMode mode) {
            updateThemeIcons(mode == app::ThemeMode::Dark);
          });

  // Kết nối các tín hiệu nút bấm
  connect(m_minBtn, &QPushButton::clicked, this,
          &UnifiedHeaderBar::minimizeRequested);
  connect(m_maxBtn, &QPushButton::clicked, this,
          &UnifiedHeaderBar::maximizeRestoreRequested);
  connect(m_closeBtn, &QPushButton::clicked, this,
          &UnifiedHeaderBar::closeRequested);
}

void UnifiedHeaderBar::setTitleText(const QString &title) {
  if (m_titleLabel) {
    m_titleLabel->setProperty("fullTitle", title);
    repositionTitleOverlay();
  }
}

void UnifiedHeaderBar::repositionTitleOverlay() {
  if (!m_titleLabel) {
    return;
  }

  int leftOccupied = (m_logoLabel ? m_logoLabel->geometry().right() : 24) + 12;
  int rightOccupied =
      (m_themeToggleBtn && m_themeToggleBtn->isVisible()
           ? (width() - m_themeToggleBtn->geometry().left())
           : (m_minBtn ? (width() - m_minBtn->geometry().left()) : 46 * 3)) +
      12;
  int sideMargin = qMax(leftOccupied, rightOccupied);

  int availWidth = width() - (sideMargin * 2);
  QString fullTitle = m_titleLabel->property("fullTitle").toString();
  if (fullTitle.isEmpty()) {
    fullTitle = tr("Secure Vault Manager");
  }

  if (availWidth <= 40) {
    m_titleLabel->clear();
    m_titleLabel->setGeometry(0, 0, 0, height());
    return;
  }

  // Đặt hình học đối xứng qua chính giữa thanh HeaderBar (width() / 2)
  m_titleLabel->setGeometry(sideMargin, 0, availWidth, height());
  QFontMetrics fm(m_titleLabel->font());
  m_titleLabel->setText(fm.elidedText(fullTitle, Qt::ElideMiddle, availWidth));
}

void UnifiedHeaderBar::paintEvent(QPaintEvent *event) {
  QStyleOption opt;
  opt.initFrom(this);
  QPainter p(this);
  style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
  QWidget::paintEvent(event);
}

void UnifiedHeaderBar::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
  repositionTitleOverlay();
}

void UnifiedHeaderBar::updateMaximizeIcon(bool isMaximized) {
  m_isMaximized = isMaximized;
  m_maxBtn->setToolTip(isMaximized ? tr("Restore") : tr("Maximize"));
  updateThemeIcons(m_darkMode);
}

void UnifiedHeaderBar::updateThemeIcons(bool darkMode) {
  m_darkMode = darkMode;
  auto &tm = app::ThemeManager::instance();

  m_minBtn->setIcon(tm.icon(QStringLiteral("win_min")));
  if (m_isMaximized) {
    m_maxBtn->setIcon(tm.icon(QStringLiteral("win_restore")));
  } else {
    m_maxBtn->setIcon(tm.icon(QStringLiteral("win_max")));
  }
  m_closeBtn->setIcon(tm.icon(QStringLiteral("win_close")));

  if (m_themeToggleBtn) {
    m_themeToggleBtn->setIcon(tm.icon(QStringLiteral("theme_toggle")));
    if (darkMode) {
      m_themeToggleBtn->setToolTip(tr("Switch to Light theme (Ctrl+T)"));
    } else {
      m_themeToggleBtn->setToolTip(tr("Switch to Dark theme (Ctrl+T)"));
    }
  }

  if (m_settingsBtn) {
    m_settingsBtn->setIcon(tm.icon(QStringLiteral("category_settings")));
  }

  if (m_aboutBtn) {
    m_aboutBtn->setIcon(tm.icon(QStringLiteral("info")));
  }
}

void UnifiedHeaderBar::retranslateUi() {
  if (m_titleLabel) {
    m_titleLabel->setText(tr("Secure Vault Manager"));
  }
  if (m_themeToggleBtn) {
    m_themeToggleBtn->setToolTip(m_darkMode
                                     ? tr("Switch to Light theme (Ctrl+T)")
                                     : tr("Switch to Dark theme (Ctrl+T)"));
  }
  if (m_settingsBtn) {
    m_settingsBtn->setToolTip(tr("Settings (Ctrl+,)"));
  }

  if (m_aboutBtn) {
    m_aboutBtn->setToolTip(tr("About Secure Vault Manager"));
  }
  if (m_minBtn) {
    m_minBtn->setToolTip(tr("Minimize"));
  }
  if (m_maxBtn) {
    const bool isMax = window() && window()->isMaximized();
    m_maxBtn->setToolTip(isMax ? tr("Restore") : tr("Maximize"));
  }
  if (m_closeBtn) {
    m_closeBtn->setToolTip(tr("Close"));
  }
}

void UnifiedHeaderBar::setSettingsVisible(bool visible) {
  if (m_settingsBtn) {
    m_settingsBtn->setVisible(visible);
    repositionTitleOverlay();
  }
}

void UnifiedHeaderBar::changeEvent(QEvent *event) {
  if (event->type() == QEvent::LanguageChange) {
    retranslateUi();
  }
  QWidget::changeEvent(event);
}

void UnifiedHeaderBar::mousePressEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    if (QWidget *win = window()) {
      if (QWindow *handle = win->windowHandle()) {
        if (handle->startSystemMove()) {
          event->accept();
          return;
        }
      }
      m_manualDragging = true;
      m_dragPosition =
          event->globalPosition().toPoint() - win->frameGeometry().topLeft();
      event->accept();
      return;
    }
  }
  QWidget::mousePressEvent(event);
}

void UnifiedHeaderBar::mouseMoveEvent(QMouseEvent *event) {
  if (m_manualDragging && (event->buttons() & Qt::LeftButton)) {
    if (QWidget *win = window()) {
      if (!win->isMaximized()) {
        win->move(event->globalPosition().toPoint() - m_dragPosition);
      }
      event->accept();
      return;
    }
  }
  QWidget::mouseMoveEvent(event);
}

void UnifiedHeaderBar::mouseReleaseEvent(QMouseEvent *event) {
  m_manualDragging = false;
  QWidget::mouseReleaseEvent(event);
}

void UnifiedHeaderBar::mouseDoubleClickEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    emit maximizeRestoreRequested();
    event->accept();
    return;
  }
  QWidget::mouseDoubleClickEvent(event);
}

} // namespace svm::components
