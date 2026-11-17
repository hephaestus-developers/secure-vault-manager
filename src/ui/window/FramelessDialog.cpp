/**
 * @file FramelessDialog.cpp
 * @brief Implementation of base FramelessDialog utilizing FramelessUtil.
 */

#include "FramelessDialog.hpp"

#include "WindowManager.hpp"

#include <QAbstractButton>
#include <QEventLoop>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QVBoxLayout>
#include <QWindow>

FramelessDialog::FramelessDialog(QWidget *parent, bool isDark)
    : QDialog(parent), m_isDark(isDark) {
  setObjectName(QStringLiteral("FramelessDialog"));
  setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
  setWindowModality(Qt::NonModal);
  setAttribute(Qt::WA_TranslucentBackground, true);
  setAttribute(Qt::WA_Hover, true);
  setMouseTracking(true);

  // Root layout providing outer margins for resize border and smooth shake
  // animation
  m_rootLayout = new QVBoxLayout(this);
  m_rootLayout->setContentsMargins(MARGIN, MARGIN, MARGIN, MARGIN);
  m_rootLayout->setSpacing(0);

  // Register with WindowManager as modal child of parent
  WindowManager::instance().registerWindow(this, parent, true);
}

FramelessDialog::~FramelessDialog() {
  WindowManager::instance().unregisterWindow(this);
}

int FramelessDialog::shakeOffset() const { return m_shakeOffset; }

void FramelessDialog::setShakeOffset(int offset) {
  m_shakeOffset = offset;
  if (m_rootLayout) {
    m_rootLayout->setContentsMargins(MARGIN + offset, MARGIN, MARGIN - offset,
                                     MARGIN);
  }
  update();
}

bool FramelessDialog::isDark() const { return m_isDark; }

void FramelessDialog::setDark(bool isDark) {
  m_isDark = isDark;
  update();
}

void FramelessDialog::setCardWidget(QWidget *cardWidget) {
  if (m_cardWidget && m_cardWidget != cardWidget) {
    m_cardWidget->removeEventFilter(this);
    m_rootLayout->removeWidget(m_cardWidget);
  }
  m_cardWidget = cardWidget;
  if (m_cardWidget) {
    m_cardWidget->setMouseTracking(true);
    m_cardWidget->installEventFilter(this);
    m_rootLayout->addWidget(m_cardWidget);
  }
}

void FramelessDialog::setHeaderBar(QWidget *headerBar) {
  if (m_headerBar) {
    m_headerBar->removeEventFilter(this);
    for (auto *child : m_headerBar->findChildren<QWidget *>()) {
      child->removeEventFilter(this);
    }
  }
  m_headerBar = headerBar;
  if (m_headerBar) {
    m_headerBar->setMouseTracking(true);
    m_headerBar->installEventFilter(this);
    for (auto *child : m_headerBar->findChildren<QWidget *>()) {
      if (!qobject_cast<QAbstractButton *>(child)) {
        child->setMouseTracking(true);
        child->installEventFilter(this);
      }
    }
  }
}

bool FramelessDialog::isResizable() const {
  return (minimumWidth() < maximumWidth() || minimumHeight() < maximumHeight());
}

int FramelessDialog::exec() {
  setModal(false);
  setWindowModality(Qt::NonModal);
  setResult(QDialog::Rejected);

  if (QWidget *p = parentWidget()) {
    if (QWidget *top = p->window()) {
      const QPoint center = top->geometry().center();
      move(center.x() - width() / 2, center.y() - height() / 2);
    }
  }

  QEventLoop loop;
  auto connFinished =
      connect(this, &QDialog::finished, &loop, &QEventLoop::exit);
  auto connDestroyed =
      connect(this, &QObject::destroyed, &loop, &QEventLoop::quit);

  show();
  raise();
  activateWindow();

  const int res = loop.exec(QEventLoop::DialogExec);

  disconnect(connFinished);
  disconnect(connDestroyed);
  return res;
}

void FramelessDialog::showEvent(QShowEvent *event) {
  QDialog::showEvent(event);
  WindowManager::instance().notifyWindowActivated(this);
  if (auto *node = WindowManager::instance().findNode(this)) {
    node->refreshWindowHandle();
  }
}

void FramelessDialog::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);
  if (!m_cardWidget) {
    return;
  }

  // Draw subtle modern drop shadow around card widget in transparent margins
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  const QRect cardRect = m_cardWidget->geometry();
  const int maxSteps = qMin(MARGIN, 6);
  for (int i = 1; i <= maxSteps; ++i) {
    const int alpha = m_isDark ? (22 - i * 3) : (15 - i * 2);
    if (alpha <= 0)
      break;
    painter.setPen(QColor(0, 0, 0, alpha));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(cardRect.adjusted(-i, -i, i, i), 4 + i, 4 + i);
  }
}

