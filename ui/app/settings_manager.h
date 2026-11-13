/**
 * @file ui/app/settings_manager.h
 * @brief Quản lý cấu hình ứng dụng tập trung theo chuẩn Single Source of Truth
 * với Rust Core (settings.json).
 * @author Hai Pham Ngoc <ngochai285nd@gmail.com>
 * @copyright (c) 2026 Hephaestus Developers. All rights reserved.
 */

#pragma once

#include "app/language_manager.h"
#include "app/theme_manager.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
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

  void initialize();

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

  // 3. Browser Integration
  [[nodiscard]] bool browserBridgeEnabled() const;
  void setBrowserBridgeEnabled(bool enable);

  // 4. SSH Agent
  [[nodiscard]] bool sshAgentEnabled() const;
  void setSshAgentEnabled(bool enable);

  [[nodiscard]] bool sshPromptApproval() const;
  void setSshPromptApproval(bool enable);

  // 5. Data & Maintenance
  [[nodiscard]] bool autoEmptyTrashDaysEnabled() const;
  void setAutoEmptyTrashDaysEnabled(bool enable);

  // 6. Security Credentials & Vaults Metadata (Single Source of Truth)
  [[nodiscard]] bool isConfigured() const;
  void setConfigured(bool configured);

  [[nodiscard]] QJsonArray cachedVaults() const;
  void setCachedVaults(const QJsonArray &vaults);

  void resetSecurityAndVaults();

signals:
  void settingsChanged();
  void autoLockTimeoutChanged(int minutes);
  void listDensityChanged(ListDensity density);

private slots:
  void onCoreSettingsUpdated(const QJsonObject &settings);

private:
  explicit SettingsManager(QObject *parent = nullptr);

  void loadDefaults();
  void loadFromCore();
  void migrateLegacyIniIfNeeded();
  void saveToCore();

  QJsonObject m_cachedSettings;
  bool m_syncingFromCore{false};
  bool m_initialized{false};
};

} // namespace svm::app
