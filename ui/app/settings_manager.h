/**
 * @file ui/app/settings_manager.h
 * @brief Quản lý cấu hình ứng dụng tập trung sử dụng QSettings và đồng bộ thời
 * gian thực.
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#pragma once

#include "app/language_manager.h"
#include "app/theme_manager.h"

#include <QObject>
#include <QSettings>
#include <QString>

namespace svm::app {

enum class ListDensity { Comfortable, Compact };

class SettingsManager : public QObject {
  Q_OBJECT

public:
  static SettingsManager &instance();

  ~SettingsManager() override = default;
  SettingsManager(const SettingsManager &) = delete;
  SettingsManager &operator=(const SettingsManager &) = delete;

  // 1. General Settings
  [[nodiscard]] ThemeMode themeMode() const;
  void setThemeMode(ThemeMode mode);

  [[nodiscard]] AppLanguage language() const;
  void setLanguage(AppLanguage lang);

  [[nodiscard]] ListDensity listDensity() const;
  void setListDensity(ListDensity density);

  [[nodiscard]] bool launchOnStartup() const;
  void setLaunchOnStartup(bool enable);

  [[nodiscard]] bool closeToTray() const;
  void setCloseToTray(bool enable);

  [[nodiscard]] bool startMinimized() const;
  void setStartMinimized(bool enable);

  [[nodiscard]] bool faviconFetchEnabled() const;
  [[nodiscard]] bool hasFaviconFetchSetting() const;
  void setFaviconFetchEnabled(bool enable);

  // 2. Security Settings
  [[nodiscard]] int autoLockMinutes() const; // 0 = Never, 1, 5, 15, 30, 60
  void setAutoLockMinutes(int minutes);

  [[nodiscard]] bool lockOnSleep() const;
  void setLockOnSleep(bool enable);

  [[nodiscard]] int clipboardClearSeconds() const; // 0 = Never, 10, 30, 60, 90
  void setClipboardClearSeconds(int seconds);

  [[nodiscard]] bool clearClipboardOnLock() const;
  void setClearClipboardOnLock(bool enable);

  [[nodiscard]] bool biometricEnabled() const;
  void setBiometricEnabled(bool enable);

  // 3. Browser Integration (Mock)
  [[nodiscard]] bool browserBridgeEnabled() const;
  void setBrowserBridgeEnabled(bool enable);

  // 4. SSH Agent (Mock)
  [[nodiscard]] bool sshAgentEnabled() const;
  void setSshAgentEnabled(bool enable);

  [[nodiscard]] bool sshPromptApproval() const;
  void setSshPromptApproval(bool enable);

  // 5. Data & Maintenance
  [[nodiscard]] bool autoEmptyTrashDaysEnabled() const;
  void setAutoEmptyTrashDaysEnabled(bool enable);

signals:
  void settingsChanged();
  void autoLockTimeoutChanged(int minutes);
  void clipboardTimeoutChanged(int seconds);
  void listDensityChanged(ListDensity density);

private:
  explicit SettingsManager(QObject *parent = nullptr);

  QSettings m_settings;
};

} // namespace svm::app
