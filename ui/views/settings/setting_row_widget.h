/**
 * @file ui/views/settings/setting_row_widget.h
 * @brief Widget chuẩn hóa cho từng hàng cấu hình (Setting Row) gồm tiêu đề, mô
 * tả và control điều khiển.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#pragma once

#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

namespace svm::views {

class SettingRowWidget : public QWidget {
  Q_OBJECT

public:
  explicit SettingRowWidget(const QString &title, const QString &description,
                            QWidget *controlWidget, QWidget *parent = nullptr);
  ~SettingRowWidget() override = default;

  void updateText(const QString &title, const QString &description);

private:
  QLabel *m_titleLabel{nullptr};
  QLabel *m_descLabel{nullptr};
  QWidget *m_controlWidget{nullptr};
};

/**
 * @brief Khung thẻ nhóm các hàng cài đặt theo phân hệ (Setting Card Container).
 */
class SettingSectionCard : public QWidget {
  Q_OBJECT

public:
  explicit SettingSectionCard(const QString &title, QWidget *parent = nullptr);
  ~SettingSectionCard() override = default;

  void setTitle(const QString &title);
  void addRow(SettingRowWidget *row);
  void addWidget(QWidget *widget);

private:
  QLabel *m_titleLabel{nullptr};
  QVBoxLayout *m_cardLayout{nullptr};
};

} // namespace svm::views
