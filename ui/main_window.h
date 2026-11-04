/**
 * @file ui/main_window.h
 * @brief Cửa sổ chính MainWindow tích hợp UnifiedHeaderBar và
 * WindowResizeHandler tương tự Notera.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#pragma once

#include <QLabel>
#include <QMainWindow>

namespace svm::components {
class AppMenuBar;
class UnifiedHeaderBar;
class WindowResizeHandler;
} // namespace svm::components

namespace svm {

class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow() override = default;

protected:
  void resizeEvent(QResizeEvent *event) override;
  void changeEvent(QEvent *event) override;

private:
  void setupUI();

  components::AppMenuBar *m_menuBar{nullptr};
  components::UnifiedHeaderBar *m_headerBar{nullptr};
  components::WindowResizeHandler *m_resizeHandler{nullptr};

  QWidget *m_contentArea{nullptr};
  QLabel *m_statusLabel{nullptr};
};

} // namespace svm
