/**
 * @file FramelessWindow.hpp
 * @brief Base class for modern frameless top-level application windows with
 * CSD, custom border resize, maximize/restore tracking, and WindowManager
 * integration.
 */

#pragma once

#include "FramelessUtil.hpp"

#include <QMainWindow>
#include <QPoint>
#include <QRect>

class QWidget;

/**
 * @class FramelessWindow
 * @brief Base class providing frameless window setup, border resizing, CSD
 * window control integration, and root window registration with WindowManager.
 */
class FramelessWindow : public QMainWindow {
  Q_OBJECT

public:
  /**
   * @brief Constructs a frameless top-level window.
   * @param parent Optional parent widget.
   */
  explicit FramelessWindow(QWidget *parent = nullptr);

  /**
   * @brief Virtual destructor.
   */
  ~FramelessWindow() override = default;

  /**
   * @brief Registers the custom header bar and connects window control signals.
   * @param headerBar Pointer to HeaderBar / UnifiedHeaderBar instance.
   */
  virtual void setHeaderBar(QWidget *headerBar);

  /**
   * @brief Returns current header bar pointer.
   */
  QWidget *headerBar() const { return m_headerBar; }

  /**
   * @brief Returns the border resize margin in pixels.
   */
  int resizeMargin() const { return m_resizeMargin; }

  /**
   * @brief Sets the border resize margin in pixels.
   */
  void setResizeMargin(int margin) { m_resizeMargin = margin; }

protected:
  void showEvent(QShowEvent *event) override;
  void changeEvent(QEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;

  QWidget *m_headerBar{nullptr};
  int m_resizeMargin{8};

private:
  FramelessUtil::ResizeEdge m_resizingEdge{FramelessUtil::ResizeEdge::None};
  QPoint m_resizeStartPos;
  QRect m_resizeStartGeometry;
};
