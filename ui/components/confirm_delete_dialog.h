/**
 * @file ui/components/confirm_delete_dialog.h
 * @brief Hộp thoại xác nhận xóa mục dữ liệu (Chuyển vào Thùng rác / Xóa vĩnh
 * viễn) chuẩn Frameless.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#pragma once

#include "models/vault_item.h"

#include <QDialog>
#include <QLabel>
#include <QPushButton>

namespace svm::components {

class ConfirmDeleteDialog : public QDialog {
  Q_OBJECT

public:
  explicit ConfirmDeleteDialog(const models::UiVaultItem &item, bool permanent,
                               QWidget *parent = nullptr);
  ~ConfirmDeleteDialog() override = default;

  static bool confirm(QWidget *parent, const models::UiVaultItem &item,
                      bool permanent);

protected:
  void keyPressEvent(QKeyEvent *event) override;

private:
  void setupUI();
  void updateThemeIcons();

  models::UiVaultItem m_item;
  bool m_permanent{false};

  QWidget *m_cardWidget{nullptr};
  QWidget *m_headerWidget{nullptr};
  QLabel *m_headerIconLabel{nullptr};
  QLabel *m_titleLabel{nullptr};
  QPushButton *m_closeBtn{nullptr};

  QLabel *m_badgeIconLabel{nullptr};
  QLabel *m_itemIconLabel{nullptr};
  QPushButton *m_cancelBtn{nullptr};
  QPushButton *m_confirmBtn{nullptr};
};

} // namespace svm::components
