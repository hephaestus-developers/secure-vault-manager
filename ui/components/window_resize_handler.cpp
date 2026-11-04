/**
 * @file ui/components/window_resize_handler.cpp
 * @brief Hiện thực bộ điều phối co giãn cửa sổ Frameless 8 hướng tương tự
 * Notera.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "window_resize_handler.h"

#include <QCursor>
#include <QMouseEvent>
#include <QWindow>

namespace svm::components {

ResizeHandle::ResizeHandle(Qt::Edges edges, QWidget *window)
    : QWidget(window), m_edges(edges), m_window(window) {
  setAttribute(Qt::WA_NoSystemBackground);
  setAttribute(Qt::WA_TranslucentBackground);

  if ((edges & Qt::LeftEdge && edges & Qt::TopEdge) ||
      (edges & Qt::RightEdge && edges & Qt::BottomEdge)) {
    setCursor(Qt::SizeFDiagCursor);
  } else if ((edges & Qt::RightEdge && edges & Qt::TopEdge) ||
             (edges & Qt::LeftEdge && edges & Qt::BottomEdge)) {
    setCursor(Qt::SizeBDiagCursor);
  } else if (edges & Qt::LeftEdge || edges & Qt::RightEdge) {
    setCursor(Qt::SizeHorCursor);
  } else if (edges & Qt::TopEdge || edges & Qt::BottomEdge) {
    setCursor(Qt::SizeVerCursor);
  }
}

void ResizeHandle::mousePressEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton && m_window &&
      !m_window->isMaximized()) {
    // 1. Ưu tiên sử dụng cơ chế co giãn cấp hệ thống
    if (QWindow *handle = m_window->windowHandle()) {
      if (handle->startSystemResize(m_edges)) {
        event->accept();
        return;
      }
    }

    // 2. Chế độ dự phòng thủ công (Manual Dragging)
    m_isResizing = true;
    m_dragStartPos = event->globalPosition().toPoint();
    m_dragStartGeometry = m_window->geometry();
    event->accept();
    return;
  }
  QWidget::mousePressEvent(event);
}

void ResizeHandle::mouseMoveEvent(QMouseEvent *event) {
  if (m_isResizing && m_window && (event->buttons() & Qt::LeftButton)) {
    QPoint delta = event->globalPosition().toPoint() - m_dragStartPos;
    QRect newGeom = m_dragStartGeometry;

    int minW = m_window->minimumWidth();
    int minH = m_window->minimumHeight();

    if (m_edges & Qt::LeftEdge) {
      int newLeft = m_dragStartGeometry.left() + delta.x();
      if (m_dragStartGeometry.right() - newLeft + 1 >= minW) {
        newGeom.setLeft(newLeft);
      } else {
        newGeom.setLeft(m_dragStartGeometry.right() - minW + 1);
      }
    }
    if (m_edges & Qt::RightEdge) {
      int newRight = m_dragStartGeometry.right() + delta.x();
      if (newRight - m_dragStartGeometry.left() + 1 >= minW) {
        newGeom.setRight(newRight);
      } else {
        newGeom.setRight(m_dragStartGeometry.left() + minW - 1);
      }
    }
    if (m_edges & Qt::TopEdge) {
      int newTop = m_dragStartGeometry.top() + delta.y();
      if (m_dragStartGeometry.bottom() - newTop + 1 >= minH) {
        newGeom.setTop(newTop);
      } else {
        newGeom.setTop(m_dragStartGeometry.bottom() - minH + 1);
      }
    }
    if (m_edges & Qt::BottomEdge) {
      int newBottom = m_dragStartGeometry.bottom() + delta.y();
      if (newBottom - m_dragStartGeometry.top() + 1 >= minH) {
        newGeom.setBottom(newBottom);
      } else {
        newGeom.setBottom(m_dragStartGeometry.top() + minH - 1);
      }
    }

    m_window->setGeometry(newGeom);
    event->accept();
    return;
  }
  QWidget::mouseMoveEvent(event);
}

void ResizeHandle::mouseReleaseEvent(QMouseEvent *event) {
  m_isResizing = false;
  QWidget::mouseReleaseEvent(event);
}

void ResizeHandle::paintEvent(QPaintEvent *event) { Q_UNUSED(event); }

WindowResizeHandler::WindowResizeHandler(QWidget *window)
    : QObject(window), m_window(window) {
  m_topLeft = new ResizeHandle(Qt::TopEdge | Qt::LeftEdge, window);
  m_top = new ResizeHandle(Qt::TopEdge, window);
  m_topRight = new ResizeHandle(Qt::TopEdge | Qt::RightEdge, window);
  m_left = new ResizeHandle(Qt::LeftEdge, window);
  m_right = new ResizeHandle(Qt::RightEdge, window);
  m_bottomLeft = new ResizeHandle(Qt::BottomEdge | Qt::LeftEdge, window);
  m_bottom = new ResizeHandle(Qt::BottomEdge, window);
  m_bottomRight = new ResizeHandle(Qt::BottomEdge | Qt::RightEdge, window);

  updateGeometry();
}

void WindowResizeHandler::updateGeometry() {
  if (!m_window) {
    return;
  }

  int w = m_window->width();
  int h = m_window->height();
  int b = BorderWidth;
  int c = CornerLength;

  if (w < c * 2 || h < c * 2) {
    return;
  }

  m_topLeft->setGeometry(0, 0, c, c);
  m_top->setGeometry(c, 0, w - c * 2, b);
  m_topRight->setGeometry(w - c, 0, c, c);

  m_left->setGeometry(0, c, b, h - c * 2);
  m_right->setGeometry(w - b, c, b, h - c * 2);

  m_bottomLeft->setGeometry(0, h - c, c, c);
  m_bottom->setGeometry(c, h - b, w - c * 2, b);
  m_bottomRight->setGeometry(w - c, h - c, c, c);

  m_topLeft->raise();
  m_top->raise();
  m_topRight->raise();
  m_left->raise();
  m_right->raise();
  m_bottomLeft->raise();
  m_bottom->raise();
  m_bottomRight->raise();
}

void WindowResizeHandler::setVisible(bool visible) {
  m_topLeft->setVisible(visible);
  m_top->setVisible(visible);
  m_topRight->setVisible(visible);
  m_left->setVisible(visible);
  m_right->setVisible(visible);
  m_bottomLeft->setVisible(visible);
  m_bottom->setVisible(visible);
  m_bottomRight->setVisible(visible);

  if (visible) {
    updateGeometry();
  }
}

} // namespace svm::components
