/**
 * @file FramelessUtil.hpp
 * @brief Common utilities, edge calculations, cursor shapes, and system
 * resize/move protocol helpers shared between FramelessWindow and
 * FramelessDialog.
 */

#pragma once

#include <QCursor>
#include <QPoint>
#include <QRect>
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

} // namespace FramelessUtil
