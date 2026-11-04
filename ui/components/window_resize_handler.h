/**
 * @file ui/components/window_resize_handler.h
 * @brief Bộ điều phối và xử lý thao tác co giãn kích thước cho cửa sổ
 * Frameless.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#pragma once

#include <QObject>
#include <QPoint>
#include <QRect>
#include <QWidget>

namespace svm::components {

class ResizeHandle : public QWidget {
  Q_OBJECT

public:
  ResizeHandle(Qt::Edges edges, QWidget *window);

protected:
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void paintEvent(QPaintEvent *event) override;

private:
  Qt::Edges m_edges;
  QWidget *m_window{nullptr};
  bool m_isResizing{false};
  QPoint m_dragStartPos;
  QRect m_dragStartGeometry;
};

class WindowResizeHandler : public QObject {
  Q_OBJECT

public:
  static constexpr int BorderWidth = 6;
  static constexpr int CornerLength = 12;

  explicit WindowResizeHandler(QWidget *window);

  void updateGeometry();
  void setVisible(bool visible);

private:
  QWidget *m_window{nullptr};

  ResizeHandle *m_topLeft{nullptr};
  ResizeHandle *m_top{nullptr};
  ResizeHandle *m_topRight{nullptr};
  ResizeHandle *m_left{nullptr};
  ResizeHandle *m_right{nullptr};
  ResizeHandle *m_bottomLeft{nullptr};
  ResizeHandle *m_bottom{nullptr};
  ResizeHandle *m_bottomRight{nullptr};
};

} // namespace svm::components
