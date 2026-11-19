/**
 * @file FramelessWindow.cpp
 * @brief Implementation of base FramelessWindow.
 */

#include "FramelessWindow.hpp"

#include "WindowManager.hpp"

#include <QEvent>
#include <QLayout>
#include <QMetaObject>
#include <QMouseEvent>
#include <QStyle>
#include <QVariant>
#include <QWidget>
#include <QWindow>

FramelessWindow::FramelessWindow(QWidget *parent) : QMainWindow(parent) {
  setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
  setAttribute(Qt::WA_TranslucentBackground, true);
  setAttribute(Qt::WA_Hover, true);
  setMouseTracking(true);

  // Register as root window with WindowManager
  WindowManager::instance().registerRootWindow(this);
}

void FramelessWindow::setHeaderBar(QWidget *headerBar) {
  m_headerBar = headerBar;
  if (m_headerBar) {
    connect(m_headerBar, SIGNAL(minimizeRequested()), this,
            SLOT(showMinimized()));
    connect(m_headerBar, SIGNAL(maximizeRestoreRequested()), this,
            SLOT(showMaximized()));
    connect(m_headerBar, SIGNAL(closeRequested()), this, SLOT(close()));
  }
}

void FramelessWindow::setBorderDark(bool isDark) {
  m_isDark = isDark;
  if (m_borderOverlay) {
    m_borderOverlay->setDark(isDark);
  }
}

void FramelessWindow::applyMaximizeState(bool isMax) {
  if (m_headerBar) {
    m_headerBar->setProperty("maximized", isMax);
    m_headerBar->setStyleSheet(
        isMax ? QStringLiteral("#UnifiedHeaderBar { border-radius: 0px; "
                               "border-top-left-radius: 0px; "
                               "border-top-right-radius: 0px; }")
              : QString());
    m_headerBar->style()->unpolish(m_headerBar);
    m_headerBar->style()->polish(m_headerBar);
    m_headerBar->update();
  }
  if (centralWidget()) {
    centralWidget()->setProperty("maximized", isMax);
    centralWidget()->setStyleSheet(
        isMax ? QStringLiteral(
                    "#CentralWidget { border: none; border-radius: 0px; }")
              : QString());
    centralWidget()->style()->unpolish(centralWidget());
    centralWidget()->style()->polish(centralWidget());
    if (auto *l = centralWidget()->layout()) {
      const int m = isMax ? 0 : 1;
      l->setContentsMargins(m, m, m, m);
    }
    centralWidget()->update();
  }
  if (m_borderOverlay) {
    m_borderOverlay->setRadius(isMax ? 0.0 : 8.0);
  }
}

void FramelessWindow::showEvent(QShowEvent *event) {
  QMainWindow::showEvent(event);
  const bool isMax = isMaximized() || isFullScreen();
  if (centralWidget() && !m_borderOverlay) {
    m_borderOverlay = new FramelessUtil::RoundedBorderOverlay(
        centralWidget(), m_isDark, isMax ? 0.0 : 8.0);
    m_borderOverlay->show();
  }
  applyMaximizeState(isMax);
  if (auto *node = WindowManager::instance().findNode(this)) {
    node->refreshWindowHandle();
  }
}

void FramelessWindow::changeEvent(QEvent *event) {
  if (event->type() == QEvent::WindowStateChange) {
    const bool isMax = isMaximized() || isFullScreen();
    if (m_headerBar) {
      QMetaObject::invokeMethod(m_headerBar, "updateMaximizeState",
                                Q_ARG(bool, isMax));
    }
    applyMaximizeState(isMax);
  }
  QMainWindow::changeEvent(event);
}

void FramelessWindow::mousePressEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton && !isMaximized()) {
    auto edge = FramelessUtil::calculateEdge(event->position().toPoint(),
                                             size(), m_resizeMargin);
    if (edge != FramelessUtil::ResizeEdge::None) {
      if (FramelessUtil::triggerSystemResize(windowHandle(), edge)) {
        event->accept();
        return;
      }

      m_resizingEdge = edge;
      m_resizeStartPos = event->globalPosition().toPoint();
      m_resizeStartGeometry = geometry();
      event->accept();
      return;
    }
  }
  QMainWindow::mousePressEvent(event);
}

void FramelessWindow::mouseMoveEvent(QMouseEvent *event) {
  if (isMaximized()) {
    QMainWindow::mouseMoveEvent(event);
    return;
  }

  if (m_resizingEdge != FramelessUtil::ResizeEdge::None) {
    QRect newGeom = FramelessUtil::calculateManualResize(
        m_resizingEdge, event->globalPosition().toPoint(), m_resizeStartPos,
        m_resizeStartGeometry, minimumSize());
    setGeometry(newGeom);
    event->accept();
    return;
  }

  auto edge = FramelessUtil::calculateEdge(event->position().toPoint(), size(),
                                           m_resizeMargin);
  FramelessUtil::updateCursorForEdge(this, edge);
  QMainWindow::mouseMoveEvent(event);
}

void FramelessWindow::mouseReleaseEvent(QMouseEvent *event) {
  m_resizingEdge = FramelessUtil::ResizeEdge::None;
  unsetCursor();
  QMainWindow::mouseReleaseEvent(event);
}
