/**
 * @file ui/views/about/about_dialog.h
 * @brief Hộp thoại giới thiệu ứng dụng AboutDialog thẩm mỹ cao chuẩn Notera.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#pragma once

#include <QDialog>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPoint>
#include <QPushButton>
#include <QTabWidget>

namespace svm::views {

class AboutDialog : public QDialog {
  Q_OBJECT

public:
  explicit AboutDialog(QWidget *parent = nullptr);
  ~AboutDialog() override = default;

  void retranslateUi();

protected:
  void changeEvent(QEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;

private:
  void setupUI();
  QWidget *createInfoTab();
  QWidget *createLicenseTab();

  QWidget *m_cardWidget{nullptr};
  QWidget *m_headerWidget{nullptr};
  QLabel *m_headerTitle{nullptr};
  QPushButton *m_closeIconBtn{nullptr};

  QTabWidget *m_tabWidget{nullptr};
  QWidget *m_infoTab{nullptr};
  QWidget *m_licenseTab{nullptr};

  QLabel *m_appNameLabel{nullptr};
  QLabel *m_versionLabel{nullptr};
  QLabel *m_taglineLabel{nullptr};
  QLabel *m_descLabel{nullptr};

  QLabel *m_authorKeyLabel{nullptr};
  QLabel *m_authorValLabel{nullptr};
  QLabel *m_emailKeyLabel{nullptr};
  QLabel *m_emailValLabel{nullptr};
  QLabel *m_orgKeyLabel{nullptr};
  QLabel *m_orgValLabel{nullptr};
  QLabel *m_copyKeyLabel{nullptr};
  QLabel *m_copyValLabel{nullptr};
  QLabel *m_repoKeyLabel{nullptr};
  QLabel *m_repoValLabel{nullptr};

  QPlainTextEdit *m_licenseEdit{nullptr};
  QPushButton *m_bottomCloseBtn{nullptr};
};

} // namespace svm::views