bool FramelessDialog::handleMousePress(QMouseEvent *event) {
  if (event->button() != Qt::LeftButton) {
    return false;
  }

  if (isResizable()) {
    const QPoint localPos = mapFromGlobal(event->globalPosition().toPoint());
    auto edge = FramelessUtil::calculateEdge(localPos, size(), MARGIN);

    if (edge != FramelessUtil::ResizeEdge::None) {
      if (FramelessUtil::triggerSystemResize(windowHandle(), edge)) {
        event->accept();
        return true;
      }

      m_resizingEdge = edge;
      m_resizeStartPos = event->globalPosition().toPoint();
      m_resizeStartGeometry = geometry();
      event->accept();
      return true;
    }
  }

  return false;
}

bool FramelessDialog::handleMouseMove(QMouseEvent *event) {
  if (m_resizingEdge != FramelessUtil::ResizeEdge::None) {
    QRect newGeom = FramelessUtil::calculateManualResize(
        m_resizingEdge, event->globalPosition().toPoint(), m_resizeStartPos,
        m_resizeStartGeometry, minimumSize());
    setGeometry(newGeom);
    event->accept();
    return true;
  }

  if (isResizable()) {
    const QPoint localPos = mapFromGlobal(event->globalPosition().toPoint());
    auto edge = FramelessUtil::calculateEdge(localPos, size(), MARGIN);
    FramelessUtil::updateCursorForEdge(this, edge);
  }
  return false;
}

bool FramelessDialog::handleMouseRelease(QMouseEvent *event) {
  Q_UNUSED(event);
  m_isDragging = false;
  m_resizingEdge = FramelessUtil::ResizeEdge::None;
  unsetCursor();
  return false;
}

bool FramelessDialog::eventFilter(QObject *watched, QEvent *event) {
  // 1. Header dragging
  if (m_headerBar &&
      (watched == m_headerBar ||
       m_headerBar->isAncestorOf(qobject_cast<QWidget *>(watched)))) {
    if (qobject_cast<QAbstractButton *>(watched)) {
      return QDialog::eventFilter(watched, event);
    }

    if (event->type() == QEvent::MouseButtonPress) {
      auto *me = static_cast<QMouseEvent *>(event);
      if (me->button() == Qt::LeftButton) {
        if (FramelessUtil::triggerSystemMove(windowHandle())) {
          me->accept();
          return true;
        }
        m_isDragging = true;
        m_dragPosition =
            me->globalPosition().toPoint() - frameGeometry().topLeft();
        me->accept();
        return true;
      }
    } else if (event->type() == QEvent::MouseMove) {
      auto *me = static_cast<QMouseEvent *>(event);
      if (m_isDragging && (me->buttons() & Qt::LeftButton)) {
        move(me->globalPosition().toPoint() - m_dragPosition);
        me->accept();
        return true;
      }
    } else if (event->type() == QEvent::MouseButtonRelease) {
      m_isDragging = false;
      return true;
    }
  }

  // 2. Card widget border resize
  if (watched == m_cardWidget || watched == this) {
    if (event->type() == QEvent::MouseButtonPress) {
      auto *me = static_cast<QMouseEvent *>(event);
      if (handleMousePress(me)) {
        return true;
      }
    } else if (event->type() == QEvent::MouseMove) {
      auto *me = static_cast<QMouseEvent *>(event);
      if (handleMouseMove(me)) {
        return true;
      }
    } else if (event->type() == QEvent::MouseButtonRelease) {
      auto *me = static_cast<QMouseEvent *>(event);
      if (handleMouseRelease(me)) {
        return true;
      }
    }
  }

  return QDialog::eventFilter(watched, event);
}

void FramelessDialog::mousePressEvent(QMouseEvent *event) {
  if (!handleMousePress(event)) {
    QDialog::mousePressEvent(event);
  }
}

void FramelessDialog::mouseMoveEvent(QMouseEvent *event) {
  if (!handleMouseMove(event)) {
    QDialog::mouseMoveEvent(event);
  }
}

void FramelessDialog::mouseReleaseEvent(QMouseEvent *event) {
  if (!handleMouseRelease(event)) {
    QDialog::mouseReleaseEvent(event);
  }
}

void FramelessDialog::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Escape) {
    reject();
    return;
  }
  QDialog::keyPressEvent(event);
}
