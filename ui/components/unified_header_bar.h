/**
 * @file ui/components/unified_header_bar.h
 * @brief Thanh tiêu đề tích hợp (Unified Header Bar / Client-Side Decoration)
 * tương tự Notera.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#pragma once

#include <QLabel>
#include <QList>
#include <QMenu>
#include <QPoint>
#include <QPushButton>
#include <QWidget>

namespace svm::components {

class UnifiedHeaderBar : public QWidget {
  Q_OBJECT

public:
  explicit UnifiedHeaderBar(QWidget *parent = nullptr);
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

  /**
   * @brief Cập nhật lại chuỗi hiển thị theo ngôn ngữ đang chọn.
   */
  void retranslateUi();

  /**
   * @brief Ẩn/Hiện các mục chỉ khả dụng sau khi mở khóa (Vault, Item,
   * Settings).
   */
  void setUnlockedMenuVisible(bool unlocked);
  void setSettingsVisible(bool visible);

  [[nodiscard]] QMenu *vaultMenu() const { return m_vaultMenu; }
  [[nodiscard]] QMenu *itemMenu() const { return m_itemMenu; }
  [[nodiscard]] QMenu *viewMenu() const { return m_viewMenu; }
  [[nodiscard]] QMenu *helpMenu() const { return m_helpMenu; }

signals:
  void minimizeRequested();
  void maximizeRestoreRequested();
  void closeRequested();
  void themeToggleRequested();
  void settingsRequested();

protected:
  bool eventFilter(QObject *obj, QEvent *event) override;
  void changeEvent(QEvent *event) override;
  void paintEvent(QPaintEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
  struct MenuEntry {
    QPushButton *button{nullptr};
    QMenu *menu{nullptr};
    const char *titleSource{nullptr};
  };

  void addMenuEntry(const char *titleSource, QMenu *&outMenu,
                    QPushButton *&outBtn, QLayout *parentLayout);
  void repositionTitleOverlay();

  QLabel *m_logoLabel{nullptr};
  QWidget *m_menuBarWidget{nullptr};
  QPushButton *m_vaultMenuBtn{nullptr};
  QPushButton *m_itemMenuBtn{nullptr};
  QPushButton *m_viewMenuBtn{nullptr};
  QPushButton *m_helpMenuBtn{nullptr};
  QMenu *m_vaultMenu{nullptr};
  QMenu *m_itemMenu{nullptr};
  QMenu *m_viewMenu{nullptr};
  QMenu *m_helpMenu{nullptr};
  QList<MenuEntry> m_menuEntries;

  QPushButton *m_themeToggleBtn{nullptr};
  QPushButton *m_settingsBtn{nullptr};
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
