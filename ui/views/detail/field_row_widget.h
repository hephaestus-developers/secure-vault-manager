/**
 * @file ui/views/detail/field_row_widget.h
 * @brief Hàng trường dữ liệu đa năng hỗ trợ ẩn/hiện mật khẩu, copy và inline
 * editing.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#pragma once

#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTimer>
#include <QWidget>

namespace svm::views {

class FieldRowWidget : public QWidget {
  Q_OBJECT

public:
  enum class FieldType { Text, Concealed, Url, Multiline };

  explicit FieldRowWidget(const QString &label,
                          FieldType type = FieldType::Text,
                          QWidget *parent = nullptr);
  ~FieldRowWidget() override = default;

  void setValue(const QString &value);
  [[nodiscard]] QString value() const;

  void setLabel(const QString &label);
  void setPlaceholderText(const QString &placeholder);

  void setEditMode(bool editable);
  [[nodiscard]] bool isEditMode() const { return m_isEditMode; }

signals:
  void copyRequested(const QString &value, const QString &label);

private slots:
  void onToggleReveal();
  void onCopyClicked();

private:
  void setupUI();
  void updateDisplay();
  void updateThemeIcons();

  QString m_label;
  FieldType m_type{FieldType::Text};
  QString m_value;
  bool m_isEditMode{false};
  bool m_isRevealed{false};

  QLabel *m_labelWidget{nullptr};
  QLabel *m_valueLabel{nullptr};
  QLineEdit *m_lineEdit{nullptr};
  QPlainTextEdit *m_plainTextEdit{nullptr};
  QPushButton *m_revealBtn{nullptr};
  QPushButton *m_copyBtn{nullptr};
  QPushButton *m_openUrlBtn{nullptr};

  QTimer m_autoHideTimer;
};

} // namespace svm::views
