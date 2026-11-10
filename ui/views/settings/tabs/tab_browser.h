/**
 * @file ui/views/settings/tabs/tab_browser.h
 * @brief Phân hệ Tích hợp Trình duyệt Web & Trình duyệt phát hiện trên hệ
 * thống.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#pragma once

#include "views/settings/setting_row_widget.h"

#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QStringList>
#include <QVBoxLayout>
#include <QWidget>

namespace svm::views {

struct DetectedBrowserInfo {
  QString id;
  QString name;
  QString description;
  QString iconPath;
  QStringList binaryCandidates;
  QStringList desktopCandidates;
  QString storeUrl;
  bool isSupported{true};
  bool isInstalled{false};
  bool isConnected{false};
};

class TabBrowser : public QWidget {
  Q_OBJECT

public:
  explicit TabBrowser(QWidget *parent = nullptr);
  ~TabBrowser() override = default;

  void retranslateUi();

public slots:
  void refreshSupportedBrowsers();

protected:
  void showEvent(QShowEvent *event) override;

private:
  void setupUI();
  void populateBrowsers();
  bool isBrowserInstalled(const DetectedBrowserInfo &info) const;

  SettingSectionCard *m_bridgeCard{nullptr};
  SettingRowWidget *m_enableRow{nullptr};
  QCheckBox *m_enableCheck{nullptr};
  QWidget *m_hintBox{nullptr};
  QLabel *m_hintLabel{nullptr};

  SettingSectionCard *m_detectedCard{nullptr};
  QVBoxLayout *m_browsersLayout{nullptr};
};

} // namespace svm::views
