/**
 * @file ui/views/detail/custom_field_row_widget.h
 * @brief Hàng hiển thị và chỉnh sửa một trường tùy biến (Custom Field) do người
 * dùng tự tạo.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#pragma once

#include "models/vault_item.h"

#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTimer>
#include <QWidget>

namespace svm::views {

class CustomFieldRowWidget : public QWidget {
  Q_OBJECT

public:
  explicit CustomFieldRowWidget(const models::UiCustomField &field = {},
                                QWidget *parent = nullptr);
  ~CustomFieldRowWidget() override = default;

  void setField(const models::UiCustomField &field);
  [[nodiscard]] models::UiCustomField field() const;

  void setEditMode(bool editable);
  [[nodiscard]] bool isEditMode() const { return m_isEditMode; }

  void focusLabelInput();

signals:
  void deleteRequested();
  void copyRequested(const QString &value, const QString &label);

private slots:
  void onToggleReveal();
  void onCopyClicked();
  void onKindChanged(int index);

private:
  void setupUI();
  void updateDisplay();

  models::UiCustomField m_field;
  bool m_isEditMode{false};
  bool m_isRevealed{false};

  // Chế độ xem (View Mode)
  QWidget *m_viewContainer{nullptr};
  QLabel *m_labelView{nullptr};
  QLabel *m_valueView{nullptr};
  QPushButton *m_revealBtn{nullptr};
  QPushButton *m_copyBtn{nullptr};

  // Chế độ sửa (Edit Mode)
  QWidget *m_editContainer{nullptr};
  QLineEdit *m_labelEdit{nullptr};
  QComboBox *m_kindCombo{nullptr};
  QLineEdit *m_valueEdit{nullptr};
  QPushButton *m_deleteBtn{nullptr};
  QPushButton *m_editRevealBtn{nullptr};

  QTimer m_autoHideTimer;
};

} // namespace svm::views
