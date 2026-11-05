/**
 * @file ui/views/detail/custom_fields_widget.cpp
 * @brief Hiện thực Section Card cho danh sách trường tùy biến.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "custom_fields_widget.h"
#include "app/theme_manager.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QUuid>

namespace svm::views {

CustomFieldsWidget::CustomFieldsWidget(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("DetailSectionCard"));
  setAttribute(Qt::WA_StyledBackground, true);
  setupUI();
}

void CustomFieldsWidget::setupUI() {
  auto *rootLayout = new QVBoxLayout(this);
  rootLayout->setContentsMargins(16, 14, 16, 14);
  rootLayout->setSpacing(10);

  // 1. Tiêu đề khối
  m_sectionTitle = new QLabel(tr("CUSTOM FIELDS"), this);
  m_sectionTitle->setObjectName(QStringLiteral("DetailSectionHeader"));
  rootLayout->addWidget(m_sectionTitle);

  // 2. Danh sách các hàng trường tùy biến
  m_rowsLayout = new QVBoxLayout();
  m_rowsLayout->setContentsMargins(0, 0, 0, 0);
  m_rowsLayout->setSpacing(8);
  rootLayout->addLayout(m_rowsLayout);

  // 3. Thông báo rỗng
  m_emptyLabel = new QLabel(
      tr("No custom fields yet. Click \"+ Add Field\" to create one."), this);
  m_emptyLabel->setObjectName(QStringLiteral("DetailSectionEmptyLabel"));
  m_emptyLabel->setStyleSheet(
      QStringLiteral("color: #64748B; font-style: italic; padding: 4px 0;"));
  rootLayout->addWidget(m_emptyLabel);

  // 4. Nút Thêm trường
  m_addFieldBtn = new QPushButton(this);
  m_addFieldBtn->setObjectName(QStringLiteral("CustomFieldAddBtn"));
  m_addFieldBtn->setIcon(
      app::ThemeManager::instance().icon(QStringLiteral("plus")));
  m_addFieldBtn->setIconSize(QSize(14, 14));
  m_addFieldBtn->setText(tr(" Add Field"));
  m_addFieldBtn->setFixedHeight(32);
  m_addFieldBtn->setCursor(Qt::PointingHandCursor);
  m_addFieldBtn->setToolTip(tr("Add a custom field (Text or Concealed)"));
  connect(m_addFieldBtn, &QPushButton::clicked, this,
          &CustomFieldsWidget::onAddFieldClicked);
  connect(&app::ThemeManager::instance(), &app::ThemeManager::themeChanged,
          this, [this]() {
            m_addFieldBtn->setIcon(
                app::ThemeManager::instance().icon(QStringLiteral("plus")));
          });
  rootLayout->addWidget(m_addFieldBtn);

  setEditMode(false);
}

void CustomFieldsWidget::setCustomFields(
    const QList<models::UiCustomField> &fields) {
  // Xóa toàn bộ row cũ
  for (auto *w : m_rowWidgets) {
    m_rowsLayout->removeWidget(w);
    w->deleteLater();
  }
  m_rowWidgets.clear();

  // Nạp danh sách mới
  for (const auto &f : fields) {
    addRow(f, false);
  }

  updateVisibility();
}

QList<models::UiCustomField> CustomFieldsWidget::customFields() const {
  QList<models::UiCustomField> list;
  for (auto *w : m_rowWidgets) {
    const auto f = w->field();
    if (!f.label.isEmpty() || !f.value.isEmpty()) {
      list.append(f);
    }
  }
  return list;
}

void CustomFieldsWidget::setEditMode(bool editable) {
  m_isEditMode = editable;

  for (auto *w : m_rowWidgets) {
    w->setEditMode(editable);
  }

  m_addFieldBtn->setVisible(editable);
  updateVisibility();
}

void CustomFieldsWidget::onAddFieldClicked() {
  models::UiCustomField newField;
  newField.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
  newField.kind = models::CustomFieldKind::Text;
  addRow(newField, true);
}

void CustomFieldsWidget::addRow(const models::UiCustomField &field,
                                bool focusImmediately) {
  auto *rowWidget = new CustomFieldRowWidget(field, this);
  rowWidget->setEditMode(m_isEditMode);

  connect(rowWidget, &CustomFieldRowWidget::deleteRequested, this,
          [this, rowWidget]() {
            m_rowWidgets.removeOne(rowWidget);
            m_rowsLayout->removeWidget(rowWidget);
            rowWidget->deleteLater();
            updateVisibility();
          });

  m_rowWidgets.append(rowWidget);
  m_rowsLayout->addWidget(rowWidget);

  updateVisibility();

  if (focusImmediately) {
    rowWidget->focusLabelInput();
  }
}

void CustomFieldsWidget::updateVisibility() {
  const bool hasItems = !m_rowWidgets.isEmpty();
  m_emptyLabel->setVisible(m_isEditMode && !hasItems);

  // Ở chế độ xem: Nếu không có custom field nào thì ẩn cả section để tiết kiệm
  // không gian
  if (!m_isEditMode && !hasItems) {
    hide();
  } else {
    show();
  }
}

} // namespace svm::views
