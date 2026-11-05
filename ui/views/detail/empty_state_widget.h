/**
 * @file ui/views/detail/empty_state_widget.h
 * @brief Widget hiển thị trạng thái trống khi chưa có bản ghi nào được chọn.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#pragma once

#include <QWidget>

namespace svm::views {

class EmptyStateWidget : public QWidget {
  Q_OBJECT

public:
  explicit EmptyStateWidget(QWidget *parent = nullptr);
  ~EmptyStateWidget() override = default;

private:
  void setupUI();
};

} // namespace svm::views
