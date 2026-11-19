/**
 * @file FramelessDialog.hpp
 * @brief Base class for modern frameless desktop dialogs with CSD, resizing,
 * and WindowManager integration.
 */

#pragma once

#include "FramelessUtil.hpp"

#include <QDialog>
#include <QPoint>
#include <QRect>

class QVBoxLayout;

/**
 * @class FramelessDialog
 * @brief Base dialog entity encapsulating frameless window behavior, CSD
 * dragging, edge resizing, system attention shake animation, and window tree
 * lifecycle.
 */
class FramelessDialog : public QDialog {
  Q_OBJECT
  Q_PROPERTY(int shakeOffset READ shakeOffset WRITE setShakeOffset)

public:
  /**
   * @brief Constructs a frameless dialog.
   * @param parent Host/parent widget (registered as hierarchy ancestor).
   * @param isDark Theme mode.
   */
  explicit FramelessDialog(QWidget *parent = nullptr, bool isDark = true);

  /**
   * @brief Virtual destructor. Unregisters dialog from WindowManager.
   */
  ~FramelessDialog() override;

  /**
   * @brief Returns current horizontal shake offset in pixels.
   */
  int shakeOffset() const;

  /**
   * @brief Sets horizontal shake offset and updates layout margins without
   * redrawing artifacts.
   * @param offset Pixel offset (-12 to +12).
   */
  void setShakeOffset(int offset);

  /**
   * @brief Returns theme mode.
   */
  bool isDark() const;

  /**
   * @brief Sets theme mode.
   */
  void setDark(bool isDark);

  /**
   * @brief Sets the main card widget for the dialog.
   * @param cardWidget The card widget containing header, body and buttons.
   */
  void setCardWidget(QWidget *cardWidget);

  /**
   * @brief Returns the card widget.
   */
  QWidget *cardWidget() const { return m_cardWidget; }

  /**
   * @brief Compatibility accessor for inner container.
   */
  QWidget *container() const { return m_cardWidget; }

  /**
   * @brief Compatibility accessor for root layout.
   */
  QVBoxLayout *containerLayout() const { return m_rootLayout; }

  /**
   * @brief Registers the CSD header bar widget for window dragging.
   * @param headerBar Widget acting as window title bar.
   */
  void setHeaderBar(QWidget *headerBar);

  /**
   * @brief Returns the header bar widget.
   */
  QWidget *headerBar() const { return m_headerBar; }

  /**
   * @brief Executes dialog event loop without triggering Wayland modal grab
   * freeze.
   */
  int exec() override;

  /**
   * @brief Returns true if this dialog can be manually resized by dragging
   * borders.
   */
  bool isResizable() const;

protected:
  void showEvent(QShowEvent *event) override;
  void paintEvent(QPaintEvent *event) override;
  bool eventFilter(QObject *watched, QEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;

  bool handleMousePress(QMouseEvent *event);
  bool handleMouseMove(QMouseEvent *event);
  bool handleMouseRelease(QMouseEvent *event);

  bool m_isDark{true};
  bool m_isDragging{false};
  QPoint m_dragPosition;

  FramelessUtil::ResizeEdge m_resizingEdge{FramelessUtil::ResizeEdge::None};
  QPoint m_resizeStartPos;
  QRect m_resizeStartGeometry;

  QVBoxLayout *m_rootLayout{nullptr};
  QWidget *m_cardWidget{nullptr};
  QWidget *m_headerBar{nullptr};
  FramelessUtil::RoundedBorderOverlay *m_borderOverlay{nullptr};

  int m_shakeOffset{0};
  static constexpr int MARGIN = 10;
};
