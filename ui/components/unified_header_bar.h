/**
 * @file ui/components/unified_header_bar.h
 * @brief Thanh tiêu đề tích hợp (Unified Header Bar / Client-Side Decoration)
 * tương tự Notera.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#pragma once

#include <QLabel>
#include <QPoint>
#include <QPushButton>
#include <QWidget>

namespace svm::components {

class AppMenuBar;

class UnifiedHeaderBar : public QWidget {
  Q_OBJECT

public:
  explicit UnifiedHeaderBar(AppMenuBar *menuBar, QWidget *parent = nullptr);
  ~UnifiedHeaderBar() override = default;

  /**
   * @brief Cập nhật nội dung tiêu đề hiển thị ở vị trí chính giữa thanh tiêu
   * đề.
   */
  void setTitleText(const QString &title);

  /**
   * @brief Cập nhật trạng thái biểu tượng nút phóng to / khôi phục cửa sổ.
   */
  void updateMaximizeIcon(bool isMaximized);

  /**
   * @brief Cập nhật bộ biểu tượng các nút điều khiển theo chủ đề sáng hoặc tối.
   */
  void updateThemeIcons(bool darkMode);

signals:
  void minimizeRequested();
  void maximizeRestoreRequested();
  void closeRequested();
  void commandPaletteRequested();

protected:
  void paintEvent(QPaintEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
  void repositionTitleOverlay();

  QLabel *m_logoLabel{nullptr};
  QPushButton *m_paletteBtn{nullptr};
  AppMenuBar *m_menuBar{nullptr};
  QLabel *m_titleLabel{nullptr};
  QPushButton *m_minBtn{nullptr};
  QPushButton *m_maxBtn{nullptr};
  QPushButton *m_closeBtn{nullptr};

  bool m_isMaximized{false};
  bool m_darkMode{true};
  bool m_manualDragging{false};
  QPoint m_dragPosition;
};

} // namespace svm::components
