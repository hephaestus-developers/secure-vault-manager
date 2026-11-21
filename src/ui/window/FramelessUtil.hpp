/**
 * @file FramelessUtil.hpp
 * @brief Common utilities, edge calculations, cursor shapes, and system
 * resize/move protocol helpers shared between FramelessWindow and
 * FramelessDialog.
 */

#pragma once

#include <QCursor>
#include <QEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPoint>
#include <QRect>
#include <QRectF>
#include <QSize>
#include <QWidget>
#include <QWindow>
#include <algorithm>

namespace FramelessUtil {

/**
 * @enum ResizeEdge
 * @brief Active window border edge or corner being dragged for resizing.
 */
enum class ResizeEdge {
  None,
  Left,
  Right,
  Top,
  Bottom,
  TopLeft,
  TopRight,
  BottomLeft,
  BottomRight
};

/**
 * @brief Identifies border edge or corner from mouse position within a window.
 */
inline ResizeEdge calculateEdge(const QPoint &pos, const QSize &windowSize,
                                int margin = 8) {
  const bool onLeft = pos.x() <= margin;
  const bool onRight = pos.x() >= windowSize.width() - margin;
  const bool onTop = pos.y() <= margin;
  const bool onBottom = pos.y() >= windowSize.height() - margin;

  if (onTop && onLeft)
    return ResizeEdge::TopLeft;
  if (onTop && onRight)
    return ResizeEdge::TopRight;
  if (onBottom && onLeft)
    return ResizeEdge::BottomLeft;
  if (onBottom && onRight)
    return ResizeEdge::BottomRight;
  if (onLeft)
    return ResizeEdge::Left;
  if (onRight)
    return ResizeEdge::Right;
  if (onTop)
    return ResizeEdge::Top;
  if (onBottom)
    return ResizeEdge::Bottom;

  return ResizeEdge::None;
}

/**
 * @brief Updates mouse cursor shape to match resize edge direction.
 */
inline void updateCursorForEdge(QWidget *widget, ResizeEdge edge) {
  if (!widget) {
    return;
  }
  switch (edge) {
  case ResizeEdge::Left:
  case ResizeEdge::Right:
    widget->setCursor(Qt::SizeHorCursor);
    break;
  case ResizeEdge::Top:
  case ResizeEdge::Bottom:
    widget->setCursor(Qt::SizeVerCursor);
    break;
  case ResizeEdge::TopLeft:
  case ResizeEdge::BottomRight:
    widget->setCursor(Qt::SizeFDiagCursor);
    break;
  case ResizeEdge::TopRight:
  case ResizeEdge::BottomLeft:
    widget->setCursor(Qt::SizeBDiagCursor);
    break;
  case ResizeEdge::None:
    widget->unsetCursor();
    break;
  }
}

/**
 * @brief Invokes native Wayland/X11 system resize on the native window handle.
 */
inline bool triggerSystemResize(QWindow *handle, ResizeEdge edge) {
  if (!handle || edge == ResizeEdge::None) {
    return false;
  }

  Qt::Edges nativeEdges;
  if (edge == ResizeEdge::Left || edge == ResizeEdge::TopLeft ||
      edge == ResizeEdge::BottomLeft) {
    nativeEdges |= Qt::LeftEdge;
  }
  if (edge == ResizeEdge::Right || edge == ResizeEdge::TopRight ||
      edge == ResizeEdge::BottomRight) {
    nativeEdges |= Qt::RightEdge;
  }
  if (edge == ResizeEdge::Top || edge == ResizeEdge::TopLeft ||
      edge == ResizeEdge::TopRight) {
    nativeEdges |= Qt::TopEdge;
  }
  if (edge == ResizeEdge::Bottom || edge == ResizeEdge::BottomLeft ||
      edge == ResizeEdge::BottomRight) {
    nativeEdges |= Qt::BottomEdge;
  }

  return handle->startSystemResize(nativeEdges);
}

/**
 * @brief Invokes native Wayland/X11 system move on the native window handle.
 */
inline bool triggerSystemMove(QWindow *handle) {
  if (!handle) {
    return false;
  }
  return handle->startSystemMove();
}

/**
 * @brief Calculates new geometry during interactive mouse dragging fallback.
 */
inline QRect calculateManualResize(ResizeEdge edge,
                                   const QPoint &currentGlobalPos,
                                   const QPoint &startGlobalPos,
                                   const QRect &startGeometry,
                                   const QSize &minSize) {
  const int dx = currentGlobalPos.x() - startGlobalPos.x();
  const int dy = currentGlobalPos.y() - startGlobalPos.y();
  QRect newGeom = startGeometry;

  const int minW = minSize.width();
  const int minH = minSize.height();

  switch (edge) {
  case ResizeEdge::Left:
    newGeom.setLeft(
        std::min(startGeometry.left() + dx, startGeometry.right() - minW));
    break;
  case ResizeEdge::Right:
    newGeom.setRight(
        std::max(startGeometry.right() + dx, startGeometry.left() + minW));
    break;
  case ResizeEdge::Top:
    newGeom.setTop(
        std::min(startGeometry.top() + dy, startGeometry.bottom() - minH));
    break;
  case ResizeEdge::Bottom:
    newGeom.setBottom(
        std::max(startGeometry.bottom() + dy, startGeometry.top() + minH));
    break;
  case ResizeEdge::TopLeft:
    newGeom.setLeft(
        std::min(startGeometry.left() + dx, startGeometry.right() - minW));
    newGeom.setTop(
        std::min(startGeometry.top() + dy, startGeometry.bottom() - minH));
    break;
  case ResizeEdge::TopRight:
    newGeom.setRight(
        std::max(startGeometry.right() + dx, startGeometry.left() + minW));
    newGeom.setTop(
        std::min(startGeometry.top() + dy, startGeometry.bottom() - minH));
    break;
  case ResizeEdge::BottomLeft:
    newGeom.setLeft(
        std::min(startGeometry.left() + dx, startGeometry.right() - minW));
    newGeom.setBottom(
        std::max(startGeometry.bottom() + dy, startGeometry.top() + minH));
    break;
  case ResizeEdge::BottomRight:
    newGeom.setRight(
        std::max(startGeometry.right() + dx, startGeometry.left() + minW));
    newGeom.setBottom(
        std::max(startGeometry.bottom() + dy, startGeometry.top() + minH));
    break;
  default:
    break;
  }

  return newGeom;
}

/**
 * @class RoundedBorderOverlay
 * @brief Lớp phủ trong suốt vẽ đường viền bo góc mượt mà bằng
 * QPainter::Antialiasing nằm trên cùng các widget con để tránh bị cắt góc hoặc
 * răng cưa QSS.
 */
class RoundedBorderOverlay : public QWidget {
public:
  explicit RoundedBorderOverlay(QWidget *parent, bool isDark = true,
                                qreal radius = 8.0)
      : QWidget(parent), m_isDark(isDark), m_radius(radius) {
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setFocusPolicy(Qt::NoFocus);
    if (parent) {
      resize(parent->size());
      parent->installEventFilter(this);
      raise();
    }
  }

