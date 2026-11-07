/**
 * @file ui/views/settings/tabs/tab_general.cpp
 * @brief Hiện thực giao diện Cài đặt Chung.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#include "tab_general.h"
#include "app/settings_manager.h"
#include "components/in_window_combobox.h"

#include <QVBoxLayout>

namespace svm::views {

TabGeneral::TabGeneral(QWidget *parent) : QWidget(parent) {
  setAttribute(Qt::WA_StyledBackground, true);
  setupUI();
}

void TabGeneral::setupUI() {
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(16);

  auto &settings = app::SettingsManager::instance();

  // 1. Thẻ Appearance
  m_appearanceCard = new SettingSectionCard(tr("Appearance"), this);

  m_themeCombo = new svm::ui::InWindowComboBox(m_appearanceCard);
  m_themeCombo->addItem(tr("Dark Theme"),
                        static_cast<int>(app::ThemeMode::Dark));
  m_themeCombo->addItem(tr("Light Theme"),
                        static_cast<int>(app::ThemeMode::Light));
  m_themeCombo->setCurrentIndex(
      (settings.themeMode() == app::ThemeMode::Dark) ? 0 : 1);
  m_themeCombo->setFixedWidth(150);
  m_themeCombo->setFixedHeight(32);
  m_themeCombo->setCursor(Qt::PointingHandCursor);
  connect(m_themeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this](int index) {
            const auto mode =
                (index == 0) ? app::ThemeMode::Dark : app::ThemeMode::Light;
            app::SettingsManager::instance().setThemeMode(mode);
          });

  m_themeRow = new SettingRowWidget(
      tr("Color Theme"),
      tr("Choose your preferred application visual appearance."), m_themeCombo,
      m_appearanceCard);
  m_appearanceCard->addRow(m_themeRow);

  m_densityCombo = new svm::ui::InWindowComboBox(m_appearanceCard);
  m_densityCombo->addItem(tr("Comfortable"),
                          static_cast<int>(app::ListDensity::Comfortable));
  m_densityCombo->addItem(tr("Compact"),
                          static_cast<int>(app::ListDensity::Compact));
  m_densityCombo->setCurrentIndex(
      (settings.listDensity() == app::ListDensity::Comfortable) ? 0 : 1);
  m_densityCombo->setFixedWidth(150);
  m_densityCombo->setFixedHeight(32);
  m_densityCombo->setCursor(Qt::PointingHandCursor);
  connect(m_densityCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this](int index) {
            const auto density = (index == 0) ? app::ListDensity::Comfortable
                                              : app::ListDensity::Compact;
            app::SettingsManager::instance().setListDensity(density);
          });

  m_densityRow = new SettingRowWidget(
      tr("List Density"),
      tr("Adjust item row height and spacing in the vault list."),
      m_densityCombo, m_appearanceCard);
  m_appearanceCard->addRow(m_densityRow);

  layout->addWidget(m_appearanceCard);

  // 2. Thẻ Language
  m_languageCard = new SettingSectionCard(tr("Language"), this);

  m_langCombo = new svm::ui::InWindowComboBox(m_languageCard);
  m_langCombo->addItem(QStringLiteral("English (US)"),
                       static_cast<int>(app::AppLanguage::English));
  m_langCombo->addItem(QStringLiteral("Tiếng Việt"),
                       static_cast<int>(app::AppLanguage::Vietnamese));
  m_langCombo->setCurrentIndex(
      (settings.language() == app::AppLanguage::English) ? 0 : 1);
  m_langCombo->setFixedWidth(150);
  m_langCombo->setFixedHeight(32);
  m_langCombo->setCursor(Qt::PointingHandCursor);
  connect(m_langCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this](int index) {
            const auto lang = (index == 0) ? app::AppLanguage::English
                                           : app::AppLanguage::Vietnamese;
            app::SettingsManager::instance().setLanguage(lang);
          });

  m_langRow = new SettingRowWidget(
      tr("Application Language"),
      tr("Select the language for all user interface text."), m_langCombo,
      m_languageCard);
  m_languageCard->addRow(m_langRow);

  layout->addWidget(m_languageCard);

  // 3. Thẻ System
  m_systemCard = new SettingSectionCard(tr("System Behavior"), this);

  m_startupCheck = new QCheckBox(m_systemCard);
  m_startupCheck->setChecked(settings.launchOnStartup());
  connect(m_startupCheck, &QCheckBox::toggled, this, [](bool checked) {
    app::SettingsManager::instance().setLaunchOnStartup(checked);
  });
  m_startupRow = new SettingRowWidget(
      tr("Launch on Startup"),
      tr("Automatically start Secure Vault Manager when your computer boots."),
      m_startupCheck, m_systemCard);
  m_systemCard->addRow(m_startupRow);

  m_trayCheck = new QCheckBox(m_systemCard);
  m_trayCheck->setChecked(settings.closeToTray());
  connect(m_trayCheck, &QCheckBox::toggled, this, [](bool checked) {
    app::SettingsManager::instance().setCloseToTray(checked);
  });
  m_trayRow =
      new SettingRowWidget(tr("Close to System Tray"),
                           tr("Closing the main window will minimize it to the "
                              "system tray instead of exiting."),
                           m_trayCheck, m_systemCard);
  m_systemCard->addRow(m_trayRow);

  m_minimizedCheck = new QCheckBox(m_systemCard);
  m_minimizedCheck->setChecked(settings.startMinimized());
  connect(m_minimizedCheck, &QCheckBox::toggled, this, [](bool checked) {
    app::SettingsManager::instance().setStartMinimized(checked);
  });
  m_minimizedRow = new SettingRowWidget(
      tr("Start Minimized"),
      tr("Start the application quietly in the background on startup."),
      m_minimizedCheck, m_systemCard);
  m_systemCard->addRow(m_minimizedRow);

  layout->addWidget(m_systemCard);
  layout->addStretch(1);
}

void TabGeneral::retranslateUi() {
  if (m_appearanceCard) {
    m_appearanceCard->setTitle(tr("Appearance"));
  }
  if (m_themeRow) {
    m_themeRow->updateText(
        tr("Color Theme"),
        tr("Choose your preferred application visual appearance."));
  }
  if (m_themeCombo) {
    m_themeCombo->setItemText(0, tr("Dark Theme"));
    m_themeCombo->setItemText(1, tr("Light Theme"));
  }
  if (m_densityRow) {
    m_densityRow->updateText(
        tr("List Density"),
        tr("Adjust item row height and spacing in the vault list."));
  }
  if (m_densityCombo) {
    m_densityCombo->setItemText(0, tr("Comfortable"));
    m_densityCombo->setItemText(1, tr("Compact"));
  }

  if (m_languageCard) {
    m_languageCard->setTitle(tr("Language"));
  }
  if (m_langRow) {
    m_langRow->updateText(
        tr("Application Language"),
        tr("Select the language for all user interface text."));
  }

  if (m_systemCard) {
    m_systemCard->setTitle(tr("System Behavior"));
  }
  if (m_startupRow) {
    m_startupRow->updateText(tr("Launch on Startup"),
                             tr("Automatically start Secure Vault Manager when "
                                "your computer boots."));
  }
  if (m_trayRow) {
    m_trayRow->updateText(tr("Close to System Tray"),
                          tr("Closing the main window will minimize it to the "
                             "system tray instead of exiting."));
  }
  if (m_minimizedRow) {
    m_minimizedRow->updateText(
        tr("Start Minimized"),
        tr("Start the application quietly in the background on startup."));
  }
}

} // namespace svm::views
