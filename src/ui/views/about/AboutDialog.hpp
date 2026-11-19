/**
 * @file src/ui/views/about/AboutDialog.hpp
 * @brief Hộp thoại giới thiệu ứng dụng AboutDialog hiện đại, tích hợp thông tin
 * ứng dụng, Qt Framework (LGPL v3) và License.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#pragma once

#include "ui/window/FramelessDialog.hpp"
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabWidget>

namespace svm::views {

/**
 * @class AboutDialog
 * @brief Hộp thoại About 3 tab (Secure Vault Manager, Qt Framework, License)
 * với thiết kế thẻ (Card) hiện đại.
 */
class AboutDialog : public FramelessDialog {
  Q_OBJECT

public:
  explicit AboutDialog(QWidget *parent = nullptr, int initialTab = 0);
  ~AboutDialog() override = default;

  void retranslateUi();

protected:
  void changeEvent(QEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;

private:
  void setupUI(int initialTab);
  QWidget *createHeaderBar();
  QWidget *createAppTab();
  QWidget *createQtTab();
  QWidget *createLicenseTab();
  void populateAppTabContent();
  void populateQtTabContent();

  QWidget *m_cardWidget{nullptr};
  QWidget *m_headerWidget{nullptr};
  QLabel *m_headerTitle{nullptr};
  QPushButton *m_closeIconBtn{nullptr};

  QTabWidget *m_tabWidget{nullptr};
  QWidget *m_appTab{nullptr};
  QWidget *m_qtTab{nullptr};
  QWidget *m_licenseTab{nullptr};

  // Tab 1: Secure Vault Manager
  QLabel *m_appHeroTitle{nullptr};
  QLabel *m_appHeroSub{nullptr};
  QLabel *m_featuresTitle{nullptr};
  QLabel *m_featuresDesc{nullptr};
  QLabel *m_productTitle{nullptr};
  QLabel *m_productDesc{nullptr};

  // Tab 2: Qt Framework
  QLabel *m_qtHeroTitle{nullptr};
  QLabel *m_qtHeroSub{nullptr};
  QLabel *m_qtDescLabel{nullptr};
  QLabel *m_lgplTitle{nullptr};
  QLabel *m_lgplDesc{nullptr};
  QLabel *m_tmTitle{nullptr};
  QLabel *m_tmDesc{nullptr};

  // Tab 3: License
  QPlainTextEdit *m_licenseEdit{nullptr};

  QPushButton *m_bottomCloseBtn{nullptr};
};

} // namespace svm::views