  void setDark(bool isDark) {
    if (m_isDark != isDark) {
      m_isDark = isDark;
      update();
    }
  }

  void setRadius(qreal radius) {
    if (m_radius != radius) {
      m_radius = radius;
      update();
    }
  }

protected:
  bool eventFilter(QObject *watched, QEvent *event) override {
    if (watched == parentWidget()) {
      if (event->type() == QEvent::Resize) {
        resize(parentWidget()->size());
        raise();
      } else if (event->type() == QEvent::Show ||
                 event->type() == QEvent::ChildAdded) {
        raise();
      }
    }
    return QWidget::eventFilter(watched, event);
  }

  void paintEvent(QPaintEvent *) override {
    if (m_radius <= 0.0 || width() <= 2 || height() <= 2) {
      return;
    }
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRectF bounds(0.0, 0.0, static_cast<qreal>(width()),
                        static_cast<qreal>(height()));

    // 1. Xóa sạch phần pixel của widget con bị tràn ra ngoài 4 góc bo tròn (có
    // anti-aliasing)
    QPainterPath fullRect;
    fullRect.addRect(bounds);

    QPainterPath innerRounded;
    innerRounded.addRoundedRect(bounds, m_radius, m_radius);

    const QPainterPath outsideCorners = fullRect.subtracted(innerRounded);
    p.setCompositionMode(QPainter::CompositionMode_Clear);
    p.fillPath(outsideCorners, Qt::black);

    // 2. Vẽ đường viền 1px bo góc mượt mà đè lên trên
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);
    const QColor borderColor =
        m_isDark ? QColor(0xa0, 0xb4, 0xdc, 0x29) : QColor(0xd4, 0xd4, 0xd8);
    p.setPen(QPen(borderColor, 1.0));
    p.setBrush(Qt::NoBrush);
    const QRectF strokeRect(0.5, 0.5, static_cast<qreal>(width()) - 1.0,
                            static_cast<qreal>(height()) - 1.0);
    p.drawRoundedRect(strokeRect, m_radius, m_radius);
  }

private:
  bool m_isDark{true};
  qreal m_radius{8.0};
};

} // namespace FramelessUtil
