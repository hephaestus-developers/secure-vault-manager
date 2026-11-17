/**
 * @file ui/views/detail/empty_state_widget.h
 * @brief Widget hiển thị trạng thái trống hiện đại khi chưa chọn bản ghi nào.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#pragma once

#include <QLabel>
#include <QPushButton>
#include <QWidget>

namespace svm::views {

class EmptyStateWidget : public QWidget {
  Q_OBJECT

public:
  explicit EmptyStateWidget(QWidget *parent = nullptr);
  ~EmptyStateWidget() override = default;

  void retranslateUi();

signals:
  void createItemRequested();

protected:
  void changeEvent(QEvent *event) override;

private:
  void setupUI();
  void updateThemeIcon();

  QWidget *m_iconContainer{nullptr};
  QLabel *m_iconLabel{nullptr};
  QLabel *m_titleLabel{nullptr};
  QLabel *m_descLabel{nullptr};

  QWidget *m_cardWidget{nullptr};
  QLabel *m_actionNewItemLabel{nullptr};
  QLabel *m_actionSearchLabel{nullptr};
  QLabel *m_actionSidebarLabel{nullptr};

  QPushButton *m_newItemBtn{nullptr};
};

} // namespace svm::views
