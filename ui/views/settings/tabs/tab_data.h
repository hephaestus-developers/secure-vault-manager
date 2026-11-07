/**
 * @file ui/views/settings/tabs/tab_data.h
 * @brief Phân hệ Quản lý Dữ liệu, Nhập/Xuất & Dọn dẹp Thùng rác.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#pragma once

#include "views/settings/setting_row_widget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QWidget>

namespace svm::views {

class TabData : public QWidget {
  Q_OBJECT

public:
  explicit TabData(QWidget *parent = nullptr);
  ~TabData() override = default;

  void retranslateUi();

private:
  void setupUI();
  void onImportClicked();
  void onExportEncryptedClicked();
  void onExportPlainClicked();
  void onFactoryResetClicked();

  SettingSectionCard *m_importCard{nullptr};
  QComboBox *m_importFormatCombo{nullptr};
  QPushButton *m_importBtn{nullptr};
  SettingRowWidget *m_importRow{nullptr};

  SettingSectionCard *m_exportCard{nullptr};
  QPushButton *m_exportEncryptedBtn{nullptr};
  SettingRowWidget *m_exportEncryptedRow{nullptr};
  QPushButton *m_exportPlainBtn{nullptr};
  SettingRowWidget *m_exportPlainRow{nullptr};

  SettingSectionCard *m_trashCard{nullptr};
  QCheckBox *m_trashAutoEmptyCheck{nullptr};
  SettingRowWidget *m_trashAutoEmptyRow{nullptr};
  QPushButton *m_emptyTrashNowBtn{nullptr};
  SettingRowWidget *m_emptyTrashNowRow{nullptr};

  SettingSectionCard *m_dangerCard{nullptr};
  QPushButton *m_factoryResetBtn{nullptr};
  SettingRowWidget *m_factoryResetRow{nullptr};
};

} // namespace svm::views
