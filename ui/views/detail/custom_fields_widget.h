/**
 * @file ui/views/detail/custom_fields_widget.h
 * @brief Khối hiển thị và quản lý danh sách các trường tùy biến (Custom Fields
 * Section).
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#pragma once

#include "custom_field_row_widget.h"
#include "models/vault_item.h"

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

namespace svm::views {

class CustomFieldsWidget : public QWidget {
  Q_OBJECT

public:
  explicit CustomFieldsWidget(QWidget *parent = nullptr);
  ~CustomFieldsWidget() override = default;

  void setCustomFields(const QList<models::UiCustomField> &fields);
  [[nodiscard]] QList<models::UiCustomField> customFields() const;

  void setEditMode(bool editable);
  [[nodiscard]] bool isEditMode() const { return m_isEditMode; }

private slots:
  void onAddFieldClicked();

private:
  void setupUI();
  void addRow(const models::UiCustomField &field,
              bool focusImmediately = false);
  void updateVisibility();

  bool m_isEditMode{false};
  QVBoxLayout *m_rowsLayout{nullptr};
  QList<CustomFieldRowWidget *> m_rowWidgets;

  QLabel *m_sectionTitle{nullptr};
  QLabel *m_emptyLabel{nullptr};
  QPushButton *m_addFieldBtn{nullptr};
};

} // namespace svm::views
