/**
 * @file ui/components/unified_header_bar.cpp
 * @brief Hiện thực thanh tiêu đề tích hợp UnifiedHeaderBar tương tự Notera.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "unified_header_bar.h"
#include "app/theme_manager.h"
#include "menubar/app_menu_bar.h"

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

UnifiedHeaderBar::UnifiedHeaderBar(AppMenuBar *menuBar, QWidget *parent)
    : QWidget(parent), m_menuBar(menuBar),
      m_darkMode(app::ThemeManager::instance().currentMode() ==
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
  QIcon appIcon(QStringLiteral(":/icons/app_logo.svg"));
  m_logoLabel->setPixmap(appIcon.pixmap(20, 20));
  m_logoLabel->setFixedSize(24, 24);
  m_logoLabel->setAlignment(Qt::AlignCenter);
  layout->addWidget(m_logoLabel, 0, Qt::AlignVCenter);

  // 2. Thanh menu nhúng trực tiếp (Embedded MenuBar)
  if (m_menuBar) {
    m_menuBar->setParent(this);
    m_menuBar->setObjectName(QStringLiteral("EmbeddedMenuBar"));
    m_menuBar->setNativeMenuBar(false);
    m_menuBar->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    layout->addWidget(m_menuBar, 0, Qt::AlignVCenter);
  }

  // Khoảng đệm dãn giữa MenuBar và các nút điều khiển cửa sổ góc phải
  layout->addStretch(1);

  // Nút Spotlight Command Palette (Ctrl+K)
  m_paletteBtn = new QPushButton(this);
  m_paletteBtn->setObjectName(QStringLiteral("HeaderCommandPaletteBtn"));
  m_paletteBtn->setIcon(QIcon(QStringLiteral(":/icons/search.svg")));
  m_paletteBtn->setIconSize(QSize(14, 14));
  m_paletteBtn->setFixedSize(28, 28);
  m_paletteBtn->setFlat(true);
  m_paletteBtn->setToolTip(tr("Command Palette (Ctrl+K)"));
  connect(m_paletteBtn, &QPushButton::clicked, this,
          &UnifiedHeaderBar::commandPaletteRequested);
  layout->addWidget(m_paletteBtn, 0, Qt::AlignVCenter);

  // 3. Nhãn tiêu đề căn giữa toàn bộ cửa sổ (Center Title Label Overlay)
  m_titleLabel = new QLabel(QStringLiteral("Secure Vault Manager"), this);
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

  int leftOccupied = (m_menuBar ? m_menuBar->geometry().right()
                                : m_logoLabel->geometry().right()) +
                     12;
  int rightOccupied =
      (m_minBtn ? (width() - m_minBtn->geometry().left()) : 46 * 3) + 12;
  int sideMargin = qMax(leftOccupied, rightOccupied);

  int availWidth = width() - (sideMargin * 2);
  QString fullTitle = m_titleLabel->property("fullTitle").toString();
  if (fullTitle.isEmpty()) {
    fullTitle = QStringLiteral("Secure Vault Manager");
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
  const QString suffix =
      darkMode ? QStringLiteral("dark") : QStringLiteral("light");

  m_minBtn->setIcon(
      QIcon(QStringLiteral(":/icons/win_min_%1.svg").arg(suffix)));
  if (m_isMaximized) {
    m_maxBtn->setIcon(
        QIcon(QStringLiteral(":/icons/win_restore_%1.svg").arg(suffix)));
  } else {
    m_maxBtn->setIcon(
        QIcon(QStringLiteral(":/icons/win_max_%1.svg").arg(suffix)));
  }
  m_closeBtn->setIcon(
      QIcon(QStringLiteral(":/icons/win_close_%1.svg").arg(suffix)));

  if (m_paletteBtn) {
    m_paletteBtn->setIcon(
        QIcon(darkMode ? QStringLiteral(":/icons/search_dark.svg")
                       : QStringLiteral(":/icons/search_light.svg")));
  }
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
